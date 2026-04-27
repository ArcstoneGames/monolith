#include "MonolithLevelSequenceIndexer.h"
#include "MonolithIndexDatabase.h"
#include "LevelSequence.h"
#include "MovieScene.h"
#include "MovieSceneBinding.h"
#include "MovieScenePossessable.h"
#include "MovieSceneSpawnable.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "Sections/MovieSceneEventTriggerSection.h"
#include "Sections/MovieSceneEventRepeaterSection.h"
#include "Channels/MovieSceneEvent.h"
#include "Channels/MovieSceneEventChannel.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CustomEvent.h"
#include "SQLiteDatabase.h"
#include "AssetRegistry/AssetData.h"
#include "UObject/UnrealType.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

// ─────────────────────────────────────────────────────────────
// Local helpers
// ─────────────────────────────────────────────────────────────

namespace
{
	FString EscapeSql(const FString& In)
	{
		return In.Replace(TEXT("'"), TEXT("''"));
	}

	/** Format a pin's data type as a human-readable string via the K2 schema. */
	FString PinTypeToString(const FEdGraphPinType& PinType)
	{
		return UEdGraphSchema_K2::TypeToText(PinType).ToString();
	}

	/** Convert an FBPVariableDescription's type to a human-readable string. */
	FString VarTypeToString(const FEdGraphPinType& PinType)
	{
		return PinTypeToString(PinType);
	}

