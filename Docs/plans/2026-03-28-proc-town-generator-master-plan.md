# Procedural Town Generator — Master Implementation Plan

> **For agentic workers:** This is a MASTER PLAN that defines 9 sub-projects. Each sub-project has its own detailed implementation plan (linked below). Execute sub-projects in dependency order. Multiple sub-projects can run in parallel where dependencies allow.

**Goal:** Build a procedural town generator that creates enterable, playable city blocks from a single MCP call — buildings with connected rooms, roofs, facades, furniture, lighting, horror dressing, navmesh, and volumes, all adaptive to terrain.

**Architecture:** 11 sub-projects (split from original 9 after review) composing into a pipeline. Each sub-project produces working, testable actions usable standalone. The critical interface is the **Building Descriptor** — a JSON contract that SP1 outputs and SP2-SP9 consume. All building specs are generated server-side (not sent over MCP wire).

**Tech Stack:** UE 5.7, GeometryScript (`GeometryScriptingCore`), C++ (MonolithMesh module), JSON for configuration/presets/registry. All actions via Monolith MCP.

---

## Sub-Project Dependency Graph

```
                    ┌─────────────┐
                    │ SP1: Grid   │ ← Foundation. Defines Building Descriptor contract.
                    │   Building  │
                    └──────┬──────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                 ▼
   ┌──────────┐     ┌──────────┐      ┌──────────┐
   │ SP2:     │     │ SP3:     │      │ SP6:     │
   │ Floor    │     │ Facades  │      │ Spatial  │
   │ Plans    │     │ & Windows│      │ Registry │
   └────┬─────┘     └──────────┘      └────┬─────┘
        │                                  │
   ┌────┼──────────────────┐               │
   ▼    ▼                  ▼               ▼
┌────┐┌────┐         ┌──────────┐    ┌──────────┐
│SP4 ││SP10│         │ SP5:     │    │ SP7:     │
│Roof││Furn│         │ Block    │    │ Auto-    │
│    ││ish │         │ Layout   │    │ Volumes  │
└────┘└────┘         └──────────┘    └────┬─────┘
                          │               │
                     ┌────┼────┐          │
                     ▼         ▼          ▼
               ┌──────────┐┌──────────┐┌──────────┐
               │ SP8a:    ││ SP8b:    ││ SP9:     │
               │ Terrain  ││ Arch.    ││ Daredevil│
               │ Found.   ││ Features ││ View     │
               └──────────┘└──────────┘└──────────┘
```

**Parallel execution groups:**
- **Phase 1:** SP1 only (foundation — defines Building Descriptor contract)
- **Phase 2 (after SP1):** SP2, SP3, SP6 in parallel
- **Phase 3 (after Phase 2):** SP4, SP5, SP7, SP10 in parallel (SP5 uses flat roofs if SP4 not done)
- **Phase 4 (after Phase 3):** SP8a, SP8b, SP9 in parallel

---

## Building Descriptor Contract (Critical Interface)

**SP1's `create_building_from_grid` returns this JSON.** All downstream SPs consume it.

```json
{
  "building_id": "clinic_01",
  "asset_path": "/Game/CityBlock/Mesh/SM_Clinic",
  "actors": [
    {"label": "Clinic_01", "actor_name": "StaticMeshActor_52", "folder": "CityBlock/Buildings/Clinic"}
  ],
  "footprint_polygon": [[-350, -300], [350, -300], [350, 300], [-350, 300]],
  "world_origin": [-2000, 0, 0],
  "floors": [
    {
      "floor_index": 0,
      "z_offset": 0,
      "height": 270,
      "rooms": [
        {
          "room_id": "waiting_room",
          "room_type": "lobby",
          "grid_cells": [[0,0],[1,0],[2,0],[0,1],[1,1],[2,1]],
          "world_bounds": {"min": [-2350, -300, 0], "max": [-2050, 0, 270]},
          "local_bounds": {"min": [-350, -300, 0], "max": [-50, 0, 270]}
        }
      ],
      "doors": [
        {
          "door_id": "door_01",
          "connects": ["waiting_room", "exam_room"],
          "world_position": [-2050, -100, 0],
          "wall": "east",
          "width": 90,
          "height": 220,
          "traversable": true
        }
      ],
      "stairwells": [
        {
          "stairwell_id": "stair_01",
          "grid_cells": [[5,0],[5,1]],
          "connects_floors": [0, 1],
          "world_position": [-1700, -300, 0]
        }
      ]
    }
  ],
  "exterior_faces": [
    {
      "wall": "north",
      "floor_index": 0,
      "world_origin": [-2350, -300, 0],
      "normal": [0, -1, 0],
      "width": 700,
      "height": 270,
      "is_exterior": true
    }
  ],
  "tags_applied": ["BuildingCeiling", "BuildingFloor", "BuildingRoof"],
  "grid_cell_size": 50,
  "wall_thickness": {"exterior": 15, "interior": 10},
  "materials_assigned": {"0": "exterior_wall", "1": "interior_wall", "2": "floor", "3": "trim"}
}
```

