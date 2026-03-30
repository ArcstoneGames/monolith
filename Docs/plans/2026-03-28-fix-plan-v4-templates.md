# Fix Plan v4: Template System + Bug Fixes + Orchestrator Wiring

**Date:** 2026-03-28
**Status:** APPROVED — Both reviewers approved. Fixes applied. Executing.
**Priority:** P0 (Critical Path)
**Scope:** 4 work packages, 3 phases, 6 files modified, 47 template JSONs created

---

## Reviewer Fixes Applied

### From Review #1 — Technical Corrections
- **R1-C1: Template JSON → FRoomDef mapping layer.** WP-A `LoadTemplate()` must explicitly reconstruct `FRoomDef::GridCells` by scanning the grid array for matching room indices, NOT rely on a `grid_cells` field in the JSON (which would be redundant and error-prone). The grid array is the source of truth. Doors must be validated: `EdgeStart.X == EdgeEnd.X || EdgeStart.Y == EdgeEnd.Y` assertion on load.
- **R1-C2: Door boundary math documentation.** `Max(DoorCell.X, NeighborCell.X)` works because `create_building_from_grid` treats grid coordinates as left/top edges — the wall boundary IS at the max coordinate. Added comments to WP-B pseudocode explaining this.
- **R1-I3: `use_templates` feature flag.** Added `use_templates` boolean param (default true) to `generate_floor_plan`. When false, skips template lookup and uses algorithmic treemap. Provides safety valve. Added to WP-A scope.
- **R1-I1: Template metadata cache.** WP-A should load only template metadata (name, category, footprint, floor_count) at startup, full grid on demand. 47 JSONs × ~50KB = 2.3MB — fine to cache headers but don't hold all grids in memory.

