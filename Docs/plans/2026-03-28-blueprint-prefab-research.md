# Blueprint-Based Prefabs: Dialog-Free Alternative to Level Instances

**Date:** 2026-03-28
**Status:** Research Complete
**Context:** `mesh_query("create_prefab")` uses `ULevelInstanceSubsystem::CreateLevelInstanceFrom` which triggers a Save As dialog that blocks MCP calls. We need a dialog-free approach to group actors into a reusable prefab.

---

## Problem

The current `create_prefab` action (MonolithMeshAdvancedLevelActions.cpp:804) calls `CreateLevelInstanceFrom` with `bAlwaysShowDialog=false` and `bPromptForSave=false`, but the engine's internal implementation still calls `bUseSaveAs=true` which opens a system file dialog. This blocks the MCP thread and hangs the editor.

## Recommended Solution: Pure SCS Blueprint Construction

**Create an Actor Blueprint programmatically, add StaticMeshComponents via the SCS, set their transforms and mesh references, compile, and save.** No dialogs involved at any step.

This approach composes entirely from existing Monolith `blueprint_query` actions -- no new C++ needed for a basic implementation. A dedicated C++ action would be faster and more ergonomic.

---

## Approach 1: Compose Existing `blueprint_query` Actions (Zero C++ Changes)

The following existing actions chain together to create a prefab blueprint:

### Step-by-Step Workflow

```
1. blueprint_query("create_blueprint", {
     save_path: "/Game/Prefabs/BP_DoorFrame",
     parent_class: "Actor"
   })

2. For each mesh component:
   blueprint_query("add_component", {
     asset_path: "/Game/Prefabs/BP_DoorFrame",
     component_class: "StaticMeshComponent",
     component_name: "Frame_Left",
     parent: "DefaultSceneRoot"           // attach to root
   })

3. Set mesh asset on each component:
   blueprint_query("set_component_property", {
     asset_path: "/Game/Prefabs/BP_DoorFrame",
     component_name: "Frame_Left",
     property_name: "StaticMesh",
     value: "/Game/Meshes/SM_DoorFrame.SM_DoorFrame"
   })

4. Set relative transforms:
   blueprint_query("set_component_property", {
     asset_path: "/Game/Prefabs/BP_DoorFrame",
     component_name: "Frame_Left",
     property_name: "RelativeLocation",
     value: "X=100.0 Y=0.0 Z=0.0"
   })

   blueprint_query("set_component_property", {
     ...,
     property_name: "RelativeRotation",
     value: "P=0.0 Y=90.0 R=0.0"
   })

5. Compile and save:
   blueprint_query("compile_blueprint", {
     asset_path: "/Game/Prefabs/BP_DoorFrame"
   })
   blueprint_query("save_asset", {
     asset_path: "/Game/Prefabs/BP_DoorFrame"
   })

6. Spawn later via:
   mesh_query("place_blueprint_actor", {
     blueprint: "/Game/Prefabs/BP_DoorFrame",
     location: [0, 0, 0]
   })
```

### One-Shot Alternative: `build_blueprint_from_spec`

The `build_blueprint_from_spec` action already supports a `components` array. Combined with `set_component_property` calls for transforms/meshes, this reduces the workflow to 2-3 calls:

```
1. blueprint_query("create_blueprint", {
     save_path: "/Game/Prefabs/BP_DoorFrame",
     parent_class: "Actor"
   })

2. blueprint_query("build_blueprint_from_spec", {
     asset_path: "/Game/Prefabs/BP_DoorFrame",
     components: [
       { name: "Frame_Left",  class: "StaticMeshComponent", parent: "DefaultSceneRoot" },
       { name: "Frame_Right", class: "StaticMeshComponent", parent: "DefaultSceneRoot" },
       { name: "Lintel",      class: "StaticMeshComponent", parent: "DefaultSceneRoot" }
     ],
     auto_compile: false
   })

3. Set properties via batch_execute or individual set_component_property calls
   (build_blueprint_from_spec does NOT currently set component properties)

4. blueprint_query("compile_blueprint", ...)
   blueprint_query("save_asset", ...)
```

### Pros
- **Zero C++ changes required** -- works today
- Uses battle-tested, well-validated actions
- Full error reporting at each step

### Cons
- Multiple round-trips (5-10 MCP calls per prefab)
- `build_blueprint_from_spec` components array does not support property initialization
- No way to read mesh/transform from existing world actors (need a separate query first)

---

## Approach 2: New Dedicated `create_blueprint_prefab` C++ Action (Recommended)

