#include "MonolithLevelSequenceActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithParamSchema.h"
#include "MonolithIndexSubsystem.h"
#include "MonolithIndexDatabase.h"
#include "SQLiteDatabase.h"
#include "Editor.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"

// ============================================================================
// Registration
// ============================================================================

void FMonolithLevelSequenceActions::RegisterActions(FMonolithToolRegistry& Registry)
{
	Registry.RegisterAction(TEXT("level_sequence"), TEXT("ping"),
		TEXT("Smoke test — returns {status:ok, module:MonolithLevelSequence} when the module is loaded."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::Ping),
		FParamSchemaBuilder().Build());

	Registry.RegisterAction(TEXT("level_sequence"), TEXT("list_directors"),
		TEXT("List all Level Sequences that have a Director Blueprint, with director name and function/variable counts. Optional asset_path_filter is a glob pattern (* and ?) matched against ls_path."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::ListDirectors),
		FParamSchemaBuilder()
			.Optional(TEXT("asset_path_filter"), TEXT("string"), TEXT("Glob pattern to filter ls_path (e.g., \"/MyModule/*\")"))
			.Build());

	Registry.RegisterAction(TEXT("level_sequence"), TEXT("get_director_info"),
		TEXT("Get summary information for a single Level Sequence Director: function counts grouped by kind (user / custom_event / sequencer_endpoint), variable count, event-binding counts (total + resolved), and a sample of up to 10 functions for quick orientation."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::GetDirectorInfo),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Full Level Sequence asset path (e.g., \"/Game/Cinematics/LS_Intro.LS_Intro\")"))
			.Build());

	Registry.RegisterAction(TEXT("level_sequence"), TEXT("list_director_functions"),
		TEXT("List a Director's own functions, optionally filtered by kind. Inherited base-class methods and compiler-generated dispatchers are not indexed (own-functions only, matching blueprint_query convention)."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::ListDirectorFunctions),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Full Level Sequence asset path"))
			.Optional(TEXT("kind"), TEXT("string"), TEXT("Filter: \"user\" | \"custom_event\" | \"sequencer_endpoint\" | \"event\" (alias for custom_event+sequencer_endpoint) | \"all\" (default)"))
			.Build());

	Registry.RegisterAction(TEXT("level_sequence"), TEXT("list_event_bindings"),
		TEXT("List all event-track bindings inside one Level Sequence, grouped by binding GUID. Each binding entry describes a Possessable (existing level actor), Spawnable (template-spawned), or master track (no GUID), and lists the sections that fire Director functions."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::ListEventBindings),
		FParamSchemaBuilder()
			.Required(TEXT("asset_path"), TEXT("string"), TEXT("Full Level Sequence asset path"))
			.Build());

	Registry.RegisterAction(TEXT("level_sequence"), TEXT("find_director_function_callers"),
		TEXT("Cross-sequence reverse lookup: given a Director function name, return every event-track section across the project that fires it, with LS path and binding context. Optional asset_path_filter is a glob (* and ?) to narrow the search."),
		FMonolithActionHandler::CreateStatic(&FMonolithLevelSequenceActions::FindDirectorFunctionCallers),
		FParamSchemaBuilder()
			.Required(TEXT("function_name"), TEXT("string"), TEXT("Exact function name to search (case-sensitive). Examples: \"Start\", \"SequenceEvent__ENTRYPOINTLS_Foo_DirectorBP_0\""))
			.Optional(TEXT("asset_path_filter"), TEXT("string"), TEXT("Glob pattern (* and ?) restricting matches to LS paths matching this pattern"))
			.Build());
}

// ============================================================================
// Handlers
// ============================================================================

