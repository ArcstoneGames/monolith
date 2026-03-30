# Procedural Town Generator v3 -- Master Plan

**Date:** 2026-03-28
**Status:** APPROVED WITH CHANGES -- Both reviewers approved. Fixes applied below.
**Scope:** Fix all remaining building generation issues, upgrade architecture to single-pass integrated facades, improve floor plans, add horror subversion, wire validation
**Estimated Total:** ~140-185 hours across 7 work packages, 4 phases (revised per R1 estimate correction)
**Research Basis:** 15 research documents (see appendix)
**Excludes:** Modular kit scanner (separate plan)

---

## Reviewer Fixes Applied

The following changes are incorporated from Review #1 (Technical) and Review #2 (Player/Design):

### From Review #1 — Technical Corrections
- **R1-C1: Wall tessellation for Selection+Inset.** WP-1 must pre-subdivide wall slabs before selecting sub-regions. `AppendBox` with 0 subdivisions produces 12 triangles — can't isolate a window region. Fix: use `ApplyMeshPlaneCut` to insert edge loops at window boundaries (horizontal cuts at sill and lintel heights, vertical cuts at window edges). This happens BEFORE the Selection+Inset step. Added as explicit Step 0 in WP-1.
- **R1-C2: Shrunk selection box computation.** The second selection (for deleting the inner face after inset) must use `WindowBox.ExpandBy(-InsetDistance)`, not a hardcoded smaller box. InsetDistance comes from the facade style's `frame_width` parameter. Added to WP-1 pseudocode.
- **R1-I1: Fix Plan v2 relationship.** v3 SUPERSEDES v2. v2's Phase 1-3 fixes are already committed. v3 builds on top.
- **R1-I2: Header file conflict.** WP-1 and WP-2 both touch `MonolithMeshBuildingTypes.h`. Fix: ALL header changes batched into WP-1. WP-2 uses the updated header but doesn't modify it.
- **R1-I3: Estimate correction.** Revised total from 110-140h to 140-185h (15-20% increase per reviewer assessment).
- **R1-I4: UV continuity.** Post-inset UV discontinuity risk. Fix: re-project UVs via `SetMeshUVsFromBoxProjection` after all Selection+Inset operations, same as current boolean pipeline.

### From Review #2 — Player/Design Corrections
- **R2-C1: Hospice minimum widths at generation time.** WP-2 must enforce minimum corridor width (150cm normal, 200cm hospice) and door width (110cm normal, 120cm hospice) DURING floor plan generation, not just flagged during validation. Added as hard constraints in `generate_floor_plan`.
- **R2-C2: Elevator shaft option.** Added `elevator_shaft` as a new room type alongside `stairwell` in `FBuildingDescriptor`. Multi-floor archetypes can specify `"vertical_access": "stairs"`, `"elevator"`, or `"both"`. Elevator is a simple shaft (no stair geometry) with a floor hole at each level. Added to WP-3.
- **R2-C3: Exterior connectivity.** Street/sidewalk generation between buildings is handled by the existing SP5 `create_street` action + the PCG roads system in the MonolithPCG plan. Added explicit cross-reference to SP5 and the PCG roads sub-plan.
- **R2-I1: Single-floor-completable tagging.** Added `single_floor_completable` flag to Building Descriptor. When true, all critical rooms are on ground floor. Encounter designers use this to avoid locking progression behind stairs.
- **R2-I2: Multiple entrances.** Large buildings (>15 rooms or >2 floors) get 2+ exterior entrances on different walls. Added to WP-2 entrance generation.

---

## Architecture Decision: Single-Pass Integrated (Option A)

### Why Not Modular HISM (Option B)?

Both approaches eliminate booleans for window generation and fix the facade alignment problem. The decision comes down to implementation cost and risk:

| Factor | Single-Pass (A) | Modular HISM (B) |
|--------|-----------------|-------------------|
| Code reuse | HIGH -- existing facade code becomes utility functions called inline | LOW -- new piece generation pipeline, new assembly logic, new registry |
| Effort | ~30-40h for facade integration | ~50-60h for piece gen + ~30h assembly + ~20h registry = ~100h |
| Risk | LOW -- proven by THE FINALS, CGA, bendemott | MEDIUM -- new architecture, new failure modes |
| Existing infrastructure | `FExteriorFaceDef`, `BuildWallSlab`, `CutOpenings`, `ComputeWindowPositions` all reusable | `SaveMeshToAsset` and `ConvertToHism` exist but untested for this workflow |
| Window quality | Selection+Inset replaces boolean (20x faster, clean topology) | Pre-baked into pieces (same Selection+Inset, but runs once at piece gen time) |
| Iteration speed | Regenerate building = new windows | Regenerate pieces + reassemble = slower cycle |

**Decision: Option A (Single-Pass Integrated)** with Selection+Inset replacing booleans for windows. This fixes all 18 remaining issues with ~40% less effort than Option B, reuses existing facade code, and matches the industry consensus (THE FINALS, CGA, bendemott).

Option B remains the correct long-term architecture. When the modular kit scanner lands (separate plan), it will naturally evolve toward HISM assembly. This plan does not block that evolution.

### Selection+Inset: The Boolean Replacement

Every research document agrees: replace `ApplyMeshBoolean(Subtract)` with `SelectMeshElementsInBox` + `SelectMeshElementsByNormalAngle` + `CombineMeshSelections` + `ApplyMeshInsetOutsetFaces` + `DeleteSelectedTrianglesFromMesh`.

**Performance:** 0.1-0.5ms vs 2-5ms per opening. 60 exterior faces x 3 windows = ~30-90ms total (vs 360-900ms with booleans).

**Quality:** No T-junctions, no degenerate triangles, no non-manifold edges. Frame geometry created natively by the inset operation. Clean UVs.

**Fallback:** If Selection+Inset has edge cases on complex geometry, fall back to per-segment boolean (already working, just slower). The single-pass architecture doesn't change either way.

---

## Remaining Issues Mapped to Work Packages

| # | Issue | WP | Priority |
|---|-------|----|----------|
| 1 | Facade is separate mesh actor | WP-1 | P0 |
| 2 | Windows use boolean subtract | WP-1 | P0 |
| 3 | Building is one monolithic blob | Deferred | P2 |
| 4 | No adjacency MUST_NOT enforcement | WP-2 | P0 |
| 5 | No public-to-private gradient | WP-2 | P0 |
| 6 | No wet wall clustering | WP-2 | P1 |
| 7 | No structural grid snapping | WP-2 | P2 |
| 8 | Corridor-only circulation | WP-2 | P1 |
| 9 | No windows on buildings | WP-1 | P0 |
| 10 | Roofs disabled | WP-3 | P1 |
| 11 | Stairwell switchback 4x6 minimum | WP-3 | P1 |
| 12 | No exterior entrance door geometry | WP-3 | P1 |
| 13 | validate_building not wired | WP-4 | P0 |
| 14 | No multi-angle captures for QA | WP-5 | P2 |
| 15 | No capsule sweep in pipeline | WP-4 | P0 |
| 16 | No horror subversion patterns | WP-6 | P1 |
| 17 | No tension curve integration | WP-6 | P1 |
| 18 | No Space Syntax scoring | WP-6 | P2 |

Issue #3 (monolithic blob / HISM assembly) is deferred to the modular kit scanner plan.

---

## File Conflict Analysis

Files touched by multiple work packages:

| File | WP-1 | WP-2 | WP-3 | WP-4 | WP-5 | WP-6 | WP-7 |
|------|-------|-------|-------|-------|-------|-------|-------|
| `MonolithMeshBuildingActions.cpp` | **W** | | R | R | | | R |
| `MonolithMeshFacadeActions.cpp` | **W** | | | | | | |
| `MonolithMeshFloorPlanGenerator.cpp` | | **W** | | R | | **W** | |
| `MonolithMeshBuildingTypes.h` | **W** | **W** | | | | **W** | |
| `MonolithMeshCityBlockActions.cpp` | | | | | | | **W** |
| `MonolithMeshBuildingValidationActions.cpp` | | | | **W** | | | |
| `MonolithMeshRoofActions.cpp` | | | **W** | | | | |

