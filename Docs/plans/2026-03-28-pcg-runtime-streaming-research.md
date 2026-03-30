# PCG Runtime Generation, World Partition, and Streaming for Large Towns

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7
**Context:** Evaluating whether PCG can handle large-scale procedural town generation at runtime with streaming for Leviathan's survival horror environments

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [PCG Generation Modes](#2-pcg-generation-modes)
3. [Hierarchical Generation (HiGen)](#3-hierarchical-generation-higen)
4. [Runtime Generation Scheduler](#4-runtime-generation-scheduler)
5. [World Partition Integration](#5-world-partition-integration)
6. [Generation Budget and Time Slicing](#6-generation-budget-and-time-slicing)
7. [Persistence and Determinism](#7-persistence-and-determinism)
8. [LOD for Distant Buildings](#8-lod-for-distant-buildings)
9. [Scale Limits and Memory](#9-scale-limits-and-memory)
10. [Player-Triggered Interior Generation](#10-player-triggered-interior-generation)
11. [GPU Processing](#11-gpu-processing)
12. [Industry Case Studies](#12-industry-case-studies)
13. [Recommended Architecture for Leviathan](#13-recommended-architecture-for-leviathan)
14. [Sources](#14-sources)

---

## 1. Executive Summary

PCG in UE 5.7 has a **complete runtime generation pipeline** with hierarchical grids, priority-based scheduling, frustum culling, automatic cleanup, and World Partition integration. It can absolutely handle a streaming procedural town -- but with important constraints.

**Key findings:**

- PCG runtime generation is **production-ready** in 5.7 (2x perf vs 5.5)
- HiGen provides 10+ grid levels from 400cm to 204800cm (2km) for multi-scale generation
- Built-in scheduler with distance+direction priority, frustum culling, and time budgets
- **5ms default frame budget** (runtime), **15ms** (editor) -- configurable via CVars
- Automatic cleanup when cells leave generation radius (with configurable hysteresis multiplier 1.1x)
- Partition actor pooling eliminates spawn/destroy churn
- World Partition integration works but has known edge cases with partitioned on-demand generation
- PCG is a **placement engine**, not geometry creation -- buildings must exist as meshes/prefabs first
- GPU compute (5.5+) can spawn instances directly, bypassing CPU entirely for simple scatter
- **Recommended approach:** Hybrid pipeline -- editor-time building generation (GeometryScript/modular), runtime PCG for placement, streaming, and detail population

---

## 2. PCG Generation Modes

Three generation trigger modes, verified from engine source (`PCGComponent.h:71-76`):

```cpp
enum class EPCGComponentGenerationTrigger : uint8
{
    GenerateOnLoad,    // One-shot on BeginPlay
    GenerateOnDemand,  // Explicit Generate() call via Blueprint/C++
    GenerateAtRuntime  // Scheduled by FPCGRuntimeGenScheduler
};
```

### GenerateOnLoad
- Generates once when the component loads (BeginPlay)
- Results serialized to disk -- zero runtime cost after first generation
- **Best for:** Static environment that never changes (pre-baked towns)
- **Streaming:** Generated actors respect World Partition spatial loading

### GenerateOnDemand
- Only generates when explicitly called: `PCGComponent->Generate(true)`
- Use this instead of calling `GenerateLocal()` in Tick
- **Known issue:** Partitioned components fail with `"ScheduleComponent didn't schedule any task"` -- the `PartitionActorsPtrs` is always nullptr when finding partition actors for on-demand generation. This appears to be an unresolved engine limitation as of 5.7.
- **Best for:** Event-triggered generation (player enters area, quest activates)

### GenerateAtRuntime
- Fully managed by `FPCGRuntimeGenScheduler`
- Automatic generation, scheduling, priority ordering, and cleanup
- Integrates with generation sources (player position, camera, custom sources)
- **Best for:** Streaming open-world content that generates/despawns around the player

---

## 3. Hierarchical Generation (HiGen)

HiGen subdivides the PCG generation volume into a hierarchy of grid cells at different scales. Verified from engine source (`PCGCommon.h:366-398`):

### EPCGHiGenGrid Enum (Complete)

| Enum Value | Grid Size (cm) | Grid Size (m) | Use Case |
|-----------|----------------|---------------|----------|
| Grid4 | 400 | 4m | Micro-detail (trash, debris, small props) |
| Grid8 | 800 | 8m | Room-scale detail (furniture, fixtures) |
| Grid16 | 1600 | 16m | Building-interior scale |
| Grid32 | 3200 | 32m | **Default** -- building footprint scale |
| Grid64 | 6400 | 64m | City block scale |
| Grid128 | 12800 | 128m | Neighborhood scale |
| Grid256 | 25600 | 256m | District scale |
| Grid512 | 51200 | 512m | Large district |
| Grid1024 | 102400 | 1024m (1km) | Macro-terrain features |
| Grid2048 | 204800 | 2048m (2km) | Biome/region scale |
| Grid4096-Grid4194304 | (hidden) | Up to ~42km | Internal use |
| Unbounded | special | N/A | Execute once, not on any grid |

Additional constants:
- `GridMin = Grid4` (400cm)
- `GridMax = Grid4194304`
- `NumGridValues = 13` (distinct visible grid levels)

### How HiGen Works

1. Graph nodes are tagged with their execution grid level using Grid Size nodes
2. Larger grids execute first, producing point data that flows down to smaller grids
3. Each grid level creates its own `APCGPartitionActor` to store results
4. Partition actors are spatially loaded independently -- fine-grained streaming
5. Larger grid = generates at greater distance, persists longer
6. Smaller grid = generates only when close, cleaned up quickly

### For Town Generation

```
Grid256 (256m)  -- Town layout: road network, block boundaries
Grid128 (128m)  -- Block layout: building footprints, lot placement
Grid64  (64m)   -- Building shells: exterior meshes, roofs
Grid32  (32m)   -- Building detail: windows, doors, signage
Grid16  (16m)   -- Interior layout: room shells (if entering)
Grid8   (8m)    -- Furniture, fixtures, props
Grid4   (4m)    -- Micro-detail: trash, papers, blood splatters
```

---

## 4. Runtime Generation Scheduler

The `FPCGRuntimeGenScheduler` is the core system that manages generation at runtime. Verified from `PCGRuntimeGenScheduler.h` and `.cpp`.

### Architecture

```
FPCGRuntimeGenScheduler (per world, owned by UPCGSubsystem)
    |
    |-- Tick() called every frame with EndTime budget
    |     |
    |     |-- 1. TickQueueComponentsForGeneration()
    |     |       - Find all GenerateAtRuntime components in range of GenSources
    |     |       - Compute priority via UPCGSchedulingPolicyBase
    |     |
    |     |-- 2. TickCleanup()
    |     |       - Remove cells that left cleanup radius
    |     |       - Time-budgeted -- aborts if EndTime exceeded
    |     |
    |     |-- 3. TickScheduleGeneration()
    |     |       - Schedule highest-priority cells first
    |     |       - Larger grids always higher priority than smaller
    |     |
    |     |-- 4. TickRequestVirtualTexturePriming()
    |     |       - Pre-prime RVTs within generation radius
    |
    |-- GeneratedComponents (TSet<FGridGenerationKey>)
    |     - Tracks all currently generated cells
    |     - Key = (GridSize, GridCoords, OriginalComponent)
    |
    |-- PartitionActorPool (TArray<APCGPartitionActor*>)
          - Dynamically growing pool (doubles when empty)
          - Eliminates spawn/destroy overhead
```

### Generation Sources

Four built-in generation source types (from `RuntimeGen/GenSources/`):

| Source | File | Description |
|--------|------|-------------|
| `PCGGenSourcePlayer` | PCGGenSourcePlayer.h | Player pawn position/direction |
| `PCGGenSourceComponent` | PCGGenSourceComponent.h | Arbitrary component (add to any actor) |
| `PCGGenSourceEditorCamera` | PCGGenSourceEditorCamera.h | Editor viewport camera |
| `PCGGenSourceWPStreamingSource` | PCGGenSourceWPStreamingSource.h | World Partition streaming source |

The `IPCGGenSourceBase` interface provides:
- `GetPosition()` -- world space position
- `GetDirection()` -- normalized forward vector
- `GetViewFrustum()` -- for frustum culling decisions

### Scheduling Policy

`UPCGSchedulingPolicyDistanceAndDirection` is the default policy:

- **Distance-based priority:** Closer cells generate first
- **Direction-based priority:** Cells in front of the player generate first
- **Frustum culling:** Optional -- only generate cells visible in view frustum
- **Network modes:** Client, Server, or All generation source filtering
- **Streaming dependency:** Per-grid option to wait for World Partition streaming completion

```cpp
// Priority = [0, 1], higher = sooner
double CalculatePriority(const IPCGGenSourceBase* InGenSource,
                          const FBox& GenerationBounds, bool bUse2DGrid);
```

### Generation Radii

Each grid size has its own generation radius. Default = `GridSize * 2.0` (the `DefaultGenerationRadiusMultiplier`).

From `PCGCommon.h:452-491`:
- `GenerationRadius` (unbounded): Default = `UnboundedGridSize * 2.0`
- `GenerationRadius400`: `400 * 2.0 = 800cm` (8m)
- `GenerationRadius800`: `800 * 2.0 = 1600cm` (16m)
- ...continuing up to...
- `GenerationRadius204800`: `204800 * 2.0 = 409600cm` (~4.1km)
- `CleanupRadiusMultiplier`: Default `1.1` -- cleanup happens at 10% beyond generation radius

All radii are overridable per-component via `bOverrideGenerationRadii`.

Global radius multiplier CVar: `pcg.RuntimeGeneration.GlobalRadiusMultiplier` (default 1.0)

---

## 5. World Partition Integration

### How It Works

1. PCG components marked as **"Is Partitioned"** split their generation into grid cells
2. Each cell creates an `APCGPartitionActor` to hold generated content
3. These partition actors are **spatially loaded** -- they follow World Partition streaming rules
4. When the player moves close enough, the WP cell loads, the partition actor loads, generation triggers
5. When the player moves away, the cell unloads, cleaning up generated content

### Configuration

- `bIsPartitioned` on UPCGComponent -- enables grid subdivision
- `bIsSpatiallyLoaded` -- partition actors respect WP streaming distances
- Grid cell size determined by PCGWorldActor (non-HiGen) or graph HiGen settings
- Runtime grids define how actors get placed into cells

### Streaming Dependency Modes

From `UPCGSchedulingPolicyBase` (`PCGSchedulingPolicyBase.h`):

```cpp
enum class EPCGGridStreamingDependencyMode
{
    AllGridsExceptUnbounded = 0,  // Default - wait for WP streaming on all grids except unbounded
    AllGrids,                      // Wait for streaming on all grids including unbounded
    SpecificGrids,                 // Only wait for specified grid sizes
    NoGrids,                       // Never wait for streaming
};
```

This is critical: if your PCG graph depends on landscape data or other streamed actors, you MUST enable streaming dependency or you'll generate against unloaded data.

### Known Issues

1. **On-demand + partitioned = broken:** `PartitionActorsPtrs` is always nullptr when attempting on-demand generation on partitioned components. Confirmed by multiple developers.
2. **Cell size scaling:** Halving cell size = 4x more cells. Performance impact is quadratic.
3. **Cleanup required between changes:** When modifying partitioned PCG, clean all content first, then regenerate.
4. **Data layers + HiGen:** Reported to break runtime hierarchical generation features in some configurations.

### World Partition Streaming Queries

CVar `pcg.RuntimeGeneration.EnableWorldStreamingQueries` controls whether the scheduler checks if nearby WP cells are fully streamed before generating.

---

## 6. Generation Budget and Time Slicing

### Frame Time Budget

Verified from engine source (`PCGGraphExecutor.cpp`):

```cpp
// Runtime (PIE/packaged):
TAutoConsoleVariable<float> CVarTimePerFrame(
    "pcg.FrameTime", 5.0f,  // 5ms per frame
    "Allocated time in ms per frame");

// Editor (non-PIE):
TAutoConsoleVariable<float> CVarEditorTimePerFrame(
    "pcg.EditorFrameTime", 15.0f,  // 15ms per frame
    "Allocated time in ms per frame when running in editor (non pie)");
```

At 60fps, 5ms = 30% of frame time. This is the total budget for ALL PCG work (scheduling + graph execution + cleanup).

### Async Time Slicing

Processors can be split across frames:

```cpp
// Per-task out-of-tick budget:
TAutoConsoleVariable<float> CVarAsyncOutOfTickBudgetInMilliseconds(
    "pcg.Async.OutOfTickBudgetInMilliseconds", 5.0f,
    "Allocated time in milliseconds per task when running async tasks out of tick");

// Debug CVars:
"pcg.DisableAsyncTimeSlicing"              // Force synchronous (debug)
"pcg.DisableAsyncTimeSlicingOnGameThread"  // Force synchronous on GT (debug)
```

### Cleanup Time Budgeting

The `TickCleanup` function checks `FPlatformTime::Seconds() >= InEndTime` after each component cleanup. If exceeded, it aborts and resumes next frame:

```
"FPCGRuntimeGenScheduler: Time budget exceeded, aborted after cleaning up %d / %d components"
```

### Practical Budget Implications for Town Generation

- **5ms total** shared between ALL PCG tasks
- A complex building-placement graph with 1000 points might take 20-50ms
- With time slicing, this gets spread across 4-10 frames (barely noticeable)
- Cleanup of 100 cells with HISM instances: ~1-5ms per cell
- **Recommendation:** Keep per-cell complexity low. Use pre-baked building meshes, not runtime geometry generation.

---

## 7. Persistence and Determinism

### Seed System

PCG is fully deterministic via seeds:

```cpp
// UPCGComponent
UPROPERTY(BlueprintReadWrite, EditAnywhere)
int Seed = 42;

// FPCGPoint
int32 Seed = 0;  // Per-point deterministic seed
```

Same seed + same graph + same inputs = identical output every time. This is the foundation of PCG persistence.

### Save/Load Strategies

**Strategy 1: Seed-Only Persistence (Recommended)**
- Save only the world seed and any player modifications (destroyed buildings, opened doors)
- On load, regenerate from seed -- identical results guaranteed
- Save file size: minimal (seed + delta list)
- Load time: depends on generation budget, can stream over multiple frames

**Strategy 2: Serialized Results**
- Generated content serialized to disk (editor-time generation with `GenerateOnLoad`)
- Partition actors saved as normal WP actors
- Load via standard WP streaming
- Disk size: potentially large for a full town

**Strategy 3: Hybrid (Best for Horror)**
- Building layout: seed-based regeneration (deterministic)
- Player modifications: delta save (broken windows, moved furniture, blood)
- Interior detail: runtime generation on enter (ephemeral, no save needed)

### GenerateOnLoad Serialization

Components with `GenerateOnLoad` + `bIsSpatiallyLoaded` serialize their results. These load like normal actors via WP streaming -- zero runtime generation cost.

---

## 8. LOD for Distant Buildings

PCG does not directly handle LOD, but several approaches integrate well:

### Approach 1: HiGen Multi-Resolution (Native PCG)

Use different mesh assets at different grid levels:
- Grid256 (far): Low-poly building silhouettes / imposters
- Grid64 (mid): Medium-detail exterior shells
- Grid32 (near): Full-detail buildings with interiors

As the player approaches, larger grid content persists while smaller grids add detail.

### Approach 2: Stand-In / Proxy Meshes (Simplygon)

Replace distant buildings with cheap proxy meshes using visibility culling. The Proxy LOD tool specifically targets "doors and windows in distant buildings" with dilation/erosion.

### Approach 3: Fast Geometry Streaming (UE 5.6+, Experimental)

New plugin that handles streaming of static geometry (buildings, signs) more intelligently than World Partition. Works alongside WP -- targets non-gameplay geometry specifically.

### Approach 4: HLOD (Hierarchical LOD)

World Partition's HLOD system automatically creates simplified merged meshes for distant cell clusters. Works with PCG-generated content if generated at editor time.

### Recommended for Leviathan

HiGen multi-resolution with 3 tiers:
1. **Distant (>256m):** Building block silhouettes from Grid128/256
2. **Medium (64-256m):** Exterior-only building shells from Grid64
3. **Close (<64m):** Full buildings with interiors from Grid32 and below

---

## 9. Scale Limits and Memory

### How Many Buildings?

**HISM Performance (verified from community data and our prior research):**
- 10,000 instances with collision: viable but taxing
- 500,000+ instances without collision: proven (Fortnite foliage)
- UE5 ISM performance is ~worse than UE4 for Add/Remove operations (5.4/5.5 regression)
- Instance buffer: 48-88 bytes per instance (4 streams)
- 10,000 instances at 88 bytes = ~860 KB

**Town Scale Estimates:**

| Scale | Buildings | Unique Meshes | HISM Instances | Memory (instances) |
|-------|----------|--------------|----------------|-------------------|
| Small town | 50-100 | 10-20 | 5,000-10,000 | ~1-2 MB |
| Medium town | 200-500 | 20-40 | 20,000-50,000 | ~4-10 MB |
| Large city | 1,000-5,000 | 40-80 | 100,000-500,000 | ~20-100 MB |

With streaming, only nearby cells are loaded. A large city with 64m grid cells:
- At any time, ~50-100 cells loaded (within generation radius)
- ~500-5,000 HISM instances active at any moment
- Memory is bounded by visible area, not total town size

### Texture Streaming

- UE5's virtual texture streaming handles texture memory automatically
- Buildings sharing material instances = shared textures, minimal overhead
- RVT (Runtime Virtual Textures) can composite landscape + building detail maps

### Actor Count

World Partition groups actors into streaming levels per runtime grid cell:
- Actors aren't loaded individually (too slow)
- Grouped by grid + data layer at edit-time
- 150-500 actors per cell is a reasonable target
- PCG's HISM spawner batches hundreds of instances into single components

### Partition Actor Pool

The runtime scheduler maintains a pool of `APCGPartitionActor` objects:
- Pool doubles when empty (geometric growth)
- Controlled by `pcg.RuntimeGeneration.BasePoolSize` (CVar)
- `pcg.RuntimeGeneration.EnablePooling` (default true)
- Eliminates actor spawn/destroy overhead entirely

---

## 10. Player-Triggered Interior Generation

### The Pattern

Generate building interiors only when the player enters or approaches a door. This is the most memory-efficient approach for a horror game where interiors matter.

### Implementation Options

**Option A: PCG GenerateOnDemand (Non-Partitioned)**
```
Player approaches door
  -> Overlap trigger fires
  -> PCGComponent->Generate(true)  // On-demand, non-partitioned
  -> Interior populates (furniture, props, items)
  -> Player enters
  -> On exit + distance: PCGComponent->Cleanup()
```
Non-partitioned on-demand works fine. The partitioned on-demand bug does not apply.

**Option B: Dynamic Level Instances**
```
Player approaches door
  -> ULevelStreamingDynamic::LoadLevelInstance()
  -> Pre-authored or PCG-decorated sublevel loads asynchronously
  -> OnLevelShown callback fires
  -> Player enters
  -> On exit: UnloadLevelInstance()
```
- Uses standard Unreal streaming (no frame hitch)
- Level Instance = modular "room" or "floor" level
- Multiple instances of same level at different transforms
- `FLoadLevelInstanceParams` struct for C++ control
- Multiplayer: `Replicated Sublevel Instances` plugin available

**Option C: HiGen Grid Hierarchy**
```
Grid64  -> Building exterior (always generated when nearby)
Grid16  -> Interior shell (generates at 32m range)
Grid8   -> Furniture/props (generates at 16m range)
Grid4   -> Micro-detail (generates at 8m range)
```
Pure HiGen approach -- no manual trigger needed. Distance-based automatic generation.

### Recommended for Horror

**Option A + C hybrid:**
- HiGen for exterior (always visible when nearby)
- Manual trigger (door interaction) generates interior via on-demand
- Interior has its own PCG component that populates rooms with horror props
- Horror-specific: generate different interiors based on fear state / AI director input
- Cleanup on exit to save memory

---

## 11. GPU Processing

### PCG GPU Compute (5.5+, Production in 5.7)

- Execute PCG graph nodes on the GPU via compute shaders
- Custom HLSL nodes for point generation
- Direct GPU instancing via `Procedural Instanced StaticMesh Component`
- Bypasses CPU entirely for spawn -- instances go straight to GPU instance buffer

### Limitations
- No collision support for GPU-spawned instances
- No indirect lighting
- Limited to supported node types
- Best for visual-only scatter (grass, debris, particles)

### For Town Generation
GPU compute is ideal for:
- Ground clutter (trash, leaves, debris)
- Distant foliage/vegetation
- Window details, rooftop detail meshes
- Anything that doesn't need collision or gameplay interaction

NOT for:
- Building shells (need collision)
- Furniture (need interaction)
- Doors/windows (need gameplay hooks)

---

## 12. Industry Case Studies

### LEGO Fortnite
- 95 km^2 playable space (19x Fortnite BR island)
- World Partition streams the entire world
- PCG + Biome Core for environment population
- Runtime Hierarchical Generation for real-time detail generation
- GDC 2024: Chaos Physics talk covered destruction system

### Shadows of Doubt
- Fully procedural city: City > District > Block > Building > Floor > Address > Room > Tile
- 100% real-time lighting (no bake possible with proc-gen)
- Light layers per building (interior lights only affect interior)
- "When we save, we have to save everything" -- full state serialization
- Memory leak issues after ~2 hours (proc-gen memory management is hard)
- Biggest perf problem: "things popping in and out of existence" outdoors

### Calysto World / Calysto Village (Marketplace)
- PCG-based procedural world generation
- Seamless World Partition integration out of box
- Level Instance spawning for buildings (POI system)
- PCG partition enabled by default for performance

### The Matrix Awakens / City Sample
- Full procedural city with buildings, vehicles, MetaHumans
- Nanite for building geometry
- World Partition for streaming
- Not using PCG (pre-dates PCG framework) -- custom placement

### Key Takeaways
1. Runtime proc-gen at city scale works but requires careful memory management
2. Light management is a major challenge (real-time only)
3. Save systems need special handling -- seed-based regeneration preferred
4. Outdoor streaming is the hardest part -- interiors are easier to isolate
5. Light layers per building are essential for performance

---

## 13. Recommended Architecture for Leviathan

### Hybrid Pipeline: Editor-Time Buildings + Runtime PCG Placement

```
PHASE 1: EDITOR TIME (Monolith MCP)
|
|-- GeometryScript generates building meshes (existing pipeline)
|-- OR modular kit pieces assembled into building templates
|-- Save as UStaticMesh / Level Instance assets
|-- These are the "vocabulary" for PCG placement
|
PHASE 2: EDITOR TIME (PCG Graphs)
|
|-- PCG graph defines town layout rules
|-- Grid256: Road network / block boundaries
|-- Grid128: Building footprint placement (spawn building mesh/level instance)
|-- Grid64: Exterior detail (street furniture, vehicles, fences)
|-- Generate with "GenerateOnLoad" -- results serialized to disk
|-- World Partition assigns cells automatically
|
PHASE 3: RUNTIME (Automatic Streaming)
|
|-- World Partition streams cells as player moves
|-- Pre-generated content loads seamlessly
|-- Zero runtime generation cost for static town
|
PHASE 4: RUNTIME (Dynamic Detail)
|
|-- PCG components with "GenerateAtRuntime" for:
|     |-- Grid8: Interior furniture/props (when player is within 16m)
|     |-- Grid4: Micro-detail (when within 8m)
|     |-- Horror atmosphere: fog, particles, blood, evidence
|
|-- On-demand generation for:
|     |-- Building interiors (door interaction trigger)
|     |-- Horror events (AI director triggers generation)
|     |-- Destructible elements (player modifies environment)
|
PHASE 5: GPU SCATTER (Visual Polish)
|
|-- GPU compute for:
|     |-- Ground debris, leaves, papers
|     |-- Distant vegetation
|     |-- Atmospheric particles
|     |-- No collision needed
```

### CVars to Configure

```ini
; Runtime frame budget (default 5ms, may need tuning)
pcg.FrameTime=5.0

; Editor frame budget
pcg.EditorFrameTime=15.0

; Async task budget
pcg.Async.OutOfTickBudgetInMilliseconds=5.0

; Runtime generation
pcg.RuntimeGeneration.Enable=true
pcg.RuntimeGeneration.EnablePooling=true
pcg.RuntimeGeneration.BasePoolSize=32
pcg.RuntimeGeneration.GlobalRadiusMultiplier=1.0
pcg.RuntimeGeneration.EnableWorldStreamingQueries=true
pcg.RuntimeGeneration.FramesBeforeFirstGenerate=5
pcg.RuntimeGeneration.EnableChangeDetection=true

; Debug
pcg.RuntimeGeneration.EnableDebugging=false
pcg.RuntimeGeneration.EnableDebugOverlay=false
pcg.RuntimeGeneration.HideActorsFromOutliner=true
```

### Grid Size Recommendations for Horror Town

| Grid | Size | Radius | Content | Mode |
|------|------|--------|---------|------|
| Grid256 | 256m | 512m | Town blocks, roads | GenerateOnLoad |
| Grid128 | 128m | 256m | Building footprints | GenerateOnLoad |
| Grid64 | 64m | 128m | Building exteriors | GenerateOnLoad |
| Grid32 | 32m | 64m | Doors, windows, signage | GenerateOnLoad |
| Grid16 | 16m | 32m | Interior shells | GenerateAtRuntime |
| Grid8 | 8m | 16m | Furniture, props | GenerateAtRuntime |
| Grid4 | 4m | 8m | Horror micro-detail | GenerateAtRuntime |

### Persistence Strategy

1. **Town layout:** Seed-based. Save seed, regenerate identically on load.
2. **Player modifications:** Delta save. Track destroyed/moved objects by PCG point ID.
3. **Horror state:** AI director state saved. Horror props regenerated from director state.
4. **Interior state:** Ephemeral. Interior detail regenerated on enter (cheap, fast).
5. **Evidence/clues:** Explicit save. Quest-relevant items saved by position, not PCG.

### Horror-Specific Features

- **AI Director integration:** Pass horror intensity parameter to PCG graphs, affecting prop density, lighting, and atmosphere generation
- **Dynamic fog/atmosphere:** Runtime PCG scatter with horror level as input
- **Evidence placement:** PCG generates potential evidence locations, game system selects which activate
- **Ambient sound sources:** PCG places audio triggers based on room type and horror level
- **Escape route management:** PCG respects navigation requirements (no blocking critical paths)

### Estimated Implementation Timeline

| Phase | Work | Hours |
|-------|------|-------|
| 1. Building vocabulary pipeline | Existing (GeometryScript) + Level Instance templates | 20-30h |
| 2. Town layout PCG graphs | Road network, block subdivision, building placement | 40-60h |
| 3. World Partition setup | Grid configuration, streaming layers, HLOD | 16-24h |
| 4. Runtime detail system | Interior generation, horror atmosphere, cleanup | 30-40h |
| 5. GPU scatter | Debris, vegetation, atmospheric effects | 12-16h |
| 6. Persistence | Seed save, delta tracking, load regeneration | 16-24h |
| 7. Horror integration | AI director hooks, fear-responsive generation | 20-30h |
| **Total** | | **154-224h** |

### Risks and Mitigations

| Risk | Severity | Mitigation |
|------|----------|------------|
| On-demand + partitioned bug | High | Use non-partitioned for on-demand, partitioned only for GenerateAtRuntime |
| 5ms frame budget insufficient | Medium | Profile early, tune `pcg.FrameTime`, keep per-cell complexity low |
| Memory pressure with many loaded cells | Medium | Aggressive cleanup radii, GPU scatter for visual-only content |
| Real-time lighting performance | High | Light layers per building (Shadows of Doubt pattern), distance-based light deactivation |
| Save file size | Low | Seed-based regeneration keeps saves tiny |
| Interior generation hitch | Medium | Pre-generate interior shell at Grid16 radius, detail at Grid8, spread across frames |

---

## 14. Sources

### Official Documentation
- [PCG Generation Modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine) -- GenerateOnLoad/OnDemand/AtRuntime
- [Hierarchical Generation](https://dev.epicgames.com/documentation/en-us/unreal-engine/hierarchical-generation) -- HiGen grid system
- [Runtime Hierarchical Generation](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-hierarchical-generation) -- Runtime HiGen specifics
- [PCG with World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-world-partition-in-unreal-engine) -- WP integration
- [FPCGRuntimeGenScheduler API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/PCG/FPCGRuntimeGenScheduler) -- Scheduler API
- [PCG GPU Processing](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine) -- GPU compute
- [PCG Biome Core](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine) -- Biome sample plugin
- [World Partition](https://dev.epicgames.com/documentation/en-us/unreal-engine/world-partition-in-unreal-engine) -- WP overview
- [Level Instancing](https://dev.epicgames.com/documentation/en-us/unreal-engine/level-instancing-in-unreal-engine) -- Dynamic level instances

### Forum Discussions
- [PCG Runtime Generation](https://forums.unrealengine.com/t/pcg-runtime-generation/2648015) -- Runtime generation issues
- [PCG Hierarchical Generation](https://forums.unrealengine.com/t/pcg-hierarchical-generation/1602888) -- HiGen discussion
- [PCG with World Partition](https://forums.unrealengine.com/t/pcg-with-world-partition/1254155) -- WP integration issues
- [Partitioned On-Demand Bug](https://forums.unrealengine.com/t/partitioned-pcg-component-does-not-generate-on-demand-and-other-bugs/2220564) -- On-demand generation failure
- [Data Layers Break HiGen](https://forums.unrealengine.com/t/data-layers-seem-to-break-runtime-pcg-hierarchical-generation-features/2541972) -- Data layer conflicts

### Community Resources
- [Dynamic Level Instances for Procedural Levels](https://www.quodsoler.com/blog/using-dynamic-level-instances-to-create-procedural-levels-in-unreal-engine-5) -- Level instance spawning guide
- [First We Make Manhattan](https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/) -- PCG city generation series
- [80.lv Runtime PCG Testing](https://80.lv/articles/testing-out-ue5-s-procedural-content-generation-framework-at-runtime) -- Runtime PCG experiments
- [UE5 Open World Performance](https://forums.unrealengine.com/t/community-tutorial-ue5-open-world-performance-optimization/2680579) -- Optimization guide
- [World Partition Internals](https://xbloom.io/2025/10/24/unreals-world-partition-internals/) -- WP deep dive

### Industry Case Studies
- [Shadows of Doubt DevBlog 30](https://colepowered.com/shadows-of-doubt-devblog-30-the-top-shadows-of-doubt-development-challenges-part-i/) -- Procedural city challenges
- [Shadows of Doubt DevBlog 13](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/) -- Procedural interior generation
- [Calysto World](https://www.fab.com/listings/8631308a-67a3-4e20-b3e4-74be19813f77) -- PCG world generation plugin
- [CityBLD Tutorial](https://dev.epicgames.com/community/learning/tutorials/wP8j/unreal-engine-creating-procedural-urban-environments-with-citybld-a-step-by-step-ue-guide-part-1) -- Procedural urban environments

### Engine Source (UE 5.7, verified)
- `PCGCommon.h:366-398` -- EPCGHiGenGrid enum definition
- `PCGCommon.h:438-494` -- FPCGRuntimeGenerationRadii struct
- `PCGComponent.h:71-76` -- EPCGComponentGenerationTrigger enum
- `PCGComponent.h:318-364` -- Runtime generation properties
- `PCGRuntimeGenScheduler.h` -- Complete scheduler architecture
- `PCGRuntimeGenScheduler.cpp:50-130` -- All runtime generation CVars
- `PCGGraphExecutor.cpp:64-67` -- Frame time budget CVars (5ms/15ms)
- `PCGAsync.cpp:13-29` -- Async time slicing CVars
- `PCGSchedulingPolicyBase.h` -- Scheduling policy base class + streaming dependency modes
- `PCGSchedulingPolicyDistanceAndDirection.h` -- Default scheduling policy
- `PCGGenSourceBase.h` -- Generation source interface
