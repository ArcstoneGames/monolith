# Architectural Feature Attachment Logic Research

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** How procedural architectural features (balconies, porches, fire escapes, ramps) should integrate with the building they attach to -- orientation, wall openings, descriptor-driven placement, self-intersection prevention
**Dependencies:** `FBuildingDescriptor` (`MonolithMeshBuildingTypes.h`), `FExteriorFaceDef`, `FMonolithMeshArchFeatureActions`, facade system, boolean doors research

---

## Executive Summary

The five existing architectural feature actions (`create_balcony`, `create_porch`, `create_fire_escape`, `create_ramp_connector`, `create_railing`) generate standalone geometry with no awareness of the building they attach to. This causes four concrete bugs:

1. **Balcony faces wrong direction** -- geometry extends in local +Y but the caller must manually set `rotation` to face the wall normal. No auto-orient from building context.
2. **Porch has no door cut** -- porch steps lead to a solid wall. The porch action does not cut a door opening in the building.
3. **Fire escape has no window/door access** -- landings are placed at floor heights but no openings are created in the building wall.
4. **Ramp switchback self-intersects** -- when `NumSegments > 1`, ramp runs reverse direction (Y flips via `Direction`) but do NOT offset laterally. Both runs occupy the same XY footprint at different Z heights, and with ADA slopes the vertical clearance between overlapping runs is only `RisePerSeg` (e.g. 76cm), far below the 203cm (80 inch) headroom code minimum.

The fix is an **Attachment Context** system: features optionally consume an `FExteriorFaceDef` (already in the Building Descriptor) to auto-derive orientation, position, and required wall openings. For ramps, the fix is geometric: switchback runs must be laterally offset by `Width + gap`.

---

## 1. Current State Analysis

### 1.1 What the Code Does Now

All five actions in `MonolithMeshArchFeatureActions.cpp` share the same placement model:

```
location: [x, y, z]   -- world position, user-supplied
rotation: degrees      -- yaw only, user-supplied
```

Geometry is built in local space:
- **Balcony:** slab at Z=0, extends in +Y (depth), centered on X. Railing on left/front/right edges.
- **Porch:** floor at Z=PorchFloorZ, extends in +Y (depth). Columns along the front edge (+Y side). Steps extend further in +Y.
- **Fire escape:** landings stacked vertically at Z = (Fi+1)*FloorHeight. All landings centered at X=0. Stairs alternate between -X and +X offsets. Everything extends in +Y.
- **Ramp:** segments go in +Y then -Y (switchback), centered on X=0.

**Key observation:** All features are generated relative to an implicit "wall at Y=0" convention. The wall face is at Y=0, features extend outward in +Y. This is a good convention but it is undocumented and there is no mechanism to transform it from the building's actual wall face.

### 1.2 What the Building Descriptor Provides

`FBuildingDescriptor` already contains `TArray<FExteriorFaceDef> ExteriorFaces` where each face has:

```cpp
struct FExteriorFaceDef
{
    FString Wall;            // "north", "south", "east", "west"
    int32 FloorIndex = 0;
    FVector WorldOrigin;     // bottom-left corner of this face in world space
    FVector Normal;          // outward-facing normal
    float Width = 0.0f;      // face width (cm)
    float Height = 0.0f;     // face height (cm)
};
```

This is **exactly** what features need to auto-orient. The face normal gives the outward direction. The world origin gives the attachment point. The width/height bound where the feature can be placed. This data is already computed by `create_building_from_grid` but never passed to feature actions.

### 1.3 What Houdini / THE FINALS / Archipack Do

Research into real production systems reveals a consistent pattern:

**THE FINALS (Embark Studios / Houdini):**
Feature Nodes use a blockout mesh as a guide. Each feature node receives the building's geometry and a "Boolean Mesh" channel. Features that need wall openings (windows, doors) produce both their own geometry AND a cutter mesh on the boolean channel. Downstream, a Boolean Node subtracts all cutters from the building walls in a single pass. Feature nodes derive their placement and orientation from the building's exterior surface normals.

**Archipack (Blender):**
Windows and doors automatically cut openings in walls using boolean operations. Balconies are generated from wall segments with the fence kept in sync with the slab dimensions. The key pattern: the wall "owns" the attachment, and the attached feature inherits the wall's orientation and position. Auto-boolean ensures the wall gets an opening wherever a door/window/access point is needed.

**CGA Shape Grammar (CityEngine / Muller 2006):**
Context-sensitive rules ensure "doors give out on terraces or the street level" and "terraces are bounded by railings." The grammar tracks which face of the building each element is placed on. The `comp(f)` (component split by face) operation extracts individual faces with their normals, and subsequent rules inherit that face's coordinate frame.

**Common Pattern Across All Systems:**
1. Feature knows which wall face it is on (inherits face normal + bounds)
2. Feature is positioned relative to the face, not world space
3. If feature requires access (door/window), it produces a cutter that is applied to the wall
4. The building orchestrator handles the boolean subtract, not the feature itself

