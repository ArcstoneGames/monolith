# P1 MonolithMesh Wishlist — API Feasibility Research

**Date:** 2026-03-28
**Source:** `Docs/TODO.md` P1 section (lines 66-88)
**Method:** UE 5.7 engine source grep + header analysis (offline, editor down)

---

## Summary

All 15 P1 actions are **feasible** at editor-time. No PIE required. Total estimate: **~90-110 hours**.

The five Horror Design actions are primarily **composition/algorithm** work that layer on top of existing Monolith mesh analysis (sightlines, hiding spots, escape routes, choke points, pacing curves) rather than new UE API calls. The five Level Design actions and five Tech Art actions each require real UE API integration.

---

## Level Design (5 actions)

### 1. `build_navmesh`

**UE5 API:**
```cpp
// NavigationSystem.h — UNavigationSystemV1
NAVIGATIONSYSTEM_API virtual void Build();           // Full rebuild, line 1083
NAVIGATIONSYSTEM_API virtual void CancelBuild();     // Cancel in-flight build
NAVIGATIONSYSTEM_API virtual void RebuildAll(bool bIsLoadTime = false); // line 1406

// Get the system
UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
```

**Editor-time feasibility:** YES. `Build()` is what the editor calls when you hit "Build Paths". Works without PIE. The build is synchronous on the game thread (logged: `UNavigationSystemV1::Build total execution time`). Can also check `IsNavigationBuildingLocked()` and `IsThereAnywhereToBuildNavigation()` before calling.

**Params / Return:**
```json
// Input
{ "mode": "full" | "dirty_only" }

// Output
{
  "success": true,
  "build_time_seconds": 3.45,
  "nav_data_count": 1,
  "warnings": []
}
```

**Complexity:** 3-4 hours. Straightforward API call plus status reporting.

**Gotchas:**
- `Build()` is synchronous and can take seconds on large maps — need to warn the caller
- Must check `IsNavigationSystemEnabled()` first; returns silently if disabled
- Need `#include "NavigationSystem.h"` — module dep on `NavigationSystem`
- `SetBuildBounds(FBox)` exists for partial rebuilds (line 2381) — could expose as optional param
- The nav system might not exist if the world has no nav mesh volume — null-check required

---

### 2. `manage_sublevel`

**UE5 API:**
```cpp
// EditorLevelUtils.h — UEditorLevelUtils (UCLASS, BlueprintCallable)
static ULevelStreaming* CreateNewStreamingLevel(
    TSubclassOf<ULevelStreaming> LevelStreamingClass,
    const FString& NewLevelPath = TEXT(""),
    bool bMoveSelectedActorsIntoNewLevel = false);

static ULevelStreaming* CreateNewStreamingLevelForWorld(
    UWorld& World, TSubclassOf<ULevelStreaming> LevelStreamingClass,
    const FString& DefaultFilename, ...);

static ULevelStreaming* AddLevelToWorld(
    UWorld* InWorld, const TCHAR* LevelPackageName,
    TSubclassOf<ULevelStreaming> LevelStreamingClass,
    const FTransform& LevelTransform = FTransform::Identity);

static bool RemoveLevelFromWorld(ULevel* InLevel,
    bool bClearSelection = true, bool bResetTransBuffer = true);

static int32 MoveActorsToLevel(
    const TArray<AActor*>& ActorsToMove,
    ULevelStreaming* DestStreamingLevel,
    bool bWarnAboutReferences = true, bool bWarnAboutRenaming = true);

// Also: MoveActorsToLevel overload taking ULevel* directly
static ULevelStreaming* SetStreamingClassForLevel(
    ULevelStreaming* InLevel, TSubclassOf<ULevelStreaming> LevelStreamingClass);
```

**Editor-time feasibility:** YES. All functions are editor-only (`UNREALED_API`), designed for the level editor.

**Params / Return:**
```json
// Input — sub-actions
{
  "sub_action": "create" | "add" | "remove" | "move_actors",

  // create
  "level_path": "/Game/Maps/SubLevels/Basement",
  "streaming_class": "LevelStreamingDynamic",  // default: LevelStreamingDynamic
  "move_selected": false,

  // add (load existing)
  "level_path": "/Game/Maps/SubLevels/Basement",
  "transform": { "location": [0,0,-500] },

  // remove
  "level_name": "Basement",

  // move_actors
  "actor_names": ["Door_01", "Light_02"],
  "dest_level": "Basement"
}

// Output
{
  "success": true,
  "level_name": "Basement",
  "streaming_class": "LevelStreamingDynamic",
  "actor_count_moved": 5  // for move_actors
}
```

**Complexity:** 8-10 hours. Four sub-actions, each with different error handling. Actor resolution by name adds complexity.

**Gotchas:**
- `CreateNewStreamingLevel` will prompt a save dialog if `NewLevelPath` is empty — must always provide path
- `MoveActorsToLevel` returns count of moved actors, not bool — check for 0
- Level names vs package paths: need to resolve between `/Game/Maps/Foo` and the `ULevelStreaming` object
- `RemoveLevelFromWorld` returns bool — can fail if level is locked or persistent
- Cannot remove the persistent level
- The newer `CreateNewStreamingLevelForWorld` overload (line 170) has `bUseExternalActors` and `bIsPartitioned` params for World Partition support — should expose

---

### 3. `place_blueprint_actor`

