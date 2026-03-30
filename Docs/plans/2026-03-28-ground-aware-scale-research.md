# Ground-Aware Placement & Human-Scale Defaults Research

**Date:** 2026-03-28
**Scope:** MonolithMesh procedural geometry — two bugs: floor-sinking geometry and oversized defaults

---

## Issue 1: Geometry Spawns Inside the Floor

### Root Cause Analysis

After reviewing the implementation in `MonolithMeshProceduralActions.cpp`, the procedural builders (create_structure, create_parametric_mesh) **already build geometry with min Z = 0**:

- All `AppendBox` calls use `EGeometryScriptPrimitiveOriginMode::Base`, which places the box's bottom face at the transform's Z position
- `create_structure` places the floor slab at `FVector(0, 0, 0)` with `Base` mode — bottom at Z=0
- Walls start at `FloorZ = FloorT` (15cm) so they sit on top of the floor slab
- Furniture builders (chair, table, etc.) also use `Base` mode, building upward from Z=0

**The mesh local-space pivot is already at bottom-center.** The real problem is that `PlaceMeshInScene` (line 307-329) places the actor at the raw `location` parameter with no floor detection:

```cpp
AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Location, Rotation, SpawnParams);
```

If the user passes `[0, 0, 0]` but the level floor isn't at Z=0 (e.g., it's a BSP or static mesh floor at Z=-10 or Z=50), the geometry will clip or float.

### However: AppendLinearStairs Has No OriginMode

The `AppendLinearStairs` API does NOT accept an `EGeometryScriptPrimitiveOriginMode` parameter (unlike `AppendBox`). Looking at the signature:

```cpp
static UDynamicMesh* AppendLinearStairs(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float StepWidth = 100.0f,
    float StepHeight = 20.0f,
    float StepDepth = 30.0f,
    int NumSteps = 8,
    bool bFloating = false,
    UGeometryScriptDebug* Debug = nullptr);
```

It uses `FLinearStairGenerator` internally, which builds stairs starting from the transform origin upward. The first step's bottom face IS at the transform Z. This is correct — stairs also have min Z = 0.

### Recommended Fix: Auto Snap-to-Floor

Add a `snap_to_floor` option to `create_parametric_mesh`, `create_structure`, `create_horror_prop`, and `create_building_shell` that traces downward from the spawn location to find the actual floor surface. This is the same pattern already used by `snap_to_floor` (MonolithMeshSceneActions.cpp line 1066):

```cpp
// Existing snap_to_floor logic:
FVector TraceStart = ActorLoc;
FVector TraceEnd = TraceStart - FVector(0, 0, TraceDistance);
FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(SnapToFloor), true);
QueryParams.AddIgnoredActor(Actor);
bool bHit = World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
// Then: NewZ = Hit.ImpactPoint.Z + ActorHalfHeight + PivotOffset;
```

**Since our meshes already have pivot at bottom-center (min Z = 0), the math simplifies to:**

```cpp
NewZ = Hit.ImpactPoint.Z;  // Just place the actor at the floor hit point
```

No half-height or pivot offset calculation needed — the mesh already sits on its origin.

### Implementation Plan

1. **Add `snap_to_floor` optional boolean param** to all procedural creation actions (default: `true`)
2. **After spawning the actor**, if `snap_to_floor` is true:
   - Trace downward from spawn location using `ECC_Visibility`
   - If hit, set actor Z to `Hit.ImpactPoint.Z`
   - If no hit (void), leave at specified location and warn in response
3. **Also add `auto_snap` option to the existing `PlaceMeshInScene` helper** since it's shared by all procedural actions

### Verification: Mesh Pivot is Correct

To be safe, add a post-build validation step using GeometryScript's bounding box query:

```cpp
#include "GeometryScript/MeshQueryFunctions.h"

FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);
if (Bounds.Min.Z > 1.0f || Bounds.Min.Z < -1.0f)
{
    // Pivot is not at bottom — translate to fix
    UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(
        Mesh, FVector(0, 0, -Bounds.Min.Z));
}
```

This handles edge cases where boolean operations shift geometry unexpectedly.

### GeometryScript APIs for Pivot Correction

If we ever need to force pivot to bottom-center:

```cpp
// Get bounding box (local space)
FBox Bounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(Mesh);

// Option A: TranslateMesh — shift all vertices so min Z = 0
FVector Offset(0, 0, -Bounds.Min.Z);
UGeometryScriptLibrary_MeshTransformFunctions::TranslateMesh(Mesh, Offset);

// Option B: TranslatePivotToLocation — same thing but clearer intent
// Note: This is literally just TranslateMesh(Mesh, -PivotLocation)
FVector BottomCenter((Bounds.Min.X + Bounds.Max.X) * 0.5f,
                     (Bounds.Min.Y + Bounds.Max.Y) * 0.5f,
                     Bounds.Min.Z);
UGeometryScriptLibrary_MeshTransformFunctions::TranslatePivotToLocation(Mesh, BottomCenter);
```

