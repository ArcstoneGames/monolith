# Prop Collision-Aware Placement Research

**Date:** 2026-03-28
**Status:** Research complete, ready for implementation
**Affects:** `scatter_props`, `scatter_on_surface`, `scatter_on_walls`, `scatter_on_ceiling`, `place_prop_kit`, `settle_props`

---

## Problem Statement

Monolith scatter/spawn actions sometimes place props halfway through walls, floors, or other geometry. The current code uses `AlwaysSpawn` collision handling and only does floor-trace or wall-trace for positioning -- it never validates whether the prop's full volume actually fits at the candidate location.

### Current Behavior by Action

| Action | Floor/Surface Trace | Overlap Check | Collision Rejection | Push-Out |
|--------|---------------------|---------------|---------------------|----------|
| `scatter_props` | Yes (LineTrace down) | Post-spawn warning only (CheckBlockoutOverlap) | No -- warns but still places | No |
| `scatter_on_surface` | Yes (LineTrace down to surface) | Overhang check (2D bounds only) | Skips if XY overhang | No |
| `scatter_on_walls` | Yes (LineTrace horizontal) | None | None | No |
| `scatter_on_ceiling` | Yes (LineTrace up) | None | None | No |
| `place_prop_kit` | None | None | None | No |
| `settle_props` | Yes (LineTrace down) | None | None | No |

### Root Causes

1. **Line traces only check a single point.** A line trace down finds the floor, but the prop's box extends in all directions. If the candidate XY is 5cm from a wall, a 30cm-wide table will clip the wall.
2. **`AlwaysSpawn` bypasses collision entirely.** Every spawn uses `ESpawnActorCollisionHandlingMethod::AlwaysSpawn`.
3. **`CheckBlockoutOverlap` is post-hoc warning only.** It correctly uses `OverlapMultiByChannel` with the actor's bounding box, but only emits a warning string -- it never rejects or adjusts the placement.
4. **No sweep trace before placement.** A sweep (moving a shape from A to B) would catch the prop clipping into nearby geometry before it's spawned.

---

## UE 5.7 Collision APIs -- Verified Signatures

All signatures verified against engine source at `C:/Program Files (x86)/UE_5.7/Engine/Source/`.

### 1. SweepSingleByChannel

```cpp
// World.h:2181
bool UWorld::SweepSingleByChannel(
    FHitResult& OutHit,
    const FVector& Start,
    const FVector& End,
    const FQuat& Rot,
    ECollisionChannel TraceChannel,
    const FCollisionShape& CollisionShape,
    const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
    const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam
) const;
```

**Use case:** Sweep a box/sphere from above the candidate position downward to find the exact resting point that clears all geometry. Unlike LineTrace, this accounts for the prop's volume.

**Key behavior with initial overlap:** If the shape starts inside geometry, the hit result has `bStartPenetrating = true` and `PenetrationDepth` gives the depenetration distance along `Normal`.

### 2. SweepMultiByChannel

```cpp
// World.h:2220
bool UWorld::SweepMultiByChannel(
    TArray<FHitResult>& OutHits,
    const FVector& Start,
    const FVector& End,
    const FQuat& Rot,
    ECollisionChannel TraceChannel,
    const FCollisionShape& CollisionShape,
    const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
    const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam
) const;
```

**Use case:** Same as SweepSingle but returns all hits. Useful if we need to know all geometry the prop would intersect during placement.

### 3. OverlapMultiByChannel

```cpp
// World.h:2313
bool UWorld::OverlapMultiByChannel(
    TArray<FOverlapResult>& OutOverlaps,
    const FVector& Pos,
    const FQuat& Rot,
    ECollisionChannel TraceChannel,
    const FCollisionShape& CollisionShape,
    const FCollisionQueryParams& Params = FCollisionQueryParams::DefaultQueryParam,
    const FCollisionResponseParams& ResponseParam = FCollisionResponseParams::DefaultResponseParam
) const;
```

**Use case:** Already used in `CheckBlockoutOverlap`. Tests if a shape at a position overlaps any geometry. Returns the set of overlapping components but does NOT give penetration depth or direction.

