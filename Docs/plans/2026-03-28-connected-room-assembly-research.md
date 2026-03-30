# Connected Room Assembly: Building Construction from Shared-Wall Rooms

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** How to assemble multi-room buildings where adjacent rooms share walls and doors align, using our existing GeometryScript-based `create_structure` system

---

## Executive Summary

The core problem: when two rooms are placed side by side, their shared wall should be a single wall (not two overlapping), and doors between them must align perfectly. After researching real game implementations (Shadows of Doubt, ARE, Rooms and Mazes, Dwarf Fortress-style), academic papers, and evaluating our existing code, the recommended approach is **"floor plan first, geometry second"** -- a 2D grid of cell IDs determines room ownership, walls are placed only at room-ID boundaries, and doors are pre-planned before any 3D geometry is created.

This research covers 10 topics: shared wall elimination, door alignment, grid snapping, wall segments, floor-plan-first generation, multi-story stacking, exterior vs interior walls, voxel approaches, real implementations, and orchestrating our existing `create_structure`.

---

## 1. Shared Wall Elimination

### The Problem

When Room A's east wall is Room B's west wall, naive generation produces two overlapping wall slabs at the same position. This causes:
- Z-fighting (flickering in renders)
- Double material thickness (looks wrong with translucent/emissive materials)
- Wasted triangles
- Boolean artifacts when cutting doors (cutter hits two separate meshes)

### Solution: Wall Ownership via Grid

The definitive solution across all studied implementations is **the grid decides, not the rooms**. Walls are not "owned" by rooms -- they exist at boundaries between cells with different room IDs.

```
Grid (5x3), each cell stores a room ID:
  [A][A][A][B][B]
  [A][A][A][B][B]
  [A][A][A][B][B]

Walls exist at every boundary where left_cell != right_cell (or top != bottom).
The boundary between column 2 and column 3 has A|B, so ONE wall is placed there.
```

**Why this works:** There is no concept of "Room A's east wall" or "Room B's west wall." There is only "the wall at grid edge (2,y)-(3,y) between rooms A and B." One wall, one piece of geometry, owned by neither room.

### Adjacency Detection Algorithm

```
For each horizontal edge (x, y) to (x+1, y):
    left_id  = grid[x][y]
    right_id = grid[x+1][y]
    if left_id != right_id:
        place_wall(x+1, y, orientation=VERTICAL)

For each vertical edge (x, y) to (x, y+1):
    top_id    = grid[x][y]
    bottom_id = grid[x][y+1]
    if top_id != bottom_id:
        place_wall(x, y+1, orientation=HORIZONTAL)
```

This is O(W*H) for a W x H grid -- trivially fast.

### From Games

- **Rooms and Mazes** (Bob Nystrom / stuffwithstuff.com): Each tile in the 2D grid has a region ID. Walls are implicit -- any solid tile adjacent to two different region IDs is a potential connector (door). Walls simply exist wherever you do NOT carve floor.
- **Shadows of Doubt**: 1.8m x 1.8m tile grid, 15x15 per building floor. Rooms are allocated as sets of tiles. Walls emerge at tile boundaries between different room allocations.
- **ARE** (Gamedeveloper.com): "If a newly generated room shared a wall with an existing room, any doors in that shared wall would automatically link to the new room." The system detects adjacency by projecting outward from room bounds.

---

## 2. Door Alignment

### The Problem

When creating a door between adjacent rooms, both rooms need an opening at exactly the same position. If Room A's door is at X=150 and Room B's door is at X=152, there's a 2cm wall sliver between them.

### Solution: Doors Are Grid-Level Decisions

Doors are not room-level features -- they are **edge-level features** on the floor plan grid. A door exists at a specific grid edge between two rooms, not "on Room A's wall."

```
Floor plan with doors (D = door position on shared wall):
  [A][A][A][B][B]
  [A][A][A|D|B][B]    <-- door at grid edge (2,1)-(3,1)
  [A][A][A][B][B]
```

Since both rooms derive their geometry from the same grid, the door opening is at exactly the same world-space position for both. There is no alignment problem because there is only one wall and one opening.

### Door Placement Algorithm

1. Identify all shared edges between rooms (adjacent cells with different room IDs)
2. For each room pair, select door positions from the set of shared edges
3. Mark those edges as "door" instead of "wall"
4. When generating geometry, skip wall segments at door edges and optionally add doorframe trim

### Door Width Spanning Multiple Grid Cells

A standard door (90-120cm) may span multiple grid cells if the grid resolution is fine (e.g., 50cm cells = 2-3 cells per door). Solutions:

