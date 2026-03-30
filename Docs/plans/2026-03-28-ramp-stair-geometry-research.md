# Ramp & Stair Geometry Research: Self-Intersection Prevention and Code Standards

**Date:** 2026-03-28
**Status:** Research Complete
**Estimate:** ~8-12h implementation across three fixes

---

## Summary

Three geometry bugs identified in the procedural building system:
1. **ADA ramp switchback self-intersection** -- runs stack vertically instead of side-by-side
2. **Fire escape stairs at ~70 degrees** -- step depth calculated from landing depth, not from proper tread standards
3. **Building stairwell stairs too steep** -- same root cause as fire escape (depth constrained to available space)

All three share the same conceptual error: **fitting stairs/ramps into an arbitrary bounding box rather than computing geometry from building code ratios first, then sizing the bounding box.**

---

## 1. ADA Ramp Geometry Rules

### Authoritative Standards (ADA Standards for Accessible Design, US Access Board)

| Property | Value | Source |
|----------|-------|--------|
| Maximum slope | 1:12 (4.76 degrees) | ADA 405.2 |
| Maximum rise per run | 76cm (30 inches) | ADA 405.6 |
| Landing clear length | 150cm (60 inches) minimum | ADA 405.7 |
| Landing clear width | At least as wide as the widest ramp run | ADA 405.7 |
| Switchback landing | 150cm x 150cm minimum | ADA 405.7.4 |
| Ramp clear width | 91.4cm (36 inches) minimum | ADA 405.5 |
| Cross slope | 1:48 maximum (1:50 recommended) | ADA 405.3 |
| Handrail height | 86-97cm (34-38 inches) | ADA 505.4 |
| Rise <= 15cm (6in) | Slope up to 1:10 permitted | ADA 405.2 Exception |

### Switchback Ramp Calculations

For a full floor rise of 270cm:
```
Segments needed:   ceil(270 / 76) = 4
Rise per segment:  270 / 4 = 67.5cm
Run per segment:   67.5 * 12 = 810cm
Total run:         4 * 810 = 3240cm
Landings:          3 intermediate + 2 end = 5 landings at 150cm each = 750cm
Total footprint:   ~3990cm along the run axis
```

With switchback (folding every segment):
```
Footprint length:  810cm (single run length) + 150cm (landing) = 960cm
Footprint width:   4 parallel runs * (120cm width + 12cm gap) = ~528cm
Total plan area:   960cm x 528cm
```

This is **massive**. For game purposes, relaxing to 1:8 slope (7.1 degrees) for non-wheelchair ramps drastically reduces footprint:
```
1:8 slope, 270cm rise:
Run per segment:  67.5 * 8 = 540cm
2 switchbacks:    footprint = 540cm x ~264cm
```

### Critical Bug: Switchback Self-Intersection

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreateRampConnector()` (lines 860-1032)
**File:** `MonolithMeshTerrainActions.cpp`, `BuildADARampGeometry()` (lines 1000-1105)

#### ArchFeature version bug (lines 901-916):

The ramp connector alternates direction along Y (`Direction = 1` then `-1`) but **never offsets in X**. Both runs share X=0 center, so segment 1 runs from Y=0 to Y=810 and segment 2 runs from Y=810 back toward Y=0 -- at a higher Z. The problem: both runs are centered on the same X=0 line, and the tilted box geometry of segment 2 intersects or sits directly above segment 1 with only Z separation.

For a 1:12 ramp, the Z separation per segment is only 67.5cm, while the ramp surface is 10cm thick and tilted. At this shallow angle, the geometry does NOT self-intersect because the Z clearance is sufficient. However, there is **no horizontal offset** -- the switchback runs are directly above each other, which:
- Creates an enclosed tunnel (segment 2's underside forms a ceiling over segment 1)
- Has no headroom clearance (67.5cm between top of segment 1 and bottom of segment 2, far below the 210cm minimum)
- Is physically impossible to walk through -- a wheelchair user would need 210cm+ headroom

#### Terrain version bug (lines 1082-1094):

```cpp
CurrentDir = -CurrentDir; // Switchback
```

Same issue: direction reverses 180 degrees but position only advances along the run axis + landing. No perpendicular offset. Runs overlap in plan view.

#### Fix: Perpendicular Offset

Each switchback run must be offset perpendicular to the run direction by `(ramp_width + gap)`. The gap between parallel runs should be at minimum 30cm (12 inches) for the inner handrail to be continuous, per ADA 505.3.

**Correct switchback algorithm:**
```
Given: rise, width, slope_ratio, max_rise_per_run
  segments = ceil(rise / max_rise_per_run)
  rise_per_seg = rise / segments
  run_per_seg = rise_per_seg / slope_ratio

  right_vector = cross(up, run_direction)
  gap = 30.0  // minimum gap between parallel runs for handrails

  For segment i:
    // Alternate direction
    seg_direction = (i % 2 == 0) ? run_direction : -run_direction

    // PERPENDICULAR offset for each pair of runs
    perp_offset = right_vector * (i * (width + gap))

    seg_start = base_position + perp_offset + FVector(0,0, rise_per_seg * i)
    seg_end   = seg_start + seg_direction * run_per_seg + FVector(0,0, rise_per_seg)

    // Landing connects seg_end to next segment's start
    // The landing must be 150x150cm minimum and handle the 180-degree turn
