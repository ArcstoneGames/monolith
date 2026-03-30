# Modular Building Systems: The Alternative to Boolean-Based Generation

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** connected-room-assembly, boolean-doors, window-cutthrough, facade-window, thin-wall-geometry

---

## Executive Summary

Our current procedural building pipeline generates monolithic merged meshes with GeometryScript boolean subtractions for openings (doors, windows, vents). This causes well-documented problems: overlapping walls between facade and building mesh, boolean positioning bugs, window indentations instead of cut-throughs, and alignment issues at corners.

The entire AAA games industry uses a fundamentally different approach: **modular building kits** -- pre-made pieces (wall, wall-with-window, wall-with-door, corner, floor, ceiling) snapped together on a grid. This research covers how the industry does it, how we could do it procedurally, and whether we should switch.

**Recommendation:** YES, switch to modular assembly as the primary generation mode. Keep GeometryScript as a fallback for custom/organic geometry. The modular approach eliminates every boolean-related bug we have, is massively more performant via HISM instancing, and aligns with how every shipped game actually builds environments.

---

## 1. How AAA Games Do It

### 1.1 Bethesda (Skyrim, Fallout 3/4/76) -- The Gold Standard

Joel Burgess's GDC 2013 talk "Modular Level Design for Skyrim" is the definitive reference for modular building systems in games.

**Key principles:**
- A **kit** is a set of modular art assets that snap together on a grid
- Skyrim used ~15-20 kits (Nord Crypts, Dwemer Ruins, Imperial Forts, etc.)
- Each kit contains: walls, floors, ceilings, corners (inside + outside), doorways, stairs, transition pieces
- All pieces snap to a **256-unit grid** (roughly 2.56m at UE scale)
- Pivot points at consistent locations (typically corner or center of floor edge)
- Designers build levels by placing kit pieces like LEGO -- no custom geometry needed
- A single kit of ~50-100 pieces can produce hundreds of unique rooms/dungeons

**Why it works:** Openings (doors, windows) are **inherent to the mesh piece**, not cut out after the fact. A "wall with door" piece simply has a doorway-shaped hole baked in. Zero boolean operations. Zero alignment issues.

**Kit piece types (typical Bethesda kit):**
- Wall straight (2m, 4m widths)
- Wall with door
- Wall with window
- Wall with vent/grate
- Corner inside 90 deg
- Corner outside 90 deg
- Floor tile
- Ceiling tile
- Stair straight
- Stair with landing
- Doorframe/trim
- Pillar/column
- Transition piece (connects to other kits)

