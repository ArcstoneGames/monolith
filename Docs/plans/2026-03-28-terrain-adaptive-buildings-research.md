# Terrain-Adaptive Procedural Buildings Research

**Date:** 2026-03-28
**Context:** Monolith `mesh_query` procedural geometry expansion -- placing procedural buildings on uneven terrain / existing landscapes for FPS survival horror (abandoned hillside town).
**Existing infrastructure:** `create_structure`, `create_building_shell`, `create_parametric_mesh` (stairs, ramp, pillar), `create_terrain_patch`, `query_raycast`, `query_multi_raycast`, `snap_to_floor`, `place_spline`, `create_pipe_network`.

---

## 1. Terrain Detection

### 1.1 Reading Landscape Height at Arbitrary Positions

**Approach A: Line Trace (Recommended)**

The simplest, most reliable method is downward line traces via `World->LineTraceSingleByChannel()`. This works against any geometry -- Landscape actors, static meshes, World Partition tiles -- with zero API coupling.

```cpp
FHitResult Hit;
FVector Start(X, Y, 10000.0f);  // high above expected terrain
FVector End(X, Y, -10000.0f);    // well below
FCollisionQueryParams QP(SCENE_QUERY_STAT(TerrainSample), true);
if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, QP))
{
    float Height = Hit.ImpactPoint.Z;
    FVector Normal = Hit.ImpactNormal;  // surface normal at hit point
}
```

**Monolith already has this** -- `query_raycast` fires arbitrary ray traces and returns `impact_point` + `impact_normal`. The existing `snap_to_floor` action traces downward from actor position.

**Approach B: Direct Landscape Height Query**

`ALandscapeProxy` exposes:
- `GetHeightAtLocation(FVector WorldLocation, TOptional<float>& Height)` -- returns height at XY
- `EditorGetHeightAndNormalAtLocation(FVector, float& OutHeight, FVector& OutNormal)` -- editor-only, returns both

To find the landscape:
```cpp
for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
{
    float H;
    FVector N;
    if (It->EditorGetHeightAndNormalAtLocation(TestPos, H, N))
    {
        // Got height and normal directly from landscape heightfield
    }
}
```

**Tradeoff:** Direct landscape queries are faster (no physics broadphase) but ONLY work with landscape actors. Line traces work with any surface (terrain meshes, World Partition, even stacked static meshes acting as terrain). **Line traces are the recommended approach** for Monolith since we don't know what the terrain is.

**Approach C: Collision Heightfield (LandscapeHeightfieldCollisionComponent)**

Each `ULandscapeComponent` has a `ULandscapeHeightfieldCollisionComponent` with raw height data. Accessing it directly gives sub-component-level resolution but requires understanding the landscape coordinate system, component tiling, and height encoding. Not worth the complexity -- line traces give identical results.

### 1.2 Multi-Point Terrain Sampling

To understand the terrain under a building footprint, sample height at multiple points:

```
Sampling Grid (9-point minimum):
  NW --- N --- NE
  |      |      |
  W  --- C --- E
  |      |      |
  SW --- S --- SE

Where:
  C  = footprint center
  N/S/E/W = edge midpoints
  NW/NE/SW/SE = corners
```

From these samples, compute:
- **Average height:** `Avg(all samples)` -- determines the "design grade"
- **Min/max height:** Determines the total height differential across the footprint
- **Slope direction:** `FVector SlopeDir = (MaxHeightCorner - MinHeightCorner).GetSafeNormal2D()`
- **Slope steepness:** `float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(AverageNormal, FVector::UpVector)))`
- **Terrain roughness:** Standard deviation of height samples -- high roughness suggests broken/uneven ground

**Proposed MCP Action: `sample_terrain_grid`**

```json
{
    "action": "sample_terrain_grid",
    "params": {
        "center": [1000, 2000, 5000],
        "size": [1500, 2000],
        "grid": [5, 5],
        "trace_channel": "Visibility"
    }
}
```

Returns:
```json
{
    "heights": [[z00, z01, ...], [z10, z11, ...], ...],
    "normals": [[n00, n01, ...], ...],
    "min_height": 120.0,
    "max_height": 285.0,
    "height_differential": 165.0,
    "average_height": 203.0,
    "average_normal": [0.02, -0.15, 0.99],
    "slope_degrees": 8.6,
    "slope_direction": [0.13, -0.99, 0.0],
    "roughness": 42.3,
    "all_hit": true
}
```

**Implementation:** ~150 lines C++. Loop over grid points, fire downward traces, accumulate stats. Uses existing `World->LineTraceSingleByChannel` infrastructure.

### 1.3 Terrain Normal and Slope Detection

From multi-point sampling, two methods to determine slope:

**Method 1: Average Normals** (fast, approximate)
Average all hit normals from the sampling grid. The angle between average normal and `FVector::UpVector` gives slope steepness. The projected XY direction of the normal gives slope direction (downhill).

