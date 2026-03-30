# MonolithPCG Module Design -- PCG Integration for Monolith MCP

**Date:** 2026-03-28
**Status:** Research Complete
**Estimated effort:** ~80-110h (5 phases)

---

## 1. Executive Summary

UE 5.7's PCG framework is production-ready with a rich C++ API that is fully suitable for programmatic graph construction, node configuration, and execution triggering. The API surface is large and well-structured: `UPCGGraph` has direct methods for `AddNode`, `AddEdge`, `RemoveNode`; `UPCGSettings` subclasses define all built-in node types; `UPCGComponent` provides `GenerateLocal()` / `CleanupLocal()` for execution control; and `UPCGSubsystem` offers lower-level `ScheduleGraph` / `ScheduleGeneric` for fine-grained job management.

A MonolithPCG module is highly feasible and would complement our existing GeometryScript-based procedural generation with a visual, reusable, data-driven scatter/placement system. The recommended approach is a **graph construction + execution bridge** (not custom PCG nodes), keeping Monolith's role as an MCP orchestrator rather than embedding logic inside PCG graphs.

---

## 2. PCG C++ API Surface (UE 5.7)

### 2.1 Core Classes

**Source locations:** `Engine/Plugins/PCG/Source/PCG/Public/`

| Class | Header | Role |
|-------|--------|------|
| `UPCGGraph` | `PCGGraph.h` | The graph asset. Contains nodes, edges, user parameters. |
| `UPCGGraphInterface` | `PCGGraph.h` | Abstract base for graph + graph instances. |
| `UPCGGraphInstance` | `PCGGraphInstance.h` | An instance of a graph with parameter overrides. |
| `UPCGNode` | `PCGNode.h` | A node in the graph. Owns a `UPCGSettingsInterface`. |
| `UPCGSettings` | `PCGSettings.h` | Abstract base for all node logic/configuration. |
| `UPCGSettingsInterface` | `PCGSettings.h` | Settings or instance-of-settings wrapper. |
| `UPCGComponent` | `PCGComponent.h` | Actor component that drives generation. |
| `UPCGSubsystem` | `Subsystems/PCGSubsystem.h` | World subsystem for scheduling/execution. |
| `APCGVolume` | `PCGVolume.h` | Volume actor with a `UPCGComponent`. |
| `UPCGData` | `PCGData.h` | Base class for all PCG data types. |
| `UPCGParamData` | `PCGParamData.h` | Attribute set data (key-value parameters). |
| `UPCGPointData` | `Data/PCGPointData.h` | Point cloud data with spatial + metadata. |
| `UPCGSpatialData` | `Data/PCGSpatialData.h` | Abstract spatial data base. |
| `FPCGDataCollection` | `PCGData.h` | Tagged collection of data flowing through pins. |
| `IPCGElement` | `PCGElement.h` | Execution element interface (the actual "do work" part). |

### 2.2 Graph Construction API

From `UPCGGraph` (all `BlueprintCallable`, all `PCG_API`):

```cpp
// Add nodes
UPCGNode* AddNode(UPCGSettingsInterface* InSettings);
UPCGNode* AddNodeOfType(TSubclassOf<UPCGSettings> InSettingsClass, UPCGSettings*& DefaultNodeSettings);
template<typename T> UPCGNode* AddNodeOfType(T*& DefaultNodeSettings);
UPCGNode* AddNodeInstance(UPCGSettings* InSettings);
UPCGNode* AddNodeCopy(const UPCGSettings* InSettings, UPCGSettings*& DefaultNodeSettings);

// Remove nodes
void RemoveNode(UPCGNode* InNode);
void RemoveNodes(TArray<UPCGNode*>& InNodes);

// Edges
UPCGNode* AddEdge(UPCGNode* From, const FName& FromPinLabel, UPCGNode* To, const FName& ToPinLabel);
bool RemoveEdge(UPCGNode* From, const FName& FromLabel, UPCGNode* To, const FName& ToLabel);

// Query
UPCGNode* GetInputNode() const;
UPCGNode* GetOutputNode() const;
const TArray<UPCGNode*>& GetNodes() const;
UPCGNode* FindNodeWithSettings(const UPCGSettingsInterface* InSettings, bool bRecursive = false) const;
UPCGNode* FindNodeByTitleName(FName NodeTitle, bool bRecursive = false, TSubclassOf<const UPCGSettings> OptionalClass = {}) const;
bool Contains(UPCGNode* Node) const;

// Iteration
bool ForEachNode(TFunctionRef<bool(UPCGNode*)> Action) const;
bool ForEachNodeRecursively(TFunctionRef<bool(UPCGNode*)> Action) const;
```

From `UPCGNode`:

```cpp
UPCGNode* AddEdgeTo(FName FromPinLabel, UPCGNode* To, FName ToPinLabel);
bool RemoveEdgeTo(FName FromPinLabel, UPCGNode* To, FName ToPinLabel);
UPCGSettings* GetSettings() const;
void SetSettingsInterface(UPCGSettingsInterface* InSettingsInterface, bool bUpdatePins = true);
UPCGPin* GetInputPin(const FName& Label);
UPCGPin* GetOutputPin(const FName& Label);
```

### 2.3 User Parameters API

From `UPCGGraph` / `UPCGGraphInterface`:

```cpp
// User parameters = FInstancedPropertyBag (StructUtils)
const FInstancedPropertyBag* GetUserParametersStruct() const;

// Template getters/setters
template<typename T> TValueOrError<T, EPropertyBagResult> GetGraphParameter(FName PropertyName) const;
template<typename T> EPropertyBagResult SetGraphParameter(FName PropertyName, const T& Value);

// Adding parameters (from UPCGGraph)
void AddUserParameters(const TArray<FPropertyBagPropertyDesc>& InDescs, const UPCGGraph* OriginalGraph = nullptr);
```

### 2.4 Execution API

**UPCGComponent** (the primary way to trigger generation):

```cpp
void Generate();                            // Networked, activates component
void GenerateLocal(bool bForce);            // Local, delayed
FPCGTaskId GenerateLocalGetTaskId(bool bForce);  // Returns task ID for tracking
void CleanupLocal(bool bRemoveComponents);
void CancelGeneration();
void SetGraph(UPCGGraphInterface* InGraph);  // Assign graph to component
const FPCGDataCollection& GetGeneratedGraphOutput() const;  // Get results
```

**UPCGSubsystem** (lower-level scheduling):

```cpp
static UPCGSubsystem* GetInstance(UWorld* World);
static UPCGSubsystem* GetActiveEditorInstance();  // Editor only

FPCGTaskId ScheduleComponent(UPCGComponent* PCGComponent, EPCGHiGenGrid Grid, bool bForce, const TArray<FPCGTaskId>& InDependencies);
FPCGTaskId ScheduleGraph(UPCGGraph* Graph, UPCGComponent* SourceComponent, FPCGElementPtr PreGraphElement, FPCGElementPtr InputElement, const TArray<FPCGTaskId>& Dependencies, const FPCGStack* InFromStack, bool bAllowHierarchicalGeneration);
FPCGTaskId ScheduleGeneric(TFunction<bool()> InOperation, UPCGComponent* SourceComponent, const TArray<FPCGTaskId>& TaskExecutionDependencies);
FPCGTaskId ScheduleRefresh(UPCGComponent* SourceComponent, bool bForceRefresh);  // Editor only
```

### 2.5 Built-in Node Settings Classes

All in `Elements/` subdirectory. Key ones for our use cases:

| Settings Class | Purpose |
|---|---|
| `UPCGSurfaceSamplerSettings` | Sample points on surfaces |
| `UPCGStaticMeshSpawnerSettings` | Spawn static meshes from points |
| `UPCGSpawnActorSettings` | Spawn actors from points |
| `UPCGDensityFilterSettings` | Filter points by density |
| `UPCGAttributeFilterSettings` | Filter by attribute value |
| `UPCGCopyPointsSettings` | Copy points onto target |
| `UPCGPointSamplerSettings` | Sample subset of points |
| `UPCGBoundsModifierSettings` | Modify point bounds |
| `UPCGCreatePointsSettings` | Create points programmatically |
| `UPCGCreateSplineSettings` | Create splines |
| `UPCGTransformPointsSettings` | Transform point positions |
| `UPCGDistanceSettings` | Distance-based filtering |
| `UPCGSelfPruningSettings` | Remove overlapping points |
| `UPCGDataFromActorSettings` | Get data from actors |
| `UPCGSubdivideSegmentSettings` | Grammar: subdivide segments |
| `UPCGSubdivideSplineSettings` | Grammar: subdivide splines |
| `UPCGSelectGrammarSettings` | Grammar: select by rule |
| `UPCGCreateCollisionDataSettings` | Create collision shapes |
| `UPCGDynamicMeshData` | Dynamic mesh data type (5.7) |

### 2.6 Module Dependencies

For `Build.cs`, based on analysis of PCG's own modules:

```csharp
// Required: Runtime PCG module
PublicDependencyModuleNames.Add("PCG");

// Optional: Editor-only PCG features (graph editing UI callbacks)
if (Target.bBuildEditor)
{
    PrivateDependencyModuleNames.Add("PCGEditor");
}
```

The PCG module itself depends on: `ComputeFramework`, `Core`, `CoreUObject`, `DeveloperSettings`, `Engine`, `Foliage`, `GeometryAlgorithms`, `GeometryCore`, `GeometryFramework`, `Landscape`, `PhysicsCore`, `Projects`, `RenderCore`, `RHI`.

**Plugin dependency** in `.uplugin`:
```json
{
    "Name": "PCG",
    "Enabled": true,
    "Optional": true
}
```

