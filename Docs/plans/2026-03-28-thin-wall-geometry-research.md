# Thin-Wall Room Geometry Research

**Date:** 2026-03-28
**Target:** Replace cube-based wall construction in `create_structure` with proper thin-wall architectural geometry
**Source file:** `Source/MonolithMesh/Private/MonolithMeshProceduralActions.cpp` (line 1577+)

---

## 1. Current Implementation Analysis

### How create_structure Works Now

The `CreateStructure` function (line 1580) builds rooms by assembling 4 separate AppendBox calls per room:

```cpp
auto BuildWalls = [&](float W, float D, float H, float WT, FVector Offset)
{
    // North wall: full-width box, WT deep
    AppendBox(Mesh, Opts, NorthXf, W, WT, H, 0, 0, 0, Base);
    // South wall: full-width box, WT deep
    AppendBox(Mesh, Opts, SouthXf, W, WT, H, 0, 0, 0, Base);
    // East wall: WT wide, D-WT*2 deep (avoiding corner overlap)
    AppendBox(Mesh, Opts, EastXf, WT, D - WT * 2, H, 0, 0, 0, Base);
    // West wall: WT wide, D-WT*2 deep
    AppendBox(Mesh, Opts, WestXf, WT, D - WT * 2, H, 0, 0, 0, Base);
};
```

Floor and ceiling are full-footprint slabs:
```cpp
AppendBox(Mesh, Opts, FloorXf, Width, Depth, FloorT, 0, 0, 0, Base);  // 15cm thick
AppendBox(Mesh, Opts, CeilXf, Width, Depth, FloorT, 0, 0, 0, Base);
```

Door/window openings use boolean subtraction (AppendBox cutter + ApplyMeshBoolean Subtract).

### Problems

1. **Walls are solid cubes** -- a 400x600x300 room with 20cm walls creates 4 solid boxes that are 20cm thick. This looks like bunker geometry, not drywall.
2. **Floor/ceiling are thick slabs** -- 15cm solid boxes for floors.
3. **Corners use overlap avoidance** -- East/West walls are shortened by `WT*2` to avoid Z-fighting with North/South walls. This creates visible seams in the corner geometry.
4. **No interior detail** -- walls are flat 6-face boxes. No baseboard, no crown molding, no wall panel subdivision.
5. **Boolean openings are expensive** -- each door/window requires a full mesh boolean operation.
6. **UV mapping is automatic** -- AppendBox generates default box UVs which don't tile well on architectural surfaces.

### The Good News: create_building_shell Already Does It Better

The `CreateBuildingShell` function (line 1782) already uses the correct approach:

```cpp
// Extrude outer shell from 2D footprint
AppendSimpleExtrudePolygon(Mesh, Opts, Identity, Footprint, TotalH, 0, true, Base);

// Inset polygon for interior void
TArray<FVector2D> InnerPoly = InsetPolygon2D(Footprint, WallT);

// Subtract interior
AppendSimpleExtrudePolygon(InnerCutter, Opts, InnerXf, InnerPoly, TotalH, 0, true, Base);
ApplyMeshBoolean(Mesh, Identity, InnerCutter, Identity, Subtract, BoolOpts);
```

This is the "shell from floor plan" approach -- extrude outer, extrude inner, boolean subtract. It works but still requires a boolean operation.

---

## 2. Available GeometryScript APIs

### Primitives (MeshPrimitiveFunctions.h)

