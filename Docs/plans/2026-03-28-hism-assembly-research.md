# HISM-Based Modular Building Assembly: Performance and Implementation Research

**Date:** 2026-03-28
**Scope:** UE 5.7 HISM C++ API, performance characteristics, memory layout, collision, LOD, auto-instancing comparison, building assembly architecture
**Related:** `2026-03-28-modular-building-research.md`, `2026-03-28-modular-pieces-research.md`

---

## 1. HISM C++ API (UE 5.7)

### Class Hierarchy

```
UStaticMeshComponent
  -> UInstancedStaticMeshComponent (ISM)
       -> UHierarchicalInstancedStaticMeshComponent (HISM)
```

Header: `Engine/Classes/Components/HierarchicalInstancedStaticMeshComponent.h`
ISM Header: `Engine/Classes/Components/InstancedStaticMeshComponent.h`

### Core API Methods

All methods are virtual overrides from `UInstancedStaticMeshComponent`:

```cpp
// --- Adding Instances ---
virtual int32 AddInstance(const FTransform& InstanceTransform, bool bWorldSpace = false);
virtual TArray<int32> AddInstances(const TArray<FTransform>& InstanceTransforms,
    bool bShouldReturnIndices, bool bWorldSpace = false, bool bUpdateNavigation = true);
void PreAllocateInstancesMemory(int32 AddedInstanceCount);  // Pre-allocate before batch add

// --- Removing Instances ---
virtual bool RemoveInstance(int32 InstanceIndex);
virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove);
virtual bool RemoveInstances(const TArray<int32>& InstancesToRemove,
    bool bInstanceArrayAlreadySortedInReverseOrder);  // Optimization: pre-sorted removal
virtual void ClearInstances();

// --- Updating Transforms ---
virtual bool UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform,
    bool bWorldSpace = false, bool bMarkRenderStateDirty = false, bool bTeleport = false);
virtual bool BatchUpdateInstancesTransforms(int32 StartInstanceIndex,
    const TArray<FTransform>& NewInstancesTransforms, bool bWorldSpace = false,
    bool bMarkRenderStateDirty = false, bool bTeleport = false);
virtual bool BatchUpdateInstancesTransform(int32 StartInstanceIndex, int32 NumInstances,
    const FTransform& NewInstancesTransform, ...);  // Same transform for N instances
virtual bool BatchUpdateInstancesData(int32 StartInstanceIndex, int32 NumInstances,
    FInstancedStaticMeshInstanceData* StartInstanceData, ...);

// --- Per-Instance Custom Data ---
virtual bool SetCustomDataValue(int32 InstanceIndex, int32 CustomDataIndex,
    float CustomDataValue, bool bMarkRenderStateDirty = false);
virtual bool SetCustomData(int32 InstanceIndex, TArrayView<const float> InCustomData,
    bool bMarkRenderStateDirty = false);
virtual bool SetCustomData(int32 InstanceIndexStart, int32 InstanceIndexEnd,
    TConstArrayView<float> CustomDataFloats, bool bMarkRenderStateDirty = false);  // Range set
virtual void SetNumCustomDataFloats(int32 InNumCustomDataFloats);  // MUST set before use

// --- Mesh & Material ---
// Inherited from UStaticMeshComponent:
void SetStaticMesh(UStaticMesh* NewMesh);
void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);

// --- Spatial Queries ---
virtual TArray<int32> GetInstancesOverlappingSphere(const FVector& Center, float Radius,
    bool bSphereInWorldSpace = true) const;
virtual TArray<int32> GetInstancesOverlappingBox(const FBox& Box,
    bool bBoxInWorldSpace = true) const;
int32 GetOverlappingSphereCount(const FSphere& Sphere) const;
int32 GetOverlappingBoxCount(const FBox& Box) const;
void GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const;

// --- Tree Management ---
bool BuildTreeIfOutdated(bool Async, bool ForceUpdate);
bool IsAsyncBuilding() const;
bool IsTreeFullyBuilt() const;
void GetTree(TArray<FClusterNode>& OutClusterTree) const;
```

### New in UE 5.7: ID-Based Interface (ISM only, NOT HISM)