**UE5 API:**
```cpp
// UEditorEngine (EditorEngine.h line 1236)
virtual AActor* AddActor(ULevel* InLevel, UClass* Class,
    const FTransform& Transform, bool bSilent = false,
    EObjectFlags ObjectFlags = RF_Transactional, bool bSelectActor = true);

// UWorld (World.h)
T* SpawnActorDeferred(UClass* Class, const FTransform& Transform, ...);

// Property setting via reflection
FProperty* Prop = Class->FindPropertyByName(FName("MyProp"));
void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Actor);
Prop->ImportText_Direct(*ValueString, ValuePtr, Actor, PPF_None);

// Or FPropertyValueIterator for nested structs
// StaticLoadClass for BP class loading
UClass* BPClass = StaticLoadClass(AActor::StaticClass(), nullptr,
    TEXT("/Game/Blueprints/BP_LockedDoor.BP_LockedDoor_C"));
```

**Editor-time feasibility:** YES. `AddActor` is the editor's own spawn path. Works without PIE.

**Params / Return:**
```json
// Input
{
  "blueprint": "/Game/Blueprints/BP_LockedDoor",
  "location": [100, 200, 0],
  "rotation": [0, 0, 90],
  "scale": [1, 1, 1],
  "properties": {
    "RequiredKeyTag": "Gameplay.Item.WardKey",
    "bIsLocked": true,
    "DamageResistance": 100.0
  },
  "label": "Ward_Door_01",  // optional actor label
  "select": true,
  "sublevel": "Basement"  // optional, default persistent
}

// Output
{
  "success": true,
  "actor_name": "BP_LockedDoor_2",
  "actor_label": "Ward_Door_01",
  "class": "BP_LockedDoor_C",
  "location": [100, 200, 0],
  "properties_set": ["RequiredKeyTag", "bIsLocked", "DamageResistance"]
}
```

**Complexity:** 6-8 hours. The property reflection part is the main work — need to handle bool, int, float, FString, FName, FGameplayTag, FVector, enums, object references.

**Gotchas:**
- BP class path needs `_C` suffix: `/Game/BP/MyActor.MyActor_C`
- `StaticLoadClass` returns null if path is wrong — no helpful error
- Property setting via `ImportText_Direct` handles most types but fails silently on malformed input
- Need to call `Actor->PostEditChangeProperty()` after setting properties, plus `Actor->MarkPackageDirty()`
- If spawning into a sublevel, must set the current level first via `World->SetCurrentLevel()`
- `AddActor` auto-selects — pass `bSelectActor=false` if `select` param is false
- Consider using existing Monolith CDO property infrastructure (`get_cdo_properties`) for type awareness

---

### 4. `select_actors`

**UE5 API:**
```cpp
// UnrealEdEngine.h (line 174-181)
virtual void SelectActor(AActor* Actor, bool InSelected, bool bNotify,
    bool bSelectEvenIfHidden = false, bool bForceRefresh = false) override;
virtual void SelectNone(bool bNoteSelectionChange, bool bDeselectBSPSurfs,
    bool WarnAboutManyActors = true) override;
virtual void NoteSelectionChange(bool bNotify = true) override;
virtual bool CanSelectActor(AActor* Actor, bool InSelected,
    bool bSelectEvenIfHidden = false, bool bWarnIfLevelLocked = false) const override;

// Camera focus (EditorEngine.h line 1113-1129)
void MoveViewportCamerasToActor(AActor& Actor, bool bActiveViewportOnly);
void MoveViewportCamerasToActor(const TArray<AActor*>& Actors, bool bActiveViewportOnly);
void MoveViewportCamerasToActor(const TArray<AActor*>& Actors,
    const TArray<UPrimitiveComponent*>& Components, bool bActiveViewportOnly);

// Current selection query
USelection* Selection = GEditor->GetSelectedActors();
Selection->GetSelectedObjects(AActor::StaticClass(), SelectedActors);
```

**Editor-time feasibility:** YES. This is core editor API.

**Params / Return:**
```json
// Input
{
  "sub_action": "select" | "deselect" | "clear" | "get" | "focus",
  "actors": ["Door_01", "Light_02"],  // by label or name
  "filter": {                          // alternative to explicit list
    "class": "PointLight",
    "tag": "horror_light",
    "sublevel": "Basement",
    "radius": 500,
    "center": [0, 0, 0]
  },
  "add_to_selection": false,  // false = replace, true = add
  "focus_camera": true        // move viewport to selection
}

// Output
{
  "success": true,
  "selected_count": 3,
  "selected_actors": [
    { "name": "PointLight_5", "label": "Horror_Light_01", "class": "PointLight", "location": [...] }
  ]
}
```

**Complexity:** 5-6 hours. Actor resolution by label/name/filter is the main logic. Camera focus is a bonus call.

**Gotchas:**
- `SelectActor` with `bNotify=false` for batch operations, then one `NoteSelectionChange()` at the end
- `CanSelectActor` should be checked first — locked levels prevent selection
- `MoveViewportCamerasToActor` focuses ALL viewports unless `bActiveViewportOnly=true`
- Actor labels vs FName — UE5 has both `GetActorLabel()` (display name) and `GetFName()` (internal). Search both.
- `WarnAboutManyActors` in `SelectNone` can trigger a dialog — pass `false`

---

### 5. `snap_to_surface`

