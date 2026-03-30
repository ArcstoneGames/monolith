# Optional Module Architecture Specification

**Status:** Approved Design | **Date:** 2026-03-27 | **Plugin:** Monolith v0.11.0+ | **Engine:** UE 5.7

---

## 1. Executive Summary

Monolith is an editor plugin distributed both as source (GitHub) and as precompiled binaries (release zips). Some users want Monolith to integrate with third-party marketplace plugins (GAS via GBAPlugin, ComboGraph, etc.) that may or may not be installed in their project.

**The problem:** A compiled DLL that links against a missing DLL will fail to load entirely. UE shows an error dialog and the whole plugin is dead. There is no per-module `bOptional` flag, no `SoftDependencies`, and `ConditionallyLoadedModuleNames` was removed in UE 5.7. You cannot gracefully degrade at the module level within a single `.uplugin`.

**The solution:** A hybrid approach using two patterns for the two distribution modes:

- **Source users (GitHub):** `Build.cs` uses `Directory.Exists()` to detect the third-party plugin at compile time. If found, it adds the dependency and defines a preprocessor symbol (`WITH_GBA`, etc.). Code behind `#if` guards compiles in. If not found, the guards strip it out. One `.uplugin`, no extra DLLs.

- **Binary users (releases):** Each optional integration ships as a **separate satellite `.uplugin`** (e.g., `MonolithGBA.uplugin`). It has a hard dependency on both `Monolith` and the target plugin. If the target plugin isn't enabled, UE silently skips loading the satellite. No error dialogs, no missing DLLs.

The module source code is identical in both modes. The only difference is packaging: one `.uplugin` with conditional compilation vs. separate `.uplugin` files with hard deps.

> **Platform note:** This specification targets Windows (Win64). Future Mac/Linux support would require updates to filesystem detection paths and packaging scripts.

---

## 2. The Two Distribution Modes

### 2.1 Source Distribution (GitHub)

Users clone the repo into their `Plugins/` folder and compile from source. UBT runs `Build.cs` at compile time, where we can probe the filesystem.

**Pattern:** `Directory.Exists()` in `Build.cs` + `#if WITH_*` guards in C++.

**Why it works:** UBT evaluates `Build.cs` as C# at build time. We can check whether a third-party plugin's `Source/` directory exists on disk. If it does, we add the module dependency and define a preprocessor macro. The C++ code compiles with or without the dependency thanks to `#if` guards.

**Why we use this for source:** It's zero-friction. Users don't create extra plugin folders or manage satellite `.uplugin` files. They clone Monolith, enable their marketplace plugins, and build. Everything Just Works.

### 2.2 Binary Distribution (Releases)

Users download a zip containing precompiled DLLs (`Binaries/Win64/`). There is no `Build.cs` evaluation -- the DLLs are already linked.

**Pattern:** Separate `.uplugin` per optional dependency with hard `PrivateDependencyModuleNames`.

**Why it works:** UE's plugin system checks whether a plugin's dependencies are satisfied before loading it. If a `.uplugin` declares `"Plugins": [{"Name": "GBAPlugin", "Enabled": true}]` and GBAPlugin isn't installed, UE simply doesn't load that plugin. No error dialog, no crash. The DLL sits on disk unused.

**Why we use this for binary:** It's the only clean option. A DLL that was linked against `GBAPlugin` modules will fail to load if those DLLs are missing. By putting optional integrations in separate `.uplugin` files, each DLL either loads fully or doesn't load at all.

### 2.3 Why the Module Code Stays Identical

Consider `MonolithGBA` -- a module that registers `gba_query` actions:

```cpp
// MonolithGBAModule.cpp — same file in both modes
void FMonolithGBAModule::StartupModule()
{
    if (!GetDefault<UMonolithSettings>()->bEnableGBA) return;
    FMonolithGBAActions::RegisterActions(FMonolithToolRegistry::Get());
}

void FMonolithGBAModule::ShutdownModule()
{
    FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("gba"));
}
```

In source mode, this module lives inside `Monolith.uplugin` and its `Build.cs` conditionally adds `GBAPlugin` dependencies. In binary mode, this same module lives inside `MonolithGBA.uplugin` which hard-depends on `GBAPlugin`. The C++ doesn't know or care which mode it's in.

---

## 3. Source Distribution Pattern (Current)

### 3.1 Build.cs Implementation