```cpp
// Stable instance IDs that survive reordering. Only on ISM, explicitly NOT supported on HISM.
TArray<FPrimitiveInstanceId> AddInstancesById(const TArrayView<const FTransform>&, ...);
void SetCustomDataById(const TArrayView<const FPrimitiveInstanceId>&, TArrayView<const float>);
void RemoveInstancesById(const TArrayView<const FPrimitiveInstanceId>&, bool bUpdateNavigation);
void UpdateInstanceTransformById(FPrimitiveInstanceId, const FTransform&, ...);
```

### Key Properties (ISM base class)

```cpp
int32 NumCustomDataFloats;           // MUST be set before calling SetCustomDataValue
int32 InstancingRandomSeed;          // Auto-populated if 0
float InstanceLODDistanceScale;      // Scale LOD distances (smaller = earlier transitions)
int32 InstanceMinDrawDistance;       // Start drawing distance
int32 InstanceStartCullDistance;     // Begin fade out
int32 InstanceEndCullDistance;       // Complete fade out
uint8 bUseGpuLodSelection : 1;      // GPU-driven LOD selection (default true in UE 5.7)
uint8 bDisableCollision : 1;        // Skip collision entirely
uint8 bSupportRemoveAtSwap : 1;     // Swap-remove instead of shift (HISM: always true)
```

### HISM-Specific: Cluster Tree

The HISM builds a spatial BVH (bounding volume hierarchy) over instances:

```cpp
struct FClusterNode {
    FVector3f BoundMin;
    int32 FirstChild;         // -1 for leaf nodes
    FVector3f BoundMax;
    int32 LastChild;
    int32 FirstInstance;      // Range of instances in this node
    int32 LastInstance;
    FVector3f MinInstanceScale;
    FVector3f MaxInstanceScale;
};
```

- Tree is built via `FClusterBuilder` (can be async via `BuildTreeAsync()`)
- `bAutoRebuildTreeOnInstanceChanges` controls automatic rebuild
- `DesiredInstancesPerLeaf()` returns heuristic target (varies by vert count per LOD)
- Builder takes `MaxInstancesPerLeaf` parameter
- Instances are reordered during tree build (`SortedInstances` maps tree order -> data order)

### Important: bMarkRenderStateDirty

Most mutating methods have a `bMarkRenderStateDirty` parameter defaulting to false. For batch operations:
- Set false for all but the last call
- Set true on the final call to trigger a single render state update
- Alternatively, call `MarkRenderStateDirty()` manually after all modifications

---

## 2. Instance Buffer Memory Layout (from source)

The GPU instance buffer is split into 4 separate vertex streams (`FStaticMeshInstanceData`):

### Stream 1: InstanceOriginData
- Type: `TStaticMeshVertexData<FVector4f>`
- Per instance: **16 bytes** (FVector4f = 4x float32)
- Contents: `{X, Y, Z, RandomInstanceID}`

### Stream 2: InstanceTransformData
- Two modes based on `bUseHalfFloat`:
  - **Half float** (default on most platforms): `FInstanceTransformMatrix<FFloat16>` = 3 rows x 4 elements x 2 bytes = **24 bytes**
  - **Full float**: `FInstanceTransformMatrix<float>` = 3 rows x 4 elements x 4 bytes = **48 bytes**
- Contents: 3x4 matrix (rotation/scale, no translation -- translation is in Origin stream)
- W column stores editor-only data (hit proxy color, selection) in WITH_EDITOR builds

### Stream 3: InstanceLightmapData
- Type: `TStaticMeshVertexData<FInstanceLightMapVector>`
- Per instance: **8 bytes** (4x int16)
- Contents: `{LightmapU, LightmapV, ShadowmapU, ShadowmapV}` biases

### Stream 4: InstanceCustomData
- Type: `TStaticMeshVertexData<float>`
- Per instance: **4 bytes x NumCustomDataFloats**
- Contents: User-defined floats

### Total Memory Per Instance

| Configuration | Bytes/Instance |
|---|---|
| Half float, 0 custom data | 48 bytes |
| Half float, 4 custom data | 64 bytes |
| Full float, 0 custom data | 72 bytes |
| Full float, 4 custom data | 88 bytes |

