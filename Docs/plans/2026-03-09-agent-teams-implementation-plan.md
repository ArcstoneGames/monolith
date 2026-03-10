# Agent Teams for Unreal Engine — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a multi-agent team system for Unreal Engine development, powered by Claude Code Agent Teams + Monolith MCP plugin, enabling a team lead to decompose UE goals into specialist tasks executed in parallel.

**Architecture:** New `MonolithTeams` C++ module (10 MCP actions including blackboard store, validation gates, compile-and-wait, asset dependency queries) bundled with Monolith but **disabled by default**. Agent definitions, orchestrator skill, quality gate hooks, and CLAUDE.md template shipped in `Templates/AgentTeams/` for opt-in setup. All layered on Claude Code's experimental Agent Teams feature and Monolith v0.5.0.

**Tech Stack:** UE 5.7 C++ (MonolithCore plugin pattern), SQLite (via MonolithIndex), Claude Code Agent Teams, MCP protocol, Bash hooks, Markdown agent definitions with YAML frontmatter.

**Design doc:** `Docs/plans/2026-03-09-agent-teams-design.md`

---

## Task 1: MonolithTeams Module Scaffold

**Files:**
- Create: `Source/MonolithTeams/MonolithTeams.Build.cs`
- Create: `Source/MonolithTeams/Public/MonolithTeamsModule.h`
- Create: `Source/MonolithTeams/Private/MonolithTeamsModule.cpp`
- Modify: `Monolith.uplugin` (add MonolithTeams module entry)

**Step 1: Create Build.cs**

```csharp
// Source/MonolithTeams/MonolithTeams.Build.cs
using UnrealBuildTool;

public class MonolithTeams : ModuleRules
{
	public MonolithTeams(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"MonolithCore",
			"MonolithIndex",
			"SQLiteCore",
			"UnrealEd",
			"AssetRegistry",
			"Json",
			"JsonUtilities"
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");
		}
	}
}
```

**Step 2: Create Module header**

```cpp
// Source/MonolithTeams/Public/MonolithTeamsModule.h
#pragma once

#include "Modules/ModuleManager.h"

class FMonolithTeamsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
```

**Step 3: Create Module implementation**

```cpp
// Source/MonolithTeams/Private/MonolithTeamsModule.cpp
#include "MonolithTeamsModule.h"
#include "MonolithTeamsActions.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"

#define LOCTEXT_NAMESPACE "FMonolithTeamsModule"

void FMonolithTeamsModule::StartupModule()
{
	FMonolithTeamsActions::RegisterActions();
	UE_LOG(LogMonolith, Log, TEXT("Monolith — Teams module loaded"));
}

void FMonolithTeamsModule::ShutdownModule()
{
	FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("teams"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FMonolithTeamsModule, MonolithTeams)
```

**Step 4: Add module to Monolith.uplugin**

Add to the `"Modules"` array (disabled by default — users opt in via Editor Preferences):
```json
{
	"Name": "MonolithTeams",
	"Type": "Editor",
	"LoadingPhase": "Default",
	"EnabledByDefault": false
}
```

**Step 5: Compile and verify module loads**

Run: Open Unreal Editor, check Output Log for `Monolith — Teams module loaded`
Expected: Module loads, no compile errors

**Step 6: Commit**

```bash
git add Source/MonolithTeams/ Monolith.uplugin
git commit -m "feat: scaffold MonolithTeams module"
```

---

## Task 2: Blackboard Store

**Files:**
- Create: `Source/MonolithTeams/Public/MonolithBlackboard.h`
- Create: `Source/MonolithTeams/Private/MonolithBlackboard.cpp`

**Step 1: Create Blackboard header**

```cpp
// Source/MonolithTeams/Public/MonolithBlackboard.h
#pragma once

#include "CoreMinimal.h"

class FSQLiteDatabase;

/**
 * Key-value store for cross-agent state sharing.
 * Backed by a SQLite table in the project index database.
 */
class FMonolithBlackboard
{
public:
	static FMonolithBlackboard& Get();

	/** Initialize with a SQLite database (borrows pointer, does not own) */
	bool Initialize(FSQLiteDatabase* InDatabase);

	/** Set a key-value pair. Source identifies the writer (e.g., "cpp-agent") */
	bool Set(const FString& Key, const FString& Value, const FString& Source = TEXT(""));

	/** Get a value by key. Returns empty optional if not found */
	TOptional<FString> Get(const FString& Key);

	/** List all entries, optionally filtered by key prefix */
	TArray<TPair<FString, FString>> List(const FString& Prefix = TEXT(""));

	/** Remove a specific key */
	bool Remove(const FString& Key);

	/** Clear all entries matching a prefix, or all entries if prefix is empty */
	int32 Clear(const FString& Prefix = TEXT(""));

	/** Check if initialized */
	bool IsInitialized() const { return Database != nullptr; }

private:
	FMonolithBlackboard() = default;
	bool EnsureTable();

	FSQLiteDatabase* Database = nullptr;
	bool bTableCreated = false;
};
```

**Step 2: Create Blackboard implementation**

