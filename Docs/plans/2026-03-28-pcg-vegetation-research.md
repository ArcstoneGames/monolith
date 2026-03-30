# PCG Vegetation, Foliage, and Natural Environment for Horror FPS

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready, Nanite Foliage Experimental, PVE Experimental)
**Context:** Outdoor natural environment generation for Leviathan's horror survival FPS town setting
**Depends on:** PCG Framework Research (`2026-03-28-pcg-framework-research.md`), City Block Layout (`2026-03-28-city-block-layout-research.md`), Spatial Registry (`2026-03-28-spatial-registry-research.md`)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [UE 5.7 Vegetation Tool Landscape](#2-ue-57-vegetation-tool-landscape)
3. [PCG Surface Sampling for Grass and Weeds](#3-pcg-surface-sampling-for-grass-and-weeds)
4. [Tree and Bush Placement](#4-tree-and-bush-placement)
5. [Overgrowth System -- Ivy, Moss, Vines](#5-overgrowth-system----ivy-moss-vines)
6. [Ground Cover -- Litter, Mud, Debris, Snow](#6-ground-cover----litter-mud-debris-snow)
7. [PCG + Landscape Integration](#7-pcg--landscape-integration)
8. [Foliage Instancing and Spawning](#8-foliage-instancing-and-spawning)
9. [Horror-Specific Vegetation](#9-horror-specific-vegetation)
10. [Performance Budget and Optimization](#10-performance-budget-and-optimization)
11. [Existing PCG Vegetation Examples](#11-existing-pcg-vegetation-examples)
12. [Integration with Town Generator](#12-integration-with-town-generator)
13. [Fog and Atmosphere Zones](#13-fog-and-atmosphere-zones)
14. [Proposed MCP Actions](#14-proposed-mcp-actions)
15. [Implementation Plan](#15-implementation-plan)
16. [Sources](#16-sources)

---

## 1. Executive Summary

PCG is the right tool for outdoor vegetation in Leviathan. It is a **placement and distribution engine** -- it does not generate geometry, but it excels at scattering existing foliage assets across landscapes with rule-based control over density, slope, biome, exclusion zones, and decay parameters.

**Key findings:**

- **PCG is production-ready in UE 5.7** with ~2x performance over 5.5, GPU compute support, and a dedicated PCG Editor Mode
- **Three vegetation systems exist in UE 5.7:** Landscape Grass (cheapest, grass only), PCG (flexible rule-based), and Procedural Vegetation Editor / PVE (experimental, generates tree meshes procedurally via node graph)
- **Nanite Foliage is experimental** -- voxelized skeletal mesh trees with wind animation, ~2x perf over traditional, but not production-ready until estimated mid-2026
- **PCG Biome plugins** ship with UE 5.7 -- demonstrate landscape-layer-driven vegetation with Attribute Set Tables and feedback loops
- **Exclusion zones** work via Difference node (subtract building footprints/roads from spawn regions) and Get Actor Data for dynamic exclusion
- **Horror vegetation** is primarily an art direction problem (dead trees, brown/grey palettes, sparse placement, overgrowth on buildings) that PCG parameters can drive via a single `decay` float (0.0 = maintained, 1.0 = abandoned/dead)
- **Overgrowth on buildings** requires either Crazy Ivy plugin ($30-50 on Fab, used in Silent Hill FS) or custom spline-mesh vine system -- PCG alone cannot grow geometry on surfaces
- **Fog zones** can be spawned as Local Volumetric Fog actors via PCG Spawn Actor node, placed at low-lying areas, near water, in alleys

**Estimated effort:** ~60-85h across 5 phases

---

## 2. UE 5.7 Vegetation Tool Landscape

UE 5.7 has three distinct vegetation/foliage placement systems plus a new procedural tree generator. Understanding which tool does what is critical:

### 2.1 Landscape Grass System (Built-in, Production)

- Cheapest option for dense ground cover (grass, small flowers)
- Driven by landscape material layers -- paint a layer, grass appears
- Uses Grass Types (data assets mapping layers to meshes)
- Renders via GPU instancing, no foliage actor overhead
- **Limitation:** No slope filtering, no exclusion zones, no arbitrary rules, no non-grass meshes
- **Use case:** Base grass layer only

### 2.2 PCG Framework (Built-in, Production-Ready in 5.7)

- Node-based graph for rule-based asset placement
- Supports: Surface Sampler, slope/density filtering, exclusion zones, biome control, spline meshes, actor spawning, arbitrary attributes, subgraphs, GPU compute
- ~100+ node types, extensible via C++ custom nodes
- Deterministic (seed-based), supports runtime generation
- **Limitation:** Does not generate geometry -- only places existing assets
- **Use case:** All vegetation placement beyond basic grass, debris scatter, fog placement, horror atmosphere

### 2.3 Procedural Foliage Spawner / PFS (Built-in, Legacy)

- Older volume-based system from UE4 era
- Uses Procedural Foliage Spawner volumes with FoliageType assets
- Supports growth simulation (seed radius, shade tolerance, overlap priority)
- Less flexible than PCG, no landscape layer reading, no spline support
- **Use case:** Largely superseded by PCG. Could still work for simple forest fills.

### 2.4 Procedural Vegetation Editor / PVE (New in 5.7, Experimental)

- Graph-based tool for **generating tree mesh geometry** inside UE5
- Outputs Nanite skeletal assemblies or static meshes
- Uses PCG-based node graph internally
- Ships with Quixel Megaplants assets (5 species, multiple variations)
- **Limitation:** Experimental, production release estimated mid-2026
- **Use case:** Creating custom tree variations without SpeedTree/external DCC. Interesting for horror dead-tree generation but too early to depend on.

### 2.5 Nanite Foliage (5.7, Experimental)

- Voxelized rendering system for trees as skeletal meshes
- Distant trees transition to voxel representation (preserves silhouette/volume)
- Wind via bone-based skeletal animation system
- One scene: 62 FPS -> 119 FPS rendering 77,376 trees at 20M polygons each
- **Limitation:** Experimental. Leviathan has Nanite OFF in renderer config.
- **Use case:** Future consideration if Nanite is enabled. Not for current build.

### Recommendation for Leviathan

| Layer | Tool | Content |
|-------|------|---------|
| Base grass | Landscape Grass System | Dense ground cover, cheapest |
| Vegetation placement | PCG | Trees, bushes, weeds, flowers, dead plants |
| Ground cover / debris | PCG | Leaf litter, mud patches, trash, rubble |
| Building overgrowth | Crazy Ivy OR custom | Ivy, vines, moss on surfaces |
| Fog / atmosphere | PCG + manual | Local volumetric fog, particle spawners |
| Tree generation | SpeedTree / PVE (future) | Create dead tree variations |

---

## 3. PCG Surface Sampling for Grass and Weeds

### 3.1 Core Pipeline

The fundamental PCG vegetation scatter follows this node chain:

```
Surface Sampler -> Normal To Density -> Density Filter -> Transform Points -> Static Mesh Spawner
```

**Surface Sampler** (`PCGSurfaceSampler`):
- Samples points on the landscape within the PCG volume bounds
- Key parameters:
  - `PointsPerSquaredMeter` -- density control (typical: 0.1-10.0)
  - `PointExtents` -- minimum spacing between points (bounds-based self-pruning)
  - `Looseness` -- randomization of point positions (0 = grid, 1 = random)
- Each output point has: Transform, Density, Normal, Steepness, Seed, BoundsMin/Max, Color

**Normal To Density** (`PCGNormalToDensity`):
- Converts surface normal to density value
- Flat surface (normal pointing up) -> density 1.0
- Vertical cliff (normal horizontal) -> density 0.0
- Slopes get intermediate values based on dot product with up vector

**Density Filter** (`PCGDensityFilter`):
- Removes points outside a density range
- For grass: LowerBound=0.5, UpperBound=1.0 (keeps surfaces flatter than ~60 degrees)
- For weeds-in-cracks: LowerBound=0.0, UpperBound=0.3 (only steep/narrow surfaces)

**Transform Points** (`PCGTransformPoints`):
- Randomize rotation (Yaw: 0-360), scale (0.8-1.2), offset
- Align to surface normal or world up

### 3.2 Density Maps via Attributes

For varying density across the town (more weeds near abandoned buildings, less near maintained areas):

```
Surface Sampler -> Get Landscape Data -> Attribute Filter (by layer weight) -> Density Remap -> ...
```

Or use a **Texture Sampler** with a hand-painted or procedurally-generated density map:

```
Surface Sampler -> Texture Sampler (density_map) -> Multiply Density -> Density Filter -> ...
```

### 3.3 Exclusion Zones

Three approaches for keeping vegetation out of buildings, roads, and sidewalks:

**A) Difference Node (Primary)**
```
Surface Sampler -----> Difference -----> Static Mesh Spawner
                          ^
Get Actor Data (buildings/roads) --/
```
The Difference node removes all points that overlap with the bounds of the exclusion actors. Works with any actor that has collision/bounds.

**Known issue in UE 5.6:** The Difference node had a regression where it stopped working with GetActorData. Verify in 5.7 -- likely fixed.

**B) Point Filter by Tag**
Tag building actors with `PCG_Exclude`, use `Filter By Tag` to remove points near tagged actors.

**C) Spline-Based Exclusion**
For roads/paths defined by splines, use spline-to-surface conversion + Difference:
```
Get Spline Data (road) -> Create Surface From Spline -> Difference input
```

### 3.4 Weeds-Through-Cracks Pattern

For the specific horror aesthetic of vegetation growing through cracked pavement:

1. Sample points on road/sidewalk surfaces (not landscape)
2. Use a noise attribute to create random density variation
3. Filter to very low density (sparse placement)
4. Spawn small weed meshes (dandelions, grass tufts, moss patches)
5. Modulate by `decay` parameter -- more weeds at higher decay

```
Point From Mesh (sidewalk) -> Attribute Noise -> Density Filter (very sparse)
    -> Scale by Decay -> Static Mesh Spawner (weeds)
```

---

## 4. Tree and Bush Placement

### 4.1 Dead Trees for Horror

Trees in a horror setting need to feel wrong. Design principles:

- **Sparse placement** -- fewer trees than a normal town, creating exposed sightlines that feel vulnerable
- **Dead/dying variants** -- leafless, broken branches, dark bark, tilted/leaning
- **Clustered placement** -- trees in tight groups create dark pockets; isolated trees on open ground create silhouette horror
- **Sightline blocking** -- strategic placement to obscure enemy approach routes
- **Scale variation** -- oversized trees feel oppressive, undersized feel sickly

### 4.2 PCG Tree Scatter Graph

```
Surface Sampler (low density: 0.01-0.05 pts/m2)
    -> Normal To Density (slope filter)
    -> Density Filter (LB: 0.7, UB: 1.0 -- flat ground only)
    -> Difference (exclude buildings, roads)
    -> Self Pruning (min distance: 300-500cm between trees)
    -> Attribute Noise (density variation for clusters)
    -> Transform Points (random rotation, scale 0.7-1.3)
    -> Static Mesh Spawner (mesh selector: by attribute)
```

**Mesh Selection by Decay Parameter:**

Use `Match And Set Attributes` or `Attribute Filter` to select tree meshes based on the global `decay` parameter:

| Decay Range | Tree Type | Example Meshes |
|-------------|-----------|----------------|
| 0.0-0.2 | Healthy deciduous | Oak, maple with full canopy |
| 0.2-0.5 | Stressed | Sparse leaves, some dead branches |
| 0.5-0.8 | Dying | Mostly bare, broken limbs, dark bark |
| 0.8-1.0 | Dead | Leafless skeleton, tilted, stumps |

### 4.3 Bush and Hedge Placement

Bushes serve gameplay purposes in horror:
- **Overgrown hedges** block sightlines and create blind corners
- **Low bushes** provide partial concealment (crouch behind)
- **Thorny brambles** serve as soft barriers (damage + slow)

Place along:
- Building perimeters (overgrown yards)
- Fence lines (breaking through/over fences)
- Road edges (encroaching on pavement)
- Garden areas (gone wild)

Use spline-based placement for hedge rows:
```
Spline Data (fence/property line) -> Spline Mesh Spawner (hedge segments)
```

### 4.4 Sightline Management for Horror

PCG can integrate with the spatial registry to enforce horror sightline rules:

- **Near enemy spawn points:** Place dense tree clusters to obscure approach
- **Along main paths:** Alternate between open and occluded segments
- **Near buildings:** Overgrown vegetation creates transition zones where threats could hide
- **Dead ends:** Dense vegetation walls that funnel the player

This requires custom PCG nodes or post-generation validation, not stock PCG.

---

## 5. Overgrowth System -- Ivy, Moss, Vines

### 5.1 The Problem

PCG places assets **on surfaces** (landscape, meshes) but cannot grow geometry along walls, wrap around corners, or drape from overhangs. Overgrowth on buildings requires specialized tools.

### 5.2 Option A: Crazy Ivy Plugin (Recommended)

**Crazy Ivy** is a commercial UE plugin for procedural ivy/vine generation.

**Credentials:**
- Used in Konami's **Silent Hill FS** and **Phantom Blade Zero**
- Selected for Epic's Showcase rotation
- Featured at UNREAL FEST 2023 TOKYO

**Features:**
- Automatic surface detection -- ivy grows along mesh surfaces
- Configurable: vine thickness, growth speed, leaf type/size, gravity, adhesion, density
- Multiple species: Hedera Helix, Glacier, Boston Ivy, leafless vines
- Animated leaves (World Position Offset wind)
- Works with baked lighting
- Fast and optimized

**Integration with Leviathan:**
- Apply to building exteriors post-generation
- Modulate growth density by `decay` parameter
- Decay 0.0-0.3: No ivy
- Decay 0.3-0.6: Sparse ivy on foundations and lower walls
- Decay 0.6-0.8: Heavy ivy coverage, reaching upper floors
- Decay 0.8-1.0: Entire facade covered, vines hanging from roof
- Could be triggered via MCP action: `apply_overgrowth(building_id, decay)`

**Price:** ~$30-50 on Fab (check current listing). Commercially safe for Steam release.

### 5.3 Option B: PCG Procedural Vines (Community Tutorial)

Epic's community tutorial demonstrates procedural vines on static meshes using PCG:
- Sample points on mesh surfaces via `Point From Mesh Element`
- Filter by surface normal (vertical faces only for wall vines)
- Spawn spline mesh vine segments along the surface
- Less realistic than Crazy Ivy but free and integrated

### 5.4 Option C: Custom Spline-Mesh Vine System

Build a custom system:
1. Ray-cast from building corners/edges to find wall surfaces
2. Create splines along walls with gravity droop
3. Spawn spline mesh components with vine mesh
4. Scatter leaf cards at spline points via PCG

**Effort:** ~20-30h for basic system, 40-60h for Crazy Ivy parity (not worth it)

### 5.5 Moss and Surface Growth

Moss on horizontal surfaces (roofs, ledges, ground near buildings):
- Use PCG `Point From Mesh Element` to sample building roof/ledge geometry
- Filter by normal (upward-facing only)
- Spawn moss decal meshes or flat moss cards
- Scale density by `decay` and proximity to water/shade

Alternatively, use **material-based moss** via vertex painting or world-aligned blend materials -- but note the project rule against vertex paint for visuals. World-aligned blend (height/normal-based) is the better approach for moss.

### 5.6 Procedural Moss via PCG (Community Reference)

A community project demonstrates Death Stranding-style surface moss generation using PCG graphs:
- Samples surface points at high density
- Filters by upward-facing normals
- Spawns moss clump meshes with scale variation
- Uses distance-to-object falloff for organic spread patterns

---

## 6. Ground Cover -- Litter, Mud, Debris, Snow

### 6.1 Debris Types by Context

| Context | Debris Type | PCG Driver |
|---------|-------------|------------|
| Abandoned yards | Dead leaves, fallen branches, trash bags | Decay parameter, building proximity |
| Roads | Broken glass, newspapers, rubble, tire marks | Decay, road surface sampling |
| Sidewalks | Weeds, cracks, scattered papers, bottles | Pavement mesh sampling, noise |
| Near water | Mud, algae, wet leaves, standing water decals | Distance to water spline/volume |
| Alleys | Dumpster overflow, cardboard, syringes | Narrow space detection, decay |
| Parks/gardens | Overgrown flower beds, broken pots, rusted tools | Garden zone tagging |

### 6.2 PCG Debris Scatter Graph

```
Surface Sampler (landscape) OR Point From Mesh (pavement)
    -> Exclusion zones (buildings interior, roads for landscape scatter)
    -> Attribute Noise (cluster variation)
    -> Match And Set Attributes (context -> mesh set)
    -> Transform Points (random rotation, scale)
    -> Static Mesh Spawner (debris meshes by attribute)
```

### 6.3 Layered Ground Cover

Multiple PCG graphs running at different densities for visual depth:

| Layer | Density | Content | Cull Distance |
|-------|---------|---------|---------------|
| 1. Base | Very high (5-10/m2) | Grass blades, tiny debris | 30-40m |
| 2. Small cover | Medium (0.5-2/m2) | Leaf piles, small rocks, moss patches | 60m |
| 3. Medium props | Low (0.05-0.2/m2) | Fallen branches, trash, bottles | 100m |
| 4. Large props | Very low (0.01/m2) | Fallen trees, dumpsters, abandoned cars | 200m+ |

### 6.4 Seasonal/Weather Variation

PCG parameters can drive seasonal appearance:
- **Fall:** Increase dead leaf density, orange/brown color tint
- **Winter:** Snow ground plane decals, bare trees, ice patches
- **Wet:** Puddle decals, darker ground materials, mist particles

Controlled via a global `season` or `weather` parameter on the PCG graph.

---

## 7. PCG + Landscape Integration

### 7.1 Reading Landscape Layers

PCG can read landscape paint layer weights to drive biome-specific vegetation:

**Get Landscape Data Node:**
- Reads layer weights at each sampled point
- Outputs as point attributes (e.g., `LandscapeLayer_Grass`, `LandscapeLayer_Dirt`, `LandscapeLayer_Mud`)
- Use `Attribute Filter` to spawn different vegetation per layer

**Physical Material Method (Recommended by Epic):**
1. Assign Physical Materials to landscape layers (PM_Grass, PM_Dirt, PM_Mud, PM_Pavement)
2. PCG reads physical material at each point via Get Landscape Data
3. Use Attribute Filter with physical material name strings
4. Route to different Static Mesh Spawner branches

```
Surface Sampler -> Get Landscape Data
    -> Attribute Filter (PM_Grass) -> grass scatter subgraph
    -> Attribute Filter (PM_Dirt) -> weed/rock scatter subgraph
    -> Attribute Filter (PM_Mud) -> mud/puddle subgraph
    -> Attribute Filter (PM_Pavement) -> crack/weed subgraph
```

### 7.2 Height-Based Rules

Use point Z position for elevation-based variation:
- **Low-lying areas** (< base height + 50cm): Swamp vegetation, standing water, dense fog
- **Street level:** Urban vegetation, weeds, debris
- **Hillside:** Sparse trees, exposed rock
- **Elevated:** Wind-bent trees, less ground cover

### 7.3 Slope-Based Rules (Verified)

The Normal To Density -> Density Filter chain is the standard approach:
- Flat (0-15 deg): Full vegetation
- Moderate (15-35 deg): Reduced density, slope-tolerant species
- Steep (35-60 deg): Sparse, only clinging plants
- Cliff (60-90 deg): No vegetation (or moss/lichen decals only)

### 7.4 Landscape Splines for Roads

If roads are defined via Landscape Splines:
- PCG can read them via `Get Landscape Spline Data` node
- Create exclusion corridors along road splines
- Spawn road-edge vegetation (gutter weeds, curb-crack plants)

---

## 8. Foliage Instancing and Spawning

### 8.1 PCG Spawning Modes

PCG's `Static Mesh Spawner` node has several output modes:

| Mode | Description | Performance |
|------|-------------|-------------|
| **HISM** (default) | Hierarchical Instanced Static Mesh | Best for spread-out foliage (trees, large bushes). Cluster culling. |
| **ISM** | Instanced Static Mesh | Good for dense, uniform foliage in small areas |
| **Static Mesh Component** | Individual actors | Worst. Only for unique/interactive objects |
| **FastGeo** (5.7 new) | Componentless primitives via PCG FastGeo Interop plugin | Best game-thread perf for massive density. Enable `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives=1` |

**For Leviathan (Nanite OFF):** Use HISM for all foliage. FastGeo is interesting but experimental.

### 8.2 HISM Details

- Each unique mesh + material combo = 1 HISM component = 1 draw call (per LOD)
- HISM splits instances into spatial clusters for hierarchical culling
- **With collision:** ~10K instances safe limit per HISM
- **Without collision:** 500K+ instances fine
- Foliage typically has collision disabled (grass/flowers) or simplified (trees)

### 8.3 Foliage Component Integration

PCG can also output to the built-in Foliage system rather than spawning its own HISM:
- Use `Convert To Foliage` or custom node to add instances to the Foliage Instance component
- Benefit: Integrates with foliage painting tools, LOD system, cull distance settings in Foliage Type assets
- **Forum reports:** PCG gets over texture streaming budget faster than native Foliage tool. May need manual streaming tweaks.

### 8.4 LOD Strategy for Vegetation

Per-mesh LOD chain for foliage:

| Distance | LOD | Tri Count | Notes |
|----------|-----|-----------|-------|
| 0-30m | LOD0 | 2-6K (trees), 50-200 (grass) | Full detail |
| 30-80m | LOD1 | 500-2K (trees), 20-50 (grass) | Simplified |
| 80-200m | LOD2 | 100-500 (trees) | Billboard/impostor |
| 200m+ | LOD3/Cull | 0 | Culled |

For grass specifically: aggressive cull at 40-50m. Players almost never notice distant grass disappearing, but the performance savings are enormous.

---

## 9. Horror-Specific Vegetation

### 9.1 Horror Vegetation Archetypes

Drawing from Silent Hill, The Last of Us, Resident Evil, and real-world haunted attraction design:

| Archetype | Description | Horror Function |
|-----------|-------------|-----------------|
| **Dead Garden** | Overgrown flower beds with dead plants, broken pots, rusted tools | Domestic decay, "something happened here" |
| **Gnarled Trees** | Twisted, leafless trees with dark bark, broken branches | Oppressive silhouettes, pareidolia (faces in bark) |
| **Thorny Barriers** | Dense bramble/rose bushes with thorns | Soft gameplay barriers, "nature reclaiming" |
| **Poisoned Ground** | Discolored earth, dead grass circles, withered plants | Supernatural contamination |
| **Creepy Cornfield** | Tall corn stalks limiting visibility to 1-2m | Extreme sightline restriction, rustling audio |
| **Swamp Growth** | Cattails, lily pads, hanging moss, algae | Low-lying areas, fog generators, water hazards |
| **Overgrown Ruins** | Vines/ivy consuming building facades | Time passage, nature vs civilization |
| **Infected Flora** | Unnaturally colored plants (grey, purple, bioluminescent) | Supernatural/disease theme |
| **Weeping Willows** | Long hanging branches creating curtains | Natural visual barriers, ethereal atmosphere |
| **Root System** | Exposed tree roots breaking through pavement/foundations | Nature reclaiming, tripping hazard |

### 9.2 The Decay Parameter

A single `decay` float (0.0 to 1.0) drives the entire vegetation aesthetic per zone/lot:

```
decay = 0.0  ->  Maintained: Mowed lawns, trimmed hedges, healthy trees
decay = 0.3  ->  Neglected: Tall grass, some weeds, leaves accumulating
decay = 0.5  ->  Abandoned: Waist-high weeds, dead patches, fallen branches
decay = 0.7  ->  Overgrown: Vegetation encroaching on structures, dense ground cover
decay = 1.0  ->  Consumed: Nature reclaiming everything, roots through walls, total overgrowth
```

**PCG Integration:**
- Pass `decay` as a PCG graph parameter (float, exposed)
- Use `Attribute Remap` to convert decay to density multipliers per layer
- Use `Match And Set Attributes` to select mesh variants by decay range
- Different PCG subgraphs for each decay tier can be blended

### 9.3 Horror Composition Rules

**Contrast is key:**
- Patches of manicured lawn next to dead zones = something wrong happened *there*
- A single healthy flower among dead vegetation = unsettling, draws attention
- Overgrowth everywhere except one clear path = "someone/something uses this path"

**Audio integration:**
- Dense vegetation zones should overlap with rustling/creaking ambient audio volumes
- Cornfields and tall grass trigger specific movement audio for player and enemies
- Dead trees in wind = creaking wood ambience zone

**Lighting interaction:**
- Dense canopy areas = darker ground, dappled shadows
- Leafless trees = stark shadow patterns on ground
- Fog collects in low vegetation areas

### 9.4 The Last of Us Overgrowth Pattern

TLOU's environmental storytelling through vegetation is the gold standard:
- Moss-covered cars, trees bursting through concrete, flowering vines on fallen buildings
- Dense vegetation provides natural cover for stealth gameplay
- Collapsed structures with vegetation create organic-feeling boundaries
- The juxtaposition of lush growth against human decay tells the story visually

For Leviathan: Apply this principle at the town level. Buildings with high `decay` get TLOU-style treatment. Buildings with low decay feel recently abandoned (more unsettling because whatever happened was *recent*).

---

## 10. Performance Budget and Optimization

### 10.1 Performance Targets

For a horror FPS targeting 60fps on current-gen hardware (Leviathan's target):

| Category | Instance Budget | Draw Call Budget | Notes |
|----------|----------------|------------------|-------|
| Grass/ground cover | 50K-100K visible | 5-10 | HISM, aggressive 40m cull |
| Small plants/weeds | 5K-10K visible | 10-15 | HISM, 60m cull |
| Bushes/hedges | 500-2K visible | 5-10 | HISM + LOD, 100m cull |
| Trees | 100-500 visible | 10-20 | HISM + impostor LOD, 200m+ |
| Debris/litter | 2K-5K visible | 5-10 | HISM, 60m cull |
| Fog volumes | 5-15 visible | N/A (volumetric) | Local fog actors |
| **Total** | **~60K-120K** | **~35-65** | |

### 10.2 Optimization Techniques

**Cull Distance per Foliage Type:**
- Set in FoliageType asset or HISM component
- Grass: 40-50m (players never notice)
- Small plants: 60-80m
- Trees: 200m+ with impostor billboard LOD

**LOD Transitions:**
- Every tree mesh needs 3-4 LODs (hero -> simplified -> billboard -> cull)
- Grass can use 2 LODs (full -> card -> cull)
- LOD transition distances match cull distances

**Material Optimization:**
- Atlas textures for foliage (fewer unique materials = fewer draw calls)
- Two-sided foliage shading model
- Simple masked materials for grass (no translucency unless hero plants)
- Shared material instances across foliage types

**PCG-Specific:**
- Use `Self Pruning` node to enforce minimum distances (prevents over-density)
- Use `Bounds Modifier` to set proper HISM cluster sizes
- PCG GPU compute for large-scale generation
- Generate at edit-time (GenerateOnLoad), not runtime -- save results with level

**World Partition:**
- PCG works with World Partition -- generation scoped to loaded partitions
- Vegetation only loaded for nearby streaming cells
- PCG Hierarchical Generation uses grid sizes matching partition cells

### 10.3 Memory Budget

Per vegetation type memory cost (rough estimates):

| Asset | Mesh Memory | Texture Memory | Notes |
|-------|-------------|----------------|-------|
| Grass blade | 1-5 KB | Shared atlas 2-4 MB | Thousands of instances |
| Weed clump | 5-20 KB | Shared atlas 2-4 MB | |
| Bush | 50-200 KB | 4-8 MB per material | LODs important |
| Tree (full) | 200KB-2MB | 8-16 MB per material | LODs + billboard critical |
| Debris mesh | 1-50 KB | Shared atlas 2-4 MB | |

Total vegetation memory budget: ~200-400 MB (textures dominant). Streaming is essential.

### 10.4 FastGeo (UE 5.7 Experimental)

New in 5.7: PCG FastGeo Interop plugin enables componentless primitives:
- Enable `PCGFastGeoInterop` plugin
- Set CVar `pcg.RuntimeGeneration.ISM.ComponentlessPrimitives=1`
- Removes need for partition actors
- Significant game-thread performance improvement for high-density spawning
- **Status:** Experimental. Evaluate before relying on it.

---

## 11. Existing PCG Vegetation Examples

### 11.1 Electric Dreams (Epic Official Sample)

- Free on Fab, 4km x 4km jungle environment built almost entirely with PCG
- Key techniques:
  - **Surface Sampler** for landscape-based vegetation scatter
  - **Flat Area Detector Subgraph** -- finds suitable placement surfaces
  - **PCGMeshSelectorByAttribute** -- selects mesh from attribute, enabling variety
  - **Transform Points** for rotation/scale variation
  - In-level text descriptions explain every technique
- **Relevance:** Production-quality reference for PCG vegetation. Download and study.

### 11.2 PCG Biome Core and Sample Plugins (Built into UE 5.7)

- Ships with engine as optional plugins
- Demonstrates:
  - **Attribute Set Tables** for biome-to-mesh mapping
  - **Feedback loops** -- generated content influences subsequent generation
  - **Recursive sub-graphs** for fractal/hierarchical placement
  - **Runtime generation** for streaming scenarios
- **Relevance:** Direct template for Leviathan's biome system. Enable and study.

### 11.3 PCG Layered Biomes (Fab Asset)

- Community asset demonstrating layered biome generation
- Each biome layer is a separate PCG graph
- Layers receive information about previously generated geometry to avoid intersections
- **Relevance:** Architecture pattern for multi-layer vegetation (grass -> bushes -> trees -> debris)

### 11.4 Calysto World / Massive World (Fab Plugin)

- Commercial procedural world generation tool built on PCG
- Features: biome drawing, road painting, vegetation masks, settlement generation
- Supports Nanite foliage and blueprint spawning
- Compatible with World Partition
- **Relevance:** Reference for large-scale PCG world generation, but likely too opinionated for Leviathan. Study but don't adopt.

### 11.5 Community PCG Moss (ArtStation)

- Death Stranding-style surface moss generation via PCG
- High-density surface sampling, normal filtering, moss clump spawning
- **Relevance:** Technique applicable to Leviathan's building decay moss.

---

## 12. Integration with Town Generator

### 12.1 Architecture: PCG as Post-Processing Layer

The town generator (city block layout, building generation, facade, etc.) creates structures. PCG vegetation runs AFTER as a post-processing layer:

```
Town Generator (Monolith MCP)
    -> Buildings, Roads, Sidewalks (actors with tags)
    -> Spatial Registry (building types, lot boundaries, decay values)

PCG Vegetation Layer (PCG Graphs)
    -> Reads: Landscape, building bounds, road splines, lot data, decay params
    -> Outputs: Foliage instances, debris, fog volumes
    -> Respects: Exclusion zones, sightlines, performance budgets
```

### 12.2 Exclusion Zone Pipeline

Building footprints and roads must be excluded from vegetation:

**Step 1: Tag Actors**
- Buildings tagged `Monolith.Building`
- Roads tagged `Monolith.Road`
- Sidewalks tagged `Monolith.Sidewalk`

**Step 2: PCG Graph Reads Tags**
```
Get Actor Data (tag: Monolith.Building) -> Difference -> removes vegetation from building footprints
Get Actor Data (tag: Monolith.Road) -> Difference -> removes vegetation from roads
```

**Step 3: Proximity-Based Density**
Near buildings, increase density for overgrowth effect:
```
Get Actor Data (buildings) -> Distance -> Attribute Remap (close=high density, far=low)
```

### 12.3 Per-Lot Decay Integration

Each lot in the spatial registry has a `decay` parameter (0.0-1.0):

1. PCG volume covers the entire block
2. PCG graph reads lot boundaries from spatial registry (custom node or data table)
3. Points within each lot inherit that lot's decay value
4. Decay drives: vegetation type, density, mesh selection, color tinting

**Implementation approaches:**

**A) Per-Lot PCG Components:**
Each lot gets its own PCG component with `decay` as a graph parameter. Simple, but many components.

**B) Single Block PCG with Attribute Lookup:**
One PCG graph per block reads lot data from a DataTable. Points sample their lot ID from position, inherit decay. More complex, fewer components.

**C) Decay Texture Map:**
Render decay values into a texture (like a density map). PCG samples the texture per point. Simplest for smooth blending across lot boundaries.

Recommendation: **Option A** for Phase 1 (simplest), migrate to **Option C** for polish.

### 12.4 Building Proximity Effects

Distance from buildings should modulate vegetation:

| Distance | Effect |
|----------|--------|
| 0-50cm | Wall-base weeds, moss, foundation plants |
| 50cm-2m | Overgrown foundation plantings, debris from building |
| 2-5m | Yard vegetation, lawn/weeds based on decay |
| 5-15m | Lot vegetation, trees, garden remnants |
| 15m+ | Street/buffer vegetation, road-edge weeds |

Use PCG `Distance` node to compute distance from building bounds, then `Attribute Remap` to set density/type.

---

## 13. Fog and Atmosphere Zones

### 13.1 Local Volumetric Fog via PCG

PCG can spawn **actors**, not just static meshes. Local Volumetric Fog actors can be procedurally placed:

```
Surface Sampler (low density, low-lying areas only)
    -> Height Filter (below threshold)
    -> Spawn Actor (ALocalFogVolume or custom BP)
```

**Local Fog Volume properties to set via PCG Actor Overrides:**
- `FogDensity` -- higher in swamps, lower in streets
- `Albedo` -- grey for normal fog, greenish for swamp, yellowish for toxic
- `Emissive` -- subtle glow for supernatural fog
- `ExtentX/Y/Z` -- volume size (5-20m typical)

### 13.2 Fog Placement Rules

| Location | Fog Type | Density | Color |
|----------|----------|---------|-------|
| Low-lying ground | Ground fog | Medium | Grey-white |
| Near water (ponds, drains) | Mist | High | Grey |
| Alleys | Creeping fog | Low-medium | Dark grey |
| Swamp areas | Dense fog | Very high | Green-grey |
| Supernatural zones | Otherworld fog | Variable | Unnatural (red, yellow) |
| Forest clearings | Morning mist | Low | White |

### 13.3 Exponential Height Fog

The global `ExponentialHeightFog` actor handles the base atmospheric fog:
- Set low `FogDensity` for general atmosphere
- `FogHeightFalloff` controls vertical falloff (higher = fog concentrated at ground level)
- `Volumetric Fog` checkbox enables light scattering (Lumen-compatible)
- `FogInscatteringColor` for horror tint (slightly blue-grey or amber)
- **Not PCG-driven** -- single global actor, manually configured per level/time-of-day

### 13.4 Fog + Vegetation Synergy

Dense vegetation zones should correlate with fog zones:
- Swamp vegetation + dense ground fog = visibility < 10m
- Dead forest + wisps of fog through branches = atmospheric depth
- Overgrown alley + creeping fog at ankle level = dread (something could be below the fog line)

---

## 14. Proposed MCP Actions

New actions for Monolith MCP to control PCG vegetation:

### 14.1 Core Vegetation Actions

| Action | Description | Params |
|--------|-------------|--------|
| `create_vegetation_graph` | Create a PCG graph for vegetation scatter with standard horror pipeline | `type` (grass/trees/debris/fog), `decay`, `density`, `biome` |
| `apply_vegetation_to_block` | Apply vegetation PCG to an entire city block | `block_id`, `decay_map` (per-lot decay values), `biome`, `season` |
| `set_lot_decay` | Set the decay parameter for a specific lot | `lot_id`, `decay` (0.0-1.0) |
| `exclude_vegetation_zone` | Add an exclusion zone for vegetation | `bounds` or `actor_tag`, `zone_type` (hard/soft) |
| `regenerate_vegetation` | Force regeneration of vegetation PCG in an area | `bounds` or `block_id`, `seed` |

### 14.2 Overgrowth Actions

| Action | Description | Params |
|--------|-------------|--------|
| `apply_overgrowth` | Apply ivy/vine/moss to a building based on decay | `building_id`, `decay`, `types` (ivy/moss/vines), `density` |
| `apply_ground_moss` | Scatter moss on horizontal surfaces near buildings | `target` (building/area), `density`, `spread` |

### 14.3 Atmosphere Actions

| Action | Description | Params |
|--------|-------------|--------|
| `spawn_fog_zone` | Place a local volumetric fog volume | `location`, `extent`, `density`, `color`, `type` |
| `apply_atmosphere_to_block` | Add fog + ambient particles to a block | `block_id`, `fog_density`, `particle_types` |
| `create_swamp_zone` | Set up a swamp area with water, fog, vegetation | `bounds`, `water_level`, `fog_density`, `decay` |

### 14.4 Horror-Specific Actions

| Action | Description | Params |
|--------|-------------|--------|
| `create_dead_garden` | Generate an abandoned garden with dead plants | `bounds`, `decay`, `garden_type` (flower/vegetable/formal) |
| `create_cornfield` | Generate a horror cornfield with optional maze paths | `bounds`, `row_spacing`, `height`, `paths` (maze specification) |
| `create_poisoned_zone` | Area of dead/discolored vegetation (supernatural) | `center`, `radius`, `intensity`, `color_tint` |

**Total: ~13 new actions**

---

## 15. Implementation Plan

### Phase 1: Core PCG Vegetation Scatter (16-22h)

- Set up base PCG vegetation graph templates (grass, weeds, trees, bushes)
- Implement Surface Sampler -> Normal To Density -> Density Filter -> Mesh Spawner pipeline
- Add exclusion zone support (Difference node with building/road actors)
- Create `create_vegetation_graph` and `regenerate_vegetation` MCP actions
- Test with sample landscape + procedural buildings
- **Deliverable:** Basic vegetation scatter that avoids buildings

### Phase 2: Decay-Driven Vegetation (12-16h)

- Implement decay parameter integration (PCG graph parameter)
- Create mesh selection tables per decay tier (healthy -> dead)
- Implement per-lot decay control via spatial registry
- Create `set_lot_decay` and `apply_vegetation_to_block` actions
- Build decay-to-density remapping subgraphs
- **Deliverable:** Vegetation appearance varies by lot decay value

### Phase 3: Overgrowth and Building Integration (12-18h)

- Evaluate and integrate Crazy Ivy plugin (or build basic vine system)
- Implement building proximity effects (foundation plants, wall moss)
- Create `apply_overgrowth` and `apply_ground_moss` actions
- Build PCG subgraph for building-edge vegetation
- **Deliverable:** Buildings with appropriate overgrowth based on decay

### Phase 4: Fog and Atmosphere (8-12h)

- Implement Local Volumetric Fog spawning via PCG Spawn Actor
- Create fog placement rules (low-lying, near water, alleys)
- Create `spawn_fog_zone` and `apply_atmosphere_to_block` actions
- Integrate with ExponentialHeightFog for base atmosphere
- **Deliverable:** Procedural fog zones that enhance horror atmosphere

### Phase 5: Horror Specialty and Polish (12-17h)

- Implement horror vegetation archetypes (dead garden, cornfield, poisoned zone)
- Create specialty MCP actions (cornfield, swamp zone, dead garden)
- Performance optimization pass (cull distances, LOD, draw call audit)
- Integration testing with full town generation pipeline
- Create reusable PCG graph asset library
- **Deliverable:** Complete horror vegetation system

**Total Estimate: ~60-85h**

### Dependencies

- **Spatial Registry** (`2026-03-28-spatial-registry-research.md`) -- for per-lot decay values
- **City Block Layout** (`2026-03-28-city-block-layout-research.md`) -- for building footprints, lot boundaries, road splines
- **Auto Collision** (`2026-03-28-auto-collision-research.md`) -- buildings need collision for Difference node exclusion
- **Foliage Assets** -- need dead trees, horror bushes, weed meshes, debris. Source from Fab (Quixel Megascans has extensive free vegetation library) or create via PVE/SpeedTree.

### Asset Requirements

Minimum vegetation asset library for horror town:

| Category | Count Needed | Source |
|----------|-------------|--------|
| Grass varieties (dead + alive) | 4-6 | Quixel / Fab |
| Weed meshes | 4-6 | Quixel / Fab |
| Dead tree variations | 3-5 | Quixel / SpeedTree / PVE |
| Healthy tree variations | 2-3 | Quixel / SpeedTree |
| Bush/hedge varieties | 3-4 | Quixel / Fab |
| Ground debris (leaves, branches) | 6-10 | Quixel / Fab |
| Urban debris (trash, bottles, paper) | 6-10 | Quixel / Fab / custom |
| Moss/lichen decals | 3-4 | Quixel / Fab |
| Ivy/vine meshes | 2-4 | Crazy Ivy / custom |
| Cornfield stalks | 1-2 | Custom / Fab |
| **Total** | **~35-55 meshes** | |

---

## 16. Sources

### Official Documentation
- [Procedural Foliage Tool - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-foliage-tool-in-unreal-engine)
- [Procedural Vegetation Editor (PVE) - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-vegetation-editor-pve-in-unreal-engine)
- [PCG Framework Node Reference - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG Biome Core and Sample Plugins Overview - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-overview-guide-in-unreal-engine)
- [PCG Biome Core and Sample Plugins Reference - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine)
- [PCG in Electric Dreams - UE Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine)
- [Nanite Foliage - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/nanite-foliage)
- [Local Fog Volumes - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine)
- [Volumetric Fog - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine)
- [Landscape Materials - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine)
- [Using PCG with GPU Processing - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)

### Tutorials and Guides
- [PCG Basics: First Procedural Scatter System - Hyperdense (2026)](https://medium.com/@sarah.hyperdense/pcg-basics-your-first-procedural-scatter-system-in-ue5-fab626e1d6f0)
- [A Tech Artist's Guide to PCG - Epic Community](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [Crash Course: Populating a Landscape with PCG - Epic Tutorial](https://dev.epicgames.com/community/learning/tutorials/qBrV/unreal-engine-crash-course-quickly-populating-a-landscape-with-procedural-content-generation-framework)
- [Master UE5 PCG: Vegetation & City Generation - Epic Community](https://forums.unrealengine.com/t/community-tutorial-master-ue5-pcg-procedural-vegetation-city-generation-tutorial-beginner-friendly/2677326)
- [How to Use Landscape Physical Materials to Control PCG - Epic Tutorial](https://dev.epicgames.com/community/learning/tutorials/EPoW/unreal-engine-how-to-use-landscape-physical-materials-to-control-pcg)
- [Procedural Vines on Static Meshes - Epic Tutorial](https://dev.epicgames.com/community/learning/tutorials/Xj1R/unreal-engine-procedural-vines-on-static-meshes-tutorial)
- [Unreal 5.5 PCG: Spline Mesh Magic - Epic Community](https://dev.epicgames.com/community/learning/tutorials/aw1D/unreal-engine-unreal-5-5-pcg-spline-mesh-magic)
- [Passing Values from PCG to Actor Blueprints - Epic Tutorial](https://dev.epicgames.com/community/learning/tutorials/9d7z/unreal-engine-passing-values-from-unreal-pcg-to-actor-blueprints)
- [Creating Intermediate PCG Foliage Scatter Tool - UE5.5 Forum](https://forums.unrealengine.com/t/community-tutorial-creating-an-intermediate-pcg-foliage-scatter-tool-in-ue5-5/2252252)
- [PCG Tutorial Series - Epic Community](https://dev.epicgames.com/community/learning/tutorials/1wro/unreal-engine-pcg-tutorial-series)

### Performance and Optimization
- [Complete Guide to Landscape and Foliage Optimization UE5 - Outscal](https://outscal.com/blog/landscape-and-foliage-optimization-unreal-engine-5)
- [Notes on Foliage in Unreal 5 - Iri Shinsoj (Medium)](https://medium.com/@shinsoj/notes-on-foliage-in-unreal-5-3522b6eb159f)
- [UE 5.7 Performance Highlights - Tom Looman](https://tomlooman.com/unreal-engine-5-7-performance-highlights/)
- [UE 5.7 Nanite Foliage - Arkoun Merchant (Medium)](https://medium.com/@thirdspaceinteractive/unreal-engine-5-7-nanite-foliage-a-game-changer-for-real-time-vegetation-c8e9692df3b5)

### Tools and Plugins
- [Crazy Ivy - Procedural Ivy Generator (Fab)](https://www.fab.com/listings/5cd4aa95-53ac-47fb-87f9-ffd4719c1156)
- [Crazy Ivy Official Site](https://www.crazyivy.com/)
- [Electric Dreams Environment (Fab)](https://www.fab.com/listings/d79688f5-29be-4fb2-a650-2d4a813f5306)
- [PCG Layered Biomes (Fab)](https://www.fab.com/listings/762ad275-f56b-4275-9c24-6da1025508fa)
- [Calysto World 2.0 PCG (Fab)](https://www.fab.com/listings/8631308a-67a3-4e20-b3e4-74be19813f77)
- [Errant Photon - Procedural Tools](https://www.errantphoton.com/)

### Articles and Community
- [UE 5.7: Foliage, PCG and In-Editor AI - Digital Production (2025)](https://digitalproduction.com/2025/11/12/unreal-engine-5-7-foliage-pcg-and-in-editor-ai/)
- [UE 5.7 PVE and Mega Plants Workflow - CGEcho](https://cgecho.net/understanding-the-ue-5-7-pve-and-mega-plants-workflow/)
- [How to Create PCG Exclusion Zones - Epic Forum](https://forums.unrealengine.com/t/how-to-create-pcg-exclusion-zones/2708969)
- [PFS vs PCG vs Foliage Instance - Epic Forum](https://forums.unrealengine.com/t/pfs-vs-pcg-vs-foliage-instance/2131745)
- [PCG Biomes + Landscape Layer Exclusion - Epic Forum](https://forums.unrealengine.com/t/pcg-biomes-landscape-layer-exclusion/1952124)
- [Foliage in Current Version Forward Looking - Epic Forum](https://forums.unrealengine.com/t/foliage-in-current-version-and-forward-looking/2685424)
- [PCG Achieving Parity with Landscape Grass - Epic Forum](https://forums.unrealengine.com/t/pcg-achieving-parity-with-landscape-grass-keep-getting-too-many-instaces/1960549)
- [UE 5.6 PCG Difference Node Regression - Epic Forum](https://forums.unrealengine.com/t/in-ue-5-6-pcg-difference-node-no-longer-removes-points-using-getactordata/2661336)
- [Custom Procedural Vegetation Placement Tool - 80.lv](https://80.lv/articles/this-procedural-tool-grows-vegetation-on-any-visible-surface)
- [PCG Procedural Moss UE5 - UnrealFab](https://www.unrealfab.com/gallery/5128/pcg-procedural-moss-ue5/)
- [Horror and Fear in Level Design - World of Level Design](https://www.worldofleveldesign.com/categories/level_design_tutorials/horror-fear-level-design/part1-survival-horror-level-design-cliches.php)
- [The Last of Us: How Nature Transforms Gaming's Apocalypse - HBO Watch](https://hbowatch.com/story/the-last-of-us-how-nature-transforms-gamings-apocalypse/)

### Related Internal Research
- `2026-03-28-pcg-framework-research.md` -- PCG architecture, API, data types, node catalog, GPU compute, PCGEx
- `2026-03-28-city-block-layout-research.md` -- Lot subdivision, building footprints, streets, decay system
- `2026-03-28-spatial-registry-research.md` -- Hierarchical spatial data, per-room/building/lot metadata
- `2026-03-28-auto-volumes-research.md` -- Audio/post-process volumes that overlap with fog zones
- `2026-03-28-auto-collision-research.md` -- Collision needed for PCG Difference exclusion