**Method 2: Least-Squares Plane Fit** (precise)
Fit a plane to all sample points using least-squares:
```cpp
// Simplified: for N points (xi, yi, zi), solve for plane z = ax + by + c
// Using normal equations (3x3 system)
FVector PlaneNormal = FVector(-a, -b, 1.0f).GetSafeNormal();
float SlopeDeg = FMath::RadiansToDegrees(FMath::Acos(PlaneNormal.Z));
```
This gives the best-fit slope even with noisy terrain. The residuals measure terrain roughness.

**Method 3: Per-Edge Slope** (for foundation decisions)
Sample along each building edge independently. A building on a hillside might have one edge level and the opposite edge 2m higher. Per-edge slopes inform which foundation strategy to use:
- North edge: +0cm (level with terrain)
- South edge: +200cm (elevated above terrain)
- Result: Building is on a north-facing slope, needs pier/stilt support on south side

### 1.4 Existing Monolith Actions for Terrain Detection

| Action | What it does | Terrain use |
|--------|-------------|-------------|
| `query_raycast` | Single ray trace, returns hit point + normal | Sample one terrain point |
| `query_multi_raycast` | Multi-hit ray trace | Detect stacked geometry |
| `snap_to_floor` | Traces down, positions actor on surface | Place building at terrain height |
| `query_radial_sweep` | Radial rays from origin | Survey area around a building site |
| `get_spatial_relationships` | Actor-to-neighbor relationships | Detect terrain actors near building |

**What's missing:** A batched terrain sampling action that fires a grid of downward traces and returns structured height/normal data. `query_raycast` works but requires N separate MCP calls for N sample points. A dedicated `sample_terrain_grid` action would be dramatically more efficient.

---

## 2. Foundation Strategies

### 2.1 Cut and Fill (Average Height)

The simplest strategy. Building floor sits at the average terrain height across the footprint.

```
Terrain profile along building width:
         ___---"""
    ___--"
---"
|<- FILL ->|<--------- CUT ---------->|
           |  Building floor level     |
           ========================== Z_avg
```

- **Cut side:** Building is partially buried into the hillside. Terrain is above floor level. Requires retaining wall or the building exterior wall IS the retaining wall.
- **Fill side:** Building overhangs the terrain. Gap between building bottom and terrain requires pier supports or the gap is left as-is (building cantilevered / on stilts).
- **Z_avg calculation:** `sum(heights) / count` from terrain sampling grid.
- **When to use:** Gentle slopes (<10 degrees). Height differential < 1 floor height.

### 2.2 Stepped Foundations

Building follows terrain in discrete steps. Each room or room group sits at the terrain height at its center.

```
Side view (building following slope):
    +------+
    |Room 3| +------+
    |  Z=6 | |Room 2| +------+
    +------+ |  Z=3 | |Room 1|
             +------+ |  Z=0 |
                       +------+
```

**Algorithm:**
1. Determine room layout (from `create_structure` calls or `create_building_shell` footprint)
2. For each room, sample terrain height at room center
3. Quantize heights to step increments (e.g., round to nearest 50cm or 100cm)
4. Generate each room at its quantized height
5. Insert connectors (stairs/ramps) between adjacent rooms at different heights

**Step quantization values:**
- 50cm step = ~3 standard stairs (comfortable, residential)
- 100cm step = ~6 stairs (split-level feel)
- 150cm step = ~8 stairs (half-story)
- 300cm step = full floor height difference

**When to use:** Moderate slopes (10-25 degrees). Height differential = 1-2 floor heights.

### 2.3 Retaining Walls

Where a building is cut into a hillside, the exposed terrain face needs a retaining wall.

```
Cross section:
        /  Terrain continues up
       /
      /  Retaining wall
     |====================|
     |                    |
     |   Building         |
     |   interior         |
     |====================|
     ~~~~~~~~~~~~~~~~~~~~~ Ground level (lower side)
```

**Geometry:** A retaining wall is a tapered slab -- thicker at the base, thinner at the top:
- Base thickness: 30-50cm
- Top thickness: 20-30cm
- Height: `terrain_height_at_wall - building_floor_height`

**GeometryScript approach:** `AppendSimpleExtrudePolygon` with a trapezoidal cross-section:
```cpp
TArray<FVector2D> RetainingProfile;
RetainingProfile.Add(FVector2D(0, 0));                    // bottom-front
RetainingProfile.Add(FVector2D(BaseThickness, 0));         // bottom-back
RetainingProfile.Add(FVector2D(TopThickness, WallHeight)); // top-back
RetainingProfile.Add(FVector2D(0, WallHeight));            // top-front
// Extrude along the building edge length
```

Or use `AppendSimpleSweptPolygon` to sweep the trapezoidal profile along the retaining wall path (supports curves and corners).

### 2.4 Piers / Stilts

For steep terrain, generate column supports under the building where it overhangs the slope.

```
Side view:
     +===================+  Building floor
     |    |    |    |    |
     |    |    |    |    |  Pier columns
     |    |    |    /
     |    |    |   /  Terrain
     |    |    |  /
     |    |   / /
     |    |  /
     |    | /
     |    /
=========  Ground
```

