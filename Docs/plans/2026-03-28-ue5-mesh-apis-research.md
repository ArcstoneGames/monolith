# UE5 Mesh Editing APIs -- Comprehensive Audit

**Date:** 2026-03-28
**Scope:** Every mesh editing API in UE 5.7 we are NOT currently using, organized by category
**Goal:** Identify functions that could improve Monolith's procedural building generation

---

## Table of Contents

1. [What We Currently Use (Baseline)](#1-baseline)
2. [GeometryScript Libraries We Do NOT Use (42 headers total)](#2-geometryscript-unused)
3. [Lower-Level GeometryCore/DynamicMesh Operations](#3-lower-level-ops)
4. [FDynamicMeshEditor Direct API](#4-fdynamicmesheditor)
5. [GeometryCore Generators](#5-generators)
6. [Mesh Component & Runtime Options](#6-mesh-components)
7. [Third-Party & Integration Options](#7-third-party)
8. [Priority Matrix for Building Generation](#8-priority-matrix)

---

## 1. Baseline -- What We Currently Use {#1-baseline}

| Library | Functions Used |
|---------|---------------|
| `MeshPrimitiveFunctions` | `AppendBox`, `AppendCylinder`, `AppendSimpleSweptPolygon`, `AppendTriangulatedPolygon3D`, `AppendLinearStairs`, `AppendSimpleExtrudePolygon` |
| `MeshBooleanFunctions` | `ApplyMeshBoolean` (subtract) |
| `MeshNormalsFunctions` | `SetPerFaceNormals`, `ComputeTangents` |
| `MeshBasicEditFunctions` | `AppendMesh` |
| `MeshTransformFunctions` | `TransformMesh` |
| `MeshUVFunctions` | `SetMeshUVsFromBoxProjection` |

**That is 6 out of 42 GeometryScript library headers, using maybe 12 out of 200+ available functions.**

---

## 2. GeometryScript Libraries We Do NOT Use {#2-geometryscript-unused}

### 2.1 MeshSelectionFunctions -- CRITICAL FOR BUILDING GEN

**Header:** `GeometryScript/MeshSelectionFunctions.h`
**Module:** `GeometryScriptingCore`

| Function | Signature | What It Does | Building Gen Use |
|----------|-----------|--------------|------------------|
| `SelectMeshElementsByMaterialID` | `(UDynamicMesh*, int MaterialID, Selection&, SelectionType)` | Select all tris with a given MaterialID | **Select wall faces vs floor faces vs trim for per-element operations** |
| `SelectMeshElementsByNormalAngle` | `(UDynamicMesh*, Selection&, Normal, MaxAngleDeg, SelectionType)` | Select faces by normal direction | **Select all floor-facing tris (Normal=Up), all wall tris (Normal=Horizontal) -- HUGE for selective operations** |
| `SelectMeshElementsWithPlane` | `(UDynamicMesh*, Selection&, PlaneOrigin, PlaneNormal, SelectionType)` | Select faces on positive side of plane | **Slice building at floor height -- select everything above/below a floor** |
| `SelectMeshElementsInBox` | `(UDynamicMesh*, Selection&, Box, SelectionType, bInvert, MinVerts)` | Select faces in AABB | **Select tris in a room region, a wall segment, etc.** |
| `SelectMeshElementsInSphere` | `(UDynamicMesh*, Selection&, Origin, Radius, SelectionType)` | Select faces in sphere | **Select geometry around a point (e.g. for damage/decay zones)** |
| `SelectMeshElementsInsideMesh` | `(Target, SelectionMesh, Selection&, Transform, SelectionType)` | Select elements inside another mesh using winding number | **Boolean-like selection without actually cutting -- select what a cutter volume overlaps** |
| `SelectMeshElementsByPolygroup` | `(UDynamicMesh*, GroupLayer, PolygroupID, Selection&)` | Select by polygroup | **If we tag rooms/walls with polygroups, select entire rooms** |
| `ExpandMeshSelectionToConnected` | `(UDynamicMesh*, Selection, NewSelection&, ConnectionType)` | Flood-fill expand selection | **Select a triangle on a wall, expand to get entire connected wall face** |
| `ExpandContractMeshSelection` | `(UDynamicMesh*, Selection, NewSelection&, Iterations, bContract)` | Grow/shrink selection by N rings | **Grow selection outward from a seed point** |
| `InvertMeshSelection` | `(UDynamicMesh*, Selection, NewSelection&, bOnlyConnected)` | Invert selection | **Select everything EXCEPT floor, for ceiling operations** |
| `CombineMeshSelections` | `(SelectionA, SelectionB, ResultSelection&, CombineMode)` | Union/Intersect/Subtract selections | **Combine wall selection + ceiling selection** |
| `SelectMeshSharpEdges` | `(UDynamicMesh*, Selection&, MinAngleDeg)` | Select edges where normals differ sharply | **Find wall/floor boundaries automatically** |
| `SelectMeshBoundaryEdges` | `(UDynamicMesh*, Selection&)` | Select all open boundary edges | **Find holes in geometry, open edges** |
| `SelectMeshUVSeamEdges` | `(UDynamicMesh*, Selection&, UVChannel, bHaveValidUVs)` | Select UV seam edges | **Debug UV issues** |
| `SelectSelectionBoundaryEdges` | `(UDynamicMesh*, Selection, BoundarySelection&)` | Get boundary edges of a selection | **Find the edge loop around a room's floor, a window opening, etc.** |

**Verdict: This is the single most impactful library we're missing. Selection is the key to targeted mesh operations.**

---

### 2.2 MeshModelingFunctions -- FACE EXTRUSION, INSET, BEVEL

**Header:** `GeometryScript/MeshModelingFunctions.h`
**Module:** `GeometryScriptingCore`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplyMeshLinearExtrudeFaces` | Extrude selected faces along a direction | **Extrude a wall face outward to create a window frame, cornice, pilaster -- no booleans needed** |
| `ApplyMeshOffsetFaces` | Offset selected faces along normals | **Push wall sections in/out for depth variation, create recessed panels** |
| `ApplyMeshInsetOutsetFaces` | Inset or outset faces (creates inner face + connecting strip) | **Create window recesses by insetting a wall face, door frames, panel details** |
| `ApplyMeshBevelEdgeSelection` | Bevel edges with subdivisions + roundness | **Bevel wall/floor edges for realistic rounded corners** |
| `ApplyMeshBevelSelection` | Bevel with region selection modes (TriangleArea, AllPolyGroupEdges, SharedPolyGroupEdges, SelectedEdges) | **Bevel all polygroup boundaries = automatic edge detailing** |
| `ApplyMeshPolygroupBevel` | Bevel all PolyGroup edges globally | **One-call detail pass on entire building** |
| `ApplyMeshDisconnectFaces` | Disconnect (detach) selected faces | **Separate a room's floor for independent manipulation** |
| `ApplyMeshDisconnectFacesAlongEdges` | Disconnect along specific edges | **Split wall mesh at door/window positions** |
| `ApplyMeshDuplicateFaces` | Duplicate selected faces in-place | **Duplicate a wall face to create a frame overlay** |
| `ApplyMeshOffset` | Offset all vertices along normals | **Create shell/thickness for thin-wall geometry** |
| `ApplyMeshShell` | Create a thickened shell from mesh | **Turn a single-sided wall into a solid wall with interior/exterior faces** |

**Verdict: Extrude + Inset + Bevel are the holy trinity for architectural detail. This replaces booleans for window frames, cornices, trim, and recessed panels.**

---

### 2.3 MeshDecompositionFunctions -- SPLIT MESH BY CRITERIA

**Header:** `GeometryScript/MeshDecompositionFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `SplitMeshByComponents` | Split into connected islands | **Separate rooms if they were generated as one mesh** |
| `SplitMeshByVertexOverlap` | Split accounting for overlapping verts | **Handle welded-but-separate geometry** |
| `SplitMeshByMaterialIDs` | One mesh per material | **Split building into wall mesh, floor mesh, trim mesh for separate LOD/material handling** |
| `SplitMeshByPolygroups` | One mesh per polygroup | **Split into per-room meshes** |
| `CopyMeshSelectionToMesh` | Copy selected faces to a new mesh | **Extract a room's geometry as standalone mesh** |
| `CopyMeshToMesh` | Clone a mesh | **Cache/duplicate building sections** |
| `SortMeshesByVolume/Area/BoundsVolume` | Sort mesh arrays by size | **Sort rooms by size for gameplay logic** |

**Verdict: Very useful for post-generation decomposition. Split by material = separate LOD groups. Split by polygroup = per-room meshes.**

---

### 2.4 MeshSpatialFunctions -- SPATIAL QUERIES ON GEOMETRY

**Header:** `GeometryScript/MeshSpatialFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `BuildBVHForMesh` | Build acceleration structure | **Required for fast spatial queries** |
| `FindNearestPointOnMesh` | Closest point on mesh surface | **Snap furniture to walls/floors** |
| `FindNearestRayIntersectionWithMesh` | Raycast against mesh | **Test line-of-sight through generated geometry** |
| `IsPointInsideMesh` | Fast winding number inside/outside test | **Verify a point is inside a room** |
| `SelectMeshElementsInBoxWithBVH` | Box selection accelerated by BVH | **Fast region queries on large meshes** |

**Verdict: Essential for furniture placement, validation, and spatial queries against generated geometry.**

---

### 2.5 MeshRepairFunctions -- FIX BAD GEOMETRY

**Header:** `GeometryScript/MeshRepairFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `WeldMeshEdges` | Weld open boundary edges together | **Fix cracks between wall segments, seal mesh after combining rooms** |
| `FillAllMeshHoles` | Fill open holes with triangulation | **Auto-fill holes left by boolean operations** |
| `ResolveMeshTJunctions` | Fix T-junctions | **Fix vertex-on-edge issues from combining meshes of different resolution** |
| `SnapMeshOpenBoundaries` | Snap boundary verts to nearby boundaries | **Align wall edges that are slightly misaligned** |
| `RemoveHiddenTriangles` | Remove interior/occluded tris (fast winding or raycast) | **Remove interior wall faces between merged rooms -- HUGE perf win** |
| `SelectHiddenTrianglesFromOutside` | Select (don't delete) hidden tris with view direction filtering | **Identify interior geometry for LOD/culling decisions** |
| `RemoveSmallComponents` | Remove tiny disconnected islands | **Clean up boolean artifacts** |
| `SplitMeshBowties` | Fix bowtie vertices | **Clean up after mesh merging** |
| `RepairMeshDegenerateGeometry` | Remove degenerate tris | **Clean up zero-area triangles from coincident vertices** |
| `CompactMesh` | Remove ID gaps | **Compact after deletions** |
| `RemoveUnusedVertices` | Remove orphaned verts | **Clean up after triangle deletion** |

**Verdict: WeldMeshEdges and RemoveHiddenTriangles alone are worth integrating. Welding fixes cracks, removing hidden tris drops polycount massively for buildings with shared walls.**

---

### 2.6 MeshPolygroupFunctions -- FACE GROUPING SYSTEM

**Header:** `GeometryScript/MeshPolygroupFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `EnablePolygroups` | Enable polygroup layer | **Required for any polygroup operations** |
| `SetPolygroupForMeshSelection` | Assign polygroup to selection | **Tag rooms, walls, floors with group IDs** |
| `ComputePolygroupsFromAngleThreshold` | Auto-group by crease angle | **Auto-detect wall/floor/ceiling groups** |
| `DeleteTrianglesInPolygroup` | Delete all tris in a group | **Delete an entire room's geometry by group** |
| `GetPolygroupIDsInMesh` | List all unique group IDs | **Enumerate rooms/components** |
| `GetTrianglesInPolygroup` | Get all tris for a group | **Get all triangles for a specific room** |
| `GetPolyGroupBoundingBox` | Bounds of a polygroup | **Get room bounds from group** |
| `ConvertComponentsToPolygroups` | Connected islands -> polygroups | **Auto-tag disconnected room meshes** |
| `ConvertUVIslandsToPolygroups` | UV islands -> polygroups | **Group by UV layout** |
| `AddNamedPolygroupLayer` | Named polygroup layers | **Multiple tagging systems: "Room", "WallType", "Floor"** |

**Verdict: Polygroups are the RIGHT way to tag architectural elements. Way better than our current MaterialID-only approach. Named layers let us have Room IDs + WallType + Floor number simultaneously.**

---

### 2.7 MeshVertexColorFunctions -- VERTEX COLOR PAINTING

**Header:** `GeometryScript/MeshVertexColorFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `SetMeshConstantVertexColor` | Set all vertex colors | **Initialize base color** |
| `SetMeshSelectionVertexColor` | Set color on selection | **Paint decay/damage/wear data per-vertex (for shader-driven effects)** |
| `BlurMeshVertexColors` | Smooth vertex colors | **Soft falloff on decay zones** |
| `TransferVertexColorsFromMesh` | Copy colors between meshes | **Transfer paint data when swapping LODs** |
| `GetMeshPerVertexColors` / `SetMeshPerVertexColors` | Batch get/set | **Read back decay data** |

**Note from CLAUDE.md feedback: "NEVER use vertex paint for VISUAL damage (too low res). OK for gameplay data only."**

**Verdict: Useful for gameplay data channels (damage state, AI visibility, decay factor) but NOT visual effects per project rules.**

---

### 2.8 MeshDeformFunctions -- MESH DEFORMATION

**Header:** `GeometryScript/MeshDeformFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplyPerlinNoiseToMesh2` | Perlin noise displacement (selection-aware) | **Terrain deformation, horror organic wall deformation, building decay** |
| `ApplyBendWarpToMesh` | Bend along axis | **Warped/twisted horror architecture** |
| `ApplyTwistWarpToMesh` | Twist along axis | **Spiral staircases, twisted columns** |
| `ApplyFlareWarpToMesh` | Bulge/flare | **Organic horror wall bulging** |
| `ApplyIterativeSmoothingToMesh` | Laplacian smoothing (selection-aware) | **Smooth sharp edges, organic surfaces** |
| `ApplyDisplaceFromTextureMap` | Texture-driven displacement | **Height-map driven surface detail** |
| `ApplyDisplaceFromPerVertexVectors` | Custom per-vertex displacement | **Fully procedural deformation** |
| `ApplyMathWarpToMesh` | Sine wave deformation | **Wavy floors, uneven surfaces** |

**Verdict: Perlin noise + smoothing + texture displacement are strong for horror decay/deformation. The warp functions are niche but useful for twisted horror environments.**

---

### 2.9 MeshSimplifyFunctions -- DECIMATION & LOD

**Header:** `GeometryScript/MeshSimplifyFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplySimplifyToPlanar` | Remove unnecessary tris on flat surfaces | **Massive win -- our proc walls have WAY more tris than needed. This auto-removes them.** |
| `ApplySimplifyToTriangleCount` | Reduce to target tri count (QEM/VolumePreserving/AttributeAware) | **LOD generation for proc meshes** |
| `ApplySimplifyToVertexCount` | Reduce to target vertex count | **LOD generation** |
| `ApplySimplifyToTolerance` | Simplify until max deviation reached | **Quality-controlled LOD** |
| `ApplySimplifyToEdgeLength` | Simplify based on edge length | **Uniform simplification** |
| `ApplySimplifyToPolygroupTopology` | Simplify down to polygroup boundaries | **Ultra-low LOD that preserves room shapes** |
| `ApplyClusterSimplifyToEdgeLength` | Fast cluster-based simplification | **Quick LOD for distant buildings** |

**Verdict: `ApplySimplifyToPlanar` is a must-use. Our box-based walls generate 2 tris per quad face but could be significantly optimized. LOD generation is also critical for city blocks.**

---

### 2.10 MeshSubdivideFunctions -- TESSELLATION

**Header:** `GeometryScript/MeshSubdivideFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplyUniformTessellation` | Subdivide all triangles | **Pre-subdivide walls before deformation** |
| `ApplyPNTessellation` | PN (curved surface) tessellation | **Smooth organic horror surfaces** |
| `ApplySelectiveTessellation` | Subdivide only selected tris | **Subdivide only the wall being deformed, not the whole building** |

**Verdict: Selective tessellation + deformation = horror wall bulging without subdividing the entire mesh.**

---

### 2.11 MeshVoxelFunctions -- VOXEL PROCESSING

**Header:** `GeometryScript/MeshVoxelFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplyMeshSolidify` | Voxel wrap (watertight shell) | **Seal non-watertight buildings for collision** |
| `ApplyMeshMorphology` | Dilate/Contract/Open/Close via SDF | **Erode building edges for decay, thicken thin walls** |

**Verdict: Morphology for decay effects (Contract = erosion). Solidify for generating watertight collision from open meshes.**

---

### 2.12 MeshRemeshFunctions -- UNIFORM REMESHING

**Header:** `GeometryScript/MeshRemeshFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ApplyUniformRemesh` | Isotropic remeshing with many constraint options | **Re-mesh organic horror surfaces to uniform tris for better deformation, fix bad topology from booleans** |

**Verdict: Useful for organic horror elements. Can fix bad mesh topology from boolean operations.**

---

### 2.13 MeshBakeFunctions -- TEXTURE BAKING

**Header:** `GeometryScript/MeshBakeFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `BakeTexture` | Bake normal/AO/curvature/position/height maps between meshes | **Bake AO for proc buildings, bake normal maps from high-res to low-res** |
| `BakeVertex` | Bake to vertex colors | **Bake AO to vertex colors for fast runtime access** |
| `BakeTextureFromRenderCaptures` | Bake from rendered scene captures | **Capture environment lighting onto building textures** |

Bake types include: TangentSpaceNormal, ObjectSpaceNormal, FaceNormal, BentNormal, Position, Curvature, AmbientOcclusion, Texture, MultiTexture, VertexColor, MaterialID, Height, UVShell, Constant.

**Verdict: AO baking is very attractive for proc buildings. Curvature baking could drive edge-wear effects. Height baking useful for height-based material blending.**

---

### 2.14 MeshMaterialFunctions -- MATERIAL ID MANAGEMENT

**Header:** `GeometryScript/MeshMaterialFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `RemapMaterialIDs` | Remap one MaterialID to another | **Fix MaterialIDs after merging buildings** |
| `RemapToNewMaterialIDsByMaterial` | Remap using material reference lists | **Unify materials across merged meshes** |
| `DeleteTrianglesByMaterialID` | Delete all tris with a given MatID | **Remove a material zone (e.g. delete all glass panes)** |
| `CompactMaterialIDs` | Remove gaps in MaterialID space | **Clean up after deletions** |
| `SetMaterialIDForMeshSelection` | Set MatID on a selection | **We probably use this indirectly, but direct use enables selection-based material assignment** |

**Verdict: We may already use some of these. RemapMaterialIDs and CompactMaterialIDs are important for merging buildings.**

---

### 2.15 ContainmentFunctions -- CONVEX HULLS & BOUNDING

**Header:** `GeometryScript/ContainmentFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ComputeMeshConvexHull` | Convex hull (with selection support) | **Generate simple collision for rooms** |
| `ComputeMeshSweptHull` | 2D convex hull swept along axis | **Z-axis swept hull = floor plan collision shape** |
| `ComputeMeshConvexDecomposition` | Approximate with multiple convex hulls | **Complex collision for L-shaped rooms** |
| `ComputeMeshOrientedBox` | Fit oriented bounding box | **Get room OBB for spatial queries** |

**Verdict: Convex decomposition is the right approach for complex room collision. OBB fitting useful for room classification.**

---

### 2.16 CollisionFunctions -- COMPREHENSIVE COLLISION GENERATION

**Header:** `GeometryScript/CollisionFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `SetStaticMeshCollisionFromMesh` | Generate collision from DynamicMesh to StaticMesh | **We already use this -- but note the many generation methods** |
| `GenerateCollisionFromMesh` | Generate standalone collision shapes | **Generate collision without writing to asset** |
| `ComputeNavigableConvexDecomposition` | Convex decomp that respects character navigation | **Collision that guarantees a capsule of MinRadius can navigate** |
| `MergeSimpleCollisionShapes` | Merge shapes respecting negative space | **Optimize collision count** |
| `SimplifyConvexHulls` | Reduce hull face count | **Simpler collision shapes** |
| `ComputeNegativeSpace` | Find negative space spheres | **Identify navigable space inside buildings** |

Generation methods: `AlignedBoxes`, `OrientedBoxes`, `MinimalSpheres`, `Capsules`, `ConvexHulls`, `SweptHulls`, `MinVolumeShapes`, `LevelSets`.

**Verdict: `ComputeNavigableConvexDecomposition` is exactly what we need for proc building collision that guarantees the player capsule can pass through doorways.**

---

### 2.17 MeshGeodesicFunctions -- SURFACE PATHS

**Header:** `GeometryScript/MeshGeodesicFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `GetShortestVertexPath` | Shortest path between vertices on mesh surface | **Route pipes/cables along walls** |
| `GetShortestSurfacePath` | Shortest path between arbitrary surface points | **More precise path routing** |
| `CreateSurfacePath` | "Straight" surface path from a point in a direction | **Project a line along a wall surface** |

**Verdict: Niche but great for pipe/cable routing in horror environments.**

---

### 2.18 MeshSamplingFunctions -- POINT SAMPLING

**Header:** `GeometryScript/MeshSamplingFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `ComputePointSampling` | Poisson-disk surface sampling | **Scatter decals, props, stains on walls** |
| `ComputeNonUniformPointSampling` | Variable-radius sampling | **Dense sampling in detail areas, sparse elsewhere** |
| `ComputeVertexWeightedPointSampling` | Vertex-weight-driven sampling | **More props in high-decay areas** |
| `ComputeRenderCaptureCamerasForBox` | Generate camera positions for baking | **Setup for AO bake** |

**Verdict: Poisson-disk sampling on wall/floor surfaces is a much better scatter algorithm than random placement.**

---

### 2.19 MeshComparisonFunctions -- MESH COMPARISON & INTERSECTION

**Header:** `GeometryScript/MeshComparisonFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `IsSameMeshAs` | Structural comparison with detailed diff | **Validate proc mesh caching -- is the cached mesh still valid?** |
| `MeasureDistancesBetweenMeshes` | Min/max/avg distance metrics | **Measure fit quality between LODs** |
| `IsIntersectingMesh` | Collision test between two meshes | **Check if furniture intersects walls during placement** |

**Verdict: `IsIntersectingMesh` is valuable for furniture/prop placement validation.**

---

### 2.20 MeshSelectionQueryFunctions -- SELECTION ANALYSIS

**Header:** `GeometryScript/MeshSelectionQueryFunctions.h`

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `GetMeshSelectionBoundingBox` | AABB of selected elements | **Get bounds of a selected room** |
| `GetMeshSelectionBoundaryLoops` | Find boundary loops of a selection | **Get the edge loop around a room floor -- needed for extrude/inset operations** |

**Verdict: `GetMeshSelectionBoundaryLoops` is essential for any selection-based modeling workflow.**

---

### 2.21 MeshBasicEditFunctions -- EXTENDED FUNCTIONS WE MISS

We use `AppendMesh` but miss several critical functions:

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `AppendMeshTransformed` | Append with array of transforms | **Place multiple copies of a window frame, railing segment, etc. in one call** |
| `AppendMeshRepeated` | Repeat-append with incremental transform | **Array pattern: fence posts, railing balusters, tile patterns** |
| `AppendMeshWithMaterials` | Append with material list merging | **Correct material handling when combining different building parts** |
| `AppendBuffersToMesh` | Add raw vertex/tri/UV/normal/color buffers | **Insert custom geometry from external sources** |
| `SetVertexPosition` / `SetAllMeshVertexPositions` | Direct vertex position editing | **Vertex snapping to grid, manual vertex adjustment** |
| `DeleteSelectedTrianglesFromMesh` | Delete by selection | **Remove selected wall faces for openings** |
| `MergeMeshVertexPair` | Merge two vertices | **Weld overlapping vertices at wall junctions** |
| `MergeMeshVerticesInSelections` | Batch vertex merge with distance threshold | **Auto-weld all boundary vertices between two selections within tolerance** |

**Verdict: `AppendMeshRepeated` and `AppendMeshTransformed` are huge for repetitive architectural elements. `MergeMeshVerticesInSelections` is the proper way to seal wall joints.**

---

### 2.22 Other Libraries

| Header | Key Functions | Use |
|--------|---------------|-----|
| `MeshSculptLayersFunctions` | `EnableSculptLayers`, `SetActiveSculptLayer`, `SetSculptLayerWeight`, `MergeSculptLayers` | Non-destructive deformation layers (horror decay as layer) |
| `MeshBoneWeightFunctions` | Bone weight editing | Not relevant for buildings |
| `MeshPoolFunctions` | `RequestMesh`, `ReturnMesh`, etc. | Memory management for batch mesh operations |
| `PointSetFunctions` | Point cloud operations | Niche |
| `PolygonFunctions` | 2D polygon operations (offset, simplify, boolean) | **Useful for floor plan manipulation** |
| `PolyPathFunctions` | Path operations | Path generation |
| `ShapeFunctions` | 2D shape generation | **Generate cross-sections for sweep operations** |
| `TextureMapFunctions` | Texture render target operations | Texture generation |
| `VectorMathFunctions` | Vector list math | Utility |
| `SceneUtilityFunctions` | Copy mesh from components | Asset conversion |
| `ListUtilityFunctions` | List operations | Utility |
| `VolumeTextureBakeFunctions` | 3D texture baking | Niche |
| `MeshUVFunctions` | We use `SetMeshUVsFromBoxProjection` but miss: `SetMeshUVsFromPlanarProjection`, `SetMeshUVsFromCylinderProjection`, `AutoGeneratePatchBasedUVs`, `AutoGenerateXAtlasUVs`, `RepackMeshUVs`, `CopyUVSet`, `TransformMeshUVs`, `SetMeshTriangleUVs` | Better UV control for non-box surfaces |

---

## 3. Lower-Level GeometryProcessing Operations {#3-lower-level-ops}

**Module:** `GeometryProcessing` plugin, `DynamicMesh` module
**Header Path:** `Plugins/Runtime/GeometryProcessing/Source/DynamicMesh/Public/Operations/`

These are C++ classes (not Blueprint-exposed) that provide more control than GeometryScript wrappers.

### 3.1 Core Operations We Can Use Directly

| Header | Class | What It Does | Building Gen Use |
|--------|-------|--------------|------------------|
| `MeshPlaneCut.h` | `FMeshPlaneCut` | Slice mesh along a plane, optionally fill the cut | **Slice walls at door positions, floor cuts for stairwells -- direct plane cut is more controlled than boolean** |
| `InsetMeshRegion.h` | `FInsetMeshRegion` | Inset a region of faces | **Window recesses, door frames -- lower-level than GeometryScript with more control** |
| `OffsetMeshRegion.h` | `FOffsetMeshRegion` | Offset a region of faces | **Push wall faces in/out -- lower-level control** |
| `ExtrudeMesh.h` | `FExtrudeMesh` | Extrude faces along direction | **Full extrude control** |
| `ExtrudeBoundaryEdges.h` | `FExtrudeBoundaryEdges` | Extrude boundary edges into faces | **Turn an open edge into a wall/fin -- great for trim pieces** |
| `MeshBevel.h` | `FMeshBevel` | Bevel edges/polygroup edges | **Direct bevel control** |
| `MeshMirror.h` | `FMeshMirror` | Mirror mesh across a plane | **Mirror symmetric buildings** |
| `JoinMeshLoops.h` | `FJoinMeshLoops` | Join two edge loops with triangulation | **Connect separate mesh sections** |
| `GroupEdgeInserter.h` | `FGroupEdgeInserter` | Insert edges along polygroup boundaries | **Insert edge loops at polygroup boundaries for subsequent operations** |
| `MeshIsoCurves.h` | `FMeshIsoCurves` | Extract iso-curves from scalar field | **Extract contour lines on terrain** |
| `FFDLattice.h` | `FFFDLattice` | Free-form deformation lattice | **Deform rooms with lattice for warped horror architecture** |

### 3.2 Hole Filling Operations

| Header | Class | What It Does |
|--------|-------|--------------|
| `HoleFiller.h` | `FHoleFiller` | General hole filler |
| `MinimalHoleFiller.h` | `FMinimalHoleFiller` | Minimal triangulation hole fill |
| `PlanarHoleFiller.h` | `FPlanarHoleFiller` | Fill holes in a plane |
| `SimpleHoleFiller.h` | `FSimpleHoleFiller` | Triangle fan hole fill |
| `SmoothHoleFiller.h` | `FSmoothHoleFiller` | Smooth surface hole fill |

### 3.3 Repair Operations

| Header | Class | What It Does |
|--------|-------|--------------|
| `MeshResolveTJunctions.h` | `FMeshResolveTJunctions` | Fix T-junctions |
| `RepairOrientation.h` | `FRepairOrientation` | Fix inconsistent triangle orientation |
| `RemoveOccludedTriangles.h` | `FRemoveOccludedTriangles` | Remove hidden geometry |
| `WeldEdgeSequence.h` | `FWeldEdgeSequence` | Weld edges sequentially |
| `MergeCoincidentMeshEdges.h` | `FMergeCoincidentMeshEdges` | Merge overlapping edges |
| `PlanarFlipsOptimization.h` | `FPlanarFlipsOptimization` | Optimize planar triangulation |
| `DetectExteriorVisibility.h` | `FDetectExteriorVisibility` | Detect visible vs occluded surfaces |

### 3.4 Other Notable Operations

| Header | Class | What It Does | Building Gen Use |
|--------|-------|--------------|------------------|
| `MeshRegionOperator.h` | `FMeshRegionOperator` | Extract, edit, and reinsert mesh regions | **Edit a room independently then put it back -- the "submesh editing" workflow** |
| `MeshAttributeTransfer.h` | `FMeshAttributeTransfer` | Transfer attributes between meshes | **Transfer UVs/normals between LODs** |
| `MeshConvexHull.h` | `FMeshConvexHull` | Direct convex hull computation | **Lower-level hull access** |
| `MeshProjectionHull.h` | `FMeshProjectionHull` | 2D swept hull | **Floor plan extraction** |
| `GroupTopologyDeformer.h` | `FGroupTopologyDeformer` | Deform mesh respecting polygroup structure | **Deform rooms while keeping walls flat** |
| `WrapMesh.h` | `FWrapMesh` | Voxel wrap mesh | **Generate watertight collision** |
| `EmbedSurfacePath.h` | `FEmbedSurfacePath` | Embed a path into mesh surface (insert edges along path) | **Insert edge loops at specific wall positions for window/door cuts** |
| `IntrinsicCorrespondenceUtils.h` | Various | Mesh correspondence | **Transfer data between mesh versions** |
| `PolygroupRemesh.h` | `FPolygroupRemesh` | Remesh respecting polygroups | **Remesh while keeping polygroup boundaries** |
| `QuadGridPatchUtil.h` | Various | Quad grid operations | **Generate quad-dominant wall patches** |

---

## 4. FDynamicMeshEditor -- Direct Mesh API {#4-fdynamicmesheditor}

**Header:** `GeometryCore/Public/DynamicMeshEditor.h`
**Module:** `GeometryCore` (Runtime)

This is the lowest-level mesh editing API, operating directly on `FDynamicMesh3`.

| Function | What It Does | Building Gen Use |
|----------|--------------|------------------|
| `StitchVertexLoopsMinimal` | Stitch two vertex loops with quad-strip | **Connect wall edges, join rooms, create transitions** |
| `WeldVertexLoops` | Weld two loops (eliminates Loop2 verts) | **Merge boundary loops of adjacent rooms** |
| `DuplicateTriangles` | Duplicate tris with full mapping | **Duplicate wall section, then offset for frame** |
| `DisconnectTriangles` | Disconnect tris from surrounding mesh | **Isolate a room's geometry** |
| `DisconnectTrianglesAlongEdges` | Disconnect along specific edges | **Split mesh at door/window positions** |
| `AddTriangleFan_OrderedVertexLoop` | Fill hole with fan from center vertex | **Simple cap for openings** |
| `RemoveTriangles` | Remove tris with isolated vert cleanup | **Delete wall faces for openings** |
| `RemoveSmallComponents` | Remove tiny disconnected islands | **Boolean cleanup** |
| `ReverseTriangleOrientations` | Flip triangle winding | **Fix normals on interior faces** |
| `SplitBowties` | Fix bowtie vertices | **Topology repair** |
| `ReinsertSubmesh` | Re-insert edited submesh back into base mesh | **The "edit region and put it back" workflow** |
| `StitchSparselyCorrespondedVertexLoops` | Stitch loops with sparse correspondence | **Handle variable-vertex-count boundaries** |

---

## 5. GeometryCore Generators {#5-generators}

**Path:** `GeometryCore/Public/Generators/`

| Generator | What It Does | Building Gen Use |
|-----------|--------------|------------------|
| `FGridBoxMeshGenerator` | Box with subdivisions per face | **Subdivided walls for deformation** |
| `FMinimalBoxMeshGenerator` | 12-tri box | **Ultra-simple room boxes** |
| `FCapsuleGenerator` | Capsule mesh | **Pipe segments, columns** |
| `FSphereGenerator` / `FBoxSphereGenerator` | Sphere variants | **Decorative elements, horror organic shapes** |
| `FDiscMeshGenerator` | Flat disc mesh | **Ceiling medallions, floor drains** |
| `FRectangleMeshGenerator` | Flat rectangle with subdivisions | **Floor planes, wall panels** |
| `FPlanarPolygonMeshGenerator` | Triangulated polygon | **Arbitrary floor plan shapes** |
| `FPolygonEdgeMeshGenerator` | Extruded polygon edges | **Wall outlines, baseboards** |
| `FRevolveGenerator` | Revolution surface from profile | **Columns, balusters, decorative elements** |
| `FSweepGenerator` | Sweep profile along path | **Cornices, moldings, trim -- this is what we use via GeometryScript but the direct API has more control** |
| `FStairGenerator` | Stair geometry | **Direct stair generation (more control than AppendLinearStairs)** |
| `FMarchingCubes` | Marching cubes isosurface | **Generate organic horror geometry from implicit functions** |
| `FLineSegmentGenerators` | Line segment mesh | **Wireframe debug visualization** |
| `FFlatTriangulationMeshGenerator` | Triangulate flat polygon | **Floor/ceiling fills** |

---

## 6. Mesh Component & Runtime Options {#6-mesh-components}

### 6.1 UDynamicMeshComponent (what we use)
- Full GeometryScript support, FDynamicMesh3 internally
- Editor + runtime

### 6.2 UProceduralMeshComponent
- Simpler API, direct vertex/tri buffers
- No GeometryScript support
- Runtime only, limited collision
- **Verdict: Inferior to DynamicMesh for our use case**

### 6.3 GeometryCollection (Chaos Destruction)
- Fracture geometry support
- Physics-driven destruction
- **Verdict: Could be useful for destructible horror elements but is a different system**

### 6.4 UOctreeDynamicMeshComponent
- Like UDynamicMeshComponent but tracks dirty chunks via octree
- Only updates changed regions
- **Verdict: Better for large meshes with localized edits (e.g. damage to one room)**

---

## 7. Third-Party & Integration {#7-third-party}

### 7.1 libigl
- Gradientspace (Ryan Schmidt, the UE modeling tools author) created `UnrealMeshProcessingTools` which integrates libigl with UE4
- License: MPL-2.0 (commercially safe)
- Provides: parameterization, deformation, geodesics, boolean ops
- **Verdict: Not needed -- UE5's native APIs now cover most of what libigl provides**

### 7.2 CGAL
- Heavyweight computational geometry library
- License: GPL/LGPL (problematic for commercial use)
- **Verdict: License issues, and UE5 native APIs are sufficient**

### 7.3 OpenMesh
- BSD-licensed half-edge mesh library
- **Verdict: FDynamicMesh3 is already a half-edge mesh -- no benefit**

### 7.4 PCG + GeometryScript Interop
- UE 5.7 has `PCGGeometryScriptInterop` plugin
- Allows PCG graphs to use GeometryScript operations
- **Verdict: Worth investigating if we move to PCG for city-block generation**

---

## 8. Priority Matrix for Building Generation {#8-priority-matrix}

### TIER 1 -- Integrate Immediately (Transformative Impact)

| API | Impact | Effort | Why |
|-----|--------|--------|-----|
| **MeshSelectionFunctions** (full library) | Critical | Low | Foundation for ALL targeted operations. Select by normal, material, plane, box. |
| **ApplyMeshLinearExtrudeFaces** | Critical | Low | Window frames, cornices, pilasters WITHOUT booleans |
| **ApplyMeshInsetOutsetFaces** | Critical | Low | Window recesses, door frames WITHOUT booleans |
| **ApplySimplifyToPlanar** | High | Low | Drop tri count 30-50% on flat surfaces for free |
| **WeldMeshEdges** | High | Low | Fix cracks between wall segments |
| **RemoveHiddenTriangles** | High | Low | Remove shared interior walls -- massive polycount savings |
| **MergeMeshVerticesInSelections** | High | Low | Auto-weld adjacent rooms at boundaries |

### TIER 2 -- High Value (Significant Quality Improvement)

| API | Impact | Effort | Why |
|-----|--------|--------|-----|
| **MeshPolygroupFunctions** (full library) | High | Medium | Proper architectural element tagging system |
| **ApplyMeshBevelEdgeSelection** | High | Medium | Realistic rounded edges on all architectural elements |
| **FillAllMeshHoles** | High | Low | Auto-fix holes from boolean ops |
| **AppendMeshRepeated** | High | Low | Fence posts, balusters, window patterns in one call |
| **GetMeshSelectionBoundaryLoops** | High | Low | Required for advanced selection-based workflows |
| **SplitMeshByMaterialIDs** | Medium | Low | Separate building into material-specific meshes |
| **ComputeNavigableConvexDecomposition** | High | Medium | Collision that guarantees player navigation |
| **FindNearestPointOnMesh + BVH** | Medium | Low | Proper furniture snapping to surfaces |
| **IsIntersectingMesh** | Medium | Low | Furniture placement validation |

### TIER 3 -- Nice To Have (Quality Polish)

| API | Impact | Effort | Why |
|-----|--------|--------|-----|
| **ApplyPerlinNoiseToMesh2** | Medium | Low | Horror decay deformation |
| **ApplyIterativeSmoothingToMesh** | Medium | Low | Smooth sharp edges |
| **ApplySelectiveTessellation** | Medium | Low | Pre-subdivide before deformation |
| **BakeTexture (AO)** | Medium | Medium | Baked AO on proc buildings |
| **ComputePointSampling** | Medium | Low | Better scatter algorithms |
| **ApplyMeshMorphology** | Medium | Medium | Erosion/dilation for decay |
| **FMeshPlaneCut** (direct C++) | Medium | Medium | More controlled cuts than boolean |
| **FExtrudeBoundaryEdges** | Medium | Medium | Turn open edges into trim pieces |
| **FEmbedSurfacePath** | Medium | Medium | Insert edge loops at arbitrary positions |
| **FRevolveGenerator** | Low | Low | Columns, balusters |
| **MeshSculptLayersFunctions** | Low | Medium | Non-destructive deformation |
| **FFFDLattice** | Low | Medium | Free-form deformation for horror |

### TIER 4 -- Future / Specialty

| API | Impact | Effort | Why |
|-----|--------|--------|-----|
| **ApplyUniformRemesh** | Low | Medium | Fix bad topology from booleans |
| **FMeshRegionOperator** | Low | High | Advanced submesh editing workflow |
| **FMarchingCubes** | Low | High | Organic horror geometry |
| **PCG + GeometryScript Interop** | Future | High | City-block-scale generation |
| **FGroupTopologyDeformer** | Low | Medium | Deform while preserving structure |

---

## Key Architectural Insight: The Selection + Operation Pipeline

The biggest gap in our current approach is that we lack a **selection** system. Every operation we perform is on an entire mesh. The correct workflow is:

```
1. Generate base geometry (AppendBox, AppendSweptPolygon, etc.)
2. TAG elements (SetPolygroupForMeshSelection with named layers)
3. SELECT elements (by material, normal, plane, box, polygroup)
4. OPERATE on selection (extrude, inset, bevel, delete, offset)
5. REPAIR (weld edges, fill holes, remove hidden, simplify planar)
```

This is how the Modeling Tools UI works internally. We should mirror this pipeline.

### Current Workflow (boolean-heavy):
```
Wall mesh + Boolean cutter = Wall with hole
Wall with hole + Frame mesh append = Wall with frame
```

### Proposed Workflow (selection-based):
```
1. Wall mesh (with polygroup: "WallFace")
2. SelectMeshElementsInBox() to select window region
3. ApplyMeshInsetOutsetFaces() to create window recess
4. SelectMeshElementsByNormalAngle() to select recessed face
5. DeleteSelectedTrianglesFromMesh() for glass opening
6. GetMeshSelectionBoundaryLoops() to get the opening edge loop
7. FExtrudeBoundaryEdges to create frame from the opening edge
```

Result: No booleans. Clean topology. Proper UVs. Half the triangle count.

---

## Sources

- [UE 5.7 GeometryScript Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-reference-in-unreal-engine)
- [UE 5.7 GeometryScript User Guide](https://dev.epicgames.com/documentation/en-us/unreal-engine/geometry-scripting-users-guide-in-unreal-engine)
- [FDynamicMeshEditor API Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/GeometryCore/FDynamicMeshEditor)
- [Unofficial GeometryScript FAQ (gradientspace)](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq)
- [Interactive Tools Framework (gradientspace)](http://www.gradientspace.com/tutorials/2022/6/1/the-interactive-tools-framework-in-ue5)
- [UE5 Procedural Building (bendemott)](https://github.com/bendemott/UE5-Procedural-Building)
- [Experimental Modeling Tools (ryanschmidtEpic)](https://github.com/ryanschmidtEpic/ExperimentalModelingTools)
- Engine source headers read directly from `C:\Program Files (x86)\UE_5.7\Engine\`

---

## Estimated Impact

If we integrate Tier 1 + Tier 2 APIs:
- **Triangle count reduction:** 30-50% from `ApplySimplifyToPlanar` + `RemoveHiddenTriangles`
- **Boolean operations eliminated:** 60-80% replaced by Inset/Extrude/Delete selection workflows
- **Topology quality:** Significantly better (no boolean artifacts)
- **UV quality:** Better (inset/extrude preserve UVs better than booleans)
- **Detail capability:** Bevels, cornices, recessed panels become trivial
- **Development effort:** ~40-60h for Tier 1+2 integration