Making it optional with a `WITH_PCG` define (same pattern as our `WITH_GEOMETRYSCRIPT`) is recommended since not all users will want PCG.

---

## 3. Custom PCG Nodes in C++

### 3.1 The UPCGSettings Subclass Pattern

Every PCG node consists of two parts:
1. **Settings** (`UPCGSettings` subclass) -- configuration, pins, metadata
2. **Element** (`IPCGElement` subclass) -- execution logic

```cpp
// --- Settings ---
UCLASS(BlueprintType, ClassGroup = (Procedural))
class UMyCustomPCGSettings : public UPCGSettings
{
    GENERATED_BODY()
public:
#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return FName(TEXT("MyCustomNode")); }
    virtual FText GetDefaultNodeTitle() const override { return NSLOCTEXT("PCG", "MyNode", "My Custom Node"); }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Generic; }
    virtual FLinearColor GetNodeTitleColor() const override { return FLinearColor(0.1f, 0.8f, 0.2f); }
#endif

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override
    {
        TArray<FPCGPinProperties> Pins;
        Pins.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial);
        return Pins;
    }

    virtual TArray<FPCGPinProperties> OutputPinProperties() const override
    {
        TArray<FPCGPinProperties> Pins;
        Pins.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Point);
        return Pins;
    }

    virtual FPCGElementPtr CreateElement() const override;

public:
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = Settings)
    float MyParameter = 1.0f;
};

// --- Element ---
class FMyCustomPCGElement : public IPCGElement
{
protected:
    virtual bool ExecuteInternal(FPCGContext* Context) const override
    {
        const UMyCustomPCGSettings* Settings = Context->GetInputSettings<UMyCustomPCGSettings>();
        check(Settings);

        // Get input data
        TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

        // Process...
        for (const FPCGTaggedData& Input : Inputs)
        {
            // Create output point data
            UPCGPointData* OutData = NewObject<UPCGPointData>();
            // ... populate points ...

            FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
            Output.Data = OutData;
            Output.Pin = PCGPinConstants::DefaultOutputLabel;
        }

        return true;  // Done (return false to continue next tick)
    }
};

// --- CreateElement implementation ---
FPCGElementPtr UMyCustomPCGSettings::CreateElement() const
{
    return MakeShared<FMyCustomPCGElement>();
}
```

### 3.2 Registration

Custom PCG nodes are **auto-discovered** through UE's reflection system. Any `UPCGSettings` subclass compiled into a loaded module automatically appears in the PCG editor's node palette. No manual registration needed -- this is a key advantage over Blueprint-based approaches.

### 3.3 Pin Types (EPCGDataType)

Available pin data types:
- `Any`, `None` -- wildcards
- `Spatial` -- any spatial data
- `Point` -- point cloud
- `PolyLine` / `Spline` -- line/spline data
- `Surface` / `Landscape` -- surface data
- `Volume` -- volume data
- `Param` -- attribute set (key-value)
- `Other` -- custom data
- `Concrete` = `Point | PolyLine | Spline | Surface | Landscape | Volume`

### 3.4 Relevance to MonolithPCG

We have two options:
1. **Bridge-only** (recommended for Phase 1): Monolith actions construct graphs from built-in nodes programmatically. No custom UPCGSettings subclasses.
2. **Custom nodes** (Phase 2+): Create Monolith-specific PCG nodes (e.g., "Read Building Descriptor", "Query Spatial Registry") that can be used in any PCG graph.

---

## 4. PCGEx Architecture Analysis

PCGEx is the gold standard for extending PCG. Key patterns worth adopting:

### 4.1 Module Structure
- `PCGExCore` -- foundational data structures, facades, thread safety
- `PCGExBlending` / `PCGExFilters` / `PCGExElementsClusters` -- domain-specific ops
- `FPCGExtendedToolkitModule` orchestrates startup in dependency order
- All custom settings derive from `UPCGExSettings` (extends `UPCGSettings`)

### 4.2 Extension Patterns
- **Processor Pattern**: Per-input processing with automatic parallelization
- **Factory System**: `Settings -> Factory -> Operation` pipeline for pluggable operations
- **Data Facades**: Type-safe, cached attribute access with thread-safe buffer management
- **Cluster Infrastructure**: Full graph/topology data structures

### 4.3 What We Should Borrow
- The "facade" pattern for wrapping PCG data access in type-safe wrappers
- The processor pattern for point-level parallelism
- Their Build.cs pattern: depend on `PCG` in PublicDependencyModuleNames

### 4.4 What We Should NOT Do
- Don't replicate PCGEx's scope. We're building an MCP bridge, not a PCG extension toolkit.
- Don't make MonolithPCG depend on PCGEx. They're complementary, not dependent.

---

## 5. PCG for Our Use Cases

### 5.1 Furniture Scatter

**Current approach:** `scatter_on_surface` / `collision_aware_scatter` Monolith actions -- imperative C++ placing individual actors with overlap checks.