**Pier placement algorithm:**
1. Identify regions where building floor is above terrain
2. Create a grid of potential pier locations (spacing: 300-500cm)
3. For each pier location, measure gap: `building_floor_Z - terrain_Z`
4. If gap > threshold (e.g., 30cm), place a pier
5. Pier height = gap. Pier radius/width = structural size (20-40cm diameter for residential)

**GeometryScript:** Use existing `BuildPillar()` (already in `create_parametric_mesh` type="pillar") for each pier. Circular or square cross-section. For horror aesthetic, vary pier sizes slightly, add slight random lean/offset.

**Pier dimensions (realistic):**
- Residential: 20-30cm diameter, round or 20x20cm square
- Commercial: 40-60cm diameter or 40x40cm square
- Industrial/horror: 30-50cm, concrete texture, possibly cracked
- Spacing: 300-500cm (residential), 400-600cm (commercial)

### 2.5 Basement Exposure (Walkout Basement)

On a slope, one side of a basement is fully underground while the opposite side is exposed at grade. Classic horror setting -- the "daylight basement."

```
Cross section through slope:
            ___-----""" Terrain
       ___--"
  ___--"
  |========================|  Main floor
  |   Basement             |
  |   (underground side)   |=====  Exposed/walkout side
  |________________________|
  ////////////////////////// Ground (low side)
```

**Implementation:**
1. Detect slope direction from terrain sampling
2. Building floor = terrain height at the UPHILL side
3. Basement extends one floor height down (300cm default)
4. On the downhill side, basement wall is fully exposed -- has windows/doors
5. On the uphill side, basement wall is buried -- retaining wall against terrain

**Horror potential:** Walkout basements are deeply unsettling:
- Dark interior visible through half-buried windows
- Water stains / mold on exposed concrete
- Broken windows suggesting something got in (or out)
- The transition from buried to exposed creates an inherently liminal space

---

## 3. Connecting Different Heights

### 3.1 Automatic Stair Placement

When two adjacent rooms are at different Z heights, stairs bridge the gap.

**Stair calculation:**
```
Standard residential stair:
  Riser height: 18cm (7 inches) -- comfortable residential
  Tread depth:  28cm (11 inches) -- ADA minimum is 27.9cm
  Width: 90cm minimum (ADA requires 91.4cm / 36 inches)

Step count = ceil(height_difference / riser_height)
Stair run  = step_count * tread_depth
```

**Monolith already has:**
- `create_parametric_mesh` type="stairs" -- uses `AppendLinearStairs()` with params: `stair_count`, `stair_depth`, width, height
- `BuildStairs()` in MonolithMeshProceduralActions.cpp calls `UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs()`

**Placement algorithm:**
1. Detect height difference between adjacent rooms: `dZ = room_B.floor_Z - room_A.floor_Z`
2. Calculate step count: `N = ceil(abs(dZ) / 18.0)`
3. Calculate stair run: `run = N * 28.0` cm
4. Position stairs: centered on the doorway between rooms, oriented toward the higher room
5. If `run > available_space`, use switchback stairs (two flights with a landing)

**Stair position formula:**
```cpp
float StairWidth = 90.0f;
float StepHeight = 18.0f;
float StepDepth = 28.0f;
int32 StepCount = FMath::CeilToInt(FMath::Abs(DeltaZ) / StepHeight);
float StairRun = StepCount * StepDepth;

// Place at doorway between rooms, bottom of stairs at lower room floor
FVector StairLoc = DoorwayCenter;
StairLoc.Z = FMath::Min(RoomA_Z, RoomB_Z);
// Rotate to face from lower room toward higher room
FRotator StairRot = (HigherRoomCenter - LowerRoomCenter).GetSafeNormal2D().Rotation();
```

### 3.2 Ramp Generation

For small height differences or accessibility, use ramps instead of stairs.

**ADA requirements (critical for hospice accessibility):**
- Maximum slope: 1:12 (8.33%) -- 1cm rise per 12cm run
- Maximum rise per run: 76cm (30 inches) before requiring a landing
- Landing length: 150cm (60 inches) minimum
- Ramp width: 91.4cm (36 inches) minimum clear width
- Handrails required on both sides for rises > 15cm (6 inches)
- Handrail height: 86-96cm (34-38 inches)
- Edge protection: curb, wall, or railing to prevent wheelchairs from rolling off

**Monolith already has:** `create_parametric_mesh` type="ramp" -- `BuildRamp()` creates a box with diagonal plane cut.

**When to use ramp vs stairs:**
| Height difference | Connector | Ramp length at 1:12 |
|-------------------|-----------|---------------------|
| < 15cm | Ramp (no handrails needed) | < 180cm |
| 15-50cm | Ramp with handrails | 180-600cm |
| 50-76cm | Ramp with landing | 600-912cm + 150cm landing |
| > 76cm | Stairs (ramp too long) OR switchback ramp | N/A |