**Practical estimate for modular buildings (half float, 4 custom floats):** ~64 bytes/instance

### Memory Budget Examples

| Scenario | Instances | Memory (half, 4 custom) |
|---|---|---|
| Single building (50 pieces) | 50 | 3.2 KB |
| City block (10 buildings) | 500 | 32 KB |
| Small town (100 buildings) | 5,000 | 320 KB |
| Large city (1000 buildings) | 50,000 | 3.2 MB |
| Extreme (10K buildings) | 500,000 | 32 MB |

Instance buffer memory is negligible even at extreme scale. The mesh data itself (shared across all instances) is the real memory cost.

---

## 3. Performance Characteristics

### AddInstance vs AddInstances (Batch)

**Critical finding:** In UE5, each `AddInstance` call can trigger collision rebuild with Chaos physics, creating exponential slowdown:
- UE4: 80,000 instances spawned in 15-20 seconds
- UE5: Performance "basically unusable after 10,000 instances" during active add/remove
- UE5: 250,000 instances takes ~5 minutes vs ~10 seconds in UE4

**Root cause:** Chaos physics rebuilds the entire collision set per AddInstance call.

**Workarounds (CRITICAL for building generation):**
1. **Disable collision before batch add, enable after:**
   ```cpp
   HISMComp->bDisableCollision = true;
   HISMComp->AddInstances(AllTransforms, false, true);
   HISMComp->bDisableCollision = false;
   // Rebuild collision once
   HISMComp->RecreatePhysicsState();
   ```
2. **Use `AddInstances()` (plural) with full array** -- single allocation + single tree build
3. **Use `PreAllocateInstancesMemory()` before adding** -- prevents reallocation

### Draw Call Behavior

- **1 draw call per material section per visible LOD group** per HISM component
- A mesh with 2 materials = 2 draw calls minimum per HISM component
- HISM can split instances across multiple LOD levels; each LOD level = separate draw call batch
- With GPU LOD selection (`bUseGpuLodSelection=true`, default in 5.7): LOD is computed on GPU per instance, reducing CPU overhead
- **All instances in one HISM component share the same mesh** -- you need separate HISM components for different meshes

**For modular buildings:** If you have 15 piece types, you need 15 HISM components = ~15-30 draw calls total (assuming 1-2 materials each). These draw calls are shared across ALL buildings using those pieces.

### Culling: How HISM's Spatial Hierarchy Works

1. HISM builds a BVH (bounding volume hierarchy) tree over instances at construction time
2. Each `FClusterNode` stores bounds of a group of instances
3. During rendering, the engine traverses the tree:
   - If a node's bounds are outside the frustum, skip entire subtree
   - If a node's bounds are occluded, skip entire subtree
   - Only leaf-level instances that pass culling are submitted to GPU
4. Only visible instance transforms are written to the GPU instance buffer each frame
5. With GPU scene (default in UE5), instances are managed in GPUScene and culled via GPU compute

**ISM (no hierarchy) vs HISM:**
- ISM: all instances are one flat group, culled as a single unit (all or nothing per component)
- HISM: hierarchical culling allows partial visibility (critical for spread-out instances)

**For buildings:** Since modular pieces within a building are spatially coherent and relatively close together, ISM might actually be sufficient. HISM's advantage kicks in when instances are spread across a large area (like foliage across a landscape).

### Maximum Instance Counts

| Context | Practical Limit | Notes |
|---|---|---|
| Per HISM component (rendering) | ~50,000-100,000 | GPU handles it fine, tree build time becomes the bottleneck |
| Per HISM component (with collision) | ~5,000-10,000 | Chaos physics registration is the bottleneck |
| Per HISM component (no collision) | ~500,000+ | Memory and tree build time are the only limits |
| Total across all HISM components | GPU-bound | Depends on total triangle count, not instance count |

**For modular buildings:** A single building is ~20-100 pieces. Even 1000 buildings = 20K-100K total instances distributed across ~15 HISM components. Well within comfortable range.

---

## 4. Per-Instance Custom Data

### Setup (C++)