**PCG approach:** Build a reusable PCG graph:
```
[Surface Sampler] -> [Density Filter] -> [Self Pruning] -> [Static Mesh Spawner]
```
With user parameters for density, mesh list, min distance.

**Verdict:** PCG is better for this. Visual, reusable, artist-tweakable. MCP action would be `create_scatter_graph` that builds a parameterized graph, then `execute_pcg` to run it.

### 5.2 Street Furniture (Spline-Based)

PCG graph:
```
[Spline Sampler] -> [Distance Filter] -> [Attribute Noise (rotation)] -> [Static Mesh Spawner]
```

**Verdict:** Natural fit for PCG. Splines are first-class PCG data types.

### 5.3 Horror Dressing (Debris, Blood, Decay)

PCG graph driven by decay_level parameter:
```
[Get Param: decay_level] -> [Surface Sampler] -> [Density Remap (by decay)] -> [Mesh Spawner]
                                                                              -> [Decal Spawner]
```

**Verdict:** Good fit. PCG's attribute system + density filtering maps naturally to decay levels.

### 5.4 Vegetation (Outdoor)

PCG graph:
```
[Landscape Data] -> [Surface Sampler] -> [Slope Filter] -> [Static Mesh Spawner (grass)]
                                      -> [Distance from Buildings] -> [Mesh Spawner (trees)]
```

**Verdict:** This is PCG's primary design purpose. Best fit of all use cases. GPU generation in 5.7 makes this very performant.

### 5.5 Modular Assembly (Wall/Floor Placement)

This is where PCG is weakest for our needs. PCG operates on point clouds and spatial data, not on structured building descriptors. Our floor-plan-first, grid-based approach (connected rooms, shared walls, stairwells) doesn't map naturally to PCG's scatter-and-filter paradigm.

**Verdict:** Keep modular assembly in our GeometryScript pipeline. PCG could potentially handle detail placement AFTER structure generation (e.g., placing trim, pipes, cables along walls).

---

## 6. Data Flow: Monolith to PCG

### 6.1 Passing Parameters via User Parameters

`UPCGGraph` uses `FInstancedPropertyBag` (from StructUtils) for user parameters:

```cpp
UPCGGraph* Graph = ...; // Our graph

// Add a float parameter
TArray<FPropertyBagPropertyDesc> Descs;
Descs.Add(FPropertyBagPropertyDesc(FName("decay_level"), EPropertyBagPropertyType::Float));
Descs.Add(FPropertyBagPropertyDesc(FName("density"), EPropertyBagPropertyType::Float));
Graph->AddUserParameters(Descs);

// Set values
Graph->SetGraphParameter<float>(FName("decay_level"), 0.7f);
Graph->SetGraphParameter<float>(FName("density"), 2.5f);
```

This is the cleanest bridge. Our MCP `set_pcg_params` action would map JSON key-value pairs to `SetGraphParameter` calls.

### 6.2 Passing Spatial Data via UPCGParamData

For structured data (room bounds, wall segments, etc.), we can create `UPCGParamData` with metadata attributes:

```cpp
UPCGParamData* ParamData = NewObject<UPCGParamData>();
UPCGMetadata* Metadata = ParamData->MutableMetadata();

// Add attributes
FPCGMetadataAttribute<FVector>* PosAttr = Metadata->CreateAttribute<FVector>(FName("position"), FVector::ZeroVector, true, true);
FPCGMetadataAttribute<FVector>* SizeAttr = Metadata->CreateAttribute<FVector>(FName("size"), FVector::OneVector, true, true);
FPCGMetadataAttribute<float>* DecayAttr = Metadata->CreateAttribute<float>(FName("decay"), 0.0f, true, true);

// Add entries
int64 Key = ParamData->FindOrAddMetadataKey(FName("Room_0"));
PosAttr->SetValue(Key, FVector(100, 200, 0));
SizeAttr->SetValue(Key, FVector(500, 400, 300));
DecayAttr->SetValue(Key, 0.6f);
```

### 6.3 Converting Spatial Registry to PCG Point Data

Our spatial registry's room/building data can be converted to `UPCGPointData`:

```cpp
UPCGPointData* PointData = NewObject<UPCGPointData>();
TArray<FPCGPoint>& Points = PointData->GetMutablePoints();

for (const auto& Room : Building.Rooms)
{
    FPCGPoint& Point = Points.Emplace_GetRef();
    Point.Transform = FTransform(FVector(Room.Center));
    Point.SetLocalBounds(FBox(-Room.HalfExtent, Room.HalfExtent));
    Point.Density = Room.DecayLevel;
    Point.Seed = Room.Seed;

    // Custom attributes via metadata
    PointData->Metadata->InitializeOnSet(Point.MetadataEntry);
    PointData->Metadata->SetAttribute<FString>(Point.MetadataEntry, FName("room_type"), Room.Type);
}
```