Sources:
- [Making the Procedural Buildings of THE FINALS (SideFX)](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [80.lv: How Embark Studios Built Procedural Environments for THE FINALS](https://80.lv/articles/how-embark-studios-built-procedural-environments-for-the-finals-using-houdini)
- [Archipack Objects Documentation](https://blender-archipack.gitlab.io/user/archipack%20objects.html)
- [Archipack Features](https://blender-archipack.org/features)
- [CGA Shape Grammar (UPC Barcelona)](https://www.cs.upc.edu/~virtual/SGI/docs/1.%20Theory/Unit%2011.%20Procedural%20modeling/CGA%20shape%20grammar.pdf)
- [CityEngine Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm)
- [Procedural Modeling of Buildings (Muller et al., ACM TOG 2006)](https://www.researchgate.net/publication/220183823_Procedural_Modeling_of_Buildings)

---

## 2. Attachment Context System

### 2.1 The FAttachmentContext Struct

A new struct consumed by all feature actions when attaching to a building:

```cpp
struct FAttachmentContext
{
    // Required: which face to attach to
    FVector WallOrigin;      // world-space origin of the target wall face
    FVector WallNormal;      // outward normal of the wall
    float WallWidth;         // wall segment width
    float WallHeight;        // wall segment height (this floor)

    // Optional: building awareness
    float WallThickness;     // for boolean cut depth (exterior wall thickness)
    float FloorHeight;       // for floor-relative Z positioning
    int32 FloorIndex;        // which floor this attachment is on
    FString BuildingId;      // for wall-opening coordination

    // Derived at parse time:
    FTransform WallToWorld;  // transform from local feature space to world space
    FVector WallRight;       // rightward direction along the wall face (cross product)
    FVector WallUp;          // upward direction (typically Z-up)
};
```

### 2.2 How Auto-Orient Works

The feature's local coordinate convention is:
- **+Y** = outward from wall (depth direction)
- **X** = along the wall face
- **Z** = up

The `WallToWorld` transform maps from this local space to world space:

```cpp
FAttachmentContext ParseAttachment(const TSharedPtr<FJsonObject>& Params)
{
    FAttachmentContext Ctx;

    // From explicit wall_normal / wall_origin params
    // OR from building_descriptor + wall + floor_index lookup

    Ctx.WallRight = FVector::CrossProduct(FVector::UpVector, Ctx.WallNormal).GetSafeNormal();
    Ctx.WallUp = FVector::UpVector; // buildings are Z-up

    // Build the transform: local X -> WallRight, local Y -> WallNormal, local Z -> WallUp
    FMatrix M = FMatrix::Identity;
    M.SetAxis(0, Ctx.WallRight);     // local X -> along wall
    M.SetAxis(1, Ctx.WallNormal);    // local Y -> outward
    M.SetAxis(2, Ctx.WallUp);        // local Z -> up
    M.SetOrigin(Ctx.WallOrigin);

    Ctx.WallToWorld = FTransform(M);
    return Ctx;
}
```

When the feature generates geometry at local position `P`, the world position is:

```cpp
FVector WorldPos = Ctx.WallToWorld.TransformPosition(P);
```

This means the balcony, generated with its slab extending in +Y, automatically faces the correct direction regardless of which wall it is on (north/south/east/west or arbitrary angle).

### 2.3 JSON Interface

Two ways to provide attachment context to feature actions:

**Option A: Explicit wall params (standalone use)**
```json
{
    "action": "create_balcony",
    "params": {
        "save_path": "/Game/Town/SM_Balcony_01",
        "width": 200,
        "depth": 120,
        "wall_origin": [1000, 2000, 300],
        "wall_normal": [0, -1, 0],
        "wall_thickness": 15
    }
}
```

**Option B: Building descriptor reference (integrated use)**
```json
{
    "action": "create_balcony",
    "params": {
        "save_path": "/Game/Town/SM_Balcony_01",
        "width": 200,
        "depth": 120,
        "building_id": "house_01",
        "wall": "south",
        "floor_index": 1,
        "wall_position": 0.5
    }
}
```

`wall_position` is a 0-1 parametric position along the wall face (0 = left edge, 0.5 = center, 1 = right edge). The action looks up the building descriptor from the spatial registry, finds the matching `FExteriorFaceDef`, and derives `WallOrigin`, `WallNormal`, etc.

**Backward compatibility:** If neither `wall_normal` nor `building_id` is provided, the action falls back to the existing `location` + `rotation` behavior. Existing call sites are unaffected.

### 2.4 Wall Opening Request System

When a feature requires access through the building wall (balcony door, porch entrance, fire escape window), it should produce a **wall opening request** rather than directly boolean-subtracting the building mesh (which it doesn't have access to).

```cpp
struct FWallOpeningRequest
{
    FString BuildingId;
    FString Wall;           // "north", "south", etc.
    int32 FloorIndex;
    float PositionAlongWall; // center of opening, in cm from wall left edge
    float Width;             // opening width
    float Height;            // opening height
    float SillHeight;        // height above floor for windows
    FString Type;            // "door", "window", "french_door"
};
```

These requests are returned in the action result JSON:

```json
{
    "type": "balcony",
    "asset_path": "/Game/Town/SM_Balcony_01",
    "wall_openings": [
        {
            "building_id": "house_01",
            "wall": "south",
            "floor_index": 1,
            "position_along_wall": 600.0,
            "width": 160,
            "height": 220,
            "sill_height": 0,
            "type": "french_door"
        }
    ]
}
```

An orchestrator action (`attach_features_to_building` or similar) would collect these requests and apply all boolean subtracts to the building wall in a single pass -- matching THE FINALS' pattern of accumulating boolean meshes then applying them in bulk.

Alternatively, a lighter-weight approach: the `create_facade` action (from the facade research) already handles window/door boolean cuts. The attachment system could inject opening definitions into the facade spec before it runs, so the facade generates with the correct openings pre-planned.

---

## 3. Balcony Integration

### 3.1 Current Bug: Faces Wrong Direction

The balcony slab extends in local +Y. The railing is on the left (-X), front (+Y), and right (+X) edges. The wall-facing edge (Y=0) is correctly left open for access.

The bug is NOT in the geometry generation -- it is in the placement. The action uses `location` + `rotation` (yaw only), so the caller must manually compute the correct yaw to face the balcony outward from the building wall. If they forget `rotation`, the balcony faces +Y in world space regardless of which wall it is on.

**Fix:** With attachment context, the `WallToWorld` transform automatically rotates the geometry so +Y (the balcony's outward direction) aligns with the wall normal. No manual rotation needed.

### 3.2 French Door / Access Opening

A balcony without a door is useless. When attachment context is provided:

- **Default opening:** French door, 160cm wide x 220cm tall, centered on the balcony width, sill at floor level (0cm)
- **Override:** `opening_width`, `opening_height`, `opening_type` params
- **Opening types:** `french_door` (default, full-height glass), `sliding_door`, `window` (with sill)
- **Return:** `wall_openings` array in result JSON

### 3.3 Visual Support (Corbels / Brackets)

From building code research:
- Cantilevered balconies typically max at 150-180cm depth
- Backspan-to-cantilever ratio: minimum 2:1 for balconies (structural)
- Corbel outer depth: minimum 0.5x effective depth (ACI 318)

For procedural blockout, decorative brackets are sufficient:

```
Side view of bracket:
    Wall
    |____
    |   /   <- Right triangle bracket
    |  /
    | /
    |/
```

**Implementation:** `AppendSimpleExtrudePolygon` with a right-triangle profile, extruded by bracket width (10-15cm). Two brackets placed at 1/4 and 3/4 of balcony width along the wall face.

**New param:** `support_style`: `none`, `brackets` (default), `corbels` (thicker, decorative), `columns` (full-height, reaches ground -- for stacked balconies or horror rickety stilts)

### 3.4 Balcony Dimensions (Building Code Reference)

| Parameter | Typical | Minimum | Maximum |
|-----------|---------|---------|---------|
| Depth | 120-150cm | 80cm | 200cm |
| Width | 200-400cm | 150cm | wall width |
| Slab thickness | 15-20cm | 12cm | 25cm |
| Railing height | 107cm (42") | 100cm (39") | 115cm |
| Baluster spacing | 10cm max gap | - | 10cm (code) |
| Bracket depth | 40-60cm | 30cm | depth |

Sources:
- [Cantilevered Balconies (DeckExpert)](https://deckexpert.com/article-library/cantilevered-balconies/)
- [Types of Balcony Structures (Balcony Systems)](https://www.balconette.co.uk/glass-balustrade/articles/types-of-methods-of-fixing-balcony-structures)
- [Floor Cantilevers (UpCodes)](https://up.codes/s/floor-cantilevers)
- [Concrete Corbel Design (StructuralCalc)](https://structuralcalc.com/concrete-corbel-design-to-aci-318-14/)

---

## 4. Porch-to-Building Connection

### 4.1 Current Bug: No Door Cut

The porch generates a floor platform, columns, roof, and steps. But it does NOT cut a door opening in the building wall. The steps lead to a solid wall.

**Fix:** When attachment context is provided, the porch action emits a `wall_openings` entry for a door:

```json
{
    "type": "door",
    "width": 100,
    "height": 220,
    "sill_height": 0,
    "position_along_wall": 0.5
}
```

The door is centered on the porch width by default. The opening height accounts for the threshold gap between porch floor and building floor.

### 4.2 Threshold and Landing Code Requirements

From the International Residential Code (IRC) and research:

- **Landing on each side of exterior door:** width >= door width, depth >= 91cm (36")
- **Maximum threshold height:** 3.8cm (1.5") below door for required egress doors
- **Maximum step-down:** 19.7cm (7.75") for non-egress exterior doors
- **Landing slope:** maximum 2% (1/4" per foot) for drainage
- **Porch-to-door alignment:** the porch floor level should be within 3.8cm of the door threshold

**Current code issue:** The porch floor height is `StepCount * StepHeight`, which may not match the building's actual floor height. With attachment context providing `FloorHeight`, the porch floor Z can be set to `FloorHeight - threshold_gap` (where threshold_gap is 0-3.8cm).

Sources:
- [Code-Compliant Landings for Exterior Doors (Fine Homebuilding)](https://www.finehomebuilding.com/2024/05/21/283-landings-for-exterior-doors)
- [Floors and Landings at Exterior Doors (UpCodes)](https://up.codes/s/floors-and-landings-at-exterior-doors)
- [Thresholds at Exterior Doors (UpCodes)](https://up.codes/s/thresholds-at-exterior-doors)

### 4.3 Roof Alignment

The porch roof should connect to the building. Two approaches:

**A. Lean-to (shed roof):**
Porch roof slopes away from the building wall. The high edge is at the building wall, the low edge is at the columns. The high edge should be at or just below the building's eave or fascia line.

```
Side view:
    Building wall
    |  \_____ Porch roof (slopes down from wall)
    |       |
    |  col  |
    |       |
    =========  Porch floor
```

With attachment context providing `FloorHeight` and the building's roof/eave height, the porch roof high-edge Z = `min(building_eave_Z, attachment_floor_Z + max_porch_height)`.

**B. Extension (flat):**
Porch roof is flat, extending from the building wall at a fixed height. Simpler geometry but less realistic.

**New param:** `roof_style`: `flat` (current), `lean_to` (new). `roof_slope` for lean-to angle. Default to `lean_to` when attachment context is available.

### 4.4 Porch Variants for Horror

- **Wrap-around:** multiple porch segments on adjacent walls, connected at corners. Requires coordination between faces.
- **Screened porch:** solid panel railing with mesh texture applied (material, not geometry).
- **Collapsed:** randomly remove columns, tilt the roof slab slightly. `decay` float param (0-1).
- **Double-height entry:** tall columns (2 stories), grand entrance feel. Used for mansion archetype.

---

## 5. Fire Escape Attachment

### 5.1 Current Bug: No Interior Access

The fire escape generates landings at floor heights and stairs between them, but no openings are created in the building wall for windows or doors to access each landing.

### 5.2 Access Point Requirements (Building Code)

From NYC Building Code Section 27-380 and UpCodes research:

| Requirement | Value |
|-------------|-------|
| Access opening minimum width | 61cm (24") |
| Access opening minimum height | 76cm (30") |
| Window sill max height above floor/landing | 76cm (30") above floor, but not more than 30" above landing |
| Landing minimum width | 91cm (3 ft) |
| Landing minimum length | 137cm (4 ft 6 in) |
| Stair minimum width | 56cm (22") |
| Stair riser maximum | 20cm (8") |
| Stair tread minimum | 20cm (8") + 2.5cm nosing |
| Handrail height | 81cm (32") min above tread |
| Guard height | 91cm (36") min on open sides |
| No flight > 366cm (12 ft) between landings | |

Sources:
- [NYC Administrative Code 27-380 (Amlegal)](https://codelibrary.amlegal.com/codes/newyorkcity/latest/NYCadmin/0-0-0-49721)
- [Fire Escape Stairs (UpCodes)](https://up.codes/s/fire-escape-stairs)
- [Windows and Doors to Fire-Escapes (UpCodes)](https://up.codes/s/windows-and-doors-to-fire-escapes)
- [Emergency Egress Window Codes (Thermal Windows)](https://www.thermalwindows.com/blogs/emergency-egress-window-codes-size-height-and-fire-safety)

### 5.3 Window Placement Per Landing

Each landing should produce a window opening request:

```json
{
    "type": "window",
    "width": 70,
    "height": 100,
    "sill_height": 60,
    "position_along_wall": "centered_on_landing"
}
```

The window is centered on the landing's position along the wall. The sill height is 60cm above the floor level (code allows up to 76cm for fire escape access; lower is better for egress).

### 5.4 Facade Conflict Avoidance

Fire escape landings must not block existing facade windows. Two strategies:

**A. Feature-first (fire escape defines windows):** The fire escape declares which window positions it needs. The facade system receives these as "reserved" positions and avoids placing decorative windows there, using the fire escape windows instead.

**B. Facade-first (fire escape adapts to existing windows):** The facade places windows normally. The fire escape reads the facade spec (or the building descriptor's window list) and aligns landings to existing window positions. This is more complex but allows the facade to drive the aesthetic.

**Recommendation:** Feature-first for MVP. The fire escape attachment declares its access windows, and these are injected into the facade spec before facade generation. This matches the CGA pattern where context-sensitive rules ensure "doors give out on terraces."

### 5.5 Fire Escape Geometry Fixes

The current fire escape has a subtle layout issue: all landings are at the same XY position (centered at X=0, extending in +Y). Stairs alternate between -X and +X offsets. This means the fire escape zigzags left-right while the landings stay put.

Real fire escapes instead have **landings that alternate position**, with stairs connecting them diagonally. The current approach works visually but creates an unusual horizontal span. This is a style choice, not a bug -- but the attachment context should compute the total bounding box so the caller knows how much lateral space the fire escape needs.

### 5.6 Bottom Landing: Drop Ladder vs Stairs to Ground

The current code generates full stairs from the first landing to ground level. Real fire escapes typically have:

- **Retractable drop ladder:** A ladder that extends from the lowest landing to the ground, normally retracted up to prevent unauthorized access. For horror: always down, swinging in the wind.
- **Counterweight stairs:** The bottom flight swings down when weight is on it. For gameplay: could be interactive.

**New param:** `bottom_access`: `stairs` (default, current behavior), `ladder` (short ladder from lowest landing), `none` (drop to ground -- horror "no escape" variant)

---

## 6. Ramp Self-Intersection Prevention

### 6.1 Current Bug: Switchback Overlaps

The ramp connector's switchback logic:

```cpp
Direction = -Direction;  // line 993
```

This reverses the Y direction of the next ramp segment, but the X position is always 0. Both the forward (+Y) and backward (-Y) ramp runs are centered on X=0 with the same width. For a switchback, the runs overlap in XY space with only `RisePerSeg` vertical separation (e.g. 76cm for ADA max rise). This violates the 203cm (80") minimum headroom requirement and creates visual self-intersection.

### 6.2 The Fix: Lateral Offset for Switchbacks

Real switchback ramps have runs side by side, connected by a 180-degree turn landing:

```
Plan view (looking down):
    +=========+
    | Landing |
    +---------+=========+
              | Run 2   | (going back -Y)
    +---------+=========+
    | Landing |
    +=========+---------+
    | Run 1   |          (going forward +Y)
    +=========+

Side view:
    Run 2 (higher)    Landing
    __________________|___|

    ___|___________________
    Landing    Run 1 (lower)
```

The runs are offset laterally by `Width + LandingWidth`. For ADA, the landing must be 150x150cm minimum.

**Corrected algorithm:**

```cpp
float LateralOffset = 0.0f;  // accumulated X offset for switchback runs

for (int32 Seg = 0; Seg < NumSegments; ++Seg)
{
    // Ramp run
    float SegStartY = 0.0f;       // always starts at Y=0 (relative to this segment)
    float SegEndY = RunPerSeg;     // always goes in +Y (we offset X to create switchback)
    float SegX = LateralOffset;

    // ... generate ramp at (SegX, 0..RunPerSeg, SegStartZ..SegEndZ) ...

    if (Seg < NumSegments - 1)
    {
        // 180-degree turn landing
        // Landing at (SegX + Width/2 + LandingLen/2, RunPerSeg, SegEndZ)
        // ... or landing spans from Run N's end to Run N+1's start

        LateralOffset += Width + Gap;  // next run is beside this one
    }
}
```

Wait -- the simpler fix that preserves the current Y-reversal approach: **offset each run in X by `(Width + gap)` when direction reverses**:

```cpp
float XOffset = 0.0f;

for (int32 Seg = 0; Seg < NumSegments; ++Seg)
{
    // Use XOffset for this segment's center X
    float SegCenterX = XOffset;

    // ... generate ramp geometry centered at X = SegCenterX ...
    // ... generate ramp going in Direction * +Y ...

    if (Seg < NumSegments - 1)
    {
        // Landing connects this run's end to next run's start
        // Landing is at (midpoint_X, CurrentY, CurrentZ) spanning both X positions

        Direction = -Direction;
        // Offset next run beside this one (not on top of it)
        XOffset += Width + SwitchbackGap;
    }
}
```

Where `SwitchbackGap` = separation between runs (10-20cm minimum, enough for a dividing wall or handrail).

### 6.3 ADA Landing Requirements for Switchbacks

From U.S. Access Board Chapter 4 and building code research:

| Requirement | Value |
|-------------|-------|
| Intermediate landing width | >= ramp width (152cm / 60" clear) |
| Intermediate landing length | >= 152cm (60") in direction of travel |
| Landing at direction change > 30 deg | >= 183cm (72") in direction of travel (California) |
| Headroom above ramp surface | >= 203cm (80") |
| Maximum slope | 1:12 (8.33%) |
| Maximum rise per run | 76cm (30") |
| Handrail extension at top/bottom | 30cm (12") horizontal beyond ramp run |
| Inside handrail on switchback | continuous between runs |
| Edge protection | required on both sides |

Sources:
- [U.S. Access Board Chapter 4: Ramps and Curb Ramps](https://www.access-board.gov/ada/guides/chapter-4-ramps-and-curb-ramps/)
- [Exterior Ramps and Landings (UpCodes)](https://up.codes/s/exterior-ramps-and-landings-on-accessible-routes)
- [ADA Ramp Requirements (Accessibility Checker)](https://www.accessibilitychecker.org/blog/ada-requirements-for-ramps/)

### 6.4 Headroom Verification

After generating switchback geometry, verify that no point on a lower ramp surface is less than 203cm below the bottom surface of an upper ramp. With ADA 1:12 slope and 76cm max rise, the vertical gap between stacked runs is 76cm -- drastically below the 203cm headroom minimum. This confirms that lateral offset (not vertical stacking) is the only valid approach for switchbacks.

For reference, how much lateral space does a switchback ramp need?

```
Rise = 200cm (typical building floor)
Max rise per run = 76cm -> 3 segments
Run per segment = 76 / (1/12) = 912cm (9.12m!)
Width = 120cm
Gap = 20cm

Total lateral footprint = 3 * 120 + 2 * 20 = 400cm (4m wide)
Total Y extent = max(912, 152) = 912cm (9.12m long)

This is a MASSIVE structure. Real buildings use stairs for most of the height
and ramps only for the last 30-50cm, or dedicated elevator access.
```

**Recommendation:** For rises > 76cm, warn the caller that the ramp will be very large. Suggest stairs with a ramp at the landing level for hospice accessibility. For the pure ramp, the geometry should be correct but the caller should understand the space requirements.

---

## 7. Building Descriptor as Feature Context

### 7.1 What Features Need from the Descriptor

| Feature | Needs from Descriptor |
|---------|----------------------|
| **Balcony** | Wall normal (orientation), wall origin (position), floor height (Z placement), wall thickness (boolean cut depth) |
| **Porch** | Wall normal, ground floor height, wall thickness, entry door location |
| **Fire escape** | Wall normal, floor heights array, wall thickness, number of floors, window avoidance zones |
| **Ramp** | Ground level Z, target floor Z, adjacent wall normal (if wall-attached) |
| **Railing** | Path points (derived from other features), height |

### 7.2 Proposed Param Extensions

Add these optional params to all five feature actions:

```json
{
    "// Attachment context (any feature)": "",
    "wall_normal": [0, -1, 0],
    "wall_origin": [x, y, z],
    "wall_thickness": 15,

    "// Building reference (looks up descriptor from spatial registry)": "",
    "building_id": "house_01",
    "wall": "south",
    "floor_index": 1,
    "wall_position": 0.5,

    "// Wall opening control": "",
    "cut_opening": true,
    "opening_type": "french_door",
    "opening_width": 160,
    "opening_height": 220
}
```

**Priority of context sources:**
1. If `building_id` is provided -> look up descriptor from spatial registry, derive wall_normal/origin/thickness
2. If `wall_normal` + `wall_origin` are provided -> use directly
3. If neither -> fall back to `location` + `rotation` (current behavior)

### 7.3 Integration with Spatial Registry

The spatial registry (from `reference_spatial_registry_research.md`) stores building descriptors indexed by `building_id`. Feature actions query:

```cpp
FBuildingDescriptor Desc = SpatialRegistry->GetBuildingDescriptor(BuildingId);
FExteriorFaceDef Face = FindExteriorFace(Desc, WallName, FloorIndex);
```

This lookup is O(1) by building ID + wall name.

### 7.4 Orchestration: attach_features_to_building

A new high-level action that takes a building ID and a list of feature specs, then:

1. Looks up the building descriptor
2. For each feature, derives attachment context from the descriptor
3. Calls the individual feature actions with attachment context
4. Collects all wall opening requests
5. Applies boolean subtracts to the building wall meshes in a single pass
6. Returns the complete result (all feature actors + modified building mesh)

```json
{
    "action": "attach_features_to_building",
    "params": {
        "building_id": "house_01",
        "features": [
            {
                "type": "balcony",
                "wall": "south",
                "floor_index": 2,
                "wall_position": 0.5,
                "width": 200,
                "depth": 120,
                "save_path": "/Game/Town/SM_Balcony_House01_S2"
            },
            {
                "type": "porch",
                "wall": "south",
                "floor_index": 0,
                "wall_position": 0.5,
                "width": 300,
                "depth": 200,
                "save_path": "/Game/Town/SM_Porch_House01"
            },
            {
                "type": "fire_escape",
                "wall": "north",
                "floor_index": 0,
                "wall_position": 0.8,
                "floor_count": 3,
                "save_path": "/Game/Town/SM_FireEscape_House01"
            }
        ]
    }
}
```

This is the recommended end-state but NOT required for the initial fix. Phase 1 should just add attachment context to individual actions. Phase 2 adds the orchestrator.

---

## 8. Specific Bug Fixes

### 8.1 Balcony Orientation Fix

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreateBalcony()`

**Current:** Geometry built in local space, placed via `SaveAndPlace()` which uses `location` + `rotation`.

**Fix:**
1. Parse attachment context (new helper function)
2. If context is available, transform all geometry vertices from local to world using `Ctx.WallToWorld`
3. Add bracket geometry under the slab (new)
4. Generate `wall_openings` result for french door
5. If context is NOT available, use existing `location` + `rotation` path

**Estimated changes:** ~80 lines new code in `CreateBalcony()`, ~60 lines shared `ParseAttachmentContext()` helper, ~20 lines bracket geometry builder.

### 8.2 Porch Door Cut Fix

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreatePorch()`

**Current:** Steps extend in +Y from the porch front edge. No door is generated.

**Fix:**
1. Parse attachment context
2. If context available: set porch floor Z = `Ctx.FloorHeight * Ctx.FloorIndex` (match building floor)
3. Auto-calculate step count from porch floor Z to ground: `StepCount = ceil(PorchFloorZ / StepHeight)`
4. Generate `wall_openings` result for entry door (centered on porch)
5. Steps should extend AWAY from the wall (outward), not in +Y blindly. With attachment context, steps face the wall normal direction.
6. Roof high edge attaches at the building wall, slopes down to columns (lean-to)

**Estimated changes:** ~50 lines new code.

### 8.3 Fire Escape Window Fix

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreateFireEscape()`

**Current:** Landings at each floor but no window openings.

**Fix:**
1. Parse attachment context
2. For each landing, generate a `wall_openings` entry for a window (70x100cm, sill at 60cm)
3. Auto-derive `floor_height` from descriptor's `Floors[i].Height`
4. Position landings at the building's actual floor heights (not just `Fi * FloorHeight`)
5. Return all window requests in result JSON

**Estimated changes:** ~40 lines new code.

### 8.4 Ramp Self-Intersection Fix

**File:** `MonolithMeshArchFeatureActions.cpp`, `CreateRampConnector()`

**Current:** Switchback segments reverse Y direction but stay at X=0.

**Fix:**
1. When `NumSegments > 1`, offset each segment laterally: `XOffset += (Width + SwitchbackGap)` per segment
2. Landings at switchback turns must span from current run's end X to next run's start X
3. Landing dimensions: `Width` (in run direction, >= 152cm for ADA) x `(Width + SwitchbackGap)` (lateral span)
4. Continuous handrail on the inside of the switchback
5. Add `switchback_gap` optional param (default 20cm)
6. Verify headroom: if lateral offset approach, headroom is infinite (runs are side by side, not stacked)

**Estimated changes:** ~60 lines modified, ~30 lines new.

---

## 9. Implementation Plan

### Phase 1: Core Attachment Context (~12h)

1. **New shared helper:** `ParseAttachmentContext()` -- parses `wall_normal`, `wall_origin`, `wall_thickness` OR `building_id` + `wall` + `floor_index` from params. Returns `FAttachmentContext` with `WallToWorld` transform. (~4h)

2. **Ramp switchback fix** -- lateral offset for switchback segments. This is a pure geometry fix, no attachment context needed. (~3h)

3. **Balcony auto-orient** -- use `WallToWorld` when attachment context is available. Add bracket geometry. (~3h)

4. **Wall opening result** -- all features return `wall_openings` array when attachment context is available. (~2h)

### Phase 2: Building-Aware Placement (~10h)

5. **Porch floor alignment** -- match building floor height, auto-calculate steps. Lean-to roof. (~4h)

6. **Fire escape window requests** -- per-landing window openings, variable floor heights from descriptor. (~3h)

7. **Spatial registry lookup** -- `building_id` param resolves to `FBuildingDescriptor` via spatial registry query. (~3h)

### Phase 3: Orchestrator (~8h)

8. **`attach_features_to_building` action** -- batch feature attachment with coordinated wall boolean cuts. (~6h)

9. **Facade integration** -- inject feature access windows into facade spec before facade generation. (~2h)

### Total: ~30h across 3 phases

---

## 10. Horror Considerations

### 10.1 Decay Parameter

All features should accept a `decay` float (0.0 = pristine, 1.0 = destroyed):

| Decay | Balcony | Porch | Fire Escape | Ramp |
|-------|---------|-------|-------------|------|
| 0.0 | Perfect | Perfect | Perfect | Perfect |
| 0.2 | Rust stains (material) | Creaky (material) | Light rust (material) | Cracks (material) |
| 0.5 | Missing balusters, tilt | Missing column, sagging roof | Missing rungs, bent railing | Broken handrail |
| 0.8 | Partially collapsed slab | Collapsed section | Detached from wall | Collapsed section |
| 1.0 | Only brackets remain | Rubble pile | Hanging by one bolt | Impassable |

Geometry-level decay (missing pieces, tilt) is Phase 3+. Material-level decay (rust, cracks) is achievable now with material slot parameters.

### 10.2 Horror-Specific Feature Patterns

- **The Hanging Fire Escape:** Partially detached, tilted at an angle. Landing separated from wall by 30cm gap. Player must jump across.
- **The Blocked Porch:** Door is boarded up. Steps are intact but lead nowhere. Use `opening_type: "boarded"`.
- **The Missing Balcony:** Only the brackets remain bolted to the wall. The slab fell. `decay: 0.95`.
- **The Infinite Ramp:** A ramp that seems to go on forever (procedural extension). Disorienting.

### 10.3 Hospice Accessibility

All features with the `accessibility: "hospice"` flag should:
- Never exceed ADA slope limits on ramps
- Always include handrails (both sides)
- Ensure wide enough passages (91cm+ clear width)
- Provide ramp alternatives to all stair-only access
- Use high-contrast materials at level changes
- Fire escape: ensure window openings are large enough for wheelchair egress (wider opening)

---

## 11. Related Research Cross-References

| Topic | Document | Relevance |
|-------|----------|-----------|
| Boolean door cuts | `2026-03-28-boolean-doors-research.md` | Wall opening cutter offset bug (WT/2 fix) |
| Facade generation | `2026-03-28-facade-window-research.md` | Window/door placement algorithm, material zones |
| Building descriptor | `2026-03-28-connected-room-assembly-research.md` | `FBuildingDescriptor` structure, shared walls |
| Terrain adaptive | `2026-03-28-terrain-adaptive-buildings-research.md` | Sections 4.1-4.3 (balcony/porch/fire escape dimensions) |
| Spatial registry | `2026-03-28-spatial-registry-research.md` | Building lookup by ID, coordinate conventions |
| Roof generation | `2026-03-28-roof-generation-research.md` | Eave height for porch roof alignment |
| Auto volumes | `2026-03-28-auto-volumes-research.md` | NavMesh around exterior features |
| Proc building algos | `2026-03-28-proc-building-algorithms-research.md` | Feature placement in building pipeline |

---

## Sources

### Academic / Industry
- [Procedural Modeling of Buildings (Muller et al., ACM TOG 2006)](https://www.researchgate.net/publication/220183823_Procedural_Modeling_of_Buildings)
- [CGA Shape Grammar (UPC Barcelona, 2014)](https://www.cs.upc.edu/~virtual/SGI/docs/1.%20Theory/Unit%2011.%20Procedural%20modeling/CGA%20shape%20grammar.pdf)
- [Declarative Procedural Generation of Architecture (IEEE, 2020)](https://ieeexplore.ieee.org/document/9231561/)
- [Algorithmic Beauty of Buildings (Trinity U Honors Thesis)](https://digitalcommons.trinity.edu/cgi/viewcontent.cgi?article=1003&context=compsci_honors)
- [Exploring Procedural Generation of Buildings (Taljsten, Diva Portal)](https://www.diva-portal.org/smash/get/diva2:1480518/FULLTEXT01.pdf)

### Game Development
- [Making the Procedural Buildings of THE FINALS (SideFX)](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [Procedural Generation of Architecture & Props (80.lv)](https://80.lv/articles/006sdf-using-houdini-to-simulate-buildings)
- [Procedural Buildings in UE4: Logic Behind the Tool (80.lv)](https://80.lv/articles/procedural-buildings-in-ue4-logic-behind-the-tool)
- [UE4 House Generation with Houdini (Polycount)](https://polycount.com/discussion/206653/wip-ue4-house-generation-with-houdini)
- [Procedural Building Generator (The Rookies)](https://www.therookies.co/entries/35552)

### Building Codes
- [U.S. Access Board Chapter 4: Ramps and Curb Ramps](https://www.access-board.gov/ada/guides/chapter-4-ramps-and-curb-ramps/)
- [NYC Admin Code 27-380: Fire Escapes](https://codelibrary.amlegal.com/codes/newyorkcity/latest/NYCadmin/0-0-0-49721)
- [Fire Escape Stairs (UpCodes)](https://up.codes/s/fire-escape-stairs)
- [Windows and Doors to Fire-Escapes (UpCodes)](https://up.codes/s/windows-and-doors-to-fire-escapes)
- [Floors and Landings at Exterior Doors (UpCodes)](https://up.codes/s/floors-and-landings-at-exterior-doors)
- [Thresholds at Exterior Doors (UpCodes)](https://up.codes/s/thresholds-at-exterior-doors)
- [Floor Cantilevers (UpCodes)](https://up.codes/s/floor-cantilevers)
- [ADA Requirements for Ramps (Accessibility Checker)](https://www.accessibilitychecker.org/blog/ada-requirements-for-ramps/)
- [Code-Compliant Landings for Exterior Doors (Fine Homebuilding)](https://www.finehomebuilding.com/2024/05/21/283-landings-for-exterior-doors)

### Tools / Addons
- [Archipack Objects Documentation](https://blender-archipack.gitlab.io/user/archipack%20objects.html)
- [Archipack Features](https://blender-archipack.org/features)
- [CityEngine Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm)

### Structural
- [Cantilevered Balconies (DeckExpert)](https://deckexpert.com/article-library/cantilevered-balconies/)
- [Types of Balcony Structures (Balcony Systems)](https://www.balconette.co.uk/glass-balustrade/articles/types-of-methods-of-fixing-balcony-structures)
- [Concrete Corbel Design ACI 318-14 (StructuralCalc)](https://structuralcalc.com/concrete-corbel-design-to-aci-318-14/)
- [Fire Escape Design and History (InspectApedia)](https://inspectapedia.com/Stairs/Fire-Escapes.php)