**UE5 API:**
```cpp
// Line trace
UWorld* World = GEditor->GetEditorWorldContext().World();
FHitResult Hit;
World->LineTraceSingleByChannel(Hit, Start, Start + Direction * TraceLength,
    ECC_WorldStatic, Params);

// Normal alignment
FRotationMatrix::MakeFromZX(Hit.ImpactNormal, Actor->GetActorForwardVector());
// or
FQuat AlignQuat = FQuat::FindBetweenNormals(FVector::UpVector, Hit.ImpactNormal);

// Math utilities (RotationMatrix.h)
static TMatrix<T> MakeFromZX(TVector<T> const& ZAxis, TVector<T> const& XAxis);
static TMatrix<T> MakeFromZY(TVector<T> const& ZAxis, TVector<T> const& YAxis);
```

**Editor-time feasibility:** YES. Line traces work in editor worlds.

**Params / Return:**
```json
// Input
{
  "actors": ["Lamp_01", "Barrel_02"],     // actors to snap
  "direction": [0, 0, -1],                // trace direction, default down
  "trace_length": 10000,                   // default 10000 cm
  "align_to_normal": true,                 // rotate to match surface
  "offset": 0,                             // cm offset from surface
  "channel": "WorldStatic"                 // collision channel
}

// Output
{
  "success": true,
  "results": [
    {
      "actor": "Lamp_01",
      "snapped": true,
      "new_location": [100, 200, 50.5],
      "surface_normal": [0, 0, 1],
      "hit_actor": "Floor_01"
    }
  ]
}
```

**Complexity:** 3-4 hours. Straightforward trace + transform math.

**Gotchas:**
- Must ignore the actor being snapped (add to `FCollisionQueryParams::AddIgnoredActor`)
- For wall-snapping (non-downward), the forward vector alignment matters — need `MakeFromZX` not just `MakeFromZ`
- Actors without collision won't be hit by traces — might need to trace against render geometry
- Multiple actors in a group: snap each independently, or snap as a group preserving relative offsets? Should support both.
- `PostEditChangeProperty` + `PostEditMove` after transform change for undo/redo support

---

## Horror Design (5 actions)

These are primarily **algorithmic composition** layers built on top of existing MonolithMesh analysis actions (`analyze_sightlines`, `find_hiding_spots`, `find_ambush_points`, `analyze_choke_points`, `analyze_escape_routes`, `classify_zone_tension`, `analyze_pacing_curve`, `find_dead_ends`). Minimal new UE API needed.

### 6. `design_encounter`

**Algorithm:** Compose multiple existing analyses into an encounter design recommendation.

**Existing Monolith actions used:**
- `analyze_sightlines` — visibility analysis
- `find_hiding_spots` — player/AI cover
- `find_ambush_points` — AI ambush positions
- `analyze_choke_points` — funneling
- `analyze_escape_routes` — player escape paths
- `classify_zone_tension` — tension scoring
- `place_blueprint_actor` (new P1) — spawn AI + props
- `suggest_patrol_route` (new P1) — AI pathing

**UE API needed:** Only navmesh path queries (see `suggest_patrol_route` below) and actor spawning. All spatial analysis reuses existing raycasting infrastructure.

**Params / Return:**
```json
// Input
{
  "region": { "center": [0, 0, 0], "radius": 2000 },
  "archetype": "stalker" | "patrol" | "ambusher" | "swarm",
  "difficulty": "low" | "medium" | "high",
  "enemy_blueprint": "/Game/AI/BP_Stalker",
  "constraints": {
    "max_enemies": 3,
    "min_escape_routes": 2,
    "max_dead_ends": 0,
    "require_hiding_spots": true
  },
  "dry_run": true  // analyze only, don't spawn
}

// Output
{
  "encounter_score": 0.82,
  "spawn_points": [...],
  "patrol_routes": [...],
  "player_escape_routes": [...],
  "sightline_coverage": 0.65,
  "hiding_spot_count": 4,
  "tension_rating": "high",
  "warnings": ["Dead end at [100, 200, 0] — player could get trapped"],
  "recommendations": ["Add cover near choke point at [300, 0, 0]"]
}
```

**Complexity:** 10-12 hours. Heavy algorithm work composing all sub-analyses. The "design" aspect requires scoring heuristics per archetype.

**Gotchas:**
- This is opinionated design — need sensible defaults per archetype that can be overridden
- "dry_run" mode is critical for iteration
- Should output a JSON "encounter spec" that can be fed back to spawn actors

---

### 7. `suggest_patrol_route`

**UE5 API:**
```cpp
// NavigationSystem.h (line 521, 636)
static UNavigationPath* FindPathToLocationSynchronously(
    UObject* WorldContextObject, const FVector& PathStart,
    const FVector& PathEnd, AActor* PathfindingContext = NULL,
    TSubclassOf<UNavigationQueryFilter> FilterClass = {});

FPathFindingResult FindPathSync(FPathFindingQuery Query,
    EPathFindingMode::Type Mode = EPathFindingMode::Regular);

// Custom costs via FRecastQueryFilter (RecastQueryFilter.h line 22)
class FRecastQueryFilter : public INavigationQueryFilterInterface, public dtQueryFilter
{
    virtual void SetAreaCost(uint8 AreaType, float Cost) override;
};

// NavMesh random point
FNavLocation RandomPoint;
NavSys->GetRandomReachablePointInRadius(Origin, Radius, RandomPoint);
```