Both functions are in `GeometryScript/MeshTransformFunctions.h` and `GeometryScript/MeshQueryFunctions.h`, already included in our procedural actions file.

---

## Issue 2: Oversized Stairs and Props (Built for Giants)

### Current Defaults vs Real-World Dimensions

| Type | Current Default | Real-World Target | Status |
|------|----------------|-------------------|--------|
| **Stairs** | W=100, D=30 (step_depth), H=20 (step_height), 8 steps | W=90, D=28, H=18 | step_height is per-step; see below |
| Chair | W=45, D=45, H=90, seat=45 | W=45, D=45, H=85-90, seat=45 | **OK** |
| Table | W=120, D=75, H=75 | W=120, D=75, H=75 | **OK** |
| Desk | W=120, D=60, H=75 | W=120, D=60, H=75 | **OK** |
| Shelf | W=80, D=30, H=180 | W=80, D=30, H=180 | **OK** |
| Cabinet | W=60, D=45, H=90 | W=60, D=45, H=90 | **OK** |
| Bed | W=100, D=200, H=55 | W=100, D=200, H=50-60 | **OK** |
| Door frame | W=100, D=20, H=210 | W=90, D=15, H=210 | Close enough |
| Window frame | W=120, D=20, H=100 | W=120, D=15, H=100 | Close enough |
| Ramp | W=100, D=200, H=100 | Varies | Reasonable |
| Pillar | W=30, D=30, H=300 | W=30-40, D=30-40, H=270-300 | **OK** |
| Counter | W=200, D=60, H=90 | W=200, D=60, H=90 | **OK** |
| Toilet | W=40, D=65, H=40 | W=38, D=65, H=40 | **OK** |
| Sink | W=60, D=45, H=85 | W=55-60, D=45, H=85 | **OK** |
| Bathtub | W=75, D=170, H=60 | W=75, D=170, H=55-60 | **OK** |

**Most furniture defaults are already reasonable!** The main problem is **stairs**.

### Stairs: The Real Problem

The stairs defaults are at line 361:
```cpp
else if (Type == TEXT("stairs")) ParseDimensions(Params, Width, Depth, Height, 100, 30, 20);
```

This means: Width=100cm, Depth(step_depth)=30cm, Height(step_height)=20cm

Then at line 451-455:
```cpp
int32 StepN = ... : 8;       // 8 steps
float StepH = ... : Height;  // 20cm per step (from dimensions.height)
float StepD = ... : Depth;   // 30cm per step (from dimensions.depth)
```

The call to AppendLinearStairs:
```cpp
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs(
    Mesh, Opts, StairXf, Width, StepHeight, StepDepth, StepCount, bFloating);
```

**Total staircase height = 8 steps x 20cm = 160cm. Total depth = 8 steps x 30cm = 240cm.**

This is actually reasonable for a standard flight! But the **Width=100cm and StepDepth=30cm** could feel oversized.

**Wait — the AppendLinearStairs default parameters are:** StepWidth=100, StepHeight=20, StepDepth=30, NumSteps=8. Our defaults match the GeometryScript defaults exactly. The problem is these are the engine's built-in defaults, not tuned for realism.

### Real-World Stair Dimensions (Building Code Reference)

- **Step rise (riser):** 17-19cm (IBC code max 17.8cm / 7 inches)
- **Step run (tread depth):** 25-28cm (IBC code min 25.4cm / 10 inches)
- **Stair width:** 90cm minimum (residential), 110cm (commercial), 120cm (public)
- **Headroom:** 200cm minimum clearance above nosing line
- **7-11 rule:** Rise + Run should be ~43-46cm. 18 + 28 = 46cm (perfect)

### UE Character Reference Dimensions

- **ACharacter default capsule:** Half-height 88cm (total 176cm), radius 34cm
- **Default eye height:** ~64cm above capsule center = ~152cm from ground (with 88cm half-height)
- **Typical first-person camera:** ~160-170cm from ground (eye level)
- **Character step-up height:** Default `MaxStepHeight = 45cm` — this is critical for stairs!

**Important:** If step rise > MaxStepHeight (45cm default), the character can't walk up the stairs. The current 20cm default is well within limits.

### Recommended Stair Defaults

```cpp
// Change from:
else if (Type == TEXT("stairs")) ParseDimensions(Params, Width, Depth, Height, 100, 30, 20);
// Change to:
else if (Type == TEXT("stairs")) ParseDimensions(Params, Width, Depth, Height, 90, 28, 18);
```

And update step_count default from 8 to a contextual value:
- For a standard floor-to-floor (270cm residential): `270 / 18 = 15 steps` (standard is 13-16)
- Keep default at 8 for a half-flight (landing), but document it clearly

### Structure Dimension Audit

