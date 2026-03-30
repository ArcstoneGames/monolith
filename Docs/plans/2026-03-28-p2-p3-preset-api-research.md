# MonolithMesh P2/P3/Preset API Feasibility Research

**Date:** 2026-03-28
**Researcher:** research-agent
**Engine:** UE 5.7 (verified against local engine source)

---

## Table of Contents

1. [P2 Actions (~10)](#p2-actions)
2. [P3 Actions (~10)](#p3-actions)
3. [Context-Aware Prop Placement (~6)](#context-aware-prop-placement)
4. [Genre Preset System (~8)](#genre-preset-system)
5. [Priority Matrix](#priority-matrix)

---

## P2 Actions

### 1. `randomize_transforms`

**Purpose:** Apply random offset/rotation/scale variation to a set of actors for organic feel.

**UE5 API:**
- `AActor::SetActorLocation()`, `SetActorRotation()`, `SetActorRelativeScale3D()`
- `FRandomStream` for seeded reproducibility (already used in `scatter_props`)
- `TActorIterator<AActor>` or direct actor label lookup for targeting

**Feasibility:** TRIVIAL. Pure transform math on existing actors. No PIE needed.

**Params:**
```json
{
  "actor_names": ["SM_Bottle_01", "SM_Bottle_02"],  // or "volume_name" for batch
  "volume_name": "optional — randomize all actors in volume",
  "offset_range": [0, 15],          // cm, random XY offset
  "rotation_range": [0, 360],       // yaw, degrees
  "pitch_range": [0, 5],            // tilt, degrees
  "scale_range": [0.9, 1.1],        // uniform scale multiplier
  "seed": 42                         // 0 = random
}
```

**Return:**
```json
{
  "modified_count": 12,
  "actors": [{"name": "SM_Bottle_01", "location": [...], "rotation": [...], "scale": [...]}]
}
```

**Complexity:** 2 hours. Reuse `FScopedMeshTransaction`, `FRandomStream` pattern from `scatter_props`.

**Gotchas:** None significant. Should support both actor-list and volume-based targeting.

---

### 2. `place_spline`

**Purpose:** Create spline-based mesh chains for pipes, cables, railings.

**UE5 API:**
- `USplineComponent` — the core spline. Methods: `SetSplinePoints()`, `SetTangentAtSplinePoint()`, `SetSplinePointType()` (Linear, Curve, Constant, CurveClamped). All editor-safe.
- `USplineMeshComponent` — deforms a mesh along a spline segment. `SetStartAndEnd()`, `SetForwardAxis()`, `SetStartScale()/SetEndScale()`.
- **No `ASplineMeshActor` class in engine.** Must spawn a plain `AActor`, add `USplineComponent` as root, then add `USplineMeshComponent` children per segment.
- `USplineComponent` is in `Engine` module — no extra deps.

**Feasibility:** CONFIRMED. USplineComponent methods are all editor-safe. The pattern is:
1. `SpawnActor<AActor>()` with a transient class or empty BP
2. `NewObject<USplineComponent>(Actor)` → `RegisterComponent()` → `SetSplinePoints()`
3. For each segment pair: `NewObject<USplineMeshComponent>()` → `SetStartAndEnd(Start, StartTangent, End, EndTangent)` → `SetStaticMesh()`
4. Register all components

**Params:**
```json
{
  "points": [[0,0,0], [200,0,100], [400,0,0]],
  "mesh_path": "/Game/Meshes/SM_Pipe_Segment",
  "forward_axis": "X",             // X, Y, or Z
  "point_type": "Curve",           // Linear, Curve, Constant
  "scale": [1.0, 1.0],             // start/end scale
  "close_loop": false,
  "actor_label": "Pipe_Run_01"
}
```

**Return:**
```json
{
  "actor_name": "Pipe_Run_01",
  "segment_count": 2,
  "total_length": 447.2,
  "spline_points": [...]
}
```

**Complexity:** 6 hours. SplineMeshComponent setup requires careful tangent computation. Need to handle forward axis correctly per mesh asset.

**Gotchas:**
- `USplineMeshComponent` forward axis MUST match the mesh's orientation axis or it deforms incorrectly. Need a `forward_axis` param.
- Tangent auto-calculation from Catmull-Rom is the default in USplineComponent. For sharp corners need `Linear` point type.
- Each `USplineMeshComponent` is a separate draw call — for very long splines (50+ segments), consider HISM alternative.
- Must call `RegisterComponent()` on each component after creation.

---

### 3. `get_level_actors`

**Purpose:** Filtered enumeration of actors in the editor world.

**UE5 API:**
- `TActorIterator<AActor>(World)` — iterates all actors. Filter by `IsA()` for class.
- `AActor::Tags` — `TArray<FName>` for tag filtering
- `AActor::GetLevel()->GetOuter()->GetName()` — sublevel name
- `UStaticMeshComponent::GetStaticMesh()->GetPathName()` — mesh path for wildcard matching
- `FWildcardString::IsMatch()` or `FString::MatchesWildcard()` — for wildcard patterns

**Feasibility:** TRIVIAL. All read-only, editor-safe.

**Params:**
```json
{
  "class_filter": "StaticMeshActor",     // optional, UClass name
  "tag_filter": "Monolith.Room:Kitchen", // optional, actor tag contains
  "sublevel_filter": "Kitchen_Props",    // optional, sublevel name
  "mesh_wildcard": "*pipe*",             // optional, mesh asset name wildcard
  "name_wildcard": "SM_Chair*",          // optional, actor label wildcard
  "volume_name": "BlockoutVol_01",       // optional, within volume bounds
  "limit": 100                           // default 100, max 500
}
```

**Return:**
```json
{
  "count": 47,
  "actors": [
    {
      "name": "SM_Chair_01",
      "class": "StaticMeshActor",
      "location": [100, 200, 0],
      "mesh": "/Game/Meshes/SM_Chair",
      "sublevel": "Kitchen_Props",
      "tags": ["Monolith.Room:Kitchen", "furniture"]
    }
  ]
}
```

**Complexity:** 3 hours. Straightforward iteration with multi-filter AND logic.

**Gotchas:** Performance on levels with 10K+ actors — add early-out and cap results. Don't load assets just to check mesh names (use component's existing mesh reference).

---

### 4. `measure_distance`

**Purpose:** Quick measurement between actors, points, or navmesh paths.

**UE5 API:**
- `FVector::Dist(A, B)` — Euclidean distance
- `UNavigationSystemV1::FindPathToLocationSynchronously()` — navmesh path. Returns `UNavigationPath*` with `GetPathPoints()`. Path length = sum of segment distances.
- `UNavigationSystemV1::GetCurrent(World)` — get nav system instance. Works in editor IF navmesh is built.

**Feasibility:** CONFIRMED. `FindPathToLocationSynchronously()` works in editor. The existing MonolithMesh audio code already uses `MonolithMeshAcoustics::FindIndirectNavmeshPath()` which calls navmesh queries successfully.

**Params:**
```json
{
  "from": "SM_Door_01",             // actor name or [x,y,z] array
  "to": "SM_Door_02",               // actor name or [x,y,z] array
  "include_navmesh_path": true       // also compute navmesh path distance
}
```

**Return:**
```json
{
  "euclidean_distance": 523.4,
  "navmesh_distance": 891.2,
  "navmesh_available": true,
  "navmesh_path_points": [[100,200,0], [300,400,0], [500,200,0]],
  "height_difference": 12.5
}
```

**Complexity:** 2 hours. Reuse navmesh pattern from `MonolithMeshAcoustics::FindIndirectNavmeshPath()`.

**Gotchas:** Navmesh must be built. Return `"navmesh_available": false` if no navdata. Actor name resolution already exists in `get_actor_info`.

---

### 5. `create_prefab` / `spawn_prefab`

**Purpose:** Save a group of actors as a reusable Level Instance, spawn copies.

**UE5 API:**
- `ALevelInstance` — the actor class representing an instance of a sub-level. Supports editing via `ILevelInstanceInterface::EnterEdit()`.
- `ULevelInstanceSubsystem` — `GetSubsystem<ULevelInstanceSubsystem>(World)`. Has `CreateLevelInstanceFrom()` which takes a set of actors and creates a Level Instance from them.
- `APackedLevelActor` — a baked/flattened variant (no nested editing). Inherits `ALevelInstance`.
- `UEditorLevelUtils` — actor operations across levels.

**Key method:** `ULevelInstanceSubsystem::CreateLevelInstanceFrom(const TArray<AActor*>& InActors, const FNewLevelInstanceParams& InParams)` — this is the core API that packages actors into a Level Instance.

**Feasibility:** CONFIRMED with caveats. `CreateLevelInstanceFrom` is an editor-only API (guarded by `WITH_EDITOR`). It creates a new level asset on disk. For `spawn_prefab`, use `World->SpawnActor<ALevelInstance>()` and set its `WorldAsset` to the saved level.

**Params (create_prefab):**
```json
{
  "actor_names": ["SM_Desk_01", "SM_Chair_01", "SM_Monitor_01"],
  "prefab_name": "Office_Desk_Setup",
  "save_path": "/Game/Prefabs/Office_Desk_Setup",
  "type": "level_instance"          // "level_instance" or "packed"
}
```

**Params (spawn_prefab):**
```json
{
  "prefab_path": "/Game/Prefabs/Office_Desk_Setup",
  "location": [1000, 500, 0],
  "rotation": [0, 90, 0],
  "scale": [1, 1, 1],
  "actor_label": "OfficeDesk_Copy_01"
}
```

**Return:**
```json
{
  "actor_name": "OfficeDesk_Copy_01",
  "prefab_path": "/Game/Prefabs/Office_Desk_Setup",
  "actor_count": 3,
  "bounds_size": [200, 150, 120]
}
```

**Complexity:** 8 hours. `ULevelInstanceSubsystem` API is complex — must handle level asset creation, actor migration, and proper undo support. `spawn_prefab` is simpler (just SpawnActor + set asset).

**Gotchas:**
- `CreateLevelInstanceFrom` moves the source actors INTO the new level — they disappear from the current level. User must understand this.
- Level Instance requires a saved `.umap` file on disk. Can't be purely transient.
- Packed Level Actors don't support nested editing but are more performant.
- Need `LevelInstanceEditor` module dependency for some subsystem features.
- 5.7 deprecates some streaming grid properties on HLODLayer — not directly relevant but level instances interact with World Partition.

---

### 6. `generate_scare_sequence`

**Purpose:** Algorithmically compose a multi-step scare sequence with escalation.

**UE5 API:** Pure algorithmic — no specific UE API. Uses existing Monolith horror analysis data (sightlines, tension classification, hiding spots) to compose sequences.

**Feasibility:** CONFIRMED. This is algorithmic composition, not engine API dependent. Uses data from existing Phase 6 horror actions (`analyze_sightlines`, `find_hiding_spots`, `find_ambush_points`, `classify_zone_tension`) to generate a sequence plan.

**Params:**
```json
{
  "path_points": [[0,0,0], [500,0,0], [1000,0,0], [1500,0,0]],
  "style": "slow_burn",             // "slow_burn", "escalating", "relentless", "single_peak"
  "intensity_cap": 0.7,             // hospice mode cap
  "scare_types": ["audio", "visual", "environmental"],  // allowed types
  "min_spacing_cm": 500,            // minimum distance between events
  "count": 5                        // number of scare events
}
```

**Return:**
```json
{
  "events": [
    {
      "position": [200, 50, 0],
      "type": "audio",
      "intensity": 0.3,
      "description": "Distant metal scraping — establishes unease",
      "timing": "player_proximity",
      "sightline_blocked": true
    },
    {
      "position": [600, -100, 0],
      "type": "environmental",
      "intensity": 0.5,
      "description": "Door slams shut behind player — isolation",
      "timing": "player_passed",
      "escape_routes": 2
    }
  ],
  "tension_curve": [0.1, 0.3, 0.3, 0.5, 0.7, 0.5, 0.3],
  "style": "slow_burn",
  "hospice_safe": true
}
```

**Complexity:** 6 hours. The challenge is encoding good horror design principles into algorithms, not engine API.

**Gotchas:** Quality depends on navmesh being built (for path validation) and existing horror analysis data. Should cross-reference with `analyze_pacing_curve` output.

---

### 7. `analyze_framing`

**Purpose:** Score camera composition quality from a viewpoint.

**UE5 API:**
- `FSceneView` / `FMinimalViewInfo` — camera projection math
- `UWorld::LineTraceSingleByChannel()` — ray casting for leading lines detection
- `FVector::ProjectOnTo()` — projection math
- Rule of thirds, golden ratio, leading lines — all pure math on screen-space projection

**Feasibility:** CONFIRMED. All math-based using existing trace infrastructure. The approach:
1. Project scene actors to screen space via `FSceneView::ProjectWorldToScreen()`
2. Compute rule-of-thirds scoring (distance of focal points from power points)
3. Cast rays in screen-space grid pattern to detect leading lines (edges that converge toward subject)
4. Score depth layering (foreground/midground/background population)

**Params:**
```json
{
  "camera_location": [0, 0, 170],
  "camera_rotation": [0, 45, 0],
  "fov": 90,
  "focal_actor": "SM_Monster_01",    // optional — what should be the subject?
  "aspect_ratio": 1.777              // 16:9 default
}
```

**Return:**
```json
{
  "overall_score": 0.72,
  "rule_of_thirds": {
    "score": 0.85,
    "focal_point_screen_pos": [0.33, 0.66],
    "nearest_power_point": [0.333, 0.666],
    "distance_to_power_point": 0.012
  },
  "leading_lines": {
    "score": 0.6,
    "detected_lines": 3,
    "lines_toward_subject": 2
  },
  "depth_layering": {
    "score": 0.7,
    "foreground_coverage": 0.15,
    "midground_coverage": 0.45,
    "background_coverage": 0.4
  },
  "negative_space": 0.35,
  "suggestions": ["Shift camera 20cm right to place subject on left third line"]
}
```

**Complexity:** 6 hours. Screen-space projection is well-understood. Leading line detection is the most complex part (edge detection via trace grid).

**Gotchas:** Need to construct a temporary `FSceneView` from the provided camera params — this is done via `FSceneViewInitOptions` which doesn't need a viewport. Works in editor without PIE.

---

### 8. `evaluate_monster_reveal`

**Purpose:** Score the quality of a monster reveal from the player's perspective.

**UE5 API:** Same as `analyze_framing` plus:
- Silhouette analysis: project monster bounds to screen, measure screen-space coverage
- Backlight detection: trace from monster toward light sources, check if light is behind monster relative to camera
- Partial visibility: what percentage of the monster mesh bounds is occluded

**Feasibility:** CONFIRMED. Builds on framing analysis infrastructure.

**Params:**
```json
{
  "player_location": [0, 0, 170],
  "player_rotation": [0, 45, 0],
  "monster_actor": "SM_Creature_01",
  "light_actors": ["PointLight_01"],  // optional, auto-detect nearby lights
  "fov": 90
}
```

**Return:**
```json
{
  "overall_score": 0.78,
  "silhouette": {
    "screen_coverage": 0.12,
    "aspect_ratio": 1.8,
    "is_recognizable": true,
    "score": 0.8
  },
  "backlight": {
    "has_backlight": true,
    "rim_light_intensity": 0.6,
    "score": 0.9
  },
  "distance": {
    "distance_cm": 800,
    "optimal_range": [500, 1200],
    "score": 0.85
  },
  "partial_visibility": {
    "visible_percentage": 0.65,
    "occluding_actors": ["SM_Pillar_03"],
    "score": 0.7
  },
  "framing_score": 0.72,
  "suggestions": ["Strong backlight. Move creature 50cm left for better partial occlusion behind pillar."]
}
```

**Complexity:** 5 hours. Extends framing analysis. Backlight detection via light actor position + traces.

**Gotchas:** Light detection should auto-find nearby point/spot lights within a radius if none specified. Silhouette "recognizability" is heuristic (aspect ratio, convexity of screen-space hull).

---

### 9. `validate_horror_intensity`

**Purpose:** Cap tension for hospice mode, flag/remove jump scares.

**UE5 API:** Algorithmic — uses existing `classify_zone_tension` and `analyze_pacing_curve` data.

**Feasibility:** CONFIRMED. Pure analysis pass over existing horror data.

**Params:**
```json
{
  "volume_name": "Level_Section_A",    // optional, or entire level
  "intensity_cap": 0.6,                // hospice default
  "flag_jump_scares": true,
  "flag_rapid_intensity_changes": true,
  "min_rest_duration_cm": 800,         // minimum low-tension distance between peaks
  "mode": "audit"                      // "audit" (report only) or "fix" (auto-adjust)
}
```

**Return:**
```json
{
  "violations": [
    {
      "type": "intensity_over_cap",
      "location": [500, 200, 0],
      "current_intensity": 0.85,
      "cap": 0.6,
      "actors_involved": ["TriggerBox_Scare_03"],
      "suggestion": "Reduce encounter density or add breather zone"
    },
    {
      "type": "insufficient_rest",
      "from_location": [1000, 200, 0],
      "to_location": [1200, 200, 0],
      "rest_distance": 350,
      "minimum_required": 800
    }
  ],
  "overall_hospice_safe": false,
  "violation_count": 2,
  "tension_curve_summary": {"avg": 0.45, "max": 0.85, "min": 0.1}
}
```

**Complexity:** 4 hours. Analysis logic over existing data. The "fix" mode would need to modify trigger volumes or adjust zone properties.

**Gotchas:** "Fix" mode is complex — start with "audit" only. What counts as a "jump scare" needs clear definition (rapid intensity spike > 0.4 within short distance).

---

### 10. `generate_hospice_report`

**Purpose:** Comprehensive accessibility audit for hospice patients.

**UE5 API:** Combines data from multiple existing actions plus new checks:
- `classify_zone_tension` — tension mapping
- `analyze_pacing_curve` — pacing analysis
- Cognitive load estimation (number of unique mechanics active in an area)
- One-handed play analysis: check all required inputs, identify simultaneous multi-button combos
- Rest area spacing and quality

**Feasibility:** CONFIRMED. Mostly aggregation of existing data plus some new heuristics.

**Params:**
```json
{
  "level_name": "Hospital_Ward_01",    // or current level
  "include_intensity": true,
  "include_rest_spacing": true,
  "include_cognitive_load": true,
  "include_mobility_analysis": true,
  "include_visual_accessibility": true
}
```

**Return:**
```json
{
  "overall_grade": "B+",
  "sections": {
    "intensity": {
      "grade": "A",
      "max_intensity": 0.55,
      "hospice_cap": 0.6,
      "violations": 0
    },
    "rest_spacing": {
      "grade": "B",
      "avg_rest_distance": 950,
      "min_rest_distance": 600,
      "recommended_min": 800,
      "rest_areas_found": 4
    },
    "cognitive_load": {
      "grade": "B+",
      "max_simultaneous_mechanics": 3,
      "recommended_max": 2,
      "complex_areas": [{"location": [...], "mechanics": ["combat", "puzzle", "navigation"]}]
    },
    "visual_accessibility": {
      "grade": "A-",
      "min_contrast_ratio": 3.8,
      "recommended_min": 4.5,
      "dark_areas": [{"location": [...], "avg_lux": 12}]
    }
  },
  "recommendations": [
    "Add a rest area between zones B and C (currently 1200cm without low-tension space)",
    "Increase ambient light in corridor at [800,200,0] — current level may be too dark for visibility"
  ]
}
```

**Complexity:** 8 hours. Aggregation framework + new heuristics. Most complex part is defining the grading rubric.

**Gotchas:** Cognitive load requires knowing what gameplay mechanics are active in each area — this can't be purely spatial. Consider reading actor tags or Blueprint properties for mechanic type classification.

---

## P3 Actions

### 11. `validate_naming_conventions`

**Purpose:** Check asset names against a convention (prefix-based: SM_, M_, T_, BP_, etc.)

**UE5 API:**
- `IAssetRegistry::GetAllAssets()` — enumerate all project assets
- `FAssetData::AssetName`, `AssetClassPath` — for checking prefix vs. class
- Convention rules are pure string matching

**Feasibility:** TRIVIAL. Read-only asset registry query.

**Params:**
```json
{
  "path_filter": "/Game/Meshes",      // optional, scope to folder
  "convention": "epic_standard",       // or custom rules
  "custom_rules": {                    // optional
    "StaticMesh": "SM_",
    "Material": "M_",
    "MaterialInstance": "MI_",
    "Texture2D": "T_",
    "Blueprint": "BP_",
    "SkeletalMesh": "SK_"
  }
}
```

**Return:**
```json
{
  "total_checked": 450,
  "violations": [
    {"asset": "/Game/Meshes/Chair_Wood", "class": "StaticMesh", "expected_prefix": "SM_", "suggested_name": "SM_Chair_Wood"}
  ],
  "violation_count": 23,
  "compliance_percentage": 94.8
}
```

**Complexity:** 2 hours. Pure string matching on asset registry data.

**Gotchas:** Need sensible defaults for common conventions. Should support custom rules.

---

### 12. `batch_rename_assets`

**Purpose:** Rename multiple assets with redirect fixup.

**UE5 API:**
- `IAssetTools::RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames)` — the core API.
  - Defined in `Developer/AssetTools/Public/IAssetTools.h`
  - `FAssetRenameData(WeakObjectPtr<UObject> Asset, FString NewPackagePath, FString NewName)`
  - Handles redirector creation and reference fixup automatically.
  - Returns `bool` success.
- `FAssetToolsModule::Get()` — access the asset tools module.
- Also: `IAssetTools::RenameAssetsWithDialog()` for confirmation UI (not wanted for MCP).

**Feasibility:** CONFIRMED. `RenameAssets()` is a well-established editor API. Works without PIE. Creates redirectors for all references.

**Params:**
```json
{
  "renames": [
    {"old_path": "/Game/Meshes/Chair_Wood", "new_name": "SM_Chair_Wood"},
    {"old_path": "/Game/Meshes/Table_Metal", "new_name": "SM_Table_Metal", "new_folder": "/Game/Meshes/Furniture"}
  ],
  "fix_redirectors": true,          // fixup references (default true)
  "dry_run": false                  // if true, just report what would change
}
```

**Return:**
```json
{
  "renamed_count": 2,
  "results": [
    {"old_path": "/Game/Meshes/Chair_Wood", "new_path": "/Game/Meshes/SM_Chair_Wood", "success": true},
    {"old_path": "/Game/Meshes/Table_Metal", "new_path": "/Game/Meshes/Furniture/SM_Table_Metal", "success": true}
  ],
  "redirectors_created": 2
}
```

**Complexity:** 3 hours. The API does the heavy lifting. Need to handle asset loading (`StaticLoadObject`), path construction, and error reporting.

**Gotchas:**
- Assets must be loaded into memory before rename. Use `FSoftObjectPath::TryLoad()`.
- Source control integration — if Diversion doesn't handle redirectors well, may need special handling.
- `RenameAssets` blocks the main thread — progress feedback for large batches.

---

### 13. `generate_proxy_mesh` / `setup_hlod`

**Purpose:** Generate merged proxy meshes for groups of static meshes, configure HLOD layers.

**UE5 API:**
- `UHLODLayer` — asset class in `Engine` module. Has `EHLODLayerType`: `Instancing`, `MeshMerge`, `MeshSimplify`, `MeshApproximate`, `Custom`.
- `UHLODBuilder` — base class for HLOD generation. `Build(FHLODBuildContext)` returns `FHLODBuildResult`.
- `FMeshMergingSettings` — controls merge quality, pivot, UV generation.
- `FMeshProxySettings` — controls proxy mesh generation (screen size, material baking).
- `FMeshApproximationSettings` — for simplified approximation.
- `IWorldPartitionHLODUtilities` — interface for HLOD utility operations.
- For non-World-Partition levels: `IMeshMergeUtilities::MergeStaticMeshComponents()` from `MeshMergeUtilities` module.

**Feasibility:** PARTIAL. HLOD setup is straightforward (create/configure `UHLODLayer` asset). But actually *building* HLODs requires the World Partition HLOD build pipeline, which is a commandlet (`WorldPartitionHLODBuildCommandlet`). For non-WP levels, `IMeshMergeUtilities::MergeStaticMeshComponents()` can merge meshes at editor time.

**Proxy mesh approach for non-WP levels:**
```cpp
FModuleManager::LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities")
    .GetUtilities().MergeStaticMeshComponents(Components, World, MergingSettings, nullptr, Package, MergedMeshName);
```

**Params (setup_hlod):**
```json
{
  "hlod_layer_path": "/Game/HLOD/HL_Buildings",
  "layer_type": "MeshMerge",        // Instancing, MeshMerge, MeshSimplify, MeshApproximate
  "merge_settings": {
    "bMergePhysicsData": false,
    "bBakeVertexDataToMesh": true,
    "LODSelectionType": "SpecificLOD",
    "SpecificLOD": 2
  }
}
```

**Params (generate_proxy_mesh):**
```json
{
  "actor_names": ["SM_Wall_01", "SM_Wall_02", "SM_Wall_03"],
  "save_path": "/Game/Meshes/Proxy/SM_WallGroup_Proxy",
  "screen_size": 300,               // target screen size in pixels
  "material_settings": {
    "bCreateMergedMaterial": true,
    "texture_size": 1024
  }
}
```

**Complexity:** 10 hours. `MergeStaticMeshComponents` API is mature but has many parameters. HLOD layer creation is simpler (asset creation + property setting).

**Gotchas:**
- `MeshMergeUtilities` module is in `Developer` — editor-only, no runtime.
- Merge can take significant time for complex meshes — need progress indication.
- Material baking requires creating new textures and materials on disk.
- World Partition HLOD builds are typically done via commandlet, not interactively.

---

### 14. `analyze_texture_budget`

**Purpose:** Query texture streaming pool usage, identify budget hogs.

**UE5 API:**
- `IStreamingManager::Get().GetRenderAssetStreamingManager()` — access the streaming manager.
- `IRenderAssetStreamingManager::GetPoolSize()` — current pool size in bytes.
- `IRenderAssetStreamingManager::GetRequiredPoolSize()` — how much is actually needed.
- `IRenderAssetStreamingManager::GetMemoryOverBudget()` — how far over budget.
- `r.Streaming.PoolSize` CVar — configured pool size in MB.
- Per-texture: `UTexture2D::GetResourceSizeBytes()` — individual texture memory.
- `FStreamingRenderAsset` — internal streaming state per asset.

**Feasibility:** CONFIRMED with caveats. Pool-level queries work in editor. Per-texture analysis requires iterating all loaded textures via `TObjectIterator<UTexture2D>`. The streaming manager members (`PoolSize`, `MemoryOverBudget`, `EffectiveStreamingPoolSize`) are accessible but some are in the Private directory (`StreamingManagerTexture.h`).

**Alternative approach:** Use stat commands:
- `r.Streaming.PoolSize` — read via `IConsoleVariable`
- Iterate `UTexture2D` objects, sum `GetResourceSizeBytes(EResourceSizeMode::EstimatedTotal)`
- Group by folder/asset type

**Params:**
```json
{
  "path_filter": "/Game/Textures",   // optional scope
  "sort_by": "size",                 // "size", "name", "format"
  "top_n": 20,                       // show N largest
  "include_streaming_info": true
}
```

**Return:**
```json
{
  "pool_size_mb": 1000,
  "used_mb": 847,
  "over_budget_mb": 0,
  "texture_count": 1523,
  "top_textures": [
    {"path": "/Game/Textures/T_Wall_Diffuse", "size_mb": 32.0, "format": "BC7", "resolution": "4096x4096", "streaming": true}
  ],
  "by_format": {"BC7": 400, "BC5": 200, "BC1": 150, "RGBA8": 97},
  "recommendations": ["4 textures at 4K could be reduced to 2K saving ~96MB"]
}
```

**Complexity:** 5 hours. Texture iteration is straightforward. Streaming pool queries depend on whether the streaming manager is active in editor (it is, but may report differently than in-game).

**Gotchas:**
- `StreamingManagerTexture.h` is in `Private/` — can't include directly. Use the `IRenderAssetStreamingManager` interface instead.
- Texture memory reporting may differ between editor and packaged builds.
- Virtual textures complicate the picture — they have their own pool.

---

### 15-19. GeometryScript Expansions

All five operations have CONFIRMED GeometryScript API support in UE 5.7:

#### `mesh_extrude`

**API:** `UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshLinearExtrudeFaces()`
- Takes `UDynamicMesh*`, `FGeometryScriptMeshLinearExtrudeOptions` (Distance, DirectionMode, Direction, AreaMode, UVScale, bSolidsToShells), `FGeometryScriptMeshSelection`
- Direction modes: `FixedDirection`, `AverageFaceNormal`
- Area modes: `EntireSelection`, `PerPolygroup`, `PerTriangle`

**Params:**
```json
{
  "handle": "mesh_01",
  "selection": {"type": "material_id", "value": 0},
  "distance": 50.0,
  "direction": [0, 0, 1],           // optional, default face normal
  "direction_mode": "AverageFaceNormal",
  "uv_scale": 1.0
}
```

**Complexity:** 2 hours. Direct wrapper around existing API. Needs selection translation.

#### `mesh_subdivide`

**API:** `UGeometryScriptLibrary_MeshSubdivideFunctions`
- `ApplyPNTessellation(Mesh, Options, TessellationLevel)` — smooth subdivision (PN Triangles)
- `ApplyUniformTessellation(Mesh, TessellationLevel)` — uniform subdivision
- `ApplySelectiveTessellation(Mesh, Selection, Options, TessellationLevel, PatternType)` — selective

**Params:**
```json
{
  "handle": "mesh_01",
  "method": "uniform",              // "uniform", "pn", "selective"
  "tessellation_level": 2,
  "selection": null                  // optional, for selective mode
}
```

**Complexity:** 2 hours. Straightforward wrapper.

#### `mesh_combine`

**API:** `UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(TargetMesh, AppendMesh, AppendTransform, ...)`
- Also: `AppendMeshWithMaterials()` for material-aware combining.
- `AppendMeshTransformed()` for batch append with multiple transforms.

**Params:**
```json
{
  "target_handle": "mesh_01",
  "append_handles": ["mesh_02", "mesh_03"],
  "preserve_materials": true
}
```

**Complexity:** 2 hours. Well-supported API. Handle pool already manages UDynamicMesh instances.

#### `mesh_separate_by_material`

**API:** `UGeometryScriptLibrary_MeshDecompositionFunctions::SplitMeshByMaterialIDs(TargetMesh, ComponentMeshes, ComponentMaterialIDs, MeshPool)`
- Returns array of `UDynamicMesh*` per material ID.
- Also available: `SplitMeshByComponents()` (connected islands), `SplitMeshByPolygroups()`.

**Params:**
```json
{
  "handle": "mesh_01",
  "split_by": "material"            // "material", "component", "polygroup"
}
```

**Return:**
```json
{
  "result_handles": ["mesh_01_mat0", "mesh_01_mat1", "mesh_01_mat2"],
  "material_ids": [0, 1, 2],
  "triangle_counts": [500, 300, 200]
}
```

**Complexity:** 3 hours. Need to register split meshes back into the handle pool.

#### `compute_ao`

**API:** `UGeometryScriptLibrary_MeshBakeFunctions::MakeBakeTypeAmbientOcclusion(OcclusionRays, MaxDistance, SpreadAngle, BiasAngle)`
- Returns `FGeometryScriptBakeTypeOptions` for use with the baking system.
- Bake pipeline: create bake options, call `BakeTexture()` or `BakeVertex()`.
- AO is vertex-color based (baked into mesh vertex data) or texture-based.

**Params:**
```json
{
  "handle": "mesh_01",
  "occlusion_rays": 32,
  "max_distance": 200.0,
  "spread_angle": 180.0,
  "output": "vertex_colors"          // "vertex_colors" or "texture"
}
```

**Complexity:** 4 hours. Need to wire up the full bake pipeline. Texture output requires creating a render target.

**Gotchas for all GeometryScript ops:** All require `WITH_GEOMETRYSCRIPT` guard (already in place for existing mesh ops). The handle pool pattern from Phase 5 applies to all of these.

---

## Context-Aware Prop Placement

### 20. `scatter_on_surface`

**Purpose:** Place props ON specific surfaces (shelf tops, table tops, cabinet interiors).

**UE5 API:**
- `AActor::GetActorBounds()` — get surface actor's bounding box
- `UWorld::LineTraceSingleByChannel()` — downward traces from above the surface to detect the top face
- `UStaticMeshComponent::GetStaticMesh()->GetBoundingBox()` — local-space bounds for surface detection

**Algorithm:**
1. Get the target surface actor's bounds (e.g., a shelf mesh)
2. Compute the "top surface" as the uppermost face of the bounds. Cast downward rays from just above `BoundsMax.Z` to find the actual mesh surface.
3. Generate placement points on the surface using grid or Poisson sampling (reuse from `scatter_props`)
4. For each point: trace downward to confirm it lands on the target actor (not some other geometry)
5. Spawn props at traced hit points, aligned to surface normal

**Feasibility:** CONFIRMED. Trace-based surface detection works reliably for regular geometry (tables, shelves). Irregular surfaces may need denser sampling.

**Params:**
```json
{
  "surface_actor": "SM_Shelf_01",
  "asset_paths": ["/Game/Props/SM_Book_01", "/Game/Props/SM_Vase_01"],
  "count": 5,
  "surface_side": "top",            // "top", "inside" (for cabinets)
  "min_spacing": 15,
  "random_rotation": true,
  "random_scale_range": [0.9, 1.1],
  "seed": 0,
  "surface_align": true             // align prop to surface normal
}
```

**Return:**
```json
{
  "placed_count": 5,
  "actors": [
    {"name": "SM_Book_01_01", "location": [...], "surface_normal": [0,0,1]}
  ],
  "surface_area_used": 0.45
}
```

**Complexity:** 5 hours. Core logic is trace-based surface detection + Poisson sampling (reuse).

**Gotchas:**
- "inside" mode for cabinets: need to trace from within bounds, not above. Requires Z-range filter.
- Very thin surfaces (< 5cm) may miss traces. Use tight trace bounds from mesh AABB.
- Props should not overhang surface edges — check prop bounds vs. surface bounds.

---

### 21. `set_room_disturbance`

**Purpose:** Apply progressive random transforms: orderly to ransacked.

**UE5 API:** Same as `randomize_transforms` but with semantic presets.

**Disturbance levels (transform parameters):**

| Level | Offset (cm) | Rotation (deg) | Scale Var | Tipped Over % | On Floor % |
|-------|-------------|-----------------|-----------|---------------|------------|
| `orderly` | 0-2 | 0-3 | 0.0 | 0% | 0% |
| `slightly_messy` | 5-15 | 5-15 | 0.05 | 5% | 0% |
| `ransacked` | 20-80 | 15-90 | 0.1 | 40% | 25% |
| `abandoned` | 30-100 | 15-90 | 0.15 | 50% | 40% |

"Tipped over" = 60-90 degree pitch/roll rotation. "On floor" = move to floor Z + retrace.

**Feasibility:** CONFIRMED. Pure transform manipulation. `abandoned` mode adds a push-to-edges effect: offset actors away from room center toward walls.

**Params:**
```json
{
  "volume_name": "Room_Kitchen",
  "disturbance": "ransacked",        // orderly, slightly_messy, ransacked, abandoned
  "seed": 42,
  "exclude_actors": ["SM_Fridge_01"],  // optional, heavy objects stay put
  "exclude_tags": ["fixed"]           // optional, tagged actors exempt
}
```

**Return:**
```json
{
  "modified_count": 23,
  "tipped_over": 9,
  "moved_to_floor": 5,
  "unchanged": 3,
  "disturbance_level": "ransacked"
}
```

**Complexity:** 4 hours. Table-driven transform parameters + special handling for tip-over and floor-drop.

**Gotchas:**
- "Abandoned" push-to-edges requires knowing room bounds/center — use volume center.
- Large objects (fridges, desks) should resist disturbance. Filter by mesh size or tags.
- Tipped objects need floor-trace to rest at the correct Z. Same trace pattern as `snap_to_floor`.

---

### 22. `configure_physics_props`

**Purpose:** Set SimulatePhysics=true + sleeping state on actors.

**UE5 API:**
- `UPrimitiveComponent::SetSimulatePhysics(true)` — enable physics simulation.
- `FBodyInstance::PutInstanceToSleep()` — put body to sleep (stays in place until disturbed).
- `FBodyInstance::WakeInstance()` — wake a sleeping body.
- `FBodyInstance::bStartAwake` — exists as an internal field in BodyInstance, referenced in `BodyInstance.cpp`. In UE 5.7, it's used internally but may not be a public UPROPERTY. The property IS set through `FBodyInstance` and propagated to `FActorCreationParams::bStartAwake`.

**Alternative approach (more reliable):**
```cpp
UStaticMeshComponent* SMC = Actor->FindComponentByClass<UStaticMeshComponent>();
SMC->SetSimulatePhysics(true);
SMC->SetMobility(EComponentMobility::Movable);  // Required!
FBodyInstance* BI = SMC->GetBodyInstance();
if (BI)
{
    BI->PutInstanceToSleep();  // Sleep immediately
}
```

**Feasibility:** CONFIRMED. `SetSimulatePhysics()` and `PutInstanceToSleep()` both work in editor. The component MUST be Movable (not Static) for physics to work.

**Params:**
```json
{
  "actor_names": ["SM_Can_01", "SM_Bottle_01", "SM_Box_01"],
  "volume_name": "optional — all actors in volume",
  "simulate_physics": true,
  "start_asleep": true,              // PutInstanceToSleep after enabling
  "mass_override": null,             // optional kg
  "collision_profile": "PhysicsActor"  // optional
}
```

**Return:**
```json
{
  "configured_count": 3,
  "actors": [
    {"name": "SM_Can_01", "physics_enabled": true, "asleep": true, "mass_kg": 0.35}
  ],
  "warnings": []
}
```

**Complexity:** 3 hours. Main concern is mobility check/auto-fix and collision setup.

**Gotchas:**
- **Mobility:** Actor root component MUST be `EComponentMobility::Movable`. If it's Static, physics silently fails. Auto-set to Movable with a warning.
- **Collision:** Physics requires collision shapes. If the mesh has no collision, `SetSimulatePhysics` fails. Detect and warn.
- `PutInstanceToSleep()` only works AFTER the body is in the physics scene. May need a one-frame delay or call after `RegisterComponent()`.

---

### 23. `settle_props`

**Purpose:** Physics settle — drop props, let them come to rest, capture final transforms.

**UE5 API:**
- There is NO clean API to step the physics simulation N frames in editor without PIE.
- `FPhysScene_Chaos::Tick(DeltaTime)` — advances the physics scene one step. This is the internal API.
- `UWorld::Tick(LEVELTICK_All, DeltaTime)` — would tick everything including physics, but this is PIE behavior.
- **UE5 "Simulate" mode in editor** uses `UEditorEngine::PlayInEditor()` with simulate flag.

**Alternative approaches:**

1. **Simulate mode API:** `GEditor->RequestPlaySession(bSimulate=true)` → wait → `GEditor->RequestEndPlayMap()`. This is the cleanest but requires PIE-like state.

2. **Direct physics stepping:** Access `FPhysScene_Chaos` via `UWorld::GetPhysicsScene()` and call `AdvanceAndDispatch_External()` / `EndFrame_External()`. This is engine-internal and not stable API.

3. **Manual gravity approximation:** For each actor, iteratively: apply gravity offset, trace downward, settle. No actual physics, but gives "dropped and landed" effect. Much simpler, deterministic, but doesn't handle prop-on-prop settling.

4. **Editor Physics Simulation plugin pattern:** Similar to the Unity plugin by alexismorin — temporarily enable physics, tick the world N times, capture transforms, disable physics.

**Recommended approach:** Option 3 (manual gravity settle) for MVP, Option 1 (simulate mode) for V2.

**Manual gravity settle algorithm:**
```
for each actor:
    save original transform
    enable physics temporarily
    for N iterations:
        trace downward from current position
        if hit:
            set position to hit point + half-extent
            add small random rotation (tumble)
            break
    capture final transform
    disable physics
    apply captured transform as static
```

**Feasibility:** PARTIAL. Manual approach is reliable but limited (no physics interactions between props). True physics settling requires PIE/Simulate mode.

**Params:**
```json
{
  "actor_names": ["SM_Can_01", "SM_Bottle_01"],
  "volume_name": "optional",
  "method": "trace",                 // "trace" (manual) or "simulate" (PIE)
  "max_iterations": 30,             // for simulate mode: frames
  "velocity_threshold": 1.0,        // for simulate mode: sleep threshold cm/s
  "gravity_multiplier": 1.0
}
```

**Return:**
```json
{
  "settled_count": 2,
  "actors": [
    {"name": "SM_Can_01", "original_location": [...], "settled_location": [...], "settled_rotation": [...], "fell_distance": 45.2}
  ],
  "method_used": "trace"
}
```

**Complexity:** 6 hours for trace method, 12 hours for simulate method.

**Gotchas:**
- Simulate method requires starting/stopping PIE which disrupts editor state.
- Trace method doesn't handle objects settling ON other settling objects.
- Physics materials affect settle behavior (friction, restitution) — trace method ignores these.
- Undo support critical — must capture all original transforms for undo.

---

### 24. Themed Prop Kits (JSON Authoring + Placement)

**Purpose:** Predefined prop arrangements ("office_desk_clutter", "hospital_tray").

**UE5 API:** Pure JSON + existing `scatter_props`/`spawn_actor` infrastructure. No new engine APIs.

**Format:**
```json
{
  "name": "office_desk_clutter",
  "description": "Typical office desk with papers, pens, mug, and monitor",
  "anchor": "center",
  "items": [
    {
      "label": "monitor",
      "asset_path": "/Game/Props/SM_Monitor_01",
      "offset": [0, -20, 0],
      "rotation": [0, 0, 0],
      "scale": 1.0,
      "required": true,
      "spawn_chance": 1.0
    },
    {
      "label": "coffee_mug",
      "asset_path": "/Game/Props/SM_Mug_01",
      "offset": [25, 10, 0],
      "rotation_range": [0, 360],
      "scale_range": [0.9, 1.1],
      "required": false,
      "spawn_chance": 0.7
    }
  ]
}
```

**Storage:** `Saved/Monolith/PropKits/` for user-authored, built-in defaults hardcoded.

**Actions:**
- `list_prop_kits` — enumerate available kits
- `create_prop_kit` — save a new kit JSON
- `place_prop_kit` — spawn a kit at a location (using `spawn_actor` under the hood)

**Complexity:** 5 hours total. JSON I/O + placement loop using existing spawn infrastructure.

**Gotchas:** Asset path validation at placement time (kit may reference assets that don't exist in the project). Return warnings for missing assets, skip those items.

---

### 25. Wall/Ceiling Scatter

**Purpose:** Extend `scatter_props` with directional surface targeting.

**UE5 API:**
- Horizontal traces (`+Y`, `-Y`, `+X`, `-X`) for wall detection
- Upward traces for ceiling detection
- `FHitResult::ImpactNormal` for surface alignment
- `FRotationMatrix::MakeFromZX(Normal, FVector::UpVector)` — align prop to surface normal

**Algorithm for wall scatter:**
1. For each sample point in the volume, cast a horizontal ray outward
2. If it hits geometry within max_distance, that's a wall point
3. Place prop at hit point, aligned to `ImpactNormal` (facing away from wall)
4. Apply random rotation around the normal axis

**Feasibility:** CONFIRMED. Same trace infrastructure as existing scatter/horror analysis.

**Extension to scatter_props params:**
```json
{
  "surface": "wall",               // "floor" (default), "wall", "ceiling"
  "wall_offset": 2.0,              // cm offset from wall surface
  "normal_align": true             // orient prop to face outward from surface
}
```

**Complexity:** 4 hours. Add surface mode to existing `scatter_props` implementation.

**Gotchas:**
- Wall scatter generates more sample candidates that miss (most rays don't hit walls) — use higher attempt count.
- Ceiling scatter needs inverted gravity alignment for props.
- Props need appropriate collision/physics disabled for wall/ceiling mounting.

---

## Genre Preset System

### 26. Preset File I/O Architecture

**Storage locations:**
- Built-in: hardcoded in C++ (current pattern — `MonolithMeshStorytellingPatterns.h`, `MonolithMeshAcoustics.h`)
- User-created: `<ProjectDir>/Saved/Monolith/Presets/<genre_name>/`
- Shared presets: `<ProjectDir>/Content/Monolith/Presets/<genre_name>/` (version controlled)

**Directory structure per genre:**
```
Saved/Monolith/Presets/horror_default/
  preset.json           <- manifest (name, version, author, description)
  patterns/             <- storytelling patterns
    violence.json
    abandoned_in_haste.json
  acoustics/            <- acoustic profiles
    concrete_corridor.json
    tile_bathroom.json
  tension/              <- tension scoring profiles
    tension_profile.json
  prop_kits/            <- themed prop kits
    hospital_tray.json
    office_desk.json
  room_templates/       <- room template definitions
    hospital_room.json
    office.json
```

**Feasibility:** CONFIRMED. All pure file I/O. UE5 `FFileHelper::SaveStringToFile()`, `LoadFileToString()`, `FJsonSerializer`. No engine API barriers.

**Complexity:** 3 hours for the file I/O framework.

---

### 27. `list_storytelling_patterns` / `create_storytelling_pattern`

**Purpose:** Enumerate and author storytelling patterns.

**Current implementation:** Patterns are hardcoded in `MonolithMeshStorytellingPatterns.h` — 5 patterns (violence, abandoned_in_haste, dragged, medical_emergency, corruption).

**Migration plan:**
1. Keep hardcoded defaults as fallback
2. On startup, scan `Saved/Monolith/Patterns/` for user JSON files
3. User patterns override built-in patterns with same name
4. New patterns just add to the available list

**Pattern JSON format:**
```json
{
  "name": "tavern_brawl",
  "description": "Fantasy tavern fight scene — overturned furniture, spilled drinks",
  "genre": "fantasy",
  "elements": [
    {
      "label": "spilled_mead",
      "type": "decal",
      "relative_offset": [0, 0, 0],
      "size": [12, 80, 80],
      "radial": true,
      "radial_min": 30,
      "radial_max": 150,
      "count_min": 1,
      "count_max": 3,
      "rotation_variance": 360,
      "scale_variance": 0.3,
      "wall_element": false
    }
  ]
}
```

**Complexity:** 3 hours. JSON deserialization + registration alongside hardcoded patterns.

---

### 28. `list_acoustic_profiles` / `create_acoustic_profile`

**Purpose:** Enumerate and author acoustic surface profiles.

**Current implementation:** Hardcoded defaults in `MonolithMeshAcoustics.cpp` + optional DataTable override via `UMonolithSettings::SurfaceAcousticsTablePath`.

**Profile JSON format:**
```json
{
  "name": "stone_dungeon",
  "genre": "fantasy",
  "surfaces": {
    "stone_floor": {
      "absorption": 0.02,
      "transmission_loss_db": 55,
      "footstep_loudness": 0.8,
      "display_name": "Stone Floor"
    },
    "wooden_door": {
      "absorption": 0.15,
      "transmission_loss_db": 25,
      "footstep_loudness": 0.0,
      "display_name": "Wooden Door"
    }
  }
}
```

**Complexity:** 3 hours. Same pattern as storytelling patterns — JSON files override hardcoded defaults.

---

### 29. `create_tension_profile`

**Purpose:** Define genre-specific tension scoring weights.

**Current implementation:** Tension factors are hardcoded in `classify_zone_tension` (sightline length, hiding spots, light level, chokepoint proximity, etc.).

**Profile JSON format:**
```json
{
  "name": "horror_default",
  "genre": "horror",
  "description": "Standard horror tension scoring — short sightlines and darkness increase tension",
  "factors": {
    "sightline_length": {"weight": 0.25, "invert": true, "note": "shorter = more tense"},
    "hiding_spot_density": {"weight": 0.15, "invert": false},
    "light_level": {"weight": 0.20, "invert": true, "note": "darker = more tense"},
    "escape_route_count": {"weight": 0.15, "invert": true, "note": "fewer = more tense"},
    "chokepoint_proximity": {"weight": 0.15, "invert": false},
    "dead_end_proximity": {"weight": 0.10, "invert": false}
  },
  "thresholds": {
    "low": 0.3,
    "medium": 0.5,
    "high": 0.7,
    "extreme": 0.9
  }
}
```

**Fantasy alternative:**
```json
{
  "name": "fantasy_adventure",
  "genre": "fantasy",
  "factors": {
    "sightline_length": {"weight": 0.10, "invert": false, "note": "vistas = wonder"},
    "vertical_space": {"weight": 0.20, "invert": false, "note": "tall spaces = awe"},
    "light_level": {"weight": 0.10, "invert": true, "note": "caves = danger"},
    "exit_count": {"weight": 0.20, "invert": true, "note": "fewer exits = more dangerous"},
    "loot_density": {"weight": 0.15, "invert": false, "note": "rewards increase engagement"},
    "npc_proximity": {"weight": 0.25, "invert": true, "note": "isolation = danger"}
  }
}
```

**Complexity:** 4 hours. Need to refactor `classify_zone_tension` to read weights from profile instead of hardcoded values.

---

### 30. `list_prop_kits` / `create_prop_kit`

**Same as section 24 above.** Part of both the Context-Aware system and the Genre Preset system.

**Complexity:** Already counted in section 24.

---

### 31. `export_genre_preset` / `import_genre_preset`

**Purpose:** Bundle all preset files into a distributable package, and import them.

**Format decision: JSON vs ZIP**

| Approach | Pros | Cons |
|----------|------|------|
| **Single JSON** | Simple, human-readable, easy to diff/merge | Large files, can't include binary assets |
| **ZIP bundle** | Can include textures/meshes, smaller | More complex, harder to inspect |
| **Directory copy** | Simplest, already the storage format | Not easily distributable |

**Recommendation:** Single JSON for MVP (no binary assets in presets). ZIP for V2 if texture/mesh references are needed.

**Export format (single JSON):**
```json
{
  "preset_name": "horror_default",
  "version": "1.0.0",
  "author": "Monolith",
  "engine_version": "5.7",
  "description": "Default horror preset pack for survival horror games",
  "patterns": { /* ... all pattern JSONs merged ... */ },
  "acoustics": { /* ... all acoustic profiles merged ... */ },
  "tension": { /* ... tension profile ... */ },
  "prop_kits": { /* ... all prop kits merged ... */ },
  "room_templates": { /* ... all room templates merged ... */ }
}
```

**Import behavior:**
- Validate JSON schema
- Check for name conflicts with existing presets
- Merge mode: `"overwrite"`, `"skip_existing"`, `"rename_conflicts"`
- Write to `Saved/Monolith/Presets/<preset_name>/`

**File system location for user presets:**
- `Saved/Monolith/Presets/` — best location. `Saved/` is in .gitignore by default so user presets don't pollute version control.
- For presets that SHOULD be version-controlled: `Content/Monolith/Presets/` (requires explicit copy).

**Complexity:** 5 hours. JSON aggregation + validation + merge logic.

**Gotchas:**
- Version compatibility — presets from newer Monolith versions may have fields that older versions don't understand. Use semver and graceful degradation.
- Asset path references in prop kits are project-specific — a kit referencing `/Game/Props/SM_Mug` won't work in another project. Presets should flag asset-dependent entries.

---

### 32. Documentation for Preset Authors

**Purpose:** Enable other LLMs and users to create genre presets without reading C++ code.

**Recommended file:** `Docs/PRESET_AUTHORING.md` — shipped with Monolith.

**Contents outline:**
1. JSON schema for each preset type (with `$schema` references)
2. Examples for 4 genres: horror (built-in), fantasy, sci-fi, detective
3. How to test presets via MCP commands
4. How to distribute (single JSON export/import)
5. Factor reference table for tension profiles

**Complexity:** 4 hours. Documentation only, no code.

---

## Priority Matrix

### Total Estimate Summary

| Category | Action Count | Total Hours | Avg Hours/Action |
|----------|-------------|-------------|-----------------|
| P2 Actions | 10 | ~50h | 5.0h |
| P3 Actions | ~9 | ~32h | 3.6h |
| Context-Aware Props | 6 | ~27h | 4.5h |
| Genre Presets | 8 | ~22h | 2.8h |
| **Total** | **~33** | **~131h** | **4.0h** |

### Recommended Implementation Order (by value/effort ratio)

**Tier 1 — Quick wins, high value (< 3h each):**
1. `randomize_transforms` — 2h, trivial, high daily use
2. `measure_distance` — 2h, reuses existing navmesh code
3. `get_level_actors` — 3h, foundation for many other actions
4. `validate_naming_conventions` — 2h, pure read-only
5. `mesh_extrude` — 2h, direct GeometryScript wrapper
6. `mesh_subdivide` — 2h, direct GeometryScript wrapper
7. `mesh_combine` — 2h, direct GeometryScript wrapper

**Tier 2 — Medium effort, high value (3-5h):**
8. `configure_physics_props` — 3h, essential for interactive horror
9. `batch_rename_assets` — 3h, well-supported API
10. `mesh_separate_by_material` — 3h, well-supported API
11. `set_room_disturbance` — 4h, table-driven, high creative value
12. `compute_ao` — 4h, bake pipeline setup
13. `validate_horror_intensity` — 4h, aggregation of existing data
14. `wall/ceiling scatter` — 4h, extends existing action
15. Preset file I/O framework — 3h, enables the whole system

**Tier 3 — Higher effort, specialized value (5-8h):**
16. `scatter_on_surface` — 5h, trace-based surface detection
17. `evaluate_monster_reveal` — 5h, extends framing analysis
18. `analyze_texture_budget` — 5h, streaming manager queries
19. `place_spline` — 6h, spline mesh component setup
20. `settle_props (trace method)` — 6h, manual gravity approach
21. `generate_scare_sequence` — 6h, algorithmic horror design
22. `analyze_framing` — 6h, screen-space projection
23. `export_genre_preset` / `import_genre_preset` — 5h, bundling

**Tier 4 — Complex, long-term (8-12h):**
24. `create_prefab` / `spawn_prefab` — 8h, Level Instance API
25. `generate_hospice_report` — 8h, comprehensive aggregation
26. `generate_proxy_mesh` / `setup_hlod` — 10h, mesh merge pipeline
27. `settle_props (simulate method)` — 12h additional, PIE-based

### Key Dependencies

```
get_level_actors ← randomize_transforms (volume-based targeting)
get_level_actors ← set_room_disturbance (actor enumeration)
get_level_actors ← configure_physics_props (volume-based targeting)
scatter_props ← scatter_on_surface (extends existing)
scatter_props ← wall/ceiling scatter (extends existing)
classify_zone_tension ← validate_horror_intensity (reads tension data)
analyze_framing ← evaluate_monster_reveal (extends framing)
preset I/O framework ← all create_*/list_* preset actions
```

### Risk Assessment

| Action | Risk | Mitigation |
|--------|------|------------|
| `settle_props (simulate)` | HIGH — PIE disrupts editor state | Start with trace method, defer simulate |
| `generate_proxy_mesh` | MEDIUM — mesh merge is slow, many params | Start with simple merge, add quality controls later |
| `create_prefab` | MEDIUM — Level Instance API is complex | Start with spawn_prefab only (simpler), add create later |
| `analyze_texture_budget` | LOW-MEDIUM — streaming manager headers are Private | Use interface methods only, skip internal state |
| All others | LOW | Standard editor APIs, well-understood |
