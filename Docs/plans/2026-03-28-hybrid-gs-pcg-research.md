# Hybrid GeometryScript + PCG Architecture for Procedural Buildings

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Architecture decision for Monolith procedural building pipeline

---

## Executive Summary

The hybrid approach is the correct architecture. GeometryScript generates custom geometry (unique building shells, terrain-adaptive foundations, complex structural pieces), saves them as StaticMesh assets, and PCG handles placement, instancing, and scatter of modular pieces and props. This combines GeometryScript's mesh-level power with PCG's production-ready placement, instancing, and visual iteration tools.

**Recommendation:** Three-layer hybrid with our existing floor plan system as the orchestrator.

---

## 1. The Three-Layer Architecture

### Layer 1: GeometryScript -- Mesh Vocabulary (Editor-Time)

**Purpose:** Generate unique mesh pieces that cannot come from a modular kit.

**What it generates:**
- Terrain-adaptive foundations (cut-fill, stepped, piers)
- Custom room shells with non-rectangular shapes
- Swept-profile walls with mitered corners
- Roof geometry (gable, hip, mansard via swept profiles)
- Stairwells with proper IBC-compliant geometry
- Facade elements with window/door cutouts (via Selection+Inset, not booleans)
- Unique one-off pieces: collapsed walls, holes, horror deformations

**Output:** Saved as `UStaticMesh` assets via `CreateNewStaticMeshAssetFromMesh()` at paths like `/Game/Generated/Buildings/{BuildingId}/{PieceName}`.

**Key constraint:** `UDynamicMeshComponent` does NOT support instancing. It owns its mesh data, unlike `UStaticMesh` which is shared. Any geometry that will be instanced MUST be baked to StaticMesh first.

**Performance note:** Generation is editor-time only (or pre-generation before runtime). GeometryScript functions like `CreateNewStaticMeshAssetFromMesh` are editor-only API.

### Layer 2: Floor Plan System -- The Orchestrator (Our Existing Code)

**Purpose:** Determine WHAT goes WHERE. This is our existing SP2 treemap + adjacency system, spatial registry, and building descriptors.

**What it decides:**
- Room layout (treemap subdivision, adjacency satisfaction)
- Wall types per edge (exterior/interior/shared, with/without openings)
- Door positions and sizes
- Window placement (facade grammar)
- Room-type assignments (kitchen, bedroom, bathroom, etc.)
- Stairwell positions and floor-to-floor coordination
- Decay parameter per room (0.0 pristine to 1.0 destroyed)

**Output:** Structured data that feeds into PCG. This is the bridge between our procedural logic and PCG's placement engine.

### Layer 3: PCG -- Placement & Instancing (Editor or Runtime)

**Purpose:** Place modular pieces and scatter props using ISM/HISM for performance.

**What it handles:**
- Wall segment placement (pick correct piece per edge type)
- Floor/ceiling tile placement
- Furniture scatter per room type (weighted random from DataAsset tables)
- Horror dressing (debris, blood, broken items driven by decay parameter)
- Prop scatter on surfaces (cobwebs in corners, dust, stains)
- Environmental storytelling elements
- All instanced via ISM components (1 draw call per unique mesh)

---

## 2. Passing Structured Data to PCG

### The Core Problem

Our floor plan is a 2D grid with room IDs, door positions, wall types. PCG works with point data and attributes. How to bridge?

### Solution: Custom PCG Data Source Node (C++)

Create a custom `UPCGSettings` + `IPCGElement` pair that reads our spatial registry and emits PCG point data with attributes.

**C++ approach (recommended):**

```
Module dependency: PCG in Build.cs

Class 1: UPCGFloorPlanSettings : public UPCGSettings
  - UPROPERTY: reference to spatial registry JSON or DataAsset
  - Override InputPinProperties() -> empty (data source, no input)
  - Override OutputPinProperties() -> Point data output pin

Class 2: FPCGFloorPlanElement : public IPCGElement
  - ExecuteInternal():
    1. Read floor plan from spatial registry (JSON at Saved/Monolith/SpatialRegistry/)
    2. For each wall segment, emit a PCG point with:
       - Transform: position + rotation of wall center
       - Bounds: wall dimensions
       - Attribute "wall_type" (string): "exterior", "interior", "shared"
       - Attribute "has_door" (bool)
       - Attribute "has_window" (bool)
       - Attribute "room_id" (int32)
       - Attribute "room_type" (string): "kitchen", "bedroom", etc.
       - Attribute "decay" (float): 0.0-1.0
       - Attribute "floor_index" (int32)
    3. Output via Context->OutputData.TaggedData
```

