# Procedural Town Generator -- Fix Plan v2

**Date:** 2026-03-28
**Status:** APPROVED WITH CHANGES — Both reviewers approved. Fixes applied, executing.
**Scope:** 20 issues across geometry, logic, room sizing, and validation
**Estimated Total:** ~55-65 hours across 7 work packages, 4 phases
**Files Affected:** 7 C++ source files, 9 archetype JSONs (new), 1 header

---

## Overview

The Procedural Town Generator (SP1-SP10, 45 actions) is functionally complete but has 20 issues discovered during playtesting. These fall into four categories:

1. **Critical Geometry Bugs** (5 issues) -- stairs/ramps at unusable angles, windows not visible, stairwells missing floor cutouts
2. **Missing Logic** (7 issues) -- no entrance doors, features don't integrate with buildings, rooms too narrow
3. **Room Size / Layout** (5 issues) -- all rooms 4-5x undersized, no per-floor assignment, no aspect ratio control
4. **Validation** (3 issues) -- no post-generation verification of playability

Research basis: 8 research documents totaling ~350KB of analysis, building code references, and algorithm specifications.

---

## Issue-to-Work-Package Map

| # | Issue | Severity | Work Package |
|---|-------|----------|-------------|
| 1 | Stairs too steep (70 deg) | P0 | WP-1a |
| 2 | Fire escape stairs at 66 deg | P0 | WP-1a |
| 3 | ADA ramp switchback self-intersects | P0 | WP-1a |
| 4 | Windows not cut through (facade/building wall overlap) | P0 | WP-1b |
| 5 | Stairwell has no ceiling/floor cutout on upper floor | P0 | WP-1c |
| 6 | No exterior entrance door | P1 | WP-2a |
| 7 | Doors too narrow (90cm for 84cm capsule) | P1 | WP-2a |
| 8 | Corridors too narrow (100cm for 84cm capsule) | P1 | WP-2a |
| 9 | Balcony faces wrong direction | P1 | WP-2b |
| 10 | Porch has no door cut into building wall | P1 | WP-2b |
| 11 | Fire escape landings have no window/door access | P1 | WP-2b |
| 12 | Architectural features don't consume Building Descriptor | P1 | WP-2b |
| 13 | All rooms 4-5x too small | P0 | WP-2c |
| 14 | No per-floor room assignment | P1 | WP-2c |
| 15 | No aspect ratio constraints | P1 | WP-2c |
| 16 | Missing building types | P2 | WP-2c |
| 17 | No minimum footprint validation | P1 | WP-2c |
| 18 | No post-generation validation | P2 | WP-3a |
| 19 | No capsule sweep test for doors | P2 | WP-3a |
| 20 | No NavMesh connectivity check | P2 | WP-3a |

---

## Reviewer Fixes Applied

The following changes were incorporated from Review #1 (Technical) and Review #2 (Player/Design):

### From Review #1 — File Conflict Resolution
- **R1-C1:** WP-1a, WP-1b, WP-1c all modify `MonolithMeshBuildingActions.cpp`. **Fix:** Merged into single agent (WP-1-BUILDING) for all BuildingActions changes. Separate agent (WP-1-FEATURES) handles ArchFeatureActions + TerrainActions changes. Two agents, not three.
- **R1-C2:** WP-2a and WP-2c both modify `MonolithMeshFloorPlanGenerator.cpp`. **Fix:** WP-2c runs first (room sizes), then WP-2a (door clearance) runs after. WP-2b (attachment) is independent.
- **R1-I3:** Phase 1→2 dependency relaxed — WP-2b (attachment context) can start alongside Phase 1 since it touches different files.
- **R1-I4:** Wall opening consumer — added `apply_wall_openings` step to `create_city_block` orchestrator in WP-2b.
- **R1-S2:** Multi-floor archetypes must include explicit stairwell entries with `min_area: 24` (4x6 cells).

### From Review #2 — Player/Design Fixes
- **R2-C1:** Boolean cutter Z extension clamped to `FloorZ` on upper floors (not `FloorZ - 3`) to prevent punching through ceiling below.
- **R2-C2:** Door meshes acknowledged as future work (not in this plan scope — buildings are blockout/greybox, door meshes are furnishing-level detail).
- **R2-C3:** Entrance priority faces south (not north) per project convention that building fronts face south.
- **R2-I1:** Corridor corners need 150cm turning radius for wheelchairs — add to hospice mode validation.
- **R2-I2:** Door width raised from 100cm to **110cm** for momentum-based FPS movement. Hospice stays at 120cm.
- **R2-I5:** Stairwell minimum 4x6 cells (24 grid cells) is a **hard reject** in floor plan generator — if footprint can't fit it, don't generate stairs.
- **R2-I6:** Per-building retry in orchestrator — if `generate_floor_plan` fails, retry with larger footprint or simpler archetype before skipping.
- **R2-S1:** Add `small_house` archetype (1BR, 1BA) for variety in residential blocks.
- **R2-S4:** Add `floor_height` field to archetype JSONs (default 270, commercial may be 300-400).
- **R2-S5:** Add `material_hints` to archetype JSONs for future material assignment.

### Revised Phase Structure (post-review)

```
Phase 1: Critical Geometry (2 agents, serialized for BuildingActions)
  WP-1-BUILDING: Stairs + stairwell cutouts + omit_exterior_walls + door width (BuildingActions.cpp + Types.h)
  WP-1-FEATURES: Fire escape stairs + ramp switchback (ArchFeatureActions.cpp + TerrainActions.cpp)

Phase 2: Logic + Sizes (WP-2c first, then WP-2a + WP-2b parallel)
  WP-2c: Room sizes + archetypes + per-floor rules (FloorPlanGenerator.cpp + JSONs)
  WP-2a: Door/corridor clearance + exterior entrance (FloorPlanGenerator.cpp) — after WP-2c
  WP-2b: Attachment context system (ArchFeatureActions.cpp + new helper) — parallel with WP-2c

Phase 3: Validation (after Phase 2)
  WP-3a: validate_building action (new file)
```

---

## Phase 1: Critical Geometry Fixes (2 Agents)

**REVISED:** WP-1a/1b/1c merged into WP-1-BUILDING (all BuildingActions.cpp changes) and WP-1-FEATURES (ArchFeature + Terrain changes). These two run in parallel.

### WP-1a: Stair and Ramp Geometry (Fixes #1, #2, #3)

**Agent:** `unreal-mesh-expert`
**Estimate:** 12-15 hours
**Research Docs:** `2026-03-28-ramp-stair-geometry-research.md`, `2026-03-28-stairwell-cutout-research.md`

#### Problem Summary

Three separate stair/ramp generators share the same conceptual bug: they compute step depth by dividing available space by step count, rather than using building-code tread depth and sizing the footprint to match.

**Current broken math (all three):**
```
StepDepth = AvailableSpace / StepCount  // e.g., 100cm / 15 = 6.7cm tread
Result: 70-degree angle (ladder!)
```

**Correct math:**
```
StepDepth = 28.0cm (IBC standard 11-inch tread)
StepCount = ceil(FloorHeight / 18.0)  // 18cm riser
RequiredRun = StepCount * StepDepth   // e.g., 15 * 28 = 420cm
If RequiredRun > AvailableSpace: use switchback (two half-flights)
```

#### Fix 1: Building Stairwell Stairs (Issues #1)

**File:** `Source/MonolithMesh/Private/MonolithMeshBuildingActions.cpp`
**Function:** `GenerateStairGeometry()` (line ~857-906)

**What to change:**

1. Replace the step depth calculation:
```cpp
// REMOVE: float StepDepth = StairDepth / static_cast<float>(StepCount);
// ADD:
const float TargetTreadDepth = 28.0f;  // IBC standard, 11 inches
const float TargetRiserHeight = 18.0f;
int32 StepCount = FMath::Max(2, FMath::RoundToInt32(FloorHeight / TargetRiserHeight));
float ActualRiser = FloorHeight / static_cast<float>(StepCount);
float RequiredRun = StepCount * TargetTreadDepth;
```

2. Implement switchback stair selection:
```cpp
float AvailableDepth = WorldMaxY - WorldMinY;  // stairwell depth in Y
float AvailableWidth = WorldMaxX - WorldMinX;  // stairwell width in X
float MinFlightWidth = 80.0f;  // minimum stair width per flight

if (AvailableDepth >= RequiredRun)
{
    // Straight stair fits
    GenerateStraightStair(Mesh, StepCount, TargetTreadDepth, ActualRiser,
        AvailableWidth, WorldMinX, WorldMinY, FloorZ);
}
else if (AvailableWidth >= MinFlightWidth * 2 + 10.0f)
{
    // Switchback: two half-flights side by side
    int32 HalfSteps = StepCount / 2;
    float HalfRun = HalfSteps * TargetTreadDepth;

    if (AvailableDepth >= HalfRun + 80.0f)  // half-run + landing
    {
        GenerateSwitchbackStair(Mesh, StepCount, TargetTreadDepth, ActualRiser,
            MinFlightWidth, WorldMinX, WorldMinY, WorldMaxX, WorldMaxY, FloorZ);
    }
    else
    {
        // Stairwell too small. Use steepest walkable angle (44 deg).
        float MaxTread = AvailableDepth / static_cast<float>(StepCount);
        UE_LOG(LogMonolithMesh, Warning,
            TEXT("Stairwell too small for standard stairs. Using %.1f deg angle. "
                 "Minimum footprint for 270cm floor: 200x300cm (4x6 cells)."),
            FMath::RadiansToDegrees(FMath::Atan2(ActualRiser, MaxTread)));
        // Fall back to current behavior but cap at walkable angle
        float CappedTread = ActualRiser / FMath::Tan(FMath::DegreesToRadians(44.0f));
        GenerateStraightStair(Mesh, StepCount, FMath::Max(MaxTread, CappedTread),
            ActualRiser, AvailableWidth, WorldMinX, WorldMinY, FloorZ);
    }
}
```

3. Add new helper functions:

**`GenerateStraightStair()`** -- Extract current `AppendLinearStairs` call into a named function with the corrected tread depth.

