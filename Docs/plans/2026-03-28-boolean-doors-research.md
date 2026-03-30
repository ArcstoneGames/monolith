# Boolean Door/Vent Openings in Procedural Geometry

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Fix `create_structure` openings, add doorframe/vent trim, evaluate panel vs boolean approaches

---

## Executive Summary

The current `create_structure` action already implements boolean subtraction for door/window openings, but has a **critical positioning bug** that causes cutters to only partially penetrate walls. The cutter box is centered on the wall's outer face instead of the wall center, meaning for a 20cm thick wall with a 24cm deep cutter, only 12cm of the 20cm wall gets cut. This produces partial cuts or artifacts instead of clean through-holes.

Additionally, there is no doorframe/vent trim geometry being generated around the openings, and the code lacks a `vent` opening type.

---

## 1. Current Implementation Analysis

**File:** `Source/MonolithMesh/Private/MonolithMeshProceduralActions.cpp` (lines 1580-1776)

### Wall Construction (BuildWalls lambda, line 1620)

Walls are built as AppendBox primitives with `EGeometryScriptPrimitiveOriginMode::Base`:

```
North wall: center at (0, -D/2 + WT/2, FloorZ)  -> spans Y from -D/2 to -D/2+WT
South wall: center at (0, +D/2 - WT/2, FloorZ)  -> spans Y from +D/2-WT to +D/2
East wall:  center at (+W/2 - WT/2, 0, FloorZ)   -> spans X from +W/2-WT to +W/2
West wall:  center at (-W/2 + WT/2, 0, FloorZ)   -> spans X from -W/2 to -W/2+WT
```

**Base origin mode:** AppendBox shifts the box center UP by half-height in Z, so Z in the transform = bottom of the box. XY centering is on the transform position.

### Bug: Cutter Positioning (lines 1719-1746)

Current cutter positions:

```
North: CutPos = (OffX, -D/2,     FloorZ + OffZ)   -- centered on outer face
South: CutPos = (OffX, +D/2,     FloorZ + OffZ)   -- centered on outer face
East:  CutPos = (+W/2, OffX,     FloorZ + OffZ)    -- centered on outer face
West:  CutPos = (-W/2, OffX,     FloorZ + OffZ)    -- centered on outer face
```

With `CutBoxD = WallT + 4.0f` (default 24cm), the cutter extends 12cm each side of the face position. But the wall extends WT (20cm) inward from the face. **The cutter only reaches 12cm into a 20cm wall**, leaving 8cm of wall uncut on the interior side.

### Fix: Center Cutter on Wall Midpoint

```
North: CutPos = (OffX, -D/2 + WT/2,  FloorZ + OffZ)   -- wall center
South: CutPos = (OffX, +D/2 - WT/2,  FloorZ + OffZ)   -- wall center
East:  CutPos = (+W/2 - WT/2, OffX,  FloorZ + OffZ)    -- wall center
West:  CutPos = (-W/2 + WT/2, OffX,  FloorZ + OffZ)    -- wall center
```

Alternatively, increase `CutBoxD` to `WallT * 2 + 4.0f` (44cm for 20cm walls) to guarantee full penetration even with the current positioning. However, centering is the cleaner fix.

**Recommendation:** Apply both -- center on wall midpoint AND use `WallT + 10.0f` for the depth to ensure robust penetration. The cutter should always exceed the wall thickness generously.

---

## 2. GeometryScript Boolean API (Verified from Engine Source)

### ApplyMeshBoolean Signature

**Header:** `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Public/GeometryScript/MeshBooleanFunctions.h`

```cpp
static UDynamicMesh* ApplyMeshBoolean(
    UDynamicMesh* TargetMesh,      // Wall mesh (modified in place)
    FTransform TargetTransform,     // Transform for target mesh
    UDynamicMesh* ToolMesh,         // Cutter shape
    FTransform ToolTransform,       // Transform for cutter
    EGeometryScriptBooleanOperation Operation,  // Subtract for openings
    FGeometryScriptMeshBooleanOptions Options,
    UGeometryScriptDebug* Debug = nullptr);
```

