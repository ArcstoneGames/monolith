# Stairwell Floor/Ceiling Cutouts and Multi-Floor Connectivity

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** How to properly cut stairwell holes in floor/ceiling slabs, correct stair geometry for 270cm floor height, switchback stair layouts, and industry approaches to procedural stairwells

---

## Executive Summary

Our current stairwell code has **two critical bugs** and **one design-level problem**:

1. **Slab suppression works but is incomplete** -- stairwell cells (ID -2) are correctly skipped for the *current* floor's floor and ceiling slabs, but the floor slab of the *floor above* is not being cut. The stairwell grid is only present on the originating floor, not the destination floor.

2. **Stairs are impossibly steep** -- with default 50cm grid cells, a 2-cell-deep stairwell provides only 100cm of run for 270cm of rise. That is a 70-degree angle (270/100 = 2.7:1 slope). Unreal's default walkable floor angle is 44.77 degrees, and comfortable game stairs should be 30-35 degrees.

3. **No switchback/landing support** -- real buildings and games use switchback (U-turn) stairs for compact stairwells. A straight-run stair for 270cm rise at a comfortable 32-degree angle needs ~432cm of horizontal run, which is 8.6 grid cells at 50cm -- far too long for a single room.

**The fix:** Increase stairwell footprint to a minimum of 6x3 grid cells (300x150cm) with a switchback (half-landing) stair, or 9x2 cells (450x100cm) for a straight run. Coordinate stairwell cells across floors. Add enclosure walls on the upper floor.

---

## 1. Current Code Analysis

### What Works

From `MonolithMeshBuildingActions.cpp`:

- **Grid cell -2 marks stairwells** (line 192 of BuildingTypes.h): `Grid` is a `TArray<TArray<int32>>` where -2 means stairwell
- **`GenerateSlabs` correctly skips stairwell cells** (line 577): `if (bSkipStairwells && CellId == -2) continue;`
- **Both floor AND ceiling slabs pass `bSkipStairwells=true`** (lines 1162-1166)
- **`GenerateStairGeometry` uses `AppendLinearStairs`** from GeometryScript (line 903)
- **Step height is targeting 18cm** (line 890): `const float StepHeight = 18.0f;`

### What Is Broken

#### Bug 1: Upper floor has no stairwell cells

The stairwell grid cells (-2) are defined in the floor plan of the *originating* floor (floor A). But the destination floor (floor B) has its own grid, and unless the caller manually marks those same cells as -2 on floor B, floor B generates a solid floor slab directly above the stairs. The player hits their head on the ceiling.

**Current code path (lines 1140-1179):**
```
For each floor:
  1. Build wall segments from grid edges
  2. Generate wall geometry
  3. Generate floor slab (skip -2 cells) <-- only checks THIS floor's grid
  4. Generate ceiling slab (skip -2 cells) <-- only checks THIS floor's grid
  5. Cut door openings
  6. Generate stair geometry
```

The floor above the stairwell origin has no knowledge that stairs are coming up from below. Its grid cells at those positions are likely room cells (>=0), so it generates solid floor slab there.

#### Bug 2: Stairs are too steep (the math problem)

Current calculation (lines 886-893):
```cpp
float StairDepth = WorldMaxY - WorldMinY; // Stairs run along Y
// ...
int32 StepCount = FMath::Max(1, FMath::RoundToInt32(FloorHeight / StepHeight));
float ActualStepHeight = FloorHeight / static_cast<float>(StepCount);
float StepDepth = StairDepth / static_cast<float>(StepCount);
```

With default values:
- `CellSize` = 50cm, stairwell = 2 cells along Y
- `StairDepth` = 2 * 50 = **100cm**
- `FloorHeight` = 270cm (+ slab thickness, passed as `FloorHeight + FloorThick`)
- `StepCount` = round(270/18) = **15 steps**
- `StepDepth` = 100/15 = **6.67cm per tread**

A 6.67cm tread is absurdly shallow. Building code minimum is 25-28cm. The resulting angle is `atan(18/6.67)` = **69.6 degrees** -- virtually a ladder.

