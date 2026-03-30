# Auto-Collision for Procedural Meshes — Research

**Date:** 2026-03-28
**Status:** Research complete, ready for implementation
**Scope:** MonolithMesh save_handle, generate_collision, and all procedural create_* actions

---

## Problem Statement

Every procedural mesh created by MonolithMesh (create_parametric_mesh, create_structure, create_maze, etc.) saves with **zero collision**. Players walk through everything. Two bugs compound:

1. **`save_handle`** explicitly sets `bBuildSimpleCollision = false` (line 207 of MonolithMeshHandlePool.cpp)
2. **`generate_collision`** computes collision shapes via GeometryScript but **discards the result** — never stores it on the mesh or handle (lines 474-484 of MonolithMeshOperationActions.cpp)

---

## Engine API Reference (UE 5.7, verified from source)

### 1. ECollisionTraceFlag (BodySetupEnums.h)

```cpp
enum ECollisionTraceFlag : int
{
    CTF_UseDefault,            // Use project physics settings
    CTF_UseSimpleAndComplex,   // Both simple + complex (per-poly for complex queries)
    CTF_UseSimpleAsComplex,    // Simple shapes only for ALL queries
    CTF_UseComplexAsSimple,    // Per-poly mesh for ALL queries (static only, no forces)
    CTF_MAX,
};
```

### 2. UStaticMesh::CreateBodySetup() (StaticMesh.cpp:9366)

```cpp
void UStaticMesh::CreateBodySetup()
{
    if (GetBodySetup() == nullptr)
    {
        UBodySetup* NewBodySetup = nullptr;
        {
            FGCScopeGuard Scope;
            NewBodySetup = NewObject<UBodySetup>(this);
        }
        NewBodySetup->DefaultInstance.SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        NewBodySetup->bSupportUVsAndFaceRemap = bSupportPhysicalMaterialMasks;
        SetBodySetup(NewBodySetup);
    }
}
```

Key: Creates a new `UBodySetup` outer'd to the StaticMesh, sets `BlockAll` profile. **Idempotent** — won't overwrite existing.

### 3. FBuildMeshDescriptionsParams bBuildSimpleCollision (StaticMesh.cpp:8879)

When `bBuildSimpleCollision = true`, the engine creates a **bounding-box collision** automatically:

```cpp
if (Params.bBuildSimpleCollision)
{
    FKBoxElem BoxElem;
    BoxElem.Center = GetRenderData()->Bounds.Origin;
    BoxElem.X = GetRenderData()->Bounds.BoxExtent.X * 2.0f;
    BoxElem.Y = GetRenderData()->Bounds.BoxExtent.Y * 2.0f;
    BoxElem.Z = GetRenderData()->Bounds.BoxExtent.Z * 2.0f;
    GetBodySetup()->AggGeom.BoxElems.Add(BoxElem);
    GetBodySetup()->CreatePhysicsMeshes();
}
```

This is a **single AABB box**. Adequate for cubes, terrible for L-shapes, hallways, mazes, stairs.

### 4. UBodySetup (BodySetup.h)

Key members:
- `FKAggregateGeom AggGeom` — the simple collision geometry container
- `ECollisionTraceFlag CollisionTraceFlag` — how collision is traced (inherited from UBodySetupCore)
- `void CreatePhysicsMeshes()` — cooks the physics data from AggGeom
- `void InvalidatePhysicsData()` — clears cooked data
- `void RemoveSimpleCollision()` — clears AggGeom + invalidates
- `void AddCollisionFrom(const FKAggregateGeom&)` — appends collision elements

### 5. FKAggregateGeom (AggregateGeom.h)

```cpp
struct FKAggregateGeom
{
    TArray<FKSphereElem> SphereElems;
    TArray<FKBoxElem> BoxElems;
    TArray<FKSphylElem> SphylElems;       // Capsules
    TArray<FKConvexElem> ConvexElems;
    TArray<FKTaperedCapsuleElem> TaperedCapsuleElems;
    TArray<FKLevelSetElem> LevelSetElems;
    TArray<FKSkinnedLevelSetElem> SkinnedLevelSetElems;
    TArray<FKMLLevelSetElem> MLLevelSetElems;
    TArray<FKSkinnedTriangleMeshElem> SkinnedTriangleMeshElems;
};
```