| Function | Description | Thin-wall use |
|----------|-------------|---------------|
| `AppendBox` | 3D box with DimensionX/Y/Z, StepsX/Y/Z, OriginMode | Current approach -- makes solid cubes |
| `AppendRectangleXY` | **Planar rectangle** (zero thickness, single face) with DimensionX/Y, StepsWidth/Height | Perfect for single-face wall panels |
| `AppendSimpleExtrudePolygon` | Extrude 2D polygon (TArray<FVector2D>) upward by Height. Capped = solid ends | Used by BuildingShell. Creates thin-wall extrusion |
| `AppendSimpleSweptPolygon` | Sweep 2D polygon along 3D path (TArray<FVector>). Loop, Capped, Scale, MiterLimit | Used by pipes. **Key for wall profile sweeping** |
| `AppendSweepPolygon` | Sweep 2D polygon along FTransform path. More control than Simple version | Advanced version with per-frame transforms |
| `AppendSweepPolyline` | Sweep an **open** polyline (not closed polygon) along FTransform path. TexParam UV control | Could sweep a wall-height line segment along floor path |
| `AppendTriangulatedPolygon` | Flat polygon triangulation (CCW). Single face | Floor/ceiling panels |
| `AppendRoundRectangleXY` | Rectangle with rounded corners | Architectural detail |
| `AppendDisc` | Planar disc with hole radius | Not relevant |
| `AppendLinearStairs` | Built-in stairs primitive | Already used elsewhere |

### Modeling Operations (MeshModelingFunctions.h)

| Function | Description | Thin-wall use |
|----------|-------------|---------------|
| `ApplyMeshShell` | **Create thickened shell** from mesh by offsetting along normals. Takes FGeometryScriptMeshOffsetOptions (OffsetDistance, bFixedBoundary, SolidsToShells) | Turn a flat panel into a thin wall with both faces |
| `ApplyMeshOffset` | Offset vertices along normals. For high-res meshes | Could thicken panels |
| `ApplyMeshLinearExtrudeFaces` | Extrude selected faces in a direction | Extrude wall faces for thickness |
| `ApplyMeshInsetOutsetFaces` | Inset/outset faces (Distance, bReproject) | Add baseboard/trim detail |
| `ApplyMeshBevel` | Bevel edges | Corner detail |

### UV Functions (MeshUVFunctions.h)

| Function | Description | Thin-wall use |
|----------|-------------|---------------|
| `SetMeshUVsFromBoxProjection` | Box-project UVs onto mesh. Takes BoxTransform, MinIslandTriCount | **Best for architectural geometry** -- projects UVs from 6 directions |
| `SetMeshUVsFromPlanarProjection` | Planar UV projection. PlaneTransform scale = world-to-UV mapping | Good for individual wall faces |
| `RecomputeMeshUVs` | Recompute based on UV islands, polygroups, or selection | General fallback |
| `ApplyTexelDensityUVScaling` | Normalize texel density across UV islands | Ensure consistent material tiling |
| `RepackMeshUVs` | Pack UV islands into 0-1 space | Post-processing step |

### Normal Functions (MeshNormalsFunctions.h)

| Function | Description |
|----------|-------------|
| `RecomputeNormals` | Full normal recomputation |
| `ComputeSplitNormals` | Split normals by angle threshold -- **already used in CleanupMesh** |
| `SetMeshTriangleNormals` | Per-triangle normal override |

---

## 3. Approach Comparison

### Approach A: Sweep Wall Profile Along Perimeter Path

**Concept:** Define a rectangular wall cross-section (e.g., 5cm wide x 300cm tall) as a 2D polygon, then sweep it along the room's floor perimeter using `AppendSimpleSweptPolygon`.

```
Wall cross-section (2D polygon):
    (0, 0) -> (T, 0) -> (T, H) -> (0, H)
    where T = wall_thickness, H = wall_height

Sweep path (3D): Room corners
    (-W/2, -D/2, 0) -> (W/2, -D/2, 0) -> (W/2, D/2, 0) -> (-W/2, D/2, 0)
    with bLoop = true
```

**Pros:**
- Single sweep call creates the entire room wall with proper thin geometry
- Automatic corner handling (MiterLimit controls corner sharpness)
- Natural UV flow along the sweep path
- No boolean operations needed for the base walls
- Corners are properly connected -- no seams, no overlap
- Already proven in pipe_network code