A single action that reads world actors, creates a Blueprint, adds their StaticMeshComponents to the SCS with correct transforms/meshes, compiles, and saves. One MCP call, no dialogs.

### API Design

```
mesh_query("create_blueprint_prefab", {
  actor_names: ["SM_DoorFrame_Left", "SM_DoorFrame_Right", "SM_Lintel"],
  save_path: "/Game/Prefabs/BP_DoorFrame",
  parent_class: "Actor",           // optional, default "Actor"
  keep_mobility: false,            // optional, default false
  center_pivot: true               // optional, recenter to group centroid
})
```

### Implementation Strategy

The C++ implementation would follow the pattern established by `FKismetEditorUtilities::HarvestBlueprintFromActors` (Kismet2.cpp:2026) but without the dialog and actor-replacement machinery:

```cpp
FMonolithActionResult CreateBlueprintPrefab(const TSharedPtr<FJsonObject>& Params)
{
    // 1. Resolve actors from world (existing pattern from MonolithMeshAdvancedLevelActions)
    TArray<AActor*> Actors;
    // ... resolve actor_names ...

    // 2. Create package (no dialog -- same as our existing create_blueprint)
    FString PackageName = SavePath;
    UPackage* Package = CreatePackage(*PackageName);
    FString AssetName = FPackageName::GetLongPackageAssetName(SavePath);

    // 3. Create Blueprint via FKismetEditorUtilities::CreateBlueprint
    //    Signature: CreateBlueprint(ParentClass, Outer, Name, Type, CallingContext)
    //    This is dialog-free. Used internally by HarvestBlueprintFromActors.
    UBlueprint* BP = FKismetEditorUtilities::CreateBlueprint(
        AActor::StaticClass(),
        Package,
        FName(*AssetName),
        BPTYPE_Normal,
        UBlueprint::StaticClass(),
        UBlueprintGeneratedClass::StaticClass(),
        FName("MonolithPrefab")
    );

    // 4. Compute pivot (centroid of actor locations)
    FVector Pivot = FVector::ZeroVector;
    for (AActor* A : Actors) Pivot += A->GetActorLocation();
    Pivot /= Actors.Num();

    // 5. Harvest components into SCS
    //    Option A: Use FKismetEditorUtilities::AddComponentsToBlueprint
    //    Option B: Manual SCS node creation (more control over transforms)

    USimpleConstructionScript* SCS = BP->SimpleConstructionScript;
    USCS_Node* RootNode = SCS->GetDefaultSceneRootNode();

    for (AActor* Actor : Actors)
    {
        for (UActorComponent* Comp : Actor->GetComponents())
        {
            UStaticMeshComponent* SMC = Cast<UStaticMeshComponent>(Comp);
            if (!SMC || SMC->IsVisualizationComponent()) continue;

            // Create SCS node
            USCS_Node* NewNode = SCS->CreateNode(
                UStaticMeshComponent::StaticClass(),
                SCS->GenerateNewComponentName(UStaticMeshComponent::StaticClass())
            );

            // Copy all properties from the live component to the template
            UEditorEngine::CopyPropertiesForUnrelatedObjects(SMC, NewNode->ComponentTemplate);

            // Adjust relative transform: world transform -> relative to pivot
            USceneComponent* Template = Cast<USceneComponent>(NewNode->ComponentTemplate);
            FTransform WorldTransform = SMC->GetComponentTransform();
            FTransform RelativeToPivot = WorldTransform;
            RelativeToPivot.SetLocation(WorldTransform.GetLocation() - Pivot);
            Template->SetRelativeTransform_Direct(RelativeToPivot);
            Template->SetMobility(EComponentMobility::Movable);

            // Attach to root
            RootNode->AddChildNode(NewNode);
        }
    }

    // 6. Compile
    FKismetEditorUtilities::CompileBlueprint(BP);

    // 7. Save package
    FAssetRegistryModule::AssetCreated(BP);
    Package->MarkPackageDirty();
    UPackage::SavePackage(Package, BP, *PackageName,
        FSavePackageArgs()); // or appropriate overload

    // 8. Return result
    // ...
}
```

### Key Engine APIs (All Verified in UE 5.7 Source)