### 4. FCollisionShape Factory Methods

```cpp
// PhysicsCore/Public/CollisionShape.h
static FCollisionShape FCollisionShape::MakeBox(const FVector& BoxHalfExtent);
static FCollisionShape FCollisionShape::MakeBox(const FVector3f& BoxHalfExtent);
static FCollisionShape FCollisionShape::MakeSphere(float SphereRadius);
static FCollisionShape FCollisionShape::MakeCapsule(float CapsuleRadius, float CapsuleHalfHeight);
```

### 5. FMTDResult and ComputePenetration

```cpp
// EngineTypes.h:2417
struct FMTDResult
{
    FVector Direction;  // Normalized direction to push out
    float Distance;     // Distance along Direction to clear penetration
};

// PrimitiveComponent.h:3085
bool UPrimitiveComponent::ComputePenetration(
    FMTDResult& OutMTD,
    const FCollisionShape& CollisionShape,
    const FVector& Pos,
    const FQuat& Rot
);
```

**Use case:** Given a shape overlapping a component, computes the minimum translation vector (MTV) to resolve the penetration. Internally calls `FBodyInstance::OverlapTest` with MTD output.

**Limitation:** Operates on a single `UPrimitiveComponent`, not the world. To resolve against multiple overlapping components, you need to iterate overlaps and pick the largest MTD, or use the sweep approach instead.

### 6. FHitResult Penetration Fields

```cpp
// HitResult.h
uint8 bStartPenetrating : 1;  // True if sweep started inside geometry
float PenetrationDepth;        // Distance along Normal to escape penetration
```

When `SweepSingleByChannel` returns with `bStartPenetrating == true`, these fields give the depenetration info directly -- no need to call `ComputePenetration` separately.

---

## Recommended Architecture

### Strategy: Sweep-Down + Overlap-Reject + Optional Push-Out

A three-phase validation pipeline for every candidate prop placement:

```
Phase 1: SWEEP to find resting position
    - Replace LineTrace with SweepSingleByChannel using the prop's bounding box
    - This naturally finds a position where the prop's VOLUME clears the floor

Phase 2: OVERLAP CHECK at candidate position
    - After sweep, do OverlapMultiByChannel at the resolved position
    - If overlaps exist, either reject or push-out

Phase 3: PUSH-OUT (optional, configurable)
    - If enabled, use ComputePenetration on each overlapping component
    - Move prop along largest MTD vector
    - Re-check overlaps after adjustment (max 3 iterations)
    - If still overlapping, reject
```

### Shared Utility Function

All scatter actions should share a single validation function:

```cpp
struct FPropPlacementResult
{
    bool bValid;              // Whether placement succeeded
    FVector FinalLocation;    // Adjusted location (may differ from candidate)
    FRotator FinalRotation;   // May be adjusted for surface alignment
    FString RejectReason;     // If bValid==false, why
    TArray<FString> Warnings; // Non-fatal issues (e.g., "pushed 12cm from wall")
};

// Proposed helper in MonolithMeshUtils or a new MonolithCollisionUtils
static FPropPlacementResult ValidatePropPlacement(
    UWorld* World,
    const FVector& CandidateLocation,
    const FQuat& CandidateRotation,
    const FVector& PropHalfExtent,      // From mesh bounds * scale
    const FCollisionQueryParams& BaseParams,
    ECollisionChannel Channel = ECC_WorldStatic,
    bool bAllowPushOut = true,
    float MaxPushOutDistance = 50.0f,    // cm - don't push more than this
    int32 MaxPushOutIterations = 3
);
```

### Implementation Per Action

#### scatter_props (BlockoutActions)

**Current:** LineTrace down -> AlwaysSpawn -> post-hoc warning
**Proposed:**
1. SweepSingle box down from volume top to volume bottom (replaces LineTrace)
2. If sweep hits floor: candidate = hit.Location + FVector(0,0, PropHalfExtent.Z)
3. OverlapMulti at candidate with prop box shape (excluding volume actor and other scattered props)
4. If overlaps: attempt push-out or skip candidate
5. Add optional `collision_mode` param: `"warn"` (current behavior), `"reject"` (skip), `"adjust"` (push-out + retry)