**Ramp generation formula:**
```cpp
float RampSlope = 1.0f / 12.0f; // ADA maximum
float RampRun = HeightDiff / RampSlope;
float RampWidth = 100.0f; // slightly wider than ADA minimum

if (HeightDiff > 76.0f)
{
    // Need switchback: two ramps with a landing
    int32 Segments = FMath::CeilToInt(HeightDiff / 76.0f);
    float SegmentRise = HeightDiff / Segments;
    float SegmentRun = SegmentRise / RampSlope;
    // Place segments in zigzag with 150cm landings between
}
```

### 3.3 Split-Level Interiors

Rooms at slightly different heights within the same building (3-4 steps up/down). Classic 1970s residential architecture -- deeply unsettling in horror contexts because sight lines are broken.

```
Plan view (split-level):
  +------------------+
  |  Living room     |  Z = 0
  |  (lower)    [3 steps up]
  +--------+---------+
           |  Kitchen    |  Z = 54cm (3 x 18cm)
           |  (upper)    |
           +-------------+
```

**Implementation:**
- Each room group has a `base_z` offset
- Height differences are small: 1-4 steps (18-72cm)
- Steps are placed at doorways between height zones
- Partial walls / half-walls at level changes create interesting sight lines

**Horror split-level patterns:**
- **The Sunken Living Room:** 3 steps down into a large open room. Something lurking below eye level.
- **The Raised Bedroom Wing:** Bedrooms 3 steps up from main level. Gives attacker height advantage.
- **The Half-Landing:** Stairs that pause at a small landing with a window, then turn. Classic scare point.

### 3.4 Exterior Entry Stairs

When building floor is above street level, generate front steps from street to door.

**Typical dimensions:**
- 3-8 steps is common for residential (54-144cm rise)
- Width: 120-180cm for front entry (wider than interior stairs)
- Add landing at top (porch) -- minimum 90cm deep
- Handrails if > 3 steps

**Generation:**
1. Determine building floor height relative to adjacent sidewalk/street
2. Calculate step count: `N = ceil(height_diff / 18cm)`
3. Generate stairs extending outward from the door opening
4. Add a small landing/porch at the top

---

## 4. Balconies and Exterior Features

### 4.1 Balcony Generation

A platform extending from an upper floor wall.

**Components:**
1. **Floor slab:** Rectangle extending 100-200cm from wall face, full width of the balcony opening (200-400cm typical)
2. **Railing:** Posts at corners and intermediate positions + top rail + optional balusters
3. **Support brackets:** Triangular brackets under the slab where it meets the wall (decorative/structural)
4. **Door opening:** The wall behind the balcony needs a door or French doors

**GeometryScript construction:**
```
Balcony floor:  AppendBox (width x depth x slab_thickness), positioned offset from wall
Railing posts:  BuildPillar() at regular intervals (120-150cm spacing)
Top rail:       AppendSimpleSweptPolygon with small square profile along post tops
Balusters:      Series of thin BuildPillar() between posts (10cm spacing)
Brackets:       AppendSimpleExtrudePolygon with right-triangle profile
```

**Dimensions:**
- Slab depth: 100-200cm (balcony depth from wall)
- Slab thickness: 15-20cm
- Railing height: 100-110cm (building code minimum 107cm / 42 inches)
- Post size: 5x5cm to 10x10cm square
- Top rail: 5x5cm to 8x8cm
- Baluster spacing: 10cm (max 10cm gap per building code -- prevents child heads fitting through)
- Bracket depth: 30-50cm triangle

### 4.2 Porches

Ground-level covered platforms at entries.

**Components:**
1. **Platform/deck:** Raised floor slab, typically 15-30cm above grade
2. **Columns:** Support posts for the roof overhang (4-6 typically)
3. **Roof overhang:** A simple slab or lean-to extending from the building wall
4. **Optional railing:** Around the porch edges
5. **Steps:** If porch is above grade

**Horror porch patterns:**
- **Wrap-around porch:** Classic Victorian horror. Extends around 2-3 sides. Creaking boards, swinging chair.
- **Collapsed porch:** Partially destroyed -- broken columns, sagging roof. Communicates abandonment.
- **Screened porch:** Wire mesh walls. Something moving behind the screen.
- **The Rocking Chair:** Single object on an otherwise empty porch. Was it rocking a moment ago?

### 4.3 Fire Escapes

Zigzag exterior stairs on building walls. Classic urban horror element.

**Structure:**
```
Side view:
  [Landing 4]---|
                |---[Flight 3 going right]
                                          |---[Landing 3]
                [Flight 2 going left]---|
  [Landing 2]---|
                |---[Flight 1 going right]
                                          |---[Landing 1]
```

**Components per level:**
1. **Landing:** Platform at each floor level (120x250cm minimum)
2. **Flight:** Stairs connecting landings (switchback, so each flight spans one floor height)
3. **Railing:** On all exposed edges
4. **Mounting brackets:** Attaching to building wall
5. **Ladder:** Drop-down ladder from lowest landing to ground (retractable, 250cm)