### PCG Attribute System (Verified in Engine Source)

PCG points support arbitrary metadata attributes. Each point has a hidden `MetadataEntry` field linking to attribute values. Supported types include:
- `float`, `double`, `int32`, `int64`
- `FVector`, `FVector4`, `FQuat`, `FTransform`
- `FString`, `FName`
- `FSoftObjectPath` (for mesh references)
- `bool`

Attributes are manipulated via `UPCGMetadata` and accessed in custom nodes via `PCGAttributeAccessorKeys`. No distinction between "properties" (prefixed `$`) and custom metadata in processing -- both are "attributes."

### Alternative: UPCGParamData (Attribute Sets)

For non-spatial data (building-level parameters, style presets):

```
UPCGParamData : public UPCGData
  - Holds metadata without spatial information
  - Can carry named key-value pairs
  - FindOrAddMetadataKey(FName) -> int64
  - FilterParamsByName(FName) -> UPCGParamData*
  - Use for: building style, decay level, room furniture tables
```

### New in 5.7: Polygon2D Data Type

**Critical discovery:** UE 5.7 introduces `UPCGPolygon2DData` which represents closed 2D polygons with 3D transforms. This maps DIRECTLY to room boundaries.

**Polygon2D capabilities (verified in engine source):**
- `UPCGPolygon2DData : public UPCGPolyLineData` -- inherits spatial data
- `UPCGPolygon2DInteriorSurfaceData` -- converts polygon to a surface for point sampling INSIDE the polygon
- `EPCGPolygonOperation`: Union, Difference, Intersection, PairwiseIntersection, ExclusiveOr, CutWithPaths
- Can be converted to surface data for interior scatter (furniture placement!)
- Spline discretization with configurable error tolerance

**This means:** We can emit room boundaries as Polygon2D data, and PCG can natively:
1. Sample points INSIDE room polygons (via `UPCGPolygon2DInteriorSurfaceData`)
2. Apply boolean operations between rooms
3. Cut polygons with paths (for corridors, doorways)
4. Use as exclusion zones

### Data Flow Architecture

```
Monolith MCP Action (create_building)
  |
  v
Floor Plan Generator (C++ / existing code)
  |
  v
Spatial Registry (JSON persistence)
  |
  v
Custom PCG Data Source Node (reads registry)
  |
  +---> Wall Points (with attributes) ---> PCG Wall Assembly Graph
  +---> Room Polygons (Polygon2D) ------> PCG Furniture Scatter Graph
  +---> Building Params (ParamData) -----> PCG Style/Decay Config
  |
  v
PCG Graphs (visual, artist-iterable)
  |
  v
ISM Components (1 draw call per unique mesh)
```

---

## 3. PCG for Furniture/Prop Scatter

### Room-Type-Driven Selection

PCG Static Mesh Spawner supports **weighted random selection**. Each entry has a Weight property (default 1). A mesh with weight 2 appears twice as often as one with weight 1.

**Per-room-type furniture tables via DataAssets:**

```
UDataAsset: DA_RoomFurniture_Kitchen
  - Meshes: [SM_Counter(3), SM_Stove(1), SM_Fridge(1), SM_Table(2), SM_Chair(4)]
  - Weights in parentheses

UDataAsset: DA_RoomFurniture_Bedroom
  - Meshes: [SM_Bed(1), SM_Nightstand(2), SM_Dresser(1), SM_Lamp(2)]
```

**PCG graph flow:**
1. Custom node reads room polygon + room_type attribute
2. Attribute Partition splits by room_type
3. Per partition: Surface Sampler on room interior polygon
4. Density Filter for appropriate furniture density
5. Static Mesh Spawner with room-type-specific DataAsset
6. Self-Pruning to prevent overlap
7. Bounds Modifier for collision avoidance

### Wall-Aligned vs Center Placement

**Wall-aligned items** (shelves, counters, beds against walls):
- Sample points along room perimeter (Polygon2D edges)
- Offset inward by item depth
- Rotate to face room center

**Center items** (tables, rugs):
- Sample points on interior surface
- Minimum distance from walls via Difference with inset polygon

### Door Clearance Zones

