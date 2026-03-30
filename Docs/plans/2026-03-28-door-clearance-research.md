# Minimum Clearance Validation & Door Opening Logic for Procedural Buildings

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Player capsule clearance, door width validation, exterior entrance generation, boolean depth, industry approaches

---

## Executive Summary

Our procedural building system has three interrelated issues: doors too narrow for the player capsule, no guaranteed exterior entrance, and boolean subtract cuts that may not fully penetrate walls. This document provides concrete numbers, validated algorithms, and implementation recommendations based on UE5 defaults, ADA accessibility standards, FPS level design conventions, and industry proc-gen approaches.

**Key numbers to remember:**
- UE5 default capsule: **34cm radius**, **88cm half-height** (176cm total, 68cm diameter)
- Minimum traversable door width: **90cm** (allows 68cm capsule + 11cm margin each side)
- Comfortable door width: **100-110cm** (recommended default)
- Hospice/wheelchair door width: **120cm minimum** (ADA: 81.3cm clear, but game scale + comfort = 120cm)
- Minimum corridor width: **150cm** comfortable, **180-200cm** hospice mode
- Boolean cutter depth: must be `WallThickness + 10cm` minimum, centered on wall midpoint

---

## 1. Player Capsule Dimensions & Clearance Standards

### 1.1 UE5 Default Capsule

The UE5 default `ACharacter` capsule component ships with:

| Property | Default Value | Notes |
|----------|--------------|-------|
| Capsule Radius | **34.0 cm** | Radius of hemispheres + cylinder |
| Capsule Half-Height | **88.0 cm** | Center to top/bottom of hemisphere |
| Total Height | **176.0 cm** | Full standing height |
| Diameter | **68.0 cm** | Width the capsule occupies |

The user's hint mentioned 42cm radius / 96cm half-height, which is the **Game Animation Sample** (GAS) character that Leviathan is based on. This is a slightly larger capsule:

| Property | GAS Value | Diameter |
|----------|-----------|----------|
| Capsule Radius | **42.0 cm** | **84.0 cm** |
| Capsule Half-Height | **96.0 cm** | 192cm total height |

**For Leviathan, use 42cm radius (84cm diameter) as the design target.** This is the actual player character.

### 1.2 Minimum Door Width for Traversal

The player capsule is a cylinder in XY with radius R. For the capsule to pass through a door opening of width W:

```
W >= 2R + buffer

Where:
  R = capsule radius (42cm for Leviathan)
  buffer = margin for diagonal approach + floating point tolerance

Absolute minimum: W >= 2 * 42 = 84cm (zero tolerance, head-on only)
Practical minimum: W >= 84 + 6 = 90cm (3cm buffer each side, allows slight angle)
Comfortable:       W >= 84 + 26 = 110cm (13cm buffer, any approach angle)
```

**Level design convention** (The Level Design Book, World of Level Design): doors should be **10-20cm wider than the player diameter**, and always playtested. The formula `diameter + 20cm` gives us **104cm** which rounds nicely to **~100-110cm**.

### 1.3 Minimum Corridor Width

The Level Design Book states: "The minimum hallway width should be at least double the player width. Even then, it will feel a bit narrow and uncomfortable."

| Scenario | Width | Formula | Notes |
|----------|-------|---------|-------|
| Absolute minimum | 84cm | 2R | Player barely fits, scrapes walls |
| Playable minimum | 100cm | 2R + 16cm | Tight but traversable, horror feel |
| Comfortable corridor | 150cm | ~3.5R | Doesn't feel claustrophobic |
| Two players can pass | 170cm | 4R + 2cm | Multiplayer consideration |
| Wheelchair / hospice | 200cm | Per ADA + scale | 4 grid cells at 50cm |

**Current implementation analysis:** The floor plan generator uses `CorridorWidth = bHospiceMode ? 4 : 2` cells at 50cm each. That gives:
- Normal mode: 100cm (2 cells) -- tight but workable for 84cm capsule (16cm clearance)
- Hospice mode: 200cm (4 cells) -- generous, matches ADA guidelines

**Issue:** Normal mode 100cm corridors only give 8cm clearance per side with a 84cm capsule. This is too tight for comfortable play. **Recommend bumping normal to 3 cells (150cm).**

### 1.4 ADA / Wheelchair Accessibility Standards