### From Review #2 — Player/Design Corrections
- **R2-I1: Post-scaling corridor width validation.** After `ScaleTemplate()`, verify all corridor rooms are still >= 3 cells wide. If scaling shrunk them, expand back to minimum. Added to WP-A.
- **R2-I2: Post-scaling stairwell minimum.** After scaling, verify stairwell rooms are still >= 4x6 cells. If not, reject scaling and use template at original size. Added to WP-A.
- **R2-I3: Post-scaling entrance validation.** After scaling, verify entrance cell is still on exterior edge. Added to WP-A.
- **R2-I4: Hospice door width clamp on template path.** When `hospice_mode=true`, clamp all template doors to >= 120cm width regardless of template definition. Added to WP-A template loading.
- **R2-I5: Horror template injection.** When `genre=="horror"` in the orchestrator, set `template_category="horror"` on FloorPlanParams so the template selector picks from horror templates, not just residential archetypes. Added to WP-C.
- **R2-I6: Missing warehouse template.** WP-D must include a warehouse template (the commercial variety doc has one — ensure it's in the JSON set).
- **R2-S6: Cell ID convention normalization.** ALL templates use: -1 = empty/exterior, -2 = stairwell, 0+ = room indices. WP-D must normalize any docs that use different conventions.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Architecture Overview](#2-architecture-overview)
3. [File Conflict Matrix](#3-file-conflict-matrix)
4. [WP-A: Template System (Critical Path)](#4-wp-a-template-system)
5. [WP-B: Door Edge Coordinate Fix (P0)](#5-wp-b-door-edge-coordinate-fix)
6. [WP-C: Orchestrator Wiring Fixes](#6-wp-c-orchestrator-wiring-fixes)
7. [WP-D: Template JSON Creation](#7-wp-d-template-json-creation)
8. [Phase Schedule](#8-phase-schedule)
9. [Test Criteria](#9-test-criteria)
10. [Risks and Mitigations](#10-risks-and-mitigations)

---

## 1. Executive Summary

The procedural town generator (`create_city_block`) has 10 known issues discovered by 6 audit agents, plus the fundamental problem that algorithmically generated floor plans produce unrealistic room layouts. This plan:

1. Introduces a **template-based floor plan system** using 47 hand-curated templates from 6 research documents, replacing the treemap algorithm as the PRIMARY layout path
2. Fixes the **P0 door edge coordinate bug** that makes every auto-generated door cutter perpendicular to its correct orientation
3. Wires **8 disconnected orchestrator features** (horror_level, decay metadata, retry regeneration, street furniture, furnishing, auto volumes, genre pools, multi-floor)
4. Creates **47 template JSON files** organized by category

### Bug Inventory

| # | Priority | Bug | Root Cause | Fix Location |
|---|----------|-----|------------|-------------|
| 1 | P0 | Door cutters 90 deg wrong | `PlaceDoors` sets EdgeStart/EdgeEnd to room interior cells, not wall boundary coords | `MonolithMeshFloorPlanGenerator.cpp` |
| 2 | P0 | Missing archetypes | Genre pools reference `commercial_shop`, `office_small`, `garage` -- none exist | `MonolithMeshCityBlockActions.cpp` |
| 3 | P1 | horror_level never passed | `FloorPlanParams` never gets `horror_level` field | `MonolithMeshCityBlockActions.cpp` |
| 4 | P1 | Decay metadata discarded | `ApplyBuildingDecay` writes to ephemeral `BuildingMeta` object, never applied to building | `MonolithMeshCityBlockActions.cpp` |
| 5 | P1 | Retry doesn't regenerate floor plan | Retry loop calls `create_building_from_grid` with same grid data, only changes seed | `MonolithMeshCityBlockActions.cpp` |
| 6 | P1 | Street furniture only on south street | Hardcoded `BlockOrigin.Y` for both start/end Y coords | `MonolithMeshCityBlockActions.cpp` |
| 7 | P2 | furnish_building not wired | Action exists (`MonolithMeshFurnishingActions.cpp`) but orchestrator never calls it | `MonolithMeshCityBlockActions.cpp` |
| 8 | P2 | auto_volumes not wired | Action exists (`MonolithMeshAutoVolumeActions.cpp`) but orchestrator never calls it | `MonolithMeshCityBlockActions.cpp` |
| 9 | P2 | Only single-floor buildings | Orchestrator calls `generate_floor_plan` once, no per-floor loop | `MonolithMeshCityBlockActions.cpp` + `MonolithMeshFloorPlanGenerator.cpp` |
| 10 | P2 | Entrance wall behind door | Treemap crushes entryway room (mitigated by templates) | Template system |

---

## 2. Architecture Overview

### Template System Data Flow

```
create_city_block (orchestrator)
  |
  v
generate_floor_plan(archetype, footprint, template?, seed)
  |
  +-- IF template param provided OR matching template found:
  |     load_template(name) -> scale to footprint -> rooms/doors/stairwells
  |
  +-- ELSE (fallback):
  |     treemap algorithm -> corridors -> doors (existing code)
  |
  v
create_building_from_grid(grid, rooms, doors, stairwells)
```

### Template JSON Schema

```json
{
  "name": "small_ranch",
  "category": "residential",
  "tags": ["ranch", "1950s", "single_story"],
  "description": "1950s American ranch house, 2BR/1BA",
  "num_floors": 1,
  "floor_height_cm": 270,
  "footprint_m": { "width": 12.5, "depth": 8.0 },
  "min_footprint_m": { "width": 10.0, "depth": 6.5 },
  "max_footprint_m": { "width": 15.0, "depth": 10.0 },
  "roof_type": "gable",
  "circulation": "hub_spoke",
  "floors": [
    {
      "floor_index": 0,
      "grid_width": 25,
      "grid_height": 16,
      "grid": [[0,0,0,...], ...],
      "rooms": [
        {
          "room_id": "living_room",
          "room_type": "living_room",
          "room_index": 0,
          "area_cells": 120,
          "exterior_wall": true
        }
      ],
      "doors": [
        {
          "door_id": "door_01",
          "room_a": "living_room",
          "room_b": "kitchen",
          "edge_start": [10, 0],
          "edge_end": [10, 2],
          "wall": "east",
          "width_cm": 110
        }
      ],
      "entrance": {
        "room": "living_room",
        "wall": "south",
        "position": [5, 0]
      },
      "stairwells": []
    }
  ],
  "horror_notes": "Long hallway to bedrooms creates tension. Bathroom has single exit.",
  "hospice_notes": "All doors 120cm. No narrow passages."
}
```

### Template Selection Algorithm

```
1. Filter templates by building_type matching archetype category
   (residential_house -> residential, commercial_shop -> commercial, etc.)
2. Filter by footprint compatibility:
   requested_width in [template.min_footprint.width, template.max_footprint.width]
   requested_depth in [template.min_footprint.depth, template.max_footprint.depth]
3. If num_floors specified, filter by num_floors match
4. Score remaining by footprint_area_ratio (closer to 1.0 = better)
5. Pick random from top 3 candidates (weighted by score) using Rng
6. Scale template grid to match requested footprint dimensions
7. If no template matches, fall back to treemap algorithm
```

### Grid Scaling Algorithm

```
Given: template grid (TW x TH cells) designed for (T_WidthM x T_DepthM)
       requested footprint (R_WidthM x R_DepthM)

1. Compute scale factors:
   ScaleX = R_WidthM / T_WidthM
   ScaleY = R_DepthM / T_DepthM

2. Compute new grid dimensions:
   NewGridW = Round(TW * ScaleX)  // Clamped to [TW * 0.7, TW * 1.5]
   NewGridH = Round(TH * ScaleY)

3. If scale is uniform (within 10%):
   Use nearest-neighbor scaling -- each template cell maps to ceil(ScaleX) x ceil(ScaleY) cells
   Room IDs preserved. Door positions scaled proportionally.

4. If scale is non-uniform (>10% difference):
   Scale each axis independently. Room boundaries snap to integer cell positions.
   Rooms that would become too small (<4 cells) are absorbed by adjacent rooms.

5. Recompute door edge coordinates on scaled grid boundaries.
6. Recompute stairwell cell positions (must maintain minimum 4x6 footprint).
```

---

## 3. File Conflict Matrix

Files each WP touches (determines parallelism):

| File | WP-A | WP-B | WP-C | WP-D |
|------|------|------|------|------|
| `MonolithMeshFloorPlanGenerator.h` | WRITE | - | - | - |
| `MonolithMeshFloorPlanGenerator.cpp` | WRITE | WRITE | - | - |
| `MonolithMeshCityBlockActions.cpp` | - | - | WRITE | - |
| `MonolithMeshCityBlockActions.h` | - | - | READ | - |
| Template JSON files (47) | - | - | - | WRITE |
| `MonolithMeshBuildingActions.cpp` | READ | READ | - | - |

**Conflict analysis:**
- **WP-A and WP-B conflict** on `MonolithMeshFloorPlanGenerator.cpp` -- must be sequenced (Phase 1 then Phase 2, or same agent)
- **WP-C is independent** of WP-A and WP-B (different file)
- **WP-D is fully independent** (only creates new JSON files)
- WP-A and WP-C can run in parallel
- WP-B must wait for WP-A (or be done by same agent in one pass)

---

## 4. WP-A: Template System

**Agent:** `unreal-mesh-expert` (primary) or `cpp-performance-expert`
**Files:** `MonolithMeshFloorPlanGenerator.h`, `MonolithMeshFloorPlanGenerator.cpp`
**Estimated size:** ~400-500 lines C++

### 4.1 New Functions to Add

**In `MonolithMeshFloorPlanGenerator.h`:**

```cpp
// ---- Template system ----

/** A loaded floor plan template */
struct FFloorPlanTemplate
{
    FString Name;
    FString Category;        // "residential", "commercial", "horror", "institutional"
    TArray<FString> Tags;
    FString Description;
    int32 NumFloors = 1;
    float FloorHeightCm = 270.0f;
    FVector2D FootprintM;    // Design footprint in meters
    FVector2D MinFootprintM; // Minimum scalable footprint
    FVector2D MaxFootprintM; // Maximum scalable footprint
    FString RoofType;
    FString Circulation;

    // Per-floor data
    struct FFloorData
    {
        int32 FloorIndex = 0;
        int32 GridWidth = 0;
        int32 GridHeight = 0;
        TArray<TArray<int32>> Grid;
        TArray<FRoomDef> Rooms;
        TArray<FDoorDef> Doors;
        FIntPoint EntrancePos = FIntPoint(-1, -1);
        FString EntranceWall;
        FString EntranceRoom;
        TArray<FStairwellDef> Stairwells;
    };
    TArray<FFloorData> Floors;

    FString HorrorNotes;
    FString HospiceNotes;
};

/** Get the template directory path */
static FString GetTemplateDirectory();

/** Load a template from a JSON file */
static bool LoadTemplate(const FString& TemplateName, FFloorPlanTemplate& OutTemplate, FString& OutError);

/** Find the best matching template for given parameters */
static FString FindBestTemplate(const FString& BuildingType, float FootprintWidthM, float FootprintDepthM,
    int32 NumFloors, FRandomStream& Rng);

/** Scale a template to fit the requested footprint dimensions */
static bool ScaleTemplate(const FFloorPlanTemplate& Template, float RequestedWidthM, float RequestedDepthM,
    TArray<TArray<int32>>& OutGrid, int32& OutGridW, int32& OutGridH,
    TArray<FRoomDef>& OutRooms, TArray<FDoorDef>& OutDoors, TArray<FStairwellDef>& OutStairwells,
    int32 FloorIndex, FString& OutError);

/** List all available templates (for the list_building_templates action) */
// (exposed via new action or added to list_building_archetypes)
```

### 4.2 Changes to `GenerateFloorPlan`

The existing `GenerateFloorPlan` function (line ~2780 of cpp) currently:
1. Parses archetype name
2. Loads archetype JSON
3. Resolves room instances
4. Runs squarified treemap
5. Inserts corridors
6. Places doors
7. Ensures exterior entrance
8. Applies horror modifiers
9. Builds output JSON

**New flow (insert at step 2.5):**

```
After loading archetype:
  IF params has "template" field:
    Load named template directly
  ELSE:
    Map archetype name to template category
    Call FindBestTemplate(category, footprintW, footprintH, numFloors, Rng)
    If template found, use it

  IF template available:
    For each floor in template:
      ScaleTemplate() to fit requested footprint
      -> produces grid, rooms, doors, stairwells
    Skip steps 3-6 (treemap, corridors, door placement)
    Jump to step 7 (ensure exterior entrance -- may be no-op if template has entrance)
    Continue with step 8 (horror modifiers)
    Continue with step 9 (output JSON)

  ELSE:
    Fall through to existing treemap path (steps 3-9 unchanged)
```

### 4.3 Template Directory Structure

```
Saved/Monolith/FloorPlanTemplates/
  residential/
    small_ranch.json
    medium_colonial.json
    l_shaped_ranch.json
    cape_cod.json
    split_level.json
    small_bungalow.json
    shotgun_house.json
    tiny_cabin.json
    studio_apartment.json
    1950s_ranch.json
    two_story_colonial.json
    modern_open_plan.json
    townhouse.json
    farmhouse.json
    duplex_unit.json
    trailer_mobile_home.json
  commercial/
    small_office.json
    retail_store.json
    restaurant.json
    bank.json
    medical_clinic.json
    auto_repair.json
    corner_store.json
    gas_station.json
    diner.json
    bar_pub.json
    laundromat.json
    fire_station.json
    small_library.json
    post_office.json
    pharmacy.json
    auto_repair_shop.json
  horror/
    abandoned_hospital.json
    victorian_mansion.json
    apartment_hallway.json
    basement_cellar.json
    abandoned_school.json
    church.json
    motel.json
    underground_bunker.json
  multistory/
    3story_apartment.json
    2story_office.json
    2story_colonial.json
    3story_hotel.json
    2story_school.json
    2story_police_station.json
```

### 4.4 Archetype-to-Template Category Mapping

```cpp
static FString MapArchetypeToTemplateCategory(const FString& ArchetypeName)
{
    if (ArchetypeName.Contains("residential") || ArchetypeName.Contains("house") || ArchetypeName.Contains("small_house"))
        return "residential";
    if (ArchetypeName.Contains("commercial") || ArchetypeName.Contains("shop") || ArchetypeName.Contains("store") || ArchetypeName.Contains("office"))
        return "commercial";
    if (ArchetypeName.Contains("apartment") || ArchetypeName.Contains("hotel"))
        return "multistory";
    if (ArchetypeName.Contains("church") || ArchetypeName.Contains("school") || ArchetypeName.Contains("clinic") || ArchetypeName.Contains("police"))
        return "commercial";  // institutional uses commercial templates
    if (ArchetypeName.Contains("warehouse"))
        return "commercial";
    return "residential";  // default
}
```

### 4.5 Done Criteria

- [ ] `FFloorPlanTemplate` struct defined in header
- [ ] `GetTemplateDirectory()`, `LoadTemplate()`, `FindBestTemplate()`, `ScaleTemplate()` implemented
- [ ] `GenerateFloorPlan` tries template path first, falls back to treemap
- [ ] New optional param `"template"` on `generate_floor_plan` action for explicit template selection
- [ ] Multi-floor templates produce per-floor grid data with stairwell alignment
- [ ] Compiles with zero errors/warnings
- [ ] Manual test: `generate_floor_plan` with `archetype=residential_house` uses a template when available

---

## 5. WP-B: Door Edge Coordinate Fix (P0)

**Agent:** `unreal-mesh-expert` (same agent as WP-A, since same file)
**Files:** `MonolithMeshFloorPlanGenerator.cpp`
**Estimated size:** ~50-80 lines changed

### 5.1 Root Cause

`FindSharedEdge()` (line 1994) returns cells belonging to RoomA that are 4-directionally adjacent to RoomB cells. `PlaceDoors()` (line 2022) then sets:
- `Door.EdgeStart = DoorCell` (a cell from Room A's interior)
- `Door.EdgeEnd = NeighborCell` (a cell from Room B's interior)

This means EdgeStart and EdgeEnd span ACROSS the wall boundary (one cell in each room). But `create_building_from_grid` (line 683-697 of BuildingActions.cpp) expects both EdgeStart and EdgeEnd to lie ON the wall boundary line, defining a span ALONG the wall.

**Example of the bug:**
```
Room A (index 0) occupies cells [0,0] to [4,7]
Room B (index 1) occupies cells [5,0] to [9,7]
Wall boundary is at X=5 (between cells X=4 and X=5)

Current (WRONG):
  EdgeStart = [4, 3] (Room A cell)
  EdgeEnd   = [5, 3] (Room B cell)
  -> create_building_from_grid sees X differs, can't determine wall axis

Correct:
  EdgeStart = [5, 3] (wall boundary X-coord)
  EdgeEnd   = [5, 4] (wall boundary X-coord, +1 cell for door width)
  -> Both on X=5, door spans Y=3 to Y=4 on the vertical wall
```

### 5.2 Fix Algorithm

In `PlaceDoors()`, after finding `DoorCell` (from Room A) and `NeighborCell` (from Room B):

```cpp
// Compute wall boundary coordinate
// The wall sits between DoorCell and NeighborCell
FIntPoint WallPoint;
if (D.X != 0) // East/West wall
{
    // Vertical wall: boundary is at max(DoorCell.X, NeighborCell.X)
    WallPoint.X = FMath::Max(DoorCell.X, NeighborCell.X);
    WallPoint.Y = DoorCell.Y;

    // Door spans along Y axis. Find contiguous shared-edge cells on same X boundary.
    // EdgeStart and EdgeEnd both have X = WallPoint.X
    int32 DoorWidthCells = FMath::Max(2, FMath::RoundToInt32(DoorWidth / CellSize));
    int32 HalfWidth = DoorWidthCells / 2;

    Door.EdgeStart = FIntPoint(WallPoint.X, FMath::Max(0, WallPoint.Y - HalfWidth));
    Door.EdgeEnd   = FIntPoint(WallPoint.X, FMath::Min(GridH - 1, WallPoint.Y + HalfWidth));
}
else // North/South wall
{
    // Horizontal wall: boundary is at max(DoorCell.Y, NeighborCell.Y)
    WallPoint.X = DoorCell.X;
    WallPoint.Y = FMath::Max(DoorCell.Y, NeighborCell.Y);

    int32 DoorWidthCells = FMath::Max(2, FMath::RoundToInt32(DoorWidth / CellSize));
    int32 HalfWidth = DoorWidthCells / 2;

    Door.EdgeStart = FIntPoint(FMath::Max(0, WallPoint.X - HalfWidth), WallPoint.Y);
    Door.EdgeEnd   = FIntPoint(FMath::Min(GridW - 1, WallPoint.X + HalfWidth), WallPoint.Y);
}
```

**Apply the same fix to:**
1. The adjacency rule loop (lines 2046-2101) -- primary door placement
2. The corridor connectivity loop (lines 2114-2170) -- corridor-to-room doors
3. `EnsureExteriorEntrance()` (line 2226) -- exterior entrance door. Currently sets `EdgeStart = EdgeEnd = BestCell` (line 2451-2452), which is a room interior cell. Must convert to wall boundary coordinates on the building perimeter.

### 5.3 EnsureExteriorEntrance Fix

For exterior entrances, the wall boundary IS the grid boundary:
```cpp
// For south wall (Y==0), boundary is at Y=0
// For north wall (Y==GridH-1), boundary is at Y=GridH (or GridH-1, depending on convention)
// For west wall (X==0), boundary is at X=0
// For east wall (X==GridW-1), boundary is at X=GridW

if (BestWall == TEXT("south"))
{
    int32 HalfWidth = DoorWidthCells / 2;
    EntDoor.EdgeStart = FIntPoint(FMath::Max(0, BestCell.X - HalfWidth), 0);
    EntDoor.EdgeEnd = FIntPoint(FMath::Min(GridW - 1, BestCell.X + HalfWidth), 0);
}
// ... similar for other walls
```

### 5.4 CellSize Variable

`PlaceDoors` currently uses `float DoorWidth = bHospiceMode ? 120.0f : 110.0f` but doesn't have CellSize. The cell size is 50cm (hardcoded throughout the generator). Either:
- Add `float CellSize = 50.0f;` local variable, or
- Pass it as a parameter, or
- Use the constant that already exists elsewhere in the file

### 5.5 Done Criteria

- [ ] `PlaceDoors` adjacency loop emits EdgeStart/EdgeEnd on the wall boundary line (same X or same Y)
- [ ] `PlaceDoors` corridor loop does the same
- [ ] `EnsureExteriorEntrance` emits boundary coordinates on building perimeter
- [ ] Door width in cells computed from `DoorWidth / CellSize`
- [ ] All generated doors pass: `EdgeStart.X == EdgeEnd.X || EdgeStart.Y == EdgeEnd.Y` (assertion-level check)
- [ ] Manual test: generate a floor plan, verify doors in `create_building_from_grid` cut correctly oriented openings

---

## 6. WP-C: Orchestrator Wiring Fixes

**Agent:** `unreal-mesh-expert` or `cpp-performance-expert`
**File:** `MonolithMeshCityBlockActions.cpp` (primary), `MonolithMeshCityBlockActions.h` (minor)
**Estimated size:** ~200-300 lines changed/added

### 6.1 Fix #2: Genre Pool Archetypes (P0)

**Location:** `GetGenreArchetypes()` (line 237)

**Problem:** Pools reference `commercial_shop`, `office_small`, `garage` which don't exist as archetype JSON files.

**Existing archetypes:**
```
residential_house, small_house, office_building, apartment, clinic,
police_station, warehouse, church, restaurant, school
```

**Fix:** Replace non-existent archetype names with existing ones. With the template system, the archetype provides room definitions and the template provides layout, so this mapping becomes less critical -- but it still needs to load a valid archetype.

```cpp
// suburban: mostly residential
TArray<FString> ArchPool = { "residential_house", "residential_house", "small_house", "residential_house" };

// downtown: commercial + mixed
TArray<FString> ArchPool = { "restaurant", "office_building", "apartment", "warehouse" };

// horror: mix
TArray<FString> ArchPool = { "residential_house", "residential_house", "restaurant", "small_house" };
```

### 6.2 Fix #3: Pass horror_level to floor plan (P1)

**Location:** Building loop, after FloorPlanParams setup (around line 1337)

**Add:**
```cpp
if (Decay > 0.0f)
{
    FloorPlanParams->SetNumberField(TEXT("horror_level"), Decay);
}
```

The `generate_floor_plan` action already accepts and processes `horror_level` (WP-6 horror modifiers). It's just never passed by the orchestrator.

### 6.3 Fix #4: Apply Decay Metadata to Building (P1)

**Location:** After `ApplyBuildingDecay` call (line 1313)

**Problem:** `BuildingMeta` is created, `ApplyBuildingDecay` writes tilt/boarded_windows into it, but then it's only checked for the "destroyed" skip condition. The tilt and boarded_windows data is never forwarded to the building.

**Fix:** After the destroy check, transfer decay metadata to building params:
```cpp
// After the skip-destroyed check (line 1325):
// Apply decay visual metadata to building params
if (BuildingMeta->HasField(TEXT("tilt_degrees")))
{
    BuildingGridParams->SetNumberField(TEXT("tilt_degrees"),
        BuildingMeta->GetNumberField(TEXT("tilt_degrees")));
}
if (BuildingMeta->HasField(TEXT("boarded_windows")))
{
    BuildingGridParams->SetBoolField(TEXT("boarded_windows"),
        BuildingMeta->GetBoolField(TEXT("boarded_windows")));
}
if (BuildingMeta->HasField(TEXT("decay_level")))
{
    BuildingGridParams->SetNumberField(TEXT("decay_level"),
        BuildingMeta->GetNumberField(TEXT("decay_level")));
}
```

Note: This requires the decay metadata to be applied to `BuildingGridParams` AFTER it's constructed (line 1354 or 1371). So the decay metadata transfer must happen after the floor plan branch, not before. Restructure:
1. Call `ApplyBuildingDecay` (already done at line 1313)
2. Check destroy condition (already done)
3. Generate floor plan (already done)
4. Build `BuildingGridParams` (already done)
5. **NEW: transfer decay fields from BuildingMeta to BuildingGridParams**

### 6.4 Fix #5: Retry Must Regenerate Floor Plan (P1)

**Location:** Retry loop (line 1648)

**Problem:** Retry only calls `create_building_from_grid` with the same `BuildingGridParams` (which still has the original floor plan grid). Changing the seed on `BuildingGridParams` only affects the building geometry seed, not the layout.

**Fix:** In the retry loop, regenerate the floor plan first:
```cpp
for (int32 Retry = 1; Retry <= MaxRetries; ++Retry)
{
    const int32 RetrySeed = Seed + i + Retry * 100;

    // Regenerate floor plan with new seed
    FloorPlanParams->SetNumberField(TEXT("seed"), RetrySeed);
    TSharedPtr<FJsonObject> RetryFloorPlan;
    FString RetryFPError;
    bool bRetryFP = TryExecuteAction(TEXT("generate_floor_plan"), FloorPlanParams, RetryFloorPlan, RetryFPError);

    if (bRetryFP && RetryFloorPlan.IsValid())
    {
        BuildingGridParams = RetryFloorPlan;
        // Re-apply building metadata (save_path, building_id, location, folder, etc.)
        BuildingGridParams->SetStringField(TEXT("save_path"), BuildingAssetPath);
        BuildingGridParams->SetStringField(TEXT("building_id"), BuildingId);
        BuildingGridParams->SetArrayField(TEXT("location"), ...);  // preserve original location
        BuildingGridParams->SetStringField(TEXT("folder"), Folder + TEXT("/Buildings"));
        BuildingGridParams->SetBoolField(TEXT("overwrite"), true);
        if (!bSkipFacades && !BlockFacadeStyle.IsEmpty())
        {
            BuildingGridParams->SetStringField(TEXT("facade_style"), BlockFacadeStyle);
            BuildingGridParams->SetNumberField(TEXT("facade_seed"), RetrySeed + i * 31);
        }
    }

    // Continue with create_building_from_grid as before
    BuildingGridParams->SetNumberField(TEXT("seed"), RetrySeed);
    ...
}
```

**Implementation note:** The `FloorPlanParams` and building location data must be preserved across the retry loop. Extract `BuildingLoc` array construction before the loop, store it, and reuse.

### 6.5 Fix #6: Street Furniture on All Streets (P1)

**Location:** Street furniture section (line 1814)

**Problem:** Only one `PlaceStreetFurniture` call with hardcoded south-street coordinates. The `StreetsArr` from lot layout contains all street segments, but furniture only gets placed on the block's front edge.

**Fix:** Loop over all street segments from the street results:
```cpp
if (!bSkipFurniture && !bSkipStreets && StreetResults.Num() > 0)
{
    TArray<FString> FurnitureTypes = GetGenreFurniture(Genre);

    for (int32 FurnIdx = 0; FurnIdx < StreetResults.Num(); ++FurnIdx)
    {
        const TSharedPtr<FJsonObject>* StreetObj = nullptr;
        if (!StreetResults[FurnIdx]->TryGetObject(StreetObj) || !StreetObj) continue;

        // Extract street geometry from the street result
        // Each street result should contain start/end world coords
        auto FurnParams = MakeShared<FJsonObject>();

        // Use the street's world coordinates
        if ((*StreetObj)->HasField(TEXT("start")))
            FurnParams->SetArrayField(TEXT("street_start"), (*StreetObj)->GetArrayField(TEXT("start")));
        if ((*StreetObj)->HasField(TEXT("end")))
            FurnParams->SetArrayField(TEXT("street_end"), (*StreetObj)->GetArrayField(TEXT("end")));

        // ... rest of furniture params ...
        FurnParams->SetNumberField(TEXT("seed"), Seed + 5000 + FurnIdx);
        FurnParams->SetStringField(TEXT("folder"), Folder + TEXT("/Furniture"));

        FMonolithActionResult FurnResult = PlaceStreetFurniture(FurnParams);
        // ... handle result ...
    }
}
```

**Note:** Need to verify what fields the street result JSON actually contains. The `CreateStreet` action result likely has the world-space start/end. If not, reconstruct from the original street segment data.

### 6.6 Fix #7: Wire furnish_building (P2)

**Location:** After building geometry generation succeeds (around line 1483)

**Add new Step 5.5 (between facades and roofs):**
```cpp
// Step 5.5: Furnish building interior
if (!bSkipFurniture)  // Reuse the skip_furniture flag or add skip_furnishing
{
    auto FurnishParams = MakeShared<FJsonObject>();
    FurnishParams->SetStringField(TEXT("building_id"), FString::Printf(TEXT("Building_%02d"), i));
    if (BuildingResult.IsValid())
    {
        FurnishParams->SetObjectField(TEXT("building_descriptor"), BuildingResult);
    }
    FurnishParams->SetNumberField(TEXT("seed"), Seed + i + 3000);
    FurnishParams->SetStringField(TEXT("folder"), Folder + TEXT("/Furnishing"));

    TSharedPtr<FJsonObject> FurnishResult;
    FString FurnishError;
    if (TryExecuteAction(TEXT("furnish_building"), FurnishParams, FurnishResult, FurnishError))
    {
        UE_LOG(LogMonolithCityBlock, Log, TEXT("    Furnish %d: OK"), i);
        if (BuildingResult.IsValid() && FurnishResult.IsValid())
            BuildingResult->SetObjectField(TEXT("furnishing"), FurnishResult);
    }
    else
    {
        UE_LOG(LogMonolithCityBlock, Warning, TEXT("    Furnish %d skipped: %s"), i, *FurnishError);
    }
}
```

**Also add `skip_furnishing` and `skip_volumes` optional params to `create_city_block`.**

### 6.7 Fix #8: Wire auto_volumes (P2)

**Location:** After furnishing, before roof generation

**Add new Step 5.7:**
```cpp
if (!bSkipVolumes)
{
    auto VolParams = MakeShared<FJsonObject>();
    VolParams->SetStringField(TEXT("building_id"), FString::Printf(TEXT("Building_%02d"), i));
    if (BuildingResult.IsValid())
    {
        VolParams->SetObjectField(TEXT("building_descriptor"), BuildingResult);
    }
    VolParams->SetStringField(TEXT("folder"), Folder + TEXT("/Volumes"));

    TSharedPtr<FJsonObject> VolResult;
    FString VolError;
    if (TryExecuteAction(TEXT("auto_volumes_for_building"), VolParams, VolResult, VolError))
    {
        UE_LOG(LogMonolithCityBlock, Log, TEXT("    Volumes %d: OK"), i);
    }
    else
    {
        UE_LOG(LogMonolithCityBlock, Warning, TEXT("    Volumes %d skipped: %s"), i, *VolError);
    }
}
```

### 6.8 Fix #9: Multi-Floor Building Support (P2)

**Location:** Floor plan generation step (line 1327)

**Problem:** `generate_floor_plan` is called once per building. For multi-floor archetypes (apartment 2-4 floors, office 2-3 floors), only one floor is generated.

**Fix approach (template path):** Multi-story templates already define per-floor grids with stairwell alignment. `generate_floor_plan` with a multi-story template returns all floors in one call. The template system (WP-A) handles this naturally since `FFloorPlanTemplate` has `TArray<FFloorData> Floors`.

**Fix approach (treemap fallback):** For the algorithmic path, call `generate_floor_plan` in a loop:
```cpp
int32 NumFloors = 1;
if (bHasFloorPlan && FloorPlanResult.IsValid() && FloorPlanResult->HasField(TEXT("num_floors")))
{
    NumFloors = static_cast<int32>(FloorPlanResult->GetNumberField(TEXT("num_floors")));
}
// Template path already returns all floors. Treemap path needs per-floor loop.
// For now: template handles multi-floor. Treemap stays single-floor (acceptable since templates are primary).
```

The `create_building_from_grid` already supports multi-floor via its `floors` array field. The key is ensuring `generate_floor_plan` outputs the correct structure.

### 6.9 New Optional Parameters for `create_city_block`

```cpp
.Optional(TEXT("skip_furnishing"), TEXT("boolean"), TEXT("Skip interior furnishing"), TEXT("false"))
.Optional(TEXT("skip_volumes"), TEXT("boolean"), TEXT("Skip navmesh/audio volume generation"), TEXT("false"))
```

### 6.10 Done Criteria

- [ ] Genre pools only reference existing archetypes (or gracefully fall back)
- [ ] `horror_level` field set on FloorPlanParams when Decay > 0
- [ ] Decay metadata (tilt, boarded_windows) transferred to building params
- [ ] Retry loop regenerates floor plan with new seed before rebuilding geometry
- [ ] Street furniture placed on all street segments, not just the south street
- [ ] `furnish_building` called for each building (unless skip_furnishing)
- [ ] `auto_volumes_for_building` called for each building (unless skip_volumes)
- [ ] Multi-floor buildings get per-floor grid data from templates
- [ ] Compiles with zero errors/warnings

---

## 7. WP-D: Template JSON Creation

**Agent:** `research-agent` or `unreal-blueprint-expert` (data conversion, no C++ needed)
**Output:** 47 JSON files in `Saved/Monolith/FloorPlanTemplates/`
**Source Documents:**

| Document | Templates | Category |
|----------|-----------|----------|
| `2026-03-28-real-floorplan-templates.md` | 7 | residential |
| `2026-03-28-residential-floorplan-templates.md` | 10 | residential |
| `2026-03-28-real-commercial-templates.md` | 6 | commercial |
| `2026-03-28-commercial-variety-templates.md` | 10 | commercial |
| `2026-03-28-horror-floorplan-templates.md` | 8 | horror |
| `2026-03-28-multistory-floorplan-templates.md` | 6 | multistory |

### 7.1 Conversion Process

For each template in each research document:

1. **Extract grid data.** Each doc has either:
   - ASCII grid diagrams with room index numbers
   - JSON room definitions with bounds `[x_min, y_min, x_max, y_max]`
   - Or both

2. **Build 2D grid array.** Convert ASCII/bounds to `grid[row][col]` format where each value is the room index.

3. **Build rooms array.** Extract `room_id`, `room_type`, `room_index`, `area_cells`, `exterior_wall` from the research doc's room definitions.

4. **Build doors array.** Extract door positions. Compute correct EdgeStart/EdgeEnd ON the wall boundary (not room cells -- apply the fix from WP-B). If the research doc doesn't specify exact door positions, derive them from room adjacency.

5. **Build entrance data.** Each template should specify which room/wall/position the main entrance is on.

6. **Build stairwell data** (multi-story only). Extract from the `stairwells` section. Verify alignment across floors.

7. **Add metadata:** footprint_m, min/max_footprint_m, roof_type, circulation, tags, description, horror_notes, hospice_notes.

8. **Validate:** Every room_id referenced in doors must exist in rooms. All grid cells must be accounted for. Stairwell cells match across floors.

### 7.2 Grid Convention Reconciliation

**IMPORTANT:** The 6 research documents use DIFFERENT grid conventions:

| Document | Y-axis | Origin |
|----------|--------|--------|
| real-floorplan-templates.md | Y=0 south, Y increases north | bottom-left |
| real-commercial-templates.md | Y=0 top, Y increases down | top-left |
| horror-floorplan-templates.md | Y=0 top, Y increases down | top-left |
| residential-floorplan-templates.md | Y=0 top (row), Y increases down | top-left |
| commercial-variety-templates.md | Not explicit, diagrams use NSEW | top-left |
| multistory-floorplan-templates.md | Y=0 top, Y increases down | top-left |

**Normalize ALL templates to:** Y=0 at top (north), Y increases downward (south). This matches the majority convention AND the existing `create_building_from_grid` expectation. Templates from `real-floorplan-templates.md` need Y-axis flipping.

### 7.3 Quality Checks Per Template

- [ ] Grid dimensions match stated footprint: `grid_width * 0.5 == footprint_m.width` (within 1 cell tolerance)
- [ ] All room indices in grid are represented in rooms array
- [ ] No room index in rooms array is absent from grid
- [ ] Door edge_start and edge_end are on the same axis (both same X or both same Y)
- [ ] Stairwell cells are marked as -2 on upper floors
- [ ] Stairwell grid positions identical across all floors
- [ ] At least one room has `exterior_wall: true` with entrance data
- [ ] Corridor/hallway width >= 3 cells (150cm) for player capsule clearance
- [ ] No room smaller than 4 cells (1 m^2) -- minimum viable room

### 7.4 Template Count Summary

| Category | Count | Templates |
|----------|-------|-----------|
| residential | 17 | small_ranch, medium_colonial, l_shaped_ranch, cape_cod, split_level, small_bungalow, shotgun_house, tiny_cabin, studio_apartment, 1950s_ranch, two_story_colonial, modern_open_plan, townhouse, farmhouse, duplex_unit, trailer_mobile_home, (1 overlap removed) |
| commercial | 16 | small_office, retail_store, restaurant, bank, medical_clinic, auto_repair, corner_store, gas_station, diner, bar_pub, laundromat, fire_station, small_library, post_office, pharmacy, auto_repair_shop |
| horror | 8 | abandoned_hospital, victorian_mansion, apartment_hallway, basement_cellar, abandoned_school, church, motel, underground_bunker |
| multistory | 6 | 3story_apartment, 2story_office, 2story_colonial, 3story_hotel, 2story_school, 2story_police_station |
| **Total** | **47** | |

Note: Some templates appear in multiple research docs (e.g., colonial appears in both residential docs and multistory). Deduplicate: if a template exists in both single-story and multi-story form, keep both as separate templates (e.g., `two_story_colonial.json` in residential for the single-floor version, `2story_colonial.json` in multistory for the multi-floor version).

### 7.5 Done Criteria

- [ ] 47 JSON files created in correct directory structure
- [ ] All JSONs parse without error (valid JSON)
- [ ] All JSONs conform to the schema defined in WP-A section 2
- [ ] Grid convention normalized (Y=0 top, Y increases downward)
- [ ] Door edge coordinates are wall-boundary coordinates (same axis)
- [ ] Multi-story templates have stairwell alignment verified
- [ ] At least 3 templates per category have been manually spot-checked against source doc

---

## 8. Phase Schedule

### Phase 1: Parallel Foundation (WP-C + WP-D)

These have zero file conflicts with each other.

| # | Task | Agent | Done When |
|---|------|-------|-----------|
| 1 | WP-C: All orchestrator wiring fixes (bugs #2-9) | `unreal-mesh-expert` | Compiles clean. Genre pools valid. horror_level passed. Decay applied. Retry regenerates. Furniture on all streets. furnish/volumes wired. |
| 2 | WP-D: Create 47 template JSON files | `research-agent` | 47 valid JSON files in `Saved/Monolith/FloorPlanTemplates/`. Schema matches. Grids normalized. |

### Phase 2: Template C++ (WP-A + WP-B)

These MUST be done by the same agent (same file). WP-A first, WP-B second.

| # | Task | Agent | Done When |
|---|------|-------|-----------|
| 3 | WP-A: Template loading, selection, scaling in FloorPlanGenerator | `unreal-mesh-expert` | Template system works. `generate_floor_plan` uses templates when available, falls back to treemap. Multi-floor via templates. |
| 4 | WP-B: Fix door edge coordinates in PlaceDoors + EnsureExteriorEntrance | `unreal-mesh-expert` | All generated doors have EdgeStart/EdgeEnd on same axis (wall boundary). Door cutters correctly oriented. |

### Phase 3: Review + Integration Test

| # | Task | Agent | Done When |
|---|------|-------|-----------|
| 5 | Code review of all changes | `unreal-code-reviewer` | No issues found. All edge cases covered. |
| 6 | Integration test: `create_city_block` with templates | `unreal-mesh-expert` | Full block generates with template-based floor plans. Doors cut correctly. Buildings furnished. Volumes placed. Street furniture on all streets. |

### Dependency Graph

```
Phase 1:  [WP-C] ----+
          [WP-D] ----+---- Phase 3: [Review] -> [Integration Test]
                      |
Phase 2:  [WP-A -> WP-B] -+
```

Phase 1 and Phase 2 can run in parallel (different files). Phase 3 waits for all.

---

## 9. Test Criteria

### Unit Tests

1. **Template loading:** Load each of the 47 templates, verify grid dimensions, room count, door validity
2. **Template selection:** Given archetype + footprint, verify correct template category match
3. **Grid scaling:** Scale a 20x16 template to 30x24, verify room proportions preserved
4. **Door coordinates:** For every generated door, assert `EdgeStart.X == EdgeEnd.X || EdgeStart.Y == EdgeEnd.Y`
5. **Stairwell alignment:** For multi-story templates, assert stairwell cells identical across floors

### Integration Tests

1. **Single building with template:** `generate_floor_plan` with `archetype=residential_house` produces template-based layout
2. **Single building fallback:** `generate_floor_plan` with novel archetype falls back to treemap
3. **Full block suburban:** `create_city_block` with `genre=suburban`, verify all buildings use templates
4. **Full block downtown:** `create_city_block` with `genre=downtown`, verify commercial templates used
5. **Horror decay:** `create_city_block` with `decay=0.7`, verify horror_level passed, decay metadata applied
6. **Retry regeneration:** Enable `validate_and_retry`, trigger retry, verify new floor plan generated
7. **Multi-story:** Block with apartment archetype, verify 2+ floor building with stairwell

### Regression Tests

1. Existing `generate_floor_plan` direct calls still work (treemap path)
2. Existing `create_building_from_grid` with manual grid input unchanged
3. `list_building_archetypes` still returns archetype list
4. Hospice mode door widths (120cm) preserved in template path

---

## 10. Risks and Mitigations

### R1: Research Doc Grid Data Has Errors
**Risk:** Several research docs contain self-corrections ("Wait -- this layout has problems", "Let me redo this properly"). The raw ASCII grids may be inconsistent with the room definitions.
**Mitigation:** WP-D agent must use the FINAL corrected version of each template. Where docs show multiple attempts, use the last one. Cross-check grid cell counts against stated room areas. Flag any template where grid doesn't match room schedule for manual review.

### R2: Grid Scaling Introduces Artifacts
**Risk:** Scaling a template by non-integer factors can create fractional cell positions, splitting rooms or creating 1-cell slivers.
**Mitigation:** Use nearest-neighbor scaling with room-ID flood fill. After scaling, run a cleanup pass that absorbs any room smaller than 4 cells into its largest neighbor. Clamp scale factors to [0.7, 1.5] range. If scale is too extreme, fall back to treemap.

### R3: Template Doesn't Match Archetype Room Types
**Risk:** A residential template might define rooms like "living_room", "kitchen", "bedroom" but the archetype expects "family_room", "den", "office". Mismatch could cause furnishing failures.
**Mitigation:** Template rooms override archetype room definitions when template is used. The template IS the room schedule. Archetype provides only: material hints, roof type, floor count range, and adjacency rules (for validation, not layout).

### R4: create_building_from_grid Door Parsing Assumptions
**Risk:** The existing `create_building_from_grid` code (line 683+) has specific expectations about edge coordinates. If our fix doesn't match exactly, doors will still be wrong.
**Mitigation:** Read `MonolithMeshBuildingActions.cpp` door parsing in detail before implementing WP-B. The BuildingActions code expects: "If edge_start.X == edge_end.X, door is on vertical wall" (line 692). Our fix must ensure this invariant.

### R5: Performance With 47 Template Files
**Risk:** Loading template directory on every `generate_floor_plan` call could be slow.
**Mitigation:** Cache template metadata (name, category, footprint ranges) on first load. Only load full grid data when a template is selected. Use `TMap<FString, FFloorPlanTemplate>` static cache, invalidated on template directory modification timestamp change.

---

## Appendix: File Reference

### Files Modified

| File | Path | Changes |
|------|------|---------|
| FloorPlanGenerator.h | `Source/MonolithMesh/Public/MonolithMeshFloorPlanGenerator.h` | +FFloorPlanTemplate struct, +5 new function declarations |
| FloorPlanGenerator.cpp | `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp` | +template loading/selection/scaling (~400 lines), door edge fix (~80 lines) |
| CityBlockActions.cpp | `Source/MonolithMesh/Private/MonolithMeshCityBlockActions.cpp` | Genre pool fix, horror_level, decay, retry, furniture, furnish, volumes (~250 lines) |

### Files Created

| Count | Path | Content |
|-------|------|---------|
| 47 | `Saved/Monolith/FloorPlanTemplates/{category}/*.json` | Template JSON files |

### Files Read-Only (for reference)

| File | Path | Why |
|------|------|-----|
| BuildingActions.cpp | `Source/MonolithMesh/Private/MonolithMeshBuildingActions.cpp` | Understand door edge consumption (lines 683-697) |
| BuildingTypes.h | `Source/MonolithMesh/Public/MonolithMeshBuildingTypes.h` | FDoorDef, FRoomDef, FFloorPlan structs |
| FurnishingActions.cpp | `Source/MonolithMesh/Private/MonolithMeshFurnishingActions.cpp` | Verify furnish_building action interface |
| AutoVolumeActions.cpp | `Source/MonolithMesh/Private/MonolithMeshAutoVolumeActions.cpp` | Verify auto_volumes_for_building interface |

### Source Documents (for WP-D)

All in `Plugins/Monolith/Docs/plans/`:
- `2026-03-28-real-floorplan-templates.md`
- `2026-03-28-real-commercial-templates.md`
- `2026-03-28-horror-floorplan-templates.md`
- `2026-03-28-residential-floorplan-templates.md`
- `2026-03-28-commercial-variety-templates.md`
- `2026-03-28-multistory-floorplan-templates.md`

---

## Review #1

**Reviewer:** unreal-code-reviewer | **Date:** 2026-03-29 | **Verdict:** Approve with required fixes (2 Critical, 3 Important, 4 Suggestions)

### What's Good

Solid plan overall. The template system architecture is well-thought-out -- template-first with treemap fallback is the right call. The bug inventory is comprehensive, root causes are correctly identified, and the file conflict matrix is a smart addition that will prevent merge headaches. The door edge coordinate bug analysis (WP-B) is spot-on: I verified `BuildingActions.cpp` line 692 confirms the `EdgeStart.X == EdgeEnd.X` invariant the plan targets. The phase ordering correctly sequences WP-A before WP-B on the shared file.

### 1. All 10 Bugs Addressed?

Yes. All 10 bugs from the inventory are covered:
- Bug 1 (door cutters): WP-B
- Bug 2 (missing archetypes): WP-C section 6.1
- Bug 3 (horror_level): WP-C section 6.2
- Bug 4 (decay metadata): WP-C section 6.3
- Bug 5 (retry): WP-C section 6.4
- Bug 6 (street furniture): WP-C section 6.5
- Bug 7 (furnish_building): WP-C section 6.6
- Bug 8 (auto_volumes): WP-C section 6.7
- Bug 9 (multi-floor): WP-C section 6.8
- Bug 10 (entrance wall): mitigated by template system

### 2. Template System Architecture

Sound design. A few concerns:

**[CRITICAL] C-1: `FFloorPlanTemplate` duplicates `FRoomDef`/`FDoorDef` data but with different field names.** The template JSON schema uses `room_index`, `area_cells`, `exterior_wall`, `width_cm` -- fields that don't exist on the actual `FRoomDef` and `FDoorDef` structs in `MonolithMeshBuildingTypes.h`. The `LoadTemplate` function will need a non-trivial mapping layer to convert template JSON rooms/doors into `FRoomDef`/`FDoorDef` instances. The plan doesn't specify this mapping. For example, `FRoomDef` stores `GridCells` as `TArray<FIntPoint>` but the template schema stores rooms by `room_index` with the grid providing the cell-to-room mapping. The implementer needs to reconstruct `GridCells` by scanning the grid for matching indices. This is doable but should be explicitly called out, or the template schema should be revised to match the existing structs more closely.

**[SUGGESTION] S-1: Template caching strategy (R5) is good but incomplete.** The plan says "static cache, invalidated on template directory modification timestamp." On Windows, directory mtime only changes when direct children are added/removed, not when file contents change. Use per-file mtime or a simple hash of directory listing. Alternatively, just cache for the lifetime of the editor session and provide a `reload_templates` action -- simpler and sufficient for a design-time tool.

### 3. File Conflicts Between WPs

The conflict matrix is correct. Verified:
- WP-A + WP-B both touch `FloorPlanGenerator.cpp` -- correctly sequenced
- WP-C touches only `CityBlockActions.cpp` -- independent
- WP-D creates new files only -- independent

**However**, the dependency graph on line 876-880 says "Phase 1 and Phase 2 can run in parallel" but Phase 2 includes WP-A which reads templates from disk that WP-D creates. If WP-A is developed before WP-D completes, the template loading code can't be tested. This is fine for compilation but the plan should note that WP-A unit testing depends on WP-D (or at least a handful of test templates).

### 4. Door Edge Coordinate Fix

**[CRITICAL] C-2: The `Max()` boundary convention is wrong for west/north walls.** Section 5.2 says: "Vertical wall: boundary is at `max(DoorCell.X, NeighborCell.X)`". This works for an east wall (Room A at X=4, Room B at X=5, boundary at X=5). But for a WEST wall (Room A at X=5, Room B at X=4), `max(5, 4) = 5` which is Room A's interior, not the wall boundary. The boundary between X=4 and X=5 is at X=5 in grid coordinates (the left edge of the higher-indexed cell), which happens to be correct here, but only by accident -- the logic description is misleading.

More precisely: the wall boundary in grid coordinates sits at the X (or Y) value of whichever cell is to the east (or south). The correct formulation is:
```
WallPoint.X = FMath::Max(DoorCell.X, NeighborCell.X);  // East/West: boundary is at the larger X
WallPoint.Y = FMath::Max(DoorCell.Y, NeighborCell.Y);  // North/South: boundary is at the larger Y
```
This is actually what the plan says, and it IS correct in practice because `create_building_from_grid` at line 695 does `WallPos = EdgeStart.X * CellSize`, which places the wall at the left edge of that grid column. So `Max(4, 5) = 5` means `5 * 50 = 250cm`, which is exactly the boundary between column 4 and column 5. The math works out but the plan's comment "boundary is at max" should explain WHY this is correct -- the boundary between cell N-1 and cell N is at coordinate N in grid space because cell coordinates reference their left/top edge.

**[IMPORTANT] I-1: The `CellSize` variable (section 5.4) needs a definitive answer, not three options.** The plan lists three approaches but doesn't pick one. The value 50.0f is used as `GridCellSize` in `FBuildingDescriptor` (line 314 of BuildingTypes.h) and hardcoded throughout FloorPlanGenerator. The implementer should use the existing constant or add one. Recommendation: add `static constexpr float CellSizeCm = 50.0f;` to `FMonolithMeshFloorPlanGenerator` and use it. Don't pass it as a parameter -- that's over-engineering for a value that's baked into the grid system.

### 5. Phase Ordering

Correct. The dependency graph accurately reflects the file conflicts. One note:

**[IMPORTANT] I-2: Phase 2 ordering says "WP-A first, WP-B second" but the plan doesn't explain why beyond "same file."** The actual reason is stronger: WP-A changes `GenerateFloorPlan` to use templates, and the template path should produce doors with CORRECT edge coordinates from the start (since template doors are pre-authored with correct coords). WP-B fixes the TREEMAP fallback path's door generation. If WP-B were done first, WP-A's template loading code might accidentally regress by not preserving the already-correct template door coordinates through `ScaleTemplate`. The plan should note that `ScaleTemplate` (WP-A) must recompute door edge coordinates using the SAME boundary convention from WP-B section 5.2.

### 6. Estimates

Reasonable but tight:
- WP-A at 400-500 lines: realistic. `LoadTemplate` JSON parsing + `FindBestTemplate` scoring + `ScaleTemplate` with flood fill + integration into `GenerateFloorPlan` will be substantial.
- WP-B at 50-80 lines: correct, it's a targeted fix in three locations.
- WP-C at 200-300 lines: slightly optimistic. The retry regeneration fix (6.4) alone is ~50 lines, street furniture loop is ~40, furnish/volumes wiring is ~60, and the other fixes add up. 250-350 is more realistic.
- WP-D at 47 JSON files: this is the wild card. Each template needs a hand-authored grid, rooms, and doors. Even at 10 minutes per template (optimistic), that's 8 hours of data entry. The research docs may not all have clean grid data ready to copy. Budget extra time here.

### 7. Missing Risks

**[IMPORTANT] I-3: No rollback strategy.** The plan modifies three critical files (`FloorPlanGenerator.cpp`, `CityBlockActions.cpp`, `BuildingTypes.h` read-only but `FloorPlanGenerator.h` write). If the template system introduces regressions in the treemap path, there's no feature flag to disable it. Recommendation: add a `"use_templates"` param (default true) on `generate_floor_plan` that can be set to false to force the treemap path. This costs ~5 lines and provides a safety valve.

**[SUGGESTION] S-2: Grid scaling (section 4.2, step 2.5) clamps to [0.7, 1.5] but doesn't define what happens at the boundaries.** If a requested footprint is 2x the template's design footprint, the plan says "fall back to treemap." But the `FindBestTemplate` selection already filters by `min_footprint_m` / `max_footprint_m`. If a template passes selection but then fails the scale clamp, there's a gap. The selection filter should be slightly tighter than the scale clamp to avoid this edge case.

**[SUGGESTION] S-3: The plan doesn't address what happens when `TryExecuteAction("furnish_building", ...)` or `TryExecuteAction("auto_volumes_for_building", ...)` fails because those actions have required params the plan doesn't supply.** The plan should verify the action signatures of `furnish_building` and `auto_volumes_for_building` from their respective source files (listed as read-only references but not actually read in the plan). If those actions require a `building_descriptor` in a specific format, the wiring code needs to match.

**[SUGGESTION] S-4: Bug #2 fix (section 6.1) replaces `commercial_shop` with `restaurant` and `office_building` in the downtown pool, but `garage` in the suburban pool is replaced with just another `residential_house`.** This means suburban blocks are now 100% residential houses with zero variety. Consider adding `small_house` or one of the commercial types to the suburban pool.

### Summary

| Category | Count | Items |
|----------|-------|-------|
| Critical | 2 | C-1 (template-to-struct mapping gap), C-2 (boundary convention comment misleading -- math is correct but explanation needs fixing) |
| Important | 3 | I-1 (CellSize needs decision), I-2 (ScaleTemplate must preserve WP-B convention), I-3 (no rollback/feature flag) |
| Suggestion | 4 | S-1 (cache invalidation), S-2 (scale clamp vs selection filter gap), S-3 (verify furnish/volumes action signatures), S-4 (suburban pool variety) |

**Recommendation:** Fix C-1 and I-3 before execution. C-2 is technically correct but the comment is confusing -- clarify for the implementer. Everything else can be addressed during implementation.

---

## Review #2: Player Experience, Horror Design, and Hospice Accessibility

**Reviewer:** unreal-code-reviewer (Independent Review #2)
**Date:** 2026-03-29
**Focus:** Navigability, variety, horror effectiveness, multi-story alignment, hospice accessibility, genre pools
**Verdict:** APPROVED with important items to address before or during implementation

---

### What Works Well

The template system is a massive upgrade over the treemap algorithm. The core decision -- hand-curated layouts over procedural generation for floor plans -- is correct. Real buildings have architectural grammar (circulation patterns, room adjacency rules, wet wall stacking) that no treemap will produce. The research backing these templates is thorough and grounded in actual American building typologies.

The horror templates are genuinely excellent. The abandoned hospital wing's "return trip" design, the Victorian mansion's dual circulation, the apartment corridor's repetition-as-dread, the basement's vertical isolation -- these are architecturally sound horror design patterns drawn from proven games (Silent Hill, Resident Evil, PT). The horror notes per template read like a level designer's playbook and will be invaluable during furnishing and lighting passes.

The multi-story stairwell alignment system is well-specified. The -2 cell convention for upper floor cutouts, the explicit alignment checklists, and the plumbing stack awareness all indicate someone who understands that buildings are 3D objects, not stacked 2D grids.

---

### Criterion 1: Will Templates Produce Navigable Buildings?

**Assessment: YES, with one concern.**

All templates specify corridor widths of 3+ cells (150cm+), which clears the 84cm player capsule with comfortable margin. Door widths are 2+ cells (100cm+). Room sizes are realistic and large enough for FPS movement. The circulation patterns (hub-and-spoke, double-loaded corridor, racetrack loop) are all standard architectural patterns that players intuitively understand.

**Important -- Grid Scaling Risk to Navigability:**

The grid scaling algorithm (Section 2, lines 141-165) allows scaling templates down to 70% of design size. At 70% scale, a 3-cell corridor (150cm) becomes ~2.1 cells. After rounding, that could be 2 cells (100cm). A 100cm corridor with an 84cm capsule leaves 8cm clearance per side. That is technically passable but will feel claustrophobic in a way that breaks navigation flow, not enhances horror.

> **IMPORTANT [I-1]:** Add a post-scaling validation pass that checks ALL corridors and doorways remain >= 3 cells wide (or >= 2 cells for doors). If any passage falls below minimum, reject the scale and fall back to treemap or pick a different template. The plan mentions absorbing rooms < 4 cells (R2 mitigation) but does not mention corridor width validation.

---

### Criterion 2: Enough Variety for a Full Town?

**Assessment: YES, with a gap.**

47 templates across 4 categories is strong. 17 residential templates alone means a suburban block of 6-8 houses will rarely repeat (especially with the top-3 weighted random selection). The commercial set of 16 covers the standard American small-town strip. Multi-story adds 6 more for downtown areas.

The variety concern is not quantity but visual distinctiveness after geometry generation. Two different templates that both produce rectangular single-story buildings of similar footprint will look identical from the outside unless the facade system differentiates them.

**Suggestion [S-1]:** The plan mentions `roof_type` in the schema but the template selection algorithm (Section 2, lines 128-139) does not factor it into scoring or variety enforcement. Consider a "no two adjacent buildings with same roof_type" constraint in the city block orchestrator to prevent visual monotony on a street.

**Gap -- No warehouse/industrial template:**

The genre pools (Section 6.1) include `warehouse` as an archetype, and the archetype-to-template mapping (Section 4.4, line 366) maps it to "commercial". But none of the 16 commercial templates are warehouse-like (large single open volume, loading docks, minimal interior walls). A warehouse template would be trivial -- essentially one big room with an office partitioned off in a corner -- but its absence means warehouse buildings will get a retail store or restaurant floor plan, which is wrong.

> **IMPORTANT [I-2]:** Add at least one `warehouse.json` template to the commercial category. Single large open volume (80%+ floor area), small office partition, loading dock entrance on one wall. This is a 30-minute template to write and fills a real gap.

---

### Criterion 3: Are Horror Templates Actually Scary?

**Assessment: YES. These are genuinely well-designed.**

Each horror template employs distinct architectural horror principles:

- **Abandoned Hospital:** Linear dead-end with forced return trip. The "operating room at the end of a patient wing" architectural violation is smart -- players who know hospitals will feel the wrongness before understanding why.
- **Victorian Mansion:** Dual circulation (grand hall + hidden servant corridor) creates information asymmetry. The player sees most of the house; the house sees all of it. The nursery-that-shouldn't-exist is effective.
- **Apartment Hallway:** Repetition-as-dread. 12 identical doors. The mirrored apartment (310) is subtle architectural uncanny valley. The locked doors that never resolve create permanent uncertainty.
- **Basement/Cellar:** Vertical isolation with impossible geometry (extends beyond building footprint). The sealed room at maximum depth from the only exit is textbook horror pacing.
- **Abandoned School:** Childhood vulnerability + progressive escalation. Classroom 110 (field hospital) is the architectural grammar violation.
- **Church:** Sacred space desecration. Linear processional forces the player down the aisle.
- **Motel:** Isolation + exterior exposure.
- **Underground Bunker:** Claustrophobia + paranoia.

The horror design is layered -- it works at the architectural level (circulation, sight lines, dead ends), the environmental storytelling level (horror notes per room), and the gameplay level (breaker panels, PA systems, key rings). The templates give level designers clear hooks without prescribing exact scares.

**Suggestion [S-2]:** The horror templates define door states (`locked`, `ajar`, `chain_locked`, `locked_from_inside`, `sealed`) that are not part of the main template JSON schema in the fix plan (Section 2, lines 74-123). The schema shows `width_cm` and `wall` but no `state` field. Either add `state` to the door schema or document that door states are furnishing-pass metadata, not geometry-pass data.

---

### Criterion 4: Do Multi-Story Buildings Work with Stairwell Alignment?

**Assessment: YES, the alignment system is solid.**

The stairwell alignment rules (multistory doc, Section 1) are correct:
- Same grid coordinates on every floor (enforced by `stairwell_id` linking)
- Upper floors mark stairwell cells as -2 (void/cutout)
- Minimum 4x6 cell footprint (200cm x 300cm) yields ~34-degree slope, well under the 44.77-degree UE5 walkable limit
- Recommended 5x7 (250cm x 350cm) gives clearance for the 42cm-radius capsule

The verification checklists in each multi-story template confirm alignment was manually checked. The plumbing stack awareness (restrooms above restrooms, powder room below bathroom) is a nice architectural realism detail.

**Important -- Stairwell Scaling Gap:**

The grid scaling algorithm (Section 2, lines 161-165) says "Recompute stairwell cell positions (must maintain minimum 4x6 footprint)." But the algorithm does not specify what happens when scaling DOWN causes a 5x7 stairwell to become 3x5 (below minimum). The room absorption rule (rooms < 4 cells get absorbed) does not apply to stairwells -- absorbing a stairwell into an adjacent room breaks vertical circulation.

> **IMPORTANT [I-3]:** Add an explicit rule: if scaling would reduce any stairwell below 4x6 cells, reject the scale factor for that template. Stairwells are structural and cannot be absorbed, shrunk, or moved. This should be a hard constraint in `ScaleTemplate()`.

---

### Criterion 5: Is Hospice Accessibility Maintained?

**Assessment: PARTIALLY. Needs attention.**

The plan mentions hospice mode in two places:
1. Section 5.4 (line 475): `float DoorWidth = bHospiceMode ? 120.0f : 110.0f`
2. Template schema (line 122): `"hospice_notes": "All doors 120cm. No narrow passages."`

However, the hospice accessibility story has gaps:

**What's covered:**
- Door width override to 120cm in hospice mode
- `hospice_notes` field in the template schema
- Corridor widths are generous (3+ cells = 150cm+)

**What's missing:**

> **IMPORTANT [I-4]:** The template system does not override door widths from templates when `bHospiceMode` is true. Templates define their own `width_cm` per door (some as low as 80cm for powder rooms). The plan's WP-B fix (Section 5.4) only handles the treemap path's `PlaceDoors` function. When using templates, the door widths come directly from the JSON. Add a post-load pass in the template path that clamps all door widths to `max(door.width_cm, 120)` when hospice mode is active.

> **SUGGESTION [S-3]:** Several horror templates have intentionally narrow or claustrophobic spaces (basement 240cm ceiling, servant corridors at 3 cells wide, apartment corridor at 3 cells). For hospice mode, consider a template tag system where templates tagged `claustrophobic` are excluded from the candidate pool, or have their corridor widths bumped to 4 cells. The game is for people in end-of-life care -- claustrophobia as a horror mechanic may not land the same way for this audience. This is a design decision, not a code issue, but worth flagging.

> **SUGGESTION [S-4]:** No mention of wheelchair-width consideration. 120cm doors are good (standard wheelchair clearance is 81.3cm / 32 inches, so 120cm is generous). But corridors need turning radius clearance too. A 150cm (3-cell) corridor allows passage but not turning. 180cm (4 cells, ~72 inches) is the ADA turning radius. For hospice mode, consider defaulting corridor minimum to 4 cells. Again, a design decision -- flag it for the team.

---

### Criterion 6: Do Genre Pools Make Sense?

**Assessment: MOSTLY. One issue.**

The genre pool fix (Section 6.1, lines 497-521) replaces non-existent archetypes with valid ones. The proposed pools:

- **Suburban:** `residential_house x2, small_house, residential_house` -- weighted toward houses. Makes sense.
- **Downtown:** `restaurant, office_building, apartment, warehouse` -- good mix. But `warehouse` hits the template gap noted in I-2.
- **Horror:** `residential_house x2, restaurant, small_house` -- wait, this is just suburban with one swap?

> **IMPORTANT [I-5]:** The horror genre pool should include at least one horror-category template building. The pool currently maps to residential and commercial archetypes, which means the horror templates (abandoned_hospital, victorian_mansion, church, etc.) will NEVER be selected by the genre pool system. The archetype-to-template mapping (Section 4.4) maps these archetypes to residential/commercial categories, not horror. Add a mechanism: when `genre == "horror"`, either (a) directly inject horror-category templates into the candidate pool regardless of archetype, or (b) add horror-specific archetypes that map to the horror template category. Without this, the 8 horror templates are only usable via explicit `template` parameter, never via the automatic `create_city_block` flow.

---

### Criterion 7: What's Missing?

**Missing Items (in addition to the above):**

> **IMPORTANT [I-6]:** No exterior entrance validation for scaled templates. The plan says templates define entrance positions. After scaling, the entrance position is recomputed proportionally. But if scaling changes the grid dimensions, the entrance might land on a cell that is no longer an exterior wall (e.g., if room boundaries shifted during non-uniform scaling). Add a post-scaling check that verifies the entrance room still has an exterior wall face, and the entrance position is on that face.

> **SUGGESTION [S-5]:** No template versioning or format version field. When the schema inevitably evolves (and it will -- door states, furniture hints, lighting zones), old templates will silently fail. Add a `"schema_version": 1` field to the JSON schema now. The loader can then warn or migrate.

> **SUGGESTION [S-6]:** The L-shaped ranch template (real-floorplan-templates.md, line 352+) uses -1 as the room index for void/exterior cells in the L-shape cutout. But the multi-story convention (multistory doc, line 51) defines -1 as corridor/hallway. These conventions conflict. The residential templates use -1 for exterior void; the multistory templates use 0 for exterior void. The grid convention reconciliation section (plan lines 797-808) addresses Y-axis differences but does NOT address this cell ID convention conflict. Resolve this before WP-D creates the JSON files, or the template loader will misinterpret exterior void as corridor.

> **SUGGESTION [S-7]:** No "windows" field in the template schema. Templates mark `exterior_wall: true` on rooms, but don't specify window positions. For horror, window placement matters enormously (sight lines to exterior, exterior threats visible through windows, boarding-up windows for decay). Consider adding an optional `windows` array to the room definitions. Not critical for geometry generation but very useful for the facade and furnishing passes.

---

### Issue Summary

| ID | Severity | Description |
|----|----------|-------------|
| I-1 | IMPORTANT | Post-scaling corridor/doorway width validation (minimum 3 cells corridor, 2 cells door) |
| I-2 | IMPORTANT | Add warehouse template to commercial category |
| I-3 | IMPORTANT | Hard constraint: stairwells cannot scale below 4x6 cells |
| I-4 | IMPORTANT | Template path must clamp door widths in hospice mode |
| I-5 | IMPORTANT | Horror genre pool never selects horror-category templates |
| I-6 | IMPORTANT | Post-scaling entrance position validation |
| S-1 | SUGGESTION | Roof type variety constraint for adjacent buildings |
| S-2 | SUGGESTION | Door state field in schema or documented as furnishing-pass data |
| S-3 | SUGGESTION | Claustrophobic template exclusion for hospice mode |
| S-4 | SUGGESTION | Wider corridor minimum (4 cells) for hospice mode turning radius |
| S-5 | SUGGESTION | Schema version field in template JSON |
| S-6 | SUGGESTION | Resolve -1 cell ID convention conflict (exterior void vs corridor) |
| S-7 | SUGGESTION | Optional windows array in room definitions |

**Zero critical/blocking issues.** The 6 important items are all additive (validation passes, one missing template, one mode override, one pool fix) and can be addressed during implementation without restructuring the plan. The plan is approved for execution.

---

*Review #2 complete. Focused on player experience, horror design, and hospice accessibility. No structural changes to the plan required.*