#### scatter_on_surface (ContextPropActions)

**Current:** LineTrace down -> overhang check (2D) -> AlwaysSpawn
**Proposed:**
1. Keep the Poisson disk sampling for XY candidates
2. Replace LineTrace with SweepSingle using a shrunk prop box (80% extent to allow minor overlap at edges)
3. OverlapMulti at resolved position (ignore surface actor itself)
4. Reject if overlapping non-surface geometry (another prop, a wall, etc.)
5. The existing 2D overhang check can stay as an early-out optimization

#### scatter_on_walls (ContextPropActions)

**Current:** LineTrace horizontal -> offset by wall_offset -> AlwaysSpawn
**Proposed:**
1. Keep the horizontal LineTrace to find wall hits
2. After finding hit point, compute candidate = hit.ImpactPoint + Normal * (wall_offset + PropHalfExtent along normal axis)
3. OverlapMulti at candidate with prop box (rotated to wall alignment)
4. Reject if overlapping other wall props or geometry
5. This prevents paintings clipping through adjacent corners or other wall-mounted objects

#### scatter_on_ceiling (ContextPropActions)

**Current:** LineTrace up -> offset by ceiling_offset -> AlwaysSpawn
**Proposed:**
1. Keep the upward LineTrace to find ceiling
2. Candidate = hit.ImpactPoint + Normal * (ceiling_offset + PropHalfExtent.Z)
3. OverlapMulti at candidate
4. Reject if overlapping

#### place_prop_kit (ContextPropActions)

**Current:** Direct spawn at user-specified location, no validation at all
**Proposed:**
1. After computing each kit item's world position (kit_location + item_offset)
2. Optional SweepSingle down to snap to floor
3. OverlapMulti to validate
4. Add optional `validate_placement` param (default true)

#### settle_props (ContextPropActions)

**Current:** LineTrace down -> snap to surface
**Proposed:**
1. Replace LineTrace with SweepSingle box down
2. This naturally prevents props from being pushed through thin geometry during settle

---

## Shape Choice: Simplified Box vs Actual Collision

### Recommendation: Use Mesh Bounding Box, NOT Actor Collision

**Rationale:**
- **Editor-time spawned StaticMeshActors may not have physics bodies initialized.** The collision shapes (UBodySetup) are loaded but the FBodyInstance may not be initialized for query until the component is registered and collision is enabled.
- **Bounding box is always available** via `UStaticMesh::GetBounds()` which returns `FBoxSphereBounds` with `BoxExtent`.
- **Good enough for placement validation.** We're preventing gross clipping (prop halfway through a wall), not simulating physics. A bounding box catches 95% of issues.
- **Much faster.** `FCollisionShape::MakeBox` with the AABB half-extent is a single primitive query. Using the actual mesh collision would require `ComponentSweepMultiByChannel` which needs a registered component with a valid body.

**Getting the box from mesh bounds:**
```cpp
FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
FVector PropHalfExtent = MeshBounds.BoxExtent * Scale;
// Slight shrink to avoid false positives at touching surfaces
PropHalfExtent *= 0.9f;
FCollisionShape PropShape = FCollisionShape::MakeBox(PropHalfExtent);
```