- Create exclusion polygons around door positions (90cm semicircle)
- Apply Polygon2D Difference to subtract clearance from scatter area
- Ensures no furniture blocks doorways

### Collision Avoidance

Native PCG provides:
- **Self-Pruning**: Prevents points within each other's bounds
- **Bounds Modifier**: Creates boundary around each mesh for spacing
- No custom collision detection needed

---

## 4. PCG for Horror Dressing

### Decay Parameter System

The decay parameter (0.0 pristine to 1.0 destroyed) from the building descriptor drives all horror scatter density.

**PCG implementation:**
1. Custom node emits room points with `decay` attribute
2. Density = decay * max_density (more decay = more debris)
3. Different decay ranges trigger different dressing layers:

```
Decay 0.0-0.2 (Lived-in):
  - Scattered papers, coffee cups, personal items
  - Density: low, Weight: light clutter meshes

Decay 0.2-0.5 (Neglected):
  - Dust particles, cobwebs in corners, knocked-over items
  - Some broken glass, peeling wallpaper decals
  - Density: medium

Decay 0.5-0.8 (Abandoned):
  - Heavy debris, broken furniture, water stains
  - Blood spatters (decals on walls/floors)
  - Boarded windows, missing ceiling tiles
  - Density: high

Decay 0.8-1.0 (Destroyed):
  - Collapsed sections, rubble piles
  - Massive environmental damage
  - Dense fog/particle effects
  - Density: maximum
```

### Vandalism Patterns

- **Knocked furniture**: Original placement + random rotation offset (0-90 deg around Z)
- **Broken windows**: Replace window mesh with broken variant (attribute swap)
- **Graffiti**: Wall-surface sampling + decal spawner (PCG can spawn BP actors with decal components)

### Corner Cobwebs

- Sample points at polygon vertices (corners)
- Filter by `decay > 0.3`
- Spawn cobweb mesh rotated to bisect corner angle
- Scale by decay value

### Flickering Lights

- PCG can spawn Blueprint actors (not just static meshes)
- Place light BP actors at ceiling-height points, spacing controlled by room size
- Decay parameter drives flicker intensity in the spawned BP

---

## 5. The Alternative: Pure GeometryScript (No PCG)

### What We Currently Do

Our existing Monolith system uses GeometryScript for everything:
- Generate building mesh (walls, floors, roofs) as single DynamicMesh
- Save as StaticMesh
- Furniture via direct actor spawning

### What PCG Adds That We Cannot Easily Replicate

| Capability | GeometryScript DIY | PCG Native |
|---|---|---|
| ISM instancing | Manual HISM management | Automatic via Static Mesh Spawner |
| Visual iteration | Code changes, recompile | Node graph, real-time preview |
| Weighted random mesh selection | Custom implementation | Built-in Spawner weights |
| Self-pruning / collision avoidance | Custom spatial queries | Built-in Self-Pruning node |
| Surface sampling (interior scatter) | Custom point generation | Native Surface Sampler |
| Polygon boolean ops | GeometryScript booleans | Polygon2D operations (2D, much faster) |
| Attribute Partition (per-room processing) | Manual loop/switch | Native node, multi-threaded |
| Artist accessibility | C++ only | Visual graph, no code |
| Density-based filtering | Custom | Native Density Filter |
| GPU-accelerated scatter | Not available | FastGeo in 5.7 |
| Runtime generation | Manual | Built-in scheduler |
| Partitioned world support | Manual | Built-in PCG partitioning |

### What GeometryScript Does That PCG Cannot

| Capability | GeometryScript | PCG |
|---|---|---|
| Custom mesh generation | Full mesh API | No mesh creation |
| Boolean operations on meshes | 3D CSG | 2D polygon only |
| Swept profiles / extrusions | Native | Not available |
| Terrain-adaptive geometry | Full control | Surface projection only |
| Mesh repair / optimization | Full API | Not available |
| Per-vertex operations | Full access | Not available |
| UV generation / manipulation | Full API | Not available |
| Material ID assignment | Per-triangle | Per-instance only |

### Verdict

**Neither alone is sufficient. The hybrid is clearly better.**

GeometryScript excels at creating geometry. PCG excels at placing things. Trying to do placement in GeometryScript means reinventing ISM management, collision avoidance, weighted selection, and visual iteration. Trying to do mesh generation in PCG is impossible -- PCG has no mesh creation capabilities.

