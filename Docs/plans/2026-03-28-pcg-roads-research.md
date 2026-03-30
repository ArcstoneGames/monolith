# PCG for Roads, Paths, Sidewalks, and Linear Infrastructure -- Horror Town Research

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready)
**Context:** Monolith MCP integration for procedural horror town linear infrastructure generation
**Depends on:** PCG Framework Research (`2026-03-28-pcg-framework-research.md`), City Block Layout Research (`2026-03-28-city-block-layout-research.md`)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Road Network Generation](#2-road-network-generation)
3. [Spline-Based Road Geometry](#3-spline-based-road-geometry)
4. [Intersection Handling](#4-intersection-handling)
5. [Sidewalks and Curbs](#5-sidewalks-and-curbs)
6. [Alleyways](#6-alleyways)
7. [Road Markings and Decals](#7-road-markings-and-decals)
8. [Street Lights](#8-street-lights)
9. [Power Lines and Telephone Poles](#9-power-lines-and-telephone-poles)
10. [Fences and Walls](#10-fences-and-walls)
11. [Railroad Tracks](#11-railroad-tracks)
12. [Drainage and Ditches](#12-drainage-and-ditches)
13. [PCGEx Graph-Based Road Networks](#13-pcgex-graph-based-road-networks)
14. [Horror Decay System](#14-horror-decay-system)
15. [Integration with Building System](#15-integration-with-building-system)
16. [Existing Tools and Marketplace](#16-existing-tools-and-marketplace)
17. [Recommended Architecture](#17-recommended-architecture)
18. [MCP Actions](#18-mcp-actions)
19. [Implementation Plan](#19-implementation-plan)
20. [Sources](#20-sources)

---

## 1. Executive Summary

Linear infrastructure (roads, sidewalks, fences, power lines, etc.) is the connective tissue of a horror town. Without it, procedural buildings are just scattered boxes. This research covers how UE5.7's production-ready PCG framework, PCGEx, and supplementary systems (GeometryScript, Landscape Splines, RVT) can handle every category of linear feature for Leviathan's abandoned small-town setting.

**Key findings:**

- **PCG splines are the backbone.** UE5.7 adds Spline Intersection and Split Splines operators, making road networks viable natively
- **PCGEx is essential.** Its graph theory nodes (Delaunay, Voronoi, MST, A*) transform road networks from manual spline placement to algorithmic generation. MIT licensed, 200+ nodes
- **Road geometry is solved multiple ways:** spline mesh deformation, Runtime Virtual Textures (RVT) for blending, modular mesh placement along splines, or GeometryScript swept profiles
- **Intersections are the hardest problem.** T-junctions and 4-way crossroads require dedicated intersection meshes or RVT compositing -- no perfect automated solution exists
- **Horror decay is a first-class parameter.** Every linear feature takes a 0.0-1.0 decay value controlling broken lights, cracked pavement, overgrown sidewalks, sagging wires, leaning poles
- **Total estimate:** ~120-160h across all linear feature types, with a phased approach starting at roads+sidewalks (~35-45h MVP)

---

## 2. Road Network Generation

### 2.1 Scale: What Leviathan Needs

Leviathan is a small horror town, not a metropolis. The road network is:
- 4-12 city blocks in a rough grid
- 2-4 main roads (wider, lit, commercial)
- 4-8 residential streets (narrower, darker)
- 1-3 alleys per block
- Possibly 1 railroad crossing at the town edge
- Dead ends and cul-de-sacs for horror (trapped feeling)

This is NOT a Cities: Skylines road network. We need a simple, controllable graph that can be hand-authored OR procedurally generated with constraints.

### 2.2 Road Network Algorithms (Background)

Three major approaches from the literature:

#### L-System Roads (Parish & Mueller, SIGGRAPH 2001)
- Extended L-system grows road segments iteratively
- Global goals: connect population centers, follow terrain
- Local constraints: avoid water, maintain minimum angles
- Output: graph of road segments that subdivide land into blocks
- **Verdict for Leviathan:** Overkill. We want designer control, not full procedural growth

#### Tensor Field Roads (Chen et al., SIGGRAPH 2008)
- Paint a 2D tensor field; trace hyperstreamlines along eigenvectors
- Major/minor eigenvectors perpendicular = natural grid patterns
- Weighted blending of multiple fields for varied layouts
- **Verdict:** Elegant but complex. Good if we ever need organic road layouts

#### Graph-Based (tmwhere / Martin Devans / phiresky)
- Road network as undirected planar graph in adjacency list
- Grow from initial segments; each iteration: dequeue segment, check local constraints, add if compatible, generate new branches
- Population density map guides branching decisions
- **Verdict:** Good reference implementation. tmwhere's JS implementation is clean and well-documented

**Our recommended approach:** Designer-placed or MCP-generated point graph (intersections as nodes, roads as edges), with PCGEx handling the graph construction and road mesh placement. For fully procedural mode, a simple grid generator with randomized dead-ends and irregular spacing.

### 2.3 Road Hierarchy for Horror Town

| Road Type | Width (UE cm) | Lanes | Sidewalk | Lights | Speed |
|-----------|---------------|-------|----------|--------|-------|
| Main Street | 1200-1400 | 2 + parking | Both sides | Yes (30m) | 35mph |
| Residential | 800-1000 | 2 | Both sides | Yes (45m) | 25mph |
| Alley | 300-600 | 0 (shared) | None | None/broken | N/A |
| Service Lane | 400-600 | 1 | One side optional | Sparse | 15mph |
| Dead End | 800-1000 | 2 | Both sides | 1-2 | 25mph |
| Railroad Crossing | Road width | Road lanes | Road sidewalks | Crossing signals | N/A |

Real US residential street ROW is typically 50-60ft (1524-1829cm), with 24-28ft (732-854cm) pavement. Our numbers are game-friendly simplifications.

### 2.4 Road Network Data Structure

```cpp
// Road network graph -- lightweight, serializable
struct FRoadNode
{
    FVector2D Position;        // 2D world position (XY plane)
    ERoadNodeType Type;        // Intersection, DeadEnd, Cul_de_sac, T_Junction, Crossing
    float Elevation;           // Terrain height at this point
    TArray<int32> ConnectedEdges;
};

struct FRoadEdge
{
    int32 StartNode;
    int32 EndNode;
    ERoadType RoadType;        // Main, Residential, Alley, Service, Railroad
    float Width;               // Total ROW width in cm
    float SpeedLimit;          // For AI navigation
    float DecayLevel;          // 0.0 = pristine, 1.0 = destroyed
    TArray<FVector2D> IntermediatePoints;  // For curves
    bool bOneWay;
};

struct FRoadNetwork
{
    TArray<FRoadNode> Nodes;
    TArray<FRoadEdge> Edges;
    FBox2D Bounds;

    // Derived data
    TArray<FBlock> Blocks;     // Closed polygons from edge cycles (the city blocks)
};
```

---

## 3. Spline-Based Road Geometry

### 3.1 Three Approaches to Road Meshes

#### Approach A: Spline Mesh Components (Traditional)
- Each road segment is a `USplineMeshComponent` stretching a flat road mesh between two points
- Pros: simple, Nanite-compatible since UE 5.3, conforms to spline curvature
- Cons: seams at joints, can't handle intersections natively, UV stretching on curves
- **Best for:** straight road segments between intersections

#### Approach B: Runtime Virtual Texture (RVT) Compositing
- Road material renders into an RVT that composites onto the landscape
- Spline drives where the RVT road material applies
- Pros: seamless blending with landscape, intersections handled naturally by layering, zero-geometry solution
- Cons: requires landscape (won't work on flat mesh ground), lower resolution than mesh, Lumen reflections don't work perfectly with RVT
- **Best for:** landscape-based terrain where roads need to blend into dirt/grass edges

#### Approach C: PCG Point Sampling + Modular Mesh Placement
- Sample points along road spline at regular intervals
- Place modular road section meshes (straight, curve-left, curve-right) at each point
- PCGEx path operations handle smoothing, subdividing, beveling
- Pros: fully modular, instanced rendering (HISM), snap to grid, easy intersection pieces
- Cons: visible tiling if modules not varied enough, curves need many segments
- **Best for:** Monolith's approach. Modular, controllable, compatible with building system

#### Approach D: GeometryScript Swept Profile
- Sweep a road cross-section profile along a spline path
- Cross-section: gutter | curb | sidewalk | road surface | center line | road surface | sidewalk | curb | gutter
- Pros: single continuous mesh, perfect UVs along sweep, customizable profile per road type
- Cons: more complex geometry generation, intersections still need special handling
- **Best for:** high-quality hero roads, or as a fallback generation method

**Recommendation:** Primary = Approach C (modular mesh + PCG), Secondary = Approach D (GeometryScript for custom segments). Approach B for landscape integration where applicable.

### 3.2 PCG Spline Road Pipeline

```
1. Define road network (graph of nodes + edges)
2. Convert each edge to a USplineComponent
3. PCG graph processes each spline:
   a. Spline Sampler -> points along road
   b. Filter/Transform -> adjust spacing, add variation
   c. Mesh Spawner -> place road section meshes
   d. Additional passes for sidewalks, markings, lights, etc.
4. At intersections, spawn intersection-specific meshes
5. Landscape sculpting pass (optional): flatten/cut terrain under roads
```

### 3.3 UE5.7 PCG Spline Features (New)

UE5.7 added several critical spline operators:
- **Spline Intersection** -- detect where two splines cross, output intersection points
- **Split Splines** -- divide a spline at intersection points into segments
- **Polygon2D data type** -- closed areas that convert to surfaces or splines
- **PCG Editor Mode** -- draw splines, paint points, create volumes interactively
- **Spline Mesh Spawner** -- native node for placing meshes along splines with rotation/scale

These are exactly the primitives needed for road networks. The Spline Intersection + Split Splines combo means we can define overlapping road splines and automatically find where they cross -- solving the biggest road generation pain point.

---

## 4. Intersection Handling

Intersections are the single hardest problem in procedural road generation. Every forum thread and tutorial highlights this.

### 4.1 The Problem

When two road splines cross:
- Road surface meshes overlap and Z-fight
- Sidewalk curbs need to break for crosswalks
- Road markings need to stop/change pattern
- Turn lanes, stop lines, crosswalks all interact
- Drainage patterns change

### 4.2 Intersection Types for Horror Town

| Type | Description | Frequency | Complexity |
|------|-------------|-----------|------------|
| 4-way | Two roads cross at ~90 degrees | Common | High |
| T-junction | Road meets another perpendicular | Very common | Medium |
| Dead end | Road terminates | Common (horror!) | Low |
| Cul-de-sac | Road loops back (circular dead end) | Occasional | Medium |
| Y-fork | Road splits at an angle | Rare | Medium |
| Railroad crossing | Road crosses tracks | 0-1 per town | High |
| Roundabout | Circular intersection | 0-1 for variety | High |

### 4.3 Solution Strategies

#### Strategy 1: Intersection Meshes (Recommended)
- Pre-made static meshes for each intersection type (4-way, T-junction, etc.)
- At each road node, determine intersection type from connected edges
- Spawn appropriate intersection mesh, rotated to match incoming road directions
- Road segment meshes terminate before the intersection
- **Pros:** Clean geometry, predictable results, easy to art-direct
- **Cons:** Needs mesh authoring for each combo, limited variation

#### Strategy 2: RVT Compositing at Intersections
- Road segments render into RVT; intersections are where they overlap
- RVT material uses Translucency Sort Priority to control layering
- Intersection markings (stop lines, crosswalks) as additional RVT decals
- **Pros:** Seamless, no special meshes needed
- **Cons:** Lower quality, can't easily do 3D features (raised crosswalks, curb cuts)

#### Strategy 3: PCGEx Graph-Based
- Use PCGEx to build the road graph from points
- At each node with degree > 2 (i.e., an intersection), compute the intersection polygon
- Generate geometry for the intersection area procedurally
- **Pros:** Fully procedural, handles arbitrary intersection angles
- **Cons:** Complex implementation, harder to control quality

#### Strategy 4: Hybrid (Our Approach)
- Use modular intersection meshes for common types (4-way, T, dead-end)
- Fall back to RVT blending for unusual angles
- PCGEx handles the graph logic of determining what type each intersection is
- Road segments are modular meshes placed by PCG

### 4.4 Intersection Detection Algorithm

```
For each node N in the road graph:
  connected_edges = edges incident to N
  degree = len(connected_edges)

  if degree == 1:
    type = DeadEnd (or Cul_de_sac if flagged)
  elif degree == 2:
    angle = angle_between(edge1_direction, edge2_direction)
    if angle > 150:
      type = Straight (no intersection, just a bend)
    else:
      type = Curve
  elif degree == 3:
    type = T_Junction
    // Determine which edge is the "through" road vs the branch
    // Typically the two most collinear edges form the through road
  elif degree == 4:
    type = FourWay
    // Check if roughly perpendicular
  else:
    type = Complex (fallback to RVT)

  // Compute rotation: align intersection mesh to the "primary" road direction
  primary_direction = direction of the widest connected road
  rotation = atan2(primary_direction.Y, primary_direction.X)
```

---

## 5. Sidewalks and Curbs

### 5.1 Sidewalk Anatomy (US Standard)

A typical US sidewalk cross-section from curb to property line:

```
Property | Planting  | Sidewalk | Furniture | Curb | Gutter | Road
 Line    | Strip     | (Walk)   | Zone      |      |        |
         | 60-120cm  | 120-180cm| 60-120cm  | 15cm | 30-60cm|
         | (grass/   | (concrete|  (lamps,  |(vert)| (drain)|
         | trees)    |  pavers) |  signs,   |      |        |
         |           |          |  hydrants)|      |        |
```

Total sidewalk zone: 150-300cm from curb face to property line.

### 5.2 PCG Sidewalk Generation

Sidewalks are generated as an offset from the road spline:

```
1. For each road edge:
   a. Create offset spline at +/- (road_width/2 + curb_width)
   b. Sample points along offset spline
   c. Spawn sidewalk section meshes (concrete slabs)
   d. Add variation: cracked slabs, heaved slabs, missing slabs (decay)
   e. Spawn curb mesh between road and sidewalk

2. At intersections:
   a. Sidewalk wraps around corners with radius
   b. Crosswalk ramps (ADA-compliant curb cuts) at each crossing point
   c. Crosswalk markings (painted lines) as decals on road surface

3. In planting strip:
   a. Sample points at regular intervals
   b. Spawn trees (mature deciduous, 8-12m spacing)
   c. Grass/weeds between trees
   d. Fire hydrants every 90-180m (prefer near intersections)
```

### 5.3 Curb Geometry

Two approaches:

**Modular mesh curbs:** Pre-made curb sections (straight, corner-inside, corner-outside, ramp, broken). Placed along road edge spline. Standard US curb: 15-20cm tall, 30cm wide, concrete.

**GeometryScript swept profile:** Sweep a curb cross-section (L-shaped: vertical face + horizontal top) along the road edge spline. Continuous, no seams, but less variation.

For horror: broken curbs, crumbled sections, grass growing through cracks, bloodstains.

### 5.4 Crosswalk Generation

At each intersection, for each crossing direction:
1. Identify the gap in sidewalks across the road
2. Place curb ramps (ADA: 1:12 slope, detectable warning surface)
3. Spawn crosswalk decal on road surface (parallel white lines, 3m wide)
4. Optional: raised crosswalk mesh (speed table) for residential areas

Horror variant: faded/barely visible markings, missing ramps, bloodstain crossing decals.

---

## 6. Alleyways

### 6.1 Alley Types for Horror Town

| Type | Width | Between | Features | Horror Potential |
|------|-------|---------|----------|-----------------|
| Service alley | 300-600cm | Building rears | Dumpsters, loading docks, utility boxes | HIGH -- ambush, hiding, chase |
| Side passage | 100-250cm | Building sides (from setbacks) | AC units, pipes, fire escapes | VERY HIGH -- claustrophobic |
| Covered passage | 200-400cm | Through building ground floor | Archway, dark interior | EXTREME -- transition zone |
| Courtyard connector | 200-300cm | Between courtyards | Gates, fences | Medium -- exploration |

### 6.2 Alley Generation

Alleys are derived from the city block layout, not the road network directly:

```
1. City block layout provides:
   - Rear service alley (if block depth > 50m): full-width lane behind lots
   - Side passages: gaps between buildings from side setbacks

2. For service alleys:
   a. Create spline down block center (rear)
   b. Ground surface: asphalt or gravel (lower quality than road)
   c. No sidewalks -- shared space
   d. Props: dumpsters, utility poles (rear), pallets, crates, puddles

3. For side passages:
   a. Narrow gap between buildings (determined by setback rules)
   b. May or may not be passable by player (42cm radius capsule needs 84cm minimum)
   c. Props: AC units, pipes, trash bags, rats
   d. Potentially gated/fenced at street end
```

### 6.3 Alley Props (PCG Scatter)

PCG scatter within alley volumes:

| Prop | Spacing | Density | Decay Variation |
|------|---------|---------|-----------------|
| Dumpster | 1 per building rear | Low | Overflowing, rusted, on fire |
| Trash bags | Clusters near dumpsters | Medium | More = more decay |
| Pallets/crates | Random | Low-Medium | Broken, stacked, scattered |
| Puddles | Low areas | Medium | Oily, bloody at high decay |
| AC units | Building walls | Per-building | Rusted, dripping, sparking |
| Fire escape ladders | Building walls | Per-building | Lowered, broken, bloody handprint |
| Utility boxes/meters | Building walls | 1 per building | Open, sparking |
| Graffiti decals | Walls | Low | More at higher decay |
| Rats | Near trash | Low | More at night/high decay |
| Newspaper/debris | Ground scatter | Medium | Piles at high decay |

---

## 7. Road Markings and Decals

### 7.1 Marking Types

| Marking | Placement | Implementation |
|---------|-----------|----------------|
| Center line (double yellow) | Road centerline | Decal along spline, dashed pattern via material |
| Lane divider (white dashed) | Between lanes | Decal, dash interval material param |
| Edge line (white solid) | Road edges | Decal along road edge spline |
| Stop line | Before intersections | Single decal perpendicular to road |
| Crosswalk | At intersection crossings | Parallel line decals or textured mesh |
| Parking lines | Parallel to curb | Decal segments at regular intervals |
| Turn arrows | Before intersections | Single decal meshes |
| Speed limit paint | Residential roads | Single decal (optional) |
| Railroad crossing X | At railroad crossing | Large decal + raised markings |

### 7.2 Implementation: Material-Based Procedural Markings

The marketplace has a "Procedural Road Markings" system with 120 decals, each with parameters for wear level. But for full procedural control:

**Material approach:** A single road material with parameters controlling:
- `CenterLineOffset` -- UV offset for dashed center line pattern
- `DashLength` / `GapLength` -- dash parameters
- `WearLevel` (0-1) -- faded markings at high values
- `WetnessLevel` (0-1) -- reflective when wet
- `CrackDensity` (0-1) -- procedural crack overlay

This avoids spawning hundreds of decal actors. The road mesh material itself handles markings based on UV coordinates along the spline.

### 7.3 PCG Decal Placement

For markings that need specific placement (stop lines, crosswalks, arrows):

```
1. At each intersection node:
   a. For each incoming road edge:
      - Place stop line decal at (intersection_center - road_direction * intersection_radius)
      - Place crosswalk decal perpendicular to road direction
   b. For each pair of crossing roads:
      - Place turn arrow decals in appropriate lanes

2. Along road segments:
   a. Parking line decals at regular intervals (if road type allows parking)
   b. No-parking zone near hydrants (5m each side)
```

### 7.4 Horror Decay for Markings

| Decay Level | Effect |
|-------------|--------|
| 0.0-0.2 | Fresh markings, clear and bright |
| 0.2-0.4 | Slightly faded, minor wear |
| 0.4-0.6 | Significantly faded, hard to read at night |
| 0.6-0.8 | Barely visible, cracked over, weeds growing through |
| 0.8-1.0 | Gone or illegible, road surface deteriorated |

---

## 8. Street Lights

### 8.1 Street Light Types

| Type | Height | Spacing | Light Type | Where |
|------|--------|---------|------------|-------|
| Cobra head (highway) | 800-1000cm | 30-45m | Sodium/LED downlight | Main streets |
| Acorn globe (decorative) | 350-450cm | 15-25m | Warm white globe | Downtown/historic |
| Modern LED arm | 600-800cm | 25-35m | White LED | Residential |
| Wall-mounted | 300-400cm | Per building | Warm spot | Alleys, building fronts |

### 8.2 PCG Street Light Placement

Street lights follow road splines:

```
PCG Graph: Street Light Generator
1. Input: Road spline + road type
2. Spline Sampler: sample at light_spacing interval
3. Offset points to sidewalk furniture zone (right side of road)
4. Optionally alternate sides (stagger pattern for residential)
5. Mesh Spawner: place pole mesh
6. For each pole: spawn point light component (or BP actor with light)
7. Horror pass: randomly disable/flicker lights based on decay
```

### 8.3 Horror Light States

| State | Probability at Decay 0.0 | Probability at Decay 1.0 | Effect |
|-------|--------------------------|--------------------------|--------|
| Working (steady) | 95% | 5% | Normal illumination |
| Flickering | 3% | 25% | Intermittent light, buzzing SFX |
| Dim/dying | 1% | 20% | Reduced intensity + warm shift |
| Dead (off) | 1% | 30% | No light, dark zone |
| Broken (shattered) | 0% | 15% | Glass on ground, no light |
| Swinging | 0% | 5% | Light swings on loose mount, dynamic shadows |

For horror pacing: ensure at least one working light between dark zones (player needs reference points). The AI Director can dynamically kill lights ahead of the player.

### 8.4 Light Performance

With Lumen, each street light is a movable point light. Performance consideration:
- At 30m spacing over 6 blocks (~50 streets x 100m avg): ~170 street lights
- Most will be shadow-casting = expensive
- Mitigation: virtual shadow maps handle this well in UE5.7
- Further: lights beyond view distance auto-disable via light culling
- Budget: ~50 visible lights at any time, manageable with VSM

---

## 9. Power Lines and Telephone Poles

### 9.1 Catenary Curve Math

Power lines sag between poles following a catenary curve:

```
y(x) = a * cosh(x/a) - a

where:
  a = T_horizontal / (w * g)
  T_horizontal = horizontal tension in wire
  w = wire weight per unit length
  g = gravity

Simplified sag formula:
  sag = w * L^2 / (8 * T_horizontal)

where L = span length between poles
```

For game purposes, the parabolic approximation is fine (error < 0.5% for typical pole spacing):

```
y(t) = 4 * sag * t * (1 - t)    // t in [0, 1] along span

Typical sag for 30m span: 50-100cm
Typical sag for 45m span: 100-200cm
```

### 9.2 Pole Types

| Type | Height | Spacing | Features | Where |
|------|--------|---------|----------|-------|
| Wooden utility pole | 900-1200cm | 30-45m | Cross-arm, insulators, transformer | Residential |
| Concrete/steel pole | 1200-1500cm | 45-60m | Multiple cross-arms | Main streets |
| Junction pole | 900-1200cm | At intersections | Guy wires, multiple directions | Intersections |

### 9.3 Wire Configuration

A typical residential utility pole carries:
1. **Power lines** (top): 3 phase wires + neutral, highest position
2. **Communication cables** (middle): telephone, cable TV, fiber
3. **Street light arm** (may be attached): extends toward road
4. **Transformer** (on some poles): cylindrical, at top

### 9.4 Implementation

**Pole placement:** PCG spline sampler along road edge, offset to planting strip.

**Wire generation:** For each consecutive pair of poles:
1. Get start/end positions of each wire attachment point
2. Compute catenary sag based on span length
3. Generate wire mesh:
   - Option A: `UCableComponent` (UE built-in, physics-capable but expensive)
   - Option B: Spline mesh with catenary shape (static, cheap)
   - Option C: GeometryScript tube along catenary sample points (best for batched generation)

**Recommended:** Option B for most wires (static spline mesh). Option A for 1-2 "hero" wires near the player that can sway in wind. Generate the catenary as a set of spline points:

```cpp
// Generate N points along catenary between two poles
TArray<FVector> CatenaryPoints;
float Sag = WireWeight * FMath::Square(SpanLength) / (8.0f * Tension);
for (int32 i = 0; i <= NumSegments; ++i)
{
    float T = (float)i / NumSegments;
    float X = FMath::Lerp(StartPos.X, EndPos.X, T);
    float Y = FMath::Lerp(StartPos.Y, EndPos.Y, T);
    float Z = FMath::Lerp(StartPos.Z, EndPos.Z, T) - 4.0f * Sag * T * (1.0f - T);
    CatenaryPoints.Add(FVector(X, Y, Z));
}
```

### 9.5 Horror Power Line States

| State | Effect |
|-------|--------|
| Normal | Wires taut, pole straight |
| Leaning pole | Pole rotated 5-15 degrees, wires slack |
| Broken wire | Wire dangles from one end, sparks particle effect |
| Downed pole | Pole on ground, wires across road (obstacle + hazard) |
| Buzzing transformer | Audio + slight spark VFX, flickering nearby light |
| Missing pole | Gap in line, wires span extra distance with heavy sag |

---

## 10. Fences and Walls

### 10.1 Fence Types for Horror Town

| Type | Height | Where | Horror Variant |
|------|--------|-------|----------------|
| Chain link | 120-180cm | Industrial, schools, backyards | Torn, bent, holes cut through |
| Wooden picket | 90-120cm | Residential front yards | Missing pickets, rotted, leaning |
| Privacy fence (wood) | 180-200cm | Residential backyards | Warped boards, graffiti, blood |
| Wrought iron | 120-180cm | Cemeteries, churches, mansions | Rusted, bent bars, impaled items |
| Stone/brick wall | 90-180cm | Property boundaries, retaining | Crumbled sections, overgrown |
| Concrete block | 180-240cm | Commercial, industrial | Cracks, graffiti, collapse sections |
| Barbed wire (topper) | +30-60cm on other fences | Industrial, restricted areas | Rusty, items caught in it |
| Barricade (horror) | Variable | Player/NPC-constructed | Makeshift, mixed materials |

### 10.2 PCG Fence Generation

UE5.7 has official documentation on fence generation using PCG Shape Grammar. The pipeline:

```
PCG Graph: Fence Generator
1. Input: Property boundary spline (from lot subdivision)
2. Spline Sampler: sample at post_spacing interval (e.g., 200-300cm)
3. At each sample point: spawn fence post mesh
4. Between posts: spawn fence panel mesh (stretched or instanced)
5. At gates: skip panel, spawn gate mesh instead
6. At corners: spawn corner post (thicker/reinforced)
7. Horror pass: randomly remove panels, lean posts, add damage
```

**PCG Shape Grammar approach** (from Epic's official docs):
- Define grammar rules: `Post -> Panel -> Post -> Panel -> Gate -> ...`
- Execute along spline with positional constraints
- Grammar handles corner resolution and gate placement automatically

### 10.3 Gate Placement Logic

Gates need to align with:
- Building entrances (front door faces street -> gate in front fence)
- Driveways (gap in fence + driveway surface)
- Alley access (rear gate in back fence)

```
For each lot:
  front_fence_spline = lot boundary facing street
  For each building entrance on this lot:
    gate_position = project entrance onto front_fence_spline
    replace fence panel at gate_position with gate mesh
```

---

## 11. Railroad Tracks

### 11.1 Track Components

| Component | Spacing/Size | Implementation |
|-----------|-------------|----------------|
| Rail (2x) | 143.5cm gauge (standard) | Spline mesh along track spline |
| Ties (sleepers) | 50-60cm spacing | PCG point sampler + mesh spawner |
| Ballast (gravel bed) | 250-300cm wide, 15-20cm deep | Landscape paint or mesh strip |
| Grade crossing surface | Road width x track width | Dedicated intersection mesh |
| Crossing gate (2x) | One per road direction | BP actor with animation |
| Crossing signal | Flashing red lights + bell | BP actor |
| Warning signs | Crossbuck, advance warning | Static mesh placement |

### 11.2 PCG Railroad Generation

```
PCG Graph: Railroad Track
1. Input: Track spline (may be straight or gentle curve)
2. Spline Sampler (fine): sample every 55cm for ties
3. Mesh Spawner: place tie mesh at each point
4. Spline Mesh: two rail meshes along spline, offset +/- 71.75cm from center
5. Surface Sampler: sample area under track for ballast
6. Mesh Spawner: scatter gravel/ballast meshes

At road crossings:
1. Detect intersection with road splines (UE5.7 Spline Intersection)
2. Spawn crossing surface mesh (flush with road)
3. Spawn crossing gate BPs on both road sides
4. Spawn crossbuck signs
5. Add audio trigger volume for train horn
```

### 11.3 Horror Railroad

- Abandoned vs active: active trains = dynamic threat, abandoned = exploration
- Rusty rails, overgrown ties, missing sections
- Derailed train car as environment piece (blocking path, explorable interior)
- Crossing gates stuck in down position, flashing malfunction pattern
- Train horn heard in distance as ambient horror audio cue

---

## 12. Drainage and Ditches

### 12.1 Drainage Types

| Type | Location | Size | Implementation |
|------|----------|------|----------------|
| Road gutter | Between curb and road surface | 30-60cm wide, 5-10cm deep | Part of road cross-section mesh |
| Storm drain inlet | At intersections, low points | 60x30cm grate | Static mesh at specific points |
| Drainage ditch | Along rural roads | 100-200cm wide, 30-60cm deep | GeometryScript trench or landscape sculpt |
| Culvert | Under driveways/roads | 30-60cm pipe diameter | Pipe mesh at crossings |
| Puddle | Low spots, broken drainage | Variable | Decal + planar reflection or SSR |

### 12.2 Storm Drain Placement

Storm drains follow a real pattern:
- At every intersection corner (4 per 4-way intersection)
- At low points between intersections (every 90-120m on slopes)
- At the base of hills

```
PCG placement:
1. At each intersection node: spawn 1 drain per corner (4 for 4-way, 3 for T)
2. Along road segments: spawn drains at regular intervals based on grade
3. Puddle decals near drains (especially at high decay levels)
```

### 12.3 Horror Drainage

- Drains as audio sources: water sounds, whispers, growling from below
- Clown arm reaching from drain (IT reference -- use sparingly)
- Flooded sections: standing water on road from blocked drains
- Storm drain as gameplay mechanic: small items fall through, can hear enemies below
- Blood flowing into drain (environmental storytelling)

---

## 13. PCGEx Graph-Based Road Networks

### 13.1 PCGEx Capabilities for Roads

PCGEx (MIT license, 200+ nodes) provides the graph theory foundation missing from native PCG:

**Graph Construction:**
- **Build Delaunay** -- triangulates points into a connected graph (every point connects to neighbors)
- **Build Voronoi** -- dual of Delaunay, creates cell boundaries (useful for block shapes)
- **Build MST** -- minimum spanning tree (guaranteed connected, no cycles = tree-like road network)
- **Build Convex Hull** -- outer boundary of point set
- **Custom Builders** -- define connection rules programmatically

**Graph Operations:**
- **Edge filtering** -- remove edges by length, angle, attribute
- **Node filtering** -- remove nodes by degree, attribute
- **Pathfinding** -- A*/Dijkstra with pluggable heuristics
- **Edge extra data** -- compute per-edge properties (length, direction, angle at nodes)

**Path Operations:**
- **Smooth** -- add curvature to angular paths
- **Subdivide** -- add intermediate points for finer resolution
- **Offset** -- parallel offset (for sidewalks!)
- **Bevel** -- round corners
- **Fuse** -- merge close points
- **Path to Spline** -- convert point path to USplineComponent

### 13.2 PCGEx Road Network Pipeline

```
Step 1: Define intersection points
  - Grid layout: create points at regular intervals (block corners)
  - Or: manually place key intersection points via MCP action

Step 2: Build graph
  - PCGEx Build Delaunay (connects all nearby points)
  - Filter edges: remove diagonals, keep only roughly horizontal/vertical
  - Or: Build from explicit edge list

Step 3: Classify roads
  - Main roads: edges connecting to town center or main axis
  - Residential: remaining grid edges
  - Alleys: mid-block edges added between lots
  - Dead ends: randomly remove some edges for horror

Step 4: Generate road geometry per edge
  - Convert each edge to spline
  - PCG subgraph processes each spline for mesh placement
  - Width, material, marking style based on road classification

Step 5: Generate intersections
  - At each node with degree > 2: detect intersection type
  - Spawn intersection mesh/decal

Step 6: Generate linear features
  - Sidewalks: offset splines from road edges
  - Street lights: sample along sidewalk spline
  - Power lines: sample along road at pole spacing
  - Fences: along lot boundaries
```

### 13.3 Horror-Specific Graph Modifications

After generating the base road network:
1. **Remove 1-2 edges** to create dead ends (horror: no escape feeling)
2. **Block roads** with vehicle barriers, collapsed buildings, or fallen trees
3. **Add one-way alleys** that funnel player toward danger
4. **Create loops** so the player feels lost (connect some dead ends with narrow passages)
5. **Ensure connectivity** -- the player must be able to reach all buildings, but the path should feel unsafe

---

## 14. Horror Decay System

### 14.1 Unified Decay Parameter

All linear infrastructure takes a single `decay` parameter (0.0-1.0) that propagates to all sub-features:

```
decay = 0.0 (pristine small town)
  - Fresh asphalt, bright markings, all lights working
  - Clean sidewalks, maintained fences, tidy alleys
  - Power lines taut, poles straight

decay = 0.3 (neglected)
  - Minor cracks in road, faded markings
  - Occasional broken light, weeds in sidewalk cracks
  - Fence needs paint, minor rust on chain link

decay = 0.5 (abandoned 1-2 years)
  - Significant road damage, potholes
  - 30% lights dead, others flickering
  - Grass growing through sidewalk, missing fence panels
  - Leaning poles, slack wires

decay = 0.7 (abandoned 5+ years)
  - Road barely recognizable, vegetation reclaiming
  - Most lights dead, poles down in places
  - Fences collapsed, overgrown with vines
  - Some roads impassable without clearing

decay = 1.0 (post-apocalyptic)
  - No functioning infrastructure
  - Nature fully reclaimed
  - Twisted metal, collapsed structures
```

### 14.2 Per-Feature Decay Tables

Each feature type maps the global decay to specific visual states using probability tables (see sections 8.3, 9.5, etc. above). The mapping can use response curves:

```cpp
// Exponential decay for lights (break faster than roads)
float LightDecay = FMath::Pow(GlobalDecay, 0.5f);  // 0.5 global -> 0.71 light

// Linear decay for road surface
float RoadDecay = GlobalDecay;

// Slow decay for stone walls (sturdy)
float StoneDecay = FMath::Pow(GlobalDecay, 2.0f);  // 0.5 global -> 0.25 stone
```

### 14.3 Spatial Decay Variation

Decay shouldn't be uniform across the town:
- **Town center:** slightly lower decay (more maintained before abandonment)
- **Outskirts:** higher decay (first to deteriorate)
- **Near water/wetland:** higher decay (moisture accelerates deterioration)
- **Near "the event":** maximum decay (whatever caused the horror)

This can be driven by a 2D decay map texture sampled at each feature's position.

---

## 15. Integration with Building System

### 15.1 Roads Define Building Placement

The road network is a prerequisite for building placement:

```
Pipeline:
1. Generate road network (this system)
2. Extract blocks (closed polygons from road graph cycles)
3. Subdivide blocks into lots (city block layout system)
4. Place buildings on lots (building generation system)
5. Generate sidewalks, lights, etc. along roads (this system)
6. Generate alley props between buildings (this system)
7. Generate fences along lot boundaries (this system)
```

### 15.2 Building Entrance Alignment

Every building needs at least one entrance facing a road:
- Query the lot's street-facing edge(s)
- Building entrance (front door) must be on a street-facing wall
- Sidewalk must connect building entrance to street
- Driveway (if residential) connects to road at right angles

### 15.3 Address System

For horror/gameplay (finding specific buildings):
```
address = street_name + lot_number_on_street
street_name = from road edge attribute
lot_number = sequential along street, odds on one side, evens on other (US convention)
```

### 15.4 Spatial Registry Integration

All generated linear features register with the Spatial Registry (`2026-03-28-spatial-registry-research.md`):
- Road segments with type, width, condition
- Intersections with connected roads
- Street light positions with working/broken state
- Blocked/impassable road segments
- This data feeds AI navigation and horror encounter design

---

## 16. Existing Tools and Marketplace

### 16.1 UE Marketplace/Fab Assets

| Tool | Type | Price | Relevance |
|------|------|-------|-----------|
| **Road Creator Pro** | Plugin | Paid | Full road system: splines, lanes, markings, traffic lights, bridges, tunnels. Day-night cycle. 1-8 lanes |
| **Errant Paths** | Plugin | Paid | Spline networks for roads, railways, tunnels, fences, power lines. Auto-sectioning, instancing, RVT |
| **Easy Decal Roads RVT** | Plugin | Paid | RVT-based road creation with customizable markings and intersections |
| **Procedural Road Generator** | BP/Plugin | Paid | PCG-based road generation with spline sampling |
| **Procedural Railroads** | Plugin | Paid | Complete railroad system: tracks, bridges, crossing signals, train simulation |
| **PowerLine Generator Pack** | BP | Paid | Spline-based power lines with auto-connecting wires and adjustable slack |
| **Street Lights Pack Modular** | Asset | Paid | Modular street light meshes |
| **Spline Fences and Walls** | BP | Paid | 13 fence/wall styles with slope handling |
| **Procedural Road Markings** | Material | Paid | 120 road marking decals with wear parameters |
| **Cassini Sample** | Free | Free | Epic's PCG demo: shape grammar, splines, primitive workflows |

### 16.2 Open Source

| Tool | License | Description |
|------|---------|-------------|
| **PCGEx** | MIT | 200+ nodes for PCG graph theory, pathfinding, spatial ops |
| **PCGPathfinding** | MIT | A* pathfinding for PCG spline generation between points |
| **Modular_Road_Tool** | MIT | Modular road tool for UE4 (outdated but reference-worthy) |
| **tmwhere City Generator** | MIT | JS procedural city generator (tensor field + L-system) |
| **ProbableTrain City Generator** | MIT | Browser-based procedural city generator |

### 16.3 Recommendation

For Leviathan, we should NOT buy a complete road tool. The horror-specific requirements (decay, broken infrastructure, AI integration, spatial registry) mean we need custom generation. However:

- **Use PCGEx** (MIT, free) -- essential for graph-based road networks
- **Reference Errant Paths** for architecture patterns (spline-to-mesh, auto-sectioning)
- **Reference Road Creator Pro** for marking/material approaches
- **Use Cassini Sample** (free) as PCG learning resource
- **Build custom** for horror decay, AI integration, Monolith MCP actions

---

## 17. Recommended Architecture

### 17.1 System Layers

```
Layer 4: Horror Overlay
  - Decay system (per-feature state machines)
  - AI Director integration (dynamic light/obstacle control)
  - Event triggers (crossing railroad = train horn)

Layer 3: Linear Feature Generators (PCG Subgraphs)
  - Road surface generator (modular mesh along spline)
  - Sidewalk generator (offset spline + mesh)
  - Street light generator (spline sampler + BP spawner)
  - Power line generator (pole placement + catenary wires)
  - Fence generator (property boundary + shape grammar)
  - Railroad generator (track spline + tie sampler)
  - Marking generator (decal placement at intersections)
  - Drain generator (intersection corners + low points)

Layer 2: Road Network Graph
  - Node/edge graph (FRoadNetwork)
  - Intersection classification
  - Block extraction (polygon cycles)
  - PCGEx graph construction

Layer 1: Input / Design
  - MCP action: create_road_network (from params or explicit graph)
  - MCP action: modify_road (add/remove/change edges)
  - Designer spline placement (manual mode)
  - Import from JSON (serialized road network)
```

### 17.2 Data Flow

```
MCP create_road_network
  |
  v
FRoadNetwork (JSON-serializable graph)
  |
  +---> Block Extraction ---> City Block Layout System
  |
  +---> Per-Edge: Create USplineComponent
  |       |
  |       +---> PCG Graph: Road Surface
  |       +---> PCG Graph: Sidewalks (offset splines)
  |       +---> PCG Graph: Street Lights
  |       +---> PCG Graph: Power Lines
  |       +---> PCG Graph: Road Markings
  |
  +---> Per-Node: Intersection Classification
  |       |
  |       +---> Spawn intersection mesh
  |       +---> Crosswalk generation
  |       +---> Drain placement
  |
  +---> Per-Lot-Boundary: Fence Generation
  |
  +---> Alley Splines (from block layout)
          |
          +---> PCG Graph: Alley Surface
          +---> PCG Graph: Alley Props
```

### 17.3 PCG Graph Organization

Each linear feature type gets its own PCG subgraph, callable from a master road generation graph:

```
PCG_Road_Master (top-level graph)
  |-- PCG_Road_Surface (subgraph, per road segment)
  |-- PCG_Sidewalk (subgraph, per road segment, both sides)
  |-- PCG_StreetLight (subgraph, per road segment)
  |-- PCG_PowerLine (subgraph, per road segment)
  |-- PCG_RoadMarkings (subgraph, per road + intersections)
  |-- PCG_Fence (subgraph, per lot boundary)
  |-- PCG_Alley (subgraph, per alley segment)
  |-- PCG_Railroad (subgraph, if applicable)
  |-- PCG_Drainage (subgraph, per intersection + road)
  |-- PCG_HorrorOverlay (subgraph, applies decay to all)
```

### 17.4 C++ vs PCG Graph Split

| Component | Implementation | Reason |
|-----------|---------------|--------|
| Road network graph | C++ (FRoadNetwork) | Complex graph algorithms, serialization |
| Block extraction | C++ | Polygon cycle detection |
| Intersection classification | C++ | Algorithm, not asset placement |
| Catenary calculation | C++ | Math utility |
| Decay state machines | C++ | Stateful, AI Director integration |
| Road mesh placement | PCG Graph | Native PCG strength |
| Sidewalk placement | PCG Graph | Spline offset + mesh spawn |
| Street light placement | PCG Graph | Regular spacing along spline |
| Prop scatter (alleys) | PCG Graph | Random distribution |
| Fence generation | PCG Shape Grammar | Epic's documented approach |
| Power line poles | PCG Graph | Regular spacing |
| Power line wires | C++ + GeometryScript | Catenary geometry generation |
| Road markings | PCG Graph + Material | Decal placement + material params |

---

## 18. MCP Actions

### 18.1 Core Road Network Actions

| Action | Params | Description |
|--------|--------|-------------|
| `create_road_network` | `grid_size`, `block_dimensions`, `road_types`, `decay`, `seed` | Generate a grid-based road network |
| `create_road_network_from_graph` | `nodes[]`, `edges[]`, `decay` | Create road network from explicit graph definition |
| `modify_road_network` | `network_id`, `operation`, `params` | Add/remove/modify roads in existing network |
| `get_road_network` | `network_id` | Return road network graph as JSON |
| `generate_road_geometry` | `network_id`, `features[]` | Generate specified feature types (roads, sidewalks, lights, etc.) |

### 18.2 Per-Feature Actions

| Action | Params | Description |
|--------|--------|-------------|
| `create_road_segment` | `start`, `end`, `road_type`, `width`, `decay` | Single road segment with surface + markings |
| `create_sidewalk` | `road_segment_id`, `side`, `width`, `decay` | Sidewalk along a road segment |
| `create_street_lights` | `road_segment_id`, `light_type`, `spacing`, `decay` | Street lights along a road |
| `create_power_lines` | `road_segment_id`, `pole_type`, `spacing`, `decay` | Power line poles + catenary wires |
| `create_fence` | `spline_points[]`, `fence_type`, `gate_positions[]`, `decay` | Fence along a boundary |
| `create_railroad` | `spline_points[]`, `has_crossing`, `decay` | Railroad track segment |
| `create_alley` | `block_id`, `alley_type`, `decay` | Alley with surface + props |
| `create_intersection` | `position`, `connected_roads[]`, `type_override` | Intersection mesh + markings |

### 18.3 Horror Actions

| Action | Params | Description |
|--------|--------|-------------|
| `set_road_decay` | `network_id`, `decay_value`, `decay_map_texture` | Set global + spatial decay |
| `block_road` | `edge_id`, `obstacle_type` | Place obstacle blocking a road segment |
| `kill_light` | `light_id`, `state` | Set specific light to dead/flickering/etc. |
| `create_road_hazard` | `position`, `hazard_type` | Downed power line, sinkhole, blood pool, etc. |

### 18.4 Query Actions

| Action | Params | Description |
|--------|--------|-------------|
| `find_nearest_road` | `position` | Find closest road segment to a world position |
| `get_road_at` | `position` | Get road info at a specific point |
| `get_blocks` | `network_id` | Return extracted city blocks from road network |
| `get_street_address` | `building_id` | Return street address for a building |

**Total new actions:** ~17-20

---

## 19. Implementation Plan

### Phase 1: Road Network Graph (C++) -- ~20-28h
- `FRoadNetwork`, `FRoadNode`, `FRoadEdge` data structures
- Grid-based road network generator (input: rows, cols, block size, variation)
- Intersection classification algorithm
- Block polygon extraction (cycle detection in planar graph)
- JSON serialization (save/load road networks)
- MCP actions: `create_road_network`, `create_road_network_from_graph`, `get_road_network`, `get_blocks`
- Integration: road network feeds into city block layout system

### Phase 2: Road Surface + Sidewalks (PCG + C++) -- ~25-35h
- Road spline creation from graph edges
- PCG subgraph: road surface mesh placement along spline
- PCG subgraph: sidewalk generation (offset spline, curb, concrete sections)
- Intersection mesh spawning (4-way, T-junction, dead-end)
- Crosswalk generation at intersections
- Landscape sculpting under roads (optional, if landscape present)
- MCP actions: `create_road_segment`, `create_sidewalk`, `create_intersection`
- Modular road mesh set: straight, curve-L, curve-R, end cap (blockout quality initially)

### Phase 3: Street Lights + Road Markings -- ~15-20h
- PCG subgraph: street light placement along road splines
- Street light BP actor: point light + flicker component + broken state
- PCG subgraph: road marking decal placement
- Material: procedural center line (dashed pattern in material, not individual decals)
- Stop lines and crosswalk decals at intersections
- MCP actions: `create_street_lights`, `kill_light`

### Phase 4: Power Lines + Fences -- ~20-28h
- PCG subgraph: utility pole placement
- Catenary wire generation (GeometryScript tube along catenary points)
- Wire sag calculation utility
- PCG Shape Grammar: fence generation along lot boundaries
- Fence types: chain link, picket, privacy, wrought iron
- Gate placement logic (align to building entrances)
- MCP actions: `create_power_lines`, `create_fence`

### Phase 5: Alleys + Drainage + Railroad -- ~15-22h
- PCG subgraph: alley surface and prop scatter
- Storm drain placement at intersections and low points
- Puddle decal scatter near drains
- Railroad track generation (if needed -- may be optional for Leviathan)
- Railroad crossing gate BP
- MCP actions: `create_alley`, `create_railroad`

### Phase 6: Horror Decay System -- ~15-20h
- Global decay parameter propagation
- Per-feature state machines (working, flickering, broken, destroyed)
- Spatial decay map (texture-driven variation)
- Response curve mapping (different features decay at different rates)
- AI Director hooks (dynamic light/obstacle control)
- Road hazard generation (downed lines, sinkholes, blood)
- MCP actions: `set_road_decay`, `block_road`, `create_road_hazard`

### Phase 7: Integration + Polish -- ~10-15h
- Spatial registry integration (all features registered)
- Building entrance alignment validation
- Address system
- Performance profiling (HISM for fences/poles, light culling)
- Debug visualization (road network overlay, intersection types, decay heat map)
- MCP query actions: `find_nearest_road`, `get_road_at`, `get_street_address`

**Total estimate: ~120-168h** across all phases

**MVP (Phases 1-2): ~45-63h** -- functional road network with surfaces and sidewalks

---

## 20. Sources

### Official Documentation
- [UE5.7 PCG Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview) -- Production-ready PCG framework documentation
- [UE5.7 PCG Editor Mode](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-editor-mode-in-unreal-engine) -- New interactive PCG tools
- [UE5.7 Landscape Splines](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-splines-in-unreal-engine) -- Landscape spline system
- [PCG Shape Grammar: Fence Generator](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-a-fence-generator-using-shape-grammar-in-unreal-engine) -- Official fence generation tutorial
- [Using Shape Grammar With PCG](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine) -- Shape grammar system
- [Runtime Virtual Texturing](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine) -- RVT documentation

### Community Tutorials
- [Procedural Road Generation in UE5 (PCG)](https://dev.epicgames.com/community/learning/tutorials/9dpd/procedural-road-generation-in-unreal-engine-5-pcg) -- Step-by-step road generation
- [City Streets with PCG and PCGEx](https://dev.epicgames.com/community/learning/tutorials/VxP9/unreal-engine-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg) -- PCGEx-based city street generation (Jan 2026)
- [Create Cities with PCG Splines](https://dev.epicgames.com/community/learning/tutorials/Obk3/create-entire-cities-automatically-with-pcg-splines-procedural-content-generation-in-unreal-engine) -- Spline-based city generation
- [Spawn Meshes Along Spline with PCG](https://dev.epicgames.com/community/learning/tutorials/Vp9X/how-to-spawn-meshes-along-a-spline-using-pcg-in-unreal-engine-5) -- Spline mesh spawning basics
- [PCG Spline Mesh Magic (UE5.5)](https://dev.epicgames.com/community/learning/tutorials/aw1D/unreal-engine-unreal-5-5-pcg-spline-mesh-magic) -- Spline mesh techniques
- [Dynamic Splines from PCG for Sidewalks](https://dev.epicgames.com/community/learning/tutorials/Zeov/unreal-engine-create-dynamic-splines-from-pcg-to-generate-a-sidewalk) -- Sidewalk generation
- [PCG Spline Fence Placement](https://dev.epicgames.com/community/learning/tutorials/0qyy/unreal-engine-the-proper-tool-for-pcg-spline-fence-placement) -- Fence tool tutorial
- [Procedural Railroads and Train Simulation](https://dev.epicgames.com/community/learning/tutorials/XjaV/unreal-engine-procedural-railroads-and-train-simulation) -- Railroad generation
- [Introduction to PCG Grammar (UE5.5)](https://dev.epicgames.com/community/learning/tutorials/PYEX/introduction-to-pcg-grammar-in-unreal-engine-5-5) -- Grammar system basics
- [Procedural Towns with PCG and Cargo](https://dev.epicgames.com/community/learning/tutorials/dXR7/unreal-engine-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo) -- Town generation tutorial
- [Smart Material for Road Markings](https://dev.epicgames.com/community/learning/tutorials/xZrq/unreal-engine-a-smart-material-for-roads-or-runways-markings) -- Material-based markings

### PCGEx
- [PCGEx GitHub](https://github.com/PCGEx/PCGExtendedToolkit) -- MIT licensed, 200+ nodes
- [PCGEx Documentation](https://pcgex.gitbook.io/pcgex) -- Official docs
- [PCGEx All Nodes Reference](https://nebukam.github.io/PCGExtendedToolkit/all-nodes.html) -- Complete node list
- [PCGEx Path Spline Mesh](https://nebukam.github.io/PCGExtendedToolkit/doc-paths/paths-spline-mesh-simple.html) -- Path to spline mesh conversion
- [PCGEx Pathfinding](https://nebukam.github.io/PCGExtendedToolkit/doc-pathfinding/) -- A*/Dijkstra pathfinding
- [PCGEx on Fab](https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624) -- Fab listing
- [Inside Unreal: PCGEx Plugin](https://forums.unrealengine.com/t/inside-unreal-taking-pcg-to-the-extreme-with-the-pcgex-plugin/2479952) -- Epic's feature on PCGEx

### Academic / Algorithmic
- Parish & Mueller, "Procedural Modeling of Cities", SIGGRAPH 2001 -- [ACM DL](https://dl.acm.org/doi/10.1145/383259.383292)
- Chen et al., "Interactive Procedural Street Modeling", SIGGRAPH 2008 -- [Paper](https://www.sci.utah.edu/~chengu/street_sig08/street_sig08.pdf)
- Zhang et al., "Road Network Generation Based on Tensor Field and Multi-Agent", ISPRS 2022 -- [ResearchGate](https://www.researchgate.net/publication/364451549_A_METHOD_FOR_ROAD_NETWORK_GENERATION_BASED_ON_TENSOR_FIELD_AND_MULTI-AGENT)
- Procedural City Generation Survey -- [CitGen](http://www.citygen.net/files/Procedural_City_Generation_Survey.pdf)

### Blog Posts / Reference Implementations
- [Martin Devans: Procedural Road Generation](https://martindevans.me/game-development/2015/12/11/Procedural-Generation-For-Dummies-Roads/) -- Excellent road generation walkthrough
- [tmwhere: Procedural City Generation](https://www.tmwhere.com/city_generation.html) -- JS tensor field road generator with source code
- [Jake Lem: Procedural City](https://jakelem.com/code/procedural-city/) -- Tensor field implementation
- [phiresky: Procedural Cities](https://github.com/phiresky/procedural-cities/blob/master/paper.md) -- Academic-style writeup with code
- [Jean-Paul Software: Manhattan to Berlin](https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/) -- PCG city generation in UE5

### Marketplace Tools (Reference)
- [Road Creator Pro](https://www.fab.com/listings/adc4696d-7ab7-4d79-9194-fd29fdc0edfa) -- Full road creation system
- [Errant Paths](https://www.errantphoton.com/) -- Spline network tool for roads, rails, fences, power lines
- [Procedural Railroads](https://www.fab.com/listings/232dabb6-1a9e-4d9b-9c8f-2aad92858e0d) -- Railroad system with crossing gates
- [Procedural Road Markings](https://www.unrealengine.com/marketplace/en-US/product/procedural-road-markings) -- 120 marking decals
- [Easy Decal Roads RVT](https://www.fab.com/listings/7ceee069-9c09-4597-bdf4-e02b503c9cad) -- RVT road tool
- [PowerLine Generator Pack](https://www.unrealengine.com/marketplace/en-US/product/allpacs-powerlines-generator-pack/) -- Power line blueprints
- [Cassini Sample (Free)](https://www.fab.com/listings/3f7cd12c-30b3-47d6-90c2-8604ed068ab7) -- Epic's PCG demo project

### Catenary Math
- [Matt Pewsey: Wire Sag-Tension Algorithm](https://mpewsey.github.io/2021/12/17/sag-tension-algorithm.html) -- Implementation reference
- [Catenary Curve Calculator](https://www.omnicalculator.com/math/catenary-curve) -- Interactive calculator
- [Wikipedia: Catenary](https://en.wikipedia.org/wiki/Catenary) -- Mathematical reference

### Forum Discussions
- [PCG Roads and Junctions](https://forums.unrealengine.com/t/pcg-2024-roads-junctions-is-it-secret-knowledge/1730472) -- Community discussion on PCG road generation challenges
- [PCG Spline Intersections Blueprint](https://blueprintue.com/blueprint/0fcusw9t/) -- Shared PCG graph for spline intersections
- [Mesh Deformation Along Spline Curves](https://forums.unrealengine.com/t/mesh-deformation-along-spline-curves-with-pcg/1311974) -- Technical discussion
- [Making Roads: The Ultimate Solution?](https://polycount.com/discussion/215697/making-roads-and-paths-have-anyone-found-the-ultimate-solution) -- Polycount community survey of approaches

### Horror Reference
- [The Quiet Horror of Procedural Generation](https://boingboing.net/2020/12/15/the-quiet-horror-of-procedural-generation.html) -- Kenopsia and procedural horror atmosphere
- [Gothic Horror in Unreal Engine](https://www.exp-points.com/marcin-wiech-creating-a-gothic-horror-in-unreal-engine) -- Horror environment art approach

---

## Appendix A: Road Cross-Section Profiles

### Main Street (1400cm total ROW)
```
|<-- 200 -->|<-- 15 -->|<-- 60 -->|<-- 250 -->|<-- 250 -->|<-- 60 -->|<-- 15 -->|<-- 200 -->|
| Sidewalk  | Curb     | Parking  |  Lane 1   |  Lane 2   | Parking  | Curb     | Sidewalk  |
|  (conc)   | (conc)   | (asphalt)| (asphalt) | (asphalt) | (asphalt)| (conc)   |  (conc)   |
|           |          |          |  < - - >  |           |          |          |           |
|           |          |          | Ctr Line  |           |          |          |           |
```

### Residential Street (1000cm total ROW)
```
|<-- 150 -->|<-- 15 -->|<-- 320 -->|<-- 320 -->|<-- 15 -->|<-- 150 -->|
| Sidewalk  | Curb     |  Lane 1   |  Lane 2   | Curb     | Sidewalk  |
|  (conc)   | (conc)   | (asphalt) | (asphalt) | (conc)   |  (conc)   |
```

### Alley (400cm, no sidewalk)
```
|<-- 400 -->|
|  Shared   |
| (gravel/  |
|  asphalt) |
```

## Appendix B: Modular Road Mesh Set

Minimum mesh set for modular road generation:

| Mesh | Dimensions | Variants |
|------|-----------|----------|
| Road_Straight_Main | 400x1400cm | Clean, Cracked, Pothole |
| Road_Straight_Residential | 400x1000cm | Clean, Cracked, Pothole |
| Road_Curve_Main_15deg | 400cm arc | Clean, Cracked |
| Road_Curve_Residential_15deg | 400cm arc | Clean, Cracked |
| Intersection_4Way_Main | 1400x1400cm | Clean, Cracked |
| Intersection_T_Main | 1400x1400cm | Clean, Cracked |
| Intersection_DeadEnd_Main | 1400x400cm | Clean, Cracked |
| Sidewalk_Straight | 400x150cm | Clean, Cracked, Heaved, Missing |
| Sidewalk_Corner_90 | 150x150cm | Clean, Cracked |
| Curb_Straight | 400x15cm | Clean, Broken |
| Curb_Ramp_ADA | 150x60cm | Clean |
| Crosswalk | 300x1000cm | Clean, Faded |

These would be blockout-quality GeometryScript meshes initially, replaced with art-quality meshes later.

## Appendix C: Performance Budget

For a 6-block horror town:

| Feature | Count | Method | Draw Calls | Memory |
|---------|-------|--------|------------|--------|
| Road segments | ~60 | HISM | 3-5 (per variant) | ~2MB meshes |
| Sidewalk sections | ~120 | HISM | 3-5 | ~1MB |
| Street lights | ~50 | BP actors | 50 (lights) | ~1MB |
| Power line poles | ~40 | HISM | 1-2 | ~500KB |
| Power line wires | ~35 | Static mesh | 35 | ~500KB |
| Fence sections | ~200 | HISM | 4-6 (per type) | ~2MB |
| Intersection meshes | ~12 | Static mesh | 12 | ~1MB |
| Road marking decals | ~30 | Decal actors | 30 | ~500KB |
| Alley props | ~100 | HISM | 5-8 | ~2MB |
| Storm drains | ~20 | HISM | 1 | ~200KB |
| **Total** | **~667** | | **~150-160** | **~11MB** |

Well within budget. HISM instancing keeps draw calls low despite hundreds of placed objects.