**All SPs consume specific fields:**
- SP3 (Facades): `exterior_faces` — where to place windows/doors/trim
- SP4 (Roofs): `footprint_polygon` + `floors[-1].z_offset + height` — roof sits on top
- SP5 (Block): entire descriptor per building — orchestrates everything
- SP6 (Spatial Registry): `floors[].rooms[]`, `floors[].doors[]`, `floors[].stairwells[]`
- SP7 (Auto-Volumes): `floors[].rooms[].world_bounds` — volume dimensions
- SP10 (Furnishing): `floors[].rooms[].room_type` + `world_bounds` — what/where to furnish

---

## Sub-Project Summaries

### SP1: Grid-Based Building Construction (Foundation)
**Research:** `2026-03-28-connected-room-assembly-research.md`
**Est:** 28h | **Priority:** CRITICAL — everything depends on this
**New Actions:** `create_building_from_grid` (grid → geometry + Building Descriptor), `create_grid_from_rooms` (room list → grid)
**New Files:**
- Create: `Public/MonolithMeshBuildingTypes.h` — FBuildingGrid, FRoomDef, FDoorDef, FStairwellDef, FBuildingDescriptor structs (**PUBLIC** — shared across all SPs)
- Create: `Private/MonolithMeshBuildingActions.h/.cpp` — grid processing, wall segment generation, door placement, stairwell handling

**What it does:** Takes a 2D grid of room IDs + door edge positions → generates geometry AND a **Building Descriptor JSON** (see contract above). The descriptor is the interface for all downstream SPs.

Geometry features:
- Walls only at room ID boundaries (no shared wall duplication)
- Interior walls (10cm) vs exterior walls (15cm) auto-detected from grid edges
- Doors aligned by construction (edge-level, not room-level)
- Floor slabs per room, ceiling slabs per room (optional)
- **Stairwell cells** suppress floor/ceiling slab generation; stairwell enclosure walls from grid edges; actual stair mesh via existing `create_parametric_mesh(type: "stairs")` with auto-calculated step count from floor height
- Trim frames around all openings
- UV box projection, auto-collision
- **Actor tags:** `BuildingCeiling` on all ceiling actors, `BuildingFloor` on floor actors, `BuildingRoof` on roof actors — enables SP9 daredevil view
- Proper outliner folder organization under `/CityBlock/Buildings/{BuildingName}/`
- **Materials:** accepts optional `materials` map (slot → asset path). Defaults: slot 0 = exterior wall, slot 1 = interior wall, slot 2 = floor/ceiling, slot 3 = trim

**Default grid cell size: 50cm.** Configurable via `cell_size` parameter.

**Test 1:** Create a 3-room grid (kitchen, living room, hallway) with 2 doors. Verify: no duplicate walls, doors connect rooms, collision works, player can walk between rooms.
**Test 2:** Create a 2-story building with stairwell. Verify: stairs connect floor 0 to floor 1, stairwell cutout in ceiling slab, player can walk up.

---