---

## 6. Performance Comparison

### Approach A: All GeometryScript (Current)

```
Per building: 1 DynamicMesh -> 1 StaticMesh -> 1 Actor
  - Triangles: ~5K-50K depending on complexity
  - Draw calls: 1 per material slot per building
  - Memory: unique mesh data per building (no sharing)
  - Furniture: individual actors, 1 draw call each

City block (20 buildings, ~200 furniture pieces):
  - Draw calls: ~60-100 (buildings) + ~200 (furniture) = 260-300
  - Memory: ~200MB unique mesh data (estimated)
  - Generation: ~2-5s per building
```

### Approach B: Hybrid GeometryScript + PCG

```
Per building: GeometryScript generates unique shell -> StaticMesh
  PCG places modular interior pieces via ISM

Modular vocabulary: ~50 unique meshes (walls, floors, trim, etc.)
  - Each mesh type = 1 ISM component = 1 draw call

City block (20 buildings):
  - Draw calls: ~20 (unique shells) + ~50 (ISM types) + ~30 (furniture ISMs) = ~100
  - Memory: ~50MB shared meshes + ~40MB unique shells = ~90MB
  - Generation: ~2s per building shell + ~0.5s PCG scatter = ~2.5s total

Net: ~60-65% fewer draw calls, ~55% less memory
```

### Approach C: Pure PCG (Modular Only)

```
Modular kit: ~100 unique meshes for all building variants
  PCG places everything via ISM

City block (20 buildings):
  - Draw calls: ~100 (ISM types) = 100
  - Memory: ~80MB shared meshes (no unique geometry)
  - Generation: ~0.5s PCG only
  - Limitation: every building looks like kit-bashed modular pieces
  - Cannot do: terrain adaptation, non-rectangular rooms, custom facades
```

### ISM Performance Caveats (UE 5.7)

**Critical:** UE5 ISM has known performance issues above 10,000 instances per component. This is NOT a problem for our building interiors (typical: 50-200 instances per building). But worth tracking for city-scale scatter (vegetation, debris across entire blocks).

**FastGeo (new in 5.7):** GPU-accelerated PCG path. Enable via `PCG FastGeo Interop` plugin + CVar `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives=1`. Nearly 2x performance vs UE 5.5.

### Bottleneck Analysis

| Bottleneck | All GS | Hybrid | Pure PCG |
|---|---|---|---|
| Generation time | HIGH (mesh ops) | MEDIUM (shell + scatter) | LOW (scatter only) |
| Runtime draw calls | HIGH | LOW | LOWEST |
| Memory (unique data) | HIGH | MEDIUM | LOW |
| Visual variety | HIGH | HIGH | MEDIUM |
| Artist iteration speed | LOW | HIGH | HIGH |

---

## 7. PCG Programmatic Execution (C++ API)

### Triggering PCG from Code

Verified in engine source (`PCGComponent.h`):

```cpp
// Networked generation (replicates)
UFUNCTION(BlueprintCallable, NetMulticast, Reliable)
void UPCGComponent::Generate(bool bForce);

// Local generation (no replication, delayed)
UFUNCTION(BlueprintCallable)
void UPCGComponent::GenerateLocal(bool bForce);

// With generation trigger control + grid level
void UPCGComponent::GenerateLocal(
    EPCGComponentGenerationTrigger RequestedGenerationTrigger,
    bool bForce,
    EPCGHiGenGrid Grid = EPCGHiGenGrid::Uninitialized,
    const TArray<FPCGTaskId>& Dependencies = {}
);

// Get task ID for dependency tracking
FPCGTaskId GenerateLocalGetTaskId(bool bForce);

// Generation triggers
enum class EPCGComponentGenerationTrigger : uint8
{
    GenerateOnLoad,    // When component loads
    GenerateOnDemand,  // When requested via Blueprint/C++
    GenerateAtRuntime  // When scheduled by runtime scheduler
};

// Completion delegates
FOnPCGGraphGenerated OnPCGGraphGenerated;  // Native
FOnPCGGraphGeneratedExternal OnPCGGraphGeneratedExternal;  // BP-bindable
```

### Monolith Integration Pattern