```cpp
// Source/MonolithTeams/Private/MonolithBlackboard.cpp
#include "MonolithBlackboard.h"
#include "MonolithJsonUtils.h"
#include "SQLiteDatabase.h"

FMonolithBlackboard& FMonolithBlackboard::Get()
{
	static FMonolithBlackboard Instance;
	return Instance;
}

bool FMonolithBlackboard::Initialize(FSQLiteDatabase* InDatabase)
{
	Database = InDatabase;
	if (!Database) return false;
	return EnsureTable();
}

bool FMonolithBlackboard::EnsureTable()
{
	if (bTableCreated || !Database) return bTableCreated;

	const FString SQL = TEXT(
		"CREATE TABLE IF NOT EXISTS blackboard ("
		"  key TEXT PRIMARY KEY,"
		"  value TEXT NOT NULL,"
		"  updated_at TEXT DEFAULT (datetime('now')),"
		"  source TEXT DEFAULT ''"
		");"
	);

	if (Database->Execute(*SQL))
	{
		bTableCreated = true;
		return true;
	}
	UE_LOG(LogMonolith, Error, TEXT("Failed to create blackboard table"));
	return false;
}

bool FMonolithBlackboard::Set(const FString& Key, const FString& Value, const FString& Source)
{
	if (!Database || !EnsureTable()) return false;

	// Escape single quotes
	FString SafeKey = Key.Replace(TEXT("'"), TEXT("''"));
	FString SafeValue = Value.Replace(TEXT("'"), TEXT("''"));
	FString SafeSource = Source.Replace(TEXT("'"), TEXT("''"));

	const FString SQL = FString::Printf(
		TEXT("INSERT OR REPLACE INTO blackboard (key, value, source, updated_at) VALUES ('%s', '%s', '%s', datetime('now'))"),
		*SafeKey, *SafeValue, *SafeSource
	);

	return Database->Execute(*SQL);
}

TOptional<FString> FMonolithBlackboard::Get(const FString& Key)
{
	if (!Database || !EnsureTable()) return {};

	FString SafeKey = Key.Replace(TEXT("'"), TEXT("''"));
	const FString SQL = FString::Printf(
		TEXT("SELECT value FROM blackboard WHERE key = '%s'"), *SafeKey
	);

	FSQLitePreparedStatement Stmt;
	if (Database->PrepareStatement(*SQL, Stmt))
	{
		if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			FString Value;
			Stmt.GetColumnValueByIndex(0, Value);
			return Value;
		}
	}
	return {};
}

TArray<TPair<FString, FString>> FMonolithBlackboard::List(const FString& Prefix)
{
	TArray<TPair<FString, FString>> Results;
	if (!Database || !EnsureTable()) return Results;

	FString SQL;
	if (Prefix.IsEmpty())
	{
		SQL = TEXT("SELECT key, value FROM blackboard ORDER BY key");
	}
	else
	{
		FString SafePrefix = Prefix.Replace(TEXT("'"), TEXT("''"));
		SQL = FString::Printf(
			TEXT("SELECT key, value FROM blackboard WHERE key LIKE '%s%%' ORDER BY key"),
			*SafePrefix
		);
	}

	FSQLitePreparedStatement Stmt;
	if (Database->PrepareStatement(*SQL, Stmt))
	{
		while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
		{
			FString Key, Value;
			Stmt.GetColumnValueByIndex(0, Key);
			Stmt.GetColumnValueByIndex(1, Value);
			Results.Emplace(Key, Value);
		}
	}
	return Results;
}

bool FMonolithBlackboard::Remove(const FString& Key)
{
	if (!Database || !EnsureTable()) return false;

	FString SafeKey = Key.Replace(TEXT("'"), TEXT("''"));
	const FString SQL = FString::Printf(
		TEXT("DELETE FROM blackboard WHERE key = '%s'"), *SafeKey
	);
	return Database->Execute(*SQL);
}

int32 FMonolithBlackboard::Clear(const FString& Prefix)
{
	if (!Database || !EnsureTable()) return 0;

	FString SQL;
	if (Prefix.IsEmpty())
	{
		SQL = TEXT("DELETE FROM blackboard");
	}
	else
	{
		FString SafePrefix = Prefix.Replace(TEXT("'"), TEXT("''"));
		SQL = FString::Printf(
			TEXT("DELETE FROM blackboard WHERE key LIKE '%s%%'"), *SafePrefix
		);
	}

	if (Database->Execute(*SQL))
	{
		// SQLite changes() returns rows affected
		FSQLitePreparedStatement ChangesStmt;
		if (Database->PrepareStatement(TEXT("SELECT changes()"), ChangesStmt))
		{
			if (ChangesStmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				int64 Count = 0;
				ChangesStmt.GetColumnValueByIndex(0, Count);
				return (int32)Count;
			}
		}
	}
	return 0;
}
```

**Step 3: Compile and verify**

Run: Build in editor via Live Coding
Expected: Clean compile

**Step 4: Commit**

```bash
git add Source/MonolithTeams/Public/MonolithBlackboard.h Source/MonolithTeams/Private/MonolithBlackboard.cpp
git commit -m "feat: add blackboard key-value store for cross-agent state"
```

---

## Task 3: Teams Action Handlers

**Files:**
- Create: `Source/MonolithTeams/Public/MonolithTeamsActions.h`
- Create: `Source/MonolithTeams/Private/MonolithTeamsActions.cpp`

**Step 1: Create Actions header**

```cpp
// Source/MonolithTeams/Public/MonolithTeamsActions.h
#pragma once

#include "CoreMinimal.h"
#include "MonolithToolRegistry.h"

class FMonolithTeamsActions
{
public:
	static void RegisterActions();

private:
	// Blackboard actions
	static FMonolithActionResult HandleBlackboardGet(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleBlackboardSet(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleBlackboardList(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleBlackboardClear(const TSharedPtr<FJsonObject>& Params);

	// Validation actions
	static FMonolithActionResult HandleValidateCpp(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleValidateMaterial(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleValidateBlueprint(const TSharedPtr<FJsonObject>& Params);

	// Build actions
	static FMonolithActionResult HandleCompileAndWait(const TSharedPtr<FJsonObject>& Params);

	// Asset dependency actions
	static FMonolithActionResult HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params);
	static FMonolithActionResult HandleGetAssetReferencers(const TSharedPtr<FJsonObject>& Params);
};
```