```

The perpendicular offset ensures runs are side-by-side in plan view, not stacked vertically.

### Headroom Validation

After generating ramp geometry, validate:
- Minimum 210cm (80 inches) headroom under any overhead geometry (including the return run above)
- For a 1:12 ramp with 76cm max rise per run, the Z clearance between stacked runs is only 76cm -- far below 210cm
- This confirms stacking is NOT viable; perpendicular layout is required

---

## 2. Fire Escape Stairs -- Angle Bug

### The Bug

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreateFireEscape()` (lines 644-648)

```cpp
const float StairStepH = 20.0f;
const int32 StepsPerFlight = FMath::Max(2, FMath::RoundToInt32(FloorHeight / StairStepH));
const float ActualStepH = FloorHeight / static_cast<float>(StepsPerFlight);
const float StepDepth = LandingD / static_cast<float>(StepsPerFlight);
```

The step depth (tread run) is calculated by dividing the **landing depth** (120cm default) by the number of steps. For a 270cm floor height:
```
StepsPerFlight = round(270 / 20) = 14 (rounded from 13.5)
ActualStepH = 270 / 14 = 19.3cm
StepDepth = 120 / 14 = 8.6cm

Angle = atan(19.3 / 8.6) = 66 degrees  (nearly vertical!)
```

This produces a near-ladder geometry because the stairs are constrained to fit within the 120cm landing depth. The stairs literally run vertically within the landing footprint.

### Real Fire Escape Standards

Per IBC Chapter 10 and NYC Building Code Chapter 27:

| Property | Minimum Value | Source |
|----------|--------------|--------|
| Tread depth | 20.3cm (8 inches) exclusive of nosing | IBC 1011.5.2 (fire escape) |
| Maximum riser | 20.3cm (8 inches) | IBC 1011.5.2 (fire escape) |
| Stair width | 55.9cm (22 inches) minimum | IBC 1011.5.2 |
| Landing width | 91.4cm (36 inches) minimum | IBC |
| Landing length | 114.3cm (45 inches) minimum | IBC |
| Stair angle | ~45 degrees maximum for fire escapes | IBC (derived from 8/8 ratio) |
| Handrail height | 81.3cm (32 inches) minimum | IBC |

For standard commercial/residential stairs (not fire escapes):

| Property | Value | Source |
|----------|-------|--------|
| Maximum riser | 17.8cm (7 inches) | IBC 1011.5.2 |
| Minimum tread | 27.9cm (11 inches) | IBC 1011.5.2 |
| Optimal angle | 30-35 degrees | Industry standard |
| Comfortable formula | 2R + T = 60-65cm | Blondel's formula |

Fire escape angle at 8"/8" riser/tread: `atan(20.3/20.3) = 45 degrees` -- steep but walkable.
Recommended for games: use the standard 17.8cm/27.9cm (7"/11") ratio for ~32.5 degrees.