### 6.4 Bidirectional: PCG Results Back to Spatial Registry

After PCG execution, read results from `UPCGComponent::GetGeneratedGraphOutput()`:

```cpp
const FPCGDataCollection& Output = PCGComponent->GetGeneratedGraphOutput();
TArray<FPCGTaggedData> OutputData = Output.GetInputsByPin(FName("Out"));

for (const FPCGTaggedData& Tagged : OutputData)
{
    if (const UPCGPointData* Points = Cast<UPCGPointData>(Tagged.Data))
    {
        for (const FPCGPoint& Point : Points->GetPoints())
        {
            // Register spawned furniture/props in our spatial registry
            SpatialRegistry.RegisterFurniture(Point.Transform, ...);
        }
    }
}
```

---

## 7. PCG vs Current Monolith Approach

| Aspect | Monolith (Current) | PCG | Winner |
|--------|-------------------|-----|--------|
| Furniture scatter | Manual C++, collision_aware_scatter | Graph-based, visual, reusable | **PCG** |
| Street furniture | Manual spline sampling | Native spline nodes | **PCG** |
| Vegetation | Not implemented | Native + GPU generation | **PCG** |
| Building structure | GeometryScript, floor-plan-first | Not suitable (point-based) | **Monolith** |
| Facades/windows | GeometryScript boolean/extrude | Not suitable | **Monolith** |
| Horror dressing | template_actions presets | Parameterized graphs | **PCG** (slightly) |
| MCP control | Direct, single action | Build graph + execute | **Monolith** (more direct) |
| Artist iteration | Requires MCP round-trips | Visual graph editor | **PCG** |
| Performance | Single-threaded exec | Multi-threaded + GPU | **PCG** |
| Reusability | JSON presets | Graph assets, subgraphs | **PCG** |

**Conclusion:** Both systems coexist. Monolith owns structure generation (buildings, rooms, facades). PCG owns scatter/placement (furniture, vegetation, dressing). MonolithPCG bridges them.

---

## 8. UE 5.7 PCG Status

- **Production-ready** as of 5.7 (was experimental in 5.2-5.5, beta in 5.6)
- **GPU generation** via compute shaders -- nearly 2x performance vs 5.5
- **Grammar nodes** (SubdivideSegment, SubdivideSpline, SelectGrammar, PrintGrammar) -- useful for facade-like splitting
- **FastGeometry components** for high-density spawning without partition actors
- **GPU parameter overrides** for dynamic runtime values
- **DynamicMesh data type** -- can feed into GeometryScript workflows
- **Collision shape data** -- native collision generation nodes

---

## 9. Proposed MonolithPCG Module

### 9.1 Module Structure

```
Source/MonolithPCG/
  Public/
    MonolithPCGModule.h
    MonolithPCGActions.h              -- Core graph CRUD actions
    MonolithPCGExecutionActions.h     -- Execute/cleanup actions
    MonolithPCGTemplateActions.h      -- Pre-built graph templates
    MonolithPCGBridgeActions.h        -- Spatial registry <-> PCG data bridge
  Private/
    MonolithPCGModule.cpp
    MonolithPCGActions.cpp
    MonolithPCGExecutionActions.cpp
    MonolithPCGTemplateActions.cpp
    MonolithPCGBridgeActions.cpp
  MonolithPCG.Build.cs
```

### 9.2 Build.cs

```csharp
using UnrealBuildTool;
using System.IO;

public class MonolithPCG : ModuleRules
{
    public MonolithPCG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "MonolithCore",
            "PCG",
            "UnrealEd",
            "Json",
            "JsonUtilities",
            "StructUtils",
            "AssetRegistry",
            "AssetTools"
        });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("PCGEditor");
        }
    }
}
```

### 9.3 uplugin Addition

```json
{
    "Name": "MonolithPCG",
    "Type": "Editor",
    "LoadingPhase": "Default"
}
```

And plugin dependency:
```json
{
    "Name": "PCG",
    "Enabled": true,
    "Optional": true
}
```

### 9.4 Proposed Actions (~35 actions)

**Phase 1: Core Graph CRUD (8 actions)**

| Action | Description |
|--------|-------------|
| `create_pcg_graph` | Create a new PCG graph asset at a given path |
| `list_pcg_graphs` | List all PCG graph assets in the project |
| `get_pcg_graph_info` | Get nodes, edges, parameters of a graph |
| `add_pcg_node` | Add a node of a given settings type to a graph |
| `remove_pcg_node` | Remove a node from a graph |
| `connect_pcg_nodes` | Wire two nodes together (from pin -> to pin) |
| `disconnect_pcg_nodes` | Remove an edge between nodes |
| `set_pcg_node_params` | Set UPROPERTY values on a node's settings |

**Phase 2: Parameters & Execution (7 actions)**