**Cons:**
- Corner geometry at 90-degree turns may bulge with high MiterLimit or collapse with low MiterLimit
- L-corridors and T-junctions need multiple sweep paths + boolean joins
- The 2D polygon orientation matters: X = "along path", Y = "right", Z = "up" in SweepPolygon's coordinate space. For AppendSimpleSweptPolygon, the polygon is in the XY plane perpendicular to the path direction
- Floor/ceiling still need separate geometry

**Implementation sketch:**
```cpp
// Wall cross-section: rectangle T wide, H tall
TArray<FVector2D> WallProfile;
WallProfile.Add(FVector2D(0, 0));         // bottom-left
WallProfile.Add(FVector2D(WallT, 0));     // bottom-right
WallProfile.Add(FVector2D(WallT, Height)); // top-right
WallProfile.Add(FVector2D(0, Height));     // top-left

// Room perimeter path (floor-level corners, CCW when viewed from above)
TArray<FVector> PerimeterPath;
float HW = Width * 0.5f, HD = Depth * 0.5f;
PerimeterPath.Add(FVector(-HW, -HD, FloorZ));
PerimeterPath.Add(FVector( HW, -HD, FloorZ));
PerimeterPath.Add(FVector( HW,  HD, FloorZ));
PerimeterPath.Add(FVector(-HW,  HD, FloorZ));

// Single sweep creates all 4 walls + corners
AppendSimpleSweptPolygon(Mesh, Opts, FTransform::Identity,
    WallProfile, PerimeterPath,
    /*bLoop=*/true, /*bCapped=*/false,
    1.0f, 1.0f, 0.0f, /*MiterLimit=*/1.5f);
```

**Corner behavior with MiterLimit:**
- MiterLimit = 1.0: No miter compensation. Profile may shrink at sharp corners
- MiterLimit = 1.5-2.0: Moderate compensation. Good for 90-degree corners
- MiterLimit > 3.0: Aggressive. May cause spikes at acute angles
- For 90-degree room corners, MiterLimit ~1.5 should produce clean mitered joints

**Estimated triangle count:** A 4-corner room with 4-vertex profile = ~32 triangles for walls (vs. 72 for 4 boxes). Much lighter.

### Approach B: Extrude + Shell (Hybrid)

**Concept:** Create a flat floor-plan polygon, extrude it to wall height with `AppendSimpleExtrudePolygon`, then use `ApplyMeshShell` to give it thickness.

```
1. Define room footprint as 2D polygon (rectangle)
2. AppendSimpleExtrudePolygon upward for wall height
3. ApplyMeshShell with OffsetDistance = -WallThickness (inward)
   This creates inner walls by duplicating + offsetting inward
```

**Pros:**
- Very clean conceptually
- Shell operation handles all normals automatically
- Works for any floor plan shape (not just rectangles)
- Creates both inner and outer wall faces properly

**Cons:**
- Creates a CLOSED shell (floor + ceiling included in the shell), which may not be desired
- Shell operation is computationally heavier than sweep
- Less control over individual wall segments
- Still need boolean ops for openings
- Shell offset on thin geometry can produce artifacts at sharp corners

**Implementation sketch:**
```cpp
// Room footprint
TArray<FVector2D> Footprint;
Footprint.Add(FVector2D(-W/2, -D/2));
Footprint.Add(FVector2D( W/2, -D/2));
Footprint.Add(FVector2D( W/2,  D/2));
Footprint.Add(FVector2D(-W/2,  D/2));

// Extrude walls (creates thin shell -- single surface, no thickness)
AppendSimpleExtrudePolygon(Mesh, Opts, FTransform::Identity,
    Footprint, Height, 0, false /* bCapped = no floor/ceiling */, Base);

// Add thickness
FGeometryScriptMeshOffsetOptions ShellOpts;
ShellOpts.OffsetDistance = WallT;  // positive = outward, negative = inward
ShellOpts.bFixedBoundary = false;
ApplyMeshShell(Mesh, ShellOpts);
```

