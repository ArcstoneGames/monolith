# Procedural City Block & Street Layout Generation Research

**Date:** 2026-03-28
**Context:** Monolith `mesh_query` procedural geometry expansion -- single city block generation for FPS survival horror (abandoned small town).
**Existing infrastructure:** `create_structure`, `create_building_shell`, `create_maze`, `create_parametric_mesh`, `create_horror_prop` already exist in `MonolithMeshProceduralActions`.

---

## 1. Street Network Generation

For Leviathan's use case (a single block or handful of blocks, not a full city), we do NOT need the heavyweight approaches. But for context:

### 1.1 Parish & Mueller L-System Roads (SIGGRAPH 2001)

The foundational paper. An extended L-system generates highway and street networks from input maps (population density, water/elevation). Streets are grown iteratively with global goals (connect population centers) and local constraints (follow terrain, avoid water). The L-system output is a graph of road segments which subdivide land into blocks.

**Relevance to us:** Overkill. We need a single rectangular block, not a network. But the *block-to-lots* step from this paper is directly useful.

**Source:** Parish & Mueller, "Procedural Modeling of Cities", SIGGRAPH 2001 -- [ACM DL](https://dl.acm.org/doi/10.1145/383259.383292)

### 1.2 Tensor Field Roads (Chen et al. 2008)

Input tensor field is converted into a road network by tracing hyperstreamlines aligned to eigenvector fields. Major/minor eigenvectors are perpendicular, producing grid-like layouts naturally. More controllable than L-systems -- designers paint tensor field, roads follow.

**Relevance to us:** Not needed for single-block, but the orthogonal street grid it produces is exactly the context our block sits in.

**Source:** Chen et al., "Interactive Procedural Street Modeling", SIGGRAPH 2008 -- [ACM DL](https://dl.acm.org/doi/10.1145/1360612.1360702)

### 1.3 What We Actually Need: Grid Definition

For a single block, the input is just:
- Block rectangle: `width` x `depth` (along-street x cross-street)
- Street widths: front, back, left, right (can differ -- main street vs alley)
- Optional: which edges face streets vs alleys vs other blocks

```
Street (front)
+------------------------------------------+
|  sidewalk                                 |
|  +--------------------------------------+ |
|  |                                      | |
S  |           CITY BLOCK                 | S
t  |                                      | t
r  |   (lots, buildings, alleys)          | r
e  |                                      | e
e  +--------------------------------------+ e
t  |  sidewalk                                | t
|  +--------------------------------------+ |
+------------------------------------------+
Street (back) or Alley
```

---

## 2. Lot Subdivision Algorithms

Given a block polygon, subdivide into individual building lots. Three main approaches:

### 2.1 Recursive OBB Subdivision (Recommended)

**How it works:**
1. Compute the minimum-area Oriented Bounding Box (OBB) of the polygon
2. Find the longest edge of the OBB
3. Split perpendicular to the longest edge at the midpoint (with optional random deviation)
4. Recurse on each half until lot area or frontage width reaches target range
5. Stop condition: lot area within `[min_area, max_area]` or frontage within `[min_frontage, max_frontage]`

**Splitting detail:** The split line is perpendicular to the longest OBB edge, placed at the midpoint +/- random jitter (e.g., +/-15%). The polygon is clipped by this line using Sutherland-Hodgman or similar.

**Strengths:** Works perfectly for Manhattan-style rectangular blocks. Produces roughly rectangular lots. Simple to implement.

**Weaknesses:** All lots tend to be similar aspect ratios. Less organic than skeleton methods.

**Implementation complexity:** ~200-300 lines C++. Only needs 2D polygon clipping + OBB computation.

**Source:** Martin Devans, "Procedural Generation For Dummies: Lot Subdivision" -- [Blog](https://martindevans.me/game-development/2015/12/27/Procedural-Generation-For-Dummies-Lots/)

### 2.2 Straight Skeleton Subdivision

**How it works:** Compute the straight skeleton of the block polygon (an inward offset skeleton). The skeleton lines connect to the boundary, creating natural lot dividers perpendicular to the street edge. Each lot automatically gets street frontage.

**Strengths:** Every lot guaranteed street access. Works well on irregular (non-rectangular) blocks. Lots naturally taper toward block interior.

**Weaknesses:** O(N^3 log N) complexity. Implementing from scratch is extremely complex (use CGAL or similar). For rectangular blocks, overkill -- OBB gives identical results faster.

**Recommendation:** Skip for V1. Only needed if we support irregular block shapes later.

### 2.3 Offset Subdivision (CityEngine Approach)

**How it works:** Inset the block boundary by a fixed depth to create a strip along each street edge. Subdivide the strip into lots by placing perpendicular dividers at regular intervals based on target lot width.

**Parameters (from CityEngine):**
- `lotWidth` -- target lot frontage width
- `lotDepth` -- how deep into the block (offset distance)
- `irregularity` -- random variation in lot widths (0.0 = uniform, 1.0 = wild)

**Strengths:** Very fast. Intuitive parameters. Street-facing lots guaranteed.

**Weaknesses:** Interior of block is leftover (becomes alley/courtyard/parking). Not recursive.

**Source:** ArcGIS CityEngine Block Parameters -- [Docs](https://doc.arcgis.com/en/cityengine/latest/help/help-layers-block-parameters.htm)

### 2.4 Vanegas et al. (EG 2012) -- Hybrid Approach

Combines recursive subdivision with offset subdivision. User controls style per block. Generates 500K parcels in <3 seconds. Uses a "persistence" scheme so lot assignments survive edits.

**Key insight:** They use offset for street-facing lots and recursive for interior subdivision. This two-pass approach is the most realistic.

**Source:** Vanegas et al., "Procedural Generation of Parcels in Urban Modeling", Eurographics 2012 -- [PDF](https://www.cs.purdue.edu/cgvlab/papers/aliaga/eg2012.pdf)

### 2.5 Recommended Algorithm for Monolith

**Offset + Recursive OBB hybrid:**
1. Define block rectangle with street edges labeled
2. Offset inward from each street edge by `lot_depth` (default 30m/3000cm residential, 20m commercial)
3. Subdivide each offset strip into lots by perpendicular cuts at `lot_width` intervals (+/- jitter)
4. Interior remainder becomes alley/courtyard/parking
5. If the block is small enough that offsets overlap, fall back to pure OBB recursive subdivision

This gives us the best results with minimal complexity.

---

## 3. Building Footprint Generation

Once lots are defined, generate building footprints within each lot (respecting setbacks).

### 3.1 Additive (Growth) Method -- Best for Residential

Start with a seed rectangle (e.g., 60-80% of lot area after setbacks), then add extensions:

```
Phase 1: Seed rectangle          Phase 2: Add wing (L-shape)
+--------+                       +--------+--+
|        |                       |        |  |
|        |                       |        |  |
+--------+                       +--------+  |
                                          +--+

Phase 3: Add porch/garage        Phase 4: Add rear extension (T-shape)
+--------+--+                    +--------+--+
|        |  |                    |        |  |
|        |  |                    |   +----+  |
+---+----+  |                    +---| ext|--+
    | pch|--+                        +----+
    +----+
```

**Algorithm:**
1. Place seed rectangle aligned to front setback line
2. Optionally extend one side (creates L-shape) with probability `p_wing` (0.3 for residential)
3. Optionally add rear extension (creates T-shape) with probability `p_rear` (0.2)
4. Optionally add front porch/overhang
5. Constrain all additions to stay within setback envelope

**Shape probabilities (residential):**
- Rectangle: 50%
- L-shape: 30%
- T-shape: 10%
- U-shape (courtyard): 5%
- Complex (multiple additions): 5%

### 3.2 Subtractive Method -- Best for Commercial

Start with lot polygon minus setbacks (max fill), then subtract:
1. Subtract corner notches for light wells
2. Subtract courtyard if building is large enough (area > 600 sqm)
3. Subtract service entrance recesses

### 3.3 Courtyard Generation

For larger buildings (commercial, institutional):
- Building footprint fills lot minus setbacks
- Interior courtyard cut when `lot_area > courtyard_threshold` (default 600 sqm)
- Courtyard positioned centered or offset toward rear
- Minimum courtyard dimension: 6m x 6m

### 3.4 Existing Monolith Integration

`create_building_shell` already accepts a `footprint` polygon (array of [x,y] points) and extrudes it into a multi-story shell with walls, floors, and optional stairwell cutouts. The city block generator would:
1. Compute lot subdivisions
2. Generate building footprint polygon per lot
3. Call `create_building_shell` with the footprint
4. Optionally compose an entire block in one action

---

## 4. Setback Rules

Setbacks define the minimum distance from lot boundary to building footprint. Based on real US residential zoning:

### 4.1 Typical US Residential Setbacks

| Setback | Typical Range | Default for Monolith | Notes |
|---------|--------------|---------------------|-------|
| Front | 20-35 ft (600-1070 cm) | 750 cm (25 ft) | From street ROW |
| Side (interior) | 5-15 ft (150-450 cm) | 250 cm (8 ft) | Between buildings |
| Side (corner lot) | 10-20 ft (300-600 cm) | 450 cm (15 ft) | Street-facing side |
| Rear | 20-40 ft (600-1200 cm) | 600 cm (20 ft) | Back of lot |

### 4.2 Commercial/Downtown Setbacks

| Setback | Value | Notes |
|---------|-------|-------|
| Front | 0 ft | Buildings at sidewalk edge |
| Side | 0 ft (shared walls) | Party wall construction |
| Rear | 10-20 ft (300-600 cm) | Service alley access |

### 4.3 Implementation

The setback envelope is the lot polygon inset by setback distances per edge. The building footprint must fit entirely within this envelope.

```cpp
// Per-lot setback computation
struct FLotSetbacks
{
    float Front = 750.0f;   // cm, from street edge
    float Rear = 600.0f;    // cm
    float SideLeft = 250.0f;
    float SideRight = 250.0f;
};

// Building envelope = lot polygon inset by setbacks
// For rectangular lots this is trivial:
// envelope_origin = lot_origin + (SideLeft, Front)
// envelope_size = (lot_width - SideLeft - SideRight, lot_depth - Front - Rear)
```

---

## 5. Street Furniture

### 5.1 Item Types and Placement Rules

| Item | Spacing | Placement Zone | Notes |
|------|---------|----------------|-------|
| Street lamp | 30-45m (100-150 ft) | Sidewalk furniture zone, 60cm from curb | 2.5-3x pole height. Residential: 6m poles, ~15-18m spacing |
| Fire hydrant | 90-180m (300-600 ft) | Sidewalk furniture zone, prefer intersections | Min 1 per intersection, both sides if street >25m wide |
| Mailbox | 1 per block face | Near intersection on main streets | USPS regulation blue box |
| Trash can | 30-60m | Sidewalk furniture zone | Near intersections and bus stops |
| Bench | 30-60m | Sidewalk furniture zone | Near transit stops, parks |
| Street sign | At every intersection | Corner, on lamp post or dedicated pole | Street name + optional one-way/stop |
| Fire escape | Per building code | Building facade, side or rear | Every 3-4 buildings in dense areas |
| Utility pole | 30-45m (100-150 ft) | Behind sidewalk or in alley | Power lines, transformers |
| Parking meter | 6m intervals | Curb edge, commercial zones | Not in residential |

### 5.2 Placement Algorithm

```
For each street edge of the block:
  1. Place street signs at both corners
  2. Place fire hydrant at one corner (alternating sides per block)
  3. Walk along street edge at lamp_spacing intervals:
     - Place lamp post in furniture zone (offset from curb)
     - At ~50% of lamp positions, also place trash can
  4. Place benches at 1-2 locations per block face
  5. Place mailbox at one location per block face (main street only)
```

### 5.3 Furniture Zone

The sidewalk cross-section has three zones:
```
|  Building  |  Pedestrian Zone  |  Furniture Zone  |  Curb  |  Street  |
|  frontage  |    (clear path)   |  (lamps, trees)  |        |          |
|            |     150-250cm     |     60-120cm      | 15cm   |          |
```

Street furniture goes in the furniture zone. Pedestrian zone must remain clear (ADA requirement, also important for hospice accessibility).

---

## 6. Sidewalks and Curbs

### 6.1 Dimensions

| Element | Dimension | Notes |
|---------|-----------|-------|
| Sidewalk total width | 150-300 cm (5-10 ft) | Residential: 150cm, Commercial: 250-300cm |
| Curb height | 15-20 cm (6-8 in) | Standard US curb |
| Curb width | 15-30 cm | Top of curb |
| Gutter | 30-45 cm | Between curb and road surface |
| ADA ramp at corners | 120cm wide, 1:12 slope | Required at every intersection |

### 6.2 Geometry Generation

Sidewalk geometry is a series of extruded cross-sections along the street edge:

```
Cross-section (looking along street):

Building face
|
|   Sidewalk (flat, raised)
|   +---------------------------+
|   |                           |  <- sidewalk_height above road
|   |                           +--+
|   |                              | <- curb face (vertical)
----+                              +---- road surface
         sidewalk_width      curb_w
```

**Implementation approach:**
1. For each street edge, define a path (polyline along the street)
2. Define the cross-section polygon (sidewalk surface + curb face + curb top)
3. Use `AppendSimpleSweptPolygon` (already verified in GeometryScript research) to sweep cross-section along path
4. At corners, add a flat triangulated patch connecting two sidewalk runs
5. Add ADA curb cuts at intersection corners (subtract a ramp-shaped volume)

### 6.3 Road Surface

The road itself is a flat plane at Z=0, extending between curbs. For a simple block:
- Road width between curbs: varies (see Scale Reference below)
- Road surface: flat with optional crown (2% slope from center to edges for drainage)
- For horror: cracks, potholes are texture/displacement, not geometry

---

## 7. Alleys

### 7.1 Types

| Alley Type | Width | Location | Purpose |
|------------|-------|----------|---------|
| Rear service alley | 3-6m (10-20 ft) | Behind buildings, bisecting block | Trash pickup, deliveries, utilities |
| Side passage | 1-2m (3-6 ft) | Between buildings | Pedestrian shortcut, fire access |
| Dead-end alley | 3-4m | Off main alley | Service access to specific building |

### 7.2 Layout Within Block

The most common US pattern: a rear alley runs through the center of the block, parallel to the main street. Buildings face the street; their rears face the alley.

```
+------ Main Street (front) ------+
|  Lot  |  Lot  |  Lot  |  Lot   |
|  1    |  2    |  3    |  4     |
+-------+-------+-------+--------+
           REAR ALLEY (4m wide)
+-------+-------+-------+--------+
|  Lot  |  Lot  |  Lot  |  Lot   |
|  5    |  6    |  7    |  8     |
+------ Back Street (rear) -------+
```

### 7.3 Implementation

For the block subdivision:
1. If block depth > `alley_threshold` (default: 50m), split block in half with an alley
2. Alley width: `alley_width` param (default: 400cm / ~13ft)
3. Lots on each side face outward toward their street
4. Side passages between buildings are implicit (from side setbacks)

---

## 8. Coordinate System

### 8.1 Block Coordinate System

```
Y (depth, away from front street)
^
|
|  +----------------------------------+  <- (0, block_depth)
|  |                                  |
|  |          BLOCK INTERIOR          |
|  |                                  |
|  +----------------------------------+  <- (0, 0) = block origin
|
+-----> X (along front street)

Origin: bottom-left corner of block (front-left when facing the street)
X-axis: runs along the front street (left to right)
Y-axis: runs perpendicular to front street (toward rear)
Z-axis: up (height)
```

### 8.2 World Placement

The block's origin maps to a world position + rotation:
```cpp
struct FCityBlockPlacement
{
    FVector WorldOrigin;     // Where block (0,0,0) maps to in world
    float RotationYaw;       // Block orientation (degrees, 0 = X-axis aligned with world X)
};
```

### 8.3 Lot Coordinates (Relative to Block)

Each lot is defined as:
```cpp
struct FBlockLot
{
    int32 LotIndex;
    TArray<FVector2D> Polygon;      // 2D polygon in block-local coords (cm)
    float FrontageWidth;            // Width along street edge
    float Depth;                    // Perpendicular depth from street
    EStreetEdge FacingStreet;       // Which street this lot faces (Front, Back, Left, Right)
    bool bCornerLot;                // Has two street-facing sides
};
```

---

## 9. Scale Reference

### 9.1 Typical US Small Town Dimensions

| Element | Metric | Imperial | Notes |
|---------|--------|----------|-------|
| **City block (residential)** | 80m x 120m | 260 x 400 ft | Common Midwest grid |
| **City block (small town)** | 100m x 100m | 330 x 330 ft | Square blocks common |
| **City block (downtown)** | 80m x 80m | 260 x 260 ft | Denser, smaller |
| **Residential lot width** | 12-18m | 40-60 ft | Frontage along street |
| **Residential lot depth** | 30-45m | 100-150 ft | Perpendicular to street |
| **Residential lot area** | 360-810 sqm | 4000-8700 sqft | Standard suburban |
| **Commercial lot width** | 6-15m | 20-50 ft | Narrower, taller buildings |
| **Commercial lot depth** | 20-30m | 65-100 ft | Shallower than residential |
| **Main street width** | 18-24m | 60-80 ft | Curb to curb, 2-4 lanes + parking |
| **Residential street width** | 10-14m | 33-46 ft | Curb to curb, 2 lanes + parking |
| **Sidewalk width** | 1.5-3.0m | 5-10 ft | Narrower residential, wider commercial |
| **Curb height** | 15-20cm | 6-8 in | Standard |
| **Rear alley width** | 3-6m | 10-20 ft | Service access |
| **Side passage** | 1-2.5m | 3-8 ft | Between buildings (from setbacks) |
| **Street lamp height** | 6-10m | 20-33 ft | Residential: 6m, Main: 8-10m |
| **Building height (residential)** | 6-10m | 20-33 ft | 1-3 stories |
| **Building height (commercial)** | 4-15m | 13-50 ft | 1-4 stories |

### 9.2 Unreal Engine Scale

UE uses centimeters. All dimensions above converted:

| Element | UE Units (cm) | Notes |
|---------|---------------|-------|
| Standard residential block | 10000 x 12000 | 100m x 120m |
| Small town square block | 10000 x 10000 | 100m x 100m |
| Residential lot (typical) | 1500 x 3500 | 15m x 35m |
| Commercial lot (typical) | 1000 x 2500 | 10m x 25m |
| Main street (curb-to-curb) | 2000 | 20m |
| Residential street | 1200 | 12m |
| Sidewalk | 200 | 2m standard |
| Curb height | 15 | Standard |
| Rear alley | 400 | 4m |
| Floor height | 300 | Already used by create_building_shell |

---

## 10. Horror Town Aesthetics

### 10.1 Environmental Decay Markers

What makes a procedurally generated town feel *abandoned* and *wrong*:

**Structural Decay:**
- Collapsed roof sections (random lots get "destroyed" flag -- no building, just debris)
- Boarded-up windows/doors (already have `create_horror_prop` barricade builder)
- Broken walls (`create_horror_prop` broken_wall builder exists)
- Leaning/tilted buildings (slight random rotation on building shells, 1-3 degrees)
- Missing walls/floors (skip random wall segments in building shell)

**Vegetation Overgrowth:**
- Cracked sidewalk with grass/weeds poking through (decal/material, not geometry)
- Overgrown lots (empty lots with tall grass volumes instead of buildings)
- Vines on building facades (decal system)
- Trees growing through collapsed structures

**Street Decay:**
- Potholes and cracks in road surface (decal/displacement)
- Abandoned vehicles blocking streets (scatter props)
- Toppled/damaged street furniture (rotated lamp posts, knocked-over trash cans)
- Missing manhole covers
- Debris in gutters

**Atmosphere:**
- Fog volumes per block (Monolith can already spawn volumes)
- Broken/flickering street lamps (some lamps off, some flickering)
- Darkness -- reduced lamp placement, many non-functional
- Blocked sightlines -- debris, vehicles, fallen trees limit visibility

### 10.2 Decay Parameters

```
decay_level: 0.0 - 1.0  (0 = pristine, 1 = fully abandoned)

At decay 0.0:  All buildings intact, all furniture placed, clean streets
At decay 0.3:  Some boarded windows, 1-2 empty lots, minor debris
At decay 0.5:  Several collapsed structures, overgrown lots, damaged furniture
At decay 0.7:  Most buildings damaged, streets partially blocked, heavy vegetation
At decay 1.0:  Ruins -- most structures collapsed, streets impassable, full overgrowth
```

### 10.3 Horror-Specific Block Modifications

- **The Wrong House:** One lot has a building that's subtly wrong -- different style, slightly too large, impossible geometry (non-Euclidean interior, different from exterior)
- **The Gap:** One lot is conspicuously empty where a building should be, with a foundation visible
- **The Barricade:** One street entrance is blocked with vehicles/debris, funneling player
- **The Dark Corner:** One section of block has no working lights, creating a visibility dead zone
- **Repeating Pattern:** Subtle repetition of building features that gets uncanny (same window count, same mailbox position)

---

## 11. Proposed Action: `create_city_block`

### 11.1 Parameters

```json
{
    "block_width": 10000,
    "block_depth": 10000,
    "streets": {
        "front": { "width": 2000, "type": "main" },
        "back": { "width": 400, "type": "alley" },
        "left": { "width": 1200, "type": "residential" },
        "right": { "width": 1200, "type": "residential" }
    },
    "lot_subdivision": {
        "method": "obb_recursive",
        "min_lot_width": 1000,
        "max_lot_width": 2000,
        "min_lot_depth": 2000,
        "max_lot_depth": 4000,
        "irregularity": 0.15,
        "rear_alley": true,
        "alley_width": 400
    },
    "setbacks": {
        "front": 750,
        "side": 250,
        "rear": 600,
        "corner_side": 450
    },
    "buildings": {
        "style": "residential",
        "floors_min": 1,
        "floors_max": 2,
        "fill_ratio": 0.85,
        "shape_weights": {
            "rectangle": 0.5,
            "l_shape": 0.3,
            "t_shape": 0.1,
            "courtyard": 0.05,
            "complex": 0.05
        }
    },
    "sidewalks": {
        "width": 200,
        "curb_height": 15,
        "generate": true
    },
    "street_furniture": {
        "lamp_spacing": 3500,
        "generate_hydrants": true,
        "generate_signs": true,
        "generate_trash": true,
        "generate_benches": true
    },
    "decay": 0.4,
    "seed": 42,
    "save_path": "/Game/Generated/CityBlocks/Block_01",
    "place_in_scene": true,
    "location": [0, 0, 0]
}
```

### 11.2 Output Structure

Returns a JSON manifest describing everything placed:

```json
{
    "block_bounds": { "min": [0, 0], "max": [10000, 10000] },
    "lots": [
        {
            "index": 0,
            "polygon": [[0, 0], [1500, 0], [1500, 3500], [0, 3500]],
            "facing": "front",
            "corner": false,
            "building": {
                "footprint": [[250, 750], [1250, 750], [1250, 2900], [250, 2900]],
                "floors": 2,
                "shape": "l_shape",
                "asset": "/Game/Generated/CityBlocks/Block_01/Building_00",
                "decay_state": "boarded_windows"
            }
        }
    ],
    "streets": {
        "sidewalk_meshes": [...],
        "road_meshes": [...]
    },
    "furniture": [
        { "type": "lamp_post", "location": [500, -100, 0], "functional": true },
        { "type": "fire_hydrant", "location": [200, -80, 0] }
    ],
    "alleys": [
        { "polygon": [...], "width": 400, "type": "rear" }
    ]
}
```

### 11.3 Implementation Phases

| Phase | Description | Estimate |
|-------|-------------|----------|
| 11.3.1 | Block definition + lot subdivision (OBB recursive) | 12-16h |
| 11.3.2 | Building footprint generation (shapes, setbacks) | 8-12h |
| 11.3.3 | Sidewalk + curb geometry (swept cross-section) | 6-8h |
| 11.3.4 | Street furniture placement | 4-6h |
| 11.3.5 | Alley generation | 3-4h |
| 11.3.6 | Decay system (damage, destruction, overgrowth flags) | 6-8h |
| 11.3.7 | Integration with existing `create_building_shell` | 4-6h |
| **Total** | | **43-60h** |

### 11.4 Dependencies on Existing Actions

- `create_building_shell` -- extrude footprints into buildings (already exists)
- `create_horror_prop` -- barricades, debris, broken walls (already exists)
- `create_parametric_mesh` -- furniture items like benches (already exists)
- Mesh handle pool / save system -- caching, auto-save (already exists)
- GeometryScript `AppendSimpleSweptPolygon` -- sidewalk geometry (verified available)
- GeometryScript `AppendBox` -- road surfaces, simple geometry (verified available)

### 11.5 Algorithm Pseudocode: Lot Subdivision

```
function SubdivideBlock(block_polygon, params):
    // Step 1: If rear alley enabled, split block in half
    if params.rear_alley and block_depth > 2 * params.min_lot_depth + params.alley_width:
        front_strip = block[0 : block_depth/2 - alley_width/2]
        alley = block[block_depth/2 - alley_width/2 : block_depth/2 + alley_width/2]
        rear_strip = block[block_depth/2 + alley_width/2 : block_depth]
        lots += SubdivideStrip(front_strip, facing=FRONT)
        lots += SubdivideStrip(rear_strip, facing=BACK)
    else:
        lots += SubdivideStrip(block, facing=FRONT)
    return lots

function SubdivideStrip(strip_polygon, facing):
    lots = []
    // OBB recursive subdivision
    if StripWidth(strip) <= params.max_lot_width:
        return [strip]  // base case: single lot

    // Compute OBB
    obb = ComputeOBB(strip_polygon)

    // Split perpendicular to longest edge
    split_t = 0.5 + random(-params.irregularity, params.irregularity)
    split_line = PerpendicularBisector(obb.longest_edge, split_t)

    left, right = ClipPolygon(strip_polygon, split_line)

    lots += SubdivideStrip(left, facing)
    lots += SubdivideStrip(right, facing)
    return lots
```

### 11.6 Algorithm Pseudocode: Building Footprint

```
function GenerateFootprint(lot, setbacks, params, rng):
    // Compute buildable envelope
    envelope = InsetLot(lot, setbacks)

    // Choose shape based on weights
    shape = WeightedChoice(params.shape_weights, rng)

    if shape == RECTANGLE:
        // Fill envelope at fill_ratio
        footprint = ScaleToCenter(envelope, sqrt(params.fill_ratio))

    elif shape == L_SHAPE:
        // Main body + wing
        main_w = envelope.width * rng.Range(0.5, 0.7)
        main_d = envelope.depth
        wing_w = envelope.width - main_w
        wing_d = envelope.depth * rng.Range(0.4, 0.6)

        footprint = [
            (0, 0), (envelope.width, 0),
            (envelope.width, wing_d),
            (main_w, wing_d),
            (main_w, main_d),
            (0, main_d)
        ]

    elif shape == T_SHAPE:
        // Cross bar + stem
        bar_w = envelope.width
        bar_d = envelope.depth * rng.Range(0.3, 0.4)
        stem_w = envelope.width * rng.Range(0.3, 0.5)
        stem_d = envelope.depth - bar_d
        stem_offset = (envelope.width - stem_w) / 2

        footprint = [
            (0, 0), (bar_w, 0),
            (bar_w, bar_d),
            (stem_offset + stem_w, bar_d),
            (stem_offset + stem_w, envelope.depth),
            (stem_offset, envelope.depth),
            (stem_offset, bar_d),
            (0, bar_d)
        ]

    // Align footprint to lot's facing street
    footprint = AlignToStreet(footprint, lot.facing_street)
    return footprint
```

---

## 12. Key Academic References

1. **Parish & Mueller (2001)** -- "Procedural Modeling of Cities", SIGGRAPH. Foundational L-system city generation. [ACM](https://dl.acm.org/doi/10.1145/383259.383292)
2. **Chen et al. (2008)** -- "Interactive Procedural Street Modeling", SIGGRAPH. Tensor field road networks. [ACM](https://dl.acm.org/doi/10.1145/1360612.1360702)
3. **Vanegas et al. (2012)** -- "Procedural Generation of Parcels in Urban Modeling", Eurographics. Block-to-lot subdivision. [PDF](https://www.cs.purdue.edu/cgvlab/papers/aliaga/eg2012.pdf)
4. **Martin Devans (2015-2016)** -- "Procedural Generation For Dummies" series. Practical game-dev implementation. [Lots](https://martindevans.me/game-development/2015/12/27/Procedural-Generation-For-Dummies-Lots/) | [Footprints](https://martindevans.me/game-development/2016/05/07/Procedural-Generation-For-Dummies-Footprints/)
5. **CityEngine Block Parameters** -- Industry-standard tool documentation. [Docs](https://doc.arcgis.com/en/cityengine/latest/help/help-layers-block-parameters.htm)

---

## 13. Summary of Recommendations

1. **Lot subdivision:** Use recursive OBB with offset hybrid. Simple, effective, well-suited to rectangular blocks.
2. **Building footprints:** Additive growth method for residential, subtractive for commercial. 5 shape types with weighted random selection.
3. **Setbacks:** Real US zoning values as defaults, fully parameterized for override.
4. **Street furniture:** Spacing-based placement along sidewalk furniture zone. Items as spawned actors (existing meshes or parametric).
5. **Sidewalks:** Swept cross-section polygon along street edges via GeometryScript.
6. **Alleys:** Automatic rear alley when block depth exceeds threshold.
7. **Coordinate system:** Block-local with origin at front-left corner, Y into block, Z up.
8. **Horror decay:** Single `decay` parameter (0-1) controls destruction probability, furniture damage, vegetation, lighting failure.
9. **Existing integration:** Leverage `create_building_shell`, `create_horror_prop`, `create_parametric_mesh`, mesh handle pool, and caching system.
10. **Total estimate:** 43-60 hours for full `create_city_block` action with all subsystems.