FMonolithActionResult FMonolithLevelSequenceActions::Ping(const TSharedPtr<FJsonObject>& Params)
{
	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("status"), TEXT("ok"));
	Result->SetStringField(TEXT("module"), TEXT("MonolithLevelSequence"));
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithLevelSequenceActions::ListDirectors(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMonolithActionResult::Error(TEXT("GEditor not available"));
	}

	UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>();
	if (!IndexSS || !IndexSS->GetDatabase())
	{
		return FMonolithActionResult::Error(TEXT("Index database not ready"));
	}

	FSQLiteDatabase* RawDB = IndexSS->GetDatabase()->GetRawDatabase();
	if (!RawDB)
	{
		return FMonolithActionResult::Error(TEXT("Raw SQLite database not available"));
	}

	// Optional glob filter; convert * -> %, ? -> _ for SQL LIKE.
	FString PathFilter;
	Params->TryGetStringField(TEXT("asset_path_filter"), PathFilter);

	FString SQL = TEXT("SELECT ls_path, director_bp_name, function_count, variable_count "
	                   "FROM level_sequence_directors");
	if (!PathFilter.IsEmpty())
	{
		const FString LikePattern = PathFilter
			.Replace(TEXT("'"), TEXT("''"))
			.Replace(TEXT("*"), TEXT("%"))
			.Replace(TEXT("?"), TEXT("_"));
		SQL += FString::Printf(TEXT(" WHERE ls_path LIKE '%s'"), *LikePattern);
	}
	SQL += TEXT(" ORDER BY ls_path");

	FSQLitePreparedStatement Stmt;
	if (!Stmt.Create(*RawDB, *SQL))
	{
		return FMonolithActionResult::Error(TEXT("Failed to prepare list_directors SQL"));
	}

	TArray<TSharedPtr<FJsonValue>> Directors;
	while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		FString LsPath, DirName;
		int64 FuncCount = 0, VarCount = 0;
		Stmt.GetColumnValueByIndex(0, LsPath);
		Stmt.GetColumnValueByIndex(1, DirName);
		Stmt.GetColumnValueByIndex(2, FuncCount);
		Stmt.GetColumnValueByIndex(3, VarCount);

		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("ls_path"), LsPath);
		Row->SetStringField(TEXT("director_bp_name"), DirName);
		Row->SetNumberField(TEXT("function_count"), FuncCount);
		Row->SetNumberField(TEXT("variable_count"), VarCount);
		Directors.Add(MakeShared<FJsonValueObject>(Row));
	}
	Stmt.Destroy();

	auto Result = MakeShared<FJsonObject>();
	Result->SetArrayField(TEXT("directors"), Directors);
	Result->SetNumberField(TEXT("count"), Directors.Num());
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithLevelSequenceActions::GetDirectorInfo(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMonolithActionResult::Error(TEXT("GEditor not available"));
	}

	UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>();
	if (!IndexSS || !IndexSS->GetDatabase())
	{
		return FMonolithActionResult::Error(TEXT("Index database not ready"));
	}

	FSQLiteDatabase* RawDB = IndexSS->GetDatabase()->GetRawDatabase();
	if (!RawDB)
	{
		return FMonolithActionResult::Error(TEXT("Raw SQLite database not available"));
	}

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("asset_path is required"));
	}

	// 1) Look up director row.
	int64 DirectorId = -1, LsAssetId = -1, FuncCount = 0, VarCount = 0;
	FString DirName;
	{
		FSQLitePreparedStatement Stmt;
		if (!Stmt.Create(*RawDB, TEXT("SELECT id, ls_asset_id, director_bp_name, function_count, variable_count FROM level_sequence_directors WHERE ls_path = ?")))
		{
			return FMonolithActionResult::Error(TEXT("Failed to prepare director lookup SQL"));
		}
		Stmt.SetBindingValueByIndex(1, AssetPath);
		if (Stmt.Step() != ESQLitePreparedStatementStepResult::Row)
		{
			Stmt.Destroy();
			return FMonolithActionResult::Error(FString::Printf(
				TEXT("No Level Sequence Director indexed for path '%s' (the LS may have no Director Blueprint, or the path is wrong — expected the full object path, e.g. '/Module/.../File.File')"),
				*AssetPath));
		}
		Stmt.GetColumnValueByIndex(0, DirectorId);
		Stmt.GetColumnValueByIndex(1, LsAssetId);
		Stmt.GetColumnValueByIndex(2, DirName);
		Stmt.GetColumnValueByIndex(3, FuncCount);
		Stmt.GetColumnValueByIndex(4, VarCount);
		Stmt.Destroy();
	}

	// 2) Function breakdown by kind (one row per kind, count).
	TMap<FString, int64> KindCounts;
	{
		FSQLitePreparedStatement Stmt;
		if (Stmt.Create(*RawDB, TEXT("SELECT kind, count(*) FROM level_sequence_director_functions "
			"WHERE director_id = ? GROUP BY kind")))
		{
			Stmt.SetBindingValueByIndex(1, DirectorId);
			while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				FString Kind;
				int64 Count = 0;
				Stmt.GetColumnValueByIndex(0, Kind);
				Stmt.GetColumnValueByIndex(1, Count);
				KindCounts.Add(Kind, Count);
			}
		}
		Stmt.Destroy();
	}

	// 3) Event-binding counts.
	int64 BindingsTotal = 0, BindingsResolved = 0;
	{
		FSQLitePreparedStatement Stmt;
		if (Stmt.Create(*RawDB, TEXT("SELECT count(*), "
			"sum(CASE WHEN fires_function_id IS NOT NULL THEN 1 ELSE 0 END) "
			"FROM level_sequence_event_bindings WHERE ls_asset_id = ?")))
		{
			Stmt.SetBindingValueByIndex(1, LsAssetId);
			if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				Stmt.GetColumnValueByIndex(0, BindingsTotal);
				Stmt.GetColumnValueByIndex(1, BindingsResolved);
			}
		}
		Stmt.Destroy();
	}

	// 4) Sample of up to 10 functions (user first, then custom_event, then sequencer_endpoint).
	TArray<TSharedPtr<FJsonValue>> SampleFns;
	{
		FSQLitePreparedStatement Stmt;
		// CASE-WHEN gives explicit kind ordering (user before custom_event before sequencer_endpoint).
		if (Stmt.Create(*RawDB, TEXT("SELECT name, kind FROM level_sequence_director_functions "
			"WHERE director_id = ? "
			"ORDER BY CASE kind "
			"  WHEN 'user' THEN 0 "
			"  WHEN 'custom_event' THEN 1 "
			"  WHEN 'sequencer_endpoint' THEN 2 "
			"  ELSE 3 END, name LIMIT 10")))
		{
			Stmt.SetBindingValueByIndex(1, DirectorId);
			while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				FString Name, Kind;
				Stmt.GetColumnValueByIndex(0, Name);
				Stmt.GetColumnValueByIndex(1, Kind);

				auto Obj = MakeShared<FJsonObject>();
				Obj->SetStringField(TEXT("name"), Name);
				Obj->SetStringField(TEXT("kind"), Kind);
				SampleFns.Add(MakeShared<FJsonValueObject>(Obj));
			}
		}
		Stmt.Destroy();
	}

	auto FunctionBreakdown = MakeShared<FJsonObject>();
	for (const TPair<FString, int64>& Pair : KindCounts)
	{
		FunctionBreakdown->SetNumberField(Pair.Key, Pair.Value);
	}

	auto BindingsObj = MakeShared<FJsonObject>();
	BindingsObj->SetNumberField(TEXT("total"), BindingsTotal);
	BindingsObj->SetNumberField(TEXT("resolved"), BindingsResolved);

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("ls_path"), AssetPath);
	Result->SetStringField(TEXT("director_bp_name"), DirName);
	Result->SetNumberField(TEXT("function_count"), FuncCount);
	Result->SetObjectField(TEXT("function_breakdown"), FunctionBreakdown);
	Result->SetNumberField(TEXT("variable_count"), VarCount);
	Result->SetObjectField(TEXT("event_bindings"), BindingsObj);
	Result->SetArrayField(TEXT("sample_functions"), SampleFns);
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithLevelSequenceActions::ListDirectorFunctions(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMonolithActionResult::Error(TEXT("GEditor not available"));
	}

	UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>();
	if (!IndexSS || !IndexSS->GetDatabase())
	{
		return FMonolithActionResult::Error(TEXT("Index database not ready"));
	}

	FSQLiteDatabase* RawDB = IndexSS->GetDatabase()->GetRawDatabase();
	if (!RawDB)
	{
		return FMonolithActionResult::Error(TEXT("Raw SQLite database not available"));
	}

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("asset_path is required"));
	}

	FString KindFilter;
	Params->TryGetStringField(TEXT("kind"), KindFilter);
	KindFilter = KindFilter.ToLower();

	// Build WHERE clause for kind filter.
	FString WhereKind;
	if (KindFilter.IsEmpty() || KindFilter == TEXT("all"))
	{
		WhereKind = TEXT("");
	}
	else if (KindFilter == TEXT("user") || KindFilter == TEXT("custom_event") || KindFilter == TEXT("sequencer_endpoint"))
	{
		WhereKind = FString::Printf(TEXT(" AND f.kind = '%s'"), *KindFilter);
	}
	else if (KindFilter == TEXT("event"))
	{
		WhereKind = TEXT(" AND f.kind IN ('custom_event', 'sequencer_endpoint')");
	}
	else
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("Unknown kind '%s'. Valid: user, custom_event, sequencer_endpoint, event, all"), *KindFilter));
	}

	// JOIN against directors so we can resolve by ls_path in one go.
	const FString SQL = FString::Printf(
		TEXT("SELECT f.name, f.kind, f.signature_json "
			 "FROM level_sequence_director_functions f "
			 "JOIN level_sequence_directors d ON f.director_id = d.id "
			 "WHERE d.ls_path = ?%s "
			 "ORDER BY CASE f.kind "
			 "  WHEN 'user' THEN 0 "
			 "  WHEN 'custom_event' THEN 1 "
			 "  WHEN 'sequencer_endpoint' THEN 2 "
			 "  ELSE 3 END, f.name"),
		*WhereKind);

	FSQLitePreparedStatement Stmt;
	if (!Stmt.Create(*RawDB, *SQL))
	{
		return FMonolithActionResult::Error(TEXT("Failed to prepare list_director_functions SQL"));
	}
	Stmt.SetBindingValueByIndex(1, AssetPath);

	TArray<TSharedPtr<FJsonValue>> Rows;
	while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		FString Name, Kind, SigRaw;
		Stmt.GetColumnValueByIndex(0, Name);
		Stmt.GetColumnValueByIndex(1, Kind);
		Stmt.GetColumnValueByIndex(2, SigRaw);

		// Parse signature_json (stored as a JSON array string) into nested JSON value.
		TSharedPtr<FJsonValue> SigValue;
		if (!SigRaw.IsEmpty())
		{
			TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(SigRaw);
			FJsonSerializer::Deserialize(Reader, SigValue);
		}
		if (!SigValue.IsValid())
		{
			SigValue = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>());
		}

		auto Obj = MakeShared<FJsonObject>();
		Obj->SetStringField(TEXT("name"), Name);
		Obj->SetStringField(TEXT("kind"), Kind);
		Obj->SetField(TEXT("signature"), SigValue);
		Rows.Add(MakeShared<FJsonValueObject>(Obj));
	}
	Stmt.Destroy();

	if (Rows.Num() == 0)
	{
		// Distinguish "no functions match this kind" from "no director at this path".
		FSQLitePreparedStatement Probe;
		Probe.Create(*RawDB, TEXT("SELECT 1 FROM level_sequence_directors WHERE ls_path = ?"));
		Probe.SetBindingValueByIndex(1, AssetPath);
		const bool bDirectorExists = (Probe.Step() == ESQLitePreparedStatementStepResult::Row);
		Probe.Destroy();
		if (!bDirectorExists)
		{
			return FMonolithActionResult::Error(FString::Printf(
				TEXT("No Level Sequence Director indexed for path '%s'"), *AssetPath));
		}
		// else: director exists but no functions match — empty array is the right answer.
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("ls_path"), AssetPath);
	Result->SetStringField(TEXT("kind_filter"), KindFilter.IsEmpty() ? TEXT("all") : KindFilter);
	Result->SetArrayField(TEXT("functions"), Rows);
	Result->SetNumberField(TEXT("count"), Rows.Num());
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithLevelSequenceActions::ListEventBindings(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMonolithActionResult::Error(TEXT("GEditor not available"));
	}

	UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>();
	if (!IndexSS || !IndexSS->GetDatabase())
	{
		return FMonolithActionResult::Error(TEXT("Index database not ready"));
	}

	FSQLiteDatabase* RawDB = IndexSS->GetDatabase()->GetRawDatabase();
	if (!RawDB)
	{
		return FMonolithActionResult::Error(TEXT("Raw SQLite database not available"));
	}

	FString AssetPath;
	if (!Params->TryGetStringField(TEXT("asset_path"), AssetPath) || AssetPath.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("asset_path is required"));
	}

	// Sanity-check the LS exists in the directors table; helps distinguish
	// "no Director / wrong path" from "Director exists but has no event-tracks".
	bool bDirectorKnown = false;
	{
		FSQLitePreparedStatement Probe;
		Probe.Create(*RawDB, TEXT("SELECT 1 FROM level_sequence_directors WHERE ls_path = ?"));
		Probe.SetBindingValueByIndex(1, AssetPath);
		bDirectorKnown = (Probe.Step() == ESQLitePreparedStatementStepResult::Row);
		Probe.Destroy();
	}
	if (!bDirectorKnown)
	{
		return FMonolithActionResult::Error(FString::Printf(
			TEXT("No Level Sequence Director indexed for path '%s'"), *AssetPath));
	}

	// JOIN bindings against the director (for ls_asset_id) and LEFT JOIN against
	// functions (some bindings may not resolve — fires_function_id is NULL).
	const FString SQL = TEXT(
		"SELECT b.binding_guid, b.binding_name, b.binding_kind, b.bound_class, "
		"       b.section_kind, b.fires_function_name, "
		"       f.kind, f.signature_json "
		"FROM level_sequence_event_bindings b "
		"JOIN level_sequence_directors d ON d.ls_asset_id = b.ls_asset_id "
		"LEFT JOIN level_sequence_director_functions f ON f.id = b.fires_function_id "
		"WHERE d.ls_path = ? "
		"ORDER BY CASE b.binding_kind "
		"  WHEN 'master' THEN 0 "
		"  WHEN 'possessable' THEN 1 "
		"  WHEN 'spawnable' THEN 2 "
		"  ELSE 3 END, b.binding_name, b.id");

	FSQLitePreparedStatement Stmt;
	if (!Stmt.Create(*RawDB, *SQL))
	{
		return FMonolithActionResult::Error(TEXT("Failed to prepare list_event_bindings SQL"));
	}
	Stmt.SetBindingValueByIndex(1, AssetPath);

	// Group rows by binding_guid (NULL guid → one master group).
	struct FBindingAccum
	{
		FString Guid;
		FString Name;
		FString Kind;
		FString BoundClass;
		TArray<TSharedPtr<FJsonValue>> Sections;
	};
	TMap<FString, FBindingAccum> Bindings;
	TArray<FString> BindingOrder;   // preserves SQL ORDER BY

	int32 TotalSections = 0;
	int32 ResolvedSections = 0;

	while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		FString BGuid, BName, BKind, BClass, SectionKind, FiresName, ResolvedKind, ResolvedSig;
		Stmt.GetColumnValueByIndex(0, BGuid);
		Stmt.GetColumnValueByIndex(1, BName);
		Stmt.GetColumnValueByIndex(2, BKind);
		Stmt.GetColumnValueByIndex(3, BClass);
		Stmt.GetColumnValueByIndex(4, SectionKind);
		Stmt.GetColumnValueByIndex(5, FiresName);
		Stmt.GetColumnValueByIndex(6, ResolvedKind);
		Stmt.GetColumnValueByIndex(7, ResolvedSig);

		// Group key. Master tracks have NULL guid → bucket them under literal "<master>".
		const FString GroupKey = BGuid.IsEmpty() ? FString(TEXT("<master>")) : BGuid;
		FBindingAccum* Acc = Bindings.Find(GroupKey);
		if (!Acc)
		{
			FBindingAccum New;
			New.Guid = BGuid;
			New.Name = BName;
			New.Kind = BKind;
			New.BoundClass = BClass;
			Bindings.Add(GroupKey, MoveTemp(New));
			BindingOrder.Add(GroupKey);
			Acc = Bindings.Find(GroupKey);
		}

		auto SectionObj = MakeShared<FJsonObject>();
		SectionObj->SetStringField(TEXT("section_kind"), SectionKind);
		if (FiresName.IsEmpty())
		{
			SectionObj->SetField(TEXT("fires_function_name"), MakeShared<FJsonValueNull>());
		}
		else
		{
			SectionObj->SetStringField(TEXT("fires_function_name"), FiresName);
		}

		// Resolved function info (null when fires_function_id was NULL).
		if (!ResolvedKind.IsEmpty())
		{
			TSharedPtr<FJsonValue> SigValue;
			if (!ResolvedSig.IsEmpty())
			{
				TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ResolvedSig);
				FJsonSerializer::Deserialize(Reader, SigValue);
			}
			if (!SigValue.IsValid())
			{
				SigValue = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>());
			}

			auto Resolved = MakeShared<FJsonObject>();
			Resolved->SetStringField(TEXT("kind"), ResolvedKind);
			Resolved->SetField(TEXT("signature"), SigValue);
			SectionObj->SetObjectField(TEXT("resolved"), Resolved);
			++ResolvedSections;
		}
		else
		{
			SectionObj->SetField(TEXT("resolved"), MakeShared<FJsonValueNull>());
		}

		Acc->Sections.Add(MakeShared<FJsonValueObject>(SectionObj));
		++TotalSections;
	}
	Stmt.Destroy();

	TArray<TSharedPtr<FJsonValue>> BindingsArr;
	for (const FString& Key : BindingOrder)
	{
		const FBindingAccum& Acc = Bindings.FindChecked(Key);
		auto BObj = MakeShared<FJsonObject>();

		if (Acc.Guid.IsEmpty())
		{
			BObj->SetField(TEXT("binding_guid"), MakeShared<FJsonValueNull>());
		}
		else
		{
			BObj->SetStringField(TEXT("binding_guid"), Acc.Guid);
		}
		if (Acc.Name.IsEmpty())
		{
			BObj->SetField(TEXT("binding_name"), MakeShared<FJsonValueNull>());
		}
		else
		{
			BObj->SetStringField(TEXT("binding_name"), Acc.Name);
		}
		BObj->SetStringField(TEXT("binding_kind"), Acc.Kind.IsEmpty() ? TEXT("unknown") : Acc.Kind);
		if (Acc.BoundClass.IsEmpty())
		{
			BObj->SetField(TEXT("bound_class"), MakeShared<FJsonValueNull>());
		}
		else
		{
			BObj->SetStringField(TEXT("bound_class"), Acc.BoundClass);
		}
		BObj->SetArrayField(TEXT("sections"), Acc.Sections);
		BObj->SetNumberField(TEXT("section_count"), Acc.Sections.Num());

		BindingsArr.Add(MakeShared<FJsonValueObject>(BObj));
	}

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("ls_path"), AssetPath);
	Result->SetArrayField(TEXT("bindings"), BindingsArr);
	Result->SetNumberField(TEXT("binding_count"), BindingsArr.Num());
	Result->SetNumberField(TEXT("section_count"), TotalSections);
	Result->SetNumberField(TEXT("resolved_section_count"), ResolvedSections);
	return FMonolithActionResult::Success(Result);
}