#### Bug 3: No stairwell enclosure walls on upper floor

When the stairwell hole opens into the floor above, there should be enclosure walls (or at minimum railings) around the opening on the upper floor. Currently nothing generates these.

---

## 2. Stair Geometry Standards

### Real-World Building Code (IBC/IRC)

| Property | Residential (IRC) | Commercial (IBC) | ADA Accessible |
|----------|-------------------|-------------------|----------------|
| Max riser height | 19.7cm (7.75") | 17.8cm (7") | 10-18cm (4-7") |
| Min tread depth | 25.4cm (10") | 27.9cm (11") | 27.9cm (11") |
| Comfortable angle | 30-35 deg | 30-33 deg | ~32 deg |
| Max angle | ~42 deg | ~37 deg | ~33 deg |
| Min width | 91.4cm (36") | 111.8cm (44") | 121.9cm (48") |
| Max flight height | 3.66m (12'0") | 3.66m (12'0") | 3.66m (12'0") |
| Landing min depth | = stair width | = stair width | = stair width |

The **7-11 rule** (7" rise, 11" run = 17.8cm rise, 27.9cm run) is the gold standard. This gives exactly 32 degrees -- the ideal stair angle per building science.

### Game-Friendly Standards (Level Design Book)

From [The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics):

| Property | Unreal Engine | Source/Quake |
|----------|--------------|--------------|
| Step height x depth | 15 x 25 cm | 8 x 12 inches |
| Stair angle | ~31 deg | ~34 deg |
| Door width x height | 110 x 220 cm | 56 x 112 in |
| Interior wall height | 300 cm | 128 in |
| Min hallway width | 150 cm | 64 in |
| Player capsule | 60 x 176 cm | 32 x 72 in |

**Game stairs are slightly shallower than real stairs** (15cm rise vs 18cm) because players move faster than real people and the step-up animation/physics need headroom.

### Unreal Engine Character Movement Defaults

| Property | Default Value | Notes |
|----------|--------------|-------|
| MaxStepHeight | 45.0 cm | Max vertical height character can step up |
| Walkable Floor Angle | 44.77 deg | Surfaces steeper than this are unwalkable |
| Capsule half-height | 88.0 cm | Standard 176cm tall character |
| Capsule radius | 30.0 cm | 60cm diameter |

**Key constraint:** Each step's riser height must be <= MaxStepHeight (45cm). Our 18cm risers are fine. But the overall stair slope must be <= 44.77 degrees for the character to walk on it at all (without slope override). **Target 30-35 degrees for comfortable gameplay.**

---

## 3. The Math: Minimum Stairwell Footprint

### For 270cm Floor Height (our default)

Using the 7-11 rule (17.8cm rise, 27.9cm tread):

```
Steps needed:  ceil(270 / 17.8)  = 16 steps (actual rise = 16.875cm each)
Total run:     16 * 27.9         = 446.4cm
Actual angle:  atan(16.875/27.9) = 31.2 degrees  (perfect)
```

### Straight Run Stair

- **Run needed:** ~450cm
- **Width needed:** 90cm minimum (player capsule is 60cm wide, need clearance)
- **Grid cells at 50cm:** 9 cells deep x 2 cells wide = **18 cells**
- **Verdict:** Way too long. Takes an entire building dimension.

### Switchback (U-Turn / Half-Landing) Stair

Split into two flights of 8 steps each with a landing in between:

```
Flight 1:  8 steps * 27.9cm = 223.2cm run, rises 8 * 16.875 = 135cm
Landing:   90cm deep (= stair width), flat, at Z+135cm
Flight 2:  8 steps * 27.9cm = 223.2cm run, rises 135cm more to Z+270cm
           (runs OPPOSITE direction from Flight 1)

Total depth:  223.2 + 90 + 0 = ~315cm  (flights stack vertically, not linearly)
              Actually: max(223.2, 223.2) + 90 = ~315cm for the stairwell depth
Total width:  90 + gap + 90 = ~200cm minimum (two flights side by side + dividing wall)
```

Wait -- switchback stairs stack the two flights side-by-side with a landing at the turn:

```
  PLAN VIEW:

  [Flight 1 UP  ] [Flight 2 UP  ]
  [  8 steps    ] [  8 steps    ]
  [  going +Y   ] [  going -Y   ]
  [_____________]  [_____________]
        [   LANDING   ]

  Width:  2 * 90cm + 10cm gap = 190cm  (~4 cells at 50cm)
  Depth:  max(223.2cm, 90cm landing) = 225cm (~5 cells at 50cm)
```

Actually, the correct switchback layout:

```
  PLAN VIEW (looking down):

  +Y ^
     |  [Flight 2 going -Y]
     |  [  8 steps down   ]
     |  [  Width: 90cm    ]
     |
     |  [=== LANDING ===] (at far end, both flights connect here)
     |
     |  [Flight 1 going +Y]
     |  [  8 steps up     ]
     |  [  Width: 90cm    ]
     +--------------------> +X

  Depth (Y): 223cm (flight run) + 90cm (landing) = ~315cm
  Width (X): 90cm (flight 1) + 10cm (wall/gap) + 90cm (flight 2) = 190cm
```

No wait. In a switchback, both flights share the same depth corridor but run in opposite directions:

```
  SECTION VIEW (side, looking from +X):

  Floor 2 ________________________________
           |
           |  Flight 2 (8 steps, going -Y)
           |/
  Landing  |__________ at Z+135cm, far end
           |
           |  Flight 1 (8 steps, going +Y)
           |/
  Floor 1 ________________________________

  PLAN VIEW:

           +--Flight 1--+--Flight 2--+
           |  runs +Y   |  runs -Y   |
           |  90cm wide |  90cm wide |
           +------------+------------+
           |<--- 190cm total width -->|
           |                          |
           |<-- 225cm depth (Y) ----->|
```

### Minimum Grid Cells for Switchback (50cm cells)

| Dimension | Real (cm) | Grid Cells (50cm) | Notes |
|-----------|-----------|-------------------|-------|
| Width (X) | 190 cm | 4 cells | Two 90cm flights + 10cm gap |
| Depth (Y) | 315 cm | 7 cells | 225cm flight run + 90cm landing |
| **Total** | | **28 cells** | 4x7 grid |

This is more realistic but still substantial. However, **this is how real buildings work** -- stairwells are significant features.

### Compact Game-Friendly Variant

Using game-standard 15cm rise, 25cm tread (Level Design Book):

```
Steps:     ceil(270 / 15) = 18 steps
Per flight: 9 steps * 25cm = 225cm run
Landing:   75cm (slightly less than stair width)
Width:     80cm per flight (tight but playable for FPS)

Total depth: 225cm + 75cm landing = 300cm = 6 cells
Total width: 80 + 10 + 80 = 170cm = 4 cells (with rounding)

Grid: 4 x 6 = 24 cells
```

### Even More Compact: L-Shaped (Quarter-Landing)

```
Flight 1:  9 steps * 25cm = 225cm along +Y
Landing:   90x90cm quarter-turn platform
Flight 2:  9 steps * 25cm = 225cm along +X (or -X)

Total footprint: 225cm x 315cm = 5x7 cells
```

This uses roughly the same space but the stairwell wraps around a corner instead of a U-turn.

### Recommended Minimum Stairwell Sizes

| Type | Dimensions (cm) | Grid (50cm) | Total Cells | Best For |
|------|-----------------|-------------|-------------|----------|
| Straight run | 100 x 450 | 2 x 9 | 18 | Long corridors, open plans |
| Switchback (tight) | 170 x 300 | 4 x 6 | 24 | Apartments, standard buildings |
| Switchback (comfy) | 200 x 325 | 4 x 7 | 28 | Commercial, horror (claustrophobia) |
| L-shaped | 225 x 315 | 5 x 7 | 35 | Corner stairwells |
| Spiral (game) | 200 x 200 | 4 x 4 | 16 | Tight spaces (needs custom geo) |

---

## 4. Stairwell Cutout Mechanics

### The Complete Stairwell Protocol

A stairwell connecting Floor N to Floor N+1 requires modifications on **both floors**:

#### Floor N (Bottom / Origin):
1. **Suppress ceiling slab** at stairwell cells -- already working (grid -2, `bSkipStairwells=true`)
2. **Suppress floor slab** at stairwell cells -- already working
3. **Generate stair geometry** starting at floor level, rising to ceiling level -- working but wrong angle
4. **Generate stairwell enclosure walls** -- NOT IMPLEMENTED (walls around the stairwell opening on this floor)

#### Floor N+1 (Top / Destination):
5. **Suppress floor slab** at stairwell cells -- **BROKEN** (floor N+1's grid has no -2 cells at this position)
6. **Generate landing geometry** where stairs arrive -- NOT IMPLEMENTED
7. **Generate stairwell enclosure walls / railings** around the opening -- NOT IMPLEMENTED
8. **Optionally suppress ceiling slab** if another stairwell continues upward -- needs coordination

#### Safety Rule:
**Never cut a floor/ceiling hole without stair geometry connecting the levels.** An uncovered hole is a death pit. The cutout and stair generation must be atomic.

### Fix: Cross-Floor Stairwell Propagation

The stairwell definition should be processed at the building level (not per-floor):

```
Algorithm: PropagateStairwells(BuildingDescriptor)
  For each Stairwell S:
    FloorA = S.ConnectsFloorA  (origin)
    FloorB = S.ConnectsFloorB  (destination)

    // Mark stairwell cells on Floor A's grid (origin -- suppress ceiling)
    For each cell C in S.GridCells:
      FloorA.Grid[C.Y][C.X] = -2

    // Mark stairwell cells on Floor B's grid (destination -- suppress floor slab)
    For each cell C in S.GridCells:
      FloorB.Grid[C.Y][C.X] = -2

    // If stairwell spans >2 floors (e.g., open atrium), mark intermediate floors too
    For floor F from FloorA+1 to FloorB-1:
      For each cell C in S.GridCells:
        F.Grid[C.Y][C.X] = -2
```

This must happen **before** geometry generation begins, during the parsing/setup phase.

### Headroom Verification

From the Vazgriz procedural dungeon research: "there must be headroom for characters to stand above the staircase itself, so the two cells directly above the staircase must be open as well."

For our system:
- Player height: 176cm (UE default capsule)
- Minimum headroom: 200cm (IBC requires 203cm / 6'8")
- At mid-stair (halfway up 270cm rise = 135cm above floor), remaining headroom = 270 - 135 = 135cm -- **NOT ENOUGH** if ceiling slab is present
- This is exactly why the ceiling slab MUST be removed above the stairwell

With ceiling removed, headroom at any stair position = FloorHeight - StairZ + NextFloorHeight. Since the ceiling is open, the character has the full next floor's height as headroom once past mid-stair.

---

## 5. Multi-Floor Stairwell Coordination

### Stairwell Alignment Across Floors

The same grid cells must be used for the stairwell on every floor it passes through. This is already implied by the `FStairwellDef` struct having `GridCells` that are building-global, but the current code does not enforce cross-floor consistency.

### Continuous Stairwells (Multi-Story)

For a building with floors 0, 1, 2, 3 where the stairwell goes all the way up:

```json
{
  "stairwells": [
    { "stairwell_id": "main_stair_0_1", "grid_cells": [[2,0],[2,1],[3,0],[3,1],[2,2],[2,3],[3,2],[3,3]], "connects_floor_a": 0, "connects_floor_b": 1 },
    { "stairwell_id": "main_stair_1_2", "grid_cells": [[2,0],[2,1],[3,0],[3,1],[2,2],[2,3],[3,2],[3,3]], "connects_floor_a": 1, "connects_floor_b": 2 },
    { "stairwell_id": "main_stair_2_3", "grid_cells": [[2,0],[2,1],[3,0],[3,1],[2,2],[2,3],[3,2],[3,3]], "connects_floor_a": 2, "connects_floor_b": 3 }
  ]
}
```

Same grid cells, different floor connections. The grid cells on each intermediate floor are marked -2 for both the floor slab (hole coming from below) and ceiling slab (hole going up).

### Switchback Direction Alternation

For switchback stairs in a continuous stairwell, each flight should alternate direction:

```
Floor 3  ________     ________
         |Flight6|   |Flight5|
Floor 2  |_______|   |_______|
         |Flight4|   |Flight3|
Floor 1  |_______|   |_______|
         |Flight2|   |Flight1|
Floor 0  |_______|   |_______|
```

Flight 1 goes +Y, Flight 2 goes -Y (landing at top), Flight 3 goes +Y again, etc. The landing alternates between the near end and far end of the stairwell on each floor.

---

## 6. Industry Approaches

### Vazgriz Procedural Dungeons (3D Grid-Based)
- Rise-to-run ratio: 1:2 (steep but playable)
- Staircase occupies 4 cells: 2 for stairs + 2 for headroom above
- Pathfinder treats staircase as a single "jump" across 3 horizontal + 1 vertical units
- Naturally produces double-width staircases and multi-floor landings
- **Key insight:** Headroom cells above stairs must be tracked as occupied

### Shadows of Doubt (Tile-Based, 1.8m tiles)
- Location hierarchy: City > District > Block > Building > Floor > Address > Room > Tile
- 15x15 tile floors (27m x 27m)
- Stairwell details not publicly documented but the tile grid handles them as special tile types
- Hallway connectivity ensures stairwell access from corridors

### Houdini Procedural Buildings
- Boolean subtract approach: stairwell shape subtracted from floor slabs
- Stair geometry generated from a single driving curve/line
- Parameters: height (single point), width, step bevel, nosing, UV, bend, handrail
- Floor holes are explicit boolean operations, not slab suppression

### Resident Evil Spencer Mansion
- Stairwells as architectural horror tools: sight lines broken by landings
- Grand central staircase creates spatial landmark for orientation
- Secondary service stairs create alternative routes (horror tension -- which path is safe?)
- Asymmetric wing staircases for disorientation

### General Game Patterns
- **Slab suppression** (our approach, Shadows of Doubt): Mark cells, skip slab generation. Simplest, fastest, no boolean cost.
- **Boolean subtract** (Houdini, ArchiCAD): Generate full slab, then subtract stairwell shape. More flexible for irregular openings, higher geometry cost.
- **Separate meshes** (most shipped games): Stairwell is a separate static mesh placed at build time, with purpose-built floor/ceiling meshes that have holes pre-modeled. Most art-controllable but not procedural.

**Recommendation for Monolith:** Keep slab suppression (it is working correctly for the current floor). Add cross-floor propagation. This avoids boolean operations which are expensive and fragile.

---

## 7. Switchback Stair Geometry

`AppendLinearStairs` only generates a single straight flight. For switchback stairs, we need to compose two flights + a landing platform.

### Proposed `GenerateSwitchbackStairs` Algorithm

```
Input: StairwellBounds (world-space AABB), FloorHeight, FloorZ, StairWidth

1. Compute total steps: N = round(FloorHeight / 17.0)  // 17cm game-friendly rise
2. Steps per flight: HalfN = N / 2  (round up for flight 1 if odd)
3. StepRise = FloorHeight / N
4. StepRun = 25.0  // cm, game standard tread depth

5. Flight 1 run = HalfN * StepRun
6. Flight 1 start: (CenterX - StairWidth/2, MinY, FloorZ)
   Flight 1 direction: +Y
   Flight 1 end Z: FloorZ + HalfN * StepRise

7. Landing platform:
   Position: (MinX, MinY + Flight1Run, FloorZ + HalfN * StepRise)
   Size: (StairwellWidth, LandingDepth, SlabThickness)
   Generate as AppendBox

8. Flight 2 run = (N - HalfN) * StepRun
9. Flight 2 start: (CenterX + StairWidth/2 + Gap, MinY + Flight1Run + LandingDepth, Landing Z)
   Flight 2 direction: -Y  (opposite direction -- ROTATE 180 degrees)
   Flight 2 end Z: FloorZ + FloorHeight

10. AppendLinearStairs(Flight1)
11. AppendBox(Landing)
12. AppendLinearStairs(Flight2, rotated 180 deg about Z)
```

### Stairwell Enclosure Walls

Around the stairwell opening on the upper floor, generate thin walls or railing geometry:

```
For each edge of the stairwell grid cells that borders a non-stairwell cell:
  If this edge is on the UPPER floor (floor B):
    Generate wall from floor level to railing height (90-100cm)
    OR generate full-height wall if stairwell is enclosed
```

This reuses the existing `BuildWallSegments` logic -- stairwell cells (-2) adjacent to room cells produce wall segments, just like room-to-room boundaries.

---

## 8. Concrete Fix Plan

### Phase 1: Cross-Floor Stairwell Propagation (~2h)

**File:** `MonolithMeshBuildingActions.cpp`, in `CreateBuildingFromGrid`

Before the per-floor geometry loop (line ~1130), add:

```cpp
// Propagate stairwell cells to destination floors
for (const FStairwellDef& Stair : Stairwells)
{
    int32 FloorB = Stair.ConnectsFloorB;
    if (FloorB >= 0 && FloorB < NumFloors)
    {
        // Get or create floor B's grid
        auto& DestGrid = FloorGrids[FloorB]; // need per-floor grid storage
        for (const FIntPoint& Cell : Stair.GridCells)
        {
            if (Cell.Y >= 0 && Cell.Y < GridH && Cell.X >= 0 && Cell.X < GridW)
            {
                DestGrid[Cell.Y][Cell.X] = -2; // Mark as stairwell on destination floor
            }
        }
    }
}
```

This requires refactoring the floor loop to support per-floor grids (currently all floors share the same base grid). The `floors` array parameter already exists but isn't parsed for per-floor grid overrides.

### Phase 2: Fix Stair Geometry (~3h)

**Option A: Increase stairwell footprint requirement (documentation + validation)**
- Add validation: if stairwell cells provide < 250cm of depth (Y), warn/error
- Document minimum footprint in MCP action schema

**Option B: Generate switchback stairs (recommended)**
- Replace single `AppendLinearStairs` with two flights + landing
- Compute whether straight or switchback based on available depth vs required run
- If `availableDepth >= requiredRun`: straight stair
- If `availableDepth < requiredRun`: switchback (requires minimum width for two flights)

**Threshold calculation:**
```cpp
float RequiredRun = StepCount * 25.0f; // 25cm tread
float AvailableDepth = WorldMaxY - WorldMinY;
float AvailableWidth = WorldMaxX - WorldMinX;

if (AvailableDepth >= RequiredRun)
{
    // Straight stair fits
    GenerateStraightStair(...);
}
else if (AvailableWidth >= StairWidth * 2 + 10.0f) // Two flights side by side + gap
{
    // Switchback fits
    GenerateSwitchbackStair(...);
}
else
{
    // Stairwell too small -- warn and generate best-effort steep stair
    UE_LOG(LogMonolithMesh, Warning, TEXT("Stairwell %s too small for comfortable stairs. "
        "Minimum footprint for 270cm floor height: 170x300cm (4x6 cells at 50cm). "
        "Current: %.0fx%.0fcm"), *Stair.StairwellId, AvailableWidth, AvailableDepth);
}
```

### Phase 3: Stairwell Enclosure Walls (~2h)

On the upper floor, detect stairwell-to-room boundaries and generate:
- Full walls for enclosed stairwells (default)
- Half-height walls/railings for open stairwells (optional parameter `open_stairwell: true`)

### Phase 4: Landing Geometry + Trim (~1h)

- Landing platform at the switchback turn point
- Floor-level landing at the top (arrival on upper floor)
- Optional: handrail geometry (low priority, can use static mesh placement later)

### Estimated Total: ~8 hours

---

## 9. Updated FStairwellDef

The current `FStairwellDef` needs additional fields:

```cpp
struct FStairwellDef
{
    FString StairwellId;
    TArray<FIntPoint> GridCells;
    int32 ConnectsFloorA = 0;
    int32 ConnectsFloorB = 1;
    FVector WorldPosition = FVector::ZeroVector;

    // NEW FIELDS:
    FString StairType = TEXT("auto");  // "auto", "straight", "switchback", "l_shaped", "spiral"
    float StairWidth = 90.0f;          // Per-flight width in cm
    float TreadDepth = 25.0f;          // Step tread depth in cm
    float RiserHeight = 17.0f;         // Target step rise in cm
    bool bEnclosed = true;             // Full walls vs open railing on upper floor
    bool bGenerateRailing = false;     // Generate railing geometry
    FString Direction = TEXT("auto");  // "north", "south", "east", "west", "auto"
};
```

The `StairType = "auto"` mode would:
1. Calculate required run for the given floor height
2. Check available stairwell dimensions
3. Pick the most appropriate stair type that fits

---

## 10. Quick Reference: Stairwell Sizing Table

For **270cm floor height** (our default), **50cm grid cells**:

| Stair Type | Rise (cm) | Tread (cm) | Steps | Run (cm) | Min Grid (WxD) | Angle |
|------------|-----------|------------|-------|----------|-----------------|-------|
| Current (BROKEN) | 18 | 6.7 | 15 | 100 | 2x2 | 69.6 deg |
| Straight (game) | 15 | 25 | 18 | 450 | 2x9 | 31 deg |
| Straight (code) | 17 | 28 | 16 | 448 | 2x9 | 31 deg |
| Switchback (tight) | 17 | 25 | 16 | 200+90 | 4x6 | 34 deg |
| Switchback (comfy) | 15 | 28 | 18 | 252+90 | 4x7 | 28 deg |
| L-shaped | 17 | 25 | 16 | 200+90 | 5x5 | 34 deg |

**Recommended default: Switchback (tight) at 4x6 cells (200x300cm).**

This fits within a single room-sized space (2m x 3m) while maintaining a comfortable 34-degree angle. For horror games, enclosed switchback stairwells are ideal -- limited visibility around the landing creates tension.

---

## Sources

- [Residential Stair Codes: Rise, Run, Handrails](https://buildingcodetrainer.com/residential-stair-code/)
- [Maximum Stair Riser Height & Minimum Tread Depth - Lapeyre Stair](https://www.lapeyrestair.com/blog/stair-riser-height-tread-depth/)
- [Standard Stair Rise and Run - Family Handyman](https://www.familyhandyman.com/article/stair-codes-rise-run-and-nosing/)
- [U-Shaped Straight Stairs - Dimensions.com](https://www.dimensions.com/element/u-shaped-straight-stairs-landing)
- [Switchback Staircases - Paragon Stairs](https://www.paragonstairs.com/blog/switchback-stairs/)
- [Stair Treads and Risers - UpCodes (IBC)](https://up.codes/s/stair-treads-and-risers)
- [U.S. Access Board - ADA Chapter 5: Stairways](https://www.access-board.gov/ada/guides/chapter-5-stairways/)
- [ADA Compliance - Stairs](http://www.ada-compliance.com/ada-compliance/ada-stairs.html)
- [Walkable Slope - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/walkable-slope-in-unreal-engine)
- [AppendLinearStairs - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Primitives/AppendLinearStairs)
- [Metrics - The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics)
- [UE5 Guide to Player Scale and Dimensions](https://www.worldofleveldesign.com/categories/ue5/guide-to-scale-dimensions-proportions.php)
- [Procedurally Generated Dungeons - Vazgriz](https://vazgriz.com/119/procedurally-generated-dungeons/)
- [Shadows of Doubt DevBlog 13: Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Procedural Stairs and Walls - Jesse Marofke (Houdini+UE)](https://jessemarofke.com/project/procedural-stairs-and-walls-houdini-and-unreal/)
- [Multi-Tiered Horror Design with RE Remastered - Gamedeveloper](https://www.gamedeveloper.com/design/multi-tiered-horror-design-with-resident-evil-remastered)
- [Recursive Unlocking: RE Map Design Analysis](https://horror.dreamdawn.com/?p=81213)
- [Procedural Building Generation - Diva Portal](https://www.diva-portal.org/smash/get/diva2:1480518/FULLTEXT01.pdf)