	/**
	 * Serialize a list of UEdGraphPin* into a JSON array of {name, type} objects.
	 * Filters out hidden pins, exec pins, and the special exec-flow pins.
	 * Only includes pins matching the requested direction.
	 */
	FString BuildSignatureJson(const TArray<UEdGraphPin*>& Pins, EEdGraphPinDirection WantDir)
	{
		FString Out;
		TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<TCHAR>::Create(&Out);
		Writer->WriteArrayStart();
		for (UEdGraphPin* Pin : Pins)
		{
			if (!Pin) continue;
			if (Pin->bHidden) continue;
			if (Pin->Direction != WantDir) continue;
			if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec) continue;
			if (Pin->PinName == UEdGraphSchema_K2::PN_Then ||
			    Pin->PinName == UEdGraphSchema_K2::PN_Execute ||
			    Pin->PinName == UEdGraphSchema_K2::PN_Self)
			{
				continue;
			}

			Writer->WriteObjectStart();
			Writer->WriteValue(TEXT("name"), Pin->PinName.ToString());
			Writer->WriteValue(TEXT("type"), PinTypeToString(Pin->PinType));
			Writer->WriteObjectEnd();
		}
		Writer->WriteArrayEnd();
		Writer->Close();
		return Out;
	}

	/** Extract input-parameter signature from a user function graph (find UK2Node_FunctionEntry). */
	FString ExtractUserFunctionSignature(UEdGraph* Graph)
	{
		if (!Graph) return TEXT("[]");
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(Node))
			{
				// On FunctionEntry, the function's input parameters are the OUTPUT pins
				// (they flow out from the entry node into the graph body).
				return BuildSignatureJson(Entry->Pins, EGPD_Output);
			}
		}
		return TEXT("[]");
	}

	/** Extract input-parameter signature from a CustomEvent node. */
	FString ExtractCustomEventSignature(UK2Node_CustomEvent* CustomEvent)
	{
		if (!CustomEvent) return TEXT("[]");
		// On CustomEvent, parameters are also the OUTPUT pins (same flow direction as FunctionEntry).
		return BuildSignatureJson(CustomEvent->Pins, EGPD_Output);
	}

	/** Find existing director row id for the given LS path; returns -1 if none. */
	int64 SelectExistingDirectorId(FSQLiteDatabase* RawDB, const FString& LsPath)
	{
		if (!RawDB) return -1;
		FSQLitePreparedStatement Stmt;
		if (!Stmt.Create(*RawDB, TEXT("SELECT id FROM level_sequence_directors WHERE ls_path = ?"), ESQLitePreparedStatementFlags::Persistent))
		{
			return -1;
		}
		Stmt.SetBindingValueByIndex(1, LsPath);
		int64 FoundId = -1;
		if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			Stmt.GetColumnValueByIndex(0, FoundId);
		}
		Stmt.Destroy();
		return FoundId;
	}

	/**
	 * Find existing director row id AND its ls_asset_id for the given LS path.
	 * The ls_asset_id from a previous reindex pass may differ from the current
	 * AssetId (the core asset row gets a fresh autoincrement id every full
	 * reindex), so we need both to clean child rows that key off ls_asset_id.
	 */
	void SelectExistingDirectorIdAndAssetId(FSQLiteDatabase* RawDB, const FString& LsPath, int64& OutDirId, int64& OutAssetId)
	{
		OutDirId = -1;
		OutAssetId = -1;
		if (!RawDB) return;

		FSQLitePreparedStatement Stmt;
		if (!Stmt.Create(*RawDB, TEXT("SELECT id, ls_asset_id FROM level_sequence_directors WHERE ls_path = ?"), ESQLitePreparedStatementFlags::Persistent))
		{
			return;
		}
		Stmt.SetBindingValueByIndex(1, LsPath);
		if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			Stmt.GetColumnValueByIndex(0, OutDirId);
			Stmt.GetColumnValueByIndex(1, OutAssetId);
		}
		Stmt.Destroy();
	}

	/**
	 * Get the function-name an FMovieSceneEvent fires.
	 * Per UE 5.7 source (MovieSceneEventSectionBase.cpp::OnPostCompile):
	 *   - Ptrs.Function is the canonical post-compile UFunction* — prefer it.
	 *   - CompiledFunctionName is editor-only, set during OnPostCompile and
	 *     normally cleared right after Ptrs.Function is wired up.
	 * Returns empty string if neither is populated (event not yet bound).
	 */
	FString GetEventFunctionName(const FMovieSceneEvent& Event)
	{
		if (Event.Ptrs.Function)
		{
			return Event.Ptrs.Function->GetName();
		}
#if WITH_EDITORONLY_DATA
		if (!Event.CompiledFunctionName.IsNone())
		{
			return Event.CompiledFunctionName.ToString();
		}
#endif
		return FString();
	}

	/** Insert one row into level_sequence_event_bindings. */
	void InsertEventBindingRow(
		FSQLiteDatabase* RawDB,
		int64 LsAssetId,
		const FString& BindingGuidStr,
		const FString& BindingName,
		const FString& BindingKind,
		const FString& BoundClass,
		const FString& SectionKind,
		const FString& FiresFunctionName,
		int64 FiresFunctionId)
	{
		// SQL NULL for empty optional text fields (binding_guid for master tracks, etc.).
		auto SqlText = [](const FString& V) -> FString
		{
			return V.IsEmpty() ? TEXT("NULL") : FString::Printf(TEXT("'%s'"), *V.Replace(TEXT("'"), TEXT("''")));
		};
		const FString FuncIdSql = (FiresFunctionId > 0) ? FString::Printf(TEXT("%lld"), FiresFunctionId) : TEXT("NULL");

		// section_kind is NOT NULL in schema — emit as literal string, never NULL.
		const FString SQL = FString::Printf(
			TEXT("INSERT INTO level_sequence_event_bindings "
				 "(ls_asset_id, binding_guid, binding_name, binding_kind, bound_class, section_kind, fires_function_name, fires_function_id) "
				 "VALUES (%lld, %s, %s, %s, %s, '%s', %s, %s)"),
			LsAssetId,
			*SqlText(BindingGuidStr),
			*SqlText(BindingName),
			*SqlText(BindingKind),
			*SqlText(BoundClass),
			*SectionKind.Replace(TEXT("'"), TEXT("''")),
			*SqlText(FiresFunctionName),
			*FuncIdSql);
		RawDB->Execute(*SQL);
	}

	/**
	 * Walk a single event track's sections and insert one row per FMovieSceneEvent
	 * (Trigger sections may contain multiple timed events; Repeater sections have one).
	 */
	void ProcessEventTrack(
		const UMovieSceneEventTrack* EvTrack,
		FSQLiteDatabase* RawDB,
		int64 LsAssetId,
		const FString& BindingGuidStr,
		const FString& BindingName,
		const FString& BindingKind,
		const FString& BoundClass,
		const TMap<FName, int64>& FuncNameToId)
	{
		if (!EvTrack) return;

		auto ResolveFuncId = [&FuncNameToId](const FString& Name) -> int64
		{
			if (Name.IsEmpty()) return -1;
			const int64* Found = FuncNameToId.Find(FName(*Name));
			return Found ? *Found : -1;
		};

		for (UMovieSceneSection* Section : EvTrack->GetAllSections())
		{
			if (!Section) continue;

			if (const UMovieSceneEventTriggerSection* Trigger = Cast<UMovieSceneEventTriggerSection>(Section))
			{
				const FMovieSceneEventChannel& Channel = Trigger->EventChannel;
				TArrayView<const FMovieSceneEvent> Events = Channel.GetData().GetValues();
				for (const FMovieSceneEvent& Ev : Events)
				{
					const FString FuncName = GetEventFunctionName(Ev);
					InsertEventBindingRow(RawDB, LsAssetId, BindingGuidStr, BindingName, BindingKind, BoundClass,
						TEXT("trigger"), FuncName, ResolveFuncId(FuncName));
				}
				if (Events.Num() == 0)
				{
					// Empty trigger section — record one stub row so cinematics with no
					// keys yet still show up in inspections.
					InsertEventBindingRow(RawDB, LsAssetId, BindingGuidStr, BindingName, BindingKind, BoundClass,
						TEXT("trigger"), FString(), -1);
				}
			}
			else if (const UMovieSceneEventRepeaterSection* Repeater = Cast<UMovieSceneEventRepeaterSection>(Section))
			{
				const FString FuncName = GetEventFunctionName(Repeater->Event);
				InsertEventBindingRow(RawDB, LsAssetId, BindingGuidStr, BindingName, BindingKind, BoundClass,
					TEXT("repeater"), FuncName, ResolveFuncId(FuncName));
			}
		}
	}

	/** Resolve a binding GUID to (name, kind, class). Mutates out-params. */
	void ResolveBinding(UMovieScene* MS, const FGuid& Guid, FString& OutName, FString& OutKind, FString& OutClass)
	{
		OutName.Reset();
		OutKind.Reset();
		OutClass.Reset();
		if (!MS) return;

		if (FMovieScenePossessable* P = MS->FindPossessable(Guid))
		{
			OutName = P->GetName();
			OutKind = TEXT("possessable");
#if WITH_EDITORONLY_DATA
			if (const UClass* Cls = P->GetPossessedObjectClass())
			{
				OutClass = Cls->GetName();
			}
#endif
		}
		else if (FMovieSceneSpawnable* S = MS->FindSpawnable(Guid))
		{
			OutName = S->GetName();
			OutKind = TEXT("spawnable");
			if (const UObject* Tmpl = S->GetObjectTemplate())
			{
				OutClass = Tmpl->GetClass()->GetName();
			}
		}
		else
		{
			OutKind = TEXT("unknown");
		}
	}
}