**Editor-time feasibility:** YES, **but requires navmesh to be built first**. `FindPathSync` and `FindPathToLocationSynchronously` work at editor time if navmesh data exists. This creates a natural dependency on `build_navmesh`.

**Params / Return:**
```json
// Input
{
  "archetype": "stalker" | "patrol" | "ambusher",
  "region": { "center": [0, 0, 0], "radius": 3000 },
  "waypoint_count": 5,
  "patrol_style": "loop" | "back_and_forth" | "random",
  "constraints": {
    "avoid_well_lit": true,
    "prefer_cover": true,
    "max_open_exposure": 500,  // cm of open sightline
    "stay_near_walls": true
  }
}

// Output
{
  "success": true,
  "route": {
    "waypoints": [
      { "location": [100, 200, 0], "wait_time": 3.0, "look_direction": [1, 0, 0] },
      ...
    ],
    "total_distance": 2400,
    "estimated_patrol_time": 45.0,
    "exposure_score": 0.3,
    "tension_contribution": "medium"
  }
}
```

**Complexity:** 8-10 hours. Navmesh pathfinding is easy; the intelligence is in waypoint selection heuristics (prefer dark areas, near walls, avoiding player sightlines).

**Gotchas:**
- **Navmesh must exist** — fail gracefully with message to run `build_navmesh` first
- `FindPathToLocationSynchronously` allocates a `UNavigationPath` that needs GC — use `FindPathSync` (stack result) instead
- Custom area costs via `FRecastQueryFilter` require knowing the area class IDs — need to enumerate `UNavArea` subclasses
- For "prefer cover" routing, combine with existing `find_hiding_spots` output
- Route loops need the last→first segment to also be valid
- Consider exposing raw waypoint editing for human refinement

---

### 8. `analyze_ai_territory`

**Algorithm:** Score a region by combining existing analyses.

**Composition:**
- `find_hiding_spots` — density of AI hiding positions
- `suggest_patrol_route` (new) — patrol route quality
- `analyze_sightlines` — AI sightline coverage vs player approach angles
- `find_ambush_points` — ambush opportunity count
- `analyze_choke_points` — control points
- `analyze_escape_routes` — player escape difficulty (higher = better territory for AI)

**UE API needed:** Same navmesh + raycasting as above. No new API.

**Params / Return:**
```json
// Input
{
  "region": { "center": [500, 0, 100], "radius": 2000 },
  "archetype": "stalker",
  "granularity": 200  // grid cell size for heatmap
}

// Output
{
  "territory_score": 0.78,
  "breakdown": {
    "hiding_density": 0.85,
    "patrol_coverage": 0.72,
    "sightline_control": 0.65,
    "ambush_potential": 0.90,
    "choke_control": 0.80,
    "player_escape_difficulty": 0.60
  },
  "heatmap": [  // per-cell scores
    { "cell": [0, 0], "score": 0.9, "dominant_factor": "ambush" },
    ...
  ],
  "recommendations": ["Add cover at [100, 300, 0] to improve patrol options"]
}
```

**Complexity:** 6-8 hours. Mainly scoring algorithm + grid sampling.

**Gotchas:**
- Granularity affects performance quadratically — need sensible limits (min 100cm cells)
- Archetype-specific weights: stalker values hiding spots, patrol values route variety, ambusher values sightlines
- Can reuse the `classify_zone_tension` infrastructure for per-cell scoring

---

### 9. `evaluate_safe_room`

**Algorithm:** Analyze room properties relevant to player safety. Primarily geometric analysis.

**UE API needed:**
- Line traces for entrance counting (existing infrastructure)
- Existing `analyze_sightlines` for visibility into room
- Existing lighting analysis from `MonolithMeshLightingActions`
- Existing acoustics from `MonolithMeshAudioActions`

**Params / Return:**
```json
// Input
{
  "room": { "center": [0, 0, 100], "bounds": [400, 600, 300] },
  // OR
  "actors": ["SafeRoom_Volume_01"]  // use a volume's bounds
}

// Output
{
  "safe_room_score": 0.85,
  "entrance_count": 2,
  "entrances": [
    { "location": [200, 0, 0], "width": 120, "has_door": true, "can_barricade": true }
  ],
  "lighting": {
    "average_lux": 150,
    "lit_percentage": 0.90,
    "rating": "well_lit"
  },
  "sound_isolation": {
    "absorption_score": 0.7,
    "exterior_noise_penetration": "low"
  },
  "size": {
    "floor_area_sqm": 24.0,
    "rating": "comfortable"  // cramped < 10, comfortable 10-30, spacious > 30
  },
  "defensibility": {
    "score": 0.80,
    "blind_spots": 1,
    "max_sightline_to_entrance": 800
  },
  "hospice_accessibility": {
    "meets_rest_point_criteria": true,
    "enough_space_for_wheelchair": true
  },
  "warnings": ["Second entrance has no door — enemies can enter freely"]
}
```

**Complexity:** 6-8 hours. Combines lighting, acoustics, and spatial analysis already in MonolithMesh.

**Gotchas:**
- Entrance detection is the hard part: need to find gaps in walls via raycasting or nav link detection
- "Has door" detection: check for actors with Door/Gate tags or BP class near entrance location
- Lighting data may not be fully baked at analysis time — use approximate light sampling
- Sound isolation scoring needs the acoustic system from `MonolithMeshAudioActions`

---

### 10. `analyze_level_pacing_structure`