**Conflict resolution:**
- `MonolithMeshBuildingTypes.h` touched by WP-1, WP-2, WP-6. **Serialize:** WP-1 adds facade fields first, WP-2 adds adjacency types after WP-1, WP-6 adds horror fields after WP-2.
- `MonolithMeshFloorPlanGenerator.cpp` touched by WP-2 and WP-6. **Serialize:** WP-2 (core floor plan logic) runs before WP-6 (horror post-processing).
- All other files have single writers.

---

## Phase 1: Integrated Facades + Selection+Inset (WP-1)

**Issues fixed:** #1, #2, #9
**Estimate:** 35-45 hours
**Agent:** `unreal-mesh-expert`
**Research docs:** `facade-alignment-research.md`, `geometryscript-deep-dive-research.md`, `modular-pieces-research.md`

This is the critical path. Everything else can ship without this, but this is what makes buildings look like buildings.

### WP-1a: Extract Facade Utilities (8-10h)

**Files modified:**
- `MonolithMeshFacadeActions.h` -- make key functions `public static` if not already
- New file: `MonolithMeshFacadeUtils.h` (shared header)

**What to do:**

1. Audit existing facade functions in `FMonolithMeshFacadeActions`:
   - `BuildWallSlab()` -- generates a wall box for one exterior face
   - `ComputeWindowPositions()` -- calculates window placement from face dims + style
   - `CutOpenings()` -- boolean subtract of windows/doors (TO BE REPLACED)
   - `AddWindowFrames()` -- appends frame geometry around openings
   - `AddDoorFrames()` -- appends door frame geometry
   - `AddGlassPanes()` -- appends glass quad inside window openings
   - `AddCornice()` -- appends cornice at roofline
   - `AddBeltCourse()` -- appends horizontal trim between floors

2. Create `MonolithMeshFacadeUtils.h` with these functions accessible outside the facade action class. Either:
   - Move to a `FMonolithFacadeUtils` static class, OR
   - Make them `static` members and forward-declare in the new header
   - The functions already take `UDynamicMesh*` as first param -- no signature changes needed

3. Create `CutOpeningsSelectionInset()` as a NEW function alongside existing `CutOpenings()`:

```
CutOpeningsSelectionInset(UDynamicMesh* Mesh, FExteriorFaceDef Face,
    TArray<FWindowPlacement> Windows, TArray<FDoorPlacement> Doors)
{
    For each window:
        // Select front-facing tris in window AABB
        SelectMeshElementsInBox(Mesh, BoxSel, WindowBox, Triangle)
        SelectMeshElementsByNormalAngle(Mesh, NormSel, Face.Normal, 30deg)
        CombineMeshSelections(BoxSel, NormSel, FinalSel, Intersection)

        // Inset to create frame geometry
        ApplyMeshInsetOutsetFaces(Mesh, PerFace, FinalSel, 5.0cm)

        // Delete inset faces to create opening
        SelectMeshElementsInBox(Mesh, InnerSel, ShrunkWindowBox, Triangle)
        SelectMeshElementsByNormalAngle(Mesh, InnerNorm, Face.Normal, 30deg)
        CombineMeshSelections(InnerSel, InnerNorm, DeleteSel, Intersection)
        DeleteSelectedTrianglesFromMesh(Mesh, DeleteSel)

    For each door:
        // Same pattern but full-height opening, no sill
}
```

4. Keep `CutOpenings()` (boolean version) as fallback, gated by a param `bUseSelectionInset = true`.

**Done when:**
- `CutOpeningsSelectionInset()` produces clean window openings in an isolated test wall slab
- Frame geometry has correct MaterialID (separate from wall material)
- All facade utility functions callable from outside `FMonolithMeshFacadeActions`
- Existing `generate_facade` action still works unchanged (regression test)

### WP-1b: Integrate Facade Into Building Generator (20-25h)

**Files modified:**
- `MonolithMeshBuildingActions.cpp` -- main integration
- `MonolithMeshBuildingTypes.h` -- add facade fields to descriptor

**What to do:**

1. Add parameters to `create_building_from_grid`:
   - `facade_style` (string, optional) -- loads JSON facade preset (same format as existing styles)
   - `facade_seed` (int, optional) -- variation seed
   - When `facade_style` is provided, automatically set `omit_exterior_walls = true`

2. Add to `FBuildingDescriptor`:
   ```
   FString FacadeStyle;
   TArray<FWindowPlacement> Windows;  // emitted for downstream consumers
   TArray<FDoorPlacement> EntranceDoors; // exterior door positions
   ```

3. In the per-floor wall generation loop, after `GenerateWallGeometry()` for interior walls:
   ```
   if (bHasFacadeStyle)
   {
       FFacadeStyle Style = LoadFacadeStyle(FacadeStyleName);
       for (const FExteriorFaceDef& Face : ThisFloorExteriorFaces)
       {
           // Build the exterior wall slab (facade IS the wall)
           FacadeUtils::BuildWallSlab(BuildingMesh, Face, Style);

           // Compute and cut windows
           auto Windows = FacadeUtils::ComputeWindowPositions(Face, Style, FacadeSeed);
           FacadeUtils::CutOpeningsSelectionInset(BuildingMesh, Face, Windows, Doors);

           // Add trim and glass
           FacadeUtils::AddWindowFrames(BuildingMesh, Face, Windows, Style);
           FacadeUtils::AddGlassPanes(BuildingMesh, Face, Windows, Style);

           // Accumulate window metadata for descriptor
           AllWindows.Append(Windows);
       }
       // Per-floor horizontal trim
       FacadeUtils::AddBeltCourse(BuildingMesh, FloorZ, Style);
   }
   else
   {
       // No facade: generate solid exterior walls as before (existing code path)
       GenerateExteriorWallGeometry(BuildingMesh, ...);
   }
   ```

4. After all floors: add cornice at roofline if facade style present.

5. Emit window and entrance door metadata in the Building Descriptor JSON output.

6. Box UV projection on facade wall segments for proper tiling.

**Done when:**
- `create_building_from_grid` with `facade_style: "colonial"` produces a single mesh with real window holes
- Camera placed inside a room can see through windows to the exterior
- No double walls, no z-fighting
- Material IDs: wall=0, floor=1, ceiling=2, trim=3, glass=4, door_frame=5 (or whatever the existing scheme is)
- Performance: single 3-story building with windows generates in <10s
- Building Descriptor contains window position metadata

### WP-1c: City Block Integration (6-8h)

**Files modified:**
- `MonolithMeshCityBlockActions.cpp`

**What to do:**

1. Remove the `if (false && !bSkipFacades)` guard (line ~1459)
2. Instead of calling separate `generate_facade` action, pass `facade_style` through to `create_building_from_grid`
3. Support per-building facade style override in the city block spec JSON:
   ```json
   {
     "buildings": [
       { "facade_style": "colonial", "facade_seed": 42 },
       { "facade_style": "victorian" },
       { "facade_style": "industrial" }
     ]
   }
   ```
4. Default facade style in block preset if not specified per-building
5. Uncomment `omit_exterior_walls` in the orchestrator (it was commented out per handover)

**Done when:**
- `create_city_block` generates a 4-building block where all buildings have windows
- Each building can have a different facade style
- No separate facade actors spawned
- Block generation in <60s

---

## Phase 2: Floor Plan Intelligence (WP-2)

**Issues fixed:** #4, #5, #6, #7, #8
**Estimate:** 25-35 hours
**Agent:** `unreal-mesh-expert`
**Research docs:** `real-floorplan-patterns-research.md`, `advanced-procgen-research.md`
**Depends on:** None (parallel with Phase 1)

### WP-2: Adjacency + Circulation + Gradient (25-35h)

**Files modified:**
- `MonolithMeshFloorPlanGenerator.cpp` -- core changes
- `MonolithMeshBuildingTypes.h` -- adjacency types
- Archetype JSONs in `Content/Data/BuildingArchetypes/` -- add adjacency matrices

**What to do:**

#### 2.1 Adjacency Matrix Types (in `MonolithMeshBuildingTypes.h`)

Add enum and struct:
```
enum class EAdjacencyRule : uint8 { MUST, SHOULD, MAY, MAY_NOT, MUST_NOT };

struct FAdjacencyMatrix
{
    TMap<FString, TMap<FString, EAdjacencyRule>> Rules;

    EAdjacencyRule GetRule(const FString& RoomTypeA, const FString& RoomTypeB) const;
    bool Violates(const FString& A, const FString& B) const; // true if MUST_NOT and adjacent
};
```

