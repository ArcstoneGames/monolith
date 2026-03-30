# Procedural Roof Generation Research

**Date:** 2026-03-28
**Context:** Extending `create_structure` and `create_building_shell` with roof geometry
**Engine:** UE 5.7 GeometryScript

---

## 1. Roof Type Taxonomy

### 1.1 Flat Roof (with optional parapet)

**Building types:** Commercial buildings, modernist houses, industrial, mid-century modern, brutalist.
**Geometry:** Simplest -- just a slab (already exists in `create_building_shell` as the roof slab at `TotalH`). Parapet = short extruded wall ring on top.
**Implementation:** `AppendSimpleExtrudePolygon` with footprint at roof height (parapet walls), or `AppendSimpleSweptPolygon` with a small rectangular profile swept around the roof perimeter.

### 1.2 Gable Roof (triangular, 2 slopes)

**Building types:** Residential houses, cabins, churches, barns, colonial, traditional European.
**Geometry:** Two rectangular slope planes meeting at a central ridge line. Ridge runs parallel to the longer building axis. Triangular gable ends on the short sides.
**Key parameters:** Pitch angle (typically 30-45 degrees) or ridge height, ridge offset (centered or asymmetric).

### 1.3 Hip Roof (4 slopes)

**Building types:** Ranch houses, Mediterranean, French country, colonial, bungalows.
**Geometry:** Four sloped faces, all meeting at a ridge (for rectangular plans) or a single apex (for square plans). No vertical gable ends -- all sides slope inward.
**Key parameters:** Pitch angle, ridge length (= building length - 2 * building width * tan(complementary_angle)).

### 1.4 Gambrel Roof (barn-style, double-slope)

**Building types:** Barns, Dutch colonial, farmhouses.
**Geometry:** Each side has two slopes: a steep lower slope (~60 degrees) and a shallower upper slope (~30 degrees). Creates more usable interior space.
**Key parameters:** Lower pitch, upper pitch, break height (where the slope changes).

### 1.5 Mansard Roof (4-sided gambrel)

**Building types:** Second Empire, French architecture, Parisian apartments, Victorian.
**Geometry:** Like a hip roof but each of the four sides has a double-slope (steep lower, shallow upper). Often nearly flat on top.
**Key parameters:** Lower pitch (steep, 70-80 degrees), upper pitch (shallow, 20-30 degrees), break height.

### 1.6 Shed Roof (single slope / mono-pitch)

**Building types:** Modern homes, additions, lean-tos, industrial.
**Geometry:** Single plane tilted from one wall (high side) to the opposite wall (low side).
**Key parameters:** Pitch direction, pitch angle.

### 1.7 Dormers

**Building types:** Added to any pitched roof for light/ventilation. Common on gable, hip, mansard roofs.
**Geometry:** Small gable-roofed or shed-roofed projection with a vertical window face. Boolean subtract into the main roof slope, then add dormer geometry.
**Key parameters:** Position along roof, width, height, dormer roof type (gable/shed/hip).

---

## 2. Straight Skeleton Algorithm

### 2.1 Overview

The **straight skeleton** is the standard algorithm for computing hip roof shapes from arbitrary polygons. Given a 2D polygon (building footprint), the straight skeleton produces:
- **Ridge lines** -- where roof planes from different edges meet
- **Ridge vertices** -- where three or more roof planes intersect
- The skeleton partitions the polygon into quadrilaterals (and some triangles), each corresponding to one roof face that slopes inward from its parent edge.

### 2.2 How It Works

1. All polygon edges begin shrinking inward simultaneously at the same speed (like a wavefront / polygon offset).
2. **Edge events:** Two adjacent shrinking edges meet, collapsing to a point. The shorter edge disappears.
3. **Split events:** A reflex (concave) vertex hits an opposite edge, splitting the polygon into two.
4. The locus of the moving vertices traces out the straight skeleton.
5. Each original edge produces a roof face whose slope is determined by the uniform shrink rate.

### 2.3 Complexity

- **Time:** O(n^2 log n) for the general case (Aichholzer & Aurenhammer 1996), O(n log n) for convex polygons.
- **Space:** O(n)
- For simple rectangular footprints: trivial degenerate case -- ridge is a single line segment.
- For L-shapes, T-shapes, and complex footprints: the algorithm handles all cases correctly.

