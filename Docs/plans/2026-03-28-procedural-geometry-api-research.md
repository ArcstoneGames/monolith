# Procedural Geometry API Research -- GeometryScript UE 5.7

**Date:** 2026-03-28 | **Status:** Research Complete
**Source:** Engine headers at `C:\Program Files (x86)\UE_5.7\Engine\Plugins\Runtime\GeometryScripting\Source\GeometryScriptingCore\Public\GeometryScript\`

All functions verified present in UE 5.7 headers. All are `BlueprintCallable` static methods on `UBlueprintFunctionLibrary` subclasses. All work at editor-time (no runtime-only restrictions). The `ScriptMethod` meta means they can be called as methods on `UDynamicMesh*`.

---

## Table of Contents

1. [Core Primitives API](#1-core-primitives-api)
2. [Boolean Operations API](#2-boolean-operations-api)
3. [Sweep & Extrusion API](#3-sweep--extrusion-api)
4. [Deformation API](#4-deformation-api)
5. [Decomposition API](#5-decomposition-api)
6. [Mesh Editing API](#6-mesh-editing-api)
7. [Transform API](#7-transform-api)
8. [Normals API](#8-normals-api)
9. [Feature-by-Feature Analysis](#9-feature-by-feature-analysis)
10. [Performance Notes](#10-performance-notes)
11. [Missing APIs & Workarounds](#11-missing-apis--workarounds)

---

## 1. Core Primitives API

**Header:** `MeshPrimitiveFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshPrimitiveFunctions`

### AppendBox

```cpp
static UDynamicMesh* AppendBox(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,                    // Position/rotate/scale the box
    float DimensionX = 100,
    float DimensionY = 100,
    float DimensionZ = 100,
    int32 StepsX = 0,                        // Subdivisions (0 = no extra)
    int32 StepsY = 0,
    int32 StepsZ = 0,
    EGeometryScriptPrimitiveOriginMode Origin = Base,  // Center or Base
    UGeometryScriptDebug* Debug = nullptr);
```

**Key insight:** The `Transform` parameter positions the primitive directly. No need to append-then-transform -- just pass the desired location/rotation/scale in the Transform. `Origin = Base` means the box sits on top of the transform origin.

Also available: `AppendBoxWithCollision`, `AppendBoundingBox`, `AppendOrientedBox`.

### AppendSphereLatLong

```cpp
static UDynamicMesh* AppendSphereLatLong(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float Radius = 50,
    int32 StepsPhi = 10,
    int32 StepsTheta = 16,
    EGeometryScriptPrimitiveOriginMode Origin = Center,
    UGeometryScriptDebug* Debug = nullptr);