### SP2: Automatic Floor Plan Generation
**Research:** `2026-03-28-proc-building-algorithms-research.md`
**Depends on:** SP1
**Est:** 28h | **Priority:** HIGH
**New Actions:** `generate_floor_plan`, `list_building_archetypes`, `get_building_archetype`
**New Files:**
- Create: `MonolithMeshFloorPlanGenerator.h/.cpp` — squarified treemap, BSP fallback, room connectivity
- Create: `Saved/Monolith/BuildingArchetypes/` — JSON archetype definitions

**What it does:** Given a building archetype (residential_house, clinic, police_station, apartment, mansion) and footprint dimensions → generates a grid + room list + door positions that feeds directly into SP1's `create_building`.

**Algorithm:** Graph-based (topology first → treemap layout → corridor insertion → door placement):
1. Load archetype JSON (room types, sizes, adjacency requirements)
2. Squarified treemap packs rooms into the footprint
3. **Corridor insertion phase** — reserve hallway cells along major room boundaries (split rooms slightly to create circulation space). Rooms that don't share a direct adjacency requirement connect via hallway. This prevents the "walk through kitchen to reach bedroom" problem.
4. Convert packed rectangles to grid cells
5. Place doors at room boundaries per adjacency graph
6. Output: grid, rooms, doors → ready for `create_building_from_grid`

**Hospice mode:** `hospice_mode: true` flag enforces minimum door width 100cm, minimum corridor width 180cm (wheelchair turning), no level changes within a floor, and adds a rest alcove per 4 rooms.

**Test:** Generate a "residential_house" floor plan at 800x600. Verify: kitchen connects to dining, bathroom off hallway (not through bedroom), bedrooms upstairs, ALL rooms reachable via hallway or direct adjacency. No rooms only accessible by walking through another room.

---

### SP3: Facade & Window Generation
**Research:** `2026-03-28-facade-window-research.md`
**Depends on:** SP1 (needs exterior walls to decorate)
**Est:** 45h | **Priority:** HIGH
**New Actions:** `generate_facade`, `list_facade_styles`, `apply_horror_damage`
**New Files:**
- Create: `MonolithMeshFacadeActions.h/.cpp` — window placement algo, door geometry, trim profiles, horror damage
- Create: `Saved/Monolith/FacadeStyles/` — JSON style presets (Victorian, Colonial, Brutalist, Abandoned)

**What it does:** Given a building's exterior walls → generates windows, doors, trim, cornices, storefronts, and optionally applies horror damage (boarded windows, broken glass, rust stains).

**Core algorithm:** `ComputeWindowPositions(WallWidth, WindowWidth, Margin, Spacing)` → evenly distributed window positions per floor. CGA-style vertical split: base (ground floor, taller) → shaft (upper floors) → cap (cornice).

**Test:** Generate a 2-story facade with 3 windows per floor. Verify: windows evenly spaced, trim surrounds each window, cornice at top, ground floor has different treatment.

---

### SP4: Roof Generation
**Research:** `2026-03-28-roof-generation-research.md`
**Depends on:** SP1 (needs building footprint for roof shape), SP2 (archetype determines roof type)
**Est:** 30h | **Priority:** MEDIUM
**New Actions:** `generate_roof`
**New Files:**
- Create: `MonolithMeshRoofActions.h/.cpp` — gable, hip, flat/parapet, shed, gambrel generation

**What it does:** Given a building footprint polygon and roof type → generates roof geometry with overhangs, proper MaterialID for roof surface vs walls.

**Primary approach:** `AppendSimpleSweptPolygon` with triangular cross-section for gable roofs (same pattern as pipe_network but with wedge profile). Hip roofs via 4 `AppendTriangulatedPolygon3D` faces.

**Test:** Generate gable roof on a 600x400 building. Verify: ridge runs along long axis, 30cm overhang on all sides, separate MaterialID for roof surface.

---