```
MCP Action: create_building_with_pcg
  1. Generate building shell via GeometryScript -> SaveStaticMesh
  2. Write floor plan to spatial registry JSON
  3. Spawn actor with UPCGComponent
  4. Set PCG graph reference (furniture scatter graph)
  5. Set graph parameters (building_id, style, decay)
  6. Call GenerateLocal(true) -> triggers PCG execution
  7. Bind OnPCGGraphGenerated for completion notification
```

---

## 8. PCGEx Extended Toolkit (Third-Party)

### What It Adds

[PCGEx](https://github.com/PCGEx/PCGExtendedToolkit) fills gaps in native PCG that are relevant to building generation:

**Graph Theory:**
- Delaunay triangulation, Voronoi diagrams
- Minimum spanning trees
- Convex hull generation
- Graph topology queries

**Pathfinding:**
- A*/Dijkstra routing through generated clusters
- Pluggable heuristics

**Spatial Operations:**
- Point fusion with attribute blending
- Lloyd relaxation (even spacing)
- Bin packing
- Octree queries

**Architecture:**
- Processor pattern for per-input parallel processing
- Factory system (filters, blenders, samplers)
- Thread-safe data facades with cached buffering

**Compatibility:** UE 5.4, 5.5, 5.6, 5.7

### Relevance to Leviathan

- Voronoi for organic room subdivision (alternative to treemap)
- MST for corridor connectivity
- Pathfinding for validating room accessibility
- Lloyd relaxation for even furniture spacing
- Bin packing for fitting furniture into rooms

**License:** MIT (commercially safe)

---

## 9. Decision Framework

### Use GeometryScript When:

- Creating NEW geometry (unique shapes, custom profiles)
- Performing 3D boolean operations (wall openings, stairwell cutouts)
- Terrain-adaptive foundations (require full mesh control)
- Swept-profile walls (mitered corners, thin-wall technique)
- Roof generation (swept triangle profiles, hip faces)
- Mesh repair, optimization, UV generation
- Any per-vertex or per-triangle operation
- One-off unique pieces (horror deformations, collapsed sections)

### Use PCG When:

- Placing MANY instances of KNOWN pieces (furniture, wall segments, debris)
- Scatter operations (props on surfaces, objects in rooms)
- Weighted random selection from mesh tables
- Collision avoidance between placed items
- Density-based filtering (decay drives debris amount)
- Artist-iterable workflows (visual graph, real-time preview)
- Runtime generation needs
- Anything that benefits from ISM instancing

### Use Our Floor Plan System When:

- Room layout decisions (treemap, adjacency satisfaction)
- Wall-type classification (exterior/interior/shared)
- Door/window position logic
- Multi-story coordination
- Horror tension curve integration
- Building archetype rules

### The Handoff Points

```
GeometryScript -> StaticMesh Asset -> PCG references it for placement
Floor Plan -> JSON -> Custom PCG Node reads it -> PCG graph processes it
Floor Plan -> Room Polygons -> PCG Polygon2D -> Interior surface sampling
```

---

## 10. Implementation Roadmap

### Phase 1: PCG Data Bridge (12-16h)

- [ ] Create `UPCGFloorPlanSettings` + `FPCGFloorPlanElement` custom node
- [ ] Read spatial registry JSON in custom node
- [ ] Emit wall segment points with attributes (type, openings, room_id, decay)
- [ ] Emit room boundaries as Polygon2D data
- [ ] Emit building parameters as ParamData
- [ ] Wire into test PCG graph

### Phase 2: Modular Wall Assembly (16-20h)

- [ ] Define modular wall vocabulary (solid, window, door variants)
- [ ] GeometryScript generates vocabulary pieces -> saved as StaticMesh
- [ ] PCG graph: read wall points -> Attribute Partition by type -> Static Mesh Spawner per type
- [ ] ISM instancing verified working
- [ ] Corner pieces, T-junctions, end caps

### Phase 3: Furniture Scatter (20-24h)

- [ ] Create room furniture DataAssets per room type
- [ ] PCG graph: Polygon2D interior sampling -> room-type partition -> weighted spawner
- [ ] Wall-aligned placement subgraph
- [ ] Center placement subgraph
- [ ] Door clearance exclusion zones
- [ ] Self-Pruning for collision avoidance

### Phase 4: Horror Dressing (12-16h)

- [ ] Decay-driven debris scatter graph
- [ ] Damage tier logic (pristine -> destroyed)
- [ ] Corner cobweb spawner
- [ ] Wall decal spawner (blood, stains, graffiti)
- [ ] Flickering light BP spawner
- [ ] Boarded window mesh swap logic

### Phase 5: MCP Integration (8-12h)

- [ ] `create_building_with_pcg` action (orchestrates full pipeline)
- [ ] `set_room_decay` action (per-room decay, triggers PCG regeneration)
- [ ] `set_furniture_style` action (swap DataAsset tables)
- [ ] `regenerate_scatter` action (re-run PCG graph only)
- [ ] PCG graph references saved to building descriptor

**Total estimate: 68-88h**

---

## 11. New UE 5.7 PCG Features Relevant to This Work

### Production-Ready Status
PCG framework is now officially Production-Ready. Nearly 2x performance vs UE 5.5.

### Polygon2D Data Type
New closed polygon data type with boolean operations (Union, Difference, Intersection, ExclusiveOr, CutWithPaths). Interior surface conversion for scatter. This is THE feature that makes PCG viable for interior generation.

### PCG Editor Mode
Library of customizable tools built on PCG graphs. Can draw splines, paint points, create volumes -- each linked to a PCG graph with real-time parameter control. Useful for artist-driven override of procedural results.

### Biome Core V2
Local update model per biome actor. Relevant pattern for per-building PCG components -- each building updates independently.

### GPU Compute (FastGeo)
Multiplatform GPU acceleration. Enable via `PCG FastGeo Interop` plugin. Key CVar: `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives=1`.

### Spline Operations
New Split Splines and Spline Intersection operators. Useful for corridor generation and room boundary operations.

---

## 12. Risks and Mitigations

### Risk: PCG Attribute System Complexity
PCG's attribute system has a learning curve. Metadata, attribute accessors, domain IDs.
**Mitigation:** Start with simple string/float attributes. PCGEx provides cleaner facades.

### Risk: ISM Instance Limits
UE5 ISM degrades above ~10K instances per component.
**Mitigation:** Our use case is ~50-200 instances per building. City blocks stay well under 10K per mesh type.

### Risk: PCG Graph Debugging
PCG graphs can be opaque when things go wrong.
**Mitigation:** Use PCG debug visualization. Start simple, add complexity incrementally.

### Risk: Editor vs Runtime Generation
Some PCG features are editor-only. Runtime generation has different constraints.
**Mitigation:** Design for editor-time first (our primary use case via MCP). Runtime generation is secondary.

### Risk: Custom Node Maintenance
Custom C++ PCG nodes tied to engine API that may change.
**Mitigation:** Minimal custom node surface area. Only the data bridge node is custom; the rest uses native PCG.

---

## Sources

### Official Documentation
- [PCG Overview (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [PCG Framework Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-in-unreal-engine)
- [PCG Development Guides](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [PCG Node Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [Assembly (PCG)](https://dev.epicgames.com/documentation/en-us/unreal-engine/assembly-pcg)
- [GeometryScript Users Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
- [CreateNewStaticMeshAssetFromMesh](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/AssetManagement/CreateNewStaticMeshAssetfromMesh)
- [UPCGComponent::Generate API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGComponent/Generate)
- [PCG GPU Processing](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)
- [PCG Biome Core Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine)

### Tutorials and Community
- [Custom PCG Nodes Guide (Blueshift Interactive)](https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/)
- [PCG Modular Building Tutorial](https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation)
- [Realistic PCG Buildings Generation](https://dev.epicgames.com/community/learning/tutorials/4JWW/unreal-engine-realistic-pcg-buildings-generation)
- [Procedural Room Generation with Splines and PCG](https://dev.epicgames.com/community/learning/tutorials/eZVR/procedural-room-generation-with-splines-and-pcg-in-unreal-engine)
- [Shooter Tutorial - Procedural Level Using Only PCG](https://kolosdev.com/shooter-tutorial-procedural-level-using-only-pcg/)
- [PCG Interior Design](https://dev.epicgames.com/community/learning/tutorials/nwdJ/easy-detailed-interiors-in-unreal-engine-5-3-pcg-framework)
- [Generating Massive Worlds with PCG](https://zacksinisi.com/generating-massive-worlds-with-pcg-framework-for-unreal-engine-5/)
- [Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [GeometryScript FAQ (gradientspace)](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq)

### Talks
- [Unreal Fest 2025: Buildings and Biomes PCG](https://dev.epicgames.com/community/learning/talks-and-demos/pBl1/unreal-engine-unreal-fest-2025-buildings-and-biomes-pcg)
- [Leveraging PCG for Building and City Creation](https://dev.epicgames.com/community/learning/talks-and-demos/Z1wa/unreal-engine-leveraging-pcg-for-building-and-city-creation)

### Third-Party
- [PCGEx Extended Toolkit (GitHub, MIT)](https://github.com/PCGEx/PCGExtendedToolkit)
- [bendemott/UE5-Procedural-Building (GitHub)](https://github.com/bendemott/UE5-Procedural-Building)

### Engine Source (Verified Locally)
- `Engine/Plugins/PCG/Source/PCG/Public/PCGCommon.h` -- EPCGDataType enum (Point, Spline, Polygon2D, Volume, Param, DynamicMesh, etc.)
- `Engine/Plugins/PCG/Source/PCG/Public/PCGData.h` -- UPCGData base class
- `Engine/Plugins/PCG/Source/PCG/Public/PCGParamData.h` -- UPCGParamData for attribute sets
- `Engine/Plugins/PCG/Source/PCG/Public/Data/PCGPointData.h` -- UPCGPointData : UPCGBasePointData
- `Engine/Plugins/PCG/Source/PCG/Public/Data/PCGPolygon2DData.h` -- UPCGPolygon2DData : UPCGPolyLineData
- `Engine/Plugins/PCG/Source/PCG/Public/Data/PCGPolygon2DInteriorData.h` -- UPCGPolygon2DInteriorSurfaceData for interior sampling
- `Engine/Plugins/PCG/Source/PCG/Public/Elements/Polygon/PCGPolygon2DOperation.h` -- Boolean ops (Union, Difference, Intersection, CutWithPaths)
- `Engine/Plugins/PCG/Source/PCG/Public/PCGComponent.h` -- Generate(), GenerateLocal(), completion delegates
- `Engine/Plugins/PCG/Source/PCG/Public/PCGSettings.h` -- UPCGSettings base for custom nodes

---

## Appendix A: PCG Data Type Hierarchy (from Engine Source)

```
EPCGDataType (uint32 bitmask):
  Point           = 1 << 1
  Spline          = 1 << 2
  LandscapeSpline = 1 << 3
  Polygon2D       = 1 << 13    // NEW in 5.7
  PolyLine        = Spline | LandscapeSpline | Polygon2D
  Landscape       = 1 << 4
  Texture         = 1 << 5
  RenderTarget    = 1 << 6
  VirtualTexture  = 1 << 12
  BaseTexture     = Texture | RenderTarget
  Surface         = Landscape | BaseTexture | VirtualTexture
  Volume          = 1 << 7
  Primitive       = 1 << 8
  DynamicMesh     = 1 << 10
  StaticMeshResource = 1 << 11
  Concrete        = Point | PolyLine | Surface | Volume | Primitive | DynamicMesh
  Composite       = 1 << 9   (boolean operations)
  Spatial         = Composite | Concrete
  Resource        = StaticMeshResource
  ProxyForGPU     = 1 << 26
  Param           = 1 << 27   (Attribute Sets)
  Settings        = 1 << 28
  Other           = 1 << 29
  Any             = (1 << 30) - 1

Class hierarchy:
  UPCGData (base)
    UPCGSpatialData
      UPCGSurfaceData
        UPCGPolygon2DInteriorSurfaceData  // room interior surface!
      UPCGBasePointData
        UPCGPointData                      // the workhorse
      UPCGPolyLineData
        UPCGSplineData
        UPCGPolygon2DData                  // room boundaries!
      UPCGVolumeData
      UPCGPrimitiveData
      UPCGDynamicMeshData
    UPCGParamData                          // attribute sets (key-value)
```

## Appendix B: Custom PCG Node Skeleton

```cpp
// FloorPlanPCGNode.h
#pragma once
#include "PCGSettings.h"

UCLASS(BlueprintType, ClassGroup = (Procedural))
class UPCGFloorPlanSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
    // Path to spatial registry JSON
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FString SpatialRegistryPath;

    // Building ID to load
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings, meta = (PCG_Overridable))
    FString BuildingId;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("FloorPlanDataSource"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override;
    virtual FPCGElementPtr CreateElement() const override;
};

// Output pins: Wall Points, Room Polygons, Building Params
// ExecuteInternal: Read JSON -> emit points with attributes
```