**Problem:** `AppendSimpleExtrudePolygon` with `bCapped=false` creates an open tube (4 wall faces, no floor/ceiling). `ApplyMeshShell` will thicken these faces but the top/bottom edges are open boundaries -- shell behavior at open boundaries depends on `bFixedBoundary`. If true, boundary verts don't move; if false, they offset along normals which may not be what we want.

### Approach C: Extrude Outer - Extrude Inner (Boolean)

**Concept:** Same as `CreateBuildingShell` -- extrude outer footprint, extrude inner (inset) footprint, boolean subtract.

This is already implemented and working. The question is whether to bring this approach into `create_structure` for the simpler room/corridor types.

**Pros:**
- Already proven in BuildingShell code
- InsetPolygon2D helper already exists
- Handles arbitrary footprint shapes
- Clean corner geometry (no overlap issues)

**Cons:**
- Requires boolean operation (expensive)
- Boolean artifacts possible on thin geometry
- Creates solid walls (not hollow) between outer and inner surfaces
- Still cube-like in concept (just with proper corners)

### Approach D: Individual Thin Panels with AppendRectangleXY + Shell

**Concept:** Build each wall as a flat rectangle, then shell it to add thickness.

```
1. AppendRectangleXY for each wall face (zero thickness, single-sided)
2. ApplyMeshShell to thicken each one to WallT
3. Or: create TWO rectangles (inner + outer face) and stitch edges manually
```

**Pros:**
- Maximum control over each wall segment
- Easy UV mapping (each wall is a simple quad)
- Can add detail per-wall (baseboards, panels, etc.)

**Cons:**
- Corner joins are manual -- need explicit corner geometry
- More code, more complexity
- Shell on a flat quad may produce artifacts (normals are all coplanar)
- Need to handle double-sided normals manually

### Approach E: Per-Wall Thin Box (Current Approach, Just Thinner)

**Concept:** Keep the current AppendBox approach but use much thinner values (2-5cm instead of 20cm).

**Pros:**
- Minimal code change
- Still works

**Cons:**
- Doesn't solve the corner overlap problem
- Doesn't solve the UV tiling problem
- Walls are still solid cubes, just thinner ones
- Seams visible at wall junctions

---

## 4. Recommended Approach: Sweep Wall Profile (Approach A)

**Approach A (Sweep) is the clear winner** for these reasons:

1. **Already proven in codebase** -- `CreatePipeNetwork` uses the exact same `AppendSimpleSweptPolygon` pattern with a circle profile. We just swap to a rectangle profile.

2. **Zero boolean operations** for base walls -- massive performance improvement. Booleans are only needed for door/window openings.

3. **Perfect corner handling** -- the sweep naturally creates mitered corners at path joints. No overlap, no seams, no gaps.

4. **Minimal triangle count** -- a swept rectangle profile around 4 corners produces far fewer triangles than 4 separate boxes.

5. **Natural UV flow** -- sweep functions generate UVs that flow along the path, which is exactly how wall materials should tile (horizontally along the wall, vertically floor-to-ceiling).

6. **Floor/ceiling as flat polygons** -- use `AppendTriangulatedPolygon` or thin `AppendSimpleExtrudePolygon` (with small height like 2cm) for floors/ceilings. Much thinner than the current 15cm slabs.

### Detailed Implementation Plan

#### Phase 1: Wall Profile Helper

```cpp
// New helper: create rectangular wall cross-section
static TArray<FVector2D> MakeWallProfile(float Thickness, float Height)
{
    // Profile in the plane perpendicular to the sweep direction
    // For AppendSimpleSweptPolygon: the 2D polygon sits in a plane
    // perpendicular to the path, with Y=right, Z=up relative to path direction
    TArray<FVector2D> Profile;
    float HalfT = Thickness * 0.5f;
    Profile.Add(FVector2D(-HalfT, 0));       // bottom-left (outer)
    Profile.Add(FVector2D( HalfT, 0));       // bottom-right (inner)
    Profile.Add(FVector2D( HalfT, Height));  // top-right (inner)
    Profile.Add(FVector2D(-HalfT, Height));  // top-left (outer)
    return Profile;
}
```