```csharp
// Source/MonolithGBA/MonolithGBA.Build.cs
using UnrealBuildTool;
using System.IO;

public class MonolithGBA : ModuleRules
{
    public MonolithGBA(ReadOnlyTargetRules Target) : base(Target)
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
            "Json",
            "JsonUtilities"
        });

        // Probe for GBAPlugin source on disk
        // Marketplace plugins use obfuscated directory names (e.g., "Gameplaya1dbec2bf155V7")
        // so we search with a wildcard pattern rather than exact path matching.
        bool bHasGBA = false;

        // Check project Plugins/ folder (manual install or symlink)
        string ProjectPluginsDir = Path.Combine(
            Target.ProjectFile.Directory.FullName, "Plugins");
        if (Directory.Exists(ProjectPluginsDir))
        {
            // Check both exact name and wildcard for obfuscated dirs
            bHasGBA = Directory.Exists(Path.Combine(ProjectPluginsDir, "BlueprintAttributes"))
                || Directory.GetDirectories(ProjectPluginsDir, "Gameplaya*").Length > 0;
        }

        // Check Engine Marketplace folder (Fab/launcher install — uses obfuscated names)
        if (!bHasGBA)
        {
            string EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
            string MarketplaceDir = Path.Combine(EngineDir, "Plugins", "Marketplace");
            if (Directory.Exists(MarketplaceDir))
            {
                // GBA's obfuscated dir starts with "Gameplaya" — match that prefix
                bHasGBA = Directory.GetDirectories(MarketplaceDir, "Gameplaya*").Length > 0;
            }
            // Also check engine Plugins/ root (some Fab installs go here)
            if (!bHasGBA)
            {
                string EnginePluginsDir = Path.Combine(EngineDir, "Plugins");
                bHasGBA = Directory.Exists(Path.Combine(EnginePluginsDir, "BlueprintAttributes"));
            }
        }

        if (bHasGBA)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "GBAPlugin",          // Main GBA module
                "GBAPluginEditor"     // Editor utilities if needed
            });

            PublicDefinitions.Add("WITH_GBA=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_GBA=0");
        }
    }
}
```

**Key points:**
- `PublicDefinitions` (not `PrivateDefinitions`) so headers can use the guard too
- Always define the symbol to either 0 or 1 -- avoids "undefined macro" warnings with `#if`
- Check both project `Plugins/` and engine `Plugins/Marketplace/` paths (Fab installs go to engine)

> **Incremental Build Warning:** UBT caches Build.cs evaluation results aggressively. If a user installs or removes GBAPlugin after a previous build, UBT may not re-evaluate `MonolithGBA.Build.cs` because no MonolithGBA source files changed. The `WITH_GBA` define can be stale. **Users must do a full rebuild** (delete `Intermediate/` or Build > Rebuild Solution) after installing or removing third-party plugins. This is a known UBT limitation shared by all plugins that use the `Directory.Exists()` pattern.

### 3.2 C++ Preprocessor Guards

```cpp
// MonolithGBAActions.cpp

#if WITH_GBA

#include "SomeGBAHeader.h"  // UBT adds Public/ to include paths automatically

void FMonolithGBAActions::RegisterActions(FMonolithToolRegistry& Registry)
{
    Registry.RegisterAction(
        TEXT("gba"), TEXT("list_abilities"),
        TEXT("List all gameplay abilities in the project"),
        FMonolithActionHandler::CreateStatic(&FMonolithGBAActions::HandleListAbilities)
    );
    // ... more actions
    // NOTE: Always use CreateStatic, never CreateRaw(this, ...) for action handlers.
    // The registry copies the delegate and may execute it after releasing its lock.
    // CreateRaw captures a raw pointer that can dangle if the object is destroyed.
}

FMonolithActionResult FMonolithGBAActions::HandleListAbilities(
    const TSharedPtr<FJsonObject>& Params)
{
    // Implementation that calls GBA APIs
}

#else

// Stub — module loads but registers nothing
void FMonolithGBAActions::RegisterActions(FMonolithToolRegistry& Registry)
{
    UE_LOG(LogMonolith, Log, TEXT("Monolith — GBA integration disabled (GBAPlugin not found)"));
}

#endif
```

### 3.3 What CAN and CANNOT Go Inside #if Blocks

**CAN go inside `#if WITH_*`:**
- `#include` directives for third-party headers
- Function bodies and implementations
- Local variable declarations
- Forward declarations of third-party types
- Static helper functions
- Entire `.cpp` files (wrap the whole file)

