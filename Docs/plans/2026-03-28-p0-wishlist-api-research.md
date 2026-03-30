# P0 Wishlist API Feasibility Research

**Date:** 2026-03-28
**Scope:** All ~15 P0 ("would use EVERY session") MonolithMesh wishlist actions
**Method:** UE 5.7 engine source verification via source_query + direct header reads

---

## Executive Summary

All 15 P0 actions are **feasible** with existing UE 5.7 APIs. No PIE required for any of them. Total estimated implementation: **~65-80 hours**.

Key findings:
- **`place_light` / `set_light_properties`** — Trivial. `spawn_actor` already handles class spawning; lights just need property setters on their components.
- **`spawn_volume`** — Moderate complexity. Requires `UCubeBuilder` + `UActorFactory::CreateBrushForVolumeActor` for brush geometry. The current `spawn_actor` explicitly blocks volumes — this needs a dedicated action.
- **`predict_player_paths`** — Most complex. Navmesh pathfinding already works in the codebase (`FindPathSync`), but multi-heuristic path generation needs custom cost weighting.
- **`auto_generate_lods` + `set_lod_screen_sizes`** — `generate_lods` already exists via GeometryScript. The gap is writing LODs back to a UStaticMesh asset with proper screen sizes.
- **`analyze_texel_density`** — Engine has `FMeshUVChannelInfo::LocalUVDensities` and `FStaticMeshRenderData::ComputeUVDensities`. Combining UV density with texture resolution gives texels/cm.

---

## 1. `place_light` / `set_light_properties`

### What It Does
Spawn point/spot/rect/directional lights and modify their properties (intensity, color, radius, cone angles, shadows, temperature, IES profiles).

### Exact UE5 API

**Spawning:**
```cpp
// APointLight, ASpotLight, ARectLight, ADirectionalLight all derive from ALight
// spawn_actor already supports class spawning:
APointLight* Light = World->SpawnActor<APointLight>(Location, Rotation, SpawnParams);

// Access the light component:
UPointLightComponent* PLC = Light->PointLightComponent;  // direct UPROPERTY
USpotLightComponent* SLC = SpotLight->SpotLightComponent;
```

**Property Setters (all on ULightComponent / UPointLightComponent / USpotLightComponent):**
```cpp
// ULightComponent (base)
void SetIntensity(float NewIntensity);                    // Engine/Classes/Components/LightComponent.h:283
void SetLightColor(FLinearColor NewLightColor, bool bSRGB = true);  // :293
void SetIndirectLightingIntensity(float NewIntensity);    // :286
void SetVolumetricScatteringIntensity(float NewIntensity); // :289
// bCastShadows is a UPROPERTY on ULightComponent — set via Modify() + direct assignment
// Temperature: UPROPERTY float Temperature, bool bUseTemperature

// UPointLightComponent
void SetSourceRadius(float bNewValue);      // PointLightComponent.h:70
void SetSoftSourceRadius(float bNewValue);  // :73
// AttenuationRadius inherited from ULocalLightComponent
void SetAttenuationRadius(float NewRadius); // LocalLightComponent.h

// USpotLightComponent
void SetInnerConeAngle(float NewInnerConeAngle);  // SpotLightComponent.cpp:217
void SetOuterConeAngle(float NewOuterConeAngle);  // SpotLightComponent.cpp:227
```

### Editor-Time Feasibility
**Yes.** All light actors can be spawned and configured at editor time. The existing `spawn_actor` action already does this for any class — lights just work. Properties persist in the level.

### Threading
**Game thread only.** Actor spawning and property modification must happen on the game thread.

### Params and Return Shape

**`place_light` input:**
```json
{
  "type": "point|spot|rect|directional",
  "location": [x, y, z],
  "rotation": [pitch, yaw, roll],
  "intensity": 5000.0,
  "color": [r, g, b],
  "attenuation_radius": 1000.0,
  "cast_shadows": true,
  "temperature": 6500.0,
  "use_temperature": false,
  "source_radius": 10.0,
  "inner_cone_angle": 25.0,
  "outer_cone_angle": 44.0,
  "name": "MyLight",
  "folder": "Lighting/Hallway",
  "mobility": "Stationary"
}
```

**`set_light_properties` input:**
```json
{
  "actor_name": "MyLight",
  "intensity": 8000.0,
  "color": [1.0, 0.8, 0.6],
  "attenuation_radius": 1500.0,
  "cast_shadows": false,
  "temperature": 3200.0,
  "inner_cone_angle": 20.0,
  "outer_cone_angle": 35.0,
  "mobility": "Movable"
}
```

**Output:**
```json
{
  "actor_name": "MyLight",
  "class": "PointLight",
  "location": [100, 200, 300],
  "properties_set": ["intensity", "color", "cast_shadows"]
}
```

### Complexity Estimate
**4 hours.** Spawn is trivial (reuse existing `spawn_actor` pattern). Property setters are straightforward — just Cast to the right component type and call setters.

### Gotchas
- `APointLight::PointLightComponent` is a direct UPROPERTY, not behind a getter. Same for `ASpotLight::SpotLightComponent`. Safe to access directly.
- `SetIntensity` uses the unit-dependent brightness system (cd, lux, etc.) — document which unit.
- IES profiles require loading a `UTextureLightProfile` asset.
- Rect lights have `SourceWidth`/`SourceHeight` instead of `SourceRadius`.
- `bCastShadows` is on `ULightComponent` (not a setter function — use `Modify()` + direct write or FProperty reflection).

---

## 2. `find_replace_mesh`

### What It Does
Find all actors using mesh X and swap to mesh Y. Essential for blockout-to-art pass.