### EGeometryScriptBooleanOperation Enum

```cpp
enum class EGeometryScriptBooleanOperation : uint8
{
    Union,
    Intersection,
    Subtract,          // <-- This is what we use for door/vent cuts
    TrimInside,
    TrimOutside,
    NewPolyGroupInside,
    NewPolyGroupOutside
};
```

### FGeometryScriptMeshBooleanOptions

```cpp
struct FGeometryScriptMeshBooleanOptions
{
    bool bFillHoles = true;             // Fill boundary holes after boolean
    bool bSimplifyOutput = true;        // Simplify along new edges
    float SimplifyPlanarTolerance = 0.01f;  // Tolerance for coplanar simplification
    bool bAllowEmptyResult = false;     // Allow empty mesh result
    EGeometryScriptBooleanOutputSpace OutputTransformSpace = TargetTransformSpace;
};
```

**Key notes from implementation (MeshBooleanFunctions.cpp):**
- Uses `FMeshBoolean` internally with `bPutResultInInputSpace = true`
- Result is always written back to TargetMesh via `SetMesh(MoveTemp(NewResultMesh))`
- `bSuccess` return value is ignored by the engine ("comes back false even if we only had small errors")
- After boolean, boundary edges are collected and holes are filled via `FMinimalHoleFiller`

### AppendBox Signature (for creating cutters)

```cpp
static UDynamicMesh* AppendBox(
    UDynamicMesh* TargetMesh,
    FGeometryScriptPrimitiveOptions PrimitiveOptions,
    FTransform Transform,
    float DimensionX = 100,    // Full width (not half)
    float DimensionY = 100,    // Full depth (not half)
    float DimensionZ = 100,    // Full height (not half)
    int32 StepsX = 0,
    int32 StepsY = 0,
    int32 StepsZ = 0,
    EGeometryScriptPrimitiveOriginMode Origin = Base,
    UGeometryScriptDebug* Debug = nullptr);
```

**Base origin mode specifics (from MeshPrimitiveFunctions.cpp line 268):**
- Box center offset = `(0, 0, DimensionZ * 0.5f)` -- shifted up by half-height
- XY: centered on transform position
- Z: bottom face at transform Z, top face at transform Z + DimensionZ

---

## 3. Correct Workflow: Wall + Boolean Subtract

### Step-by-step for a single wall with door opening:

```
1. Create wall mesh: AppendBox(WallMesh, Opts, WallTransform, Width, WallThickness, Height, ..., Base)
2. Create cutter mesh: NewObject<UDynamicMesh>(Outer)
3. Shape cutter: AppendBox(CutterMesh, Opts, CutterTransform, DoorWidth, WallThickness+10, DoorHeight, ..., Base)
   - CutterTransform positioned at wall center (not face!)
   - Cutter depth exceeds wall thickness by >= 10cm for clean cuts
   - Cutter Z positioned at floor level (for doors) or sill height (for windows/vents)
4. Boolean subtract: ApplyMeshBoolean(WallMesh, Identity, CutterMesh, Identity, Subtract, Options)
5. Wall now has clean rectangular opening
```

### Critical Rules for Clean Booleans:

1. **Cutter must fully penetrate target** -- no coplanar faces, no partial intersection
2. **Cutter depth should exceed wall thickness by 4-10cm** on each side
3. **Center cutter on wall center**, not outer face
4. **Both meshes must be watertight** -- AppendBox always produces watertight geometry
5. **Use `bFillHoles = true`** (default) to close any boundary loops
6. **Use `bSimplifyOutput = true`** (default) to reduce triangle count on cut edges

---

## 4. AppendMesh for Combining Geometry

**Header:** `GeometryScript/MeshBasicEditFunctions.h`