**CANNOT go inside `#if WITH_*`:**
- `UPROPERTY()` declarations -- UHT parses before preprocessor, generates reflection code unconditionally
- `UCLASS()`, `USTRUCT()`, `UENUM()` declarations
- `UFUNCTION()` declarations
- `GENERATED_BODY()` macros
- Any reflected type that UHT needs to see

**Workaround for typed members:**

```cpp
// BAD -- UHT will choke
UPROPERTY()
#if WITH_GBA
UGBAAbilityComponent* AbilityComp;
#endif

// GOOD -- use void* or TWeakObjectPtr with manual casting
#if WITH_GBA
// Non-reflected pointer, managed manually
class UGBAAbilityComponent* CachedAbilityComp = nullptr;
#endif

// GOOD -- use a separate non-reflected struct
#if WITH_GBA
struct FGBAIntegrationData
{
    UGBAAbilityComponent* AbilityComp = nullptr;
};
TUniquePtr<FGBAIntegrationData> GBAData;
#endif
```

For Monolith's use case (editor-only action handlers that query and report), this limitation rarely matters. The action handler functions aren't reflected -- they're bound via delegates.

### 3.4 Module Registration in .uplugin (Source Mode)

In source mode, the optional module is listed in `Monolith.uplugin` like any other module:

```json
{
    "Name": "MonolithGBA",
    "Type": "Editor",
    "LoadingPhase": "Default"
}
```

This is safe because the module itself compiles to a stub when `WITH_GBA=0`. The DLL exists and loads, it just doesn't register any actions.

---

## 4. Binary Distribution Pattern (Future)

### 4.1 Separate .uplugin Structure

```
Monolith/                          # Main plugin (always ships)
    Monolith.uplugin
    Binaries/Win64/...
    Source/...

MonolithGBA/                       # Satellite plugin (optional)
    MonolithGBA.uplugin
    Binaries/Win64/...
    Source/MonolithGBA/...          # Same source as in main plugin

MonolithComboGraph/                 # Another satellite
    MonolithComboGraph.uplugin
    Binaries/Win64/...
    Source/MonolithComboGraph/...
```

### 4.2 Satellite .uplugin Example

```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "0.11.0",
    "FriendlyName": "Monolith GBA Integration",
    "Description": "Adds GAS Blueprint Attributes (GBA) support to Monolith MCP. Requires both Monolith and GBAPlugin.",
    "Category": "Editor",
    "CreatedBy": "tumourlove",
    "CreatedByURL": "https://github.com/tumourlove/monolith",
    "CanContainContent": false,
    "Installed": true,
    "EnabledByDefault": true,
    "SupportedTargetPlatforms": ["Win64"],
    "Plugins": [
        {
            "Name": "Monolith",
            "Enabled": true
        },
        {
            "Name": "GBAPlugin",
            "Enabled": true
        }
    ],
    "Modules": [
        {
            "Name": "MonolithGBA",
            "Type": "Editor",
            "LoadingPhase": "Default"
        }
    ]
}
```

**Hard dependencies are fine here.** The `.uplugin` declares both `Monolith` and `GBAPlugin` as required. If GBAPlugin isn't installed/enabled, UE won't attempt to load this plugin at all. The DLL never gets touched, so the missing GBAPlugin DLLs are irrelevant.

`"EnabledByDefault": true` means users don't have to manually enable it in their project settings. If both parent plugins are present, the satellite activates automatically.

`"Installed": true` prevents UE from auto-disabling the plugin when the `.uproject` doesn't explicitly list it. Without this, a freshly-extracted satellite can be silently skipped on first launch because UE treats unlisted non-installed plugins as disabled by default.

`"SupportedTargetPlatforms": ["Win64"]` explicitly limits the satellite to Windows, matching Monolith's current platform support.

### 4.3 Build.cs in Binary Mode (No Conditional Logic)

```csharp
// MonolithGBA/Source/MonolithGBA/MonolithGBA.Build.cs (binary satellite version)
using UnrealBuildTool;

public class MonolithGBA : ModuleRules
{
    public MonolithGBA(ReadOnlyTargetRules Target) : base(Target)
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
            "Json",
            "JsonUtilities",
            "GBAPlugin",           // Hard dep — safe because .uplugin guards loading
            "GBAPluginEditor"
        });

        // Always defined as 1 in satellite plugin mode
        PublicDefinitions.Add("WITH_GBA=1");
    }
}
```

The C++ source is identical. Only `Build.cs` differs: no `Directory.Exists()` check, hard deps, `WITH_GBA=1` unconditionally.

### 4.4 Directory Layout for Release Zip