#### 2.2 Adjacency Enforcement in Floor Plan Generator

In `MonolithMeshFloorPlanGenerator.cpp`, after room placement (constrained growth / treemap):

```
ValidateAdjacency(FloorPlan, AdjacencyMatrix):
    For each pair of adjacent rooms (sharing a wall):
        rule = Matrix.GetRule(RoomA.Type, RoomB.Type)
        if rule == MUST_NOT:
            // Attempt swap: find a non-violating room to swap with
            // If no swap possible: regenerate with different seed
            return FAIL

    For each MUST pair in the matrix:
        if !AreAdjacent(RoomA, RoomB):
            // Attempt to move one room closer
            // If impossible: regenerate
            return FAIL
```

Run this as a post-placement validation pass with up to 3 retries before falling back.

#### 2.3 Public-to-Private Gradient

Add `EPrivacyZone` enum: `PUBLIC, SEMI_PUBLIC, SEMI_PRIVATE, PRIVATE, SERVICE`.

Map room types to privacy zones:
- PUBLIC: entry, foyer, lobby, living_room
- SEMI_PUBLIC: dining_room, family_room
- SEMI_PRIVATE: hallway, corridor, kitchen
- PRIVATE: bedroom, bathroom, office
- SERVICE: laundry, utility, mechanical, storage

**Enforcement in floor plan generator:**
1. Entry room must be PUBLIC zone
2. Rooms reachable in 1 step from entry must be PUBLIC or SEMI_PUBLIC
3. PRIVATE rooms must be >= 2 graph steps from entry
4. Compute graph depth from entry; rooms are placed with increasing privacy at greater depth

#### 2.4 Wet Wall Clustering

During room placement, after placing a bathroom or kitchen:
- Check if another wet room exists within 2 cells
- If yes: prefer placing the new wet room adjacent to the existing one (share a wall)
- If no: mark the wet room's wall as a "plumbing chase" and prefer placing future wet rooms nearby

Implementation: Add a scoring bonus (+50 weight) when a wet room candidate placement shares a wall with an existing wet room. The constrained growth algorithm already has a scoring step -- this is an additional term.

#### 2.5 Circulation Pattern Support

Currently: corridor-only (double-loaded). Add:

**Hub-and-spoke:** For residential buildings.
- Place entry/foyer as hub room (larger, central)
- Rooms connect directly to hub (no corridor needed for <6 rooms)
- Add corridor only when rooms > hub perimeter capacity

**Racetrack/Loop:** For office/commercial buildings.
- Reserve central core cells for stairs/elevator/restrooms
- Generate corridor as a loop around the core
- Rooms fill perimeter between corridor and exterior walls

**Enfilade:** For horror mansion wings.
- Rooms connect sequentially through aligned doors (no corridor)
- Door positions align on a single axis per wing

Map to archetype via `circulation_type` field in archetype JSON:
```json
{
  "circulation_type": "hub_and_spoke",  // or "double_loaded", "racetrack", "enfilade", "tree"
  ...
}
```

The floor plan generator dispatches to the appropriate layout algorithm based on this field.

#### 2.6 Structural Grid Snapping

Add `structural_bay` field to archetype (default: 300cm residential, 750cm commercial).
After room placement: snap room boundaries to nearest grid line within 50cm tolerance. This is a post-processing pass that nudges walls, not a constraint during placement.

#### 2.7 Update Archetype JSONs

Add to all 10 existing archetypes:
- `adjacency` matrix (per Section 21.1 of floorplan research)
- `circulation_type` field
- `privacy_gradient` boolean (default true)
- `structural_bay` field
- `wet_wall_clustering` boolean (default true)

**Done when:**
- `generate_floor_plan` with `ranch_house` archetype: entry connects to living room, bedrooms in wing off hallway, kitchen adjacent to dining, no bedroom-bedroom pass-through
- `generate_floor_plan` with `office` archetype: racetrack corridor around central core
- MUST_NOT violations (bathroom->kitchen) do not occur across 20 test generations
- Private rooms (bedrooms) are always >= 2 steps from entry
- Bathrooms cluster on shared walls

---

## Phase 2 (continued): Building Quality (WP-3)

**Issues fixed:** #10, #11, #12
**Estimate:** 12-16 hours
**Agent:** `unreal-mesh-expert`
**Depends on:** WP-1 (needs facade integration for entrance door placement)

### WP-3: Roofs + Stairwell + Entrance Door (12-16h)

**Files modified:**
- `MonolithMeshRoofActions.cpp` -- height offset fix
- `MonolithMeshBuildingActions.cpp` -- stairwell + entrance door
- `MonolithMeshCityBlockActions.cpp` -- re-enable roof in orchestrator

#### 3.1 Re-enable Roofs with Correct Height

The `create_roof` action exists and works, but the orchestrator passes an incorrect Z offset. Fix:
- Roof Z = `building_origin.Z + (num_floors * floor_height)`
- Account for ceiling thickness (currently ignored)
- Verify roof footprint polygon matches building exterior footprint
- Re-enable roof call in `create_city_block` orchestrator

#### 3.2 Stairwell Switchback with 4x6 Minimum

Fix Plan v2 established the 4x6 cell minimum (24 cells) and the switchback algorithm. Verify it's correctly implemented:
- Two half-flights: each 2 cells wide, 3 cells deep
- Landing between flights at mid-height
- 10cm gap between flights (for railing)
- Floor cutout on upper floor aligns with stairwell cells
- If footprint < 4x6 cells: reject and do not generate stairs (hard reject per R2-I5)

#### 3.3 Exterior Entrance Door Geometry

Currently buildings have entrance openings but no door geometry. Add:
- At the primary entrance (identified by `FDoorDef` where `RoomB == "exterior"`):
  - Generate door frame geometry (swept profile, same as interior door frames)
  - Generate threshold/step if floor Z > ground Z
  - Add stoop/landing if more than 1 step needed (max 3 steps before requiring a ramp)
  - Position the entrance door frame as part of the building mesh (single-pass)

The entrance door position is already computed by the floor plan generator and stored in the Building Descriptor. This task adds the visual geometry.

**Done when:**
- Roofs sit at correct height on 1, 2, and 3-story buildings
- Stairwells are 4x6 minimum, switchback stairs at ~32deg angle
- Entrance has visible door frame geometry and threshold/steps
- All three features work in a `create_city_block` run

---

## Phase 3: Validation Pipeline (WP-4)

**Issues fixed:** #13, #15
**Estimate:** 12-16 hours
**Agent:** `unreal-mesh-expert`
**Research docs:** `procgen-validation-research.md`
**Depends on:** Phase 1 + Phase 2 (validates their output)

### WP-4: Wire Validation Into Pipeline (12-16h)

**Files modified:**
- `MonolithMeshBuildingValidationActions.cpp` -- expand existing action
- `MonolithMeshCityBlockActions.cpp` -- wire into orchestrator

#### 4.1 Tier 1: Fast Programmatic Checks (in `validate_building`)

The `validate_building` action exists but isn't wired. Expand it with:

**Capsule sweep through doors:**
```
For each door in Building Descriptor:
    sweep_start = room_a_center offset toward door
    sweep_end = room_b_center offset toward door
    capsule = (42cm radius, 96cm half-height)
    if SweepSingleByChannel blocks: FAIL with door_id + blocking geometry
```

**Connectivity flood fill:**
```
BFS from entrance room through door adjacency graph
Any room not reached = FAIL
```

**Window ray check (new -- validates facade integration):**
```
For each window in Building Descriptor:
    ray from 50cm interior to 200cm exterior along face normal
    if blocked: FAIL (window not fully cut through)
```

**Stair angle check:**
```
For each stairwell:
    angle = atan2(floor_height, stair_run)
    if angle > 45deg: WARN
    if angle > 60deg: FAIL
```

**Mesh topology (optional, add if time permits):**
```
CheckValidity() on the building mesh
IsClosed() -- should be false (buildings have openings) but no non-manifold edges
```

#### 4.2 Tier 2: Spatial Analysis