**The 0.9x shrink factor** is important because AABB bounds are often larger than the visual mesh (they're axis-aligned and encompass all vertices). Without shrinking, props that visually fit would be rejected because their AABBs technically overlap.

### When to Use Actual Collision

For `ComponentSweepMultiByChannel` or `ComponentOverlapMultiByChannel`, you'd need:
```cpp
// The spawned actor's mesh component must have collision enabled
UStaticMeshComponent* MeshComp = PropActor->GetStaticMeshComponent();
MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

TArray<FHitResult> Hits;
World->ComponentSweepMultiByChannel(Hits, MeshComp, Start, End, Rotation, ECC_WorldStatic, QueryParams);
```

This gives exact mesh-shape collision but is expensive and requires the actor to already be spawned. Only worth it for high-fidelity placement (e.g., tight-fitting props in cabinets).

---

## Push-Out Algorithm

For cases where a prop is partially embedded and we want to salvage the placement rather than reject it:

```cpp
static bool TryPushOutProp(
    UWorld* World,
    FVector& InOutLocation,
    const FQuat& Rotation,
    const FVector& PropHalfExtent,
    const FCollisionQueryParams& Params,
    float MaxPushDistance,
    int32 MaxIterations)
{
    FCollisionShape PropShape = FCollisionShape::MakeBox(PropHalfExtent);

    for (int32 Iter = 0; Iter < MaxIterations; ++Iter)
    {
        TArray<FOverlapResult> Overlaps;
        if (!World->OverlapMultiByChannel(Overlaps, InOutLocation, Rotation,
            ECC_WorldStatic, PropShape, Params))
        {
            return true; // No overlaps -- success
        }

        // Find the largest penetration across all overlapping components
        FVector BestDirection = FVector::ZeroVector;
        float BestDistance = 0.0f;

        for (const FOverlapResult& Overlap : Overlaps)
        {
            UPrimitiveComponent* Comp = Overlap.GetComponent();
            if (!Comp) continue;

            FMTDResult MTD;
            if (Comp->ComputePenetration(MTD, PropShape, InOutLocation, Rotation))
            {
                if (MTD.Distance > BestDistance)
                {
                    BestDirection = MTD.Direction;
                    BestDistance = MTD.Distance;
                }
            }
        }

        if (BestDistance <= UE_KINDA_SMALL_NUMBER)
        {
            return false; // Can't compute MTD -- reject
        }

        // Apply push-out with a small epsilon to clear the surface
        FVector Adjustment = BestDirection * (BestDistance + 0.25f);

        if (Adjustment.Size() > MaxPushDistance)
        {
            return false; // Would push too far -- reject
        }

        InOutLocation += Adjustment;
        MaxPushDistance -= Adjustment.Size(); // Reduce remaining budget
    }

    // Ran out of iterations -- do a final check
    TArray<FOverlapResult> FinalOverlaps;
    return !World->OverlapMultiByChannel(FinalOverlaps, InOutLocation, Rotation,
        ECC_WorldStatic, FCollisionShape::MakeBox(PropHalfExtent), Params);
}
```

### Caveats

1. **ComputePenetration requires FBodyInstance.** The overlapping component must have a valid physics body. In the editor world, static mesh actors placed in the level generally have this. But blockout volumes using brush geometry may not -- filter these out.
2. **MTD is approximate.** For complex mesh shapes, the MTD from PhysX/Chaos is computed against the convex hull decomposition, which may not perfectly match the visual mesh.
3. **Multiple overlaps need iterative resolution.** Pushing out of one wall might push into another. The iteration loop handles this, but 3 iterations is a reasonable cap.
4. **Prefer rejection over aggressive push-out.** For scatter actions, it's usually better to skip a bad candidate and try the next Poisson sample than to push a prop into a weird position. Push-out is most useful for `settle_props` and `place_prop_kit` where the position is more intentional.

---

## Sweep-Down for Floor Finding

The biggest win is replacing `LineTraceSingleByChannel` with `SweepSingleByChannel` for floor detection in `scatter_props`:

```cpp
// Current (line trace -- only checks a single point)
FHitResult FloorHit;
bool bHitFloor = World->LineTraceSingleByChannel(
    FloorHit, TraceStart, TraceEnd, ECC_Visibility, FloorTraceParams);
SpawnLocation = FloorHit.Location;

// Proposed (box sweep -- checks the prop's full footprint)
FCollisionShape PropShape = FCollisionShape::MakeBox(
    FVector(PropHalfExtent.X, PropHalfExtent.Y, 1.0f)); // Thin box for floor detection

FHitResult FloorHit;
bool bHitFloor = World->SweepSingleByChannel(
    FloorHit,
    FVector(WorldX, WorldY, VolumeMax.Z),    // Start above
    FVector(WorldX, WorldY, VolumeMin.Z),    // End at bottom
    FQuat::Identity,                          // No rotation for AABB
    ECC_WorldStatic,                          // WorldStatic catches floors
    PropShape,
    FloorTraceParams);

if (bHitFloor && !FloorHit.bStartPenetrating)
{
    // Place prop bottom at sweep hit point
    SpawnLocation = FloorHit.Location + FVector(0, 0, PropHalfExtent.Z);
}
else if (bHitFloor && FloorHit.bStartPenetrating)
{
    // Started inside geometry -- candidate is invalid
    continue;
}
```

**Why `ECC_WorldStatic` instead of `ECC_Visibility`?**
The current code uses `ECC_Visibility` for floor traces. This works but can miss geometry that has visibility traces disabled (common for invisible collision volumes). `ECC_WorldStatic` is more correct for "will this prop physically fit here" queries. However, changing the channel might break existing behavior, so this should be configurable or at least well-tested.

---

## New Parameter: `collision_mode`

Add to all scatter actions:

```
Optional("collision_mode", "string",
    "How to handle prop-geometry collisions: 'none' (current behavior, always place),
     'warn' (place but report overlaps), 'reject' (skip overlapping placements),
     'adjust' (try to push-out, reject if can't)", "warn")
```

This gives backward compatibility (default "warn" matches current `scatter_props` behavior) while enabling stricter validation when needed.

---

## Per-Action Changes Summary

### scatter_props (MonolithMeshBlockoutActions.cpp:2497)

| Change | Lines | Effort |
|--------|-------|--------|
| Replace LineTrace with SweepSingle for floor finding | ~20 lines changed | Low |
| Use OverlapMulti before spawn instead of after | ~15 lines moved/modified | Low |
| Add collision_mode param + rejection logic | ~40 lines new | Medium |
| Add push-out logic for "adjust" mode | ~60 lines new (shared utility) | Medium |
| **Total** | | **~4h** |

### scatter_on_surface (MonolithMeshContextPropActions.cpp:377)

| Change | Lines | Effort |
|--------|-------|--------|
| Add OverlapMulti check after floor trace | ~20 lines new | Low |
| Add collision_mode param | ~10 lines | Low |
| SweepSingle optional (most surface placements are constrained enough) | ~15 lines | Low |
| **Total** | | **~2h** |

### scatter_on_walls (MonolithMeshContextPropActions.cpp:1467)

| Change | Lines | Effort |
|--------|-------|--------|
| Compute prop extent along wall normal for offset | ~10 lines | Low |
| OverlapMulti at candidate before spawn | ~20 lines | Low |
| Add collision_mode param | ~10 lines | Low |
| **Total** | | **~2h** |

### scatter_on_ceiling (MonolithMeshContextPropActions.cpp:1668)

| Change | Lines | Effort |
|--------|-------|--------|
| Compute prop extent for ceiling offset | ~10 lines | Low |
| OverlapMulti at candidate before spawn | ~20 lines | Low |
| Add collision_mode param | ~10 lines | Low |
| **Total** | | **~1.5h** |

### place_prop_kit (ContextPropActions)

| Change | Lines | Effort |
|--------|-------|--------|
| Optional SweepSingle snap-to-floor per item | ~25 lines | Low |
| OverlapMulti per item | ~20 lines | Low |
| Add validate_placement param | ~10 lines | Low |
| **Total** | | **~2h** |

### settle_props (ContextPropActions)

| Change | Lines | Effort |
|--------|-------|--------|
| Replace LineTrace with SweepSingle | ~15 lines changed | Low |
| **Total** | | **~1h** |

### Shared Utility (new MonolithCollisionUtils or added to MonolithMeshUtils)

| Change | Lines | Effort |
|--------|-------|--------|
| ValidatePropPlacement function | ~80 lines | Medium |
| TryPushOutProp function | ~60 lines | Medium |
| MakeCollisionShapeFromMesh helper | ~15 lines | Low |
| **Total** | | **~3h** |

### Grand Total: ~15.5h

---

## Implementation Order

1. **Shared utility functions** (ValidatePropPlacement, TryPushOutProp, MakeCollisionShapeFromMesh)
2. **scatter_props** -- highest impact, most-used action, already has warning infrastructure
3. **scatter_on_surface** -- second most used
4. **scatter_on_walls** -- wall clipping is visually obvious
5. **scatter_on_ceiling** -- less common
6. **settle_props** -- swap LineTrace for SweepSingle
7. **place_prop_kit** -- add optional validation

---

## Edge Cases and Gotchas

### 1. Rotated Props and AABB vs OBB

`FCollisionShape::MakeBox` creates an AABB (axis-aligned bounding box). When a prop has random yaw rotation, its AABB grows larger than the oriented bounding box. For a long, thin prop (e.g., a pipe or sword), a 45-degree rotation makes the AABB ~41% wider.

**Solution:** Pass the prop's rotation quaternion to `SweepSingleByChannel` and `OverlapMultiByChannel`. These APIs accept `FQuat Rot` which rotates the collision shape, making it an OBB query.

```cpp
FQuat PropRot = SpawnRotation.Quaternion();
World->OverlapMultiByChannel(Overlaps, Location, PropRot, Channel, PropShape, Params);
```

### 2. Mesh Bounds vs Collision Bounds

`UStaticMesh::GetBounds()` returns the visual mesh bounding box. The actual collision might be simpler (a convex hull or box primitive). For placement validation, the visual bounds are actually what we want -- they represent the visible extent that shouldn't clip through walls.

### 3. Brush Geometry (BSP)

Editor blockout geometry using brushes responds to collision queries differently than static meshes. The `ABrush` actor's `UBrushComponent` may not always have collision enabled for overlap queries in the editor. Test this -- if brushes are invisible to OverlapMulti, we may need to also do a multi-line-trace check as fallback.

### 4. Foliage and Instanced Static Meshes

Foliage actors (painted grass, etc.) should be ignored during overlap checks. Add `ECC_WorldStatic` channel filtering and consider adding foliage actors to the ignore list, or use `FCollisionQueryParams::AddIgnoredActors` with a tag filter.

### 5. Scale

Current code applies random scale AFTER computing mesh bounds. Must multiply `MeshBounds.BoxExtent` by the scale factor:

```cpp
float Scale = RandStream.FRandRange(ScaleMin, ScaleMax);
FVector PropHalfExtent = MeshBounds.BoxExtent * Scale * 0.9f; // 0.9 shrink factor
```

### 6. Editor World Physics

In the editor (not PIE), physics queries work on the `UWorld` returned by `MonolithMeshUtils::GetEditorWorld()`. Components must have collision enabled (they do by default for static meshes) and be registered in the world. This is already working for `CheckBlockoutOverlap`, so the infrastructure is proven.

---

## Files to Modify

| File | Changes |
|------|---------|
| `Source/MonolithMesh/Private/MonolithMeshBlockoutActions.cpp` | scatter_props overhaul, move CheckBlockoutOverlap to shared |
| `Source/MonolithMesh/Private/MonolithMeshContextPropActions.cpp` | scatter_on_surface, scatter_on_walls, scatter_on_ceiling, settle_props, place_prop_kit |
| `Source/MonolithMesh/Public/MonolithMeshUtils.h` | Add collision validation helpers (or new header) |
| `Source/MonolithMesh/Private/MonolithMeshUtils.cpp` | Implement collision validation helpers |

---

## Testing Plan

1. **scatter_props in a room with walls:** Place 20 props in a small room volume. With `collision_mode: "reject"`, verify zero props clip through walls. Compare count placed vs requested to confirm some were rejected.
2. **scatter_on_surface on a shelf against a wall:** Props on a shelf flush against a wall should not clip through the wall.
3. **scatter_on_walls in a corner:** Wall props near a corner should not extend past the corner into the adjacent wall.
4. **settle_props through thin geometry:** Place props on a thin platform (10cm thick). Settle should land ON the platform, not fall through.
5. **push-out mode:** Intentionally place a prop kit partially inside a wall. With `collision_mode: "adjust"`, verify it gets pushed clear.
6. **Performance:** Scatter 200 props with collision validation. Should complete in under 2 seconds (collision queries are fast in editor).
7. **Backward compatibility:** `collision_mode: "none"` should produce identical results to current behavior.