```
Monolith-v0.11.0.zip
    Monolith/
        Monolith.uplugin
        Binaries/
            Win64/
                UnrealEditor-MonolithCore.dll
                UnrealEditor-MonolithBlueprint.dll
                UnrealEditor-MonolithMaterial.dll
                ... (all core modules)
        Source/
            ... (all core source, no optional modules)
        Scripts/
        Skills/
        Docs/

    MonolithGBA/                    # Only if GBA integration exists
        MonolithGBA.uplugin
        Binaries/
            Win64/
                UnrealEditor-MonolithGBA.dll
        Source/
            MonolithGBA/
                ...

    MonolithComboGraph/             # Only if ComboGraph integration exists
        MonolithComboGraph.uplugin
        Binaries/
            Win64/
                UnrealEditor-MonolithComboGraph.dll
        Source/
            MonolithComboGraph/
                ...
```

Users extract the whole zip into their `Plugins/` folder. UE discovers each `.uplugin` independently. Satellites whose dependencies aren't met are silently skipped.

### 4.5 make_release.ps1 Updates

The current `make_release.ps1` copies tracked files from the Monolith git repo and adds `Binaries/`. For satellite plugins, it needs to:

1. Build all satellite plugins (they'll be in separate project plugin folders or a monorepo subfolder)
2. Copy each satellite's `Binaries/`, `Source/`, and `.uplugin` into its own top-level folder in the zip
3. Strip `.pdb` and `.patch_*` files from satellite binaries (same as core)
4. Set `"Installed": true` in each satellite `.uplugin`

The exact implementation depends on whether satellites live in the same git repo (subfolder) or separate repos. Same-repo subfolder is recommended for maintainability.

---

## 5. Action Registration and Discovery

### 5.1 FMonolithToolRegistry: The Shared Singleton

`FMonolithToolRegistry` (in `MonolithCore`) is an `MONOLITHCORE_API`-exported singleton. Any module in any plugin can access it via `FMonolithToolRegistry::Get()`. This is the key architectural feature that makes satellite plugins work -- they register actions into the same registry as core modules.

**Current API surface (from `MonolithToolRegistry.h`):**
- `RegisterAction(Namespace, Action, Description, Handler, ParamSchema)` -- add an action
- `UnregisterNamespace(Namespace)` -- remove all actions in a namespace (module shutdown)
- `ExecuteAction(Namespace, Action, Params)` -- dispatch a call
- `GetNamespaces()` / `GetActions(Namespace)` / `GetAllActions()` -- introspection
- `HasAction(Namespace, Action)` -- existence check
- `GetActionCount()` -- total count

The `RegistryLock` (FCriticalSection) protects concurrent access. Satellite modules can register/unregister at any point during editor lifetime.

> **Thread safety note:** In `ExecuteAction`, the lock is released before the handler delegate is invoked (the handler is copied out first). Between `Lock.Unlock()` and `HandlerCopy.Execute()`, it is theoretically possible for `UnregisterNamespace` to be called on that namespace. This is safe in practice because action handlers use `CreateStatic` delegates (no `this` pointer capture, no dangling reference). The static function address remains valid for the lifetime of the DLL. For editor-only code with no hot-unload of modules mid-request, this race window is acceptable.

### 5.2 Separate Namespaces Per Optional Module

Each optional integration gets its own MCP tool namespace:

| Integration | Namespace | MCP Tool Name |
|---|---|---|
| GBA (GAS Blueprint Attributes) | `gba` | `gba_query` |
| ComboGraph | `combograph` | `combograph_query` |
| (future) DialogueTree | `dialogue` | `dialogue_query` |

This keeps optional actions cleanly separated from core namespaces (`blueprint`, `material`, etc.) and enables clean unloading via `UnregisterNamespace()`.

### 5.3 Clean Shutdown

Every module already follows this pattern (verified across all 10 existing modules):

```cpp
void FMonolithXxxModule::ShutdownModule()
{
    FMonolithToolRegistry::Get().UnregisterNamespace(TEXT("xxx"));
}
```

Satellite modules do the same. When UE unloads a satellite plugin (or the editor shuts down), all its actions are removed from the registry. The HTTP server will stop advertising them in `tools/list`.

### 5.4 Enhanced monolith_discover

Currently, `monolith_discover` only reports namespaces that have registered actions. Enhance it to also report **known-but-unavailable** optional modules:

```json
{
    "namespaces": {
        "blueprint": { "actions": 86, "status": "loaded" },
        "material": { "actions": 57, "status": "loaded" },
        "gba": { "actions": 0, "status": "not_installed",
                 "hint": "Install GBAPlugin (Fab) and enable MonolithGBA" },
        "combograph": { "actions": 12, "status": "disabled",
                        "hint": "Enable in Project Settings > Plugins > Monolith > Modules > Optional" }
    }
}
```

Implementation: `MonolithCore` maintains a static list of known optional namespaces with their install hints. During discover, it checks whether each namespace has registered actions. If not, it distinguishes two states:

- **`"not_installed"`** -- the optional module's target plugin is not present on disk (satellite didn't load, or `WITH_FOO=0` in source mode).
- **`"disabled"`** -- the module loaded but the user toggled it off in `UMonolithSettings`. The module's `StartupModule` returned early without registering actions. This is distinct from `"not_installed"` because re-enabling requires only a settings change and editor restart, not a plugin install.