**`GenerateSwitchbackStair()`** -- New function:
```
Algorithm:
  HalfSteps = StepCount / 2
  FlightWidth = (AvailableWidth - 10) / 2  // 10cm gap between flights
  HalfRun = HalfSteps * TreadDepth
  LandingDepth = min(FlightWidth, AvailableDepth - HalfRun)  // at least flight width

  Flight 1: AppendLinearStairs at (MinX, MinY, FloorZ)
    - StepCount = HalfSteps
    - Width = FlightWidth, StepDepth = TreadDepth, StepHeight = Riser
    - Direction: +Y

  Landing: AppendBox at (MinX, MinY + HalfRun, FloorZ + HalfSteps * Riser)
    - Width = AvailableWidth, Depth = LandingDepth, Height = SlabThickness

  Flight 2: AppendLinearStairs at (MinX + FlightWidth + 10, MinY + HalfRun + LandingDepth, MidZ)
    - StepCount = StepCount - HalfSteps
    - Width = FlightWidth, StepDepth = TreadDepth, StepHeight = Riser
    - Direction: -Y (rotate 180 about Z)
```

#### Fix 2: Fire Escape Stairs (Issue #2)

**File:** `Source/MonolithMesh/Private/MonolithMeshArchFeatureActions.cpp`
**Function:** `CreateFireEscape()` (lines ~644-648)

**What to change:**

Replace step depth from landing-constrained to code-standard:
```cpp
// REMOVE: const float StepDepth = LandingD / static_cast<float>(StepsPerFlight);
// ADD:
const float StepDepth = 20.0f;  // 8 inches -- fire escape minimum (IBC 1011.5.2)
// Fire escape stairs are permitted steeper than interior (up to 45 deg at 8"/8")
```

Recalculate stair run length -- stairs now extend BEYOND the landing:
```cpp
float StairRunLength = StepDepth * StepsPerFlight;
// Stairs extend outward from the landing, not constrained to landing depth
```

Update stair geometry generation to position flights extending away from the building wall, with the landing as the connection point. Each flight runs from landing to landing, not within a single landing's footprint.

Add a switchback option: for fire escapes, adjacent flights run in opposite directions (standard zigzag fire escape pattern). The mid-level landing is offset laterally (in X) from the floor-level landings.

#### Fix 3: ADA Ramp Switchback Self-Intersection (Issue #3)

**File:** `Source/MonolithMesh/Private/MonolithMeshArchFeatureActions.cpp`
**Function:** `CreateRampConnector()` (lines ~860-1032)

**Also:** `Source/MonolithMesh/Private/MonolithMeshTerrainActions.cpp`
**Function:** `BuildADARampGeometry()` (lines ~1000-1105)

**What to change in both files:**

Add perpendicular offset calculation for switchback segments:
```cpp
// REMOVE: just reversing direction
// Direction = -Direction;  // This only flips Y, no X offset

// ADD: perpendicular lateral offset
FVector RightVector = FVector::CrossProduct(FVector::UpVector, RunDirection).GetSafeNormal();
float LateralGap = Width + 30.0f;  // ADA requires 30cm min gap for handrails

for (int32 Seg = 0; Seg < NumSegments; ++Seg)
{
    FVector SegDirection = (Seg % 2 == 0) ? RunDirection : -RunDirection;
    FVector LateralOffset = RightVector * (Seg * LateralGap);

    FVector SegStart = BasePosition + LateralOffset + FVector(0, 0, RisePerSeg * Seg);
    FVector SegEnd = SegStart + SegDirection * RunPerSeg + FVector(0, 0, RisePerSeg);

    // Generate ramp segment from SegStart to SegEnd
    // ...

    // Generate 180-degree landing connecting this segment to the next
    if (Seg < NumSegments - 1)
    {
        FVector LandingPos = SegEnd;
        FVector LandingSize = FVector(LateralGap, LandingLength, SlabThickness);
        // Landing spans from this run's end to next run's start
        AppendBox(Mesh, LandingSize, LandingPos, ...);
    }
}
```

Add headroom validation after generation:
```cpp
// Validate: vertical clearance between stacked runs >= 210cm
float VerticalClearance = RisePerSeg - RampThickness;
if (VerticalClearance < 210.0f)
{
    // This should never happen with proper lateral offset (runs are side-by-side,
    // not stacked), but log a warning for safety
    UE_LOG(LogMonolithMesh, Warning, TEXT("Ramp headroom clearance %.0fcm < 210cm minimum"), VerticalClearance);
}
```

#### Test Criteria

- [ ] Building stairwell stairs have angle between 28-35 degrees for standard 270cm floor height
- [ ] Fire escape stairs have angle <= 45 degrees
- [ ] ADA ramp switchback runs are side-by-side in plan view (no XY overlap)
- [ ] ADA ramp headroom >= 210cm at all points
- [ ] Stairwell with 4x6 cells (200x300cm) generates comfortable switchback stairs
- [ ] Stairwell with only 2x2 cells logs a warning and uses steepest walkable angle
- [ ] `AppendLinearStairs` slope <= 44.76 degrees (UE WalkableFloorAngle)
- [ ] Player character (42cm radius capsule) can walk up all generated stairs

---

### WP-1b: Window Cut-Through (Fix #4)

**Agent:** `unreal-mesh-expert`
**Estimate:** 6-8 hours
**Research Doc:** `2026-03-28-window-cutthrough-research.md`

#### Problem Summary

The facade system (`generate_facade`) creates its own wall slabs with window holes via boolean subtract. But the building mesh from `create_building_from_grid` retains solid exterior walls behind the facade. Two overlapping wall layers = windows appear as dark rectangles (the building wall behind the facade blocks light/visibility).

#### Solution: Option A -- Facade Replaces Building Exterior Walls

The building generator skips exterior wall geometry when a flag is set. The facade wall slab becomes the only exterior wall.

#### Implementation

**Step 1: Add `omit_exterior_walls` parameter to `create_building_from_grid`**

**File:** `Source/MonolithMesh/Private/MonolithMeshBuildingActions.cpp`

In `CreateBuildingFromGrid()` param parsing:
```cpp
bool bOmitExteriorWalls = false;
if (Params->HasField(TEXT("omit_exterior_walls")))
    bOmitExteriorWalls = Params->GetBoolField(TEXT("omit_exterior_walls"));
```

Pass this flag to `GenerateWallGeometry()`.

**Step 2: Skip exterior wall segments in `GenerateWallGeometry()`**

**File:** `Source/MonolithMesh/Private/MonolithMeshBuildingActions.cpp`
**Function:** `GenerateWallGeometry()` (line ~460)

In the loop that generates wall boxes from segments:
```cpp
for (const FWallSegment& Seg : Segments)
{
    // NEW: Skip exterior walls if facade will replace them
    if (bOmitExteriorWalls && Seg.bExterior)
    {
        // Still emit FExteriorFaceDef so the facade knows where to build
        // (this code already exists later in the function)
        continue;  // Skip the AppendBox call
    }

    // ... existing AppendBox wall geometry code ...
}
```

**Step 3: Verify facade wall positioning matches building walls**

**File:** `Source/MonolithMesh/Private/MonolithMeshFacadeActions.cpp`
**Function:** `BuildWallSlab()` (line ~265)

The facade's `BuildWallSlab()` uses `Face.WorldOrigin` and `Face.Normal` from `FExteriorFaceDef` -- the same data the building generator produces. Verify that:
- Wall thickness matches `ExteriorWallThickness` from the building descriptor
- Wall position is centered on the same plane as the building's exterior walls would be
- No Z offset mismatch between floors

**Step 4: Fix glass pane material assignment**

**File:** `Source/MonolithMesh/Private/MonolithMeshFacadeActions.cpp`
**Function:** `AddGlassPanes()` (line ~619)

The glass panes use MaterialID 0 (wall material). Change to use the facade style's `GlassMaterialId`:
```cpp
// Currently: AppendBox with default material
// Change to: use GlassMaterialId from the style
// The style struct already has GlassMaterialId -- just apply it to the pane's triangles
```

**Step 5: Return `omit_exterior_walls: true` in building descriptor**

When the flag is set, include it in the descriptor JSON so downstream actions know the building expects a facade.

#### Test Criteria

- [ ] With `omit_exterior_walls: true`, building mesh has no exterior wall geometry
- [ ] Facade wall slabs are positioned exactly where exterior walls would be (no gaps, no overlap)
- [ ] Boolean window cuts in facade reveal interior rooms (line-of-sight through windows)
- [ ] Glass panes use a translucent/glass material, not the wall material
- [ ] Building mesh WITHOUT the flag still generates complete exterior walls (backward compat)
- [ ] Multi-story buildings have aligned facade walls across floors
- [ ] L-shaped and non-rectangular buildings have correct facade wall placement at corners

---

### WP-1c: Stairwell Floor/Ceiling Cutouts (Fix #5)

**Agent:** `unreal-mesh-expert`
**Estimate:** 6-8 hours
**Research Doc:** `2026-03-28-stairwell-cutout-research.md`

#### Problem Summary

Stairwell cells (grid value -2) correctly suppress floor/ceiling slabs on the originating floor. But the destination floor has its own grid with no -2 cells at the stairwell position, so it generates a solid floor slab directly above the stairs. The player hits the ceiling.

#### Implementation

**File:** `Source/MonolithMesh/Private/MonolithMeshBuildingActions.cpp`
**Function:** `CreateBuildingFromGrid()` (line ~1130 area)

**Step 1: Parse per-floor grids (or propagate stairwell cells)**

Currently all floors share the same base grid. Add stairwell propagation BEFORE the per-floor geometry loop:

```cpp
// After parsing all floor plans and stairwells:

// Build per-floor grids (copy base grid, then overlay stairwells)
TArray<TArray<TArray<int32>>> PerFloorGrids;
for (int32 F = 0; F < NumFloors; ++F)
{
    // Start with this floor's grid (or the base grid if per-floor grids aren't specified)
    PerFloorGrids.Add(BaseGrid);  // deep copy
}

// Propagate stairwell cells to destination floors
for (const FStairwellDef& Stair : AllStairwells)
{
    // Mark cells on the originating floor (already done by caller, but ensure)
    if (Stair.ConnectsFloorA >= 0 && Stair.ConnectsFloorA < NumFloors)
    {
        auto& GridA = PerFloorGrids[Stair.ConnectsFloorA];
        for (const FIntPoint& Cell : Stair.GridCells)
        {
            if (Cell.Y >= 0 && Cell.Y < GridH && Cell.X >= 0 && Cell.X < GridW)
                GridA[Cell.Y][Cell.X] = -2;
        }
    }

    // Mark cells on the destination floor (THE FIX)
    if (Stair.ConnectsFloorB >= 0 && Stair.ConnectsFloorB < NumFloors)
    {
        auto& GridB = PerFloorGrids[Stair.ConnectsFloorB];
        for (const FIntPoint& Cell : Stair.GridCells)
        {
            if (Cell.Y >= 0 && Cell.Y < GridH && Cell.X >= 0 && Cell.X < GridW)
                GridB[Cell.Y][Cell.X] = -2;
        }
    }

    // For multi-floor stairwells (atriums), mark intermediate floors too
    for (int32 F = Stair.ConnectsFloorA + 1; F < Stair.ConnectsFloorB; ++F)
    {
        if (F >= 0 && F < NumFloors)
        {
            auto& GridMid = PerFloorGrids[F];
            for (const FIntPoint& Cell : Stair.GridCells)
            {
                if (Cell.Y >= 0 && Cell.Y < GridH && Cell.X >= 0 && Cell.X < GridW)
                    GridMid[Cell.Y][Cell.X] = -2;
            }
        }
    }
}
```

**Step 2: Use per-floor grids in the geometry loop**

In the per-floor geometry generation loop, replace `Grid` references with `PerFloorGrids[FloorIndex]`:

```cpp
for (int32 F = 0; F < NumFloors; ++F)
{
    const auto& FloorGrid = PerFloorGrids[F];  // Use per-floor grid

    // Build wall segments from FloorGrid (not base Grid)
    auto WallSegs = BuildWallSegments(FloorGrid, GridW, GridH, FloorDoors);

    GenerateWallGeometry(Mesh, WallSegs, ...);
    GenerateSlabs(Mesh, FloorGrid, ...);        // Floor slab: skip -2 cells
    GenerateSlabs(Mesh, FloorGrid, ...);        // Ceiling slab: skip -2 cells
    // ...
}
```

**Step 3: Generate stairwell enclosure walls on upper floor**

When stairwell cells are marked on the upper floor, the wall segment builder will naturally detect boundaries between -2 cells and room cells. These produce wall segments. However, the enclosure walls should be:
- Full-height walls for enclosed stairwells (default)
- Half-height walls/railings for open stairwells

Add a parameter to `FStairwellDef`:
```cpp
// In MonolithMeshBuildingTypes.h:
struct FStairwellDef
{
    // ... existing fields ...

    // NEW: stairwell behavior
    FString StairType = TEXT("auto");  // "auto", "straight", "switchback"
    bool bEnclosed = true;             // Full walls vs open railing on upper floor
};
```

Parse the new fields in `ParseStairwells()`.

For enclosed stairwells, the existing wall segment builder handles it. For open stairwells, generate half-height walls (90cm) using `AppendBox` at the stairwell perimeter on the upper floor.

**Step 4: Verify headroom**

Add a runtime check after stairwell propagation:
```cpp
// At any point on the stair, headroom = distance to next floor's ceiling
// With ceiling slab suppressed, headroom = FloorHeight (270cm typical) -- well above 203cm minimum
// Log warning if FloorHeight < 220cm (player height + margin)
if (FloorHeight < 220.0f)
{
    UE_LOG(LogMonolithMesh, Warning,
        TEXT("Floor height %.0fcm may have insufficient headroom for stairwell"), FloorHeight);
}
```

#### Test Criteria

- [ ] Upper floor has no floor slab at stairwell cell positions
- [ ] Player can walk up stairs without hitting ceiling
- [ ] Stairwell cells on upper floor have enclosure walls to prevent falling
- [ ] Multi-floor stairwells (e.g., 3+ stories) suppress slabs on all intermediate floors
- [ ] Stairwell on floor 0->1 does NOT suppress floor 2's slab (unless explicitly defined)
- [ ] Building descriptor JSON includes stairwell cells per floor in the output

---

## Phase 2: Logic and Size Fixes (Parallel, after Phase 1)

Phase 1 must complete first because WP-2a depends on WP-1c (stairwell changes affect the grid system), and WP-2c depends on understanding the corrected geometry sizes. However, all three Phase 2 work packages are independent of each other.

### WP-2a: Door, Corridor, and Entrance Fixes (Fixes #6, #7, #8)

**Agent:** `unreal-mesh-expert`
**Estimate:** 6-8 hours
**Research Doc:** `2026-03-28-door-clearance-research.md`

#### Fix 1: Widen Default Door Width (Issue #7)

**File:** `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp`
**Function:** `PlaceDoors()` (line ~922 area)

```cpp
// CHANGE:
float DoorWidth = bHospiceMode ? 100.0f : 90.0f;
// TO:
float DoorWidth = bHospiceMode ? 120.0f : 100.0f;
```

Rationale: 100cm gives 8cm clearance per side for the 84cm capsule. Hospice mode at 120cm gives 18cm per side for wheelchair comfort.

Also update the door height default if it's hardcoded anywhere to ensure 220cm:
```cpp
float DoorHeight = 220.0f;  // Verify this is the default
```

#### Fix 2: Widen Normal-Mode Corridors (Issue #8)

**File:** `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp`
**Function:** `InsertCorridors()` (line ~709 area)

```cpp
// CHANGE:
int32 CorridorWidth = bHospiceMode ? 4 : 2;
// TO:
int32 CorridorWidth = bHospiceMode ? 4 : 3;  // 3 * 50cm = 150cm
```

This gives 33cm clearance per side for the 84cm capsule in normal mode. Hospice mode (200cm) remains unchanged.

#### Fix 3: Guarantee Exterior Entrance (Issue #6)

**File:** `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp`
**Function:** `PlaceDoors()` or new function called from `GenerateFloorPlan()`

Add exterior entrance placement after room placement, before regular door placement:

```cpp
void EnsureExteriorEntrance(TArray<TArray<int32>>& Grid, int32 GridW, int32 GridH,
    TArray<FRoomDef>& Rooms, TArray<FDoorDef>& Doors, bool bHospiceMode, FRandomStream& Rng)
{
    // 1. Find rooms that touch the building perimeter
    TArray<int32> PerimeterRoomIndices;
    for (int32 RoomIdx = 0; RoomIdx < Rooms.Num(); ++RoomIdx)
    {
        for (const FIntPoint& Cell : Rooms[RoomIdx].GridCells)
        {
            if (Cell.X == 0 || Cell.X == GridW - 1 || Cell.Y == 0 || Cell.Y == GridH - 1)
            {
                PerimeterRoomIndices.AddUnique(RoomIdx);
                break;
            }
        }
    }

    if (PerimeterRoomIndices.IsEmpty())
    {
        UE_LOG(LogMonolithMesh, Warning, TEXT("No rooms touch building perimeter -- cannot place entrance"));
        return;
    }

    // 2. Prefer corridor, lobby, foyer, entryway types
    int32 BestRoomIdx = -1;
    for (int32 Idx : PerimeterRoomIndices)
    {
        const FString& Type = Rooms[Idx].RoomType;
        if (Type == TEXT("lobby") || Type == TEXT("corridor") || Type == TEXT("foyer") || Type == TEXT("entryway"))
        {
            BestRoomIdx = Idx;
            break;
        }
    }

    // 3. Fallback: largest perimeter room
    if (BestRoomIdx < 0)
    {
        int32 MaxCells = 0;
        for (int32 Idx : PerimeterRoomIndices)
        {
            if (Rooms[Idx].GridCells.Num() > MaxCells)
            {
                MaxCells = Rooms[Idx].GridCells.Num();
                BestRoomIdx = Idx;
            }
        }
    }

    // 4. Find best perimeter cell on preferred face (south > east > west > north)
    FIntPoint BestCell(-1, -1);
    FString BestWall;
    // Priority: Y == GridH-1 (south in our convention), X == GridW-1 (east), etc.
    for (const FIntPoint& Cell : Rooms[BestRoomIdx].GridCells)
    {
        if (Cell.Y == 0 && BestWall.IsEmpty()) { BestCell = Cell; BestWall = TEXT("south"); }
        if (Cell.Y == GridH - 1) { BestCell = Cell; BestWall = TEXT("north"); break; }
        if (Cell.X == 0 && BestWall != TEXT("north")) { BestCell = Cell; BestWall = TEXT("west"); }
        if (Cell.X == GridW - 1 && BestWall != TEXT("north")) { BestCell = Cell; BestWall = TEXT("east"); }
    }

    // 5. Create exterior door
    FDoorDef EntDoor;
    EntDoor.DoorId = TEXT("entrance_01");
    EntDoor.RoomA = Rooms[BestRoomIdx].RoomId;
    EntDoor.RoomB = TEXT("exterior");
    EntDoor.EdgeStart = BestCell;
    EntDoor.EdgeEnd = BestCell;
    EntDoor.Wall = BestWall;
    EntDoor.Width = bHospiceMode ? 140.0f : 110.0f;  // Wider than interior doors
    EntDoor.Height = 240.0f;  // Taller exterior entrance
    EntDoor.bTraversable = true;
    Doors.Add(EntDoor);
}
```

The exterior door must also be boolean-subtracted from the building's exterior wall. In `CutDoorOpenings()`, handle doors where `RoomB == "exterior"` by positioning the cutter on the building's perimeter wall.

#### Fix 4: Add Clearance Validation Pass

After `PlaceDoors()`, add a validation pass that checks each door width against the player capsule:

```cpp
void ValidateDoorClearance(TArray<FDoorDef>& Doors, float CapsuleRadius)
{
    const float MinWidth = CapsuleRadius * 2 + 16.0f;  // 84 + 16 = 100cm

    for (FDoorDef& Door : Doors)
    {
        if (Door.Width < MinWidth && Door.bTraversable)
        {
            UE_LOG(LogMonolithMesh, Warning,
                TEXT("Door %s width %.0fcm < minimum %.0fcm. Widening."),
                *Door.DoorId, Door.Width, MinWidth);
            Door.Width = MinWidth;
        }
    }
}
```