### Exact UE5 API
```cpp
// Iterate all actors with static mesh components
for (TActorIterator<AActor> It(World); It; ++It)
{
    TArray<UStaticMeshComponent*> SMCs;
    It->GetComponents<UStaticMeshComponent>(SMCs);
    for (UStaticMeshComponent* SMC : SMCs)
    {
        if (SMC->GetStaticMesh() == SourceMesh)
        {
            SMC->SetStaticMesh(TargetMesh);  // Engine/Private/Components/StaticMeshComponent.cpp
        }
    }
}

// UStaticMeshComponent::SetStaticMesh is:
// bool UStaticMeshComponent::SetStaticMesh(UStaticMesh* NewMesh)
// Returns true if mesh was actually changed
```

### Editor-Time Feasibility
**Yes.** `SetStaticMesh` works at editor time and persists. Need `Modify()` call before change for undo support.

### Threading
**Game thread only.** Component modification requires game thread.

### Params and Return Shape

**Input:**
```json
{
  "source_mesh": "/Game/Blockout/SM_Wall_01",
  "target_mesh": "/Game/Art/SM_Wall_Concrete_01",
  "actors": ["Wall_01", "Wall_02"],
  "match_mode": "exact|contains",
  "preview": false
}
```
If `actors` is omitted, searches all actors in the level.

**Output:**
```json
{
  "replaced": 47,
  "actors_modified": ["Wall_01", "Wall_02", "..."],
  "source_mesh": "/Game/Blockout/SM_Wall_01",
  "target_mesh": "/Game/Art/SM_Wall_Concrete_01"
}
```

### Complexity Estimate
**3 hours.** Straightforward iteration. Patterns already exist in MonolithMesh codebase.

### Gotchas
- Must call `SMC->Modify()` before `SetStaticMesh` for proper undo.
- If the new mesh has different material slots, existing material overrides may break.
- Bounds change may affect lighting/shadow. Consider flagging actors that need lighting rebuild.
- HISM/ISM components also have static meshes — decide whether to include those.

---

## 3. `spawn_volume`

### What It Does
Spawn trigger/kill/pain/blocking/nav_modifier/audio/post_process volumes with proper brush geometry.

### Exact UE5 API

**The pattern (verified from UActorFactory::CreateBrushForVolumeActor at ActorFactory.cpp:1828):**
```cpp
#include "Builders/CubeBuilder.h"
#include "Engine/BlockingVolume.h"
#include "Factories/ActorFactory.h"

// 1. Spawn the volume actor
AVolume* Volume = World->SpawnActor<ATriggerVolume>(Location, FRotator::ZeroRotator, SpawnParams);

// 2. Create brush geometry using UCubeBuilder
UCubeBuilder* Builder = NewObject<UCubeBuilder>();
Builder->X = ExtentX * 2;  // full width, not half-extent
Builder->Y = ExtentY * 2;
Builder->Z = ExtentZ * 2;

// 3. Build the brush for the volume (static helper)
UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
// This internally does:
//   - Creates UModel + UPolys
//   - Attaches to BrushComponent
//   - Calls Builder->Build(World, Volume)
//   - Calls FBSPOps::csgPrepMovingBrush(Volume)
//   - Nulls material refs on polys
```

**Volume classes and their headers:**
| Volume Type | Class | Header |
|---|---|---|
| Blocking | `ABlockingVolume` | `Engine/BlockingVolume.h` |
| Trigger | `ATriggerVolume` | `Engine/TriggerVolume.h` |
| Kill | `AKillZVolume` | `GameFramework/KillZVolume.h` |
| Pain | `APainCausingVolume` | `GameFramework/PainCausingVolume.h` |
| NavModifier | `ANavModifierVolume` | `NavigationSystem/NavModifierVolume.h` (NavSystem module) |
| Audio | `AAudioVolume` | `Sound/AudioVolume.h` |
| PostProcess | `APostProcessVolume` | `Engine/PostProcessVolume.h` |

### Editor-Time Feasibility
**Yes.** `UActorFactory::CreateBrushForVolumeActor` is specifically designed for editor use. Confirmed by WorldPartitionEditorGrid2D.cpp and ActorFactory.cpp usage patterns.

### Threading
**Game thread only.** BSP operations, actor spawning.

### Params and Return Shape

**Input:**
```json
{
  "type": "trigger|blocking|kill|pain|nav_modifier|audio|post_process",
  "location": [x, y, z],
  "extent": [500, 500, 300],
  "shape": "box|sphere|cylinder",
  "rotation": [0, 0, 0],
  "name": "TriggerVolume_Scare01",
  "folder": "Volumes/Triggers",
  "properties": {
    "damage_per_sec": 10.0,
    "pain_interval": 0.5,
    "reverb_effect": "/Game/Audio/RE_LargeRoom"
  }
}
```

**Output:**
```json
{
  "actor_name": "TriggerVolume_Scare01",
  "class": "TriggerVolume",
  "location": [100, 200, 50],
  "extent": [500, 500, 300],
  "brush_valid": true
}
```

### Complexity Estimate
**8 hours.** The brush geometry creation is the tricky part. Need to:
1. Support multiple volume types with different properties
2. Handle box/sphere/cylinder shapes (UCubeBuilder, UTetrahedronBuilder/sphere approximation, UCylinderBuilder)
3. Set type-specific properties (APainCausingVolume::DamagePerSec, AAudioVolume reverb settings, etc.)
4. Remove the current BlockingVolume block in `spawn_actor`