### 6. GeometryScript Collision API (CollisionFunctions.h)

Full API exists in `UGeometryScriptLibrary_CollisionFunctions`. Already linked via `GeometryScriptingCore`. Key functions:

| Function | Purpose |
|----------|---------|
| `SetStaticMeshCollisionFromMesh()` | **THE function we want.** Takes DynamicMesh + StaticMesh, generates collision from mesh and applies it directly. |
| `GenerateCollisionFromMesh()` | Generates collision as `FGeometryScriptSimpleCollision` struct (what `generate_collision` calls but discards) |
| `SetSimpleCollisionOfStaticMesh()` | Sets pre-built collision on a StaticMesh (for custom collision) |
| `ComputeNavigableConvexDecomposition()` | Advanced: character-size-aware decomposition |

### 7. FGeometryScriptCollisionFromMeshOptions

```cpp
struct FGeometryScriptCollisionFromMeshOptions
{
    bool bEmitTransaction = true;
    EGeometryScriptCollisionGenerationMethod Method = MinVolumeShapes;
    bool bAutoDetectSpheres = true;
    bool bAutoDetectBoxes = true;
    bool bAutoDetectCapsules = true;
    float MinThickness = 1.0;
    bool bSimplifyHulls = true;
    int ConvexHullTargetFaceCount = 25;
    int MaxConvexHullsPerMesh = 1;
    float ConvexDecompositionSearchFactor = .5f;
    float ConvexDecompositionErrorTolerance = 0;
    float ConvexDecompositionMinPartThickness = 0.1;
    float SweptHullSimplifyTolerance = 0.1;
    EGeometryScriptSweptHullAxis SweptHullAxis = Z;
    bool bRemoveFullyContainedShapes = true;
    int MaxShapeCount = 0;
};
```

Generation methods: `AlignedBoxes`, `OrientedBoxes`, `MinimalSpheres`, `Capsules`, `ConvexHulls`, `SweptHulls`, `MinVolumeShapes`, `LevelSets`

### 8. How SetStaticMeshCollisionFromMesh Works (CollisionFunctions.cpp:404)

```cpp
UDynamicMesh* SetStaticMeshCollisionFromMesh(
    UDynamicMesh* FromDynamicMesh,
    UStaticMesh* ToStaticMeshAsset,
    FGeometryScriptCollisionFromMeshOptions Options, ...)
{
    FKAggregateGeom NewCollision;
    FromDynamicMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh) {
        ComputeCollisionFromMesh(ReadMesh, NewCollision, Options);
    });
    SetStaticMeshSimpleCollision(ToStaticMeshAsset, NewCollision, ...);
    return FromDynamicMesh;
}
```

Internally, `SetStaticMeshSimpleCollision` does:
1. `BodySetup->RemoveSimpleCollision()` — clears old collision + invalidates
2. `BodySetup->AggGeom = NewSimpleCollision` — assigns new geometry
3. `BodySetup->CreatePhysicsMeshes()` — cooks the physics data
4. `StaticMesh->RecreateNavCollision()` — rebuilds nav mesh collision
5. Iterates all StaticMeshComponents using this mesh and calls `RecreatePhysicsState()`

---

## Root Cause Analysis

### Bug 1: save_handle never generates collision

```cpp
// MonolithMeshHandlePool.cpp:207
BuildParams.bBuildSimpleCollision = false;  // <-- THE BUG
```

Even setting this to `true` would only produce a single bounding box. Not useful for concave geometry.

### Bug 2: generate_collision throws away results

```cpp
// MonolithMeshOperationActions.cpp:474-484
FGeometryScriptSimpleCollision Collision = ...::GenerateCollisionFromMesh(Mesh, CollisionOpts);
// ... result is NEVER stored anywhere, just reported as "generated"
```

The collision is computed, reported to the user, and garbage collected.

---

## Recommended Fix: Three-Part Solution