#### Test Criteria

- [ ] All doors are >= 100cm wide (normal) or >= 120cm (hospice)
- [ ] All corridors are >= 150cm wide (normal) or >= 200cm (hospice)
- [ ] Every building has at least one exterior entrance door
- [ ] Exterior entrance is on a perimeter room, preferring lobby/foyer types
- [ ] Exterior entrance door is boolean-cut from the building's exterior wall
- [ ] Player (42cm radius capsule) can pass through all doors at 45-degree approach angle

---

### WP-2b: Attachment Context System (Fixes #9, #10, #11, #12)

**Agent:** `unreal-mesh-expert`
**Estimate:** 10-12 hours
**Research Doc:** `2026-03-28-attachment-logic-research.md`

#### Problem Summary

The 5 architectural feature actions (`create_balcony`, `create_porch`, `create_fire_escape`, `create_ramp_connector`, `create_railing`) generate standalone geometry with no awareness of the building they attach to. This causes: balconies facing wrong direction, porches with no door, fire escapes with no window access, and features that don't consume the building descriptor.

#### Implementation

**Step 1: Add `FAttachmentContext` struct**

**File:** `Source/MonolithMesh/Public/MonolithMeshBuildingTypes.h` (append to end)

```cpp
/** Attachment context for architectural features -- derived from FExteriorFaceDef */
struct FAttachmentContext
{
    FVector WallOrigin = FVector::ZeroVector;
    FVector WallNormal = FVector::ZeroVector;
    float WallWidth = 0.0f;
    float WallHeight = 0.0f;
    float WallThickness = 15.0f;
    float FloorHeight = 270.0f;
    int32 FloorIndex = 0;
    FString BuildingId;

    // Derived:
    FVector WallRight = FVector::ZeroVector;    // Along-wall direction
    FTransform WallToWorld = FTransform::Identity;

    bool bValid = false;

    void ComputeDerived()
    {
        WallRight = FVector::CrossProduct(FVector::UpVector, WallNormal).GetSafeNormal();
        FMatrix M = FMatrix::Identity;
        M.SetAxis(0, WallRight);
        M.SetAxis(1, WallNormal);
        M.SetAxis(2, FVector::UpVector);
        M.SetOrigin(WallOrigin);
        WallToWorld = FTransform(M);
        bValid = !WallNormal.IsNearlyZero();
    }
};

/** Wall opening request -- returned by features that need access through the building wall */
struct FWallOpeningRequest
{
    FString BuildingId;
    FString Wall;
    int32 FloorIndex = 0;
    float PositionAlongWall = 0.0f;  // cm from wall left edge
    float Width = 100.0f;
    float Height = 220.0f;
    float SillHeight = 0.0f;
    FString Type = TEXT("door");  // "door", "window", "french_door"

    TSharedPtr<FJsonObject> ToJson() const
    {
        auto J = MakeShared<FJsonObject>();
        J->SetStringField(TEXT("building_id"), BuildingId);
        J->SetStringField(TEXT("wall"), Wall);
        J->SetNumberField(TEXT("floor_index"), FloorIndex);
        J->SetNumberField(TEXT("position_along_wall"), PositionAlongWall);
        J->SetNumberField(TEXT("width"), Width);
        J->SetNumberField(TEXT("height"), Height);
        J->SetNumberField(TEXT("sill_height"), SillHeight);
        J->SetStringField(TEXT("type"), Type);
        return J;
    }
};
```

**Step 2: Add attachment context parsing helper**

**File:** `Source/MonolithMesh/Private/MonolithMeshArchFeatureActions.cpp`

```cpp
static FAttachmentContext ParseAttachmentContext(const TSharedPtr<FJsonObject>& Params)
{
    FAttachmentContext Ctx;

    // Option A: Explicit wall params
    if (Params->HasField(TEXT("wall_normal")))
    {
        const TArray<TSharedPtr<FJsonValue>>* NormalArr;
        if (Params->TryGetArrayField(TEXT("wall_normal"), NormalArr) && NormalArr->Num() >= 3)
        {
            Ctx.WallNormal.X = (*NormalArr)[0]->AsNumber();
            Ctx.WallNormal.Y = (*NormalArr)[1]->AsNumber();
            Ctx.WallNormal.Z = (*NormalArr)[2]->AsNumber();
        }
        // Parse wall_origin similarly
        // Parse wall_thickness, wall_width, wall_height, floor_height, floor_index
        Ctx.ComputeDerived();
    }
    // Option B: Building descriptor reference (future -- look up from spatial registry)
    // else if (Params->HasField(TEXT("building_id"))) { ... }

    return Ctx;
}
```

