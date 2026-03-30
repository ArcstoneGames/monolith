# MonolithPCG Module + Modular Kit Scanner -- Master Implementation Plan

> **For agentic workers:** This is a MASTER PLAN defining 6 sub-plans across 4 delivery milestones. Each sub-plan has independent work packages (WPs) that can be dispatched to implementation agents in parallel. Dependencies between sub-plans are explicit. The killer feature (Sub-Plan B: Kit Scanner) has highest priority and should be started first.

**Status:** APPROVED WITH CHANGES — Both reviewers approved. Fixes applied below.

**Goal:** Add MonolithPCG as module #12 to the Monolith MCP plugin, providing a `pcg_query(action, params)` namespace for programmatic PCG graph construction, execution, and management. The crown jewel is `scan_modular_kit` -- point at a folder of modular meshes, auto-classify them, detect grid/openings/pivots, generate missing pieces, and build entire towns from the user's own art.

## Reviewer Fixes Applied

### From Review #1 — Technical Corrections
- **R1-C1: Kit Scanner ships independently of PCG Foundation.** `build_with_kit` (B5) uses direct HISM placement, not PCG graphs. Sub-Plan B has NO dependency on Sub-Plan A. This is a major scheduling win — the killer feature can ship before any PCG work. Updated dependency graph: B1-B5 are fully independent. A is only needed for Sub-Plans C/D/F.
- **R1-C2: `build_with_kit` is editor-time only.** Runtime generation is Sub-Plan F territory. B5 scoped explicitly as editor-time.
- **R1-I1: Build.cs dependencies for A1.** MonolithPCG module needs `GeometryScriptingCore`, `GeometryFramework`, `GeometryCore`, `MonolithMesh`, `MonolithCore` in addition to `PCG`. All optional via `WITH_PCG` and `WITH_GEOMETRYSCRIPT`.
- **R1-I2: PCG node parameter setting.** Use `FindFProperty` + `SetValue_InContainer` (reflection), NOT `JsonObjectToUStruct`. PCG settings classes use complex property types that JSON deserialization can't handle.
- **R1-I3: Spatial registry coupling.** Add null-safety for missing registry data — scanning/classification must work without a spatial registry populated.
- **R1-I4: Opening detection budget.** Revised from 40-55h to 50-65h to account for mesh-edge false positive filtering edge cases.
- **R1-I5: swap_proxies orientation data.** Proxy swap must store rotation + pivot offset per instance, not just position. Added to B5 spec.
- **R1-I6: HISM collision limitation.** `bDisableCollision = true` on rendering HISM. Separate simple box collision per room for gameplay traces. Documented in architecture section.
- **R1-I7: Classification weight renormalization.** Weights must sum to 1.0 after filtering unavailable signals (e.g., no sockets = redistribute 0.05 across other signals).
- **R1-S1: Loosen D dependency on C.** Sub-Plan D (gameplay) only needs spatial registry (already exists), not full town dressing (C). Updated dependency.

### From Review #2 — Player/Design Corrections
- **R2-C1: Kit type detection.** Added `kit_type` classification output: `modular_building`, `prop_library`, `facade_kit`, `pre_assembled`, `mixed`. Each type gets a different workflow path after scanning. Pre-assembled buildings (KitBash3D) bypass floor plan generation entirely — placed as-is with interior volumes.
- **R2-C2: Unified hospice configuration.** Added `configure_hospice_profile` action that sets ALL hospice-relevant params across all subsystems in one call: door widths, corridor widths, ramp slopes, scare intensity, gore level, flicker safety, item density, enemy aggression. Single JSON profile, single action.
- **R2-C3: Flickering light safety.** Added `visual_comfort` parameter (0.0-1.0) to all lighting actions. At 1.0 (max comfort / hospice): NO flickering, NO strobes, NO rapid intensity changes. Warm steady lighting only. At 0.0 (full horror): unrestricted. Default 0.3 (mild flicker OK, no strobes). This is a SAFETY requirement for hospice patients with photosensitive conditions.
- **R2-I1: Two-pass scan.** Phase 1 (fast, <1s): name parsing + dimensions only, gives user instant feedback. Phase 2 (slower, 2-5s): topology analysis + opening detection + material inspection. User sees results progressively.
- **R2-I2: Thumbnail generation.** `scan_modular_kit` captures a 128x128 thumbnail per classified piece for the review UI. Uses existing `editor_query("capture_thumbnail")`.
- **R2-I3: Tension plateau prevention.** Active horror layer budget: max 3 horror layers active per room. Prevents desensitization.
- **R2-I4: Bidirectional lock/key reachability.** Lock placement verified with BFS from BOTH directions — player must reach key before lock AND return path must exist.
- **R2-I5: Runtime interior pop-in.** Use LOD-0 shell (exterior only) for distant buildings, generate interior when player enters trigger volume. 500ms generation budget per interior.
- **R2-I6: Kit JSON path portability.** Use relative paths from project Content root, not absolute. Kit JSONs shareable across machines.
- **R2-S1: Split Sub-Plan C for faster delivery.** C1 (vegetation + debris) ships as MVP. C2 (roads + vehicles) follows. C3 (lighting + audio atmosphere) after.

**Architecture:** Three-layer hybrid (established in research):
1. **GeometryScript** -- mesh vocabulary generation (unique shells, fallback pieces, terrain-adaptive foundations)
2. **Floor Plan System** -- the orchestrator (our existing SP2 treemap + adjacency, spatial registry, building descriptors)
3. **PCG** -- placement and instancing (furniture scatter, horror dressing, vegetation, street furniture, all via ISM/HISM)

**Tech Stack:** UE 5.7 (PCG Production-Ready), C++ (new MonolithPCG module), GeometryScript (fallback piece generation), JSON (kit configs, presets), PCGEx (MIT, optional for road networks).

**Estimated Total:** ~800-1080h across all sub-plans (multi-month roadmap).

---

## Table of Contents