```cpp
// Before adding any instances, set the custom data count
HISMComp->NumCustomDataFloats = 4;  // Reserve 4 floats per instance

// After adding instances, set data
HISMComp->SetCustomDataValue(InstanceIndex, 0, RoomID);       // Data slot 0
HISMComp->SetCustomDataValue(InstanceIndex, 1, DecayLevel);   // Data slot 1
HISMComp->SetCustomDataValue(InstanceIndex, 2, DamageTint);   // Data slot 2
HISMComp->SetCustomDataValue(InstanceIndex, 3, MaterialVar);  // Data slot 3

// Batch set for a single instance (all custom data at once)
TArray<float> CustomData = {RoomID, DecayLevel, DamageTint, MaterialVar};
HISMComp->SetCustomData(InstanceIndex, CustomData, true);
```

### Material Access

- Node: `PerInstanceCustomData` (available in material graph)
- **Vertex shader only** natively -- use `VertexInterpolator` to pass to pixel shader
- Access by data index (0-based)

```
PerInstanceCustomData[0] -> VertexInterpolator -> Pixel Shader use
```

### Use Cases for Modular Buildings

| Slot | Use | Range |
|---|---|---|
| 0 | Room/Zone ID | 0-255 (encoded) |
| 1 | Decay/damage level | 0.0-1.0 |
| 2 | Material tint/variant | Encoded color or index |
| 3 | Building ID or misc | 0-N |

### Performance Cost

- **GPU memory:** 4 bytes per float per instance (see memory table above)
- **GPU compute:** Minimal -- just a buffer read in vertex shader
- **CPU cost:** `SetCustomDataValue` writes to a TArray, then the buffer is uploaded to GPU on render state dirty
- **No additional draw calls** -- custom data does NOT break batching
- **Limitation:** Custom data is per-instance, not per-vertex. For vertex-level variation, use world-position-based procedural techniques in the material.

### HISM + Custom Data Gotcha

Per-instance custom data works with HISM as of UE 4.27+. Earlier versions had bugs where HISM ignored custom data. In UE 5.7 this is confirmed working. The `FClusterBuilder` constructor explicitly takes `TArray<float> InCustomDataFloats` and `int32 InNumCustomDataFloats`.

---

## 5. HISM vs ISM vs Auto-Instancing

### Comparison Matrix

| Feature | Static Mesh Actors + Auto-Instancing | ISM Component | HISM Component |
|---|---|---|---|
| **Draw calls** | Auto-merged if same mesh+material+pipeline | 1 per material section | 1 per material section per LOD group |
| **CPU overhead** | Per-actor overhead (transform, ticking infrastructure) | Single component overhead | Single component + tree overhead |
| **Hierarchical culling** | Per-actor frustum/occlusion | No (all or nothing) | Yes (BVH tree) |
| **Per-instance LOD** | Per-actor LOD | Shared LOD (all same) | Per-cluster LOD, GPU LOD selection |
| **Per-instance custom data** | No (CustomPrimitiveData is per-actor) | Yes | Yes |
| **Collision** | Full per-actor collision | Component-level, issues with Chaos | Same as ISM, tree-accelerated queries |
| **Instance removal** | Delete actor | RemoveInstance (reindex or swap) | RemoveInstance (swap, always) |
| **Outliner** | Every actor visible | Single actor | Single actor |
| **Nav mesh** | Automatic | Supported | Supported |
| **Code complexity** | Simplest (SpawnActor) | Medium | Medium |
| **Max practical count** | ~5,000-10,000 actors | ~50K+ (no collision) | ~500K+ (no collision) |

### Auto-Instancing Deep Dive

`r.MeshDrawCommands.DynamicInstancing` (default: 1 in UE5):
- Merges draw calls for identical mesh + material + pipeline state at render time
- **Compatibility requirements:** Same StaticMesh, same materials, same blend mode, same shading model
- Debug: `r.MeshDrawCommands.LogDynamicInstancingStats 1` prints merge ratios
- **Performance comparison (2222 spheres test):** ISM ~1.2ms vs auto-instanced SM actors ~5.3ms
- Auto-instancing reduces draw calls but does NOT reduce per-actor CPU overhead (tick, transform updates, GC pressure)

### When to Use What

