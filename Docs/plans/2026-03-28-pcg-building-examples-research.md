# PCG Building/City Examples in UE5 -- Comprehensive Research

**Date:** 2026-03-28
**Status:** Complete
**Scope:** Every findable example of UE5 PCG framework used for buildings, cities, interiors, or architectural generation

---

## Table of Contents

1. [Epic Official Resources](#1-epic-official-resources)
2. [Conference Talks (Unreal Fest 2025)](#2-conference-talks)
3. [Community Tutorials -- Buildings](#3-community-tutorials-buildings)
4. [Community Tutorials -- Cities/Streets](#4-community-tutorials-cities)
5. [Community Tutorials -- Interiors/Rooms](#5-community-tutorials-interiors)
6. [Community Tutorials -- Dungeons](#6-community-tutorials-dungeons)
7. [PCG + GeometryScript Combined](#7-pcg-geometryscript-combined)
8. [Marketplace/Fab Products](#8-marketplace-products)
9. [Blog Posts and Written Guides](#9-blog-posts)
10. [ArtStation Showcases](#10-artstation-showcases)
11. [Forum Discussions](#11-forum-discussions)
12. [Books](#12-books)
13. [Open Source / GitHub](#13-open-source)
14. [Key Findings -- What PCG Can and Cannot Do](#14-key-findings)
15. [Relevance to Monolith](#15-relevance-to-monolith)

---

## 1. Epic Official Resources

### 1a. Cassini Sample Project (CRITICAL -- First Official PCG Building)
- **URL:** https://www.fab.com/listings/3f7cd12c-30b3-47d6-90c2-8604ed068ab7
- **Announcement:** https://www.unrealengine.com/en-US/news/the-cassini-sample-project-is-now-available
- **What:** Epic's FIRST official use of PCG to generate buildings and complex artificial environments. A modular space station built using Shape Grammar.
- **PCG Features Used:** Shape Grammar (spline & primitive workflows), Geometry Processing, GPU Compute, GPU Scene Instancing, Pathfinding, Raycasts, Spline Meshes, Instance Data Packer
- **Uses Modular Pieces:** Yes -- modular space station segments assembled via grammar rules
- **Quality Level:** Production reference (official Epic sample)
- **Source Available:** Yes -- downloadable sample project
- **Interiors:** Space station corridors and rooms (not residential/building interiors)
- **Limitations:** Space station context, not traditional architecture. Grammar corridors use Instance Data Packer for material variation. Fixed modular vocabulary.
- **Key Detail:** The `Grammar` PCG graph generates corridors with properties packed via Instance Data Packer into Static Mesh Spawner nodes for per-instance material variation.

### 1b. Electric Dreams Environment Sample
- **URL:** https://www.unrealengine.com/en-US/electric-dreams-environment
- **Docs:** https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine
- **What:** 4km x 4km jungle environment, mixes procedural + hand-crafted. Demonstrates PCG + Substrate + Lumen + Nanite + Soundscape.
- **PCG Features Used:** Landscape scatter, biome rules, real-time adjustment
- **Uses Modular Pieces:** Quixel assets for vegetation/rocks
- **Quality Level:** Production showcase
- **Buildings:** NO -- purely natural environment (jungle). No buildings or architecture.
- **Source Available:** Yes -- free downloadable project

### 1c. City Sample (Matrix Awakens Demo)
- **URL:** https://www.fab.com/listings/4898e707-7855-404b-af0e-a505ee690e68
- **Docs:** https://dev.epicgames.com/documentation/en-us/unreal-engine/city-sample-project-unreal-engine-demonstration
- **What:** Massive city from The Matrix Awakens experience
- **PCG Usage:** Does NOT use the PCG framework. Uses Rule Processor (a separate, older Epic internal tool), World Partition, Nanite, Lumen, Mass AI, Chaos.
- **Buildings:** Pre-made building assets placed via Rule Processor, not PCG Grammar
- **Source Available:** Yes -- free download
- **Key Insight:** The City Sample's buildings are NOT PCG-generated. They use a custom Rule Processor pipeline that is separate from the PCG framework.

### 1d. PCG Biome Core Plugin (Official)
- **URL:** https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-in-unreal-engine
- **What:** Official Epic plugin showing Attribute Set Tables, Feedback loops, Recursive Sub-graphs, Runtime generation
- **Buildings:** Biome-focused (vegetation, terrain), not buildings. But the "Buildings and Biomes" Learning Lab extends this concept to include building placement within biomes.

### 1e. Official Shape Grammar Documentation
- **URL:** https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine
- **What:** Official UE 5.7 docs on Shape Grammar in PCG
- **Key Concept:** Grammar rules take a "box" (volume) and subdivide it into sub-boxes, each assigned to different rules. Special rules spawn props/meshes at box locations. This is the CGA-style shape grammar approach directly in PCG.

### 1f. PCG Node Reference
- **URL:** https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine
- **Key Nodes for Buildings:** Static Mesh Spawner (with PCGMeshSelectorByAttribute), Spline Sampler, Grammar Execute, Grammar Selection, Surface Sampler, Transform Points, Self Pruning, Density Filter, Merge

---

## 2. Conference Talks

### 2a. "Leveraging PCG for Building and City Creation" -- Daniel Mor, Virtuos (Unreal Fest Bali 2025)
- **URL:** https://dev.epicgames.com/community/learning/talks-and-demos/Z1wa/unreal-engine-leveraging-pcg-for-building-and-city-creation
- **Forum:** https://forums.unrealengine.com/t/talks-and-demos-leveraging-pcg-for-building-and-city-creation/2705278
- **Duration:** 49 minutes
- **What:** In-depth walk-through of creating dynamic cities using PCG. Covers crafting modular assets, designing flexible grammar rules, packaging Blueprints, optimizing workflows.
- **Key Topics:** Modular asset design for PCG, grammar rules for urban environments, workflow optimization
- **Quality:** Conference-grade presentation from Virtuos (AAA outsourcing studio)

### 2b. "Buildings and Biomes PCG" -- Learning Lab (Unreal Fest 2025)
- **URL:** https://dev.epicgames.com/community/learning/talks-and-demos/pBl1/unreal-engine-unreal-fest-2025-buildings-and-biomes-pcg
- **Forum:** https://forums.unrealengine.com/t/talks-and-demos-unreal-fest-2025-buildings-and-biomes-pcg/2656776
- **What:** Integration of PCG + Biomes plugin for dynamic environments including buildings
- **Focus:** Modular, adaptable systems for efficiency and creative control

### 2c. "Procedural Roads and Buildings in PCG" -- Chris Murphy, Epic
- **URL:** https://forums.unrealengine.com/t/talks-and-demos-procedural-roads-and-buildings-in-pcg/2705285
- **What:** Epic Senior Technical Artist demonstrating PCG for urban biome generation
- **Key Topics:** PCG Biome Core, runtime vs static serialization, source control contention, performance
- **Quality:** Official Epic presentation

### 2d. GDC -- "Developing Large Procedural Systems" -- Chris Murphy, Epic
- **URL:** https://schedule.gdconf.com/session/developing-large-procedural-systems-with-low-friction-and-fast-generation-presented-by-epic-games/917366
- **What:** Key concepts for large UE projects using PCG, including performance and workflow tips

---

## 3. Community Tutorials -- Buildings

### 3a. UE5 PCG Modular Building Tutorial (Nov 2025)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-ue5-pcg-modular-building-tutorial-procedural-content-generation/2679812
- **What:** Full modular building workflow using PCG
- **Covers:** Theory, PCG Graph fundamentals, generating full modular structure, ceilings, custom box colliders for doors/windows
- **Interiors:** Exterior structure with door/window placement via collider volumes
- **Modular Pieces:** Yes -- swappable modular wall/floor/ceiling pieces
- **Source:** Tutorial walkthrough (video + project likely)

### 3b. Intermediate PCG Building System (UE 5.6, Nov 2025)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/rpDx/unreal-engine-5-6-intermediate-pcg-building-system-tutorial
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-unreal-engine-5-6-intermediate-pcg-building-system-tutorial/2680488
- **What:** Production-ready Procedural Building System
- **Covers:** Flexible building system, logic behind each decision, multi-wall styles, stories with staircases
- **Quality Level:** Intermediate/production-ready
- **Key Detail:** Includes staircases and multi-story support

### 3c. Realistic PCG Buildings Generation
- **URL:** https://dev.epicgames.com/community/learning/tutorials/4JWW/unreal-engine-realistic-pcg-buildings-generation
- **What:** Realistic-looking buildings via PCG
- **Quality Level:** Production visual quality target

### 3d. PCG Grammar Tutorial -- Procedural Building Generator (UE 5.5)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/9d3a/unreal-engine-pcg-grammer-tutorial-procedural-building-generator
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-pcg-grammer-tutorial-procedural-building-generator/2153986
- **What:** Step-by-step PCG Grammar for building generation
- **PCG Nodes:** Grammar execution nodes, Grammar selection, Static Mesh Spawner
- **Approach:** Shape Grammar rules subdivide building volume into floors, walls, corners

### 3e. Advanced PCG Grammar -- Tile-by-Tile Roof Generation (UE 5.5)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/nzVe/unreal-engine-5-5-advanced-pcg-grammar-tutorial-tile-by-tile-building-roof-generation
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-unreal-engine-5-5-advanced-pcg-grammar-tutorial-tile-by-tile-building-roof-generation/2230234
- **What:** Advanced grammar for roof generation tile by tile, builds on building tutorial
- **Quality Level:** Advanced, detailed roof construction

### 3f. Introduction to PCG Grammar (UE 5.5)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/PYEX/introduction-to-pcg-grammar-in-unreal-engine-5-5
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-introduction-to-pcg-grammar-in-unreal-engine-5-5/2254055
- **What:** Foundational grammar syntax, using splines to spawn meshes through grammar

### 3g. PCG Building in Unreal Engine
- **URL:** https://dev.epicgames.com/community/learning/tutorials/pvEw/pcg-building-in-unreal-engine
- **What:** General PCG building tutorial

### 3h. Creating Modular Walls with Gates Using Splines & PCG
- **URL:** https://forums.unrealengine.com/t/creating-modular-walls-with-gates-using-splines-pcg-in-unreal-engine-5-tutorial-preview/2090382
- **What:** Modular walls with gate insertion points via splines + PCG
- **Modular Pieces:** Yes -- wall segments with gates
- **Use Case:** Building perimeters, compound walls, fences

### 3i. PCG Grammar Generator Tool (Community-Made)
- **URL:** https://forums.unrealengine.com/t/created-unreal-engine-pcg-grammar-generator-tool/2531546
- **What:** Custom tool for PCG grammar-based building generation

### 3j. Making Modular Buildings Completely with PCG (YouTube)
- **URL:** https://forums.unrealengine.com/t/making-modular-buildings-completely-with-pcg/2134609
- **What:** YouTube tutorial on full modular buildings using only PCG

---

## 4. Community Tutorials -- Cities/Streets

### 4a. "City Streets Can Be Easy in UE5 Using PCG" (Jan 2026)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/VxP9/unreal-engine-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg/2693391
- **What:** PCG + PCGEx plugin for city streets, roads, intersections
- **Key:** Uses PCGEx for graph-based road network generation

### 4b. Master UE5 PCG: Vegetation & City Generation (Nov 2025)
- **URL:** https://forums.unrealengine.com/t/community-tutorial-master-ue5-pcg-procedural-vegetation-city-generation-tutorial-beginner-friendly/2677326
- **What:** Beginner-friendly, covers spawning buildings, trees, grasses, excluding roads/city plots
- **Approach:** PCG rules for plot exclusion zones, building density control

### 4c. Create Amazing Procedural Towns with PCG + Blueprints + Cargo
- **URL:** https://dev.epicgames.com/community/learning/tutorials/dXR7/unreal-engine-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo/1236888
- **What:** PCG towns using Blueprint actors and the Cargo system

### 4d. Create Entire Cities with PCG Splines
- **URL:** https://dev.epicgames.com/community/learning/tutorials/Obk3/create-entire-cities-automatically-with-pcg-splines-procedural-content-generation-in-unreal-engine
- **What:** Spline-driven city generation

### 4e. Procedural Road Generation in UE5 (PCG)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/9dpd/procedural-road-generation-in-unreal-engine-5-pcg
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-procedural-road-generation-in-unreal-engine-5-pcg/2254453
- **What:** Road network generation using PCG

### 4f. Dynamic Splines from PCG for Sidewalk Generation
- **URL:** https://dev.epicgames.com/community/learning/tutorials/Zeov/unreal-engine-create-dynamic-splines-from-pcg-to-generate-a-sidewalk
- **What:** Creating splines procedurally from PCG points for sidewalks

### 4g. PCG Village Creation
- **URL:** https://dev.epicgames.com/community/learning/tutorials/33WR/unreal-engine-pcg-create-a-village-using-the-procedural-content-generation-framework
- **What:** Village-scale PCG generation

---

## 5. Community Tutorials -- Interiors/Rooms

### 5a. Procedural Room Generation with Splines and PCG (UE 5.7)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/eZVR/procedural-room-generation-with-splines-and-pcg-in-unreal-engine
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-procedural-room-generation-with-splines-and-pcg-in-unreal-engine/2680384
- **What:** Interior spaces using PCG + spline-based shape definition
- **Quality:** Intermediate-to-advanced

### 5b. PCG Graph -- Procedural Interior and Walls (Blueprint paste)
- **URL:** https://blueprintue.com/blueprint/fjpz0nys/
- **What:** PCG graph for interior wall and room generation, shared as pasteable blueprint

### 5c. Dynamic Floors and Paths using PCG (UE 5.7)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/EWlW/unreal-engine-5-7-tutorial-pcg-create-dynamic-floors-and-paths-using-pcg
- **What:** Floor and path generation for interiors

---

## 6. Community Tutorials -- Dungeons

### 6a. PCG Custom Graphs and Functions for Dungeon Layouts (UE 5.6/5.7)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/opYB/unreal-engine-pcg-custom-graphs-and-functions-for-dungeon-layouts
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-designing-procedural-dungeon-layouts-with-pcg-functions-in-unreal-engine-5-6/2680144
- **What:** Structured dungeon layout workflow using PCG Functions + light Blueprints
- **Covers:** Room setup, exits, PCG logic, modular dungeon structure

### 6b. Custom Dungeon using PCG & Basic Artwork (UE 5.5)
- **URL:** https://forums.unrealengine.com/t/community-tutorial-how-to-use-procedurally-generated-content-pcg-basic-artwork-to-create-a-custom-dungeon-using-unreal-engine-5-5/2686198
- **What:** PCG dungeon with basic art assets

### 6c. PCG Dungeon Level Builder WIP
- **URL:** https://forums.unrealengine.com/t/pcg-dungeon-level-builder-wip/1702124
- **What:** WIP dungeon builder using PCG, showcased in UE 5.4 era

---

## 7. PCG + GeometryScript Combined

### 7a. PCG + Geometry Script Wall Generator (UE 5.5)
- **URL:** https://dev.epicgames.com/community/learning/tutorials/q3E3/unreal-engine-pcg-geometry-script-in-ue-5-5-wall-generator
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-pcg-geometry-script-in-ue-5-5-wall-generator/2118674
- **What:** Combines PCG placement with GeometryScript mesh generation for walls
- **Approach:** PCG drives placement logic, GeometryScript generates the actual wall geometry
- **Quality Level:** Technical proof-of-concept

### 7b. Make Dynamic Mesh Inside PCG with Geometry Script
- **URL:** https://dev.epicgames.com/community/learning/tutorials/EPjL/unreal-engine-make-dynamic-mesh-inside-pcg-with-geometry-script
- **What:** Advanced tutorial on creating dynamic meshes inside PCG using GeometryScript via custom PCG nodes
- **Key Insight:** Custom PCG nodes can call GeometryScript functions to generate geometry procedurally within the PCG pipeline

### 7c. Renjie Zhang -- PCG City (UE 5.2, ArtStation)
- **URL:** https://www.artstation.com/artwork/WBrwnE
- **What:** City generation combining PCG Frame + Geometry Script
- **Key Detail:** Uses OSM (OpenStreetMap) data for road curves, City Sample assets for buildings
- **Author:** Technical Artist at Tencent
- **Approach:** Geometry Script generates building geometry, PCG handles placement and city layout

### 7d. bendemott/UE5-Procedural-Building (GitHub, GeometryScript only)
- **URL:** https://github.com/bendemott/UE5-Procedural-Building
- **What:** Procedural building generation using GeometryScript APIs in C++
- **Note:** Uses GeometryScript directly, NOT the PCG framework. But relevant as a comparison point for PCG+GS hybrid approaches.

---

## 8. Marketplace/Fab Products

### 8a. Procedural City Generator (Procedural World Lab)
- **URL:** https://www.fab.com/listings/924ddf22-cbd6-4bf9-9aed-f54e002078fe
- **Website:** https://proceduralworldlab.com/procedural-city-generator/
- **What:** Editor tools for generating true-to-scale cities
- **Features:** Procedural road network (highways, main, minor, side roads, cul-de-sacs), city zones controlling building/prop/foliage spawning
- **Interior:** No -- exterior city layout only
- **Modular:** Yes -- uses modular building pieces
- **Quality:** Production tool (commercial)

### 8b. Interactive Procedural City Creator (iPCC) (Procedural World Lab)
- **URL:** https://www.fab.com/listings/a41732dd-26f2-49e1-9f76-2167934651c2
- **What:** Advanced city builder with road networks, rivers, freeways, ramps, tunnels, bridges, real-world building shapes
- **Features:** Spline shape generator for building footprints, grammar-based procedural building generator
- **Interior:** No
- **Quality:** Production (commercial, premium)

### 8c. PCG City Buildings
- **URL:** https://www.fab.com/listings/09667242-f2f9-4d3a-b439-b762b142f9d2
- **What:** 40 unique spline-driven PCG buildings
- **Features:** Drop into world, adjust spline points, modify floor count and trims
- **Modular:** Fully modular, includes non-PCG versions
- **Interior:** No -- exterior facades only
- **Quality:** Production-ready assets

### 8d. BuildGen -- Procedural Buildings
- **URL:** https://www.unrealengine.com/marketplace/en-US/product/buildgen-procedural-buildings
- **What:** PCG building generator with drag-and-drop workflow
- **Features:** Procedural interior spawner, procedural sidewalk tool, custom wall meshes, any-shape buildings
- **Interior:** YES -- includes procedural interior spawner
- **Key Detail:** "Don't need to work with PCG graphs" -- blueprint wrapper hides PCG complexity
- **Quality:** Production tool

### 8e. Procedural Building Generator (PBG)
- **URL:** https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator
- **What:** Assemble custom/marketplace/Megascans modular meshes into buildings
- **Modular:** Yes -- works with any modular kit
- **Interior:** Limited

### 8f. Procedural Building Generator Pro (Procedural World Lab)
- **URL:** https://www.fab.com/listings/e57d496f-f8e5-447e-86dc-8efdb55e8add
- **What:** Grammar-based modular building generator
- **Key:** Uses grammar rules for building subdivision

### 8g. OmniScape -- Procedural City Generator
- **URL:** https://www.fab.com/listings/89685028-7a1e-4859-8fdf-47f9dca8e0be
- **Tech Doc:** https://gist.github.com/jazzmoradia/6c4383fb51914f9bf083ccd472ee6b12
- **What:** Fully parametric city generator for UE 5.5-5.7
- **KEY DETAIL:** Zero PCG dependency. Self-contained C++ algorithms, no PCG graphs.
- **Features:** Node-graph layout (landmark nodes anchor city), 3-tier weighted building system, deterministic seeds, terrain snapping, slope rejection
- **Interior:** No
- **Quality:** Premium commercial tool

### 8h. Ultimate Procedural Apartment (Trashcraft)
- **URL:** https://forums.unrealengine.com/t/trashcraft-ultimate-procedural-apartment/2671643
- **What:** Art-directed spline-based modular apartment generation (UE 5.6)
- **Interior:** YES -- detailed modular apartments, facades, architectural layouts
- **Key:** Non-destructive framework, art-direction precision with infinite variation
- **Quality:** Production-quality interiors

### 8i. Dungeon Architect
- **URL:** https://dungeonarchitect.dev/
- **Fab:** https://www.fab.com/listings/0ad73dc2-3daa-4c29-a70a-61fb9cea0c7c
- **What:** Dungeon generation combining PCG framework + Grid Flow Builder
- **Features:** Cyclic paths, key-locks, teleporters, foliage, elevation maps, PCG integration (UE 5.4+)
- **Interior:** YES -- full dungeon interiors
- **Quality:** Mature commercial product

### 8j. Calysto World 2.0 (formerly Massive World)
- **URL:** https://www.fab.com/listings/8631308a-67a3-4e20-b3e4-74be19813f77
- **What:** PCG-based world generation (landscapes, forests, biomes, roads, lakes, rivers)
- **Buildings:** No -- landscape/biome focused
- **Works with:** World Partition, any art style

### 8k. Ghost Town VOL.6 -- Road Generator
- **URL:** https://www.unrealengine.com/marketplace/en-US/product/ghost-town-vol-6-road-generator
- **What:** Roads, curbs, sidewalks, guardrails, road signs, road lines via construction scripts + spline mesh
- **Not PCG:** Uses construction scripts, not PCG framework

### 8l. Modular Eastern City -- Istanbul (Procedural World Lab)
- **URL:** https://forums.unrealengine.com/t/www-proceduralworldlab-com-modular-eastern-city-lowpoly-istanbul-enterable-interior/2708023
- **What:** Modular city kit with enterable interiors
- **Interior:** YES -- enterable building interiors
- **Quality:** Lowpoly/stylized

### 8m. Procedural Building (Fab)
- **URL:** https://www.fab.com/listings/8af3d376-e0e0-4566-a8ae-85d9e8af68aa
- **What:** PCG-based procedural building system

### 8n. RoadBuilder (GitHub, free)
- **URL:** https://github.com/fullike/RoadBuilder
- **What:** Road design tool with lane editing, markings, signs. Creates boundary splines for PCG detail generation.

---

## 9. Blog Posts and Written Guides

### 9a. "First We Make Manhattan, Then We Make Berlin" -- Jean-Paul Software (Feb 2025)
- **URL:** https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/
- **What:** Multi-part blog series on building a London-based game city with PCG
- **PCG Nodes Used:** Spline Sampler, Attribute Noise, Attribute Reduce, Match and Set Attributes, Copy Attributes, Compare, Loop, Load Data Table
- **Key Insights:**
  - Points are the fundamental unit (transform + bounds + density + steepness + seed)
  - Collections require careful handling -- some operations expect single collections
  - PCGEx plugin called "the amazing plugin" -- enables non-forest applications
  - Loop node needed for multi-collection processing
- **Source:** Blog text, subsequent parts detail city building specifics

### 9b. Thomas Piessat -- Procedural City Generation Notebook
- **URL:** https://thomaspiessat.github.io/UnrealEngine/UE5/proceduralCityGeneration.html
- **What:** Technical notebook on runtime city generation
- **Approach:** Spline-based city boundaries + road-alongside building placement
- **PCG Nodes:** DensityFilter, SelfPruning, TransformPoints, ExtendModifier, Merge
- **Key Techniques:**
  - DensityFilter -- controls building type density
  - SelfPruning -- prevents building overlap
  - TransformPoints -- places buildings at offset distances from roads (both sides)
  - ExtendModifier -- adjusts spacing
  - Merge -- combines symmetric placements
- **Limitations:** Mesh size must align with transform settings; no explicit interior support

### 9c. Kolosdev -- Shooter Tutorial PCG Level
- **URL:** https://kolosdev.com/shooter-tutorial-procedural-level-using-only-pcg/
- **What:** Indoor FPS level (tunnels, corridors) built entirely with PCG
- **Approach:** Dynamic tunnel length/width via PCG parameters, non-destructive workflow
- **Interior:** YES -- fully indoor environment
- **Features:** Floor generation, wall systems (left/right walls), ceiling wires, Infiltrator assets, layered materials
- **Key Insight:** PCG works well for constrained indoor spaces, not just open-world scatter. "Level art adapts procedurally to level design."
- **Source:** Downloadable project available
- **Limitations:** Simple tunnel/corridor geometry, not room-based layouts

### 9d. deaconline -- PCG in a Nutshell (Medium)
- **URL:** https://medium.com/@deaconline/procedural-content-generation-pcg-b54f4c1959cd
- **What:** Overview article covering PCG basics and city generation concepts

### 9e. Zack Sinisi -- Generating Massive Worlds with PCG
- **URL:** https://zacksinisi.com/generating-massive-worlds-with-pcg-framework-for-unreal-engine-5/
- **What:** Advanced PCG techniques for large-scale environments

### 9f. Subobject.co -- "Sculpting with Nodes"
- **URL:** https://subobject.co/unreal-engine-pcg-procedural-generation/
- **What:** PCG guide covering advanced node usage

### 9g. SlashSkill -- UE5 PCG Framework Beginner's Guide
- **URL:** https://www.slashskill.com/unreal-engine-5-pcg-framework-a-beginners-guide-to-procedural-level-design/
- **What:** Beginner-oriented PCG for level design

### 9h. Blueshift Interactive -- Custom PCG Nodes (Sept 2025)
- **URL:** https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/
- **What:** How to create custom PCG nodes in C++ and Blueprint
- **Blueprint:** Subclass `PCGBlueprintElement`, override `ExecuteWithContext`
- **C++:** Two classes: `UPCGSettings` (node definition) + `IPCGElement` (behavior). Override `ExecuteInternal`.
- **Key Detail:** Custom nodes can integrate GeometryScript for building generation within PCG pipeline

### 9i. "How to Create a Procedural Building in UE5" -- HighAvenue
- **URL:** https://highavenue.co/how-to-create-a-procedural-building-in-unreal-engine-5/
- **What:** Step-by-step building creation guide

### 9j. Horror Environment with PCG and Landmass
- **URL:** https://dev.epicgames.com/community/learning/tutorials/DlzR/how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass
- **Forum:** https://forums.unrealengine.com/t/community-tutorial-how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass/1216332
- **What:** Horror environment creation using PCG + Landmass plugin
- **Interior:** No -- exterior horror environment
- **Relevance:** Directly relevant to Leviathan's genre (horror)

---

## 10. ArtStation Showcases

### 10a. Regimantas Ramanauskas -- PCG Modular Building Template WIP
- **URL:** https://www.artstation.com/artwork/Nqrqvd
- **What:** PCG modular building with parametrized dimensions
- **Approach:** Main graph prepares spline data, samples points, conditional checks, passes to subgraphs for ground floor walls, typical walls, parapets, outer/inner corners
- **Modular:** Yes -- swappable pieces, bool parameter for alternative layouts
- **Limitation:** Currently 90-degree corners only, 45-degree and inclined walls planned
- **Key Detail:** All main dimensions parametrized. Multiple subgraphs handle different building elements.

### 10b. PCG Building & Speed Environment
- **URL:** https://www.artstation.com/artwork/aoeO3q
- **What:** PCG building with speed environment creation showcase

### 10c. PCG Level Builder for UE5
- **URL:** https://www.artstation.com/artwork/kNn8Wx
- **What:** PCG level builder for organic levels, dungeons, caves (UE 5.4+)

### 10d. DC -- Level Building Options with PCG
- **URL:** https://undertow.artstation.com/projects/WBZ5W2
- **What:** PCG level building exploration

---

## 11. Forum Discussions

### 11a. PCG for Interior (Challenges)
- **URL:** https://forums.unrealengine.com/t/pcg-for-interior/1190003
- **Key Challenges Identified:**
  - Positioning meshes at bounding box edges (walls) is difficult -- DistanceToDensity not tied to box size
  - Raycast hit queries only from world center, not dynamic origins
  - Surface Sampler cannot distribute points vertically (on walls)
  - No intuitive tools for boundary-constrained point distribution
- **Proposed Solutions:** Auto spline generation around edges, rule-based systems, raycast + projection

### 11b. Walkable Buildings and Rooms (On-the-Fly)
- **URL:** https://forums.unrealengine.com/t/tool-for-procedural-generated-walkable-buildings-and-rooms-in-ue5-generating-full-buildings-on-the-fly/657506
- **What:** Early-stage inquiry about market demand for runtime walkable building generation
- **Features Described:** Customizable X/Y sizes, multiple floors, rooms reachable through doors, floors reachable by stairs
- **Status:** Concept stage, minimal technical details

### 11c. PCG Building from Random Splines
- **URL:** https://forums.unrealengine.com/t/pcg-building-creation-from-random-splines/1945380
- **What:** Discussion on generating buildings from random spline shapes

### 11d. Runtime Drawn Walls with PCG
- **URL:** https://forums.unrealengine.com/t/runtime-drawn-walls-unreal-engine-5-pcg/2622663
- **What:** Player draws on map, strokes generate real-time procedural walls (dynamic mesh + instanced bricks)

### 11e. How to Make Procedural Interiors for Buildings
- **URL:** https://forums.unrealengine.com/t/how-do-i-make-procedural-interiors-for-buildings/1775610
- **What:** Community discussion on interior generation approaches

### 11f. PCG Roads/Junctions -- "Secret Knowledge?"
- **URL:** https://forums.unrealengine.com/t/pcg-2024-roads-junctions-is-it-secret-knowledge/1730472
- **What:** Discussion on road/junction generation difficulties with PCG in 2024

---

## 12. Books

### 12a. "Procedural Content Generation with Unreal Engine 5" (Packt, 2024)
- **URL (Packt):** https://www.packtpub.com/en-us/product/procedural-content-generation-with-unreal-engine-5-9781837637058
- **Amazon:** https://www.amazon.com/Procedural-Content-Generation-Unreal-Engine/dp/1801074461
- **GitHub:** https://github.com/PacktPublishing/Procedural-Content-Generation-with-Unreal-Engine-5
- **Author:** Paul Martin Eliasz
- **Building Chapter:** Chapter 7 -- "Let's Build a Building Using the PCG Spline Controller"
- **Covers:** Spline controllers, building templates, subgraphs, loops, Modulo Node
- **Source Code:** Available on GitHub (PacktPublishing)

---

## 13. Open Source / GitHub

### 13a. PCGEx (PCG Extended Toolkit)
- **URL:** https://github.com/PCGEx/PCGExtendedToolkit
- **Fab:** https://www.fab.com/listings/3f0bea1c-7406-4441-951b-8b2ca155f624
- **Docs:** https://pcgex.gitbook.io/pcgex
- **License:** MIT (free)
- **What:** Extends PCG with graph theory, pathfinding, spatial ops, filtering, data manipulation
- **Key Features for Buildings/Cities:**
  - Delaunay, Voronoi, MST, convex hulls for connected networks
  - A*/Dijkstra routing with pluggable heuristics
  - Polyline manipulation (smooth, subdivide, offset, bevel, fuse)
  - Octree lookups, point fusion, Lloyd relaxation, bin packing
  - Data transfer between points, paths, splines, and textures
- **City Usage:** Multiple tutorials use PCGEx for city street networks (see Section 4a)
- **Featured in:** Epic "Inside Unreal" stream -- https://forums.unrealengine.com/t/inside-unreal-taking-pcg-to-the-extreme-with-the-pcgex-plugin/2479952

### 13b. ProceduralDungeon (BenPyton)
- **URL:** https://github.com/BenPyton/ProceduralDungeon
- **What:** UE4/5 plugin for procedural dungeon generation
- **Approach:** Handmade room levels + procedural assembly, custom generation rules in BP/C++
- **Not PCG-based:** Uses its own system, not UE5 PCG framework

### 13c. DungeonGenerator (shun126)
- **URL:** https://github.com/shun126/DungeonGenerator
- **What:** UE5 plugin for procedural 3D dungeon generation
- **Features:** Tiled generation (editor + runtime), mini-maps, missions, grid scale parameters
- **Not PCG-based:** Custom tile-based system

### 13d. Packt Book Code
- **URL:** https://github.com/PacktPublishing/Procedural-Content-Generation-with-Unreal-Engine-5
- **What:** Code examples from the Packt PCG book, including building chapter

---

## 14. Key Findings -- What PCG Can and Cannot Do for Buildings

### What PCG DOES Well for Buildings

1. **Exterior facade generation via Shape Grammar** -- The Grammar node (UE 5.5+) enables CGA-style subdivision of volumes into walls, floors, corners, roofs. This is the primary building generation approach. (Cassini Sample, Grammar tutorials)

2. **Modular piece placement on grids/splines** -- PCG excels at placing modular wall/floor/ceiling pieces along splines or within volumes. Points are generated, filtered, and used to spawn static meshes via HISM. (Modular Building Tutorial, PCG City Buildings)

3. **City-scale building scatter** -- Placing building actors/blueprints at city scale with density control, overlap prevention (SelfPruning), road setbacks (TransformPoints). (Thomas Piessat, Jean-Paul Software, City tutorials)

4. **Road/street network generation** -- Especially with PCGEx for graph-based road networks, intersections, sidewalks. (City Streets tutorial, RoadBuilder)

5. **Runtime generation** -- PCG supports runtime hierarchical generation around the player with streaming. (Official docs, Runtime Hierarchical Generation)

6. **Non-destructive iteration** -- Edit parameters, regenerate instantly. Crucial for production workflows.

7. **HISM output** -- PCG natively outputs to Hierarchical Instanced Static Meshes for optimal performance.

### What PCG STRUGGLES With for Buildings

1. **Interior wall placement** -- PCG lacks intuitive tools for placing meshes at bounding box edges. Surface Sampler can't distribute vertically. DistanceToDensity isn't tied to volume size. (Forum: PCG for Interior)

2. **Room layout algorithms** -- PCG has no built-in BSP, graph-based, or constraint-based room layout. You must implement this externally (custom nodes, blueprints) or use Grammar rules to approximate it.

3. **Adjacency and connectivity** -- No native concept of "rooms must connect via doors" or "corridor must reach all rooms." Must be handled by external logic or PCGEx pathfinding.

4. **Geometry generation** -- PCG places existing meshes; it doesn't generate geometry. For custom geometry, must combine with GeometryScript via custom PCG nodes.

5. **Boolean operations** -- PCG cannot cut holes (doors/windows) in walls. Must pre-cut modular pieces or use GeometryScript.

6. **Structural coherence** -- PCG is fundamentally a point scatter system. Building generation requires higher-level structural concepts (floors, rooms, walls-as-boundaries) that must be imposed via grammar rules or external logic.

7. **Corner resolution** -- Modular wall placement at corners requires careful handling. The Ramanauskas WIP shows 90-degree-only limitation as a common issue.

### Fortnite's Approach (Separate from PCG Framework)
- **Shape Grammar + Wave Function Collapse** implemented in Verse (not UE5 PCG)
- Shape Grammar: rules take boxes, generate sub-boxes (floor slicing, corner/wall assignment)
- WFC: used for 2D flat areas, not 3D buildings
- Requires pre-made modular props that Verse spawns at runtime
- This confirms Epic uses grammar-based approaches for buildings internally, but implemented separately from the PCG framework

### City Sample (Matrix Demo) -- NOT PCG
- Uses Rule Processor (separate Epic tool), not PCG framework
- Buildings are pre-made assets, not procedurally generated architecture
- Important to dispel the misconception that City Sample uses PCG for buildings

---

## 15. Relevance to Monolith

### PCG Grammar vs Monolith GeometryScript Approach

| Aspect | PCG Grammar | Monolith GeometryScript |
|--------|-------------|------------------------|
| **Building exterior** | Strong (shape subdivision) | Strong (sweep walls, facade) |
| **Interior rooms** | Weak (no layout algorithms) | Strong (grid-based BSP) |
| **Door/window openings** | Pre-cut modular only | Boolean subtract |
| **Geometry generation** | None (places existing meshes) | Full (generates meshes) |
| **City-scale scatter** | Excellent | Good (MCP actions) |
| **Performance** | Excellent (HISM native) | Depends on mesh complexity |
| **Non-destructive** | Yes (regenerate graph) | Yes (MCP re-invoke) |
| **Runtime** | Yes (hierarchical streaming) | Editor-time only |
| **Art direction** | High (modular kit swapping) | Medium (parametric) |

### What Monolith Should Adopt from PCG World

1. **PCG Grammar for city-scale building placement** -- Use PCG graphs for scattering building blueprints across a city, with PCGEx for road networks. Feed Monolith-generated buildings as the assets PCG places.

2. **HISM integration** -- Monolith's modular building research already identified HISM as critical. PCG's native HISM output is a reference implementation.

3. **Modular kit compatibility** -- BuildGen and PCG City Buildings show the value of working with marketplace modular kits. Monolith's modular building system should accept standard modular piece conventions.

4. **Shape Grammar for facade variation** -- PCG Grammar's subdivision approach could complement Monolith's facade system for exterior variety within a structural framework.

5. **PCGEx as road network generator** -- For Leviathan's procedural city blocks, PCGEx's Delaunay/Voronoi/MST graph ops could drive street layout before Monolith generates buildings on lots.

### What Monolith Already Does Better Than PCG

1. **Interior layout** -- Grid-based room generation with adjacency, doors, corridors
2. **Geometry creation** -- GeometryScript generates actual meshes, not just placement
3. **Structural openings** -- Boolean subtraction for doors/windows
4. **Multi-story coherence** -- Stairwell propagation, floor stacking
5. **Architectural features** -- Balconies, porches, fire escapes, roofs
6. **Horror-specific** -- Decay parameters, horror metrics, pacing integration

### Recommended Hybrid Architecture

```
PCG (city scale)    -->  Monolith (building scale)
  Road networks           Room layout
  Lot subdivision          Floor plans
  Building scatter         Wall geometry
  Street furniture         Doors/windows
  Vegetation               Stairs
  Runtime streaming        Furniture scatter (collision-aware)
                           Horror features
```

PCG handles the macro (where buildings go) while Monolith handles the micro (what the buildings contain). This maps to the separation of concerns that most successful projects demonstrate.

---

## Summary Statistics

| Category | Count |
|----------|-------|
| Epic Official Resources | 6 |
| Conference Talks | 4 |
| Building Tutorials | 10 |
| City/Street Tutorials | 7 |
| Interior/Room Tutorials | 3 |
| Dungeon Tutorials | 3 |
| PCG+GeometryScript | 4 |
| Marketplace Products | 14 |
| Blog Posts/Guides | 10 |
| ArtStation Showcases | 4 |
| Forum Discussions | 6 |
| Books | 1 |
| Open Source | 4 |
| **Total Examples** | **76** |

---

## Source URLs (Complete List)

### Epic Official
- https://dev.epicgames.com/documentation/en-us/unreal-engine/using-shape-grammar-with-pcg-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview
- https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides
- https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-hierarchical-generation
- https://dev.epicgames.com/documentation/en-us/unreal-engine/electric-dreams-environment-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/unreal-engine/city-sample-project-unreal-engine-demonstration
- https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-in-unreal-engine
- https://dev.epicgames.com/documentation/en-us/fortnite/procedural-building-template-in-unreal-editor-for-fortnite
- https://www.unrealengine.com/en-US/news/the-cassini-sample-project-is-now-available
- https://www.unrealengine.com/en-US/electric-dreams-environment

### Tutorials
- https://dev.epicgames.com/community/learning/tutorials/OD52/unreal-engine-ue5-pcg-modular-building-tutorial-procedural-content-generation
- https://dev.epicgames.com/community/learning/tutorials/rpDx/unreal-engine-5-6-intermediate-pcg-building-system-tutorial
- https://dev.epicgames.com/community/learning/tutorials/4JWW/unreal-engine-realistic-pcg-buildings-generation
- https://dev.epicgames.com/community/learning/tutorials/9d3a/unreal-engine-pcg-grammer-tutorial-procedural-building-generator
- https://dev.epicgames.com/community/learning/tutorials/nzVe/unreal-engine-5-5-advanced-pcg-grammar-tutorial-tile-by-tile-building-roof-generation
- https://dev.epicgames.com/community/learning/tutorials/PYEX/introduction-to-pcg-grammar-in-unreal-engine-5-5
- https://dev.epicgames.com/community/learning/tutorials/pvEw/pcg-building-in-unreal-engine
- https://dev.epicgames.com/community/learning/tutorials/VxP9/unreal-engine-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg
- https://dev.epicgames.com/community/learning/tutorials/dXR7/unreal-engine-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo
- https://dev.epicgames.com/community/learning/tutorials/Obk3/create-entire-cities-automatically-with-pcg-splines-procedural-content-generation-in-unreal-engine
- https://dev.epicgames.com/community/learning/tutorials/9dpd/procedural-road-generation-in-unreal-engine-5-pcg
- https://dev.epicgames.com/community/learning/tutorials/Zeov/unreal-engine-create-dynamic-splines-from-pcg-to-generate-a-sidewalk
- https://dev.epicgames.com/community/learning/tutorials/33WR/unreal-engine-pcg-create-a-village-using-the-procedural-content-generation-framework
- https://dev.epicgames.com/community/learning/tutorials/eZVR/procedural-room-generation-with-splines-and-pcg-in-unreal-engine
- https://dev.epicgames.com/community/learning/tutorials/EWlW/unreal-engine-5-7-tutorial-pcg-create-dynamic-floors-and-paths-using-pcg
- https://dev.epicgames.com/community/learning/tutorials/opYB/unreal-engine-pcg-custom-graphs-and-functions-for-dungeon-layouts
- https://dev.epicgames.com/community/learning/tutorials/q3E3/unreal-engine-pcg-geometry-script-in-ue-5-5-wall-generator
- https://dev.epicgames.com/community/learning/tutorials/EPjL/unreal-engine-make-dynamic-mesh-inside-pcg-with-geometry-script
- https://dev.epicgames.com/community/learning/tutorials/DlzR/how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass

### Talks
- https://dev.epicgames.com/community/learning/talks-and-demos/Z1wa/unreal-engine-leveraging-pcg-for-building-and-city-creation
- https://dev.epicgames.com/community/learning/talks-and-demos/pBl1/unreal-engine-unreal-fest-2025-buildings-and-biomes-pcg
- https://forums.unrealengine.com/t/talks-and-demos-procedural-roads-and-buildings-in-pcg/2705285
- https://schedule.gdconf.com/session/developing-large-procedural-systems-with-low-friction-and-fast-generation-presented-by-epic-games/917366

### Marketplace
- https://www.fab.com/listings/924ddf22-cbd6-4bf9-9aed-f54e002078fe
- https://www.fab.com/listings/a41732dd-26f2-49e1-9f76-2167934651c2
- https://www.fab.com/listings/09667242-f2f9-4d3a-b439-b762b142f9d2
- https://www.unrealengine.com/marketplace/en-US/product/buildgen-procedural-buildings
- https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator
- https://www.fab.com/listings/e57d496f-f8e5-447e-86dc-8efdb55e8add
- https://www.fab.com/listings/89685028-7a1e-4859-8fdf-47f9dca8e0be
- https://www.fab.com/listings/3f7cd12c-30b3-47d6-90c2-8604ed068ab7
- https://dungeonarchitect.dev/
- https://www.fab.com/listings/8631308a-67a3-4e20-b3e4-74be19813f77

### Blog/Articles
- https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/
- https://thomaspiessat.github.io/UnrealEngine/UE5/proceduralCityGeneration.html
- https://kolosdev.com/shooter-tutorial-procedural-level-using-only-pcg/
- https://medium.com/@deaconline/procedural-content-generation-pcg-b54f4c1959cd
- https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/
- https://highavenue.co/how-to-create-a-procedural-building-in-unreal-engine-5/
- https://80.lv/articles/testing-out-ue5-s-procedural-content-generation-framework-at-runtime

### GitHub
- https://github.com/PCGEx/PCGExtendedToolkit
- https://github.com/BenPyton/ProceduralDungeon
- https://github.com/shun126/DungeonGenerator
- https://github.com/PacktPublishing/Procedural-Content-Generation-with-Unreal-Engine-5
- https://github.com/bendemott/UE5-Procedural-Building
- https://github.com/fullike/RoadBuilder

### ArtStation
- https://www.artstation.com/artwork/Nqrqvd
- https://www.artstation.com/artwork/WBrwnE
- https://www.artstation.com/artwork/aoeO3q
- https://www.artstation.com/artwork/kNn8Wx