### SP5: City Block Layout
**Research:** `2026-03-28-city-block-layout-research.md`
**Depends on:** SP1 (building geometry), SP2 (floor plans). SP3 (facades) and SP4 (roofs) are optional — uses flat roofs and plain walls if not available.
**Est:** 55h | **Priority:** HIGH
**New Actions:** `create_city_block` (orchestrator), `create_lot_layout` (just subdivision, returns lot positions), `create_street`, `place_street_furniture`
**New Files:**
- Create: `MonolithMeshCityBlockActions.h/.cpp` — lot subdivision, footprint generation, street geometry, orchestration
- Create: `Saved/Monolith/BlockPresets/` — JSON block configuration presets

**What it does:** The top-level orchestrator, but also exposes each step as standalone actions. `create_city_block` is a convenience wrapper — agents can also call individual steps for finer control.

**MCP call is lightweight** — building specs generated server-side:
```json
{"buildings": 4, "genre": "horror", "seed": 42, "block_size": [6000, 4000], "decay": 0.6}
```
Individual building specs never sent over MCP wire.

**Pipeline (generates one building at a time to avoid context pressure):**
1. `create_lot_layout` — subdivide block into lots (OBB recursive), return lot positions
2. For each lot: generate footprint shape (L, rect, T per weighted random)
3. For each building: `generate_floor_plan` (SP2) → `create_building_from_grid` (SP1)
4. For each building: `generate_facade` (SP3, if available) — skip if SP3 not done
5. For each building: `generate_roof` (SP4, if available) — flat roof fallback if SP4 not done
6. `create_street` + `place_street_furniture` — sidewalks, curbs, lamps, hydrants
7. Apply horror decay per `decay` parameter
8. Register everything in spatial registry (SP6)
9. Auto-volumes + navmesh build (SP7, if available)

**Graceful degradation:** Works without SP3 (plain walls), SP4 (flat roofs), SP7 (no volumes), SP10 (empty rooms). Each adds quality when available.

**Test:** Generate a 4-building block with seed=42. Verify: buildings don't overlap, streets exist, each building is unique and enterable. Test with SP4 unavailable — should produce flat-roof buildings.

---

### SP6: Spatial Registry
**Research:** `2026-03-28-spatial-registry-research.md`
**Depends on:** SP1 (registers rooms created by `create_building`)
**Est:** 36h | **Priority:** HIGH
**New Actions:** `register_building`, `register_room`, `register_street_furniture`, `query_room_at`, `query_adjacent_rooms`, `query_rooms_by_filter`, `query_building_exits`, `path_between_rooms`, `save_block_descriptor`, `load_block_descriptor`
**New Files:**
- Create: `MonolithMeshSpatialRegistry.h/.cpp` — hierarchical JSON descriptor, adjacency graph, queries
- Create: `Saved/Monolith/SpatialRegistry/` — persisted block descriptors

**What it does:** Maintains a queryable database of what's where. Block → Buildings → Floors → Rooms → Openings. Room adjacency graph with BFS pathfinding. Feeds into horror analysis, AI director, encounter design.

**Test:** Register a 5-room building. Query "what room is at position X?" and "what's adjacent to the kitchen?". Verify correct results.

---

### SP7: Auto-Volume Generation
**Research:** `2026-03-28-auto-volumes-research.md`
**Depends on:** SP6 (needs spatial registry to know room positions/sizes)
**Est:** 36h | **Priority:** MEDIUM
**New Actions:** `auto_volumes_for_building`, `auto_volumes_for_block`, `spawn_nav_link`
**Modify:** `spawn_volume` — add `nav_mesh_bounds` type

**What it does:** Given a building from the spatial registry → auto-spawns:
- NavMeshBoundsVolume covering the block
- BlockingVolume per room (enables scatter_props, acoustics, horror analysis)
- AudioVolume per room with reverb based on room size
- TriggerVolume at building entrances and stairwells
- Builds navmesh after all geometry is placed

**Pipeline order:** geometry → collision → volumes → navmesh build → audio analysis

**Test:** Auto-volumes on a 3-room building. Verify: navmesh path exists between rooms, audio volume has correct reverb, trigger volume at entrance.

---