**Use HISM when:**
- 1000+ instances of the same mesh
- Instances spread across a large area (city-scale)
- Need per-instance custom data for material variation
- Need hierarchical culling for performance
- Need per-instance LOD selection

**Use ISM when:**
- Using Nanite meshes (HISM tree is redundant -- Nanite does its own GPU culling)
- Moderate instance count (~100-5000)
- Instances are spatially coherent (single building)
- Need stable instance IDs (FPrimitiveInstanceId, ISM only in UE 5.7)

**Use StaticMesh actors + auto-instancing when:**
- Small number of unique placements (<100)
- Need full per-actor collision and physics
- Need actors in outliner for level design
- Prototyping / simplicity matters more than peak performance

### DrawCallReducer Plugin Interaction

Our DrawCallReducer plugin operates at the mesh merge level, combining draw calls for different meshes that share materials. This is complementary to HISM:
- **HISM:** Batches identical meshes into single draw calls
- **DrawCallReducer:** Merges different meshes with same material into single draw calls
- They operate at different levels and do not conflict
- For HISM components, DrawCallReducer has no additional benefit (already 1 draw call per material)
- DrawCallReducer is more relevant for non-instanced static mesh actors

---

## 6. Collision with HISM

### What Works

- **Simple collision shapes** (boxes, convex hulls) work for line traces and overlap queries
- **Complex collision as simple** (`CTF_UseComplexAsSimple`) works for walkable surfaces
- **Per-instance collision bodies** are created -- each instance gets its own physics body
- **Line traces** work against HISM instances (returns instance index via `FHitResult::Item`)
- **HISM spatial queries** (`GetInstancesOverlappingSphere/Box`) use the cluster tree for fast lookups

### What Does NOT Work Well

- **HISM-to-HISM collisions** are generally ignored (by design -- background geometry)
- **Overlap events** (`bGenerateOverlapEvents`) are unreliable with HISM
- **Physics simulation** is not supported on HISM instances
- **Per-instance collision toggling** is not supported -- collision is component-wide
- **Chaos physics registration** is expensive: ~150 microseconds per collision shape toggle

### The Collision Performance Problem

The #1 performance issue with HISM is collision registration:
- Each instance creates a separate Chaos physics body
- Adding 10,000 instances with collision: seconds of freeze
- This is the reason UE5 ISM "regressed" from UE4 (PhysX was faster at bulk registration)

### Recommendation for Modular Buildings

```
CRITICAL: Set bDisableCollision = true on HISM components used for visual geometry.
Use a separate, simplified collision approach:
```

**Strategy:**
1. **HISM components for rendering** -- `bDisableCollision = true`, maximum performance
2. **Simplified collision volumes** per room/building:
   - Box collision for each room (walls + floor + ceiling)
   - Or: Use the room's AABB as a simple blocking volume
   - Or: A single merged collision mesh per building (generated once)
3. **Walkable floors:** Generate a simple plane collision mesh per floor level
4. **Doors/openings:** Small blocking volumes or nav links

This is how AAA games handle it. The visual mesh and collision mesh are decoupled. A building with 200 HISM instances might only need 10-20 simple box collisions.

---

## 7. LOD with HISM

### Per-Instance LOD Selection

- **UE 5.7 default:** `bUseGpuLodSelection = true` -- GPU computes LOD per instance based on screen size
- CPU fallback: LOD is determined per cluster node (group of instances), not per individual instance
- LOD transitions happen at the instance level (each instance can be at a different LOD)
- `InstanceLODDistanceScale` property adjusts all LOD distances uniformly

### LOD Transitions at Modular Seams

**This is a real concern for modular buildings:**
- Adjacent wall pieces at different LODs can create visible gaps at seams
- Lower LODs may simplify geometry in ways that don't align at boundaries

**Mitigation strategies:**
1. **Conservative LOD distances:** Set `InstanceLODDistanceScale` high enough that nearby pieces share the same LOD
2. **Careful LOD authoring:** Ensure LOD meshes maintain edge profiles at piece boundaries
3. **Per-component LOD:** Put all pieces of a single building in the same HISM component with shared cull distances
4. **Dithered LOD transitions:** Use material-based dithered fade between LODs to hide seam pops
5. **Skip LODs for small pieces:** Simple wall pieces may only need LOD0 and a merged LOD at distance