1. [Delivery Milestones](#delivery-milestones)
2. [Sub-Plan A: MonolithPCG Module Foundation](#sub-plan-a-monolithpcg-module-foundation)
3. [Sub-Plan B: Modular Kit Scanner (Killer Feature)](#sub-plan-b-modular-kit-scanner-killer-feature)
4. [Sub-Plan C: PCG Town Dressing](#sub-plan-c-pcg-town-dressing)
5. [Sub-Plan D: PCG Gameplay Integration](#sub-plan-d-pcg-gameplay-integration)
6. [Sub-Plan E: Terrain + Landscape](#sub-plan-e-terrain--landscape)
7. [Sub-Plan F: Runtime + Streaming](#sub-plan-f-runtime--streaming)
8. [Cross-Cutting Concerns](#cross-cutting-concerns)
9. [File Conflict Analysis](#file-conflict-analysis)
10. [Risk Register](#risk-register)

---

## Delivery Milestones

```
Milestone 1: "I can build PCG graphs from MCP and scan any modular kit"
  Sub-Plan A (Foundation) + Sub-Plan B (Kit Scanner)
  Duration: ~8-12 weeks
  Actions added: ~60-75

Milestone 2: "Generated towns have vegetation, horror dressing, and atmosphere"
  Sub-Plan C (Town Dressing)
  Duration: ~6-10 weeks (can overlap M1 tail)
  Actions added: ~25-35

Milestone 3: "Gameplay elements are procedurally placed"
  Sub-Plan D (Gameplay Integration)
  Duration: ~5-8 weeks
  Actions added: ~20-25

Milestone 4: "Towns stream at runtime and adapt to terrain"
  Sub-Plan E (Terrain) + Sub-Plan F (Runtime)
  Duration: ~8-12 weeks
  Actions added: ~20-30
```

### Inter-Sub-Plan Dependencies

```
Sub-Plan A ──────────────────────────────────┐
  (PCG Foundation)                           │
  Must complete Phase A1 before any          │
  other sub-plan can use PCG                 │
                                             │
Sub-Plan B ──────────────────────────────────┤
  (Kit Scanner)                              │
  Independent of A for scanning/classify.    │
  Needs A Phase 1 for build_with_kit         │
  (PCG placement of scanned pieces)          │
                                             │
Sub-Plan C ──── requires A Phase 1-2 ────────┤
  (Town Dressing)                            │
                                             │
Sub-Plan D ──── requires A Phase 1-2, C ─────┤
  (Gameplay)                                 │
                                             │
Sub-Plan E ──── independent of PCG ──────────┤
  (Terrain)    (uses landscape APIs, not PCG)│
  PCG reads terrain after E modifies it      │
                                             │
Sub-Plan F ──── requires A, B, C complete ───┘
  (Runtime/Streaming)
```

---

## Sub-Plan A: MonolithPCG Module Foundation

**Estimated effort:** 80-110h
**Research docs:** `pcg-framework-research`, `pcg-mcp-integration-research`, `hybrid-gs-pcg-research`
**Deliverable:** New `MonolithPCG` module with `pcg_query` namespace, ~35 actions for graph CRUD, execution, templates, and spatial registry bridge.

### Architecture Decision Record

The module is a **graph construction + execution bridge**, not a PCG extension toolkit. Monolith's role is orchestration via MCP -- agents construct PCG graphs programmatically from built-in UE nodes, set parameters, trigger execution, and read results. Custom PCG nodes (UPCGSettings subclasses) are limited to 2-3 data bridge nodes for reading our spatial registry.

Key API surfaces (all verified in engine source):
- `UPCGGraph::AddNodeOfType()`, `AddEdge()`, `SetGraphParameter<T>()` -- graph construction
- `UPCGComponent::GenerateLocal()`, `SetGraph()` -- execution
- `UPCGSubsystem::ScheduleComponent()` -- low-level scheduling
- `FInstancedPropertyBag` (StructUtils) -- user parameters
- `UPCGPointData`, `UPCGPolygon2DData`, `UPCGParamData` -- data types
- PCG nodes auto-register via UE reflection (no manual registration)

### Phase A1: Module Bootstrap + Graph CRUD (25-30h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| A1.1 | Create MonolithPCG module scaffold | `Source/MonolithPCG/MonolithPCG.Build.cs`, `Public/MonolithPCGModule.h`, `Private/MonolithPCGModule.cpp` | Follow MonolithMesh pattern. Build.cs: depend on `PCG`, `MonolithCore`, `MonolithIndex`, `StructUtils`, `Json`, `JsonUtilities`, `AssetRegistry`, `AssetTools`, `UnrealEd`. Optional `PCGEditor` if editor build. WITH_PCG define gated like WITH_GEOMETRYSCRIPT. | Compiles cleanly with UBT. Module loads in editor log. | 4h |
| A1.2 | Register `pcg_query` namespace in MonolithCore | `MonolithPCGModule.cpp`, minor edit to `MonolithCore` registration | Register namespace "pcg" with tool name "pcg_query" in `FMonolithToolRegistry`. Follow pattern of MonolithMesh registration. | `monolith_discover("pcg")` returns action list. | 3h |
| A1.3 | Add module + plugin deps to uplugin | `Monolith.uplugin` | Add `MonolithPCG` module entry (Type: Editor, LoadingPhase: Default). Add `PCG` plugin dependency (Enabled: true, Optional: true). | Plugin loads without errors. PCG available. | 1h |
| A1.4 | Implement graph CRUD actions (8 actions) | `Public/MonolithPCGActions.h`, `Private/MonolithPCGActions.cpp` | Actions: `create_pcg_graph`, `list_pcg_graphs`, `get_pcg_graph_info`, `add_pcg_node`, `remove_pcg_node`, `connect_pcg_nodes`, `disconnect_pcg_nodes`, `set_pcg_node_params`. For `add_pcg_node`: maintain a `TMap<FString, TSubclassOf<UPCGSettings>>` mapping friendly names ("SurfaceSampler", "StaticMeshSpawner", "DensityFilter", etc.) to settings classes. Use `UPCGGraph::AddNodeOfType()`. For connections: `UPCGGraph::AddEdge(FromNode, FromPin, ToNode, ToPin)`. For params: use `FJsonObjectConverter::JsonObjectToUStruct` to apply JSON properties to UPCGSettings UPROPERTY fields. | Can create a graph, add 3 nodes (Surface Sampler -> Density Filter -> Static Mesh Spawner), connect them, and read back the graph structure. | 14h |
| A1.5 | PCG settings class resolver | `Private/MonolithPCGSettingsResolver.h/.cpp` | Scan all UPCGSettings subclasses at module startup via `GetDerivedClasses()`. Build friendly-name -> class map stripping "UPCG" prefix and "Settings" suffix (e.g., `UPCGSurfaceSamplerSettings` -> "SurfaceSampler"). Cache as TMap. Expose `list_pcg_node_types` action. | `list_pcg_node_types` returns 50+ node types. `add_pcg_node({type: "SurfaceSampler"})` resolves correctly. | 5h |

**Done criteria for Phase A1:** An agent can create a PCG graph asset, add nodes, wire them, set parameters, and query the result -- all via MCP.

### Phase A2: Execution + Scene Integration (20-25h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| A2.1 | Parameter + execution actions (7 actions) | `Public/MonolithPCGExecutionActions.h`, `Private/MonolithPCGExecutionActions.cpp` | Actions: `add_pcg_parameter`, `set_pcg_parameter`, `get_pcg_parameters`, `execute_pcg`, `cleanup_pcg`, `get_pcg_output`, `cancel_pcg_execution`. Parameters via `UPCGGraph::AddUserParameters()` (FPropertyBagPropertyDesc) and `SetGraphParameter<T>()`. Execution via `UPCGComponent::GenerateLocal(bForce)`. Output via `GetGeneratedGraphOutput()` returning point counts, spawned actor names, ISM instance counts. | Set a float param, execute, read output showing spawned meshes. | 12h |
| A2.2 | Scene integration actions (7 actions) | `Public/MonolithPCGSceneActions.h`, `Private/MonolithPCGSceneActions.cpp` | Actions: `spawn_pcg_volume`, `spawn_pcg_component`, `set_pcg_component_graph`, `list_pcg_components`, `refresh_pcg_component`, `get_pcg_generated_actors`, `set_pcg_generation_trigger`. Spawn `APCGVolume` with SetFolderPath("Monolith/PCG"). Set generation trigger enum. List via `TActorIterator<APCGVolume>` + component scan. | Spawn a volume, assign a graph, execute, list generated actors. | 10h |

**Done criteria for Phase A2:** Full create-configure-execute-inspect loop works via MCP. An agent can create a scatter graph, place a volume, execute it, and see spawned meshes in the level.

### Phase A3: Templates + Presets (15-20h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| A3.1 | Template graph builders (8 actions) | `Public/MonolithPCGTemplateActions.h`, `Private/MonolithPCGTemplateActions.cpp` | Actions: `create_scatter_graph`, `create_spline_scatter_graph`, `create_vegetation_graph`, `create_horror_dressing_graph`, `create_debris_graph`, `apply_pcg_preset`, `save_pcg_preset`, `list_pcg_presets`. Each template builds a complete graph programmatically: e.g., `create_scatter_graph` builds `GetActorData -> SurfaceSampler -> DensityFilter -> SelfPruning -> StaticMeshSpawner` with user params for density, mesh_list, min_distance, seed. Presets serialize/deserialize to `Saved/Monolith/PCGPresets/`. | `create_scatter_graph({density: 0.5, meshes: ["/Game/Props/Chair"]})` produces a working graph that spawns chairs when executed. | 15h |

### Phase A4: Spatial Registry Bridge + Custom PCG Nodes (20-25h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| A4.1 | Bridge actions (5 actions) | `Public/MonolithPCGBridgeActions.h`, `Private/MonolithPCGBridgeActions.cpp` | Actions: `spatial_registry_to_pcg`, `pcg_output_to_registry`, `building_descriptor_to_pcg`, `create_room_scatter_graph`, `batch_execute_pcg`. `spatial_registry_to_pcg`: Read spatial registry JSON from `Saved/Monolith/SpatialRegistry/`, convert rooms to UPCGPointData (one point per room, with room_type/decay/bounds attributes) OR UPCGPolygon2DData (room boundary as closed polygon). `pcg_output_to_registry`: Read generated actors, write back to spatial registry. `batch_execute_pcg`: Execute multiple PCG components with dependency chain via `UPCGSubsystem::ScheduleComponent()`. | Convert a building with 6 rooms to PCG data, execute a room scatter graph, verify furniture placed inside room bounds. | 10h |
| A4.2 | Custom PCG data source node: FloorPlan | `Public/PCGNodes/PCGFloorPlanDataSource.h`, `Private/PCGNodes/PCGFloorPlanDataSource.cpp` | `UPCGFloorPlanSettings : UPCGSettings` + `FPCGFloorPlanElement : IPCGElement`. Settings: SpatialRegistryPath (FString), BuildingId (FString). No input pins (data source). Three output pins: "WallPoints" (Point data with wall_type, has_door, has_window, room_id, decay, floor_index attributes), "RoomPolygons" (Polygon2D data for each room boundary), "BuildingParams" (ParamData with style, floors, grid_size). Auto-discovered by PCG via reflection. | Node appears in PCG editor palette. Connect to StaticMeshSpawner, execute, see wall pieces placed at correct positions. | 8h |
| A4.3 | Custom PCG data source node: ModularKit | `Public/PCGNodes/PCGModularKitDataSource.h`, `Private/PCGNodes/PCGModularKitDataSource.cpp` | `UPCGModularKitSettings : UPCGSettings`. Settings: KitPath (FString, path to kit JSON). Output pin: "KitPieces" (ParamData with piece_type, asset_path, dimensions, opening_info per entry). Used downstream by PCG graphs to drive mesh selection based on wall_type attribute matching. | Load a scanned kit JSON, output piece catalog as PCG param data. | 5h |

**Done criteria for Phase A4:** PCG graphs can consume our spatial registry data natively. A building's rooms flow into PCG as Polygon2D data, enabling native interior surface sampling for furniture scatter.

### Phase A Summary

| Phase | Actions | Est Hours | Dependencies |
|-------|---------|-----------|--------------|
| A1: Module + Graph CRUD | 9 (+list_pcg_node_types) | 25-30h | None |
| A2: Execution + Scene | 14 | 20-25h | A1 |
| A3: Templates + Presets | 8 | 15-20h | A1, A2 |
| A4: Bridge + Custom Nodes | 5 + 2 custom PCG nodes | 20-25h | A1, A2, existing spatial registry |
| **Total** | **~37 actions** | **80-110h** | |

---

## Sub-Plan B: Modular Kit Scanner (Killer Feature)

**Estimated effort:** 120-160h
**Research docs:** `asset-scanning-research`, `mesh-opening-detection-research`, `marketplace-kit-conventions-research`, `kit-scanner-ux-research`, `grid-detection-research`, `fallback-piece-generation-research`, `modular-building-research`, `modular-pieces-research`, `hism-assembly-research`
**Deliverable:** `scan_modular_kit` and supporting actions that let a user point at ANY folder of modular meshes and immediately get classified pieces, detected grid, gap analysis, generated fallbacks, and the ability to build entire towns from their art.

### The Golden Path (3-Turn UX)

1. User: "scan /Game/HorrorKit/Meshes/ as a modular building kit"
   - System scans 47 meshes, classifies by 5-signal pipeline, detects 200cm grid, reports 78% coverage, flags 6 uncertain pieces
2. User: "SM_Panel_Large is wall_solid, SM_Arch_001 is wall_door, accept the rest"
   - System updates classifications, persists corrections, coverage rises to 82%
3. User: "generate a 4-building horror block using HorrorKit"
   - System generates buildings using scanned pieces via HISM, GeometryScript fills 4 missing piece types

### Architecture

```
scan_modular_kit
  |
  +-- Asset Registry scan (IAssetRegistry::GetAssets with FARFilter)
  +-- Per-mesh classification pipeline:
  |     1. Name parsing (regex, ~80% accuracy on well-named)
  |     2. Dimension analysis (bounds ratios, height/width/thickness)
  |     3. Material slot analysis (names like "glass", "frame" = window)
  |     4. Topology analysis (FMeshBoundaryLoops for opening detection)
  |     5. Socket detection (snap points imply modular)
  |
  +-- Grid detection (candidate scoring + histogram validation)
  +-- Pivot detection (bounds.Origin vs mesh AABB)
  +-- Coverage analysis (present vs missing piece types)
  +-- Fallback plan (GeometryScript generation for missing types)
  +-- Kit persistence (JSON at Saved/Monolith/ModularKits/{name}.json)
  |
  v
build_with_kit
  |
  +-- Read kit JSON
  +-- Read building descriptor (from existing SP1/SP2)
  +-- Map wall edges to kit pieces (wall_type -> piece_type)
  +-- Place via HISM (one HISM component per unique mesh)
  +-- Generate fallback pieces on-demand for missing types
  +-- Outliner organization (SetFolderPath)
```

### Phase B1: Asset Scanning + Name Classification (20-25h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| B1.1 | Core scanning infrastructure | `Public/MonolithPCGKitScannerActions.h`, `Private/MonolithPCGKitScannerActions.cpp` (or in separate MonolithPCG module, or extend MonolithMesh -- see file conflict analysis) | `IAssetRegistry::ScanPathsSynchronous()` + `GetAssets(FARFilter)` with `ClassPaths = UStaticMesh`. Recursive. Collect `FAssetData` array. Load each mesh for bounds/tris/materials. Return structured scan results. | Scan a folder of 30+ meshes, return all with bounds/tris/material counts in <5s. | 6h |
| B1.2 | Name parsing classifier | Same files | Regex-based classifier using marketplace conventions survey. Primary regex: `^SM_(?:Env_)?(?<tileset>\w+_)?(?<type>Wall\|Floor\|Ceiling\|Roof\|Stair\|Door\|Window\|Trim\|Column\|Pillar\|Railing\|Balcony)(?:_(?<subtype>...))?(?:_(?<size>...))?(?:_(?<variant>...))?$`. FRegexMatcher in UE. Secondary patterns for material-first naming (SM_brick_wall), tileset naming (SM_Env_Dungeon_Wall), and size encoding (_1x1, _200, _Half). Output: predicted type + confidence score (0.0-1.0). | Correctly classifies 80%+ of Synty POLYGON naming, PurePolygons naming, and Junction City naming patterns. | 6h |
| B1.3 | Dimension analysis classifier | Same files | Analyze bounds ratios: Wall = height >> width >> thickness (Z > X > Y with Y < 30cm). Floor = width ~= depth, height < 10cm. Stair = moderate aspect with stepped height. Use ratio thresholds derived from marketplace survey: wall thickness 8-25cm, wall height 240-360cm, floor thickness 3-10cm. Output: predicted type + confidence. | Correctly classifies a 200x15x300 mesh as wall_solid, 200x200x5 as floor_tile. | 5h |
| B1.4 | Material slot classifier | Same files | Scan material slot names for keywords: "glass"/"window" -> has window, "frame"/"trim" -> trim piece, "floor"/"tile" -> floor, "wall"/"brick"/"concrete" -> wall. Count of unique materials: 1 = simple piece, 3-4 = wall with openings (wall + glass + frame + trim). Boost confidence of other signals. | Material slots named "M_Glass" and "M_WoodFrame" boost window wall classification by +0.15 confidence. | 3h |
| B1.5 | Multi-signal fusion + confidence scoring | Same files | Weighted sum: name_score * 0.35 + dimension_score * 0.30 + material_score * 0.15 + topology_score * 0.15 + socket_score * 0.05. Three-tier routing: >0.80 auto-accept, 0.50-0.80 flag for review, <0.50 unclassified. Persist per-mesh classification to kit JSON. | Combined confidence exceeds individual signals. A mesh with matching name + dimensions scores >0.85. | 4h |

**Done criteria for Phase B1:** `scan_modular_kit` returns structured JSON with per-mesh classifications, confidence scores, category counts, and uncertain items flagged for review.

### Phase B2: Grid + Opening + Pivot Detection (25-35h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| B2.1 | Grid size detection | `Private/MonolithPCGGridDetection.h/.cpp` | Candidate scoring algorithm from research: Generate candidates from industry priors {50, 100, 150, 200, 250, 300, 400, 512} + data-derived (smallest dim, most common, pairwise GCDs of top-5 dims). Score each: inlier_ratio * 100 - mean_residual * 0.5 + industry_bonus + size_bonus. Tolerance 5cm default. Validate with histogram peak detection. Return grid_size + confidence. | 200cm kit detected as 200cm (not 100 or 50). 300cm kit detected correctly even with one 195cm outlier. | 8h |
| B2.2 | Mesh opening detection | `Private/MonolithPCGOpeningDetection.h/.cpp` | Pipeline: `CopyMeshFromStaticMeshV2()` -> `FDynamicMesh3` -> `FMeshBoundaryLoops::Compute()` -> filter (reject perimeter < 10cm cracks, reject loops touching mesh AABB faces) -> classify per loop by `GetBounds()` dimensions and position: Door (sill ~0, H 180-260, W 70-180), Window (sill > 60cm, H 60-180, W 40-200), Archway (sill ~0, full height), Vent (any dim < 30cm). Validate with ray-cast pass-through (grid of rays through opening center, >80% pass = confirmed). Output: `TArray<FOpeningDescriptor>` with type, bounds, centroid, normal, confidence. **Editor-time only** (source mesh required, cooked mesh has split topology). | Wall with door opening: 1 boundary loop detected, classified as door, dimensions within 5% of actual. Wall with 2 windows: 2 loops, both classified as window. Solid wall: 0 loops (closed mesh). | 12h |
| B2.3 | Pivot point detection | Same files as B1 | Analyze `FBoxSphereBounds::Origin` relative to mesh AABB. Classify: base_center (origin at bottom face center), base_corner (origin at min corner), center (origin at volume center), wall_center_bottom (origin at center-bottom of thin face). Report pivot type per mesh. Detect pivot consistency across kit. | Correctly identifies base_center pivot on PurePolygons pieces. Flags inconsistent pivots across a kit. | 4h |
| B2.4 | Socket detection | Same files as B1 | Iterate `UStaticMesh::Sockets`. Report socket names, positions, tags. Classify: "Snap_*" = connection points, "Attach_*" = decoration points. Detect if kit uses socket-based snapping vs grid-based. | Kit with sockets: reports snap points and their positions. Kit without sockets: reports "grid-based" assumption. | 3h |
| B2.5 | `classify_mesh` single-mesh action | Same action files | Expose the full classification pipeline for a single mesh. Useful for debugging and iterative correction. | `classify_mesh({asset: "/Game/Kit/SM_Wall_01"})` returns all 5 signal scores, predicted type, confidence, openings, pivot, dimensions. | 3h |

**Done criteria for Phase B2:** Grid size auto-detected. Openings detected and classified at 95%+ accuracy on well-formed kits. Pivots detected. Full per-mesh diagnostic available.

### Phase B3: Kit Management + Corrections (15-20h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| B3.1 | Kit persistence (JSON) | `Private/MonolithPCGKitPersistence.h/.cpp` | Kit config saved to `Saved/Monolith/ModularKits/{kit_name}.json`. Schema: `{kit_name, scan_path, grid_size_cm, grid_confidence, pivot_type, wall_thickness_cm, wall_height_cm, pieces: [{asset_path, piece_type, confidence, dimensions, openings, pivot, material_slots, is_user_override}], corrections: [{asset, old_type, new_type, timestamp}], coverage: {percentage, present, missing, fallback_plan}}`. Load/save via `FFileHelper::SaveStringToFile` + `FJsonSerializer`. | Save and reload a kit with 47 pieces. Corrections persist across editor restarts. | 6h |
| B3.2 | Classification correction actions | Same action files | Actions: `edit_kit_classification({kit, asset, new_type})` for single correction. `edit_kit_classification({kit, pattern: "SM_Panel*", new_type: "wall_solid"})` for batch by glob. `accept_uncertain({kit})` to auto-accept all uncertain at current predicted type. `reset_kit_classification({kit, asset?})` to re-scan. Mark corrections as `is_user_override: true` so re-scans don't overwrite. | Correct a piece from "furniture" to "wall_solid". Re-scan the kit -- user override persists. | 5h |
| B3.3 | Kit listing and management | Same action files | Actions: `list_kits` (scan Saved/Monolith/ModularKits/), `get_kit_info({kit})`, `delete_kit({kit})`, `export_kit({kit})` (copy JSON for sharing), `import_kit({json_path})`. Kit info includes coverage summary, piece counts by type, grid info. | List kits, get info for one, delete another. Export and re-import a kit JSON. | 4h |
| B3.4 | Asset metadata stamping | Same files | After classification, stamp each mesh with `UMonolithKitMetadata : UAssetUserData` containing piece_type, kit_name, grid_units. Override `GetAssetRegistryTags()` for fast future lookups without full load. Future scans check for existing metadata first (skip re-classification). | Scan a kit, verify asset tags are written. Re-scan the same kit -- metadata-tagged meshes skip classification (fast path). | 4h |

**Done criteria for Phase B3:** Kits persist as shareable JSON files. User corrections stick across sessions. Asset metadata enables fast re-scanning.

### Phase B4: Fallback Piece Generation (25-35h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| B4.1 | Dimension extraction from kit | `Private/MonolithPCGFallbackGeneration.h/.cpp` | Extract from classified kit pieces: grid_size (from B2.1), wall_thickness (min bounding dim of wall pieces, median), wall_height (max Z of wall pieces, median), floor_thickness (Z of floor pieces), door_dimensions (from opening detection on wall_door pieces, or defaults 100x230), window_dimensions (from opening detection on wall_window pieces, or defaults 80x120 at sill 90cm). Store in kit JSON as `kit_params`. | Extracts correct dimensions from a kit with 12 wall pieces and 4 floors. | 4h |
| B4.2 | P0 fallback generators (5 piece types) | Same files | Generate: wall_solid (AppendBox), floor_tile (AppendBox), ceiling_tile (AppendBox), corner_outer (L-shaped AppendSimpleExtrudePolygon), corner_inner (inverted L). All use kit_params for dimensions. MaterialID assignment per face group. Box UV projection. Save via existing `SaveMeshToAsset()` at `Saved/Monolith/ModularKits/{kit}/Fallbacks/SM_Fallback_{type}.uasset`. | Generate all 5 P0 types. Each has correct dimensions matching the kit. Each saves as valid UStaticMesh. | 8h |
| B4.3 | P1 fallback generators (5 piece types) | Same files | Generate: wall_door (wall box + Selection+Inset+Delete for door opening), wall_window (wall box + Selection+Inset+Delete for window, sill preserved), wall_t_junction (T-shaped extrusion from 2D polygon), wall_end_cap (box with one exposed end face), wall_half (box at half wall_height). Use Selection+Inset technique from modular-pieces-research -- select front face triangles in opening region, inset by bevel_width, delete inner selection. | wall_door has a boundary loop detected by opening detection (validates correct opening geometry). wall_t_junction has correct T-shaped cross-section. | 10h |
| B4.4 | P2 fallback generators (4 piece types) | Same files | Generate: floor_stairwell (floor box with rectangular cutout via Selection+Delete), stair_straight (stepped geometry via repeated AppendBox or AppendSimpleExtrudePolygon with step profile), stair_landing (flat box at landing height), wall_x_junction (column at intersection). IBC-compliant stair geometry: 17.8cm rise, 27.9cm run. | Stair has correct rise/run. Floor cutout matches stairwell dimensions. | 8h |
| B4.5 | Three-tier quality system | Same files | Tier 1 "Blockout": Box-only geometry, single material, <1ms per piece. Tier 2 "Functional": Box + openings + basic UV, 2-3 materials, <5ms. Tier 3 "Style-Matched": Detect bevel width from kit (dihedral angle analysis on existing wall pieces), apply matching bevels to fallbacks, attempt baseboard via bottom-face height segmentation, <20ms. Default: Tier 2. User override via `generation_quality` param. | Tier 1 fallback is a box. Tier 2 has openings. Tier 3 has matching bevel width (tested against a kit with 5cm beveled edges). | 5h |
| B4.6 | Fallback caching + invalidation | Same files | Cache generated fallbacks at `Saved/Monolith/ModularKits/{kit}/Fallbacks/`. Regenerate only when kit_params change (hash kit_params, compare on next build). `regenerate_fallbacks({kit})` action for manual refresh. | Generate fallbacks for a kit. Re-build -- no regeneration (cache hit). Change grid size -- fallbacks regenerate. | 3h |

**Done criteria for Phase B4:** All 14 fallback piece types generate correctly at matching kit dimensions. Three quality tiers work. Caching prevents redundant generation.

### Phase B5: Build With Kit + Proxy Swap (20-30h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| B5.1 | `build_with_kit` core action | `Private/MonolithPCGBuildWithKitActions.h/.cpp` (or extend kit scanner actions) | Input: kit_name + building_descriptor JSON. Process: Read kit JSON -> for each wall edge in building descriptor, map wall_type + opening_type to kit piece_type -> select best matching piece from kit (exact match, then closest dimensions, then fallback) -> place via HISM. One HISM component per unique mesh (wall_solid, wall_door, etc.), hosted on a single parent actor per building. Transform = edge position + rotation from building descriptor. Floors and ceilings via floor_tile HISM. SetFolderPath("Monolith/Buildings/{building_id}"). | Build a 6-room building using a scanned kit. All walls correct type. HISM draw calls < 10 per building. | 12h |
| B5.2 | Piece selection algorithm | Same files | For each edge, rank kit pieces by: 1) exact type match (wall_solid, wall_door, etc.), 2) dimension match (within 5cm tolerance), 3) variant diversity (avoid same variant repeatedly -- weighted random from available variants). If no kit piece matches: use fallback. If multiple match: prefer user_override > high_confidence > first_alphabetical. | Given a kit with 3 wall_solid variants, all 3 appear in a building (not just variant_01 repeated). | 5h |
| B5.3 | `swap_proxies` action | Same files | Replace blockout/whitebox geometry with real kit pieces. Input: actors to swap + kit_name. For each actor: get bounds -> find closest kit piece by dimensions and type tag -> replace actor with HISM instance of kit piece at same transform. Supports "swap all in folder" mode. | Replace 20 whitebox wall actors with kit pieces. All positions preserved. Original actors deleted. | 6h |
| B5.4 | `build_block_with_kit` action | Same files | Higher-level action: generate N buildings on a city block layout using a specific kit. Calls existing `create_city_block` -> building descriptors -> `build_with_kit` per building. Returns summary (building count, piece count, fallback count, draw calls). | Generate a 4-building block with a scanned kit in a single MCP call. | 5h |

**Done criteria for Phase B5:** End-to-end workflow works: scan kit -> build buildings -> see HISM instances in the level. Proxy swap replaces whitebox with real pieces.

### Phase B Summary

| Phase | Actions | Est Hours | Dependencies |
|-------|---------|-----------|--------------|
| B1: Asset Scan + Name Classification | scan_modular_kit (partial) | 20-25h | None |
| B2: Grid + Opening + Pivot | classify_mesh + grid/opening detection | 25-35h | B1 |
| B3: Kit Management | edit/list/export/import kit actions (~6) | 15-20h | B1, B2 |
| B4: Fallback Generation | regenerate_fallbacks + internal generators | 25-35h | B1, B2 |
| B5: Build With Kit | build_with_kit, swap_proxies, build_block_with_kit | 20-30h | B1-B4, A1 (for PCG placement), existing building system |
| **Total** | **~15-20 actions** | **120-160h** | |

---

## Sub-Plan C: PCG Town Dressing

**Estimated effort:** 200-280h
**Research docs:** `pcg-vegetation-research`, `pcg-horror-atmosphere-research`, `pcg-lighting-audio-research`, `pcg-vehicles-props-research`, `pcg-roads-research`
**Deliverable:** PCG graph templates + MCP actions for vegetation, horror atmosphere, lighting, props, and road networks.
**Dependency:** Requires Sub-Plan A Phase A1-A2 complete (graph construction + execution).

### Phase C1: Vegetation + Foliage (40-55h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| C1.1 | Base vegetation scatter graph template | `Private/MonolithPCGVegetationTemplates.cpp` | `create_vegetation_graph` builds: Landscape Data -> Surface Sampler (density param) -> Normal to Density (slope filter) -> Density Filter (min/max slope) -> Difference (building footprint exclusion) -> Self Pruning -> Static Mesh Spawner (weighted mesh list). User params: density, slope_min, slope_max, seed, mesh_list, exclusion_actors. | Grass spawns on flat areas, excluded from building footprints. | 10h |
| C1.2 | Tree + bush placement template | Same files | Separate template for larger vegetation: lower density, larger bounds for Self Pruning, distance-from-building filter via Distance node. Height-based species selection (taller trees in parks, smaller bushes near buildings). | Trees appear in open areas, bushes near building perimeters, no overlap. | 8h |
| C1.3 | Decay-driven overgrowth | Same files | Overgrowth increases with building decay param. PCG graph: sample points near building perimeters -> density = decay * max_density -> spawn overgrowth meshes (dead vines, weeds breaking through pavement, grass in gutters). Two modes: floor-level (weeds) and wall-level (wall decals for ivy/moss). | Decay 0.8 building has dense weeds at base, decay 0.2 building has minimal. | 10h |
| C1.4 | Ground cover (litter, mud, debris) | Same files | Surface scatter with decay-driven density. Different mesh tables per context: street (litter, paper, cans), yard (leaves, twigs), parking lot (oil stains decals). | Streets have litter, yards have leaves, density scales with decay. | 8h |
| C1.5 | Fog zone placement | Same files | Spawn Local Volumetric Fog actors via PCG SpawnActor node at low-lying areas. Place near water features, in alleys, in dense vegetation. Density driven by time-of-day param and horror_intensity. | Fog actors appear in alleys and low areas, with configurable density. | 6h |

### Phase C2: Horror Atmosphere -- 7-Layer System (50-65h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| C2.1 | Layer 1: Structural decay | `Private/MonolithPCGHorrorTemplates.cpp` | PCG graph driven by per-room decay attribute. Decay 0.5-0.8: spawn broken furniture variants, displaced ceiling tiles, cracked floor decals. Decay 0.8-1.0: collapsed wall sections, rubble piles, exposed rebar. Uses Attribute Filter on decay ranges. | Room at decay 0.7 has broken furniture. Room at 0.9 has rubble. Room at 0.2 is clean. | 10h |
| C2.2 | Layer 2: Surface contamination | Same files | Decal scatter on floors (blood, water stains, oil), walls (handprints, scratches, mold). Uses existing `place_decals` infrastructure but driven by PCG point generation for batch efficiency. Decal density = f(decay, room_type). | Bathroom has water stains, hallway has drag marks at high decay. | 8h |
| C2.3 | Layer 3: Debris + clutter | Same files | Room interior scatter: papers, broken glass, toppled chairs, scattered medical supplies. Mesh selection weighted by room_type (medical rooms get syringes, offices get papers). Uses PCG Polygon2D interior surface sampling on room boundaries. | Each room type has thematically appropriate debris. | 8h |
| C2.4 | Layer 4: Organic growth | Same files | Corner cobwebs (sample polygon vertices, spawn at corners where decay > 0.3). Mold patches (wall surface scatter, UV-aligned decals). Dead insects (floor scatter in high-decay rooms). | Cobwebs appear in corners of abandoned rooms. | 6h |
| C2.5 | Layer 5: Light degradation | Same files | Spawn light BP actors at ceiling height. Working/flickering/broken ratio driven by decay (70-80% broken at high decay). Flickering lights use Timeline BP with randomized keyframes. Broken lights are mesh-only (dark). | Horror corridor: mostly dark with 1-2 flickering lights and 1 working. | 8h |
| C2.6 | Layer 6: Audio atmosphere | Same files | Spawn ambient sound emitters via PCG SpawnActor: dripping water (bathrooms, high decay), electrical hum (near transformers), wind through broken windows, distant creaking. Volume = f(decay). | Each room type has appropriate ambient audio at correct volume. | 6h |
| C2.7 | Layer 7: Environmental storytelling | Same files | Composite "vignette" spawning: combine existing `place_storytelling_scene` patterns with PCG-driven placement. Select vignette type based on room_type + decay + tension_level. Place 0-2 vignettes per room. | Rooms have contextually appropriate horror vignettes. | 7h |

### Phase C3: Vehicles, Props, Street Furniture (35-50h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| C3.1 | Per-lot prop system | `Private/MonolithPCGPropTemplates.cpp` | PCG graph template: for each building lot, spawn context-appropriate props. Residential: mailbox, trash cans, garden hose, patio furniture. Commercial: dumpster, signage, AC units. Per-lot props placed at lot boundary from spatial registry. | Residential lot has mailbox at road edge, commercial has dumpster in back. | 10h |
| C3.2 | Street furniture | Same files | Spline-based placement: fire hydrants (90-180m, prefer corners), benches (20-50m near commercial), newspaper boxes (clusters near commercial), phone booths (rare), street signs (at intersections). All driven by road spline + building type data. | Street has regularly spaced hydrants, benches near shops, signs at intersections. | 10h |
| C3.3 | Vehicle placement | Same files | Driveway vehicles (0-2 per residential lot, probability = 1 - lot_abandonment). Street parking (parallel, along curb, probability-based per block). Abandoned vehicles: shifted/rotated from proper parking, doors open, broken windows. Wrecked vehicles at key horror locations. | Residential driveways have cars. Some abandoned cars block streets at high decay. | 8h |
| C3.4 | Horror prop overlays | Same files | Missing person posters (on poles, walls), police tape (across doors), quarantine signs, boarded windows (facade-aligned). Density = horror_intensity parameter. | High-horror areas have missing person posters and police tape. | 7h |

### Phase C4: Road Network Generation (40-55h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| C4.1 | Road graph to spline conversion | `Private/MonolithPCGRoadActions.h/.cpp` | Action: `create_road_network`. Input: road graph (nodes = intersections, edges = road segments with type). Output: USplineComponent per road segment. Support simple grid generator with randomized dead-ends for fully procedural mode. Also accept manually placed spline actors. | Generate a 4x3 block grid with 2 dead ends. Splines follow road centerlines. | 10h |
| C4.2 | Road surface + sidewalk | Same files | Spline mesh deformation for road surface. Sidewalk as offset spline with modular curb pieces. Road width from hierarchy (main: 1200cm, residential: 800cm, alley: 400cm). Curb modular pieces placed along spline via PCG Spline Sampler. | Roads have surfaces, curbs, and sidewalks at correct widths. | 12h |
| C4.3 | Road markings + infrastructure | Same files | Decal-based road markings (center line, parking lines). Street lamps along road splines (distance-based placement, working/broken ratio from decay). Power lines (catenary spline meshes between poles). | Roads have lane markings, regularly spaced street lamps, power lines. | 10h |
| C4.4 | Intersection handling | Same files | Dedicated intersection meshes/decals at spline junctions. T-junction and 4-way variants. Detect intersection points from road graph. Place intersection assets, extend/trim road splines to meet cleanly. | T-junctions and 4-way intersections have clean geometry and markings. | 10h |

### Phase C5: Integration + Presets (15-20h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| C5.1 | `dress_town_block` composite action | `Private/MonolithPCGTownDressingActions.cpp` | Orchestrator action: given a city block with buildings, run all dressing passes in order: roads -> vegetation -> street furniture -> per-lot props -> vehicles -> horror atmosphere. Each pass is a PCG graph template. Configurable via preset JSON. | Single call produces a fully dressed town block. | 8h |
| C5.2 | Dressing presets | Same files + `Saved/Monolith/PCGPresets/` | Pre-built presets: "abandoned_suburb", "active_downtown", "hospital_campus", "industrial_district". Each preset configures decay ranges, prop densities, vegetation types, road hierarchy, lighting ratios. | Apply "abandoned_suburb" preset, get appropriate dressing. Switch to "active_downtown", result changes accordingly. | 6h |

### Phase C Summary

| Phase | Actions | Est Hours |
|-------|---------|-----------|
| C1: Vegetation | ~5 templates | 40-55h |
| C2: Horror Atmosphere | 7-layer templates | 50-65h |
| C3: Props/Vehicles | ~4 templates | 35-50h |
| C4: Roads | ~4 actions | 40-55h |
| C5: Integration | 2 actions + presets | 15-20h |
| **Total** | **~25-30 actions/templates** | **200-280h** |

---

## Sub-Plan D: PCG Gameplay Integration

**Estimated effort:** 130-180h
**Research docs:** `pcg-gameplay-elements-research`
**Deliverable:** PCG-driven placement of gameplay elements (items, enemies, objectives, safe rooms).
**Dependency:** Requires Sub-Plan A (Phase A1-A2), Sub-Plan C (horror atmosphere for decay integration), existing spatial registry.

### Phase D1: Item + Loot Placement (30-40h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| D1.1 | Room-type loot tables (DataAssets) | `Private/MonolithPCGGameplayActions.h/.cpp` + DataAssets | Create `UMonolithLootTable` DataAsset class with room_type, weighted entries (item, weight, max_per_room), total_items range, empty_probability. Ship default tables: bathroom (health items), kitchen (food, melee), office (notes, keys), bedroom (health, notes), storage (ammo, tools), medical (health kits), security (ammo, weapons). Action: `create_loot_table`, `edit_loot_table`. | DataAssets load correctly. Weighted selection produces expected distribution over 100 samples. | 10h |
| D1.2 | PCG loot scatter template | Same files | Template graph: Room polygon -> Interior Surface Sampler -> Density Filter (by loot_density param) -> Self Pruning (min 50cm distance) -> Spawn Actor (BP_LootSpawnPoint with loot_table reference). BP spawn points are dormant until activated by AI Director. | Spawn points appear inside room bounds, correctly spaced, with room-type loot table assigned. | 10h |
| D1.3 | Surface-aware item placement | Same files | Enhanced placement: items on surfaces (shelves, tables, desks) not just floors. Use existing `scatter_on_surface` logic to find valid surfaces within rooms, then PCG scatter on those surfaces. Combine PCG room selection with Monolith surface detection. | Items appear on table tops and shelves, not floating in air. | 10h |
| D1.4 | Hospice accessibility | Same files | Configurable floor_min_health parameter guarantees minimum health items per floor. Items must have clear visual indicators (subtle glow material). Never place critical items in hard-to-reach locations (filter out high-Z surfaces, surfaces behind physics objects). | With floor_min_health=3, every floor has at least 3 health items accessible from floor level. | 5h |

### Phase D2: Enemy Spawn + AI Director Integration (35-50h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| D2.1 | Spawn point generation | Same files | PCG template: identify valid enemy spawn locations via spatial analysis. Criteria: not in player sightline from room entry (use sightline analysis from existing MonolithMesh), minimum distance from safe rooms, prefer rooms with single entry (ambush potential), prefer dark areas. Output: scored spawn points with type tags (lurker, patrol, ambush, horde). | Spawn points favor dark corners, avoid safe rooms, prefer single-entry rooms. | 12h |
| D2.2 | Patrol path generation | Same files | Generate patrol paths as splines through connected rooms. Use navmesh (existing) to find valid paths. PCG graph: room center points -> pathfinding subgraph -> output patrol splines. Patrol types: loop (returns to start), linear (back and forth), roaming (visits all rooms in zone). | Patrol splines follow valid navmesh paths through room doors. | 10h |
| D2.3 | Difficulty scaling | Same files | PCG parameter `difficulty` (0.0-1.0) controls: spawn point density, spawn type distribution (more lurkers at low, more horde at high), patrol path complexity, resource scarcity. Hospice mode caps difficulty at 0.3 and guarantees generous resources. | Difficulty 0.2: few enemies, generous items. Difficulty 0.8: many enemies, scarce items. | 8h |
| D2.4 | AI Director hooks | Same files | Expose PCG generation as AI Director input. Director can: activate/deactivate spawn points based on player performance, modify loot table weights reactively, trigger PCG regeneration for specific rooms. Actions: `set_room_threat_level`, `activate_spawn_zone`, `deactivate_spawn_zone`. | AI Director call to `set_room_threat_level({room: "kitchen", level: 0.8})` increases spawn density in that room. | 10h |

### Phase D3: Objectives + Progression (30-40h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| D3.1 | Lock-and-key placement | Same files | Graph-based progression: divide building into zones separated by locked doors. Place keys in preceding zones (never in the locked zone). Algorithm: build zone graph from spatial registry, identify chokepoint doors, assign lock types (key, keycard, puzzle), place keys in rooms reachable before the lock. Validate all zones are reachable. | 3-zone building: key_A in zone 1 unlocks door to zone 2, key_B in zone 2 unlocks door to zone 3. All zones reachable. | 12h |
| D3.2 | Safe room placement | Same files | Identify candidate safe rooms: single entry, interior room (no windows), moderate size. Score using existing `evaluate_safe_room` action. Place safe room markers (save point, storage chest, guaranteed health). PCG graph places safe room furnishing. | Safe rooms are defensible, have save points and health, appear every 3-5 rooms of progression. | 8h |
| D3.3 | Jump scare placement | Same files | Use existing `suggest_scare_positions` analysis. PCG spawns trigger volumes at identified scare positions. Scare types: proximity trigger, door-open trigger, item-pickup trigger, line-of-sight trigger. Intensity validated via `validate_horror_intensity` for hospice mode. | Scare triggers at good positions (dark corners, after tight turns, near objectives). Hospice mode has reduced intensity. | 8h |
| D3.4 | `dress_gameplay` composite action | Same files | Orchestrator: given a dressed town block, place all gameplay elements. Order: safe rooms -> lock/key -> loot -> spawn points -> patrol paths -> scare triggers. Validates all progression paths. | Single call produces a playable level with items, enemies, objectives, and valid progression. | 6h |

### Phase D Summary

| Phase | Actions | Est Hours |
|-------|---------|-----------|
| D1: Items/Loot | ~4 actions | 30-40h |
| D2: Enemies/AI | ~4 actions | 35-50h |
| D3: Objectives | ~4 actions | 30-40h |
| **Total** | **~12-15 actions** | **130-180h** |

---

## Sub-Plan E: Terrain + Landscape

**Estimated effort:** 95-130h
**Research docs:** `pcg-terrain-research`
**Deliverable:** Landscape modification actions for building foundations, road cuts, and ground texturing.
**Dependency:** Independent of PCG module (uses landscape APIs directly). PCG reads terrain after modification.

**Critical constraint:** PCG CANNOT write to landscape heightmaps or weightmaps. Terrain modification uses `FLandscapeEditDataInterface` directly.

### Phase E1: Landscape Modification (40-55h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| E1.1 | Flatten terrain for buildings | `Private/MonolithPCGTerrainActions.h/.cpp` (or MonolithMesh extension) | Action: `flatten_terrain_for_building({building_id})`. Read building footprint polygon from building descriptor. Use `FLandscapeEditDataInterface::GetHeightData()` to sample current heights within footprint + margin. Compute target height (average of perimeter samples). `SetHeightData()` to flatten footprint to target height. Apply Gaussian blur at edges for smooth blending. | Building footprint area is flat. Edges blend smoothly into surrounding terrain (no cliff). | 14h |
| E1.2 | Cut terrain for roads | Same files | Action: `cut_terrain_for_road({road_spline})`. Sample road spline points. For each point, flatten terrain within road width + shoulder. Maintain grade along road length (interpolate heights along spline). Cut/fill logic: where terrain is above road grade, cut; where below, fill. | Road runs at consistent grade through hilly terrain. | 14h |
| E1.3 | Retaining walls | Same files | Where cut depth > threshold (150cm), spawn retaining wall meshes along cut edge. Use spline-sampled placement. Material matches local terrain type. | Tall cuts have retaining walls, shallow cuts blend naturally. | 8h |
| E1.4 | Foundation generation | Same files | For buildings on slopes: detect max height differential across footprint. If > threshold: generate terrain-adaptive foundation (piers, stepped, cut-fill). Use GeometryScript to create foundation mesh adapted to terrain profile. | Building on a slope has a visible foundation raising the low side to level. | 10h |

### Phase E2: Ground Texturing (25-35h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| E2.1 | PCG-driven layer painting | Same files | After terrain modification, paint landscape layers: road surface layer along road splines, building foundation layer under buildings, grass layer in yards, dirt layer in neglected areas. Use `FLandscapeEditDataInterface::SetAlphaData()`. Decay parameter drives layer selection (well-maintained grass vs dead grass vs bare dirt). | Road areas painted with asphalt layer, yards with grass, high-decay areas with dead grass. | 12h |
| E2.2 | Drainage and low-area detection | Same files | Detect local minima in terrain (potential puddle/drainage locations). Spawn water puddle decals or shallow water planes at low points. In road context, detect and mark drainage gutter positions along curbs. | Low spots have puddles, road edges have drainage visual. | 8h |
| E2.3 | Ground detail scatter | Same files | PCG template for ground-level detail driven by landscape layer: gravel on dirt, leaves on grass, trash on asphalt. Uses landscape layer weight attribute from PCG Surface Sampler. | Gravel appears on dirt paths, leaves on grass, litter on asphalt. | 8h |

### Phase E Summary

| Phase | Actions | Est Hours |
|-------|---------|-----------|
| E1: Landscape Modification | ~4 actions | 40-55h |
| E2: Ground Texturing | ~3 actions | 25-35h |
| **Total** | **~7 actions** | **95-130h** |

---

## Sub-Plan F: Runtime + Streaming

**Estimated effort:** 155-225h
**Research docs:** `pcg-runtime-streaming-research`
**Deliverable:** Runtime PCG generation with World Partition integration, HiGen multi-scale, and on-demand interior generation.
**Dependency:** Requires Sub-Plans A, B, C substantially complete. This is the optimization/scale-up phase.

### Phase F1: HiGen + World Partition (50-70h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| F1.1 | HiGen grid configuration | `Private/MonolithPCGRuntimeActions.h/.cpp` | Configure PCG graphs for hierarchical generation. Map generation layers to HiGen grid levels: Grid2048 = town layout, Grid128 = block layout, Grid32 = building placement, Grid8 = room detail, Grid4 = micro-detail (debris, cobwebs). Action: `configure_higen({graph, grid_mapping})`. | Graph executes at correct grid levels. Block-level content generates before building-level. | 12h |
| F1.2 | Runtime generation trigger setup | Same files | Configure `EPCGComponentGenerationTrigger::GenerateAtRuntime` on PCG components. Set generation radii per grid level. Configure scheduler CVars: `pcg.RuntimeGeneration.GridSizeDefault`, `pcg.RuntimeGenScheduler.FrameBudgetMs` (5ms default). Action: `configure_runtime_generation({radius_map, budget_ms})`. | Content generates as player approaches, cleans up when player leaves. Stays within 5ms frame budget. | 12h |
| F1.3 | World Partition integration | Same files | Ensure PCG-generated actors participate in World Partition streaming. PCG partition actors auto-register. Configure streaming distances per content type (buildings visible far, interior detail only when close). Handle the known `GenerateOnDemand` partition actor bug (use `GenerateAtRuntime` instead). | Town content streams in/out with World Partition. No pop-in at boundaries (hysteresis multiplier). | 12h |
| F1.4 | Performance monitoring | Same files | Action: `get_pcg_runtime_stats`. Report: active PCG components, generation queue depth, frame budget usage, instance counts per HISM. Use `UPCGSubsystem` metrics. | Stats show generation load, budget utilization, and instance counts in real-time. | 8h |

### Phase F2: Runtime Interior Generation (45-65h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| F2.1 | On-demand interior generation | Same files | Buildings render as exterior-only shells at distance. When player approaches building entrance (proximity trigger), generate interior: walls, floors, furniture, dressing. Use `GenerateOnDemand` triggered by overlap volume at doorway. Pre-compute building descriptors at editor time, defer interior PCG execution to runtime. | Approach building -> interior generates in <500ms. Enter building -> fully furnished. | 15h |
| F2.2 | Interior LOD system | Same files | Three interior LODs: L0 (full detail: furniture, debris, decals, lights), L1 (structure only: walls, floors, doors), L2 (invisible: interior cleaned up). Distance-driven transitions. Furniture HISM instances removed/added per LOD. | Inside building: full detail. Adjacent building (through window): walls only. Far buildings: no interior. | 12h |
| F2.3 | Deterministic generation | Same files | Same seed = same building interior. Store building seeds in building descriptor. PCG seed propagation: building seed -> per-room seeds -> per-scatter seeds. Verify: generate building, cleanup, regenerate = identical placement. Critical for multiplayer and save/load. | Generate twice with same seed, diff the spawned actor positions -- all match. | 10h |
| F2.4 | Memory budget system | Same files | Track total instance counts, mesh memory, generated actor count. Configurable budget caps. When approaching budget: reduce detail level, increase cleanup aggressiveness, defer lower-priority generation. Action: `set_memory_budget({max_instances, max_generated_actors, max_mesh_memory_mb})`. | Stay under budget (e.g., 50K instances, 500 generated actors, 2GB mesh memory) even with 20+ generated buildings. | 10h |

### Phase F3: Scale Testing + Optimization (30-45h)

| WP | Task | Files | Algorithm/Approach | Test Criteria | Est |
|----|------|-------|--------------------|---------------|-----|
| F3.1 | GPU compute integration | Same files | Enable FastGeo for scatter-heavy graphs. CVar: `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives=1`. Enable `PCG FastGeo Interop` plugin. Benchmark CPU vs GPU path for vegetation and debris scatter. | GPU scatter is measurably faster than CPU for 10K+ point operations. | 8h |
| F3.2 | Large-scale stress test | Test maps + profiling | Test with 100+ buildings, 5000+ HISM instances per block, 20+ blocks. Profile with Unreal Insights. Identify bottlenecks: generation time, draw calls, memory, streaming transitions. | 100-building town runs at 30+ FPS on target hardware with streaming. | 12h |
| F3.3 | Partition actor pooling | Same files | Enable PCG partition actor pooling to reduce spawn/destroy churn during streaming. Configure pool sizes per content type. | Streaming transitions don't cause frame hitches (pooled actors reused). | 8h |

### Phase F Summary

| Phase | Actions | Est Hours |
|-------|---------|-----------|
| F1: HiGen + World Partition | ~4 actions | 50-70h |
| F2: Runtime Interiors | ~4 actions | 45-65h |
| F3: Scale + Optimization | ~3 actions | 30-45h |
| **Total** | **~11 actions** | **155-225h** |

---

## Cross-Cutting Concerns

### Hospice Mode

Every sub-plan must respect hospice accessibility:
- **Items:** Generous minimums, clear visibility, accessible locations (D1.4)
- **Horror:** Intensity caps via `validate_horror_intensity` (C2.7, D3.3)
- **Difficulty:** Hospice mode caps at 0.3, generous resources (D2.3)
- **Controls:** All PCG parameters have hospice-safe defaults documented
- **Reports:** `generate_hospice_report` action validates entire generated level

### Outliner Organization

ALL spawned actors MUST use `SetFolderPath()`:
- `Monolith/PCG/Graphs/` -- PCG volumes and components
- `Monolith/PCG/Vegetation/` -- foliage scatter
- `Monolith/PCG/Props/` -- street furniture, vehicles
- `Monolith/PCG/Horror/` -- horror dressing
- `Monolith/Buildings/{building_id}/` -- building HISM
- `Monolith/Roads/` -- road infrastructure

### Testing Strategy

Each sub-plan phase requires:
1. **Unit tests:** Individual action input/output validation
2. **Integration tests:** Multi-action workflows (scan -> build -> dress -> gameplay)
3. **Visual verification:** Scene captures comparing generated results against reference
4. **Performance benchmarks:** Frame time, generation time, memory, draw calls

### Documentation Updates Required

After EACH phase completion:
- `Docs/SPEC.md` -- new actions documented
- `Docs/TODO.md` -- completed items marked
- `Docs/TESTING.md` -- test results recorded
- `CLAUDE.md` -- action count updated
- `.claude/skills/unreal-pcg.md` -- new skill file for PCG namespace (created at A1)
- Agent definitions updated if new agents needed

---

## File Conflict Analysis

### Within Sub-Plan A (MonolithPCG module)

No conflicts -- all new files in `Source/MonolithPCG/`.

### Between Sub-Plan A and B

**Decision: Kit Scanner lives in MonolithPCG, not MonolithMesh.**

Rationale: The kit scanner is deeply tied to PCG (scanned kits feed into PCG graph execution). While scanning uses Asset Registry and GeometryScript (MonolithMesh dependencies), the output drives PCG-based building assembly. Housing it in MonolithPCG keeps the feature self-contained.

MonolithPCG.Build.cs must depend on GeometryScript (for fallback generation) and GeometryCore (for FMeshBoundaryLoops). Add these as optional dependencies with `WITH_GEOMETRYSCRIPT` guard, same pattern as MonolithMesh.

### Between Sub-Plan B and existing MonolithMesh

B5.1 (`build_with_kit`) will use existing MonolithMesh infrastructure:
- `SaveMeshToAsset()` for fallback pieces
- `ConvertToHism()` for HISM assembly
- Spatial registry (MonolithMesh owns this)

Solution: MonolithPCG depends on MonolithMesh (private dependency). Kit scanner calls MonolithMesh functions internally.

### Between Sub-Plan C and existing MonolithMesh

Existing MonolithMesh has horror actions (decals, storytelling scenes, context props). PCG horror templates (C2) will call these internally for non-batch operations, but use PCG for batch placement. No file conflicts -- new files in MonolithPCG.

### Between Sub-Plans E (Terrain) and existing code

Terrain modification could live in MonolithMesh (where terrain actions already exist: `MonolithMeshTerrainActions.h/.cpp`) or MonolithPCG. **Decision: Keep in MonolithMesh** since it uses landscape APIs, not PCG. The existing terrain actions file gets extended.

Files affected: `MonolithMeshTerrainActions.h/.cpp` (extend with flatten/cut actions).

### Cross-Sub-Plan File Conflicts

| File | Sub-Plans Touching | Resolution |
|------|-------------------|------------|
| `Monolith.uplugin` | A (add module + PCG dep) | Single edit in A1.3 |
| `MonolithMeshTerrainActions.h/.cpp` | E (extend) | E extends existing, no conflict with A-D |
| `Saved/Monolith/ModularKits/*.json` | B (read/write), B5+A4 (read) | B owns write, others read-only |
| `Saved/Monolith/SpatialRegistry/*.json` | A4 (read), D (read/write) | Existing format, additive writes only |
| `Saved/Monolith/PCGPresets/*.json` | A3 (create), C5 (create) | Different preset categories, no conflict |

---

## Risk Register

| # | Risk | Impact | Probability | Mitigation |
|---|------|--------|-------------|------------|
| R1 | PCG API changes in UE 5.8 | High | Medium | Minimize custom PCG nodes (only 2-3). Graph construction uses stable public API. WITH_PCG guard allows graceful degradation. |
| R2 | Opening detection false positives on cooked meshes | Medium | Low | Editor-time only (research confirms source mesh preserves topology). Document as editor-only feature. |
| R3 | ISM instance count degrades above 10K | Medium | Low | Our use case stays under 10K per component (50-200 per building). Monitor in F3.2 stress test. |
| R4 | PCGEx dependency for roads | Medium | Medium | PCGEx is MIT licensed but external. Make it optional -- roads work without it (manual spline placement), PCGEx enables auto-generation. |
| R5 | Fallback piece UVs don't match kit art style | Low | High | Expected -- fallbacks are functional, not artistic. Three quality tiers mitigate (Tier 3 attempts style matching). Users can replace fallbacks with real art. |
| R6 | Kit scanner confidence too low on unusual kits | Medium | Medium | 5-signal fusion + user correction loop. Corrections persist. Confidence thresholds adjustable per kit. |
| R7 | PCG GenerateOnDemand partition actor bug (UE 5.7) | High | Confirmed | Use `GenerateAtRuntime` instead of `GenerateOnDemand` for partitioned components. Documented in research. |
| R8 | GeometryScript not available (optional dependency) | Medium | Low | WITH_GEOMETRYSCRIPT guard. If absent: fallback generation disabled, opening detection disabled, basic scan-only mode still works. |
| R9 | Memory pressure from large-scale generation | High | Medium | Memory budget system in F2.4. Interior LOD in F2.2. Cleanup aggressiveness scales with pressure. |
| R10 | Landscape edit layer API changes (5.7 deprecations) | Medium | Low | Non-edit-layer landscapes deprecated in 5.7. Our implementation targets edit layers from the start. |

---

## Agent Assignment Recommendations

| Sub-Plan | Phase | Recommended Agent | Rationale |
|----------|-------|-------------------|-----------|
| A1-A4 | Module + all PCG actions | `unreal-editor-tools` (Opus) | New module creation, Build.cs, plugin integration, C++ PCG API |
| B1-B3 | Kit scanning + classification | `unreal-mesh-expert` (Opus) | Mesh inspection, Asset Registry, boundary loops, GeometryScript |
| B4 | Fallback generation | `unreal-mesh-expert` (Opus) | GeometryScript mesh generation (existing proc gen expertise) |
| B5 | Build with kit | `unreal-mesh-expert` (Opus) | HISM assembly, building descriptor integration |
| C1-C2 | Vegetation + horror | `unreal-niagara-expert` (Opus) or `unreal-mesh-expert` | PCG template construction, environmental scatter |
| C3 | Props/vehicles | `unreal-mesh-expert` (Opus) | Prop placement, spatial awareness |
| C4 | Roads | `unreal-mesh-expert` (Opus) | Spline-based, GeometryScript roads |
| D1-D3 | Gameplay | `unreal-ai-expert` (Opus) | AI Director, spawn logic, progression design |
| E1-E2 | Terrain | `unreal-mesh-expert` (Opus) | Landscape API, terrain modification |
| F1-F3 | Runtime | `cpp-performance-expert` (Opus) | Performance, streaming, memory budgets |

**Parallel execution in Milestone 1:**
- Sub-Plan A (Phase A1) runs first (module bootstrap)
- Sub-Plan B (Phase B1-B3) can start immediately (no PCG dependency for scanning)
- Sub-Plan A (Phase A2-A4) runs in parallel with B after A1 completes
- Sub-Plan B (Phase B4-B5) runs after B1-B3, needs A1 for PCG-based placement

---

## Appendix: Complete Action List (~110-135 new actions)

### pcg_query namespace (~37 actions from Sub-Plan A)

| # | Action | Phase |
|---|--------|-------|
| 1 | `create_pcg_graph` | A1 |
| 2 | `list_pcg_graphs` | A1 |
| 3 | `get_pcg_graph_info` | A1 |
| 4 | `add_pcg_node` | A1 |
| 5 | `remove_pcg_node` | A1 |
| 6 | `connect_pcg_nodes` | A1 |
| 7 | `disconnect_pcg_nodes` | A1 |
| 8 | `set_pcg_node_params` | A1 |
| 9 | `list_pcg_node_types` | A1 |
| 10 | `add_pcg_parameter` | A2 |
| 11 | `set_pcg_parameter` | A2 |
| 12 | `get_pcg_parameters` | A2 |
| 13 | `execute_pcg` | A2 |
| 14 | `cleanup_pcg` | A2 |
| 15 | `get_pcg_output` | A2 |
| 16 | `cancel_pcg_execution` | A2 |
| 17 | `spawn_pcg_volume` | A2 |
| 18 | `spawn_pcg_component` | A2 |
| 19 | `set_pcg_component_graph` | A2 |
| 20 | `list_pcg_components` | A2 |
| 21 | `refresh_pcg_component` | A2 |
| 22 | `get_pcg_generated_actors` | A2 |
| 23 | `set_pcg_generation_trigger` | A2 |
| 24 | `create_scatter_graph` | A3 |
| 25 | `create_spline_scatter_graph` | A3 |
| 26 | `create_vegetation_graph` | A3 |
| 27 | `create_horror_dressing_graph` | A3 |
| 28 | `create_debris_graph` | A3 |
| 29 | `apply_pcg_preset` | A3 |
| 30 | `save_pcg_preset` | A3 |
| 31 | `list_pcg_presets` | A3 |
| 32 | `spatial_registry_to_pcg` | A4 |
| 33 | `pcg_output_to_registry` | A4 |
| 34 | `building_descriptor_to_pcg` | A4 |
| 35 | `create_room_scatter_graph` | A4 |
| 36 | `batch_execute_pcg` | A4 |

### pcg_query namespace (Kit Scanner actions from Sub-Plan B, ~15-20)

| # | Action | Phase |
|---|--------|-------|
| 37 | `scan_modular_kit` | B1 |
| 38 | `classify_mesh` | B2 |
| 39 | `detect_grid_size` | B2 |
| 40 | `detect_mesh_openings` | B2 |
| 41 | `edit_kit_classification` | B3 |
| 42 | `accept_uncertain` | B3 |
| 43 | `reset_kit_classification` | B3 |
| 44 | `list_kits` | B3 |
| 45 | `get_kit_info` | B3 |
| 46 | `delete_kit` | B3 |
| 47 | `export_kit` | B3 |
| 48 | `import_kit` | B3 |
| 49 | `regenerate_fallbacks` | B4 |
| 50 | `build_with_kit` | B5 |
| 51 | `swap_proxies` | B5 |
| 52 | `build_block_with_kit` | B5 |

### Additional actions from Sub-Plans C-F (~50-70)

Town dressing templates, road actions, gameplay actions, terrain actions, runtime configuration -- see individual sub-plan phases for details.

**Grand total: ~110-135 new actions, taking Monolith from 684 to ~800-820 actions.**

---

## Research Document Index

| Doc | Key Finding | Used In |
|-----|-------------|---------|
| `pcg-framework-research` | PCG data type hierarchy, 100+ node types, FPCGPoint structure | A1, A2 |
| `pcg-mcp-integration-research` | Graph construction API (AddNodeOfType, AddEdge), 35-action design, Build.cs pattern | A1-A4 |
| `pcg-building-examples-research` | 76 examples, Cassini Shape Grammar, community tutorial patterns | A3, C4 |
| `hybrid-gs-pcg-research` | Three-layer architecture, Polygon2D for rooms, PCG vs GS decision matrix | A4, architecture |
| `pcg-vegetation-research` | Surface Sampler + slope filter + exclusion zones, decay-driven overgrowth, 60-85h | C1 |
| `pcg-roads-research` | PCGEx for graph-based roads, spline mesh deformation, intersection handling | C4 |
| `pcg-horror-atmosphere-research` | 7-layer decay system, existing Monolith overlap analysis, BP actor templates for lights | C2 |
| `pcg-gameplay-elements-research` | L4D AI Director pattern, room-type loot tables, hospice accessibility | D1-D3 |
| `pcg-terrain-research` | PCG is read-only for landscape, FLandscapeEditDataInterface for writes | E1-E2 |
| `pcg-lighting-audio-research` | BP actor templates for lights/audio, street light decay ratios, MegaLights | C2.5, C2.6 |
| `pcg-vehicles-props-research` | 28+ prop types, placement context hierarchy, per-lot vs street placement | C3 |
| `pcg-runtime-streaming-research` | HiGen grid levels, 5ms frame budget, partition actor pooling, GenerateOnDemand bug | F1-F3 |
| `asset-scanning-research` | 5-signal classification pipeline, IAssetRegistry API, UMonolithKitMetadata | B1 |
| `mesh-opening-detection-research` | FMeshBoundaryLoops, boundary loop -> opening classification, 95% accuracy | B2 |
| `marketplace-kit-conventions-research` | Naming conventions (SM_ prefix 90%), grid sizes (100-512cm), pivot conventions | B1, B2 |
| `kit-scanner-ux-research` | 3-turn golden path, confidence tiers, progressive disclosure, correction UX | B1, B3 |
| `grid-detection-research` | Candidate scoring + histogram validation + industry prior bias, >95% accuracy | B2 |
| `fallback-piece-generation-research` | 14 piece types, 3 quality tiers, dimension matching, Selection+Inset for openings | B4 |
| `modular-building-research` | AAA modular approach (Bethesda, Arkane, Embark), HISM instancing, wall-type mapping | B5 |
| `modular-pieces-research` | GeometryScript generation pipeline, AppendBox + Selection+Inset, existing SaveMeshToAsset | B4 |
| `hism-assembly-research` | HISM C++ API, AddInstances batch, PreAllocateInstancesMemory, spatial queries, ID bug on HISM | B5 |

---

## Review #1

**Reviewer:** unreal-code-reviewer (independent)
**Date:** 2026-03-29
**Verdict:** Strong plan with excellent research backing. Approved with conditions below.

### 1. Sub-Plan Dependencies -- Correct Ordering?

The dependency graph is mostly correct. A few observations:

**Correct:**
- B1-B3 (scanning/classification) are genuinely independent of A (no PCG needed for asset scanning).
- A1 must complete before any PCG graph construction.
- E (terrain) is correctly marked as independent of PCG.
- F requiring A, B, C is correct -- you cannot optimize what does not exist.

**Issue (Important):** Sub-Plan B5.1 (`build_with_kit`) is listed as needing "A1 for PCG-based placement" but the architecture description says it uses HISM directly, not PCG. Looking at the build_with_kit algorithm: "place via HISM (one HISM component per unique mesh)." This is direct HISM spawning, not PCG graph execution. The PCG dependency only applies if you route placement through PCG's StaticMeshSpawner node. The plan should clarify: does `build_with_kit` use PCG or direct HISM? If direct HISM (which the description implies), B5 has zero dependency on A and the entire Kit Scanner (Sub-Plan B) can ship independently as Milestone 1 without waiting on any PCG foundation work. This is good news for scheduling.

**Issue (Suggestion):** Sub-Plan D's dependency on C ("requires C") is overly strict. D needs the horror atmosphere *system* to exist but not the full town dressing. D could start after C2 (horror atmosphere layer) without waiting for C3-C5 (props, roads, integration). Loosening this allows more parallelism.

**Can parallel WPs truly run in parallel?** Within each phase, yes. The WPs within B1 (B1.1-B1.5) all write to the same file pair (`MonolithPCGKitScannerActions.h/.cpp`), so they must be serialized to a single agent -- but the plan already assigns them to one agent (unreal-mesh-expert), so no issue. The A1 WPs similarly share action files but are assigned to one agent.

### 2. File Conflicts Between WPs

The file conflict analysis in the plan is thorough and correct. One gap:

**Issue (Important):** WP A4.1 specifies `MonolithPCGBridgeActions.h/.cpp` and WP A4.2-A4.3 specify separate PCG node files. But A4.1's `spatial_registry_to_pcg` reads the spatial registry JSON that is owned by MonolithMesh. If MonolithMesh's spatial registry format changes (it is under active development given the 192-action mesh module), the bridge code breaks silently. The plan should specify a versioned schema for the spatial registry JSON, or better yet, have MonolithMesh expose a C++ API (`FMonolithSpatialRegistry::GetRooms()`) rather than reading JSON files directly.

**Verified clean:** The new `Source/MonolithPCG/` directory contains no overlap with any existing module. The `Monolith.uplugin` edit is a single additive change. The `MonolithMeshTerrainActions.h/.cpp` extension in Sub-Plan E is correctly identified as touching existing files.

### 3. PCG API Usage -- Correctly Understood?

The PCG C++ API usage is well-researched and accurate for UE 5.7. Specific verification:

**Correct:**
- `UPCGGraph::AddNodeOfType()`, `AddEdge()`, `SetGraphParameter<T>()` -- confirmed in framework research, these are the right entry points.
- `UPCGComponent::GenerateLocal(bForce)` -- correct execution method.
- `FInstancedPropertyBag` for user parameters -- correct (StructUtils module dependency needed, which is listed).
- `EPCGComponentGenerationTrigger` enum values -- correct for UE 5.7.
- The `GenerateOnDemand` partition actor bug (R7) -- correctly flagged and mitigated.

**Issue (Important):** WP A1.4 proposes using `FJsonObjectConverter::JsonObjectToUStruct` to set node parameters. This works for simple UPROPERTY fields but PCG node settings frequently use `PCG_Overridable` meta and `FPCGAttributePropertySelector` types that are not simple structs. The JSON-to-UStruct approach will silently fail on these. A more robust approach: iterate the `UPCGSettings` class's UPROPERTY fields via UE reflection (`TFieldIterator<FProperty>`), match by name from the JSON keys, and set values using `FProperty::ImportText()` or property-specific setters. This is more work (~2-3h extra on A1.4) but covers edge cases.

**Issue (Suggestion):** The plan mentions `GetGeneratedGraphOutput()` in A2.1 for reading output. This method returns `FPCGDataCollection` which contains tagged data. The plan should specify how this opaque data gets serialized back to JSON for the MCP response -- probably by iterating `TaggedData`, casting to `UPCGPointData`, and extracting point count + spawned component info. Not technically wrong, just under-specified.

### 4. Kit Scanner Classification -- 5-Signal Pipeline Technically Sound?

This is the strongest part of the plan. The research is thorough.

**Technically sound:**
- Signal 1 (name parsing): Regex patterns match marketplace conventions well. The 80% accuracy claim for well-named assets is conservative -- likely higher.
- Signal 2 (dimension analysis): Sort-then-classify approach is rotation-agnostic as noted. Good.
- Signal 3 (material slots): Correctly identified as a confidence booster, not a primary signal. Weight of 0.15 is appropriate.
- Signal 4 (topology/FMeshBoundaryLoops): The research correctly identifies this as editor-time only due to cooked mesh UV seam splitting. The filter phase (reject loops on AABB faces, perimeter threshold) is the right approach.
- Signal 5 (sockets): Low weight (0.05) is correct since most kits do not use sockets.

**Issue (Important):** The fusion weights (0.35 + 0.30 + 0.15 + 0.15 + 0.05 = 1.0) assume all signals are available. When topology analysis is skipped (e.g., user opts for fast scan), the weights need renormalization. The plan should specify fallback weight sets for partial-signal scenarios: fast mode (name 0.50, dimension 0.35, material 0.15) vs full mode (all 5 signals).

**Issue (Suggestion):** The plan does not mention handling kits where pieces share a common prefix that is NOT the kit name (e.g., "SM_Env_Dungeon_Wall_01" where "Env" is a prefix and "Dungeon" is the kit). The regex handles the "_Env_" prefix case, but a pre-pass to detect common prefixes across the scanned folder and strip them would improve classification on eccentric naming schemes.

### 5. HISM Strategy -- Collision Disabled, Separate Box Collisions

**Correct and well-justified.** The HISM research is excellent:
- `bDisableCollision = true` on HISM components is the right call. The Chaos physics registration cost per instance is the #1 performance trap and the plan correctly identifies it.
- Separate `UBoxComponent` collisions per room is the AAA approach.
- `AddInstances()` (plural, batch) instead of looping `AddInstance()` -- correctly identified as critical.
- `PreAllocateInstancesMemory()` before batch add -- good.
- Per-building actor with multiple HISM components is the right Phase 1 approach for <100 buildings.

**Issue (Suggestion):** The plan mentions `bMarkRenderStateDirty = false` for all-but-last calls, which is correct. But the sample code in B5.1 uses `SetCustomData(Indices[i], Data, i == Indices.Num() - 1)` -- this marks dirty on the last iteration of *each HISM component's loop*, not the last component overall. Since each component's render state is independent, this is actually fine. Just confirming there is no issue here.

**Issue (Important):** The box collision per room strategy works for blocking but does not support line traces against individual HISM instances (e.g., for damage decals, interaction highlighting). The plan mentions `FHitResult::Item` returning instance index for line traces against HISM, but this only works with collision enabled. If any gameplay system needs to identify which specific wall piece was hit (for destruction, inspection, etc.), you will need a secondary trace approach -- either re-enable collision on a subset of HISM components or use a custom spatial query against stored instance transforms. This should be documented as a known limitation or addressed in Sub-Plan D.

### 6. Time Estimates -- Realistic?

**Sub-Plan A (80-110h):** Reasonable. Graph CRUD (A1.4 at 14h) is appropriately the largest WP. The PCG API is well-documented but wiring up JSON parameter setting, node resolution, and edge connection with proper error handling justifies the estimate.

**Sub-Plan B (120-160h):** The opening detection WP (B2.2 at 12h) is aggressive for the complexity involved -- FMeshBoundaryLoops computation, AABB face rejection, ray-cast validation, and classification heuristics across diverse marketplace kits. I would budget 16-20h. The fallback generation (B4, 25-35h) is realistic; the Selection+Inset technique for door/window openings in generated meshes is well-understood from the research but fiddly to get right.

**Sub-Plan C (200-280h):** This is the riskiest estimate. The 7-layer horror system (C2, 50-65h) is a content-heavy effort that depends heavily on having appropriate meshes/BPs available. If the horror props, decals, and audio assets do not already exist, this balloons. The plan assumes they exist ("use existing `place_storytelling_scene` patterns") which is reasonable for this project.

**Sub-Plan D (130-180h):** The lock-and-key placement (D3.1, 12h) is underestimated. Graph-based zone partitioning with guaranteed reachability is a non-trivial algorithmic problem. Budget 16-20h.

**Sub-Plan E (95-130h):** Terrain flattening and road cutting are well-scoped. `FLandscapeEditDataInterface` is finicky but the research correctly identifies it as the right API.

**Sub-Plan F (155-225h):** Runtime generation is inherently hard to debug. The "deterministic generation" WP (F2.3 at 10h) is underestimated -- seed propagation bugs are subtle and take significant testing time. Budget 15-20h.

**Overall (800-1080h):** With the above adjustments, a realistic range is **850-1150h**. The plan's range is slightly optimistic on the low end.

### 7. Missing Risks

**Risk R11 (Medium/High): Asset loading during scan causes editor stall.** Scanning 100+ meshes requires loading each for bounds/topology analysis. `UStaticMesh::GetBounds()` requires the mesh to be loaded. For large kits, this could cause a multi-second editor freeze. Mitigation: use `FStreamableManager::RequestAsyncLoad` for batch loading, or process meshes across multiple frames using a tickable object or latent action.

**Risk R12 (Medium/Medium): PCG Grammar node stability.** The plan references PCG Grammar for future use (in the research). The Grammar system in 5.7 is marked production-ready but has far fewer community examples than the core scatter nodes. If Sub-Plan C4 or later phases lean on Grammar, expect undocumented edge cases.

**Risk R13 (Low/High): Naming collision in PCG settings resolver.** The friendly-name stripping logic ("UPCG" prefix + "Settings" suffix) will produce collisions if Epic adds nodes with overlapping names. For example, `UPCGMeshSamplerSettings` and `UPCGPointMeshSamplerSettings` would both strip to names containing "MeshSampler." The resolver should use the full stripped name (e.g., "MeshSampler" vs "PointMeshSampler") and handle collisions by falling back to the full class name.

**Risk R14 (Medium/Medium): MonolithPCG module adds significant compile time.** The PCG module headers are large. Depending on `PCG`, `GeometryScriptingCore`, `GeometryCore`, `MonolithMesh`, `MonolithCore`, `MonolithIndex`, `StructUtils`, `Json`, `JsonUtilities`, `AssetRegistry`, and `AssetTools` creates a wide dependency surface. Use private dependencies and forward declarations aggressively. Consider splitting the kit scanner into a separate MonolithKitScanner module if compile times become painful.

### 8. Action Count -- Reasonable Scope Per Sub-Plan?

**Sub-Plan A (37 actions):** This is a lot for a foundation module. However, many of these are thin wrappers (e.g., `list_pcg_graphs` is a directory scan, `get_pcg_parameters` is a property read). The 8 template actions in A3 are the meatiest. Reasonable.

**Sub-Plan B (15-20 actions):** Well-scoped. The `scan_modular_kit` action is complex internally but represents a single user-facing action with a rich return payload. The split between scanning, management, fallback, and building actions is clean.

**Sub-Plan C (25-30 actions/templates):** Heavy on content, but each "action" here is really a template builder (a function that creates a PCG graph). The 7-layer horror system is ambitious but each layer is a focused PCG graph template.

**Sub-Plan D (12-15 actions):** Lean and focused. Good.

**Grand total (~110-135):** Bringing Monolith from 635 to ~750-770 actions. The plan says "684 to ~800-820" which does not match CLAUDE.md's stated 635 actions. This discrepancy should be reconciled -- either the current count in CLAUDE.md is stale or the plan's baseline is wrong.

### 9. Module Structure -- Build.cs Dependencies Correct?

**Proposed MonolithPCG.Build.cs dependencies (from A1.1):**
- `PCG` -- correct, required for all PCG classes
- `MonolithCore` -- correct, needed for `FMonolithToolRegistry`
- `MonolithIndex` -- correct if using project index for asset lookups
- `StructUtils` -- correct, needed for `FInstancedPropertyBag` (user parameters)
- `Json`, `JsonUtilities` -- correct, for JSON parameter handling
- `AssetRegistry`, `AssetTools` -- correct, for kit scanning and graph asset creation
- `UnrealEd` -- correct (editor module)

**Missing from the plan's Build.cs list:**
- `GeometryScriptingCore`, `GeometryFramework`, `GeometryCore` -- needed for B2.2 (opening detection via FMeshBoundaryLoops) and B4 (fallback generation). The plan mentions these in the file conflict analysis section but does not include them in A1.1's Build.cs specification. They must be optional (WITH_GEOMETRYSCRIPT guard), following the MonolithMesh pattern.
- `MonolithMesh` -- mentioned in the file conflict analysis as a private dependency for B5 (`SaveMeshToAsset`, `ConvertToHism`, spatial registry). Must be in Build.cs.
- `MeshDescription`, `StaticMeshDescription` -- may be needed for mesh inspection during scanning, depending on whether the kit scanner calls MonolithMesh functions directly or re-implements.
- `PCGEditor` -- mentioned as "optional if editor build" but since MonolithPCG is Type: Editor, this will always be an editor build. Make it a private dependency unconditionally, or skip it if you only need runtime PCG classes.

**Verified correct pattern:** The `WITH_GEOMETRYSCRIPT` conditional compilation guard and directory-existence check in MonolithMesh.Build.cs is the established pattern. MonolithPCG should replicate this exactly, plus add a similar `WITH_PCG` guard checking for the PCG plugin directory existence.

### 10. The Killer Feature (Sub-Plan B) -- Properly Prioritized and Scoped?

**Prioritization: Excellent.** The plan correctly identifies Sub-Plan B as the crown jewel and prioritizes it for Milestone 1. The "golden path" 3-turn UX is compelling and well-designed. Starting B1-B3 in parallel with A1 is the right call.

**Scope concerns:**

**Issue (Critical):** The plan conflates two very different use cases under "build_with_kit":
1. **Editor-time blockout**: Place HISM instances from a scanned kit to visualize a building layout. This is the MVP and what the golden path demonstrates.
2. **Runtime building generation**: Generate buildings at runtime using PCG + HISM with streaming. This is Sub-Plan F territory.

The plan should explicitly scope B5 as editor-time only. The runtime path comes later in F. This is implied but not stated, and without it, B5's time estimate is way too low for what someone might interpret as runtime-capable.

**Issue (Important):** The `swap_proxies` action (B5.3) is conceptually powerful but under-specified. "For each actor: get bounds -> find closest kit piece by dimensions and type tag -> replace actor with HISM instance." This assumes the whitebox actors have type tags or that pure dimension matching is sufficient. In practice, a 200x15x300 whitebox wall and a 200x15x300 floor placed vertically have identical dimensions. The swap action needs an orientation signal (wall normal, up vector) or explicit type tagging on whitebox actors.

**Issue (Suggestion):** Phase B4 (fallback generation) generates 14 piece types. The plan lists P0 (5 types), P1 (5 types), and P2 (4 types). This is well-structured, but the Tier 3 "Style-Matched" quality level (B4.5) that attempts bevel matching via dihedral angle analysis is risky. Dihedral angle analysis on arbitrary marketplace meshes is unreliable -- many kits use normal maps for edge detail rather than actual geometry bevels. I would mark Tier 3 as stretch goal / nice-to-have and not block the phase on it.

### Summary of Issues by Severity

**Critical (must fix before implementation):**
1. Clarify whether `build_with_kit` (B5) uses direct HISM or PCG. If direct HISM, remove the A1 dependency from B5 -- this unlocks shipping Kit Scanner independently.
2. Scope B5 explicitly as editor-time only.

**Important (should fix):**
3. A1.4: Use `FProperty::ImportText()` reflection approach instead of `FJsonObjectConverter::JsonObjectToUStruct` for PCG node parameter setting.
4. A4.1: Expose spatial registry via C++ API, not JSON file reads, to avoid silent breakage.
5. B1.5: Specify fallback weight renormalization for partial-signal classification (fast scan mode).
6. B2.2: Budget 16-20h, not 12h, for opening detection.
7. B5.3: `swap_proxies` needs orientation signal beyond pure dimension matching.
8. HISM collision limitation: Document that individual instance identification via line traces is not supported with `bDisableCollision = true`, and propose a workaround for gameplay systems that need it.
9. A1.1 Build.cs: Add `GeometryScriptingCore`, `GeometryFramework`, `GeometryCore` (optional), `MonolithMesh` (private), and verify `PCGEditor` need.
10. Reconcile action count baseline (plan says 684, CLAUDE.md says 635).

**Suggestions (nice to have):**
11. Loosen Sub-Plan D dependency on C (only needs C2, not all of C).
12. Add R11 (editor stall during bulk mesh loading) to risk register with async loading mitigation.
13. Add R13 (PCG settings name collision) to risk register.
14. Consider MonolithKitScanner as separate module if compile times suffer from wide dependency surface.
15. Mark B4.5 Tier 3 style-matching as stretch goal.
16. Add common-prefix detection pre-pass to name parser for eccentric naming schemes.

### What Was Done Well

- The research backing is exceptional. 18 research documents covering every major technical question. The HISM research in particular is production-quality with verified engine source citations.
- The three-layer hybrid architecture (GeometryScript + Floor Plan + PCG) is the right call. Trying to force everything through PCG would have been a mistake, and the research correctly identifies PCG's limitations (no geometry creation, no room subdivision, no vertical connectivity).
- The 5-signal classification pipeline is cleverly designed. The weighted fusion with confidence tiers and user correction loop is pragmatic engineering.
- The file conflict analysis is thorough and the decisions (kit scanner in MonolithPCG, terrain in MonolithMesh) are well-justified.
- The hospice accessibility cross-cutting concern is consistently applied across all sub-plans.
- The risk register catches the key UE 5.7-specific issues (GenerateOnDemand bug, Chaos physics collision cost).
- The phased delivery with independent milestones is well-structured for a multi-month effort.

---

## Review #2: User Experience, Marketplace Compatibility, Horror Design, and Hospice Accessibility

**Reviewer:** Code Review Agent (Opus 4.6)
**Date:** 2026-03-29
**Perspective:** End-user experience, marketplace kit compatibility, horror design quality, hospice accessibility, and practical deployment concerns.

---

### Overall Assessment

This is one of the most thoroughly researched feature plans I have encountered in this codebase. The 18+ research documents backing 6 sub-plans demonstrate genuine depth, and the plan correctly identifies the kit scanner as the killer feature. The three-layer hybrid architecture (GeometryScript / Floor Plan / PCG) is sound and avoids the common trap of forcing PCG to do everything. The progressive disclosure UX design for the scanner is well above the standard for tooling in this space.

That said, the plan has real gaps that would hurt actual users. What follows is organized by the review criteria requested.

---

### 1. Kit Scanner UX -- Is the 3-Turn Golden Path Achievable?

**Verdict: Achievable for well-structured kits. Fragile for messy real-world kits.**

The golden path (scan -> correct -> build) is well-designed and the progressive disclosure pattern is the right call. The confidence-based three-tier routing (auto-accept / flag / unknown) directly addresses the most common friction point in classification systems.

**Critical Issue: Scan time for large kits.**
WP B1.1 says "return results in <5s for 30+ meshes." However, Phase B2 adds opening detection via `FMeshBoundaryLoops`, which requires loading each mesh into `FDynamicMesh3`. For a 100-piece kit, that could easily be 30-60 seconds. The plan does not specify whether the full pipeline (name + dimensions + materials + topology + sockets) runs synchronously or whether it progressively returns results.

- **Recommendation (Important):** Add a two-pass scan strategy. Pass 1: name + dimensions + material slots (fast, <5s for 100 meshes, no mesh loading). Return initial results immediately. Pass 2: topology + opening detection (slow, async, updates classifications as results arrive). This matches the progressive disclosure philosophy and prevents the user from staring at a loading state.

**Important Issue: The regex will miss more kits than expected.**
The primary regex pattern assumes `SM_` prefix naming (90% of kits per the marketplace research). But the remaining 10% includes KitBash3D (`KB3D_*`), Megascans (hash IDs like `ukjsehbdw`), and custom imports with no prefix at all. The 5-signal fusion should handle this via dimension analysis as the fallback, but the name parser confidence weight of 0.35 means a kit with opaque naming starts at a significant disadvantage.

- **Recommendation (Suggestion):** Add a "naming strategy detection" pre-pass before classification. If fewer than 30% of meshes match the `SM_` regex, reduce name_score weight to 0.15 and redistribute to dimension_score (0.45). This adaptive weighting would handle Megascans and import-heavy kits much better.

**Important Issue: No preview or thumbnail in classification output.**
The JSON output includes piece dimensions and material slots but no visual preview. When a user sees "SM_Panel_Large -> wall_solid? (confidence: 0.62)" they need to see what the mesh looks like to make a correction decision. The L2 detail level mentions thumbnails but there is no mechanism to generate or serve them.

- **Recommendation (Important):** Add thumbnail generation to the scan pipeline. UE has `FObjectThumbnail` / asset thumbnail rendering. Even a low-res 128x128 thumbnail saved alongside the kit JSON would dramatically improve the correction UX. This could be a Phase B3 addition without blocking the scanner itself.

---

### 2. Marketplace Kit Compatibility

**Verdict: Strong coverage for the top 80% of kits. Specific blind spots for the remaining 20%.**

The marketplace conventions research is excellent -- it covers Synty, KitBash3D, Quixel, Dekogon, PurePolygons, BigMediumSmall, and Junction City. The naming regex patterns and grid detection algorithms are well-calibrated to these vendors.

**Critical Issue: KitBash3D kits are NOT modular building kits.**
The research correctly notes that KitBash3D uses "Packed Level Actors" -- pre-assembled buildings, not modular tiles. But the scanner architecture (classify pieces -> detect grid -> build buildings) fundamentally assumes tile-based modularity. The plan says "Scanner should detect as 'assembled' not 'tile-kit'" but there is no action or workflow for what happens AFTER that detection. A user who scans a KitBash3D kit will get "0% coverage" and no path forward.

- **Recommendation (Critical):** Add a kit type classification as the first step of `scan_modular_kit`. Detect whether the kit is: (a) tile-based modular, (b) pre-assembled buildings, (c) prop/furniture collection, (d) facade/detail kit. For type (b), the workflow should be: identify the pre-assembled buildings, register them as whole building templates, allow direct placement via HISM without piece-by-piece assembly. This is a different code path but an important one for KitBash3D and similar kits.

**Important Issue: Quixel/Megascans architectural kits are zone-based, not piece-type-based.**
Megascans organizes by "Foundation Kit", "Base Wall Kit", "3rd Floor Kit" etc. The scanner's piece-type classification (wall_solid, floor_tile) does not map cleanly to this zone-based organization. A "Foundation Kit" piece could be classified as wall_solid or floor_tile depending on its orientation.

- **Recommendation (Important):** Add zone-based classification as an alternative to piece-type classification. When folder structure matches Pattern B (zone folders like Foundation/, Base/, etc.), use the folder path as a primary classification signal. This would be a lightweight addition to the B1.2 name parser.

**Suggestion: Material instance detection for Synty atlas kits.**
Synty uses a single-material color atlas approach. The material slot classifier (B1.4) looks for keywords like "glass" and "frame" -- but Synty pieces typically have 1 material slot with a generic name like "M_Palette_01". The scanner should recognize single-material kits as a distinct pattern and not penalize them for lacking material signal.

---

### 3. Horror Atmosphere -- Does the 7-Layer Decay System Maintain Tension?

**Verdict: The system is technically sophisticated and well-designed. Two design risks need attention.**

The 7-layer architecture with independent enable/disable per layer is the right approach. The master decay parameter with per-room modulation via room type and Perlin noise is exactly how AAA games handle this. The tension curve integration (Section 15 of the horror research) connects pacing to atmosphere density correctly.

**Important Issue: Tension plateau risk.**
When all 7 layers activate simultaneously at high decay (0.7+), there is a risk of sensory overload that flattens the tension curve. If every room has dense debris, blood, flickering lights, dripping sounds, fog, cobwebs, AND a storytelling vignette, the player acclimates and stops feeling scared. The plan mentions a probability gate for storytelling scenes (not every room gets one), but the other 6 layers all activate based on decay threshold alone.

- **Recommendation (Important):** Add a per-room "active layer budget" that caps the number of simultaneously active layers. For example, at decay 0.7, allow a maximum of 4-5 active layers per room (selected per room via weighted random). This creates variation -- one room is dark and foggy but clean, the next is well-lit but covered in blood. Contrast drives tension far more effectively than uniform density.

**Suggestion: Missing "false safety" pattern.**
The horror research covers decay progression but does not address one of the most effective horror techniques: the clean room in a decayed building. A single pristine room amid high-decay surroundings is deeply unsettling (someone has been maintaining it -- why?). The system should support decay values that are intentionally LOWER than the building baseline for specific rooms, not just higher.

- The room_decay_offset mechanism supports this (negative offset), but no preset or template leverages it. Adding a "maintained_room" storytelling pattern that sets decay to 0.0-0.1 in an otherwise high-decay building would be a powerful addition to Layer 7.

---

### 4. Hospice Accessibility

**Verdict: Good foundation. Needs more granular controls and a dedicated configuration surface.**

The plan addresses hospice accessibility in multiple locations: gore_level (0-3), scare intensity caps, difficulty profile with generous resources, `validate_horror_intensity`, and `generate_hospice_report`. The hospice_comfort difficulty profile is thoughtful with its 0.6 false_alarm_ratio and 0.4 menace ceiling.

**Critical Issue: No unified hospice configuration surface.**
Hospice settings are scattered across multiple systems: gore_level in the horror atmosphere, scare_intensity in gameplay, difficulty_multiplier in items, enemy_aggression in AI, and various per-layer overrides. A caregiver or patient configuring the experience would need to understand all of these independently. There is no single "hospice mode" toggle or profile that sets everything at once.

- **Recommendation (Critical):** Create a `hospice_profile` composite action (or extend the existing difficulty profile) that sets ALL hospice-relevant parameters from a single call. Input: a comfort level (1-5, where 1 is "exploration only, no horror" and 5 is "full horror, generous resources"). This maps to specific values across all subsystems. The `generate_hospice_report` action already exists for validation; the missing piece is the unified configuration input.

**Critical Issue: Motion and visual sensitivity not addressed.**
The plan handles gore, scare intensity, and difficulty but does not address:
- Flickering light sensitivity (photosensitive epilepsy / migraine triggers). The flickering light system (Layer 5) could be harmful. Hospice patients on certain medications have lower seizure thresholds.
- Audio volume spikes from jump scares. The gameplay research mentions "configurable volume cap" but this is not specified as a concrete parameter.
- Screen shake or rapid camera movement from scare events.

- **Recommendation (Critical):** Add a `visual_comfort` parameter to the hospice profile: at level 1, flickering lights are replaced with steady dim lights, screen effects are disabled, and audio uses dynamic range compression (no sudden loud sounds). This is not just nice-to-have -- for patients with neurological conditions, flickering lights are a genuine health risk.

**Suggestion: Colorblind-safe item indicators.**
D1.4 mentions "clear visual indicators (subtle glow material)" for items but does not specify colorblind-safe design. A red health item glow is invisible to protanopes. Use luminance contrast (bright vs dark) as the primary indicator, with color as secondary.

---

### 5. Gameplay Integration -- Lock/Key Solvability, Safe Rooms, Difficulty Scaling

**Verdict: The algorithmic foundations are solid. Two structural gaps.**

The lock-and-key placement algorithm (mission graph approach with BFS validation) is correct and well-referenced (metazelda, Dormans cyclic generation). Safe room placement rules are appropriate (max 8 rooms between safe rooms, single entry, always on critical path). The RE4-style DDA with hidden rank is the right model for this genre.

**Important Issue: No dead-end detection for solvability validation.**
The lock-and-key algorithm places keys in rooms reachable without the corresponding lock. But it does not explicitly check for scenarios where the player could reach a dead-end area with no exit except through a lock they do not yet have the key for. The BFS validation step checks forward reachability but not "can the player always backtrack to the critical path from any reachable room."

- **Recommendation (Important):** Add bidirectional reachability validation. After placing all locks and keys, verify that from every room in the reachable set, the player can return to the start without passing through a lock they have not yet opened. This prevents soft-locks where the player enters an optional side area and cannot return.

**Important Issue: Safe room placement is build-time only.**
The plan places safe rooms during floor plan generation and marks them in the spatial registry. But for runtime-generated interiors (Sub-Plan F), safe room placement timing is unclear. If interiors generate on approach, does the safe room appear before or after the player enters the building? The player needs to know a safe room exists before committing to entering a dangerous building.

- **Recommendation (Important):** Pre-compute safe room locations at build time even if interior detail is deferred to runtime. The building descriptor should always contain safe room positions so that exterior indicators (a distinct door, a light above the entrance) can be placed at build time. The interior furnishing of the safe room can remain runtime-deferred.

---

### 6. Performance at Scale -- 20+ Buildings, Runtime Streaming

**Verdict: The architecture is sound but the hour estimates for Sub-Plan F are optimistic.**

The HiGen grid mapping, 5ms frame budget, memory budget system, and interior LOD tiers are all the right ideas. The decision to use `GenerateAtRuntime` instead of `GenerateOnDemand` (avoiding the known partition actor bug) shows good awareness of engine gotchas.

**Important Issue: Interior generation latency.**
F2.1 targets "interior generates in <500ms" on approach. For a 2-story building with 8 rooms, each needing wall pieces, floor, ceiling, furniture, and horror dressing, that is a lot of HISM instance creation, PCG graph execution, and potentially fallback mesh generation in a single burst. 500ms is an entire frame at 2 FPS -- even if distributed across multiple frames via the scheduler, the player may see pop-in or incomplete interiors.

- **Recommendation (Important):** Define a loading strategy for interior generation. Options: (a) pre-generate interior structure (walls/floors) at editor time, defer only furniture/dressing to runtime; (b) use a "door opening" animation (1-2 seconds) to mask the generation; (c) generate a simplified LOD interior first (walls only), then fill in detail over subsequent frames. Option (a) is safest for the hospice audience, who may be sensitive to visual pop-in.

**Suggestion: Draw call budget per building.**
The plan mentions "HISM draw calls < 10 per building" as a test criterion for B5.1, which is reasonable. But Sub-Plan C adds vegetation, horror dressing, props, road furniture, etc. around each building. The total per-building draw call budget including dressing is not specified. At 20+ buildings with 7 atmosphere layers each, draw calls could balloon.

- **Recommendation:** Add a composite draw call budget (structure + dressing) per building and enforce it in the `dress_town_block` orchestrator. If a building exceeds budget, reduce lower-priority layers first (audio emitters before debris, cobwebs before fog).

---

### 7. Non-Programmer Usability via MCP

**Verdict: The conversational UX design is strong. The action count is a risk.**

The progressive disclosure pattern, confidence-based routing, and the golden path conversation flow are all excellent design for MCP-based interaction. The user never needs to know about regex classifiers or GCD algorithms -- they just see "Scanned 47 meshes, 78% coverage, 6 uncertain."

**Important Issue: The 110-135 new action surface area is overwhelming for discovery.**
When a user runs `monolith_discover("pcg")`, they will see 37+ actions from Sub-Plan A alone. Add kit scanner, dressing, gameplay, terrain, and runtime actions and the namespace exceeds 100 actions. Even with `monolith_discover`, finding the right action requires knowing the vocabulary.

- **Recommendation (Important):** Organize pcg_query actions into sub-categories within the discover output. Group by workflow stage: "Graph Construction" (create/add/connect), "Kit Scanner" (scan/classify/edit), "Generation" (build/dress/gameplay), "Runtime" (configure/monitor). Alternatively, add high-level composite actions that are the primary user entry points (scan_modular_kit, build_with_kit, dress_town_block, dress_gameplay) and mark the lower-level graph CRUD as "advanced."

**Suggestion: Add a `pcg_wizard` or `quick_start` action.**
For a user who has never used the PCG system, a single action that walks through the entire workflow (scan kit -> build a test building -> apply dressing) would dramatically reduce the onboarding curve. This could be a template that generates a 1-building demo from any scanned kit.

---

### 8. What Is Missing That a Real User Would Expect?

**Undo/rollback for PCG generation.**
The plan has `cleanup_pcg` to remove generated actors, but there is no undo for kit classification changes, no rollback for a bad `build_with_kit` result, and no "try a different seed" workflow. Users experimenting with generation will want to quickly discard results and try again.

- **Recommendation (Important):** Add a `pcg_undo` or `rollback_generation` action that removes all actors from the last generation pass (tracked by a generation ID or timestamp). Also add a `regenerate_with_seed` action that re-runs the last generation with a different seed. These are essential for an iterative design workflow.

**No visual diff between generation runs.**
When a user changes the kit, decay level, or seed and regenerates, there is no way to compare the before and after. The existing `editor_query("capture_scene")` could be leveraged here.

- **Suggestion:** Add optional before/after scene capture to `build_with_kit` and `dress_town_block`. Save screenshots with generation metadata for comparison.

**No cost/time estimate before generation.**
When a user says "generate a 4-building horror block," they have no idea whether that will take 2 seconds or 2 minutes, or how many instances it will create. The plan returns a summary after generation but not before.

- **Suggestion:** Add a `preview_generation` or `estimate_generation` action that returns expected piece counts, estimated time, and memory impact without actually generating. This is especially important for the runtime streaming system where budget awareness matters.

---

### 9. Community Features -- Shareable Kit JSONs, Preset Library

**Verdict: The foundation is there but community sharing needs explicit design.**

Kit JSON export/import (B3.3) and preset save/load (A3.1, C5.2) are included. The kit JSON schema is clean and well-versioned.

**Important Issue: Kit JSONs contain absolute asset paths.**
The kit JSON stores `"asset_path": "/Game/HorrorKit/Meshes/SM_Wall_Solid_01"`. If a user shares this JSON and the recipient has the same marketplace kit installed at a different path (e.g., `/Game/Content/POLYGON_Horror/`), every path will fail.

- **Recommendation (Important):** Add a `rebind_kit` action that takes an existing kit JSON and a new scan path, then re-maps piece entries to matching meshes in the new path (matched by name similarity + dimension matching). This makes shared kit JSONs portable across projects with different content organization.

**Suggestion: Kit JSON could include a marketplace product identifier.**
If the kit JSON includes the marketplace product name or publisher, the `import_kit` action could attempt auto-discovery of the content path on the recipient's project. This is not reliable enough to be automatic but could suggest "This kit was built for POLYGON Horror. Found /Game/PolygonHorror/ -- use this path?"

---

### 10. Incremental Value Delivery -- Does Each Milestone Produce Something Usable?

**Verdict: Milestones 1 and 2 deliver standalone value. Milestones 3 and 4 require 1+2 to be useful.**

- **Milestone 1 (Foundation + Kit Scanner):** Delivers the killer feature. A user can scan a kit and build buildings. This is independently valuable and shippable. Good.
- **Milestone 2 (Town Dressing):** Adds vegetation, horror atmosphere, props, roads. Depends on M1 but adds clear visual value. Good.
- **Milestone 3 (Gameplay):** Items, enemies, objectives. Requires M1 buildings + M2 atmosphere to be meaningful. Cannot demo in isolation. Acceptable -- this is inherently dependent.
- **Milestone 4 (Runtime + Streaming):** Optimization and scale. Only valuable with everything else in place. Correct ordering.

**Important Issue: Sub-Plan C (Town Dressing) at 200-280h is the largest sub-plan but is not the killer feature.**
The kit scanner (Sub-Plan B, 120-160h) is correctly prioritized. But Sub-Plan C is almost twice the size and includes road network generation (40-55h) which is a significant engineering effort for what may not be needed in every project. The plan does not distinguish between "essential dressing" (horror atmosphere, debris) and "nice-to-have dressing" (full road networks with lane markings).

- **Recommendation (Important):** Split Sub-Plan C into two milestones: C-essential (horror atmosphere layers, basic vegetation, per-building props) and C-extended (road networks, street furniture, vehicles, ground detail). Ship C-essential as part of Milestone 2 and defer C-extended. This reduces M2 from ~200h to ~120h, delivering usable horror dressing much sooner. Road networks are valuable but not required for a horror game set in individual buildings.

---

### Summary of Issues by Severity

**Critical (must fix before implementation):**
1. Kit type detection -- handle pre-assembled kits (KitBash3D) and prop-only kits, not just tile-based modular
2. Unified hospice configuration surface -- single action/profile that sets all comfort parameters
3. Flickering light safety -- visual_comfort parameter to disable photosensitive triggers for hospice patients

**Important (should fix, significantly impacts quality):**
4. Two-pass scan strategy -- fast initial results, async topology analysis
5. Thumbnail generation for classification review
6. Per-room active layer budget to prevent tension plateau
7. Bidirectional reachability validation for lock/key solvability
8. Pre-compute safe room locations even for runtime-deferred interiors
9. Interior generation loading strategy to prevent pop-in
10. Sub-categorize the 100+ action namespace for discoverability
11. Undo/rollback and seed-based regeneration for iterative workflow
12. Kit JSON path rebinding for cross-project portability
13. Split Sub-Plan C into essential and extended phases
14. Zone-based classification for Megascans-style kits
15. Adaptive classification weight based on naming strategy detection

**Suggestions (nice to have):**
16. "False safety" maintained_room storytelling pattern
17. Colorblind-safe item indicators (luminance over hue)
18. pcg_wizard quick-start action for onboarding
19. Before/after scene capture for generation comparison
20. preview_generation cost estimation action
21. Draw call budget enforcement in dress_town_block
22. Marketplace product identifier in kit JSON for auto-discovery
23. Single-material atlas kit detection for Synty

---

### Final Remarks

The plan is ambitious -- 800-1080h across 6 sub-plans -- but the incremental delivery strategy and parallel work package design make it tractable. The research depth is genuinely impressive and the horror design draws from the right references (Alien Isolation's menace gauge, RE4's DDA, L4D's intensity states). The kit scanner UX is a genuine competitive advantage over every comparable tool surveyed.

The three critical issues (kit type detection, unified hospice config, flickering light safety) should be addressed before implementation begins. The hospice safety concern is not theoretical -- this game serves patients who may have seizure disorders, neurological conditions, or medication-induced photosensitivity. Getting this wrong is not a bug, it is a harm.

The important issues are implementation quality concerns that will become apparent during the first real-world kit scan or the first playtest with a hospice patient. Addressing them proactively will save significant rework.

This plan is ready for implementation with the critical issues resolved. Milestone 1 should proceed with Sub-Plans A and B in parallel as specified.