- **Coarse grid (100-200cm cells):** One cell = one door width. Simple but limits room size granularity.
- **Fine grid (25-50cm cells) with door spans:** A door is marked as a contiguous run of 2-4 "door" edges. The geometry builder merges adjacent door edges into a single opening.
- **Hybrid:** Grid at 50cm resolution, doors always occupy 2 cells (100cm standard door). Walls and doors both snap to 50cm increments.

### From Games

- **Rooms and Mazes**: "Connectors" are solid tiles adjacent to two different regions. The algorithm picks connectors to open (carve into floor), creating doors. Since connectors are grid tiles, alignment is guaranteed.
- **Shadows of Doubt**: Door positions are constrained to tile boundaries. "Door quantity varies by room type (bathrooms: single door, others: multiple)."

---

## 3. The Grid Approach

### Core Concept

Snap all room dimensions and positions to a uniform grid. All walls align to grid lines. All doors sit at grid-aligned positions. This eliminates the entire class of "almost aligned" bugs.

### Grid Resolution

| Resolution | Pros | Cons | Best For |
|-----------|------|------|----------|
| 25cm | Fine-grained room sizes, thin corridors possible | Many cells per room, more memory, more edge checks | Detailed interiors, horror game tight spaces |
| 50cm | Good balance, standard door = 2 cells wide | Some size quantization (rooms snap to 50cm increments) | **Recommended for Leviathan** |
| 100cm | Very simple, fast, minimal cells | Coarse -- rooms must be multiples of 1m, hallways minimum 1m wide | Large outdoor structures, warehouses |
| 180cm | Shadows of Doubt uses this | Very coarse for horror game corridors | Open-plan interiors |

**Recommendation: 50cm grid.** This allows:
- Standard door: 2 cells (100cm) or 2.5 cells rounded
- Narrow horror corridor: 3 cells (150cm)
- Standard room: 8x12 cells (400x600cm)
- Wall thickness is NOT grid-aligned -- walls sit centered on grid edges and can be any thickness (10-20cm)

### Grid Data Structure

```cpp
struct FBuildingGrid
{
    int32 Width;         // cells in X
    int32 Depth;         // cells in Y
    float CellSize;      // world units per cell (50.0 = 50cm)
    TArray<int32> Cells; // Width * Depth, stores room ID (0 = empty/exterior)

    int32 GetRoomID(int32 X, int32 Y) const
    {
        return Cells[Y * Width + X];
    }

    FVector CellToWorld(int32 X, int32 Y) const
    {
        // Returns world position of cell center
        return FVector(X * CellSize + CellSize * 0.5f,
                       Y * CellSize + CellSize * 0.5f, 0.0f);
    }

    FVector EdgeToWorld(int32 X, int32 Y, bool bVertical) const
    {
        // Returns world position of a grid edge
        if (bVertical)
            return FVector(X * CellSize, Y * CellSize + CellSize * 0.5f, 0.0f);
        else
            return FVector(X * CellSize + CellSize * 0.5f, Y * CellSize, 0.0f);
    }
};
```

### Grid Advantages for Horror Games

- **Tight spaces are predictable.** A 150cm (3-cell) corridor is exactly wide enough for the player but too narrow to dodge enemies comfortably.
- **Sightlines are calculable.** Grid-aligned walls make raycasting for AI line-of-sight trivial.
- **Navmesh generation is cleaner.** Aligned geometry produces better navmesh with fewer edge cases.
- **Door mechanics are simple.** Doors always occupy known grid edges, making interaction volumes predictable.

---

## 4. Wall Segments Instead of Full Walls

### The Problem with Full Walls

Current `create_structure` builds one continuous wall per room side. When assembling a building, you get:
- Room A: full north wall (400cm)
- Room B (above A): full south wall (400cm)
- These overlap entirely, producing double geometry

### The Wall Segment Approach

Instead of "one wall per room side," build **individual wall segments per grid edge**. A wall segment exists at a grid edge only when:
1. The two cells on either side have different room IDs (interior wall between rooms)
2. One cell is a room and the other is exterior/empty (exterior wall)
3. The edge is NOT marked as a door

A wall segment does NOT exist when:
1. Both cells belong to the same room (no wall needed)
2. The edge is marked as a door (opening instead of wall)