**Important:** The orientation of the profile relative to the sweep path matters. In `AppendSimpleSweptPolygon`, the 2D polygon vertices are interpreted as (U, V) where:
- The sweep path defines the "forward" direction
- U maps to the "right" direction (perpendicular to path, in the horizontal plane)
- V maps to the "up" direction

For walls, we want the thickness to be along U (right/left of path) and the height to be along V (up).

#### Phase 2: Room Walls via Sweep

```cpp
// Replace BuildWalls lambda with:
auto BuildWallsSweep = [&](float W, float D, float H, float WT, FVector Offset)
{
    TArray<FVector2D> WallProfile = MakeWallProfile(WT, H);

    // Perimeter path (CCW when viewed from above, at floor level)
    // The path traces the CENTERLINE of the wall (not inner or outer edge)
    float HW = W * 0.5f;
    float HD = D * 0.5f;
    TArray<FVector> Path;
    Path.Add(Offset + FVector(-HW, -HD, 0));  // NW corner
    Path.Add(Offset + FVector( HW, -HD, 0));  // NE corner
    Path.Add(Offset + FVector( HW,  HD, 0));  // SE corner
    Path.Add(Offset + FVector(-HW,  HD, 0));  // SW corner

    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleSweptPolygon(
        Mesh, Opts, FTransform::Identity,
        WallProfile, Path,
        /*bLoop=*/true,     // close the loop (connect SW back to NW)
        /*bCapped=*/false,  // don't cap ends (loop has no ends)
        1.0f, 1.0f,         // uniform scale
        0.0f,                // no rotation
        1.5f);               // MiterLimit for 90-degree corners
};
```

#### Phase 3: Thin Floor/Ceiling

Replace thick floor/ceiling slabs with thin panels:

```cpp
// Option A: Very thin extrusion (2cm instead of 15cm)
float ThinFloorT = 2.0f;
AppendSimpleExtrudePolygon(Mesh, Opts, FloorXf, FloorPoly, ThinFloorT, 0, true, Base);

// Option B: Flat polygon + shell
// AppendTriangulatedPolygon for zero-thickness, then ApplyMeshShell
// (more complex, probably not worth it)
```

Recommendation: Keep `AppendSimpleExtrudePolygon` for floors/ceilings but with reduced thickness (2-5cm instead of 15cm). The extrusion creates proper two-sided geometry with edge faces.

#### Phase 4: L-Corridor and T-Junction

For complex shapes, sweep each segment separately and use boolean union:

```cpp
// L-corridor: two perpendicular swept segments
// Segment 1: horizontal arm
TArray<FVector> HPath = { ... }; // horizontal arm perimeter (U-shape, not loop)
AppendSimpleSweptPolygon(Mesh, Opts, Identity, WallProfile, HPath, false, true, ...);

// Segment 2: vertical arm
TArray<FVector> VPath = { ... };
AppendSimpleSweptPolygon(Mesh, Opts, Identity, WallProfile, VPath, false, true, ...);

// Boolean union to merge overlapping geometry at the junction
// OR: carefully plan paths so they share edges (no overlap needed)
```

**Better approach for L/T shapes:** Plan the sweep path as the full perimeter of the L or T shape. An L-corridor has 6 corners (not 4). A T-junction has 8 corners.

```
L-corridor perimeter (8 points):
    1---2
    |   |
    |   3---4
    |       |
    6---5---+  (wait, that's wrong)

Actually an L-shape interior perimeter:
    1-----------2
    |           |
    |   8---7   |
    |   |   |   |
    |   |   3---+
    |   |       |
    6---5   4---+  (this gets complex)
```

For L/T shapes, the simplest approach is:
1. Define the OUTER perimeter of the L/T shape as one closed polygon
2. Define the INNER perimeter (inset by WallT) as another closed polygon
3. Extrude both, boolean subtract inner from outer