**NavMesh connectivity (if navmesh available):**
```
For each room: FindPathSync(entrance, room_center)
Any path failure = FAIL
```

**Room size sanity:**
```
For each room: area = grid_cells * cell_size^2
if area < minimum for room_type: WARN
```

#### 4.3 Wire Into Orchestrator

In `create_city_block`, after each building is generated:
```
validation_result = validate_building(building_descriptor)
if FAIL:
    if retries < 3:
        regenerate with different seed
    else:
        log warning, skip building
```

**Done when:**
- `validate_building` catches: blocked doors, disconnected rooms, failed window cuts, steep stairs
- Orchestrator retries on validation failure (up to 3 attempts)
- Validation report is JSON with per-check pass/fail and details
- Capsule sweep correctly flags a door that's too narrow (test with 80cm door vs 84cm capsule)

---

## Phase 3 (continued): Multi-Angle QA Capture (WP-5)

**Issues fixed:** #14
**Estimate:** 14-18 hours
**Agent:** `unreal-mesh-expert`
**Research docs:** `ai-vision-upgrade-research.md`, `procgen-validation-research.md`
**Depends on:** Phase 1 (captures buildings with windows)

### WP-5: Automated Multi-Angle Capture (14-18h)

**Files modified:**
- New file: `MonolithMeshCaptureActions.cpp` (or extend `MonolithMeshDebugViewActions.cpp`)

**New MCP action: `capture_building_qa`**

Parameters:
- `building_id` -- which building to capture
- `view_set` -- "minimal" (6 views) or "extended" (12+ views)
- `resolution` -- default 1024x1024
- `output_dir` -- where to save PNGs
- `capture_depth` -- boolean, also capture depth buffer

#### Algorithm:

1. Read Building Descriptor for building bounds and room data
2. Compute camera positions:
   - **Top-down orthographic:** Center above building, OrthoWidth = max(bounds.X, bounds.Y) * 1.2
   - **4 cardinal elevations:** Distance = max(bounds.X, bounds.Y), at mid-height, facing building center
   - **First-person entrance:** Eye height (170cm), at entrance door position, facing inward
   - Extended: 4 corners, per-room interiors, through-door views
3. For each camera position:
   - Spawn `USceneCaptureComponent2D` on a temp actor
   - Set `CaptureSource = SCS_FinalColorLDR`
   - Set render target `UTextureRenderTarget2D` at requested resolution
   - `CaptureScene()` or `CaptureSceneDeferred()`
   - Export RT to PNG via `FImageUtils::ExportRenderTarget2DAsHDR` or texture export
   - If `capture_depth`: additional capture with `SCS_SceneDepth`
4. Generate contact sheet (NxM grid of all views in one image) -- optional
5. Return JSON with paths to all captured images + camera metadata

**Done when:**
- `capture_building_qa` produces 6 PNG images for a generated building
- Top-down view shows room layout clearly
- Cardinal views show windows and facade
- First-person view from entrance shows interior
- All images saved to disk with descriptive filenames
- Action returns JSON manifest of captured images

---

## Phase 3 (continued): Horror Subversion (WP-6)

**Issues fixed:** #16, #17, #18
**Estimate:** 18-24 hours
**Agent:** `unreal-mesh-expert`
**Research docs:** `real-floorplan-patterns-research.md` (Section 18), `advanced-procgen-research.md` (Sections 6.2, 6.6, 7)
**Depends on:** WP-2 (needs adjacency + gradient as baseline to subvert)

### WP-6: Horror Post-Processing + Space Syntax (18-24h)

**Files modified:**
- `MonolithMeshFloorPlanGenerator.cpp` -- horror modifier post-pass
- `MonolithMeshBuildingTypes.h` -- horror config struct
- New file or extend existing: `MonolithMeshHorrorFloorPlanActions.cpp` (if splitting is cleaner)
- Archetype JSONs -- add horror modifier presets

#### 6.1 Horror Modifier System

Add `FHorrorModifiers` struct to `MonolithMeshBuildingTypes.h`:
```
struct FHorrorModifiers
{
    float DoorLockRatio = 0.0f;         // 0.0 = all open, 0.6 = 60% locked
    float LoopBreakChance = 0.0f;       // chance to collapse a circulation loop
    int32 ImpossibleDepthFloors = 0;    // extra basement floors beyond expected
    bool bServiceReveal = false;         // add hidden service corridors
    bool bAsymmetricWing = false;        // one wing diverges from layout
    FString WrongRoomType;              // room type that doesn't belong (e.g., "laboratory")
    float DeadEndRatioTarget = 0.3f;    // target dead-end room percentage
    float CorridorStretchFactor = 1.0f; // 1.5 = corridors 50% longer
};
```

#### 6.2 Horror Post-Processing Pass

After a "normal" floor plan is generated and validated (WP-2 + WP-4), apply horror modifiers:

**Blocked routes:**
```
locked_count = 0
target = floor(total_doors * DoorLockRatio)
For each door NOT on the critical path (BFS from entrance):
    if locked_count < target:
        door.bTraversable = false
        door.bLocked = true  // new field
        locked_count++
```
Critical: never lock a door that would make any room unreachable. Run BFS after each lock to verify connectivity.

**Loop breaking:**
```
For each loop in the room adjacency graph:
    if random() < LoopBreakChance:
        Find the edge that, if removed, maximizes path length increase
        Remove that edge (collapse the wall, remove the door)
        // Player must go the long way
```

**Dead-end ratio adjustment:**
```
current_ratio = count_dead_end_rooms() / total_rooms
while current_ratio < DeadEndRatioTarget:
    Find a room with 2+ doors that isn't the last connection for another room
    Lock one of its non-critical doors
    current_ratio = recount()
```

**Corridor stretch:**
```
For each corridor room:
    if CorridorStretchFactor > 1.0:
        Extend corridor by (length * (factor - 1)) in its long axis
        Shift downstream rooms to accommodate
```
This one is tricky with grid-based layout. Simpler implementation: when generating corridor, allocate `ceil(normal_cells * CorridorStretchFactor)` cells instead of normal count.

**Wrong room insertion:**
```
if WrongRoomType is set:
    Find the deepest room (most graph steps from entry)
    Replace its room_type with WrongRoomType
    // A laboratory under a house, an operating theater in a school
```

#### 6.3 Space Syntax Scoring

Implement basic Space Syntax metrics on the room adjacency graph:

**Integration (closedness):** For each room, compute mean shortest path to all other rooms. Normalize. High integration = hub, low = isolated.

**Connectivity:** For each room, count direct neighbors.

**Depth from entry:** BFS depth from entrance room.

```
SpaceSyntaxScore ComputeSpaceSyntax(FloorPlan):
    graph = BuildAdjacencyGraph(FloorPlan.Rooms, FloorPlan.Doors)

    For each room:
        integration[room] = 1.0 / mean_shortest_path_to_all(room, graph)
        connectivity[room] = degree(room, graph)
        depth[room] = bfs_depth(entrance, room, graph)

    return {
        avg_integration,
        max_depth,
        dead_end_ratio,
        hub_count (rooms with connectivity >= 3),
        integration_gradient (correlation of integration with depth -- should be negative for horror)
    }
```

**Integration into generation loop:**
After generating a floor plan, compute Space Syntax score. If horror modifiers are active:
- `integration_gradient` should be negative (decreasing accessibility deeper in)
- `dead_end_ratio` should be >= `DeadEndRatioTarget`
- `max_depth` should be >= 4 for small buildings, >= 6 for large

If score doesn't match: retry with different seed (up to 3 attempts).

#### 6.4 Tension Curve Metadata

Don't generate the tension curve here (that's the AI Director's job), but emit the data it needs:

Add to Building Descriptor:
```
TArray<FRoomTensionData> RoomTensionHints;

struct FRoomTensionData
{
    FString RoomId;
    float DepthFromEntry;     // graph depth
    float Integration;         // Space Syntax integration value
    int32 Connectivity;        // number of exits
    bool bDeadEnd;            // only 1 exit
    bool bOnCriticalPath;     // required for traversal
    FString PrivacyZone;      // PUBLIC through SERVICE
};
```

This metadata lets the AI Director or encounter placement system decide WHERE to put enemies, items, and scares based on architectural properties.