**Fire escape dimensions:**
- Landing: 120cm wide x 250cm deep (along building wall)
- Flight width: 60-90cm (narrower than interior stairs)
- Rise per flight: one floor height (300cm)
- Step count per flight: ~17 steps at 18cm rise
- Total horizontal extent from wall: ~600cm (landing + stair run)

**Generation algorithm:**
1. Determine number of floors
2. For each floor, place a landing against the building wall
3. Connect landings with alternating-direction stair flights
4. Each flight is offset from the wall by the landing depth
5. Add railing around all platforms and stair edges
6. Add a drop-down ladder from the lowest landing

**GeometryScript approach:**
- Landings: `AppendBox` for the platform
- Stair flights: `AppendLinearStairs` with appropriate width/rise
- Railing posts: `BuildPillar` at intervals
- Top rail: `AppendSimpleSweptPolygon` with square profile
- Full fire escape = one combined mesh via mesh appends (no booleans needed)

### 4.4 Loading Docks

Raised platforms at building rear. Commercial/industrial buildings.

**Components:**
1. **Dock platform:** 120cm above grade (standard truck bed height), 300-400cm deep
2. **Dock bumpers:** Rubber/concrete bumpers at the edge
3. **Ramp:** Vehicle access ramp on one end (slope 1:8 to 1:10)
4. **Steps:** Personnel stairs on one end
5. **Overhead cover:** Optional canopy

---

## 5. GeometryScript API for Terrain-Adaptive Features

### 5.1 Railing Generation

**Approach: Composite mesh (posts + rail + balusters)**

```cpp
// Railing along a path (array of FVector points)
void BuildRailing(UDynamicMesh* Mesh, const TArray<FVector>& Path,
                  float RailHeight, float PostSpacing, float PostSize, float RailSize)
{
    // 1. Posts at each path vertex and intermediate positions
    for (float Dist = 0; Dist < TotalLength; Dist += PostSpacing)
    {
        FVector Pos = SamplePathAtDistance(Path, Dist);
        // BuildPillar at Pos, height = RailHeight
    }

    // 2. Top rail: sweep a small square profile along the path at RailHeight
    TArray<FVector2D> RailProfile;
    RailProfile.Add(FVector2D(-RailSize/2, -RailSize/2));
    RailProfile.Add(FVector2D( RailSize/2, -RailSize/2));
    RailProfile.Add(FVector2D( RailSize/2,  RailSize/2));
    RailProfile.Add(FVector2D(-RailSize/2,  RailSize/2));

    TArray<FVector> RailPath; // offset Path by Z = RailHeight
    AppendSimpleSweptPolygon(Mesh, ..., RailProfile, RailPath, ...);

    // 3. Balusters: thin pillars between posts at ~10cm spacing
    for (float Dist = 0; Dist < TotalLength; Dist += 10.0f)
    {
        FVector Pos = SamplePathAtDistance(Path, Dist);
        // Thin pillar: 2cm x 2cm x (RailHeight - 5cm)
    }
}
```

**Alternative:** Use `create_pipe_network` for the top rail (already supports swept profiles along paths), and `create_parametric_mesh` type="pillar" for posts. This reuses existing actions.

### 5.2 Stair Mesh with Terrain Placement

Already have `BuildStairs()` calling `AppendLinearStairs()`. Need to add:
- Automatic step count from height difference
- Correct positioning at doorway between rooms
- Optional railing on sides
- Switchback stair support (two flights + landing)

### 5.3 Retaining Wall Mesh

A wedge/trapezoidal shape filling between terrain slope and building floor.

**Two approaches:**

**A. Swept profile along wall edge:**
```cpp
// Trapezoidal cross-section
TArray<FVector2D> Profile;
Profile.Add(FVector2D(0, 0));              // bottom-front (at terrain level)
Profile.Add(FVector2D(BaseThick, 0));       // bottom-back
Profile.Add(FVector2D(TopThick, Height));   // top-back (at building floor)
Profile.Add(FVector2D(0, Height));          // top-front

// Sweep along the building edge
AppendSimpleSweptPolygon(Mesh, ..., Profile, WallEdgePath, ...);
```

**B. Per-segment varying height (terrain-conforming):**
For walls on uneven terrain, the retaining wall height varies along its length. Sample terrain at regular intervals along the wall edge and create a custom mesh:

```cpp
// For each segment of wall, compute local height
for (int i = 0; i < Segments; i++)
{
    float TerrainZ = SampleTerrainAt(WallEdge[i]);
    float WallHeight = BuildingFloorZ - TerrainZ;
    // Extrude a trapezoidal section of height WallHeight
}
```

This creates a retaining wall whose top edge is level (building floor) but whose bottom edge follows the terrain.

### 5.4 Pier / Stilt Column Generation