The known-modules list is hardcoded in `MonolithCore` -- it's metadata, not a dependency.

---

## 6. Settings Integration

### 6.1 Module Toggles

Extend `UMonolithSettings` with toggles for optional modules, matching the existing pattern:

```cpp
// In MonolithSettings.h, under "// --- Module Toggles ---"

UPROPERTY(config, EditAnywhere, Category="Modules|Optional")
bool bEnableGBA = true;

UPROPERTY(config, EditAnywhere, Category="Modules|Optional")
bool bEnableComboGraph = true;
```

**Keep it simple: always show the toggle.** If the optional module isn't loaded (plugin missing or `WITH_GBA=0`), the toggle does nothing and the startup log says "GBA integration disabled (GBAPlugin not found)".

> **Do NOT use `#if WITH_GBA` to conditionally show/hide these properties.** `UMonolithSettings` lives in MonolithCore, which has no `WITH_GBA` define and must never depend on optional modules. The `#if` guard would always evaluate to 0 in MonolithCore's compilation unit.
>
> If you want to hide toggles for uninstalled modules, use `IDetailCustomization` registered by the optional module itself, or check `FModuleManager::Get().IsModuleLoaded("MonolithGBA")` in a settings customization in MonolithEditor.

### 6.2 Settings Check at Startup

Same as every existing module:

```cpp
void FMonolithGBAModule::StartupModule()
{
    if (!GetDefault<UMonolithSettings>()->bEnableGBA) return;
    FMonolithGBAActions::RegisterActions(FMonolithToolRegistry::Get());
    UE_LOG(LogMonolith, Log, TEXT("Monolith - GBA module loaded (%d actions)"), ActionCount);
}
```

---

## 7. Error Handling

### 7.1 Module Not Loaded: Tool Doesn't Appear

The MCP `tools/list` response is built dynamically from `FMonolithToolRegistry::GetNamespaces()`. If `MonolithGBA` didn't load (plugin missing, setting disabled, or dependency unmet), the `gba` namespace has zero registered actions. The `gba_query` tool simply doesn't appear in `tools/list`. Claude never sees it, never calls it.

### 7.2 Direct Call to Missing Namespace

If a caller somehow invokes `gba_query` when the namespace isn't registered, `ExecuteAction` returns:

```cpp
FMonolithActionResult::Error(
    FString::Printf(TEXT("Unknown action: %s.%s"), *Namespace, *Action),
    FMonolithJsonUtils::ErrMethodNotFound  // -32601 per JSON-RPC 2.0
);
```

Note: this error fires for any unregistered namespace+action combination. The registry does not distinguish "unknown namespace" from "unknown action within a known namespace" -- both produce the same `"Unknown action: namespace.action"` error. This is intentional; from the caller's perspective the distinction rarely matters, and it avoids leaking information about which namespaces exist but have no matching action.

The MCP proxy translates this to a standard JSON-RPC error response. No crash, no undefined behavior.

### 7.3 Discover Reports Availability

As described in section 5.4, `monolith_discover` tells the caller which optional modules are known but not installed (or installed but disabled), with actionable hints. This lets AI agents understand what capabilities are available vs. what could be added.

---

## 8. Migration Path: Source to Binary

When the time comes to ship binary releases with optional module support, the migration is mechanical:

### 8.1 For Each Optional Module (e.g., MonolithGBA)

1. **Create satellite plugin directory:**
   ```
   Plugins/MonolithGBA/
       MonolithGBA.uplugin          # New file (see section 4.2)
       Source/MonolithGBA/           # MOVE from Monolith/Source/MonolithGBA/
   ```