### SP8a: Terrain + Foundations
**Research:** `2026-03-28-terrain-adaptive-buildings-research.md`
**Depends on:** SP5 (needs city block to adapt)
**Est:** 45h | **Priority:** LOW (works on flat terrain without this)
**New Actions:** `sample_terrain_grid`, `analyze_building_site`, `create_foundation`, `create_retaining_wall`, `place_building_on_terrain`

**What it does:** Adapts buildings to uneven terrain: samples NxM height grid via downward traces, auto-selects foundation strategy (Flat <30cm diff, CutAndFill <10°, Stepped 10-25°, Piers >25°, WalkoutBasement >70% floor height), generates retaining walls and pier supports. ADA-compliant ramp generation for hospice mode (1:12 slope, 76cm max rise, 150cm landings).

**Test:** Place a building on a 15-degree slope. Verify: foundation adapts, front steps or ramp generated, no floating geometry.

---

### SP8b: Architectural Features
**Research:** `2026-03-28-terrain-adaptive-buildings-research.md` (sections 14-17)
**Depends on:** SP1 (needs building exterior faces). Independent of terrain — works on flat ground.
**Est:** 35h | **Priority:** LOW
**New Actions:** `create_balcony`, `create_porch`, `create_fire_escape`, `create_ramp_connector`, `create_railing`

**What it does:** Standalone architectural features that attach to building exteriors. Balconies (floor slab + railing extending from upper floors), porches (ground-level covered entry with columns), fire escapes (zigzag exterior stairs between floor landings), railings (swept profile along edge paths). All usable independently of terrain adaptation.

**Test:** Add a balcony to a 2-story building's second floor. Verify: extends outward, has railing, doesn't intersect building walls.

---

### SP9: Daredevil Debug View
**Research:** `2026-03-28-daredevil-debug-view-research.md`
**Depends on:** SP1 (needs ceiling tags), SP6 (needs spatial registry for room queries)
**Est:** 45h | **Priority:** LOW (quality-of-life, not functional)
**New Actions:** `toggle_section_view`, `toggle_ceiling_visibility`, `capture_floor_plan`, `highlight_room`, `save_camera_bookmark`, `load_camera_bookmark`

**What it does:** Debug visualization for inspecting generated buildings:
- MPC-based section clip (hide everything above a Z height)
- Toggle ceiling/roof visibility via actor tags
- Orthographic top-down floor plan capture to PNG
- Room highlighting with overlay materials
- Camera bookmarks for saved viewpoints

**Test:** Generate a building, toggle section view at floor 1 height. Verify: roof/ceiling hidden, all rooms visible from above.

---

### SP10: Room Furnishing Pipeline
**Research:** `2026-03-28-proc-building-algorithms-research.md` (Section 11 — room type catalogs)
**Depends on:** SP1 (needs Building Descriptor with room types), SP6 (needs spatial registry for room queries)
**Est:** 25h | **Priority:** MEDIUM
**New Actions:** `furnish_room`, `furnish_building`, `list_furniture_presets`
**New Files:**
- Create: `MonolithMeshFurnishingActions.h/.cpp` — room-type-to-furniture mapping, placement rules
- Create: `Saved/Monolith/FurniturePresets/` — JSON furniture configs per room type

**What it does:** Given a room type and bounds from the Building Descriptor → places appropriate furniture using existing `create_parametric_mesh` and `scatter_on_surface`. Maps room types to furniture templates:
- Kitchen: counter along one wall, table center, cabinets on opposite wall
- Bedroom: bed against back wall, nightstand beside it, shelf/cabinet
- Bathroom: toilet, sink, bathtub/shower along walls
- Office: desk, chair, shelf, cabinet
- Lobby/Waiting: desk near entrance, chairs along walls
- Corridor: empty or occasional shelf/bench

Leverages proc mesh caching — identical furniture types reuse cached assets.

**Horror dressing:** After furnishing, optionally applies `set_room_disturbance` (ransacked/abandoned) and `place_storytelling_scene` per room based on building decay level.

**Test:** Furnish a "kitchen" room at 400x300. Verify: counter, table, and cabinets placed within room bounds, no furniture clipping walls (collision_mode: "reject").

---

## Execution Strategy