Reuse `BuildPillar()` with variable height per pier location:
```cpp
// For each pier location
float TerrainZ = SampleTerrainAt(PierXY);
float PierHeight = BuildingFloorZ - TerrainZ;
if (PierHeight > MinPierThreshold)  // e.g., 30cm
{
    BuildPillar(PierMesh, PierRadius, PierHeight, PierSides, bRound, Error);
    // Position at (PierXY.X, PierXY.Y, TerrainZ)
}
```

---

## 6. Horror-Specific Terrain Considerations

### 6.1 Abandoned Hillside Town (Silent Hill Pattern)

Silent Hill's town design uses terrain as a psychological tool:
- **Blocked roads forcing descent** -- the player is funneled downhill into increasingly dark areas
- **Buildings at odd angles** from subsidence -- foundations have shifted, doors don't close properly
- **Collapsed terrain** -- sinkholes, cave-ins where the ground gave way
- **Fog as terrain occlusion** -- can't see the slope, disorienting

**Subsidence effects (procedural):**
- Random slight rotation of building placement (1-3 degrees off-level)
- Cracked foundations -- boolean subtract thin wedges from foundation geometry
- Tilted floors -- apply a small rotation to floor slab generation
- Exposed foundation -- lower terrain slightly below building base on one side

### 6.2 Horror Foundation Details

- **Cracked retaining walls:** Boolean-subtract crack patterns from retaining wall mesh
- **Exposed rebar:** Thin cylinders poking out of broken concrete (use `create_pipe_network`)
- **Water damage lines:** Material work, not geometry -- horizontal staining at watermark height
- **Root intrusion:** Organic shapes pushing through foundation cracks
- **The Sinkhole:** Circular depression in terrain near a building. Building edge overhangs the void.

### 6.3 Accessibility on Slopes (Hospice Critical)

For hospice patients, terrain challenges are amplified:
- **Maximum path slope: 5% preferred, 8.33% (1:12) absolute maximum** per ADA
- **Resting areas every 9m** on sloped paths (landing or bench)
- **Handrails on both sides** of any ramp
- **Visual contrast** at elevation changes (different floor color/texture at steps)
- **Audio cues** for elevation changes (footstep sound changes on stairs/ramps)
- **Auto-aim / aim assist** adjustments on slopes (looking up/down is harder)
- **Avoid stairs-only routes** -- always provide ramp alternative. Monolith's `analyze_path_complexity` already flags stairs-only paths.

**Hospice mode parameter:** When `accessibility: "hospice"`, the generator should:
1. Prefer ramps over stairs everywhere
2. Add handrails to all ramps and stairs
3. Ensure ramp slopes never exceed 1:12
4. Place resting landings every 9m of ramp run
5. Increase visual contrast at all elevation changes
6. Flag any path that is stairs-only with no ramp alternative

---

## 7. Proposed New MCP Actions

### 7.1 Terrain Analysis Actions

| Action | Purpose | Effort |
|--------|---------|--------|
| `sample_terrain_grid` | Fire NxM downward traces, return height/normal grid, slope stats | ~4h |
| `analyze_building_site` | Given footprint + location, return foundation recommendation | ~6h |

### 7.2 Foundation Actions

| Action | Purpose | Effort |
|--------|---------|--------|
| `create_foundation` | Generate foundation geometry (cut-fill, stepped, piers, retaining walls) based on site analysis | ~16h |
| `create_retaining_wall` | Terrain-conforming retaining wall along a path | ~6h |
| `create_pier_supports` | Grid of variable-height piers under a building footprint | ~4h |

### 7.3 Connector Actions

| Action | Purpose | Effort |
|--------|---------|--------|
| `create_stair_connector` | Auto-calculate and place stairs between two heights | ~4h |
| `create_ramp_connector` | ADA-compliant ramp with handrails between two heights | ~6h |
| `create_railing` | Railing along a path (posts + top rail + balusters) | ~6h |

### 7.4 Exterior Feature Actions

| Action | Purpose | Effort |
|--------|---------|--------|
| `create_balcony` | Balcony slab + railing + brackets attached to a wall face | ~6h |
| `create_porch` | Covered platform with columns and optional railing | ~8h |
| `create_fire_escape` | Multi-story zigzag exterior stairs with landings | ~10h |
| `create_loading_dock` | Raised platform with ramp/stairs/bumpers | ~6h |

### 7.5 Composite Actions

| Action | Purpose | Effort |
|--------|---------|--------|
| `place_building_on_terrain` | Full pipeline: sample terrain -> choose foundation -> generate building + foundation + connectors | ~20h |

---

## 8. Implementation Architecture

### 8.1 Terrain Sampler Class