FMonolithActionResult FMonolithLevelSequenceActions::FindDirectorFunctionCallers(const TSharedPtr<FJsonObject>& Params)
{
	if (!GEditor)
	{
		return FMonolithActionResult::Error(TEXT("GEditor not available"));
	}

	UMonolithIndexSubsystem* IndexSS = GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>();
	if (!IndexSS || !IndexSS->GetDatabase())
	{
		return FMonolithActionResult::Error(TEXT("Index database not ready"));
	}

	FSQLiteDatabase* RawDB = IndexSS->GetDatabase()->GetRawDatabase();
	if (!RawDB)
	{
		return FMonolithActionResult::Error(TEXT("Raw SQLite database not available"));
	}

	FString FunctionName;
	if (!Params->TryGetStringField(TEXT("function_name"), FunctionName) || FunctionName.IsEmpty())
	{
		return FMonolithActionResult::Error(TEXT("function_name is required"));
	}

	FString PathFilter;
	Params->TryGetStringField(TEXT("asset_path_filter"), PathFilter);

	// Build SQL with optional ls_path filter (glob → LIKE).
	FString PathClause;
	FString LikePattern;
	if (!PathFilter.IsEmpty())
	{
		LikePattern = PathFilter
			.Replace(TEXT("'"), TEXT("''"))
			.Replace(TEXT("*"), TEXT("%"))
			.Replace(TEXT("?"), TEXT("_"));
		PathClause = TEXT(" AND d.ls_path LIKE ?");
	}

	const FString SQL = FString::Printf(TEXT(
		"SELECT d.ls_path, b.binding_guid, b.binding_name, b.binding_kind, b.bound_class, "
		"       b.section_kind, f.kind "
		"FROM level_sequence_event_bindings b "
		"JOIN level_sequence_directors d ON d.ls_asset_id = b.ls_asset_id "
		"LEFT JOIN level_sequence_director_functions f ON f.id = b.fires_function_id "
		"WHERE b.fires_function_name = ?%s "
		"ORDER BY d.ls_path, CASE b.binding_kind "
		"  WHEN 'master' THEN 0 "
		"  WHEN 'possessable' THEN 1 "
		"  WHEN 'spawnable' THEN 2 "
		"  ELSE 3 END, b.binding_name, b.id"),
		*PathClause);

	FSQLitePreparedStatement Stmt;
	if (!Stmt.Create(*RawDB, *SQL))
	{
		return FMonolithActionResult::Error(TEXT("Failed to prepare find_director_function_callers SQL"));
	}
	Stmt.SetBindingValueByIndex(1, FunctionName);
	if (!PathFilter.IsEmpty())
	{
		Stmt.SetBindingValueByIndex(2, LikePattern);
	}

	TArray<TSharedPtr<FJsonValue>> Callers;
	while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
	{
		FString LsPath, BGuid, BName, BKind, BClass, SectionKind, ResolvedKind;
		Stmt.GetColumnValueByIndex(0, LsPath);
		Stmt.GetColumnValueByIndex(1, BGuid);
		Stmt.GetColumnValueByIndex(2, BName);
		Stmt.GetColumnValueByIndex(3, BKind);
		Stmt.GetColumnValueByIndex(4, BClass);
		Stmt.GetColumnValueByIndex(5, SectionKind);
		Stmt.GetColumnValueByIndex(6, ResolvedKind);

		auto Row = MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("ls_path"), LsPath);
		if (BGuid.IsEmpty())  Row->SetField(TEXT("binding_guid"), MakeShared<FJsonValueNull>()); else Row->SetStringField(TEXT("binding_guid"), BGuid);
		if (BName.IsEmpty())  Row->SetField(TEXT("binding_name"), MakeShared<FJsonValueNull>()); else Row->SetStringField(TEXT("binding_name"), BName);
		Row->SetStringField(TEXT("binding_kind"), BKind.IsEmpty() ? TEXT("unknown") : BKind);
		if (BClass.IsEmpty()) Row->SetField(TEXT("bound_class"), MakeShared<FJsonValueNull>()); else Row->SetStringField(TEXT("bound_class"), BClass);
		Row->SetStringField(TEXT("section_kind"), SectionKind);
		Row->SetBoolField(TEXT("resolved"), !ResolvedKind.IsEmpty());

		Callers.Add(MakeShared<FJsonValueObject>(Row));
	}
	Stmt.Destroy();

	auto Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("function_name"), FunctionName);
	if (PathFilter.IsEmpty())
	{
		Result->SetField(TEXT("asset_path_filter"), MakeShared<FJsonValueNull>());
	}
	else
	{
		Result->SetStringField(TEXT("asset_path_filter"), PathFilter);
	}
	Result->SetArrayField(TEXT("callers"), Callers);
	Result->SetNumberField(TEXT("count"), Callers.Num());
	return FMonolithActionResult::Success(Result);
}