### 2.4 Existing Implementations

- **CGAL** (C++): `CGAL::create_straight_skeleton_2()` -- robust, well-tested, LGPL. NOT suitable for inclusion in a UE plugin directly (license, build complexity).
- **Boost.Polygon**: Does not include straight skeleton.
- **UE 5.7 Engine**: **No straight skeleton implementation found** in the engine source (searched for "straight skeleton" -- no results). The `InsetPolygon2D` helper in Monolith does simple per-vertex offset but does NOT handle topology changes (edge collapse, split events).
- **Felkel & Obdrzalek (1998)**: Clean reference implementation, easy to port. ~200-400 lines of C++.
- **Simplified rectangular approach**: For axis-aligned rectangles, hip roof computation is trivial (no algorithm needed -- just compute the ridge from geometry).

### 2.5 Recommendation for Monolith

**Phase 1 (immediate):** Support only rectangular footprints. Hip roof is trivially computed:
- Ridge height = min(W/2, D/2) * tan(pitch_angle)
- Ridge endpoints for rectangular building of W x D: two points along the longer axis

**Phase 2 (future, if needed):** Implement simplified straight skeleton for convex polygons (O(n log n)). This covers 90%+ of real building footprints. ~300 lines of C++.

**Phase 3 (unlikely needed):** Full straight skeleton with split event handling for non-convex polygons.

---

## 3. Simple Approaches for Rectangular Buildings

### 3.1 Approach A: AppendSimpleExtrudePolygon with Triangular Cross-Section

**NOT directly applicable.** `AppendSimpleExtrudePolygon` extrudes a 2D polygon **vertically** (along Z). It cannot create angled roof slopes. The polygon is 2D (XY) and height is uniform.

### 3.2 Approach B: AppendSimpleSweptPolygon with Wedge Profile

**YES -- this is the primary recommended approach for gable roofs.**

Define a triangular (or trapezoidal) cross-section as the 2D polygon, then sweep it along the ridge line:

```
Gable cross-section (looking along ridge):
    /\          Profile vertices (CCW):
   /  \         (-W/2-OH, 0), (0, RH), (W/2+OH, 0)
  /    \        where OH = overhang, RH = ridge_height
 /______\
```

Sweep this triangle along a 2-point path from one gable end to the other. The path runs along the building's long axis. `bLoop=false`, `bCapped=true` gives triangulated gable ends.

**API signature (verified):**
```cpp
AppendSimpleSweptPolygon(
    TargetMesh, PrimitiveOptions, Transform,
    PolygonVertices,  // TArray<FVector2D> -- the 2D cross-section
    SweepPath,        // TArray<FVector> -- the 3D path
    bLoop = false,    // open-ended for gable
    bCapped = true,   // triangulate the gable ends
    StartScale = 1.0f, EndScale = 1.0f,
    RotationAngleDeg = 0.0f,
    MiterLimit = 1.0f
)
```

The 2D polygon coordinate system: if sweep path goes along X, then Y is "right" and Z is "up" in the polygon's local frame.

### 3.3 Approach C: AppendTriangulatedPolygon3D + AppendRectangleXY

For hip roofs on rectangles, construct the 4 triangular/trapezoidal faces manually:

1. Compute the 4 roof face polygons as 3D vertex arrays
2. Use `AppendTriangulatedPolygon3D` for each face (supports arbitrary 3D polygon, ear-clipping triangulation)
3. This gives maximum control over individual face geometry and material assignment

**API signature (verified):**
```cpp
AppendTriangulatedPolygon3D(
    TargetMesh, PrimitiveOptions, Transform,
    PolygonVertices3D,  // TArray<FVector> -- 3D vertices
    Debug
)
```

### 3.4 Approach D: AppendSweepPolyline (most flexible)

`AppendSweepPolyline` sweeps an **open** 2D polyline along a path defined by `TArray<FTransform>` (not just positions). This allows rotation/scale changes along the path. Useful for:
- Gambrel/mansard profiles where the cross-section varies
- Dormers where the sweep needs to be short and positioned precisely

### 3.5 Approach E: AppendBox with Transform Rotation

For simple shed roofs, a rotated box works: `AppendBox` with a rotated `FTransform` to tilt the slab. Quick and dirty but UV-unfriendly.

---

## 4. Recommended Implementation Strategy Per Roof Type

