# GeometryScript Fallback Piece Generation -- Filling Kit Gaps

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** modular-pieces-research, asset-scanning-research, modular-building-research, geometryscript-deep-dive-research
**Depends On:** asset-scanning-research (scan_modular_kit provides gap detection), modular-pieces-research (generation pipeline)

---

## Executive Summary

No modular kit is 100% complete. When `scan_modular_kit` classifies a user's meshes and detects missing piece types, the system generates fallback pieces with GeometryScript that match the kit's grid, dimensions, and (where possible) visual style. This research covers which pieces are commonly missing, how to extract dimension/style parameters from existing kit pieces, generation algorithms for each missing type, material strategies, caching, and a 3-tier quality system.

**Key findings:**
1. The most commonly missing pieces across marketplace kits are: T-junctions, interior corners, wall end caps, half-height walls, floor stairwell cutouts, ceiling variants, and smaller filler/transition pieces for irregular gaps
2. Dimension matching is straightforward: extract grid size (GCD of widths), wall thickness (min bounding dimension), and height from existing pieces
3. Style matching is feasible at a basic level: detect bevels via dihedral angle analysis, baseboards via bottom-face height segmentation, wall thickness via min-axis measurement
4. All 14 fallback piece types can be generated with existing GeometryScript APIs (AppendBox, AppendSimpleExtrudePolygon, AppendSweepPolygon, Selection+Inset+Delete)
5. Three quality tiers: blockout (box-only, <1ms), functional (box+openings, <5ms), style-matched (bevel/trim/detail, <20ms)
6. Fallbacks cache as UStaticMesh at `Saved/Monolith/ModularKits/{KitName}/Fallbacks/` and regenerate only when kit dimensions change
7. Total estimate: ~28-38h across 3 phases

---

## 1. Commonly Missing Pieces in Modular Kits

### 1.1 Industry Survey: What Kits Typically Ship

From analysis of marketplace kits (200-512cm grids), The Level Design Book's modular kit checklist, Polycount discussions, and Houdini building generators, a "standard" kit ships:

**Almost always present:**
- wall_solid (straight wall segment)
- wall_door (wall with door opening)
- wall_window (wall with window opening)
- floor_tile (basic floor)
- corner_outer (outside 90-degree corner)

**Usually present (70-80% of kits):**
- ceiling_tile
- stair_straight
- pillar/column
- doorframe, window frame (separate trim pieces)

**Often missing (40-60% of kits):**
- corner_inner (inside corner) -- less glamorous, frequently skipped
- wall_half (half-height wall/railing/parapet)
- wall_double_door (wider opening)
- stair_landing (intermediate platform)
- floor_stairwell (floor with cutout for stairs)

**Rarely present (<30% of kits):**
- wall_t_junction (where a wall meets another mid-run)
- wall_x_junction (four-way intersection)
- wall_end_cap (terminates a free-standing wall)
- ceiling_variants (with lighting cutouts, different heights)
- transition/filler pieces (half-width, quarter-width segments)
- baseboard trim (usually baked into wall meshes OR absent entirely)
- cornice trim (crown molding at ceiling-wall junction)

### 1.2 Sources and Evidence

**The Level Design Book** defines a minimal kit as: floor, ceiling, wall, doorway (single), doorway (double), window, corner -- plus "glue pieces" for filling gaps and interior connections. It emphasizes that floors must have thickness, not be paper-thin.

**Polycount discussions** consistently identify corners and T-junctions as the hardest pieces. One canonical thread notes: "Make a corner piece or several to cover different angles. Maybe pieces for intersections too: 3 and 4 way wall connections." Standard kits commonly lack "smaller filler pieces for irregular spaces."