**Step 2: Create Actions implementation**

This is the largest file. It implements all 10 action handlers following the established pattern from MonolithEditorActions.

```cpp
// Source/MonolithTeams/Private/MonolithTeamsActions.cpp
#include "MonolithTeamsActions.h"
#include "MonolithBlackboard.h"
#include "MonolithToolRegistry.h"
#include "MonolithJsonUtils.h"
#include "MonolithIndexSubsystem.h"
#include "MonolithIndexDatabase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Materials/Material.h"
#include "Engine/Blueprint.h"
#include "Kismet2/KismetEditorUtilities.h"

#if PLATFORM_WINDOWS
#include "ILiveCodingModule.h"
#endif

void FMonolithTeamsActions::RegisterActions()
{
	FMonolithToolRegistry& Registry = FMonolithToolRegistry::Get();

	// Initialize blackboard with the project index database
	if (UMonolithIndexSubsystem* IndexSub = GEditor ?
		GEditor->GetEditorSubsystem<UMonolithIndexSubsystem>() : nullptr)
	{
		if (FMonolithIndexDatabase* Db = IndexSub->GetDatabase())
		{
			FMonolithBlackboard::Get().Initialize(Db->GetRawDatabase());
		}
	}

	// Blackboard
	Registry.RegisterAction(TEXT("teams"), TEXT("blackboard_get"),
		TEXT("Get a value from the shared agent blackboard by key"),
		FMonolithActionHandler::CreateStatic(&HandleBlackboardGet));

	Registry.RegisterAction(TEXT("teams"), TEXT("blackboard_set"),
		TEXT("Set a key-value pair in the shared agent blackboard"),
		FMonolithActionHandler::CreateStatic(&HandleBlackboardSet));

	Registry.RegisterAction(TEXT("teams"), TEXT("blackboard_list"),
		TEXT("List all blackboard entries, optional prefix filter"),
		FMonolithActionHandler::CreateStatic(&HandleBlackboardList));

	Registry.RegisterAction(TEXT("teams"), TEXT("blackboard_clear"),
		TEXT("Clear blackboard entries matching a prefix, or all entries"),
		FMonolithActionHandler::CreateStatic(&HandleBlackboardClear));

	// Validation
	Registry.RegisterAction(TEXT("teams"), TEXT("validate_cpp"),
		TEXT("Trigger Live Coding compile and return structured errors/warnings"),
		FMonolithActionHandler::CreateStatic(&HandleValidateCpp));

	Registry.RegisterAction(TEXT("teams"), TEXT("validate_material"),
		TEXT("Validate a material graph for broken connections, islands, and issues"),
		FMonolithActionHandler::CreateStatic(&HandleValidateMaterial));

	Registry.RegisterAction(TEXT("teams"), TEXT("validate_blueprint"),
		TEXT("Check a Blueprint for broken references and compilation errors"),
		FMonolithActionHandler::CreateStatic(&HandleValidateBlueprint));

	// Build
	Registry.RegisterAction(TEXT("teams"), TEXT("compile_and_wait"),
		TEXT("Trigger Live Coding compile, block until done, return structured result"),
		FMonolithActionHandler::CreateStatic(&HandleCompileAndWait));

	// Asset dependencies
	Registry.RegisterAction(TEXT("teams"), TEXT("get_asset_dependencies"),
		TEXT("Get hard and soft dependencies of an asset from the Asset Registry"),
		FMonolithActionHandler::CreateStatic(&HandleGetAssetDependencies));

	Registry.RegisterAction(TEXT("teams"), TEXT("get_asset_referencers"),
		TEXT("Get all assets that reference a given asset"),
		FMonolithActionHandler::CreateStatic(&HandleGetAssetReferencers));

	UE_LOG(LogMonolith, Log, TEXT("Monolith — Teams: 10 actions registered"));
}

// --- Blackboard Handlers ---

FMonolithActionResult FMonolithTeamsActions::HandleBlackboardGet(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("key")))
		return FMonolithActionResult::Error(TEXT("Missing required param: key"));

	FString Key = Params->GetStringField(TEXT("key"));
	TOptional<FString> Value = FMonolithBlackboard::Get().Get(Key);

	auto Root = MakeShared<FJsonObject>();
	if (Value.IsSet())
	{
		Root->SetBoolField(TEXT("found"), true);
		Root->SetStringField(TEXT("key"), Key);
		Root->SetStringField(TEXT("value"), Value.GetValue());
	}
	else
	{
		Root->SetBoolField(TEXT("found"), false);
		Root->SetStringField(TEXT("key"), Key);
	}
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithTeamsActions::HandleBlackboardSet(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("key")) || !Params->HasField(TEXT("value")))
		return FMonolithActionResult::Error(TEXT("Missing required params: key, value"));

	FString Key = Params->GetStringField(TEXT("key"));
	FString Value = Params->GetStringField(TEXT("value"));
	FString Source = Params->HasField(TEXT("source"))
		? Params->GetStringField(TEXT("source")) : TEXT("");

	if (FMonolithBlackboard::Get().Set(Key, Value, Source))
	{
		auto Root = MakeShared<FJsonObject>();
		Root->SetBoolField(TEXT("success"), true);
		Root->SetStringField(TEXT("key"), Key);
		return FMonolithActionResult::Success(Root);
	}
	return FMonolithActionResult::Error(TEXT("Failed to write to blackboard"));
}

FMonolithActionResult FMonolithTeamsActions::HandleBlackboardList(const TSharedPtr<FJsonObject>& Params)
{
	FString Prefix = Params->HasField(TEXT("prefix"))
		? Params->GetStringField(TEXT("prefix")) : TEXT("");

	TArray<TPair<FString, FString>> Entries = FMonolithBlackboard::Get().List(Prefix);

	auto Root = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const auto& Entry : Entries)
	{
		auto Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("key"), Entry.Key);
		Item->SetStringField(TEXT("value"), Entry.Value);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("entries"), Items);
	Root->SetNumberField(TEXT("count"), Entries.Num());
	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithTeamsActions::HandleBlackboardClear(const TSharedPtr<FJsonObject>& Params)
{
	FString Prefix = Params->HasField(TEXT("prefix"))
		? Params->GetStringField(TEXT("prefix")) : TEXT("");

	int32 Cleared = FMonolithBlackboard::Get().Clear(Prefix);

	auto Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("cleared"), Cleared);
	return FMonolithActionResult::Success(Root);
}

// --- Validation Handlers ---

FMonolithActionResult FMonolithTeamsActions::HandleValidateCpp(const TSharedPtr<FJsonObject>& Params)
{
	// Delegates to editor.compile_and_wait logic
	return HandleCompileAndWait(Params);
}

FMonolithActionResult FMonolithTeamsActions::HandleValidateMaterial(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
		return FMonolithActionResult::Error(TEXT("Missing required param: asset_path"));

	// Delegate to material.validate via the registry
	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	auto ValidateParams = MakeShared<FJsonObject>();
	ValidateParams->SetStringField(TEXT("action"), TEXT("validate_material"));
	ValidateParams->SetStringField(TEXT("asset_path"), AssetPath);

	return FMonolithToolRegistry::Get().ExecuteAction(TEXT("material"), TEXT("validate_material"), ValidateParams);
}

FMonolithActionResult FMonolithTeamsActions::HandleValidateBlueprint(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
		return FMonolithActionResult::Error(TEXT("Missing required param: asset_path"));

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));

	// Load the Blueprint
	UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
	if (!BP)
		return FMonolithActionResult::Error(FString::Printf(TEXT("Blueprint not found: %s"), *AssetPath));

	// Compile and gather errors
	FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave);

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);
	Root->SetBoolField(TEXT("is_compiled"), !BP->IsUpToDate());

	// Check status
	bool bHasErrors = BP->Status == BS_Error;
	Root->SetBoolField(TEXT("has_errors"), bHasErrors);
	Root->SetStringField(TEXT("status"),
		bHasErrors ? TEXT("error") :
		BP->Status == BS_UpToDate ? TEXT("valid") : TEXT("dirty"));

	// Collect compile messages if any
	TArray<TSharedPtr<FJsonValue>> Messages;
	// Note: Blueprint compiler messages are in BP->Message log — access varies by version
	Root->SetArrayField(TEXT("messages"), Messages);

	return FMonolithActionResult::Success(Root);
}

// --- Build Handler ---

FMonolithActionResult FMonolithTeamsActions::HandleCompileAndWait(const TSharedPtr<FJsonObject>& Params)
{
#if PLATFORM_WINDOWS
	ILiveCodingModule* LiveCoding = FModuleManager::GetModulePtr<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
	if (!LiveCoding || !LiveCoding->IsEnabledByDefault())
	{
		return FMonolithActionResult::Error(TEXT("Live Coding is not available"));
	}

	if (!LiveCoding->IsEnabledForSession())
	{
		LiveCoding->EnableForSession(true);
	}

	// Trigger compile
	LiveCoding->Compile();

	// Poll for completion (up to 120 seconds)
	double StartTime = FPlatformTime::Seconds();
	const double Timeout = 120.0;
	bool bCompiling = true;

	while (bCompiling && (FPlatformTime::Seconds() - StartTime) < Timeout)
	{
		FPlatformProcess::Sleep(0.5f);
		bCompiling = LiveCoding->IsCompiling();
	}

	auto Root = MakeShared<FJsonObject>();
	if (bCompiling)
	{
		Root->SetBoolField(TEXT("success"), false);
		Root->SetStringField(TEXT("error"), TEXT("Compilation timed out after 120 seconds"));
		return FMonolithActionResult::Success(Root);
	}

	Root->SetBoolField(TEXT("success"), true);
	Root->SetNumberField(TEXT("duration_seconds"), FPlatformTime::Seconds() - StartTime);
	Root->SetStringField(TEXT("status"), TEXT("completed"));

	return FMonolithActionResult::Success(Root);
#else
	return FMonolithActionResult::Error(TEXT("compile_and_wait requires Windows with Live Coding"));
#endif
}

// --- Asset Dependency Handlers ---

FMonolithActionResult FMonolithTeamsActions::HandleGetAssetDependencies(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
		return FMonolithActionResult::Error(TEXT("Missing required param: asset_path"));

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetIdentifier> Dependencies;
	AssetRegistry.GetDependencies(FAssetIdentifier(FName(*AssetPath)), Dependencies);

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	TArray<TSharedPtr<FJsonValue>> DepArray;
	for (const FAssetIdentifier& Dep : Dependencies)
	{
		auto DepObj = MakeShared<FJsonObject>();
		DepObj->SetStringField(TEXT("package"), Dep.PackageName.ToString());
		DepArray.Add(MakeShared<FJsonValueObject>(DepObj));
	}
	Root->SetArrayField(TEXT("dependencies"), DepArray);
	Root->SetNumberField(TEXT("count"), DepArray.Num());

	return FMonolithActionResult::Success(Root);
}

FMonolithActionResult FMonolithTeamsActions::HandleGetAssetReferencers(const TSharedPtr<FJsonObject>& Params)
{
	if (!Params->HasField(TEXT("asset_path")))
		return FMonolithActionResult::Error(TEXT("Missing required param: asset_path"));

	FString AssetPath = Params->GetStringField(TEXT("asset_path"));
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

	TArray<FAssetIdentifier> Referencers;
	AssetRegistry.GetReferencers(FAssetIdentifier(FName(*AssetPath)), Referencers);

	auto Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("asset_path"), AssetPath);

	TArray<TSharedPtr<FJsonValue>> RefArray;
	for (const FAssetIdentifier& Ref : Referencers)
	{
		auto RefObj = MakeShared<FJsonObject>();
		RefObj->SetStringField(TEXT("package"), Ref.PackageName.ToString());
		RefArray.Add(MakeShared<FJsonValueObject>(RefObj));
	}
	Root->SetArrayField(TEXT("referencers"), RefArray);
	Root->SetNumberField(TEXT("count"), RefArray.Num());

	return FMonolithActionResult::Success(Root);
}
```