This is basically what BuildingShell does, but with a non-rectangular footprint. The `InsetPolygon2D` helper already supports arbitrary polygons.

#### Phase 5: Door/Window Openings

Openings still require boolean subtraction. No way around it for arbitrary placements. But the boolean is now against a swept mesh instead of multiple boxes, which is cleaner.

One optimization: for standard door openings on a rectangular room, the sweep path could include the door opening as part of the profile:

```
Wall profile with door cutout:
    (0,0) -> (0, DoorH) -> (DoorW/2, DoorH) -> (DoorW/2, 0) -> continue wall
```

This is complex and only works for openings aligned with the sweep direction. Boolean subtraction remains the pragmatic choice.

#### Phase 6: UV Mapping

**Box projection is ideal for architectural geometry:**

```cpp
// After building all geometry, apply box UV projection
FGeometryScriptMeshSelection EmptySelection; // empty = entire mesh
FTransform UVBoxTransform = FTransform::Identity;
// Scale determines UV tiling: 100cm = 1 UV unit (1m tiling)
UVBoxTransform.SetScale3D(FVector(100.0f));

UGeometryScriptLibrary_MeshUVFunctions::SetMeshUVsFromBoxProjection(
    Mesh, 0, UVBoxTransform, EmptySelection, 2);
```

Box projection projects from 6 directions and picks the best one per triangle. This naturally gives:
- Vertical walls: horizontal U, vertical V (material tiles correctly)
- Floor/ceiling: top-down projection (material tiles correctly)
- Corner faces: projected from the most-aligned axis

The `MinIslandTriCount=2` prevents tiny triangles from getting their own UV island.

For tighter control, apply planar projection per-wall-segment using a selection to isolate each wall face.

---

## 5. Corner Handling Deep Dive

### Sweep MiterLimit Behavior at 90-Degree Corners

When `AppendSimpleSweptPolygon` encounters a sharp turn in the path:

- **MiterLimit = 1.0 (default):** No miter compensation. The profile is placed perpendicular to each path segment. At a 90-degree corner, the inside edge of the profile gets compressed while the outside edge stretches. For a thin wall (5cm), this creates a nearly-square corner with minimal distortion.

- **MiterLimit = 1.5:** Moderate compensation. The profile is scaled at corners to prevent the apparent cross-section from shrinking. For 90-degree turns with thin profiles, this produces clean mitered corners similar to how baseboard trim is cut.

- **MiterLimit = 2.0+:** More aggressive. Good for obtuse angles, may overshoot on acute angles.

For rectangular rooms (all 90-degree corners), **MiterLimit 1.0-1.5 is optimal**. The wall thickness is small relative to the room size, so corner distortion is minimal.

### Alternative: Explicit Corner Geometry

If sweep corner quality is insufficient, build walls as individual segments and add explicit corner pieces:

```
Per-wall segment: AppendBox(W, WallT, H) -- thin box for each wall
Corner piece: AppendBox(WallT, WallT, H) -- small square column at each corner
```

This is essentially the current approach with explicit corner handling. It works but creates more mesh objects and potential seams.

### Miter vs. Butt Joints

- **Miter joint** (sweep with MiterLimit > 1): Clean 45-degree cut at corners. Best visual quality. Automatic with sweep.
- **Butt joint** (current approach): One wall runs full length, the other butts against it. Creates a visible seam. Current E/W walls are shortened by `WT*2` for this.
- **Overlap joint**: Walls overlap at corners, rely on self-union to merge. Current approach requires post-cleanup.

**Sweep naturally produces miter joints** -- this is the architecturally correct approach.

---

## 6. Triangle Count Comparison

For a simple 400x600x300 room with 20cm walls:

| Approach | Walls | Floor | Ceiling | Total | Booleans |
|----------|-------|-------|---------|-------|----------|
| Current (4 boxes) | 48 tris | 12 | 12 | 72 | 0 (no opens) |
| Sweep profile | ~32 tris | 4 | 4 | ~40 | 0 |
| Extrude+Boolean | ~24 tris | 4 | 4 | ~32 | 1 (inner cut) |
| Shell from flat | ~24 tris | ~12 | ~12 | ~48 | 0 |