**Algorithm:** Macro-level extension of existing `analyze_pacing_curve`. Walk the player's expected path through the entire level and map tension zones.

**UE API needed:**
- Navmesh pathfinding (same as `suggest_patrol_route`) for player path estimation
- All existing horror analysis actions for per-zone scoring
- Level streaming info for sublevel boundaries

**Params / Return:**
```json
// Input
{
  "start": [0, 0, 0],        // level start point
  "end": [5000, 3000, 0],    // level end/boss room
  "waypoints": [...],         // optional intermediate points
  "sample_interval": 500      // score every 500cm along path
}

// Output
{
  "pacing_curve": [
    { "distance": 0, "tension": 0.2, "zone": "safe_start", "location": [...] },
    { "distance": 500, "tension": 0.35, "zone": "exploration" },
    { "distance": 1500, "tension": 0.8, "zone": "encounter_01" },
    { "distance": 2000, "tension": 0.3, "zone": "safe_room" },
    ...
  ],
  "structure": {
    "total_distance": 8000,
    "tension_peaks": 4,
    "rest_zones": 3,
    "average_tension": 0.55,
    "max_sustained_tension_distance": 1200,  // longest stretch above 0.7
    "tension_variety": 0.8  // 0=flat, 1=highly varied
  },
  "hospice_assessment": {
    "rest_frequency_adequate": true,
    "max_tension_duration_ok": true,
    "cognitive_load_rating": "moderate",
    "recommendations": ["Add rest zone between distances 3000-4000"]
  },
  "warnings": [
    "Tension plateau from 1500-2500 — no relief for 1000cm",
    "No safe room in second half of level"
  ]
}
```

**Complexity:** 8-10 hours. Path estimation + per-sample tension scoring + structural analysis.

**Gotchas:**
- Player path estimation without explicit waypoints is hard — use navmesh shortest path as baseline
- "Expected path" may branch — consider main path only, or add branch analysis
- Sublevel boundaries may affect navmesh continuity — check for nav links across boundaries
- Hospice assessment ties into existing accessibility actions — reuse `MonolithMeshAccessibilityActions`
- Should produce a visual-friendly format (distance vs tension) for charting

---

## Tech Art (5 actions)

### 11. `import_mesh`

**UE5 API:**
```cpp
// IAssetTools.h (line 522, 531)
// Fully automated import (no dialogs):
virtual TArray<UObject*> ImportAssetsAutomated(
    const UAutomatedAssetImportData* ImportData) = 0;

// Manual import with file list:
virtual TArray<UObject*> ImportAssets(
    const TArray<FString>& Files, const FString& DestinationPath,
    UFactory* ChosenFactory = NULL, bool bSyncToBrowser = true,
    TArray<TPair<FString, FString>>* FilesAndDestinations = nullptr,
    bool bAllowAsyncImport = false, bool bSceneImport = false) const = 0;

// UAutomatedAssetImportData (AutomatedAssetImportData.h)
UPROPERTY() TArray<FString> Filenames;
UPROPERTY() FString DestinationPath;
UPROPERTY() FString FactoryName;  // "FbxFactory", "glTFFactory"
UPROPERTY() bool bReplaceExisting;
UPROPERTY() bool bSkipReadOnly;
UPROPERTY() TObjectPtr<UFactory> Factory;

// Access:
FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
IAssetTools& AssetTools = AssetToolsModule.Get();
```

**Editor-time feasibility:** YES. `ImportAssetsAutomated` is designed for exactly this — headless batch import.

**Params / Return:**
```json
// Input
{
  "files": ["D:/Assets/chair.fbx", "D:/Assets/table.glb"],
  "destination": "/Game/Meshes/Furniture",
  "replace_existing": true,
  "factory": "auto",  // auto-detect, or "FbxFactory" / "InterchangeGltfFactory"
  "import_settings": {
    "combine_meshes": true,
    "generate_lightmap_uvs": true,
    "auto_generate_collision": true,
    "normal_import_method": "ImportNormalsAndTangents",
    "material_import": "create_new" | "find_existing" | "skip"
  }
}

// Output
{
  "success": true,
  "imported": [
    {
      "source": "D:/Assets/chair.fbx",
      "asset_path": "/Game/Meshes/Furniture/chair",
      "type": "StaticMesh",
      "vertex_count": 1200,
      "triangle_count": 800,
      "material_slots": ["Wood", "Metal"]
    }
  ],
  "warnings": ["table.glb: 2 materials created"]
}
```

**Complexity:** 8-10 hours. The import settings configuration is extensive (FBX has dozens of options via `UFbxImportUI`).

**Gotchas:**
- FBX import options are on `UFbxImportUI` — need to create one and configure before import
- glTF uses Interchange pipeline — different settings path (`UInterchangeGenericStaticMeshPipeline`)
- `ImportAssetsAutomated` returns `TArray<UObject*>` — can be StaticMesh, SkeletalMesh, AnimSequence etc.
- File paths must be absolute and accessible from the editor process
- Large meshes can take significant time — consider async import (`bAllowAsyncImport = true`)
- `bReplaceExisting` can fail if the asset is in use — need to handle gracefully
- Post-import: may want to auto-set collision, lightmap res — chain with other P1 actions

---

### 12. `analyze_material_cost_in_region`