Real-world ADA requirements (translated to game scale):

| Standard | Real-World | Game Scale (1:1 cm) | Notes |
|----------|-----------|---------------------|-------|
| Min door clear width | 81.3cm (32") | 81.3cm | Measured from face of open door to opposite stop |
| Standard wheelchair width | 66-71cm (26-28") | 66-71cm | Standard manual wheelchair |
| Hallway minimum | 91.4cm (36") | 91.4cm | ADA minimum for wheelchair passage |
| Hallway for passing | 152.4cm (60") | 152.4cm | Two wheelchairs can pass |
| Turning space | 152.4cm (60") diameter | 152.4cm | Full 360 wheelchair turn |
| Threshold height | 1.27cm (0.5") max | 1.27cm | New construction limit |

**For hospice mode:** Our 100cm door width and 200cm corridor width already exceed ADA minimums. Good. But the normal-mode 90cm door width is only 6cm above the real ADA minimum, which leaves almost no room for the game capsule's larger-than-human-body collision. **Hospice door width should be 120cm minimum** (current: 100cm -- borderline).

### 1.5 Door Height

Standard door height is 203-213cm (80-84"). UE5 convention and our current default of **220cm** is good. The 42cm-radius capsule at 192cm total height clears 220cm with 28cm headroom.

### 1.6 Recommended Dimension Table

| Element | Normal Mode | Hospice Mode | Constant Name |
|---------|-------------|--------------|---------------|
| Door width | 100cm | 120cm | `kDoorWidthNormal` / `kDoorWidthHospice` |
| Door height | 220cm | 220cm | `kDoorHeight` |
| Corridor width | 150cm (3 cells) | 200cm (4 cells) | -- |
| Min room dimension | 200cm | 250cm | -- |
| Player capsule radius | 42cm | 42cm | -- |
| Player capsule diameter | 84cm | 84cm | -- |
| Min clearance buffer | 8cm per side | 18cm per side | -- |

---

## 2. Door Placement Validation Algorithms

### 2.1 Post-Placement Capsule Sweep Test

The gold standard for validating door traversability is a **capsule sweep** (also called capsule trace or sweep test) through the door opening. This is the same technique used for navmesh agent validation.

**Algorithm:**

```
ValidateDoorTraversable(DoorDef, WallMesh, CapsuleRadius, CapsuleHalfHeight):
    // 1. Compute sweep start/end positions on each side of the door
    DoorCenter = Door.WorldPosition
    WallNormal = GetWallNormal(Door.Wall)  // e.g., (0,1,0) for north wall

    // Start 50cm before the wall, end 50cm after
    SweepStart = DoorCenter - WallNormal * (WallThickness/2 + 50)
    SweepEnd   = DoorCenter + WallNormal * (WallThickness/2 + 50)

    // 2. Trace a capsule through the opening
    CapsuleShape = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight)
    bBlocked = World->SweepSingleByChannel(
        HitResult, SweepStart, SweepEnd,
        FQuat::Identity, ECC_Pawn, CapsuleShape)

    // 3. If blocked, the door opening is too small or partially obstructed
    if (bBlocked):
        return { Valid: false, BlockPoint: HitResult.ImpactPoint }

    // 4. Also test diagonal approach (45 degrees) -- players rarely approach head-on
    for Angle in [-45, -30, -15, 15, 30, 45]:
        RotatedStart = RotateAroundZ(SweepStart, DoorCenter, Angle)
        bBlocked = World->SweepSingleByChannel(...)
        if (bBlocked):
            return { Valid: false, Angle: Angle }

    return { Valid: true }
```

**UE5 API:** `UWorld::SweepSingleByChannel()` or `SweepMultiByChannel()`. Works at editor-time with collision data present.

**Caveat:** Requires collision mesh to be built first. For procedural geometry, this means: generate mesh -> build collision (SetStaticMeshCollisionFromMesh) -> run sweep test -> if fails, widen door and regenerate.

### 2.2 Geometric Width Check (No Physics Required)

For situations where collision isn't yet built (our primary case since we generate geometry procedurally), a pure geometric check is faster:

```
ValidateDoorWidth(DoorDef, WallThickness, CapsuleRadius):
    MinWidth = CapsuleRadius * 2 + MIN_CLEARANCE_BUFFER  // e.g., 84 + 16 = 100cm

    if Door.Width < MinWidth:
        Door.Width = MinWidth  // Auto-fix
        Log("Door %s widened from %f to %f for clearance", Door.Id, OldWidth, MinWidth)

    // Also check that the door doesn't exceed wall segment length
    WallSegmentLength = ComputeSharedEdgeLength(Door)
    if Door.Width > WallSegmentLength - DoorFrameWidth * 2:
        // Door wider than the wall segment -- invalid placement
        return { Valid: false, Reason: "Door wider than wall segment" }

    return { Valid: true }
```

This is what we should do **at generation time** before cutting any booleans.

### 2.3 Wall-to-Wall Distance Check at Door Location

When a door is placed between two rooms, the actual traversable width may be narrower than `Door.Width` if:
- Adjacent wall segments protrude into the opening (T-junctions)
- Door frame geometry reduces the clear opening
- Furniture or other obstructions are near the door

**Algorithm for wall-to-wall check:**

```
CheckClearanceAtDoor(DoorDef, Building):
    // Get the two wall surfaces bounding the door opening
    // For a door on a north-south wall at X = WallPos:
    //   Left bound = max(wall_surface_to_left_of_door)
    //   Right bound = min(wall_surface_to_right_of_door)

    // In grid terms:
    DoorCenterOnWall = (Door.EdgeStart + Door.EdgeEnd) / 2 * CellSize
    HalfWidth = Door.Width / 2

    LeftClear = DoorCenterOnWall - HalfWidth
    RightClear = DoorCenterOnWall + HalfWidth

    // Check for any wall geometry that intrudes into the opening
    // (e.g., a perpendicular wall ending at the door position)
    for each WallSegment adjacent to door:
        if WallSegment.Intersects(DoorOpening):
            EffectiveWidth -= WallSegment.Thickness

    if EffectiveWidth < CapsuleRadius * 2 + buffer:
        return FAIL
```

### 2.4 "Walk Test" Validation

The most robust approach, used by games like **Left 4 Dead** and **Dead Space**, is to simulate an AI agent walking through every door after generation:

```
WalkTest(Building):
    // Build navmesh over generated geometry
    RebuildNavMesh()

    // For each door, try to find a path through it
    for each Door in Building.Doors:
        RoomA_Center = GetRoomCenter(Door.RoomA)
        RoomB_Center = GetRoomCenter(Door.RoomB)

        Path = FindPath(RoomA_Center, RoomB_Center)

        if Path is empty:
            MarkDoorInvalid(Door)
        else:
            // Check if the path actually goes through this door
            // (it might route through a different door)
            if !PathPassesThrough(Path, Door.WorldPosition, Tolerance=50cm):
                MarkDoorRedundant(Door)
```

This is an expensive post-generation pass but catches all problems. Best used as a debug/validation tool rather than in the hot path.

---

## 3. Exterior Entrance Generation

### 3.1 The Problem

Our floor plan generator (`MonolithMeshFloorPlanGenerator.cpp`) generates interior doors between adjacent rooms and corridor-to-room doors, but has **no explicit logic to create an exterior entrance**. The facade system (`MonolithMeshFacadeActions.cpp`, line 990) does place exterior doors on ground-floor faces, but this is a downstream decoration step -- the floor plan itself doesn't guarantee an exterior-accessible room.

### 3.2 How Proc-Gen Systems Ensure Exterior Access

**Approach 1: Mandatory Entrance Room (Recommended)**

The most reliable method, used by virtually all real implementations:

1. Designate one room type as "entry" / "lobby" / "foyer"
2. During floor plan generation, require that this room touches at least one exterior wall
3. Place a door on that exterior wall facing outward

```
EnsureExteriorEntrance(Grid, Rooms, Doors):
    // 1. Find rooms that touch the building perimeter
    PerimeterRooms = []
    for each Room in Rooms:
        for each Cell in Room.GridCells:
            if IsPerimeterCell(Cell, GridW, GridH):
                PerimeterRooms.Add(Room)
                break

    // 2. Prefer corridor or lobby if one exists on the perimeter
    EntryRoom = null
    for Room in PerimeterRooms:
        if Room.Type in ["lobby", "corridor", "foyer", "entry"]:
            EntryRoom = Room
            break

    // 3. Otherwise pick the largest perimeter room
    if EntryRoom is null:
        EntryRoom = PerimeterRooms.SortByArea().Last()

    // 4. Find the best exterior wall cell for the entrance
    BestCell = null
    BestWall = ""
    for Cell in EntryRoom.GridCells:
        for Dir in [South, East, West, North]:  // South preferred (front of building)
            Neighbor = Cell + Dir
            if IsOutsideGrid(Neighbor, GridW, GridH):
                BestCell = Cell
                BestWall = DirToWall(Dir)
                break  // Take first found on preferred face

    // 5. Create exterior door
    ExteriorDoor = FDoorDef()
    ExteriorDoor.DoorId = "entrance_01"
    ExteriorDoor.RoomA = EntryRoom.RoomId
    ExteriorDoor.RoomB = "exterior"  // Special marker
    ExteriorDoor.Width = bHospiceMode ? 140 : 110  // Wider than interior doors
    ExteriorDoor.Height = 240  // Taller exterior doors
    ExteriorDoor.bExterior = true
    Doors.Add(ExteriorDoor)
```

**Approach 2: Graph-Based Connectivity Verification**

After placing all doors, verify the building's room graph is connected to the exterior:

```
VerifyExteriorConnectivity(Rooms, Doors):
    // Build adjacency graph
    Graph = BuildRoomGraph(Rooms, Doors)

    // Check if any door connects to "exterior"
    ExteriorDoors = Doors.Where(d => d.RoomB == "exterior" || d.bExterior)

    if ExteriorDoors.IsEmpty():
        // No exterior entrance! Fix it.
        AddExteriorEntrance(...)

    // Verify every room can reach the exterior via the door graph
    ExteriorRooms = ExteriorDoors.Select(d => d.RoomA)
    Reachable = BFS(Graph, ExteriorRooms)

    Unreachable = Rooms.Except(Reachable)
    if Unreachable.Any():
        // Add doors to connect unreachable rooms
        for Room in Unreachable:
            AddDoorToNearestReachable(Room, Reachable)
```

**Approach 3: Entrance-First Generation** (from Freiknecht & Effelsberg, IEEE 2019)

"An entrance is a feature common to all possible floor plans -- every house shares the common feature of a door, typically occurring on the front of the house, making the front door the logical first step in constructing a house."

This means: place the entrance FIRST, then generate the rest of the building outward from it. The entrance room becomes the seed, and all subsequent rooms must maintain a connected path back to it.

### 3.3 Porch/Entry Alignment

Once an exterior door is placed, the exterior geometry needs:

1. **Boolean cut on the exterior wall** at the door position (same as interior doors but on the outer face)
2. **Facade door frame** -- our facade system already handles this at line 466-520 of `MonolithMeshFacadeActions.cpp`
3. **Optional porch geometry** -- a slab extending outward from the door:

```
BuildPorch(ExteriorDoor, BuildingDescriptor):
    PorchWidth = Door.Width + 60  // 30cm overhang each side
    PorchDepth = 120  // 120cm deep outward
    PorchHeight = 15  // 15cm raised slab

    // Position porch just outside the exterior wall
    PorchPos = Door.WorldPosition + WallNormal * (WallThickness/2 + PorchDepth/2)

    AppendBox(PorchMesh, PorchWidth, PorchDepth, PorchHeight, PorchPos)

    // Add step if building is raised
    if BuildingFloorZ > GroundLevel + 5:
        AddSteps(PorchPos, BuildingFloorZ - GroundLevel, StepRise=18, StepRun=28)
```

### 3.4 Recommendations for Our System

1. **Add `bRequireExteriorEntrance` flag** (default true) to floor plan generator
2. **Place entrance door during `PlaceDoors()`**, not as a facade afterthought
3. **Mark entrance doors with `bExterior = true`** so downstream systems (auto-volumes, spatial registry) can find them
4. **Register exterior doors** in `ExteriorDoorIds` on the `FSpatialBuilding` (already exists in spatial registry)
5. **Entrance room should touch the "south" edge** by convention (front of building faces south in our coordinate system)

---

## 4. Boolean Subtract Depth for Doors

### 4.1 The Core Problem

When cutting a door opening through a wall using boolean subtraction, the cutter box must be deep enough to penetrate the **entire wall thickness** from both sides. A cutter that's too shallow creates:
- A blind recess instead of a through-hole
- Thin geometry artifacts at the remaining wall slab
- Visual glitches where the player can see through but not walk through (or vice versa)

### 4.2 Current Implementation Status

**`create_building_from_grid` path** (MonolithMeshBuildingActions.cpp, line 703):
```cpp
float CutterOvershoot = WallT + 10.0f;  // e.g., 25cm for 15cm exterior wall
```
The cutter is positioned at `WallPos` (computed from grid edge position), and the overshoot of `WallT + 10` should cleanly cut through. **This path looks correct.**

**`create_structure` path** (MonolithMeshProceduralActions.cpp, line ~1719):
This path has the **critical bug** documented in `reference_boolean_doors_research.md`: the cutter is centered on the wall's outer face instead of the wall center. For a 20cm wall with a 24cm cutter, only 12cm penetrates into the 20cm wall, leaving 8cm uncut.

### 4.3 Rules for Clean Boolean Penetration

**Rule 1: Cutter must be centered on the wall midpoint, not on a face.**

```
Wall center = WallOuterFace - WallNormal * (WallThickness / 2)

// For a north wall at Y = -D/2:
WallCenter.Y = -D/2 + WallThickness/2
```

**Rule 2: Cutter depth must exceed wall thickness with generous margin.**

```
CutterDepth = WallThickness + 10.0cm  // minimum
CutterDepth = WallThickness * 2.0     // safer, handles misalignment
```

The 10cm overshoot (5cm each side beyond the wall) ensures:
- Floating point tolerance (sub-millimeter)
- Slight wall position misalignment from grid snapping
- Boolean engine tolerance (GeometryScript fills holes, but coplanar faces cause issues)

**Rule 3: Cutter must extend below the floor plane.**

A door cutter that starts exactly at FloorZ can leave a thin sliver of floor material at the threshold. Extend the cutter 2-5cm below:

```
CutterBottomZ = FloorZ - 3.0cm  // Cuts slightly into floor to eliminate threshold sliver
CutterHeight = DoorHeight + 3.0cm  // Add the below-floor extension
```

**Rule 4: Avoid coplanar faces.**

If the cutter face is exactly aligned with a wall face, the boolean engine may produce degenerate geometry. The overshoot margin (Rule 2) naturally avoids this.

**Rule 5: Cutter width should match door width exactly (or +1cm tolerance).**

Don't make the cutter wider than the intended door opening -- that would cut into adjacent wall segments. The width dimension should be `DoorWidth + 1.0cm` (0.5cm tolerance each side) at most.

### 4.4 Visual Diagram

```
Cross-section through wall at door height (top-down):

WRONG (cutter centered on outer face):
    +-----------+
    |   WALL    |       Cutter box
    |  20cm     |    +---------+
    |           |    | 12cm    | 12cm
    +-----------+    +---------+
    ^outer      ^inner
    Cutter center is here -- only 12cm of 20cm wall gets cut

CORRECT (cutter centered on wall midpoint):
         +-----------+
         |   WALL    |
    +----| - 20cm  --|-+
    |    |           |  |  Cutter: 30cm deep, centered on wall
    +----|-----------|-+
         ^outer      ^inner
         Center of cutter = center of wall
```

---

## 5. Industry Approaches

### 5.1 Houdini Procedural Buildings (SideFX Labs)

SideFX's Building Generator uses a **module replacement** approach rather than boolean subtraction:

- The building facade is divided into a grid of "panels" (wall segments)
- Each panel can be assigned a type: solid wall, window, door, vent, etc.
- Door placement is done by **replacing a panel region** with a door module
- A "box covering the entrance" is connected to the building generator's 4th input, and panels in that region are replaced with the door variant

**Validation:** Houdini validates geometry types with a `Validate Geometry Type` utility. For door placement, the validation is implicit: if a module fits in the grid slot, it's valid.

**Relevance to us:** Our facade system already uses a similar approach for exterior doors (panel-based replacement). The interior door system uses boolean subtract, which is more flexible for arbitrary door positions but requires the depth/centering fixes above.

### 5.2 Unity ProBuilder

ProBuilder's boolean operations documentation notes:
- Both meshes must be **watertight** (no open edges)
- No coincident/coplanar faces (causes degenerate output)
- The boolean operation itself is marked **experimental** in ProBuilder, reflecting the inherent difficulty

ProBuilder recommends: "Use a box collider for doors and make the box a fair bit thicker than the door." This directly mirrors our `CutterOvershoot = WallT + 10` approach.

### 5.3 Minecraft Dungeon Generation

Minecraft's procedural structures use **tile-based connectivity** rather than continuous geometry:

- Rooms are placed as sets of blocks on a grid
- Corridors are carved as 1-3 block wide passages
- **Doors are 1-block wide openings** in walls (2 blocks high)
- Validation is trivial: if the block at door position is air, the door is passable

**Roguelike Dungeons mod** (MC 1.6+): Links rooms with corridors and stairwells. Validates connectivity using **A* pathfinding** from each room to every other room. If no path exists, additional corridors are carved.

**Relevance:** The Minecraft approach of carving corridors and validating via pathfinding is essentially what our floor plan generator does with `FindCorridorPath()` + BFS reachability. The key lesson is that **validation should happen at the grid level (before geometry), not after.**

### 5.4 Dwarf Fortress Room/Corridor Validation

Dwarf Fortress has decades of refined pathfinding and room design:

- **Door placement rule:** Must be placed cardinally adjacent to a wall
- **Corridor width guidance:** "Make high-traffic routes at least 2 tiles wide, and avoid single doors and single stairs" -- because single-tile corridors cause pathfinding overhead when dwarves try to pass each other
- **Traffic system:** Each tile has a traffic cost (High=1, Normal=2, Low=5, Restricted=25). Doors can be locked/unlocked dynamically, affecting pathfinding
- **Room validation:** Rooms are defined by enclosed spaces. A room must be fully enclosed (all boundary tiles are wall/door) to be recognized

**Key insight for our system:** DF's emphasis on **2-tile-wide corridors** for traffic efficiency maps directly to our recommendation of 3+ cell corridors (150cm+). Single-cell corridors (50cm) are sub-capsule-width and completely impassable.

### 5.5 Bob Nystrom's "Rooms and Mazes"

The canonical proc-gen dungeon algorithm (stuffwithstuff.com, 2014):

1. Place random non-overlapping rooms
2. Fill remaining space with maze (recursive backtracking)
3. Find "connectors" -- wall tiles adjacent to two different regions
4. Open connectors to link regions (these become doors)
5. Remove dead-end maze passages

**Door validation is built into step 4:** A connector is only valid if it borders exactly two different regions. Opening it creates a 1-tile-wide passage. The algorithm guarantees full connectivity because the maze fills all space and connectors link all regions.

**Relevance:** Our grid-based approach already uses this philosophy (walls at room-ID boundaries, doors at shared edges). The Nystrom algorithm's strength is that connectivity is **guaranteed by construction**, not validated after the fact.

### 5.6 Freiknecht & Effelsberg (IEEE 2019) -- "Procedural Generation of Multistory Buildings With Interior"

Key contributions relevant to door clearance:

- **Coherency checker:** Tests the geometric model to verify that "all connecting rooms own a common shared wall with available space to place doors"
- **Wall/door sequencing:** "Inner marks are placed to flag doors that connect pairs of rooms, then walls are extruded taking into account these previously flagged transitions"
- **Post-generation validation:** BFS from entrance to verify all rooms are reachable

This is the most directly applicable academic work. Their pipeline is: room placement -> door flagging -> wall extrusion (with door gaps) -> coherency check -> BFS reachability.

---

## 6. Concrete Recommendations for Our System

### 6.1 Immediate Fixes (< 4 hours)

**Fix 1: Increase normal-mode door width from 90cm to 100cm**

File: `MonolithMeshFloorPlanGenerator.cpp`, line 922
```
Current:  float DoorWidth = bHospiceMode ? 100.0f : 90.0f;
Proposed: float DoorWidth = bHospiceMode ? 120.0f : 100.0f;
```
Rationale: 90cm with 84cm capsule = 3cm clearance per side. Too tight. 100cm gives 8cm per side.

**Fix 2: Increase normal-mode corridor width from 2 to 3 cells**

File: `MonolithMeshFloorPlanGenerator.cpp`, line 709
```
Current:  int32 CorridorWidth = bHospiceMode ? 4 : 2;
Proposed: int32 CorridorWidth = bHospiceMode ? 4 : 3;  // 3 * 50 = 150cm
```
Rationale: 100cm corridors with 84cm capsule = 8cm clearance per side. Walking feels like squeezing through a crack. 150cm gives 33cm per side.

**Fix 3: Fix `create_structure` boolean cutter centering** (already documented in `reference_boolean_doors_research.md`)

Center cutter on wall midpoint instead of outer face. ~1 hour fix.

### 6.2 Add Clearance Validation Pass (4-8 hours)

Add a `ValidateDoorClearance()` function called after `PlaceDoors()` but before geometry generation:

```
ValidateDoorClearance(Doors, Rooms, Grid, CapsuleRadius=42):
    MinDoorWidth = CapsuleRadius * 2 + 16  // 100cm minimum

    for each Door in Doors:
        // 1. Width check
        if Door.Width < MinDoorWidth:
            Door.Width = MinDoorWidth

        // 2. Wall segment length check
        SharedEdgeLength = CountSharedEdgeCells(Door) * CellSize
        MaxDoorWidth = SharedEdgeLength - FrameWidth * 2 - 10  // Leave 5cm margin each end
        if Door.Width > MaxDoorWidth:
            if MaxDoorWidth < MinDoorWidth:
                Log.Warning("Door %s: wall segment too short for traversable door", Door.Id)
                Door.bTraversable = false  // Mark as non-traversable (window instead?)
            else:
                Door.Width = MaxDoorWidth

        // 3. Adjacent wall check (T-junctions)
        // If a perpendicular wall ends within DoorWidth/2 of the door center,
        // the effective opening is narrowed
        EffectiveWidth = CheckForAdjacentWallIntrusion(Door, Grid)
        if EffectiveWidth < MinDoorWidth:
            // Shift door position away from the perpendicular wall
            ShiftDoorAlongWall(Door, needed_offset)
```

### 6.3 Add Exterior Entrance Guarantee (4-6 hours)

Add to floor plan generator, after room placement but before door placement:

1. Identify perimeter rooms (rooms with cells on grid edge)
2. Prefer "corridor" or "lobby" type rooms for entrance
3. Place an exterior door on the south-facing perimeter cell (or first available)
4. Mark door with `bExterior = true`
5. Register in spatial registry's `ExteriorDoorIds`

### 6.4 Optional: Post-Generation Walk Test (8-12 hours)

As a debug/validation MCP action (`validate_building_traversal`):

1. Generate collision for the building mesh
2. For each door, sweep a capsule through the opening
3. For each room pair, verify path exists through door graph
4. Output report: { valid_doors, blocked_doors, unreachable_rooms }

This would be a `mesh_query` action, not part of the generation pipeline.

---

## 7. Summary of Key Numbers

```
=== PLAYER CAPSULE (Leviathan / GAS) ===
Radius:        42 cm
Diameter:      84 cm
Half-Height:   96 cm
Total Height:  192 cm

=== DOOR DIMENSIONS ===
                     Normal    Hospice
Width (current):     90 cm     100 cm    <-- TOO NARROW
Width (proposed):    100 cm    120 cm    <-- FIX
Height:              220 cm    220 cm
Frame width:         5 cm      5 cm

=== CORRIDOR DIMENSIONS ===
                     Normal    Hospice
Width (current):     100 cm    200 cm    <-- Normal too narrow
Width (proposed):    150 cm    200 cm    <-- FIX
Grid cells:          3         4

=== BOOLEAN CUTTER ===
Depth:          WallThickness + 10 cm (minimum)
Centering:      On wall midpoint, NOT outer face
Below floor:    -3 cm (eliminate threshold sliver)
Width:          DoorWidth + 1 cm (tolerance)

=== ADA REFERENCE (real-world) ===
Min door clear:  81.3 cm (32 inches)
Wheelchair width: 66-71 cm
Hallway min:     91.4 cm (36 inches)
Turning space:   152.4 cm (60 inch diameter)
```

---

## Sources

### Level Design & Metrics
- [The Level Design Book - Metrics](https://book.leveldesignbook.com/process/blockout/metrics) -- player dimensions, corridor/door conventions
- [World of Level Design - UE5 Scale Guide](https://www.worldofleveldesign.com/categories/ue5/guide-to-scale-dimensions-proportions.php) -- UE5-specific architecture dimensions
- [World of Level Design - UE4 Scale Guide](https://www.worldofleveldesign.com/categories/ue4/ue4-guide-to-scale-dimensions.php) -- player scale and world dimensions
- [Polycount - Corridor Size Recommendations](https://polycount.com/discussion/158767/size-recommendations-for-corridors-in-ue4)
- [Medium/IronEqual - Practical Guide on FPS Level Design](https://medium.com/ironequal/practical-guide-on-first-person-level-design-e187e45c744c)

### ADA / Accessibility
- [U.S. Access Board - Entrances, Doors, and Gates](https://www.access-board.gov/ada/guides/chapter-4-entrances-doors-and-gates/) -- ADA door clearance standards
- [ADA.gov - Design Standards](https://www.ada.gov/law-and-regs/design-standards/1991-design-standards/) -- ADA Standards for Accessible Design
- [1800wheelchair - Doorway Width for Wheelchair](https://www.1800wheelchair.com/faq/how-wide-doorway-hallway-wheelchair/) -- practical wheelchair clearance
- [Vortex Doors - ADA Door Width](https://www.vortexdoors.com/blog/what-is-the-correct-door-width-for-a-wheelchair)

### Procedural Generation Algorithms
- [Bob Nystrom - Rooms and Mazes](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/) -- canonical dungeon generation with guaranteed connectivity
- [Freiknecht & Effelsberg - Procedural Generation of Multistory Buildings (IEEE 2019)](https://ieeexplore.ieee.org/document/8926482/) -- coherency checker, entrance-first generation
- [Manakal - Novel Algorithm for Building Floor Plans (arXiv)](https://arxiv.org/pdf/1211.5842) -- real-time floor plan generation
- [Dahl & Rinde - Procedural Generation of Indoor Environments (Chalmers)](https://www.cse.chalmers.se/~uffe/xjobb/Lars%20Rinde%20o%20Alexander%20Dahl-Procedural%20Generation%20of%20Indoor%20Environments.pdf)
- [Annunziato - Procedural Level Generation](https://www.stevetech.org/blog/procedural-level-generation)

### Houdini / SideFX
- [SideFX - Building Generator Tutorial](https://www.sidefx.com/tutorials/building-generator/) -- module replacement approach
- [SideFX - Procedural Building from Modules](https://www.sidefx.com/tutorials/procedural-building-from-modules-in-houdini/)
- [OreateAI - Houdini Procedural Architecture Lab Analysis](https://www.oreateai.com/blog/technical-analysis-and-application-guide-for-houdini-procedural-architecture-lab-building/da378776216046312b040597e67ace56)

### Unity / ProBuilder
- [Unity ProBuilder - Boolean Operations](https://docs.unity3d.com/Packages/com.unity.probuilder@6.0/manual/boolean.html) -- experimental boolean, watertight requirements

### Game-Specific
- [Dwarf Fortress Wiki - Door](https://dwarffortresswiki.org/index.php/Door) -- cardinal adjacency requirement
- [Dwarf Fortress Wiki - Path](https://dwarffortresswiki.org/index.php/Path) -- 2-tile corridor recommendation
- [Dwarf Fortress Wiki - Design Strategies](https://dwarffortresswiki.org/index.php/DF2014:Design_strategies) -- room/corridor design patterns
- [Grokipedia - Procedural Dungeon Generation in Minecraft](https://grokipedia.com/page/Procedural_Dungeon_Generation_in_Minecraft)
- [ProceduralDungeon Wiki](https://benpyton.github.io/ProceduralDungeon/guides/Best-Practices/Workflows/Dungeon-Generation-Algorithm)

### UE5 Capsule Documentation
- [UE5.7 - Set Capsule Size](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Components/Capsule/SetCapsuleSize)
- [UE5.7 - Get Scaled Capsule Half Height](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Components/Capsule/GetScaledCapsuleHalfHeight)

---

## Estimated Implementation Effort

| Task | Hours | Priority |
|------|-------|----------|
| Fix door width defaults (90->100, 100->120) | 0.5h | P0 |
| Fix corridor width (2->3 cells normal mode) | 0.5h | P0 |
| Fix `create_structure` boolean centering bug | 1h | P0 |
| Add clearance validation pass | 6h | P1 |
| Add guaranteed exterior entrance | 5h | P1 |
| Post-generation walk test MCP action | 10h | P2 |
| **Total** | **~23h** | |