| Action | Description |
|--------|-------------|
| `add_pcg_parameter` | Add a user parameter to a graph |
| `set_pcg_parameter` | Set a user parameter value |
| `get_pcg_parameters` | List all user parameters and their values |
| `execute_pcg` | Trigger generation on a PCG component |
| `cleanup_pcg` | Cleanup generated output |
| `get_pcg_output` | Read generated output data (point counts, spawned actors) |
| `cancel_pcg_execution` | Cancel in-progress generation |

**Phase 3: Scene Integration (7 actions)**

| Action | Description |
|--------|-------------|
| `spawn_pcg_volume` | Place an APCGVolume in the scene with a given graph |
| `spawn_pcg_component` | Add UPCGComponent to an existing actor |
| `set_pcg_component_graph` | Assign a graph to a PCG component |
| `list_pcg_components` | Find all PCG components in the level |
| `refresh_pcg_component` | Force refresh a PCG component |
| `get_pcg_generated_actors` | List actors spawned by a PCG component |
| `set_pcg_generation_trigger` | Set GenerateOnLoad / OnDemand / AtRuntime |

**Phase 4: Templates & Presets (8 actions)**

| Action | Description |
|--------|-------------|
| `create_scatter_graph` | Build a parameterized surface scatter graph |
| `create_spline_scatter_graph` | Build a spline-based placement graph |
| `create_vegetation_graph` | Build a landscape vegetation graph |
| `create_horror_dressing_graph` | Build a decay-driven dressing graph |
| `create_debris_graph` | Build a debris scatter with physics settle |
| `apply_pcg_preset` | Apply a JSON preset to configure a template graph |
| `save_pcg_preset` | Save current graph config as a JSON preset |
| `list_pcg_presets` | List available PCG presets |

**Phase 5: Bridge & Custom Nodes (5 actions)**

| Action | Description |
|--------|-------------|
| `spatial_registry_to_pcg` | Convert spatial registry rooms/buildings to PCG point data |
| `pcg_output_to_registry` | Register PCG-spawned items back into spatial registry |
| `building_descriptor_to_pcg` | Convert building descriptor JSON to PCG parameters |
| `create_room_scatter_graph` | Build a graph that scatters within room bounds from registry |
| `batch_execute_pcg` | Execute multiple PCG components with dependency ordering |

---

## 10. Implementation Approach

### 10.1 Graph Construction (create_pcg_graph, add_pcg_node)

```cpp
FMonolithActionResult HandleCreatePCGGraph(const TSharedPtr<FJsonObject>& Params)
{
    FString Path = Params->GetStringField("path"); // e.g., "/Game/PCG/FurnitureScatter"
    FString Name = Params->GetStringField("name"); // e.g., "PCG_FurnitureScatter"

    // Create package
    FString PackagePath = Path + "/" + Name;
    UPackage* Package = CreatePackage(*PackagePath);

    // Create graph
    UPCGGraph* Graph = NewObject<UPCGGraph>(Package, *Name, RF_Public | RF_Standalone);

    // Save
    FAssetRegistryModule::AssetCreated(Graph);
    Package->MarkPackageDirty();
    UEditorAssetLibrary::SaveAsset(PackagePath);

    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField("path", PackagePath);
    Result->SetStringField("class", "PCGGraph");
    return FMonolithActionResult::Success(Result);
}
```

### 10.2 Adding Nodes (add_pcg_node)

```cpp
FMonolithActionResult HandleAddPCGNode(const TSharedPtr<FJsonObject>& Params)
{
    // Load graph
    UPCGGraph* Graph = LoadObject<UPCGGraph>(nullptr, *Params->GetStringField("graph"));
    FString NodeType = Params->GetStringField("type"); // e.g., "SurfaceSampler"

    // Map type string to settings class
    TSubclassOf<UPCGSettings> SettingsClass = ResolvePCGSettingsClass(NodeType);

    UPCGSettings* DefaultSettings = nullptr;
    UPCGNode* NewNode = Graph->AddNodeOfType(SettingsClass, DefaultSettings);

    // Optionally set properties from params
    if (Params->HasField("settings"))
    {
        ApplyJsonToSettings(DefaultSettings, Params->GetObjectField("settings"));
    }

    // Set node position for editor layout
    if (Params->HasField("position"))
    {
        auto PosObj = Params->GetObjectField("position");
        NewNode->PositionX = PosObj->GetIntegerField("x");
        NewNode->PositionY = PosObj->GetIntegerField("y");
    }

    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField("node_id", NewNode->GetFName().ToString());
    Result->SetStringField("type", NodeType);
    return FMonolithActionResult::Success(Result);
}
```

### 10.3 Executing (execute_pcg)