**Done when:**
- Horror modifiers can lock 60% of doors while maintaining full connectivity
- Loop breaking increases max path length by 30%+ when applied
- Space Syntax metrics compute in <1ms for buildings with <50 rooms
- `RoomTensionHints` emitted in Building Descriptor
- 20 test generations with horror modifiers produce no disconnected rooms
- Dead-end ratio matches target within 10%

---

## Phase 4: Integration + Polish (WP-7)

**Estimate:** 8-12 hours
**Agent:** `unreal-mesh-expert`
**Depends on:** All previous phases

### WP-7: End-to-End Integration (8-12h)

**Files modified:**
- `MonolithMeshCityBlockActions.cpp` -- full orchestrator update
- Archetype JSONs -- final polish

**What to do:**

1. Update `create_city_block` orchestrator to wire all new systems:
   ```
   For each building slot:
       1. generate_floor_plan(archetype, seed)  // WP-2: adjacency, gradient, circulation
       2. apply_horror_modifiers(floor_plan, horror_config)  // WP-6: if horror enabled
       3. create_building_from_grid(floor_plan, facade_style)  // WP-1: integrated facade
       4. validate_building(descriptor)  // WP-4: capsule sweep, connectivity
       5. if validation fails: retry (up to 3x with different seed)
       6. create_roof(descriptor)  // WP-3: correct height
       7. optionally: capture_building_qa(descriptor)  // WP-5: QA images
   ```

2. Test full pipeline with each of the 10 archetypes:
   - `ranch_house`, `two_story_house`, `victorian_mansion`
   - `apartment_unit`, `apartment_building`
   - `office`, `hospital`, `police_station`
   - `school`, `warehouse`

3. Fix any integration bugs discovered during end-to-end testing

4. Update block presets (`residential_block.json`, `commercial_block.json`) with facade styles and horror modifiers

5. Performance budget verification: full 4-building block in <90s

**Done when:**
- `create_city_block` with residential preset generates 4 houses with windows, roofs, correct floor plans, validated
- `create_city_block` with commercial preset generates 4 offices with racetrack corridors and facade windows
- Horror mode: locked doors, broken loops, tension hints in descriptor
- No separate facade actors anywhere
- Total block gen time <90s
- `capture_building_qa` produces useful QA images of the generated block

---

## Phase / Dependency Summary

```
Phase 1 (parallel):
    WP-1a (facade utils)     -----> WP-1b (building integration) -----> WP-1c (city block)
    WP-2  (floor plan intel)  -----> (independent)

Phase 2 (after Phase 1 WP-1b):
    WP-3  (roofs + stairwell + entrance)

Phase 2 (after WP-2):
    WP-6  (horror subversion)

Phase 3 (after WP-1c + WP-2):
    WP-4  (validation pipeline)
    WP-5  (QA capture system)     -- can start after WP-1b

Phase 4 (after all):
    WP-7  (integration)
```

**Critical path:** WP-1a -> WP-1b -> WP-1c -> WP-7
**Parallel track A:** WP-2 -> WP-6
**Parallel track B:** WP-3 (after WP-1b)
**Parallel track C:** WP-5 (after WP-1b)
**Joins at:** WP-4 (needs WP-1 + WP-2), WP-7 (needs everything)

**Maximum parallelism: 2 agents** (one on WP-1 series, one on WP-2 -> WP-6). At Phase 3, up to 3 agents (WP-4 + WP-5 + WP-3 if WP-1b is done).

---

## Effort Summary

| WP | Description | Hours | Agent | Phase |
|----|-------------|-------|-------|-------|
| WP-1a | Facade utility extraction + Selection+Inset | 8-10 | `unreal-mesh-expert` | 1 |
| WP-1b | Integrate facade into building generator | 20-25 | `unreal-mesh-expert` | 1 |
| WP-1c | City block orchestrator integration | 6-8 | `unreal-mesh-expert` | 1 |
| WP-2 | Floor plan adjacency + gradient + circulation | 25-35 | `unreal-mesh-expert` | 1 (parallel) |
| WP-3 | Roofs + stairwell + entrance door | 12-16 | `unreal-mesh-expert` | 2 |
| WP-4 | Validation pipeline + orchestrator wiring | 12-16 | `unreal-mesh-expert` | 3 |
| WP-5 | Multi-angle QA capture system | 14-18 | `unreal-mesh-expert` | 3 |
| WP-6 | Horror subversion + Space Syntax | 18-24 | `unreal-mesh-expert` | 2-3 |
| WP-7 | End-to-end integration | 8-12 | `unreal-mesh-expert` | 4 |
| **Total** | | **123-164h** | | |

---

## Risks

1. **Selection+Inset edge cases.** The GeometryScript `ApplyMeshInsetOutsetFaces` function operates on selected triangles. If the wall slab has too few subdivisions, the box selection may not cleanly isolate the window region. Mitigation: generate wall slabs with subdivision matching window grid (same technique as the pre-tessellated approach in modular-pieces-research Section 1.5).

2. **FloorPlan generator complexity.** WP-2 adds 5 new concerns (adjacency, gradient, wet walls, circulation, grid snapping) to an already complex generator. Mitigation: implement as separate validation/post-processing passes, not inline with placement logic. Each concern is a pure function that scores or rejects a placement.

3. **Orchestrator retry budget.** With adjacency enforcement + horror modifiers + validation, some archetype/footprint combinations may fail repeatedly. Mitigation: cap retries at 3 per building, fall back to simpler archetype (small_house) on repeated failure. Log failures for offline analysis.

4. **File conflicts between WP-1 and WP-2 on Types.h.** Both add fields to the header. Mitigation: WP-1 runs first (or at least commits first). WP-2 adds its fields to the already-updated header. Alternatively, batch both header changes in WP-1a since it runs first.

5. **Performance regression.** Selection+Inset is faster than booleans per-opening, but we're now doing MORE work per building (facade integrated). Overall gen time should be similar or faster (eliminated separate facade actor creation). Budget: single building <10s, 4-building block <90s.

6. **Horror modifiers disconnecting rooms.** Door locking and loop breaking could create unreachable rooms if not carefully implemented. Mitigation: BFS connectivity check AFTER every modification. Never lock a bridge edge (an edge whose removal disconnects the graph). Use Tarjan's bridge-finding algorithm.

---

## Appendix: Research Documents Referenced

All in `Plugins/Monolith/Docs/plans/`:

| Document | Key Contribution to Plan |
|----------|-------------------------|
| `2026-03-28-facade-alignment-research.md` | Architecture decision (single-pass), integration algorithm |
| `2026-03-28-geometryscript-deep-dive-research.md` | Selection+Inset API, MeshModelingFunctions, MeshSelectionFunctions |
| `2026-03-28-ue5-mesh-apis-research.md` | Full API inventory, priority matrix |
| `2026-03-28-real-floorplan-patterns-research.md` | Adjacency matrices, circulation patterns, horror subversion, privacy gradient |
| `2026-03-28-advanced-procgen-research.md` | Constrained growth, Space Syntax, horror fitness functions |
| `2026-03-28-procgen-validation-research.md` | 3-tier validation, capsule sweep, flood fill, mesh topology |
| `2026-03-28-ai-vision-upgrade-research.md` | Multi-angle capture, SceneCaptureComponent2D config, G-buffer |
| `2026-03-28-modular-pieces-research.md` | Selection+Inset algorithm for windows, piece generation pipeline |
| `2026-03-28-modular-building-research.md` | AAA modular approach reference (Bethesda, Arkane, Embark) |
| `2026-03-28-hism-assembly-research.md` | HISM API reference (deferred to modular kit scanner plan) |
| `2026-03-28-town-gen-fix-plan-v2.md` | Context on what's already fixed (20 issues) |
| `2026-03-28-town-gen-session-2-handover.md` | Current state: 46 actions, facades disabled, validation unwired |

---

## Review #1

**Reviewer:** `unreal-code-reviewer` (independent)
**Date:** 2026-03-29
**Verdict:** APPROVE WITH CHANGES (2 Critical, 4 Important, 5 Suggestions)

### What's Done Well