| API | File | Dialog-Free? | Notes |
|-----|------|:---:|-------|
| `FKismetEditorUtilities::CreateBlueprint(ParentClass, Outer, Name, Type, BPClass, BGCClass, Context)` | KismetEditorUtilities.h:124 | YES | Core BP creation, no UI |
| `USimpleConstructionScript::CreateNode(UClass*, FName)` | SimpleConstructionScript.h:167 | YES | Creates SCS node with template |
| `USimpleConstructionScript::AddNode(USCS_Node*)` | SimpleConstructionScript.h:91 | YES | Adds to root set |
| `USCS_Node::AddChildNode(USCS_Node*, bool)` | SCS_Node.h:126 | YES | Parent-child attachment |
| `UEditorEngine::CopyPropertiesForUnrelatedObjects()` | Used in Kismet2.cpp:1214 | YES | Deep property copy |
| `FKismetEditorUtilities::CompileBlueprint()` | KismetEditorUtilities.h (implicit) | YES | Compiles without UI |
| `FKismetEditorUtilities::AddComponentsToBlueprint()` | KismetEditorUtilities.h:225 | YES | Bulk component harvest |
| `FKismetEditorUtilities::HarvestBlueprintFromActors(FName, UPackage*, ...)` | KismetEditorUtilities.h:403 | YES | The Package overload is dialog-free |
| `FKismetEditorUtilities::CreateBlueprintFromActor(FName, UObject*, AActor*, Params)` | KismetEditorUtilities.h:305 | YES | Name+Outer overload is dialog-free |

**Critical distinction:** The `FString Path` overloads of these functions call `CreateBlueprintPackage()` which just calls `CreatePackage()` -- also dialog-free. The dialog comes from `FCreateBlueprintFromActorDialog::OpenDialog()` (KismetWidgets module) which is only called from the Editor UI, never from the programmatic API.

### Pros
- **Single MCP call** -- one action does everything
- Reads mesh/transform data directly from world actors
- Automatic pivot centering
- No intermediate actor state needed

### Cons
- Requires new C++ code (~100-150 lines)
- Needs to handle non-StaticMesh components (lights, audio, etc.)

---

## Approach 3: Use `HarvestBlueprintFromActors` Directly

The engine's `HarvestBlueprintFromActors(FName, UPackage*, Actors, Params)` overload is completely dialog-free and does exactly what we want -- it harvests all components from a set of actors into a new Blueprint's SCS.

```cpp
// This overload takes FName + UPackage* -- no dialog
FKismetEditorUtilities::FHarvestBlueprintFromActorsParams HarvestParams;
HarvestParams.bReplaceActors = false;   // Don't delete source actors
HarvestParams.bOpenBlueprint = false;   // Don't open BP editor
HarvestParams.ParentClass = AActor::StaticClass();

UPackage* Package = CreatePackage(*SavePath);
FString AssetName = FPackageName::GetLongPackageAssetName(SavePath);

UBlueprint* BP = FKismetEditorUtilities::HarvestBlueprintFromActors(
    FName(*AssetName),
    Package,
    Actors,
    HarvestParams
);
```

### Why This Is the Best C++ Path

1. **Engine does all the heavy lifting** -- component iteration, property copying, SCS hierarchy, parent resolution
2. **Handles ALL component types** -- not just StaticMeshComponent
3. **Preserves parent-child relationships** between components
4. **Multi-actor support** -- appends owner name to avoid conflicts (`HarvestMode::Havest_AppendOwnerName`)
5. **~30 lines of new C++** vs ~150 for manual SCS construction

### The One Catch: Transform Handling

`HarvestBlueprintFromActors` converts component transforms to world space when there are multiple root actors (Kismet2.cpp:2043-2053), which is correct for our use case. But it doesn't apply a pivot offset -- the components retain their world positions. For a proper prefab, we'd want to:

1. Compute centroid of all actors
2. Offset all actor positions by -centroid before harvesting
3. Restore original positions after

Or post-process the SCS nodes to adjust `RelativeLocation` on each template.

---

## Approach 4: Existing `blueprint_query` Actions Only (Simplest for Now)

No C++ changes. Create a higher-level workflow using existing actions:

```
mesh_query("get_actors_in_box", ...) or manual actor list
  -> blueprint_query("create_blueprint", { parent_class: "Actor", save_path: ... })
  -> For each actor's mesh:
       blueprint_query("add_component", { component_class: "StaticMeshComponent", ... })
       blueprint_query("set_component_property", { property_name: "StaticMesh", value: ... })
       blueprint_query("set_component_property", { property_name: "RelativeLocation", value: ... })
       blueprint_query("set_component_property", { property_name: "RelativeRotation", value: ... })
  -> blueprint_query("compile_blueprint", ...)
  -> blueprint_query("save_asset", ...)
```