```cpp
FMonolithActionResult HandleExecutePCG(const TSharedPtr<FJsonObject>& Params)
{
    // Find or spawn PCG component
    FString ActorName = Params->GetStringField("actor");
    AActor* Actor = FindActorByName(ActorName);

    UPCGComponent* PCGComp = Actor->FindComponentByClass<UPCGComponent>();
    if (!PCGComp)
    {
        return FMonolithActionResult::Error("No PCG component found on actor");
    }

    bool bForce = Params->GetBoolField("force");
    PCGComp->GenerateLocal(bForce);

    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField("generating", true);
    return FMonolithActionResult::Success(Result);
}
```

### 10.4 Spawning a PCG Volume (spawn_pcg_volume)

```cpp
FMonolithActionResult HandleSpawnPCGVolume(const TSharedPtr<FJsonObject>& Params)
{
    UWorld* World = GEditor->GetEditorWorldContext().World();

    // Parse transform
    FVector Location = ParseVector(Params, "location");
    FVector Extent = ParseVector(Params, "extent", FVector(500, 500, 500));

    // Spawn APCGVolume
    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APCGVolume* Volume = World->SpawnActor<APCGVolume>(APCGVolume::StaticClass(), FTransform(Location), SpawnParams);

    // Scale brush to desired extent
    Volume->SetActorScale3D(Extent / 100.0);  // Default brush is 200x200x200

    // Assign graph
    if (Params->HasField("graph"))
    {
        UPCGGraphInterface* Graph = LoadObject<UPCGGraphInterface>(nullptr, *Params->GetStringField("graph"));
        Volume->PCGComponent->SetGraph(Graph);
    }

    // Set generation trigger
    Volume->PCGComponent->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;

    // Outliner folder (Monolith requirement)
    Volume->SetFolderPath(FName("Monolith/PCG"));

    auto Result = MakeShared<FJsonObject>();
    Result->SetStringField("actor", Volume->GetName());
    return FMonolithActionResult::Success(Result);
}
```

---

## 11. Settings Class Resolution

A critical piece: mapping user-friendly type strings to `UPCGSettings` subclasses.

```cpp
static TSubclassOf<UPCGSettings> ResolvePCGSettingsClass(const FString& TypeName)
{
    // Build lookup on first call via UObjectIterator
    static TMap<FString, TSubclassOf<UPCGSettings>> ClassMap;
    if (ClassMap.IsEmpty())
    {
        for (TObjectIterator<UClass> It; It; ++It)
        {
            if (It->IsChildOf(UPCGSettings::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract))
            {
                FString Name = It->GetName();
                Name.RemoveFromStart("PCG");
                Name.RemoveFromEnd("Settings");
                ClassMap.Add(Name, *It);

                // Also add full class name
                ClassMap.Add(It->GetName(), *It);
            }
        }
    }

    if (auto Found = ClassMap.Find(TypeName))
    {
        return *Found;
    }
    return nullptr;
}
```

This means `"SurfaceSampler"`, `"StaticMeshSpawner"`, `"DensityFilter"`, etc. all resolve correctly.

---

## 12. Template Graph Construction Example

Building a furniture scatter graph programmatically:

```cpp
UPCGGraph* BuildFurnitureScatterGraph(const FString& SavePath)
{
    UPackage* Package = CreatePackage(*SavePath);
    UPCGGraph* Graph = NewObject<UPCGGraph>(Package, FName("PCG_FurnitureScatter"), RF_Public | RF_Standalone);

    // Add user parameters
    TArray<FPropertyBagPropertyDesc> Params;
    Params.Add(FPropertyBagPropertyDesc(FName("density"), EPropertyBagPropertyType::Float));
    Params.Add(FPropertyBagPropertyDesc(FName("min_distance"), EPropertyBagPropertyType::Float));
    Params.Add(FPropertyBagPropertyDesc(FName("seed"), EPropertyBagPropertyType::Int32));
    Graph->AddUserParameters(Params);
    Graph->SetGraphParameter<float>(FName("density"), 0.5f);
    Graph->SetGraphParameter<float>(FName("min_distance"), 100.0f);

    // Create nodes
    UPCGSurfaceSamplerSettings* SamplerSettings = nullptr;
    UPCGNode* SamplerNode = Graph->AddNodeOfType<UPCGSurfaceSamplerSettings>(SamplerSettings);
    SamplerSettings->PointsPerSquaredMeter = 0.5f;

    UPCGSelfPruningSettings* PruningSettings = nullptr;
    UPCGNode* PruningNode = Graph->AddNodeOfType<UPCGSelfPruningSettings>(PruningSettings);
    PruningSettings->PruningType = EPCGSelfPruningType::LargeToSmall;

    UPCGStaticMeshSpawnerSettings* SpawnerSettings = nullptr;
    UPCGNode* SpawnerNode = Graph->AddNodeOfType<UPCGStaticMeshSpawnerSettings>(SpawnerSettings);

    // Wire: Input -> Sampler -> Pruning -> Spawner -> Output
    UPCGNode* InputNode = Graph->GetInputNode();
    UPCGNode* OutputNode = Graph->GetOutputNode();

    Graph->AddEdge(InputNode, FName("Out"), SamplerNode, FName("Surface"));
    Graph->AddEdge(SamplerNode, FName("Out"), PruningNode, FName("In"));
    Graph->AddEdge(PruningNode, FName("Out"), SpawnerNode, FName("In"));
    Graph->AddEdge(SpawnerNode, FName("Out"), OutputNode, FName("In"));

    // Save
    FAssetRegistryModule::AssetCreated(Graph);
    Package->MarkPackageDirty();

    return Graph;
}
```