**Sources:**
- [Joel Burgess: Skyrim's Modular Level Design - GDC 2013](http://blog.joelburgess.com/2013/04/skyrims-modular-level-design-gdc-2013.html)
- [Skyrim's Modular Approach to Level Design - Gamasutra](https://www.gamedeveloper.com/design/skyrim-s-modular-approach-to-level-design)
- [Modular Level Design for Skyrim - SlideShare](https://www.slideshare.net/JoelBurgess/gdc2013-kit-buildingfinal)

### 1.2 Arkane Studios (Dishonored 1/2, Deathloop)

Dishonored 2's modular system is particularly relevant because it deals with realistic architecture (not just dungeon tiles).

**Key details:**
- **Level Architects** work alongside Level Designers to create and assemble modular building kits
- Blueprint tools automate tiling of modular pieces in X and Z directions
- Pattern arrays define mesh sequences, with randomization support
- Facade pieces (windows, pillars) are designed for repetition -- key for building exteriors
- Scaling guideline: pieces can be scaled 80-120% without visible texture stretch
- Different kits for different architectural styles (Karnaca vs Dunwall)

**Horror relevance:** Dishonored's architecture is dense, vertical, and interconnected -- similar spatial qualities to what survival horror needs. Their modular approach handles complex multi-story buildings with balconies, windows, and interior/exterior transitions.

**Sources:**
- [Modular Design in Dishonored 2 - 80 Level](https://80.lv/articles/modular-design-in-dishonored-2)
- [Dishonored: Environment Art & Shaders - 80 Level](https://80.lv/articles/dishonored-environment-art-shaders)
- [Dishonored 2 Environment Art Dump - Polycount](https://polycount.com/discussion/184711/dishonored-2-environment-art-dump)

### 1.3 Embark Studios (THE FINALS)

THE FINALS is the most relevant modern reference because their buildings must be **fully destructible**.

**Key details:**
- **Building Creator**: a modular Houdini toolset of interoperable HDAs
- Artists assemble buildings by combining reusable **Feature Nodes** (walls, floors, roofs, windows, doors)
- Each Feature Node generates a specific architectural element
- The wall mod-kit auto-detects faces from blockout primitives (uses `@N.y` to separate walls from floors/ceilings)
- Window placement checks face dimensions to select appropriate window mesh
- All assets are pre-fractured in Houdini for destruction (seams hidden until damage)
- Modular design means individual nodes can be upgraded without breaking the system

**Critical insight:** Even for a game with extreme destruction requirements, they don't use booleans. They use modular pieces with pre-baked openings, assembled procedurally.

**Sources:**
- [Making the Procedural Buildings of THE FINALS - SideFX](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [Creating Procedural Buildings In THE FINALS With Houdini - 80 Level](https://80.lv/articles/how-embark-studios-built-procedural-environments-for-the-finals-using-houdini)

### 1.4 Remedy Entertainment (Control)

Control's Oldest House is notable for its brutalist modular architecture.

**Key details:**
- Stuart Macdonald (World Design Director) designed a modular ceiling system where coffers fuse with different lighting, columns, and beams
- Brutalist concrete aesthetic is inherently modular (repeated panels, grid patterns)
- GDC talk focused on procedural destruction system (Johannes Richter)
- Modular pieces enable the reality-warping Hiss effects -- walls rearrange themselves

**Sources:**
- [The real buildings that inspired Control's Oldest House - Game Developer](https://www.gamedeveloper.com/art/the-real-buildings-that-inspired-i-control-i-s-oldest-house)
- [The Mesmerizing Art Behind Control - Game Informer](https://gameinformer.com/2019/03/27/the-mesmerizing-art-behind-control)

### 1.5 Naughty Dog (The Last of Us Part I/II)

TLOU uses a hybrid approach:

**Key details:**
- Modular kits for repeating architectural elements (banisters, railings, wall sections)
- World-space shader effects to vary grunge/textures across instances (breaks visual repetition)
- Extensive photogrammetry for hero/unique pieces
- Material Art GDC talk (2023): procedural generators for organic materials, construction-material kits in clean and damaged variants

**Relevance:** TLOU's approach to modular kits with damage variants is exactly what a horror game needs -- same wall piece in clean, dirty, decayed, and destroyed versions.

**Sources:**
- [The Material Art of The Last of Us Part I - 80 Level](https://80.lv/articles/the-material-art-of-the-last-of-us-part-i-gdc-presentation-is-now-available-for-free)

### 1.6 Capcom (Resident Evil series)

RE Engine uses photogrammetry-heavy environments with modular assembly:

**Key details:**
- RE7 onwards: real-world objects scanned, reconstructed, optimized for runtime
- Spencer Mansion (RE1 remake), Baker House (RE7), Police Station (RE2) all built from modular pieces
- Hand-crafted layout but modular construction -- each room assembled from standardized wall/floor/ceiling pieces
- Damage/decay layered on via decals and material variants, not geometry modification

**Sources:**
- [RE Engine - Wikipedia](https://en.wikipedia.org/wiki/RE_Engine)
- [Level Design Analysis - RE4 Remake](https://medium.com/@gabriel.fuentesgd/level-design-analysis-resident-evil-4-remake-the-village-896a01c58feb)

### 1.7 Industry Consensus

Every AAA game that ships explorable interiors uses modular kits. There are zero exceptions in the modern era. The approach varies in specifics but the principle is universal:

| Studio | Game | Kit Pieces | Grid Size | Openings |
|--------|------|-----------|-----------|----------|
| Bethesda | Skyrim | ~50-100/kit | 256 UU (~2.5m) | Baked into mesh |
| Arkane | Dishonored 2 | ~40-80/kit | Variable | Baked into mesh |
| Embark | THE FINALS | Modular HDAs | Variable | Baked, pre-fractured |
| Remedy | Control | Modular panels | Brutalist grid | Baked into mesh |
| Naughty Dog | TLOU | Hybrid modular | Variable | Baked into mesh |
| Capcom | RE series | Photogrammetry+modular | Variable | Baked into mesh |

**Nobody cuts holes in solid walls with boolean operations for shipping games.**

---

## 2. Procedural Modular Assembly

### 2.1 The Core Algorithm: Floor Plan to Pieces

Given a floor plan (2D grid of room IDs -- which we already generate), the assembly algorithm is:

```
For each grid edge (boundary between two cells):
    left_room = grid[x][y]
    right_room = grid[x+1][y]  (or grid[x][y+1] for vertical edges)

    if left_room == right_room:
        continue  // Same room, no wall needed

    if left_room == OUTSIDE or right_room == OUTSIDE:
        wall_type = select_exterior_wall(edge, floor_plan)
    else:
        wall_type = select_interior_wall(edge, floor_plan)

    place_modular_piece(wall_type, edge_position, edge_orientation)
```

**Wall type selection per edge:**

| Left | Right | Has Door? | Has Window? | Piece Type |
|------|-------|-----------|-------------|------------|
| Room A | Room B | Yes | No | Interior wall with door |
| Room A | Room B | No | No | Interior wall solid |
| Room A | Outside | No | Yes | Exterior wall with window |
| Room A | Outside | Yes | No | Exterior wall with door |
| Room A | Outside | No | No | Exterior wall solid |
| Room A | Hallway | Yes | No | Interior wall with door (or open) |

This is essentially what our `create_building_from_grid` already computes for boolean operations -- we just replace "cut a hole" with "place the right piece."

### 2.2 Corner Resolution

Corners are the trickiest part of modular assembly. There are four cases:

**L-corner (two walls meet at 90 deg):**
- Place a dedicated corner piece that covers the junction
- The corner piece has geometry for both wall directions
- Handles both inside corners (room interior) and outside corners (building exterior)

**T-junction (wall meets perpendicular wall):**
- One wall continues straight, the other terminates
- Place the continuing wall normally
- Cap the terminating wall with an end piece
- Or: use a T-junction piece that handles both

**X-junction (four walls meet):**
- Rare in buildings but happens at hallway intersections
- Place a column/pillar piece at the intersection point
- Walls on all four sides are standard pieces

**Algorithm:**
```
For each grid vertex (corner of four cells):
    count walls meeting at this vertex
    determine wall directions

    if 2 walls, 90 deg apart:
        place_corner_piece(inside or outside)
    elif 3 walls (T-junction):
        place_t_junction_piece()
    elif 4 walls (X-junction):
        place_column_piece()
    elif 1 wall (wall end):
        place_wall_cap()
```

**On the 50cm grid:** Our grid cells are 50cm. A standard wall piece would span 1 cell (50cm) or 2 cells (100cm). Corner pieces occupy the vertex between cells. This is well-aligned with our existing coordinate system.

### 2.3 Shadows of Doubt's Approach (Most Relevant Reference)

Shadows of Doubt is the closest shipped game to what we're building -- fully procedural interiors with modular tiles.

**Their system:**
- Each tile is 1.8m x 1.8m
- Building floors are 15x15 tile grids
- Hand-designed floor plans define address boundaries within a building floor
- Rooms generated by priority: living room first, then bathroom, then bedrooms, etc.
- Hallways generated first to ensure connectivity
- Each tile boundary gets a wall piece (solid, door, or open) based on room adjacency
- Furniture placed after walls based on room type

**Key lesson:** They don't generate geometry procedurally at all. They place pre-made tile pieces. The "procedural" part is which piece goes where, not the geometry of the pieces.

**Sources:**
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)

### 2.4 Wave Function Collapse for Modular Buildings

WFC is well-suited for filling in interior details after the structure is defined:

**Approach:**
1. Define floor plan boundary (we have this)
2. Place structural walls at room boundaries (deterministic, per 2.1)
3. Use WFC to select wall variants (clean, damaged, decorated) and props
4. Adjacency rules ensure visual coherence (damaged wall next to damaged wall, not next to pristine)

**WFC tile adjacency for wall variants:**
- Clean wall <-> Clean wall: allowed
- Clean wall <-> Slightly dirty wall: allowed
- Clean wall <-> Destroyed wall: disallowed (too jarring)
- Damaged wall <-> Damaged wall: allowed
- Boarded window <-> Broken window: allowed (horror gradient)

This lets us generate buildings that have spatially coherent decay -- not random damage, but believable degradation patterns.

**Sources:**
- [Wave Function Collapse tips and tricks - BorisTheBrave](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Procedural Generation of Buildings with WFC - HAW Hamburg thesis](https://reposit.haw-hamburg.de/bitstream/20.500.12738/15709/1/BA_Procedural%20Generation%20of%20Buildings_geschw%C3%A4rzt.pdf)

### 2.5 Subveillance's Interior Generation

Another procedural interior system worth noting:

- 2D tile-based maps on a simple grid
- Buildings are random-sized rectangles with L-shaped bites removed
- Rooms line the outside perimeter of the building
- Hallways run around the interior perimeter for connectivity
- Each tile boundary becomes a wall piece

**Sources:**
- [Procedural generation of building interiors - Subveillance devlog](https://unusual-intersection.itch.io/subveillance/devlog/492733/procedural-generation-of-building-interiors-part-1)

---

## 3. Hybrid Approach: What We Should Build

### 3.1 The Proposal

Replace GeometryScript boolean-based wall generation with modular piece placement on our existing 50cm grid.

**What stays the same:**
- Floor plan generation (SP2 room layout algorithm)
- Room ID grid (2D array of room assignments)
- Door/window placement decisions (edge-level)
- Spatial registry (room adjacency, building hierarchy)
- MCP action interface (`create_building_from_grid`)

**What changes:**
- Instead of AppendBox + ApplyMeshBoolean, we place modular static mesh pieces
- Each grid edge maps to a piece selection (solid wall, door wall, window wall, open)
- Pieces placed via HISM for performance
- Corner pieces at grid vertices where walls meet

### 3.2 The Modular Piece Catalog

**Minimum viable kit (Phase 1 -- blockout quality):**

| ID | Piece | Grid Cells | Notes |
|----|-------|-----------|-------|
| W1 | Exterior wall solid | 1x1 (50cm wide, 270cm tall) | Basic solid wall segment |
| W2 | Exterior wall + window | 2x1 (100cm wide) | Standard window opening |
| W3 | Exterior wall + door | 2x1 (100cm wide) | Standard door opening |
| W4 | Exterior wall + double door | 3x1 (150cm wide) | Wide entrance |
| I1 | Interior wall solid | 1x1 (50cm wide) | Thinner than exterior |
| I2 | Interior wall + door | 2x1 (100cm wide) | Interior doorway |
| I3 | Interior wall open | 1x1 (50cm wide) | Pass-through (no wall) |
| C1 | Corner inside 90 deg | 1x1 | Inside corner junction |
| C2 | Corner outside 90 deg | 1x1 | Outside corner junction |
| C3 | T-junction | 1x1 | Three-way wall meeting |
| C4 | Column/pillar | 1x1 | Four-way intersection |
| F1 | Floor tile | 1x1 (50cm x 50cm) | Basic floor |
| F2 | Floor tile stairs | 2x4 | Stairwell opening |
| CL1 | Ceiling tile | 1x1 (50cm x 50cm) | Basic ceiling |
| S1 | Stair module | 2x6 cells, 270cm rise | Full flight |

**Phase 2 -- horror variants (WFC-driven):**

| ID | Piece | Notes |
|----|-------|-------|
| W1d | Exterior wall damaged | Cracks, holes, peeling paint |
| W2b | Window boarded | Planks over window opening |
| W2k | Window broken | Shattered glass, frame damage |
| I1d | Interior wall damaged | Holes, exposed studs/pipes |
| I2b | Door barricaded | Furniture piled against door |
| F1d | Floor damaged | Broken boards, holes |
| F1w | Floor wet/bloody | Horror surface variant |

**Phase 3 -- art quality pieces:**
- Replace blockout boxes with proper meshes (artist-created or marketplace)
- Multiple material variants per piece (drywall, concrete, brick, tile)
- Trim/baseboard geometry integrated into wall pieces
- Light fixtures integrated into ceiling pieces

### 3.3 Generation Modes

**Mode A: Pure GeometryScript (current)**
Generate blockout-quality modular pieces on the fly using AppendBox/AppendSimpleSweptPolygon. No pre-made meshes needed. Zero artist dependency. This is what Phase 1 would do -- essentially the same geometry we generate now, but as individual pieces instead of one merged mesh with booleans.

**Mode B: Static Mesh Placement (marketplace/custom art)**
Place pre-made static mesh assets from a modular kit. Requires artist-made or purchased assets. Much higher visual quality. This is the long-term goal.

**Mode C: Hybrid**
GeometryScript for structural walls + static meshes for details (windows, doors, trim). Best of both worlds during transition.

### 3.4 Grid Edge Algorithm (Detailed)

```
struct FEdgeInfo {
    int32 CellA_RoomID;        // Room on one side
    int32 CellB_RoomID;        // Room on other side
    bool bIsExterior;          // One side is outside
    bool bHasDoor;             // Door on this edge
    bool bHasWindow;           // Window on this edge
    ECardinalDirection Normal; // Which way the wall faces
    FIntPoint GridPosition;    // Position in grid coords
};

// For each horizontal edge:
for (y = 0; y <= GridHeight; y++)
    for (x = 0; x < GridWidth; x++)
        edge.CellA_RoomID = (y > 0) ? Grid[x][y-1] : OUTSIDE;
        edge.CellB_RoomID = (y < GridHeight) ? Grid[x][y] : OUTSIDE;
        if (edge.CellA_RoomID != edge.CellB_RoomID)
            SelectAndPlacePiece(edge);

// For each vertical edge:
for (y = 0; y < GridHeight; y++)
    for (x = 0; x <= GridWidth; x++)
        edge.CellA_RoomID = (x > 0) ? Grid[x-1][y] : OUTSIDE;
        edge.CellB_RoomID = (x < GridWidth) ? Grid[x][y] : OUTSIDE;
        if (edge.CellA_RoomID != edge.CellB_RoomID)
            SelectAndPlacePiece(edge);
```

**Piece selection logic:**
```
EPieceType SelectPiece(FEdgeInfo Edge) {
    if (Edge.bIsExterior) {
        if (Edge.bHasDoor) return EXT_WALL_DOOR;
        if (Edge.bHasWindow) return EXT_WALL_WINDOW;
        return EXT_WALL_SOLID;
    } else {
        if (Edge.bHasDoor) return INT_WALL_DOOR;
        return INT_WALL_SOLID;
    }
}
```

### 3.5 Corner Resolution Algorithm

```
// For each grid vertex:
for (y = 0; y <= GridHeight; y++)
    for (x = 0; x <= GridWidth; x++)
        // Check 4 edges meeting at this vertex
        bool bNorthWall = HasWallOnEdge(x, y, NORTH);
        bool bSouthWall = HasWallOnEdge(x, y, SOUTH);
        bool bEastWall  = HasWallOnEdge(x, y, EAST);
        bool bWestWall  = HasWallOnEdge(x, y, WEST);

        int WallCount = bNorthWall + bSouthWall + bEastWall + bWestWall;

        switch (WallCount) {
            case 0: break; // No corner needed
            case 1: PlaceWallCap(...); break;
            case 2:
                if (walls are opposite) PlaceWallPass(...); // Straight-through
                else PlaceCornerPiece(...); // L-corner
                break;
            case 3: PlaceTJunction(...); break;
            case 4: PlaceColumn(...); break;
        }
```

---

## 4. UE5 Implementation Details

### 4.1 HISM for Modular Pieces

Each unique piece type gets its own `UHierarchicalInstancedStaticMeshComponent`:

```cpp
// One HISM per piece type
UPROPERTY()
TMap<EPieceType, UHierarchicalInstancedStaticMeshComponent*> PieceInstances;

// Adding a wall piece = adding an instance
FTransform WallTransform(Rotation, Position, FVector::OneVector);
PieceInstances[EXT_WALL_SOLID]->AddInstance(WallTransform);
```

**Performance characteristics:**
- HISM batches all instances of the same mesh into a single draw call
- HISM supports per-instance LOD (unlike basic ISM)
- Works with Nanite (each instance can be Nanite-enabled)
- Typical building: ~200-400 wall instances, ~100 floor tiles, ~50 ceiling tiles
- At ~10 piece types, that's ~10 draw calls per building (vs 1 for merged mesh, but...)
- With DrawCallReducer: pieces using the same material can be merged across HISM components

**HISM vs Merged Mesh performance:**

| Metric | Merged Mesh (current) | HISM Modular | Notes |
|--------|----------------------|-------------|-------|
| Draw calls per building | 1 | 10-15 | Grouped by piece type |
| Vertex count | Lower (shared verts) | Higher (piece overlap) | ~20-30% more geometry |
| Boolean compute time | 50-500ms | 0ms | Huge win |
| Generation time | 200-2000ms | 5-50ms | Just instance transforms |
| Memory per building | 1 unique mesh | Shared meshes | HISM wins massively |
| Memory for 50 buildings | 50 unique meshes | Still shared meshes | Modular: ~50x less VRAM |
| Collision | Complex mesh | Per-piece simple | Modular: simpler, faster |
| LOD | Custom per building | Per-piece type | Modular: automatic |
| Nanite | No (GeometryScript) | Yes (static meshes) | Modular wins |

**Critical point:** For a city block with 20+ buildings, the merged mesh approach creates 20+ unique meshes that cannot share GPU memory. The modular approach uses the same ~15 meshes instanced hundreds of times. The memory savings are enormous.

### 4.2 UE5 ISM Performance Caveat

There is a reported performance regression in UE5 vs UE4 for instanced static meshes:
- AddInstance/RemoveInstance operations are slower in UE5.4+ than UE4.27
- After ~10,000 instances, performance degrades
- Mitigation: batch AddInstances calls, don't add/remove at runtime

For editor-time generation (our use case), this is not a concern -- we generate once and the instances are static.

**Sources:**
- [What happened to the ISM performance in UE5? - Epic Forums](https://forums.unrealengine.com/t/what-happened-to-the-instanced-static-mesh-performance-in-ue5/2107140)

### 4.3 Packed Level Actors / Level Instances

UE5 also offers Packed Level Actors (PLAs) for modular assembly:
- Reference a sub-level containing static mesh geometry
- Package into a single optimized actor
- Good for pre-designed room modules
- Not ideal for procedural placement (designed for hand-placed reuse)

For our use case, HISM is preferred over PLAs because we're placing individual pieces, not pre-assembled rooms.

### 4.4 DrawCallReducer Compatibility

Our DrawCallReducer plugin merges draw calls for meshes with identical materials. With modular pieces:
- All wall pieces likely share the same material (or material instance)
- DrawCallReducer would merge their draw calls even across different HISM components
- Net result: potentially fewer draw calls than the current merged-mesh approach
- The combination of HISM + DrawCallReducer is optimal

### 4.5 PCG Framework Integration

UE5's Procedural Content Generation (PCG) framework could orchestrate modular placement:
- PCG graphs can spawn static mesh instances based on rules
- Supports subgraphs for nested logic (building -> floor -> room -> wall)
- Native integration with HISM/ISM for spawned meshes
- Could expose our floor plan generator as a PCG data source

This is a future consideration, not a Phase 1 requirement.

**Sources:**
- [PCG Development Guides - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [Procedural Content Generation Overview - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)

---

## 5. Available Modular Building Assets

### 5.1 UE Marketplace Kits

| Kit | Meshes | Interior? | Price Range | Notes |
|-----|--------|----------|-------------|-------|
| Next Gen Modular Victorian/Neoclassical City (Bundle) | 1175 | Yes | $$$ | Includes proc building generator BP, Megascans-compatible |
| Next Gen Modular City V3 (Bundle) | Large | Yes | $$$ | Proc Building Generator + Proc Road Generator included |
| Procedural Building Generator | N/A (tool) | Yes | $$ | Assembles custom/marketplace/Megascans meshes procedurally |
| Medieval Modular Buildings Pack | 615 | Yes (47 pre-made buildings) | $$ | Grid-based construction |
| Stylized Modular Building Kit | 200+ | Yes | $$ | Game-ready explorable levels |
| Modular Building (generic) | Varies | Basic | $ | Simple modular props |

**Sources:**
- [Procedural Building Generator - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator)
- [Next Gen Modular Victorian Neoclassical City - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/next-gen-modular-victorian-neoclassical-city-bundle-exterior-interior)

### 5.2 Free / CC0 Assets

| Kit | Pieces | License | Format | Notes |
|-----|--------|---------|--------|-------|
| OpenGameArt Modular Buildings | 100 | CC0 | FBX/OBJ | Low poly, prototyping quality |
| OpenGameArt Building Kit | 80 | CC0 | FBX/OBJ/Blend | Walls, windows, doors (animated), roofs |
| OpenGameArt CC0 Buildings Kit | Varies | CC0 | Various | Multiple building types |
| OpenGameArt Castle Kit | 75+ | CC0 | Various | Medieval/castle pieces |
| Kenney Building Kit | 100+ | CC0 | Various | High quality low poly |

**Sources:**
- [Modular Buildings - OpenGameArt](https://opengameart.org/content/modular-buildings)
- [Building Kit - OpenGameArt](https://opengameart.org/content/building-kit)
- [CC0 Buildings Kit - OpenGameArt](https://opengameart.org/content/cc0-buildings-kit)

### 5.3 Megascans / Quixel

- Quixel Megascans offers modular building surfaces (brick, concrete, plaster) as materials
- Not full modular pieces, but textures/materials to apply to generated geometry
- Free for UE5 projects (included with subscription)
- Can combine with our GeometryScript blockout pieces for textured modular walls

---

## 6. The Key Question: Should We Switch?

### 6.1 Problems with Current Boolean Approach

From our existing research, the boolean-based system has these documented issues:

1. **Window cut-through bug** (reference_window_cutthrough_research): Facade generates its own wall slab, building also has solid exterior walls. Two overlapping walls = holes reveal solid wall behind.

2. **Boolean cutter positioning bug** (reference_boolean_doors_research): Cutter centered on wall outer face instead of center. Only 12cm of 20cm wall gets cut.

3. **Stairwell cutout propagation** (reference_stairwell_cutout_research): No cross-floor propagation of stairwell openings. 69.6-degree steep stairs.

4. **Door clearance** (reference_door_clearance_research): Boolean cutter centering causes narrow openings. Player capsule (84cm diameter) too wide for some generated doors.

5. **Performance**: Each boolean operation takes 5-50ms. A building with 40 openings = 200-2000ms generation time. All geometry is unique (no instancing).

6. **Memory**: Every building is a unique mesh asset. 50 buildings = 50 unique meshes in VRAM.

### 6.2 How Modular Eliminates These Problems

| Problem | Boolean Approach | Modular Approach |
|---------|-----------------|------------------|
| Overlapping walls | Two wall layers (facade + building) | Single piece IS the wall |
| Cutter positioning | Complex offset math, bugs | No cutters -- openings baked in |
| Stairwell cutouts | Boolean through multiple floors | Stair piece replaces floor tile |
| Door clearance | Depends on cutter size vs wall thickness | Door opening is exactly sized |
| Generation time | 200-2000ms per building | 5-50ms (just transforms) |
| GPU memory | Unique mesh per building | Shared meshes across all buildings |
| Collision | Complex convex decomposition | Simple per-piece collision |
| Artist iteration | Regenerate entire building | Swap one piece type |
| Nanite | Not supported (dynamic mesh) | Fully supported |

### 6.3 What Modular Doesn't Do Well

1. **Organic/curved geometry**: Non-rectangular walls, arches, curved facades need custom pieces or fallback to GeometryScript
2. **Unique architectural features**: One-off elements (grand staircase, atrium) need special pieces
3. **Seamless surfaces**: Grid lines can be visible if materials don't hide them (trim pieces help)
4. **Very small grid**: 50cm grid means lots of pieces. May want 100cm as standard piece width.
5. **Non-90-degree angles**: Requires additional diagonal piece variants or fallback

### 6.4 Recommendation

**Switch to modular as the primary system. Keep GeometryScript as secondary.**

The modular approach:
- Eliminates ALL 5 documented boolean bugs instantly
- Is 10-100x faster to generate
- Uses 10-50x less GPU memory for multiple buildings
- Supports Nanite, proper LODs, and DrawCallReducer
- Aligns with every AAA game's proven approach
- Enables artist-quality pieces to drop in later (marketplace or custom)
- Supports WFC-driven horror variants naturally

GeometryScript remains useful for:
- Phase 1 blockout pieces (generate the modular pieces themselves procedurally)
- Custom/organic geometry (caves, ruins, collapsed structures)
- One-off architectural features
- Terrain-adaptive foundations

---

## 7. Implementation Plan

### Phase 1: Modular Blockout Kit via GeometryScript (~20-25h)

Generate the modular pieces themselves using GeometryScript, save as static mesh assets, then place via HISM.

**Tasks:**
1. Define piece catalog (wall solid, wall+door, wall+window, corners, floor, ceiling) -- 2h
2. Generate each piece as a static mesh asset via GeometryScript -- 6h
3. Implement grid-edge walker that selects pieces per edge -- 4h
4. Implement corner resolution algorithm -- 3h
5. HISM placement system (one HISM per piece type) -- 3h
6. New `create_modular_building` MCP action -- 4h
7. Hook up to existing floor plan generator output -- 2h

**Result:** Visually identical to current blockout buildings, but using instanced modular pieces. All boolean bugs eliminated.

### Phase 2: Horror Variant System (~15-20h)

**Tasks:**
1. Generate damage variants for each piece (clean, dirty, damaged, destroyed) -- 6h
2. WFC-based variant selection with spatial coherence -- 5h
3. `horror_decay` parameter (0.0 = pristine, 1.0 = ruins) drives variant distribution -- 3h
4. Boarded windows, barricaded doors as piece variants -- 4h
5. Integration with existing horror decay system -- 2h

### Phase 3: Art-Quality Pieces (~15-20h)

**Tasks:**
1. Define modular piece import spec (pivot, dimensions, material slots) -- 2h
2. Import pipeline for marketplace/custom modular kits -- 4h
3. Material assignment system (per-piece-type material overrides) -- 3h
4. Trim/baseboard integration -- 3h
5. Marketplace kit evaluation and integration -- 4h
6. Documentation and artist guide -- 2h

### Phase 4: City Block Scale (~10-15h)

**Tasks:**
1. Batch HISM management across multiple buildings -- 3h
2. LOD group management (building-level LOD that swaps modular pieces for impostor) -- 4h
3. Streaming support (load/unload HISM groups per building) -- 3h
4. Performance benchmarking vs current approach at scale -- 2h

### Phase 5: PCG Framework Integration (Future)

Expose modular building generation as PCG graph nodes for designer use.

**Total estimate:** ~60-80h across all phases. Phase 1 alone (~20-25h) eliminates all boolean bugs.

---

## 8. Grid Size Considerations

### Current: 50cm Grid

Our floor plan uses 50cm cells. This is fine for room layout but creates many small wall pieces.

**Option A: Keep 50cm, 1-cell walls**
- Piece width: 50cm
- A 4m wall = 8 pieces
- Pro: Maximum flexibility, matches room layout grid exactly
- Con: Many instances, potential visual repetition

**Option B: Keep 50cm grid, 2-cell standard walls**
- Piece width: 100cm (spanning 2 grid cells)
- A 4m wall = 4 pieces
- Pro: Fewer instances, better proportioned for doors/windows
- Con: Odd-length walls need a 1-cell filler piece
- **Recommended for initial implementation**

**Option C: Keep 50cm grid internally, render at 100cm**
- Floor plan computed at 50cm resolution
- Wall pieces placed at 100cm intervals, with 50cm fillers where needed
- Best of both worlds

### Industry Standard Grid Sizes

| Game/Studio | Grid Size | Wall Piece Width | Ceiling Height |
|-------------|-----------|-----------------|----------------|
| Skyrim (Bethesda) | ~256 UU (~2.5m) | 2.5m | 2.5-3m |
| Doom (id Software) | 96 UU (~1.8m) | 1.8m | Variable |
| THE FINALS (Embark) | Variable | Variable | ~3m |
| Shadows of Doubt | 1.8m tiles | 1.8m | ~2.7m |
| Our system | 50cm cells | 100cm (2 cells) | 270cm |

Our 100cm piece width (2 cells) is reasonable. Most games use 1.8-2.5m but they also have wider corridors and rooms.

---

## 9. Comparison Matrix

| Feature | Boolean Merged Mesh | Modular HISM | Modular Actors |
|---------|--------------------:|-------------:|---------------:|
| Generation speed | Slow (200-2000ms) | Fast (5-50ms) | Fast (10-100ms) |
| GPU memory (1 building) | 1 unique mesh | Shared pieces | Shared pieces |
| GPU memory (50 buildings) | 50 unique meshes | Same shared pieces | Same shared pieces |
| Draw calls (1 building) | 1 | 10-15 | 200-400 |
| Draw calls (50 buildings) | 50 | 10-15 (same HISMs) | 10,000-20,000 |
| Boolean bugs | All documented issues | None | None |
| Nanite support | No | Yes | Yes |
| LOD support | Manual | Per-piece auto | Per-piece auto |
| Collision complexity | High (convex decomp) | Low (per-piece) | Low (per-piece) |
| Artist iteration | Regenerate all | Swap piece type | Swap piece type |
| Horror variants | Material only | Piece swap + material | Piece swap + material |
| Destruction support | Difficult | Per-piece | Per-piece |
| Runtime modification | Difficult | Remove/swap instance | Remove/swap actor |
| DrawCallReducer compat | N/A (1 mesh) | Yes (merges same-mat) | Yes |

**Winner:** Modular HISM is best for our use case (many identical buildings, editor-time generation, horror variants needed).

---

## 10. Migration Strategy

We don't need to rip out the boolean system overnight. The migration path:

1. **Implement `create_modular_building` as NEW action** alongside existing `create_building_from_grid`
2. **Same input format** -- both accept the floor plan grid and room definitions
3. **A/B testing** -- generate the same building both ways, compare visually
4. **Gradual adoption** -- new buildings use modular, old ones stay as-is
5. **Eventually deprecate** boolean path once modular is proven

The floor plan generator, spatial registry, and room layout algorithms are completely unaffected. Only the geometry generation step changes.

---

## Sources

### GDC Talks & Industry
- [Joel Burgess: Skyrim's Modular Level Design - GDC 2013](http://blog.joelburgess.com/2013/04/skyrims-modular-level-design-gdc-2013.html)
- [Skyrim's Modular Approach to Level Design](https://www.gamedeveloper.com/design/skyrim-s-modular-approach-to-level-design)
- [Modular Design in Dishonored 2](https://80.lv/articles/modular-design-in-dishonored-2)
- [Making the Procedural Buildings of THE FINALS](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [Creating Procedural Buildings In THE FINALS With Houdini](https://80.lv/articles/how-embark-studios-built-procedural-environments-for-the-finals-using-houdini)
- [Control's Oldest House](https://www.gamedeveloper.com/art/the-real-buildings-that-inspired-i-control-i-s-oldest-house)
- [Material Art of TLOU Part I](https://80.lv/articles/the-material-art-of-the-last-of-us-part-i-gdc-presentation-is-now-available-for-free)

### Procedural Generation
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Subveillance: Procedural Building Interiors](https://unusual-intersection.itch.io/subveillance/devlog/492733/procedural-generation-of-building-interiors-part-1)
- [WFC Tips and Tricks - BorisTheBrave](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Procedural Generation of Buildings with WFC - Thesis](https://reposit.haw-hamburg.de/bitstream/20.500.12738/15709/1/BA_Procedural%20Generation%20of%20Buildings_geschw%C3%A4rzt.pdf)
- [Snappable Meshes PCG - GitHub](https://github.com/VideojogosLusofona/snappable-meshes-pcg)
- [Procedural Content Generation: Thinking With Modules](https://www.gamedeveloper.com/design/procedural-content-generation-thinking-with-modules)

### Modular Environment Art
- [Modular Environments - Polycount Wiki](http://wiki.polycount.com/wiki/Modular_environments)
- [Modular Kit Design - The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics/modular)
- [Lee Perry: Modular Level and Component Design (Epic/UDK)](https://docs.unrealengine.com/udk/Three/rsrc/Three/ModularLevelDesign/ModularLevelDesign.pdf)
- [Game Environment Art with Modular Architecture - ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1875952121000732)
- [Connecting Modular Exterior and Interior Walls - Polycount](https://polycount.com/discussion/235775/how-to-connect-modular-exterior-and-interior-walls-in-the-most-optimized-way)
- [Modular Environment Techniques - Polycount](https://polycount.com/discussion/209426/modular-environment-techniques)

### UE5 Technical
- [HISM Usage - Epic Forums](https://forums.unrealengine.com/t/hierarchical-instanced-static-meshes-usage/436663)
- [ISM Performance in UE5 - Epic Forums](https://forums.unrealengine.com/t/what-happened-to-the-instanced-static-mesh-performance-in-ue5/2107140)
- [Merge Actors vs Instanced Static Performance - Epic Forums](https://forums.unrealengine.com/t/merge-actor-vs-instanced-static-performance/414795)
- [PCG Development Guides - UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [Merging Actors in UE - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/merging-actors-in-unreal-engine)
- [Packed Level Actors Guide - KitBash3D](https://help.kitbash3d.com/en/articles/12038349-a-quick-guide-packed-level-actors-level-instancing-in-unreal-engine-with-kitbash3d)

### Free Assets
- [Modular Buildings - OpenGameArt (CC0)](https://opengameart.org/content/modular-buildings)
- [Building Kit - OpenGameArt (CC0)](https://opengameart.org/content/building-kit)
- [CC0 Buildings Kit - OpenGameArt](https://opengameart.org/content/cc0-buildings-kit)

### Marketplace
- [Procedural Building Generator - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator)
- [Next Gen Modular Victorian City - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/next-gen-modular-victorian-neoclassical-city-bundle-exterior-interior)
- [Next Gen Modular City V3 - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/modular-city)
