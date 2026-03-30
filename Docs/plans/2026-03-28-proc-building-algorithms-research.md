# Procedural Building Generation Algorithms Research

**Date:** 2026-03-28
**Purpose:** Comprehensive research into algorithms for generating complete, enterable buildings from connected rooms for a UE 5.7 FPS survival horror game (Leviathan).
**Context:** Monolith already has `create_structure` (single rooms/corridors/junctions), `create_building_shell` (exterior-only), `create_maze`, and 14+ parametric furniture/horror prop builders via GeometryScript. This research covers how to compose those primitives into full buildings.

---

## Table of Contents

1. [Shape Grammars / Split Grammars (CGA)](#1-shape-grammars--split-grammars-cga)
2. [L-Systems for Architecture](#2-l-systems-for-architecture)
3. [BSP (Binary Space Partitioning)](#3-bsp-binary-space-partitioning)
4. [Wave Function Collapse (WFC)](#4-wave-function-collapse-wfc)
5. [Graph-Based Approaches](#5-graph-based-approaches)
6. [Cyclic Dungeon Generation (Dormans)](#6-cyclic-dungeon-generation-dormans)
7. [Delaunay + MST (TinyKeep Method)](#7-delaunay--mst-tinykeep-method)
8. [Rooms and Mazes (Nystrom Method)](#8-rooms-and-mazes-nystrom-method)
9. [Squarified Treemap Floor Plans](#9-squarified-treemap-floor-plans)
10. [Constrained Growth Method](#10-constrained-growth-method)
11. [Real-World Building Archetypes](#11-real-world-building-archetypes)
12. [The Connected Rooms Approach](#12-the-connected-rooms-approach)
13. [Horror-Specific Architectural Design](#13-horror-specific-architectural-design)
14. [Existing Implementations](#14-existing-implementations)
15. [Algorithm Comparison Matrix](#15-algorithm-comparison-matrix)
16. [Recommended Architecture for Monolith](#16-recommended-architecture-for-monolith)

---

## 1. Shape Grammars / Split Grammars (CGA)

### Overview

CGA Shape (Computer Generated Architecture) is a rule-based system developed by Mueller et al. (2006) for procedural building modeling. It is the core language of ESRI CityEngine. Rules iteratively transform shapes through split, repeat, extrude, and component operations, producing buildings with high geometric detail from compact rule sets.

### How It Works

**Rule syntax:** `Predecessor --> Successor`

Rules operate on "shapes" -- geometric volumes with a local bounding box called the **scope** (position, rotation, size). Each rule replaces a shape symbol with operations that produce child shapes.

**Core operations:**

| Operation | Description |
|-----------|-------------|
| `extrude(h)` | Lifts 2D footprint into 3D volume of height h |
| `split(axis) { sizes : symbols }` | Divides shape along x/y/z into sub-shapes |
| `repeat(axis) { size : symbol }*` | Repeats a sub-shape to fill available space |
| `comp(type) { selector : symbol }` | Component split -- extracts faces/edges/vertices |
| `insert(asset)` | Replaces shape with 3D model asset |
| `t(x,y,z)` / `s(x,y,z)` / `r(a)` | Translate, scale, rotate scope |

**Example hierarchy -- Lot to Building:**

```
Lot      --> extrude(12)                     Building
Building --> split(y) { 4: GroundFloor | ~4: UpperFloor }*
GroundFloor --> split(x) { ~3: Shopfront }*
UpperFloor  --> split(x) { ~2.5: WindowBay }*
WindowBay   --> split(y) { 0.5: Sill | ~2: Window | 0.5: Header }
Window      --> insert("window_asset.obj")
```

The `~` prefix means "flexible size" -- the engine adjusts to fill remaining space evenly. The `*` suffix means "repeat until full."

### Strengths for Our Use Case

- **Facade generation**: Excellent at turning a box into a detailed exterior with windows, doors, cornices, trim
- **Style variation**: Stochastic rules (`p(0.3): StyleA | p(0.7): StyleB`) produce visual variety from one ruleset
- **Context sensitivity**: Rules can test height, edge angle, neighbor adjacency to avoid invalid placements (doors only at ground level, windows not intersecting corners)
- **Compact**: 190 rules generated the entire city of Pompeii in one academic project

### Weaknesses for Our Use Case

- **Exterior-focused**: CGA was designed for building shells viewed from outside. It does NOT natively generate room layouts or interiors
- **No topology awareness**: Rules operate on individual shapes without knowledge of the building's spatial graph (which rooms connect where)
- **Overkill for blockout**: We need rooms first, facades second. CGA solves the facade problem well but not the floor plan problem

### Relevance to Monolith

CGA split/repeat logic maps directly onto our existing `create_structure` wall subdivision. We could use CGA-style rules as a **post-processing pass** after floor plans are generated -- turning blank walls into windowed/doored facades. The rule system is lightweight to implement: each rule is essentially a recursive function that takes a bounding box and produces child boxes.

### Key Sources

- Mueller et al. "Procedural Modeling of Buildings" (SIGGRAPH 2006) -- the foundational paper
- [CGA Reference (ArcGIS CityEngine)](https://doc.arcgis.com/en/cityengine/latest/help/help-component-split.htm)
- [PSU GEOG 497 CGA Tutorial](https://www.e-education.psu.edu/geogvr/node/891)
- [GameDev.net Shape Grammar Tutorial](https://www.gamedev.net/tutorials/programming/engines-and-middleware/procedural-modeling-of-buildings-with-shape-grammars-r4596/)

---

## 2. L-Systems for Architecture

### Overview

L-systems (Lindenmayer systems) are parallel string rewriting systems originally designed to model plant growth. Adapted for architecture, they use parametric rules with conditions and stochastic branching to grow building structures iteratively.

### How It Works

**Components:**
- **Alphabet**: Symbols representing building elements (F = floor, W = wall, R = room, etc.)
- **Axiom**: Starting string (e.g., `Foundation`)
- **Production rules**: `predecessor : condition --> successor`
- **Interpretation**: Turtle graphics or direct geometry mapping

**Parametric example:**

```
axiom: Building(1, 12, 20, 30)

# Building(floor, height, width, depth) where floor < maxFloors
Building(f, h, w, d) : h > 3 --> Floor(f, 3, w, d) Building(f+1, h-3, w, d)
Building(f, h, w, d) : h <= 3 --> Roof(w, d)

# Floor(f, h, w, d) -- subdivide into rooms
Floor(f, h, w, d) : f == 1 --> Lobby(h, w*0.4, d) Hallway(h, w*0.2, d) [Rooms(h, w*0.4, d)]
Floor(f, h, w, d) : f > 1  --> Hallway(h, w*0.2, d) [Rooms(h, w*0.4, d)] [Rooms(h, w*0.4, d)]
```

Brackets `[` and `]` represent branching (push/pop scope). The system terminates when only terminal symbols remain.

### Strengths

- **Natural growth patterns**: Buildings can "grow" organically, adding wings and extensions
- **Parametric control**: Room sizes, floor heights, and proportions are explicit parameters
- **Stochastic variation**: Rules with probability weights produce unique buildings per seed
- **Hierarchical**: Building -> Floor -> Wing -> Room decomposition is natural

### Weaknesses

- **Hard to control topology**: L-systems are great at branching structures but poor at ensuring specific room adjacencies or connectivity constraints
- **String rewriting overhead**: For buildings with many rooms, the intermediate string manipulation is unnecessary complexity compared to direct graph-based approaches
- **Better for exteriors/massing than interiors**: CityEngine uses L-systems for road networks and building mass models, not room layouts

### Relevance to Monolith

L-systems are a **poor fit** for our core problem (generating connected room interiors) but could be useful for:
- Generating building footprint shapes (L-shaped, T-shaped, U-shaped by growth rules)
- Creating organic vine/root/pipe decorations that grow along walls
- Producing hedge mazes or garden layouts around buildings

### Key Sources

- [L-Systems Wikipedia](https://en.wikipedia.org/wiki/L-system)
- [Michael Hansmeyer - L-Systems in Architecture](https://michael-hansmeyer.com/l-systems)
- phiresky/procedural-cities on GitHub -- uses L-systems for road networks + buildings
- "Algorithmic Beauty of Buildings" (Trinity University thesis)

---

## 3. BSP (Binary Space Partitioning)

### Overview

BSP is the most widely-used algorithm for roguelike dungeon generation. It recursively bisects a rectangle into smaller partitions, places rooms in leaf nodes, and connects siblings via corridors. Used by NetHack, Dungeon Crawl Stone Soup, and countless roguelikes.

### Algorithm

```
function BSP_Generate(rect, depth):
    if depth >= max_depth or rect.area < min_area:
        return LeafNode(rect)

    # Choose split direction (prefer splitting the longer axis)
    if rect.width > rect.height * 1.25:
        direction = VERTICAL
    elif rect.height > rect.width * 1.25:
        direction = HORIZONTAL
    else:
        direction = random_choice(VERTICAL, HORIZONTAL)

    # Choose split position (45-55% for balanced rooms)
    split_ratio = random(0.45, 0.55)

    if direction == VERTICAL:
        left  = Rect(rect.x, rect.y, rect.w * split_ratio, rect.h)
        right = Rect(rect.x + rect.w * split_ratio, rect.y, rect.w * (1-split_ratio), rect.h)
    else:
        left  = Rect(rect.x, rect.y, rect.w, rect.h * split_ratio)
        right = Rect(rect.x, rect.y + rect.h * split_ratio, rect.w, rect.h * (1-split_ratio))

    node = InternalNode()
    node.left  = BSP_Generate(left,  depth + 1)
    node.right = BSP_Generate(right, depth + 1)
    return node
```

**Room placement in leaves:**

```
function PlaceRoom(leaf):
    padding = random(0, leaf.w / 3)  # per side
    room.x = leaf.x + padding
    room.y = leaf.y + padding
    room.w = leaf.w - 2 * padding
    room.h = leaf.h - 2 * padding
    return room
```

**Corridor connection (siblings):**

For each internal node, draw an L-shaped corridor between the centers of its left and right children. This guarantees full connectivity by construction.

### Data Structures

```
struct BSPNode {
    Rect bounds;
    BSPNode* left;
    BSPNode* right;
    Room* room;       // non-null only for leaf nodes
};
```

### Strengths

- **Guaranteed no overlap**: Rooms are confined to non-overlapping partitions
- **Guaranteed connectivity**: Sibling connection traversal ensures all rooms reachable
- **Simple to implement**: ~100 lines of code for the core algorithm
- **Controllable room sizes**: Min/max partition size directly controls room dimensions
- **Grid-friendly**: Naturally produces axis-aligned rectangular rooms

### Weaknesses

- **Grid-like results**: Buildings feel like subdivided rectangles (because they are)
- **Poor room variety**: All rooms are rectangular; no L-shapes, irregular polygons
- **No adjacency control**: You can't specify that the kitchen must be next to the dining room
- **Corridors feel artificial**: L-shaped corridors between room centers don't look like hallways in real buildings
- **Single floor only**: No native multi-story support (must be layered manually)

### Enhancement: BSP + Room Type Assignment

After BSP generates the spatial layout, assign room types based on:
- Position (ground floor rooms near entrance = lobby/hallway)
- Size (larger partitions = living rooms; smaller = closets/bathrooms)
- Adjacency to exterior walls (windowed rooms vs interior storage)

### Relevance to Monolith

BSP is a strong **starting point** for floor plan generation. It works well within a rectangular building footprint and guarantees valid, non-overlapping rooms. The main limitation -- inability to specify room adjacency -- can be addressed by a hybrid approach: use BSP for spatial subdivision, then swap room assignments to satisfy an adjacency graph.

### Key Sources

- [RogueBasin BSP Tutorial](https://www.roguebasin.com/index.php/Basic_BSP_Dungeon_generation)
- [eskerda.com BSP Dungeon Generation](https://eskerda.com/bsp-dungeon-generation/)
- [Bracketproductions BSP Tutorial (Rust)](https://bfnightly.bracketproductions.com/chapter_25.html)

---

## 4. Wave Function Collapse (WFC)

### Overview

WFC (Maxim Gumin, 2016) is a constraint-propagation algorithm that generates output by tiling a grid such that every tile's neighbors satisfy adjacency rules. For buildings, tiles can represent room segments, corridors, walls, and doors.

### Algorithm

```
function WFC_Generate(grid_size, tileset, adjacency_rules):
    # Initialize: every cell can be any tile
    for each cell in grid:
        cell.possibilities = copy(tileset)

    while any cell has > 1 possibility:
        # 1. Find cell with minimum entropy (fewest possibilities)
        cell = min_entropy_cell(grid)

        # 2. Collapse: choose one tile (weighted random)
        chosen = weighted_random(cell.possibilities, tile_weights)
        cell.possibilities = [chosen]

        # 3. Propagate constraints to neighbors
        propagate(grid, cell, adjacency_rules)

    return grid

function propagate(grid, changed_cell, rules):
    stack = [changed_cell]
    while stack not empty:
        current = stack.pop()
        for each neighbor of current:
            for each possible_tile in neighbor.possibilities:
                if not compatible(current.collapsed_tile, possible_tile, direction, rules):
                    neighbor.possibilities.remove(possible_tile)
                    if neighbor.possibilities changed:
                        stack.push(neighbor)
            if neighbor.possibilities is empty:
                # Contradiction! Need backtracking
                raise Contradiction
```

### Tile Design for Buildings

The key insight is designing the **tile set** -- what each tile represents and what can be adjacent to what.

**Minimal floor plan tileset (2D top-down):**

| Tile | Description | Edge Labels (N/E/S/W) |
|------|-------------|----------------------|
| `EMPTY` | Outside/void | void/void/void/void |
| `ROOM` | Interior floor | wall_or_open on all sides |
| `WALL_H` | Horizontal wall | room/wall/room/wall |
| `WALL_V` | Vertical wall | wall/room/wall/room |
| `DOOR_H` | Horizontal door | room/door/room/door |
| `DOOR_V` | Vertical door | door/room/door/room |
| `CORNER` | Wall corner | wall/room/room/wall (+ rotations) |
| `CORRIDOR` | Hallway segment | open/wall/open/wall (+ rotation) |
| `T_JUNCTION` | T-intersection | open/open/wall/open (+ rotations) |
| `CROSSROADS` | 4-way intersection | open/open/open/open |
| `DEAD_END` | Corridor terminus | open/wall/wall/wall (+ rotations) |

**Edge matching rules**: Two adjacent tiles are compatible if their touching edge labels match. `room` matches `room`, `wall` matches `wall`, `void` matches `void`.

### Practical Tips (Boris the Brave)

- **Marching cubes approach**: Design tiles by their corner behavior rather than overall appearance. If corners match, connections work automatically
- **Fixed tiles**: Pre-place entrance/exit points before generation. WFC integrates these anchors seamlessly
- **Path constraint**: Add a global constraint ensuring at least one continuous path between entrance and exit -- solves WFC's tendency to create disconnected regions
- **Boundary condition**: Make border cells collapsed to `EMPTY` tile, forcing the building to have exterior walls
- **Regional tilesets**: Use different tile sets per floor/zone to create distinct areas (hospital wing vs maintenance corridor)
- **Tile weights**: Adjust frequency to control room-to-corridor ratio. Higher weight on `ROOM` tiles = more open spaces; higher weight on `CORRIDOR` = narrower passages

### Strengths

- **Local constraints produce global structure**: Simple adjacency rules create coherent layouts
- **Easy to extend**: Adding new room types = adding new tiles + adjacency rules
- **Non-rectangular rooms possible**: With 3D tiles or overlapping tile boundaries
- **Natural variety**: Different seeds produce very different layouts
- **Horror-friendly**: Can encode unsettling patterns (dead ends, loops, ambiguous spaces)

### Weaknesses

- **No global structure guarantee**: Without path constraints, WFC can produce disconnected rooms
- **Tile design is the real challenge**: The algorithm is simple; designing a good tileset that produces realistic buildings is hard
- **Backtracking needed**: Contradictions require restart or backtracking, which can be slow for large grids
- **Grid-locked**: Rooms are composed of tiles, so room shapes are constrained to tile boundaries
- **Room identity**: WFC doesn't inherently know "this is the kitchen" -- rooms are just collections of floor tiles

### Academic Application

A 2023 HAW Hamburg thesis investigated WFC specifically for architectural design, adapting the algorithm with:
- Predefined room adjacency rules as constraints
- Dataset of room types with adjacency matrices
- Grid-based floor plan output satisfying architectural objectives

A BIM42 blog post demonstrated using WFC with architectural plan adjacencies, creating Excel-based adjacency rule tables and C# implementations outputting CSV floor plan grids.

### Relevance to Monolith

WFC is excellent for **interior detailing** -- filling a room with furniture, placing wall decorations, generating tile patterns. For whole-building floor plans, WFC works but requires careful tileset design and global connectivity constraints. Best used as a **second pass** after a higher-level algorithm determines room layout.

### Key Sources

- [mxgmn/WaveFunctionCollapse (GitHub)](https://github.com/mxgmn/WaveFunctionCollapse) -- the original implementation
- [Boris the Brave WFC Tips](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Sam Aston - WFC & Plan Adjacencies](https://www.samuelaston.com/wave-function-collapse-plan-adjacencies/)
- [BIM42 - Generative Design with WFC](https://www.bim42.com/2020/09/generative-design-with-the-wave-function-collapse-algorithm)
- [WFC for Building Massing (academia.edu)](https://www.academia.edu/44870033/)

---

## 5. Graph-Based Approaches

### Overview

Graph-based methods separate the **topological** problem (which rooms connect to which) from the **spatial** problem (where rooms go physically). First generate an adjacency graph, then lay it out in 2D/3D space.

### Algorithm: Lopes et al. (2012) -- Real-time Floor Plan Generation

This is one of the most complete academic algorithms for procedural floor plans. Five phases:

**Phase 1: Outer Shape**
Generate the building footprint as a rectangle with aspect ratio sampled from a distribution (reject ratios > 2.5 for realism). Area sampled from building-type-specific distribution.

**Phase 2: Room Sizing**
- Room count from statistical distributions (based on Canadian census data)
- Three functional categories: **social** (living room, dining), **service** (kitchen, laundry), **private** (bedrooms, bathrooms)
- Each room type has its own area distribution:
  - Bedrooms: Uniform(8, 18) sq meters
  - Other rooms: Uniform(3, 11) sq meters

**Phase 3: Hierarchical Connectivity Graph**
Build a tree representing required room connections:

```
Outside (root)
  +-- Living Room
  |     +-- Kitchen
  |     |     +-- Laundry
  |     |     +-- Pantry
  |     +-- Master Bedroom
  |     |     +-- Master Bathroom
  |     +-- Bedroom 2
  |     +-- Bathroom (if only 1)
  +-- Hallway (optional corridor node)
```

**Connection rules:**
- No bedroom-to-bedroom direct connections
- No bathroom-to-bathroom adjacencies (except through bedrooms)
- Kitchen connects to living room
- All bedrooms accessible from hallway or living room
- Every bedroom has access to at least one bathroom without passing through another bedroom

**Phase 4: Spatial Layout via Squarified Treemap**
Use the Squarified Treemap algorithm to subdivide the building rectangle into rooms, maintaining the hierarchical tree structure. This produces zero-waste layouts where all space is allocated.

**Phase 5: Corridor Optimization**
- Identify rooms that need corridors (not directly adjacent to their parent in the tree)
- Build a graph of walls from corridor-dependent rooms
- Apply Dijkstra's shortest path to minimize corridor area
- For rooms sharing only a vertex (no shared edge), either shift or lengthen adjacent walls to create a door-capable shared edge

**Phase 6: Doors and Windows**
- Doors placed randomly along shared walls between connected rooms
- Windows placed on exterior walls (excluded from bathrooms)

### Graph2Plan (2020)

A more recent approach using neural networks:
- Input: layout graph of rooms (nodes) and adjacencies (edges)
- A GNN + CNN translates the abstract plan into a spatial floor plan
- Explicitly encodes functional relationships (hallway connects to multiple rooms, living and dining share boundary)

### Strengths

- **Architectural realism**: Room connectivity mirrors real buildings
- **Constraint-satisfiable**: Can enforce specific adjacency requirements
- **Building-type flexibility**: Swap the connectivity graph template to get different building types (house vs clinic vs office)
- **Separates concerns**: Topology and geometry are independent problems

### Weaknesses

- **Spatial layout is the hard part**: Going from an abstract graph to non-overlapping rectangles that fit a footprint is a constraint satisfaction problem that can be expensive
- **Treemap produces boxy results**: All rooms perfectly fill the rectangle with no corridors unless explicitly added
- **No natural dead space**: Real buildings have closets, mechanical rooms, leftover spaces -- treemaps are too efficient

### Relevance to Monolith

Graph-based is the **strongest candidate** for our primary algorithm. We can define building archetype templates as connectivity graphs (see Section 11) and use treemap or CSP layout to place rooms spatially. This maps directly to our existing `create_structure` API -- generate the graph, then emit `create_structure` calls for each room.

### Key Sources

- [Lopes et al. "A Novel Algorithm for Real-time Procedural Generation of Building Floor Plans" (2012)](https://ar5iv.labs.arxiv.org/html/1211.5842)
- [Marson & Musse "Automatic Real-Time Generation of Floor Plans Based on Squarified Treemaps" (2010)](https://onlinelibrary.wiley.com/doi/10.1155/2010/624817)

---

## 6. Cyclic Dungeon Generation (Dormans)

### Overview

Developed by Dr. Joris Dormans for the roguelite Unexplored (2017). Instead of generating rooms then connecting them, this approach generates **gameplay cycles** (circular loops of rooms) that create meaningful progression with lock-key patterns and shortcuts.

### Algorithm

1. **Initialize** a 5x5 grid with start and end nodes
2. **Create a cycle**: A circular loop connecting start to end via two independent arcs
3. **Select cycle type** from 24 predefined types (lock-key, hidden shortcut, hub, one-way gate, etc.)
4. **Add minor cycles**: Branch additional loops off the main cycle for complexity
5. **Resolve through graph rewriting**: Apply PhantomGrammar rules (~5000 rules in Unexplored) to transform abstract nodes into concrete room types
6. **Expand grid**: 2x for corridor space, then 5x for final tilemap
7. **Apply terrain**: Cellular automata generate 4-tier terrain variation within rooms

### Cycle Types (examples)

- **Lock-Key**: One arc contains the key, the other contains the locked door
- **Hidden Shortcut**: Main path is long; secondary path is hidden but shorter
- **Hub**: Central room connects to multiple dead-end branches
- **One-Way Gate**: Can only traverse the shortcut in one direction
- **Gauntlet**: Both arcs are dangerous; choice of which danger to face

### Why It Matters for Horror

This is the **most relevant algorithm for horror game design** because:
- Cycles create **uncertainty**: Players don't know if they're going the right way
- Lock-key patterns create **backtracking** which builds tension
- Multiple paths enable **ambush points** and **escape routes**
- The structure feels handcrafted because it IS designed (the cycles are authored, only the specifics are random)

### Relevance to Monolith

Cyclic generation would be excellent as a **high-level layout planner** that operates above the room-by-room generation. Define a building as a set of interconnected cycles, then use graph-based layout to spatialize them. This could drive encounter/scare placement alongside room generation.

### Key Sources

- [Gamasutra: "Unexplored's Secret: Cyclic Dungeon Generation"](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-)
- [Boris the Brave: "Dungeon Generation in Unexplored"](https://www.boristhebrave.com/2021/04/10/dungeon-generation-in-unexplored/)

---

## 7. Delaunay + MST (TinyKeep Method)

### Overview

Used by TinyKeep and numerous indie games. Generates organic-feeling dungeons by treating rooms as physics objects, separating them, then connecting with a graph.

### Algorithm

**Step 1: Generate rooms in a circle**
```
function generate_rooms(count, circle_radius):
    rooms = []
    for i in range(count):
        # Random point in circle (uniform distribution)
        angle = random() * 2 * PI
        u = random() + random()
        r = (2 - u) if u > 1 else u
        pos = (circle_radius * r * cos(angle), circle_radius * r * sin(angle))

        # Room dimensions from normal distribution
        w = normal_distribution(mean=8, stddev=3)
        h = normal_distribution(mean=8, stddev=3)
        rooms.append(Room(pos, w, h))
    return rooms
```

**Step 2: Separate rooms (physics simulation)**
Apply separation steering behavior until no rooms overlap. Snap to tile grid.

**Step 3: Select main rooms**
Filter rooms where both width > mean_width * 1.25 AND height > mean_height * 1.25. These become hub rooms.

**Step 4: Delaunay triangulation**
Compute Delaunay triangulation of main room centers using Bowyer-Watson algorithm. This produces a fully-connected graph with no crossing edges.

**Step 5: Minimum spanning tree + extras**
Compute MST from the Delaunay graph (ensures all rooms reachable with minimum edges). Then add back 8-15% of the removed Delaunay edges to create loops.

**Step 6: Generate corridors**
For each edge in the final graph:
- If rooms overlap on X axis: vertical corridor
- If rooms overlap on Y axis: horizontal corridor
- Otherwise: L-shaped corridor (horizontal then vertical)

Corridors are 3 tiles wide minimum. Any small rooms that intersect a corridor path become part of the dungeon.

### Strengths

- **Organic layouts**: Physics separation produces natural-feeling room arrangements
- **Loop control**: MST + selective edge restoration gives tunable loop frequency
- **Non-rectangular overall shape**: Rooms spread in a circle, not confined to a rectangle
- **Room size variety**: Normal distribution produces natural size variation

### Weaknesses

- **Dungeon-oriented, not building-oriented**: Results look like caves/dungeons, not buildings
- **No room type awareness**: Rooms are undifferentiated rectangles
- **Corridor-heavy**: Many corridors connecting distant rooms
- **Physics dependency**: Separation steering requires a physics simulation step

### Relevance to Monolith

The Delaunay + MST approach is good for generating **floor-level connectivity graphs** -- which rooms should connect to which. We could use it as an alternative to hand-authored building archetype graphs, letting the system discover interesting topologies. The separation steering is overkill for building interiors (rooms should tile neatly, not spread apart).

### Key Sources

- [Gamasutra: "Procedural Dungeon Generation Algorithm"](https://www.gamedeveloper.com/programming/procedural-dungeon-generation-algorithm)
- [vazgriz.com: "Procedurally Generated Dungeons"](https://vazgriz.com/119/procedurally-generated-dungeons/)
- [Future Data Lab: Procedural Dungeon Generator](https://slsdo.github.io/procedural-dungeon/)

---

## 8. Rooms and Mazes (Nystrom Method)

### Overview

Bob Nystrom's influential approach (2014) generates dungeons by placing rooms, filling remaining space with mazes, connecting all regions, adding loops, and pruning dead ends.

### Algorithm

**Step 1: Place rooms**
Place non-overlapping rectangular rooms at random positions, aligned to odd tile boundaries. Fixed number of placement attempts (not target count).

**Step 2: Fill remaining space with mazes**
Iterate over every solid tile. When a solid tile is found where a maze could start, run a randomized flood-fill maze generator (growing tree algorithm). The maze avoids carving into existing rooms, naturally filling interstitial space.

**Step 3: Identify connectors**
Find all solid tiles adjacent to two different regions (different rooms or different maze sections). These are potential doorways.

**Step 4: Connect all regions (spanning tree)**
```
pick random room as "main region"
while unconnected regions exist:
    pick random connector touching main region
    open it (create doorway)
    merge connected region into main
    remove now-redundant connectors
```

**Step 5: Add loops (imperfection)**
When culling redundant connectors, give each a small chance (~3%) of being opened anyway. This creates cycles/loops -- imperfect mazes with multiple paths.

**Step 6: Remove dead ends**
Repeatedly fill in tiles with walls on 3 sides until no dead ends remain. This prunes maze passages down to only those connecting rooms.

### Strengths

- **Guaranteed connectivity**: Every room is reachable
- **Organic hallways**: Maze-derived corridors feel less artificial than L-shaped connectors
- **Tunable loop frequency**: The imperfection percentage controls alternate paths
- **Dead end control**: Can leave some or remove all

### Weaknesses

- **Maze feel**: Results feel like mazes with rooms, not buildings with hallways
- **Single floor only**
- **Grid-locked**: All rooms are rectangular, axis-aligned
- **No room function**: Rooms are undifferentiated

### Relevance to Monolith

The connector/spanning-tree approach from Steps 3-4 is an excellent **door placement algorithm** that could work with any room layout method. The maze fill in Step 2 could generate creepy interstitial spaces between rooms -- perfect for horror.

### Key Sources

- [Bob Nystrom: "Rooms and Mazes: A Procedural Dungeon Generator"](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/)

---

## 9. Squarified Treemap Floor Plans

### Overview

Marson & Musse (2010) applied the Squarified Treemap visualization algorithm to floor plan generation. The algorithm subdivides a rectangle into sub-rectangles with target areas while minimizing aspect ratios (keeping rooms square-ish).

### Algorithm

**Input:** Building rectangle, hierarchical room tree with area targets

```
function squarified_layout(rooms, rect):
    if len(rooms) == 1:
        assign rooms[0] to rect
        return

    # Sort rooms by area (descending)
    rooms.sort(by=area, descending)

    # Try adding rooms to a row along the shorter side
    row = []
    remaining = rooms

    while remaining:
        # Add next room to current row
        candidate_row = row + [remaining[0]]

        # Calculate aspect ratios with candidate
        if worst_aspect_ratio(candidate_row, rect) <= worst_aspect_ratio(row, rect):
            row = candidate_row
            remaining = remaining[1:]
        else:
            # Finalize current row, start new one
            layout_row(row, rect)
            rect = remaining_rect(rect, row)
            row = []

    layout_row(row, rect)
```

**Two-pass approach:**
1. First pass: Divide building into 3 zones (social, service, private) using treemap
2. Second pass: Within each zone, subdivide into individual rooms

### Strengths

- **Zero waste**: All space is allocated (no corridors by default)
- **Realistic proportions**: Minimized aspect ratios produce usable room shapes
- **Real-time**: Very fast -- suitable for on-the-fly generation
- **Hierarchical**: Natural support for zone > room decomposition

### Weaknesses

- **No corridors**: Must be added as a post-process
- **Too efficient**: Real buildings have wasted space, closets, mechanical rooms
- **All rectangular**: No L-shapes or irregular rooms
- **Adjacency is emergent, not controlled**: You get whatever adjacencies the treemap produces

### Relevance to Monolith

Treemap is an excellent **spatial layout solver** for the graph-based approach. Given a room list with target areas and a building footprint, treemap produces a valid non-overlapping layout quickly. Use it after defining the connectivity graph, then verify/adjust adjacencies.

### Key Sources

- [Marson & Musse "Automatic Real-Time Generation of Floor Plans Based on Squarified Treemaps" (2010)](https://onlinelibrary.wiley.com/doi/10.1155/2010/624817)

---

## 10. Constrained Growth Method

### Overview

Lopes, Bidarra et al. (2010, TU Delft) proposed a method where rooms grow from seed positions on a grid, expanding outward until they reach target sizes or collide with other rooms.

### Algorithm

1. **Grid initialization**: Divide floor into uniform grid cells
2. **Seed placement**: Place initial seed cells for each room at positions influenced by the connectivity graph (adjacent rooms get nearby seeds)
3. **Growth phase**: Each room iteratively claims adjacent unclaimed cells, prioritizing growth toward required neighbors
4. **Constraint enforcement**: Growth is blocked when:
   - Room reaches target area
   - Cell is already claimed by another room
   - Growth would violate minimum dimension constraints
   - Growth would break required adjacency
5. **Door placement**: Place doors on shared walls between rooms that are connected in the adjacency graph
6. **Corridor detection**: If connected rooms don't share a wall, a corridor room is inserted

### Strengths

- **User-controllable**: Designers specify the connectivity graph and room priorities
- **Irregular rooms**: Growth can produce L-shapes and non-rectangular rooms
- **Adjacency-aware**: Growth directions are influenced by required connections
- **Handles complex footprints**: Works with non-rectangular building outlines

### Weaknesses

- **Order-dependent**: Room growth order affects results significantly
- **Can fail**: Growth can deadlock if constraints conflict
- **Slow convergence**: Many iterations needed for large floor plans

### Relevance to Monolith

The constrained growth method is a strong candidate for **non-rectangular buildings** (L-shapes, T-shapes). It handles irregular footprints better than treemaps. Could be combined with our existing `create_structure` shape types.

### Key Sources

- [Lopes et al. "A Constrained Growth Method for Procedural Floor Plan Generation" (GAME-ON 2010)](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)

---

## 11. Real-World Building Archetypes

### Room Type Definitions and Typical Sizes

For procedural generation, each building archetype needs a catalog of room types with size ranges, connectivity rules, and required counts.

### Residential House

| Room Type | Min Size (m) | Max Size (m) | Typical Size (m) | Required | Exterior Wall |
|-----------|-------------|-------------|------------------|----------|---------------|
| Living Room | 4.0 x 4.5 | 6.0 x 9.0 | 5.0 x 6.0 | Yes | Preferred |
| Kitchen | 3.0 x 3.0 | 4.5 x 6.0 | 3.5 x 4.0 | Yes | Preferred |
| Dining Room | 3.0 x 3.0 | 4.5 x 5.5 | 3.5 x 4.0 | No | Preferred |
| Master Bedroom | 4.0 x 4.0 | 5.0 x 6.5 | 4.5 x 5.0 | Yes | Required |
| Secondary Bedroom | 2.5 x 3.0 | 4.0 x 4.5 | 3.0 x 3.5 | 1-3 | Preferred |
| Bathroom | 1.5 x 2.0 | 3.0 x 3.5 | 2.0 x 2.5 | 1-2 | No |
| Master Bathroom | 2.0 x 2.5 | 3.0 x 4.0 | 2.5 x 3.0 | No | No |
| Hallway | 1.2 x 3.0 | 1.5 x 10.0 | 1.2 x 5.0 | Yes | No |
| Closet | 0.8 x 1.0 | 1.5 x 2.0 | 1.0 x 1.5 | 0-3 | No |
| Laundry | 1.5 x 1.5 | 2.5 x 3.0 | 2.0 x 2.0 | No | No |
| Garage | 3.0 x 5.5 | 6.0 x 7.0 | 5.5 x 6.0 | No | Required |
| Entryway/Foyer | 1.5 x 1.5 | 3.0 x 3.0 | 2.0 x 2.0 | Yes | Required |

**Connectivity graph:**
```
Exterior --> Entryway --> Hallway
Hallway --> Living Room
Hallway --> Kitchen --> Dining Room
Hallway --> Bedroom(s) --> Closet
Hallway --> Bathroom
Master Bedroom --> Master Bathroom
Master Bedroom --> Walk-in Closet
Kitchen --> Laundry (optional)
Garage --> Kitchen or Hallway
```

**Constraints:**
- Bedrooms must NOT connect directly to other bedrooms
- Every bedroom must reach a bathroom without going through another bedroom
- Kitchen connects to dining (or is open-plan with living)
- Entryway is the sole exterior entrance

### Abandoned Hospital / Asylum (Horror Classic)

Based on Kirkbride Plan asylums and modern hospital layouts:

| Room Type | Min Size (m) | Max Size (m) | Count | Notes |
|-----------|-------------|-------------|-------|-------|
| Ward (patient room) | 3.0 x 3.0 | 4.0 x 5.0 | 4-12 | Along corridor, exterior wall |
| Operating Theater | 5.0 x 5.0 | 8.0 x 8.0 | 1-2 | Central, no windows |
| Morgue | 4.0 x 5.0 | 6.0 x 8.0 | 0-1 | Basement preferred |
| Padded Cell / Seclusion | 2.5 x 2.5 | 3.5 x 3.5 | 0-4 | Interior, heavy door |
| Nurses Station | 2.0 x 3.0 | 3.0 x 4.0 | 1-2 | Central to wards |
| Storage / Supply | 2.0 x 2.0 | 3.0 x 4.0 | 2-6 | Interior |
| Main Corridor | 2.5 x 10+ | 3.5 x 30+ | 1+ | Central spine |
| Reception / Lobby | 4.0 x 4.0 | 8.0 x 8.0 | 1 | Near entrance |
| Office / Admin | 3.0 x 3.0 | 4.0 x 5.0 | 1-4 | Near reception |
| Kitchen / Cafeteria | 5.0 x 5.0 | 8.0 x 10.0 | 1 | Service area |
| Bathroom (communal) | 3.0 x 4.0 | 5.0 x 6.0 | 1-2 | Near wards |
| Laundry | 3.0 x 4.0 | 5.0 x 6.0 | 1 | Service area |
| Chapel / Common Room | 4.0 x 5.0 | 6.0 x 8.0 | 0-1 | Social area |
| Stairwell | 2.5 x 3.0 | 3.5 x 4.0 | 1-2 | Vertical access |
| Mechanical / Boiler | 3.0 x 4.0 | 5.0 x 6.0 | 1 | Basement |

**Layout pattern (Kirkbride):**
- Central administrative building
- Symmetrical wings extending outward in staggered pattern
- Each wing = one ward
- Corridor spine runs through all wings
- Service areas (kitchen, laundry) at rear or basement

### Police Station

| Room Type | Min Size (m) | Max Size (m) | Count | Notes |
|-----------|-------------|-------------|-------|-------|
| Lobby / Reception | 4.0 x 5.0 | 6.0 x 8.0 | 1 | Public-facing |
| Dispatch / Control | 3.0 x 4.0 | 5.0 x 6.0 | 1 | Secured |
| Open Office / Bullpen | 6.0 x 8.0 | 10.0 x 12.0 | 1 | Desks for officers |
| Chief's Office | 3.5 x 4.0 | 5.0 x 5.0 | 1 | Private |
| Interrogation Room | 2.5 x 3.0 | 3.5 x 4.0 | 1-3 | Soundproofed, no windows |
| Holding Cell | 2.0 x 2.5 | 3.0 x 3.5 | 2-6 | Near sally port |
| Evidence Room | 3.0 x 4.0 | 5.0 x 8.0 | 1 | Secured, no public access |
| Booking / Processing | 3.0 x 4.0 | 5.0 x 6.0 | 1 | Adjacent to cells |
| Locker Room | 3.0 x 4.0 | 4.0 x 6.0 | 1-2 | Staff only |
| Break Room | 3.0 x 3.0 | 4.0 x 5.0 | 1 | Staff only |
| Armory | 2.0 x 3.0 | 3.0 x 4.0 | 1 | Heavily secured |
| Bathroom | 2.0 x 2.5 | 3.0 x 4.0 | 2+ | Public + staff |
| Corridor | 2.0 x varies | 2.5 x varies | 1+ | Main circulation |
| Sally Port / Garage | 4.0 x 6.0 | 6.0 x 8.0 | 1 | Vehicle entrance |

**Zoning**: Public zone (lobby, waiting) -> Secure zone (offices, dispatch) -> Restricted zone (cells, evidence, armory). Access control between zones.

### Mansion (Victorian Horror)

| Room Type | Min Size (m) | Max Size (m) | Count | Notes |
|-----------|-------------|-------------|-------|-------|
| Grand Foyer | 5.0 x 5.0 | 8.0 x 10.0 | 1 | Double-height, main entrance |
| Grand Staircase | 3.0 x 5.0 | 5.0 x 8.0 | 1 | Central, ornate |
| Drawing Room / Parlor | 4.0 x 5.0 | 6.0 x 8.0 | 1-2 | Social rooms |
| Dining Room | 4.0 x 5.0 | 6.0 x 8.0 | 1 | Formal |
| Library / Study | 3.5 x 4.0 | 5.0 x 6.0 | 1 | Books, desk |
| Kitchen | 4.0 x 4.0 | 6.0 x 7.0 | 1 | Service wing |
| Servants Quarters | 2.5 x 3.0 | 3.5 x 4.0 | 1-4 | Service wing or attic |
| Master Suite | 5.0 x 5.0 | 7.0 x 8.0 | 1 | Upper floor |
| Guest Bedroom | 3.5 x 4.0 | 5.0 x 6.0 | 2-4 | Upper floor |
| Bathroom | 2.0 x 2.5 | 3.5 x 4.0 | 2-4 | |
| Wine Cellar | 3.0 x 4.0 | 5.0 x 6.0 | 0-1 | Basement |
| Billiard Room | 4.0 x 5.0 | 5.0 x 7.0 | 0-1 | |
| Conservatory | 3.5 x 4.0 | 5.0 x 6.0 | 0-1 | Glass walls, exterior |
| Secret Room | 2.0 x 2.0 | 3.0 x 3.0 | 0-2 | Hidden behind bookcase etc. |
| Attic | Full floor | | 0-1 | Low ceiling, irregular |
| Basement | Full floor | | 0-1 | Mechanical + storage |

**Layout pattern:**
- Ground floor: Public rooms (foyer, parlor, dining, library) around central hall
- Service wing: Kitchen, servants quarters, back entrance
- Upper floor(s): Bedrooms off a central hallway, master suite at end
- Secret passages connecting otherwise-unrelated rooms (horror element)

---

## 12. The Connected Rooms Approach

### Core Concept

Build buildings by composing rooms as the primary unit -- each room is a self-contained box with walls, floor, and ceiling. Rooms connect through shared walls with aligned doors. This is the approach most natural for Monolith since `create_structure` already generates individual rooms.

### Algorithm: Room-by-Room Assembly

**Step 1: Generate building spec**
```json
{
    "archetype": "residential_house",
    "footprint": {"width": 15.0, "depth": 12.0},
    "floors": 2,
    "floor_height": 3.0,
    "rooms": [
        {"type": "entryway", "floor": 0, "area": 4.0},
        {"type": "hallway", "floor": 0, "area": 6.0},
        {"type": "living_room", "floor": 0, "area": 25.0},
        {"type": "kitchen", "floor": 0, "area": 14.0},
        ...
    ],
    "connections": [
        {"from": "entryway", "to": "hallway", "type": "door"},
        {"from": "hallway", "to": "living_room", "type": "opening"},
        {"from": "hallway", "to": "kitchen", "type": "door"},
        {"from": "hallway", "to": "stairwell", "type": "opening"},
        ...
    ]
}
```

**Step 2: Layout rooms (choose algorithm)**
Use BSP, treemap, or constrained growth to assign each room a rectangular region within the footprint. Output: position and dimensions for each room.

**Step 3: Calculate adjacencies**
For each pair of rooms, determine if they share a wall segment:
```
function find_shared_wall(roomA, roomB):
    # Check all 4 sides of roomA against all 4 sides of roomB
    for each edge of roomA:
        for each edge of roomB:
            if edges are collinear and overlapping:
                overlap_start = max(edgeA.start, edgeB.start)
                overlap_end = min(edgeA.end, edgeB.end)
                if overlap_end - overlap_start >= MIN_DOOR_WIDTH:
                    return SharedWall(overlap_start, overlap_end, normal)
    return None
```

**Step 4: Place doors on shared walls**
For each required connection, find the shared wall and place a door:
```
function place_door(shared_wall, door_width=1.0):
    available_length = shared_wall.length
    if available_length < door_width:
        ERROR: rooms don't share enough wall for a door

    # Place door centered, or at random position with margins
    margin = 0.3  # 30cm from corners
    min_pos = shared_wall.start + margin
    max_pos = shared_wall.end - margin - door_width
    door_pos = random(min_pos, max_pos)
    return Door(door_pos, door_width)
```

**Step 5: Place windows on exterior walls**
For each room with exterior wall segments, place windows based on room type:
```
function place_windows(room, exterior_walls):
    if room.type in ["bathroom", "closet", "storage"]:
        return []  # No windows

    windows = []
    for wall in exterior_walls:
        window_count = floor(wall.length / WINDOW_SPACING)
        for i in range(window_count):
            pos = wall.start + (i + 0.5) * (wall.length / window_count)
            windows.append(Window(pos, WINDOW_WIDTH, WINDOW_HEIGHT, SILL_HEIGHT))
    return windows
```

**Step 6: Handle multi-story**
- Duplicate floor layout for each story (with modifications per floor)
- Place stairwell rooms at the same position on every floor
- Add floor/ceiling geometry between stories
- Stairs connect through openings in floor/ceiling at stairwell positions

**Step 7: Generate geometry**
For each room, call `create_structure` with:
- Room shape (rect, L, T)
- Dimensions
- Wall positions (with door/window cutouts)
- Floor and ceiling

### Shared Wall Management

The critical challenge is avoiding double-walls (two rooms each generating their own wall, resulting in a 2x thick wall between them).

**Solutions:**

1. **One-sided walls**: Each room generates only its "owned" walls (e.g., north and west). The neighbor provides south and east. Requires careful ownership assignment.

2. **Full walls with boolean subtraction**: Each room generates all 4 walls. Where rooms share a wall, one room's wall is deleted and replaced by the neighbor's. Simpler but wastes geometry.

3. **Thin-wall approach** (already researched for Monolith): Use `AppendSimpleSweptPolygon` to sweep a wall-thickness rectangle along the room perimeter. Adjacent rooms share the wall polygon edge, producing exactly one wall.

4. **Grid-based walls**: Define walls on a grid. Each grid edge gets exactly one wall object. Rooms reference grid edges rather than owning walls. This is the cleanest approach for connected buildings.

### Door Alignment Guarantees

When using BSP or treemap layouts, rooms naturally share axis-aligned edges. Door alignment is guaranteed as long as:
- Both rooms use the same grid
- The shared edge is long enough for a door (>= 1.0m)
- Door position is computed from the shared edge, not from room-local coordinates

For non-grid-based layouts (constrained growth, physics separation), alignment requires explicit snapping:
```
function snap_to_neighbor(roomA, roomB, required_connection):
    shared = find_shared_wall(roomA, roomB)
    if shared is None:
        # Rooms don't share a wall -- extend one room
        gap = distance_between_nearest_edges(roomA, roomB)
        if gap < MAX_CORRIDOR_LENGTH:
            insert_corridor(roomA, roomB, gap)
        else:
            # Shift room position to create adjacency
            shift_room_toward(roomA, roomB)
```

---

## 13. Horror-Specific Architectural Design

### Principles for Procedural Horror Buildings

**Spatial manipulation for dread:**
- Hallways that are slightly too long
- Rooms that feel "off" in proportions (2.5m ceiling in a 6m wide room)
- Dead ends that force backtracking (player knows something is behind them)
- Blocked sightlines around every corner

**Impossible geometry:**
- Corridors that loop back on themselves without the player realizing
- Rooms that are larger on the inside than the outside suggests
- Staircases that go down but arrive at a higher floor (subtle disorientation)

**Pacing through architecture:**
- Alternate between tight spaces (corridors, closets) and open spaces (lobbies, wards)
- Vertical drops and exposed balconies trigger primal vulnerability
- Basement/attic spaces exploit claustrophobia and fear of the unknown

**Environmental storytelling:**
- Room condition varies (some pristine, some destroyed)
- Evidence of events (blood trails leading between rooms, barricaded doors)
- Progressive decay deeper into the building

### Procedural Parameters for Horror

```json
{
    "horror_params": {
        "corridor_length_multiplier": 1.3,     // 30% longer than normal
        "dead_end_frequency": 0.25,             // 25% of corridors are dead ends
        "sightline_max_distance": 8.0,          // Max unobstructed view distance (meters)
        "room_proportion_variance": 0.15,       // 15% deviation from normal proportions
        "secret_room_chance": 0.1,              // 10% chance of hidden room per building
        "loop_frequency": 0.3,                  // 30% of corridors form loops
        "symmetry_breaking": 0.2,               // 20% chance to break expected symmetry
        "window_reduction": 0.4,                // 40% fewer windows than normal
        "lighting_zone_count": 3,               // Number of distinct lighting zones
        "barricade_frequency": 0.15             // 15% of doors are barricaded/blocked
    }
}
```

### The "Shadows of Doubt" Approach (Case Study)

Shadows of Doubt (ColePowered Games) uses a hybrid design-procedural system worth studying:

**Hierarchy:** City > District > Block > Building > Floor > Address > Room > Tile (1.8m x 1.8m tiles)

**Key insights:**
- Designers create **floor template layouts** defining zones ("addresses") -- procedural generation handles rooms within zones
- Room placement uses a **priority system**: most important rooms placed first (living room, then bathroom, etc.)
- Rooms ranked by 3 criteria: floor space, uniform shape (fewer corners), and window access
- **Hallway generation is automatic**: If entrance is too far from room cluster, hallways are inserted
- **Space stealing**: Later rooms can claim excess space from earlier rooms
- **Connectivity rules**: Bathrooms single-door only; kitchens connect directly to living rooms; entrance doors never open into bedrooms

This is directly applicable to our approach -- template-driven zone layouts with procedural room filling.

---

## 14. Existing Implementations

### Open Source / Free

| Project | Language | Algorithm | Building Type | URL |
|---------|----------|-----------|---------------|-----|
| bendemott/UE5-Procedural-Building | C++ (GeometryScript) | Grid-based box composition | Exterior shells | [GitHub](https://github.com/bendemott/UE5-Procedural-Building) |
| BenPyton/ProceduralDungeon | C++ (UE5) | Template rooms, random connection | Dungeons | [GitHub](https://github.com/BenPyton/ProceduralDungeon) |
| shun126/DungeonGenerator | C++ (UE5) | BSP + mission graph | Dungeons with interiors | [GitHub](https://github.com/shun126/DungeonGenerator) |
| SyMbolzz/Triangulation-Dungeon-Generator-UE5 | UE5 | Delaunay+MST | Dungeon rooms | [GitHub](https://github.com/SyMbolzz/Triangulation-Dungeon-Generator-UE5) |
| sajarin/RanDun | C/C++ | BSP, cellular automata, Nystrom | Roguelike dungeons | [GitHub](https://github.com/sajarin/RanDun) |
| halftheopposite/bsp-dungeon-generator | TypeScript | BSP + hand-made rooms | 2D dungeons | [GitHub](https://github.com/halftheopposite/bsp-dungeon-generator) |
| mxgmn/WaveFunctionCollapse | C# | WFC | Tilemaps/textures | [GitHub](https://github.com/mxgmn/WaveFunctionCollapse) |
| watabou/Procgen Mansion | Unknown | Proprietary (roof stitching) | Mansion floor plans | [itch.io](https://watabou.itch.io/procgen-mansion) |
| Zeraphil/NystromGenerator | C# (Unity) | Nystrom rooms+mazes | 2D dungeons | [GitHub](https://github.com/zeraphil/NystromGenerator) |

### UE Marketplace / Commercial

| Product | Approach | Interiors? | Notes |
|---------|----------|------------|-------|
| Procedural Building Generator (Marketplace) | Spline-based + mesh instancing | No (shells only) | Blueprint-only |
| Procedural Building (Code Plugin) | Modular mesh composition | Partial | Exterior + some interior |
| Dungeon Architect (Marketplace) | Multi-algorithm (BSP, grid, etc.) | Yes | Full dungeon system, most mature |

### Academic / Research

| Paper | Year | Algorithm | Key Contribution |
|-------|------|-----------|------------------|
| Mueller et al. "Procedural Modeling of Buildings" | 2006 | CGA Shape Grammar | Foundational facade generation |
| Marson & Musse "Squarified Treemaps for Floor Plans" | 2010 | Treemap | Space-efficient room layout |
| Lopes & Bidarra "Constrained Growth Method" | 2010 | Seed growth | Irregular room shapes from constraints |
| Lopes et al. "Real-time Floor Plan Generation" | 2012 | Graph+Treemap+Corridors | Most complete floor plan pipeline |
| Freiknecht & Effelsberg "Multi-Story Buildings with Interior" | 2019 | Bottom-up room assembly | First full multi-story algorithm |
| Dormans "Cyclic Dungeon Generation" | 2017 | Graph rewriting cycles | Gameplay-driven level structure |

---

## 15. Algorithm Comparison Matrix

| Criterion | BSP | WFC | Graph+Treemap | Constrained Growth | Nystrom | Delaunay+MST | CGA | Cyclic |
|-----------|-----|-----|---------------|--------------------|---------|--------------| ----|--------|
| **Room type control** | Low | Medium | High | High | Low | Low | Low | Medium |
| **Adjacency control** | None | Local | Full | Partial | None | None | None | Full |
| **Non-rectangular rooms** | No | With tiles | No | Yes | No | No | No | No |
| **Multi-story** | Manual | Manual | Natural | Manual | No | No | Natural | Manual |
| **Corridors** | L-shaped | Tile-based | Optimized | Grown | Maze-based | L-shaped | N/A | Graph-derived |
| **Speed** | Fast | Medium | Fast | Slow | Medium | Medium | Fast | Medium |
| **Architectural realism** | Low | Low | High | Medium | Low | Low | High (facade) | Medium |
| **Horror suitability** | Medium | Medium | High | Medium | High (mazes) | Medium | Low | Highest |
| **Implementation complexity** | Simple | Medium | Medium | Complex | Simple | Medium | Complex | Complex |
| **Building footprint** | Rectangle | Grid | Rectangle | Any | Rectangle | Circle | Any | Grid |
| **Guaranteed connectivity** | Yes | No* | Yes | Yes | Yes | Yes | N/A | Yes |

\* WFC requires additional path constraints for connectivity guarantee

---

## 16. Recommended Architecture for Monolith

### Proposed System: Hybrid Graph + Layout + Assembly

Based on this research, the recommended approach for `create_building` (or `generate_building`) is a **4-phase pipeline**:

```
Phase 1: SPECIFY      Phase 2: LAYOUT         Phase 3: CONNECT       Phase 4: GENERATE
(What rooms?)         (Where do they go?)     (How do they connect?) (Build geometry)

Archetype Template -> Room Layout Solver  ->  Door/Window Placer ->  create_structure calls
 + Random Params      (BSP or Treemap)        + Corridor Solver      + create_parametric_mesh
                                              + Stairwell Placer     + horror prop placement
```

### Phase 1: Building Specification

**Input:** Archetype name + seed + optional overrides

```json
{
    "action": "generate_building",
    "params": {
        "archetype": "abandoned_hospital",
        "seed": 42,
        "footprint": {"width": 30, "depth": 20},
        "floors": 2,
        "floor_height": 3.2,
        "horror_level": 0.7,
        "overrides": {
            "extra_rooms": [{"type": "secret_room", "connected_to": "morgue"}],
            "force_connections": [{"from": "lobby", "to": "operating_theater", "type": "locked_door"}]
        }
    }
}
```

**Process:**
1. Load archetype template (JSON data: room types, counts, size ranges, connectivity graph)
2. Roll room count variations from distributions
3. Assign room areas from type-specific ranges
4. Build the connectivity graph (required + optional connections)
5. Apply horror modifiers (extra dead ends, secret rooms, barricaded doors)

### Phase 2: Spatial Layout

**Primary algorithm: Squarified Treemap** (for rectangular footprints)
- Input: Room list with target areas + building footprint
- Output: Position and dimensions for each room

**Fallback: BSP** (for when treemap produces poor adjacencies)
- Input: Same
- Output: Same but with different room arrangement

**For non-rectangular footprints: Constrained Growth**
- Input: Room seeds placed within irregular footprint
- Output: Irregular room shapes filling the footprint

**Multi-story handling:**
- Generate each floor independently
- Stairwell/elevator rooms placed at consistent positions across floors
- Ground floor and upper floors use different room priority lists

### Phase 3: Connection Resolution

1. **Verify adjacencies**: Check that all required connections have a shared wall
2. **Insert corridors**: Where connected rooms don't share walls, insert corridor rooms
3. **Place doors**: On each shared wall for connected rooms, place appropriately-sized doors
4. **Place windows**: On exterior walls, respecting room type rules
5. **Apply horror modifications**:
   - Barricade some doors (player must find alternate route)
   - Add dead-end corridors
   - Insert secret passages between non-adjacent rooms
   - Break symmetry randomly

### Phase 4: Geometry Generation

For each room, emit a `create_structure` call:
```json
{
    "shape": "rect",
    "width": 5.0,
    "depth": 4.0,
    "height": 3.2,
    "wall_thickness": 0.2,
    "openings": [
        {"wall": "north", "type": "door", "position": 2.0, "width": 1.0, "height": 2.1},
        {"wall": "east", "type": "window", "position": 1.5, "width": 1.2, "height": 1.5, "sill": 0.9}
    ]
}
```

Then furnish with `create_parametric_mesh`:
- Room type determines furniture set (bedroom -> bed + desk + closet)
- Horror level determines prop density (barricades, debris, broken items)

### Data Architecture

**Archetype definitions** stored as JSON files in `Content/Monolith/BuildingArchetypes/`:
```
residential_house.json
abandoned_hospital.json
police_station.json
victorian_mansion.json
apartment_building.json
warehouse.json
school.json
church.json
```

Each archetype contains:
- `room_types[]`: Room definitions with size ranges and properties
- `connectivity_graph`: Required and optional room connections
- `floor_templates[]`: Per-floor room priority lists
- `style`: Facade/window/door style rules (CGA-lite)
- `horror_modifiers`: Horror-specific adjustments

### Implementation Priority

| Priority | Component | Effort | Depends On |
|----------|-----------|--------|------------|
| P0 | Archetype JSON schema + 3 archetypes (house, hospital, station) | 8h | Nothing |
| P0 | Squarified Treemap layout solver | 12h | JSON schema |
| P0 | Adjacency verification + door placement | 8h | Layout solver |
| P1 | Corridor insertion algorithm | 10h | Adjacency |
| P1 | Window placement algorithm | 4h | Adjacency |
| P1 | Multi-story support (stairwell handling) | 12h | Layout solver |
| P1 | BSP layout solver (alternative) | 8h | JSON schema |
| P2 | Horror modifiers (dead ends, secrets, barricades) | 12h | Corridors |
| P2 | Auto-furnishing via create_parametric_mesh | 16h | Full pipeline |
| P2 | Constrained growth solver (non-rect footprints) | 16h | JSON schema |
| P3 | Cyclic structure overlay (gameplay pacing) | 20h | Full pipeline |
| P3 | CGA-style facade detailing | 16h | Full pipeline |
| P3 | WFC interior detailing pass | 20h | Full pipeline |

**Total estimated effort: ~162h for full system, ~28h for MVP (P0 only)**

### Integration with Existing Monolith

The new `generate_building` action would:
1. Accept the spec JSON as params
2. Run Phases 1-3 in C++ (pure algorithms, no UE dependency for layout)
3. Call existing `create_structure` internally for each room (Phase 4)
4. Call existing `create_parametric_mesh` for furniture
5. Call existing `create_horror_prop` for horror elements
6. Return the complete building as a set of static mesh actors in a folder

This leverages everything already built in MonolithMesh Phases 19A-D without reinventing geometry generation.

---

## Sources

### Papers
- Mueller et al. "Procedural Modeling of Buildings" (SIGGRAPH 2006) -- [ACM](https://dl.acm.org/doi/10.1145/1141911.1141931)
- Lopes et al. "A Novel Algorithm for Real-time Procedural Generation of Building Floor Plans" (2012) -- [arXiv](https://ar5iv.labs.arxiv.org/html/1211.5842)
- Marson & Musse "Automatic Real-Time Generation of Floor Plans Based on Squarified Treemaps" (2010) -- [Wiley](https://onlinelibrary.wiley.com/doi/10.1155/2010/624817)
- Lopes & Bidarra "A Constrained Growth Method for Procedural Floor Plan Generation" (2010) -- [TU Delft](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)
- Freiknecht & Effelsberg "Procedural Generation of Multistory Buildings with Interior" (2019) -- [IEEE](https://ieeexplore.ieee.org/document/8926482/)

### Blog Posts and Tutorials
- Bob Nystrom: ["Rooms and Mazes: A Procedural Dungeon Generator"](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/)
- Boris the Brave: ["Wave Function Collapse Tips and Tricks"](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- Boris the Brave: ["Dungeon Generation in Unexplored"](https://www.boristhebrave.com/2021/04/10/dungeon-generation-in-unexplored/)
- eskerda.com: ["BSP Dungeon Generation"](https://eskerda.com/bsp-dungeon-generation/)
- ColePowered Games: ["Shadows of Doubt DevBlog 13: Creating Procedural Interiors"](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- Gamasutra: ["Procedural Dungeon Generation Algorithm" (TinyKeep)](https://www.gamedeveloper.com/programming/procedural-dungeon-generation-algorithm)
- Gamasutra: ["Procedural Dynamic Room Generation in ARE"](https://www.gamedeveloper.com/programming/procedural-dynamic-room-generation-in-quot-are-quot-)
- Dr Wedge: ["Horror Game Level Design: Technical Drawing To Trigger Fear"](https://drwedge.uk/2025/06/02/horror-game-level-design/)
- pvigier: ["Room Generation using Constraint Satisfaction"](https://pvigier.github.io/2022/11/05/room-generation-using-constraint-satisfaction.html)
- Sam Aston: ["Wave Function Collapse & Plan Adjacencies"](https://www.samuelaston.com/wave-function-collapse-plan-adjacencies/)
- Future Data Lab: ["Procedural Dungeon Generator"](https://slsdo.github.io/procedural-dungeon/)

### GDC Talks
- ["Building Blocks: Artist Driven Procedural Buildings"](https://gdcvault.com/play/1012655/Building-Blocks-Artist-Driven-Procedural)
- ["Constructing the Catacombs: Procedural Architecture for Platformers"](https://gdcvault.com/play/1021877/Constructing-the-Catacombs-Procedural-Architecture)
- ["Practical Procedural Generation for Everyone"](https://www.gdcvault.com/play/1024213/Practical-Procedural-Generation-for)
- ["Unexplored's Secret: Cyclic Dungeon Generation"](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-)

### Open Source
- [mxgmn/WaveFunctionCollapse](https://github.com/mxgmn/WaveFunctionCollapse)
- [bendemott/UE5-Procedural-Building](https://github.com/bendemott/UE5-Procedural-Building)
- [BenPyton/ProceduralDungeon](https://github.com/BenPyton/ProceduralDungeon)
- [shun126/DungeonGenerator](https://github.com/shun126/DungeonGenerator)
- [sajarin/RanDun](https://github.com/sajarin/RanDun)
- [RogueBasin BSP Tutorial](https://www.roguebasin.com/index.php/Basic_BSP_Dungeon_generation)
- [Bracketproductions Roguelike Tutorial (Rust)](https://bfnightly.bracketproductions.com/)

### Architecture References
- [Kirkbride Plan (Wikipedia)](https://en.wikipedia.org/wiki/Kirkbride_Plan)
- [WBDG Outpatient Clinic Design Guide](https://www.wbdg.org/building-types/health-care-facilities/outpatient-clinic)
- [CGA Reference (ArcGIS CityEngine)](https://doc.arcgis.com/en/cityengine/latest/help/help-component-split.htm)
- [PSU GEOG 497 CGA Tutorial](https://www.e-education.psu.edu/geogvr/node/891)

### Horror Design
- ["Creating Horror through Level Design" (Gamasutra)](https://www.gamedeveloper.com/design/creating-horror-through-level-design-tension-jump-scares-and-chase-sequences)
- ["3 Level Design Horror Principles" (World of Level Design)](https://www.worldofleveldesign.com/categories/level_design_tutorials/alan-wake-horror.php)
- ["Blueprints of Dread: How Architecture Shapes Horror"](https://www.bryanwalaspa.com/post/blueprints-of-dread-how-architecture-shapes-horror-and-makes-buildings-feel-like-monsters)

### UE5 PCG
- [PCG Framework Documentation (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [Antonios Liapis: "Constructive Generation Methods for Dungeons and Levels"](https://antoniosliapis.com/articles/pcgbook_dungeons.php)
