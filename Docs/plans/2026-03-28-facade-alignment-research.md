# Facade-Building Mesh Alignment and Window Integration Research

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Root cause analysis of facade/building misalignment, industry approaches to integrated building generation, recommendation for Monolith architecture
**Dependencies:** `create_building_from_grid`, `generate_facade`, `omit_exterior_walls`, existing facade-window-research, window-cutthrough-research

---

## Executive Summary

The current Monolith procedural town generator has a fundamental architecture problem: the building and its facade are **two separate StaticMeshActors**. This creates visible gaps, double-walls (windows appear as indentations, not holes), and uncovered holes when exterior walls are omitted. Facade generation in `create_city_block` is currently **disabled** (`if (false && ...)`).

After researching industry approaches across Houdini (THE FINALS), CityEngine (CGA), Unity, Blender, and UE5 GeometryScript projects, the clear recommendation is **Approach 4: Single-Pass Integrated Generation** -- building the facade directly into the building mesh during wall generation, with no separate facade actor.

**Estimated effort:** ~30-40h for the core integration, building on existing facade code.

---

## 1. Problem Analysis

### 1.1 Current Architecture

```
Step 1: create_building_from_grid
  -> Generates ONE StaticMesh with all walls (interior + exterior), floors, ceilings
  -> Exterior walls are solid AppendBox slabs
  -> Emits FExteriorFaceDef array in Building Descriptor (world coordinates)

Step 2: generate_facade (separate call)
  -> Reads FExteriorFaceDef from Building Descriptor
  -> Creates its OWN wall slabs via BuildWallSlab()
  -> Boolean-subtracts windows from ITS OWN wall slabs
  -> Appends trim, cornices, frames
  -> Saves as SEPARATE StaticMesh, spawns as SEPARATE actor
```

### 1.2 Three Failure Modes

**Double-wall (windows appear as indentations):**
The facade generates wall slabs at the same position as the building's exterior walls. Boolean subtraction cuts holes in the facade slab, but the building's solid exterior wall is still behind it. Player sees the solid building wall through the "window."

**Gap/Z-fighting between meshes:**
Two independently generated meshes at nearly-identical positions. Floating point differences in vertex positions, actor transforms, and mesh origins create visible seams, z-fighting, and light leaking.

**Uncovered holes with `omit_exterior_walls`:**
The `omit_exterior_walls` flag (already implemented) removes building exterior walls so the facade can replace them. But if the facade doesn't perfectly cover every exterior segment (e.g. different grid snapping, corner handling, or a facade face is slightly shorter/narrower), you get holes into the building interior.

### 1.3 Coordinate Space Mismatch

The building generates geometry in **local space** relative to the mesh origin. `FExteriorFaceDef.WorldOrigin` stores world-space coordinates. The facade reads these world coordinates and generates its geometry at those world positions -- but then saves the result as a StaticMesh asset and spawns it as an actor. The actor's transform (even if at 0,0,0) introduces a layer of indirection that makes perfect alignment fragile.

Even if both actors are at the same world position, the mesh vertices are in different local spaces because they were generated in different UDynamicMesh objects with different coordinate assumptions.

---

## 2. Industry Approaches

### 2.1 THE FINALS (Embark Studios / Houdini)

**Source:** [Making the Procedural Buildings of THE FINALS (SideFX)](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)

**Architecture:** Single unified mesh per building. The "Building Creator" toolset uses modular Feature Nodes that all operate on **the same geometry**:
1. Blockout mesh defines building volume
2. Exterior Walls Feature Node generates wall geometry with edge loops for floors/subdivisions
3. Manual Module Feature Node places windows/doors as **modules** with three components:
   - Visual Meshes (merged into building mesh)
   - Boolean Meshes (cut holes in the building wall geometry)
   - Mesh Sockets (spawn points for runtime details)
4. Floor/Room Feature Nodes generate interior geometry aligned to exterior wall inner faces