With 2 door openings:

| Approach | Base | Per-opening bool | Total bools | Rough total tris |
|----------|------|------------------|-------------|------------------|
| Current | 72 | +1 bool each | 2 | ~200-300 (post-bool) |
| Sweep | ~40 | +1 bool each | 2 | ~150-250 (post-bool) |
| Extrude+Bool | ~32 | +1 bool each | 3 (1 inner + 2 opens) | ~200-350 |

Sweep wins on base geometry count. Boolean operations dominate final tri count regardless of approach.

---

## 7. AppendSweepPolyline for Wall Panels (Alternative)

`AppendSweepPolyline` sweeps an **open** polyline (not a closed polygon) along a path of FTransforms. This could be used to create a single-sided wall panel:

```cpp
// Open polyline: vertical wall face (floor to ceiling)
TArray<FVector2D> WallLine;
WallLine.Add(FVector2D(0, 0));       // floor level
WallLine.Add(FVector2D(0, Height));  // ceiling level

// Sweep path: along wall length
TArray<FTransform> Path;
Path.Add(FTransform(FRotator::ZeroRotator, FVector(-W/2, -D/2, FloorZ)));
Path.Add(FTransform(FRotator::ZeroRotator, FVector( W/2, -D/2, FloorZ)));

// UV texture params
TArray<float> PolylineTexParamU = {0.0f, Height / 100.0f}; // V tiling
TArray<float> SweepPathTexParamV = {0.0f, Width / 100.0f};  // U tiling

AppendSweepPolyline(Mesh, Opts, Identity,
    WallLine, Path, PolylineTexParamU, SweepPathTexParamV,
    false, 1.0f, 1.0f, 0.0f, 1.0f);
```

This creates a **single-sided** wall. Would need two passes (inner + outer face) or use `ApplyMeshShell` to thicken. The UV texture params give explicit control over material tiling.

**Verdict:** More complex than sweep polygon, useful for single-sided panels only. Not recommended for primary wall construction.

---

## 8. Implementation Recommendations

### Priority Order

1. **Swap create_structure to use AppendSimpleSweptPolygon** for room and corridor types
   - Add `MakeWallProfile(Thickness, Height)` helper (similar to `MakeCirclePolygon`)
   - Room: 4-point loop sweep
   - Corridor: 4-point loop sweep (same as room, just different proportions)
   - Reduce floor/ceiling thickness to 2-5cm

2. **Keep Extrude+Boolean for L-corridor and T-junction**
   - Define full L/T perimeter as 2D polygon
   - Use `InsetPolygon2D` for inner perimeter
   - Extrude both + boolean subtract (same as BuildingShell)
   - Or: define separate sweep paths for each arm, boolean union at junction

3. **Add UV box projection pass** to all structure types
   - Call `SetMeshUVsFromBoxProjection` in `CleanupMesh` or after wall construction
   - Scale transform controls tiling density (100cm = 1 tile by default)

4. **Optionally add baseboard/crown molding** via additional sweep with a molding profile
   - Small L-shaped profile swept along the base of each wall
   - Future enhancement, not critical for initial improvement

### New Parameters to Add

```
wall_mode: "sweep" | "box" (default: "sweep", backward compat via "box")
floor_thickness: reduce default from 15 to 3
uv_scale: world-space units per UV tile (default: 100 = 1m tiling)
```

### Backward Compatibility

Keep the existing `BuildWalls` lambda as `BuildWallsBox` for backward compatibility. New `BuildWallsSweep` becomes the default. Parameter `wall_mode: "box"` can select the old behavior.

### Estimated Effort