**Step 3: Update `CreateBalcony()` to use attachment context (Fix #9)**

When `FAttachmentContext.bValid`, transform all local geometry through `Ctx.WallToWorld` instead of using `location` + `rotation`. The balcony slab extending in local +Y automatically faces the wall normal direction.

Also emit a `wall_openings` array in the result JSON with a french door opening:
```json
"wall_openings": [{
    "type": "french_door",
    "width": 160,
    "height": 220,
    "sill_height": 0,
    "position_along_wall": <centered on balcony width>
}]
```

**Step 4: Update `CreatePorch()` to cut door opening (Fix #10)**

When attachment context is provided, emit a `wall_openings` entry for a standard door:
```json
"wall_openings": [{
    "type": "door",
    "width": 110,
    "height": 220,
    "sill_height": 0
}]
```

Also align porch floor Z to the building floor level from `Ctx.FloorHeight * Ctx.FloorIndex`.

**Step 5: Update `CreateFireEscape()` to emit window openings (Fix #11)**

For each landing, emit a window opening request:
```json
"wall_openings": [{
    "type": "window",
    "width": 70,
    "height": 100,
    "sill_height": 60,
    "floor_index": <landing floor>
}]
```

**Step 6: Backward Compatibility**

If neither `wall_normal` nor `building_id` is provided, fall back to the existing `location` + `rotation` behavior. Existing MCP call sites are unaffected.

#### Test Criteria

- [ ] Balcony with `wall_normal: [0, -1, 0]` faces south automatically (no manual rotation)
- [ ] Balcony result JSON includes `wall_openings` with french_door entry
- [ ] Porch result JSON includes `wall_openings` with door entry
- [ ] Fire escape result JSON includes `wall_openings` with per-landing window entries
- [ ] All features without attachment context work exactly as before (backward compat)
- [ ] `FAttachmentContext` correctly transforms geometry for all 4 cardinal wall directions

---

### WP-2c: Room Sizes, Archetypes, and Floor Assignment (Fixes #13, #14, #15, #16, #17)

**Agent:** `unreal-mesh-expert`
**Estimate:** 12-15 hours
**Research Doc:** `2026-03-28-realistic-room-sizes-research.md`

#### Problem Summary

All rooms are approximately 4-5x too small. The archetype JSONs use area values that appear to treat 50cm grid cells as 1m^2 cells when they're actually 0.25m^2 each. Additionally, there's no per-floor room assignment (lobby should only be on ground floor), no aspect ratio constraints, and missing building types.

#### Fix 1: Write 9 New Archetype JSONs (Issue #13, #16)

**Location:** Archetype JSON directory (find via `GetArchetypeDirectory()` -- likely `Plugins/Monolith/Content/BuildingArchetypes/` or similar)

Replace existing undersized archetypes and add missing types. Each archetype must use corrected cell counts based on `real_world_m2 * 4 = grid_cells`.

**9 Archetype JSONs to create/replace:**

1. **`residential_house.json`** -- 3BR house, 1-2 floors
   - Living room: 72-120 cells (18-30 m^2)
   - Kitchen: 48-80 cells
   - Master bedroom: 72-112 cells
   - Bedrooms: 40-72 cells
   - Bathroom: 20-32 cells
   - Min footprint: 20x24 grid (10x12m)

2. **`clinic.json`** -- Small medical clinic, 1 floor
   - Waiting room: 80-160 cells (20-40 m^2)
   - Exam rooms: 36-48 cells, count 2-4
   - Min footprint: 24x30 grid (12x15m)

3. **`office_building.json`** -- Multi-floor office, 3-10 floors
   - Ground: lobby 120-280, reception, elevators, stairwells
   - Upper: open office 200-480, private offices, conference rooms
   - Top: executive offices, boardroom
   - Per-floor mandatory: elevator, stairwell, restrooms M+F, utility closet
   - Min footprint: 30x40 grid (15x20m)

4. **`police_station.json`** -- 1-2 floors
   - Lobby: 120-200 cells
   - Bullpen: 200-400 cells
   - Holding cells: 20-28 cells, count 2-4
   - Interrogation: 40-56 cells
   - Min footprint: 30x50 grid (15x25m)

5. **`apartment_building.json`** -- Multi-unit, 3-6 floors
   - Common areas: lobby, mailbox, laundry, stairwell, elevator
   - Per-unit: living 56-88, kitchen 28-48, bedroom 40-64, bathroom 18-28
   - Min footprint: 24x60 grid (12x30m)

6. **`restaurant.json`** -- Single floor
   - Dining: 200-480 cells (50-120 m^2)
   - Kitchen: 100-180 cells
   - Bar area: 72-120 cells
   - Min footprint: 20x36 grid (10x18m)

7. **`warehouse.json`** -- Single floor, high ceiling
   - Main floor: 1200-3200 cells (300-800 m^2)
   - Loading dock: 160-240 cells
   - Office cluster: 40-72 cells
   - Min footprint: 40x60 grid (20x30m)

8. **`church.json`** -- 1-2 floors
   - Nave: 400-1000 cells (100-250 m^2)
   - Altar/chancel: 80-160 cells
   - Vestibule/narthex: 96-200 cells
   - Min footprint: 24x50 grid (12x25m)

9. **`school.json`** -- 1-3 floors
   - Classrooms: 260-340 cells (65-85 m^2), count 4-8
   - Gymnasium: 1400-2200 cells (ground only)
   - Cafeteria: 600-1000 cells (ground only)
   - Min footprint: 40x80 grid (20x40m)

**Important:** Each archetype JSON must include corrected cell counts per the room sizes research. Full archetype definitions with adjacency rules, floor assignments, and aspect ratios are provided in the research doc.

#### Fix 2: Add Per-Floor Room Assignment (Issue #14)

**File:** `Source/MonolithMesh/Public/MonolithMeshFloorPlanGenerator.h`

Extend `FArchetypeRoom` struct:
```cpp
struct FArchetypeRoom
{
    // ... existing fields ...
    FString FloorAssignment = TEXT("any");  // "ground", "upper", "top", "every", "any"
    bool bFixedPosition = false;            // Must occupy same grid cells on every floor
    float MinAspect = 1.0f;                 // Minimum aspect ratio
    float MaxAspect = 5.0f;                 // Maximum aspect ratio (default 5:1)
};
```

**File:** `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp`

In `ParseArchetypeJson()`, parse the new fields:
```cpp
if (RoomObj->HasField(TEXT("floor")))
    Room.FloorAssignment = RoomObj->GetStringField(TEXT("floor"));
if (RoomObj->HasField(TEXT("fixed_position")))
    Room.bFixedPosition = RoomObj->GetBoolField(TEXT("fixed_position"));
if (RoomObj->HasField(TEXT("min_aspect")))
    Room.MinAspect = RoomObj->GetNumberField(TEXT("min_aspect"));
if (RoomObj->HasField(TEXT("max_aspect")))
    Room.MaxAspect = RoomObj->GetNumberField(TEXT("max_aspect"));
```

In `ResolveRoomInstances()`, filter rooms by floor:
```cpp
// When generating floor F's room list:
for (const FArchetypeRoom& AR : Archetype.Rooms)
{
    bool bInclude = false;
    if (AR.FloorAssignment == TEXT("any")) bInclude = true;
    else if (AR.FloorAssignment == TEXT("every")) bInclude = true;
    else if (AR.FloorAssignment == TEXT("ground") && FloorIndex == 0) bInclude = true;
    else if (AR.FloorAssignment == TEXT("upper") && FloorIndex > 0 && FloorIndex < NumFloors - 1) bInclude = true;
    else if (AR.FloorAssignment == TEXT("top") && FloorIndex == NumFloors - 1) bInclude = true;

    if (bInclude) { /* add to this floor's room instances */ }
}
```

For `bFixedPosition` rooms (stairwells, elevators), store the grid cells from floor 0 and force the same cells on all subsequent floors.

#### Fix 3: Add Aspect Ratio Constraints (Issue #15)

**File:** `Source/MonolithMesh/Private/MonolithMeshFloorPlanGenerator.cpp`

After treemap layout produces `FGridRect` results, add an aspect ratio correction pass:

```cpp
void CorrectAspectRatios(TArray<FGridRect>& Rects, const TArray<FRoomInstance>& Instances,
    const TArray<FArchetypeRoom>& ArchetypeRooms)
{
    for (int32 i = 0; i < Rects.Num(); ++i)
    {
        float Aspect = Rects[i].AspectRatio();
        float MaxAspect = 5.0f;  // default

        // Find the archetype room definition for this instance
        for (const FArchetypeRoom& AR : ArchetypeRooms)
        {
            if (AR.Type == Instances[i].RoomType)
            {
                MaxAspect = AR.MaxAspect;
                break;
            }
        }

        if (Aspect > MaxAspect)
        {
            // Room is too elongated. Reduce the longer dimension.
            if (Rects[i].W > Rects[i].H)
            {
                int32 NewW = FMath::Max(1, FMath::RoundToInt32(Rects[i].H * MaxAspect));
                Rects[i].W = FMath::Min(Rects[i].W, NewW);
            }
            else
            {
                int32 NewH = FMath::Max(1, FMath::RoundToInt32(Rects[i].W * MaxAspect));
                Rects[i].H = FMath::Min(Rects[i].H, NewH);
            }
        }
    }
}
```

#### Fix 4: Add Minimum Footprint Validation (Issue #17)

At the start of `GenerateFloorPlan()`, after loading the archetype, validate that the requested footprint can accommodate the required rooms:

```cpp
// Calculate total required area (sum of min_area for all required rooms)
float TotalRequiredCells = 0.0f;
for (const FArchetypeRoom& AR : Archetype.Rooms)
{
    if (AR.bRequired)
    {
        TotalRequiredCells += AR.MinArea * AR.CountMin;
    }
}

float AvailableCells = GridW * GridH;
float CorridorOverhead = 1.25f;  // Corridors typically consume ~25% of floor area

if (TotalRequiredCells * CorridorOverhead > AvailableCells)
{
    return FMonolithActionResult::Error(FString::Printf(
        TEXT("Footprint %dx%d (%d cells) too small for archetype '%s'. "
             "Required: %.0f cells (with corridor overhead). "
             "Minimum footprint: %.0fx%.0f meters."),
        GridW, GridH, GridW * GridH,
        *Archetype.Name,
        TotalRequiredCells * CorridorOverhead,
        FMath::Sqrt(TotalRequiredCells * CorridorOverhead * 0.25f),
        FMath::Sqrt(TotalRequiredCells * CorridorOverhead * 0.25f)));
}
```

#### Test Criteria

- [ ] Residential house living room generates at 18-30 m^2 (72-120 cells), not 5 m^2
- [ ] All 9 archetype JSONs load and generate valid buildings
- [ ] Lobby room only appears on ground floor
- [ ] Stairwell occupies same grid position on every floor
- [ ] Restrooms appear on every floor of office building
- [ ] Bathroom rooms have aspect ratio <= 1.5:1
- [ ] Open office rooms have aspect ratio <= 1.5:1
- [ ] Corridors have aspect ratio >= 3:1
- [ ] Footprint validation rejects a 5x5 grid for "office_building" archetype with clear error message
- [ ] Calling `list_building_archetypes` returns all 9 new types

---

## Phase 3: Validation System (After Phase 2)

### WP-3a: `validate_building` Action (Fixes #18, #19, #20)

**Agent:** `unreal-mesh-expert`
**Estimate:** 10-12 hours
**Research Doc:** `2026-03-28-procgen-validation-research.md`

#### Overview

Add a new `mesh_query` action `validate_building` that performs a 3-tier validation on a generated building. This is a read-only diagnostic action -- it does not modify geometry.

#### Implementation

**New File:** `Source/MonolithMesh/Private/MonolithMeshValidationActions.cpp`
**New File:** `Source/MonolithMesh/Public/MonolithMeshValidationActions.h`

Register as a new action class with a single action: `validate_building`.

**Input:**
```json
{
    "action": "validate_building",
    "params": {
        "building_id": "house_01",
        "actor_name": "StaticMeshActor_52",
        "capsule_radius": 42,
        "capsule_half_height": 96,
        "tier": 2
    }
}
```

`tier` controls validation depth:
- **Tier 1 (fast, <100ms):** Geometric checks only -- no physics required
- **Tier 2 (medium, 1-5s):** Capsule sweeps + connectivity graph
- **Tier 3 (slow, 5-30s):** NavMesh build + full path validation

#### Tier 1: Geometric Validation

```cpp
struct FValidationResult
{
    bool bPass = true;
    TArray<FString> Errors;    // CRITICAL -- blocks gameplay
    TArray<FString> Warnings;  // Should fix
    TArray<FString> Info;      // Nice to know
};

FValidationResult ValidateTier1(const FBuildingDescriptor& Desc)
{
    FValidationResult Result;

    // 1. Door width check
    for (const FFloorPlan& Floor : Desc.Floors)
    {
        for (const FDoorDef& Door : Floor.Doors)
        {
            float MinWidth = CapsuleRadius * 2 + 16.0f;
            if (Door.Width < MinWidth && Door.bTraversable)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Door %s width %.0fcm < minimum %.0fcm for %.0fcm capsule"),
                    *Door.DoorId, Door.Width, MinWidth, CapsuleRadius * 2));
                Result.bPass = false;
            }
        }
    }

    // 2. Corridor width check (compute from grid)
    // Walk all corridor cells, check minimum dimension

    // 3. Exterior entrance check
    bool bHasEntrance = false;
    for (const FFloorPlan& Floor : Desc.Floors)
    {
        for (const FDoorDef& Door : Floor.Doors)
        {
            if (Door.RoomB == TEXT("exterior") || Door.RoomA == TEXT("exterior"))
            {
                bHasEntrance = true;
                break;
            }
        }
        if (bHasEntrance) break;
    }
    if (!bHasEntrance)
    {
        Result.Errors.Add(TEXT("Building has no exterior entrance door"));
        Result.bPass = false;
    }

    // 4. Room connectivity (BFS from entrance through door graph)
    // Build adjacency graph from doors, flood-fill from entrance room
    // Any unreachable room = error

    // 5. Stairwell continuity check
    // Every stairwell should have -2 cells on both connected floors

    // 6. Stair angle check (if geometry data available)
    // Compute angle from floor height / stairwell depth

    return Result;
}
```

#### Tier 2: Capsule Sweep Validation (Fix #19)

Requires the building mesh to have collision generated.

```cpp
FValidationResult ValidateTier2(UWorld* World, const FBuildingDescriptor& Desc,
    float CapsuleRadius, float CapsuleHalfHeight)
{
    FValidationResult Result = ValidateTier1(Desc);

    FCollisionShape Capsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);

    for (const FFloorPlan& Floor : Desc.Floors)
    {
        for (const FDoorDef& Door : Floor.Doors)
        {
            if (!Door.bTraversable) continue;

            // Sweep capsule through each door opening
            FVector DoorCenter = Door.WorldPosition + FVector(0, 0, CapsuleHalfHeight);
            FVector Normal = GetDoorNormal(Door);  // derive from Door.Wall

            FVector SweepStart = DoorCenter - Normal * 80.0f;
            FVector SweepEnd = DoorCenter + Normal * 80.0f;

            FHitResult Hit;
            bool bBlocked = World->SweepSingleByChannel(
                Hit, SweepStart, SweepEnd, FQuat::Identity, ECC_Pawn, Capsule);

            if (bBlocked)
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Door %s BLOCKED: capsule sweep hit %s at (%.0f, %.0f, %.0f)"),
                    *Door.DoorId,
                    *Hit.GetActor()->GetName(),
                    Hit.ImpactPoint.X, Hit.ImpactPoint.Y, Hit.ImpactPoint.Z));
                Result.bPass = false;
            }

            // Also test 45-degree approach angles
            for (float Angle : {-45.0f, -30.0f, 30.0f, 45.0f})
            {
                FVector RotStart = DoorCenter + Normal.RotateAngleAxis(Angle, FVector::UpVector) * -80.0f;
                bBlocked = World->SweepSingleByChannel(
                    Hit, RotStart, SweepEnd, FQuat::Identity, ECC_Pawn, Capsule);
                if (bBlocked)
                {
                    Result.Warnings.Add(FString::Printf(
                        TEXT("Door %s partially blocked at %.0f degree approach"),
                        *Door.DoorId, Angle));
                }
            }
        }
    }

    // Window line-of-sight test
    // For each window, ray-cast from interior to exterior
    // If blocked, the boolean likely failed

    return Result;
}
```

#### Tier 3: NavMesh Connectivity (Fix #20)

```cpp
FValidationResult ValidateTier3(UWorld* World, const FBuildingDescriptor& Desc)
{
    FValidationResult Result = ValidateTier2(World, Desc, 42.0f, 96.0f);

    // Trigger navmesh rebuild around the building
    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetNavigationSystem(World);
    if (!NavSys) return Result;

    // For each room, try to find a nav path from entrance to room center
    FVector EntrancePos = FindEntrancePosition(Desc);

    for (const FFloorPlan& Floor : Desc.Floors)
    {
        for (const FRoomDef& Room : Floor.Rooms)
        {
            FVector RoomCenter = Room.WorldBounds.GetCenter();
            FPathFindingQuery Query(nullptr, *NavSys->GetDefaultNavDataInstance(),
                EntrancePos, RoomCenter);
            FPathFindingResult PathResult = NavSys->FindPathSync(Query);

            if (!PathResult.IsSuccessful())
            {
                Result.Errors.Add(FString::Printf(
                    TEXT("Room %s (floor %d) UNREACHABLE via NavMesh from entrance"),
                    *Room.RoomId, Floor.FloorIndex));
                Result.bPass = false;
            }
        }
    }

    return Result;
}
```

#### Output Format

```json
{
    "action": "validate_building",
    "result": {
        "pass": false,
        "tier_validated": 2,
        "errors": [
            "Door entrance_01 BLOCKED: capsule sweep hit SM_Building_01 at (100, 200, 96)",
            "Room storage_1 (floor 1) unreachable from entrance"
        ],
        "warnings": [
            "Door door_03 partially blocked at 45 degree approach"
        ],
        "info": [
            "16 doors validated, 14 pass, 2 fail",
            "8 rooms validated, 7 reachable, 1 unreachable"
        ],
        "stats": {
            "total_doors": 16,
            "passable_doors": 14,
            "blocked_doors": 2,
            "total_rooms": 8,
            "reachable_rooms": 7,
            "unreachable_rooms": 1,
            "validation_time_ms": 2340
        }
    }
}
```

#### Registration

Register in the mesh_query namespace alongside existing building actions:
```cpp
Registry.RegisterAction(TEXT("mesh"), TEXT("validate_building"), ...);
```

#### Test Criteria

- [ ] Tier 1: Detects doors narrower than capsule diameter + buffer
- [ ] Tier 1: Detects buildings with no exterior entrance
- [ ] Tier 1: Detects disconnected rooms (unreachable via door graph)
- [ ] Tier 2: Capsule sweep correctly identifies blocked doors
- [ ] Tier 2: Detects doors that are passable head-on but blocked at 45-degree approach
- [ ] Tier 3: NavMesh pathfinding identifies rooms unreachable due to geometry issues
- [ ] Validation of a correctly-generated building returns `pass: true`
- [ ] Action registered and discoverable via `monolith_discover("mesh")`

---

## Phase 4: Future Improvements (Optional, Not Blocking)

These are documented for future work but NOT part of the current fix plan. They came from the advanced procgen research.

### Algorithm Upgrades (from `2026-03-28-advanced-procgen-research.md`)

| Improvement | Description | Priority | Effort |
|-------------|------------|----------|--------|
| Constrained Growth | Replace treemap with constrained growth for better room shapes | Medium | 20h |
| Space Syntax Evaluation | Integration/connectivity metrics for generated layouts | Low | 8h |
| WFC Interior Detail | Use Driven WFC for furniture/debris placement within rooms | Medium | 15h |
| ASP Constraint Solver | Use Answer Set Programming for topology validation | Low | 12h |
| Voronoi Rooms | Organic room shapes for horror/corrupted building areas | Low | 10h |
| Per-Room Height | Support variable ceiling heights (double-height lobbies, warehouses) | Medium | 8h |
| Security Zones | Prevent adjacency between public/secure areas (police station) | Medium | 6h |

### Horror-Specific Features

| Feature | Description | Priority |
|---------|-------------|----------|
| Decay parameter | Randomly remove/tilt geometry for abandoned buildings | Medium |
| Non-Euclidean rooms | Rooms slightly larger/smaller inside than outside | Low |
| Sound propagation hints | Tag rooms with acoustic properties for reverb zones | Medium |
| Sight-line analysis | Ensure horror encounters have limited sightlines | Low |

---

## Execution Summary

| Phase | Work Packages | Parallel? | Total Estimate |
|-------|--------------|-----------|----------------|
| Phase 1 | WP-1a, WP-1b, WP-1c | Yes (all 3) | 24-31h |
| Phase 2 | WP-2a, WP-2b, WP-2c | Yes (all 3) | 28-35h |
| Phase 3 | WP-3a | Sequential | 10-12h |
| **Total** | **7 work packages** | | **~62-78h** |

**Critical path:** Phase 1 (24-31h parallel) -> Phase 2 (28-35h parallel) -> Phase 3 (10-12h) = **~62-78h elapsed if fully parallelized across 3 agents**.

**Minimum viable fix (P0 issues only):** WP-1a + WP-1b + WP-1c + Fix #13 from WP-2c = ~30-40h, addresses the 6 most critical issues.

---

## Files Modified Summary

| File | Work Packages | Changes |
|------|--------------|---------|
| `MonolithMeshBuildingActions.cpp` | WP-1a, WP-1b, WP-1c | Stair geometry, exterior wall suppression, per-floor grids |
| `MonolithMeshBuildingActions.h` | WP-1c | Per-floor grid support |
| `MonolithMeshBuildingTypes.h` | WP-1c, WP-2b | FStairwellDef extensions, FAttachmentContext, FWallOpeningRequest |
| `MonolithMeshArchFeatureActions.cpp` | WP-1a, WP-2b | Fire escape stairs, ramp switchback, attachment context |
| `MonolithMeshTerrainActions.cpp` | WP-1a | Ramp switchback lateral offset |
| `MonolithMeshFacadeActions.cpp` | WP-1b | Glass pane material fix |
| `MonolithMeshFloorPlanGenerator.h` | WP-2c | FArchetypeRoom extensions |
| `MonolithMeshFloorPlanGenerator.cpp` | WP-2a, WP-2c | Door/corridor widths, entrance, floor assignment, aspect ratios |
| `MonolithMeshValidationActions.cpp` (NEW) | WP-3a | validate_building action |
| `MonolithMeshValidationActions.h` (NEW) | WP-3a | Validation action class |
| 9 archetype JSON files (NEW) | WP-2c | All building type definitions |

---

## Research Documents Reference

| Document | Issues Addressed | Work Packages |
|----------|-----------------|---------------|
| `2026-03-28-door-clearance-research.md` | #6, #7, #8 | WP-2a |
| `2026-03-28-window-cutthrough-research.md` | #4 | WP-1b |
| `2026-03-28-stairwell-cutout-research.md` | #1, #5 | WP-1a, WP-1c |
| `2026-03-28-attachment-logic-research.md` | #9, #10, #11, #12 | WP-2b |
| `2026-03-28-procgen-validation-research.md` | #18, #19, #20 | WP-3a |
| `2026-03-28-advanced-procgen-research.md` | Future work | Phase 4 |
| `2026-03-28-ramp-stair-geometry-research.md` | #1, #2, #3 | WP-1a |
| `2026-03-28-realistic-room-sizes-research.md` | #13, #14, #15, #16, #17 | WP-2c |

---

## Key Constants Reference

```
PLAYER CAPSULE (Leviathan / GAS Character):
  Radius:      42 cm
  Diameter:    84 cm
  Half-Height: 96 cm
  Total Height: 192 cm

DOOR DIMENSIONS:
  Normal:  100 cm wide x 220 cm tall (was 90cm -- FIX)
  Hospice: 120 cm wide x 220 cm tall (was 100cm -- FIX)
  Entrance: 110 cm wide x 240 cm tall (NEW)

CORRIDOR DIMENSIONS:
  Normal:  150 cm / 3 cells (was 100cm/2 cells -- FIX)
  Hospice: 200 cm / 4 cells (unchanged)

STAIR GEOMETRY (IBC Standard):
  Riser:   18 cm (7 inches)
  Tread:   28 cm (11 inches)
  Angle:   32.7 degrees
  Walkable limit: 44.76 degrees

FIRE ESCAPE (IBC Maximum):
  Riser:   20 cm (8 inches)
  Tread:   20 cm (8 inches)
  Angle:   45 degrees

ADA RAMP:
  Slope:   1:12 (4.76 degrees)
  Max rise per run: 76 cm
  Landing: 150 cm x 150 cm minimum

GRID:
  Cell size: 50 cm x 50 cm = 0.25 m^2
  Conversion: real_m2 * 4 = grid_cells

BOOLEAN CUTTER:
  Depth: WallThickness + 10 cm
  Center: On wall midpoint, NOT outer face
  Below floor: -3 cm (eliminate threshold sliver)
```

---

## Review #1 -- APPROVED WITH CHANGES

**Reviewer:** Independent Code Reviewer (unreal-code-reviewer agent)
**Date:** 2026-03-28

### What Was Done Well

The plan is thorough, well-researched, and well-structured. 20 issues are clearly cataloged with severity levels, and each has a concrete fix with code sketches. The research basis is solid -- 8 research documents with real building code references (IBC, ADA, IRC), engine source analysis, and industry comparisons (THE FINALS, Archipack, CGA grammars). The tiered validation system (WP-3a) is a smart addition. The grid cell math is correct throughout (grid_cells = m^2 * 4). The stair angle fix math checks out (270cm rise / 28cm tread / 18cm riser = 32.7 degrees). The Key Constants Reference at the bottom is a great touch for implementors.

### Issues Found

#### CRITICAL: File Conflicts Break Phase 1 Parallelism

**All three Phase 1 work packages modify `MonolithMeshBuildingActions.cpp`:**
- WP-1a: `GenerateStairGeometry()` (~line 857-906)
- WP-1b: `GenerateWallGeometry()` (~line 460), `CreateBuildingFromGrid()` param parsing
- WP-1c: `CreateBuildingFromGrid()` (~line 1130), the per-floor geometry loop

These are different functions within the same file, but agents cannot safely merge concurrent edits to the same file. If three agents are writing to `MonolithMeshBuildingActions.cpp` simultaneously, the last one to save wins and the other two agents' changes are lost.

**Fix:** Either (a) serialize WP-1a, WP-1b, WP-1c instead of parallelizing them, or (b) split `MonolithMeshBuildingActions.cpp` into separate files first (stair generation, wall generation, building orchestrator) so each WP touches its own file. Option (a) is simpler but adds ~18-24h to the critical path. Option (b) is a prerequisite refactor (~2-3h) that preserves the parallel benefit.

#### CRITICAL: File Conflicts Break Phase 2 Parallelism

**WP-2a and WP-2c both modify `MonolithMeshFloorPlanGenerator.cpp`:**
- WP-2a: `PlaceDoors()` (~line 922), `InsertCorridors()` (~line 709), new `EnsureExteriorEntrance()`
- WP-2c: `ParseArchetypeJson()`, `ResolveRoomInstances()`, `CorrectAspectRatios()` (new)

Same problem. These touch different functions but the same file.

**Fix:** Same options as above. The safest approach is to run WP-2a before WP-2c (or vice versa), letting WP-2b run in parallel with whichever goes first since WP-2b only touches `MonolithMeshArchFeatureActions.cpp` and `MonolithMeshBuildingTypes.h`.

#### IMPORTANT: Questionable Phase 2 Dependency on Phase 1

The plan states: "Phase 1 must complete first because WP-2a depends on WP-1c (stairwell changes affect the grid system)." This dependency is weak. WP-2a modifies the floor plan generator (`MonolithMeshFloorPlanGenerator.cpp`) -- door widths, corridor widths, entrance placement. WP-1c modifies the building construction pipeline (`MonolithMeshBuildingActions.cpp`) -- per-floor grid propagation. These are separate pipeline stages: the floor plan generator produces a grid + room list, then the building constructor consumes it. WP-2a's changes to door/corridor sizing do not depend on how the building constructor handles stairwell grids.

However, WP-2c (room sizes) arguably should wait for WP-1c because the stairwell footprint requirements (minimum 4x6 cells for switchback stairs, per WP-1a) directly affect how much footprint area is available for rooms in the archetypes.

**Fix:** Relax the blanket Phase 1 -> Phase 2 dependency. WP-2a and WP-2b can start as soon as WP-1a completes (WP-2b depends on the fire escape changes in WP-1a for the attachment context). WP-2c should wait for both WP-1a (stairwell sizing) and WP-1c (per-floor grids).

#### IMPORTANT: WP-2b `wall_openings` Array Has No Consumer

The Attachment Context system (WP-2b) emits `wall_openings` arrays in the JSON result of `create_balcony`, `create_porch`, and `create_fire_escape`. But the plan never describes who processes these opening requests. The orchestrator (SP5 in the master plan) would need to take each `wall_openings` entry and call the building's boolean subtract system to cut the actual holes. This orchestration logic is missing from the fix plan.

Without a consumer, the `wall_openings` data is informational-only -- it tells you what openings are needed but does not actually cut them. The plan should specify either:
1. A new helper function in `MonolithMeshBuildingActions.cpp` that takes a `TArray<FWallOpeningRequest>` and cuts boolean holes in the building mesh, or
2. An explicit note that the SP5 orchestrator (in the master plan, not this fix plan) is responsible for processing `wall_openings`.

**Fix:** Add a paragraph to WP-2b clarifying the consumer. If the consumer is SP5, note it as a forward dependency. If it should be a standalone action (e.g., `cut_wall_openings`), add it to WP-2b's scope and adjust the estimate.

#### IMPORTANT: WP-1b Facade/Building Wall Alignment Is Under-Specified

WP-1b Step 3 says to "Verify that wall thickness matches" and "wall position is centered on the same plane." But it does not provide concrete code to fix any misalignment if found. The facade's `BuildWallSlab()` and the building's `GenerateWallGeometry()` may use different origin conventions (center vs. inner face). If the facade wall is offset even 1cm from where the building wall would have been, rooms will have visible gaps or z-fighting at the wall edges.

**Fix:** WP-1b should include a concrete alignment formula or at minimum a debug visualization step that spawns debug lines at both the building's expected wall plane and the facade's actual wall plane so the implementor can confirm alignment during testing.

#### SUGGESTION: Entrance Door Face Priority Logic Has a Bug

In WP-2a Fix 3 (line 585-588), the face priority loop has questionable logic:
```cpp
if (Cell.Y == 0 && BestWall.IsEmpty()) { BestCell = Cell; BestWall = TEXT("south"); }
if (Cell.Y == GridH - 1) { BestCell = Cell; BestWall = TEXT("north"); break; }
```

The comment says "Priority: Y == GridH-1 (south in our convention)" but the code assigns Y==0 as "south" and Y==GridH-1 as "north." This may be intentional (UE coordinate system), but the comment contradicts the code. Additionally, the `break` after finding a north-facing cell means the loop exits after the first north-face hit for a single room, but if the preferred room has cells on multiple faces, a south-facing cell found earlier could be overwritten by an east/west cell before a north cell is found (because the if-statements are not else-if). The logic should use explicit priority scoring rather than cascading if-statements.

#### SUGGESTION: Missing Stairwell Minimum Footprint in Archetypes

WP-1a establishes that the minimum stairwell footprint for comfortable switchback stairs is 4x6 cells (200x300cm). WP-2c defines 9 new archetype JSONs. But the plan does not specify minimum stairwell cell counts in any of the archetypes. The archetype JSONs should include stairwell room entries with `min_area: 24` (6*4 cells) to prevent the floor plan generator from allocating too-small stairwells.

#### SUGGESTION: ValidateTier2 Null-Safety on GetActor

In WP-3a Tier 2 code (line 1158), `Hit.GetActor()->GetName()` will crash if the sweep hits BSP or landscape geometry with no owning actor. Should check `Hit.GetActor()` before dereferencing.

#### SUGGESTION: Tier 3 Validation Hardcodes Capsule Dimensions

`ValidateTier3()` at line 1192 calls `ValidateTier2(World, Desc, 42.0f, 96.0f)` with hardcoded values instead of forwarding the `CapsuleRadius` and `CapsuleHalfHeight` from the input params. This means Tier 3 always uses 42/96 even if the caller specified different dimensions.

### Recommendations

1. **Resolve file conflicts before execution.** Either refactor `MonolithMeshBuildingActions.cpp` into 3 files (stair gen, wall gen, orchestrator) as a Phase 0 step (~2-3h), or serialize the Phase 1 WPs. The time estimate section should be updated to reflect whichever approach is chosen.

2. **Revise the Phase 2 parallel claim.** At minimum, WP-2a and WP-2c cannot truly run in parallel due to the shared `.cpp` file. Run one before the other, or have WP-2a's changes land first (it is smaller scope).

3. **Add a `wall_openings` consumer** -- either as a new helper function in WP-2b or as an explicit forward-reference to the SP5 orchestrator. Without this, features like porches will still have no door.

4. **Add stairwell `min_area: 24` to all multi-floor archetype JSONs** (office_building, apartment_building, school, church with tower).

5. **Fix the entrance face priority logic** to use explicit priority scoring and add an else-if chain or scoring system instead of cascading if-statements.

6. **Add null-safety to `Hit.GetActor()`** in Tier 2 validation code.

7. **Forward capsule dimensions from Tier 3 to Tier 2** instead of hardcoding.

### Revised Phase Schedule (Accounting for File Conflicts)

If the pre-refactor approach is chosen:

| Phase | Duration | Notes |
|-------|----------|-------|
| Phase 0 | 2-3h | Split `MonolithMeshBuildingActions.cpp` into 3 files |
| Phase 1 | 12-15h parallel | WP-1a, WP-1b, WP-1c now touch separate files |
| Phase 2 | 12-15h serial(2a,2c) + parallel(2b) | WP-2a then WP-2c on FloorPlanGenerator; WP-2b in parallel |
| Phase 3 | 10-12h | WP-3a unchanged |
| **Total** | ~38-45h elapsed | vs. original 62-78h claim |

Note the original "62-78h elapsed" estimate assumed full parallelism. With file conflict resolution, the wall-clock time is actually shorter because the individual WP estimates already account for the work -- the issue was just that some cannot overlap.

If serializing instead of pre-refactoring:

| Phase | Duration | Notes |
|-------|----------|-------|
| Phase 1 | 24-31h serial | WP-1a -> WP-1b -> WP-1c (same file) |
| Phase 2 | 22-27h | WP-2b parallel with (WP-2a -> WP-2c serial) |
| Phase 3 | 10-12h | Unchanged |
| **Total** | ~56-70h elapsed | Marginally worse but no refactor needed |

### Verdict

**APPROVED WITH CHANGES.** The plan is solid in its technical analysis, algorithm choices, and scope management. The research backing is excellent. The two critical file conflict issues must be resolved before handing this to agents -- otherwise parallel execution will produce corrupted files. The `wall_openings` consumer gap should also be addressed to avoid a "feature emits data nobody reads" situation. With those fixes, this plan is ready for execution.

---

## Review #2 -- APPROVED WITH CHANGES

**Reviewer:** Independent Reviewer #2 (Player Experience, Hospice Accessibility, Horror Design, Integration)
**Date:** 2026-03-28

### What Was Done Well

The research depth is genuinely impressive. Eight research documents with real building code references (IBC, ADA), academic citations (Freiknecht & Effelsberg, Smith & Mateas), and industry analysis (Houdini, Dwarf Fortress, Nystrom) ground this plan in reality rather than guesswork. The 4-5x room scaling error catch is the kind of bug that would have shipped and made every building feel like a dollhouse. The tiered validation system (geometric -> capsule sweep -> NavMesh) is the right architecture -- fast checks always, expensive checks on demand.

The attachment context system (WP-2b) is particularly well-designed. The `FWallOpeningRequest` pattern of features declaring what they need rather than cutting holes themselves is clean separation of concerns.

### Issues Found

#### Critical (Must Fix)

**C1: Floor threshold elimination cuts into rooms below on multi-story buildings.**
The boolean cutter extends 3cm below FloorZ to "eliminate threshold sliver." On a multi-story building, this means the door cutter on floor 1 punches 3cm into the ceiling slab of floor 0. For interior doors this is cosmetic, but for stairwell access doors it could create visible holes in the room below. The plan needs a guard: only extend below FloorZ on ground-floor doors, or ensure the ceiling slab of the floor below is thick enough to absorb the 3cm overshoot.

**C2: No door MESH or frame -- just holes in walls.**
The entire plan focuses on cutting door openings but never mentions door geometry itself. A building full of open doorways with no actual door meshes or frames is not horror -- it is a convention center. Closed doors that the player must open are fundamental to horror pacing (what is behind this door?). The plan should at minimum acknowledge this gap and flag it as a follow-up, or better, include a basic door frame + pivot door mesh in WP-2a. Even a simple box-with-hinge parametric door would transform the player experience.

**C3: Entrance direction priority logic is inconsistent with the coordinate convention.**
In `EnsureExteriorEntrance()` (line ~585), the code checks `Cell.Y == 0` and assigns "south", then `Cell.Y == GridH - 1` and assigns "north", with north getting `break` priority. But the master plan states "Entrance room should touch the south edge by convention (front of building faces south)." The implementation gives priority to north (the `break` exits the loop on north, not south). This needs to match the convention or the buildings will have their "front" door on the back. Note: Review #1 also flagged the logic structure of this code block, but this issue is specifically about the priority inversion relative to the stated convention.

#### Important (Should Fix)

**I1: Hospice corridor corner clearance is unvalidated.**
The plan correctly cites ADA 152.4cm turning circle and sets hospice corridors at 200cm (4 cells). However, corridor intersections -- T-junctions and L-bends -- are where turning matters most. With 10cm interior walls, the effective turning space at a 90-degree corridor bend is reduced. The clearance validation pass in WP-2a should include a check for corridor intersection geometry, not just straight-run width.

**I2: Normal-mode door width (100cm) is marginal for momentum-based movement.**
The plan thoroughly analyzes the 42cm capsule radius but does not consider the Game Animation Sample character's motion matching movement model. Players approach doors at a run with momentum. A 100cm door gives 8cm clearance per side -- tight when the character is curving into a doorway. The research document itself recommends 100-110cm as "comfortable." Bumping the normal default to 110cm costs negligible room area and meaningfully improves navigation feel. The plan's own research supports this.

**I3: Horror atmosphere is entirely deferred.**
Every horror-specific feature (decay parameter, non-Euclidean rooms, sightline analysis, dead ends) sits in the Phase 4 "Future" section. After this fix plan, buildings will be architecturally correct but have zero horror atmosphere. At minimum, the 9 new archetype JSONs should carry horror-relevant metadata: which rooms are "dread rooms" (small, single-entrance), which corridors should be claustrophobic, where dead ends should occur. This metadata costs nothing to add now and pays off when horror features arrive.

**I4: Interior lighting is completely unaddressed.**
Every generated room will be pitch black inside. For a horror game, intentional darkness is valid (flashlight gameplay), but this is not stated anywhere. If darkness is intentional, say so. If not, even one point light per room during development would make buildings inspectable. This matters especially for hospice patients who may find total darkness disorienting or distressing rather than atmospheric.

**I5: Stairwell minimum size (2x2 cells) should be a hard reject, not a warning.**
WP-1a test criteria accept a 2x2 cell stairwell (100x100cm) with a warning. An 84cm-diameter capsule in a 100cm-wide space has 8cm clearance per side. This is not playable -- the player cannot physically navigate a stairwell this tight with any approach angle other than dead-straight. Hard-reject below 3x4 cells (150x200cm).

**I6: Orchestrator has no per-building retry logic.**
The master plan mentions graceful degradation, but this fix plan does not address what happens when building N of a city block fails validation. Does the block fail? Retry with a different seed? Skip the lot? A simple retry budget (3 attempts, different seeds, fall back to simpler archetype) would prevent a single unlucky generation from stalling the pipeline.

#### Suggestions (Nice to Have)

**S1: Add a "residential_small" archetype.**
A horror town needs cramped houses -- shotgun houses, single-room shacks. A 1-2 room building at 8x12 grid (4x6m) footprint fills the gap between "no building" and "full house." Think Silent Hill residential neighborhoods.

**S2: Larger buildings should support multiple entrances.**
The fix guarantees one entrance. A police station or school with a single door is unrealistic. Allow the archetype to specify `min_entrances` (default 1) and place additional exterior doors on buildings with multiple exterior-touching rooms.

**S3: Add a `validate_block` batch action.**
`validate_building` validates one building. For a 4-8 building block, the agent calls it 4-8 times. A batch action consolidating all buildings in a spatial registry block into one call saves MCP round-trips.

**S4: Archetype JSONs should include `floor_height` per room type now.**
A warehouse at 270cm ceiling is laughably short. A church nave at 270cm is a closet. The fix plan acknowledges variable ceiling height as Phase 4 future work, but if the archetype JSONs being written NOW include a `floor_height` field (warehouse: 500-600cm, church nave: 600-1000cm), the data is ready when the feature lands. Zero C++ cost.

**S5: Material hints in archetypes.**
Every building renders in checkerboard. While material assignment is downstream, archetype JSONs should carry semantic material hints (`"wall_material_hint": "brick"`, `"floor_material_hint": "linoleum"`) so the furnishing pipeline and any future material system have data to work with.

### Scale Correctness Verification

With 50cm grid cells and corrected room sizes, spot-checking the math:
- Residential living room: 72-120 cells = 18-30 m^2. A 6x12 cell room = 3x6m. Realistic.
- Office open plan: 200-600 cells = 50-150 m^2. A 20x30 cell room = 10x15m. Correct.
- Warehouse main floor: 1200-3200 cells = 300-800 m^2. A 40x80 cell space = 20x40m. Reasonable for small warehouse.
- Church nave: 400-1000 cells = 100-250 m^2. A 20x50 cell space = 10x25m. Small to medium church. Correct.
- School classroom: 260-340 cells = 65-85 m^2. A 14x20 cell room = 7x10m. Standard classroom. Correct.

The minimum footprint validation math (`TotalRequiredCells * 1.25 > AvailableCells`) is sound. One concern: the 25% corridor overhead may be low for buildings with many small rooms (clinics, police stations) where corridors dominate floor area. 30-35% would be safer. If this produces false rejections, tune it down.

### Performance Assessment

Validation tier budgets are sane. One note: Tier 3 NavMesh validation of an entire city block should build NavMesh ONCE covering the whole block, not per-building. A per-building NavMesh rebuild for 4-8 buildings is 40-240 seconds. A single block-wide build is 5-15 seconds.

Generation performance is not directly impacted -- the fixes modify existing code paths. Corrected room sizes produce larger buildings (more geometry), but since each building is one merged mesh, draw call count is unaffected. Boolean operations scale with cut count, not building size, so larger rooms with the same door count are actually faster.

### Integration Risk Assessment

**Character movement:** The Game Animation Sample character uses UCharacterMovementComponent with motion matching. The 100cm door width at 16cm total clearance is technically passable but will feel tight during sprint + turn maneuvers. The 45-degree sweep test in Tier 2 validation is the right mitigation. Recommend 110cm as stated above.

**AI navigation:** Buildings need NavMesh for AI enemies. SP7 (auto-volumes) handles this but is a separate sub-project. If SP7 is not complete when buildings are tested, AI cannot enter buildings. The fix plan should note this dependency.

**Weapons:** FPS line traces will interact with generated collision. Boolean door openings may produce thin collision triangles at cut edges. Worth a test case but not blocking.

### Archetype Diversity Assessment

The 9 types provide solid variety. Missing types that would strengthen the horror setting:
- **Gas station / convenience store** -- iconic horror location, 2-3 rooms
- **Motel / hotel** -- repeating units, long corridors, horror staple
- **Bar / pub** -- gathering point, basement potential
- **Fire station** -- large vehicle bay, upstairs quarters

Not critical for the fix plan. The format supports adding them trivially.

### Recommendations

1. **Fix C1-C3 before implementation.** C3 (entrance direction priority) silently produces wrong results. C1 (floor overshoot) is geometry corruption. C2 (no door meshes) is a player experience gap that must at least be acknowledged and scheduled.

2. **Bump normal-mode door width to 110cm.** The plan's own research supports this. Negligible area cost, meaningful navigation improvement.

3. **Add `floor_height` and `material_hint` fields to archetype JSONs now.** Zero C++ cost, high future value.

4. **Hard-reject stairwells below 3x4 cells** instead of just warning. A 2x2 stairwell is not playable.

5. **Add per-building retry (3 attempts, different seed) in the orchestrator.** Ten lines of code, prevents single-failure block stalls.

6. **Add horror metadata to archetype JSONs.** Dread rooms, claustrophobic corridors, dead-end locations. Data-only, zero implementation cost, captures architectural intent for horror features.

7. **State the lighting policy explicitly.** Either "darkness is intentional, flashlight gameplay" or "interior lighting is a follow-up item."

### Verdict

**APPROVED WITH CHANGES.** The plan is technically sound, well-researched, and correctly prioritized. The critical items (C1-C3) are implementation-level bugs that must be corrected in the code sketches before agents execute. The important items (I1-I6) are design gaps that should be addressed or explicitly deferred with rationale. The suggestions are genuinely optional. With the critical fixes applied, this plan is ready for execution.