This is a thorough plan. The research backing is excellent -- 15 documents, direct header reads of the GeometryScript API, real production references (THE FINALS, CGA, bendemott). The issue-to-WP mapping is clean, the file conflict analysis is proactive, and the dependency graph is correctly structured. The decision to defer modular HISM (Issue #3) while keeping the door open is pragmatic.

### 1. Issue Coverage (18 of 18)

All 18 issues are mapped to work packages. Issue #3 is explicitly deferred with justification. No gaps.

One observation: Issues #1-3 and #5-8 from the Fix Plan v2 (stair angles, room sizes, ramp switchback) overlap with v3's scope. The plan references v2 in the appendix but does not state whether v2 fixes are a prerequisite or whether v3 supersedes v2. If v2 is not yet fully executed, WP-3 (stairwell switchback) and WP-2 (floor plan intelligence) may be building on broken foundations.

**[Important-1]** Clarify the v2 relationship explicitly. Add a "Prerequisites" section stating which v2 work packages must be complete before v3 begins, or mark v3 as superseding v2 entirely and absorb the remaining v2 work.

### 2. Architecture Decision: Single-Pass

The single-pass integrated approach is the correct call. The comparison matrix is honest, the industry precedent is strong, and the code reuse argument holds up -- I verified that `BuildWallSlab`, `CutOpenings`, `AddWindowFrames`, `AddGlassPanes`, `AddCornice`, and `AddBeltCourse` in `MonolithMeshFacadeActions.h` all take `UDynamicMesh*` as their first parameter and operate on `FExteriorFaceDef` inputs. They genuinely can be called from inside the building generator loop without signature changes.

The deferral of modular HISM is also correct. Building the single-pass system first gives you a working baseline that the modular system can later replace piece-by-piece.

No issues here.

### 3. File Conflicts

The conflict matrix (lines 74-87) identifies `MonolithMeshBuildingTypes.h` as the hot file (WP-1, WP-2, WP-6). The serialization strategy (WP-1 first, WP-2 second, WP-6 third) is sound.

However, the plan says WP-1 and WP-2 run in parallel (Phase 1). If both touch `MonolithMeshBuildingTypes.h`, parallel execution requires coordination. The plan says "WP-1 adds facade fields first" but in a parallel phase, "first" has no guaranteed ordering unless explicitly enforced.

**[Important-2]** Either (a) batch ALL `MonolithMeshBuildingTypes.h` changes into WP-1a (which runs before WP-1b and WP-2), or (b) state that WP-2 must not start its Types.h modifications until WP-1a commits. The plan already suggests option (a) in Risk #4 -- promote that from "alternatively" to the actual plan.

### 4. Algorithm Correctness: Selection+Inset Pipeline

This is the highest-risk item in the plan and warrants the most scrutiny.

The proposed pipeline (lines 123-147):
1. `SelectMeshElementsInBox` to pick wall tris in the window AABB
2. `SelectMeshElementsByNormalAngle` to filter to front-facing tris
3. `CombineMeshSelections` (intersection)
4. `ApplyMeshInsetOutsetFaces` to create the frame border
5. Select the inner region again (shrunk box)
6. `DeleteSelectedTrianglesFromMesh` to punch the hole

**[Critical-1]** The plan assumes wall slabs generated by `AppendBox` will have sufficient triangle subdivision for box-selection to cleanly isolate a window-sized region. A standard `AppendBox` with 0 subdivisions produces exactly 12 triangles (2 per face). `SelectMeshElementsInBox` on a window-sized sub-region of a 6-triangle front face will select either zero triangles (box smaller than any triangle) or all front-face triangles (box intersects all of them). The inset would then operate on the entire face, not a window-sized region.

Risk #1 acknowledges this ("generate wall slabs with subdivision matching window grid") but the WP-1a spec does not include this as an explicit implementation step. The `BuildWallSlab` function currently calls `AppendBox` -- it needs to either:
- Add `SubdivisionsWidth` / `SubdivisionsHeight` params to `AppendBox` calls matching the window grid, OR
- Generate the wall as a subdivided plane via `AppendRectangleXY` with sufficient edge loops, then extrude for thickness, OR
- Use `ApplyMeshPlaneCut` to insert edge loops at window boundaries before selection.

Add a concrete step in WP-1a specifying HOW wall slabs get pre-tessellated. Without this, Selection+Inset will fail on the first attempt.

**[Critical-2]** The double-selection pattern (steps 4-6: inset, then re-select inner, then delete) has a sequencing problem. After `ApplyMeshInsetOutsetFaces`, the mesh topology has changed -- new vertices and triangles exist where the inset created the frame border. The second `SelectMeshElementsInBox` with a "shrunk" box needs to precisely match the new inner face geometry. But the inset distance is 5cm, so the shrunk box must be exactly `WindowBox` contracted by 5cm on each side. If the inset produced slightly different geometry (e.g., the inset distance is approximate due to triangle shapes), the second selection may miss triangles or grab frame tris.

This is not a dealbreaker -- it should work on cleanly subdivided rectangular geometry. But the plan should acknowledge that the shrunk box dimensions must be derived from the inset distance, not hardcoded. Add a note that `ShrunkWindowBox = WindowBox.ExpandBy(-InsetDistance)` and that this must be computed, not constant.

### 5. Time Estimates

| WP | Plan Estimate | Assessment |
|----|--------------|------------|
| WP-1a | 8-10h | Reasonable. Utility extraction is mechanical. Selection+Inset is new ground -- could slip to 12h if edge cases arise. |
| WP-1b | 20-25h | Aggressive. This is the core integration and involves modifying `CreateBuildingFromGrid` which is already ~1300 lines. I would budget 25-30h. |
| WP-1c | 6-8h | Fair. Straightforward orchestrator wiring. |
| WP-2 | 25-35h | This is actually 5 sub-features (adjacency, gradient, wet walls, circulation, grid snapping). 25h is optimistic; 35h is realistic. The circulation pattern support alone (hub-and-spoke, racetrack, enfilade) is 3 distinct layout algorithms. |
| WP-3 | 12-16h | Fair. Roof re-enablement is simple; stairwell verification depends on v2 status; entrance door geometry is moderate. |
| WP-4 | 12-16h | Fair. Capsule sweep and flood fill are well-defined. |
| WP-5 | 14-18h | Possibly over-scoped for a QA tool. A simpler version (4 cardinal views + top-down) could ship in 8-10h. |
| WP-6 | 18-24h | Fair for the scope. Space Syntax is algorithmically simple but the horror modifiers (loop breaking, dead-end tuning) need careful connectivity preservation. |
| WP-7 | 8-12h | Integration always takes longer than expected. 12-16h would be safer. |

**[Important-3]** Total plan range is 123-164h. Realistic range is closer to 140-185h. The critical path (WP-1a -> WP-1b -> WP-1c -> WP-7) alone is 42-55h estimated, likely 50-65h actual. Flag this so scheduling expectations are calibrated.

### 6. Test Criteria

Each WP has a "Done when" section with concrete, verifiable criteria. This is good. Specific callouts:

- WP-1b: "Camera placed inside a room can see through windows to the exterior" -- excellent, this is the real test.
- WP-4: "Capsule sweep correctly flags a door that's too narrow (test with 80cm door vs 84cm capsule)" -- specific negative test, good.
- WP-6: "20 test generations with horror modifiers produce no disconnected rooms" -- statistical test, good.

**[Suggestion-1]** WP-1a's "Done when" says "produces clean window openings in an isolated test wall slab" but does not specify what "clean" means. Add: "No T-junctions, no degenerate triangles (area < 0.01 cm^2), no non-manifold edges. Validate with `CheckValidity()` returning no errors."

**[Suggestion-2]** WP-2's "Done when" tests specific archetypes (ranch_house, office) but does not include a regression test for the existing floor plan behavior without the new features. Add: "Existing `generate_floor_plan` calls without adjacency/circulation params produce identical output to pre-v3 behavior."

### 7. Missing Risks

**[Important-4]** The plan does not address UV continuity across the Selection+Inset boundary. When `ApplyMeshInsetOutsetFaces` creates new frame geometry, the UVs on those new faces are controlled by the `UVScale` option in `FGeometryScriptMeshInsetOutsetFacesOptions`. If the wall slab already has box-projected UVs, the inset faces may have discontinuous UVs at the frame/wall boundary. For architectural trim this might be acceptable (frame gets its own MaterialID anyway), but for the wall surface around the window, UV seams could be visible. Add a risk entry noting that a UV repair pass (re-project box UVs after inset) may be needed.

**[Suggestion-3]** The plan assigns all 7 WPs to `unreal-mesh-expert`. WP-2 (floor plan intelligence) and WP-6 (horror subversion) are fundamentally graph algorithm work, not mesh work. Consider whether `unreal-ai-expert` or a general algorithms agent would be better suited for the adjacency matrix enforcement, Space Syntax scoring, and Tarjan's bridge-finding algorithm. The mesh expert is correct for WP-1, WP-3, WP-5, and the geometry portions of WP-4.

**[Suggestion-4]** No rollback strategy is documented. If WP-1b (the biggest, riskiest WP) fails mid-implementation and leaves `CreateBuildingFromGrid` in a broken state, what's the recovery plan? Recommend: WP-1b should be implemented behind a feature flag (`bIntegratedFacade`, defaulting to false) so the old code path remains functional until the new path is validated. Flip the default only after WP-1b's "Done when" criteria all pass.

**[Suggestion-5]** The plan does not mention material slot count limits. The current facade style uses MaterialIDs 0-7 (8 slots). The building generator already uses IDs 0-2 (wall, floor, ceiling). The integrated approach needs a unified material ID scheme. WP-1b's "Done when" lists "Material IDs: wall=0, floor=1, ceiling=2, trim=3, glass=4, door_frame=5 (or whatever the existing scheme is)" -- the "(or whatever)" is concerning. Nail down the material ID map in WP-1a so WP-1b implements against a fixed contract.

### 8. Dependencies

The dependency graph (lines 773-798) is correctly structured. The critical path identification is accurate. One note:

WP-4 (validation) depends on Phase 1 + Phase 2, but the "window ray check" sub-test in WP-4 specifically validates that facade integration worked (ray from interior through window). This means WP-4 cannot be fully tested until WP-1b is complete and generating real windows. The dependency is correctly captured but worth highlighting: WP-4 development can start in parallel (capsule sweep, flood fill don't need facades), but the window ray check is blocked on WP-1b.

This is already implicitly handled by the phase structure. No action needed, just confirming correctness.

### Summary of Action Items

| ID | Severity | Item |
|----|----------|------|
| Critical-1 | Must fix | Specify HOW wall slabs get pre-tessellated for Selection+Inset. `AppendBox` with 0 subdivisions will not work. Add explicit subdivision step to WP-1a. |
| Critical-2 | Must fix | Document that `ShrunkWindowBox` must be computed from inset distance, not hardcoded. Add note to WP-1a pseudocode. |
| Important-1 | Should fix | Clarify Fix Plan v2 relationship -- prerequisite or superseded? |
| Important-2 | Should fix | Resolve parallel Types.h conflict -- batch all header changes into WP-1a. |
| Important-3 | Should fix | Adjust total hour estimate upward (~140-185h realistic). Flag critical path at 50-65h. |
| Important-4 | Should fix | Add UV continuity risk for Selection+Inset frame geometry. |
| Suggestion-1 | Nice to have | Define "clean" in WP-1a done criteria with specific mesh validity checks. |
| Suggestion-2 | Nice to have | Add regression test for existing floor plan behavior. |
| Suggestion-3 | Nice to have | Consider different agents for graph-algorithm WPs (WP-2, WP-6). |
| Suggestion-4 | Nice to have | Add feature flag rollback strategy for WP-1b. |
| Suggestion-5 | Nice to have | Lock down material ID map in WP-1a as a contract for WP-1b. |

---

## Review #2: Player Experience, Horror Design, and Hospice Accessibility

**Reviewer:** Independent Review (Code Reviewer agent)
**Date:** 2026-03-29
**Perspective:** Player experience, horror design, hospice accessibility, scale
**Verdict:** APPROVE WITH CHANGES (3 Critical, 5 Important, 4 Suggestions)

### What This Plan Gets Right

This is an impressively well-researched plan. 15 research documents feeding into 7 work packages, with real architectural principles (Kahn's served/servant distinction, the public-to-private gradient, wet wall clustering) and direct references to production systems (Shadows of Doubt, THE FINALS, Spelunky, Resident Evil). The architecture decision for single-pass integrated facades is clearly justified. The Selection+Inset approach for windows is backed by concrete performance numbers. The file conflict analysis and dependency graph show mature planning.

The horror design work is particularly strong -- Space Syntax integration, the RE recursive unlocking pattern, tension curve metadata for AI Director consumption, and quantifiable horror metrics (corridor L:W ratios, dead-end percentages, isovist areas). The tiered validation pipeline is well-structured.

### 1. Player Experience -- Will Buildings Feel Natural?

**Assessment: Strong foundation, two gaps.**

The adjacency matrix system (MUST/SHOULD/MAY/MAY_NOT/MUST_NOT), public-to-private gradient, wet wall clustering, and circulation pattern taxonomy are exactly the right architectural foundations. The research correctly identifies the 10 "dead giveaways" of proc-gen buildings and the plan systematically addresses each one.

**[Important-1] Door Placement Within Walls.** The research (Section 1, point 3) explicitly calls out "real doors are placed to preserve wall space for furniture. A bedroom door goes to one side, not dead center." WP-2 addresses adjacency, gradient, wet walls, circulation, and grid snapping -- but door offset within a wall is never specified. Center-of-wall doors on short walls are a major proc-gen tell. Add a door placement heuristic: offset doors toward the corner nearest the corridor, leaving 60-90cm of wall on the hinge side and full remaining wall opposite for furniture.

**[Important-2] Room Aspect Ratios in Growth Phase.** The research documents ideal aspect ratios per room type (living room 1:1.2-1.5, galley kitchen 1:2-2.5, bathroom 1:1.5-2.0). WP-4 includes room size validation, but aspect ratio needs to be enforced DURING constrained growth (WP-2), not just flagged after the plan is fully generated. Reject a room expansion that would push its aspect ratio past the allowed range for its type. Post-validation is too late -- the whole floor plan would need regeneration.

**Navigation clarity:** The circulation pattern support is good. Players in hub-and-spoke houses will orient naturally. One gap: the Building Descriptor should tag rooms with a `navigation_role` field (hub, branch, terminus, connector) alongside the existing `PrivacyZone`. This lets downstream systems (signage placement, lighting emphasis, AI patrol routes) reinforce wayfinding without analyzing the full graph themselves.

### 2. Hospice Accessibility

**Assessment: Significant gap that needs addressing.**

The plan mentions a ramp once (WP-3, Section 3.3: "max 3 steps before requiring a ramp") and the validation research mentions wheelchair capsule sweeps (60cm radius). There is no systematic accessibility consideration in any work package.

This is a game for hospice patients. While the player character is presumably able-bodied in fiction, the spaces still affect gameplay accessibility for players who may have motor or cognitive impairments.

**[Critical-1] Minimum Widths Enforced in Generation.** The capsule sweep (WP-4) uses a 42cm radius (84cm diameter) for traversal validation. But minimum COMFORTABLE widths need enforcement during floor plan generation (WP-2), not just validation:
- Corridors: 120cm minimum. The research lists 0.9-1.2m for residential hallways -- the lower end is too tight for FPS gameplay where players strafe and may use imprecise input devices.
- Doors: 90cm minimum clear width. Standard ADA, and it prevents the FPS frustration of snagging on door frames.
- These are generation constraints (reject a corridor placement below minimum width), not validation checks (flag it after the building exists).

**[Critical-2] Multi-Story Access Alternatives.** Buildings with stairs-only vertical circulation create gameplay chokepoints that may be disproportionately difficult for hospice patients with reduced dexterity (stair combat requires quick vertical aim adjustment). Two recommendations:
- Emit an `elevator_shaft` option alongside stairwells in the Building Descriptor. Encounter designers can then ensure critical-path items are reachable without stair traversal.
- For the floor plan generator: tag buildings where ALL critical-path content can be completed on a single floor. The horror modifier system could use this -- locking upper floors behind optional exploration rather than mandatory progression.

This is not about simulating wheelchair access in-game. It is about ensuring level design does not force high-dexterity sequences for core progression.

**[Important-3] Cognitive Accessibility.** The existing Monolith action `validate_navigation_complexity` scores cognitive difficulty of routes. The plan should wire this into WP-4's validation pipeline with a maximum threshold. Buildings that are architecturally correct but cognitively overwhelming (too many identical doors, no visual landmark positions, excessive depth) are a design failure for this audience. Add `validate_navigation_complexity` as a Tier 2 check with a configurable threshold per archetype.

### 3. Horror Design

**Assessment: Excellent, one design gap.**

The horror subversion system (WP-6) is well-designed. Door locking with critical-path preservation (BFS after each lock), loop breaking, dead-end ratio adjustment, corridor stretching, and "wrong room" insertion are strong horror tools. The Space Syntax scoring to verify pacing is sophisticated. The tension curve metadata gives the AI Director rich data.

**[Important-4] Sightline Modification Pass.** Horror is as much about what the player CAN see as where they CAN go. The horror modifiers alter topology (lock doors, break loops) but not geometry or visibility. WP-6 should include:
- A corridor L-bend insertion pass: convert straight corridors into L or S shapes by shifting a segment, breaking long sightlines. This is the single most effective horror geometry trick -- you cannot see what is around the corner.
- Emit `sightline_data` per room in the Building Descriptor: longest visible distance from room center, number of visible exits, and whether the room has a "blind corner" (an entrance not visible from another entrance). This lets the AI Director place scares at sightline transitions without re-running isovist analysis.

The existing `analyze_sightlines` and `analyze_choke_points` Monolith actions could provide this data, but computing it during generation and baking it into the descriptor avoids runtime analysis cost.

**[Suggestion-1] Asymmetric Wing Detail.** The `bAsymmetricWing` horror modifier exists in the `FHorrorModifiers` struct but its implementation is not specified in WP-6's algorithms. Define concretely: does one wing get extra rooms? Different room types? A different floor height? A slightly different grid alignment (1-2 degree rotation for subliminal unease)? The uncanny valley of architecture -- a wing that LOOKS the same from outside but is WRONG inside -- is a powerful horror tool that deserves a paragraph of implementation detail.

### 4. Performance

**Assessment: Well-budgeted for editor-time.**

Performance budgets are clear: single building <10s, 4-building block <90s. The component costs are well-understood (Selection+Inset 30-90ms, capsule sweep 2ms, Space Syntax <1ms). No concerns for the stated editor-time workflow.

**[Important-5] Runtime Scope Statement.** The plan never explicitly states whether runtime generation is in-scope or out-of-scope. If the game ever needs to generate buildings as the player approaches a new area, the 10s budget is too slow. The plan should include one sentence in the architecture section: either "All generation is editor-time only; runtime loads pre-baked static meshes" or "Runtime generation is a future goal; WP-X identifies the bottlenecks." Without this, someone will assume runtime works and hit a wall.

### 5. Archetype Diversity

**Assessment: 10 is a solid starting set.**

Ranch house, two-story house, Victorian mansion, apartment unit, apartment building, office, hospital, police station, school, warehouse -- good spread across residential, commercial, institutional, industrial.

**[Suggestion-2] Add Retail Storefront.** For survival horror, shops are iconic exploration targets (pharmacy for supplies, hardware store for weapons). A storefront archetype is structurally simple (open retail floor + back room + optional upstairs office) and high-value for gameplay variety.

**[Suggestion-3] Add Church/Chapel.** The research describes this archetype in detail (narthex-nave-chancel sequence maps directly to the enfilade circulation pattern). For horror, churches are high-impact locations. Even one per town justifies the archetype.

### 6. What Is Missing

**[Critical-3] Exterior Connectivity Between Buildings.** The plan covers individual buildings and the city block orchestrator places multiple buildings. But there is no mention of sidewalks, streets, parking lots, or exterior navigation surfaces connecting buildings. A player in a 20-building town needs to walk between them. If this is handled by a separate system (city block exterior layout), it should be explicitly cross-referenced here. If it is not handled anywhere, it is a critical gap -- buildings without connecting paths are islands.

**[Important-5 continued] Furniture Placement Interface.** The plan generates empty shells. The research (advanced-procgen Section 4.2) mentions PCG integration for interior population. At minimum, the Building Descriptor should define the metadata a furniture placement system needs:
- Floor surface polygon per room (for valid placement area)
- "Primary wall" designation per room (the long wall opposite the door -- where beds, desks, TVs go)
- Clearance zones around doors and windows (no-furniture buffers)
Room type + dimensions + door/window positions are already emitted, but the above three fields would make furniture placement a straightforward downstream consumer instead of a separate research project.

**[Suggestion-4] Sound and Lighting Metadata.** Two low-cost, high-value additions to the Building Descriptor:
- **Sound:** Wall thickness and material type per wall segment, plus door open/closed state, for audio occlusion computation. The adjacency graph edges already exist -- tagging them with occlusion properties is minimal work.
- **Lighting:** Window positions are already planned. Add light fixture mounting points (ceiling center per room) and a `natural_light_level` estimate (window count x orientation factor) so the lighting system can create horror-appropriate illumination without a separate analysis pass.

### 7. Integration with Existing Game Systems

**Assessment: Good internally, under-specified externally.**

Monolith-internal integration is solid: the orchestrator wires floor plan, facade, validation, and capture into a coherent pipeline with the Building Descriptor as the data contract.

Connections to game systems that need clarification:
- **AI Director:** `FRoomTensionData` is well-defined as output, but how does the AI Director consume it? Is there a known interface, or is this speculative metadata hoping a consumer will materialize?
- **Save System:** If the game saves building state (locked/unlocked doors, destroyed walls), the Building Descriptor needs stable room/door IDs that survive serialization. `FString RoomId` is used -- confirm these are deterministic given the same seed, or document that they are not.
- **Navigation System:** Validation builds NavMesh, but does the runtime use pre-baked NavMesh per building or rebuild on load? For 20+ buildings, NavMesh memory and rebuild cost matter for AI pathfinding.

### 8. Scale -- 20+ Building Town

**Assessment: Viable with caveats.**

At <90s per 4-building block, a 5-block town (20 buildings) is ~7.5 minutes editor-time. Acceptable for level baking.

Concerns at scale:
- **Triangle budget:** The plan should specify expected per-building triangle counts. Rough estimate: 5K-20K triangles per building shell depending on window count and trim. At 20 buildings, that is 100K-400K triangles for shells alone, before furniture and props.
- **Retry budget:** 3 retries per building x 20 buildings = worst case 60 generation attempts at 10s each = 10 minutes of retries. Consider a global retry budget (fail the block after N total failures across all buildings) rather than per-building.
- **Variety at scale:** 10 archetypes with seed variation is reasonable, but players will notice if all ranch houses have the same room count. Archetype JSONs should support room count ranges (2-4 bedrooms) and multiple circulation options per archetype to increase combinatorial variety.

### Summary Table

| ID | Severity | Area | Action |
|----|----------|------|--------|
| Critical-1 | Must fix | Accessibility | Enforce minimum corridor (120cm) and door (90cm) widths in WP-2 generation, not just WP-4 validation |
| Critical-2 | Must fix | Accessibility | Add elevator shaft option and single-floor-completable tagging for multi-story buildings |
| Critical-3 | Must fix | Scale | Add exterior connectivity plan or cross-reference for between-building navigation |
| Important-1 | Should fix | Player experience | Add door placement offset heuristic to WP-2 (doors toward corners, not wall centers) |
| Important-2 | Should fix | Player experience | Enforce room aspect ratio constraints during constrained growth, not just post-validation |
| Important-3 | Should fix | Accessibility | Wire `validate_navigation_complexity` into WP-4 Tier 2 with configurable threshold |
| Important-4 | Should fix | Horror | Add sightline modification pass to WP-6 (L-bend insertion, sightline metadata per room) |
| Important-5 | Should fix | Architecture | Explicitly state runtime vs. editor-time scope; define furniture placement metadata interface |
| Suggestion-1 | Nice to have | Horror | Detail `bAsymmetricWing` implementation in WP-6 |
| Suggestion-2 | Nice to have | Diversity | Add retail storefront archetype |
| Suggestion-3 | Nice to have | Diversity | Add church/chapel archetype |
| Suggestion-4 | Nice to have | Integration | Add sound occlusion and lighting metadata to Building Descriptor |