**Houdini building generators** (SideFX tutorials, Jonathan Yu's procedural modular generator) handle gaps by either scaling modules (which stretches UVs) or by detecting gap measurements and selecting fallback modules. When no fallback exists, a gap remains.

**Shadows of Doubt** (gold standard for procedural interiors) uses 1.8m tiles, sidestepping the gap problem by constraining everything to tile boundaries. Missing variants are handled by the generation algorithm simply not requesting them.

### 1.3 Fallback Priority Matrix

| Priority | Piece Type | Why It's Critical | Generation Complexity |
|----------|-----------|-------------------|----------------------|
| P0 | wall_solid | Base building block | Trivial (AppendBox) |
| P0 | floor_tile | Required for every room | Trivial (AppendBox) |
| P0 | ceiling_tile | Required for every room | Trivial (AppendBox) |
| P0 | corner_outer | Every L-shaped room needs one | Low (L-extrusion) |
| P0 | corner_inner | Inside corners are structural | Low (L-extrusion) |
| P1 | wall_door | Buildings need doors | Medium (Selection+Delete) |
| P1 | wall_window | Buildings need windows | Medium (Selection+Inset+Delete) |
| P1 | wall_t_junction | Interior walls meet exterior | Low (T-extrusion) |
| P1 | wall_end_cap | Free-standing walls look broken without | Trivial (AppendBox) |
| P1 | wall_half | Railings, parapets, half-walls | Trivial (AppendBox, half height) |
| P2 | floor_stairwell | Multi-story needs stair cutouts | Medium (Box+Selection+Delete) |
| P2 | stair_straight | Vertical circulation | Medium (stepped extrusion) |
| P2 | stair_landing | Switchback stairs need landings | Trivial (AppendBox) |
| P2 | wall_x_junction | Four-way intersections | Low (column) |
| P3 | baseboard_trim | Visual polish | Medium (SweepPolygon) |
| P3 | cornice_trim | Visual polish | Medium (SweepPolygon) |
| P3 | window_frame | Visual polish | Medium (SweepPolygon loop) |
| P3 | door_frame | Visual polish | Medium (SweepPolygon U-shape) |

---

## 2. Dimension Matching -- Extracting Kit Parameters

### 2.1 Grid Size Detection

Already researched in `asset-scanning-research.md`. The algorithm:

```
1. For all wall-classified pieces, collect Width dimensions
2. Compute GCD of all widths -> grid_size
3. Common results: 100cm, 200cm, 300cm, 400cm, 512cm
4. Fallback: if GCD < 50cm or ambiguous, default to 200cm
```

Grid sizes found in the wild (from prior research):
- 100cm: indie/small kits
- 200cm: most common marketplace standard
- 300cm: stylized games
- 400cm: sci-fi, large-scale environments
- 512cm: UEFN/Fortnite standard

### 2.2 Wall Thickness Extraction

Wall thickness is the **minimum bounding box dimension** of wall pieces:

```cpp
FVector GetWallThickness(UStaticMesh* WallMesh)
{
    FBoxSphereBounds Bounds = WallMesh->GetBounds();
    FVector Size = Bounds.BoxExtent * 2.0; // Full dimensions

    // Sort dimensions ascending
    TArray<double> Dims = { Size.X, Size.Y, Size.Z };
    Dims.Sort();

    // Smallest dimension = thickness (walls are always thin relative to width/height)
    return Dims[0]; // typically 10-20cm
}
```

**Validation:** If min dimension > 30cm, the piece probably isn't a wall. If < 5cm, it's likely a trim/decal piece. Typical wall thickness range: 8-25cm.

**Consensus from multiple pieces:** Average wall thickness across all wall-classified pieces. If exterior and interior walls differ (15cm vs 10cm is common), track both.

### 2.3 Wall Height Extraction

Wall height is the **maximum bounding box Z dimension** of wall pieces:

```cpp
float GetWallHeight(UStaticMesh* WallMesh)
{
    FBoxSphereBounds Bounds = WallMesh->GetBounds();
    return Bounds.BoxExtent.Z * 2.0; // Full Z height
}
```

**Common heights:** 270cm (standard), 300cm (generous), 360cm (commercial/warehouse), 240cm (residential low-ceiling).

**Consensus:** Use median of all wall piece heights. Outliers (half-height walls, trim) are excluded by filtering pieces where Height < 0.7 * MedianHeight.

### 2.4 Opening Dimensions from Existing Pieces

If the kit HAS window or door pieces, extract opening dimensions:

```cpp
struct FOpeningDimensions
{
    float Width;
    float Height;
    float SillHeight; // 0 for doors, ~90cm for windows
};

FOpeningDimensions ExtractOpening(UStaticMesh* WallWithOpening, UStaticMesh* SolidWall)
{
    // Load both as DynamicMesh
    // The opening is where the wall-with-opening DIFFERS from a solid wall
    // Approach 1: Boundary edge detection
    //   - Solid wall has 0 open boundary edges
    //   - Wall with opening has a boundary loop around the opening
    //   - The AABB of that boundary loop = opening dimensions

    // Approach 2: Triangle density analysis
    //   - In the opening region, front-face triangles are missing
    //   - Ray-cast grid from front: holes = opening

    // Approach 3: Bounding box subtraction
    //   - If opening piece and solid piece have same outer bounds,
    //     the difference in inner volume indicates opening size
}
```

**Boundary edge approach (recommended):** Use `GetMeshBoundaryEdgeLoops()` from GeometryScript. A solid wall has no boundary loops (closed mesh). A wall with an opening has boundary loops around each opening. The AABB of a boundary loop gives the opening dimensions directly.

**Defaults when no existing opening pieces exist:**
- Door: 100cm wide x 230cm tall, sill at 0cm
- Window: 80cm wide x 120cm tall, sill at 90cm
- Double door: 140cm wide x 230cm tall, sill at 0cm

These defaults match the modular-pieces-research spec and IBC/ADA standards.

### 2.5 Floor Thickness Extraction

Floor tile thickness = Z-extent of floor-classified pieces. Typical range: 3-10cm.

---

## 3. Style Matching -- Analyzing Existing Pieces

### 3.1 What "Style" Means for Fallback Pieces

Style matching operates at three levels, corresponding to our quality tiers:

| Level | What We Match | How We Detect It | Tier |
|-------|--------------|-----------------|------|
| Dimensions | Grid, thickness, height | Bounding box analysis | Tier 1 (blockout) |
| Openings | Window/door size, position | Boundary edge loops | Tier 2 (functional) |
| Details | Bevels, baseboards, trim | Edge angle + face height analysis | Tier 3 (style-matched) |

### 3.2 Bevel Detection

Beveled edges are common in quality kits. To detect and measure them:

**Algorithm: Dihedral angle analysis**

Beveled edges have intermediate dihedral angles between adjacent faces. A sharp 90-degree box edge has exactly 90 degrees between face normals. A beveled edge replaces this with two faces at ~135 degrees each (for a 45-degree bevel) or a series of faces approximating a curve.

```cpp
struct FBevelParams
{
    bool bHasBevel = false;
    float BevelWidth = 0.0f;  // cm
    int32 BevelSegments = 1;  // 1 = chamfer, 2+ = rounded
};

FBevelParams DetectBevel(UDynamicMesh* KitPiece)
{
    // Step 1: Find edges at expected sharp-edge locations
    //         (wall top, wall sides, corner edges)
    //
    // Step 2: For each edge, compute dihedral angle between adjacent faces
    //         Sharp edge: angle < 100 degrees
    //         Beveled edge: 100 < angle < 170 degrees (per segment)
    //
    // Step 3: If beveled edges found, measure bevel width:
    //         - Select faces near the edge with SelectMeshElementsByNormalAngle
    //           (normals that are NOT axis-aligned = bevel faces)
    //         - Compute AABB of bevel faces perpendicular to edge
    //         - Width of that region = bevel width
    //
    // Step 4: Count bevel segments (faces between the two main surfaces)
    //         1 segment = simple chamfer
    //         2-4 segments = rounded bevel
    //         5+ segments = smooth curve

    // Implementation using lower-level FDynamicMesh3 API:
    const FDynamicMesh3& RawMesh = *KitPiece->GetMeshPtr();

    int32 BevelFaceCount = 0;
    float MaxBevelWidth = 0.0f;

    for (int32 EdgeID : RawMesh.EdgeIndicesItr())
    {
        if (RawMesh.IsBoundaryEdge(EdgeID)) continue;

        FIndex2i EdgeTris = RawMesh.GetEdgeT(EdgeID);
        FVector3d N0 = RawMesh.GetTriNormal(EdgeTris.A);
        FVector3d N1 = RawMesh.GetTriNormal(EdgeTris.B);
        double Angle = FMathd::Acos(N0.Dot(N1)) * 180.0 / PI;

        // Bevel faces: angle between 100-170 (not sharp, not coplanar)
        if (Angle > 100.0 && Angle < 170.0)
        {
            BevelFaceCount++;
            FVector3d V0 = RawMesh.GetVertex(RawMesh.GetEdgeV(EdgeID).A);
            FVector3d V1 = RawMesh.GetVertex(RawMesh.GetEdgeV(EdgeID).B);
            double EdgeLen = (V1 - V0).Length();
            MaxBevelWidth = FMath::Max(MaxBevelWidth, EdgeLen);
        }
    }

    FBevelParams Result;
    Result.bHasBevel = (BevelFaceCount > 4); // Minimum threshold
    Result.BevelWidth = MaxBevelWidth;
    // Segments estimated from face count relative to edge count
    return Result;
}
```

**Applying bevels to fallback pieces:**
```cpp
if (KitStyle.BevelParams.bHasBevel)
{
    // After generating the base box:
    // 1. Select all edges (or vertical edges specifically)
    FGeometryScriptMeshSelection EdgeSelection;
    // Select edges by angle: edges where adjacent face normals differ > 80 degrees

    // 2. Apply bevel
    FGeometryScriptMeshBevelOptions BevelOpts;
    BevelOpts.BevelDistance = KitStyle.BevelParams.BevelWidth;
    BevelOpts.Subdivisions = KitStyle.BevelParams.BevelSegments;

    UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshBevelSelection(
        Mesh, EdgeSelection, EGeometryScriptMeshBevelSelectionMode::SharedEdges,
        BevelOpts);
}
```

**Practical note from research:** The `ApplyMeshBevelEdgeSelection` function is available in UE 5.5+ (confirmed via docs). It takes a selection, bevel mode (SharedEdges or TriangleRegion), and bevel options (distance + subdivisions). This is the correct API for style-matching bevels.

### 3.3 Baseboard Detection

Detect whether kit pieces have built-in baseboards:

```cpp
struct FBaseboardParams
{
    bool bHasBaseboard = false;
    float BaseboardHeight = 0.0f; // cm, typically 5-12cm
    float BaseboardDepth = 0.0f;  // how far it protrudes, typically 0.5-2cm
};

FBaseboardParams DetectBaseboard(UDynamicMesh* WallPiece)
{
    // Strategy: Look for geometry that protrudes from the wall surface
    // at the bottom of the piece (Z < 15cm)

    // 1. Get the "main wall plane" normal (dominant front-face normal)
    // 2. Select faces in the bottom 15cm of the piece
    // 3. Among those, find faces whose normal differs from the main wall normal
    //    (angled faces = baseboard profile)
    // 4. The max Z of those angled faces = baseboard height
    // 5. The max protrusion beyond the main wall plane = baseboard depth

    // Alternative: Compare cross-section at Z=5cm vs Z=100cm
    // If the cross-section at Z=5cm is wider, there's a baseboard
}
```

**Applying baseboards to fallback pieces:**
If detected, generate baseboard trim as a separate sweep piece (see modular-pieces-research Section 1.6) with matching height and depth, using MaterialID = trim slot.

### 3.4 Material Slot Convention Detection

Analyze existing kit pieces to determine which MaterialID maps to what surface:

```cpp
struct FKitMaterialConvention
{
    int32 MainSurfaceSlot = 0;    // Primary wall/floor material
    int32 TrimSlot = -1;          // Baseboard, cornice (-1 = not used)
    int32 FrameSlot = -1;         // Door/window frames (-1 = not used)
    int32 EdgeSlot = -1;          // Wall cap/edge (-1 = not used)
    int32 TotalSlots = 1;
};

FKitMaterialConvention DetectMaterialConvention(const TArray<UStaticMesh*>& KitPieces)
{
    // Count unique material slots across all pieces
    // Pieces with openings tend to have more slots (frame material)
    // The most common slot count = the kit's convention

    // Heuristic:
    // 1 slot  = everything same material (blockout kit)
    // 2 slots = main surface + accent/trim
    // 3 slots = exterior + interior + edge (our default for walls)
    // 4 slots = exterior + interior + edge + frame (our default for opening pieces)

    // Match by name: scan MaterialSlotName for keywords
    // "trim", "base", "molding" -> trim slot
    // "frame", "door", "window" -> frame slot
    // "brick", "stucco", "siding", "exterior" -> exterior slot
    // "drywall", "plaster", "interior" -> interior slot
}
```

**Fallback material assignment:** When generating fallback pieces, use the kit's detected convention. If the kit has 2 material slots, the fallback gets 2. If slot names can be matched, use corresponding materials from existing kit pieces. Otherwise, assign a neutral blockout material (checkerboard with the grid size printed on it for debugging).

### 3.5 Thick vs Thin Wall Detection

Already handled by Section 2.2. The key decision:

- Thick walls (15-25cm): Common for exterior walls, older architectural styles, structural walls. Use `AppendBox` with matching thickness.
- Thin walls (8-12cm): Common for interior partitions. Generate at detected thickness.
- Paper-thin walls (<5cm): Likely a flat panel/decal system, not true modular. Log a warning.

### 3.6 Style Parameter Summary

All extracted parameters stored in a `FKitStyleProfile`:

```cpp
struct FKitStyleProfile
{
    // Dimensions
    float GridSize = 200.0f;          // cm
    float ExteriorWallThickness = 15.0f; // cm
    float InteriorWallThickness = 10.0f; // cm
    float WallHeight = 270.0f;        // cm
    float FloorThickness = 3.0f;      // cm

    // Openings (from existing pieces or defaults)
    FVector2D WindowSize = FVector2D(80, 120);  // cm
    float WindowSillHeight = 90.0f;   // cm
    FVector2D DoorSize = FVector2D(100, 230);   // cm

    // Style details
    FBevelParams Bevel;
    FBaseboardParams Baseboard;
    FKitMaterialConvention Materials;

    // Computed
    int32 StairsPerFloor;  // = WallHeight / 18cm, rounded
    float StairTreadDepth = 28.0f; // IBC standard
};
```

---

## 4. Fallback Generation Algorithms

### 4.1 wall_solid -- The Foundation

```cpp
UDynamicMesh* GenerateFallback_WallSolid(const FKitStyleProfile& Kit)
{
    UDynamicMesh* Mesh = AllocateMesh();

    AppendBox(Mesh, Kit.GridSize, Kit.ExteriorWallThickness, Kit.WallHeight,
              /*Origin=*/ Base);

    // MaterialIDs by normal
    AssignMaterialIDsByNormal(Mesh, Kit.Materials);

    // Optional bevel (Tier 3)
    if (Kit.Bevel.bHasBevel)
        ApplyMatchingBevel(Mesh, Kit.Bevel);

    // Box UV projection
    SetMeshUVsFromBoxProjection(Mesh);
    ComputeSplitNormals(Mesh);

    return Mesh;
}
```

**Complexity:** Trivial. ~0.1ms. All tiers.

### 4.2 wall_door -- Wall with Door Opening

```cpp
UDynamicMesh* GenerateFallback_WallDoor(const FKitStyleProfile& Kit)
{
    UDynamicMesh* Mesh = AllocateMesh();

    // Wider wall to accommodate door (2 grid cells or use GridSize if large enough)
    float Width = FMath::Max(Kit.GridSize, Kit.DoorSize.X + 40.0f); // 20cm margin each side
    AppendBox(Mesh, Width, Kit.ExteriorWallThickness, Kit.WallHeight, Base);

    // Tier 1 (blockout): Just a box, no opening
    if (Quality == Tier1) return FinalizeAndReturn(Mesh, Kit);

    // Tier 2+: Cut door opening via Selection+Delete
    FBox DoorBox(
        FVector(-Kit.DoorSize.X/2, -Kit.ExteriorWallThickness*2, 0),
        FVector( Kit.DoorSize.X/2,  Kit.ExteriorWallThickness*2, Kit.DoorSize.Y)
    );

    // Select front-facing triangles in door region
    FGeometryScriptMeshSelection BoxSel, NormalSel, FinalSel;
    SelectMeshElementsInBox(Mesh, BoxSel, DoorBox, Triangle);
    SelectMeshElementsByNormalAngle(Mesh, NormalSel, FVector(0,1,0), 30.0);
    CombineMeshSelections(BoxSel, NormalSel, FinalSel, Intersection);

    // Also select back-facing triangles (through-opening)
    FGeometryScriptMeshSelection BackNormalSel, BackFinalSel;
    SelectMeshElementsByNormalAngle(Mesh, BackNormalSel, FVector(0,-1,0), 30.0);
    CombineMeshSelections(BoxSel, BackNormalSel, BackFinalSel, Intersection);

    // Union front + back selections
    FGeometryScriptMeshSelection AllOpeningSel;
    CombineMeshSelections(FinalSel, BackFinalSel, AllOpeningSel, Union);

    // Tier 2: Delete selected faces (simple opening)
    DeleteSelectedTrianglesFromMesh(Mesh, AllOpeningSel);

    // Tier 3: Before deletion, inset first (creates frame geometry)
    // Use ApplyMeshInsetOutsetFaces with 5cm inset distance

    AssignMaterialIDsByNormal(Mesh, Kit.Materials);
    // Frame faces get Kit.Materials.FrameSlot

    return FinalizeAndReturn(Mesh, Kit);
}
```

**Key insight:** The opening MUST go through the full wall thickness (front AND back faces deleted). This is why we select both front-normal and back-normal triangles within the box region.

**Pre-tessellation alternative (simpler, more reliable):**
```cpp
// Create box with subdivisions aligned to door boundaries
// SubdivisionsX = 2 (left-margin | door-width | right-margin)
// SubdivisionsZ = 1 (door-height | above-door)
// Then delete the door-region quads
AppendBox(Mesh, Width, Thickness, Height, /*SubX=*/2, /*SubY=*/0, /*SubZ=*/1);
```

This approach is more reliable because the subdivision boundaries exactly match the opening edges, avoiding selection edge-case issues.

### 4.3 wall_window -- Wall with Window Opening

Same as wall_door but:
- Opening starts at `Kit.WindowSillHeight` (typically 90cm), not at Z=0
- Opening dimensions from `Kit.WindowSize`
- Tier 3 adds inset frame + optional window sill (AppendBox at bottom of opening, slight protrusion)

### 4.4 corner_outer -- Outside Corner (90 degrees)

```cpp
UDynamicMesh* GenerateFallback_CornerOuter(const FKitStyleProfile& Kit)
{
    float T = Kit.ExteriorWallThickness;
    float StubLen = T + Kit.GridSize * 0.25; // Quarter-cell stub

    // L-shaped cross-section
    TArray<FVector2D> LShape;
    LShape.Add(FVector2D(0, 0));           // inner corner
    LShape.Add(FVector2D(StubLen, 0));     // X-wall end
    LShape.Add(FVector2D(StubLen, T));     // X-wall outer face
    LShape.Add(FVector2D(T, T));           // corner junction
    LShape.Add(FVector2D(T, StubLen));     // Y-wall end
    LShape.Add(FVector2D(0, StubLen));     // Y-wall outer face

    AppendSimpleExtrudePolygon(Mesh, LShape, Kit.WallHeight, Base);

    return FinalizeAndReturn(Mesh, Kit);
}
```

**Why L-extrusion over two-box-union:** Clean topology, no boolean artifacts, single connected mesh. The modular-pieces-research confirms this is the recommended approach.

### 4.5 corner_inner -- Inside Corner (90 degrees)

```cpp
UDynamicMesh* GenerateFallback_CornerInner(const FKitStyleProfile& Kit)
{
    float T = Kit.ExteriorWallThickness;

    // Small fill piece at the inside of a corner junction
    // Just a T x T column extruded to wall height
    AppendBox(Mesh, T, T, Kit.WallHeight, Base);

    return FinalizeAndReturn(Mesh, Kit);
}
```

Alternatively, this can be the same L-shape as corner_outer but with opposite normals/MaterialIDs (interior-facing surfaces).

### 4.6 wall_t_junction -- T-Junction

```cpp
UDynamicMesh* GenerateFallback_WallTJunction(const FKitStyleProfile& Kit)
{
    float T = Kit.ExteriorWallThickness;
    float StubLen = T + Kit.GridSize * 0.25;

    // Approach A: T-shaped extrusion (recommended)
    TArray<FVector2D> TShape;
    TShape.Add(FVector2D(-StubLen, 0));     // left end of main wall
    TShape.Add(FVector2D(StubLen, 0));      // right end of main wall
    TShape.Add(FVector2D(StubLen, T));      // main wall outer face right
    TShape.Add(FVector2D(T/2, T));          // right side of perpendicular
    TShape.Add(FVector2D(T/2, T + StubLen)); // perpendicular end right
    TShape.Add(FVector2D(-T/2, T + StubLen)); // perpendicular end left
    TShape.Add(FVector2D(-T/2, T));         // left side of perpendicular
    TShape.Add(FVector2D(-StubLen, T));     // main wall outer face left

    AppendSimpleExtrudePolygon(Mesh, TShape, Kit.WallHeight, Base);

    // Approach B (simpler): Two overlapping wall_solid pieces + SelfUnion
    // Main wall (full length) + perpendicular wall (stub)
    // More code, but conceptually simpler

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.7 wall_x_junction -- Four-Way Cross

```cpp
UDynamicMesh* GenerateFallback_WallXJunction(const FKitStyleProfile& Kit)
{
    float T = Kit.ExteriorWallThickness;

    // +-shaped extrusion, OR just a T x T column (simpler)
    // The adjacent walls overlap at the junction, so the column fills the gap
    AppendBox(Mesh, T, T, Kit.WallHeight, Base);

    // Or for more coverage: + shape extrusion
    // with 4 stubs extending half a cell in each direction

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.8 wall_end_cap

```cpp
UDynamicMesh* GenerateFallback_WallEndCap(const FKitStyleProfile& Kit)
{
    float T = Kit.ExteriorWallThickness;

    // Thin slab that caps the exposed end of a wall
    // Dimensions: T x T x WallHeight (a column-like piece at the wall end)
    AppendBox(Mesh, T, T, Kit.WallHeight, Base);

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.9 wall_half -- Half-Height Wall / Railing

```cpp
UDynamicMesh* GenerateFallback_WallHalf(const FKitStyleProfile& Kit)
{
    float HalfHeight = Kit.WallHeight * 0.5f; // ~135cm, suitable for railing

    AppendBox(Mesh, Kit.GridSize, Kit.ExteriorWallThickness, HalfHeight, Base);

    // Top face gets a distinct MaterialID for railing cap

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.10 floor_tile and ceiling_tile

```cpp
UDynamicMesh* GenerateFallback_FloorTile(const FKitStyleProfile& Kit)
{
    AppendBox(Mesh, Kit.GridSize, Kit.GridSize, Kit.FloorThickness, Base);
    // MaterialID 0 = top (floor surface), MaterialID 1 = bottom
    return FinalizeAndReturn(Mesh, Kit);
}
```

Ceiling tile is identical but placed at WallHeight offset. Can be the same mesh, just transformed at placement time.

### 4.11 floor_stairwell -- Floor with Cutout

```cpp
UDynamicMesh* GenerateFallback_FloorStairwell(const FKitStyleProfile& Kit)
{
    // Floor tile with a rectangular hole for stairwell
    // Stairwell size: 2 x 4 cells (100cm x 200cm for 50cm grid) or proportional
    float StairwellW = Kit.GridSize; // 1 cell wide
    float StairwellD = Kit.GridSize * 2; // 2 cells deep (for straight stair)

    // Approach A: AppendBox floor + Boolean Subtract for cutout
    AppendBox(Mesh, Kit.GridSize * 2, Kit.GridSize * 4, Kit.FloorThickness, Base);
    UDynamicMesh* Cutter = AllocateMesh();
    AppendBox(Cutter, StairwellW, StairwellD, Kit.FloorThickness * 3, Center);
    ApplyMeshBoolean(Mesh, Cutter, Subtract);

    // Approach B: Pre-subdivided box + face deletion (cleaner)
    // Subdivide to align with stairwell boundaries, delete center quads

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.12 stair_straight

```cpp
UDynamicMesh* GenerateFallback_StairStraight(const FKitStyleProfile& Kit)
{
    int32 NumSteps = FMath::RoundToInt(Kit.WallHeight / 18.0f); // IBC: 17.8cm max riser
    float RiserH = Kit.WallHeight / NumSteps;
    float TreadD = 28.0f; // IBC: 27.9cm min tread

    // Staircase profile (zigzag cross-section)
    TArray<FVector2D> Profile;
    Profile.Add(FVector2D(0, 0));
    for (int32 i = 0; i < NumSteps; i++)
    {
        float Z = i * RiserH;
        float X = i * TreadD;
        Profile.Add(FVector2D(X, Z + RiserH));         // riser top
        Profile.Add(FVector2D(X + TreadD, Z + RiserH)); // tread end
    }
    Profile.Add(FVector2D(NumSteps * TreadD, 0)); // close bottom

    // Extrude to stair width
    AppendSimpleExtrudePolygon(Mesh, Profile, Kit.GridSize, Base);

    // MaterialIDs: 0=tread (top faces), 1=riser (front faces), 2=stringer (sides)

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.13 stair_landing

```cpp
UDynamicMesh* GenerateFallback_StairLanding(const FKitStyleProfile& Kit)
{
    // Simple floor slab at intermediate height
    float LandingSize = Kit.GridSize; // Square landing
    AppendBox(Mesh, LandingSize, LandingSize, Kit.FloorThickness, Base);

    return FinalizeAndReturn(Mesh, Kit);
}
```

### 4.14 Trim Pieces (Tier 3 only)

Baseboard, cornice, window frame, and door frame are generated via `AppendSweepPolygon` with profiles matched to detected kit style. Full algorithm documented in modular-pieces-research Section 1.6.

**Key adaptation for fallback:** If the kit has baseboards built into wall pieces (detected via Section 3.3), the fallback wall_solid must also include a baseboard. Two options:
1. Append a baseboard sweep piece at the bottom of the wall mesh (more geometry)
2. Thicken the bottom portion of the wall box (simpler, blockier)

---

## 5. Material Assignment Strategy

### 5.1 Three Strategies by Context

**Strategy A: Reuse kit materials (best)**
When fallback pieces will be placed alongside real kit pieces, reuse the kit's own materials. The `scan_modular_kit` action already knows which materials are on each piece. Pick the most common material from wall pieces for wall fallbacks, floor material for floor fallbacks, etc.

```cpp
// From kit scan results:
UMaterialInterface* KitWallMaterial = GetMostCommonMaterial(KitPieces, "wall");
UMaterialInterface* KitFloorMaterial = GetMostCommonMaterial(KitPieces, "floor");
// Assign to fallback mesh material slots
```

**Strategy B: Neutral blockout material (development)**
A checkerboard material with grid lines and dimensions printed on it. Clearly marks pieces as "fallback/placeholder" so artists know what to replace.

```cpp
// Use M_Monolith_Blockout with dynamic parameters:
// - GridSize printed as text
// - Checker scale matches grid
// - Distinct color per piece type (blue=wall, green=floor, orange=stairs)
```

**Strategy C: Match MaterialID convention (hybrid)**
Match the kit's MaterialID slot count and assign kit materials to matching slots. If kit has 3 slots (exterior/interior/edge), fallback uses 3 slots with the same materials. If slot matching fails, fall back to Strategy B.

### 5.2 Material Slot Mapping

| Fallback Piece | Slot 0 | Slot 1 | Slot 2 | Slot 3 |
|---------------|--------|--------|--------|--------|
| wall_solid | exterior | interior | edge | -- |
| wall_door | exterior | interior | edge | frame |
| wall_window | exterior | interior | edge | frame |
| floor_tile | floor_top | floor_bottom | -- | -- |
| ceiling_tile | ceiling_bottom | ceiling_top | -- | -- |
| corner_* | exterior | interior | edge | -- |
| stair_* | tread | riser | stringer | -- |
| trim_* | trim | -- | -- | -- |

---

## 6. Caching and Persistence

### 6.1 Cache Location

```
{ProjectDir}/Saved/Monolith/ModularKits/{KitName}/Fallbacks/
    SM_Fallback_{KitName}_wall_solid.uasset
    SM_Fallback_{KitName}_wall_door.uasset
    SM_Fallback_{KitName}_wall_window.uasset
    SM_Fallback_{KitName}_corner_outer.uasset
    ...
    fallback_manifest.json
```

**Alternative (in-project for shipping):**
```
/Game/Generated/ModularKits/{KitName}/Fallbacks/
    SM_Fallback_{KitName}_wall_solid
    ...
```

The Saved/ path is for development-only (non-packaged). The /Game/ path is for pieces that should ship with the build. User chooses via `save_to_project: true/false` parameter.

### 6.2 Manifest Format

```json
{
    "kit_name": "UrbanExterior",
    "kit_hash": "a1b2c3d4",
    "generated_at": "2026-03-28T14:30:00Z",
    "quality_tier": "functional",
    "style_profile": {
        "grid_size": 200.0,
        "wall_thickness": 15.0,
        "wall_height": 300.0,
        "floor_thickness": 5.0,
        "has_bevel": true,
        "bevel_width": 1.5,
        "bevel_segments": 1,
        "has_baseboard": false,
        "window_size": [80, 120],
        "window_sill": 90,
        "door_size": [100, 230],
        "material_slots": 3
    },
    "pieces": {
        "wall_solid": {
            "asset_path": "/Game/Generated/ModularKits/UrbanExterior/Fallbacks/SM_Fallback_UrbanExterior_wall_solid",
            "dimensions": [200, 15, 300],
            "tri_count": 12,
            "material_ids": [0, 1, 2],
            "generated_ms": 0.3
        },
        "wall_door": { ... },
        "corner_outer": { ... }
    },
    "missing_from_kit": ["wall_t_junction", "corner_inner", "wall_end_cap", "floor_stairwell"],
    "total_pieces": 8,
    "total_generation_ms": 45.2
}
```

### 6.3 Staleness Detection

A fallback set is "stale" and must be regenerated when:

1. **Kit dimensions changed:** Recompute `kit_hash` from style_profile params. If hash differs, regenerate.
2. **Quality tier changed:** User requested higher quality than cached fallbacks.
3. **Kit pieces added:** If the kit now has a piece type that was previously missing, the fallback for that type is no longer needed (mark as `"superseded": true` in manifest).
4. **Kit pieces removed:** If a kit piece is deleted, check if its type now needs a fallback.

`kit_hash` = MD5 of `"{grid_size}|{wall_thickness}|{wall_height}|{floor_thickness}|{quality_tier}"` -- same approach as proc-mesh-caching-research.

### 6.4 Regeneration Trigger

Fallbacks regenerate when:
- `scan_modular_kit` is called and detects new gaps
- User calls `generate_fallback_pieces` explicitly
- Quality tier changes
- Kit manifest's `kit_hash` doesn't match current kit analysis

**NOT at runtime.** All fallback generation is editor-time only. Generated pieces are saved as UStaticMesh and loaded like any other asset.

---

## 7. Quality Tiers

### 7.1 Tier 1: Blockout (Fast)

**What you get:** Plain boxes. No openings. No bevels. No trim.
**When to use:** Rapid iteration, testing layouts, prototyping.
**Performance:** <1ms per piece, <10ms for full set of 14 pieces.
**Visual quality:** Developer-only. Clearly identifiable as placeholder.

```
wall_solid     = AppendBox
wall_door      = AppendBox (no opening -- it's just a solid wall)
wall_window    = AppendBox (no opening)
corner_outer   = L-shaped extrusion
corner_inner   = AppendBox (T x T column)
floor_tile     = AppendBox (flat)
ceiling_tile   = AppendBox (flat)
wall_half      = AppendBox (half height)
stair_straight = AppendBox (ramp approximation)
```

### 7.2 Tier 2: Functional (Medium)

**What you get:** Correct openings for doors/windows. Proper stair steps. T-junction geometry. Correct corner shapes.
**When to use:** Gameplay testing, pre-production, AI navigation validation.
**Performance:** <5ms per piece, <50ms for full set.
**Visual quality:** Playable but obviously procedural.

```
wall_solid     = AppendBox + MaterialIDs by normal
wall_door      = AppendBox + subdivisions + face deletion (through-opening)
wall_window    = AppendBox + subdivisions + face deletion + sill box
corner_outer   = L-shaped extrusion with MaterialIDs
corner_inner   = L-shaped fill with MaterialIDs
wall_t_junction = T-shaped extrusion
floor_stairwell = AppendBox + boolean subtract for cutout
stair_straight = Stepped zigzag extrusion (IBC dimensions)
stair_landing  = AppendBox slab
wall_end_cap   = AppendBox column
wall_half      = AppendBox half-height + top MaterialID
```

### 7.3 Tier 3: Style-Matched (Slow)

**What you get:** Beveled edges matching kit style. Inset window/door frames. Trim pieces (baseboard, cornice). Material convention matching. Near-art-quality fallbacks.
**When to use:** Final look-dev before custom art, shipping with procedural buildings.
**Performance:** <20ms per piece, <200ms for full set.
**Visual quality:** Blends with kit when viewed from gameplay distance. Close inspection reveals procedural origin.

```
Everything from Tier 2, plus:
- ApplyMeshBevelSelection on all sharp edges (matching kit bevel width/segments)
- ApplyMeshInsetOutsetFaces for window/door frames (creates recessed frame geometry)
- AppendSweepPolygon for baseboard trim (matching kit baseboard height)
- AppendSweepPolygon for cornice trim (matching kit cornice profile)
- AppendSweepPolygon for window/door frames (U-shape or full loop)
- Full MaterialID convention matching with kit materials
```

### 7.4 Quality Tier Selection

Via MCP action parameter:

```json
{
    "action": "scan_modular_kit",
    "params": {
        "folder": "/Game/MyModularKit",
        "fallback_quality": "functional",  // "blockout" | "functional" | "style_matched"
        "generate_fallbacks": true,
        "save_to_project": false
    }
}
```

Or as a separate action:

```json
{
    "action": "generate_fallback_pieces",
    "params": {
        "kit_name": "UrbanExterior",
        "quality": "style_matched",
        "piece_types": ["wall_t_junction", "corner_inner", "floor_stairwell"],
        "save_to_project": true
    }
}
```

---

## 8. MCP Actions

### 8.1 New Action: generate_fallback_pieces

```json
{
    "action": "generate_fallback_pieces",
    "params": {
        "kit_name": "string (required) -- name from scan_modular_kit",
        "quality": "string (optional) -- blockout|functional|style_matched, default: functional",
        "piece_types": "array<string> (optional) -- specific types to generate, default: all missing",
        "save_to_project": "bool (optional) -- save under /Game/ vs Saved/, default: false",
        "force_regenerate": "bool (optional) -- ignore cache, default: false",
        "material_strategy": "string (optional) -- reuse_kit|blockout|match_convention, default: reuse_kit"
    },
    "returns": {
        "pieces_generated": 8,
        "pieces": [
            {
                "type": "wall_t_junction",
                "asset_path": "/Game/Generated/ModularKits/UrbanExterior/Fallbacks/SM_Fallback_UrbanExterior_wall_t_junction",
                "dimensions": [200, 15, 300],
                "tri_count": 36,
                "generation_ms": 2.1
            }
        ],
        "total_ms": 45.2,
        "cache_path": "Saved/Monolith/ModularKits/UrbanExterior/Fallbacks/fallback_manifest.json"
    }
}
```

### 8.2 Modified Action: scan_modular_kit (extension)

Add to existing `scan_modular_kit` response:

```json
{
    "missing_piece_types": ["wall_t_junction", "corner_inner", "wall_end_cap", "floor_stairwell"],
    "style_profile": {
        "grid_size": 200,
        "wall_thickness": 15,
        "wall_height": 300,
        "has_bevel": true,
        "bevel_width": 1.5,
        "material_slots": 3
    },
    "fallback_recommendation": {
        "suggested_quality": "functional",
        "estimated_generation_ms": 45,
        "piece_count": 4
    }
}
```

### 8.3 New Action: get_fallback_status

```json
{
    "action": "get_fallback_status",
    "params": {
        "kit_name": "string (required)"
    },
    "returns": {
        "has_fallbacks": true,
        "quality_tier": "functional",
        "piece_count": 8,
        "is_stale": false,
        "manifest_path": "...",
        "pieces": { ... }
    }
}
```

---

## 9. Integration with Building Assembly

### 9.1 Piece Resolution Order

When `build_with_kit` needs a specific piece type:

```
1. Check kit vocabulary (user's scanned pieces)
   -> If found, use the kit's piece
2. Check fallback cache
   -> If found and not stale, use the fallback piece
3. Generate fallback on-the-fly
   -> Save to cache for future use
4. Log warning if generation fails
   -> Use absolute fallback: untextured box at correct dimensions
```

### 9.2 Proxy/Swap Workflow

From the asset-scanning-research, the proxy workflow:

```
1. scan_modular_kit -> identifies pieces + gaps
2. generate_fallback_pieces -> fills gaps with procedural pieces
3. build_with_kit -> assembles building using kit + fallbacks
4. Artist replaces fallback pieces with custom art
5. swap_proxies -> batch-replace all instances of a fallback with the new art piece
```

The fallback naming convention (`SM_Fallback_{KitName}_{PieceType}`) makes batch replacement trivial. The `swap_proxies` action matches by name prefix.

---

## 10. Edge Cases and Failure Modes

### 10.1 Non-Grid Kits

Some kits don't use a consistent grid (organic/natural environments). Detection: if GCD of piece widths produces a grid size < 30cm or widths have no common factor, the kit is "non-grid."

**Handling:** Skip fallback generation. Log warning: "Kit does not appear to use a consistent grid. Fallback generation requires grid-based kits."

### 10.2 Extreme Dimensions

Kits with unusual dimensions (e.g., 512cm grid with 3cm walls):
- Validate that wall thickness > 5cm and < 50cm
- Validate that wall height > 100cm and < 1000cm
- Validate that grid size > 50cm and < 1000cm
- If out of range, clamp and warn

### 10.3 Material Slot Mismatch

If kit pieces have inconsistent material slot counts (some have 2, others have 5):
- Use the **mode** (most common) slot count as the convention
- For outlier pieces, map extra slots to the closest convention slot

### 10.4 Overlapping Piece Types

If the scanner classifies a piece as both "wall_solid" and "wall_door" (ambiguous classification), prefer the more specific type. If the kit has BOTH a solid wall and a wall-with-door, no fallback is needed for either.

### 10.5 One-Cell vs Multi-Cell Pieces

Some kits use 1-cell walls, others use 2-cell or 3-cell. Fallback generation should match:
- If kit walls span 1 grid cell: fallback walls = 1 cell
- If kit walls span 2 cells: fallback walls = 2 cells
- Mixed: generate both sizes as separate fallback pieces

---

## 11. Performance Budget

| Operation | Time Budget | Notes |
|-----------|------------|-------|
| Style analysis (per kit) | <100ms | One-time on scan |
| Tier 1 piece generation | <1ms each | AppendBox only |
| Tier 2 piece generation | <5ms each | Box + selections + deletions |
| Tier 3 piece generation | <20ms each | Full detail + bevel + trim |
| Full fallback set (14 pieces, Tier 2) | <70ms | One-time, cached |
| Full fallback set (14 pieces, Tier 3) | <280ms | One-time, cached |
| Save per piece (UStaticMesh) | <50ms | Disk I/O |
| Cache check (manifest load) | <5ms | JSON parse |

All within interactive editor-time budgets. No perceivable hitches.

---

## 12. Implementation Plan

### Phase 1: Core Fallback Generation (~12-16h)
- Implement `FKitStyleProfile` extraction from scanned pieces (dimensions, thickness, height)
- Implement Tier 1 and Tier 2 generators for all 14 piece types
- Implement `generate_fallback_pieces` MCP action
- Implement cache manifest (JSON read/write)
- Implement `SaveMeshToAsset` integration (reuse existing)
- Wire into `scan_modular_kit` response (missing_piece_types, style_profile)

### Phase 2: Style Matching (~10-14h)
- Implement bevel detection (dihedral angle analysis on FDynamicMesh3)
- Implement baseboard detection (bottom-face height analysis)
- Implement material convention detection (slot count + name heuristics)
- Implement Tier 3 generators (bevel application, trim sweeps, inset frames)
- Implement kit material reuse strategy
- Implement `get_fallback_status` MCP action

### Phase 3: Integration and Polish (~6-8h)
- Wire fallback resolution into `build_with_kit` (piece resolution order)
- Implement proxy/swap workflow for fallback -> custom art replacement
- Implement staleness detection and auto-regeneration
- Edge case handling (non-grid kits, extreme dimensions, slot mismatch)
- Testing with 3+ marketplace kits at different grid sizes
- Documentation update

**Total: ~28-38h**

---

## 13. Sources

### Level Design and Modular Kit Theory
- [Modular kit design -- The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics/modular) -- Core kit checklist: floor, ceiling, wall, doorway (single/double), window, corner, glue pieces
- [Joel Burgess: Skyrim's Modular Level Design -- GDC 2013](http://blog.joelburgess.com/2013/04/skyrims-modular-level-design-gdc-2013.html) -- Kit design principles, snap points, piece taxonomy
- [Tips on Modular Level Design in UE4 -- 80.lv](https://80.lv/articles/tips-on-modular-level-design-in-ue4) -- Sectioning into Ceilings, Floors, Walls, Props, Decals folders
- [Creating Modular Game Art For Fast Level Design -- Gamedeveloper.com](https://www.gamedeveloper.com/production/creating-modular-game-art-for-fast-level-design) -- Kit planning and variant management

### Polycount Community Discussions
- [Creating modular walls with thickness -- Polycount](https://polycount.com/discussion/232991/creating-modular-walls-with-thickness-how-to-handle-corners-and-alignment) -- Corner handling, T-junctions, thick wall strategies, "expect quite a lot of parts"
- [Modular walls, how to do it -- Polycount](https://polycount.com/discussion/218188/modular-walls-how-to-do-it) -- Filler pieces for irregular gaps, texture stretching vs world-aligned UVs
- [Modular Environment Techniques -- Polycount](https://polycount.com/discussion/209426/modular-environment-techniques) -- Floor X junction, ceiling X junction modules
- [Modular house parts -- Polycount](https://polycount.com/discussion/212785/modular-house-parts) -- Residential kit piece taxonomy

### Procedural Building Generation
- [bendemott/UE5-Procedural-Building -- GitHub](https://github.com/bendemott/UE5-Procedural-Building) -- GeometryScript C++ procedural buildings, proof of concept, welded unified mesh approach
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/) -- 1.8m tile grid, floorplan-based generation, no "missing piece" problem because tile system
- [SideFX Building Generator Tutorial](https://www.sidefx.com/tutorials/building-generator/) -- Houdini module gap handling, scaling vs fallback selection
- [SideFX Modular House Generator](https://www.sidefx.com/tutorials/house-generation-with-modular-models/) -- Panel packing logic, gap measurement vs available module widths
- [Procedural Generation of 3D-Buildings Based on Existing Asset Kits -- Hawaii ScholarSpace](https://scholarspace.manoa.hawaii.edu/items/6cfe2efe-651d-4b06-88df-199a9e9c710a) -- Connector-based technique with composition rules

### Geometry Processing and Style Analysis
- [Guided Proceduralization: Grammar Extraction for Architectural Models -- ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S0097849318300785) -- Extracting procedural representations from existing 3D models
- [Mesh Grammars: Procedural Articulation of Form -- CUMINCAD](https://papers.cumincad.org/data/works/att/caadria2013_259.content.pdf) -- Face-level mesh attributes for style classification
- [Mesh Analysis -- Blender Manual](https://docs.blender.org/manual/en/latest/modeling/meshes/mesh_analysis.html) -- Wall thickness via ray-casting distance analysis
- [Edge-Sharpener: Recovering Sharp Features -- Georgia Tech](https://faculty.cc.gatech.edu/~jarek/papers/EdgeSharpener.pdf) -- Chamfer edge detection via subdividing vertices at plane intersections

### UE5 GeometryScript
- [Geometry Scripting Reference -- UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-reference-in-unreal-engine) -- Full API reference
- [SelectMeshElementsByNormalAngle -- UE 5.6 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/MeshSelection/SelectMeshElementsbyNormalAngle) -- Normal-angle selection for face classification
- [ApplyMeshBevelEdgeSelection -- UE 5.5 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Modeling/ApplyMeshBevelEdgeSelection) -- Edge bevel with distance + subdivisions params
- [Basic Bevel Using Geometry Script -- UE Community Snippet](https://dev.epicgames.com/community/snippets/J5lz/unreal-engine-basic-bevel-using-geometry-script) -- Practical bevel example
- [Geometry Script FAQ -- gradientspace](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq) -- DynamicMesh patterns, performance tips

### UE5 Proxy and Gap Filling
- [Filling Gaps Using the Proxy Geometry Tool -- UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/filling-gaps-using-the-proxy-geometry-tool-in-unreal-engine) -- Built-in UE gap filling (different from our approach but relevant)
- [Blender Geometry Nodes to UE5 -- James Roha, Medium](https://medium.com/@Jamesroha/blender-geometry-nodes-to-unreal-engine-5-the-procedural-environment-art-guide-05cf8d8b4701) -- External procedural asset pipeline, seed-based variant baking

### Marketplace Kit References
- [City Building Modular Kit -- Fab/UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/city-building-modular-kit) -- Typical exterior kit piece inventory
- [Modular Building Set -- Fab](https://www.fab.com/listings/474a0598-ed86-40b6-baa1-c801d96ef4ab) -- Standard modular building piece types
- [Junction City Modular Building Kit -- UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/junction-city) -- Named "junction" but likely still missing T-junction wall pieces

### Related Monolith Research (Internal)
- `modular-pieces-research.md` -- Full piece generation pipeline, 20 piece types, Selection+Inset algorithms, MaterialID strategy, SaveMeshToAsset/ConvertToHism infrastructure
- `asset-scanning-research.md` -- 5-signal classification, grid size detection, scan_modular_kit/build_with_kit/swap_proxies actions
- `modular-building-research.md` -- Industry modular systems comparison, recommendation to switch to modular as primary
- `geometryscript-deep-dive-research.md` -- Full API audit, Selection+Inset 20x faster than Boolean, MeshModelingFunctions gap
- `proc-mesh-caching-research.md` -- Param hashing, JSON manifests, cache staleness detection