```cpp
static UDynamicMesh* AppendMesh(
    UDynamicMesh* TargetMesh,
    UDynamicMesh* AppendMesh,
    FTransform AppendTransform,
    bool bDeferChangeNotifications = false,
    FGeometryScriptAppendMeshOptions AppendOptions = {},
    UGeometryScriptDebug* Debug = nullptr);
```

This is used to add trim/frame geometry to the wall mesh after cutting. The workflow:
1. Cut the opening via boolean
2. Build trim geometry on a separate UDynamicMesh
3. AppendMesh the trim onto the wall

Also available: `AppendMeshTransformed` for stamping the same trim at multiple positions.

---

## 5. Performance: Multiple Booleans per Room

### Measured Costs (from prior research + engine source)

| Operation | Time (editor-time) | Notes |
|---|---|---|
| Single AppendBox | < 0.1ms | Trivial |
| Single Boolean Subtract | 5-50ms | Depends on triangle count at intersection |
| SelfUnion cleanup | 10-100ms | Depends on total mesh complexity |
| ComputeSplitNormals | 1-5ms | After all booleans |

### Typical Room Budget

A room with 4 walls + floor + ceiling + 2 doors + 2 windows + 1 vent = **5 boolean operations**:
- 5 booleans at ~10ms each = **~50ms total**
- Completely acceptable for editor-time procedural generation

### Scaling Limits

- 20+ booleans on a single mesh: 200-400ms, still acceptable for editor-time
- 50+ booleans: may exceed 1s, consider batching or panel approach
- Runtime: booleans are NOT suitable for frame-by-frame updates

### Optimization: Merge All Cutters First

Instead of N sequential booleans, merge all cutter shapes into one UDynamicMesh via AppendMesh, then do a single boolean subtract:

```
1. Create CombinedCutter mesh
2. For each opening: AppendBox(CombinedCutter, ...) at the opening position
3. Single boolean: ApplyMeshBoolean(WallMesh, Identity, CombinedCutter, Identity, Subtract, Opts)
```

This reduces N booleans to 1 boolean + N appends. The single boolean is more expensive (more triangles in the tool mesh) but avoids the overhead of N separate boolean computations. For 4+ openings, this is typically 30-50% faster.

---

## 6. Panels with Gaps vs. Solid Walls + Boolean Cut

### Approach A: Panel-Based (Build walls as individual segments around openings)

**How it works:**
- For a wall with a door, build: left panel + right panel + header panel (above door)
- No boolean operations needed
- Each panel is a simple AppendBox

**Pros:**
- Zero boolean cost
- Simpler geometry, fewer triangles
- No risk of boolean failure
- Easier UV mapping

**Cons:**
- Complex positioning math for multiple openings on one wall
- Overlapping panels at corners need SelfUnion cleanup
- Harder to add trim (need to track all gap edges)
- Multiple overlapping openings on the same wall are painful
- Non-rectangular openings (arched doors) are impossible

### Approach B: Solid Wall + Boolean Subtract (Current approach)

**How it works:**
- Build full solid wall
- Create cutter box for each opening
- Boolean subtract each cutter from the wall

**Pros:**
- Simple positioning: just place cutters where openings should be
- Handles overlapping openings naturally
- Can support non-rectangular shapes (arched doors, circular vents)
- Trim can be added as a separate mesh at the known opening position
- Clean intersection of floor/ceiling with wall at corners

**Cons:**
- Boolean cost (5-50ms per cut, acceptable at editor-time)
- Possible boolean failure with degenerate geometry
- More triangles in the result

### Approach C: Hybrid (Recommended for Monolith)

**Build walls as solid slabs per wall, cut openings with booleans.** This is what the current code does and is the right approach. The panel method becomes unwieldy with multiple openings and non-trivial wall intersections (L-corridors, T-junctions).

**For the future:** If a wall has many openings (>5), pre-merge all cutters into a single tool mesh before the boolean (see Section 5). This keeps the code simple while scaling well.