### 4.1 Flat Roof (with parapet)
```
Method: AppendSimpleExtrudePolygon (slab) + AppendSimpleSweptPolygon (parapet walls)
Complexity: Trivial
Tri count: ~8 (slab) + ~16-32 (parapet)
```
- Slab: extrude footprint by `slab_thickness` at building top
- Parapet: sweep a small rectangular profile (e.g., 15cm wide x 60cm tall) around the roof perimeter with `bLoop=true`
- Parapet top: optionally cap with a thin coping slab

### 4.2 Gable Roof
```
Method: AppendSimpleSweptPolygon (triangular profile along ridge)
Complexity: Simple
Tri count: ~6-10 (2 slopes + 2 gable ends)
```
- Cross-section: triangle with base = building width + 2*overhang, height = ridge_height
- Sweep path: 2 points along the long axis, from -depth/2-overhang to +depth/2+overhang
- `bLoop=false`, `bCapped=true` for gable end walls
- **Overhang:** Extend the base of the triangle past the wall faces by `overhang` (30-50cm)
- **Soffit underside:** The overhang creates visible underside geometry automatically (the swept polygon is closed)

### 4.3 Hip Roof (rectangular)
```
Method: AppendTriangulatedPolygon3D (4 faces) or manual vertex construction
Complexity: Moderate
Tri count: ~8-16 (4 faces, each 2-4 tris)
```

For a rectangular building W x D (W >= D), the hip roof has:
- **Ridge line** at height H from `(-RidgeLen/2, 0, H)` to `(RidgeLen/2, 0, H)` where `RidgeLen = W - D` (if W > D) and `H = D/2 * tan(pitch)`
- **4 eave corners** at `(+-W/2+OH, +-D/2+OH, 0)` (OH = overhang)
- **2 trapezoidal faces** (long sides): each has 4 vertices (2 eave corners + 2 ridge endpoints)
- **2 triangular faces** (short sides): each has 3 vertices (2 eave corners + 1 ridge endpoint, or apex if square)

Use `AppendTriangulatedPolygon3D` for each face with appropriate `MaterialID` in `PrimitiveOptions`.

**Alternative for hip:** Use `AppendSimpleSweptPolygon` with the triangular profile BUT with `StartScale=0.0, EndScale=1.0` from apex to mid-ridge, then mirror. This is trickier; direct polygon construction is cleaner.

### 4.4 Gambrel Roof
```
Method: AppendSimpleSweptPolygon (gambrel profile along ridge)
Complexity: Simple (same as gable, different profile)
Tri count: ~10-14
```
- Cross-section: hexagonal profile with two break points per side
  ```
  Profile (CCW, looking along ridge):
    (-W/2-OH, 0) -> (-BreakX, BreakH) -> (-RidgeInset, RH) -> (RidgeInset, RH) -> (BreakX, BreakH) -> (W/2+OH, 0)
  ```
  where BreakX/BreakH define the slope transition point
- Sweep along ridge exactly like gable

### 4.5 Mansard Roof
```
Method: AppendTriangulatedPolygon3D (8 faces) or layered approach
Complexity: Moderate-High
Tri count: ~16-24
```

Two approaches:
1. **Layered:** Use `InsetPolygon2D` to create the inner rectangle at the break height. Then:
   - 4 lower faces (steep slope): from eave corners to inset corners at break height
   - 4 upper faces (shallow slope): from inset corners to ridge/flat top
   - Each face via `AppendTriangulatedPolygon3D`

2. **Profile sweep (if rectangular):** Like gambrel but for all 4 sides. This requires 4 separate sweeps or manual face construction.

### 4.6 Shed Roof
```
Method: AppendTriangulatedPolygon3D (1 quad + 2 triangular gable walls)
Complexity: Trivial
Tri count: ~4-6
```
- Single sloped plane: 4 corners at different heights
- Two triangular side walls connecting high edge to low edge
- Or simply use `AppendSimpleSweptPolygon` with a right-triangle profile swept across the building width

### 4.7 Dormers
```
Method: Boolean subtract + AppendSimpleSweptPolygon
Complexity: High (requires intersection with existing roof)
Tri count: varies, ~20-40 per dormer
```
- Boolean subtract a box from the main roof to create the dormer opening
- Add dormer walls (3 sides -- 2 side cheeks + 1 face wall)
- Add dormer roof (small gable or shed sweep)
- Add window opening in face wall
- **Defer to Phase 2** -- dormers are complex and less critical for blockout