```
Example: 6x4 grid, rooms A, B, C
  [A][A][A][B][B][B]
  [A][A][A][B][B][B]
  [C][C][C][C][C][C]
  [C][C][C][C][C][C]

Vertical wall segments (between columns):
  Col 0-1: A|A = no wall, A|A = no wall, C|C = no wall, C|C = no wall
  Col 1-2: A|A = no wall, A|A = no wall, C|C = no wall, C|C = no wall
  Col 2-3: A|B = WALL,    A|B = WALL,    C|C = no wall, C|C = no wall  (shared wall A-B)
  Col 3-4: B|B = no wall, B|B = no wall, C|C = no wall, C|C = no wall
  Col 4-5: B|B = no wall, B|B = no wall, C|C = no wall, C|C = no wall

Horizontal wall segments (between rows):
  Row 0-1: A|A, A|A, A|A, B|B, B|B, B|B = no walls
  Row 1-2: A|C = WALL x3, B|C = WALL x3  (shared walls A-C and B-C)
  Row 2-3: C|C x6 = no walls
```

### Merging Adjacent Segments

Adjacent wall segments with the same room pair can be merged into a single longer wall for fewer draw calls:

```
Col 2-3 has two consecutive A|B segments (rows 0 and 1).
Merge into one wall segment spanning rows 0-1 at column 2-3.
```

**Merge algorithm:**
```
For each column boundary X:
    segments = []
    for Y = 0 to Height-1:
        left = grid[X-1][Y], right = grid[X][Y]
        if left != right and not is_door(X, Y, VERTICAL):
            extend current segment or start new one
        else:
            close current segment if any
    merge contiguous segments into single wall geometry
```

This produces the minimum number of wall meshes. Each merged segment becomes one `AppendBox` or swept profile call.

---

## 5. Floor Plan First, Geometry Second

### The Two-Phase Architecture

This is the single most important architectural decision. Every successful implementation studied follows this pattern:

**Phase 1: 2D Floor Plan (abstract, no geometry)**
- Allocate rooms on the grid
- Determine room adjacency
- Place doors at shared edges
- Classify walls (interior/exterior)
- Validate connectivity (every room reachable from entrance)

**Phase 2: 3D Geometry (from floor plan)**
- For each grid edge marked "wall": generate wall segment geometry
- For each grid edge marked "door": generate opening + trim geometry
- For each room: generate floor and ceiling slabs
- For exterior edges: use thicker wall geometry

### Why This Order Matters

If you generate geometry per-room (our current approach), you must retroactively fix:
- Overlapping walls (detect and delete duplicates)
- Door misalignment (cut openings after the fact)
- Inconsistent wall thickness (interior vs exterior)

If you generate geometry from the floor plan, these problems never arise because the floor plan is the single source of truth.

### Floor Plan Data Structure

```cpp
enum class ECellType : uint8
{
    Empty = 0,      // Exterior / unallocated
    Room,           // Interior floor space
    Corridor,       // Hallway
    Stairwell,      // Vertical circulation
};

enum class EEdgeType : uint8
{
    None = 0,       // No wall (same room on both sides, or both empty)
    InteriorWall,   // Wall between two rooms
    ExteriorWall,   // Wall between room and exterior
    Door,           // Opening with doorframe
    Window,         // Opening with sill + header
    Archway,        // Opening without door
};

struct FFloorPlan
{
    int32 Width, Depth;
    float CellSize;
    float WallHeight;
    float InteriorWallThickness;  // 10cm
    float ExteriorWallThickness;  // 20cm

    TArray<int32> RoomIDs;        // Width * Depth, 0 = empty
    TArray<ECellType> CellTypes;  // Width * Depth

    // Edges: stored separately for vertical and horizontal
    // Vertical edges: (Width+1) * Depth
    // Horizontal edges: Width * (Depth+1)
    TArray<EEdgeType> VerticalEdges;
    TArray<EEdgeType> HorizontalEdges;

    // Door metadata (position, width override, type)
    TMap<FIntPoint, FDoorSpec> DoorSpecs; // key = edge coordinate

    // Room metadata
    TMap<int32, FRoomSpec> Rooms;  // room ID -> room properties
};
```

### Room Placement Algorithms

Several viable approaches for populating the grid, ordered by complexity:

**A. Manual / AI-Specified Placement**
The MCP caller (Claude or designer) specifies room positions and sizes directly. The system just validates and fills the grid.

```json
{
    "action": "create_building",
    "grid_size": [10, 8],
    "cell_size": 50,
    "rooms": [
        {"id": 1, "name": "living_room", "x": 0, "y": 0, "w": 5, "h": 4},
        {"id": 2, "name": "kitchen",     "x": 5, "y": 0, "w": 3, "h": 4},
        {"id": 3, "name": "bedroom",     "x": 0, "y": 4, "w": 4, "h": 4},
        {"id": 4, "name": "bathroom",    "x": 4, "y": 4, "w": 2, "h": 2},
        {"id": 5, "name": "hallway",     "x": 4, "y": 6, "w": 4, "h": 2}
    ],
    "doors": [
        {"from": 1, "to": 2, "position": [5, 2]},
        {"from": 1, "to": 3, "position": [2, 4]},
        {"from": 3, "to": 5, "position": [4, 5]},
        {"from": 5, "to": 4, "position": [5, 6]}
    ]
}
```