### Fix

The stairs should NOT be constrained to fit within the landing depth. Instead:

```cpp
// CORRECT: Calculate step depth from proper building code ratio
const float StairStepH = 18.0f;    // ~7 inches (IBC standard)
const float StairStepD = 28.0f;    // ~11 inches (IBC standard)
const int32 StepsPerFlight = FMath::Max(2, FMath::RoundToInt32(FloorHeight / StairStepH));
const float ActualStepH = FloorHeight / static_cast<float>(StepsPerFlight);
const float StepDepth = StairStepD;  // FIXED: use building code tread depth

// The stair run extends BEYOND the landing -- this is physically correct
float StairRunLength = StepDepth * StepsPerFlight;
// For 270cm floor: 28 * 15 = 420cm stair run
```

For the fire escape specifically, the stairs extend outward from the landing platforms. Each flight runs from one landing down to the next, spanning half a floor in many real designs. The stair geometry should extend in Y well beyond the landing footprint.

**Fire escape layout fix:**
- Each landing sits at floor level, attached to the building wall
- Stairs descend from each landing at proper angle, extending outward (in +Y)
- The stair run for a full floor at 18cm/28cm: `15 steps * 28cm = 420cm`
- With switchback (half-floor flights): `7-8 steps * 28cm = ~210cm run per half-flight`
- Mid-landing at half-floor height, offset in X from the floor landing

### Angle Comparison

| Configuration | Riser | Tread | Angle | Walkable? |
|--------------|-------|-------|-------|-----------|
| Current code (270cm floor, 120cm depth) | 19.3cm | 8.6cm | 66 deg | NO -- ladder-like |
| Fire escape minimum (8"/8") | 20.3cm | 20.3cm | 45 deg | Steep but code-legal |
| Standard residential (7"/11") | 17.8cm | 27.9cm | 32.5 deg | YES -- comfortable |
| Game recommended (18/28) | 18cm | 28cm | 32.7 deg | YES -- comfortable |

---

## 3. Building Stairwell Stairs -- Same Bug

### The Bug

**File:** `MonolithMeshBuildingActions.cpp`, `GenerateStairGeometry()` (lines 857-906)

```cpp
float StairDepth = WorldMaxY - WorldMinY; // Stairs run along Y
float StepDepth = StairDepth / static_cast<float>(StepCount);
```

Same pattern: step depth is divided from the available stairwell space rather than from building code. For a 2-cell stairwell (100cm depth at 50cm/cell) with 270cm floor height:
```
StepCount = round(270 / 18) = 15
StepDepth = 100 / 15 = 6.7cm

Angle = atan(18 / 6.7) = 69.6 degrees  (practically a ladder!)
```

### Fix

The stairwell must either:
1. **Be large enough** for proper stairs (at 28cm tread and 15 steps, need 420cm run -- 8.4 cells at 50cm)
2. **Use switchback stairs** (two half-flights with mid-landing, each ~210cm run -- 4.2 cells)
3. **Report an error** if the stairwell is too small for walkable stairs

**Recommended approach: switchback stairwell with `AppendLinearStairs`**

```
For a 2x4 cell stairwell (100cm x 200cm):
  Half-flight: 7 steps * 28cm = 196cm run (fits in 200cm Y)
  Half-flight rise: 270/2 = 135cm
  Mid-landing at 135cm, offset in X

  Flight 1: starts at (0, 0, 0), goes +Y for 196cm, rises to 135cm
  Mid-landing: 100cm x 100cm at Z=135cm
  Flight 2: starts at (100, 200, 135), goes -Y for 196cm, rises to 270cm
```

Minimum stairwell size for switchback:
- X: 2 * stair_width (e.g., 2 * 80cm = 160cm = ~3.2 cells)
- Y: stair_run_per_flight (e.g., 196cm = ~4 cells)
- Total: ~4x4 cells minimum for a comfortable stairwell

If the stairwell grid cells are too small, either:
- Warn the user and suggest enlarging
- Fall back to spiral stairs (steeper but fits smaller footprint)
- Use the steepest acceptable angle (45 degrees) and compute tread from that