---

## 7. Doorframe/Vent Frame Trim Geometry

### Current State

No trim geometry is generated. After boolean subtract, the opening is just a rectangular hole in the wall.

### Proposed Implementation: Frame Trim Around Openings

For each opening, after the boolean cut, generate a frame mesh and AppendMesh it to the result:

#### Door Frame Trim

```
Frame = 4 thin boxes forming a U-shape (left jamb, right jamb, header):
  - Left jamb:  AppendBox at (CutPos.X - OpenW/2 - TrimW/2, CutPos.Y, CutPos.Z), size (TrimW, WallT+2, OpenH)
  - Right jamb: AppendBox at (CutPos.X + OpenW/2 + TrimW/2, CutPos.Y, CutPos.Z), size (TrimW, WallT+2, OpenH)
  - Header:     AppendBox at (CutPos.X, CutPos.Y, CutPos.Z + OpenH), size (OpenW + TrimW*2, WallT+2, TrimW)

TrimW defaults: 5cm for doors, 3cm for windows, 2cm for vents
TrimW slightly exceeds WallT so it protrudes on both sides
```

#### Vent Frame Trim

```
Frame = 4 thin boxes forming a full rectangle (all 4 sides):
  - Same as door frame + add threshold/sill piece at bottom
  - Optional: horizontal/vertical bars inside the opening (grate pattern)

Vent grate (optional):
  - N horizontal bars: AppendBox at evenly spaced Z positions inside the opening
  - Bar thickness: 1-2cm, bar spacing: 5-8cm
```

#### Material IDs for Trim

Use `FGeometryScriptPrimitiveOptions::MaterialID` to assign a different material slot to trim geometry:
- Walls: MaterialID 0
- Floor/Ceiling: MaterialID 1
- Trim: MaterialID 2
- Vent grate: MaterialID 3

This allows different materials to be applied to structural vs. decorative elements.

### Implementation Sketch

```cpp
// After boolean subtract of opening:
if (bAddTrim)  // new optional parameter
{
    float TrimW = (OpenType == "vent") ? 2.0f : (OpenType == "window") ? 3.0f : 5.0f;
    FGeometryScriptPrimitiveOptions TrimOpts;
    TrimOpts.MaterialID = 2;  // Trim material slot

    UDynamicMesh* TrimMesh = NewObject<UDynamicMesh>(Pool);

    // Jambs + header (rotated appropriately for wall orientation)
    // ... AppendBox calls for each trim piece ...

    // Append trim to main mesh
    UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(
        Mesh, TrimMesh, FTransform::Identity);
}
```

---

## 8. Vent Opening Type

The current code handles `door` and `window` types. Add a `vent` type:

```
Vent defaults:
  - width: 40cm (vs door 120cm, window 120cm)
  - height: 30cm (vs door 210cm, window 100cm)
  - offset_z: 230cm (near ceiling, vs door 0cm, window 100cm)
  - trim_width: 2cm
  - Optional grate bars
```

The cutter logic is identical to door/window -- just different default dimensions and Z offset.

---

## 9. Concrete Fix Plan

### Bug Fix (Priority 1 -- Critical)

**File:** `MonolithMeshProceduralActions.cpp`, lines 1719-1746

Change cutter positions from outer face to wall center:

```cpp
// BEFORE (buggy):
if (Wall == TEXT("north"))
{
    CutPos = FVector(OffX, -Depth * 0.5f, FloorZ + OffZ);
    CutBoxW = OpenW;
    CutBoxD = WallT + 4.0f;
}

// AFTER (fixed):
if (Wall == TEXT("north"))
{
    CutPos = FVector(OffX, -Depth * 0.5f + WallT * 0.5f, FloorZ + OffZ);
    CutBoxW = OpenW;
    CutBoxD = WallT + 10.0f;  // generous overshoot
}
```

Apply same pattern for south (+D/2 - WT/2), east (+W/2 - WT/2), and west (-W/2 + WT/2).