2. **Move the source folder** from `Monolith/Source/MonolithGBA/` to `MonolithGBA/Source/MonolithGBA/`.

3. **Replace Build.cs** -- remove `Directory.Exists()` logic, hard-code deps (see section 4.3). Or maintain two Build.cs files and swap during release packaging.

4. **Remove from Monolith.uplugin** -- delete the `MonolithGBA` entry from the `Modules` array.

5. **Update make_release.ps1** -- add the satellite to the zip layout.

### 8.2 No Code Changes

The actual `.cpp` and `.h` files don't change. The `#if WITH_GBA` guards still work because:
- Source mode: `WITH_GBA` is 0 or 1 based on `Directory.Exists()`
- Binary satellite mode: `WITH_GBA=1` always (unconditional in satellite Build.cs)

### 8.3 Maintaining Both Modes Simultaneously

If you want to support both source and binary users from the same repo (you do), keep the source inside the main Monolith tree with the conditional `Build.cs`. The release script extracts it into a satellite structure. This means:

- **Repo layout:** `Monolith/Source/MonolithGBA/` (source mode)
- **Release zip layout:** `MonolithGBA/Source/MonolithGBA/` (binary mode)
- **make_release.ps1** handles the restructuring, including swapping `Build.cs` files

Keep a `MonolithGBA.Build.cs.release` next to the source `Build.cs`. The release script copies it as `Build.cs` in the output. Simple, no git branch gymnastics.

---

## 9. Checklist for Adding a New Optional Module

Use this step-by-step when adding support for a new third-party plugin integration.

### Step 1: Create the Module Source

```
Source/MonolithFoo/
    MonolithFoo.Build.cs           # With Directory.Exists() pattern
    Private/
        MonolithFooModule.h
        MonolithFooModule.cpp      # StartupModule checks bEnableFoo
        MonolithFooActions.h
        MonolithFooActions.cpp     # #if WITH_FOO guarded
    Public/
        (headers if needed)
```

### Step 2: Build.cs with Conditional Detection

Follow the template in section 3.1. Key checklist:
- [ ] Check both project `Plugins/` and engine `Plugins/Marketplace/` paths
- [ ] Define `WITH_FOO=1` or `WITH_FOO=0` (never undefined)
- [ ] Add dependency modules only when detected
- [ ] Use `PrivateDependencyModuleNames` for third-party modules (not Public)
- [ ] Research the obfuscated directory name for your target plugin by examining a Fab install. The prefix derives from the marketplace slug and is stable across versions but unique per product. For example, GBA uses `"Gameplaya*"` but another plugin will have a completely different prefix.

### Step 3: Implement Actions with Guards

- [ ] All third-party `#include` directives inside `#if WITH_FOO`
- [ ] All API calls to third-party code inside `#if WITH_FOO`
- [ ] Stub `RegisterActions()` in the `#else` branch (log message only)
- [ ] No `UPROPERTY`/`UCLASS`/`USTRUCT` inside `#if` blocks

### Step 4: Register in Monolith.uplugin

- [ ] Add module entry: `{"Name": "MonolithFoo", "Type": "Editor", "LoadingPhase": "Default"}`
- [ ] If the third-party plugin is always available (engine plugin), add it to the `Plugins` array

### Step 5: Settings Toggle

- [ ] Add `bool bEnableFoo = true;` to `UMonolithSettings` under `Modules|Optional` category
- [ ] Check `bEnableFoo` in `StartupModule()`

### Step 6: Discover Metadata

- [ ] Add entry to the known-optional-modules list in `MonolithCore` with namespace name and install hint
- [ ] Include both `"not_installed"` and `"disabled"` hint strings

### Step 7: Create Satellite Build.cs for Binary Releases

- [ ] Create `MonolithFoo.Build.cs.release` with hard deps and `WITH_FOO=1`
- [ ] Create `MonolithFoo.uplugin.release` template (see section 4.2)

### Step 8: Update Release Script

- [ ] Add satellite to `make_release.ps1` packaging logic

### Step 9: Documentation

- [ ] Update `CLAUDE.md` action counts and tool list
- [ ] Update `Docs/SPEC.md` with new namespace
- [ ] Add to wiki if applicable

---

## 10. UE 5.7 Constraints Reference

### 10.1 What Doesn't Exist