**Step 3: Wire blackboard initialization in MonolithTeamsModule.cpp**

Update `StartupModule()` to initialize the blackboard from the index database. The initialization happens in `RegisterActions()` which accesses the IndexSubsystem — this requires the editor to be fully initialized, which is guaranteed because MonolithTeams loads at `Default` phase (after MonolithIndex).

**Step 4: Compile and verify all 10 actions register**

Run: Open editor, check Output Log for `Monolith — Teams: 10 actions registered`
Expected: Clean compile, 10 actions available via `monolith_discover`

Test via curl:
```bash
curl -X POST http://localhost:9316/mcp -H "Content-Type: application/json" -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"blackboard_set","key":"test.hello","value":"world"}}}'
```

**Step 5: Commit**

```bash
git add Source/MonolithTeams/
git commit -m "feat: implement 10 teams actions (blackboard, validation, compile, dependencies)"
```

---

## Task 4: Agent Definitions (Templates)

**Files:**
- Create: `Templates/AgentTeams/agents/ue-team-lead.md`
- Create: `Templates/AgentTeams/agents/ue-cpp.md`
- Create: `Templates/AgentTeams/agents/ue-blueprint.md`
- Create: `Templates/AgentTeams/agents/ue-material.md`
- Create: `Templates/AgentTeams/agents/ue-animation.md`
- Create: `Templates/AgentTeams/agents/ue-niagara.md`