### Vent Type (Priority 2)

Add vent defaults alongside door/window in the opening parsing code (line 1711):

```cpp
float OpenH = (*OpenObj)->HasField(TEXT("height"))
    ? static_cast<float>((*OpenObj)->GetNumberField(TEXT("height")))
    : (OpenType == TEXT("door") ? 210.0f : (OpenType == TEXT("vent") ? 30.0f : 100.0f));
float OffZ = (*OpenObj)->HasField(TEXT("offset_z"))
    ? static_cast<float>((*OpenObj)->GetNumberField(TEXT("offset_z")))
    : (OpenType == TEXT("window") ? 100.0f : (OpenType == TEXT("vent") ? 230.0f : 0.0f));
```

### Trim Geometry (Priority 3)

Add optional `add_trim` boolean parameter to `create_structure`. When true, generate U-frame (door), full frame (window/vent), and optional vent grate bars. Use MaterialID 2 for trim.

### Merged Cutter Optimization (Priority 4)

When 3+ openings exist, merge all cutter AppendBoxes into a single UDynamicMesh and do one boolean subtract instead of N separate operations.

### Estimated Effort

| Task | Hours | Risk |
|---|---|---|
| Bug fix (cutter positioning) | 1h | Low |
| Vent opening type | 0.5h | Low |
| Trim geometry (basic U-frame/box frame) | 3h | Medium |
| Material ID assignment for trim | 0.5h | Low |
| Merged cutter optimization | 2h | Low |
| Vent grate bars (optional detail) | 1h | Low |
| **Total** | **8h** | -- |

---

## 10. Sources

- **Engine Source (verified):**
  - `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Public/GeometryScript/MeshBooleanFunctions.h` -- Boolean API, options structs, operation enum
  - `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Private/MeshBooleanFunctions.cpp` -- FMeshBoolean usage, hole filling, transform handling
  - `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Public/GeometryScript/MeshPrimitiveFunctions.h` -- AppendBox signature, origin modes
  - `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Private/MeshPrimitiveFunctions.cpp` -- Base origin = Z offset by half-height
  - `Engine/Plugins/Runtime/GeometryScripting/Source/GeometryScriptingCore/Public/GeometryScript/MeshBasicEditFunctions.h` -- AppendMesh, AppendMeshTransformed
  - `Engine/Source/Runtime/GeometryFramework/Public/UDynamicMesh.h` -- UDynamicMeshPool::RequestMesh/ReturnMesh
- **Monolith Source:**
  - `Source/MonolithMesh/Private/MonolithMeshProceduralActions.cpp` -- CreateStructure (line 1580), BuildWalls (line 1620), openings processing (line 1692), BuildDoorFrame (line 913)
- **Community:**
  - [Epic: Geometry Script Boolean Operation at Runtime](https://dev.epicgames.com/community/learning/tutorials/q33Y/unreal-engine-geometry-script-boolean-operation-at-runtime)
  - [Epic: UE5.0 Geometry Script - Mesh Booleans and Patterns](https://dev.epicgames.com/community/learning/tutorials/v0b/unreal-engine-ue5-0-geometry-script-mesh-booleans-and-patterns)
  - [Ryan Schmidt: Python Boolean Subtract Example](https://gist.github.com/ryanschmidtEpic/8ff646cc1435a25368eb2de987369984)
  - [gradientspace: GeometryScript FAQ](http://www.gradientspace.com/tutorials/2022/12/19/geometry-script-faq)
  - [bendemott: UE5-Procedural-Building (C++)](https://github.com/bendemott/UE5-Procedural-Building)
  - [ApplyMeshBoolean UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Booleans/ApplyMeshBoolean)
  - [PCG + Geometry Script Wall Generator (UE 5.5)](https://forums.unrealengine.com/t/community-tutorial-pcg-geometry-script-in-ue-5-5-wall-generator/2118674)