```
room:      W=400, D=600, H=300  — 4m x 6m x 3m — reasonable residential room
corridor:  W=200, D=800, H=300  — 2m wide x 8m long x 3m — good
l_corridor: W=200, D=600, H=300 — same corridor width — good
t_junction: W=200, D=600, H=300 — good
stairwell: W=300, D=300, H=600  — 3m x 3m x 6m (2 floors) — good
```

Structure defaults are fine. The 300cm height matches a standard residential ceiling.

### Other Dimensions to Tune

| Parameter | Current | Recommended | Reason |
|-----------|---------|-------------|--------|
| wall_thickness | 20cm | 15cm | 20cm is exterior wall; interior = 10-15cm. Default to interior |
| floor_thickness | 15cm | 15cm | OK for blockout |
| door opening default | W=120, H=210 | W=90, H=210 | 120cm is a double door; standard single = 80-90cm |
| window opening | W=120, H=100 | OK | Standard window |
| window sill_height | 90cm | 90cm | **OK** |
| door_frame width | 100cm | 90cm | Match standard single door |
| corridor width | 200cm | 180cm | 200cm is generous; 180cm is standard comfortable |

### The "Human Scale Profile" Concept

Rather than a single `human_scale` bool, provide a `scale_profile` parameter:

```json
{
    "scale_profile": "residential"  // or "commercial", "industrial", "horror"
}
```

Profiles override defaults:

| Profile | Ceiling | Door W | Door H | Corridor W | Wall T | Notes |
|---------|---------|--------|--------|------------|--------|-------|
| residential | 270 | 85 | 210 | 120 | 12 | Tight, claustrophobic |
| commercial | 300 | 100 | 220 | 200 | 15 | Standard office |
| industrial | 400 | 150 | 300 | 300 | 25 | Warehouse/factory |
| horror | 260 | 80 | 200 | 100 | 15 | Cramped, oppressive — ideal for survival horror |

**For Leviathan:** The `horror` profile should be the implicit default. Tight spaces, low ceilings, narrow corridors — all amplify claustrophobia and tension.

---

## Combined Fix Workflow

The correct workflow for procedural geometry placement should be:

1. **Build geometry with bottom at Z=0** (already the case)
2. **Post-build validation:** Check `GetMeshBoundingBox().Min.Z ≈ 0`, translate if not
3. **Save mesh to StaticMesh asset**
4. **Spawn actor at requested location**
5. **Auto snap-to-floor:** Trace downward, place actor Z at hit point
6. **Report:** Include snap result, actual placement Z, and warnings if no floor hit

### Implementation Priority

| Fix | Effort | Impact | Priority |
|-----|--------|--------|----------|
| Add snap_to_floor to PlaceMeshInScene | ~30 min | High | P0 |
| Post-build min-Z validation + correction | ~15 min | Medium | P0 |
| Tune stair defaults (90/28/18) | ~5 min | High | P0 |
| Tune door_frame width (100 -> 90) | ~5 min | Low | P1 |
| Scale profiles (residential/commercial/horror) | ~2 hr | Medium | P1 |
| Document all real-world dimensions in action descriptions | ~30 min | Medium | P1 |
| Wall thickness default 20 -> 15 | ~5 min | Low | P2 |

### Code Locations to Modify

All changes in `Source/MonolithMesh/Private/MonolithMeshProceduralActions.cpp`:

1. **`PlaceMeshInScene`** (line 307): Add floor trace after spawn
2. **`CreateParametricMesh`** (line 335): Call post-build pivot validation
3. **`CreateStructure`** (line 1580): Call post-build pivot validation
4. **Line 361:** Change stair defaults `(100, 30, 20)` to `(90, 28, 18)`
5. **Line 359:** Change door_frame defaults `(100, 20, 210)` to `(90, 15, 210)`
6. **`CreateHorrorProp`**: Same snap_to_floor + pivot validation pattern
7. **`CreateBuildingShell`** (line 1782): Same snap_to_floor + pivot validation

### Headers Already Included

Both required headers are already in the includes:
- `GeometryScript/MeshTransformFunctions.h` (line 19) — for `TranslateMesh`
- `GeometryScript/MeshQueryFunctions.h` — **NOT included, needs adding** for `GetMeshBoundingBox`

Actually checking the includes:

```cpp
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshDeformFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"  // TranslateMesh available
#include "GeometryScript/MeshDecompositionFunctions.h"
```

**Missing:** `#include "GeometryScript/MeshQueryFunctions.h"` — needed for `GetMeshBoundingBox`

---

## Summary

**Issue 1** is not really about pivot placement (which is correct at bottom-center) but about the lack of automatic floor detection when placing actors. The fix is straightforward: add a downward line trace after spawning, using the same pattern as the existing `snap_to_floor` action.

**Issue 2** is mostly fine — furniture defaults are realistic. The main offender is stairs using GeometryScript's engine defaults (100/30/20) instead of building-code-appropriate values (90/28/18). Scale profiles would be a nice-to-have for different architectural contexts, with a `horror` profile as the default for Leviathan.

Total estimated fix time: ~1 hour for P0 items, ~3 hours including P1.