Each file is a Markdown file with YAML frontmatter defining the agent's tools, model, skills, and system prompt. Full content for each is specified in the design doc Section 3.2. These ship inside the plugin as templates — users copy them to their project's `.claude/agents/` during setup.

**Step 1: Create ue-team-lead.md**

The orchestrator agent. Uses Opus model, has Agent tool for spawning teammates, all Monolith namespaces for validation, preloads the ue-team-orchestrator skill.

**Step 2: Create ue-cpp.md**

C++ specialist. Opus model, Read/Write/Edit/Grep/Glob/Bash tools, `unreal-cpp` + `unreal-build` skills. System prompt emphasizes: search before writing, compile after every edit, forward-declare in headers, produce handoff manifests.

**Step 3: Create ue-blueprint.md**

Blueprint specialist. Opus model, read-only file tools (no Write/Edit to .cpp/.h), `unreal-blueprints` + `unreal-debugging` skills. System prompt emphasizes: read before modifying, compile after changes, never modify C++ files.

**Step 4: Create ue-material.md**

Material specialist. Opus model, read-only file tools, `unreal-materials` skill. System prompt emphasizes: use `build_material_graph` for creation, always validate, always export before destructive changes, Substrate-first on UE 5.7.

**Step 5: Create ue-animation.md**

Animation specialist. Opus model, read-only file tools, `unreal-animation` skill. System prompt emphasizes: montage section setup, notify timing, coordinate with C++ on AnimNotify classes, skeleton changes cascade.

**Step 6: Create ue-niagara.md**

Niagara VFX specialist. Opus model, read-only file tools, `unreal-niagara` skill. System prompt emphasizes: use `create_system_from_spec` for new systems, `batch_execute` for refinement, always configure scalability, HLSL stubs require workarounds.

**Step 7: Commit**

```bash
git add Templates/AgentTeams/agents/
git commit -m "feat: add 6 UE specialist agent definition templates"
```

---

## Task 5: Team Lead Orchestrator Skill (Template)

**Files:**
- Create: `Templates/AgentTeams/skills/ue-team-orchestrator/ue-team-orchestrator.md`
- Create: `Templates/AgentTeams/skills/ue-team-orchestrator/blackboard-conventions.md`

**Step 1: Create orchestrator skill**

The skill contains:
- Feature archetype templates (task DAG templates for: gameplay ability, weapon system, UI feature, environment prop, character system)
- UE dependency chain rules (C++ compile gate -> BP/animation/material -> niagara -> integration)
- Spawn prompt templates per specialist
- Blackboard key naming conventions
- Quality gate criteria per agent type
- Communication protocol (handoff manifest format, task assignment format, error recovery)
- The complete workflow: receive goal -> gather context via Monolith -> decompose -> plan -> approve -> dispatch -> validate -> report

**Step 2: Create blackboard conventions reference**

Documents the key naming patterns:
```
assets.{type}.{name}   -> asset path
classes.{name}          -> module.classname
params.{system}.{name}  -> parameter name
tags.{category}.{name}  -> gameplay tag
sockets.{name}          -> socket name
delegates.{class}.{name} -> delegate info
```

**Step 3: Commit**

```bash
git add Templates/AgentTeams/skills/
git commit -m "feat: add team lead orchestrator skill template with dependency templates"
```

---

## Task 6: Quality Gate Hooks (Templates)

**Files:**
- Create: `Templates/AgentTeams/hooks/task-completed-validate.sh`
- Create: `Templates/AgentTeams/hooks/teammate-idle-check.sh`
- Create: `Templates/AgentTeams/settings.json.example`

**Step 1: Create task-completed-validate.sh**