### Part A: Auto-collision in save_handle (centralized fix)

**Where:** `UMonolithMeshHandlePool::SaveHandle()`
**Strategy:** After `BuildFromMeshDescriptions`, call `SetStaticMeshCollisionFromMesh` to auto-generate collision.

```cpp
// After BuildFromMeshDescriptions(MeshDescs, BuildParams) on line 210:

// Auto-generate collision from the mesh geometry
FGeometryScriptCollisionFromMeshOptions CollisionOpts;
CollisionOpts.bEmitTransaction = false;
CollisionOpts.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
CollisionOpts.MaxConvexHullsPerMesh = 4;
CollisionOpts.bSimplifyHulls = true;
CollisionOpts.ConvexHullTargetFaceCount = 25;

UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromMesh(
    DynMesh, StaticMesh, CollisionOpts);
```

**Benefits:**
- Fixes ALL procedural meshes at once (create_parametric_mesh, create_structure, create_maze, etc.)
- Single code change
- Every saved mesh gets collision automatically

**New params for save_handle:**
- `collision` (string, optional, default `"auto"`) — `"auto"`, `"box"`, `"convex"`, `"complex_as_simple"`, `"none"`
- `max_hulls` (integer, optional, default `4`) — for convex decomposition

### Part B: Fix generate_collision to actually store results

**Where:** `FMonolithMeshOperationActions::GenerateCollision()`
**Strategy:** Store the `FGeometryScriptSimpleCollision` on the handle pool, apply when saving.

Two approaches:
1. **Apply immediately to all StaticMesh assets loaded from this handle** — complex, handle is UDynamicMesh not StaticMesh
2. **Store collision data alongside the handle, apply at save time** — cleaner

Recommend option 2: add a `TMap<FString, FGeometryScriptSimpleCollision> HandleCollision` to the pool. When `save_handle` runs, check if collision was pre-generated; if so, apply it instead of auto-generating.

### Part C: Per-shape collision strategy selection

Different proc gen shapes need different collision:

| Shape Type | Best Collision Strategy | Why |
|-----------|------------------------|-----|
| Box/cube rooms | `AlignedBoxes` or bBuildSimpleCollision AABB | Perfect fit, cheapest |
| Walls (thin) | `SweptHulls` along Z | Preserves thin geometry |
| Stairs | `ConvexHulls` (max_hulls=8-12) | Stair steps need multiple hulls |
| L-shaped hallways | `ConvexHulls` (max_hulls=4-6) | Concave, needs decomposition |
| Mazes | `CTF_UseComplexAsSimple` | Complex geometry, many thin walls |
| Cylinders/pillars | `MinimalSpheres` or `Capsules` | Natural fit |
| Furniture (chairs/tables) | `ConvexHulls` (max_hulls=4) | Reasonable approximation |
| Terrain/ground | `CTF_UseComplexAsSimple` | Complex surface |

---

## Approach Comparison: Where To Hook

### Option 1: save_handle only (RECOMMENDED)

**Pros:**
- Single code path, all meshes get collision
- Default "auto" mode picks ConvexHulls which works for 80% of cases
- Optional `collision` param lets callers customize per-shape
- No need to change any create_* actions

**Cons:**
- Auto-detect may pick suboptimal strategy for some shapes
- Small perf cost at save time

### Option 2: Each create_* action individually

**Pros:**
- Each action knows its geometry and can pick optimal collision

**Cons:**
- Must update every create_* action (create_parametric_mesh, create_structure, create_maze, create_room_layout, create_pipe_network, etc.)
- Collision generated on DynamicMesh but not saved until save_handle — same storage problem
- Future actions would need to remember to add collision too

### Option 3: Hybrid (BEST)

save_handle auto-generates collision by default (Option 1), but create_* actions can pre-generate and store collision on the handle pool. save_handle checks for pre-stored collision first.

This way:
- All meshes get collision (Option 1 fallback)
- Specific actions can override with optimal strategy (Option 2 when needed)
- `generate_collision` action works for manual override by users

---

## Performance Analysis

### UseComplexAsSimple (per-poly collision)