---

## 5. Overhangs and Eaves

### 5.1 Overhang Distance
- Typical residential: 30-60cm (12-24 inches)
- Commercial/modern: 0-30cm (minimal or flush)
- Craftsman/prairie: 60-120cm (deep overhangs)
- Default recommendation: **40cm** (moderate, good shadow casting)

### 5.2 Implementation
Overhangs are achieved by making the roof footprint **larger** than the wall footprint:

```cpp
// Roof footprint = building footprint expanded by overhang
TArray<FVector2D> RoofBase = InsetPolygon2D(Footprint, -Overhang); // negative inset = outset
```

For gable roofs via sweep: make the triangular cross-section base wider than the building:
```
Base width = BuildingWidth + 2 * Overhang
```
And extend the sweep path by `Overhang` past each gable end.

### 5.3 Soffit (Underside of Overhang)
The swept polygon naturally creates the soffit surface as the bottom face of the cross-section. For hip roofs with manually constructed faces, add a flat polygon under the overhang area:
```
Soffit face vertices: outer eave corner, inner eave corner (at wall), same for next corner
```

### 5.4 Fascia Board
A thin vertical strip at the eave edge. Can be a separate swept geometry:
```cpp
// Fascia: thin rectangle profile swept along eave edge
TArray<FVector2D> FasciaProfile; // e.g., 2cm wide x 15cm tall
// Sweep along the eave path (bottom edge of roof, going around the building)
```

---

## 6. Gutters and Fascia (Trim Geometry)

### 6.1 Gutter Profile
A small C-shaped or U-shaped profile swept along the eave edge:
```
Profile (2D, ~8cm wide x 8cm tall):
  Half-round gutter: semicircle
  Box gutter: rectangular U-shape
  K-style gutter: angular profile
```

Use `AppendSimpleSweptPolygon` with the gutter profile swept along the eave edge path. The eave edge path follows the bottom edge of the roof at each side.

### 6.2 Fascia Board
Thin rectangular profile (2cm x 15-20cm) swept along eave edges. Sits behind the gutter.

### 6.3 Ridge Cap
A small inverted-V or rounded profile swept along the ridge line. Typically 10-15cm wide.

### 6.4 Material Assignment
Use `PrimitiveOptions.MaterialID` to assign different material slots:
- Slot 0: Walls
- Slot 1: Roof surface (shingles/tiles/metal)
- Slot 2: Trim/fascia
- Slot 3: Gutter

### 6.5 Recommendation for Blockout
**Skip gutters and fascia for V1.** These are detail geometry better handled by:
1. Static mesh instancing from asset library
2. A dedicated `add_roof_trim` action in a later phase
3. Spline mesh components for production quality

For blockout, the roof surface itself is sufficient. Add a `bool add_trim` parameter defaulting to `false`.

---

## 7. Material Zones and UV Mapping

### 7.1 Roof Surface Materials
- **Asphalt shingles:** 1m x 0.33m tile pattern, UV scale ~100x33
- **Clay/concrete tiles:** 0.3m x 0.4m, UV scale ~30x40
- **Standing seam metal:** Vertical ribs at ~40cm spacing
- **Slate:** ~30cm x 20cm irregular
- **Flat/membrane:** Usually solid color, UV doesn't matter much

### 7.2 UV Strategy for Roof Faces

**Option A: Box Projection (existing approach)**
`SetMeshUVsFromBoxProjection` already used in `create_structure`. Works well for walls but can produce stretching on angled roof surfaces where no projection axis aligns well.

**Option B: Per-Face Planar Projection (recommended for roofs)**
For each roof face, project UVs onto the plane of that face. This gives proper tiling along the slope direction. Implementation:
1. Compute face normal for each roof polygon group
2. Project vertices onto the face plane for UV coordinates
3. Scale by desired tiling factor

**Option C: Sweep-Generated UVs**
`AppendSimpleSweptPolygon` and `AppendSweepPolygon` automatically generate UVs based on the sweep. The polygon profile maps to U, the sweep distance maps to V. This produces good results for gable/gambrel roofs without extra work.