```cpp
struct FTerrainSample
{
    FVector Location;    // world XY + sampled Z
    FVector Normal;      // surface normal at this point
    bool bHit;           // whether trace found terrain
    float Height;        // shortcut for Location.Z
};

struct FTerrainGrid
{
    TArray<FTerrainSample> Samples;  // row-major, GridX * GridY
    int32 GridX, GridY;
    FVector2D GridOrigin;  // world XY of (0,0) cell
    FVector2D CellSize;    // size of each cell

    // Computed stats
    float MinHeight, MaxHeight, AvgHeight;
    FVector AvgNormal;
    float SlopeDegrees;
    FVector2D SlopeDirection;  // 2D downhill direction
    float Roughness;           // std dev of heights

    // Methods
    float GetHeightAt(float WorldX, float WorldY) const;  // bilinear interpolation
    FVector GetNormalAt(float WorldX, float WorldY) const;
    float GetHeightAlongEdge(const FVector2D& Start, const FVector2D& End, int32 NumSamples) const;
};

class FTerrainSampler
{
public:
    static FTerrainGrid SampleGrid(UWorld* World, FVector2D Center, FVector2D Size,
                                   int32 GridX, int32 GridY, ECollisionChannel Channel);
    static float SampleSingleHeight(UWorld* World, float X, float Y, ECollisionChannel Channel);
};
```

### 8.2 Foundation Strategy Selector

```cpp
enum class EFoundationStrategy
{
    Flat,           // Terrain is level enough -- no adaptation needed
    CutAndFill,     // Gentle slope -- average height, fill gaps
    Stepped,        // Moderate slope -- rooms at different heights
    Piers,          // Steep slope -- building on stilts
    WalkoutBasement // Slope allows exposed basement
};

EFoundationStrategy SelectFoundationStrategy(const FTerrainGrid& Terrain, float MaxFloorHeight)
{
    float HeightDiff = Terrain.MaxHeight - Terrain.MinHeight;
    float Slope = Terrain.SlopeDegrees;

    if (HeightDiff < 30.0f)    return EFoundationStrategy::Flat;
    if (Slope < 10.0f)         return EFoundationStrategy::CutAndFill;
    if (Slope < 25.0f)         return EFoundationStrategy::Stepped;
    if (HeightDiff > MaxFloorHeight * 0.7f) return EFoundationStrategy::WalkoutBasement;
    return EFoundationStrategy::Piers;
}
```

### 8.3 Integration with Existing Procedural Actions

The terrain-adaptive system layers ON TOP of existing actions:

```
Agent workflow:
1. sample_terrain_grid  -> understand the site
2. analyze_building_site -> get foundation recommendation
3. create_building_shell -> generate base building geometry
4. create_foundation     -> generate foundation (piers, retaining walls, etc.)
5. create_stair_connector (x N) -> connect rooms at different heights
6. create_railing (x N) -> add railings to stairs, balconies
7. create_balcony / create_fire_escape -> exterior features
```

Each step is an independent MCP call. The agent orchestrates. This keeps individual actions simple and composable.

---

## 9. Real-World Architectural References

### 9.1 Hillside Building Techniques

| Technique | Description | Common slope | Horror potential |
|-----------|-------------|-------------|-----------------|
| Cut and fill | Excavate high side, fill low side | < 10% | Hidden basement rooms |
| Step foundation | Concrete steps following slope | 10-25% | Uneven floors, disorienting |
| Pier and beam | Elevated on concrete/steel piers | > 15% | Exposed underside, things hiding under |
| Walkout/daylight basement | One wall fully exposed | 10-30% | Half-buried windows, eerie transition |
| Retaining wall | Holds back hillside soil/rock | Any | Cracking, seeping water |
| Cantilevered | Building overhangs slope | > 25% | Vertigo, sense of instability |

**Sources:**
- Stepped foundations: each step height <= foundation thickness, minimum 300mm overlap per UK building regs
- Pier foundations: typical 25-30cm diameter, 150-370cm spacing, 12" depth minimum
- Cut and fill: retaining wall needed when fill > 120cm

### 9.2 Standard Dimensions Reference