**B. Constrained Growth (Shadows of Doubt style)**
1. Place the most important room first (largest, or the one with the entrance)
2. For each subsequent room in priority order, try all positions adjacent to existing rooms
3. Score each position (window access, shape regularity, adjacency to required neighbors)
4. Pick the best-scoring position

**C. BSP Subdivision**
1. Start with the full building footprint
2. Recursively split into halves (alternating X and Y axis)
3. Stop when a partition is small enough to be a single room
4. Corridors are added along split lines

**D. Squarified Treemap (academic approach)**
Hierarchical room layout based on room area requirements. Produces well-proportioned rooms that fill the available space completely. Used in the Lopes et al. floor plan paper (arXiv:1211.5842).

For Monolith/Leviathan, **approach A (manual placement via MCP)** is the primary mode, with **approach B (constrained growth)** as an optional auto-layout feature.

---

## 6. Multi-Story Stacking

### Vertical Grid Extension

The 2D grid extends to 3D by adding a floor index. Each floor is a separate 2D grid with the same XY dimensions.

```cpp
struct FBuildingPlan
{
    int32 NumFloors;
    float FloorHeight;     // typically 300cm
    float SlabThickness;   // 15cm between floors
    TArray<FFloorPlan> Floors;  // one per story

    // Vertical connections
    TArray<FStairwellSpec> Stairwells;  // spans 2+ floors
    TArray<FElevatorSpec> Elevators;    // spans all floors
};
```

### Stairwell Openings

Stairwells are rectangular regions that cut through floor slabs. They appear on the floor plan as `ECellType::Stairwell` cells.

```
Floor 0 grid:        Floor 1 grid:
  [A][A][S][B]         [C][C][S][D]
  [A][A][S][B]         [C][C][S][D]
  [A][A][A][B]         [C][C][C][D]

S = stairwell cells. Floor slab between floor 0 and floor 1
is NOT generated at stairwell cell positions.
```

**Implementation:**
1. Generate each floor's walls independently from its floor plan
2. Generate floor slabs as polygons with stairwell cutouts (boolean subtract the stairwell rectangle from the floor slab)
3. Stairwell walls span multiple floors -- generate once from the stairwell spec
4. Use our existing `create_structure(type: "stairwell")` for the stair geometry itself

### Vertical Alignment Constraints

- **Stairwells must align vertically** across all floors they span. The same grid cells are marked as stairwell on each floor.
- **Structural walls** (load-bearing) should align vertically. In our context, this means interior walls on upper floors should generally sit above walls on lower floors. Not physically enforced by the game, but produces more realistic buildings.
- **Exterior walls are always the same footprint** on every floor (no cantilevers in this system).

### From Research

- **Vazgriz (Procedurally Generated Dungeons)**: Extended 2D Delaunay triangulation to 3D tetrahedralization for multi-floor dungeons. Staircases require 4 cells (2 for stairs, 2 for headroom above) with a 1:2 rise-to-run ratio.
- **IEEE paper (Procedural Generation of Multi-Story Buildings)**: "A natural bottom-up approach where each building is defined by a set of rooms that can be connected by doors or stairways." Elements spanning multiple floors (elevator shafts, staircases) are positioned first.

---

## 7. Exterior Walls vs Interior Walls

### Classification

From the floor plan, wall classification is trivial:

```
For each grid edge with a wall:
    cell_a = one side of edge
    cell_b = other side of edge
    if cell_a == 0 (empty/exterior) or cell_b == 0:
        wall_type = EXTERIOR
    else:
        wall_type = INTERIOR
```

### Thickness Differences

| Wall Type | Typical Thickness | Material | Notes |
|-----------|------------------|----------|-------|
| Exterior | 15-25cm | Brick, concrete, siding | Weather barrier, insulation |
| Interior load-bearing | 12-15cm | Drywall over studs | Supports upper floors |
| Interior partition | 8-10cm | Drywall, thin plaster | Non-structural dividers |
| Bathroom wet wall | 12-15cm | Drywall + tile backer | Houses plumbing |

For horror game purposes, wall thickness affects:
- Sound transmission (thin walls = more audible enemy footsteps)
- Bullet penetration (if applicable)
- Destruction potential
- Crawl space / vent routing (thick walls can house vent ducts)

### Implementation

When generating wall segment geometry, use the edge's wall type to select thickness:

```cpp
float GetWallThickness(EEdgeType EdgeType, bool bExterior)
{
    if (bExterior) return ExteriorWallThickness;  // 20cm default
    if (EdgeType == EEdgeType::InteriorWall) return InteriorWallThickness; // 10cm default
    return 0.0f; // doors, arches = no wall
}
```