**Property value formats** (verified from UE source -- these are `FVector`/`FRotator` text import formats):
- `RelativeLocation`: `"X=100.0 Y=200.0 Z=50.0"`
- `RelativeRotation`: `"P=0.0 Y=90.0 R=0.0"` (Pitch/Yaw/Roll)
- `RelativeScale3D`: `"X=1.0 Y=1.0 Z=1.0"`
- `StaticMesh`: `"/Game/Meshes/SM_Wall.SM_Wall"` (full object path)

**Note:** `RelativeLocation`, `RelativeRotation`, and `RelativeScale3D` are **private** with `AllowPrivateAccess="true"` (SceneComponent.h:133-145). UE's `ImportText` respects `AllowPrivateAccess` when used via property reflection, so `set_component_property` should work. If not, `SetRelativeLocation_Direct()` and friends are the fallback (would need C++ changes).

---

## Comparison Matrix

| Criterion | Approach 1: Chain Existing | Approach 2: New C++ Action | Approach 3: HarvestFromActors | Approach 4: Workflow Only |
|-----------|:---:|:---:|:---:|:---:|
| Dialog-free | YES | YES | YES | YES |
| C++ changes needed | None | ~150 lines | ~30 lines | None |
| MCP calls per prefab | 5-15 | 1 | 1 | 5-15 |
| Reads from world actors | No (manual) | Yes | Yes | No (manual) |
| Preserves component hierarchy | Manual | Manual | Automatic | Manual |
| Handles all component types | Yes | Needs work | Yes | Yes |
| Transform pivot support | Manual | Yes | Needs post-processing | Manual |
| Available today | YES | After implementation | After implementation | YES |

---

## Recommendation

### Immediate (Today): Use Approach 1/4

Chain existing `blueprint_query` actions. This works now with zero changes. The agent can orchestrate the multi-call workflow. Update `create_prefab` documentation to note the Level Instance dialog issue and recommend the Blueprint approach.

### Short-Term: Implement Approach 3 (HarvestBlueprintFromActors wrapper)

Create a new `create_blueprint_prefab` action in MonolithMesh that wraps `HarvestBlueprintFromActors` with the FName/UPackage overload. ~30 lines of C++, dialog-free, handles all component types automatically. Add pivot centering as a post-processing step.

### Implementation Plan for Approach 3

1. Add `create_blueprint_prefab` to `MonolithMeshAdvancedLevelActions`
2. Resolve actors from `actor_names` (existing helper)
3. Create package via `CreatePackage` (no dialog)
4. Call `HarvestBlueprintFromActors(FName, UPackage*, Actors, Params)` with `bReplaceActors=false`, `bOpenBlueprint=false`
5. If `center_pivot=true`, iterate SCS nodes and offset `RelativeLocation` by -centroid
6. Compile blueprint
7. Save package
8. Return asset path + component list

**Estimated effort:** ~2 hours including tests

### What About the Existing `create_prefab`?

Keep it but update the description to note the dialog limitation. Some users may actually want Level Instances (they have advantages like streaming). The Blueprint prefab is an alternative, not a replacement.

---

## Spawning Blueprint Prefabs

The existing `place_blueprint_actor` action already handles this perfectly:

```
mesh_query("place_blueprint_actor", {
  blueprint: "/Game/Prefabs/BP_DoorFrame",
  location: [1000, 2000, 0],
  rotation: [0, 45, 0],
  scale: [1, 1, 1],
  label: "DoorFrame_01"
})
```

No changes needed on the spawn side.

---

## Files Referenced

- **Current create_prefab impl:** `Source/MonolithMesh/Private/MonolithMeshAdvancedLevelActions.cpp:804`
- **Existing component actions:** `Source/MonolithBlueprint/Private/MonolithBlueprintComponentActions.cpp`
- **Existing build_blueprint_from_spec:** `Source/MonolithBlueprint/Private/MonolithBlueprintBuildActions.cpp`
- **Engine HarvestBlueprintFromActors:** `Engine/Source/Editor/UnrealEd/Private/Kismet2/Kismet2.cpp:2026`
- **Engine CreateBlueprint:** `Engine/Source/Editor/UnrealEd/Public/Kismet2/KismetEditorUtilities.h:110,124`
- **Engine SCS API:** `Engine/Source/Runtime/Engine/Classes/Engine/SimpleConstructionScript.h`
- **Engine SCS_Node:** `Engine/Source/Runtime/Engine/Classes/Engine/SCS_Node.h`
- **SceneComponent transform properties:** `Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h:134,138,145`