**UE5 API:**
```cpp
// Get material from mesh component
UMaterialInterface* Mat = MeshComp->GetMaterial(SlotIndex);

// Get material resource for instruction counts
FMaterialResource* Resource = Mat->GetMaterialResource(
    GMaxRHIShaderPlatform, EMaterialQualityLevel::High);

// Instruction count (MaterialShared.h line 1582)
uint32 MaxInstructions = Resource->GetMaxNumInstructionsForShader(ShaderType);

// Compile errors
const TArray<FString>& Errors = Resource->GetCompileErrors(); // line 2561

// Cached expression data
const FMaterialCachedExpressionData& ExprData = Mat->GetCachedExpressionData();
```

**Editor-time feasibility:** YES. Material resources and compilation stats are available at editor time.

**Cross-module:** This action reads mesh placements (MonolithMesh spatial queries) and calls material inspection (MonolithMaterial's `get_compilation_stats`). Two approaches:
1. Direct cross-module call within the plugin (preferred — one C++ call chain)
2. Composition at the action level (call both modules from one handler)

**Params / Return:**
```json
// Input
{
  "region": { "center": [0, 0, 0], "radius": 3000 },
  // OR
  "actors": ["Wall_01", "Floor_Tile_*"]  // wildcard support
}

// Output
{
  "total_unique_materials": 12,
  "total_material_instances": 45,
  "total_instruction_count": 3400,
  "hotspots": [
    {
      "material": "/Game/Materials/M_Water",
      "instruction_count": 450,
      "used_by_actors": 8,
      "shader_model": "SM5",
      "warnings": ["High instruction count — consider LOD material"]
    }
  ],
  "per_actor": [
    {
      "actor": "Wall_01",
      "materials": ["M_Brick"],
      "total_instructions": 120
    }
  ],
  "budget_assessment": {
    "rating": "moderate",
    "heaviest_material": "/Game/Materials/M_Water",
    "recommendation": "Replace M_Water with baked normal map version for distant meshes"
  }
}
```

**Complexity:** 6-8 hours. Spatial query + material iteration + stats aggregation.

**Gotchas:**
- `GetMaterialResource` may return null for uncompiled materials
- Material instances inherit from parent — need to resolve the full chain
- `GetMaxNumInstructionsForShader` needs a specific `FShaderType*` — iterate over vertex/pixel shaders
- 5.7 deprecates the `ERHIFeatureLevel` overload of `GetMaterialResource` — use `EShaderPlatform` version
- Large regions with many actors can be slow — consider caching material stats

---

### 13. `fix_mesh_quality`

**UE5 API (GeometryScript):**
```cpp
// MeshRepairFunctions.h — UGeometryScriptLibrary_MeshRepairFunctions
static UDynamicMesh* RepairMeshDegenerateGeometry(
    UDynamicMesh* TargetMesh, FGeometryScriptDegenerateTriangleOptions Options, ...);

static UDynamicMesh* WeldMeshEdges(
    UDynamicMesh* TargetMesh, FGeometryScriptWeldEdgesOptions WeldOptions, ...);

static UDynamicMesh* RemoveSmallComponents(
    UDynamicMesh* TargetMesh, FGeometryScriptRemoveSmallComponentOptions Options, ...);

static UDynamicMesh* FillAllMeshHoles(
    UDynamicMesh* TargetMesh, FGeometryScriptFillHolesOptions FillOptions,
    int32& NumFilledHoles, int32& NumFailedHoleFills, ...);

static UDynamicMesh* ResolveMeshTJunctions(
    UDynamicMesh* TargetMesh, FGeometryScriptResolveTJunctionOptions Options, ...);

static UDynamicMesh* SplitMeshBowties(
    UDynamicMesh* TargetMesh, bool bMeshBowties, bool bAttributeBowties, ...);

static UDynamicMesh* CompactMesh(UDynamicMesh* TargetMesh, ...);

// MeshNormalsFunctions.h
static UDynamicMesh* RecomputeNormals(UDynamicMesh* TargetMesh, ...);
static UDynamicMesh* ComputeTangents(UDynamicMesh* TargetMesh, ...);

// Options structs:
FGeometryScriptDegenerateTriangleOptions {
    EGeometryScriptRepairMeshMode Mode; // DeleteOnly, RepairOrDelete, RepairOrSkip
    double MinTriangleArea = 0.001;
    double MinEdgeLength = 0.0001;
    bool bCompactOnCompletion = true;
};

FGeometryScriptWeldEdgesOptions {
    float Tolerance = 1e-06f;
    bool bOnlyUniquePairs = true;
};

FGeometryScriptRemoveSmallComponentOptions {
    float MinVolume = 0.0001;
    float MinArea = 0.0001;
    int MinTriangleCount = 1;
};
```

**Editor-time feasibility:** YES. GeometryScript operates on `UDynamicMesh` — need to convert StaticMesh to/from DynamicMesh.

**Pipeline:**
1. Load `UStaticMesh` asset
2. Convert to `UDynamicMesh` via `UGeometryScriptLibrary_MeshAssetFunctions::CopyMeshFromStaticMesh`
3. Run repair chain: degenerate removal → weld edges → fill holes → T-junctions → bowties → recompute normals → compact
4. Copy back via `CopyMeshToStaticMesh`

**Params / Return:**
```json
// Input
{
  "mesh": "/Game/Meshes/Broken_Chair",
  "operations": ["remove_degenerate", "weld_edges", "fill_holes", "recompute_normals"],
  // OR
  "operations": "auto",  // run all applicable
  "settings": {
    "weld_tolerance": 0.001,
    "min_triangle_area": 0.001,
    "min_edge_length": 0.0001,
    "fill_method": "Automatic",
    "repair_mode": "RepairOrDelete"
  },
  "dry_run": false  // true = report only
}

// Output
{
  "success": true,
  "original": { "vertices": 1500, "triangles": 800, "open_edges": 12 },
  "repaired": { "vertices": 1480, "triangles": 790, "open_edges": 0 },
  "operations_applied": [
    { "operation": "remove_degenerate", "removed": 10 },
    { "operation": "weld_edges", "welded": 24 },
    { "operation": "fill_holes", "filled": 3, "failed": 0 },
    { "operation": "recompute_normals", "applied": true }
  ]
}
```

**Complexity:** 8-10 hours. GeometryScript pipeline + DynamicMesh conversion + write-back.

**Gotchas:**
- `CopyMeshFromStaticMesh` / `CopyMeshToStaticMesh` — verify these exist in 5.7 GeometryScripting module
- The write-back to StaticMesh modifies the asset — must save package
- LODs: only processes LOD0 unless explicitly handling other LODs
- Nanite meshes have different internal representation — GeometryScript may not handle Nanite data
- `UDynamicMesh` is transient — create via `NewObject<UDynamicMesh>(GetTransientPackage())`
- Extends existing `analyze_mesh_quality` — should chain: analyze first, then suggest/apply fixes
- Module dependency on `GeometryScriptingCore`

---

### 14. `set_mesh_collision`

**UE5 API:**
```cpp
// StaticMesh.h
ENGINE_API void UStaticMesh::CreateBodySetup();
void SetBodySetup(UBodySetup* InBodySetup);
TObjectPtr<class UBodySetup> BodySetup; // member, line 1238

// BodySetup.h
struct FKAggregateGeom AggGeom;  // contains collision shapes
void AddCollisionFrom(const FKAggregateGeom& FromAggGeom);

// AggregateGeom.h — FKAggregateGeom members:
TArray<FKSphereElem> SphereElems;
TArray<FKBoxElem> BoxElems;
TArray<FKSphylElem> SphylElems;   // capsules
TArray<FKConvexElem> ConvexElems;
TArray<FKTaperedCapsuleElem> TaperedCapsuleElems;
TArray<FKLevelSetElem> LevelSetElems;

// BodySetupCore.h
TEnumAsByte<enum ECollisionTraceFlag> CollisionTraceFlag;

// BodySetupEnums.h — ECollisionTraceFlag
CTF_UseDefault,
CTF_UseSimpleAndComplex,
CTF_UseSimpleAsComplex,
CTF_UseComplexAsSimple

// Auto collision generation
UBodySetup->bAutoGenerateCollision = true;
UStaticMesh->Build(true); // rebuild with collision
```

**Editor-time feasibility:** YES. All BodySetup manipulation is editor-time.

**Params / Return:**
```json
// Input
{
  "mesh": "/Game/Meshes/Table",
  "sub_action": "set" | "auto" | "clear" | "add_shape",

  // set — replace all collision
  "collision_type": "simple_and_complex" | "simple_as_complex" | "complex_as_simple" | "default",

  // auto — auto-generate
  "auto_convex_count": 4,
  "auto_max_verts_per_hull": 16,

  // add_shape — add primitive
  "shape": {
    "type": "box" | "sphere" | "capsule" | "convex",
    "location": [0, 0, 50],
    "rotation": [0, 0, 0],
    "extent": [50, 50, 50]  // half-extents for box, radius for sphere
  },

  // clear — remove all collision
}

// Output
{
  "success": true,
  "collision_trace_flag": "CTF_UseSimpleAndComplex",
  "simple_shapes": {
    "boxes": 1,
    "spheres": 0,
    "capsules": 0,
    "convex_hulls": 4
  }
}
```

**Complexity:** 6-8 hours. Multiple sub-actions, shape creation, auto-convex decomposition.

**Gotchas:**
- After modifying BodySetup: `Mesh->Build(true)` to rebuild physics, then `Mesh->MarkPackageDirty()`
- Auto convex decomposition uses `UBodySetup::CreatePhysicsMeshes()` — can be slow for complex meshes
- `FKConvexElem` has vertex data — for custom convex shapes, need to provide vertex arrays
- Convex decomposition params are on `UBodySetup`: `bAutoGenerateCollision`, then call `CreatePhysicsMeshesAsync`
- For "complex as simple", no simple shapes are needed — just set the flag
- Must handle StaticMesh vs SkeletalMesh (SkeletalMesh has per-bone collision)

---

### 15. `analyze_lightmap_density`

**UE5 API:**
```cpp
// StaticMesh.h
int32 UStaticMesh::GetLightMapResolution() const;
void UStaticMesh::SetLightMapResolution(int32 InLightMapResolution);
int32 UStaticMesh::GetLightMapCoordinateIndex() const;
void UStaticMesh::SetLightMapCoordinateIndex(int32 InLightMapCoordinateIndex);

// Per-component override
UStaticMeshComponent::bOverrideLightMapRes;
UStaticMeshComponent::OverriddenLightMapRes;

// UV area calculation — via GeometryScript or raw FStaticMeshLODResources access
FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
// Iterate UV channel at LightMapCoordinateIndex to compute area
```

**Editor-time feasibility:** YES. All properties are readable at editor time. UV area calculation works on loaded meshes.

**Params / Return:**
```json
// Input
{
  "region": { "center": [0, 0, 0], "radius": 5000 },
  // OR
  "actors": ["Wall_01", "Floor_*"],
  "target_texel_density": 64  // texels per meter (default: 64)
}

// Output
{
  "actors_analyzed": 45,
  "results": [
    {
      "actor": "Wall_01",
      "mesh": "/Game/Meshes/Wall_Large",
      "lightmap_resolution": 64,
      "lightmap_coord_index": 1,
      "uv_area": 0.85,  // 0-1 UV space utilization
      "world_area_sqm": 12.5,
      "texel_density": 32.0,  // texels per world meter
      "rating": "low",  // vs target: low/adequate/high/wasteful
      "recommended_resolution": 128
    }
  ],
  "summary": {
    "total_lightmap_memory_mb": 24.5,
    "under_dense_count": 12,
    "over_dense_count": 3,
    "adequate_count": 30
  }
}
```

**Complexity:** 6-8 hours. UV area calculation from raw mesh data is the main technical challenge.

**Gotchas:**
- Lightmap UV is at `LightMapCoordinateIndex` (usually 1) — must read the correct UV channel
- UV area calculation: sum triangle areas in UV space vs world space → texel density
- Component-level overrides (`OverriddenLightMapRes`) take priority over mesh default
- Leviathan uses Lumen + VSM, not baked lighting — lightmaps are less relevant. But some meshes may still use lightmaps for GI cache. Consider flagging this.
- `GetRenderData()` may return null if mesh isn't loaded — force load first
- Static Lighting is OFF per project config — this action's primary value may be ensuring lightmap UVs exist for potential future use, or for marketplace meshes that need them

---

## Complexity Summary

| # | Action | Category | Hours | New UE API | Risk |
|---|--------|----------|-------|-----------|------|
| 1 | `build_navmesh` | Level | 3-4 | NavigationSystemV1::Build | Low |
| 2 | `manage_sublevel` | Level | 8-10 | UEditorLevelUtils (6 methods) | Medium |
| 3 | `place_blueprint_actor` | Level | 6-8 | AddActor + property reflection | Medium |
| 4 | `select_actors` | Level | 5-6 | SelectActor/SelectNone + camera | Low |
| 5 | `snap_to_surface` | Level | 3-4 | LineTrace + FRotationMatrix | Low |
| 6 | `design_encounter` | Horror | 10-12 | Composition only | High (algorithm) |
| 7 | `suggest_patrol_route` | Horror | 8-10 | NavMesh pathfinding | Medium |
| 8 | `analyze_ai_territory` | Horror | 6-8 | Composition only | Medium (algorithm) |
| 9 | `evaluate_safe_room` | Horror | 6-8 | Composition only | Medium (algorithm) |
| 10 | `analyze_level_pacing_structure` | Horror | 8-10 | NavMesh + composition | Medium |
| 11 | `import_mesh` | Tech Art | 8-10 | ImportAssetsAutomated | Medium |
| 12 | `analyze_material_cost_in_region` | Tech Art | 6-8 | FMaterialResource + spatial | Low |
| 13 | `fix_mesh_quality` | Tech Art | 8-10 | GeometryScript repair suite | Medium |
| 14 | `set_mesh_collision` | Tech Art | 6-8 | UBodySetup + FKAggregateGeom | Medium |
| 15 | `analyze_lightmap_density` | Tech Art | 6-8 | GetLightMapResolution + UV calc | Low |

**Total estimate: 90-110 hours** (TODO estimated ~100h for ~20 actions — we have 15 and hit that range)

## Module Dependencies Required

New module dependencies for MonolithMesh.Build.cs:
- `NavigationSystem` — for `build_navmesh`, `suggest_patrol_route`, `analyze_level_pacing_structure`
- `AssetTools` — for `import_mesh` (may already be present)
- `GeometryScriptingCore` — for `fix_mesh_quality`
- `PhysicsCore` — for `set_mesh_collision` (ECollisionTraceFlag)

Already present (assumed): `UnrealEd`, `Engine`, `CoreUObject`

## Recommended Implementation Order

**Phase 1 — Foundation (enables other actions):**
1. `build_navmesh` (3-4h) — unblocks all navmesh-dependent horror actions
2. `select_actors` (5-6h) — core editor workflow, needed by many
3. `snap_to_surface` (3-4h) — quick win, standalone

**Phase 2 — Content Pipeline:**
4. `import_mesh` (8-10h) — entry point for all mesh content
5. `set_mesh_collision` (6-8h) — complements import
6. `place_blueprint_actor` (6-8h) — actor spawning with properties

**Phase 3 — Analysis:**
7. `analyze_lightmap_density` (6-8h) — standalone analysis
8. `analyze_material_cost_in_region` (6-8h) — cross-module analysis
9. `fix_mesh_quality` (8-10h) — extends analyze_mesh_quality

**Phase 4 — Horror Intelligence:**
10. `manage_sublevel` (8-10h) — level structure management
11. `suggest_patrol_route` (8-10h) — navmesh pathfinding
12. `evaluate_safe_room` (6-8h) — room scoring
13. `analyze_ai_territory` (6-8h) — region scoring
14. `analyze_level_pacing_structure` (8-10h) — macro analysis
15. `design_encounter` (10-12h) — capstone composition action