**Key insight:** Booleans cut into the **building's own wall geometry**, not a separate facade mesh. The facade IS the exterior wall -- there is no separate facade layer.

**Performance:** Blockout to fractured asset in 4-6 minutes. Over 100 unique buildings produced.

### 2.2 CityEngine (CGA Shape Grammar)

**Source:** [CGA Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm)

**Architecture:** Single shape tree where each rule transforms geometry in place:
```
Building -> split(y) floors -> split(x) tiles -> Window | Wall | Door
```
The facade is not a separate object -- it's the result of **recursively subdividing the building's own face geometry**. Each CGA rule takes geometry as input and replaces it with output. The leaf nodes of the shape tree ARE the final geometry.

**Key insight:** There is never a "building mesh" and a "facade mesh" -- there is only one shape being progressively refined.

### 2.3 bendemott/UE5-Procedural-Building (GeometryScript C++)

**Source:** [GitHub: bendemott/UE5-Procedural-Building](https://github.com/bendemott/UE5-Procedural-Building)

**Architecture:** Single ADynamicBuilding actor. Generates panels per wall direction, applies boolean window cuts to each panel independently, then **AppendMesh** merges everything into one mesh:
```cpp
UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
    PanelMesh, TempMesh, PanelBoxTransform);
```

**Key details:**
- Panels built in **local space** (forward-facing), then rotated via `GetPanelBoxTransform()`
- Booleans applied to individual panels BEFORE merging (cheaper than cutting the whole mesh)
- All geometry ends up in one UDynamicMesh

**Key insight:** Boolean per-panel, then merge. Not boolean on final mesh. This is the exact pattern Monolith should follow.

### 2.4 Houdini Panel-Based Facade (SideFX Tutorials)

**Source:** [Building Generator (SideFX)](https://www.sidefx.com/tutorials/building-generator/)

**Architecture:** Walls are divided into panels on a grid. Panels are classified (window panel, wall panel, door panel, corner panel). Each panel type is a pre-made mesh or procedurally generated. Panels are packed into wall segments with even spacing.

**Key insight:** No booleans needed if you build walls as discrete panels from the start. A "window panel" is simply a panel with a hole in it -- you don't need to cut a hole in a solid wall.

### 2.5 Shadows of Doubt (ColePowered Games)

**Source:** [DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)

**Architecture:** Tile-based (1.8m x 1.8m grid). Buildings are 15x15 tiles per floor. Walls are placed at tile boundaries. Exterior and interior are part of the same tile-based system -- there is no separate facade generation step.

**Key insight:** When everything is on the same grid, alignment is guaranteed by construction.

### 2.6 SkyscrapX (Blender)

**Source:** [SkyscrapX - Blender Procedural Building Generator](https://baogames.itch.io/skyscrapx)

**Architecture:** Single-mesh generation with "optimized single-mesh generation" for maximum performance. Fixed-width meshes for windows/doors, scalable meshes for plain walls.

**Key insight:** Pre-segmented wall approach (fixed modules for openings, scalable fill for solid walls) avoids booleans entirely while maintaining single-mesh output.

---

## 3. Five Candidate Approaches

### 3.1 Approach 1: Merge-Based (Post-Hoc)

Generate building and facade separately, then merge into one mesh.

```
1. create_building_from_grid (with omit_exterior_walls=true)
2. generate_facade (as UDynamicMesh, NOT saved as asset)
3. AppendMesh(BuildingMesh, FacadeMesh, FacadeTransform)
4. Save merged result as one StaticMesh
```

**Pros:**
- Minimal code change -- both generators already work
- Facade styles remain swappable (just regenerate facade portion)
- AppendMesh is trivial (~1 line of code)

**Cons:**
- Still two separate generation passes with independent coordinate assumptions
- Must ensure facade geometry covers exactly the same area as omitted exterior walls
- Corner handling between building and facade still fragile
- Material ID conflicts if both generators use different ID ranges
- Boolean artifacts at merge boundaries (T-junctions, non-manifold edges)

**Effort:** ~12-15h
**Risk:** Medium -- alignment is better but not guaranteed

### 3.2 Approach 2: Pre-Cut Walls (Segment-Based, No Booleans)

During wall generation in `create_building_from_grid`, read the facade style and generate walls as discrete segments with gaps for windows/doors.

```
Wall segment layout:
[solid][gap][solid][gap][solid]  <- exterior wall with window openings
```

Each "solid" is an AppendBox. Each "gap" is left empty. Trim/frame geometry is appended around gaps.

**Pros:**
- Zero boolean operations (fastest)
- Perfect alignment by construction (gaps are structural)
- Single mesh, single pass
- Trivially correct -- no overlapping geometry

**Cons:**
- More complex wall generation (must segment walls based on window placement)
- Harder to support complex window shapes (arches, bay windows) without booleans
- Wall UV mapping becomes fragmented across many small boxes
- Facade style must be known at building generation time (couples the two)

**Effort:** ~25-30h
**Risk:** Low -- alignment guaranteed, but implementation complexity higher

### 3.3 Approach 3: Coordinate Fix Only

Keep separate actors but fix the coordinate space alignment.

```
1. Build facade geometry in building's local space (not world space)
2. Apply identical transform to facade actor
3. Use exact same wall thickness/position calculations
```

**Pros:**
- Minimal code change
- Preserves current architecture

**Cons:**
- Does NOT fix the double-wall problem (still two meshes with overlapping walls)
- Does NOT fix the window indentation problem
- Z-fighting still possible
- Two draw calls, two collision bodies, two actors to manage
- Fundamental architecture is still wrong

**Effort:** ~4-6h
**Risk:** High -- band-aid, doesn't fix root cause

### 3.4 Approach 4: Single-Pass Integrated Generation (RECOMMENDED)

Fold facade generation INTO `create_building_from_grid`. After generating each exterior wall segment, immediately apply the facade style: cut windows, add trim, add frames.

```
For each floor:
  For each exterior wall segment:
    1. Generate wall slab (existing code)
    2. Compute window/door positions (from facade style)
    3. Boolean-subtract openings from THIS wall slab
    4. AppendMesh trim/frames to building mesh
    5. AppendMesh glass panes
  Generate interior walls (existing code)
  Generate floor/ceiling slabs (existing code)
```

**Pros:**
- One mesh, one actor, zero alignment issues
- Boolean operates on individual wall segments (cheap, localized)
- Facade style is a parameter, still swappable (just regenerate building)
- Reuses ALL existing facade code (BuildWallSlab, CutOpenings, AddWindowFrames, etc.)
- Perfect material ID consistency (one mesh, one material assignment pass)
- THE FINALS, CGA, and bendemott all use this pattern

**Cons:**
- Requires refactoring `generate_facade` into utility functions callable from building generator
- Facade style must be provided at building creation time
- Slightly larger single function (mitigated by good decomposition)
- Regenerating facade means regenerating entire building (acceptable for editor-time tool)

**Effort:** ~30-40h
**Risk:** Low -- proven pattern, reuses existing code, alignment by construction

### 3.5 Approach 5: Hybrid Panel System

Replace exterior wall generation with a panel grid system. Each panel slot is filled with a typed panel mesh (wall, window, door, corner).

```
Exterior wall grid:
[corner][wall][window][wall][window][wall][corner]
[corner][wall][window][wall][window][wall][corner]
[corner][wall][storefront     ][wall][door][corner]
```

Each panel is a pre-generated UDynamicMesh (or AppendBox + optional boolean). Panels are AppendMesh'd into the building mesh.

**Pros:**
- Most modular approach
- Panels can be cached/reused across buildings
- No boolean needed if panels are pre-cut
- Easy to swap styles (different panel sets)

**Cons:**
- Largest code change -- fundamentally different wall generation
- Panel seams need careful handling (T-junctions between panels)
- Custom panel sizes needed for different wall widths
- Over-engineered for current needs

**Effort:** ~50-60h
**Risk:** Medium -- best long-term but highest upfront cost

---

## 4. Comparison Matrix

| Criterion | Merge (1) | Pre-Cut (2) | Coord Fix (3) | Single-Pass (4) | Panel (5) |
|-----------|-----------|-------------|---------------|-----------------|-----------|
| Alignment guaranteed | Partial | Yes | No | Yes | Yes |
| Window holes real | Yes | Yes | No | Yes | Yes |
| Single mesh output | Yes | Yes | No | Yes | Yes |
| Single actor | Yes | Yes | No | Yes | Yes |
| Code reuse | High | Low | High | High | Low |
| Boolean count | Full | Zero | N/A | Per-segment | Zero or per-panel |
| Style swappable | Yes | Build-time | Yes | Build-time | Yes |
| Performance (<30s) | Yes | Yes | Yes | Yes | Yes |
| Industry precedent | Low | Medium | None | High (THE FINALS, CGA, bendemott) | High (SideFX tutorials, SkyscrapX) |
| Effort | 12-15h | 25-30h | 4-6h | 30-40h | 50-60h |
| Risk | Medium | Low | High | Low | Medium |

---

## 5. Recommendation: Approach 4 (Single-Pass Integrated)

### 5.1 Why This Wins

1. **Industry consensus.** THE FINALS (Embark/Houdini), CityEngine CGA, and the only public UE5 GeometryScript building project (bendemott) all use this pattern. Facades are not separate objects -- they are part of the building mesh.

2. **Alignment by construction.** When the facade geometry is generated in the same UDynamicMesh as the building walls, there is zero possibility of coordinate mismatch, actor transform drift, or double-wall overlap.

3. **Maximum code reuse.** The existing facade code (`BuildWallSlab`, `CutOpenings`, `AddWindowFrames`, `AddDoorFrames`, `AddCornice`, `AddBeltCourse`, `AddGlassPanes`, `ComputeWindowPositions`) already operates on a UDynamicMesh with FExteriorFaceDef inputs. These functions can be called directly from inside `create_building_from_grid`'s wall generation loop with zero modification.

4. **Boolean per-segment is fast.** bendemott's project demonstrates that applying booleans to individual wall panels (before merging into the building mesh) is significantly faster than boolean on the complete mesh. Our FExteriorFaceDef segments are already the right granularity.

5. **Style is still swappable.** The facade style becomes a parameter on `create_building_from_grid` (and by extension `create_city_block`). Different buildings in the same block can have different styles. Changing a style just means regenerating the building -- which is the expected workflow for editor-time procedural tools.

### 5.2 What Changes

**`create_building_from_grid` gains:**
- `facade_style` parameter (string, loads JSON preset -- same system as current `generate_facade`)
- `facade_seed` parameter (integer, for variation)
- After calling `GenerateWallGeometry()` for exterior segments, immediately runs facade logic on each `FExteriorFaceDef`

**`generate_facade` becomes:**
- A utility wrapper that calls the same underlying functions
- Still useful as a standalone action for applying facades to manually-built buildings or non-grid buildings
- But no longer the primary facade generation path for city blocks

**`create_city_block` gains:**
- Re-enables facade generation (currently `if (false && ...)`)
- Passes facade style through to building generation instead of calling separate `generate_facade` action
- `omit_exterior_walls` becomes the default when a facade style is provided (building skips solid exterior walls, facade functions generate the exterior walls with openings)

### 5.3 Detailed Integration Plan

#### Phase 1: Extract Facade Utilities (~8h)

1. Move facade geometry functions from `FMonolithMeshFacadeActions` to a shared utility header (or make them `public static` -- they already are)
2. Ensure all facade functions take `UDynamicMesh*` as first parameter (they already do)
3. Verify `FFacadeStyle` loading works from building actions context
4. No behavioral changes -- just accessibility refactoring

#### Phase 2: Integrate Into Building Generator (~16h)

1. Add `facade_style` and `facade_seed` parameters to `create_building_from_grid`
2. In the per-floor loop, after `GenerateWallGeometry()`:
   ```
   if (bHasFacadeStyle && !ExteriorFaces.IsEmpty())
   {
       for (const FExteriorFaceDef& Face : ExteriorFaces)
       {
           // These already exist and work correctly:
           BuildWallSlab(Mesh, Face, ExteriorT, Style.WallMaterialId);
           windows = ComputeWindowPositions(Face, Style, ...);
           CutOpenings(Mesh, Face, windows, doors, ExteriorT, bHadBooleans);
           AddWindowFrames(Mesh, Face, windows, Style);
           AddDoorFrames(Mesh, Face, doors, Style);
           AddGlassPanes(Mesh, Face, windows, Style);
       }
       // Per-floor horizontal trim
       AddBeltCourse(Mesh, ...);
   }
   ```
3. When `facade_style` is set, automatically enable `omit_exterior_walls` for the base wall generator (so facade's `BuildWallSlab` is the ONLY exterior wall)
4. After all floors: add cornice at roofline
5. Emit window/door metadata in Building Descriptor (for AI pathing, horror systems, etc.)

#### Phase 3: City Block Integration (~6h)

1. Remove `if (false && ...)` guard in `create_city_block`
2. Pass facade style through building params instead of calling `generate_facade`
3. Support per-building style override in city block spec
4. Horror decay parameter maps to facade damage level

#### Phase 4: Testing and Polish (~6h)

1. Verify windows are real holes (camera inside room, looking out)
2. Verify no double-walls
3. Verify material IDs are consistent (exterior wall, trim, glass, door in correct slots)
4. Verify UV mapping on segmented exterior walls
5. Performance test: 4-building block in <30s
6. Test with multiple facade styles in one block

### 5.4 The `omit_exterior_walls` Interaction

The existing `omit_exterior_walls` flag is the linchpin. When facade generation is integrated:

1. **Without facade style:** Building generates solid exterior walls as before (`omit_exterior_walls=false`)
2. **With facade style:** Building skips solid exterior walls (`omit_exterior_walls=true` automatically). Facade functions generate the exterior walls WITH openings cut in. The facade IS the exterior wall.

This means `FExteriorFaceDef` entries are always emitted (they describe WHERE exterior faces are), but the solid wall geometry is only generated if no facade will replace it. This is exactly how the flag was designed.

---

## 6. Performance Analysis

### 6.1 Boolean Cost Per Segment

Each exterior wall segment typically has 2-5 windows. The merged-cutter optimization (already implemented in `CutOpenings`) combines all cutters into one boolean operation per segment. Cost per segment: ~5-15ms.

A typical 4-building city block has ~40-60 exterior face segments total. Total boolean time: ~200-900ms.

### 6.2 Comparison With Current Two-Mesh Approach

| Metric | Current (disabled) | Proposed (single-pass) |
|--------|-------------------|----------------------|
| Actors per building | 2 | 1 |
| Draw calls per building | 2+ | 1 |
| Boolean operations | Same count | Same count (per-segment) |
| Alignment errors | Frequent | Impossible |
| Total mesh generation time | ~15-25s per block | ~15-25s per block (same work, no overhead) |
| Collision bodies | 2 | 1 |

### 6.3 AppendMesh Cost

`AppendMesh` is essentially a vertex/triangle copy operation. For trim, frames, and glass panes (small geometry), cost is negligible (<1ms per element).

---

## 7. Alternative: When to Use Approach 2 (Pre-Cut)

The pre-cut segment approach (no booleans) becomes attractive if:
- Boolean performance becomes a bottleneck (unlikely at editor-time)
- We need runtime procedural generation (booleans are too slow for runtime)
- Window shapes become complex enough that booleans produce artifacts

For now, Approach 4 with per-segment booleans is the right call. The boolean code already works, is tested, and handles the existing window/door shapes correctly.

If we later need to optimize, the pre-cut approach can replace the boolean step inside the same single-pass architecture. The integration architecture doesn't change -- only the "how do we make holes in this wall segment" implementation.

---

## 8. Edge Cases and Mitigations

### 8.1 Corner Handling

When two exterior faces meet at a building corner, both faces extend to the corner point. The facade's `BuildWallSlab` already handles this (it builds a box matching the face dimensions). Corner trim (quoins) can be appended as separate geometry at corner vertices.

### 8.2 Non-Rectangular Buildings

L-shaped and T-shaped buildings have interior corners where exterior faces change direction. `FExteriorFaceDef` already captures these correctly (each straight segment is a separate face). The facade functions process each face independently.

### 8.3 Varying Floor Heights

The CGA tripartite division (base/shaft/cap) requires different floor heights. Currently `create_building_from_grid` uses uniform floor heights. The facade integration should respect per-floor height overrides from the Building Descriptor.

### 8.4 Ground Floor Special Treatment

Storefronts, entrances, and residential ground floors need different facade treatment. The existing `ground_floor_style` parameter already handles this. In the integrated approach, this parameter would be part of the facade style spec.

---

## 9. Relationship to Existing Research

This research supersedes and consolidates:

| Document | Relationship |
|----------|-------------|
| `facade-window-research.md` | Facade generation theory and algorithms -- still valid, now integrated INTO building generator |
| `window-cutthrough-research.md` | Root cause confirmed (double-wall). Fix is Approach 4 (single-pass). `omit_exterior_walls` fix is a subset of this |
| `connected-room-assembly-research.md` | Room assembly generates the building structure; facade is applied to its exterior faces |
| `thin-wall-geometry-research.md` | Swept-wall approach for exterior perimeter complements facade integration (swept wall = base exterior, facade adds openings) |

---

## Sources

### Production Systems
- [Making the Procedural Buildings of THE FINALS (SideFX)](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [Creating Procedural Buildings in THE FINALS with Houdini (80.lv)](https://80.lv/articles/how-embark-studios-built-procedural-environments-for-the-finals-using-houdini)
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)

### Academic / Reference
- [CGA Shape Grammar (Muller et al., ACM TOG 2006)](https://dl.acm.org/doi/10.1145/1141911.1141931)
- [CityEngine Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm)
- [CityEngine Tutorial 7: Facade Modeling](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-7-facade-modeling.htm)
- [Procedural Generation of Multi-Story Buildings with Interior (Freiknecht & Effelsberg)](https://www.researchgate.net/publication/340615711_Procedural_Generation_of_Multi-Story_Buildings_with_Interior)
- [Procedural Facade Variations from a Single Layout (ACM TOG)](https://dl.acm.org/doi/10.1145/2421636.2421644)
- [Exploring Procedural Generation of Buildings (Taljsten, 2021)](https://www.diva-portal.org/smash/get/diva2:1480518/FULLTEXT01.pdf)

### GeometryScript / UE5
- [bendemott/UE5-Procedural-Building (GitHub)](https://github.com/bendemott/UE5-Procedural-Building)
- [GeometryScript Users Guide (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
- [AppendMesh API (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/MeshEdits/AppendMesh)
- [ApplyMeshBoolean API (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Booleans/ApplyMeshBoolean)
- [Geometry Script Boolean Operations Tutorial](https://dev.epicgames.com/community/learning/tutorials/v0b/unreal-engine-ue5-0-geometry-script-mesh-booleans-and-patterns)
- [GeometryScript FAQ (gradientspace)](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq)

### Tools / Plugins
- [SideFX Building Generator Tutorial](https://www.sidefx.com/tutorials/building-generator/)
- [SkyscrapX - Blender Procedural Building Generator](https://baogames.itch.io/skyscrapx)
- [Procedural Building Generator (UE Marketplace)](https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator)

### Coordinate Systems
- [Coordinate System and Spaces in UE (UE 5.7 Docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/coordinate-system-and-spaces-in-unreal-engine)