### Phase 1: Foundation (SP1 only) — ~28h
Build and test the grid-based building system + Building Descriptor contract. This unlocks everything.

### Phase 2: Content Generation (SP2 + SP3 + SP6 in parallel) — ~109h wall-clock ~45h
Three independent agent teams simultaneously:
- **Agent Team A:** Floor plan generator with corridors (SP2) — 36h
- **Agent Team B:** Facade/window system + horror damage (SP3) — 45h
- **Agent Team C:** Spatial registry + queries (SP6) — 36h

### Phase 3: Assembly (SP4 + SP5 + SP7 + SP10 in parallel) — ~146h wall-clock ~55h
Four independent agent teams:
- **Agent Team D:** Roofs (SP4) — 30h
- **Agent Team E:** City block orchestrator (SP5) — 55h (critical path)
- **Agent Team F:** Auto-volumes (SP7) — 36h
- **Agent Team G:** Room furnishing (SP10) — 25h

### Phase 4: Polish (SP8a + SP8b + SP9 in parallel) — ~110h wall-clock ~45h
Three independent agent teams:
- **Agent Team H:** Terrain + foundations (SP8a) — 45h
- **Agent Team I:** Architectural features (SP8b) — 35h
- **Agent Team J:** Daredevil view (SP9) — 30h

### Total: ~420h implementation, ~173h wall-clock with full parallelism
With 3-4 parallel agent teams per phase across 4 sequential phases.

### Performance Budget
| Metric | Target | Notes |
|--------|--------|-------|
| Buildings per block | 4-8 | Each is one merged mesh |
| Rooms per building | 5-30 | Depends on archetype |
| Total mesh actors per block | <50 | Buildings + roofs + furniture |
| Volumes per block | <100 | BlockingVolumes per building (not per room), AudioVolumes per building, triggers at doors |
| NavMesh build time | <10s | Single build after all geometry |
| Boolean ops per facade | <20 per wall face | Use merged cutter optimization |
| Total generation time | <30s | For a 4-building block |

---

## File Impact Summary

### New Files (9 sub-projects)
| File | Sub-Project | Responsibility |
|------|------------|----------------|
| `MonolithMeshBuildingActions.h/.cpp` | SP1 | Grid → geometry conversion |
| `MonolithMeshBuildingTypes.h` | SP1 | FBuildingGrid, FRoomDef, FDoorDef |
| `MonolithMeshFloorPlanGenerator.h/.cpp` | SP2 | Treemap layout, archetype loading |
| `MonolithMeshFacadeActions.h/.cpp` | SP3 | Windows, doors, trim, horror damage |
| `MonolithMeshRoofActions.h/.cpp` | SP4 | Gable, hip, flat, mansard generation |
| `MonolithMeshCityBlockActions.h/.cpp` | SP5 | Block orchestrator, lot subdivision |
| `MonolithMeshSpatialRegistry.h/.cpp` | SP6 | JSON descriptor, adjacency graph |
| `MonolithMeshAutoVolumeActions.h/.cpp` | SP7 | NavMesh, blocking, audio, trigger |
| `MonolithMeshTerrainActions.h/.cpp` | SP8a | Height sampling, foundations |
| `MonolithMeshArchFeatureActions.h/.cpp` | SP8b | Balconies, porches, fire escapes, railings |
| `MonolithMeshDebugViewActions.h/.cpp` | SP9 | Section clip, floor plan capture |
| `MonolithMeshFurnishingActions.h/.cpp` | SP10 | Room-type furniture mapping, placement |

### Modified Files
| File | Sub-Projects | Changes |
|------|-------------|---------|
| `MonolithMeshModule.cpp` | All | Register new action classes |
| `MonolithMeshProceduralActions.cpp` | SP1 | Reuse MakeWallProfile, CleanupMesh, UV projection |
| `MonolithMeshVolumeActions.cpp` | SP7 | Add nav_mesh_bounds to spawn_volume |
| `MonolithMesh.Build.cs` | SP9 | Add MaterialParameterCollection module dep if needed |