- **Physics cost:** Uses the render mesh triangles for collision. For a 10K tri mesh, physics must test against all triangles.
- **NavMesh cost:** NavMesh generation iterates every triangle. A maze with 50K tris = slow navmesh build.
- **Memory:** No additional memory (reuses render data).
- **Good for:** Static geometry that won't move. Ground/terrain. Mazes where precise collision matters.
- **Bad for:** Anything that moves. High-poly meshes. Many instances.
- **Verdict:** Viable for proc gen blockout/prototyping where precision matters. Not recommended for final gameplay meshes.

### Convex Decomposition (ConvexHulls, max_hulls=4)

- **Physics cost:** 4 convex shapes tested per object. Very fast.
- **NavMesh cost:** 4 convex hulls processed. Fast.
- **Memory:** ~1-5KB per mesh for hull vertex/index data.
- **Generation time:** 10-50ms for typical proc gen mesh (tested: GeometryScript uses FMeshSimpleShapeApproximation internally).
- **Good for:** Most proc gen shapes. Default recommendation.
- **Bad for:** Thin walls that get "filled in" by convex hulls. Very complex concave shapes.

### AABB Box (bBuildSimpleCollision)

- **Physics cost:** Single box test. Cheapest possible.
- **NavMesh cost:** Single box. Instant.
- **Memory:** ~100 bytes.
- **Good for:** Cubes, rectangular rooms.
- **Bad for:** Everything else — players can walk into concave areas.

### Recommendation for Default

**ConvexHulls with max_hulls=4, bSimplifyHulls=true, ConvexHullTargetFaceCount=25**

This is the GeometryScript default `MinVolumeShapes` behavior but with decomposition. Handles:
- Rooms (walls decompose into separate hulls)
- Furniture (reasonable approximation)
- Corridors (convex pieces fit well)
- Stairs (multiple hulls approximate steps)

For mazes specifically, the auto-collision should use higher max_hulls (8-16) or complex-as-simple.

---

## Implementation Plan

### Phase 1: Fix save_handle (critical, ~30 min)

1. Add `collision` and `max_hulls` params to save_handle registration
2. In `SaveHandle()`, after `BuildFromMeshDescriptions`:
   - If collision == "none": do nothing (current behavior)
   - If collision == "box": set `bBuildSimpleCollision = true` in BuildParams
   - If collision == "convex" (default/auto): call `SetStaticMeshCollisionFromMesh`
   - If collision == "complex_as_simple": set `CollisionTraceFlag = CTF_UseComplexAsSimple` on BodySetup
3. Default to "convex" for auto collision

### Phase 2: Fix generate_collision (important, ~45 min)

1. Add `TMap<FString, FGeometryScriptSimpleCollision> HandleCollisionData` to pool
2. In `GenerateCollision()`, store result: `HandleCollisionData.Add(HandleName, Collision)`
3. In `SaveHandle()`, check HandleCollisionData first before auto-generating
4. In `ReleaseHandle()`, clean up stored collision data
5. Report collision shape count in generate_collision response

### Phase 3: Smart defaults per action (nice-to-have, ~1 hr)

1. create_maze: default collision="complex_as_simple" (too many thin walls for convex)
2. create_structure: default collision="convex" max_hulls=8
3. create_parametric_mesh: default collision="convex" max_hulls=4
4. create_room_layout: default collision="convex" max_hulls=6
5. create_pipe_network: default collision="convex" max_hulls=8

These would be hints passed through to save_handle or pre-generated via the pool.

---

## Required Include Changes

For MonolithMeshHandlePool.cpp, add:
```cpp
#include "GeometryScript/CollisionFunctions.h"
#include "PhysicsEngine/BodySetup.h"
```

Both modules already linked in MonolithMesh.Build.cs:
- `GeometryScriptingCore` (conditional, WITH_GEOMETRYSCRIPT)
- `PhysicsCore` (already linked)

No Build.cs changes needed.

---

## Code Sketch: save_handle Fix