### HISM LOD Draw Call Impact

Each active LOD level within a HISM component generates separate draw calls:
- 100 instances, all LOD0: 1 draw call per material
- 100 instances, 80 LOD0 + 20 LOD1: 2 draw calls per material
- With GPU LOD selection, this is managed efficiently on the GPU side

---

## 8. Building Assembly Architecture

### Recommended: One Actor Per Building, Multiple HISM Components

```
ABuildingActor (per building)
  |-- UHISMComponent "Walls"        (SM_Wall_Standard)
  |-- UHISMComponent "WallsDoor"    (SM_Wall_Door)
  |-- UHISMComponent "WallsWindow"  (SM_Wall_Window)
  |-- UHISMComponent "Floors"       (SM_Floor_Standard)
  |-- UHISMComponent "Ceilings"     (SM_Ceiling_Standard)
  |-- UHISMComponent "Corners"      (SM_Corner_Standard)
  |-- UHISMComponent "Trim"         (SM_Trim_Standard)
  |-- UHISMComponent "Stairs"       (SM_Stairs_Standard)
  ... (one per unique mesh type)
```

**Pros:**
- Clean outliner organization (one actor per building + SetFolderPath)
- Each building can be moved/deleted as a unit
- Per-building collision volumes are natural children
- Custom data can encode room ID per instance
- Building-level culling (entire actor culled when out of range)

**Cons:**
- 10-20 HISM components per building = 10-20 draw calls per building
- Not ideal for thousands of buildings all visible simultaneously

### Alternative: One Actor Per Piece Type (Block-Wide)

```
ABlockPieceManager (per city block)
  |-- UHISMComponent "Walls"        (ALL walls across ALL buildings in block)
  |-- UHISMComponent "WallsDoor"    (ALL door walls across ALL buildings)
  |-- UHISMComponent "Floors"       (ALL floors across ALL buildings)
  ...
```

**Pros:**
- Minimum draw calls (15-20 total for entire block)
- Maximum instancing efficiency
- Better HISM tree distribution (more instances = better tree)

**Cons:**
- Cannot move/delete individual buildings easily
- Outliner has one opaque actor per block
- Harder to manage per-building state
- Must use custom data to encode building ID for per-building effects

### Recommended Hybrid Approach

```
Phase 1 (MVP): One actor per building, multiple HISM components
  - Simple, debuggable, good outliner
  - Works fine for <100 buildings visible at once
  - 15-20 draw calls per building * ~20 visible = ~300-400 draw calls (acceptable)

Phase 2 (Optimization): Block-wide HISM when needed
  - Merge building HISM components into block-level managers
  - Only needed if draw call count becomes a bottleneck
  - Custom data encodes {BuildingID, RoomID, DecayLevel, Variant}

Phase 3 (Streaming): World Partition integration
  - Each block's HISM actors in streaming cells
  - Load/unload with world partition
  - HLOD for distant buildings (merge to simplified mesh)
```

### Outliner Organization

Per the project rule (all spawned actors MUST use SetFolderPath):

```cpp
BuildingActor->SetFolderPath(FName(TEXT("ProceduralBuildings/Block_0/Building_3")));
// Or for block-wide:
BlockManager->SetFolderPath(FName(TEXT("ProceduralBuildings/Block_0")));
```

---

## 9. Alternative: StaticMeshActors with Auto-Instancing

### The Simple Path

```cpp
// Instead of HISM, just spawn actors:
FActorSpawnParameters Params;
AStaticMeshActor* WallActor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, Params);
WallActor->GetStaticMeshComponent()->SetStaticMesh(WallMesh);
WallActor->SetFolderPath(TEXT("Buildings/Building_3/Walls"));
```

Auto-instancing (`r.MeshDrawCommands.DynamicInstancing=1`) will merge identical mesh+material draw calls automatically.

### When This Works

- **< 1000 actors total:** Perfectly fine. Actor overhead is negligible.
- **1000-5000 actors:** Noticeable in editor (outliner lag, selection), runtime still OK.
- **5000-10000 actors:** Editor becomes sluggish. Runtime depends on GPU.
- **> 10000 actors:** Significant CPU overhead from actor management, GC pressure, transform updates.