| Feature | Status in UE 5.7 | Notes |
|---|---|---|
| `bOptional` on module descriptors | Does not exist | Only exists on plugin references in `.uplugin` |
| `ConditionallyLoadedModuleNames` | Removed | Was deprecated, fully gone in 5.7 |
| `SoftDependencies` in `.uplugin` | Does not exist | Never was a thing despite sounding logical |
| Per-module Optional flag | Does not exist | Modules in a plugin all load or none load |
| Runtime `LoadModule()` with graceful failure | Crashes | Missing DLL = fatal error dialog |

### 10.2 What Does Exist and How It Works

| Feature | How It Works |
|---|---|
| `"Optional": true` on plugin references | In `.uplugin` `Plugins` array. If the referenced plugin isn't found, the referencing plugin still loads. But this only affects plugin-level dependency resolution, NOT missing DLLs. |
| `FModuleManager::Get().IsModuleLoaded()` | Runtime check. Returns false if module isn't loaded. Safe to call, never crashes. Use this to check if a third-party plugin's module is available before calling into it. |
| `Directory.Exists()` in Build.cs | Compile-time filesystem probe. UBT runs Build.cs as C#. Full access to `System.IO`. Works reliably on all platforms. |
| `PublicDefinitions.Add()` / `PrivateDefinitions.Add()` | Preprocessor defines from Build.cs. Pass to C++ via `/D` compiler flag. |
| Plugin-level `Plugins` array (hard refs) | If a listed plugin with `"Enabled": true` isn't found, the entire plugin is skipped. No error dialog -- just a log warning and silent skip. This is the mechanism that makes satellite plugins work. |

### 10.3 Key Source Files

For anyone who wants to verify these constraints against engine source:

- **Plugin discovery:** `Engine/Source/Runtime/Projects/Private/PluginManager.cpp` -- `FPluginManager::ConfigureEnabledPlugins()`
- **Module loading:** `Engine/Source/Runtime/Core/Private/Modules/ModuleManager.cpp` -- `FModuleManager::LoadModuleWithFailureReason()`
- **Build.cs evaluation:** `Engine/Source/Programs/UnrealBuildTool/System/ModuleRules.cs`
- **Plugin descriptor:** `Engine/Source/Runtime/Projects/Public/PluginDescriptor.h` -- `FPluginReferenceDescriptor` has `bOptional`

---

## Appendix: Architecture Diagram

```
                    Monolith.uplugin
                    (always loads)
    ┌──────────────────────────────────────────┐
    │  MonolithCore                             │
    │    FMonolithToolRegistry (singleton)  <───┼──── Satellite plugins
    │    FMonolithHttpServer                    │     register here too
    │    UMonolithSettings                     │
    │    monolith_discover (reports all)        │
    ├──────────────────────────────────────────┤
    │  MonolithBlueprint  (86 actions)         │
    │  MonolithMaterial   (57 actions)         │
    │  MonolithAnimation  (115 actions)        │
    │  MonolithNiagara    (96 actions)         │
    │  MonolithEditor     (19 actions)         │
    │  MonolithConfig     (6 actions)          │
    │  MonolithIndex      (7 actions)          │
    │  MonolithSource     (11 actions)         │
    │  MonolithUI         (42 actions)         │
    └──────────────────────────────────────────┘

    MonolithGBA.uplugin              MonolithComboGraph.uplugin
    (loads only if GBAPlugin         (loads only if ComboGraph
     is installed + enabled)          is installed + enabled)
    ┌───────────────────┐            ┌───────────────────────┐
    │  MonolithGBA      │            │  MonolithComboGraph   │
    │  Namespace: gba   │──register──│  Namespace: combograph│
    │  N actions        │   into     │  N actions            │
    └───────────────────┘  registry  └───────────────────────┘
```

---

## 11. Relationship to IModularFeatures Bridge Pattern

The existing `Plugins/Monolith/Docs/OPTIONAL_MODULES.md` documents an **IModularFeatures bridge pattern** already validated in the codebase. This is a complementary approach — not a replacement — for the architecture described above.

### When to Use Which

| Scenario | Use This Pattern |
|----------|-----------------|
| Optional module registers MCP actions (gba_query, combograph_query) | **This spec** — direct FMonolithToolRegistry registration |
| Core module needs to call into optional module's C++ API | **IModularFeatures bridge** — abstract interface in core, implementation in bridge module |
| Both | Both — the optional module registers actions AND publishes an IModularFeature interface |

### How They Work Together

The IModularFeatures bridge is valuable when a **core** Monolith module (e.g., MonolithBlueprint) wants to optionally use functionality from a third-party plugin without any compile-time coupling. Example: `auto_layout` optionally using Blueprint Assist's formatter.