// ─────────────────────────────────────────────────────────────
// IMonolithIndexer overrides
// ─────────────────────────────────────────────────────────────

TArray<FString> FLevelSequenceIndexer::GetSupportedClasses() const
{
	return { TEXT("LevelSequence") };
}

void FLevelSequenceIndexer::EnsureTablesExist(FMonolithIndexDatabase& DB)
{
	if (bTablesCreated) return;

	FSQLiteDatabase* RawDB = DB.GetRawDatabase();
	if (!RawDB) return;

	// NB: ls_asset_id is intentionally NOT a FOREIGN KEY on assets(id). The core
	// database's ResetDatabase() (called by force=true reindex) wipes built-in
	// tables (assets, nodes, etc.) without knowing about our custom tables. A
	// FK from us to assets makes that DELETE fail and aborts the whole reindex.
	// Pattern borrowed from MonolithAI/FAIIndexer (its ai_assets is also
	// FK-free against core assets for the same reason).
	RawDB->Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS level_sequence_directors ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  ls_asset_id INTEGER NOT NULL,"
		"  ls_path TEXT NOT NULL UNIQUE,"
		"  director_bp_name TEXT NOT NULL,"
		"  function_count INTEGER NOT NULL,"
		"  variable_count INTEGER NOT NULL"
		")"
	));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_path ON level_sequence_directors(ls_path)"));

	RawDB->Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS level_sequence_director_functions ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  director_id INTEGER NOT NULL,"
		"  name TEXT NOT NULL,"
		"  is_event_function INTEGER NOT NULL,"
		"  signature_json TEXT,"
		"  FOREIGN KEY (director_id) REFERENCES level_sequence_directors(id)"
		")"
	));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_func_dir ON level_sequence_director_functions(director_id)"));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_func_name ON level_sequence_director_functions(name)"));

	RawDB->Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS level_sequence_director_variables ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  director_id INTEGER NOT NULL,"
		"  name TEXT NOT NULL,"
		"  type TEXT NOT NULL,"
		"  FOREIGN KEY (director_id) REFERENCES level_sequence_directors(id)"
		")"
	));

	// ls_asset_id deliberately FK-free; see comment above on level_sequence_directors.
	RawDB->Execute(TEXT(
		"CREATE TABLE IF NOT EXISTS level_sequence_event_bindings ("
		"  id INTEGER PRIMARY KEY AUTOINCREMENT,"
		"  ls_asset_id INTEGER NOT NULL,"
		"  binding_guid TEXT,"
		"  binding_name TEXT,"
		"  binding_kind TEXT,"
		"  bound_class TEXT,"
		"  section_kind TEXT NOT NULL,"
		"  fires_function_name TEXT,"
		"  fires_function_id INTEGER,"
		"  FOREIGN KEY (fires_function_id) REFERENCES level_sequence_director_functions(id)"
		")"
	));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_eb_ls ON level_sequence_event_bindings(ls_asset_id)"));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_eb_func ON level_sequence_event_bindings(fires_function_id)"));
	RawDB->Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_lsdir_eb_funcname ON level_sequence_event_bindings(fires_function_name)"));

	bTablesCreated = true;
}