### Wall Centering on Grid Edge

The wall geometry is centered on the grid edge, straddling both rooms equally:

```
Grid edge at X=250 (world space):
  Interior wall (10cm): spans X=245 to X=255
  Exterior wall (20cm): spans X=240 to X=260
```

This means rooms are slightly smaller than their grid allocation (by half the wall thickness on each side). A 5-cell room (250cm grid) with 10cm interior walls on both sides has an interior clear width of 250 - 5 - 5 = 240cm. This matches real-world construction.

---

## 8. The Voxel / Minecraft / Dwarf Fortress Approach

### Concept

Treat the building as a 3D voxel grid. Each voxel is a material type (air, wall, floor, door). Geometry is generated by meshing the voxel boundaries (similar to Minecraft chunk meshing).

```
3D grid (X=5, Y=3, Z=2):
  Layer 0 (floor):  [F][F][F][F][F]  F = floor voxel
                    [F][F][F][F][F]
                    [F][F][F][F][F]

  Layer 1 (walls):  [W][W][W][W][W]  W = wall, A = air
                    [W][A][A][A][W]
                    [W][W][W][W][W]

  Layer 2 (ceiling): [C][C][C][C][C]  C = ceiling voxel
                     [C][C][C][C][C]
                     [C][C][C][C][C]
```

### Pros

- **Extremely robust.** No edge cases with wall overlap, door alignment, or corner geometry. If two rooms share a voxel, there is one voxel, period.
- **Multi-story is trivial.** Just extend the Z axis. Floor/ceiling is a single voxel layer.
- **Destruction is natural.** Remove a wall voxel = hole in wall.
- **Well-understood meshing algorithms.** Greedy meshing, marching cubes, etc.

### Cons

- **Visual quality.** Standard voxel meshing produces blocky geometry. Smooth normals and beveled edges require post-processing.
- **Memory.** A 20m x 20m x 6m building at 10cm voxel resolution = 200 x 200 x 60 = 2.4M voxels. At 25cm = 80 x 80 x 24 = 153K voxels. Manageable but wasteful for mostly-empty rooms.
- **Wall thickness is quantized.** Walls must be N voxels thick. At 25cm resolution, a wall is minimum 25cm.
- **Diagonal walls impossible** without sub-voxel meshing.
- **Overkill for our use case.** We are generating editor-time blockout geometry, not runtime destructible environments.

### Verdict

Not recommended for Monolith. The 2D grid + wall segments approach gives us the same shared-wall benefits with much less complexity and better visual quality. If we ever need runtime destruction, voxels could be revisited.

---

## 9. Real Implementations

### Shadows of Doubt (ColePowered Games)

- **Grid:** 1.8m x 1.8m tiles, 15x15 per building floor
- **Room placement:** Priority-based ranking. Cycles through room types in importance order. For each room type, tries every position, scores based on rules (floor space, shape, window access), picks highest-scoring.
- **Hallways:** Auto-generated when entrance is far from room cluster. Acts as a connecting spine.
- **Shared walls:** Implicit -- rooms are sets of tiles. No wall geometry is generated between tiles of the same room. Walls emerge at tile boundaries between different rooms.
- **Custom editor:** Designer tool for creating floorplan templates with live preview.

### ARE (Gamedeveloper.com article)

- **No strict grid.** Rooms are placed as rectangles with arbitrary positions.
- **Shared walls via projection:** New rooms expand outward until flush with existing rooms. "Project outward to see if there were any other rooms close by. If there were, expand just far enough to become flush."
- **Doors as generators:** New rooms spawn from existing doors. "Select a random door that was only attached to a single room, and generate a room that used that door as an ingress."
- **Auto-linking:** "If a newly generated room shared a wall with an existing room, any doors in that shared wall would automatically link to the new room."

### Rooms and Mazes (Bob Nystrom)

- **Grid-based.** All rooms at odd positions/sizes to mesh with maze passages.
- **Region IDs.** Each disconnected area gets a unique ID. Flood-fill to unify connected regions.
- **Connectors.** Solid tiles adjacent to two different region IDs. Algorithm opens minimal connectors for full connectivity (spanning tree), plus random extras for loops.
- **Wall = uncarved tile.** There is no explicit wall entity. A tile that is solid rock adjacent to floor IS the wall.

### Proc World Blog (Miguel Cepero)