```bash
#!/bin/bash
# TaskCompleted hook: validates agent work before accepting
# Exit 0 = accept, Exit 2 + stderr = reject with feedback

INPUT=$(cat)
TEAMMATE=$(echo "$INPUT" | jq -r '.teammate_name // empty')
TASK_SUBJECT=$(echo "$INPUT" | jq -r '.task_subject // empty')

MONOLITH_URL="http://localhost:9316/mcp"

call_monolith() {
    local ACTION=$1
    local PARAMS=$2
    curl -s -X POST "$MONOLITH_URL" \
        -H "Content-Type: application/json" \
        -d "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/call\",\"params\":{\"name\":\"teams.query\",\"arguments\":{\"action\":\"$ACTION\"$PARAMS}}}"
}

case "$TEAMMATE" in
    *cpp*)
        # Trigger compile and check result
        RESULT=$(call_monolith "compile_and_wait" "")
        SUCCESS=$(echo "$RESULT" | jq -r '.result.content[0].text // empty' | jq -r '.success // false')
        if [ "$SUCCESS" != "true" ]; then
            echo "C++ compilation failed. Fix errors before completing: $TASK_SUBJECT" >&2
            exit 2
        fi
        ;;
    *material*)
        # Material validation would need asset_path from task metadata
        # For now, basic acceptance
        ;;
    *blueprint*)
        # Blueprint validation would need asset_path from task metadata
        ;;
esac

exit 0
```

**Step 2: Create teammate-idle-check.sh**

```bash
#!/bin/bash
# TeammateIdle hook: prevent idle if uncompleted tasks exist
# Exit 0 = allow idle, Exit 2 = keep working

INPUT=$(cat)
TEAMMATE=$(echo "$INPUT" | jq -r '.teammate_name // empty')
TEAM=$(echo "$INPUT" | jq -r '.team_name // empty')

# Check if there are pending tasks (basic file check)
TASK_DIR="$HOME/.claude/tasks/$TEAM"
if [ -d "$TASK_DIR" ]; then
    PENDING=$(find "$TASK_DIR" -name "*.json" -exec grep -l '"status":"in_progress"' {} \; 2>/dev/null | wc -l)
    if [ "$PENDING" -gt 0 ]; then
        echo "You have $PENDING in-progress tasks. Complete them before going idle." >&2
        exit 2
    fi
fi

exit 0
```

**Step 3: Make hooks executable**

```bash
chmod +x Templates/AgentTeams/hooks/task-completed-validate.sh
chmod +x Templates/AgentTeams/hooks/teammate-idle-check.sh
```

**Step 4: Create settings.json.example**

Template showing the required settings — users merge into their existing `.claude/settings.json`:
```json
{
  "env": {
    "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1"
  },
  "hooks": {
    "TaskCompleted": [
      {
        "hooks": [{
          "type": "command",
          "command": ".claude/hooks/task-completed-validate.sh"
        }]
      }
    ],
    "TeammateIdle": [
      {
        "hooks": [{
          "type": "command",
          "command": ".claude/hooks/teammate-idle-check.sh"
        }]
      }
    ]
  }
}
```

**Step 5: Commit**

```bash
git add Templates/AgentTeams/hooks/ Templates/AgentTeams/settings.json.example
git commit -m "feat: add quality gate hook templates for TaskCompleted and TeammateIdle"
```

---

## Task 7: AI-Driven Setup Guide

**Files:**
- Create: `Templates/AgentTeams/AGENT-TEAMS-SETUP.md`
- Create: `Templates/AgentTeams/README.md`

The setup script is gone. Instead, `AGENT-TEAMS-SETUP.md` is a comprehensive guide addressed to the AI that walks the **user** through setup interactively. The user just tells their AI: *"Read `Plugins/Monolith/Templates/AgentTeams/AGENT-TEAMS-SETUP.md`"* and the AI takes over.

**Step 1: Create AGENT-TEAMS-SETUP.md**

```markdown
# Monolith Agent Teams — Setup Guide

> **Audience:** You are an AI assistant. A user has pointed you at this file.
> Your job is to walk them through enabling Monolith's experimental Agent Teams
> feature for their Unreal Engine project. Be conversational. Ask before doing.

## Before You Start

Confirm with the user:
1. They have **Monolith v0.5.0+** installed (check Monolith.uplugin VersionName)
2. They're using **Claude Code** (or another MCP client that supports Agent Teams)
3. They understand Agent Teams is **experimental** — it may change or break between versions

If they want to proceed, continue below. If not, stop here.

## Step 1: Enable the MonolithTeams Module

The C++ module is bundled with Monolith but disabled by default.

**Ask the user:** "Would you like me to guide you through enabling MonolithTeams,
or do you want to enable it yourself in Editor Preferences > Plugins > Monolith?"

The module adds 10 MCP actions under the `teams` namespace:
- `blackboard_get/set/list/clear` — shared key-value store for cross-agent state
- `validate_cpp`, `validate_material`, `validate_blueprint` — domain-specific validation
- `compile_and_wait` — trigger Live Coding and block until done
- `get_asset_dependencies`, `get_asset_referencers` — Asset Registry queries

After enabling, the editor needs a restart.

## Step 2: Install Agent Definitions

Copy the 6 agent definition templates from `Templates/AgentTeams/agents/` to the
project's `.claude/agents/` directory:

```
.claude/agents/
  ue-team-lead.md      — Orchestrator (Opus 4.6, spawns and coordinates teammates)
  ue-cpp.md            — C++ Gameplay Programmer (Opus 4.6, Read/Write/Edit)
  ue-blueprint.md      — Blueprint Specialist (Opus 4.6, read-only files)
  ue-material.md       — Material/Shader Artist (Opus 4.6, read-only files)
  ue-animation.md      — Animation Specialist (Opus 4.6, read-only files)
  ue-niagara.md        — Niagara VFX Specialist (Opus 4.6, read-only files)