### When This Breaks Down

The per-actor overhead comes from:
1. **UObject management:** ~1 KB per actor minimum (UObject + AActor + components)
2. **Garbage collection:** GC scans all UObjects; 10K actors = noticeable GC hitches
3. **Transform propagation:** Each actor has its own transform chain
4. **Outliner:** Editor outliner struggles past ~5000 actors
5. **Replication:** NetDriver supports ~10,000 replicated objects max (if multiplayer)

### Verdict: Actors vs HISM for Modular Buildings

| Scenario | Recommendation |
|---|---|
| Prototyping / < 10 buildings | StaticMeshActors (simplest) |
| Production / 10-100 buildings | HISM per building (clean, performant) |
| City scale / 100+ buildings | HISM per block (max performance) |
| Mixed use (some interactive) | HISM for static + Actors for interactive pieces |

---

## 10. Practical Implementation Pattern

### Optimal Spawn Sequence

```cpp
void SpawnBuildingHISM(ABuildingActor* Building, const FBuildingDescriptor& Desc)
{
    // 1. Create HISM components (one per piece type used in this building)
    TMap<UStaticMesh*, UHierarchicalInstancedStaticMeshComponent*> PieceToHISM;

    for (const auto& PiecePlacement : Desc.Pieces)
    {
        UStaticMesh* Mesh = PiecePlacement.Mesh;
        if (!PieceToHISM.Contains(Mesh))
        {
            auto* HISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(Building);
            HISM->SetStaticMesh(Mesh);
            HISM->NumCustomDataFloats = 4;
            HISM->bDisableCollision = true;        // CRITICAL for performance
            HISM->bUseGpuLodSelection = true;      // Per-instance GPU LOD
            HISM->SetupAttachment(Building->GetRootComponent());
            HISM->RegisterComponent();
            PieceToHISM.Add(Mesh, HISM);
        }
    }

    // 2. Batch-add instances per component
    for (auto& [Mesh, HISM] : PieceToHISM)
    {
        // Collect all transforms for this mesh type
        TArray<FTransform> Transforms;
        TArray<float> CustomDataBatch;

        for (const auto& Piece : Desc.Pieces)
        {
            if (Piece.Mesh == Mesh)
            {
                Transforms.Add(Piece.Transform);
                CustomDataBatch.Append({Piece.RoomID, Piece.DecayLevel, 0.f, 0.f});
            }
        }

        // Pre-allocate and batch add
        HISM->PreAllocateInstancesMemory(Transforms.Num());
        TArray<int32> Indices = HISM->AddInstances(Transforms, true, true);

        // Set custom data
        for (int32 i = 0; i < Indices.Num(); i++)
        {
            TArrayView<const float> Data(&CustomDataBatch[i * 4], 4);
            HISM->SetCustomData(Indices[i], Data, i == Indices.Num() - 1);
        }
    }

    // 3. Add simplified collision (separate from HISM)
    for (const auto& Room : Desc.Rooms)
    {
        UBoxComponent* Collision = NewObject<UBoxComponent>(Building);
        Collision->SetBoxExtent(Room.Extents);
        Collision->SetWorldLocation(Room.Center);
        Collision->SetCollisionProfileName(TEXT("BlockAll"));
        Collision->SetupAttachment(Building->GetRootComponent());
        Collision->RegisterComponent();
    }
}
```

### Performance Expectations

For a single building (50-100 pieces, ~15 unique mesh types):
- **Generation time:** 5-20ms (mostly AddInstances + tree build)
- **Draw calls:** 15-30 (one per material per HISM component)
- **GPU memory (instances):** ~5-7 KB
- **Collision:** Near-zero (disabled on HISM, simple boxes per room)

For a city block (10 buildings, ~1000 total pieces):
- **Generation time:** 50-200ms
- **Draw calls:** 150-300 (per-building architecture) or 15-30 (block-wide)
- **GPU memory (instances):** ~64 KB
- **Frame time impact:** < 1ms for instance rendering

---