bool FLevelSequenceIndexer::IndexAsset(const FAssetData& AssetData, UObject* LoadedAsset, FMonolithIndexDatabase& DB, int64 AssetId)
{
	EnsureTablesExist(DB);

	FSQLiteDatabase* RawDB = DB.GetRawDatabase();
	if (!RawDB) return false;

	ULevelSequence* Seq = Cast<ULevelSequence>(LoadedAsset);
	if (!Seq) return false;

	const FString LsPathEarly = Seq->GetPathName();

	// Look up any prior director row for this LS — both its id and the asset_id
	// it was created under (which may differ from current AssetId after a force
	// reindex, since core's `assets` table is wiped and autoincrement restarts).
	int64 OldDirId = -1, OldAssetId = -1;
	SelectExistingDirectorIdAndAssetId(RawDB, LsPathEarly, OldDirId, OldAssetId);

	// Wipe event_bindings under BOTH the current asset id and the old one.
	// Without the OldAssetId pass, prior-reindex rows would orphan and never
	// be cleaned because their ls_asset_id no longer matches anything we know.
	RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_event_bindings WHERE ls_asset_id = %lld"), AssetId));
	if (OldAssetId > 0 && OldAssetId != AssetId)
	{
		RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_event_bindings WHERE ls_asset_id = %lld"), OldAssetId));
	}

#if WITH_EDITORONLY_DATA
	UBlueprint* DirBP = Seq->GetDirectorBlueprint();
	if (!DirBP)
	{
		// No Director — also wipe any leftover director/function/variable rows.
		if (OldDirId > 0)
		{
			RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_director_functions WHERE director_id = %lld"), OldDirId));
			RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_director_variables WHERE director_id = %lld"), OldDirId));
			RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_directors WHERE id = %lld"), OldDirId));
		}
		return true;
	}

	const FString& LsPath = LsPathEarly;
	const FString DirName = DirBP->GetName();

	// Collect user functions (FunctionGraphs) and event functions (CustomEvent in UbergraphPages).
	struct FFnRecord { FString Name; bool bIsEvent; FString SignatureJson; };
	TArray<FFnRecord> Functions;

	for (UEdGraph* FuncGraph : DirBP->FunctionGraphs)
	{
		if (!FuncGraph) continue;
		FFnRecord Rec;
		Rec.Name = FuncGraph->GetFName().ToString();
		Rec.bIsEvent = false;
		Rec.SignatureJson = ExtractUserFunctionSignature(FuncGraph);
		Functions.Add(MoveTemp(Rec));
	}

	for (UEdGraph* UberGraph : DirBP->UbergraphPages)
	{
		if (!UberGraph) continue;
		for (UEdGraphNode* Node : UberGraph->Nodes)
		{
			UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Node);
			if (!CustomEvent) continue;

			FFnRecord Rec;
			Rec.Name = CustomEvent->CustomFunctionName.ToString();
			Rec.bIsEvent = true;
			Rec.SignatureJson = ExtractCustomEventSignature(CustomEvent);
			Functions.Add(MoveTemp(Rec));
		}
	}

	// Compiled synthetic functions on the generated class: when a Sequencer Event
	// Track uses "Create New Endpoint" / "Quick Bind", UE generates UFunctions
	// like "SequenceEvent__ENTRYPOINT<DirBP>_N" that have no UK2Node_CustomEvent
	// in any graph. They exist only on the compiled UClass and are the actual
	// targets of FMovieSceneEvent::Ptrs::Function. Without indexing them, our
	// event_bindings table can't resolve fires_function_id at all.
	if (UClass* GenClass = DirBP->GeneratedClass)
	{
		TSet<FString> AlreadySeen;
		for (const FFnRecord& Rec : Functions) { AlreadySeen.Add(Rec.Name); }

		for (TFieldIterator<UFunction> It(GenClass); It; ++It)
		{
			UFunction* Fn = *It;
			if (!Fn) continue;
			const FString FnName = Fn->GetName();
			if (AlreadySeen.Contains(FnName)) continue;

			FFnRecord Rec;
			Rec.Name = FnName;
			Rec.bIsEvent = true;          // anything not in our graph lists is event-driven
			Rec.SignatureJson = TEXT("[]");
			Functions.Add(MoveTemp(Rec));
			AlreadySeen.Add(FnName);
		}
	}

	const int32 TotalFuncCount = Functions.Num();
	const int32 VarCount = DirBP->NewVariables.Num();

	// Clean prior director's children + director row (event_bindings already cleaned above).
	if (OldDirId > 0)
	{
		RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_director_functions WHERE director_id = %lld"), OldDirId));
		RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_director_variables WHERE director_id = %lld"), OldDirId));
		RawDB->Execute(*FString::Printf(TEXT("DELETE FROM level_sequence_directors WHERE id = %lld"), OldDirId));
	}

	// Insert fresh director row.
	const FString InsertDirSQL = FString::Printf(
		TEXT("INSERT INTO level_sequence_directors "
			 "(ls_asset_id, ls_path, director_bp_name, function_count, variable_count) "
			 "VALUES (%lld, '%s', '%s', %d, %d)"),
		AssetId,
		*EscapeSql(LsPath),
		*EscapeSql(DirName),
		TotalFuncCount,
		VarCount);

	if (!RawDB->Execute(*InsertDirSQL))
	{
		return false;
	}
	const int64 DirectorId = RawDB->GetLastInsertRowId();

	// Insert functions; remember name -> row-id for later event-binding resolution.
	TMap<FName, int64> FuncNameToId;
	for (const FFnRecord& Fn : Functions)
	{
		const FString InsertFnSQL = FString::Printf(
			TEXT("INSERT INTO level_sequence_director_functions "
				 "(director_id, name, is_event_function, signature_json) "
				 "VALUES (%lld, '%s', %d, '%s')"),
			DirectorId,
			*EscapeSql(Fn.Name),
			Fn.bIsEvent ? 1 : 0,
			*EscapeSql(Fn.SignatureJson));
		if (RawDB->Execute(*InsertFnSQL))
		{
			FuncNameToId.Add(FName(*Fn.Name), RawDB->GetLastInsertRowId());
		}
	}

	// Insert variables.
	for (const FBPVariableDescription& Var : DirBP->NewVariables)
	{
		const FString InsertVarSQL = FString::Printf(
			TEXT("INSERT INTO level_sequence_director_variables "
				 "(director_id, name, type) "
				 "VALUES (%lld, '%s', '%s')"),
			DirectorId,
			*EscapeSql(Var.VarName.ToString()),
			*EscapeSql(VarTypeToString(Var.VarType)));
		RawDB->Execute(*InsertVarSQL);
	}

	// Walk event tracks and record one row per FMovieSceneEvent.
	// fires_function_id is left NULL here; a post-pass UPDATE below resolves it
	// via SQL JOIN against level_sequence_director_functions for this asset.
	if (UMovieScene* MS = Seq->GetMovieScene())
	{
		// Master tracks (not bound to a binding GUID).
		for (UMovieSceneTrack* Track : MS->GetTracks())
		{
			if (UMovieSceneEventTrack* EvTrack = Cast<UMovieSceneEventTrack>(Track))
			{
				ProcessEventTrack(EvTrack, RawDB, AssetId, FString(), FString(), TEXT("master"), FString(), FuncNameToId);
			}
		}

		// Object-bound tracks. Use the const overload of GetBindings (the non-const one
		// is UE_DEPRECATED(5.7)). FindPossessable/FindSpawnable still need non-const MS.
		const UMovieScene* CMS = MS;
		for (const FMovieSceneBinding& Binding : CMS->GetBindings())
		{
			const FGuid Guid = Binding.GetObjectGuid();
			FString BindingName, BindingKind, BoundClass;
			ResolveBinding(MS, Guid, BindingName, BindingKind, BoundClass);

			const FString GuidStr = Guid.ToString(EGuidFormats::Digits);
			for (UMovieSceneTrack* Track : Binding.GetTracks())
			{
				if (UMovieSceneEventTrack* EvTrack = Cast<UMovieSceneEventTrack>(Track))
				{
					ProcessEventTrack(EvTrack, RawDB, AssetId, GuidStr, BindingName, BindingKind, BoundClass, FuncNameToId);
				}
			}
		}

		// Post-pass: resolve fires_function_id by joining on (director's asset, function name).
		// Done in SQL because the in-process FuncNameToId map proved unreliable across
		// the indexer's threading context — and a JOIN-based resolve is robust regardless.
		const FString ResolveSQL = FString::Printf(
			TEXT("UPDATE level_sequence_event_bindings AS b "
				 "SET fires_function_id = ("
				 "  SELECT f.id FROM level_sequence_director_functions f "
				 "  JOIN level_sequence_directors d ON f.director_id = d.id "
				 "  WHERE d.ls_asset_id = %lld AND f.name = b.fires_function_name "
				 "  LIMIT 1"
				 ") "
				 "WHERE b.ls_asset_id = %lld AND b.fires_function_id IS NULL AND b.fires_function_name IS NOT NULL"),
			AssetId, AssetId);
		RawDB->Execute(*ResolveSQL);
	}
#endif

	return true;
}