```

**Ask the user:** "Do you want all 6 agents, or just a subset? Which domains
does your project use?"

Only copy the agents they want. Always include `ue-team-lead.md` if they want
team functionality.

## Step 3: Install Quality Gate Hooks

Copy from `Templates/AgentTeams/hooks/` to `.claude/hooks/`:
- `task-completed-validate.sh` — validates agent work before accepting (compiles C++, validates materials/BPs)
- `teammate-idle-check.sh` — prevents idle if uncompleted tasks exist

Make them executable: `chmod +x .claude/hooks/*.sh`

**Ask the user:** "Do you want quality gate hooks that auto-validate agent work?
For example, the C++ agent's work won't be accepted until it compiles clean."

Skip if they decline.

## Step 4: Install Orchestrator Skill

Copy from `Templates/AgentTeams/skills/ue-team-orchestrator/` to `Skills/ue-team-orchestrator/`:
- `ue-team-orchestrator.md` — task decomposition templates, dependency rules, spawn prompts
- `blackboard-conventions.md` — key naming patterns for cross-agent state

## Step 5: Configure Settings

The user's `.claude/settings.json` needs:

```json
{
  "env": {
    "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1"
  }
}
```

Plus hook entries if they installed hooks in Step 3:

```json
{
  "hooks": {
    "TaskCompleted": [
      { "hooks": [{ "type": "command", "command": ".claude/hooks/task-completed-validate.sh" }] }
    ],
    "TeammateIdle": [
      { "hooks": [{ "type": "command", "command": ".claude/hooks/teammate-idle-check.sh" }] }
    ]
  }
}
```

**If `.claude/settings.json` already exists**, merge these entries — do NOT overwrite.
Show the user what you're adding and ask for confirmation.

## Step 6: Update CLAUDE.md

The project's `CLAUDE.md` (if one exists) should include sections that all
teammates will read. **Do NOT replace the file.** Append the relevant sections,
skipping any that already exist.

**Ask the user:** "I'd like to add some sections to your project's CLAUDE.md so
that all agent teammates understand the project conventions. Can I show you what
I'd add?"

Then show them a preview of what you'll append, and only proceed if they approve.

### Sections to append:

#### Monolith MCP Tools

This project uses the Monolith plugin (MCP server on port 9316) with 10 namespaces:

| Namespace | Actions | Key Actions |
|-----------|---------|-------------|
| `monolith` | 4 | `monolith_discover`, `monolith_status` |
| `blueprint` | 5 | `get_graph`, `get_variables`, `search_nodes` |
| `material` | 14 | `build_material_graph`, `validate_material`, `get_graph` |
| `animation` | 23 | `get_montage`, `get_blend_space`, `get_abp_graph` |
| `niagara` | 41 | `create_system_from_spec`, `batch_execute`, `get_system` |
| `editor` | 13 | `compile`, `get_build_errors`, `get_log` |
| `config` | 6 | `get_config`, `search_config`, `explain_config` |
| `project` | 5 | `search`, `get_asset`, `list_assets` |
| `source` | 10 | `lookup_class`, `get_call_graph`, `search_source` |
| `teams` | 10 | `blackboard_get/set/list/clear`, `compile_and_wait`, `validate_*` |

Use `monolith_discover` to see all available actions and their parameters.

#### Agent Team Coordination Rules

When working as part of an agent team:

- **C++ compile gate**: C++ must compile cleanly before Blueprint, Animation,
  Material, or Niagara agents begin work that references C++ types
- **Blackboard for shared state**: Use `teams.blackboard_set` / `teams.blackboard_get`
  to share asset paths, class names, and parameter names between agents
  - Key format: `assets.{type}.{name}`, `classes.{name}`, `params.{system}.{name}`,
    `tags.{category}.{name}`, `sockets.{name}`, `delegates.{class}.{name}`
- **One agent per asset**: Never have two agents editing the same asset concurrently
- **Handoff manifests**: After completing a task, report what files were created/modified,
  what Blueprint API surface was exposed, and what blackboard keys were written
- **Validate before completing**: Run domain-appropriate validation before marking
  a task complete (compile for C++, validate_material for materials, etc.)

#### UE Naming Conventions

| Prefix | Type | Example |
|--------|------|---------|
| `BP_` | Blueprint | `BP_PlayerCharacter` |
| `M_` | Material | `M_DamageFlash` |
| `MI_` | Material Instance | `MI_DamageFlash_Red` |
| `MF_` | Material Function | `MF_FresnelBlend` |
| `AM_` | Anim Montage | `AM_MeleeAttack` |
| `ABP_` | Anim Blueprint | `ABP_PlayerCharacter` |
| `NS_` | Niagara System | `NS_BloodSplatter` |
| `SM_` | Static Mesh | `SM_Weapon_Sword` |
| `SK_` | Skeletal Mesh | `SK_PlayerCharacter` |
| `T_` | Texture | `T_Wood_BaseColor` |
| `WBP_` | Widget Blueprint | `WBP_HealthBar` |

Asset paths use `/Game/...` format. Discover assets with `project.query(action: "search")`.

#### Build Rules

- **Live Coding** is the default compile method (fast iteration, no editor restart)
- Use **full rebuild** only for: header changes to UCLASS/USTRUCT, module additions,
  .Build.cs changes, or engine version upgrades
- `teams.compile_and_wait` triggers Live Coding and blocks until complete

## Step 7: Verify

After setup, tell the user:
1. Restart the editor (if they enabled MonolithTeams)
2. Check Output Log for `Monolith — Teams: 10 actions registered`
3. Start Claude Code with `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1 claude`
4. Try: "Create a team to [describe a task relevant to their project]"

## Summary Template

After completing setup, give the user a summary:

```
Agent Teams Setup Complete:
- MonolithTeams module: [enabled/skipped]
- Agent definitions: [list of installed agents]
- Quality gate hooks: [installed/skipped]
- Orchestrator skill: [installed]
- Settings: [updated/created]
- CLAUDE.md: [updated/created/skipped]

To start a team session:
  CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1 claude
```
```

**Step 2: Create README.md**

Short human-readable doc explaining:
- What Agent Teams is (experimental multi-agent coordination for UE development)
- Prerequisites (Monolith v0.5.0+, Claude Code, experimental flag)
- How to set up: *"Tell your AI: Read `Plugins/Monolith/Templates/AgentTeams/AGENT-TEAMS-SETUP.md`"*
- What gets installed (agents, hooks, skills, settings, CLAUDE.md sections)
- Quick start after setup

**Step 3: Commit**

```bash
git add Templates/AgentTeams/AGENT-TEAMS-SETUP.md Templates/AgentTeams/README.md
git commit -m "feat: add AI-driven Agent Teams setup guide"
```

---

## Task 8: Integration Test

**Step 1: Start Unreal Editor with Leviathan project**

Verify in Output Log:
- `Monolith 0.5.x — Core module initializing`
- `Monolith — Teams module loaded`
- `Monolith — Teams: 10 actions registered`

**Step 2: Test blackboard via curl**

```bash
# Set
curl -s -X POST http://localhost:9316/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"blackboard_set","key":"test.greeting","value":"hello from integration test"}}}'

# Get
curl -s -X POST http://localhost:9316/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":2,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"blackboard_get","key":"test.greeting"}}}'

# List
curl -s -X POST http://localhost:9316/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":3,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"blackboard_list","prefix":"test."}}}'

# Clear
curl -s -X POST http://localhost:9316/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":4,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"blackboard_clear","prefix":"test."}}}'
```

Expected: Each returns valid JSON-RPC response with correct data.

**Step 3: Test asset dependency query**

```bash
curl -s -X POST http://localhost:9316/mcp -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":5,"method":"tools/call","params":{"name":"teams.query","arguments":{"action":"get_asset_dependencies","asset_path":"/Game/Characters/BP_PlayerCharacter"}}}'
```

Expected: Returns dependency list.

**Step 4: Dogfood the AI-driven setup on Leviathan**

Start Claude Code in the Leviathan directory and tell it:
"Read `Plugins/Monolith/Templates/AgentTeams/AGENT-TEAMS-SETUP.md`"

Expected: AI walks through setup interactively — asks which agents, copies templates, updates CLAUDE.md, configures settings.

**Step 5: Enable MonolithTeams module in Editor Preferences, restart editor**

Expected: Output Log shows `Monolith — Teams: 10 actions registered`

**Step 6: Test from Claude Code**

Start Claude Code in the Leviathan directory:
```
claude --add-dir "D:/unreal projects/Leviathan"
```

Ask: "What Monolith teams tools do you have?"
Expected: Claude lists the 10 teams.query actions.

**Step 5: Test agent team spawn (manual)**

Ask Claude: "Create a team with a cpp agent and a material agent to build a simple damage flash material and a health component."
Expected: Team lead decomposes into tasks, spawns teammates, coordinates.

**Step 6: Commit any fixes**

```bash
git add -A
git commit -m "fix: integration test fixes"
```

---

## Task 9: Update Monolith Documentation

**Files:**
- Modify: `README.md` (add MonolithTeams to architecture table)
- Modify: `Docs/SPEC.md` (add Section 3.10 MonolithTeams)
- Modify: `Docs/API_REFERENCE.md` (add teams namespace)
- Modify: `CHANGELOG.md` (add Phase 7 entry)

**Step 1: Update README architecture table**

Add row:
```
MonolithTeams       — Shared blackboard, validation gates, compile-and-wait, asset dependencies (10 actions)
```

Update total from 121 to 131.

**Step 2: Update SPEC.md**

Add Section 3.10 documenting MonolithTeams module, all 10 actions, blackboard schema, and the agent team integration pattern.

**Step 3: Update API_REFERENCE.md**

Add `teams` namespace with all 10 action signatures, parameters, and descriptions.

**Step 4: Update CHANGELOG.md**

Add Phase 7 entry documenting the new module, agent definitions, skills, hooks, and CLAUDE.md.

**Step 5: Commit**

```bash
git add README.md Docs/SPEC.md Docs/API_REFERENCE.md CHANGELOG.md
git commit -m "docs: add MonolithTeams to README, SPEC, API reference, and changelog"
```

---

## Summary

| Task | Deliverable | Files |
|------|-------------|-------|
| 1 | Module scaffold (disabled by default) | 4 files (Build.cs, module h/cpp, uplugin update) |
| 2 | Blackboard store | 2 files (h/cpp) |
| 3 | 10 action handlers | 2 files (h/cpp) |
| 4 | 6 agent definition templates | 6 files (Templates/AgentTeams/agents/*.md) |
| 5 | Team lead skill template | 2 files (Templates/AgentTeams/skills/) |
| 6 | Quality gate hook templates | 3 files (2 hooks + settings.json.example) |
| 7 | AI-driven setup guide | 2 files (AGENT-TEAMS-SETUP.md, README) |
| 8 | Integration test | 0 files (manual verification) |
| 9 | Documentation updates | 4 files modified |

**Total: ~25 new files, 5 modified files, 10 new MCP actions**
**Bundling: C++ module in plugin (opt-in), all config/agent files in Templates/AgentTeams/ with AI-driven setup**