### Data Files
| Directory | Sub-Project | Contents |
|-----------|------------|----------|
| `Saved/Monolith/BuildingArchetypes/` | SP2 | JSON room catalogs per building type |
| `Saved/Monolith/FacadeStyles/` | SP3 | JSON facade presets (Victorian, etc.) |
| `Saved/Monolith/BlockPresets/` | SP5 | JSON block configuration presets |
| `Saved/Monolith/SpatialRegistry/` | SP6 | Persisted block descriptors |
| `Saved/Monolith/CameraBookmarks/` | SP9 | Saved camera positions |
| `Saved/Monolith/FurniturePresets/` | SP10 | Room-type furniture configs |

---

## New Action Count

| Sub-Project | New Actions | Running Total |
|-------------|------------|---------------|
| Current | 0 | 193 mesh (636 total) |
| SP1: Grid Building | 2 | 195 |
| SP2: Floor Plans | 3 | 198 |
| SP3: Facades | 3 | 201 |
| SP4: Roofs | 1 | 202 |
| SP5: Block Layout | 4 | 206 |
| SP6: Spatial Registry | 10 | 216 |
| SP7: Auto-Volumes | 3 | 219 |
| SP8a: Terrain | 5 | 224 |
| SP8b: Arch Features | 5 | 229 |
| SP9: Debug View | 6 | 235 |
| SP10: Furnishing | 3 | 238 |
| **Total New** | **45** | **238 mesh / 681 total** |

---

## Research Documents Index

Each sub-project plan MUST reference its research doc for API signatures, algorithms, and code sketches:

| Research Doc | Sub-Projects |
|-------------|-------------|
| `2026-03-28-connected-room-assembly-research.md` | SP1 |
| `2026-03-28-proc-building-algorithms-research.md` | SP2, SP5 |
| `2026-03-28-facade-window-research.md` | SP3 |
| `2026-03-28-roof-generation-research.md` | SP4 |
| `2026-03-28-city-block-layout-research.md` | SP5 |
| `2026-03-28-spatial-registry-research.md` | SP6 |
| `2026-03-28-auto-volumes-research.md` | SP7 |
| `2026-03-28-terrain-adaptive-buildings-research.md` | SP8 |
| `2026-03-28-daredevil-debug-view-research.md` | SP9 |

---

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|-----------|
| Grid building geometry quality | HIGH | Keep box-based fallback (`wall_mode: "box"`) alongside sweep walls |
| Treemap produces awkward room proportions | MEDIUM | Add aspect ratio constraints, BSP fallback |
| Facade boolean performance with many windows | LOW | Merged cutter optimization (one boolean per wall face) |
| 232 actions overwhelming for agents | MEDIUM | Group actions under clear namespaces, update skills per SP |
| Context window limits for large building specs | MEDIUM | Keep building JSON specs under 2KB, stream room generation |
| Terrain adaptation scope creep | HIGH | SP8 is optional — flat terrain works fine without it |
| Cross-SP integration bugs | MEDIUM | Each SP produces standalone testable actions first |
| Boolean subtract reliability on thin walls | MEDIUM | Pre-inset cut region by 0.5cm to avoid coplanar faces; merged cutter optimization; validate result mesh; fall back to rectangular indentations if boolean fails |
| Context window pressure on orchestrator | MEDIUM | SP5 generates one building at a time, registers each before moving to next. Never holds all building specs simultaneously |
| Treemap produces corridor-less plans | HIGH (fixed) | SP2 now includes corridor insertion phase between treemap and door placement |

---

## Success Criteria

The procedural town generator is "done" when:

1. `create_city_block({buildings: 4, genre: "horror", seed: 42})` produces a playable block
2. Every building is enterable with functional doors
3. No shared wall duplication, no floating geometry, no overlapping rooms
4. Roofs look like roofs (not flat slabs)
5. Windows and doors are properly placed and framed
6. Horror damage is visible and seed-deterministic
7. Player can navigate between all rooms (navmesh works)
8. Outliner is cleanly organized under `/CityBlock/`
9. The spatial registry can answer "what room am I in?" queries
10. A hospice patient can navigate the space (ADA ramps, gentle pacing)