```cpp
bool UMonolithMeshHandlePool::SaveHandle(
    const FString& HandleName, const FString& TargetPath,
    bool bOverwrite, const FString& CollisionMode, int32 MaxHulls,
    FString& OutError)
{
    // ... existing code up to BuildFromMeshDescriptions ...

    UStaticMesh::FBuildMeshDescriptionsParams BuildParams;
    BuildParams.bMarkPackageDirty = true;
    BuildParams.bBuildSimpleCollision = false; // We handle collision ourselves
    BuildParams.bCommitMeshDescription = true;
    StaticMesh->BuildFromMeshDescriptions(MeshDescs, BuildParams);

    // --- NEW: Auto-generate collision ---
    if (CollisionMode != TEXT("none"))
    {
        // Ensure BodySetup exists
        StaticMesh->CreateBodySetup();
        UBodySetup* BS = StaticMesh->GetBodySetup();
        check(BS);

        if (CollisionMode == TEXT("complex_as_simple"))
        {
            BS->CollisionTraceFlag = CTF_UseComplexAsSimple;
            BS->CreatePhysicsMeshes();
        }
        else if (CollisionMode == TEXT("box"))
        {
            FKBoxElem BoxElem;
            BoxElem.Center = StaticMesh->GetRenderData()->Bounds.Origin;
            BoxElem.X = StaticMesh->GetRenderData()->Bounds.BoxExtent.X * 2.0f;
            BoxElem.Y = StaticMesh->GetRenderData()->Bounds.BoxExtent.Y * 2.0f;
            BoxElem.Z = StaticMesh->GetRenderData()->Bounds.BoxExtent.Z * 2.0f;
            BS->AggGeom.BoxElems.Add(BoxElem);
            BS->CreatePhysicsMeshes();
        }
        else // "auto" or "convex"
        {
            // Check for pre-generated collision from generate_collision action
            if (FGeometryScriptSimpleCollision* PreGenerated = HandleCollisionData.Find(HandleName))
            {
                UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfStaticMesh(
                    *PreGenerated, StaticMesh,
                    FGeometryScriptSetSimpleCollisionOptions{false});
            }
            else
            {
                // Auto-generate from mesh geometry
                FGeometryScriptCollisionFromMeshOptions Opts;
                Opts.bEmitTransaction = false;
                Opts.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
                Opts.MaxConvexHullsPerMesh = FMath::Max(1, MaxHulls);
                Opts.bSimplifyHulls = true;
                Opts.ConvexHullTargetFaceCount = 25;

                UGeometryScriptLibrary_CollisionFunctions::SetStaticMeshCollisionFromMesh(
                    DynMesh, StaticMesh, Opts);
            }
        }
    }

    // ... existing save code ...
}
```

---

## Estimated Effort

| Phase | Time | Impact |
|-------|------|--------|
| Phase 1: Fix save_handle | 30 min | ALL proc gen meshes get collision |
| Phase 2: Fix generate_collision | 45 min | Manual collision override works |
| Phase 3: Smart defaults | 1 hr | Optimal collision per shape type |
| **Total** | **~2.25 hrs** | |

---

## Key Findings Summary

1. **GeometryScript has a complete collision API** — `SetStaticMeshCollisionFromMesh()` is the single function that does everything we need: generates collision from DynamicMesh, applies to StaticMesh, cooks physics, rebuilds nav collision, updates components.

2. **No new module dependencies required** — `GeometryScriptingCore` and `PhysicsCore` already linked.

3. **The fix is centralized in save_handle** — one code change fixes all 187 mesh actions. No need to touch individual create_* actions.

4. **ConvexHulls with max_hulls=4 is the best default** — works for rooms, furniture, corridors, stairs. Users can override via params.

5. **UseComplexAsSimple is viable for mazes** — complex geometry with many thin walls where convex decomposition fails. Set via `BodySetup->CollisionTraceFlag = CTF_UseComplexAsSimple`.

6. **generate_collision exists but is broken** — computes collision then discards the result. Needs a storage mechanism in the handle pool.

7. **NavigableConvexDecomposition is available** for character-size-aware collision if needed later (`ComputeNavigableConvexDecomposition` with MinRadius for player capsule).