For GBA/ComboGraph, the MCP action pattern (this spec) is primary because:
- AI agents interact via JSON actions, not C++ calls
- The optional module owns its entire namespace
- No core module needs to call GBA APIs directly

But if a future core action (e.g., `blueprint_query("create_attribute_set")`) wanted to automatically delegate to GBA when available, the IModularFeatures bridge would be the right way to do that — define `IMonolithAttributeSetProvider` in MonolithCore, implement it in MonolithGBA, and check `IsAvailable()` at action time.

### Key Differences

| Aspect | This Spec (Registry) | IModularFeatures Bridge |
|--------|---------------------|------------------------|
| Communication | JSON-in/JSON-out MCP actions | Direct C++ virtual calls |
| Registration | FMonolithToolRegistry::RegisterAction | IModularFeatures::RegisterModularFeature |
| Discovery | monolith_discover | IMyFeature::IsAvailable() |
| Consumer coupling | Zero (JSON dispatch) | Interface header only (no optional headers) |
| Use case | AI agent → optional actions | Core C++ → optional C++ API |

Both patterns coexist naturally. The optional module's `StartupModule()` can register both MCP actions and an IModularFeature interface.

---

## 12. Edge Cases

### 12.1 Plugin Uninstalled After Satellite Was Enabled

If a user removes GBAPlugin after `MonolithGBA` was already enabled in their `.uproject`:

1. **Next editor launch:** UE scans `MonolithGBA.uplugin`, finds `GBAPlugin` dependency unsatisfied
2. **Result:** UE silently disables `MonolithGBA` (logs a warning, does NOT show error dialog)
3. **The `.uproject` retains the `MonolithGBA` entry** but with `"Enabled": false` auto-set
4. **If user reinstalls GBAPlugin later:** they must re-enable `MonolithGBA` manually in Edit > Plugins
5. **No data loss, no crashes.** The MCP actions simply disappear from `tools/list`.

For source mode: the next build evaluates `Directory.Exists()` → false → `WITH_GBA=0` → stub compiles. **Requires clean rebuild** if UBT cached the previous evaluation (see section 3.1 warning).

### 12.2 LoadingPhase Ordering

MonolithCore uses `PostEngineInit`. All domain modules (including satellites) use `Default`. UE processes loading phases in order: `PreDefault` → `Default` → `PostDefault` → `PostEngineInit`.

Critically, UE's loading phase ordering works **across all plugins**, not per-plugin. UE loads ALL `Default`-phase modules from ALL enabled plugins before advancing to `PostEngineInit` for ANY plugin. This means satellite modules in separate `.uplugin` files (e.g., `MonolithGBA.uplugin`) also get their `Default`-phase modules loaded before MonolithCore's `PostEngineInit` startup runs. The cross-plugin phase guarantee is what makes the satellite architecture work: by the time MonolithCore starts the HTTP server at `PostEngineInit`, every satellite module has already had its `StartupModule()` called and registered its actions.

**Do NOT set satellite modules to PostEngineInit.** This would create a race where the HTTP server might handle requests before satellite actions are registered. There is no guaranteed ordering between modules at the same loading phase across different plugins.

### 12.3 Live Coding / Hot Reload

Live Coding does not affect satellite modules as long as the registry singleton lives in MonolithCore and satellites only modify their own namespace. The singleton pointer returned by `FMonolithToolRegistry::Get()` remains stable because MonolithCore's DLL is not patched when a satellite's code changes.

Full module reloads via Live Coding are not supported — restart the editor. Live Coding patches are in-memory only and do not update the on-disk DLL.

### 12.4 Stub DLL Accumulation (Source Mode)

In source mode, each optional module listed in `Monolith.uplugin` produces a DLL even when the third-party plugin is not installed (`WITH_FOO=0` → stub). The stub DLL is tiny (just the module boilerplate) and loads in microseconds.

For a small number of satellites (up to ~8) this is trivial overhead. If the satellite count grows beyond that, consider a build-time flag (`MONOLITH_EXCLUDE_OPTIONAL_MODULES`) that removes optional module entries from `.uplugin` entirely, preventing UBT from compiling them. This is a future optimization, not a current concern.

---

*This document synthesizes findings from four research agents examining UE 5.7 plugin architecture, Monolith's current structure, real-world plugin patterns, and implementation design, plus reconciliation with the existing IModularFeatures bridge pattern in `OPTIONAL_MODULES.md`. It is the definitive reference for optional module support in Monolith.*
