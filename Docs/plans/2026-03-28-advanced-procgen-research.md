# Advanced Procedural Building Generation: State-of-the-Art Research (2024-2026)

**Date:** 2026-03-28
**Purpose:** Deep research into the latest algorithms, techniques, and commercial systems for high-quality procedural building generation with playable interiors. Focused on what could improve Monolith's existing proc-gen pipeline for Leviathan (FPS survival horror, UE 5.7).
**Context:** Monolith already has `create_structure`, `create_building_shell`, `create_maze`, connected room assembly, facade generation, and 14+ parametric furniture/horror prop builders. This research covers what's new and what we should adopt.

---

## Table of Contents

1. [Constraint-Based Generation](#1-constraint-based-generation)
2. [WaveFunctionCollapse and Variants for Buildings](#2-wavefunctioncollapse-and-variants-for-buildings)
3. [Graph-Based Room Generation Improvements](#3-graph-based-room-generation-improvements)
4. [Commercial Proc-Gen Building Systems (2024-2026)](#4-commercial-proc-gen-building-systems-2024-2026)
5. [Academic Papers (2024-2025)](#5-academic-papers-2024-2025)
6. [Specific Algorithms We Could Adopt](#6-specific-algorithms-we-could-adopt)
7. [Horror-Specific Procedural Generation](#7-horror-specific-procedural-generation)
8. [Synthesis: Recommendations for Monolith](#8-synthesis-recommendations-for-monolith)

---

## 1. Constraint-Based Generation

### 1.1 Answer Set Programming (ASP) for Room Layout Constraints

**What it is:** ASP is a declarative logic programming paradigm where you specify *what* a valid solution looks like, and a solver (typically Clingo) finds all configurations satisfying those constraints. Unlike imperative generation where you build and validate, ASP generates only valid solutions by construction.

**How it works for level generation:**
- Declare variables: room positions, sizes, types on a grid
- Declare constraints: rooms must not overlap, every room must be reachable from the entrance, bathrooms must be adjacent to at least one bedroom, etc.
- The ASP solver explores the combinatorial space and returns valid configurations

**Expressing "every room must be reachable":**
```prolog
% A room is reachable if it connects to the entrance
reachable(entrance).
% A room is reachable if it has a door to a reachable room
reachable(R) :- door(R, R2), reachable(R2).
% Constraint: every room must be reachable
:- room(R), not reachable(R).
```

This is elegantly simple in ASP but would require complex validation logic in imperative code. The solver guarantees the constraint is satisfied -- no post-hoc validation needed.

**Performance:** Smith & Mateas (2011) report average generation times of ~12 seconds for ASP-based level generation. This is too slow for real-time but fine for editor-time building generation where we'd invoke it from an MCP action.

**Key limitation:** ASP works on discrete domains. Room positions must snap to a grid. Continuous optimization (exact room sizes, smooth wall positions) requires a different approach. The typical pattern is: ASP for topology and connectivity -> continuous solver for exact geometry.

**Relevance to Monolith:** High for topology validation. We could use ASP as a constraint checker or generator for room adjacency graphs before committing to geometry. Clingo has C++ bindings and is lightweight to embed.

**Key sources:**
- [Smith & Mateas, "ASP for PCG" (2011)](https://adamsmith.as/papers/tciaig-asp4pcg.pdf)
- [ASP with Applications to Mazes and Levels](https://link.springer.com/chapter/10.1007/978-3-319-42716-4_8)
- [Clingo solver (Potassco)](https://github.com/potassco/clingo)
- [ASP Level Generation Examples (PCG Book)](https://www.kmjn.org/notes/pcgbook_asp_examples.html)

### 1.2 Constraint Satisfaction Problems (CSP) for Architectural Rules

**What it is:** CSP is a broader framework than ASP. Variables have domains, constraints restrict combinations. Solvers use constraint propagation (arc consistency, AC-3) to prune impossible values before searching.

**For architectural layout:**
- Variables: room_x, room_y, room_width, room_height for each room
- Domains: valid ranges within building footprint
- Constraints:
  - No overlap: rooms don't intersect
  - Adjacency: kitchen shares a wall with dining room
  - Non-adjacency: bedroom not adjacent to garage
  - Area bounds: living room >= 20m^2
  - Aspect ratio: no room wider than 3:1
  - Reachability: connected component check on room adjacency graph

**pvigier's blog (2022) implementation:** Used CSP for room generation in a game context. Key insight: "CSPs are very intuitive to use -- if you want to have a shower next to a wall, you just have to add a constraint to do so, and they provide strong guarantees on the output."

**AC-3 constraint propagation:** Before searching, AC-3 eliminates impossible values from variable domains. For room placement, this means: if room A must be adjacent to room B, and room B is placed at (5,5), then room A's position domain is pruned to only locations adjacent to (5,5). This dramatically reduces the search space.

**Relevance to Monolith:** Medium-high. CSP is more flexible than ASP for continuous/mixed domains. We could encode architectural rules as constraints and use a solver to find valid configurations. Libraries like Google OR-Tools have C++ CSP solvers.

**Key sources:**
- [pvigier's Room Generation using CSP](https://pvigier.github.io/2022/11/05/room-generation-using-constraint-satisfaction.html)
- [CSP for Architectural Functional Layout (Liggett, 1985)](https://www.researchgate.net/publication/233052982)
- [Constraint Satisfaction Techniques for Spatial Planning](https://link.springer.com/chapter/10.1007/978-3-642-84392-1_13)

### 1.3 Grammar-Based Approaches (Updated)

**CGA Shape Grammar (CityEngine 2025.1 update):**
- CityEngine 2025.1 (December 2025) expanded CGA geometry tools and introduced Python 3 API
- New facade components in ESRI.lib allow converting massing studies into complete buildings without writing rules
- Building volumes can be split into floors, assigned facade layouts, and populated with windows/shading systems automatically
- **Still exterior-focused** -- CGA excels at facade subdivision but doesn't natively generate room layouts or interior topology

**L-Systems for Architecture:**
- Parametric L-systems can grow building structures iteratively
- Best for organic/evolving structures (vines growing through corridors, building decay progression)
- Not ideal for precise room layout control

**Our existing approach (JSON-based split grammar):** Already implemented in Monolith's facade generation -- this remains solid for exterior detailing.

**Key sources:**
- [CityEngine 2025.1 Release Notes](https://www.esri.com/arcgis-blog/products/city-engine/3d-gis/whats-new-in-arcgis-cityengine-2025-1)
- [CityEngine 2025.1 Feature Expansion](https://digitalproduction.com/2025/12/11/cityengine-2025-1-expands-cga-geometry-tools-and-introduces-python-3-api/)

---

## 2. WaveFunctionCollapse and Variants for Buildings

### 2.1 Standard WFC for Room Layout

**The problem with vanilla WFC for buildings:** WFC is fundamentally a local constraint propagation algorithm. It ensures adjacent tiles match, but has no concept of global structure -- rooms need doors, hallways need connectivity, every space needs to be reachable. Pure WFC generates visually coherent tile arrangements but architecturally nonsensical buildings.

**The "Rooms" tileset approach:** WFC can generate room layouts using a tileset designed for architecture (wall segments, door segments, floor tiles, corner pieces). Boris the Brave documents that a simple 4-tile system can generate square rooms, with room size controlled through tile weighting. Adding door and corridor tiles creates varied floor plans. But results are unpredictable and often produce dead ends or disconnected spaces.

### 2.2 Enhanced WFC with Global Constraints

**Path Constraint (Boris the Brave):** The most important WFC enhancement for buildings. A path constraint enforces that a walkable route exists between specified points (e.g., entrance and every room). This prevents disconnected chambers that pure WFC tends to produce. Implementation: during propagation, if removing a tile possibility would break the only remaining path between required points, that removal is forbidden.

**Fixed Tile Constraints:** Lock specific tiles before generation to establish entrance positions, pre-authored areas, or level boundaries. WFC generates around these anchors seamlessly.

**Biome/Theme Constraints:** Disable thematically inappropriate tiles per region. For horror buildings: disable "clean" tiles in decayed zones, disable "ornate" tiles in utility areas.

### 2.3 Driven WFC (Townscaper's Approach)

**Key insight from Boris the Brave (2021):** Driven WFC inverts the typical approach. Instead of WFC generating the level, WFC acts as a *tile selection algorithm* that fills in details within a pre-determined structure.

**How it works:**
1. Determine building structure externally (treemap, graph-based, manual)
2. Encode that structure as constraints on the WFC grid (this cell must be "wall", this cell must be "floor", etc.)
3. Run WFC to select compatible detail tiles within those boundaries

**Townscaper's implementation:** The user defines solidity (solid vs. empty) on irregular grid vertices. These boolean values filter which tiles can occupy adjacent cells. WFC then composes detailed meshes from a marching cubes-derived tileset. The result looks hand-crafted but is algorithmically consistent.

**Why this matters for Monolith:** This is the hybrid approach we should adopt. Use our existing treemap/graph-based room layout to determine gross structure, then use Driven WFC to add interior detail (furniture arrangement, wall decoration placement, debris patterns for horror).

### 2.4 Hierarchical/Chunked WFC (2024)

**Christie et al. (IVCNZ 2024): "Procedurally Generating Large Synthetic Worlds: Chunked Hierarchical WFC"**
- Hierarchical approach: first pass does broad partitions, successive passes refine
- Enables WFC to work at city-block scale without combinatorial explosion
- Each chunk is solved independently but with boundary constraints from neighbors
- **Directly applicable** to our block->building->floor->room hierarchy

**Zhang et al. (2025): "Rural residential layout generation using WFC"**
- Applied WFC to architectural layout generation (not just tiles)
- Published in International Journal of Architectural Computing, March 2025
- Uses WFC for placement of building units within village layouts

### 2.5 WFC Failure Handling

**Contradictions are common** in constrained WFC. When no valid tile exists for a position:
- **Backtracking:** Undo the last placement and try another option. Expensive but guarantees eventual success if a solution exists.
- **Restart:** Discard and regenerate. Fast but wasteful. Works well if success rate is high.
- **Relaxation:** Temporarily remove the most restrictive constraint, generate, then fix violations in a post-pass.

For building generation at editor-time, backtracking is acceptable (sub-second for room-scale WFC). For runtime generation, restart with a generation budget is more predictable.

**Key sources:**
- [Boris the Brave: WFC Tips and Tricks](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Boris the Brave: Driven WFC](https://www.boristhebrave.com/2021/06/06/driven-wavefunctioncollapse/)
- [Christie et al., Chunked Hierarchical WFC (IVCNZ 2024)](https://dl.acm.org/doi/abs/10.1145/3402942.3402987)
- [Enhancing WFC with Design-Level Constraints (FDG 2019)](https://dl.acm.org/doi/10.1145/3337722.3337752)
- [DeBroglie Library (Boris the Brave)](https://boristhebrave.github.io/DeBroglie/articles/constraints.html)
- [Exploration of WFC in Architecture (2024)](https://link.springer.com/chapter/10.1007/978-3-031-71008-7_28)

---

## 3. Graph-Based Room Generation Improvements

### 3.1 Planar Graph Embedding for Room Adjacency

**The mathematical foundation:** A floor plan IS a planar graph -- rooms are faces, walls are edges, and door junctions are vertices. The problem of generating a floor plan from an adjacency requirement is equivalent to finding a planar embedding of a graph where faces have specified areas.

**Giffin et al. (1995):** Linear time algorithm for constructing orthogonal floorplans from a planar triangulated graph with room area requirements. Uses orderly spanning trees.

**Liao et al. (2003):** Linear time algorithm for dimensionless orthogonal floorplans for any n-vertex planar triangulated graph.

**Shekhawat et al. (2024):** "A graph theoretic approach for generating T-shaped floor plans" -- extends classical algorithms to handle non-rectangular room shapes. Published in Theoretical Computer Science.

**Recent (2025):** "Automated generation of floor plans with minimum bends" (AI EDAM, Cambridge) -- optimizes rectilinear room layouts to minimize wall bends, producing cleaner geometry.

**What this means for Monolith:** Our graph-based approach is mathematically sound. The improvement path is:
1. Define room adjacency as a planar graph
2. Use a planar embedding algorithm to find a valid spatial arrangement
3. Use treemap/constrained growth to assign actual areas
4. This guarantees no room overlaps and all adjacencies are satisfiable

### 3.2 Voronoi/Delaunay for Organic Room Shapes

**Standard approach:** Place room center points, compute Voronoi diagram, each cell becomes a room. Produces organic, irregular shapes that feel natural for caves, ruins, or decayed buildings.

**Weighted Voronoi:** Assign weights to control room sizes. Larger weight = larger Voronoi cell = larger room. A "pseudo-Voronoi" approach using approximate weighted diagrams with straight cell walls produces room shapes suitable for architecture.

**Perlin noise distortion:** Apply Perlin noise to distort cell boundaries, creating more organic-looking borders. Good for horror (decayed, unsettling spaces) but harder to furnish than rectangular rooms.

**For horror:** Voronoi rooms are excellent for "wrong" feeling spaces -- rooms that don't quite make geometric sense, organic shapes that feel grown rather than built. Perfect for corrupted/infected areas of buildings.

**Limitation:** Voronoi rooms are hard to furnish procedurally (furniture expects rectangular spaces). Best used selectively for special areas, not as the primary room generation method.

### 3.3 Force-Directed Graph Layout for Room Positioning

**The approach:** Treat rooms as nodes in a force-directed graph. Connected rooms attract each other (spring forces), all rooms repel each other (Coulomb forces). Run the simulation until equilibrium.

**Algorithm (Fruchterman-Reingold):**
1. Start with random room positions
2. Calculate repulsive forces between all room pairs (push apart)
3. Calculate attractive forces between adjacent rooms (pull together)
4. Apply forces, move rooms
5. Repeat until positions stabilize
6. Snap to grid, resolve overlaps

**Advantages:** Naturally clusters connected rooms together, separates unrelated rooms. Produces layouts that feel organic without being chaotic.

**Disadvantages:** Can produce overlapping rooms that need post-processing. No guarantee of rectangular room shapes. Convergence can be slow.

**For Monolith:** This could be a useful intermediate step between adjacency graph definition and final room placement. Use force-directed layout to get approximate positions, then snap to grid and use constrained growth to finalize shapes.

### 3.4 Guaranteeing Corridor Connectivity

**The fundamental problem:** Graph-based room placement can create situations where two rooms that should be adjacent end up separated by other rooms. Corridors must bridge these gaps.

**Solutions from the literature:**

**Nystrom's Connector Spanning Tree:** After placing rooms, create a grid. Flood-fill corridors between rooms using a maze algorithm. Then create a spanning tree of connections between rooms by opening "connectors" (doors) between rooms and corridors. This guarantees all rooms are reachable.

**Shadows of Doubt's hallway-first approach:** Calculate a skeleton hallway through the building center first, then grow rooms outward from the hallway. Guarantees connectivity by construction -- every room touches the hallway.

**Subveillance's perimeter inward approach:** Run iterative passes from the building perimeter inward, allocating room space. Reserve 3+ tiles for hallway space in the center. Rooms are guaranteed reachable because they all border the central hallway.

**Constrained Growth with corridor reservation (Bidarra, TU Delft):** Reserve corridor space during room growth. When a room grows, it cannot consume corridor tiles. After all rooms are placed, corridors connect to every room.

**Key sources:**
- [Shekhawat, "T-shaped floor plans" (TCS, 2024)](https://www.sciencedirect.com/science/article/abs/pii/S0304397524003396)
- [Automated floor plans with minimum bends (AI EDAM, 2025)](https://www.cambridge.org/core/journals/ai-edam/article/automated-generation-of-floor-plans-with-minimum-bends/214D14B8D2D263DE4B2D5C97103165F0)
- [Constrained Growth Method (Bidarra, TU Delft 2010)](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)

---

## 4. Commercial Proc-Gen Building Systems (2024-2026)

### 4.1 Houdini Labs Building Generator (v4.0+)

**Current state (2025-2026):**
- Four-stage pipeline: base model preparation, template material processing, generator parameter configuration, output optimization
- Supports Houdini 20.5 with UE5 integration via Houdini Engine plugin
- Exterior-focused but supports interior floor/wall generation via custom HDA nodes
- Megastructure Generator tutorial (2025) teaches full custom building generators from scratch
- **Limitation:** Requires Houdini license ($269/year indie, $4,495/year studio). Heavy dependency for a plugin we ship.

**Relevance:** Houdini's approach validates our architecture (parametric rules -> geometry). We shouldn't depend on it but can learn from its patterns.

### 4.2 Unreal Engine PCG Framework

**UE 5.7 PCG capabilities for buildings:**
- Node-based graph system for rule-based mesh placement
- Supports modular building workflows: box volume -> procedurally populated walls/floors/windows/furniture
- "Building Grammar" approach: PCG graphs assemble modular pieces based on rules
- Can split graphs into subgraphs for ground floor walls, typical walls, parapets, corners
- **Interior support:** PCG can place interior objects (desks with randomly positioned items) but does NOT generate room layouts or floor plans
- **Unreal Fest Bali 2025:** Demonstrated dynamic city creation with PCG, including building exteriors and biome integration
- **Unreal Fest 2025 "Buildings and Biomes":** Integration of PCG with Biomes plugin for dynamic environments

**Key limitation:** PCG is a mesh placement system, not a layout generation system. It can populate a floor plan with furniture but cannot generate the floor plan itself. This is exactly where Monolith adds value -- we generate the layout, PCG could handle the detail population.

**Potential integration:** Monolith generates building geometry + metadata (room types, door positions, window positions). A PCG graph reads that metadata and populates interiors with modular mesh kits.

### 4.3 Unreal Marketplace Plugins

**"Procedural Building" (Code Plugin):**
- Generates building interiors including interior walls, doors, hallways, stairs
- Parameters: number of floors, corridors
- Interior elements: chairs, tables, beds
- **Most directly comparable** to what we're building

**"Procedural Building Generator" (Blueprint):**
- Assembles modular/Megascans meshes procedurally
- Supports exterior and interior
- Full artistic control over mesh selection
- Blueprint-based, no C++ required

**"PowerHouse - Procedural Building System":**
- Blueprint-based, varying sizes and styles
- Speeds up repetitive building creation
- Exterior-focused with basic interior support

**"Procedural Building Lot":**
- Generates entire rows of buildings
- Style-based system with randomized facades
- Uses database of patterns
- Exterior-only, no interiors

**Assessment:** None of these handle the *layout generation* problem well. They're mesh assembly tools. Our approach (algorithm-first, geometry-second) is more architecturally sound.

### 4.4 Townscaper (Oskar Stalberg)

**Algorithm:** Irregular Voronoi grid + WFC + marching cubes variant. Users place solid/empty blocks on an irregular grid. WFC selects appropriate tile meshes based on neighbor configurations. Automatically generates arches, stairways, bridges, and backyards.

**Key insight:** The irregular grid was a deliberate choice to avoid the artificial regularity of voxel grids. Creates more natural, winding geometry.

**Interior generation:** Townscaper does NOT generate interiors. Buildings are solid blocks with exterior detailing only. It's a toy/visualization tool, not a game level generator.

**What we can learn:** The Driven WFC approach (structure first, WFC for detail) is the transferable technique. The irregular grid concept could be interesting for outdoor areas but doesn't apply to building interiors where rectangular rooms are expected.

### 4.5 CityEngine (Esri) 2025.1

**Latest updates (December 2025):**
- Expanded CGA geometry tools
- Python 3 API for scripting
- New facade components in ESRI.lib for no-code building generation
- Automatic floor-split, facade layout, window/shading population
- **Still exterior-only** -- no interior room generation capability
- Professional/enterprise pricing ($4,200+/year)

**Key sources:**
- [UE5 PCG Modular Building Tutorial](https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation)
- [Unreal Fest 2025: Buildings and Biomes PCG](https://dev.epicgames.com/community/learning/talks-and-demos/pBl1/unreal-engine-unreal-fest-2025-buildings-and-biomes-pcg)
- [Leveraging PCG for Building and City Creation](https://dev.epicgames.com/community/learning/talks-and-demos/Z1wa/unreal-engine-leveraging-pcg-for-building-and-city-creation)
- [Procedural Building (UE Marketplace)](https://www.unrealengine.com/marketplace/en-US/product/procedural-building)
- [Houdini Labs Building Generator](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_generator-4.0.html)
- [Townscaper Algorithm Analysis](https://www.gamedeveloper.com/game-platforms/how-townscaper-works-a-story-four-games-in-the-making)

---

## 5. Academic Papers (2024-2025)

### 5.1 Deep Learning Floor Plan Generation (Explosion of Activity)

The 2024-2025 period has seen an explosion of deep learning approaches to floor plan generation. These are primarily aimed at architectural design assistance, not game development, but the algorithms are directly transferable.

**GSDiff (AAAI 2025):** "Synthesizing Vector Floorplans via Geometry-enhanced Structural Graph Generation"
- Views floor plan synthesis as structural graph generation
- Two tasks: wall junction generation (diffusion model) + wall segment prediction (Transformer)
- Alignment loss for geometric consistency
- Trained on 80,788 RPLAN dataset layouts
- Produces vector-format floor plans (not raster images)
- [Paper](https://arxiv.org/abs/2408.16258) | [Code](https://github.com/SizheHu/GSDiff)

**GFLAN (2025):** "Generative Functional Layouts"
- Two-stage pipeline: CNN-based room center placement -> Graph-based rectangle regression
- Stage A: Dual DeepLabV3 (ResNet-101) encoders predict room center heatmaps
- Stage B: 7-layer TransformerConv GNN produces axis-aligned room rectangles
- Enforces containment, no-overlap, connectivity, adjacency, area realism
- 168.6ms inference time on NVIDIA A100
- 82% usability ratio, 95% full connectivity
- Handles functional constraints (bedrooms >= 9m^2, living rooms >= 12m^2)
- [Paper](https://arxiv.org/html/2512.16275)

**GenPlan (ICLR 2025 submission):** "Automated Floor Plan Generation"
- Two modules: GenCenter (ResNet101 encoders + decoders for room center prediction) + GenLayout (Transformer-based GNN for room boundary delineation)
- Graph representation: boundary corners as nodes, walls as edges, room centers as typed nodes
- Supports both deterministic and stochastic generation
- [Paper](https://openreview.net/forum?id=kA5egaJjya)

**ChatHouseDiffusion (2024):** Prompt-guided generation and editing of floor plans
- Natural language input ("add a bedroom next to the kitchen")
- Uses pre-trained diffusion model
- Enables iterative editing of generated plans
- [Paper](https://arxiv.org/html/2410.11908v1)

**HouseGAN/HouseGAN++ (foundational, updated):** Relational GAN conditioned on room adjacency graph. HouseGAN++ additionally learns to place doors. Still referenced as baseline in 2025 papers.

**DBGNN (2025):** Dual-branch graph neural network for automated residential bubble diagram generation. Uses variational graph autoencoder (VGAE) to learn distribution of topological patterns.

**Floorplan-Diffusion (2025):** Uses pre-trained large latent diffusion model for floor plan generation. Published at ICMR 2025.

### 5.2 Graph-Theoretic Floor Plans

**"A graph theoretic approach for generating T-shaped floor plans" (TCS, 2024):** Extends classical rectangular dual algorithms to handle T-shaped rooms. Practical for buildings where some rooms have L or T shaped footprints (e.g., kitchens wrapping around a corner).

**"Automated generation of floor plans with minimum bends" (AI EDAM, 2025):** Optimizes rectilinear room layouts to minimize the total number of wall bends. Produces cleaner, more buildable geometry.

**SOAG (2025):** "Generating floor plan diagrams using a self-organising adjacency graph"
- Adapts Self-Organizing Maps (SOM) for floor plan generation
- SOM neurons represented as geometric grid inside floor plan outline
- Cosine distance for Best Matching Unit (BMU) selection
- Zone elements localized next to specific neurons (e.g., entrance, viewpoints)
- Users can set fixed positions for key elements
- Real-time generation with interactive constraints
- Published in Frontiers of Architectural Research (2025)

### 5.3 Comprehensive Surveys

**"Computer-Aided Layout Generation for Building Design: A Review" (ICCVM 2025):**
- Comprehensive survey of all approaches: optimization-based, rule-based, GAN-based, diffusion-based, GNN-based
- Classification taxonomy for the field
- Identifies that graph-based methods with GNN refinement are the current leading approach

**"A State-of-Art Survey on Generative AI Techniques for Floor Planning" (GenAI+HCI 2025):**
- Covers GANs, Vision Transformers, diffusion models for floor plans
- Identifies Vision Transformers as best for analyzing high-resolution blueprints globally
- Notes the trend toward multi-modal approaches combining multiple architectures

**"Floor plan generation: The interplay among data, machine, and designer" (IJAC, 2025):**
- Philosophical paper on the role of human designers vs. AI in floor plan generation
- Argues for interactive systems where AI proposes and humans refine

### 5.4 Space Syntax for Evaluation

**"Developing a Space Syntax-Based Evaluation Method for Procedurally Generated Game Levels" (Biyik & Surer):**
- Uses integration, connectivity, and depth distance metrics
- VGA (Visibility Graph Analysis) + axial line analysis
- Key finding: smaller room dimensions (15x15 units) generate higher connectivity -> more decision points -> better gameplay
- Spawning points should be in high-connectivity areas
- Proposes using Space Syntax parameters IN the generation loop, not just as post-hoc evaluation

**"Space Syntax-guided Post-training for Residential Floor Plan Generation" (2025, arXiv):**
- Uses Space Syntax metrics as a training signal for neural floor plan generators
- Integration and connectivity values guide the loss function
- Generated plans score higher on architectural quality metrics

**Key sources:**
- [GSDiff (AAAI 2025)](https://arxiv.org/abs/2408.16258)
- [GFLAN](https://arxiv.org/html/2512.16275)
- [GenPlan (ICLR 2025)](https://openreview.net/forum?id=kA5egaJjya)
- [T-shaped floor plans (TCS 2024)](https://www.sciencedirect.com/science/article/abs/pii/S0304397524003396)
- [Minimum bends floor plans (AI EDAM 2025)](https://www.cambridge.org/core/journals/ai-edam/article/automated-generation-of-floor-plans-with-minimum-bends/214D14B8D2D263DE4B2D5C97103165F0)
- [SOAG (Frontiers Arch. Res. 2025)](https://www.sciencedirect.com/science/article/pii/S2095263525001827)
- [Space Syntax evaluation for game levels](https://www.academia.edu/75209801/Developing_a_Space_Syntax_Based_Evaluation_Method_for_Procedurally_Generated_Game_Levels)
- [Space Syntax post-training (arXiv 2025)](https://arxiv.org/html/2602.22507v1)

---

## 6. Specific Algorithms We Could Adopt

### 6.1 Inside-Out Generation (Corridor-First)

**The approach:** Start from the corridor network, grow rooms outward. The inverse of most algorithms which place rooms first and connect them after.

**Cogmind's "Tunneler" algorithm:**
1. Seed map with one or more "tunnelers"
2. Each tunneler digs corridors with random direction changes
3. Every N tiles, there's a probability of spawning a room to the side of the corridor
4. Tunnelers can branch (spawn child tunnelers)
5. Result: organic corridor network with rooms branching off

**Subveillance's "Perimeter Inward" approach:**
1. Start from building perimeter
2. Run iterative passes, each identifying one-tile-deep perimeter
3. Allocate perimeter tiles to rooms (max 6 tiles deep)
4. Reserve minimum 3 tiles for central hallway
5. Hallway uses same perimeter detection algorithm applied to remaining space

**Shadows of Doubt's approach (the gold standard for game buildings):**

The most detailed published algorithm for game-quality building interiors. Key details:

- **Grid:** 1.8m x 1.8m tiles, floors are 15x15 tiles
- **Hierarchy:** City > District > Block > Building > Floor > Address > Room > Tile
- **Designer-authored floorplans** define address zones (like SimCity zoning)
- **Hallway-first:** If entrance is too far from a corner, draw a hallway connecting entrance to nearby corner
- **Priority-based room placement:** Rooms are ranked by importance (living room > bathroom > bedroom > study)
- **Scoring heuristic per placement:** Floor space requirement + shape uniformity (fewer corners) + window access
- **Space stealing:** A room's override system allows rooms to claim excess floorspace from previously placed rooms
- **Connectivity rules:** Bathrooms get single doors, kitchens connect to living rooms, entrances never open directly into bedrooms

**For Monolith:** Shadows of Doubt's approach is the most production-proven for playable interiors. We should adopt its scoring heuristic for room placement quality.

### 6.2 Space Syntax Analysis for Quality

**What Space Syntax measures (and how to use it):**

| Metric | What It Measures | Horror Application |
|--------|-----------------|-------------------|
| **Integration** | How accessible a space is relative to all others | High integration = hub rooms (safe rooms, save points). Low integration = hidden areas (secret rooms, boss arenas) |
| **Connectivity** | Direct links between spaces | Low connectivity = isolation (claustrophobic corridors). High connectivity = vulnerability (open areas with many entrances) |
| **Depth Distance** | Minimum steps between locations | High depth from entrance = remote areas perfect for boss encounters or key items |
| **Isovist Area** | Visible floor area from a point | Small isovist = limited visibility = tension. Large isovist = vulnerability = different kind of tension |

**Integration into generation loop:**
1. Generate candidate floor plan
2. Compute Space Syntax metrics
3. Score the plan: does it match the intended tension curve? (high integration near entrance for safety, decreasing integration deeper in for horror)
4. Accept or reject/regenerate

**Computational cost:** Space Syntax analysis on a room graph with <50 nodes is sub-millisecond. Can easily be part of the generation loop.

### 6.3 Evolutionary/Genetic Algorithms for Floor Plan Optimization

**The approach:**
1. Generate a population of random floor plans (using any fast method: treemap, BSP, random placement)
2. Evaluate fitness: room area requirements, adjacency satisfaction, corridor efficiency, Space Syntax metrics
3. Select best plans, crossover (swap room arrangements between plans), mutate (shift/resize rooms)
4. Repeat for N generations

**Recent advances (2024-2025):**
- Machine learning surrogate models for faster fitness evaluation (don't need to compute full Space Syntax every generation)
- Discriminator models to control population diversity
- ML-based crossover operations that understand architectural semantics
- Evolution in latent space (encode floor plans as vectors, evolve in latent space, decode back)

**Fitness function for horror buildings:**
```
fitness = w1 * area_satisfaction     // rooms meet size requirements
        + w2 * adjacency_satisfaction // required adjacencies exist
        + w3 * connectivity_score    // all rooms reachable
        + w4 * tension_curve_match   // Space Syntax metrics match horror pacing
        + w5 * dead_end_count        // horror likes dead ends (bonus, not penalty!)
        + w6 * corridor_ratio        // not too much, not too little corridor
        - w7 * aspect_ratio_penalty  // penalize extremely elongated rooms
```

**For Monolith:** GA is excellent as a *refinement* step. Generate an initial plan quickly (treemap), then evolve it to optimize for horror pacing metrics. The horror-specific fitness function is the key innovation here.

### 6.4 The "Bubble Diagram" Approach

**From architecture practice:**
1. Start with an adjacency matrix (which rooms must be near which)
2. Represent each room as a "bubble" (circle proportional to area)
3. Use force-directed layout to position bubbles (adjacent rooms attract, others repel)
4. Resolve bubble positions into rectangular rooms

**Modern implementations:**
- SOAG (2025): Self-Organizing Adjacency Graph adapts SOM to convert bubble diagrams to floor plans in real-time
- DBGNN (2025): Dual-branch GNN generates bubble diagrams from adjacency specs
- Archi Bubble (online tool): Interactive bubble diagram to floor plan conversion

**Adjacency types from architecture:**
- **Primary:** Must be directly adjacent (bedroom-bathroom)
- **Secondary:** Must be close but not necessarily touching (kitchen-dining)
- **Undesired:** Must NOT be adjacent (bedroom-garage)

**For Monolith:** We should support adjacency matrices as input to `generate_building`. Users specify room types + adjacency requirements, Monolith handles the spatial layout. The bubble diagram intermediate step provides intuitive visualization of the constraint space.

### 6.5 Constrained Growth Method (Bidarra, TU Delft, 2010)

**The definitive algorithm for game building floor plans:**

1. **Grid initialization:** Divide floor into uniform grid cells
2. **Seed placement:** Place room seeds at positions satisfying adjacency constraints
3. **Growth phase:** Each room grows outward from its seed, claiming adjacent cells
   - Growth is constrained by: maximum area, maximum aspect ratio, collision with other rooms
   - Priority ordering determines which room grows first when competing for cells
4. **Corridor reservation:** Corridor cells are pre-reserved or grown as a special room type
5. **Door placement:** Doors placed at cell boundaries between adjacent rooms that should be connected
6. **Constraint validation:** Verify all constraints (reachability, adjacency, area) are satisfied

**Key contribution:** The paper formally defines constraints including reachability ("every room must be reachable from the entrance via doors") and connectivity ("rooms that should be adjacent must share at least one wall segment").

**Why this is the best fit for Monolith:** It operates on the same grid system we already use, supports arbitrary building footprints, handles multi-floor buildings, and its constraint system maps directly onto our JSON building specs.

### 6.6 Cyclic Dungeon Generation (Dormans) for Horror Topology

**Why cycles matter for horror:**

Traditional tree-based level layouts create one path from start to finish with dead-end branches. Cyclic layouts create loops where the player has multiple paths between areas. This is critical for horror because:

- **Escape routes:** Players need the option to flee from enemies via an alternate path
- **Sound propagation:** Hearing enemies through walls on the other side of a loop creates tension
- **Backtracking with dread:** Returning through previously safe areas that are now dangerous
- **Lock-and-key pacing:** One path has the key, the other has the lock

**Unexplored's implementation (Boris the Brave analysis, 2021):**
- ~5,000 graph rewriting rules in PhantomGrammar/Ludoscope
- 24 major cycle types defining narrative flow patterns
- Lock-and-key as abstract non-terminal symbols that persist through generation
- Three phases: abstract structure -> iterative complication -> concrete resolution
- 5x5 abstract grid -> 10x10 with corridors -> 50x50 final resolution
- Room shapes via cellular automata (caves) or rectangles (rooms)

**For Monolith horror buildings:**
- Define cycle types for horror scenarios: "hunter patrol loop", "escape route cycle", "locked wing with key elsewhere"
- Apply cycles at the floor level (rooms within a floor form cycles)
- Apply cycles at the building level (floors connected by stairwells form vertical cycles)
- Use the lock-and-key abstraction for horror mechanics: barricaded doors (need tool), locked doors (need key), powered doors (need to restore electricity)

**Key sources:**
- [Shadows of Doubt DevBlog #13](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Subveillance Devlog: Procedural Interiors](https://unusual-intersection.itch.io/subveillance/devlog/492733/procedural-generation-of-building-interiors-part-1)
- [Cyclic Dungeon Generation (Dormans)](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-)
- [Dungeon Generation in Unexplored (Boris the Brave)](https://www.boristhebrave.com/2021/04/10/dungeon-generation-in-unexplored/)
- [Constrained Growth Method (Bidarra)](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)
- [Novel Real-time Floor Plan Generation](https://ar5iv.labs.arxiv.org/html/1211.5842)

---

## 7. Horror-Specific Procedural Generation

### 7.1 RE Engine and Resident Evil Level Design

**Key finding: Resident Evil does NOT use procedural generation for levels.** All spaces are meticulously hand-crafted. However, the design patterns are invaluable for informing procedural generation constraints.

**"Recursive Unlocking" pattern (analysis by Chris's Survival Horror Quest):**
- Player unlocks shortcuts progressively, spiraling outward through available space
- 116 unique rooms across 4 areas in RE1 (mansion, courtyard, guardhouse, laboratory)
- Most rooms visited only twice; 38% visited only once
- Most-visited room: upper-right mansion hallway connecting multiple zones (visited 8x)
- "Micro-backtracking" within local areas, then opening new zones
- Mansion = 43% of game time (59 rooms), Laboratory = 25% (21 rooms)

**Implications for proc-gen:** We should generate buildings where:
- Hub rooms have high connectivity (many doors/corridors leading out)
- Wing rooms have low connectivity (one entrance, dead ends)
- The ratio of hub-to-wing follows RE's ~40/60 pattern
- Lock-and-key items are placed to create the recursive unlocking flow

### 7.2 Horror Architectural Design Rules for Generation

**From academic research (Liapis et al., "Targeting Horror via Level and Soundscape Generation"):**
- Tension curve fitness function: rooms placed to match an intended tension ramp
- Monster placement follows the tension curve (more monsters deeper in = higher tension)
- Items placed inversely to tension (more items near entrance = lower tension)
- Suspense curve calculated per alternative path through the level

**From horror level design practice (DrWedge, 2025):**
- **Asymmetrical layouts** create cognitive friction and unease
- **Disproportionately long corridors** build dread through spatial elongation
- **Tight corners and obscured sightlines** elevate physiological stress
- **Impossible geometry** (corridors intersecting without logic, rooms too large for their context) triggers uncanny response
- **Vulnerability zones** (exposed balconies, basements) exploit subconscious fears
- **Vertical elements** (stairs, pits, attics) generate additional fear dimension

**Quantifiable horror metrics for proc-gen:**
- **Corridor length-to-width ratio:** > 5:1 creates dread, > 10:1 creates claustrophobia
- **Average isovist area:** Small = tension, large = vulnerability
- **Dead end percentage:** 15-25% of total rooms should be dead ends
- **Average depth from entrance:** Deeper = scarier
- **Symmetry score:** Slight asymmetry (0.7-0.85 on a 0-1 scale) is most unsettling

### 7.3 Liminal Spaces / Backrooms Generation

**What makes a liminal space:**
- Monochromatic color (yellowed fluorescent lighting)
- Repetitive architecture (identical corridors, identical doors)
- Absence of human presence despite signs of habitation
- Wrong scale (ceilings too high or too low, corridors too wide)
- Wet carpet smell (environmental storytelling, not geometry)

**Generation approach for backrooms-style areas:**
1. Generate a maze (our `create_maze` already handles this)
2. Apply uniform styling (same wallpaper, same carpet, same fluorescent lights)
3. Introduce subtle wrongness: slightly non-orthogonal walls (1-2 degree rotation), ceiling height variation, doors that lead to unexpected rooms
4. Use WFC with a tiny tileset (4-6 tiles) for maximum repetition with local variation
5. Infinite generation via chunked approach: generate chunks as player moves, discard old chunks

**Backrooms: Escape Together (UE5, 2024-2025):** Popular commercial implementation. Uses procedural generation with seed-based world generation. Dynamically loaded maze chunks for infinite exploration.

**Implementation note:** This is achievable with our existing `create_maze` + `create_structure` with minimal extension. The key is the *styling constraint* -- forcing uniformity where normal generation would add variety.

### 7.4 Non-Euclidean Horror Spaces

**Types of non-Euclidean horror geometry:**
- **Portals:** Room A's door leads to Room C (not adjacent Room B)
- **Size distortion:** Interior larger than exterior (TARDIS effect)
- **Topology loops:** Walking straight through 4 right turns returns to a different room
- **Shifting geometry:** Corridors change while player isn't looking

**Implementation approaches:**
- **Portal-based (UE5 native):** Use scene capture + render target for portal windows. The player teleports between disconnected geometry when crossing thresholds.
- **Seamless portals (advanced):** Requires rendering both sides simultaneously. Technically challenging in UE5 but doable with custom stencil buffer techniques.
- **Shifting geometry:** Swap room geometry when player is in a different room (out of line of sight). Simple to implement with our room assembly system.

**Critical warning from research:** "Non-Euclidean geometry would be very difficult to pull off in a procedurally generated game -- it can be done but without a whole lot of research and development time all you'd generate are confusing, unplayable levels." The key is *subtle* wrongness, not overt impossibility.

### 7.5 Tension Curve Integration During Generation

**The key innovation:** Don't generate first and evaluate second. Build the tension curve INTO the generation algorithm.

**Approach:**
1. Define a target tension curve: T(d) where d = depth from entrance, T = tension level [0,1]
2. During room placement, score each room's contribution to the tension curve:
   - Room depth in the building -> desired tension level
   - Room size: smaller = higher tension contribution
   - Room connectivity: fewer exits = higher tension
   - Dead end status: dead end = high tension spike
   - Window count: fewer windows = higher tension
3. Accept room placement only if it improves the tension curve match
4. Post-process: place horror elements (gore, environmental damage, lighting defects) according to T(d)

**Specific tension curve shapes for horror:**
- **Slow burn:** T(d) = d^2 (gradual increase, explosive at the end)
- **Roller coaster:** T(d) = sin(d * pi) * d (oscillating with increasing amplitude)
- **Constant dread:** T(d) = 0.6 + 0.1*noise(d) (always tense, unpredictable spikes)
- **Safe-to-horror transition:** T(d) = sigmoid(d - 0.5) (normal building -> horror zone)

**Key sources:**
- [Recursive Unlocking: Analyzing RE Map Design](https://horror.dreamdawn.com/?p=81213)
- [Targeting Horror via Level and Soundscape Generation (Liapis)](https://antoniosliapis.com/papers/targeting_horror_via_level_and_soundscape_generation.pdf)
- [Horror Level Design: Technical Drawing (DrWedge, 2025)](https://drwedge.uk/2025/06/02/horror-game-level-design/)
- [Non-Euclidean Horror Spaces (2025)](https://drwedge.uk/2025/02/14/the-power-of-non-euclidean-horror-spaces-in-video-games/)
- [Tension Flow and Contrast in Horror](https://medium.com/@pavkovic.dusan99/fundamental-game-design-elements-of-horror-games-tension-flow-and-contrast-632027e97608)
- [Creating Horror Through Level Design (Gamasutra)](https://www.gamedeveloper.com/design/creating-horror-through-level-design-tension-jump-scares-and-chase-sequences)

---

## 8. Synthesis: Recommendations for Monolith

### 8.1 What's New Since Our Last Research

Our previous research (earlier today, `reference_proc_building_algorithms.md`) covered the foundational algorithms well. This research adds:

1. **Production-proven implementations:** Shadows of Doubt's scoring heuristic, Subveillance's perimeter-inward approach
2. **Deep learning floor plan generation (2024-2025 explosion):** GSDiff, GFLAN, GenPlan -- not for runtime use but could generate training data for our heuristics
3. **Driven WFC:** The missing piece for interior detailing -- generate structure first, use WFC for detail
4. **Space Syntax as a generation constraint (not just evaluation):** Integration into the fitness function
5. **SOAG:** Self-organizing adjacency graph as an alternative to force-directed layout for room positioning
6. **Horror-specific quantifiable metrics:** Corridor ratios, dead end percentages, isovist areas
7. **Non-Euclidean approaches:** Subtle wrongness > overt impossibility
8. **Tension curve integration:** During generation, not after

### 8.2 Recommended Algorithm Pipeline (Updated)

```
                    +-------------------+
                    |  Building Spec    |  JSON: room types, adjacencies,
                    |  (Input)          |  area requirements, building type
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Topology Phase   |  Graph-based: define room adjacency
                    |                   |  Cyclic generation for horror pacing
                    |                   |  Lock-and-key overlay
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Layout Phase     |  Constrained Growth (Bidarra) on grid
                    |                   |  Corridor reservation
                    |                   |  Force-directed initial positions
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Validation Phase |  ASP/CSP constraint check:
                    |                   |  - Reachability
                    |                   |  - Adjacency satisfaction
                    |                   |  - Area bounds
                    |                   |  Space Syntax scoring:
                    |                   |  - Tension curve match
                    |                   |  - Connectivity distribution
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Optimization     |  GA refinement (optional):
                    |  Phase            |  Evolve layout to maximize
                    |                   |  horror fitness function
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Geometry Phase   |  GeometryScript: walls, floors,
                    |                   |  ceilings, doors, windows
                    |                   |  (existing Monolith pipeline)
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Detail Phase     |  Driven WFC: interior furnishing,
                    |  (NEW)            |  wall decoration, debris placement
                    |                   |  Horror element distribution
                    +--------+----------+
                             |
                    +--------v----------+
                    |  Volume Phase     |  Auto-volumes: NavMesh, audio,
                    |  (Existing)       |  post-process, triggers
                    +--------+----------+
```

### 8.3 Priority Adoption List

| Priority | Technique | Why | Effort |
|----------|-----------|-----|--------|
| **P0** | Constrained Growth (Bidarra) | Best general-purpose room layout algorithm, grid-based, constraint-aware | ~20h |
| **P0** | Space Syntax scoring | Quantitative quality metrics for generated layouts, enables horror pacing | ~8h |
| **P0** | Shadows of Doubt scoring heuristic | Production-proven room placement quality scoring | ~6h |
| **P1** | Cyclic generation overlay | Lock-and-key horror pacing, escape routes, patrol loops | ~16h |
| **P1** | Tension curve integration | Horror-specific fitness function during generation | ~12h |
| **P1** | Driven WFC for interior detailing | Detail population after structure generation | ~24h |
| **P2** | ASP constraint validation (Clingo) | Formal guarantee of reachability and constraint satisfaction | ~12h |
| **P2** | Non-Euclidean horror rooms | Subtle portal/topology tricks for horror areas | ~16h |
| **P2** | GA refinement step | Evolutionary optimization of layout quality | ~10h |
| **P3** | Deep learning floor plans (GFLAN-style) | ML-based generation -- overkill for editor-time but interesting future | ~40h+ |
| **P3** | SOAG interactive layout | SOM-based interactive floor plan exploration | ~20h |

**Total P0:** ~34h
**Total P0+P1:** ~86h
**Total all:** ~184h

### 8.4 Key Takeaways

1. **Constraint-first is the trend.** The field is moving from generate-then-validate to generate-with-constraints. ASP, CSP, and constrained growth all express rules upfront.

2. **Deep learning is exploding but not practical for us.** GSDiff, GFLAN, GenPlan require training data and GPU inference. For editor-time MCP actions, algorithmic approaches (constrained growth, treemap, graph-based) are faster, more controllable, and require no model weights.

3. **Driven WFC is the breakthrough insight.** Use structure generators for layout, WFC for detail. This separation of concerns is exactly what we need.

4. **Space Syntax bridges architecture and game design.** Integration/connectivity/depth metrics let us quantify "does this building play well?" and "does this building feel scary?"

5. **Horror requires quantifiable metrics.** Dead end ratio, corridor length ratio, isovist area, depth distribution -- these can be computed and optimized during generation.

6. **Shadows of Doubt is the gold standard for game building interiors.** Its priority-based, scoring-heuristic approach with space-stealing is the most practical production technique documented.

7. **Non-Euclidean horror should be subtle.** Overt impossibility creates confusion, not fear. Subtle wrongness (rooms slightly too large, corridors that feel longer than they should be) is more effective and easier to generate.

8. **Cyclic generation (Dormans) is underused in horror.** Lock-and-key cycles map perfectly onto horror building exploration patterns (find key -> unlock wing -> find next key).

---

## Appendix: Source Bibliography

### Foundational Papers
- Mueller et al., "Procedural Modeling of Buildings" (SIGGRAPH 2006)
- Lopes et al., "A Constrained Growth Method for Procedural Floor Plan Generation" (GAMEON 2010)
- Marson & Musse, "Automatic Real-Time Generation of Floor Plans Based on Squarified Treemaps" (IJCGT 2010)
- Smith & Mateas, "Answer Set Programming for Procedural Content Generation" (2011)
- Dormans, "Cyclic Generation" in Procedural Generation in Game Design (2017)

### 2024-2025 Papers
- GSDiff: Synthesizing Vector Floorplans (AAAI 2025) -- [arXiv](https://arxiv.org/abs/2408.16258)
- GFLAN: Generative Functional Layouts (2025) -- [arXiv](https://arxiv.org/html/2512.16275)
- GenPlan: Automated Floor Plan Generation (ICLR 2025 submission) -- [OpenReview](https://openreview.net/forum?id=kA5egaJjya)
- ChatHouseDiffusion: Prompt-Guided Floor Plans (2024) -- [arXiv](https://arxiv.org/html/2410.11908v1)
- Floorplan-Diffusion (ICMR 2025) -- [ACM](https://dl.acm.org/doi/10.1145/3731715.3733343)
- SOAG: Self-Organising Adjacency Graph (Frontiers Arch. Res. 2025)
- T-shaped floor plans (TCS 2024) -- [ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0304397524003396)
- Minimum bends floor plans (AI EDAM 2025)
- Space Syntax post-training for floor plans (arXiv 2025)
- Chunked Hierarchical WFC (IVCNZ 2024)
- Rural residential layout via WFC (IJAC 2025)
- DBGNN for bubble diagrams (Applied Sciences 2025)
- Computer-Aided Layout Generation survey (ICCVM 2025)

### Game Development Sources
- [Shadows of Doubt DevBlog #13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Subveillance Devlog: Procedural Interiors](https://unusual-intersection.itch.io/subveillance/devlog/492733/procedural-generation-of-building-interiors-part-1)
- [Boris the Brave: Driven WFC](https://www.boristhebrave.com/2021/06/06/driven-wavefunctioncollapse/)
- [Boris the Brave: WFC Tips and Tricks](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Boris the Brave: Dungeon Generation in Unexplored](https://www.boristhebrave.com/2021/04/10/dungeon-generation-in-unexplored/)
- [Dormans: Cyclic Dungeon Generation](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-)
- [Recursive Unlocking: RE Map Design Analysis](https://horror.dreamdawn.com/?p=81213)
- [Horror Level Design Technical Drawing (DrWedge 2025)](https://drwedge.uk/2025/06/02/horror-game-level-design/)
- [Non-Euclidean Horror Spaces (DrWedge 2025)](https://drwedge.uk/2025/02/14/the-power-of-non-euclidean-horror-spaces-in-video-games/)
- [Townscaper Algorithm Analysis](https://www.gamedeveloper.com/game-platforms/how-townscaper-works-a-story-four-games-in-the-making)
- [Liapis: Targeting Horror via Level and Soundscape Generation](https://antoniosliapis.com/papers/targeting_horror_via_level_and_soundscape_generation.pdf)

### Commercial Tools
- [Houdini Labs Building Generator v4.0](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_generator-4.0.html)
- [CityEngine 2025.1](https://www.esri.com/arcgis-blog/products/city-engine/3d-gis/whats-new-in-arcgis-cityengine-2025-1)
- [UE5 PCG Framework Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [UE5 PCG Modular Building Tutorial](https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation)
- [Procedural Building (UE Marketplace)](https://www.unrealengine.com/marketplace/en-US/product/procedural-building)

### Solver/Library References
- [Clingo ASP Solver](https://github.com/potassco/clingo)
- [DeBroglie WFC Library](https://boristhebrave.github.io/DeBroglie/articles/constraints.html)
- [Space Syntax methodology](https://spacesyntax.com/the-space-syntax-approach/)