---

## 4. UE5 Navigation Constraints

### Character Movement Defaults

| Property | Default Value | Notes |
|----------|--------------|-------|
| MaxStepHeight | 45cm | Maximum single-step obstacle the character can climb |
| WalkableFloorAngle | 44.76 degrees | ~45 degrees; surfaces steeper than this are unwalkable |
| WalkableFloorZ | 0.71 | cos(44.76 degrees); computed from WalkableFloorAngle |
| CapsuleHalfHeight | 88cm (default) | Standard character capsule |
| CapsuleRadius | 34cm (default) | Standard character capsule |

### Stair Walkability Rules

For individual-step stairs (box treads):
- Each step rise must be < MaxStepHeight (45cm) -- our 18cm steps pass easily
- The stair surface slope doesn't matter because each step is flat
- What matters is the character can reach the next step

For ramp-style stairs (continuous slope like `AppendLinearStairs`):
- The overall slope must be < WalkableFloorAngle (44.76 degrees)
- At 18cm rise / 28cm tread: angle = 32.7 degrees -- PASS
- At 20cm rise / 8.6cm tread (current bug): angle = 66.7 degrees -- FAIL (unwalkable!)

### NavMesh and Stairs

- NavMesh auto-generates on stairs if the slope is < WalkableFloorAngle
- The `StepHeight` in `ARecastNavMesh` (default 44cm in UE5) controls what nav agents can step up
- For `AppendLinearStairs`, the stair surface IS a continuous slope -- it must be < 44.76 degrees
- For box-tread stairs, nav works via step-up behavior as long as each step < 45cm

---

## 5. Ramp Clearance Validation Algorithm

After generating any ramp or stair geometry, validate with this algorithm:

```
ValidateAccessibility(mesh, player_capsule_height=180, min_headroom=210):
  1. Sample points along the ramp centerline every 50cm
  2. At each point, cast a ray upward to find overhead geometry
  3. If overhead_distance < min_headroom: flag "insufficient headroom"

  4. At each point, compute surface normal
  5. If angle_from_vertical > WalkableFloorAngle: flag "slope too steep"

  6. For ramps: verify slope < 4.76 degrees (1:12) for ADA
  7. For stairs: verify each step rise < 45cm (MaxStepHeight)

  8. Check for mesh self-intersection:
     - Use GeometryScript MeshIntersection test or
     - Check if any ramp segment's AABB overlaps another in XY plane

  9. Verify landing dimensions:
     - Each landing >= 150cm x 150cm (ADA)
     - Direction-change landings >= 150cm x 150cm (ADA)
```

---

## 6. Concrete Fix Specifications

### Fix 1: Ramp Switchback Perpendicular Offset (~3h)

**Files:** `MonolithMeshArchFeatureActions.cpp` (CreateRampConnector), `MonolithMeshTerrainActions.cpp` (BuildADARampGeometry)

**Changes:**
- Add perpendicular offset calculation: `offset = right_vector * seg_index * (width + gap)`
- Landing connects end of one segment to start of next (handle 180-degree turn)
- Landing dimensions: 150cm along run direction, `width + gap + width` perpendicular (to span both runs)
- Inner handrails must be continuous through the turn
- Add `gap` parameter (default 30cm) to both functions
- Add headroom validation: check Z clearance between stacked elements > 210cm

### Fix 2: Fire Escape Stair Angle (~3h)

**File:** `MonolithMeshArchFeatureActions.cpp` (CreateFireEscape)

**Changes:**
- Replace `StepDepth = LandingD / StepsPerFlight` with `StepDepth = 28.0f` (configurable)
- Add `step_rise` (default 18) and `step_depth` (default 28) optional parameters
- Recompute stair run length as `StepDepth * StepsPerFlight`
- Stairs extend outward from landing in Y, not constrained to landing depth
- Add mid-level landings for switchback flights
- Validate angle < 45 degrees (or warn)
- Update stringer geometry to match new dimensions

### Fix 3: Building Stairwell Stairs (~4h)

