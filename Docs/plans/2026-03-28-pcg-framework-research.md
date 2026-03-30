# UE5 PCG Framework -- Complete Deep Dive for Building Generation

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG now Production-Ready)
**Context:** Evaluating whether to add a PCG module to Monolith MCP for building generation

---

## Table of Contents

1. [PCG Framework Architecture](#1-pcg-framework-architecture)
2. [What PCG is Good At (Building Placement)](#2-what-pcg-is-good-at)
3. [PCG for Modular Building Assembly](#3-pcg-for-modular-building-assembly)
4. [What PCG Cannot Do (Limitations)](#4-what-pcg-cannot-do)
5. [PCG + GeometryScript Hybrid](#5-pcg--geometryscript-hybrid)
6. [PCG C++ API for MCP Integration](#6-pcg-c-api-for-mcp-integration)
7. [PCG Plugins and Extensions](#7-pcg-plugins-and-extensions)
8. [Real-World PCG Building Examples](#8-real-world-pcg-building-examples)
9. [Verdict: Should Monolith Add a PCG Module?](#9-verdict)
10. [Sources](#10-sources)

---

## 1. PCG Framework Architecture

### Overview

The Procedural Content Generation (PCG) framework is a **node-based graph system** built into Unreal Engine for generating content through rules rather than manual placement. It was experimental in UE 5.2-5.3, beta in 5.4, and became **production-ready in UE 5.7** with nearly 2x the performance of 5.5.

Key selling point: PCG is a **placement and distribution engine**, not a geometry creation engine. It generates **point clouds** that are then used to spawn existing assets (static meshes, actors, spline meshes, etc.) with rule-based variation.

### Core Components

**UPCGComponent** (`PCGComponent.h`)
- Inherits from `UActorComponent`
- Added to any actor to make it a PCG source
- References a `UPCGGraphInterface` (the PCG graph asset)
- Controls generation triggers, seed, partitioning
- Key methods:
  - `Generate()` / `GenerateLocal(bool bForce)` -- trigger generation
  - `Cleanup()` / `CleanupLocal(bool bRemoveComponents)` -- remove generated content
  - `CancelGeneration()` -- abort in-progress generation
  - `SetGraph(UPCGGraphInterface*)` -- assign graph at runtime
  - `GetGeneratedGraphOutput()` -- retrieve output data
  - `ClearPCGLink()` -- detach generated content from PCG management

**UPCGGraph** (`PCGGraph.h`)
- Inherits from `UPCGGraphInterface`
- The core graph asset containing nodes and edges
- Key methods for programmatic construction:
  - `AddNode(UPCGSettingsInterface*)` -- add node with settings
  - `AddNodeOfType(TSubclassOf<UPCGSettings>)` -- create node by type
  - `AddNodeInstance(UPCGSettings*)` -- create node referencing existing settings
  - `AddNodeCopy(const UPCGSettings*)` -- create node copying settings
  - `RemoveNode(UPCGNode*)` / `RemoveNodes(TArray<UPCGNode*>&)` -- removal
  - `AddEdge(UPCGNode*, FName, UPCGNode*, FName)` -- connect pins
  - `RemoveEdge(...)` -- disconnect
  - `GetInputNode()` / `GetOutputNode()` -- graph entry/exit
  - `FindNodeByTitleName(FName)` -- search nodes
  - `ForEachNodeRecursively(...)` -- traverse all nodes including subgraphs
- Supports **User Parameters** (FInstancedPropertyBag) -- typed parameters exposed to graph consumers
- `SetGraphParameter<T>(FName, const T&)` / `GetGraphParameter<T>(FName)` -- set/get params

**UPCGSubsystem** (`PCGSubsystem.h`)
- World subsystem (`UTickableWorldSubsystem`) -- one per world
- Central scheduler for all PCG generation
- Key methods:
  - `ScheduleComponent(UPCGComponent*, EPCGHiGenGrid, bool bForce, Dependencies)` -- schedule a component
  - `ScheduleGraph(UPCGGraph*, UPCGComponent*, PreGraphElement, InputElement, Dependencies, Stack, bAllowHierarchical)` -- schedule a graph directly
  - `ScheduleGeneric(TFunction<bool()>, UPCGComponent*, Dependencies)` -- schedule arbitrary work
  - `ScheduleCleanup(UPCGComponent*, bool bRemoveComponents, Dependencies)` -- schedule cleanup
  - `RegisterOrUpdatePCGComponent(UPCGComponent*)` -- register with octree
  - `GetAllIntersectingComponents(FBoxCenterAndExtent)` -- spatial query
  - `RefreshRuntimeGenComponent(UPCGComponent*)` -- trigger runtime refresh
  - `GetInstance(UWorld*)` -- static accessor

**PCGVolume**
- Simple box volume actor that defines a generation region
- Contains a UPCGComponent
- Used to scope where PCG graphs execute

### Data Flow Model

PCG graphs process data left-to-right through nodes. The fundamental currency is **point data** -- collections of points with transforms, bounds, density, color, and arbitrary metadata attributes.

Data flows through `FPCGTaggedData` containers in `FPCGDataCollection` objects. Each tagged data has:
- A `UPCGData*` pointer (polymorphic)
- A pin label string
- Tags for filtering

### Data Types (from UE 5.7 source)

Complete hierarchy from engine source:

```
UPCGData (base)
  |-- UPCGSpatialData (spatial base -- has transform, bounds, density functions)
  |     |-- UPCGSpatialDataWithPointCache
  |     |     |-- UPCGBasePointData
  |     |     |     |-- UPCGPointData (THE core type -- collection of FPCGPoint)
  |     |     |     |-- UPCGPointArrayData
  |     |     |-- UPCGSurfaceData (2D surface -- landscape, render targets)
  |     |     |     |-- UPCGLandscapeData
  |     |     |     |-- UPCGBaseTextureData
  |     |     |     |     |-- UPCGTextureData
  |     |     |     |     |-- UPCGRenderTargetData
  |     |     |     |-- UPCGSplineInteriorSurfaceData
  |     |     |     |-- UPCGVirtualTextureData
  |     |     |     |-- UPCGWorldRayHitData
  |     |     |     |-- UPCGPolygon2DInteriorSurfaceData  [NEW 5.7]
  |     |     |-- UPCGVolumeData (3D volume)
  |     |     |     |-- UPCGWorldVolumetricData
  |     |     |-- UPCGPolyLineData (spline/polyline base)
  |     |     |     |-- UPCGSplineData
  |     |     |     |-- UPCGLandscapeSplineData
  |     |     |     |-- UPCGPolygon2DData  [NEW 5.7]
  |     |     |-- UPCGPrimitiveData
  |     |     |-- UPCGCollisionShapeData
  |     |     |-- UPCGDifferenceData
  |     |     |-- UPCGIntersectionData
  |     |     |-- UPCGProjectionData
  |     |     |     |-- UPCGSplineProjectionData
  |     |     |-- UPCGUnionData
  |     |-- UPCGCollisionWrapperData
  |     |-- UPCGDynamicMeshData
  |-- UPCGResourceData
  |     |-- UPCGStaticMeshResourceData
  |-- UPCGUserParametersData
```

**FPCGPoint** (the fundamental unit):
```cpp
struct FPCGPoint {
    FTransform Transform;           // Position, Rotation, Scale
    float Density = 1.0f;           // 0-1 weight for filtering
    FVector BoundsMin = -FVector::One();  // Local space AABB min
    FVector BoundsMax = FVector::One();   // Local space AABB max
    FVector4 Color = FVector4::One();     // RGBA color
    float Steepness = 0.5f;         // 0-1 density falloff sharpness
    int32 Seed = 0;                 // Per-point deterministic seed
    int64 MetadataEntry = -1;       // Index into metadata table
};
```

Points can carry arbitrary **attributes** (metadata) -- any typed data stored per-point. Attributes and point properties are treated interchangeably in most nodes, prefixed with `$` for built-in properties.

### PCG Node Categories (from engine source headers, ~100+ nodes)

**Generation:**
- `PCGCreatePoints` / `PCGCreatePointsGrid` / `PCGCreatePointsSphere` -- generate point patterns
- `PCGSurfaceSampler` -- sample points on landscape/surfaces
- `PCGPointFromMeshElement` -- generate points from mesh surface
- `PCGCreateSpline` -- create spline data
- `PCGCreateSurfaceFromSpline` -- convert spline to surface

**Filtering:**
- `PCGDensityFilter` -- filter by density threshold
- `PCGFilterByAttribute` -- filter by attribute value
- `PCGFilterByIndex` -- filter by point index
- `PCGFilterByTag` -- filter by data tags
- `PCGFilterByType` -- filter by data type
- `PCGNormalToDensity` -- convert surface normal to density
- `PCGCullPointsOutsideActorBounds` -- spatial culling
- `PCGAttributeFilter` -- general attribute predicate

**Transforms:**
- `PCGTransformPoints` -- move/rotate/scale points
- `PCGCopyPoints` -- copy points from source to target positions
- `PCGBoundsModifier` -- adjust point bounds
- `PCGPointExtentsModifier` -- modify extents
- `PCGApplyScaleToBounds` -- scale bounds

**Set Operations:**
- `PCGDifferenceElement` -- boolean subtract point sets
- `PCGInnerIntersectionElement` -- boolean intersect
- `PCGOuterIntersectionElement` -- boolean union
- `PCGMergeElement` -- merge multiple data streams
- `PCGCombinePoints` -- combine point collections

**Attributes:**
- `PCGCreateAttribute` -- add attribute
- `PCGDeleteAttributesElement` -- remove attributes
- `PCGCopyAttributes` -- copy attributes between data
- `PCGMergeAttributes` -- merge attribute sets
- `PCGAttributeNoise` -- add noise to attributes
- `PCGAttributeRemap` -- remap attribute values
- `PCGMatchAndSetAttributes` -- conditional attribute setting

**Spawning:**
- `PCGStaticMeshSpawner` -- spawn ISM/HISM instances
- `PCGSpawnActor` -- spawn full actors
- `PCGSpawnSplineMesh` -- spawn spline mesh components
- `PCGAddComponent` -- add components to actors

**Spatial:**
- `PCGDistance` -- compute distances between point sets
- `PCGPointNeighborhood` -- find neighboring points
- `PCGConvexHull2D` -- compute 2D convex hull
- `PCGClusterElement` -- cluster points
- `PCGPathfindingElement` -- pathfinding between points

**Spline:**
- `PCGGetSplineControlPoints` -- extract control points
- `PCGCleanSpline` -- simplify splines
- `PCGElevationIsolines` -- generate elevation contours

**Grammar:**
- `PCGExecuteGrammar` -- execute shape grammar rules
- Grammar modules -- define rule-based generation (CGA-like)

**Utility:**
- `PCGLoopElement` -- iterate subgraphs
- `PCGPrintElement` -- debug output
- `PCGDebugElement` -- debug visualization
- `PCGDataTableRowToParamData` -- load DataTable rows
- `PCGGetActorProperty` -- read actor properties
- `PCGGetConsoleVariable` -- read CVars
- `PCGMutateSeed` -- modify seeds
- `PCGParseString` -- parse string data

### Generation Modes

```cpp
enum class EPCGComponentGenerationTrigger : uint8 {
    GenerateOnLoad,    // Generate when component loads into level
    GenerateOnDemand,  // Generate only when explicitly requested
    GenerateAtRuntime  // Generate when scheduled by Runtime Generation Scheduler
};
```

- **GenerateOnLoad** -- standard editor-time generation, results saved with level
- **GenerateOnDemand** -- triggered via Blueprint/C++ call to `Generate()`
- **GenerateAtRuntime** -- managed by `FPCGRuntimeGenScheduler`, generates/cleans up based on player proximity and grid partitioning. Used by LEGO Fortnite for procedural island generation.

### Deterministic Seeds

The `UPCGComponent` has a `Seed` property (default 42). Combined with per-node and per-point seeds, this creates deterministic, reproducible output. Same seed = same result. Critical for:
- Level design iteration
- Debugging
- Multiplayer synchronization

### Hierarchical Generation

PCG supports multi-grid hierarchical generation where different nodes execute at different grid sizes. Larger grids handle macro placement (buildings on lots), smaller grids handle detail (furniture within rooms). Controlled by `EPCGHiGenGrid` and grid size settings on the graph.

### GPU Execution (UE 5.7)

PCG 5.7 adds GPU compute support via `UPCGComputeGraph`. Custom HLSL nodes can run on GPU for massive parallelism. GPU parameter overrides, FastGeo support, and multi-platform GPU compute. Performance is roughly 2x vs CPU for compatible operations. Not all nodes support GPU -- primarily point operations and attribute math.

---

## 2. What PCG is Good At

PCG excels at **point-based spatial distribution** -- deciding WHERE to place EXISTING assets. This is its core strength.

### Scatter/Placement

- **Surface sampling**: Generate thousands of points on landscape with configurable density
- **Rule-based filtering**: Filter by slope, height, distance, physical material, attributes
- **Density maps**: Use textures/render targets as density masks
- **Exclusion zones**: Difference operations remove points near roads, buildings, etc.
- **Self-pruning**: Prevent overlapping spawns via bounds checking
- **Noise-based variation**: Perlin/Voronoi/FBM noise for natural distribution

### Instancing Performance

PCG automatically outputs to **ISM/HISM** (Instanced Static Mesh / Hierarchical ISM) components. One draw call per unique mesh type, regardless of instance count. Combined with Nanite, this handles massive scenes. Note: UE5 ISM performance degrades above ~10,000 instances per component (regression from UE4), but Nanite meshes don't use traditional ISM.

### Spline-Based Placement

- Sample points along splines (roads, rivers, fences, walls)
- Spawn spline mesh components for deformable assets
- Spline intersection/splitting operators (new in 5.7)
- Landscape spline integration

### Actor Spawning

- `SpawnActor` node places full Blueprint actors at point locations
- Attributes can drive which actor class to spawn
- Parameters can be passed from PCG graph to spawned actors
- Combined with Blueprint actors, this is the bridge to GeometryScript

### What This Means for Building Generation

PCG is excellent for:
- **Placing pre-made building actors** on lots in a city block
- **Scattering furniture/props** inside rooms (collision-aware via bounds)
- **Distributing debris/decals** on surfaces
- **Placing street furniture** along roads (lampposts, mailboxes, signs)
- **Spawning modular wall pieces** along a spline (fence generator pattern)
- **Biome-aware vegetation** around buildings

---

## 3. PCG for Modular Building Assembly

### The Grid Snapping Approach

Several community tutorials demonstrate using PCG for modular building assembly:

1. **Define building footprint** as a grid of points (e.g., `CreatePointsGrid`)
2. **Tag points** with attributes (wall, floor, ceiling, corner, door, window)
3. **Use attribute-based mesh selection** to spawn the right modular piece at each point
4. **Grammar rules** determine which piece goes where based on neighbors

This works for **modular kit assembly** -- snapping pre-made mesh pieces together on a uniform grid. Common grid snapping at 10cm or 5cm increments.

### PCG Grammar (Shape Grammar)

UE 5.5+ includes a **Grammar** system loosely inspired by CGA (Computer Generated Architecture):
- Define **grammar modules** -- named rule sets
- Grammar strings are parsed and executed
- Supports sequences, choices (weighted random), repetitions, and fallbacks
- Can split a facade into floors, then floors into bays, then bays into windows
- Community tutorials show building facade generation and tile-by-tile roof generation

Limitations of PCG Grammar:
- Less powerful than full CGA (no arithmetic split, no component splits along normals)
- Operates on point data, not directly on geometry
- Good for deciding WHAT to place WHERE, not for creating geometry

### Custom PCG Nodes in C++

Creating custom nodes for our specific needs:

```cpp
// Settings class (defines node appearance and parameters)
UCLASS()
class UMyPCGSettings : public UPCGSettings {
    GENERATED_BODY()
public:
    FName GetDefaultNodeTitle() const override;
    TArray<FPCGPinProperties> InputPinProperties() const override;
    TArray<FPCGPinProperties> OutputPinProperties() const override;
    FPCGElementPtr CreateElement() const override;

    UPROPERTY(EditAnywhere, meta=(PCG_Overridable))
    float MyParam = 1.0f;
};

// Element class (implements execution logic)
class FMyPCGElement : public IPCGElement {
protected:
    bool ExecuteInternal(FPCGContext* Context) const override;
};
```

Execution flow:
1. `FPCGContext` provides input data, pin connections, settings
2. Query inputs by pin name from `Context->InputData.GetInputsByPin(PinName)`
3. Cast to specific types (UPCGPointData, UPCGSplineData, etc.)
4. Process and create output data
5. Append to `Context->OutputData.TaggedData`

Module dependency: Add `"PCG"` to PublicDependencyModuleNames.

### Feeding Floor Plan Data INTO PCG

This is the key question. **Can we pass our room grid into PCG?**

**Yes, via several mechanisms:**
1. **UPCGPointData as input**: Create a UPCGPointData programmatically with one point per grid cell, tagging each with room type, wall flags, etc. Feed into graph via component input.
2. **User Parameters**: Pass building descriptor as struct parameters to the graph. Graph nodes interpret the parameters to generate the right points.
3. **Custom Input Node**: Write a custom C++ node that reads our spatial registry / building descriptor and outputs tagged point data.
4. **DataTable input**: Store room layouts in DataTables, use `DataTableRowToParamData` to load them.
5. **Attribute Set input**: Use `PCGParamData` (attribute sets without spatial data) to pass configuration.

### Subgraphs for Composition

PCG supports subgraphs (reusable graph components):
- One subgraph for wall placement
- One for floor placement
- One for furniture scatter
- One for door/window insertion
- Loop subgraphs for iterating per-room or per-floor

Parameters can be overridden per-subgraph instance.

---

## 4. What PCG Cannot Do (Limitations)

### Fundamental Limitations

1. **PCG CANNOT generate mesh geometry.** It is a placement/distribution engine. It outputs spawn commands for existing assets -- it does not create new StaticMesh assets, perform boolean operations, or modify mesh topology. No CSG, no extrusion, no mesh merging.

2. **PCG cannot sample ProceduralMeshComponent** -- only StaticMeshComponents work with Surface Sampler. This means you can't scatter ON procedural meshes easily.

3. **No room subdivision algorithms.** PCG has no built-in BSP, treemap, graph-based room partitioning, or constraint solving. It processes point clouds, not spatial graphs.

4. **No vertical connectivity logic.** PCG doesn't understand "floors" or "stairs" -- it doesn't know that rooms on different levels need to connect. Multi-floor buildings require custom logic.

5. **No adjacency awareness.** Stock PCG has no notion of "room A must be adjacent to room B" or "bathrooms cluster on wet walls." PCGEx adds some graph/topology tools, but they're not room-layout-specific.

6. **No collision/physics resolution.** PCG uses bounds-based self-pruning for overlap avoidance, but has no physics simulation for settling or complex collision avoidance.

7. **No mesh texturing pipeline.** As noted by community: "PCG is good, although it doesn't really have the 'build and texture the meshes' pipeline down."

8. **Limited architectural intelligence.** PCG doesn't understand building codes, door clearances, structural integrity, or spatial relationships beyond what you explicitly program via custom nodes.

### Performance Limitations

- **ISM regression**: UE5 ISM performance degrades above ~10,000 instances (worse than UE4 at 80,000). Nanite mitigates this for compatible meshes.
- **Attribute Partition is slow**: The node that splits points by attribute value is significantly slower than ungrouped operations. Union back ASAP.
- **Electric Dreams ran at 30fps on RTX 3080 at 1080p** -- PCG scenes can be expensive.

### Compared to Houdini

From community consensus:
- Houdini is far ahead for building/infrastructure generation
- PCG's focus has been on "landscape and nature, while the complex topic of buildings, infrastructure and cities has been approached rather cautiously"
- Houdini handles the complete pipeline from asset creation through texturing
- However, Houdini requires import/export and is slow for iteration; PCG is real-time

---

## 5. PCG + GeometryScript Hybrid

### The Bridge: Blueprint Actors with PCG Components

The key integration pattern:

1. **GeometryScript** creates modular mesh pieces (walls, floors, trim, etc.) as StaticMesh assets or DynamicMeshActors
2. **PCG** places those pieces according to rules (spawn actors at grid positions)
3. PCG's `SpawnActor` node places Blueprint actors that contain GeometryScript logic
4. The spawned actor's Construction Script runs GeometryScript to generate its specific mesh

### Community Example: bendemott/UE5-Procedural-Building

An open-source project on GitHub demonstrates this pattern:
- `ADynamicBuilding` actor uses GeometryScript C++ to generate building geometry
- Can be spawned/placed procedurally
- Requires `GeometryScriptingEditor`, `GeometryScriptingCore`, `GeometryCore` modules

### PCG + GeometryScript Wall Generator

A community tutorial for UE 5.5 demonstrates a wall generator that:
1. Uses PCG to generate points along a spline (wall path)
2. At each point, spawns a Blueprint actor
3. The Blueprint actor uses GeometryScript to create the wall mesh segment
4. Parameters from PCG attributes control wall height, thickness, openings

### What This Means for Monolith

The hybrid approach is viable but architecturally redundant for our case:
- **We already have GeometryScript-based building generation** (create_structure, create_building, facades, etc.)
- **We already have our own placement logic** (spatial registry, city block layout)
- Adding PCG as a middleman between our layout algorithm and our mesh generation adds complexity without clear benefit
- PCG's value proposition (point-based distribution) overlaps with what we already do

Where PCG WOULD add value:
- **Furniture/prop scatter inside rooms** -- PCG's surface sampling + exclusion zones + density filtering is purpose-built for this
- **Street-level detail** (signs, trash, puddles, lights) -- scatter along splines
- **Vegetation around buildings** -- PCG's core strength
- **Debris/horror props** -- density maps for controlled randomness

---

## 6. PCG C++ API for MCP Integration

### Programmatic Graph Creation

From the 5.7 engine source, graphs CAN be created entirely from C++:

```cpp
// Create graph asset
UPCGGraph* Graph = NewObject<UPCGGraph>(GetTransientPackage());

// Add nodes
UPCGSettings* SamplerSettings;
UPCGNode* SamplerNode = Graph->AddNodeOfType(UPCGSurfaceSamplerSettings::StaticClass(), SamplerSettings);

UPCGSettings* SpawnerSettings;
UPCGNode* SpawnerNode = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass(), SpawnerSettings);

// Connect nodes
Graph->AddEdge(SamplerNode, PCGPinConstants::DefaultOutputLabel,
               SpawnerNode, PCGPinConstants::DefaultInputLabel);

// Set parameters on settings
SamplerSettings->PointsPerSquaredMeter = 0.5f;
```

### Programmatic Generation

```cpp
// Get subsystem
UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World);

// Create component on an actor
UPCGComponent* PCGComp = NewObject<UPCGComponent>(TargetActor);
PCGComp->SetGraph(Graph);
PCGComp->Seed = 12345;
PCGComp->RegisterComponent();

// Trigger generation
PCGComp->GenerateLocal(/*bForce=*/true);

// OR schedule via subsystem
FPCGTaskId TaskId = Subsystem->ScheduleComponent(PCGComp, EPCGHiGenGrid::Uninitialized, true, {});
```

### Event Hooks (from source)

Delegates for generation lifecycle:
```cpp
// Native delegates
FOnPCGGraphStartGenerating OnPCGGraphStartGeneratingDelegate;  // Generation begins
FOnPCGGraphGenerated OnPCGGraphGeneratedDelegate;              // Generation complete
FOnPCGGraphCancelled OnPCGGraphCancelledDelegate;              // Generation cancelled
FOnPCGGraphCleaned OnPCGGraphCleanedDelegate;                  // Cleanup complete

// Blueprint-accessible dynamic delegates
FOnPCGGraphGeneratedExternal OnGenerationDoneExternal;
```

### Parameter Passing

```cpp
// Set graph parameters programmatically
UPCGGraphInterface* GraphInterface = PCGComp->GetGraphInstance();
GraphInterface->SetGraphParameter<int32>(FName("RoomCount"), 5);
GraphInterface->SetGraphParameter<float>(FName("WallHeight"), 300.0f);
GraphInterface->SetGraphParameter<FString>(FName("BuildingType"), TEXT("Hospital"));
```

### Retrieving Results

```cpp
// After generation completes
const FPCGDataCollection& Output = PCGComp->GetGeneratedGraphOutput();
for (const FPCGTaggedData& TaggedData : Output.TaggedData) {
    if (UPCGPointData* PointData = Cast<UPCGPointData>(TaggedData.Data)) {
        const TArray<FPCGPoint>& Points = PointData->GetPoints();
        // Process points...
    }
}
```

### MCP Integration Feasibility

All APIs above are accessible from our MCP C++ module. Potential actions:
- `pcg_create_graph` -- create/configure a PCG graph programmatically
- `pcg_set_parameters` -- set graph user parameters
- `pcg_generate` -- trigger generation and return results
- `pcg_cleanup` -- clean up generated content
- `pcg_spawn_scatter` -- high-level: scatter meshes on a surface with PCG

---

## 7. PCG Plugins and Extensions

### PCGEx (PCG Extended Toolkit)

**GitHub:** github.com/PCGEx/PCGExtendedToolkit
**License:** MIT (commercially safe)
**Stars:** 599+
**Status:** Production-ready, actively maintained, featured on Inside Unreal

200+ nodes filling gaps in vanilla PCG:

| Category | Features |
|----------|----------|
| **Clusters** | Delaunay, Voronoi, convex hulls, MST, custom graph builders |
| **Pathfinding** | A*, Dijkstra, Bellman-Ford with composable heuristics |
| **Paths** | Offset, bevel, smooth, subdivide, split, stitch, solidify |
| **Filters** | Universal composable filters (AND/OR, attribute tests, spatial, bitmask) |
| **Staging** | Weighted asset collections, fitting, material variations |
| **Sampling** | Nearest point, surface, spline, bounds, textures |
| **Tensors** | Directional flow fields for orientation |
| **Topology** | Clusters/paths to triangulated mesh surfaces, boundary detection, flood fill, island analysis |
| **Valency** | **WFC-style constraint solving on cluster topology** |
| **Transform** | Point fusion, blending, sorting, partitioning, bitmasks, attribute math |
| **Shapes** | Parametric generation, seed-adaptive, resolution-aware |

**Critical for building generation:** The Valency system provides WFC-style constraint solving directly within PCG. This could handle modular piece adjacency rules (which wall tile goes next to which).

### PCG Biome Core Plugin (Official)

Ships with UE 5.7. Demonstrates:
- Attribute Set Tables for biome configuration
- Feedback loops
- Recursive sub-graphs
- Runtime features

### PCG Layered Biomes (Community)

**GitHub:** github.com/lazycatsdev/PCGLayeredBiomes
- Divides generation into biome regions with separate PCG logic per layer
- Each layer references previously generated geometry

### WFC Plugin

**GitHub:** github.com/bohdon/WFCPlugin
- Standalone WFC for UE5
- `UWFCGenerator` with configurable model, grid, constraints, cell selectors
- Entropy-based cell selection
- Could complement PCG for room layout, then hand off to PCG for detail

### Dungeon Architect

**Site:** dungeonarchitect.dev
- Commercial plugin with PCG framework integration
- Grid Flow Builder for dungeon flow (key-lock, teleporters, elevation)
- Snap Builder with Graph Grammar rule editor
- Supports UE5 Lumen/Nanite
- Most relevant existing product for building interiors

### Massive World (Fab)

- Commercial tool for full world generation via PCG
- Landscape Stamps + PCG for non-destructive biome workflows
- City/town level, not individual building interiors

---

## 8. Real-World PCG Building Examples

### LEGO Fortnite (Epic Games, shipped)

- Uses PCG with `GenerateAtRuntime` for procedural island generation
- Seed-based deterministic worlds
- Runtime generation/cleanup based on player proximity via `FPCGRuntimeGenScheduler`
- Focuses on terrain and biome placement, not building interiors

### Electric Dreams (Official Sample Project)

- 4km x 4km procedurally generated environment
- Demonstrates PCG Assemblies, spline-based generation, cliff structures
- All vegetation/foliage via PCG
- Performance: ~30fps at 1080p on RTX 3080 (heavy scene)
- No building interiors

### City Generation Tutorials

- **Daniel Mor (Virtuos) at Unreal Fest Bali 2025**: Full city generation walkthrough using PCG
- **PCG City Buildings (Fab)**: 40 spline-driven building exteriors
- **Jean-Paul Software "Manhattan to Berlin"**: PCG + PCGEx for city streets, using point data for lot placement
- All focus on **building exteriors and street layout**, not interiors

### Modular Building Tutorials

- **UE5 PCG Modular Building Tutorial**: Full modular structure assembly using PCG Graph
  - PCG Graph fundamentals for buildings
  - Full modular structures with walls, ceilings
  - Box colliders for door/window placement
  - Grid-based assembly
- **PCG Grammar Building Generator**: Grammar-based facade generation
- **Tile-by-Tile Roof Generation**: PCG Grammar for roof tile assembly

### Dungeon Generation with PCG

- **UE 5.6 Tutorial**: PCG functions for dungeon layouts -- room setup, exits, modular structure generation
- **PCG Dungeon Level Builder (WIP)**: Community project using PCG for dungeon rooms
- **Shooter Tutorial**: Procedural level using only PCG

### Building Interiors

**No comprehensive PCG-based building interior generation examples exist in the community.** All examples either:
1. Use PCG for exterior placement/scatter only
2. Use modular piece assembly for single-room or corridor layouts
3. Rely on Dungeon Architect or custom solutions for multi-room interiors

This is consistent with the observation that "the complex topic of buildings, infrastructure and cities has been approached rather cautiously" in PCG.

---

## 9. Verdict: Should Monolith Add a PCG Module?

### Assessment Matrix

| Capability | PCG Can Do It? | Monolith Already Does It? | PCG Better? |
|-----------|---------------|--------------------------|-------------|
| Room layout generation | No (needs custom) | Yes (grid-based) | No |
| Wall/floor mesh generation | No (placement only) | Yes (GeometryScript) | No |
| Boolean operations (doors/windows) | No | Yes (GeometryScript) | No |
| Multi-floor buildings | No (needs custom) | Yes (create_building) | No |
| Furniture scatter | Yes (excellent) | Partial (collision_aware_scatter) | **YES** |
| Exterior prop placement | Yes (excellent) | Partial (surface_scatter) | **YES** |
| Vegetation around buildings | Yes (core strength) | No | **YES** |
| Street furniture along roads | Yes (spline-based) | No | **YES** |
| Debris/horror atmosphere props | Yes (density maps) | Partial (disturbance_field) | **YES** |
| Modular piece assembly | Yes (with grammar) | Yes (sweep walls, modular) | Comparable |
| City block lot placement | Possible (with PCGEx) | Yes (city block layout) | No |
| Runtime generation | Yes (built-in) | No (editor-time only) | **YES** |
| Deterministic seeds | Yes (native) | Partial (per-action) | **YES** |
| GPU acceleration | Yes (5.7) | No | **YES** |

### Recommendation: SELECTIVE Integration, Not Wholesale Replacement

**Do NOT** replace our building generation pipeline with PCG. Our GeometryScript-based system (create_structure, create_building, facades, spatial registry) is fundamentally better suited for building interiors because:
1. We need **mesh generation**, not just placement
2. We need **boolean operations** for doors/windows
3. We need **multi-floor connectivity** with stairs
4. We need **adjacency-aware room layouts**
5. PCG adds a layer of indirection that would slow our MCP workflow

**DO** add PCG integration for what it excels at:

#### Phase 1: Detail Scatter (~20-28h)
- `pcg_scatter_interior` -- scatter furniture/props inside rooms using PCG's surface sampling + exclusion zones + density filtering. Feed room bounds and exclusion data from spatial registry as input points.
- `pcg_scatter_exterior` -- scatter debris, vegetation, street furniture around buildings
- `pcg_scatter_along_spline` -- place objects along roads/paths

#### Phase 2: Atmosphere Generation (~16-24h)
- `pcg_horror_atmosphere` -- PCG graph with density maps for horror debris (blood splatters, broken glass, scattered papers)
- `pcg_decay_scatter` -- controlled deterioration: peeling wallpaper points, mold patches, cobwebs
- Integration with our existing `disturbance_field` and `horror_composition` actions

#### Phase 3: Runtime Support (~24-32h)
- `pcg_generate_runtime` -- configure a PCG component for runtime generation (useful for infinite corridors, procedural rooms during gameplay)
- `pcg_configure_component` -- set up PCG component with graph, parameters, generation trigger
- `pcg_create_graph` -- programmatically create simple PCG graphs from MCP

#### Phase 4: PCGEx Integration (~16-20h)
- Require PCGEx as optional dependency (MIT license, safe)
- Leverage Valency (WFC) for modular piece assembly rules
- Use graph/pathfinding for building connectivity validation

### Estimated Total: ~76-104h across 4 phases

### Architecture for MCP Integration

```
MCP Action                  Internal Flow
-----------                 -------------
pcg_scatter_interior  -->   Create UPCGPointData from room bounds
                            Configure scatter graph (surface sample + filter + spawn)
                            Set parameters (density, mesh list, seed)
                            Generate via UPCGComponent::GenerateLocal()
                            Listen for OnPCGGraphGeneratedDelegate
                            Return spawned actor info

pcg_create_graph      -->   Create UPCGGraph programmatically
                            Add nodes via AddNodeOfType()
                            Connect via AddEdge()
                            Set user parameters
                            Save as asset
                            Return graph path

pcg_generate          -->   Find/create PCGComponent on target actor
                            Set graph, parameters, seed
                            GenerateLocal(true)
                            Await completion delegate
                            Return generated output data
```

### Key Files/Modules to Depend On

```cpp
// Build.cs
PublicDependencyModuleNames.AddRange(new string[] { "PCG" });
// Optional: PCGEx if installed
```

Headers:
- `PCGComponent.h` -- UPCGComponent
- `PCGGraph.h` -- UPCGGraph, UPCGGraphInterface
- `PCGSubsystem.h` -- UPCGSubsystem
- `PCGPoint.h` -- FPCGPoint
- `PCGData.h` -- UPCGData base
- `Data/PCGPointData.h` -- UPCGPointData
- `PCGSettings.h` -- UPCGSettings (for custom nodes)
- `PCGElement.h` -- IPCGElement (for custom node execution)
- `PCGContext.h` -- FPCGContext

---

## 10. Sources

### Official Documentation
- [PCG Overview (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [PCG Node Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG Data Types Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-data-types-reference-in-unreal-engine)
- [PCG Generation Modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine)
- [PCG Shape Grammar](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine)
- [PCG GPU Processing](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)
- [PCG Development Guides](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [PCG Biome Plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-in-unreal-engine)
- [UPCGGraph API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGGraph)
- [UPCGComponent API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGComponent)
- [UPCGSubsystem API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGSubsystem)
- [Electric Dreams Sample](https://www.unrealengine.com/en-US/electric-dreams-environment)
- [Point Properties Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/point-properties)

### Tutorials and Community
- [How to Create Custom PCG Nodes in Unreal Engine (Blueshift)](https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/)
- [PCG Modular Building Tutorial](https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation)
- [PCG Grammar Building Generator](https://dev.epicgames.com/community/learning/tutorials/9d3a/unreal-engine-pcg-grammer-tutorial-procedural-building-generator)
- [PCG Grammar Introduction (UE 5.5)](https://dev.epicgames.com/community/learning/tutorials/PYEX/introduction-to-pcg-grammar-in-unreal-engine-5-5)
- [Advanced PCG Grammar - Roof Generation](https://dev.epicgames.com/community/learning/tutorials/nzVe/unreal-engine-5-5-advanced-pcg-grammar-tutorial-tile-by-tile-building-roof-generation)
- [Leveraging PCG for Building and City Creation (Unreal Fest)](https://dev.epicgames.com/community/learning/talks-and-demos/Z1wa/unreal-engine-leveraging-pcg-for-building-and-city-creation)
- [PCG + GeometryScript Wall Generator](https://forums.unrealengine.com/t/community-tutorial-pcg-geometry-script-in-ue-5-5-wall-generator/2118674)
- [Procedural Towns with PCG and Cargo](https://dev.epicgames.com/community/learning/tutorials/dXR7/unreal-engine-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo)
- [PCG Dungeon Layouts (UE 5.6)](https://forums.unrealengine.com/t/community-tutorial-designing-procedural-dungeon-layouts-with-pcg-functions-in-unreal-engine-5-6/2680144)
- [First We Make Manhattan (Jean-Paul Software)](https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/)
- [Massive Worlds with PCG (Zack Sinisi)](https://zacksinisi.com/generating-massive-worlds-with-pcg-framework-for-unreal-engine-5/)
- [A Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [PCG City Streets Tutorial](https://dev.epicgames.com/community/learning/tutorials/VxP9/unreal-engine-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg)
- [Testing PCG at Runtime (80.lv)](https://80.lv/articles/testing-out-ue5-s-procedural-content-generation-framework-at-runtime)
- [PCG in UE5.6/5.7 (80.lv)](https://80.lv/articles/what-s-new-in-pcg-in-ue5-6-ue5-7)
- [Can PCG Replace Houdini? (Forum)](https://forums.unrealengine.com/t/can-pcg-replace-houdini/1231383)
- [PCG Grammar Generator Tool](https://forums.unrealengine.com/t/created-unreal-engine-pcg-grammar-generator-tool/2531546)

### Plugins and Extensions
- [PCGEx (PCG Extended Toolkit)](https://github.com/PCGEx/PCGExtendedToolkit) -- MIT, 200+ nodes
- [PCGEx Documentation](https://pcgex.gitbook.io/pcgex)
- [PCGEx on Fab](https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624)
- [Inside Unreal: PCGEx](https://forums.unrealengine.com/t/inside-unreal-taking-pcg-to-the-extreme-with-the-pcgex-plugin/2479952)
- [PCG Layered Biomes](https://github.com/lazycatsdev/PCGLayeredBiomes)
- [WFC Plugin for UE5](https://github.com/bohdon/WFCPlugin)
- [Dungeon Architect](https://dungeonarchitect.dev/)
- [bendemott/UE5-Procedural-Building](https://github.com/bendemott/UE5-Procedural-Building) -- GeometryScript C++ buildings

### UE 5.7 Release Coverage
- [UE 5.7 PCG Production-Ready (DigitalProduction)](https://digitalproduction.com/2025/10/17/unreal-5-7-preview-pcg-grows-up-foliage-gets-fancy/)
- [UE 5.7 Release Notes](https://www.unrealengine.com/en-US/news/unreal-engine-5-7-is-now-available)
- [UE 5.7 Performance Improvements (Tom's Hardware)](https://www.tomshardware.com/video-games/pc-gaming/unreal-engine-5-7-brings-significant-improvements-over-the-notoriously-demanding-5-4-version-tester-claims-benchmark-shows-up-to-25-percent-gpu-performance-increase-35-percent-cpu-boost)

### Engine Source (Local, UE 5.7)
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/PCGComponent.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/PCGGraph.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/Subsystems/PCGSubsystem.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/PCGPoint.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/PCGData.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/PCGSettings.h`
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/Data/*.h` (full data type hierarchy)
- `C:/Program Files (x86)/UE_5.7/Engine/Plugins/PCG/Source/PCG/Public/Elements/*.h` (~100+ node implementations)