- Phase 1 (MakeWallProfile helper): 30 minutes
- Phase 2 (Room/corridor sweep): 2-3 hours (including testing corner quality)
- Phase 3 (Thin floors/ceilings): 30 minutes
- Phase 4 (L/T junction rework): 3-4 hours
- Phase 5 (Opening booleans -- no change needed): 0 hours
- Phase 6 (UV box projection): 1 hour
- Testing all structure types: 2-3 hours

**Total: ~10-12 hours**

---

## 9. Key API Signatures (Verified from Engine Source)

### AppendSimpleSweptPolygon (THE key function)
```cpp
static UDynamicMesh* AppendSimpleSweptPolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,  // Wall cross-section
    const TArray<FVector>& SweepPath,          // Room perimeter
    bool bLoop = false,                        // true for closed rooms
    bool bCapped = true,                       // false for loop (no ends)
    float StartScale = 1.0f,
    float EndScale = 1.0f,
    float RotationAngleDeg = 0.0f,
    float MiterLimit = 1.0f,                   // 1.0-1.5 for 90-degree corners
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendSimpleExtrudePolygon (for floors/ceilings)
```cpp
static UDynamicMesh* AppendSimpleExtrudePolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,  // Floor plan polygon
    float Height = 100,                        // 2-5cm for thin floor
    int32 HeightSteps = 0,
    bool bCapped = true,                       // true = solid floor slab
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

### ApplyMeshShell (for thickening flat geometry)
```cpp
static UDynamicMesh* ApplyMeshShell(
    UDynamicMesh* TargetMesh,
    FGeometryScriptMeshOffsetOptions Options,  // OffsetDistance, bFixedBoundary
    UGeometryScriptDebug* Debug = nullptr);

struct FGeometryScriptMeshOffsetOptions {
    float OffsetDistance = 1.0;
    bool bFixedBoundary = false;
    int SolveIterations = 5;
    bool bSolidsToShells = true;
};
```

### SetMeshUVsFromBoxProjection (for wall UVs)
```cpp
static UDynamicMesh* SetMeshUVsFromBoxProjection(
    UDynamicMesh* TargetMesh,
    int UVSetIndex,
    FTransform BoxTransform,                   // Scale controls tiling
    FGeometryScriptMeshSelection Selection,    // Empty = full mesh
    int MinIslandTriCount = 2,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendRectangleXY (for flat panels)
```cpp
static UDynamicMesh* AppendRectangleXY(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float DimensionX = 100,
    float DimensionY = 100,
    int32 StepsWidth = 0,
    int32 StepsHeight = 0,
    UGeometryScriptDebug* Debug = nullptr);
```

---

## 10. Risk Assessment

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Sweep corner distortion at 90 degrees | Low | Test MiterLimit 1.0-2.0, fall back to box+corner-pieces |
| Profile orientation confusion | Medium | Test with simple 2-point path first, verify which axis is "up" |
| UV seams at sweep joins | Low | Box projection post-pass normalizes UVs |
| L/T junction complexity | Medium | Keep boolean approach for complex shapes, only sweep simple rooms |
| Boolean artifacts on thin walls | Medium | Increase cutter overshoot (already +4cm), ensure clean input mesh |
| Backward compatibility | Low | Add wall_mode param, default to new behavior |

---

## 11. Reference: How Archviz Tools Build Rooms

Professional archviz workflows (e.g., Blender Archimesh, 3ds Max AEC tools, SketchUp):

1. **Define floor plan as 2D polygon** (architect's floor plan)
2. **Offset polygon** inward by wall thickness (Clipper library in production tools)
3. **Extrude both polygons** to wall height
4. **Cap top/bottom** for floor and ceiling
5. **Boolean subtract openings** for doors and windows
6. **UV unwrap** with per-face planar projection

This maps exactly to our recommended approach: sweep for simple shapes (step 2+3 combined), extrude+boolean for complex shapes.

The key insight from archviz: **walls are defined by their centerline path + a cross-section profile, not by individual solid blocks.** Our sweep approach is the GeometryScript equivalent of this industry-standard workflow.