- **L-system grammar** for recursive subdivision of building volumes.
- **Occlusion-based wall handling:** When a large room borders multiple small rooms, overlapping parallel walls occur. Solution: use occlusion queries to detect which walls are hidden behind other geometry, then hide the non-occluded walls and keep the occluded ones. Elegant but computationally unusual.
- **Snap planes:** Imaginary grid lines that constrain door/window placement to regular intervals.

### Grid-Based Dungeon Generator (RogueBasin)

- **Cell grid.** Dungeon divided into equal-sized cells. One room per cell, room size <= cell size - 1.
- **No overlap possible.** "Each room will automatically fit within the map array, and no room will overlap another room."
- **Corridors connect cells.** Carved through cell boundaries.
- **Doors at boundaries.** "When the corridor-building algorithm reaches a room boundary, a door can be placed."

### Academic: Squarified Treemaps for Floor Plans (Lopes et al.)

- **Hierarchical room tree.** Rooms organized by function (social/service/private).
- **Treemap subdivision.** Squarified treemap algorithm produces well-proportioned rooms filling available space.
- **Corridor optimization.** When rooms need connections but aren't adjacent, shortest-path corridors are planned through the room graph.
- **Single-story only** in the paper.

---

## 10. Orchestrating Our Existing create_structure

### Current State

`create_structure` builds individual rooms as self-contained meshes. Each room has 4 walls, floor, and ceiling. The L-corridor and T-junction types connect two room boxes via boolean subtract. There is no concept of a building-level floor plan.

`create_building_shell` creates the exterior walls of a multi-story building by extruding a 2D footprint polygon, but does not subdivide into rooms.

### The Gap

We need a **new action** -- `create_building` -- that:
1. Accepts a floor plan (grid of room IDs + door positions)
2. Generates all wall segments, floors, ceilings, and door openings as a single mesh
3. Uses the grid to guarantee shared walls and aligned doors

### Proposed Architecture: `create_building`

```
Input:
  - grid: 2D array of room IDs (or width/depth + rooms as rectangles)
  - cell_size: world units per grid cell (default 50)
  - wall_height: per floor (default 300)
  - floors: number of stories (default 1)
  - floor_height: total per story including slab (default 315)
  - exterior_wall_thickness: default 20
  - interior_wall_thickness: default 10
  - doors: array of {from_room, to_room, position, width, height}
  - windows: array of {room, wall_direction, position, width, height}
  - stairwells: array of {x, y, w, d, from_floor, to_floor}

Output:
  - Single UDynamicMesh containing entire building
  - Material IDs: 0=exterior wall, 1=interior wall, 2=floor/ceiling, 3=trim
```

### Geometry Generation Pipeline

```
Phase 1: Parse input, populate FFloorPlan grid
Phase 2: Classify edges (interior wall, exterior wall, door, window, none)
Phase 3: Merge adjacent same-type edges into wall segments
Phase 4: For each floor:
    4a. Generate wall segments (AppendBox or swept profile per segment)
    4b. Generate floor slab (AppendSimpleExtrudePolygon with stairwell cutouts)
    4c. Generate ceiling slab (same, or skip if next floor exists)
Phase 5: Generate door openings (merged boolean subtract)
Phase 6: Generate window openings (merged boolean subtract)
Phase 7: Generate trim geometry (door frames, window frames)
Phase 8: UV box projection + normal recomputation + collision
```

### Wall Segment Geometry

For each merged wall segment, we have a start position, end position, height, and thickness. Two options:

**Option A: AppendBox per segment** (simpler)
```cpp
FVector Start = EdgeToWorld(SegStartX, SegStartY);
FVector End   = EdgeToWorld(SegEndX, SegEndY);
float Length  = FVector::Dist(Start, End);
FVector Center = (Start + End) * 0.5f;
float Angle   = FMath::Atan2(End.Y - Start.Y, End.X - Start.X);

FTransform WallXf(FRotator(0, FMath::RadiansToDegrees(Angle), 0),
                   FVector(Center.X, Center.Y, FloorZ), FVector::OneVector);
AppendBox(Mesh, Opts, WallXf, Length, Thickness, WallHeight, 0, 0, 0, Base);
```

**Option B: Swept profile along building perimeter** (better for exterior walls)
For the building exterior, sweep a wall profile along the entire outer perimeter. This produces perfect mitered corners with zero overlaps. Use our existing `BuildWallsSweep` approach but with the building's outer polygon instead of a single room.

**Recommended hybrid:**
- Exterior walls: swept profile along outer footprint (one sweep call for entire perimeter)
- Interior walls: AppendBox per merged segment (simple, fast, no corner issues since interior walls T-junction rather than turn corners)

### Integrating with Existing Actions

The `create_building` action does NOT replace `create_structure`. Instead:

| Action | Use Case |
|--------|----------|
| `create_structure` | Single room/corridor blockout, quick prototyping |
| `create_building_shell` | Exterior-only multi-story shell, no interior subdivision |
| `create_building` (NEW) | Complete building with rooms, shared walls, doors, multi-story |

`create_building` internally reuses:
- `MakeWallProfile()` for swept exterior walls
- `InsetPolygon2D()` if computing interior polygons
- `CleanupMesh()` for post-processing (normals, UVs, collision)
- The boolean subtract pattern for door/window openings
- The trim generation code from `create_structure`

### Example MCP Call

```json
{
    "action": "create_building",
    "cell_size": 50,
    "grid_width": 12,
    "grid_depth": 10,
    "wall_height": 300,
    "exterior_wall_thickness": 20,
    "interior_wall_thickness": 10,
    "rooms": [
        {"id": 1, "name": "living_room", "x": 0, "y": 0, "w": 6, "h": 5, "type": "room"},
        {"id": 2, "name": "kitchen",     "x": 6, "y": 0, "w": 4, "h": 5, "type": "room"},
        {"id": 3, "name": "hallway",     "x": 10,"y": 0, "w": 2, "h": 10,"type": "corridor"},
        {"id": 4, "name": "bedroom",     "x": 0, "y": 5, "w": 5, "h": 5, "type": "room"},
        {"id": 5, "name": "bathroom",    "x": 5, "y": 5, "w": 3, "h": 3, "type": "room"},
        {"id": 6, "name": "closet",      "x": 5, "y": 8, "w": 3, "h": 2, "type": "room"}
    ],
    "doors": [
        {"from": 1, "to": 2, "edge_x": 6, "edge_y": 2, "width": 100, "height": 210},
        {"from": 1, "to": 3, "edge_x": 10,"edge_y": 2, "width": 100, "height": 210},
        {"from": 4, "to": 3, "edge_x": 10,"edge_y": 7, "width": 100, "height": 210},
        {"from": 5, "to": 3, "edge_x": 8, "edge_y": 6, "width": 80,  "height": 210},
        {"from": 5, "to": 6, "edge_x": 6, "edge_y": 8, "width": 80,  "height": 210},
        {"from": 1, "to": 4, "edge_x": 2, "edge_y": 5, "width": 100, "height": 210}
    ],
    "windows": [
        {"room": 1, "wall": "south", "position": 3, "width": 120, "height": 100, "sill_height": 100},
        {"room": 4, "wall": "south", "position": 2, "width": 120, "height": 100, "sill_height": 100}
    ],
    "save_path": "/Game/Generated/Buildings/Apartment_01",
    "place_in_scene": true
}
```

This produces a 6m x 5m building (12 cells x 10 cells at 50cm each) with 6 rooms, 6 doors, 2 windows, correct shared walls, and proper exterior/interior wall thickness differentiation.

---

## 11. Implementation Plan

### Phase 1: Floor Plan Data Structures (~4h)

- `FBuildingCell`, `FBuildingEdge`, `FFloorPlan`, `FBuildingPlan` structs
- Grid population from room rectangles
- Edge classification (interior/exterior/door/window/none)
- Adjacent segment merging algorithm
- Validation (no overlapping rooms, all rooms connected, doors on valid edges)

### Phase 2: Wall Segment Geometry (~6h)

- Exterior wall generation via swept profile along outer perimeter
- Interior wall generation via AppendBox per merged segment
- Material ID assignment (exterior=0, interior=1)
- Floor/ceiling slab generation with stairwell cutouts

### Phase 3: Openings (~4h)

- Door opening boolean subtract (merged cutter approach from existing code)
- Window opening boolean subtract
- Trim frame generation (reuse existing trim code)
- Edge case: door at exterior wall (front door), door at interior wall

### Phase 4: Multi-Story (~4h)

- Per-floor grid support in FBuildingPlan
- Floor slab generation between stories
- Stairwell cutout (boolean subtract through floor slabs)
- Vertical wall continuity (exterior walls span all floors as single sweep)
- Stair geometry placement (delegate to `create_structure(type: "stairwell")`)

### Phase 5: MCP Action + Cleanup (~4h)

- `create_building` action registration and parameter parsing
- JSON schema for rooms, doors, windows, stairwells
- CleanupMesh integration (normals, UVs, collision)
- Auto-save path generation
- Testing with various building configurations

### Phase 6: Optional Auto-Layout (~6h, future)

- Constrained growth room placement (Shadows of Doubt style)
- BSP subdivision option
- Corridor auto-routing between non-adjacent rooms
- Entrance placement and hallway spine generation

### Total Estimate

- **Phases 1-5 (core):** ~22 hours
- **Phase 6 (auto-layout):** ~6 hours (optional, future)
- **Total with auto-layout:** ~28 hours