```

Also: `AppendSphereBox` (box-topology sphere, better for subdivision/deformation).

### AppendCylinder

```cpp
static UDynamicMesh* AppendCylinder(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float Radius = 50,
    float Height = 100,
    int32 RadialSteps = 12,
    int32 HeightSteps = 0,
    bool bCapped = true,
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendCone

```cpp
static UDynamicMesh* AppendCone(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float BaseRadius = 50,
    float TopRadius = 5,           // 0 = true cone, >0 = truncated
    float Height = 100,
    int32 RadialSteps = 12,
    int32 HeightSteps = 4,
    bool bCapped = true,
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendCapsule

```cpp
static UDynamicMesh* AppendCapsule(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float Radius = 30,
    float LineLength = 75,
    int32 HemisphereSteps = 5,
    int32 CircleSteps = 8,
    int32 SegmentSteps = 0,         // New in 5.5+
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendTorus

```cpp
static UDynamicMesh* AppendTorus(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    FGeometryScriptRevolveOptions RevolveOptions,  // RevolveDegrees, DegreeOffset, etc.
    float MajorRadius = 50,
    float MinorRadius = 25,
    int32 MajorSteps = 16,
    int32 MinorSteps = 8,
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendLinearStairs (!)

```cpp
static UDynamicMesh* AppendLinearStairs(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float StepWidth = 100.0f,
    float StepHeight = 20.0f,
    float StepDepth = 30.0f,
    int NumSteps = 8,
    bool bFloating = false,          // true = open risers
    UGeometryScriptDebug* Debug = nullptr);
```

**This is a native staircase primitive!** No need to build stairs from boxes. The wishlist's `stairs` type in `create_parametric_mesh` maps directly to this.

### AppendCurvedStairs

```cpp
static UDynamicMesh* AppendCurvedStairs(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float StepWidth = 100.0f,
    float StepHeight = 20.0f,
    float InnerRadius = 150.0f,
    float CurveAngle = 90.0f,
    int NumSteps = 8,
    bool bFloating = false,
    UGeometryScriptDebug* Debug = nullptr);
```

**Bonus:** Curved stairs are natively supported. Perfect for stairwell structures.

### AppendRectangleXY

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

Useful for floor slabs, walls as thin rectangles, decal base meshes.

### AppendDisc

```cpp
static UDynamicMesh* AppendDisc(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float Radius = 50,
    int32 AngleSteps = 16,
    int32 SpokeSteps = 0,
    float StartAngle = 0,
    float EndAngle = 360,
    float HoleRadius = 0,           // >0 creates annulus/ring
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendTriangulatedPolygon

```cpp
static UDynamicMesh* AppendTriangulatedPolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,   // CCW winding, not repeated endpoint
    bool bAllowSelfIntersections = true,
    UGeometryScriptDebug* Debug = nullptr);
```

Useful for decal meshes -- generate irregular 2D polygon, triangulate, done.

### AppendVoronoiDiagram2D

```cpp
static UDynamicMesh* AppendVoronoiDiagram2D(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& VoronoiSites,
    FGeometryScriptVoronoiOptions VoronoiOptions,   // BoundsExpand, Bounds, CreateCells, bIncludeBoundary
    UGeometryScriptDebug* Debug = nullptr);
```

**Important for fracture:** This generates 2D Voronoi cells as a flat mesh with polygroups per cell. For 3D fracture, combine with plane cuts or use the polygroup-based approach.

### FGeometryScriptPrimitiveOptions

```cpp
struct FGeometryScriptPrimitiveOptions {
    EGeometryScriptPrimitivePolygroupMode PolygroupMode = PerFace;  // SingleGroup, PerFace, PerQuad
    bool bFlipOrientation = false;
    EGeometryScriptPrimitiveUVMode UVMode = Uniform;                // Uniform, ScaleToFill
    int32 MaterialID = 0;
};
```

---

## 2. Boolean Operations API

**Header:** `MeshBooleanFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshBooleanFunctions`

### ApplyMeshBoolean

```cpp
static UDynamicMesh* ApplyMeshBoolean(
    UDynamicMesh* TargetMesh,         // Modified in-place, receives result
    FTransform TargetTransform,        // World position of target
    UDynamicMesh* ToolMesh,            // The "cutter" mesh
    FTransform ToolTransform,          // World position of tool
    EGeometryScriptBooleanOperation Operation,
    FGeometryScriptMeshBooleanOptions Options,
    UGeometryScriptDebug* Debug = nullptr);
```

### EGeometryScriptBooleanOperation

```cpp
enum class EGeometryScriptBooleanOperation : uint8 {
    Union,              // A + B
    Intersection,       // A & B
    Subtract,           // A - B (removes ToolMesh volume from TargetMesh)
    TrimInside,         // Remove parts of Target inside Tool (no fill)
    TrimOutside,        // Remove parts of Target outside Tool (no fill)
    NewPolyGroupInside, // Mark intersection region with new polygroup
    NewPolyGroupOutside // Mark outside region with new polygroup
};
```

### FGeometryScriptMeshBooleanOptions

```cpp
struct FGeometryScriptMeshBooleanOptions {
    bool bFillHoles = true;
    bool bSimplifyOutput = true;
    float SimplifyPlanarTolerance = 0.01f;
    bool bAllowEmptyResult = false;
    EGeometryScriptBooleanOutputSpace OutputTransformSpace = TargetTransformSpace;
};
```

**Critical detail:** Both meshes need `FTransform` parameters. The boolean is computed in a shared space. If both transforms are identity, both meshes must be in the same coordinate space. For furniture construction where primitives are appended with transforms, pass `FTransform::Identity` for both and the geometry is already positioned.

**Pattern for furniture:** Append all additive primitives to TargetMesh, create subtractive shapes in a separate ToolMesh, then `ApplyMeshBoolean(Target, Identity, Tool, Identity, Subtract)`.

### ApplyMeshSelfUnion

```cpp
static UDynamicMesh* ApplyMeshSelfUnion(
    UDynamicMesh* TargetMesh,
    FGeometryScriptMeshSelfUnionOptions Options,  // bFillHoles, bTrimFlaps, bSimplifyOutput, WindingThreshold
    UGeometryScriptDebug* Debug = nullptr);
```

**Essential cleanup pass.** After appending multiple overlapping primitives (e.g., maze walls), call SelfUnion to merge overlapping geometry into a single clean mesh. Fixes T-junctions, removes internal faces.

### ApplyMeshPlaneCut

```cpp
static UDynamicMesh* ApplyMeshPlaneCut(
    UDynamicMesh* TargetMesh,
    FTransform CutFrame,              // Origin = point on plane, Z axis = plane normal
    FGeometryScriptMeshPlaneCutOptions Options,
    UGeometryScriptDebug* Debug = nullptr);
```

**Behavior:** Removes everything on one side of the plane (the side the normal points away from, unless `bFlipCutSide`). Fills the cut surface if `bFillHoles = true`. Returns the **remaining** mesh (not both halves).

### ApplyMeshPlaneSlice

```cpp
static UDynamicMesh* ApplyMeshPlaneSlice(
    UDynamicMesh* TargetMesh,
    FTransform CutFrame,
    FGeometryScriptMeshPlaneSliceOptions Options,  // bFillHoles, GapWidth, etc.
    UGeometryScriptDebug* Debug = nullptr);
```

**Behavior:** Inserts edges along the plane but keeps both sides. Creates a gap of `GapWidth` (default 0.01). After slicing, the two halves become separate connected components that can be split with `SplitMeshByComponents`.

**This is the key to destruction fragments.** Slice repeatedly, then decompose.

---

## 3. Sweep & Extrusion API

**Header:** `MeshPrimitiveFunctions.h` (same class as primitives)

### AppendSimpleExtrudePolygon

```cpp
static UDynamicMesh* AppendSimpleExtrudePolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,  // CCW, no repeated endpoint
    float Height = 100,
    int32 HeightSteps = 0,
    bool bCapped = true,
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Building shells from 2D footprints, door frames (U-shape extrusion), wall profiles, thin decal meshes.

### AppendSimpleSweptPolygon

```cpp
static UDynamicMesh* AppendSimpleSweptPolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,  // Closed 2D polygon profile
    const TArray<FVector>& SweepPath,           // 3D path points
    bool bLoop = false,
    bool bCapped = true,
    float StartScale = 1.0f,
    float EndScale = 1.0f,
    float RotationAngleDeg = 0.0f,
    float MiterLimit = 1.0f,                    // >1 prevents shrinkage at sharp turns
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Pipes/ducts. Define a circle polygon, sweep along path points. The `MiterLimit` handles sharp corners. Simple version -- path is just 3D points, no per-point rotation control.

### AppendSweepPolygon (Full Control)

```cpp
static UDynamicMesh* AppendSweepPolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,  // Closed 2D polygon profile
    const TArray<FTransform>& SweepPath,        // Full transform at each point
    bool bLoop = false,
    bool bCapped = true,
    float StartScale = 1.0f,
    float EndScale = 1.0f,
    float RotationAngleDeg = 0.0f,
    float MiterLimit = 1.0f,
    UGeometryScriptDebug* Debug = nullptr);
```

**Better for pipes:** Each path point has a full FTransform, allowing rotation/scale variation along the path. For elbow joints, generate intermediate transforms along an arc between straight segments.

### AppendSweepPolyline (Open Profile)

```cpp
static UDynamicMesh* AppendSweepPolyline(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolylineVertices,    // Open 2D path (not closed)
    const TArray<FTransform>& SweepPath,
    const TArray<float>& PolylineTexParamU,       // UV U coords (can be empty)
    const TArray<float>& SweepPathTexParamV,      // UV V coords (can be empty)
    bool bLoop = false,
    float StartScale = 1.0f,
    float EndScale = 1.0f,
    float RotationAngleDeg = 0.0f,
    float MiterLimit = 1.0f,
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendRevolvePolygon

```cpp
static UDynamicMesh* AppendRevolvePolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,     // +X = outward, +Y = up (Z local)
    FGeometryScriptRevolveOptions RevolveOptions,  // RevolveDegrees, HardNormals, etc.
    float Radius = 100,
    int32 Steps = 8,
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Bowls (sink/toilet), vases, lathe-type shapes. For pipe elbows, could revolve a circle 90 degrees.

### AppendSpiralRevolvePolygon

```cpp
static UDynamicMesh* AppendSpiralRevolvePolygon(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    const TArray<FVector2D>& PolygonVertices,
    FGeometryScriptRevolveOptions RevolveOptions,
    float Radius = 100,
    int Steps = 18,
    float RisePerRevolution = 50,
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Spiral staircases, coiled pipes, springs.

### FGeometryScriptRevolveOptions

```cpp
struct FGeometryScriptRevolveOptions {
    float RevolveDegrees = 360.0f;
    float DegreeOffset = 0.0f;
    bool bReverseDirection = false;
    bool bHardNormals = false;
    float HardNormalAngle = 30.0f;
    bool bProfileAtMidpoint = false;
    bool bFillPartialRevolveEndcaps = true;
};
```

---

## 4. Deformation API

**Header:** `MeshDeformFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshDeformFunctions`

### ApplyPerlinNoiseToMesh2

```cpp
static UDynamicMesh* ApplyPerlinNoiseToMesh2(   // Note: "2" suffix, the 5.7 version
    UDynamicMesh* TargetMesh,
    FGeometryScriptMeshSelection Selection,       // Empty = full mesh
    FGeometryScriptPerlinNoiseOptions Options,
    UGeometryScriptDebug* Debug = nullptr);
```

**WARNING:** `ApplyPerlinNoiseToMesh` (without "2") is deprecated in 5.7 -- it incorrectly squared the frequency. Always use `ApplyPerlinNoiseToMesh2`.

### FGeometryScriptPerlinNoiseOptions

```cpp
struct FGeometryScriptPerlinNoiseOptions {
    FGeometryScriptPerlinNoiseLayerOptions BaseLayer;  // Magnitude, Frequency, FrequencyShift, RandomSeed
    bool bApplyAlongNormal = true;    // Displace along vertex normal (not world axis)
    EGeometryScriptEmptySelectionBehavior EmptyBehavior = FullMeshSelection;
};

struct FGeometryScriptPerlinNoiseLayerOptions {
    float Magnitude = 5.0;
    float Frequency = 0.25;
    FVector FrequencyShift = FVector::Zero();
    int RandomSeed = 0;
};
```

**Key for terrain:** Subdivide a box/rectangle with StepsX/StepsY, then apply Perlin noise displacement. The `bApplyAlongNormal` = true displaces along the surface normal (great for terrain), false would displace along a world axis.

**Key for broken walls:** Apply noise to a sphere before using it as a boolean subtract tool. Creates irregular holes.

### ApplyDisplaceFromTextureMap

```cpp
static UDynamicMesh* ApplyDisplaceFromTextureMap(
    UDynamicMesh* TargetMesh,
    UTexture2D* Texture,
    FGeometryScriptMeshSelection Selection,
    FGeometryScriptDisplaceFromTextureOptions Options,  // Magnitude, UVScale, UVOffset, Center, ImageChannel
    int32 UVLayer = 0,
    UGeometryScriptDebug* Debug = nullptr);
```

### ApplyDisplaceFromPerVertexVectors

```cpp
static UDynamicMesh* ApplyDisplaceFromPerVertexVectors(
    UDynamicMesh* TargetMesh,
    FGeometryScriptMeshSelection Selection,
    const FGeometryScriptVectorList& VectorList,   // One vector per vertex
    float Magnitude = 5.0,
    UGeometryScriptDebug* Debug = nullptr);
```

### Other Deformations

- `ApplyBendWarpToMesh` -- bend around axis (horror: warped corridors)
- `ApplyTwistWarpToMesh` -- twist around axis (horror: twisted metal)
- `ApplyFlareWarpToMesh` -- bulge/flare (horror: organic pulsing)
- `ApplyMathWarpToMesh` -- sine wave displacement
- `ApplyIterativeSmoothingToMesh` -- Laplacian smoothing

---

## 5. Decomposition API

**Header:** `MeshDecompositionFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshDecompositionFunctions`

### SplitMeshByComponents

```cpp
static UDynamicMesh* SplitMeshByComponents(
    UDynamicMesh* TargetMesh,
    TArray<UDynamicMesh*>& ComponentMeshes,    // Output: one mesh per island
    UDynamicMeshPool* MeshPool,                 // Strongly recommended for perf
    UGeometryScriptDebug* Debug = nullptr);
```

**Critical for fragments.** After plane-slicing a mesh into disconnected pieces, call this to extract each piece as a separate `UDynamicMesh`.

### SplitMeshByPolygroups

```cpp
static UDynamicMesh* SplitMeshByPolygroups(
    UDynamicMesh* TargetMesh,
    FGeometryScriptGroupLayer GroupLayer,
    TArray<UDynamicMesh*>& ComponentMeshes,
    TArray<int>& ComponentPolygroups,
    UDynamicMeshPool* MeshPool,
    UGeometryScriptDebug* Debug = nullptr);
```

### CopyMeshSelectionToMesh

```cpp
static UDynamicMesh* CopyMeshSelectionToMesh(
    UDynamicMesh* TargetMesh,
    UDynamicMesh* StoreToSubmesh,          // ref
    FGeometryScriptMeshSelection Selection,
    UDynamicMesh*& StoreToSubmeshOut,      // out
    bool bAppendToExisting = false,
    bool bPreserveGroupIDs = false,
    UGeometryScriptDebug* Debug = nullptr);
```

---

## 6. Mesh Editing API

**Header:** `MeshBasicEditFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshBasicEditFunctions`

### AppendMesh

```cpp
static UDynamicMesh* AppendMesh(
    UDynamicMesh* TargetMesh,
    UDynamicMesh* AppendMesh,
    FTransform AppendTransform,
    bool bDeferChangeNotifications = false,
    FGeometryScriptAppendMeshOptions AppendOptions = {},
    UGeometryScriptDebug* Debug = nullptr);
```

### AppendMeshTransformed (Instanced Append)

```cpp
static UDynamicMesh* AppendMeshTransformed(
    UDynamicMesh* TargetMesh,
    UDynamicMesh* AppendMesh,
    const TArray<FTransform>& AppendTransforms,   // One copy per transform
    FTransform ConstantTransform,
    bool bConstantTransformIsRelative = true,
    bool bDeferChangeNotifications = false,
    FGeometryScriptAppendMeshOptions AppendOptions = {},
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Cage bars (cylinder x N transforms), fence posts, shelf shelves, barricade boards.

### AppendMeshRepeated

```cpp
static UDynamicMesh* AppendMeshRepeated(
    UDynamicMesh* TargetMesh,
    UDynamicMesh* AppendMesh,
    FTransform AppendTransform,     // Applied iteratively
    int RepeatCount = 1,
    bool bApplyTransformToFirstInstance = true,
    bool bDeferChangeNotifications = false,
    FGeometryScriptAppendMeshOptions AppendOptions = {},
    UGeometryScriptDebug* Debug = nullptr);
```

**Best for:** Regular patterns -- fence slats, vent grate bars, shelf shelves at even spacing.

### SetVertexPosition

```cpp
static UDynamicMesh* SetVertexPosition(
    UDynamicMesh* TargetMesh,
    int VertexID,
    FVector NewPosition,
    bool& bIsValidVertex,
    bool bDeferChangeNotifications = false);
```

---

## 7. Transform API

**Header:** `MeshTransformFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshTransformFunctions`

```cpp
static UDynamicMesh* TransformMesh(UDynamicMesh* TargetMesh, FTransform Transform, ...);
static UDynamicMesh* ScaleMesh(UDynamicMesh* TargetMesh, FVector Scale, FVector ScaleOrigin, ...);
static UDynamicMesh* TransformMeshSelection(UDynamicMesh* TargetMesh, FGeometryScriptMeshSelection Selection, FTransform Transform, ...);
static UDynamicMesh* ScaleMeshSelection(UDynamicMesh* TargetMesh, FGeometryScriptMeshSelection Selection, FVector Scale, FVector ScaleOrigin, ...);
```

---

## 8. Normals API

**Header:** `MeshNormalsFunctions.h`
**Class:** `UGeometryScriptLibrary_MeshNormalsFunctions`

```cpp
// Recompute normals preserving hard edges
static UDynamicMesh* RecomputeNormals(UDynamicMesh* TargetMesh, FGeometryScriptCalculateNormalsOptions Options, ...);

// Recompute + re-split normals by angle
static UDynamicMesh* ComputeSplitNormals(UDynamicMesh* TargetMesh,
    FGeometryScriptSplitNormalsOptions SplitOptions,
    FGeometryScriptCalculateNormalsOptions CalculateOptions, ...);

// Set explicit per-vertex normals
static UDynamicMesh* SetMeshPerVertexNormals(UDynamicMesh* TargetMesh,
    FGeometryScriptVectorList VertexNormalList, ...);
```

**After booleans:** Always call `ComputeSplitNormals` to get clean shading. Booleans can create bad normals at intersection seams.

---

## 9. Feature-by-Feature Analysis

### 9.1 Parametric Furniture (`create_parametric_mesh`)

**Approach:** Additive-only construction using `AppendBox`/`AppendCylinder`/`AppendSphereLatLong` with per-primitive transforms, then optional `ApplyMeshBoolean(Subtract)` for subtractive features (sink bowl, bathtub interior, cabinet recess).

| Furniture Type | Primitives Used | Boolean Needed? | Notes |
|---|---|---|---|
| chair | 6x AppendBox (seat + back + 4 legs) | No | Additive only, SelfUnion optional |
| table | 5x AppendBox (top + 4 legs) | No | Additive only |
| desk | 6-8x AppendBox | No | Additive only |
| shelf | (2 + N) AppendBox | No | Use `AppendMeshRepeated` for shelves |
| cabinet | AppendBox + Boolean Subtract | Yes | Recess for door |
| bed | 3-4x AppendBox | No | Frame + mattress + headboard |
| door_frame | AppendBox - AppendBox | Yes | Or: extrude U-shape polygon |
| window_frame | AppendBox - AppendBox | Yes | Or: extrude polygon |
| stairs | **AppendLinearStairs** native | No | Direct API! |
| ramp | AppendBox with wedge transform | No | Or AppendCone with large radius |
| pillar | AppendCylinder or AppendBox | No | Direct |
| counter | Multiple AppendBox | No | L-shape = 2 boxes |
| toilet | AppendBox + AppendCylinder - AppendSphereLatLong | Yes | Bowl subtraction |
| sink | AppendBox - AppendSphereLatLong | Yes | Bowl subtraction |
| bathtub | AppendBox - AppendBox (smaller) | Yes | Interior subtraction |

**Code pattern (chair):**
```cpp
UDynamicMesh* Mesh = NewObject<UDynamicMesh>();
FGeometryScriptPrimitiveOptions Opts;

// Seat
FTransform SeatT(FRotator::ZeroRotator, FVector(0, 0, SeatHeight - SeatThickness), FVector::OneVector);
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(Mesh, Opts, SeatT, Width, Depth, SeatThickness);

// Back
FTransform BackT(FRotator::ZeroRotator, FVector(0, -Depth/2 + LegThickness/2, SeatHeight), FVector::OneVector);
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(Mesh, Opts, BackT, Width, LegThickness, BackHeight - SeatHeight);

// 4 Legs
for (const FVector2D& Pos : LegPositions) {
    FTransform LegT(FRotator::ZeroRotator, FVector(Pos.X, Pos.Y, 0), FVector::OneVector);
    UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder(Mesh, Opts, LegT, LegThickness/2, SeatHeight);
}

// Cleanup
UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshSelfUnion(Mesh, SelfUnionOpts);
UGeometryScriptLibrary_MeshNormalsFunctions::ComputeSplitNormals(Mesh, SplitOpts, CalcOpts);
```

**Performance:** Additive-only furniture (no booleans) is extremely fast -- sub-millisecond. Boolean subtract adds ~5-50ms per operation depending on mesh complexity. A sink or toilet with one boolean subtract: ~20ms total.

### 9.2 Structure Generator (`create_structure`)

**Approach A (Simple):** Build walls as thin boxes, floor/ceiling as boxes, subtract door/window openings via boolean.

**Approach B (Polygon Extrusion):** Define 2D wall profile with openings already cut out, extrude with `AppendSimpleExtrudePolygon`.

**Recommended: Hybrid.** Use Approach A for the basic room shell (faster to code), Approach B for the building shell with complex footprints.

**Room construction:**
```cpp
// Floor slab
AppendBox(Mesh, Opts, FloorT, Width, Depth, FloorThickness);
// Ceiling slab
AppendBox(Mesh, Opts, CeilingT, Width, Depth, CeilingThickness);
// 4 Walls (each a thin box)
AppendBox(Mesh, Opts, WallNorthT, Width, WallThickness, Height);
AppendBox(Mesh, Opts, WallSouthT, Width, WallThickness, Height);
AppendBox(Mesh, Opts, WallEastT, WallThickness, Depth, Height);
AppendBox(Mesh, Opts, WallWestT, WallThickness, Depth, Height);

// Door openings: subtract boxes
for (auto& Door : Doors) {
    UDynamicMesh* DoorCut = NewObject<UDynamicMesh>();
    AppendBox(DoorCut, Opts, DoorT, Door.Width, WallThickness * 3, Door.Height);  // Oversized depth
    ApplyMeshBoolean(Mesh, FTransform::Identity, DoorCut, FTransform::Identity, Subtract, BoolOpts);
}

// Window openings: same approach
for (auto& Window : Windows) {
    UDynamicMesh* WinCut = NewObject<UDynamicMesh>();
    AppendBox(WinCut, Opts, WindowT, Window.Width, WallThickness * 3, Window.Height);
    ApplyMeshBoolean(Mesh, FTransform::Identity, WinCut, FTransform::Identity, Subtract, BoolOpts);
}
```

**Building shell (`create_building_shell`):**
```cpp
// Extrude outer footprint polygon to full building height
TArray<FVector2D> OuterPoly = FootprintVertices;
AppendSimpleExtrudePolygon(Mesh, Opts, FTransform::Identity, OuterPoly, TotalHeight);

// Create inner polygon (inset by wall_thickness)
TArray<FVector2D> InnerPoly = InsetPolygon(OuterPoly, WallThickness);
UDynamicMesh* InnerVoid = NewObject<UDynamicMesh>();
AppendSimpleExtrudePolygon(InnerVoid, Opts, FTransform::Identity, InnerPoly, TotalHeight);

// Subtract interior to create shell
ApplyMeshBoolean(Mesh, FTransform::Identity, InnerVoid, FTransform::Identity, Subtract, BoolOpts);

// Add floor slabs per story
for (int i = 0; i < Floors; i++) {
    FTransform FloorT(FRotator::ZeroRotator, FVector(0, 0, i * FloorHeight), FVector::OneVector);
    AppendTriangulatedPolygon(Mesh, Opts, FloorT, OuterPoly);  // Flat slab
}
```

**Performance:** Room with 2 doors + 2 windows = 6 boxes + 4 booleans. ~100-200ms. Building shell = 2 extrusions + 1 boolean + N floor slabs. ~50-150ms depending on polygon complexity.

### 9.3 Maze Generator (`create_maze`)

**Approach:** Pure algorithm generates wall segments, each wall = `AppendBox`. Then `ApplyMeshSelfUnion` to merge.

```cpp
// After maze algorithm produces wall segments as (start, end, thickness, height):
for (auto& Wall : WallSegments) {
    float Length = FVector2D::Distance(Wall.Start, Wall.End);
    FVector Center = FVector((Wall.Start.X + Wall.End.X) / 2, (Wall.Start.Y + Wall.End.Y) / 2, 0);
    float Angle = FMath::Atan2(Wall.End.Y - Wall.Start.Y, Wall.End.X - Wall.Start.X);
    FTransform WallT(FRotator(0, FMath::RadiansToDegrees(Angle), 0), Center, FVector::OneVector);
    AppendBox(Mesh, Opts, WallT, Length, WallThickness, WallHeight);
}

// Merge overlapping walls at corners
ApplyMeshSelfUnion(Mesh, SelfUnionOpts);
```

**Alternative (faster):** Skip SelfUnion, accept minor overlaps. For blockout quality this is fine. SelfUnion on a large maze (64x64 cells, ~500 wall segments) could take 1-5 seconds.

**Performance:** 8x8 maze = ~100 wall segments. AppendBox per wall: <1ms. Total without SelfUnion: ~100ms. With SelfUnion: ~500ms-2s.

### 9.4 Pipe/Duct Network (`create_pipe_network`)

**Approach:** Use `AppendSimpleSweptPolygon` or `AppendSweepPolygon` with a circle polygon profile.

```cpp
// Generate circle profile
TArray<FVector2D> CircleProfile;
for (int i = 0; i < Segments; i++) {
    float Angle = 2.0f * PI * i / Segments;
    CircleProfile.Add(FVector2D(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius));
}

// Simple version: just path points, auto-framing
AppendSimpleSweptPolygon(Mesh, Opts, FTransform::Identity, CircleProfile, PathPoints,
    /*bLoop=*/false, /*bCapped=*/true, /*StartScale=*/1.0f, /*EndScale=*/1.0f,
    /*RotationAngleDeg=*/0.0f, /*MiterLimit=*/2.0f);  // MiterLimit > 1 prevents pinching at bends
```

**Elbow handling:** `AppendSimpleSweptPolygon` handles bends automatically via the path points. For sharp 90-degree bends, insert intermediate points along an arc:

```cpp
// Insert arc points between straight segments at corners
void InsertElbowArc(TArray<FVector>& Path, int CornerIdx, float ElbowRadius, int ArcSteps) {
    // Calculate corner tangent directions
    FVector DirIn = (Path[CornerIdx] - Path[CornerIdx-1]).GetSafeNormal();
    FVector DirOut = (Path[CornerIdx+1] - Path[CornerIdx]).GetSafeNormal();
    // Generate arc points...
    // Replace corner point with arc points
}
```

**Alternative for smooth elbows:** Use `AppendRevolvePolygon` with `RevolveDegrees = 90` at each bend, then `AppendMesh` to merge.

**Joint types:**
- `elbow`: Arc interpolation (smooth)
- `mitered`: Just use sharp path points, `MiterLimit = 1` (flat corners)
- `ball`: Append a sphere at each junction point

**Performance:** Single pipe run with 4 segments and 3 elbows: <10ms. Network of 20 pipes: ~100ms.

### 9.5 Destruction Fragments (`create_fragments`)

**Approach:** Iterative `ApplyMeshPlaneSlice` to split mesh, then `SplitMeshByComponents` to extract pieces.

**NOT Voronoi fracture from GeometryScript** -- there is no native 3D Voronoi mesh fracture in GeometryScript. `AppendVoronoiDiagram2D` only generates 2D cell diagrams. Must implement fracture via plane slicing.

**Algorithm:**
```cpp
// 1. Generate random cutting planes (pseudo-Voronoi via random planes through random interior points)
TArray<FTransform> CutPlanes;
FBox MeshBounds = GetMeshBoundingBox(SourceMesh);
FRandomStream Rand(Seed);

for (int i = 0; i < FragmentCount - 1; i++) {
    // Random point inside mesh bounds
    FVector Point = FVector(
        Rand.FRandRange(MeshBounds.Min.X, MeshBounds.Max.X),
        Rand.FRandRange(MeshBounds.Min.Y, MeshBounds.Max.Y),
        Rand.FRandRange(MeshBounds.Min.Z, MeshBounds.Max.Z));
    // Random normal (optionally add noise)
    FVector Normal = Rand.VRand();
    CutPlanes.Add(FTransform(FRotationMatrix::MakeFromZ(Normal).Rotator(), Point, FVector::OneVector));
}

// 2. Apply all plane slices
FGeometryScriptMeshPlaneSliceOptions SliceOpts;
SliceOpts.bFillHoles = true;     // Cap cut surfaces
SliceOpts.GapWidth = 0.01f;      // Tiny gap so pieces separate
for (const FTransform& CutPlane : CutPlanes) {
    ApplyMeshPlaneSlice(SourceMesh, CutPlane, SliceOpts);
}

// 3. Split into individual fragments
TArray<UDynamicMesh*> Fragments;
SplitMeshByComponents(SourceMesh, Fragments, MeshPool);
// Each element of Fragments is now a separate piece
```

**Better approach (true Voronoi):** Generate N random seed points inside the mesh bounds. For each pair of adjacent seeds, create a plane bisecting them. This produces more natural-looking fragments than purely random planes. Can use `AppendVoronoiDiagram2D` to determine adjacency, then compute bisecting planes.

**Noise parameter:** Add Perlin noise displacement to cut planes or post-process fragments with `ApplyPerlinNoiseToMesh2` to create rough/irregular break surfaces.

**Performance:** N-1 plane slices for N fragments. Each slice: ~5-20ms. 8 fragments = 7 slices = ~50-140ms. SplitByComponents: ~10-50ms. Total for 8 fragments: ~100-200ms. Acceptable for editor-time.

### 9.6 Terrain Patches (`create_terrain_patch`)

**Approach:** `AppendBox` with high subdivision, then `ApplyPerlinNoiseToMesh2` for heightmap displacement.

```cpp
// Create subdivided grid
FGeometryScriptPrimitiveOptions Opts;
Opts.PolygroupMode = EGeometryScriptPrimitivePolygroupMode::PerQuad;

AppendBox(Mesh, Opts, FTransform::Identity,
    SizeX, SizeY, 1.0f,      // Very thin box
    Resolution, Resolution, 0, // High XY subdivision, no Z subdivision
    EGeometryScriptPrimitiveOriginMode::Base);

// Apply Perlin noise displacement
FGeometryScriptPerlinNoiseOptions NoiseOpts;
NoiseOpts.BaseLayer.Magnitude = Amplitude;      // e.g., 100
NoiseOpts.BaseLayer.Frequency = Scale;           // e.g., 0.01
NoiseOpts.BaseLayer.RandomSeed = Seed;
NoiseOpts.bApplyAlongNormal = true;              // Displaces upward for flat surface

ApplyPerlinNoiseToMesh2(Mesh, FGeometryScriptMeshSelection(), NoiseOpts);
```

**Multi-octave noise:** GeometryScript only provides single-layer Perlin. For octave noise (fbm), apply `ApplyPerlinNoiseToMesh2` multiple times with decreasing magnitude and increasing frequency:

```cpp
for (int Octave = 0; Octave < NumOctaves; Octave++) {
    NoiseOpts.BaseLayer.Magnitude = Amplitude * FMath::Pow(Persistence, Octave);
    NoiseOpts.BaseLayer.Frequency = Scale * FMath::Pow(Lacunarity, Octave);
    NoiseOpts.BaseLayer.RandomSeed = Seed + Octave;
    ApplyPerlinNoiseToMesh2(Mesh, FGeometryScriptMeshSelection(), NoiseOpts);
}
```

**Alternative -- manual vertex displacement:** Use `FMath::PerlinNoise2D` (verified in `UnrealMathUtility.h`) with manual `SetVertexPosition` calls for full control:

```cpp
// UE5 built-in noise functions (verified in source):
float FMath::PerlinNoise1D(float Value);
float FMath::PerlinNoise2D(const FVector2D& Location);
float FMath::PerlinNoise3D(const FVector& Location);
```

**Worley/Ridged noise:** Not built into UE or GeometryScript. Must implement manually via `SetVertexPosition` + custom noise functions. Worley (cellular) noise requires generating random cell centers and computing nearest-neighbor distances.

**Performance:** 32x32 grid = 1024 quads = ~2048 tris. Append + noise: <50ms. 128x128 grid = ~32K tris: ~200ms. Acceptable.

### 9.7 Horror Props (`create_horror_prop`)

| Prop | Construction Method |
|---|---|
| barricade | `AppendMeshTransformed` with a plank box template, random rotations per board |
| debris_pile | Multiple `AppendBox`/`AppendCylinder` with random transforms, then `ApplyMeshSelfUnion` |
| cage | `AppendMeshRepeated` with cylinder bar template, regular spacing |
| coffin | `AppendBox` - `AppendBox` (interior) boolean, optional lid as separate piece |
| gurney | Multiple `AppendBox` (top, frame) + `AppendCylinder` (legs, wheels) |
| operating_table | Same as gurney but wider dimensions |
| broken_wall | `AppendBox` (wall) - noise-displaced sphere boolean (see below) |
| vent_grate | `AppendBox` (frame) + `AppendMeshRepeated` (bars) or boolean grid |
| chain_link_fence | Too complex for simple boolean approach -- would need texture/alpha instead |

**Broken wall technique:**
```cpp
// 1. Create wall
AppendBox(WallMesh, Opts, WallT, WallWidth, WallThickness, WallHeight, 0, 0, 0);

// 2. Create hole cutter sphere with high subdivision
UDynamicMesh* CutterMesh = NewObject<UDynamicMesh>();
AppendSphereBox(CutterMesh, Opts, HoleT, HoleRadius, 8, 8, 8);  // Box-topology for better deformation

// 3. Apply Perlin noise to make it irregular
FGeometryScriptPerlinNoiseOptions NoiseOpts;
NoiseOpts.BaseLayer.Magnitude = HoleRadius * Noise;  // e.g., 0.3 * radius
NoiseOpts.BaseLayer.Frequency = 0.5;
NoiseOpts.BaseLayer.RandomSeed = Seed;
ApplyPerlinNoiseToMesh2(CutterMesh, FGeometryScriptMeshSelection(), NoiseOpts);

// 4. Boolean subtract
ApplyMeshBoolean(WallMesh, FTransform::Identity, CutterMesh, FTransform::Identity,
    EGeometryScriptBooleanOperation::Subtract, BoolOpts);
```

### 9.8 Decal Meshes (`create_decal_mesh`)

**Approach:** Generate irregular 2D polygon, triangulate, optionally extrude thin.

**Splatter shape:**
```cpp
// Generate irregular polygon
TArray<FVector2D> SplatterVerts;
FRandomStream Rand(Seed);
for (int i = 0; i < NumPoints; i++) {
    float Angle = 2.0f * PI * i / NumPoints;
    float R = Radius * (1.0f + Irregularity * (Rand.FRand() * 2.0f - 1.0f));
    SplatterVerts.Add(FVector2D(FMath::Cos(Angle) * R, FMath::Sin(Angle) * R));
}

// Flat triangulated polygon
AppendTriangulatedPolygon(Mesh, Opts, FTransform::Identity, SplatterVerts);

// Optional: thin extrusion for mesh decal (needs slight depth)
// Better approach: use AppendSimpleExtrudePolygon with tiny height
AppendSimpleExtrudePolygon(Mesh, Opts, FTransform::Identity, SplatterVerts,
    /*Height=*/0.5f, /*HeightSteps=*/0, /*bCapped=*/true);
```

**Crack shape:** Generate branching line segments, thicken each segment with thin `AppendBox`, then `SelfUnion`.

**Performance:** Simple polygon triangulation: <1ms. Thin extrusion: <1ms. Trivial.

---

## 10. Performance Notes

### Boolean Operations
- **Single boolean:** 5-50ms depending on mesh complexity
- **Multiple sequential booleans:** Each adds 5-50ms. For N subtractions (doors/windows), total is roughly N * 20ms
- **SelfUnion:** 50ms-5s depending on overlap complexity. Maze with 500 walls: potentially slow
- **Recommendation:** Avoid SelfUnion for large meshes. For mazes, accept overlapping geometry. For furniture, only use boolean when subtractive features are needed

### Mesh Creation
- **AppendBox/Cylinder/Sphere:** <1ms each
- **AppendSimpleExtrudePolygon:** <1ms for simple polygons, ~5ms for complex
- **AppendSimpleSweptPolygon:** ~5-10ms per pipe segment
- **All primitives:** Cost grows with subdivision steps. Keep Steps <= 16 for blockout quality

### Decomposition
- **SplitMeshByComponents:** ~10-50ms for up to 20 components
- **SplitMeshByPolygroups:** Similar, but scales with polygroup count

### Deformation
- **ApplyPerlinNoiseToMesh2:** ~1-10ms for meshes under 10K verts
- **SetVertexPosition (manual loop):** ~0.1ms per 1000 vertices

### Total Budget Estimates
| Feature | Typical Time |
|---|---|
| Simple furniture (no boolean) | <10ms |
| Furniture with 1-2 booleans | 30-80ms |
| Room with 4 openings | 100-250ms |
| Building shell (3 floors) | 100-300ms |
| 8x8 maze | 100ms-2s (depends on SelfUnion) |
| Pipe network (4 segments) | <50ms |
| 8 destruction fragments | 100-200ms |
| Terrain 32x32 | <50ms |
| Horror prop (complex) | 50-150ms |
| Decal mesh | <5ms |

All times are editor-time on a modern CPU. Well within acceptable range for on-demand procedural generation.

---

## 11. Missing APIs & Workarounds

### Not in GeometryScript

| Missing Feature | Workaround |
|---|---|
| **3D Voronoi fracture** | Iterative `ApplyMeshPlaneSlice` + `SplitMeshByComponents` |
| **Multi-octave Perlin** | Apply `ApplyPerlinNoiseToMesh2` multiple times with different params |
| **Worley/Cellular noise** | Manual via `SetVertexPosition` + custom noise |
| **Simplex noise** | `FMath::PerlinNoise3D` is gradient noise (similar characteristics). True simplex would need custom impl |
| **Ridged noise** | `abs(noise)` -- apply Perlin then manually transform vertex heights |
| **Polygon inset/offset** | Must implement 2D polygon offset manually (Clipper-style) for building shells |
| **2D boolean on polygons** | No 2D CSG in GeometryScript. Use 3D booleans on thin extrusions, or implement 2D polygon clipping |
| **Mesh UV projection** | `SetMeshUVsFromBoxProjection`, `SetMeshUVsFromCylinderProjection`, `SetMeshUVsFromPlanarProjection` exist in `MeshUVFunctions.h` |
| **Chain link fence pattern** | Too fine for geometry -- use alpha-masked material on a simple plane |

### Key API Aliases (wishlist vs actual)

| Wishlist Name | Actual UE 5.7 API |
|---|---|
| `AppendSweepPolygon` | `AppendSweepPolygon` (exists as named) |
| `AppendLinearExtrudePath` | Does not exist. Use `AppendSimpleExtrudePolygon` (extrude 2D polygon upward) or `AppendSimpleSweptPolygon` (sweep along 3D path) |
| `ApplyMeshSelfUnion` | Exists as named |
| `ApplyMeshPlaneCut` | Exists as named (keeps one side). Use `ApplyMeshPlaneSlice` to keep both sides |
| `SetMeshPerVertexNormals` | Exists as named |

### Module Dependencies

```cpp
// Build.cs
PublicDependencyModuleNames.AddRange(new string[] {
    "GeometryScriptingCore",   // All the APIs above
    "GeometryFramework",       // UDynamicMesh, UDynamicMeshPool
});
```

### Critical Gotcha: UDynamicMesh Lifetime

`UDynamicMesh` is a UObject. Temporary meshes used as boolean tools must be properly managed:
- Use `UDynamicMeshPool` for allocation/recycling (prevents GC churn)
- Do NOT create via `NewObject<UDynamicMesh>()` in a tight loop without pool
- After `SplitMeshByComponents`, the pool owns the output meshes -- release back when done

---

## Summary: API Completeness by Feature

| Feature | API Coverage | Custom Code Needed |
|---|---|---|
| Parametric furniture | **95%** -- all primitives + booleans native | Furniture type dispatch, parameter mapping |
| Structure generator | **85%** -- primitives + booleans + extrusion | 2D polygon inset, opening placement math |
| Maze generator | **70%** -- boxes + SelfUnion | Maze algorithms (DFS, Prim's, etc.) |
| Pipe/duct network | **90%** -- sweep polygon + revolve | Elbow arc interpolation, junction logic |
| Destruction fragments | **80%** -- plane slice + decomposition | Pseudo-Voronoi plane generation |
| Terrain patches | **75%** -- subdivided box + Perlin | Multi-octave noise, Worley/ridged impl |
| Horror props | **85%** -- same as furniture + noise deform | Type-specific layouts, noise displacement |
| Decal meshes | **95%** -- polygon triangulation + extrude | Irregular polygon generation |