## 11. Key Findings Summary

1. **Use HISM, not ISM** -- GPU LOD selection and hierarchical culling justify the overhead for spatially distributed building pieces across a town

2. **ALWAYS disable collision on HISM components** -- use simplified box/convex collisions separately. This is the #1 performance trap.

3. **ALWAYS use AddInstances (plural)** -- never loop AddInstance. The batch path avoids per-call overhead.

4. **Memory is not a concern** -- 64 bytes/instance means 50K instances = 3.2 MB. Trivial.

5. **Per-instance custom data works** -- 4 floats is enough for room ID, decay, tint, building ID. Access via VertexInterpolator in materials.

6. **Start with per-building actors** -- simpler code, better outliner, good enough for <100 buildings. Optimize to block-wide HISM only if draw calls become a bottleneck.

7. **Auto-instancing is NOT a replacement** -- 4-5x slower than explicit HISM, no per-instance custom data, no hierarchical culling. Good for prototyping only.

8. **LOD seams are manageable** -- GPU LOD selection + conservative LOD distances + careful LOD authoring handles modular seam issues.

9. **Nanite changes the calculus** -- If using Nanite meshes, ISM is preferred over HISM (Nanite does its own GPU culling, making HISM's tree redundant). Since we have Nanite OFF in this project, HISM is the right choice.

10. **UE 5.7 bUseGpuLodSelection** -- defaults to true, GPU-driven per-instance LOD. Major improvement over CPU-side cluster LOD in older versions.

---

## Sources

### Engine Source (UE 5.7, verified locally)
- `Engine/Classes/Components/HierarchicalInstancedStaticMeshComponent.h` -- Full HISM API
- `Engine/Classes/Components/InstancedStaticMeshComponent.h` -- ISM base class, properties, ID-based API
- `Engine/Public/StaticMeshResources.h` -- `FStaticMeshInstanceData` buffer layout (lines 1025-1688)
- `Engine/Private/InstancedStaticMesh.cpp` -- GPU LOD selection, collision, scene proxy

### Web Sources
- [SM, ISM, HISM, Lightweight Instances and Auto-Instancing differences](https://forums.unrealengine.com/t/sm-ism-hism-lightweight-instances-and-auto-instancing-what-are-the-differences/609954) -- Comprehensive comparison
- [ISM Performance Regression in UE5](https://forums.unrealengine.com/t/what-happened-to-the-instanced-static-mesh-performance-in-ue5/2107140) -- 80K UE4 vs 10K UE5 limits, Chaos physics root cause
- [Per Instance Custom Data Tips](https://forums.unrealengine.com/t/tips-custom-primitive-data-and-per-instance-custom-data/1212364) -- CustomPrimitiveData vs PerInstanceCustomData
- [Per Instance Custom Data Wiki](https://unrealcommunity.wiki/using-per-instance-custom-data-on-instanced-static-mesh-bpiygo0s) -- Setup guide, VertexInterpolator requirement
- [Level Instance vs PLA vs ISM/HISM](https://forums.unrealengine.com/t/when-to-use-level-instance-packed-level-actor-or-ism-hism-in-ue5/2681508) -- FastGeo in 5.6, Nanite ISM preference, runtime cell transformers
- [HISM Collision Issues](https://forums.unrealengine.com/t/hierarchical-instanced-static-mesh-collision-doesnt-work-no-matter-what-i-do/2271817) -- HISM-to-HISM ignored, overlap events broken
- [HISM Draw Calls](https://forums.unrealengine.com/t/hism-doesnt-decrease-draw-calls-it-increases-it/460819) -- Editor measurement artifact, game view shows correct reduction
- [UE5 Storing Custom Data in Materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/storing-custom-data-in-unreal-engine-materials-per-primitive) -- Official Epic docs
- [Auto-Instancing vs ISM Comparison](https://forums.unrealengine.com/t/auto-instancing-vs-ism-comparison-4-24/138105) -- ISM 1.2ms vs auto 5.3ms
- [UE5 Visibility and Occlusion Culling](https://dev.epicgames.com/documentation/en-us/unreal-engine/visibility-and-occlusion-culling-in-unreal-engine) -- Official culling docs
