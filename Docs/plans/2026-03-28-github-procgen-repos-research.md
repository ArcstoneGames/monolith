# GitHub Repositories & Resources for Procedural Building Generation

**Date:** 2026-03-28
**Type:** Research Reference
**Purpose:** Catalog open-source repos, algorithms, blog posts, and GDC talks relevant to Monolith's procedural building pipeline

---

## Table of Contents

1. [Unreal Engine Repositories](#1-unreal-engine-repositories)
2. [Unity Repositories](#2-unity-repositories)
3. [Blender / Python Repositories](#3-blender--python-repositories)
4. [Engine-Agnostic / Standalone](#4-engine-agnostic--standalone)
5. [Wave Function Collapse Projects](#5-wave-function-collapse-projects)
6. [Floor Plan & Layout Generators](#6-floor-plan--layout-generators)
7. [Furniture / Interior Decoration](#7-furniture--interior-decoration)
8. [Dungeon Generators (Relevant Algorithms)](#8-dungeon-generators-relevant-algorithms)
9. [Shape Grammar / CGA Implementations](#9-shape-grammar--cga-implementations)
10. [Blog Posts & Tutorials](#10-blog-posts--tutorials)
11. [GDC Talks](#11-gdc-talks)
12. [Academic Papers with Code](#12-academic-papers-with-code)
13. [Key Takeaways for Monolith](#13-key-takeaways-for-monolith)

---

## 1. Unreal Engine Repositories

### bendemott/UE5-Procedural-Building
- **URL:** https://github.com/bendemott/UE5-Procedural-Building
- **Stars:** 35 | **License:** MIT | **Updated:** Sep 2023
- **Language:** C++ (98%)
- **What it does:** Single-actor procedural building using GeometryScript. Places box primitives on a grid, merges into one DynamicMesh. Configurable via Details Panel with seed-based randomization.
- **Algorithms:** Grid-based box placement, UV assignment per face
- **GeometryScript APIs used:** `AppendMesh`, `MeshPrimitiveFunctions`, `MeshTransformFunctions`, `MeshUVFunctions`, `MeshMaterialFunctions`
- **Handles:** Exterior shell only. No windows, doors, interiors, stairs, or multi-floor
- **Performance note:** >30 seconds for complex buildings (UV/material bottleneck)
- **What we could borrow:** Reference for GeometryScript C++ API usage patterns. Shows the performance ceiling of naive single-mesh approach. Validates our decision to use per-component meshes.

### Erisbv-zz/UNREAL-Procedural-Cities
- **URL:** https://github.com/Erisbv-zz/UNREAL-Procedural-Cities
- **Stars:** 47 | **License:** MIT | **Updated:** 2017 (unmaintained)
- **Language:** C++ (99.5%)
- **What it does:** Full city generation including **interiors for all buildings**. City layout, building shells, and apartment interiors. Master thesis project.
- **Algorithms:** Not documented (thesis referenced but hard to find)
- **Handles:** Exteriors AND interiors. One of the few UE projects attempting full interior generation.
- **What we could borrow:** Proof of concept that interiors in UE proc-gen is viable. Worth reading the source for interior generation approach despite being UE4/outdated.

### VladimirKobranov/configurator-unreal-building
- **URL:** https://github.com/VladimirKobranov/configurator-unreal-building
- **Stars:** 4 | **License:** Apache-2.0 | **Updated:** Nov 2022
- **Language:** C++ (100%)
- **What it does:** Modular building with X/Y/Z axis element counting, seed-based randomization, brandmauer (firewall) walls, facade stairs.
- **Handles:** Exterior modular assembly. No interiors.
- **What we could borrow:** Modular piece randomization by seed approach.

### Grzybojad/ProceduralCityGeneration
- **URL:** https://github.com/Grzybojad/ProceduralCityGeneration
- **Stars:** 16 | **License:** MIT | **Updated:** Jun 2023
- **Language:** C++ (97%)
- **What it does:** Terrain (Perlin, Diamond-square) + roads (Voronoi) + buildings (plot extrusion, shrinking layer stacking).
- **Handles:** City-scale exterior only. No interiors.
- **What we could borrow:** Shrinking layer stacking for building silhouettes (each floor slightly smaller than below).

### salaark/shape-grammar-city
- **URL:** https://github.com/salaark/shape-grammar-city
- **Stars:** ~10 | **License:** Not specified
- **Language:** C++/UE
- **What it does:** Shape grammar-based procedural buildings and city spawner. Uses Eternal Temple modular meshes.
- **Handles:** Exterior only with modular mesh replacement.
- **What we could borrow:** Shape grammar rule application in UE C++ context.

### nvdomidi/ProceduralGenerator
- **URL:** https://github.com/nvdomidi/ProceduralGenerator
- **Stars:** 8 | **License:** Not specified
- **Language:** C++ (99%), UE5 plugin
- **What it does:** Procedural city generator -- roads, parcels, buildings, urban infrastructure. Can import OpenStreetMap data.
- **Handles:** City-scale layout. Buildings as placed elements, not generated geometry.
- **What we could borrow:** Parcel subdivision from road graphs, OpenStreetMap import pipeline.

### shun126/DungeonGenerator (UE5 Plugin)
- **URL:** https://github.com/shun126/DungeonGenerator
- **Stars:** 129 | **License:** GPL-3.0 (Apache on Fab) | **Updated:** Oct 2025
- **Language:** C++ (92.5%), UE 5.1-5.7
- **What it does:** Full 3D dungeon generator with rooms, corridors, stairs, doors, keys/locks (MissionGraph), mini-maps.
- **Algorithms:** Based on Vazgriz algorithm (Delaunay + MST + A*)
- **Handles:** Multi-floor with stairs, door/key generation, interior decoration (beta). Sub-level support.
- **What we could borrow:** **HIGH VALUE.** Active UE5 plugin doing multi-floor generation. MissionGraph for lock-and-key. Stair generation in UE context. Room template system. Direct reference for our spatial registry + stairwell work.

### Esri/cityengine_for_unreal (Vitruvio)
- **URL:** https://github.com/Esri/cityengine_for_unreal
- **Stars:** 257 | **License:** Apache-2.0 | **Updated:** Aug 2025
- **Language:** C++, UE5
- **What it does:** Executes CGA shape grammar rule packages (RPKs) from CityEngine inside UE5. Runtime procedural generation. Full CGA rule support.
- **Handles:** Primarily exteriors/facades via CGA rules. Theoretically can do interiors if rules define them.
- **What we could borrow:** The architecture of integrating a rule engine with UE5 runtime. Proof that CGA works in UE5. However, depends on proprietary CityEngine RPKs -- we cannot use it directly but the plugin architecture is instructive.

### PCGEx/PCGExtendedToolkit
- **URL:** https://github.com/PCGEx/PCGExtendedToolkit
- **Stars:** 599 | **License:** MIT | **Updated:** Active (3,656 commits)
- **Language:** C++, UE5
- **What it does:** 200+ nodes extending UE5 PCG framework. Delaunay triangulation, Voronoi, MST, convex hull, A*/Dijkstra pathfinding, Lloyd relaxation, bin packing, polygon clipping (Clipper2).
- **Handles:** Low-level spatial operations, not buildings directly. But the graph/spatial algorithms are exactly what building layout needs.
- **What we could borrow:** **HIGH VALUE.** Delaunay/Voronoi/MST implementations in UE5 C++. Clipper2 polygon operations. These are the same algorithms we need for lot subdivision, room adjacency graphs, and corridor pathfinding. Could potentially use as a dependency or reference implementation.

### Flone-dnb/FWorldGenerator
- **URL:** https://github.com/Flone-dnb/FWorldGenerator
- **Stars:** ~20 | **License:** Not specified | **Updated:** Older (UE4)
- **What it does:** Procedural world generator plugin, terrain + object placement.
- **What we could borrow:** Limited -- UE4, terrain focus.

---

## 2. Unity Repositories

### Syomus/ProceduralToolkit
- **URL:** https://github.com/Syomus/ProceduralToolkit
- **Stars:** 2,900 | **License:** MIT | **Updated:** Active
- **Language:** C# (Unity)
- **What it does:** Full procedural generation library. **Building generator creates entire meshes from scratch** with facade planning and construction strategies.
- **Key architecture:**
  - `BuildingGenerator.cs` -- main orchestrator
  - `FacadePlanner.cs` / `FacadeConstructor.cs` -- strategy pattern for facades
  - `RoofPlanner.cs` / `RoofConstructor.cs` -- strategy pattern for roofs
  - `HorizontalLayout` / `VerticalLayout` -- facade element layout
  - `ProceduralFacadeElements.cs` -- window/wall/door mesh elements
  - `ProceduralRoofs.cs` -- multiple roof styles
- **Algorithms:** Layout-based facade subdivision (horizontal/vertical splits, like CGA but simpler), strategy pattern for swappable styles
- **Handles:** Exteriors: facades + roofs. **No interiors.**
- **What we could borrow:** **HIGH VALUE.** The FacadePlanner/FacadeConstructor strategy pattern is very close to what we need. The horizontal/vertical layout system for facade elements maps directly to our facade_window_research.md design. The separation of planning (what goes where) from construction (how to build the mesh) is a proven pattern we should adopt.

### marian42/wavefunctioncollapse
- **URL:** https://github.com/marian42/wavefunctioncollapse
- **Stars:** 4,800 | **License:** MIT | **Updated:** Jan 2019
- **Language:** C# (Unity)
- **What it does:** Walk through an **infinite procedurally generated city** using WFC with backtracking. Generates as you walk.
- **Key architecture:** ModulePrototype system with connector-based neighbor rules, spawn probabilities, slot inspector
- **Handles:** Exteriors AND interiors (v0.2 added interiors). 3D tile-based generation.
- **What we could borrow:** Module/connector adjacency system. Incremental generation (generate near player). The proof that WFC + handcrafted tiles can produce convincing city blocks with interiors. Backtracking strategy for constraint conflicts.

### OndrejNepozitek/Edgar-Unity + Edgar-DotNet
- **URL:** https://github.com/OndrejNepozitek/Edgar-Unity (Unity) / https://github.com/OndrejNepozitek/Edgar-DotNet (.NET core)
- **Stars:** 894 (Unity) | **License:** MIT | **Updated:** Apr 2025
- **Language:** C#
- **What it does:** Graph-based procedural level generator. Designer defines room graph (how many rooms, which connect to which), then generator produces layout using handmade room templates.
- **Algorithms:** Graph-based constraint satisfaction. Room template selection and placement with connectivity constraints.
- **Handles:** 2D room layouts with doors and corridors. Room templates are designer-authored.
- **Limitation:** <30 rooms recommended
- **What we could borrow:** **HIGH VALUE.** The graph-first approach (define connectivity, then solve layout) is the gold standard for designer-controlled proc-gen. Edgar-DotNet is engine-agnostic and could theoretically be ported. The room template + graph connectivity model maps well to our building archetype system.

### vazgriz/DungeonGenerator
- **URL:** https://github.com/vazgriz/DungeonGenerator
- **Stars:** 804 | **License:** MIT | **Updated:** Nov 2019
- **Language:** C# (Unity)
- **What it does:** Implements the classic Delaunay + MST + A* dungeon algorithm, extended to 3D multi-floor.
- **Key algorithm details (from blog post):**
  - Room placement on 30x30 grid with 1-unit buffers
  - Bowyer-Watson Delaunay triangulation for connectivity graph
  - Prim's MST for minimum spanning tree + 12.5% random edge re-addition for loops
  - A* pathfinding for corridors with cost function favoring existing hallways
  - **3D extension:** Custom 3D Delaunay (circumspheres, 4x4 matrices). Grid becomes 30x5x30.
  - **Staircase algorithm:** 1:2 rise-to-run ratio, 4 cells per staircase, 2-cell headroom, modified A* with path history to allow "jumps" over stair cells. O(N^2) but workable.
- **Handles:** Multi-floor with stairs, corridors, room connectivity
- **What we could borrow:** **HIGH VALUE.** The 3D Delaunay + staircase A* algorithm is directly applicable to our stairwell placement. The modified A* that "jumps" over stair cells solves the same problem we face in stairwell_cutout_research.md. The cost function that makes reusing existing hallways cheaper is elegant.

### oddmax/unity-wave-function-collapse-3d
- **URL:** https://github.com/oddmax/unity-wave-function-collapse-3d
- **Stars:** 229 | **License:** Not specified
- **Language:** C# (Unity)
- **What it does:** WFC in 3D with Simple Tiled model. Symmetry-based tile classification (X, L, T, I, /).
- **What we could borrow:** Tile symmetry classification system for modular building pieces.

### nielsdejong/unity-city-generation
- **URL:** https://github.com/nielsdejong/unity-city-generation
- **Stars:** ~30 | **License:** Not specified
- **Language:** C# (Unity)
- **What it does:** Procedural city generation with roads, blocks, and buildings.
- **What we could borrow:** City block subdivision approach.

### gregoryneal/Cigen
- **URL:** https://github.com/gregoryneal/Cigen
- **Stars:** ~20 | **License:** Not specified
- **Language:** C# (Unity)
- **What it does:** Procedural city generator for Unity.
- **What we could borrow:** Limited -- less documented than alternatives.

---

## 3. Blender / Python Repositories

### wojtryb/Procedural-Building-Generator
- **URL:** https://github.com/wojtryb/Procedural-Building-Generator
- **Stars:** 38 | **License:** MIT | **Updated:** Aug 2025 (archived)
- **Language:** Python (Blender addon)
- **What it does:** Two floor plan generation methods: **grid placement** and **squarified treemaps**. Creates logical room connections, extrudes walls, cuts doors/windows, generates materials.
- **Algorithms:**
  - **Grid placement:** Rooms placed on 2D grid, expand competitively
  - **Squarified treemaps:** Recursive subdivision of rectangular floor plan into room-sized rectangles. Better aspect ratios than naive BSP.
- **Handles:** Single-floor interiors with walls, doors, windows, materials. No multi-floor.
- **What we could borrow:** **HIGH VALUE.** Squarified treemaps is an excellent algorithm for room subdivision that we haven't explored. It naturally produces rooms with good aspect ratios (unlike BSP which often creates long thin rooms). The competitive grid expansion is similar to constrained growth. Both algorithms are implementable in our grid system. The door/window cutting via boolean is exactly our approach.

### s-leger/archipack
- **URL:** https://github.com/s-leger/archipack
- **Stars:** 377 | **License:** GPL-3.0 | **Updated:** Apr 2018
- **Language:** Python (Blender addon)
- **What it does:** Parametric architectural modeling: walls, windows, doors, stairs, roofs, floors, fences, blinds, molding, kitchens, trusses, slabs. Each element is a separate parametric object.
- **Handles:** Individual architectural elements -- all of them. Walls with openings, window variants, door variants, stair variants, roof styles. Not a building *generator* but a toolkit of parametric parts.
- **What we could borrow:** **MEDIUM VALUE.** The parametric definitions for each architectural element (window proportions, stair geometry, roof profiles) are reference implementations. The way Archipack separates each element into its own parametric generator matches our per-component approach. Stair geometry parameters (riser height, tread depth, landing proportions) are validated against real construction codes.

### nortikin/prokitektura-blender
- **URL:** https://github.com/nortikin/prokitektura-blender
- **Stars:** ~50 | **License:** GPL-3.0
- **Language:** Python (Blender)
- **What it does:** CGA-inspired shape grammar building generation. Small Python rule functions iteratively refine building models.
- **What we could borrow:** CGA-in-Python implementation reference. Shows how to express shape grammar rules as simple functions.

### josauder/procedural_city_generation
- **URL:** https://github.com/josauder/procedural_city_generation
- **Stars:** 587 | **License:** MPL-2.0
- **Language:** Python (visualized in Blender)
- **What it does:** Full city pipeline: terrain -> roads (growth-based from axiom) -> blocks -> lots -> buildings. Buildings parameterized by population density (height, textures).
- **What we could borrow:** Growth-based road generation algorithm. Lot-to-building parameter mapping based on zone density.

### lsimic/ProceduralBuildingGenerator
- **URL:** https://github.com/lsimic/ProceduralBuildingGenerator
- **Stars:** 40 | **License:** GPL-3.0
- **Language:** Python (Blender addon)
- **What it does:** Building generation addon (bachelor's thesis). Parameter-driven building creation.
- **What we could borrow:** Limited documentation but thesis may contain algorithm details.

### aaronjolson/Blender-Python-Procedural-Level-Generation
- **URL:** https://github.com/aaronjolson/Blender-Python-Procedural-Level-Generation
- **Language:** Python (Blender)
- **What it does:** Scripts for generating 3D dungeon environments using Blender's Python API.
- **What we could borrow:** Blender mesh generation patterns that map to GeometryScript equivalents.

---

## 4. Engine-Agnostic / Standalone

### tudelft3d/Random3Dcity
- **URL:** https://github.com/tudelft3d/Random3Dcity
- **Stars:** 237 | **License:** MIT | **Updated:** Sep 2022 (archived)
- **Language:** Python
- **What it does:** Procedural building generation in CityGML with **16 LOD levels**. Generates basic interiors (3 LODs: solid per floor, 2D per storey, full building). 5 roof types (flat, gabled, hipped, pyramidal, shed). Garages, alcoves. **392 different CityGML representations per building** through permutation of LOD, semantics, geometry variants, and indoor representations.
- **What we could borrow:** LOD strategy for procedural buildings. The idea of 392 variants from combinatorial parameters is powerful. Interior LOD approach (full detail -> floor solids -> single box).

### kchapelier/procedural-generation
- **URL:** https://github.com/kchapelier/procedural-generation
- **Stars:** ~300 | **License:** Various
- **What it does:** Curated list of procedural generation resources, mostly JavaScript-focused. Extensive categorization.
- **What we could borrow:** Reference list for finding more implementations.

---

## 5. Wave Function Collapse Projects

### mxgmn/WaveFunctionCollapse (The Original)
- **URL:** https://github.com/mxgmn/WaveFunctionCollapse
- **Stars:** 23,000+ | **License:** MIT
- **Language:** C#
- **What it does:** The original WFC implementation. Bitmap and tilemap generation from a single example. 2D only but foundational.
- **Ports exist in:** C++, Python, Kotlin, Rust, Julia, Go, Haxe, Java, Clojure, Dart, JavaScript, adapted to Unity, UE5, Godot 4, Houdini
- **What we could borrow:** Core WFC algorithm if we ever want detail-level tile placement (e.g., wall decorations, facade details).

### marian42/wavefunctioncollapse (covered above in Unity section)
- **Key insight:** WFC works best for **detail assembly** (which specific tile variant goes here) rather than **layout generation** (where rooms go). Townscaper/Bad North use it for visual variety, not structural planning.

### Primarter/WaveFunctionCollapse
- **URL:** https://github.com/Primarter/WaveFunctionCollapse
- **Stars:** ~20
- **What it does:** 3D WFC in Blender and Unity. Blender used to define adjacency data.
- **What we could borrow:** Blender-side adjacency authoring workflow.

---

## 6. Floor Plan & Layout Generators

### hellguz/Magnetizing_FloorPlanGenerator
- **URL:** https://github.com/hellguz/Magnetizing_FloorPlanGenerator
- **Stars:** 67 | **License:** Not specified
- **Language:** C# (98%, Rhino/Grasshopper)
- **What it does:** Automated floor plan generation for public buildings using "magnetizing" approach with quasi-evolutionary optimization. Room placement considering adjacencies, circulation generation, iterative refinement.
- **Algorithms:** Evolutionary optimization, room magnetization, adjacency constraints
- **From:** Bauhaus-University Weimar
- **What we could borrow:** **MEDIUM VALUE.** The evolutionary optimization approach for room placement could work as a secondary strategy when constrained growth fails. The adjacency-aware placement matches our room adjacency requirements from real_floorplan_patterns_research.md.

### tyrvi/evo_layout
- **URL:** https://github.com/tyrvi/evo_layout
- **Stars:** ~30 | **License:** Not specified
- **What it does:** Evolutionary floor plan layout using NEAT (Neuroevolution of Augmenting Topologies).
- **What we could borrow:** Novel approach but too complex for our needs. Interesting as future research.

### z-aqib/Floor-Plan-Generator-Using-AI
- **URL:** https://github.com/z-aqib/Floor-Plan-Generator-Using-AI
- **Language:** Python + Java (via Jython)
- **What it does:** Floor plans via Constraint Satisfaction Problem (CSP) trees.
- **What we could borrow:** CSP approach reference for room placement.

### HanHan55/Graph2plan
- **URL:** https://github.com/HanHan55/Graph2plan
- **What it does:** ML-based floor plan generation from layout graphs + building boundaries. Deep neural network.
- **What we could borrow:** Interesting but ML approach is overkill for runtime game generation.

---

## 7. Furniture / Interior Decoration

### david-dot/FittingPlacer + FittingPlacerForUnity
- **URL:** https://github.com/david-dot/FittingPlacer | https://github.com/david-dot/FittingPlacerForUnity
- **Stars:** 13 | **License:** MIT + CC0
- **Language:** C# 6.0
- **What it does:** Automatic furniture placement in rectangular rooms. Registers furniture sides and wall sides, connects them at defined distances. Checks overlaps and clearance zones around furniture, windows, doors.
- **Algorithm:** Side-matching with overlap/clearance constraint checking
- **What we could borrow:** **MEDIUM VALUE.** The side-registration concept (furniture back against wall, desk facing window) is a practical furniture placement algorithm. The clearance zone concept maps to our door/window clearance requirements. Could adapt for our `create_furniture` action's placement logic.

---

## 8. Dungeon Generators (Relevant Algorithms)

### AtTheMatinee/dungeon-generation
- **URL:** https://github.com/AtTheMatinee/dungeon-generation
- **Stars:** ~300 | **License:** MIT
- **Language:** Python
- **What it does:** Demonstrates multiple dungeon algorithms including BSP, cellular automata, random walk, Nystrom's Rooms and Mazes, and drunkard's walk.
- **What we could borrow:** Side-by-side algorithm comparison. Clean implementations of BSP and Nystrom.

### aleksandrbazhin/mazegen
- **URL:** https://github.com/aleksandrbazhin/mazegen
- **Stars:** ~30 | **License:** MIT
- **Language:** C++ (header-only library)
- **What it does:** Maze generation based on Nystrom's algorithm. Header-only, easily integrable.
- **What we could borrow:** **MEDIUM VALUE.** Header-only C++ maze generation that we could directly include for corridor/hallway maze patterns.

### fdefelici/dungen-unity
- **URL:** https://github.com/fdefelici/dungen-unity
- **What it does:** Cellular Automata mixed with Room/Corridor concept. Interesting hybrid.
- **What we could borrow:** Cellular automata + room hybrid approach for organic cave-like basement areas.

### JohnAndrewTaylor/BSP-Dungeon-Generator
- **URL:** https://github.com/JohnAndrewTaylor/BSP-Dungeon-Generator
- **Language:** C# (Unity)
- **What it does:** Classic BSP dungeon with rooms and corridor connections.
- **Algorithm:** Recursive subdivision -> room placement in leaves -> corridor connection
- **What we could borrow:** Clean BSP implementation reference.

---

## 9. Shape Grammar / CGA Implementations

### stefalie/shapeml
- **URL:** https://github.com/stefalie/shapeml
- **Stars:** 86 | **License:** GPL-3.0 | **Updated:** Jan 2021
- **Language:** C++ (99%)
- **What it does:** Grammar-based procedural 3D modeling framework. Inspired by Shape Grammars, L-Systems, CGA/CityEngine, G2. Includes ShapeMaker interactive preview, PBR renderer, OBJ export.
- **Key operations:** `splitRepeatX`, `splitRepeatY` for facade subdivision into window tiles. Parallel rewriting systems.
- **Handles:** Exterior facades with windows, shutters, roofs. No interiors.
- **What we could borrow:** **MEDIUM VALUE.** The grammar definition syntax and split/repeat operations are exactly what CGA does. Reference for implementing our own shape grammar rules. GPL-3.0 means we can study but not copy code.

### pvallet/CGA_interpreter
- **URL:** https://github.com/pvallet/CGA_interpreter
- **Stars:** 15 | **License:** Not specified
- **Language:** C++ (84%) + Yacc/Lex parser (14%)
- **What it does:** CGA/CGA++ grammar interpreter. Reads grammar files, produces .obj meshes.
- **What we could borrow:** Grammar parsing approach. Uses Yacc/Lex for CGA rule parsing which shows the full formal grammar approach.

### santipaprika/procedural-buildings
- **URL:** https://github.com/santipaprika/procedural-buildings
- **Stars:** 3 | **License:** GPL-3.0
- **Language:** C++ (96%)
- **What it does:** Shape grammar generator with scope-based transformations. 3 primitives (cube, cylinder, sphere). Probabilistic rules, space subdivision, PLY output.
- **Grammar features:** Translation, rotation, scaling, probabilistic branching, axis-aligned subdivision, random parameter ranges
- **What we could borrow:** Scope-based transformation system. Shows minimal viable shape grammar in C++.

### LudwikJaniuk/cga-shape
- **URL:** https://github.com/LudwikJaniuk/cga-shape
- **What it does:** Simplified CGA-shape grammar implementation.
- **What we could borrow:** Minimal CGA reference.

---

## 10. Blog Posts & Tutorials

### Martin Evans -- "Procedural Generation For Dummies" Series
- **URL:** https://martindevans.me/game-development/2015/12/11/Procedural-Generation-For-Dummies/
- **Building Footprints:** https://martindevans.me/game-development/2016/05/07/Procedural-Generation-For-Dummies-Footprints/
- **Lot Subdivision:** https://martindevans.me/game-development/2015/12/27/Procedural-Generation-For-Dummies-Lots/
- **Facades:** https://martindevans.me/heist-game/2015/05/29/Procedural-Generation-Of-Facades-And-Other-Miscellania/
- **Key insights for Monolith:**
  - Two footprint approaches: **additive** (grow from seed -- good for houses) vs **subtractive** (start with lot, slice off -- good for commercial)
  - Per-floor footprints: higher floors receive previous floor's shape as input, allowing natural narrowing
  - Shape operation pipeline: bevel -> invert corners -> shrink -> twist -> clip, chained as simple functions
  - Conditional operations with area threshold checks
  - Metadata key-value system for inter-generator communication
  - **Directly applicable** to our building footprint generation. The subtractive approach with per-floor reduction maps to our create_building pipeline.

### Shadows of Doubt DevBlog #13 -- Creating Procedural Interiors
- **URL:** https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/
- **Also:** DevBlog #21 (How Voxels Saved the Project), DevBlog #30 (Top Development Challenges)
- **Key insights for Monolith:**
  - **Tile grid:** 1.8m x 1.8m tiles (we use 0.5m = 50cm, much finer)
  - **Hierarchy:** City > District > Block > Building > Floor > Address > Room > Tile (matches our spatial registry)
  - **Floorplan tool:** Designer-authored templates sectioned into "addresses" (apartments, offices, etc.)
  - **Hallway generation:** If entrance too far from corner, auto-draw hallway. Critical for preventing rooms-only-accessible-through-other-rooms.
  - **Room priority system:** Cycle through rooms by importance (living room first, then bathroom, then bedrooms...). For each, simulate placement, rank by: floor space suitability, uniform shape (corner count), window availability. Pick best.
  - **Room stealing:** Oversized rooms can have space "stolen" by later rooms.
  - **Connection rules:** Bathrooms = single door only, kitchens can connect to living rooms, entrances never open into bedrooms.
  - **THIS IS THE GOLD STANDARD** for game building interior generation. Our room placement should follow this priority-based scoring approach.

### Bob Nystrom -- "Rooms and Mazes: A Procedural Dungeon Generator"
- **URL:** https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/
- **Key insights:** Place random rooms first, then fill remaining space with maze corridors, then connect rooms to corridors with doors, then remove dead-ends. Simple but effective.
- **Directly applicable** to corridor-heavy building types (hospitals, schools).

### Vazgriz -- "Procedurally Generated Dungeons"
- **URL:** https://vazgriz.com/119/procedurally-generated-dungeons/
- **Covered in detail** under Unity repos above. Essential reading for multi-floor staircase placement.

### Marian42 -- "Infinite Procedurally Generated City"
- **URL:** https://marian42.de/article/wfc/
- **Also:** https://marian42.de/article/infinite-wfc/ (infinite extension)
- **Key insights:** WFC for city-scale generation. Chunked generation near player. Module system with symmetry rules.

### 80.lv -- "Make Procedural Buildings in Houdini Using WFC"
- **URL:** https://80.lv/articles/learn-to-make-procedural-buildings-in-houdini-using-wave-function-collapse-algorithm
- **Key insight:** WFC in Houdini for building detail. Entropy-based piece selection. Corner locations have lowest entropy (most constrained).

### SideFX -- "Procedural City: Building Generator"
- **URL:** https://www.sidefx.com/tutorials/procedural-city-1-building-generator/
- **Key insight:** Houdini building generators work big-to-small: building shell -> floor division -> facade modules -> detail scattering. Box with face attributes controlling subnet application. Layers and layers of geometry scattering to points.

### kiryha/Houdini Wiki -- Procedural City
- **URL:** https://github.com/kiryha/Houdini/wiki/procedural-city
- **Key insight:** Facade = initial shape -> vertical splits (floors) -> horizontal splits (modules) -> element placement (walls, windows, doors). This is the canonical CGA pipeline.

### UE5 PCG Modular Building Tutorial
- **URL:** https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation
- **Key insight:** PCG framework for modular piece placement. Not geometry generation but instance placement. Works for kit-based buildings.

---

## 11. GDC Talks

### "Building Blocks: Artist Driven Procedural Buildings"
- **URL:** https://gdcvault.com/play/1012655/Building-Blocks-Artist-Driven-Procedural
- **Key insights:** Rule types: splitting, repeating, occlusion tests, edge-angle tests, height tests. Auto-LOD generation. Texturing for low-LOD buildings. **CGA in games context.**

### "Creating FPS Open Worlds Using Procedural Techniques"
- **URL:** https://gdcvault.com/play/1020340/Creating-FPS-Open-Worlds-Using
- **Key insights:** Procedural methods for FPS open world: terrain, architecture, prop placement. Data structures and storage. **Most directly relevant to Leviathan as an FPS.**

### "Constructing the Catacombs: Procedural Architecture for Platformers"
- **URL:** https://gdcvault.com/play/1021877/Constructing-the-Catacombs-Procedural-Architecture
- **Key insights:** Procedural architecture for Spelunky-like games. Room template selection and connectivity.

### "Practical Procedural Generation for Everyone"
- **URL:** https://www.gdcvault.com/play/1024213/Practical-Procedural-Generation-for
- **Key insights:** Overview of PCG techniques, simple algorithms, scalable data structures.

### "Continuous World Generation in No Man's Sky"
- **URL:** https://www.gdcvault.com/play/1024265/Continuous-World-Generation-in-No
- **Key insights:** Real-time generation pipeline, voxel-based, LOD streaming. Scale is different from buildings but streaming concepts apply.

---

## 12. Academic Papers with Code

### Freiknecht & Effelsberg (2020) -- "Procedural Generation of Multistory Buildings With Interior"
- **Published:** IEEE Transactions on Games, Vol 12, pp 323-336
- **URL:** https://ieeexplore.ieee.org/document/8926482/
- **Key algorithm:** Bottom-up approach. Buildings defined as sets of rooms connected by doors/stairways. Growth algorithm with automatic corridor detection and stairwell placement. Multi-floor elements (elevator shafts, staircases) placed first, then floors subdivided. Adjacent room constraints, coherency checker.
- **Code:** Not openly available but algorithm is fully described
- **Relevance:** **CRITICAL REFERENCE.** This is the most comprehensive academic treatment of exactly what we're building. Their stairwell-first approach, corridor detection, and coherency checker directly inform our architecture.

### Lopes, Tutenel, Smelik, de Kraker, Bidarra (2010) -- "A Constrained Growth Method for Procedural Floor Plan Generation"
- **Published:** GAMEON'10, TU Delft
- **URL:** https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf
- **Key algorithm:** Grid-based constrained growth. Place initial room positions, grow rooms to maximal feasible rectangular size. Adjacency and connectivity constraints. Multi-floor support.
- **Relevance:** **HIGH.** The grid-based growth method is what we implement. Their constraint system (adjacency, connectivity, reachability) maps to our validation pipeline.

### Dahl & Rinde -- "Procedural Generation of Indoor Environments"
- **URL:** https://www.cse.chalmers.se/~uffe/xjobb/Lars%20Rinde%20o%20Alexander%20Dahl-Procedural%20Generation%20of%20Indoor%20Environments.pdf
- **Published:** Chalmers University thesis
- **Key insight:** Comprehensive survey of indoor generation methods with implementation.

### Taljsten (2021) -- "Exploring Procedural Generation of Buildings"
- **URL:** https://www.diva-portal.org/smash/get/diva2:1480518/FULLTEXT01.pdf
- **Key insight:** Comparison of multiple building generation approaches.

### Proc-GS (2024) -- "Procedural Building Generation for City Assembly with 3D Gaussians"
- **URL:** https://arxiv.org/html/2412.07660v1
- **Key insight:** Combines procedural building code with 3D Gaussian Splatting. Repetitive facade patterns (windows, doors) recognized and proceduralized. Novel but rendering-technique-specific.

---

## 13. Key Takeaways for Monolith

### Algorithms Worth Adopting (Priority Order)

1. **Shadows of Doubt priority-based room scoring** -- Room placement by cycling through priority list, scoring each placement candidate. Already partially in our design but the scoring criteria (floor space, shape uniformity, window access) should be formalized.

2. **Squarified Treemaps** (from wojtryb) -- Better room aspect ratios than BSP. Recursive subdivision that optimizes for square-ish rooms. Should be an alternative to our current grid-based approach for certain building types.

3. **Vazgriz 3D staircase A*** -- Modified A* with path history for stair cell jumping. Directly solves our stairwell cutout problem (stairwell_cutout_research.md).

4. **ProceduralToolkit FacadePlanner/Constructor pattern** -- Strategy pattern separating planning from construction. Apply to our facade system.

5. **Martin Evans per-floor footprint reduction** -- Higher floors receive previous floor's shape. Simple but produces natural building silhouettes.

6. **Edgar graph-first layout** -- Define room connectivity graph, then solve spatial layout. For designer-controlled building templates.

### Window Integration Approaches Found

| Approach | Used By | How |
|----------|---------|-----|
| **Boolean subtract** | wojtryb (Blender), our system | Cut opening in wall mesh. Works but slow at scale. |
| **Pre-cut modular tiles** | marian42 WFC, Houdini tutorials | Wall tiles with window holes pre-modeled. Fastest at runtime. |
| **Shape grammar split** | ShapeML, CGA, ProceduralToolkit | Subdivide wall face into window/wall/sill panels. No booleans. |
| **Face replacement** | SoD, various | Wall face tagged, replaced with window mesh at render time. |

**Recommendation:** Our boolean approach works for bespoke buildings. For mass city generation, consider pre-split facade panels (shape grammar style) to avoid boolean performance cost.

### Room Layout Approaches Found

| Algorithm | Source | Strength | Weakness |
|-----------|--------|----------|----------|
| **Constrained growth** | Bidarra/TU Delft | Flexible, any building shape | Can produce odd room shapes |
| **Priority-based scoring** | Shadows of Doubt | Most game-proven, good control | Requires careful priority tuning |
| **BSP subdivision** | Classic dungeon gen | Fast, guaranteed coverage | Long thin rooms, less natural |
| **Squarified treemaps** | wojtryb thesis | Best aspect ratios | Less control over adjacency |
| **Graph-first + templates** | Edgar | Best designer control | Limited room count |
| **Evolutionary/magnetizing** | Bauhaus-Weimar | Optimizes for adjacency | Slow, non-deterministic |

**Recommendation:** Hybrid approach. Use priority-based scoring (SoD) as primary, with squarified treemaps as fallback for simple rectangular buildings. Graph-first for manually-designed key locations.

### Multi-Floor / Staircase Approaches Found

| Approach | Source | Details |
|----------|--------|---------|
| **Stairwell-first** | Freiknecht (2020) | Place stairs/elevators before room subdivision. All floors share stairwell positions. |
| **3D A* pathfinding** | Vazgriz | Modified A* finds stair positions connecting floor graphs. Stair cells "jumped" in pathfinding. |
| **Template duplication** | Freiknecht | Stair room placed on ground floor, duplicated to same position on subsequent floors. |
| **Voxel pillar** | Shadows of Doubt | Stairwell occupies a column of tiles across all floors. |

**Recommendation:** Adopt stairwell-first placement from Freiknecht. Reserve a column of cells in the grid before room subdivision begins. Our 50cm grid with minimum 4x6 cell stairwell (from stairwell_cutout_research.md) matches their approach.

### Facade Approaches Found

| Approach | Source | Details |
|----------|--------|---------|
| **Horizontal/Vertical layout** | ProceduralToolkit | Recursive layout objects splitting facade into panels |
| **CGA split/repeat** | ShapeML, CGA_interpreter, Vitruvio | Formal grammar rules subdividing shapes |
| **Template modules** | Houdini, WFC | Pre-built facade pieces assembled via rules |
| **Direct mesh** | UE5-Procedural-Building | Box primitives placed as facade elements |

**Recommendation:** Our facade_window_research.md design (JSON-based split/repeat rules) is sound. ProceduralToolkit's C# implementation is the closest reference for coding it.

### Repos to Star and Monitor

1. **PCGEx/PCGExtendedToolkit** (599 stars, MIT, very active) -- Graph algorithms in UE5 C++
2. **shun126/DungeonGenerator** (129 stars, active) -- UE5 3D dungeon with stairs
3. **Syomus/ProceduralToolkit** (2.9k stars, MIT) -- Building facade system architecture
4. **OndrejNepozitek/Edgar-DotNet** (MIT) -- Graph-based layout solver
5. **marian42/wavefunctioncollapse** (4.8k stars, MIT) -- WFC city with interiors

### What Nobody Has (Gaps in Open Source)

- **Full building pipeline from floor plan to furnished interior in UE5** -- Nobody has this. Closest is Erisbv's 2017 UE4 project and shun126's dungeon plugin.
- **Horror-specific procedural buildings** -- Zero repos address this. Dead-end ratios, sight-line control, dread pacing -- all unique to us.
- **Terrain-adaptive building foundations** -- Only Houdini tutorials cover this. No game engine implementation found.
- **Accessibility-validated procedural layouts** -- Nobody validates door widths, ramp slopes, or capsule clearance in proc-gen. We're pioneering this.
- **GeometryScript building interiors** -- bendemott's repo is exterior-only. We are likely the first to do full interior generation with GeometryScript in C++.

---

## Appendix: Quick Reference Table

| Repository | Stars | Engine | License | Exteriors | Interiors | Multi-Floor | Stairs | Facades | Windows | Doors |
|------------|-------|--------|---------|-----------|-----------|-------------|--------|---------|---------|-------|
| bendemott/UE5-Procedural-Building | 35 | UE5 | MIT | Y | N | N | N | Basic | N | N |
| Erisbv/UNREAL-Procedural-Cities | 47 | UE4 | MIT | Y | Y | Y | ? | Y | ? | ? |
| shun126/DungeonGenerator | 129 | UE5 | GPL | Y | Y | Y | Y | N | N | Y |
| Esri/cityengine_for_unreal | 257 | UE5 | Apache | Y | ? | Y | N | Y | Y | Y |
| PCGEx/PCGExtendedToolkit | 599 | UE5 | MIT | N/A | N/A | N/A | N/A | N/A | N/A | N/A |
| Syomus/ProceduralToolkit | 2900 | Unity | MIT | Y | N | Y | N | Y | Y | N |
| marian42/wavefunctioncollapse | 4800 | Unity | MIT | Y | Y | Y | ? | Y | Y | Y |
| Edgar-Unity | 894 | Unity | MIT | N/A | Y (2D) | N | N | N/A | N/A | Y |
| vazgriz/DungeonGenerator | 804 | Unity | MIT | N | Y | Y | Y | N | N | Y |
| wojtryb/Procedural-Building-Gen | 38 | Blender | MIT | Y | Y | N | N | Y | Y | Y |
| s-leger/archipack | 377 | Blender | GPL | Y | N | Y | Y | Y | Y | Y |
| tudelft3d/Random3Dcity | 237 | Python | MIT | Y | Y | Y | N | Y | Y | Y |
| stefalie/shapeml | 86 | Standalone | GPL | Y | N | N | N | Y | Y | N |
| josauder/procedural_city_gen | 587 | Python | MPL | Y | N | N | N | Y | N | N |

---

*Research conducted 2026-03-28. Star counts and update dates verified at time of research.*