---

## 13. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| PCG API changes between UE versions | Medium | Wrap all PCG calls behind our API; version check with `#if ENGINE_MAJOR_VERSION` |
| Graph construction without editor graph sync | High | Call `Graph->NotifyGraphChanged(EPCGChangeType::Structural)` after modifications |
| Thread safety for async PCG execution | Medium | Use `ScheduleGeneric` with proper dependencies; don't read output until task completes |
| PCG module not loaded (optional dependency) | Low | Guard with `WITH_PCG` define, graceful degradation |
| Node settings property names changing | Low | Use reflection-based property setting, not hardcoded member access |
| Large PCG output overwhelming MCP response | Medium | Summarize output (point count, actor count) instead of serializing all points |

---

## 14. Phase Plan

### Phase 1: Core CRUD (~20-25h)
- MonolithPCG module scaffolding (Build.cs, Module.h/cpp, uplugin entry)
- Settings class resolver
- `create_pcg_graph`, `list_pcg_graphs`, `get_pcg_graph_info`
- `add_pcg_node`, `remove_pcg_node`, `connect_pcg_nodes`, `disconnect_pcg_nodes`
- `set_pcg_node_params` (reflection-based property setter)

### Phase 2: Parameters & Execution (~15-20h)
- User parameter CRUD (`add/set/get_pcg_parameter`)
- `execute_pcg`, `cleanup_pcg`, `cancel_pcg_execution`
- `get_pcg_output` with summary statistics
- Async execution tracking

### Phase 3: Scene Integration (~15-20h)
- `spawn_pcg_volume` with graph assignment
- `spawn_pcg_component` on existing actors
- `list_pcg_components`, `refresh_pcg_component`
- `get_pcg_generated_actors`
- `set_pcg_generation_trigger`

### Phase 4: Templates & Presets (~15-20h)
- `create_scatter_graph` (furniture)
- `create_spline_scatter_graph` (street furniture)
- `create_vegetation_graph` (landscape)
- `create_horror_dressing_graph` (decay-driven)
- JSON preset save/load/list

### Phase 5: Bridge & Custom Nodes (~15-25h)
- `spatial_registry_to_pcg` / `pcg_output_to_registry`
- `building_descriptor_to_pcg`
- `create_room_scatter_graph` (per-room from spatial registry)
- `batch_execute_pcg` with dependency ordering
- Optional: Custom UPCGSettings nodes for Monolith data access

**Total: ~80-110h**

---

## 15. Sources

- [UPCGGraph API Reference (5.6)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGGraph)
- [PCG Development Guides (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [PCG Node Reference (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG Overview (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [PCG Generation Modes (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine)
- [PCG GPU Processing (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)
- [How to Create Custom PCG Nodes -- Blueshift Interactive](https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/)
- [PCGEx Extended Toolkit Documentation](https://pcgex.gitbook.io/pcgex)
- [PCGEx GitHub Source](https://github.com/PCGEx/PCGExtendedToolkit)
- [PCGEx DeepWiki Architecture](https://deepwiki.com/Nebukam/PCGExtendedToolkit/1-overview)
- [Inside Unreal: PCG to the Extreme with PCGEx](https://forums.unrealengine.com/t/inside-unreal-taking-pcg-to-the-extreme-with-the-pcgex-plugin/2479952)
- [UE 5.7 Performance Highlights -- Tom Looman](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)
- [UPCGGraph::AddUserParameters API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/UPCGGraph/AddUserParameters)
- UE 5.7 source: `Engine/Plugins/PCG/Source/PCG/Public/` (local install)

---

## 16. Key Source File Paths (Local)

- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGGraph.h` -- Graph API
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGNode.h` -- Node API
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGSettings.h` -- Settings base class
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGComponent.h` -- Component (execution)
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\Subsystems\PCGSubsystem.h` -- Subsystem (scheduling)
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGVolume.h` -- Volume actor
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGData.h` -- Data + FPCGDataCollection
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGParamData.h` -- Attribute set data
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\PCGPin.h` -- Pin types
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\Elements\PCGStaticMeshSpawner.h` -- Mesh spawner
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\Public\Elements\PCGSurfaceSampler.h` -- Surface sampler
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCG\PCG.Build.cs` -- PCG module dependencies
- `C:\Program Files (x86)\UE_5.7\Engine\Plugins\PCG\Source\PCGEditor\PCGEditor.Build.cs` -- PCGEditor dependencies
