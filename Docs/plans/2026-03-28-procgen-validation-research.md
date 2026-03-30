# Procedural Content Validation Research

**Date:** 2026-03-28
**Scope:** Visual validation, AI-assisted QA, programmatic verification, and self-correcting generation for procedural buildings in Monolith/Leviathan.

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Scene Capture + Vision Model Validation](#2-scene-capture--vision-model-validation)
3. [Programmatic Validation Without Vision](#3-programmatic-validation-without-vision)
4. [Industry Approaches to Proc-Gen Validation](#4-industry-approaches-to-proc-gen-validation)
5. [Viewport Capture for Automated Review](#5-viewport-capture-for-automated-review)
6. [Self-Correcting Generation](#6-self-correcting-generation)
7. [Existing Monolith Tools](#7-existing-monolith-tools)
8. [Proposed Validation Architecture](#8-proposed-validation-architecture)
9. [Effort Estimates](#9-effort-estimates)
10. [Sources](#10-sources)

---

## 1. Executive Summary

Procedural building validation splits into two fundamentally different problems:

**Geometric correctness** (are booleans clean? are walls solid?) -- best solved programmatically with capsule sweeps, ray traces, navmesh connectivity, and topology checks. Fast, deterministic, zero cost per run.

**Aesthetic/semantic correctness** (does this look like a building? is the door proportioned right? would a player find this space navigable?) -- benefits from vision model analysis but is slower, costlier, and not yet reliable enough for autonomous deployment.

**Recommendation:** Build a 3-tier validation pipeline:
1. **Tier 1 -- Fast programmatic checks** (sub-second): capsule sweeps through doors, flood-fill connectivity, mesh topology, collision verification. Run on every generation.
2. **Tier 2 -- NavMesh + spatial analysis** (~1-3s): build navmesh, verify all rooms connected, check path widths, validate stair angles. Run on every generation.
3. **Tier 3 -- Vision model review** (5-30s, optional): capture orthographic + perspective views, send to Claude/GPT-4V for semantic review. Run on demand or nightly batch.

---

## 2. Scene Capture + Vision Model Validation

### 2.1 The Vision Model Landscape (2026)

**VideoGameQA-Bench** (NeurIPS 2025) is the definitive benchmark -- 9 tasks, 4,786 questions, 11 proprietary + 5 open-weight models evaluated. Key findings:

| Task | Best Model | Accuracy | Notes |
|------|-----------|----------|-------|
| Image glitch detection | GPT-4o | 82.8% | Good at obvious visual errors |
| Video glitch detection | Gemini-2.5-Pro | 78.1% | Frame-by-frame analysis |
| Visual unit testing | Gemini-2.5-Pro | 53% | *Poor* -- struggles with precise spatial details |
| UI unit testing | Best model | 40% | *Very poor* |
| Visual regression testing | o4-mini | 45.2% | *Worst category* -- misses subtle changes |
| Bug report generation | GPT-4o | 54% | Useful but noisy |

**Critical finding:** At realistic 5% defect prevalence, GPT-4o's 17.8% false-positive rate means ~5 false alarms per real issue. Not viable for autonomous deployment without human review.

**What VLMs struggle with for our use case:**
- Precise spatial measurements (is this door exactly 90cm wide?)
- Object clipping in borderline cases
- Common-sense reasoning about architecture (is this staircase at a walkable angle?)
- Fine-grained topology (did the boolean fully punch through?)

**What VLMs are good at for our use case:**
- Obvious visual errors (floating geometry, missing walls, z-fighting)
- General composition review (does this look like a building?)
- Identifying egregiously wrong proportions
- Natural language critique that helps iterate

### 2.2 Model Options and Cost/Speed Tradeoffs

| Model | Cost per image | Latency | Quality | Local? |
|-------|---------------|---------|---------|--------|
| Claude Sonnet 4 | ~$0.003-0.01 | 2-5s | High | No |
| Claude Opus 4 | ~$0.015-0.05 | 3-8s | Highest | No |
| GPT-4o | ~$0.005-0.015 | 2-5s | High | No |
| Gemini 2.5 Pro | ~$0.003-0.01 | 2-5s | High | No |
| LLaVA 1.6 34B (Ollama) | Free | 5-15s | Medium | Yes (16GB+ RAM) |
| LLaVA 1.6 13B (Ollama) | Free | 2-8s | Lower | Yes (8GB+ RAM) |
| Qwen-2.5-VL 72B | Free | 10-30s | Medium-High | Yes (48GB+ VRAM) |

**Recommendation:** Use Claude Sonnet 4 for cloud validation (best balance of quality/cost) with LLaVA 34B as offline fallback. At $0.01/image and 6 views per building, that's $0.06/building -- $6 for 100 buildings in a batch test.

### 2.3 Prompt Engineering for Procedural Content Review

**Structured prompt template for building validation:**

```
You are a QA inspector for procedurally generated game buildings. Analyze this {view_type} view of a building and report issues.

Building spec: {room_count} rooms, {floor_count} floors, {door_count} doors, {window_count} windows.

Check for:
1. DOORS: Are door openings fully cut through walls? Are they proportioned for a human (min 80cm wide, 200cm tall)?
2. WINDOWS: Are window openings complete? Any partially-cut geometry?
3. WALLS: Any gaps, z-fighting, or interpenetrating geometry?
4. STAIRS: Are stairs at a walkable angle (< 45 degrees)? Consistent step heights?
5. FLOORS: Any holes or missing floor sections?
6. PROPORTIONS: Do rooms feel reasonably sized? Any impossibly thin corridors?
7. GEOMETRY ARTIFACTS: Floating geometry, inverted normals, dark patches suggesting missing faces?

For each issue found, report:
- Severity: CRITICAL (blocks gameplay), WARNING (visual artifact), INFO (minor)
- Location: describe where in the image
- Description: what's wrong
- Suggested fix: if obvious

If everything looks correct, say "PASS" with confidence level (high/medium/low).
```

**Multi-view capture strategy:**
- Top-down orthographic (floor plan) -- room connectivity, wall completeness
- 4 cardinal exterior elevations -- facade/window/door completeness
- Per-room interior perspective -- proportions, door passability, floor integrity
- Through-door views -- verify openings are clear
- Stairwell views -- step consistency, angle

### 2.4 Integration Architecture

```
Generate Building
    |
    v
[Tier 1: Programmatic] -- instant, always runs
    |
    v
[Tier 2: NavMesh/Spatial] -- 1-3s, always runs
    |
    v
Pass? --> Done (most buildings)
    |
    No / Optional deep check
    v
[Tier 3: Vision Model] -- 5-30s, on-demand
    |
    Spawn SceneCapture2D actors
    Capture 6-12 views to PNG
    Send to vision API
    Parse structured response
    |
    v
Validation Report (JSON)
```

---

## 3. Programmatic Validation Without Vision

### 3.1 Traversal Testing (Capsule Sweep Through Doors)

**Approach:** For each door opening in the spatial registry, sweep a player-sized capsule (radius 42cm, half-height 96cm -- standard UE pawn) through the opening from one room to the adjacent room.

**UE5 API:** `UWorld::SweepSingleByChannel()` with `FCollisionShape::MakeCapsule(42.f, 96.f)`

**Implementation:**
```
For each door in spatial_registry:
    room_a_center = get_room_center(door.room_a)
    room_b_center = get_room_center(door.room_b)
    door_center = door.world_position

    // Sweep from room A through door to room B
    sweep_start = lerp(room_a_center, door_center, 0.8)  // just inside room A
    sweep_end = lerp(door_center, room_b_center, 0.2)    // just inside room B

    hit = SweepSingleByChannel(sweep_start, sweep_end, capsule, Pawn)
    if (hit.bBlockingHit):
        flag_door_blocked(door, hit.ImpactPoint, hit.GetActor())
```

**Edge cases:**
- Double doors: sweep with wider capsule or two parallel sweeps
- Wheelchair accessibility: sweep with 60cm radius capsule (120cm diameter)
- Angled doors: orient capsule along door normal, not room-to-room vector

**Performance:** ~0.1ms per door. 20 doors per building = 2ms total. Negligible.

### 3.2 Connectivity Validation (Flood Fill from Entrance)

**Approach:** Starting from the building entrance, flood-fill through the room adjacency graph. Any room not reached is disconnected.

**Algorithm:**
```
visited = set()
queue = [entrance_room_id]

while queue:
    room = queue.pop(0)
    if room in visited: continue
    visited.add(room)
    for neighbor in adjacency_graph[room]:
        if neighbor not in visited:
            // Verify the connecting door is actually passable (Tier 1 capsule check)
            if door_between(room, neighbor).passable:
                queue.append(neighbor)

disconnected = all_rooms - visited
if disconnected:
    flag_disconnected_rooms(disconnected)
```

**Enhancement -- NavMesh-based connectivity:**
After building navmesh, query path from entrance to every room center. This catches cases where the adjacency graph says rooms are connected but geometry actually blocks the path.

```
For each room in building:
    path = query_navmesh(entrance, room.center)
    if path.failed:
        flag_room_unreachable(room)
```

**Performance:** Adjacency flood-fill is O(N) where N = rooms. NavMesh pathfinding ~1-5ms per query. 20 rooms = 20-100ms. Acceptable.

### 3.3 Line-of-Sight Through Windows

**Approach:** For each window opening, cast a ray from inside the room through the window center to the exterior. If blocked, the boolean failed or geometry is occluding.

```
For each window in spatial_registry:
    interior_point = window.position - window.normal * 50  // 50cm inside
    exterior_point = window.position + window.normal * 200  // 200cm outside

    hit = raycast(interior_point, exterior_point, channel=Visibility)
    if hit.bBlockingHit and hit.Distance < expected_wall_thickness + tolerance:
        flag_window_blocked(window, hit)
```

**Multi-ray variant for robustness:** Cast a grid of rays (3x3 or 5x5) across the window opening. If >50% are blocked, the boolean likely failed.

### 3.4 Mesh Topology Checks

**GeometryScript provides (verified in UE 5.7 source):**
- `GetMeshVolumeArea()` -- returns volume and surface area; volume near zero indicates degenerate mesh
- `GetMeshBoundingBox()` -- sanity check dimensions
- `GetNumVertexIDs()` / `GetNumTriangleIDs()` -- vertex/triangle counts for complexity budget
- `GetTriangleNormal()` -- per-triangle normal for detecting inverted faces
- `GetMeshHasAttributeSet()` -- verify UVs/normals exist

**Not directly exposed but available via FDynamicMesh3 C++ API:**
- `FDynamicMesh3::IsCompact()` -- no unused vertex/triangle slots
- `FDynamicMesh3::IsClosed()` -- all edges have exactly 2 adjacent triangles (watertight)
- `FDynamicMesh3::CheckValidity()` -- comprehensive topology validation
- `FMeshRepairFunctions` -- fix degenerate triangles, fill holes, repair non-manifold edges
- Euler characteristic: V - E + F = 2 for genus-0 closed mesh (sphere topology)
- Genus calculation: g = 1 - (V - E + F)/2 -- genus > 0 means through-holes exist

**Validation checks to implement:**
1. **Closed mesh test:** `IsClosed()` -- if false after booleans, the subtract didn't fully punch through
2. **Volume sanity:** Volume should be positive and within expected range for room dimensions
3. **Inverted normals:** Sample triangle normals; if any point inward on exterior faces, flag it
4. **Degenerate triangles:** Area < epsilon, usually from boolean edge cases
5. **Non-manifold edges:** Edges with >2 adjacent triangles indicate boolean artifacts

### 3.5 NavMesh Validation

**After navmesh build:**
1. **Connected components:** Use flood-fill on navmesh polys from entrance. Multiple components = disconnected areas.
2. **Path to every room:** `UNavigationSystemV1::FindPathSync()` from entrance to each room center.
3. **Stair validation:** Stairs at < 44 degrees auto-navigate. Check that navmesh connects floors via stairwells.
4. **Width validation:** NavMesh poly width at doorways should exceed agent radius * 2.

**UE5 API:** `UNavigationSystemV1::GetNavigationSystem(World)->FindPathSync()` returns `FPathFindingResult` with path points and status.

### 3.6 Collision Sweep Comprehensive

**Full building walkthrough simulation:**
```
Generate a path visiting every room (TSP-approximation or just BFS order)
For each path segment:
    Sweep player capsule along the path
    If blocked, record position and blocking actor
    Check for:
        - Stuck positions (sweep distance < epsilon)
        - Ceiling bonks (height < 200cm)
        - Floor holes (no ground hit within 10cm below path)
```

This is the most comprehensive programmatic test -- it simulates an actual player walking through the entire building.

---

## 4. Industry Approaches to Proc-Gen Validation

### 4.1 Spelunky's Guaranteed Path

Spelunky uses a 4x4 grid of rooms. The algorithm:
1. Start at a random column in the top row
2. Random walk left/right/down, never up
3. When reaching the bottom row, place the exit
4. This path is the "solution path" -- rooms along it are guaranteed traversable
5. Room templates are selected based on which edges need openings (type 0-3)
6. Non-path rooms get random templates but always have at least the required connections

**Key insight:** Connectivity is guaranteed by construction, not by post-generation validation. The generation algorithm structurally cannot produce an uncompletable level.

**Applicability to our system:** Our spatial registry already defines room adjacency. If we generate rooms from a connectivity graph (which `create_building` does), connectivity is inherent. The risk is that *geometry* doesn't match the *graph* -- booleans fail, doors are too narrow, etc. That's what programmatic validation catches.

### 4.2 Wave Function Collapse Validation

WFC uses constraint propagation:
- Each cell has a set of possible tiles
- Placing a tile eliminates incompatible neighbors
- If any cell reaches 0 possibilities: **contradiction** -- backtrack

**Validation is built into generation:**
- Constraint propagation ensures local consistency (adjacent tiles are compatible)
- Backtracking handles contradictions
- Complete tilesets prevent contradictions entirely (no backtracking needed)

**Applicability:** WFC's constraint system maps well to room/door placement rules. If we used WFC for building layout, many validation concerns would be moot. However, our current approach is more free-form (BSP subdivision + connectivity graph), so we need post-generation validation.

### 4.3 Houdini's Procedural Building QA

Houdini studios (Embark for THE FINALS, etc.) use:
- **Color-coded geometry visualization** for debugging randomization
- **SOP-level validation** at each pipeline stage (50% fewer geometry inconsistencies reported)
- **Shape grammar rules** encoded in JSON that constrain generation
- **Automated attribute checks** at each subdivision step

**Key insight:** Validate at each pipeline stage, not just at the end. If the floor plan is wrong, don't bother checking window booleans.

### 4.4 UE5 PCG Framework Validation

Epic's PCG framework (production-ready in 5.7) uses:
- **Difference node** for exclusion zones (removes overlapping points)
- **Density Filter** for post-placement culling
- **Bounds checking** to prevent placement outside valid regions
- **Collision checks** built into spawn logic

No formal validation pipeline, but the node graph structure encourages incremental validation at each stage.

### 4.5 Bob Nystrom's "Rooms and Mazes" (Widely Adopted)

Algorithm:
1. Place non-overlapping rooms on odd-boundary grid
2. Fill remaining space with maze (randomized flood-fill)
3. Identify "connectors" (solid tiles between two regions)
4. Build spanning tree through connectors (guarantees all regions connected)
5. Optionally open extra connectors for loops
6. Remove dead-end maze corridors

**Guarantee:** Spanning tree ensures every room is reachable. Dead-end removal preserves connectivity because only leaf corridors are pruned.

### 4.6 modl.ai Automated Game Testing

Commercial platform for automated game testing:
- **Exploratory bots** sweep content 20x faster than humans
- **Overnight testing cycles:** upload build, bot tests 60+ levels, morning review
- **Custom event tracking** for procedural content validation
- **No SDK required** -- uses screen-level interaction

**Applicability:** Too heavyweight for our use case (we validate individual buildings, not full game sessions), but the overnight batch concept maps well to Tier 3 vision validation.

### 4.7 Academic Research (2024-2025)

**"Artificial Players in the Design Process"** (CHI Play) -- AI agents that simulate player behavior to validate level designs. Used pathfinding + behavioral models to identify unreachable areas and difficulty spikes.

**"Automated Evaluation of PCG with Deep RL Agents"** (IEEE Trans. Games, 2025) -- DRL agents trained to play generated levels. If agent can't complete the level, it's flagged. Expensive to train but generalizes across content variations.

**"Improving Conditional Level Generation using Automated Validation"** (Avalon, 2024) -- Bot playtesting extracts difficulty statistics offline. Invalid levels (can't solve in N moves) fail validation. No auto-repair; invalid outputs are simply rejected.

---

## 5. Viewport Capture for Automated Review

### 5.1 UE5 SceneCaptureComponent2D

**How it works:** Spawns an invisible camera that renders to a `UTextureRenderTarget2D`. We already use this for `capture_floor_plan`.

**Relevant capture modes for validation:**

| Mode | What it captures | Validation use |
|------|-----------------|----------------|
| Lit (default) | Full rendered scene | Visual review, proportions |
| Unlit | Base color only | Detect missing materials |
| Wireframe | Edge structure | Boolean artifacts, topology |
| SceneDepth | Depth buffer | Geometry gaps, z-fighting |
| WorldNormal | Surface normals | Inverted faces, normal artifacts |
| Custom Stencil | Object IDs | Isolate specific elements |

**Implementation:** Set `SceneCaptureComponent2D->CaptureSource` to appropriate `ESceneCaptureSource` enum.

### 5.2 Multi-View Capture Strategy

For each generated building, capture:

1. **Floor plan (per floor):** Top-down orthographic. Already implemented as `capture_floor_plan`. Shows room layout, wall completeness, door positions.

2. **4 exterior elevations:** Position camera at each cardinal direction, looking at the building center. Orthographic projection. Shows facade completeness, window/door placement, roof line.

3. **Per-door through-views:** Position camera at door opening, looking through. Perspective projection. Verifies door is fully cut through and passable.

4. **Per-room interior views:** Position camera at room center, wide FOV. Shows floor, ceiling, walls, openings.

5. **Depth buffer captures:** Same viewpoints but capture SceneDepth. Depth discontinuities at wall surfaces indicate holes or missing geometry.

### 5.3 Depth Buffer Analysis

Capture SceneDepth to render target, read back pixels, analyze:
- **Sudden depth jumps** at wall boundaries indicate holes
- **Depth values at window/door locations** should show through to next room/exterior
- **Consistent depth across floor surfaces** (no holes)
- **Ceiling depth should be consistent** (no gaps)

**Async readback approach (from nicholas477):**
```cpp
// Create staging texture with CPUReadback flag
FTexture2DRHIRef AsyncReadTexture = RHICreateTexture2D(
    Width, Height, PF_FloatRGBA, 1, 1,
    TexCreate_CPUReadback, ERHIAccess::CopyDest, CreateInfo);

// Copy RT to staging texture
RHICmdList.CopyTexture(RTTexture, AsyncReadTexture, CopyInfo);

// Write fence (non-blocking)
FGPUFenceRHIRef Fence = RHICreateGPUFence(TEXT("DepthReadback"));
RHICmdList.WriteGPUFence(Fence);

// Poll fence on game thread (typically ready in 2-3 frames)
if (Fence->Poll()) {
    void* Buffer = nullptr;
    int32 W, H;
    GDynamicRHI->RHIMapStagingSurface(AsyncReadTexture, Fence, Buffer, W, H);
    // Analyze depth values
    RHICmdList.UnmapStagingSurface(AsyncReadTexture);
}
```

**Performance:** ~40us CPU vs ~4ms for synchronous ReadPixels. Essential for batch validation.

### 5.4 Wireframe + Solid Comparison

Capture both wireframe and solid views of the same viewpoint. Overlay comparison reveals:
- **Boolean artifacts:** Extra edges where the subtract didn't clean up
- **Missing faces:** Wireframe shows edges but solid shows holes
- **Degenerate triangles:** Very thin wireframe triangles

This is more useful for debugging than automated validation, but could be included in validation reports.

### 5.5 Automated Test Harness Architecture

```
validate_building(building_id, block_id, options):
    // Phase 1: Programmatic (always)
    results = []
    results += check_door_passability(building_id)      // capsule sweep
    results += check_room_connectivity(building_id)     // flood fill
    results += check_window_sightlines(building_id)     // ray trace
    results += check_mesh_topology(building_id)         // IsClosed, genus
    results += check_collision_coverage(building_id)    // collision exists

    // Phase 2: NavMesh (always if navmesh exists)
    if navmesh_built:
        results += check_navmesh_connectivity(building_id)
        results += check_path_widths(building_id)
        results += check_stair_angles(building_id)

    // Phase 3: Vision (optional)
    if options.vision_check:
        captures = capture_building_views(building_id)  // 6-12 PNGs
        vision_report = send_to_vision_model(captures, building_spec)
        results += parse_vision_report(vision_report)

    // Compile report
    report = compile_validation_report(results)
    save_report(building_id, report)  // JSON + optional HTML

    return report
```

### 5.6 UE5 Screenshot Comparison Tool

UE5 has a built-in screenshot comparison system:
- `FScreenshotRequest` for capturing
- `FFunctionalTestBase` for placing test actors in levels
- Gauntlet framework for running tests headlessly

**Not directly useful** for procedural content (no "golden reference" to compare against), but the infrastructure (capture, save, compare) can be repurposed.

---

## 6. Self-Correcting Generation

### 6.1 Generate-Validate-Fix Loop

```
max_attempts = 3
for attempt in range(max_attempts):
    building = generate_building(spec)
    report = validate_building(building)

    if report.all_passed:
        return building

    if attempt < max_attempts - 1:
        spec = apply_fixes(spec, report)
        destroy_building(building)
```

### 6.2 Auto-Fixable Issues

| Issue | Auto-Fix Strategy | Confidence |
|-------|------------------|------------|
| Door too narrow | Widen door opening by 20cm, re-boolean | High |
| Boolean didn't punch through | Increase cutter depth by wall_thickness, retry | High |
| Window blocked | Same as door -- deepen cutter | High |
| Room unreachable | Add door to nearest connected room | Medium |
| Missing floor section | Re-generate floor slab for that room | Medium |
| Inverted normals | Flip affected triangles | High |
| Degenerate triangles | Remove and re-triangulate affected region | Medium |
| NavMesh gap at door | Spawn NavLink at door location | High |
| Staircase too steep | Reduce rise, extend run, regenerate | Medium |
| Floating geometry | Delete disconnected mesh components | Medium |
| Non-manifold edges | Run MeshRepair weld + fill holes | Medium |

### 6.3 Issues Requiring Regeneration

- Room layout fundamentally wrong (overlapping rooms)
- Structural wall missing entirely
- Floor plan doesn't match spec (wrong room count)
- Building footprint exceeds lot bounds

These require discarding and regenerating from the layout phase.

### 6.4 Constraint Propagation (Prevention > Fix)

Better to prevent issues than fix them:

| Constraint | When Applied | What It Prevents |
|-----------|-------------|-----------------|
| Min door width = 90cm | Door placement | Too-narrow doors |
| Boolean cutter depth = wall_thickness * 1.5 | Boolean operation | Incomplete cuts |
| Room min dimension = 200cm | Room layout | Impossibly small rooms |
| Max stair angle = 40 degrees | Stair generation | Unclimbable stairs |
| Door must be on shared wall | Door placement | Doors to nowhere |
| Window min 30cm from corner | Window placement | Corner boolean artifacts |
| Floor-to-floor height = 300cm | Multi-story stacking | Inconsistent floors |

### 6.5 ML-Based Parameter Tuning

Long-term: collect validation results across hundreds of generated buildings. Train a simple model to predict which parameter combinations produce valid output. Use as a prior to bias generation toward valid configurations.

This is the Avalon approach -- condition the generator on statistics extracted from validated output.

---

## 7. Existing Monolith Tools

### 7.1 Directly Applicable

| Action | Module | Validation Use |
|--------|--------|---------------|
| `query_raycast` | mesh/spatial | Shoot rays through openings to verify they're clear |
| `query_multi_raycast` | mesh/spatial | Multi-hit through walls to detect geometry layers |
| `query_line_of_sight` | mesh/spatial | Binary visibility check between two points |
| `query_radial_sweep` | mesh/spatial | Fan of rays to check openness around a point |
| `query_overlap` | mesh/spatial | Capsule/box overlap test at door openings |
| `query_navmesh` | mesh/spatial | Pathfinding between rooms to verify connectivity |
| `validate_path_width` | mesh/accessibility | Check passage widths (designed for wheelchair, perfect for doors) |
| `validate_navigation_complexity` | mesh/accessibility | Score cognitive difficulty of routes |
| `analyze_sightlines` | mesh/horror | Fan-of-rays with claustrophobia scoring -- useful for room quality |
| `analyze_choke_points` | mesh/horror | Find narrow passages -- useful for detecting tight spots |
| `capture_floor_plan` | mesh/debug | Orthographic top-down view of building floor |
| `highlight_room` | mesh/debug | Visual debugging overlay for rooms |
| `capture_scene_preview` | editor | Arbitrary scene screenshots (asset previews, but could be extended) |
| `get_scene_statistics` | mesh/spatial | Actor counts, triangle counts, navmesh status |
| `get_spatial_relationships` | mesh/spatial | Check if actors are inside/on-top-of/adjacent |
| `generate_accessibility_report` | mesh/accessibility | Combined validation report |

### 7.2 Could Be Extended

| Action | Extension Needed |
|--------|-----------------|
| `capture_floor_plan` | Add wireframe mode, depth mode, per-room isolation |
| `capture_scene_preview` | Extend to support arbitrary world positions (not just asset preview) |
| `query_overlap` | Add capsule sweep (moving overlap) not just static overlap |
| `generate_accessibility_report` | Add building-specific checks (door width, room reachability) |

### 7.3 Missing (Need to Build)

| Proposed Action | Purpose |
|----------------|---------|
| `validate_building` | Orchestrator: runs all Tier 1+2 checks on a building, returns structured report |
| `validate_door_passability` | Capsule sweep through every door in a building |
| `validate_room_connectivity` | Flood-fill from entrance, flag unreachable rooms |
| `validate_window_openings` | Ray grid through each window, flag blocked ones |
| `validate_mesh_topology` | IsClosed, genus check, degenerate triangle detection |
| `capture_building_views` | Multi-view capture (floor plan + elevations + through-door + interior) |
| `validate_building_vision` | Send captured views to vision model, parse report |
| `fix_building_issues` | Apply auto-fixes based on validation report |
| `sweep_capsule_path` | Sweep player capsule along a path, report blockages |

---

## 8. Proposed Validation Architecture

### 8.1 Three-Tier Pipeline

```
                    +-----------------+
                    | generate_building|
                    +--------+--------+
                             |
                    +--------v--------+
                    | TIER 1: GEOMETRY |  <-- Always runs, <100ms
                    | - door passability|
                    | - window sightlines|
                    | - mesh topology   |
                    | - collision check  |
                    +--------+--------+
                             |
                    +--------v--------+
                    | TIER 2: SPATIAL  |  <-- Always runs, 1-3s
                    | - navmesh build   |
                    | - room connectivity|
                    | - path widths     |
                    | - stair angles    |
                    | - accessibility   |
                    +--------+--------+
                             |
                   +---------+---------+
                   |                   |
              ALL PASS            ISSUES FOUND
                   |                   |
                   v                   v
              +----+----+     +--------+--------+
              |  DONE   |     | AUTO-FIX ATTEMPT |
              +---------+     +--------+--------+
                                       |
                              +--------v--------+
                              | RE-VALIDATE T1+T2|
                              +--------+--------+
                                       |
                              +--------v--------+
                              | TIER 3: VISION   |  <-- Optional, 5-30s
                              | - multi-view cap  |
                              | - VLM analysis    |
                              | - semantic review  |
                              +--------+--------+
                                       |
                              +--------v--------+
                              |  FINAL REPORT   |
                              +-----------------+
```

### 8.2 Validation Report Schema (JSON)

```json
{
    "building_id": "bld_001",
    "block_id": "default",
    "timestamp": "2026-03-28T14:30:00Z",
    "generation_params": { ... },
    "overall_status": "PASS" | "FAIL" | "WARN",
    "tier1_geometry": {
        "status": "PASS",
        "door_passability": {
            "total_doors": 8,
            "passable": 8,
            "blocked": 0,
            "details": []
        },
        "window_sightlines": {
            "total_windows": 12,
            "clear": 11,
            "blocked": 1,
            "details": [
                {
                    "window_id": "win_3",
                    "room_id": "room_2",
                    "blocked_percentage": 0.6,
                    "severity": "WARNING",
                    "blocking_actor": "Wall_37"
                }
            ]
        },
        "mesh_topology": {
            "is_closed": true,
            "degenerate_triangles": 0,
            "inverted_normals": 0,
            "genus": 0
        },
        "collision": {
            "all_rooms_have_collision": true,
            "missing_collision": []
        }
    },
    "tier2_spatial": {
        "status": "PASS",
        "navmesh_connectivity": {
            "all_rooms_reachable": true,
            "unreachable_rooms": []
        },
        "path_widths": {
            "min_width_cm": 95,
            "narrow_points": []
        },
        "stair_validation": {
            "all_stairs_climbable": true,
            "max_angle_degrees": 38
        }
    },
    "tier3_vision": null,
    "auto_fixes_applied": [],
    "execution_time_ms": 1250
}
```

### 8.3 MCP Action Design

**Primary orchestrator action:**
```
mesh_query("validate_building", {
    "building_id": "bld_001",
    "block_id": "default",
    "tiers": [1, 2],              // which tiers to run
    "auto_fix": true,             // attempt fixes for failed checks
    "max_fix_attempts": 2,        // retry limit
    "vision_model": "claude",     // for tier 3: "claude", "gpt4", "local"
    "capture_views": true,        // save validation PNGs regardless of tier 3
    "output_report": true         // save JSON report to Saved/Monolith/Validation/
})
```

**Individual validation actions (composable):**
```
mesh_query("validate_door_passability", {
    "building_id": "bld_001",
    "capsule_radius": 42,        // player radius
    "capsule_half_height": 96    // player half-height
})

mesh_query("validate_room_connectivity", {
    "building_id": "bld_001",
    "entrance_room_id": "room_0" // or auto-detect
})

mesh_query("validate_window_openings", {
    "building_id": "bld_001",
    "ray_grid_size": 3           // 3x3 grid per window
})

mesh_query("validate_mesh_topology", {
    "building_id": "bld_001"
})

mesh_query("capture_building_views", {
    "building_id": "bld_001",
    "views": ["floor_plan", "elevations", "doors", "interiors"],
    "resolution": 1024
})

mesh_query("sweep_capsule_path", {
    "path_points": [[x,y,z], ...],
    "capsule_radius": 42,
    "capsule_half_height": 96
})
```

### 8.4 Phase Rollout

**Phase 1 -- Core Programmatic Validation (~24-32h)**
- `validate_building` orchestrator
- `validate_door_passability` (capsule sweep)
- `validate_room_connectivity` (flood fill + navmesh)
- `validate_window_openings` (ray grid)
- `sweep_capsule_path` (full building walkthrough)
- JSON report output

**Phase 2 -- Topology + Auto-Fix (~16-24h)**
- `validate_mesh_topology` (IsClosed, genus, degenerates)
- `fix_building_issues` (auto-widen doors, deepen booleans, flip normals)
- Re-validation loop
- Integration with spatial registry

**Phase 3 -- Vision Model Integration (~20-28h)**
- `capture_building_views` (multi-view capture system)
- `validate_building_vision` (VLM integration)
- Structured prompt system
- Report parsing and integration with JSON report
- Support for Claude, GPT-4V, and local LLaVA models

**Phase 4 -- Batch + CI (~8-12h)**
- Batch validation across entire city blocks
- Overnight validation harness
- HTML report generation with embedded screenshots
- Statistics dashboard (pass rate, common issues, trends)

---

## 9. Effort Estimates

| Component | Hours | Dependencies |
|-----------|-------|-------------|
| Phase 1: Core Programmatic | 24-32h | Spatial registry |
| Phase 2: Topology + Auto-Fix | 16-24h | Phase 1, GeometryScript |
| Phase 3: Vision Model | 20-28h | Phase 1, capture system |
| Phase 4: Batch + CI | 8-12h | Phase 1-2 |
| **Total** | **68-96h** | |

**Recommended MVP:** Phase 1 only (24-32h). Catches 80%+ of issues programmatically. Add Phase 2 once boolean generation is more mature. Phase 3 is a nice-to-have for complex buildings.

---

## 10. Sources

### Academic / Research
- [VideoGameQA-Bench: Evaluating VLMs for Game QA](https://arxiv.org/abs/2505.15952) -- NeurIPS 2025, Sony Interactive Entertainment
- [Improving Conditional Level Generation using Automated Validation](https://arxiv.org/html/2409.06349v2) -- Avalon, Match-3 validation
- [Artificial Players in the Design Process](https://dl.acm.org/doi/10.1145/3410404.3414249) -- CHI Play, automated level testing
- [A Procedural Method for Automatic Generation of Spelunky Levels](https://link.springer.com/chapter/10.1007/978-3-319-16549-3_25) -- Guaranteed path algorithms
- [Procedural Content Generation Survey with LLM Integration](https://arxiv.org/html/2410.15644v1)
- [Human-AI Collaborative Game Testing with VLMs](https://arxiv.org/html/2501.11782v1)
- [Algorithm 964: Efficient Genus Computation](https://torroja.dmt.upm.es/pubs/2016/Lozano-duran_Borrell_TOMS_2016.pdf) -- Topology validation
- [Interactive and Robust Mesh Booleans](https://dl.acm.org/doi/10.1145/3550454.3555460) -- Boolean validation challenges
- [Manifold Library](https://github.com/elalish/manifold/wiki/Manifold-Library) -- Manifold mesh operations

### Industry / Tools
- [modl.ai: Testing Content-Heavy Games](https://modl.ai/testing-content-heavy-games-with-ai-bots) -- Commercial automated game testing
- [modl.ai: AI Bots for QA Testing](https://modl.ai/ai-bots-help-game-developers-escape-the-infinity-loop-of-qa-testing/)
- [Rooms and Mazes: Bob Nystrom](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/) -- Dungeon generation with guaranteed connectivity
- [Spelunky Generator Explained](https://gameasart.com/blog/2016/03/11/spelunkys-procedural-level-generation-explained/)
- [WFC Explained (BorisTheBrave)](https://www.boristhebrave.com/2020/04/13/wave-function-collapse-explained/)
- [THE FINALS: Procedural Buildings with Houdini](https://80.lv/articles/how-embark-studios-built-procedural-environments-for-the-finals-using-houdini)
- [Machine Learning for Visual Validation in Game Dev](https://8thlight.com/insights/machine-learning-visual-validation-game-devops)

### Unreal Engine
- [UE5 Automation Test Framework](https://dev.epicgames.com/documentation/en-us/unreal-engine/automation-test-framework-in-unreal-engine)
- [UE5 Screenshot Comparison Tool](https://dev.epicgames.com/documentation/en-us/unreal-engine/screenshot-comparison-tool-in-unreal-engine)
- [USceneCaptureComponent2D](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponent2D)
- [GeometryScript Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-reference-in-unreal-engine)
- [Capsule Trace By Channel](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Collision/CapsuleTraceByChannel)
- [PCG Framework Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [Async RT Readback with GPU Fences](https://nicholas477.github.io/blog/2023/reading-rt/)

### Vision Models
- [Claude Vision API](https://platform.claude.com/docs/en/build-with-claude/vision)
- [Ollama Vision Models](https://ollama.com/blog/vision-models)
- [LLaVA on Ollama](https://ollama.com/library/llava)
- [VLM Test Automation (Unmesh Gundecha)](https://unmesh.dev/post/vision_ai/)
- [Claude AI for QA Automation](https://www.secondtalent.com/resources/claude-ai-for-test-case-generation-and-qa-automation/)