### 7.3 MaterialID Assignment
Different roof components should use different material IDs:
```cpp
// Roof slopes
FGeometryScriptPrimitiveOptions RoofOpts;
RoofOpts.MaterialID = 1; // Roof material slot

// Gable end walls (same as building walls)
FGeometryScriptPrimitiveOptions GableOpts;
GableOpts.MaterialID = 0; // Wall material slot

// Trim/fascia
FGeometryScriptPrimitiveOptions TrimOpts;
TrimOpts.MaterialID = 2; // Trim material slot
```

### 7.4 Sweep UV Behavior
For `AppendSimpleSweptPolygon`:
- The polygon profile vertices map to U coordinates (based on arc length around the profile)
- The sweep path distance maps to V coordinates
- This means for a gable roof swept along the ridge, U runs across the slope (eave to ridge to eave) and V runs along the building length
- This is actually ideal for shingle/tile patterns

---

## 8. Multi-Story Roofs

### 8.1 Integration with create_building_shell
Currently, `create_building_shell` places a flat roof slab at `TotalH`. The pitched roof should:
1. **Replace** the flat roof slab (don't generate it if `roof_type != "flat"`)
2. Sit on top of the wall crown at `TotalH`
3. Optionally extend down into the top floor for attic space (reducing the interior void)

### 8.2 Intermediate Floor Ceilings
For multi-story buildings, intermediate floors are flat slabs (already implemented). Only the **topmost** floor gets a pitched roof. The parameter `roof_type` should only affect the top of the building.

### 8.3 Half-Story / Attic
For gambrel and mansard roofs, the roof structure creates usable space. The "attic floor" is at `TotalH`, and the roof volume sits above it. The interior void should NOT be cut into the roof volume.

### 8.4 Implementation
```
if (RoofType == "flat")
{
    // Existing behavior: slab at TotalH
    AppendSimpleExtrudePolygon(Mesh, ..., Footprint, SlabT, ...);
}
else
{
    // Skip the flat roof slab
    // Add pitched roof geometry at Z = TotalH
    GenerateRoof(Mesh, Footprint, RoofType, TotalH, RoofParams);
}
```

---

## 9. Proposed API Design

### 9.1 New Parameters for create_building_shell
```json
{
    "footprint": [[x,y], ...],
    "floors": 2,
    "floor_height": 300,
    "roof_type": "gable",
    "roof_params": {
        "pitch": 35,
        "overhang": 40,
        "ridge_direction": "auto",
        "add_fascia": false,
        "add_gutter": false
    }
}
```

### 9.2 Roof Types Enum
```
"flat"      -- existing behavior (default, backward compatible)
"parapet"   -- flat + parapet walls
"gable"     -- triangular, 2 slopes
"hip"       -- 4 slopes
"gambrel"   -- barn-style double slope
"mansard"   -- 4-sided double slope
"shed"      -- single slope
```

### 9.3 Standalone create_roof Action (Optional)
Could also be a standalone action for adding roofs to arbitrary geometry:
```json
{
    "action": "create_roof",
    "params": {
        "footprint": [[x,y], ...],
        "type": "hip",
        "pitch": 30,
        "overhang": 40,
        "base_height": 600,
        "auto_save": true
    }
}
```

---

## 10. GeometryScript API Summary for Roof Generation

| API | Use Case | Verified |
|-----|----------|----------|
| `AppendSimpleSweptPolygon` | Gable, gambrel, shed roofs (profile along ridge) | Yes (line 548, MeshPrimitiveFunctions.h) |
| `AppendSimpleExtrudePolygon` | Flat roof slabs, parapet walls | Yes (line 524) |
| `AppendTriangulatedPolygon3D` | Hip roof faces, mansard faces, custom shapes | Yes (line 664) |
| `AppendSweepPolyline` | Gutter profiles, fascia, ridge caps (open polyline) | Yes (line 503) |
| `AppendSweepPolygon` | Closed-profile trim along FTransform path | Yes (line 577) |
| `AppendBox` | Quick parapet segments, flat trim pieces | Yes (line 168) |
| `AppendCone` | Turret/tower roof caps | Yes (line 408) |
| `SetMeshUVsFromBoxProjection` | Fallback UV mapping | Yes (used in create_structure) |
| `ApplyMeshBoolean` | Dormer cutouts | Yes (used in create_structure) |
| `InsetPolygon2D` (Monolith) | Overhang expansion (negative inset), inner polygon | Yes (line 1460, MonolithMeshProceduralActions.cpp) |

---

## 11. Implementation Plan

### Phase 1: Core Roof Types (~12-16h)
1. **GenerateRoof helper** -- dispatch function taking footprint, type, params, target mesh
2. **Flat + Parapet** (~2h) -- parapet via swept rectangle profile around roof edge
3. **Gable** (~3h) -- triangular profile swept along ridge, with overhang
4. **Hip (rectangular only)** (~4h) -- 4 faces via AppendTriangulatedPolygon3D, compute ridge from dimensions
5. **Shed** (~2h) -- single tilted face via AppendTriangulatedPolygon3D
6. **Integration with create_building_shell** (~3h) -- replace flat roof slab, handle multi-story

### Phase 2: Advanced Roof Types (~8-10h)
1. **Gambrel** (~3h) -- hexagonal profile swept along ridge
2. **Mansard** (~4h) -- 8-face construction, two slope tiers, InsetPolygon2D for the break polygon
3. **Per-face UV projection** (~3h) -- proper slope-aligned UVs instead of box projection

### Phase 3: Trim and Dormers (~10-14h, defer)
1. **Fascia** (~3h) -- thin rect profile swept along eave edges
2. **Gutters** (~3h) -- half-round or K-style profile along eaves
3. **Ridge cap** (~2h) -- small profile swept along ridge
4. **Dormers** (~6h) -- boolean subtract + gable/shed mini-roof + window

### Total Estimate: ~30-40h across all phases

---

## 12. Key Technical Notes

### 12.1 Coordinate System for Swept Profiles
Per the header comments (line 489, 543): "If the 2D vertices are (U,V), then in the coordinate space of the FTransform, X points 'along' the path, Y points 'right' (U) and Z points 'up' (V)."

For `AppendSimpleSweptPolygon` (which uses `TArray<FVector>` path, not `TArray<FTransform>`), the framing is auto-computed. The profile polygon is in Y/Z space, swept along the path direction.

### 12.2 Polygon Winding
All polygons must be **counter-clockwise** for correct outward-facing normals. The existing `InsetPolygon2D` assumes CCW winding. Roof face polygons must be ordered CCW when viewed from outside the building.

### 12.3 PolygroupMode
Set `PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerFace` for roof geometry so individual faces can be selected later for material assignment or editing.

### 12.4 Performance
- Swept polygon: ~1-2ms per call
- Triangulated polygon: <1ms per face
- Boolean (dormers only): 5-50ms each
- Total roof generation for a rectangular building: <10ms for basic types, <100ms with dormers

### 12.5 The Overhang Soffit Problem
When using `AppendSimpleSweptPolygon` for gable roofs, the profile is a closed triangle. The bottom edge of the triangle IS the soffit -- it's automatically generated. However, it will be a single flat plane from eave to eave, passing through the building. This is correct for exterior view (the interior void subtraction in `create_building_shell` handles the inside). For standalone `create_roof`, the bottom face may need to be removed or clipped.

### 12.6 Gable End Walls
When `bCapped=true` on `AppendSimpleSweptPolygon`, the gable end triangles are generated automatically. These should use `MaterialID=0` (wall material), but the sweep uses a single MaterialID for the entire mesh. Solutions:
1. Generate the roof sweep without caps (`bCapped=false`), then add gable triangles separately with wall MaterialID
2. Use a post-process to reassign MaterialID on the end-cap triangles based on normal direction (horizontal-ish normals = wall, steep normals = roof)
3. Accept uniform MaterialID for blockout (simplest for V1)

**Recommendation:** Option 1 (separate gable ends) for production quality, Option 3 for initial blockout.

---

## 13. Horror-Specific Considerations

For Leviathan's survival horror context:
- **Dilapidated roofs:** `ApplyPerlinNoiseToMesh2` on roof surface for warped/sagging appearance
- **Missing shingles:** Boolean subtract small random patches from roof surface
- **Collapsed sections:** Remove triangles from specific roof faces, add debris geometry below
- **Attic access:** Dormer or roof hatch openings for gameplay traversal
- **Skylight horror:** Glass ceiling panels in flat roofs for moonlight/lightning effects

These are stretch goals, not blockers for the roof generation system.