---

## 12. Key Design Decisions

### Decision 1: One Mesh vs Multiple Meshes

**One mesh per building** (recommended). Merge all geometry into a single UDynamicMesh with material IDs for differentiation. This is what `create_structure` already does.

Rationale:
- Fewer actors in the level
- One collision body
- One draw call per material slot
- Easier to save as a single static mesh asset
- Room-level separation not needed for blockout (rooms are distinguished by material, not by actor)

### Decision 2: Grid Cell Size

**50cm default** with parameter override. See Section 3 for rationale.

### Decision 3: Wall Construction Method

**Hybrid:** Swept profile for exterior perimeter, AppendBox for interior segments. See Section 10.

### Decision 4: Door Specification

**Edge-based coordinates** (grid edge X, Y) rather than room-relative offsets. This guarantees alignment because the door position is defined once on the shared edge, not twice on each room's wall.

### Decision 5: Keep create_structure Separate

`create_building` is a new action, not a replacement. `create_structure` remains for quick single-room prototyping. They share utility functions (MakeWallProfile, InsetPolygon2D, CleanupMesh, trim generation).

---

## 13. Horror Game Considerations

### Tight Spaces and Claustrophobia

The grid system makes it easy to create intentionally cramped layouts:
- 150cm (3-cell) corridors where the player can barely turn around
- L-shaped hallways with blind corners (place walls to break sightlines)
- Rooms that feel too small by making them 3x3 cells (150x150cm)

### Sound Propagation

Wall thickness and material type (stored as MaterialID) can inform a sound occlusion system:
- Thin interior walls: partial sound occlusion
- Thick exterior walls: full occlusion
- Doors: variable occlusion based on open/closed state
- The grid makes it trivial to trace a path between two rooms through doors

### AI Navigation

Grid-aligned walls produce clean navmesh. Door positions on the grid map directly to navmesh links. The floor plan grid itself can be used as a high-level pathfinding graph (rooms as nodes, doors as edges) for AI that needs to plan multi-room paths.

### Environmental Storytelling

Different room types can have different:
- Floor materials (MaterialID on floor slab)
- Wall materials (per-room material override)
- Ceiling height (per-room override of wall_height)
- Lighting (spawn light actors at room centers)

---

## 14. Sources

### Game Implementations
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Procedural Dynamic Room Generation in ARE](https://www.gamedeveloper.com/programming/procedural-dynamic-room-generation-in-quot-are-quot-)
- [Rooms and Mazes: A Procedural Dungeon Generator (Bob Nystrom)](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/)
- [Procedurally Generated Dungeons (Vazgriz) -- 3D multi-floor](https://vazgriz.com/119/procedurally-generated-dungeons/)
- [Grid Based Dungeon Generator (RogueBasin)](https://www.roguebasin.com/index.php/Grid_Based_Dungeon_Generator)

### Academic / Technical
- [A Novel Algorithm for Real-time Procedural Generation of Building Floor Plans (Lopes et al.)](https://ar5iv.labs.arxiv.org/html/1211.5842)
- [Procedural Generation of Building Interiors -- Subveillance devlog](https://unusual-intersection.itch.io/subveillance/devlog/492733/procedural-generation-of-building-interiors-part-1)
- [Proc World Blog: Building Rooms (Miguel Cepero)](http://procworld.blogspot.com/2012/03/building-rooms.html)
- [Procedural Generation of Multi-Story Buildings with Interior (IEEE)](https://ieeexplore.ieee.org/document/8926482/)
- [Constrained Growth Method for Procedural Floor Plan Generation (TU Delft)](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)

### UE5 / GeometryScript
- [UE5-Procedural-Building (bendemott, GitHub) -- GeometryScript C++ reference](https://github.com/bendemott/UE5-Procedural-Building)
- [Procedural building best approach UE5 (Epic Forums)](https://forums.unrealengine.com/t/procedural-building-best-technical-approach-ue5/675855)

### Constraint-Based
- [Wave Function Collapse Tips and Tricks (BorisTheBrave)](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [WFC Explained (BorisTheBrave)](https://www.boristhebrave.com/2020/04/13/wave-function-collapse-explained/)

### Our Existing Code
- `Source/MonolithMesh/Private/MonolithMeshProceduralActions.cpp` -- `CreateStructure` (line 1683), `CreateBuildingShell` (line 2049), `MakeWallProfile` (line 1449), `InsetPolygon2D` (line 1460), `BuildWallsSweep` (line 1749)
- Prior research: `Docs/plans/2026-03-28-thin-wall-geometry-research.md`, `Docs/plans/2026-03-28-boolean-doors-research.md`
