# Auto-Volume Generation for Procedural Buildings

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Automatic NavMesh, Blocking, Audio, Trigger, and Post-Process volume generation when procedurally creating buildings/city blocks via Monolith MCP.

---

## Table of Contents

1. [Goal](#goal)
2. [Existing Monolith Actions](#existing-monolith-actions)
3. [Auto-NavMesh](#1-auto-navmesh)
4. [Auto-BlockingVolumes Per Room](#2-auto-blockingvolumes-per-room)
5. [Auto-AudioVolumes Per Room](#3-auto-audiovolumes-per-room)
6. [Auto-TriggerVolumes](#4-auto-triggervolumes)
7. [Auto-PostProcessVolumes](#5-auto-postprocessvolumes)
8. [Stairs & NavLinks](#6-stairs--navlinks)
9. [Performance Budget](#7-performance-budget)
10. [Pipeline Ordering](#8-pipeline-ordering)
11. [New Actions](#9-new-actions)
12. [Implementation Plan](#10-implementation-plan)
13. [Effort Estimate](#11-effort-estimate)

---

## Goal

When a building or city block is generated via `create_structure`, `create_city_block`, or future proc-gen actions, all supporting volumes should be auto-created so the space is **immediately playable**:

- AI navigation works (navmesh built)
- `scatter_props` can target rooms (blocking volumes with Monolith tags)
- Acoustics are configured (reverb per room)
- Gameplay triggers exist (door entries, floor transitions)
- Visual mood is set (post-process per building/room)

---

## Existing Monolith Actions

Already implemented and fully functional:

| Action | Module | Does What |
|--------|--------|-----------|
| `spawn_volume` | VolumeActions | Spawns trigger/blocking/kill/pain/nav_modifier/audio/post_process with UCubeBuilder brush geometry |
| `build_navmesh` | VolumeActions | `UNavigationSystemV1::Build()` — synchronous full or dirty_only |
| `setup_blockout_volume` | BlockoutActions | Tags a BlockingVolume with Monolith room_type, density, content tags |
| `create_audio_volume` | AudioActions | Spawns AAudioVolume matching a blocking volume's shape, sets reverb/priority |
| `analyze_room_acoustics` | AudioActions | Sabine RT60 estimation, surface material absorption, room classification |
| `suggest_audio_volumes` | AudioActions | Analyzes a region and recommends reverb settings |
| `get_blockout_volumes` | BlockoutActions | Lists all Monolith-tagged blockout volumes |
| `get_actor_properties` | VolumeActions | Read any UPROPERTY via reflection |
| `classify_zone_tension` | HorrorActions | Horror tension classification for areas |
| `get_light_coverage` | LightingActions | Light coverage analysis for volumes |

**NOT currently supported:** `ANavMeshBoundsVolume` (not in `ResolveVolumeClass`), `ANavLinkProxy`.

---

## 1. Auto-NavMesh

### 1.1 NavMeshBoundsVolume

**API (verified UE 5.7):**
```cpp
// NavMesh/NavMeshBoundsVolume.h
class ANavMeshBoundsVolume : public AVolume
{
    UPROPERTY(EditAnywhere, Category = Navigation)
    FNavAgentSelector SupportedAgents;

    virtual void PostRegisterAllComponents() override; // auto-registers with NavSys
    virtual void PostUnregisterAllComponents() override;
};
```

**Spawning approach:** Same as existing `spawn_volume` — `SpawnActor<ANavMeshBoundsVolume>` + `UCubeBuilder` + `UActorFactory::CreateBrushForVolumeActor()`. The `PostRegisterAllComponents()` override automatically calls `UNavigationSystemV1::OnNavigationBoundsAdded()`.

**Extent calculation from building bounding boxes:**
```
1. Iterate all buildings in block descriptor
2. Compute union AABB of all building footprints
3. Pad by NAVMESH_PADDING (200cm = 2m on each side)
4. Z extent: from ground level to max building height + 100cm
5. Single NavMeshBoundsVolume covers entire block
```

**Padding rationale:** 200cm covers sidewalks, allows AI to navigate around building exteriors. Overly tight bounds cause navmesh edge artifacts.

**One volume per block, not per building.** Multiple overlapping NavMeshBoundsVolumes work but are unnecessary overhead. The nav system unions all bounds anyway. One padded block volume is optimal.

### 1.2 NavMesh Build Timing

**Critical ordering:** Must happen AFTER:
1. All geometry is placed (static meshes)
2. All collision is generated on all meshes (see auto-collision research — `SetStaticMeshCollisionFromMesh`)
3. All blocking volumes are placed
4. All NavModifierVolumes are placed

**API:** `UNavigationSystemV1::Build()` is synchronous, blocks game thread. Existing `build_navmesh` action wraps this.

**The nav system REQUIRES at least one NavMeshBoundsVolume** to generate navmesh. Without it, `Build()` finds no bounds and skips generation (checked in `NavigationSystem.cpp:2541`):
```cpp
for (auto It = NavBoundsVolumes.CreateConstIterator(); It; ++It)
{
    ANavMeshBoundsVolume const* const V = (*It);
    if (IsValid(V)) { bCreateNavigation = true; break; }
}
```

### 1.3 NavModifierVolumes for Behavior Zones

**API (verified UE 5.7):**
```cpp
// NavModifierVolume.h
class ANavModifierVolume : public AVolume, public INavRelevantInterface
{
    TSubclassOf<UNavArea> AreaClass;          // NavArea to apply inside
    TSubclassOf<UNavArea> AreaClassToReplace; // Optional replacement target
    bool bMaskFillCollisionUnderneathForNavmesh;
    ENavigationDataResolution NavMeshResolution;

    void SetAreaClass(TSubclassOf<UNavArea> NewAreaClass);
    void SetAreaClassToReplace(TSubclassOf<UNavArea> NewAreaClassToReplace);
    void RebuildNavigationData();
};
```

**Built-in NavArea classes:**
- `UNavArea_Default` — normal walkable (cost 1.0)
- `UNavArea_Null` — completely blocked (cost MAX, used as obstacle)
- `UNavArea_LowHeight` — crouching areas (`IsLowArea()` returns true)
- `UNavArea_Obstacle` — high cost, avoidable (cost ~100)

**Use cases for procedural buildings:**

| Zone | NavArea | Why |
|------|---------|-----|
| Narrow corridor (<150cm wide) | `UNavArea_LowHeight` | AI crouches, moves slower |
| Stairwells | Custom `UNavArea_Stairs` | Higher cost, AI prefers other routes |
| Doorways | `UNavArea_Default` | Normal traversal but useful as chokepoint marker |
| Blocked debris | `UNavArea_Null` | Completely impassable |
| Window openings | `UNavArea_Null` | AI can't jump through windows |

**Custom NavArea creation:** Subclass `UNavArea`, set `DrawColor`, `DefaultCost`, and `FixedAreaEnteringCost` in constructor. For procedural use, recommend creating 2-3 custom NavAreas in C++:
- `UNavArea_Stairs` (DefaultCost = 2.0, FixedAreaEnteringCost = 50.0)
- `UNavArea_SlowZone` (DefaultCost = 3.0) — for cluttered rooms, debris

### 1.4 NavMesh Stair Handling

**UE 5.7 RecastNavMesh handles stairs automatically** if the geometry has collision and the step height is within `AgentMaxStepHeight` (default 35cm for humanoids). The rasterization process detects walkable surfaces via `walkableSlopeAngle` and `walkableClimb`:

```cpp
// RecastNavMeshGenerator.cpp:5191
OutConfig.walkableClimb = FMath::CeilToInt(AgentMaxClimb / CellHeight);
OutConfig.walkableRadius = FMath::CeilToInt(AgentRadius / CellSize);
OutConfig.walkableSlopeAngle = AgentMaxSlope;
```

**Default values:** `AgentMaxSlope` = 44 degrees, `AgentMaxStepHeight` = 35cm. Standard residential stairs (18cm rise, ~30 degree slope) are auto-navigable. **No NavLinks needed for standard stairs.**

**NavLinks ARE needed when:**
- Stairs are too steep (>44 degrees)
- There's a gap/jump between floors
- Ladder or climbing mechanic
- One-way drops (windows, ledges)

---

## 2. Auto-BlockingVolumes Per Room

### 2.1 Room Volume Strategy

For each room in the spatial registry, spawn a `ABlockingVolume` matching its **interior dimensions** (wall thickness subtracted from room AABB).

**Calculation from spatial registry room data:**
```
Interior extent.X = room.width / 2 - wall_thickness
Interior extent.Y = room.length / 2 - wall_thickness
Interior extent.Z = room.height / 2
Location = room.center (already center-origin in spatial registry convention)
```

**Wall thickness:** Default 20cm (from `create_structure` defaults). The blocking volume should NOT include the walls — it represents the traversable/fillable interior.

### 2.2 Auto-Setup with Monolith Tags

After spawning, immediately call `setup_blockout_volume` logic internally with:

```json
{
    "volume_name": "BV_{BuildingName}_{Floor}_{RoomName}",
    "room_type": "{from spatial registry room.purpose}",
    "density": "Normal",
    "tags": ["Furniture.{RoomType}"]
}
```

**Room type mapping from spatial registry:**

| Room Purpose | room_type | Suggested Tags |
|-------------|-----------|----------------|
| kitchen | Kitchen | Furniture.Kitchen, Props.Food |
| bedroom | Bedroom | Furniture.Bedroom, Props.Clothing |
| bathroom | Bathroom | Furniture.Bathroom, Props.Hygiene |
| living_room | LivingRoom | Furniture.LivingRoom, Props.Books |
| corridor | Hallway | Props.Small |
| office | Office | Furniture.Office, Props.Papers |
| storage | Storage | Props.Boxes, Props.Tools |
| stairwell | Stairwell | (minimal props) |
| lobby | Lobby | Furniture.Commercial |
| utility | Utility | Props.Tools, Props.Pipes |

### 2.3 Naming Convention

`BV_{BuildingName}_{FloorNumber}_{RoomName}`

Examples:
- `BV_House01_F0_Kitchen`
- `BV_Clinic_F1_Hallway`
- `BV_Police_F0_LobbyA`

Use `SetFolderPath("Volumes/Blockout/{BuildingName}")` for outliner organization.

---

## 3. Auto-AudioVolumes Per Room

### 3.1 API Surface (Verified UE 5.7)

```cpp
// AudioVolume.h
class AAudioVolume : public AVolume
{
    float Priority;
    uint32 bEnabled:1;
    FReverbSettings Settings;        // reverb effect, volume, fade time
    FInteriorSettings AmbientZoneSettings; // exterior/interior attenuation
    TArray<FAudioVolumeSubmixSendSettings> SubmixSendSettings;

    void SetPriority(float NewPriority);
    void SetEnabled(bool bNewEnabled);
    void SetReverbSettings(const FReverbSettings& NewReverbSettings);
    void SetInteriorSettings(const FInteriorSettings& NewInteriorSettings);
};

// FInteriorSettings members:
//   ExteriorVolume, ExteriorTime, ExteriorLPF, ExteriorLPFTime
//   InteriorVolume, InteriorTime, InteriorLPF, InteriorLPFTime

// FReverbSettings members:
//   bApplyReverb, ReverbEffect (UReverbEffect*), Volume, FadeTime
```

### 3.2 Room Size → Reverb Preset Mapping

Based on Sabine equation and real-world acoustic data:

| Category | Volume Range (m³) | RT60 Target (s) | Reverb Preset Name | Example Rooms |
|----------|-------------------|-----------------|-------------------|---------------|
| Tiny | < 10 | 0.3-0.5 | Bathroom | Closet, small bathroom |
| Small | 10-50 | 0.5-0.8 | Office | Bedroom, office, kitchen |
| Medium | 50-200 | 0.8-1.5 | Hall | Living room, classroom |
| Large | 200-1000 | 1.5-3.0 | Warehouse | Gym, warehouse, church |
| Huge | > 1000 | 3.0-6.0 | Cathedral | Atrium, parking garage |
| Corridor | Any (L/W ratio > 4) | 0.8-1.2 | Corridor | Hallway, tunnel |

Volume calculation: `room.width * room.length * room.height` (in cm³, convert to m³ by /1e6).

### 3.3 Material-Aware Absorption

The existing `analyze_room_acoustics` action already computes Sabine RT60 with surface material absorption coefficients. For auto-generation:

1. **Concrete/tile rooms** (bathroom, utility, stairwell): Higher reverb, longer RT60
2. **Carpeted rooms** (bedroom, living room): Lower reverb, shorter RT60
3. **Mixed rooms** (kitchen, office): Medium reverb

If the room has been prop-scattered, re-analyze acoustics because furniture adds absorption.

### 3.4 Interior Settings for Horror

**Key settings for horror audio isolation:**

```cpp
FInteriorSettings Settings;
Settings.ExteriorVolume = 0.3f;    // Muffle outside sounds when inside
Settings.ExteriorTime = 0.5f;      // Quick transition
Settings.ExteriorLPF = 2000.0f;    // Low-pass filter (Hz) — muffled
Settings.ExteriorLPFTime = 0.3f;
Settings.InteriorVolume = 1.0f;    // Full inside volume
Settings.InteriorTime = 0.2f;
Settings.InteriorLPF = 20000.0f;   // No filter inside
Settings.InteriorLPFTime = 0.2f;
```

These settings make it so entering a room immediately muffles exterior sounds — critical for horror tension (sudden silence when entering a building).

### 3.5 Priority System

- **Building-level audio volume:** Priority 0.0 (base layer)
- **Room-level audio volume:** Priority 1.0 (overrides building)
- **Special room overrides:** Priority 2.0 (e.g., the "red room")

Higher priority volumes override lower ones where they overlap. Rooms inside buildings naturally override the building-level ambient.

---

## 4. Auto-TriggerVolumes

### 4.1 API (Verified UE 5.7)

```cpp
// TriggerVolume.h — almost no API beyond AVolume
class ATriggerVolume : public AVolume
{
    GENERATED_UCLASS_BODY()
};
```

Trigger volumes are intentionally minimal. Gameplay logic is connected via Blueprints (OnActorBeginOverlap/EndOverlap). For procedural use, the volume placement is what matters; gameplay binding happens in a subsequent Blueprint or level-scripting step.

### 4.2 Building Entrance Triggers

**Placement:** At each exterior door opening (from spatial registry `openings[]` where `connects_to` is null or exterior).

**Extent:** Door width × door depth (30cm deep) × door height.

**Naming:** `TV_Enter_{BuildingName}_{DoorIndex}`

**Use cases:**
- Change background music on building entry
- Spawn enemies inside building when player approaches
- Trigger environmental storytelling (lights flicker, sounds play)
- AI Director: player entered a new building, adjust pacing

### 4.3 Floor Transition Triggers

**Placement:** At stairwell openings between floors.

**Extent:** Stairwell width × stairwell length × 50cm tall (thin trigger plane).

**Location:** At the threshold where floor changes (Z = floor_height * floor_number).

**Naming:** `TV_Floor_{BuildingName}_F{N}to{N+1}`

### 4.4 Room Entry Triggers

**Placement:** At each interior door (spatial registry `openings[]` where `connects_to` references another room).

**Extent:** Door width × 30cm × door height.

**Naming:** `TV_Room_{BuildingName}_{Floor}_{RoomName}`

**Horror uses:**
- Scare triggers (one-shot events: sound + light change)
- Door-slam events
- Lighting state changes
- Enemy awareness (AI knows player entered room)

---

## 5. Auto-PostProcessVolumes

### 5.1 API (Verified UE 5.7)

```cpp
// PostProcessVolume.h
class APostProcessVolume : public AVolume, public IInterface_PostProcessVolume
{
    FPostProcessSettings Settings;  // Massive struct with ALL PP settings
    float Priority;
    float BlendRadius;              // Transition blend in world units
    float BlendWeight;              // 0-1 effect strength
    uint32 bEnabled:1;
    uint32 bUnbound:1;              // Infinite extent (global)
};
```

`FPostProcessSettings` is enormous (~200+ properties). Key ones for horror:

- `Settings.bOverride_ColorSaturation` + `Settings.ColorSaturation`
- `Settings.bOverride_VignetteIntensity` + `Settings.VignetteIntensity`
- `Settings.bOverride_FilmGrainIntensity` + `Settings.FilmGrainIntensity`
- `Settings.bOverride_AutoExposureBias` + `Settings.AutoExposureBias`
- `Settings.bOverride_WhiteTemp` + `Settings.WhiteTemp`
- `Settings.bOverride_BloomIntensity` + `Settings.BloomIntensity`

**Each setting requires its `bOverride_*` flag set to true** or it won't apply.

### 5.2 Building-Level Mood Presets

**One PPV per building** (covers full building bounds):

| Building Type | WhiteTemp | Saturation | Vignette | AutoExposureBias | Mood |
|--------------|-----------|------------|----------|------------------|------|
| House | 5500 (warm) | 0.95 | 0.3 | 0.0 | Homey, slightly warm |
| Clinic | 7500 (cold) | 0.7 | 0.4 | +0.5 | Clinical, sterile |
| Police Station | 6000 | 0.8 | 0.35 | +0.3 | Harsh, institutional |
| Warehouse | 4500 | 0.6 | 0.5 | -0.5 | Dark, industrial |
| Church | 5000 | 0.85 | 0.3 | -0.3 | Somber, reverent |
| Abandoned | 4000 | 0.4 | 0.6 | -1.0 | Desaturated, oppressive |

**BlendRadius:** 200cm — smooth transition when entering building.
**Priority:** 1.0 (above world default of 0).

### 5.3 Room-Level Overrides

**Optional, only for "special" rooms** to avoid PPV explosion:

| Room Type | Override | Values |
|-----------|----------|--------|
| Red Light Room | Tint | Red color grading, saturation 0.3 |
| Pitch Dark Room | Exposure | AutoExposureBias = -4.0, vignette 0.8 |
| Bathroom (horror) | Grain | FilmGrainIntensity = 0.5, saturation 0.5 |
| Safe Room | Warmth | WhiteTemp = 5000, saturation 1.0, vignette 0.1 |

**Priority:** 2.0 (above building level).
**BlendRadius:** 100cm — tighter blend for room-level transitions.

### 5.4 Horror Effects Driven by Tension Score

If `classify_zone_tension` has been run on a room:

| Tension Level | Vignette | Saturation | Grain | ChromAb |
|--------------|----------|------------|-------|---------|
| safe (0.0-0.2) | 0.0-0.2 | 1.0 | 0.0 | 0.0 |
| uneasy (0.2-0.4) | 0.2-0.3 | 0.9 | 0.05 | 0.0 |
| tense (0.4-0.6) | 0.3-0.5 | 0.7 | 0.1 | 0.1 |
| dangerous (0.6-0.8) | 0.5-0.7 | 0.5 | 0.2 | 0.3 |
| panic (0.8-1.0) | 0.7-0.9 | 0.3 | 0.4 | 0.5 |

---

## 6. Stairs & NavLinks

### 6.1 Standard Stairs — No NavLinks Needed

UE 5.7 RecastNavMesh handles standard stairs automatically:

- **Max slope:** 44 degrees (AgentMaxSlope default)
- **Max step height:** 35cm (AgentMaxStepHeight default)
- **Standard residential stairs:** ~18cm rise, ~28cm run = ~33 degree slope → auto-navigable

The navmesh rasterizer walks the stair geometry at CellHeight granularity (default 10cm) and generates walkable surfaces on each tread.

**Requirement:** The stair static mesh MUST have collision. With the auto-collision fix (convex hulls), procedural stairs from `create_structure` will have collision and thus generate proper navmesh.

### 6.2 When NavLinks ARE Needed

```cpp
// NavLinkProxy.h
class ANavLinkProxy : public AActor, public INavLinkHostInterface, public INavRelevantInterface
{
    TArray<FNavigationLink> PointLinks; // Simple point-to-point links
    UNavLinkCustomComponent* SmartLinkComp; // Dynamic enable/disable
    bool bSmartLinkIsRelevant;
};

// FNavigationLink (NavLinkDefinition.h)
struct FNavigationLink : public FNavigationLinkBase
{
    FVector Left;   // Start point (local space)
    FVector Right;  // End point (local space)
};

// FNavigationLinkBase key fields:
//   ENavLinkDirection Direction; // BothWays, LeftToRight, RightToLeft
//   float MaxFallDownLength;    // Max fall distance (default 1000)
//   float SnapRadius;           // Snap to navmesh radius (default 30)
```

**Scenarios requiring NavLinks in procedural buildings:**
1. **Multi-story buildings with open stairwells** (where geometry gap > 35cm step height)
2. **Ladders** (vertical traversal)
3. **One-way drops** (balconies, broken floors) — `Direction = LeftToRight`
4. **Jump points** (gaps in corridors from horror decay)

**Smart Links** (`bSmartLinkIsRelevant = true`) can be dynamically toggled — useful for:
- Doors that lock/unlock during gameplay
- Barricades that can be broken
- Elevators

### 6.3 Auto-NavLink Placement for Stairs

When the spatial registry has floors connected by stairs, place NavLinks at:
- **Bottom of stairwell:** Left = floor N landing center
- **Top of stairwell:** Right = floor N+1 landing center
- **Direction:** BothWays

Only needed if standard navmesh generation fails to connect floors (test by building navmesh first, then checking connectivity).

---

## 7. Performance Budget

### 7.1 Volume Count Analysis

For a typical city block (4 buildings, 2-3 floors each, ~6 rooms per floor):

| Volume Type | Count | Per Building | Notes |
|-------------|-------|-------------|-------|
| NavMeshBoundsVolume | 1 | — | One per block |
| BlockingVolume (rooms) | ~50 | ~12 | One per room |
| AudioVolume (rooms) | ~50 | ~12 | One per room |
| TriggerVolume (doors) | ~30 | ~8 | One per door |
| PostProcessVolume (buildings) | 4 | 1 | One per building |
| PostProcessVolume (special rooms) | ~5 | ~1 | Only horror rooms |
| NavModifierVolume | ~10 | ~2-3 | Stairs, narrow areas |
| **Total** | **~150** | **~37** | |

### 7.2 Performance Impact

**Brush volumes are lightweight.** Each volume is:
- An `ABrush` actor with a `UBrushComponent` (convex hull geometry)
- Registered in the world's actor array
- BSP-based collision (simple AABB for box volumes)

**Runtime cost per volume type:**
- **BlockingVolume:** Zero tick cost. Collision only checked on overlap queries.
- **AudioVolume:** Checked per-frame for listener position (binary search through sorted array, `O(log n)` per active audio source). 50 volumes is trivial.
- **TriggerVolume:** Zero tick cost. Only fires on overlap begin/end events.
- **PostProcessVolume:** Checked per-frame for camera position. Cost is in the PP stack blending, not volume count. 10 volumes = negligible.
- **NavModifierVolume:** Only used during navmesh build (not runtime cost). Zero tick cost.
- **NavMeshBoundsVolume:** Only used during navmesh build. Zero runtime cost.

**150 volumes = completely fine.** UE regularly handles 500+ volumes in production levels. The bottleneck would be navmesh build time, not volume count.

### 7.3 Navmesh Build Time

RecastNavMesh build time scales with navigable area, not volume count:
- Small building (~500m²): 1-3 seconds
- Medium block (~2000m²): 3-8 seconds
- Large block (~5000m²): 8-20 seconds

`build_navmesh` already documents this as synchronous/blocking. For very large blocks, consider:
- Dirty-only rebuilds after incremental changes
- Building navmesh once after ALL volumes are placed (not per-room)

---

## 8. Pipeline Ordering

Critical ordering for the full auto-generation pipeline:

```
Phase 1: Geometry Generation
  ├── create_structure / create_building_shell (walls, floors, ceilings)
  ├── create_staircase (floor connections)
  └── create_furniture / scatter_props (optional, can be deferred)

Phase 2: Collision Generation
  └── save_handle with collision: "auto" (applies to ALL meshes)
      MUST complete before Phase 3

Phase 3: Volume Placement (order within phase doesn't matter)
  ├── spawn NavMeshBoundsVolume (one per block)
  ├── spawn BlockingVolumes per room
  │   └── setup_blockout_volume per room (tags)
  ├── spawn NavModifierVolumes (stairs, corridors)
  ├── spawn TriggerVolumes (doors, floor transitions)
  ├── spawn AudioVolumes per room
  └── spawn PostProcessVolumes per building + special rooms

Phase 4: NavMesh Build
  └── build_navmesh (MUST be after all geometry + volumes)

Phase 5: Audio Analysis (optional, post-navmesh)
  ├── analyze_room_acoustics per room
  └── Update AudioVolume reverb settings based on analysis

Phase 6: Horror Intelligence (optional)
  ├── classify_zone_tension per room
  └── Update PostProcessVolume settings based on tension
```

---

## 9. New Actions

### 9.1 `auto_volumes_for_building` (Composite Action)

**Parameters:**
```json
{
    "building_name": "string (required) — building ID in spatial registry",
    "block_descriptor": "string (optional) — path to block descriptor JSON, or inline object",
    "options": {
        "blocking_volumes": true,
        "audio_volumes": true,
        "trigger_volumes": true,
        "post_process": true,
        "nav_modifier": true,
        "navmesh_bounds": true,
        "auto_reverb": true,
        "building_mood": "house|clinic|police|warehouse|church|abandoned|custom",
        "horror_tension": true,
        "folder": "Volumes/{BuildingName}"
    }
}
```

**Returns:** Summary of all spawned volumes with names/locations.

This is the main orchestration action. Internally it:
1. Reads building from spatial registry (rooms, floors, openings)
2. Spawns all configured volume types
3. Tags blocking volumes
4. Configures audio presets
5. Sets PP mood

### 9.2 `auto_volumes_for_block` (Block-Level)

Calls `auto_volumes_for_building` for each building in a block, plus:
- One `NavMeshBoundsVolume` covering the entire block
- One `build_navmesh` call at the end
- Street-level trigger volumes at block entry points

### 9.3 `spawn_nav_link` (New Volume Action)

**Parameters:**
```json
{
    "start": [x, y, z],
    "end": [x, y, z],
    "direction": "both|start_to_end|end_to_start",
    "smart": false,
    "name": "string (optional)",
    "folder": "string (optional)"
}
```

Spawns an `ANavLinkProxy` with configured `PointLinks[0]`.

### 9.4 Extension to `spawn_volume`

Add `nav_mesh_bounds` to the existing `ResolveVolumeClass()`:
```cpp
if (TypeStr.Equals(TEXT("nav_mesh_bounds"), ESearchCase::IgnoreCase))
    return ANavMeshBoundsVolume::StaticClass();
```

This is a one-line fix that unlocks NavMeshBoundsVolume spawning via the existing action.

---

## 10. Implementation Plan

### Phase 1: Foundation (8-12h)
- [ ] Add `nav_mesh_bounds` to `spawn_volume` ResolveVolumeClass
- [ ] Add `spawn_nav_link` action for NavLinkProxy creation
- [ ] Add NavModifierVolume `area_class` property support to `spawn_volume`
- [ ] Create custom NavArea subclasses: `UNavArea_Stairs`, `UNavArea_SlowZone`
- [ ] Add PostProcessSettings property shortcuts to `spawn_volume` (vignette, saturation, white_temp, grain, exposure_bias)

### Phase 2: Room Volume Generator (12-16h)
- [ ] `auto_volumes_for_building` action
  - Read spatial registry room data
  - Spawn BlockingVolumes per room with Monolith tags
  - Spawn AudioVolumes per room with reverb presets
  - Spawn TriggerVolumes at doorways
  - Spawn PostProcessVolume per building with mood
  - Spawn NavModifierVolumes for stairs/corridors
- [ ] Room size → reverb preset mapping (the table from section 3.2)
- [ ] Building type → PP mood mapping (the table from section 5.2)
- [ ] Naming convention enforcement

### Phase 3: Block Orchestrator (6-8h)
- [ ] `auto_volumes_for_block` action
  - Iterate buildings in block descriptor
  - Call Phase 2 per building
  - Spawn one NavMeshBoundsVolume covering block + padding
  - Call `build_navmesh` once at end
  - Optional: run `analyze_room_acoustics` + refine reverb
- [ ] Integration with `create_city_block` pipeline (if/when implemented)

### Phase 4: Horror Enhancement (6-8h)
- [ ] Tension-driven PP settings (section 5.4)
- [ ] Smart NavLinks for dynamically lockable doors
- [ ] One-way NavLinks for horror drops/jumps
- [ ] Special room PP overrides (red room, dark room, safe room)
- [ ] Integration with AI Director (future)

### Phase 5: Testing & Polish (4-6h)
- [ ] Test with multi-building blocks
- [ ] Verify navmesh connectivity across floors
- [ ] Audio volume overlap/priority testing
- [ ] PP blending verification
- [ ] Performance profiling with 200+ volumes
- [ ] Update SPEC.md, TODO.md, TESTING.md

---

## 11. Effort Estimate

| Phase | Hours | Dependencies |
|-------|-------|-------------|
| Phase 1: Foundation | 8-12h | Auto-collision fix |
| Phase 2: Room Volumes | 12-16h | Spatial registry (P0) |
| Phase 3: Block Orchestrator | 6-8h | Phase 2, create_city_block |
| Phase 4: Horror Enhancement | 6-8h | Phase 2, classify_zone_tension |
| Phase 5: Testing | 4-6h | All phases |
| **Total** | **36-50h** | |

**Critical dependency:** Spatial registry must exist (at least the JSON schema and room data) before Phase 2. The auto-collision fix must land before navmesh generation works on proc-gen meshes.

**Recommended priority:** Phase 1 can ship independently and immediately benefits any volume-spawning workflow. Phase 2 is the high-value target. Phase 3-4 are luxury that multiplies the value of the city block generator.

---

## Key API Signatures (Verified UE 5.7)

```cpp
// Volume spawning
UActorFactory::CreateBrushForVolumeActor(AVolume* NewActor, UBrushBuilder* BrushBuilder);
UCubeBuilder::X/Y/Z  // FULL dimensions (not half-extents)

// NavMesh
UNavigationSystemV1::Build();  // Synchronous, blocks game thread
ANavModifierVolume::SetAreaClass(TSubclassOf<UNavArea>);

// NavLinks
ANavLinkProxy::PointLinks  // TArray<FNavigationLink>
FNavigationLink(const FVector& InLeft, const FVector& InRight);
FNavigationLinkBase::Direction  // ENavLinkDirection::BothWays

// Audio
AAudioVolume::SetReverbSettings(const FReverbSettings&);
AAudioVolume::SetInteriorSettings(const FInteriorSettings&);
AAudioVolume::SetPriority(float);
AAudioVolume::SetEnabled(bool);

// Post-Process
APostProcessVolume::Settings  // FPostProcessSettings — set bOverride_* + value
APostProcessVolume::Priority, BlendRadius, BlendWeight, bEnabled, bUnbound
```

---

## Gaps & Open Questions

1. **Spatial registry dependency:** This entire system assumes the spatial registry exists. If rooms aren't registered, we'd need a fallback (e.g., derive rooms from BlockingVolumes already in the scene).

2. **UReverbEffect assets:** The existing `create_audio_volume` only sets `bApplyReverb = true` without linking an actual `UReverbEffect` asset. For proper reverb, we either need pre-made reverb presets in Content/, or we create them programmatically via `NewObject<UReverbEffect>()` with Sabine-derived parameters.

3. **PostProcessSettings bulk property setting:** Setting 6+ override flags + values per PPV is verbose via FProperty reflection. A helper function that takes a "mood preset" string and applies all relevant settings would be cleaner than exposing every individual PP property to MCP.

4. **NavLink testing:** It's unclear if NavLinks placed programmatically auto-register with the nav system the same way editor-placed ones do. `PostRegisterAllComponents()` should handle it, but needs verification.

5. **Audio volume scaling vs brush:** Current `create_audio_volume` scales the actor (`SetActorScale3D(Extent / 100.0f)`) instead of rebuilding the brush. This works but means the brush geometry doesn't match the visual extent in the editor. Consider using the `UCubeBuilder` approach (like `spawn_volume`) for consistency.