| Element | Dimension | Notes |
|---------|-----------|-------|
| Stair riser | 18cm (7") | Residential comfortable. Max 19.6cm per IBC |
| Stair tread | 28cm (11") | ADA min 27.9cm |
| Stair width | 91.4cm (36") | ADA minimum. 100-120cm common residential |
| Ramp slope | 1:12 max (8.33%) | ADA requirement. 1:20 (5%) preferred |
| Ramp width | 91.4cm (36") | ADA minimum clear width |
| Ramp landing | 150cm (60") | At top, bottom, and every 76cm of rise |
| Railing height | 86-96cm (34-38") | ADA handrail range |
| Baluster spacing | max 10cm (4") | Building code -- prevent child heads |
| Balcony railing | 107cm (42") min | Residential building code |
| Pier diameter | 25-30cm | Residential. 40-60cm commercial |
| Pier spacing | 150-370cm | Depends on load and beam span |
| Floor height | 270-310cm | 300cm default for Monolith |
| Foundation wall | 20-30cm | Residential minimum |
| Retaining wall base | 30-50cm | Thicker at base for stability |

### 9.3 Horror Architecture Patterns

**The Subsiding House:** Building on unstable terrain (clay, mine subsidence). Doors don't close. Floors slope. Cracks in walls. Generate by applying small random rotations to the building placement and adding crack booleans.

**The Cliffside House:** Building cantilevered over a drop. Rear of house is on solid ground. Front extends over nothing. Looking down through floor gaps reveals a long fall. Generate with piers on the cliff side, gaps in floor slab.

**The Terraced Town:** Multiple levels of buildings stepping down a hillside. Streets at different levels. Stairs and ramps connecting them. Narrow alleys between buildings at different heights. Classic Innsmouth / Dunwich vibes.

**The Flooded Basement:** Low side of a walkout basement is below the water table. Water seeps in. Dark water of unknown depth. Generate with the walkout basement pattern + a water plane at a height between floor and terrain grade.

---

## 10. Implementation Roadmap

### Phase 1: Terrain Sampling (~8h)
- `FTerrainSampler` class with `SampleGrid()` and `SampleSingleHeight()`
- `sample_terrain_grid` MCP action
- `analyze_building_site` MCP action (calls sampler, returns recommendation)

### Phase 2: Foundation Generation (~20h)
- `FFoundationBuilder` class
- `create_foundation` action (dispatches to strategy)
- `create_retaining_wall` action
- `create_pier_supports` action
- Integration: `create_building_shell` gains optional `terrain_adapt: true` param

### Phase 3: Connectors (~16h)
- `create_stair_connector` action (auto-calculate from height diff)
- `create_ramp_connector` action (ADA-compliant)
- `create_railing` action (generic path-following railing)

### Phase 4: Exterior Features (~30h)
- `create_balcony` action
- `create_porch` action
- `create_fire_escape` action
- `create_loading_dock` action

### Phase 5: Composite Pipeline (~20h)
- `place_building_on_terrain` orchestration action
- Automatic foundation selection
- Automatic connector placement between rooms
- Integration with spatial registry (from `reference_spatial_registry_research.md`)
- Horror decay parameter application

**Total estimate: ~94h across all phases**

Priority order: Phase 1 > Phase 2 > Phase 3 > Phase 5 > Phase 4

Phase 1 is immediately useful even without the rest -- agents can use `sample_terrain_grid` to manually decide building placement. Phases 2-3 are the core value. Phase 4 is nice-to-have. Phase 5 ties it all together.

---

## 11. Key References

### Academic / Technical
- Parish & Mueller, "Procedural Modeling of Cities", SIGGRAPH 2001
- Chen et al., "Interactive Procedural Street Modeling", SIGGRAPH 2008
- Vanegas et al., "Inverse Design of Urban Procedural Models", Eurographics 2012
- Martin Devans, [Procedural Generation for Dummies: Building Footprints](https://martindevans.me/game-development/2016/05/07/Procedural-Generation-For-Dummies-Footprints/)
- bendemott, [UE5-Procedural-Building (GeometryScript C++)](https://github.com/bendemott/UE5-Procedural-Building)

### UE5 API
- [ALandscapeProxy API (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Landscape/ALandscapeProxy)
- [AppendLinearStairs (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Primitives/AppendLinearStairs)
- [AppendSimpleExtrudePolygon (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Primitives/AppendSimpleExtrudePolygon)
- [GeometryScript Reference (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-reference-in-unreal-engine)
- [Creating Landscapes (5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-landscapes-in-unreal-engine)

### Building Codes / Accessibility
- [U.S. Access Board: Ramps and Curb Ramps (ADA Chapter 4)](https://www.access-board.gov/ada/guides/chapter-4-ramps-and-curb-ramps/)
- [ADA Ramp Requirements & Specs](https://www.simplifiedbuilding.com/projects/how-ada-wants-ramps-built)
- [Stepped Foundation Design](https://www.designingbuildings.co.uk/wiki/Stepped_foundation)
- [Pier Foundation Field Guide (JLC)](https://www.jlconline.com/how-to/foundations/pier-foundations/pier-foundations-field-guide)

### Horror Architecture
- [When Buildings Dream: Environmental Storytelling in Horror Game Design](https://drwedge.uk/2025/05/04/when-buildings-dream-horror-game-design/)
- [Deconstructing the Level Design of Iconic Horror Mansions](https://gamehaunt.com/deconstructing-the-level-design-of-iconic-horror-mansions/)

### Existing Monolith Infrastructure
- `MonolithMeshProceduralActions.cpp` -- `BuildStairs()`, `BuildRamp()`, `BuildPillar()`, `CreateStructure()`, `CreateBuildingShell()`
- `MonolithMeshSpatialActions.cpp` -- `QueryRaycast()`, `QueryMultiRaycast()`, `QueryRadialSweep()`
- `MonolithMeshSceneActions.cpp` -- `SnapToFloor()`
- `MonolithMeshAccessibilityActions.cpp` -- `AnalyzePathComplexity()` (already flags stairs-only paths)
- `MonolithMeshAdvancedLevelActions.cpp` -- `PlaceSpline()` (for railings via spline meshes)