### Gotchas
- **`UActorFactory::CreateBrushForVolumeActor` is in UnrealEd module** — editor-only, which is fine for Monolith.
- `UCubeBuilder`, `UCylinderBuilder` are in `Builders/` headers in UnrealEd.
- The current `spawn_actor` explicitly blocks `ABlockingVolume` — this new action should handle all volumes properly.
- `ANavModifierVolume` requires the `NavigationSystem` module dependency (already present in MonolithMesh.Build.cs).
- Volume brush is **different from collision** — it's BSP geometry that defines the volume's shape. Without it, the volume has zero extent.
- `FBSPOps::csgPrepMovingBrush` is needed after brush creation.
- For sphere volumes, use `UTetrahedronBuilder` (Epic's factory uses it at ActorFactory.cpp:1930).

---

## 4. `get_actor_properties` / `copy_actor_properties`

### What It Does
Read arbitrary UPROPERTY values from actors/components; copy property sets between actors.

### Exact UE5 API

**Reading properties via FProperty reflection:**
```cpp
// Iterate FProperty chain on a UObject
for (TFieldIterator<FProperty> It(Actor->GetClass()); It; ++It)
{
    FProperty* Prop = *It;
    FString ValueStr;
    // ExportText_Direct exports the value as a string
    Prop->ExportText_Direct(ValueStr,
        Prop->ContainerPtrToValuePtr<void>(Actor),
        nullptr, Actor, PPF_None);
    // ValueStr now contains the text representation
}

// Writing properties:
const TCHAR* ImportResult = Prop->ImportText_Direct(
    *ValueStr,
    Prop->ContainerPtrToValuePtr<void>(Actor),
    Actor, PPF_None);
// Returns nullptr on failure, otherwise pointer past consumed text
```

**The editor-level API (PropertyHandleImpl.h) also provides:**
```cpp
FPropertyAccess::Result FPropertyValueImpl::ImportText(
    const FString& InValue,
    EPropertyValueSetFlags::Type Flags);
```

**For component properties specifically:**
```cpp
// Get all components, then iterate their properties
TArray<UActorComponent*> Components = Actor->GetComponents();
for (UActorComponent* Comp : Components)
{
    for (TFieldIterator<FProperty> It(Comp->GetClass()); It; ++It)
    {
        // Same ExportText_Direct / ImportText_Direct pattern
    }
}
```

### Editor-Time Feasibility
**Yes.** FProperty reflection is fully available at editor time. The existing `set_actor_properties` action already uses a similar pattern but is hardcoded to specific properties. This generalizes it.

### Threading
**Game thread only.** UObject property access must be on game thread.

### Params and Return Shape

**`get_actor_properties` input:**
```json
{
  "actor_name": "PointLight_01",
  "properties": ["Intensity", "LightColor", "AttenuationRadius"],
  "component": "PointLightComponent0",
  "include_defaults": false
}
```
If `properties` omitted, returns all non-default UPROPERTYs.

**Output:**
```json
{
  "actor_name": "PointLight_01",
  "class": "PointLight",
  "properties": {
    "Intensity": "5000.0",
    "LightColor": "(R=1.0,G=0.8,B=0.6,A=1.0)",
    "AttenuationRadius": "1000.0"
  },
  "component": "PointLightComponent0"
}
```

**`copy_actor_properties` input:**
```json
{
  "source_actor": "PointLight_01",
  "target_actors": ["PointLight_02", "PointLight_03"],
  "properties": ["Intensity", "LightColor"],
  "component_class": "PointLightComponent"
}
```

### Complexity Estimate
**6 hours.** FProperty iteration is well-understood. The complexity is in:
1. Property path resolution (nested structs, arrays, maps)
2. Component targeting (which component on a multi-component actor)
3. Proper text serialization/deserialization of complex types
4. Filtering out transient/editor-only properties

### Gotchas
- `ExportText_Direct` / `ImportText_Direct` work with text representations — need to handle struct types like FLinearColor, FVector, FRotator correctly.
- Some properties are `EditConst` or `BlueprintReadOnly` — should we allow writing them anyway? Probably yes, since we're an editor tool.
- Property paths need to handle nested properties: `"PointLightComponent0.Intensity"` vs `"Intensity"`.
- `FProperty::HasAnyPropertyFlags(CPF_Transient)` should be filtered from reads.
- Consider using existing CDO property reading from `blueprint_query("get_cdo_properties")` as reference implementation.

---

## 5. `predict_player_paths`

### What It Does
Auto-generate weighted navmesh paths between key points using multiple heuristics (shortest, safest, curious/explorer, cautious). THE multiplier for all horror analysis.

### Exact UE5 API

**Core pathfinding (already used extensively in MonolithMesh):**
```cpp
// Get nav system (pattern from MonolithMeshAnalysis.cpp:502)
UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
ANavigationData* NavData = NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);

// Basic pathfinding
FNavAgentProperties AgentProps;
AgentProps.AgentRadius = 45.0f;
AgentProps.AgentHeight = 180.0f;
FPathFindingQuery Query(nullptr, *NavData, Start, End);
FPathFindingResult PathResult = NavSys->FindPathSync(AgentProps, Query);
// PathResult.Path->GetPathPoints() gives TArray<FNavPathPoint>

// Path cost/length queries
ENavigationQueryResult::Type GetPathCost(WorldCtx, Start, End, double& PathCost, NavData, FilterClass);
ENavigationQueryResult::Type GetPathLength(WorldCtx, Start, End, double& PathLength, NavData, FilterClass);

// Random reachable points (for explorer paths)
bool K2_GetRandomReachablePointInRadius(WorldCtx, Origin, RandomLocation, Radius, NavData, FilterClass);
```

**For custom cost weighting (cautious/curious heuristics):**
```cpp
// Custom navigation query filters
// UNavigationQueryFilter provides area cost overrides
// FRecastQueryFilter allows per-area cost modification

// Alternative: sample multiple intermediate waypoints and compose paths
// Use sightline analysis (already implemented) to weight "safety" of segments
```

**For multi-path generation with different strategies:**
The approach should be:
1. **Shortest path**: Standard `FindPathSync` (default cost = distance)
2. **Safest path**: Sample grid, score safety (existing `analyze_sightlines`, `find_hiding_spots`), pathfind through high-safety waypoints
3. **Explorer/Curious path**: Find interesting points (dead ends, hidden areas, items) and route through them
4. **Cautious path**: Prefer wide corridors (existing `analyze_choke_points`), good sightlines, near walls

### Editor-Time Feasibility
**Yes, IF navmesh is built.** All navmesh queries work at editor time without PIE. The navmesh must be pre-built. The TODO notes `build_navmesh` as P1 — having that would complement this action.

### Threading
**Mixed.** `FindPathSync` is synchronous and game-thread. However, the sightline/safety scoring for multi-heuristic paths could be parallelized using `ParallelFor` for the grid sampling phase.

### Params and Return Shape

**Input:**
```json
{
  "start": [x, y, z],
  "end": [x, y, z],
  "strategies": ["shortest", "safest", "curious", "cautious"],
  "agent_radius": 45.0,
  "agent_height": 180.0,
  "waypoints": [[x, y, z], [x, y, z]],
  "sample_density": 200.0,
  "max_paths_per_strategy": 3
}
```

**Output:**
```json
{
  "paths": [
    {
      "strategy": "shortest",
      "points": [[x, y, z], ...],
      "total_distance": 2500.0,
      "estimated_time_seconds": 12.5,
      "safety_score": 0.3,
      "visibility_score": 0.7
    },
    {
      "strategy": "safest",
      "points": [[x, y, z], ...],
      "total_distance": 3200.0,
      "estimated_time_seconds": 16.0,
      "safety_score": 0.85,
      "choke_points": 1
    }
  ],
  "navmesh_available": true
}
```

### Complexity Estimate
**12-16 hours.** The most complex P0 action:
- Shortest path: 1 hour (existing pattern)
- Path scoring infrastructure: 3 hours
- Safety heuristic (reuse sightline/hiding spot analysis): 3 hours
- Explorer heuristic (waypoint discovery + TSP-lite routing): 3 hours
- Cautious heuristic (corridor width + wall proximity): 3 hours
- Multi-path deduplication and output: 2 hours

### Gotchas
- **Navmesh must exist.** If not built, all queries fail. Should return clear error with hint to build navmesh.
- `FindPathSync` finds ONE path. For multiple paths with different costs, need to either:
  - Use `FNavigationQueryFilter` with modified area costs (complex, requires Recast internals)
  - Compose paths through scored waypoints (recommended — more flexible, easier to implement)
- Grid sampling for safety scores can be expensive. Use coarse grid (200cm) and limit to bounding box of start-to-end corridor.
- Existing horror actions (`analyze_sightlines`, `find_hiding_spots`, `classify_zone_tension`) can be called internally as scoring functions — reuse the helper functions, not the MCP wrappers.
- Player walk speed assumption needed for time estimates (~400 cm/s default UE character).

---

## 6. `evaluate_spawn_point`

### What It Does
Composite score for a spawn location: visibility delay, audio cover, lighting, escape proximity, path commitment.

### Exact UE5 API
This is a **composite action** that calls existing Monolith infrastructure:
- `analyze_sightlines` → claustrophobia score, blocked percentages (already implemented)
- `sample_light_levels` → luminance at point (already implemented)
- `analyze_escape_routes` → exit proximity (already implemented)
- `find_ambush_points` → concealment scoring (already implemented)
- Line traces for visibility checks: `World->LineTraceTestByChannel()`

**New analysis needed:**
```cpp
// Audio cover (ambient noise sources near spawn)
// Already have audio_analysis infrastructure in MonolithMeshAudioActions

// Visibility delay: time before player can see spawn from predicted paths
// Uses predict_player_paths output + sightline checks

// Path commitment: how far along a one-way path is the player when spawn triggers
// Uses choke_points analysis + escape_routes
```

### Editor-Time Feasibility
**Yes.** All sub-analyses work at editor time.

### Threading
**Mostly game thread.** Could parallelize independent scoring dimensions.

### Params and Return Shape

**Input:**
```json
{
  "location": [x, y, z],
  "player_paths": [[x, y, z], ...],
  "weights": {
    "visibility_delay": 1.0,
    "lighting": 1.0,
    "audio_cover": 0.5,
    "escape_proximity": 1.5,
    "path_commitment": 1.0
  }
}
```

**Output:**
```json
{
  "score": 0.82,
  "grade": "A",
  "breakdown": {
    "visibility_delay": {"score": 0.9, "delay_distance_cm": 450},
    "lighting": {"score": 0.7, "luminance": 0.03},
    "audio_cover": {"score": 0.6, "nearby_sources": 2},
    "escape_proximity": {"score": 0.95, "nearest_exit_cm": 800},
    "path_commitment": {"score": 0.8, "commitment_ratio": 0.65}
  },
  "suggestions": ["Add audio source within 300cm for cover", "Reduce ambient light"]
}
```

### Complexity Estimate
**6 hours.** Most work is compositing existing analyses. New code needed for:
- Visibility delay calculation (sightline from path points to spawn)
- Path commitment scoring
- Weighted composite scoring with grade thresholds

### Gotchas
- Depends on `predict_player_paths` for best results — without it, requires manual path input.
- Scoring weights should be configurable per horror archetype (stalker wants high concealment, berserker wants short visibility delay).
- Need clear documentation of what each sub-score means.

---

## 7. `suggest_scare_positions`

### What It Does
Find optimal positions for scripted scare events along a path. Scores anticipation, visibility, timing, player agency.

### Exact UE5 API
Purely algorithmic composition of existing APIs:
- Path sampling at intervals
- `analyze_sightlines` at each sample (directional, FOV-based)
- `classify_zone_tension` for tension context
- `evaluate_spawn_point` for each candidate
- `find_hiding_spots` for concealment options near path

**Additional spatial analysis:**
```cpp
// Timing: distance along path → estimated time (constant walk speed)
// Visibility: first point where candidate becomes visible from path
// Anticipation: tension ramp from analyze_pacing_curve
// Agency: nearby escape routes from analyze_escape_routes
```

### Editor-Time Feasibility
**Yes.** Pure computation over existing data.

### Threading
**Parallelizable.** Candidate evaluation at each path sample is independent — `ParallelFor` over candidates.

### Params and Return Shape

**Input:**
```json
{
  "path_points": [[x, y, z], ...],
  "scare_type": "audio|visual|entity_spawn|environmental",
  "count": 5,
  "min_spacing_cm": 1000,
  "intensity_curve": "escalating|wave|random",
  "hospice_mode": false
}
```

**Output:**
```json
{
  "positions": [
    {
      "location": [x, y, z],
      "score": 0.91,
      "path_distance_cm": 2400,
      "estimated_time_s": 12.0,
      "visibility_from_path_cm": 300,
      "tension_context": "tense",
      "escape_options": 2,
      "suggested_direction": [0.7, -0.7, 0]
    }
  ],
  "pacing_analysis": "Good escalation. 3 positions have breather gaps > 5s."
}
```

### Complexity Estimate
**5 hours.** Mostly composition and scoring logic. The individual analyses are already built.

### Gotchas
- `hospice_mode` should cap intensity and ensure adequate spacing between scares.
- Scare types have different requirements: audio scares need acoustic context, visual scares need sightline geometry, entity spawns need concealment.
- Min spacing should be enforced AFTER scoring to avoid greedy local optima — score all, then select with spacing constraint (greedy or dynamic programming).

---

## 8. `evaluate_encounter_pacing`

### What It Does
Analyze spacing and intensity across multiple encounters along a path. Flag back-to-back encounters with no breather.

### Exact UE5 API
Composition of existing `analyze_pacing_curve` (already implemented) + encounter metadata:
```cpp
// analyze_pacing_curve already samples tension at intervals along a path
// This action extends it with encounter locations and types

// Per-encounter scoring uses:
// - classify_zone_tension at encounter location
// - Distance/time between encounters
// - Post-encounter recovery (tension drop rate in surrounding area)
```

### Editor-Time Feasibility
**Yes.** Pure analysis.

### Threading
**Parallelizable.** Each encounter's zone analysis is independent.

### Params and Return Shape

**Input:**
```json
{
  "path_points": [[x, y, z], ...],
  "encounters": [
    {"location": [x, y, z], "type": "combat", "intensity": 0.8, "duration_s": 30},
    {"location": [x, y, z], "type": "jumpscare", "intensity": 0.9, "duration_s": 3}
  ],
  "target_pacing": "horror_standard|hospice_gentle|action",
  "walk_speed_cms": 400
}
```

**Output:**
```json
{
  "overall_score": 0.72,
  "issues": [
    {"type": "too_close", "encounters": [0, 1], "gap_seconds": 8.0, "recommended_min": 20.0},
    {"type": "no_breather", "region": [1200, 2400], "tension_min": 0.6, "recommended_max": 0.3}
  ],
  "tension_curve": [
    {"distance": 0, "tension": 0.2},
    {"distance": 500, "tension": 0.5},
    {"distance": 1200, "tension": 0.8}
  ],
  "hospice_compliance": true
}
```

### Complexity Estimate
**4 hours.** Extends existing `analyze_pacing_curve` with encounter-aware scoring.

### Gotchas
- Tension recovery rate after encounters is genre-dependent. Horror recovery is slow; action is fast.
- Hospice mode needs strict caps: max intensity, mandatory rest periods, no jump scares.
- Consider cumulative fatigue — later encounters feel more intense even at same objective intensity.

---

## 9. `set_actor_material` / `swap_material_in_level`

### What It Does
Assign materials to placed actors by slot index or name. Swap all instances of material X with material Y across a level.

### Exact UE5 API

```cpp
// UMeshComponent (base for Static, Skeletal, Procedural mesh components)
void UMeshComponent::SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
// Engine/Private/Components/MeshComponent.cpp:63

void UMeshComponent::SetMaterialByName(FName MaterialSlotName, UMaterialInterface* Material);
// Engine/Private/Components/MeshComponent.cpp:138

// Reading current materials:
UMaterialInterface* UMeshComponent::GetMaterial(int32 ElementIndex) const;
int32 UMeshComponent::GetNumMaterials() const;
TArray<FName> UMeshComponent::GetMaterialSlotNames() const;
```

### Editor-Time Feasibility
**Yes.** `SetMaterial` creates a per-instance override that persists in the level. Does NOT modify the source mesh asset.

### Threading
**Game thread only.** Component modification.

### Params and Return Shape

**`set_actor_material` input:**
```json
{
  "actor_name": "SM_Wall_01",
  "material": "/Game/Materials/MI_Concrete_Dirty",
  "slot": 0,
  "slot_name": "Base"
}
```

**`swap_material_in_level` input:**
```json
{
  "source_material": "/Game/Materials/MI_Blockout_Grey",
  "target_material": "/Game/Materials/MI_Concrete_01",
  "actors": ["Wall_01", "Wall_02"],
  "preview": true
}
```

**Output:**
```json
{
  "actors_modified": 23,
  "slots_modified": 31,
  "details": [
    {"actor": "SM_Wall_01", "component": "StaticMeshComponent0", "slot": 0, "old": "MI_Blockout_Grey", "new": "MI_Concrete_01"}
  ]
}
```

### Complexity Estimate
**4 hours.** Clean, straightforward API. Pattern matches `find_replace_mesh`.

### Gotchas
- `SetMaterial` creates an **override array** on the component. If you set slot 2 but not 0-1, the override array fills slots 0-1 with the mesh's default materials.
- `SetMaterialByName` requires the slot name from the mesh's material list, not a arbitrary name.
- Calling `Modify()` before changes is essential for undo support.
- Material instances (MIDs) created at runtime won't persist — only asset references persist in the level.

---

## 10. `analyze_texel_density`

### What It Does
Calculate texels/cm for meshes. Reports UV space usage vs world-space area, combined with texture resolution.

### Exact UE5 API

**UV density is already computed and stored:**
```cpp
// FMeshUVChannelInfo (MeshUVChannelInfo.h)
struct FMeshUVChannelInfo {
    bool bInitialized;
    bool bOverrideDensities;
    float LocalUVDensities[MAX_TEXCOORDS];  // world units per UV unit
};

// Accessible per-section in FStaticMeshRenderData
FStaticMeshRenderData* RenderData = StaticMesh->GetRenderData();
// Each LODResource section has UVDensities

// The scene proxy also provides it:
bool FStaticMeshSceneProxy::GetMeshUVDensities(int32 LODIndex, int32 SectionIndex, FVector4& WorldUVDensities) const;

// Full computation path:
void FStaticMeshRenderData::ComputeUVDensities(UStaticMesh* Mesh);
// StaticMesh.cpp:3939
```

**Texel density formula:**
```
TexelsPerCm = TextureResolution / (LocalUVDensity * ActorScale)
// LocalUVDensity = world units per full UV (0-1) range
// E.g., UV density of 200 means 200cm covers full UV, with 1024 texture = 5.12 texels/cm
```

**Getting texture resolution from material:**
```cpp
UMaterialInterface* Mat = MeshComp->GetMaterial(SectionIndex);
// Walk the material graph to find texture parameters
TArray<UTexture*> Textures;
Mat->GetUsedTextures(Textures, EMaterialQualityLevel::High, true, GMaxRHIFeatureLevel, true);
// Each UTexture has GetSizeX() / GetSizeY()
```

### Editor-Time Feasibility
**Yes.** All data is available at editor time from mesh render data and material textures.

### Threading
**Read-only analysis can run on any thread.** Material texture walking should be on game thread.

### Params and Return Shape

**Input:**
```json
{
  "actor_name": "SM_Wall_01",
  "target_density": 5.12,
  "uv_channel": 0
}
```
Or region mode:
```json
{
  "region_min": [0, 0, 0],
  "region_max": [5000, 5000, 500],
  "target_density": 5.12
}
```

**Output:**
```json
{
  "actors": [
    {
      "actor_name": "SM_Wall_01",
      "mesh": "/Game/Art/SM_Wall_01",
      "sections": [
        {
          "slot": 0,
          "material": "MI_Concrete_01",
          "texture_resolution": 2048,
          "uv_density_world_units": 200.0,
          "actor_scale": [1.0, 1.0, 1.0],
          "texels_per_cm": 10.24,
          "deviation_from_target": "+100%",
          "status": "over"
        }
      ]
    }
  ],
  "summary": {
    "average_density": 7.5,
    "min_density": 2.1,
    "max_density": 15.3,
    "actors_under_target": 3,
    "actors_over_target": 5
  }
}
```

### Complexity Estimate
**6 hours.** UV density data exists; the work is:
1. Collecting per-section UV densities (from FMeshUVChannelInfo or manual calculation from MeshDescription)
2. Walking materials to find texture resolutions
3. Combining with actor scale
4. Comparison and reporting against target density

### Gotchas
- `FMeshUVChannelInfo::LocalUVDensities` may not be initialized on all meshes. Check `bInitialized`.
- Texture resolution requires walking the material to find which texture is used per UV channel — complex for layered materials or material instances.
- Actor scale affects world-space density. Non-uniform scale means non-uniform texel density.
- Nanite meshes may not have traditional LODs/UV densities exposed the same way.
- Consider offering `compare_texel_density_in_region` as part of the same action with a region mode.

---

## 11. `find_instancing_candidates`

### What It Does
Iterate actors, group by mesh path, count instances, report candidates for HISM conversion.

### Exact UE5 API

```cpp
// Pattern: iterate all StaticMeshActors, group by mesh reference
TMap<UStaticMesh*, TArray<AStaticMeshActor*>> MeshGroups;
for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
{
    UStaticMeshComponent* SMC = It->GetStaticMeshComponent();
    if (SMC && SMC->GetStaticMesh())
    {
        MeshGroups.FindOrAdd(SMC->GetStaticMesh()).Add(*It);
    }
}
// Filter groups with count >= threshold (e.g., 5+)
```

### Editor-Time Feasibility
**Yes.** Read-only iteration.

### Threading
**Can background.** Read-only actor iteration. However, `TActorIterator` should be game-thread.

### Params and Return Shape

**Input:**
```json
{
  "min_count": 5,
  "region_min": [0, 0, 0],
  "region_max": [10000, 10000, 1000],
  "include_materials": true
}
```

**Output:**
```json
{
  "candidates": [
    {
      "mesh": "/Game/Art/SM_Pipe_01",
      "count": 47,
      "estimated_draw_call_savings": 46,
      "total_triangles": 14100,
      "unique_material_sets": 2,
      "actors": ["Pipe_01", "Pipe_02", "..."]
    }
  ],
  "total_potential_savings": 180
}
```

### Complexity Estimate
**3 hours.** Simple grouping and counting. Very similar to existing performance analysis patterns.

### Gotchas
- Actors with different material overrides on the same mesh create different "instance groups" — HISM requires identical materials per instance.
- Include scale in grouping? Non-uniform scale per instance is supported by HISM but worth flagging.
- Already-instanced meshes (ISM/HISM components) should be excluded or reported separately.

---

## 12. `convert_to_hism`

### What It Does
Convert groups of StaticMeshActors sharing the same mesh into a single HISM actor.

### Exact UE5 API

```cpp
// UHierarchicalInstancedStaticMeshComponent (Engine/Classes/Components/HierarchicalInstancedStaticMeshComponent.h)
// Derives from UInstancedStaticMeshComponent

// 1. Spawn a new actor with HISM component
AActor* HISMActor = World->SpawnActor<AActor>(Location, FRotator::ZeroRotator, SpawnParams);
UHierarchicalInstancedStaticMeshComponent* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(HISMActor);
HISM->RegisterComponent();
HISM->SetStaticMesh(SharedMesh);
HISMActor->SetRootComponent(HISM);

// 2. Add instances from each source actor
for (AStaticMeshActor* SMA : SourceActors)
{
    FTransform InstanceTransform = SMA->GetActorTransform();
    HISM->AddInstance(InstanceTransform, true /*bWorldSpace*/);
}

// 3. Copy material overrides if consistent
HISM->SetMaterial(SlotIndex, Material);

// 4. Delete original actors
for (AStaticMeshActor* SMA : SourceActors)
{
    World->DestroyActor(SMA);
}

// Key methods:
int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);  // :305
TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms, bool bShouldReturnIndices, bool bWorldSpace, bool bUpdateNavigation);  // :306
```

### Editor-Time Feasibility
**Yes.** HISM component creation and instance adding work at editor time. The component will build its cluster tree on the next tick.

### Threading
**Game thread only.** Actor creation/destruction, component registration.

### Params and Return Shape

**Input:**
```json
{
  "mesh": "/Game/Art/SM_Pipe_01",
  "actors": ["Pipe_01", "Pipe_02", "Pipe_03"],
  "name": "HISM_Pipes",
  "folder": "Instanced/Pipes",
  "preserve_materials": true
}
```

**Output:**
```json
{
  "hism_actor": "HISM_Pipes",
  "mesh": "/Game/Art/SM_Pipe_01",
  "instance_count": 47,
  "actors_removed": 47,
  "draw_call_savings": 46
}
```

### Complexity Estimate
**6 hours.** The HISM creation is straightforward, but edge cases add complexity:
1. Material override validation (all instances must share materials)
2. Undo support (need to store original actor data for reversal)
3. Per-instance custom data preservation
4. Collision settings matching

### Gotchas
- **Material overrides must be uniform** across all instances. If actors have different overrides, either reject or split into multiple HISMs.
- HISM tree rebuild can be slow for very large instance counts (1000+). Use `AddInstances` (batch) instead of `AddInstance` (single) for performance.
- Collision on HISM is expensive — consider disabling per-instance collision and using simple collision on the HISM itself.
- Per-instance custom data (custom primitive data) won't carry over from individual actors.
- `bUpdateNavigation = false` during batch add, then trigger once after.

---

## 13. `auto_generate_lods`

### What It Does
One-shot LOD pipeline: load mesh → generate LODs via simplification → save back with screen sizes.

### Exact UE5 API

**Current state:** `generate_lods` already exists in MonolithMesh! It uses GeometryScript:
```cpp
// Already implemented in MonolithMeshOperationActions.cpp:487
FGeometryScriptSimplifyMeshOptions SimplifyOpts;
UGeometryScriptLibrary_MeshSimplifyFunctions::ApplySimplifyToTriangleCount(LodMesh, TargetTris, SimplifyOpts);
```

**What's missing is writing LODs back to a UStaticMesh asset:**
```cpp
// UStaticMesh LOD management (StaticMesh.h)
void SetNumSourceModels(int32 Num);       // :1943
FStaticMeshSourceModel& AddSourceModel();  // :1940
FStaticMeshSourceModel& GetSourceModel(int32 Index);  // :1947

// FStaticMeshSourceModel contains:
// - FMeshDescription MeshDescription
// - FMeshBuildSettings BuildSettings
// - FMeshReductionSettings ReductionSettings
// - FPerPlatformFloat ScreenSize

// Screen sizes on render data:
FStaticMeshRenderData::ScreenSize[LODIndex]  // FPerPlatformFloat, StaticMeshResources.h:789

// Write path: set source model screen sizes, then Build():
SM->GetSourceModel(LODIndex).ScreenSize = ScreenSizeValue;
SM->Build(false);  // Rebuild render data
SM->PostEditChange();
SM->MarkPackageDirty();
```

**Alternative: use IMeshReductionManagerModule:**
```cpp
// The engine's built-in mesh reduction (uses Simplygon or internal reducer)
IMeshReductionManagerModule& Module = FModuleManager::Get().LoadModuleChecked<IMeshReductionManagerModule>("MeshReductionInterface");
IMeshReduction* MeshReduction = Module.GetStaticMeshReductionInterface();
// This is what the editor's LOD generation UI uses
```

### Editor-Time Feasibility
**Yes.** The editor's LOD generation is entirely editor-time. Both GeometryScript simplification and the built-in mesh reduction work without PIE.

### Threading
**Mostly game thread.** Mesh simplification can theoretically be backgrounded, but `UStaticMesh::Build()` must be game thread.

### Params and Return Shape

**Input:**
```json
{
  "asset_path": "/Game/Art/SM_Wall_01",
  "lod_count": 3,
  "reduction_per_lod": 0.5,
  "screen_sizes": [1.0, 0.5, 0.25, 0.125],
  "auto_screen_sizes": true,
  "preserve_uv_borders": true
}
```

**Output:**
```json
{
  "asset_path": "/Game/Art/SM_Wall_01",
  "lods": [
    {"lod": 0, "triangles": 5000, "screen_size": 1.0},
    {"lod": 1, "triangles": 2500, "screen_size": 0.5},
    {"lod": 2, "triangles": 1250, "screen_size": 0.25},
    {"lod": 3, "triangles": 625, "screen_size": 0.125}
  ],
  "status": "built"
}
```

### Complexity Estimate
**8 hours.** The generation already works. The new work:
1. Bridge from GeometryScript DynamicMesh → FMeshDescription for UStaticMesh source models
2. Set screen sizes on source models
3. Trigger `UStaticMesh::Build()` to regenerate render data
4. Batch mode (multiple meshes in one call)
5. Error handling and progress reporting

### Gotchas
- **GeometryScript → FMeshDescription bridge:** Need `UGeometryScriptLibrary_MeshBasicEditFunctions::CopyMeshToStaticMesh` or manual conversion via `FMeshDescriptionToDynamicMesh`/`FDynamicMeshToMeshDescription`.
- `UStaticMesh::Build()` can be slow for complex meshes. Consider async if processing multiple meshes.
- Existing LODs on the mesh will be overwritten — warn the user.
- Screen sizes should be monotonically decreasing. Validate input.
- After `Build()`, need `PostEditChange()` and `MarkPackageDirty()`.

---

## 14. `set_lod_screen_sizes`

### What It Does
Set LOD screen size transitions on a UStaticMesh without regenerating geometry.

### Exact UE5 API

```cpp
UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *AssetPath);

// Method 1: Set on source models (persistent, survives rebuild)
for (int32 i = 0; i < SM->GetNumSourceModels(); ++i)
{
    SM->GetSourceModel(i).ScreenSize = NewScreenSizes[i];
}

// Method 2: Set directly on render data (immediate but overwritten on rebuild)
FStaticMeshRenderData* RenderData = SM->GetRenderData();
RenderData->ScreenSize[LODIndex].Default = NewScreenSize;

// After setting on source models, rebuild to update render data:
SM->Build(false);
SM->PostEditChange();
SM->MarkPackageDirty();
```

### Editor-Time Feasibility
**Yes.** Property setting on UStaticMesh works at editor time.

### Threading
**Game thread only.** Asset modification.

### Params and Return Shape

**Input:**
```json
{
  "asset_path": "/Game/Art/SM_Wall_01",
  "screen_sizes": [1.0, 0.4, 0.15, 0.05]
}
```

**Output:**
```json
{
  "asset_path": "/Game/Art/SM_Wall_01",
  "lod_count": 4,
  "screen_sizes": [1.0, 0.4, 0.15, 0.05],
  "previous_sizes": [1.0, 0.5, 0.25, 0.125]
}
```

### Complexity Estimate
**2 hours.** Very straightforward property setting.

### Gotchas
- Screen sizes must be monotonically decreasing (LOD0 > LOD1 > LOD2 > ...).
- `FPerPlatformFloat` has `.Default` plus platform overrides. Only set `.Default` unless platform-specific.
- Need `Build(false)` after changing source model screen sizes for the changes to take effect on render data.
- If there are more screen_sizes than LODs, error. If fewer, only modify the provided ones.
- The render data ScreenSize array is indexed by LOD. LOD0 screen size is typically 1.0 (always visible when on screen).

---

## 15. Summary Table

| Action | Feasibility | Threading | Complexity | Key API | Dependencies |
|--------|------------|-----------|------------|---------|-------------|
| `place_light` | Trivial | Game thread | 4h | `World->SpawnActor<APointLight>` + component setters | None new |
| `set_light_properties` | Trivial | Game thread | (included above) | `ULightComponent::SetIntensity/SetLightColor` | None new |
| `find_replace_mesh` | Easy | Game thread | 3h | `TActorIterator` + `SetStaticMesh` | None new |
| `spawn_volume` | Moderate | Game thread | 8h | `UCubeBuilder` + `UActorFactory::CreateBrushForVolumeActor` | `Builders/CubeBuilder.h` |
| `get_actor_properties` | Moderate | Game thread | 6h | `FProperty::ExportText_Direct/ImportText_Direct` | None new |
| `copy_actor_properties` | (included above) | Game thread | (included above) | Same as above | None new |
| `predict_player_paths` | Complex | Mixed | 12-16h | `FindPathSync` + scoring composition | Existing nav system |
| `evaluate_spawn_point` | Moderate | Mixed | 6h | Composition of existing analyses | `predict_player_paths` |
| `suggest_scare_positions` | Moderate | Parallelizable | 5h | Path sampling + scoring | Existing horror actions |
| `evaluate_encounter_pacing` | Easy | Parallelizable | 4h | Extends `analyze_pacing_curve` | None new |
| `set_actor_material` | Trivial | Game thread | 4h | `UMeshComponent::SetMaterial` | None new |
| `swap_material_in_level` | Easy | Game thread | (included above) | `TActorIterator` + `SetMaterial` | None new |
| `analyze_texel_density` | Moderate | Mixed | 6h | `FMeshUVChannelInfo::LocalUVDensities` + material texture walk | None new |
| `find_instancing_candidates` | Easy | Game thread | 3h | `TActorIterator` grouping | None new |
| `convert_to_hism` | Moderate | Game thread | 6h | `UHierarchicalInstancedStaticMeshComponent::AddInstances` | None new |
| `auto_generate_lods` | Moderate | Mixed | 8h | Existing `generate_lods` + `UStaticMesh::Build` | GeometryScript |
| `set_lod_screen_sizes` | Trivial | Game thread | 2h | `FStaticMeshSourceModel::ScreenSize` | None new |

**Total: ~65-80 hours** (TODO estimated 80h, validated as accurate)

---

## Implementation Priority Recommendation

### Phase 1 — Quick Wins (14h, enables immediate use)
1. `place_light` / `set_light_properties` (4h) — Lighting IS horror
2. `set_actor_material` / `swap_material_in_level` (4h) — Bridges mesh + material
3. `find_replace_mesh` (3h) — Blockout-to-art essential
4. `set_lod_screen_sizes` (2h) — Trivial, completes LOD pipeline
5. `find_instancing_candidates` (3h) — Read-only, immediate perf insight

### Phase 2 — Level Design Power (14h)
6. `spawn_volume` (8h) — Can't build functional levels without this
7. `get_actor_properties` / `copy_actor_properties` (6h) — General-purpose property tool

### Phase 3 — Horror Intelligence (27-31h)
8. `predict_player_paths` (12-16h) — THE multiplier for all horror tools
9. `evaluate_spawn_point` (6h) — Depends on paths
10. `suggest_scare_positions` (5h) — Depends on paths + spawn eval
11. `evaluate_encounter_pacing` (4h) — Standalone but benefits from paths

### Phase 4 — Performance Tools (14h)
12. `analyze_texel_density` (6h) — Visual quality #1
13. `convert_to_hism` (6h) — Depends on find_instancing_candidates
14. `auto_generate_lods` (8h) — Extends existing generate_lods

---

## Build.cs Changes Needed

```cpp
// MonolithMesh.Build.cs additions needed:
"UnrealEd",          // For UActorFactory::CreateBrushForVolumeActor, UCubeBuilder (already present?)
"Builders",          // UCubeBuilder, UCylinderBuilder (check if separate module)
```

The `NavigationSystem` dependency is already present. `GeometryScriptingCore` is already present for mesh operations. No new plugin dependencies required.