**File:** `MonolithMeshBuildingActions.cpp` (GenerateStairGeometry)

**Changes:**
- Replace `StepDepth = StairDepth / StepCount` with proper tread calculation
- Implement switchback stairs within stairwell bounds:
  - Half-flight of `ceil(StepCount/2)` steps per flight
  - Mid-landing at half-floor height
  - Two flights offset in X within the stairwell footprint
- Validate stairwell is large enough: minimum `stair_width * 2` in X, `step_depth * ceil(StepCount/2)` in Y
- If too small, warn and use steepest walkable angle (44 degrees)
- Switch from `AppendLinearStairs` to manual box treads for switchback geometry
- Add stairwell railing

### Fix 4: Validation Pass (~2h)

- Add `ValidateStairGeometry()` helper: checks angle, headroom, step dimensions
- Add `ValidateRampGeometry()` helper: checks slope, landings, headroom, self-intersection
- Both return warnings/errors in the action result JSON
- Add `validation_warnings` array to all stair/ramp action results

---

## 7. Reference Dimensions Quick Card

For rapid geometry construction:

```
STAIRS (Game Standard - IBC Residential):
  Riser:  18cm (7 inches)
  Tread:  28cm (11 inches)
  Angle:  32.7 degrees
  Nosing: 2-3cm overhang
  Width:  90cm minimum (interior), 60cm minimum (fire escape)

  For 270cm floor height:
    Steps per flight:     15
    Full run length:      15 * 28 = 420cm
    Switchback run:       8 * 28 = 224cm per half-flight
    Switchback width:     2 * 90 + 10 gap = 190cm

FIRE ESCAPE STAIRS (IBC Maximum):
  Riser:  20cm (8 inches)
  Tread:  20cm (8 inches)
  Angle:  45 degrees
  Width:  56cm (22 inches) minimum

ADA RAMP:
  Slope:          1:12 (4.76 degrees)
  Max rise/run:   76cm
  Run per 76cm:   912cm (!)
  Landing:        150cm x 150cm
  Width:          120cm (91.4cm clear minimum)
  Handrails:      86-97cm height

  For 270cm rise:
    Segments: 4
    Run per segment: 810cm
    Total plan footprint (switchback): 960cm x 528cm

UE5 CHARACTER:
  MaxStepHeight:       45cm
  WalkableFloorAngle:  44.76 degrees
  Capsule height:      176cm (half-height 88cm)
  Capsule radius:      34cm
  Min headroom:        210cm (clearance above walkable surface)
```

---

## Sources

- [US Access Board - Chapter 4: Ramps and Curb Ramps](https://www.access-board.gov/ada/guides/chapter-4-ramps-and-curb-ramps/)
- [ADA Ramp Requirements & Standards](https://www.accessibilitychecker.org/blog/ada-requirements-for-ramps/)
- [Ramp Switchback Dimensions](https://www.dimensions.com/element/ramp-landings-switchback)
- [ADA Ramp Compliance](http://www.ada-compliance.com/ada-compliance/ada-ramp)
- [IBC Stairs Code & Requirements](https://upsideinnovations.com/blog/ibc-stairs-code/)
- [IBC Commercial Stair Codes Explained](https://buildingcodetrainer.com/commercial-stair-codes-explained/)
- [2024 IBC Section 1011.5.2 - Riser Height](https://codes.iccsafe.org/s/IBC2024P1/chapter-10-means-of-egress/IBC2024P1-Ch10-Sec1011.5.2)
- [Fire Escapes - UpCodes](https://up.codes/s/fire-escapes-fire-stairs-and-fire-towers)
- [Standard Stair Angle Reference](https://www.lapeyrestair.com/blog/standard-stair-angle/)
- [UE5 Walkable Slope Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/walkable-slope-in-unreal-engine)
- [UE5 CharacterMovementComponent API](https://dev.epicgames.com/documentation/en-us/unreal-engine/python-api/class/CharacterMovementComponent?application_version=5.4)
- [UE5 Scale and Dimensions Guide](https://www.worldofleveldesign.com/categories/ue5/guide-to-scale-dimensions-proportions.php)
