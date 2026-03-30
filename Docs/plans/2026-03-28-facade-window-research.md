# Procedural Facade, Window, and Door Generation Research

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Facade composition, window/door placement algorithms, exterior trim, material zones, horror-specific damage, storefront geometry, and integration plan for Monolith `create_facade` action
**Dependencies:** `create_building_shell`, `create_structure`, `create_furniture` (door_frame, window_frame), boolean doors research

---

## Executive Summary

This document covers the full theory and implementation plan for procedurally generating realistic building facades -- the exterior "skin" of buildings, including window placement, door placement, exterior trim, material zoning, and horror-specific damage. The core approach adapts CGA Shape Grammar concepts (split/repeat operators) into a JSON-driven system compatible with Monolith's existing procedural geometry pipeline. A single `create_facade` action would accept a wall face definition and a style spec, then generate windows, doors, trim, and material zones using GeometryScript boolean subtract and AppendMesh operations.

---

## 1. Architectural Facade Theory

### 1.1 Classical Tripartite Division

Real buildings follow a three-part vertical composition analogous to a classical column (base, shaft, capital):

```
+----------------------------------+
|          CORNICE / CAP           |  <- Top: decorative crown, parapet, cornice
|  +---+  +---+  +---+  +---+    |     Shorter floor, decorative emphasis
|  | W |  | W |  | W |  | W |    |
+--+---+--+---+--+---+--+---+----+
|  +---+  +---+  +---+  +---+    |  <- Middle: repetitive "shaft"
|  | W |  | W |  | W |  | W |    |     All floors identical
|  +---+  +---+  +---+  +---+    |
|  +---+  +---+  +---+  +---+    |
|  | W |  | W |  | W |  | W |    |
|  +---+  +---+  +---+  +---+    |
+----------------------------------+
|                                  |  <- Belt course (horizontal band)
+----------------------------------+
|  +---------+  +---+  +---+     |  <- Base: ground floor
|  |  DOOR   |  | W |  | W |     |     Taller, bigger openings
|  |         |  +---+  +---+     |     Different material (stone/rustication)
+--+----+----+--+---+--+---+-----+
```

**Key proportions (from architectural standards and ResearchGate analysis):**

| Zone | Height Ratio | Character |
|------|-------------|-----------|
| Base (ground floor) | 1.2-1.5x standard floor | Taller, heavier, wider openings |
| Middle (shaft) | Nx standard floor | Repetitive, identical |
| Cap (top floor + cornice) | 0.8-1.0x standard floor + cornice | Decorative termination |

**Sources:**
- [Tripartite Division, Chicago Architecture Center](https://www.architecture.org/online-resources/architecture-encyclopedia/tripartite-division)
- [Marquette Building Facade, MacArthur Foundation](https://marquette.macfound.org/slide/tripartite-facade.html)
- [Historic Building Proportions, Maysville Design Guidelines](https://cms5.revize.com/revize/maysville/Document%20Center/Forms/New%20node/Planning%20&%20Zoning/Maysville-Design-Review-Guidelines-Part-3.pdf)

### 1.2 Floor Height Standards (Metric)

| Floor Type | Height (cm) | Typical Range |
|-----------|-------------|---------------|
| Ground floor (residential) | 300-360 | 10-12 ft |
| Ground floor (retail/storefront) | 400-500 | 13-16 ft |
| Upper floors (residential) | 270-300 | 9-10 ft |
| Upper floors (commercial) | 350-400 | 11.5-13 ft |
| Attic/top floor | 240-270 | 8-9 ft |

### 1.3 Facade Zone Materials

Different vertical zones of a facade typically use different materials:

| Zone | Common Materials | Character |
|------|-----------------|-----------|
| Base / plinth (0-60cm) | Stone, granite, concrete | Heavy, water-resistant |
| Ground floor walls | Rusticated stone, brick | Substantial, grounded |
| Belt course | Stone, cast stone, GFRC | Horizontal band separator |
| Upper walls | Brick, clapboard, stucco, concrete | Lighter, repetitive |
| Cornice | Stone, cast stone, wood (corniced) | Projecting, decorative |
| Window surrounds | Stone, cast stone, wood | Frame/accent material |
| Quoins (corners) | Stone, contrasting brick | Vertical corner emphasis |

---

## 2. CGA Shape Grammar -- Adapted for Monolith

### 2.1 Core CGA Concepts

CGA Shape Grammar (Muller & Wonka, 2006) is a context-sensitive grammar for procedural building generation. The key operations relevant to facade generation:

**Split:** Divide a shape along an axis into named sub-shapes with absolute or relative sizes.
```
Facade --> split(y) { groundFloorH : GroundFloor | { ~floorH : Floor }* | corniceH : Cornice }
```

**Repeat:** Fill remaining space with repeated sub-shapes using the `*` operator.
```
Floor --> split(x) { ~tileW : Tile }*
```

**Tilde (~):** Flexible size -- scales to fill available space while maintaining integer count.

**Component Split:** Extract specific faces (front, back, left, right, top, bottom) from a 3D shape.

**Insert:** Replace a shape with a geometric asset (window model, door model, etc).

**Sources:**
- [CGA Shape Grammar (UPC Barcelona, 2014)](https://www.cs.upc.edu/~virtual/SGI/docs/1.%20Theory/Unit%2011.%20Procedural%20modeling/CGA%20shape%20grammar.pdf)
- [Procedural Modeling of Buildings (Muller et al., ACM TOG 2006)](https://dl.acm.org/doi/10.1145/1141911.1141931)
- [CityEngine Tutorial 6: Basic Shape Grammar](https://doc.arcgis.com/en/cityengine/2024.0/tutorials/tutorial-6-basic-shape-grammar.htm)
- [CityEngine Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm)
- [Understanding CGA Shape Grammar (Penn State)](https://www.e-education.psu.edu/geogvr/node/891)

### 2.2 Monolith Facade Grammar (JSON Adaptation)

Translate CGA split/repeat into a JSON spec that Monolith can process:

```json
{
  "wall_face": { "origin": [0, 0, 0], "normal": [0, -1, 0], "width": 1200, "height": 900 },
  "style": "residential_victorian",
  "zones": {
    "base": {
      "height": 60,
      "material_id": 0,
      "elements": []
    },
    "ground_floor": {
      "height": 360,
      "material_id": 1,
      "elements": [
        { "type": "door", "variant": "front_recessed", "position": "center", "width": 120, "height": 240 },
        { "type": "window", "variant": "single", "repeat": "fill", "spacing": 200, "margin": 100,
          "width": 100, "height": 150, "sill_height": 90 }
      ]
    },
    "belt_course": {
      "height": 15,
      "material_id": 2,
      "profile": "molding_simple"
    },
    "upper_floors": {
      "repeat": 2,
      "height": 300,
      "material_id": 3,
      "elements": [
        { "type": "window", "variant": "single", "repeat": "fill", "spacing": 200, "margin": 80,
          "width": 100, "height": 150, "sill_height": 90,
          "align_to": "ground_floor" }
      ]
    },
    "cornice": {
      "height": 30,
      "material_id": 2,
      "profile": "cornice_classical"
    }
  }
}
```

### 2.3 CityEngine Advanced Facade Rules (Reference Implementation)

From the CityEngine Tutorial 9, the complete facade grammar hierarchy:

```
Facade -->
    split(y) { ~groundfloorH : Floor(split.index)
             | { ~floorH     : Floor(split.index) }* }

Floor(floorIndex) -->
    split(x) { borderwallW : Wall
             | ~1          : FloorSub(floorIndex)
             | borderwallW : Wall }

FloorSub(floorIndex) -->
    case floorIndex == 0:
        split(y) { 1      : Wall
                 | ~1     : TileRow(floorIndex)
                 | ledgeH : Wall }
    case floorIndex > 0:
        split(y) { ~1     : TileRow(floorIndex)
                 | ledgeH : Ledge }

TileRow(floorIndex) -->
    split(x) { ~tileW : Tile }*

Tile -->
    split(x) { frameW : Frame
             | ~1     : split(y) { frameW : Frame
                                 | ~1     : Window
                                 | frameW : Frame }
             | frameW : Frame }
```

**Key insight:** The ground floor (index 0) gets Wall padding above and below the window row, while upper floors only get a ledge on top. This creates the visual weight difference between base and shaft.

---

## 3. Window Placement Algorithm

### 3.1 Core Algorithm: Even Spacing with Constraints

Given a wall face of width `W`, place `N` windows of width `WinW` with minimum margin `M` from edges and minimum spacing `S` between windows.

```
Algorithm: ComputeWindowPositions(W, WinW, M, S)

Input:
  W      = wall width (cm)
  WinW   = window width (cm)
  M      = minimum margin from wall edges (cm)
  S      = minimum spacing between windows (cm)

Compute:
  available = W - 2*M                          // usable width after margins
  N = floor((available + S) / (WinW + S))      // max windows that fit
  N = max(N, 0)                                // clamp to 0

  if N == 0: return empty                      // wall too narrow

  total_window = N * WinW                      // total glass width
  total_gap = available - total_window          // remaining space for gaps
  gap = total_gap / (N + 1)                    // even gap between and at edges

  positions = []
  for i in 0..N-1:
    x = -W/2 + M + gap + i*(WinW + gap) + WinW/2   // center of each window
    positions.append(x)

  return positions
```

**Variant: Fixed count with even spacing**
```
Given N windows to place:
  gap = (W - 2*M - N*WinW) / (N + 1)
  if gap < S_min: reduce N and retry
```

### 3.2 Alignment Rules

Windows should align vertically across floors. The algorithm runs once for the floor with the most constraints (usually ground floor, due to the door), then upper floors inherit those X positions:

```
1. Compute ground floor positions (accounts for door placement)
2. For each upper floor:
   a. If "align_to": "ground_floor" -> use same X positions
   b. If independent -> run algorithm fresh (may differ if floor width varies on setbacks)
3. Special case: top floor may have fewer/smaller windows
```

### 3.3 Window Pattern Types

| Pattern | Description | Algorithm |
|---------|------------|-----------|
| Single | One window per bay | Basic even spacing |
| Paired | Two windows close together per bay | Treat pair as single unit (width = 2*WinW + mullion), then even-space units |
| Triple | Three windows per bay | Same as paired but 3-wide |
| Bay window | Projecting 3-panel window | Central window + 2 angled side panels (30 or 45 degrees) |
| Ribbon | Continuous horizontal band | One wide opening, subdivided by mullions |
| Storefront | Large plate glass | Single wide opening at ground level |

### 3.4 Window Dimensions (Standard Metric)

| Type | Width (cm) | Height (cm) | Sill Height (cm) |
|------|-----------|-------------|-------------------|
| Small residential | 60-80 | 90-120 | 90-100 |
| Standard residential | 90-120 | 120-150 | 80-100 |
| Large residential | 120-180 | 150-180 | 60-80 |
| Paired (2x) | 180-240 | 120-150 | 80-100 |
| Triple (3x) | 240-360 | 120-150 | 80-100 |
| Bay window | 180-300 (projected) | 120-180 | 40-60 |
| Storefront plate | 200-400+ | 240-300 | 0-30 |
| Basement/transom | 60-120 | 30-60 | 180-220 |

**Sources:**
- [Standard Window Sizes Guide (DERCHI)](https://www.derchidoor.com/blogs/what-is-the-standard-size-of-doors-and-windows.html)
- [Window Size Chart Guide (Gladiator)](https://gladiatorwindowanddoors.com/blogs/news/window-size-chart-guide-standards-measurements-sizing)
- [Window Placement: Use One Ratio (Fine Homebuilding)](https://www.finehomebuilding.com/2016/09/14/window-placement-use-one-ratio)
- [Practical Guide to Window Placement (CRD Design Build)](https://www.crddesignbuild.com/blog/a-practical-guide-to-window-placement/)

---

## 4. Window Geometry

### 4.1 Anatomy of a Window

```
                   LINTEL (header)
            +========================+
            |  +------------------+  |
            |  |                  |  |  <- FRAME (jamb)
  FRAME ->  |  |    GLASS PANE   |  |
  (jamb)    |  |                  |  |
            |  |    (separate     |  |
            |  |     mesh or      |  |
            |  |     translucent  |  |
            |  |     material)    |  |
            |  +------------------+  |
            +========================+
                     SILL
                 (projects outward)
```

### 4.2 Component Geometry (GeometryScript)

Each window is built from AppendBox primitives:

```
Frame components (MaterialID = 2, "trim"):
  Left jamb:   AppendBox(FrameW, FrameD, WinH + FrameW*2)  at (CenterX - WinW/2 - FrameW/2, ...)
  Right jamb:  AppendBox(FrameW, FrameD, WinH + FrameW*2)  at (CenterX + WinW/2 + FrameW/2, ...)
  Lintel:      AppendBox(WinW + FrameW*2, FrameD, FrameW)   at (CenterX, ..., SillZ + WinH)
  Sill:        AppendBox(WinW + FrameW*2 + SillExt*2, SillD, SillH) at (CenterX, ..., SillZ - SillH)

Where:
  FrameW = frame width (3-8cm typical)
  FrameD = frame depth (equal to or slightly more than wall thickness)
  SillExt = sill extension beyond frame (3-5cm each side)
  SillD = sill depth (projects 5-10cm beyond wall face)
  SillH = sill thickness (3-5cm)

Glass pane (MaterialID = 4, "glass"):
  AppendBox(WinW, 1.0, WinH) at wall center plane
  Or: skip geometry, use transparent material on the opening void
```

### 4.3 Mullion Subdivisions

For multi-pane windows, add vertical/horizontal mullion bars:

```
Single-hung (2 panes, horizontal split):
  Horizontal mullion: AppendBox(WinW, FrameD, MullionW) at mid-height

Casement (2 panes, vertical split):
  Vertical mullion: AppendBox(MullionW, FrameD, WinH) at center

Grid (e.g., 3x2):
  2 horizontal mullions at 1/3, 2/3 height
  1 vertical mullion at center
  MullionW = 2-3cm typically
```

### 4.4 Bay Window Geometry

A bay window projects outward from the wall face:

```
Plan view (top-down):
                WALL FACE
    ============+       +============
                 \     /
          angle   \   /   angle
          panel    \ /    panel
                    |
              center panel

Three panels:
  Center: AppendBox(CenterW, ProjD, WinH)   at (CenterX, WallY - ProjD/2, SillZ)
  Left:   AppendBox(SideW, ProjD, WinH)     rotated +30 or +45 degrees about Z
  Right:  AppendBox(SideW, ProjD, WinH)     rotated -30 or -45 degrees about Z

Typical dimensions:
  Projection depth: 30-60cm
  Center panel width: 80-120cm
  Side panel width: 40-60cm
  Angle: 30 or 45 degrees from wall plane

Roof cap: small hip or shed roof over the projection
Floor shelf: AppendBox extending from wall to bay bottom
```

---

## 5. Door Types and Geometry

### 5.1 Door Dimensions (Metric)

| Type | Width (cm) | Height (cm) | Recess (cm) | Notes |
|------|-----------|-------------|-------------|-------|
| Interior single | 80-90 | 200-210 | 0-5 | Flush with wall |
| Exterior front | 90-100 | 210-240 | 10-20 | Recessed, with transom |
| Exterior double | 150-180 | 210-240 | 10-20 | Two leaves |
| Back/service | 80-90 | 200-210 | 0-5 | Flush, plain |
| Fire door | 90-120 | 210 | 0 | Flush, heavy frame |
| Sliding glass | 180-240 | 210-240 | 0 | Track at top/bottom |
| Garage | 240-500 | 210-240 | 0 | Roll-up or sectional |
| Storefront entry | 90-180 | 240-300 | 15-30 | Deeply recessed |

**Source:** [Standard Door Sizes Guide (U Brothers Construction)](https://ubrothersconstruction.com/blog/the-ultimate-guide-to-standard-door-size-heights-frames-and-door-dimensions)

### 5.2 Front Door (Recessed Entry)

```
Side view:
    WALL FACE          RECESS
    |==========|       |
    |          |_______|
    |          |       |  <- recessed alcove (10-20cm deep)
    |          | DOOR  |
    |          |       |
    |          |_______|
    |==========|       |

Components:
  1. Boolean subtract recess alcove from wall: box (DoorW + 2*Reveal, RecessD, DoorH + FrameW)
  2. Boolean subtract door opening from recess back wall: box (DoorW, WallT+10, DoorH)
  3. Add door frame geometry (U-frame): 2 jambs + 1 header
  4. Optional: transom window above door (small horizontal window)
  5. Optional: sidelights (narrow vertical windows flanking the door)
  6. Optional: steps/porch platform below (AppendBox)
```

### 5.3 Storefront Entry

```
Elevation:
    +-------------------------------------------+
    |            SIGNAGE AREA (fascia)           |  <- flat panel, MaterialID 5
    +---+------+---+------+---+------+---+------+
    |   | PLATE|   | PLATE|   |      |   | PLATE|
    | M | GLASS| M | GLASS| M | DOOR | M | GLASS|  <- M = mullion
    | U |      | U |      | U |      | U |      |
    | L |      | L |      | L |      | L |      |
    | L |      | L |      | L |      | L |      |
    +---+------+---+------+---+------+---+------+
    |              BULKHEAD / KICKPLATE          |  <- 30-45cm base panel
    +-------------------------------------------+

Components:
  Kickplate: AppendBox(FullW, WallT, KickH) at base, MaterialID 0
  Plate glass panels: boolean subtract from wall, each WinW wide
  Mullions: thin vertical boxes between glass, 5-8cm wide
  Signage fascia: AppendBox(FullW, 5, SignH) above glass
  Entry: recessed opening (wider mullion gap or no mullion)
```

---

## 6. Exterior Trim Elements

### 6.1 Trim Profile Types

| Element | Location | Cross-Section | Typical Size |
|---------|----------|---------------|-------------|
| Cornice | Top of building | L or S-curve profile | 15-40cm projection, 20-50cm height |
| Belt course | Between floors | Rectangle or simple molding | 5-15cm projection, 10-20cm height |
| Window header | Above window | Rectangle, keystone, or arch | 5-10cm projection, 8-15cm height |
| Window sill | Below window | Beveled rectangle | 5-10cm projection, 3-5cm height |
| Pilaster | Vertical, between bays | Rectangular column | 10-20cm projection, 15-30cm width |
| Quoin | Building corners | Alternating blocks | 5-10cm projection, 20-40cm blocks |
| Baseboard/plinth | Base of wall | Beveled rectangle | 3-10cm projection, 30-60cm height |

### 6.2 Implementation via Sweep Profiles

Trim elements with continuous profiles (cornice, belt course, baseboard) are best implemented via `AppendSimpleSweptPolygon` -- sweep a 2D profile along the wall edge path:

```
Cornice profile (2D cross-section):
    +--+
    |  |       <- fascia (vertical face)
    +--+---+
           |   <- soffit (horizontal underside)
    +------+
    |          <- bed molding (transition)

As 2D polygon vertices:
  (0, 0), (ProjectionD, 0), (ProjectionD, FasciaH),
  (CrownW, FasciaH + BedH), (0, FasciaH + BedH)
```

Belt course and baseboard use simpler rectangular profiles:

```
Belt course: rectangle (ProjectionD x BeltH)
  Sweep path: horizontal line along wall face at floor division height

Baseboard: beveled rectangle
  (0, 0), (PlinthD, 0), (PlinthD - Bevel, PlinthH), (0, PlinthH)
```

### 6.3 Pilasters and Quoins

Pilasters are vertical trim elements that divide the facade into bays:

```
Pilaster: AppendBox(PilW, PilD, FloorH) at each bay division point
  Typically placed at: wall edges + evenly between (dividing facade into N bays)
  Each bay contains windows

Quoins: alternating blocks at building corners
  For i in 0..N:
    if i % 2 == 0: AppendBox(QuoinW, QuoinD, QuoinH) -- large block
    else:          AppendBox(QuoinW*0.6, QuoinD, QuoinH) -- small block
    Z offset: cumulative height
```

---

## 7. Material Zone Assignment

### 7.1 MaterialID Scheme

Assign MaterialIDs to geometry during construction so different materials can be applied in UE:

| MaterialID | Zone | Typical Material |
|-----------|------|-----------------|
| 0 | Wall surface (base/plinth) | Stone, concrete |
| 1 | Wall surface (upper) | Brick, clapboard, stucco |
| 2 | Trim (cornice, belt, sills) | Painted wood, cast stone |
| 3 | Window frame | Painted wood, aluminum |
| 4 | Glass | Translucent/emissive |
| 5 | Door | Wood, painted wood |
| 6 | Signage/fascia | Painted panel |
| 7 | Accent (quoins, keystones) | Contrasting stone |

### 7.2 Facade Material Styles

| Style | Base | Upper Walls | Trim | Character |
|-------|------|------------|------|-----------|
| Brick row house | Brownstone | Red/brown brick | Painted wood | Urban residential |
| Clapboard colonial | Stone foundation | White/gray clapboard | White wood | Suburban, New England |
| Stucco Mediterranean | Stucco (darker) | Stucco (lighter) | Terracotta tile | Warm, southern |
| Concrete brutalist | Raw concrete | Raw concrete | None | Institutional, hostile |
| Victorian painted | Stone | Painted wood siding | Ornate wood | Colorful, detailed |
| Commercial modern | Glass/steel | Curtain wall | Aluminum | Office building |
| Abandoned/horror | Crumbling concrete | Stained stucco/brick | Broken/missing | Decrepit |

### 7.3 Implementation

During geometry construction, pass `FGeometryScriptPrimitiveOptions` with appropriate `MaterialID` for each element:

```cpp
FGeometryScriptPrimitiveOptions WallOpts;
WallOpts.MaterialID = bIsGroundFloor ? 0 : 1;  // base vs upper wall

FGeometryScriptPrimitiveOptions TrimOpts;
TrimOpts.MaterialID = 2;  // all trim

FGeometryScriptPrimitiveOptions GlassOpts;
GlassOpts.MaterialID = 4;  // glass panes
```

The resulting static mesh has N material slots. The caller assigns actual materials per slot.

---

## 8. Horror-Specific Facade Elements

### 8.1 Boarded-Up Windows

```
Board pattern over window opening:
  Option A (horizontal boards):
    For i in 0..N:
      AppendBox(WinW + 10, BoardT, BoardH)
        at Z = SillZ + i * (BoardH + GapH)
        slightly randomized: rotation +/- 2 degrees, offset +/- 3cm

  Option B (diagonal cross-boards):
    Board 1: AppendBox rotated +15 to +30 degrees
    Board 2: AppendBox rotated -15 to -30 degrees
    Both spanning the full window opening

  Option C (plywood sheet):
    Single AppendBox(WinW + 5, PlywoodT, WinH + 5)
    MaterialID = 8 (plywood/OSB texture)

Board dimensions:
  BoardH = 15-20cm (standard lumber width)
  BoardT = 2-3cm (board thickness)
  GapH = 2-5cm between boards (or 0 for tight boarding)
  NailHead = optional tiny cylinder at board ends
```

### 8.2 Broken Windows

```
Broken glass effect:
  1. Generate normal window geometry
  2. Add "broken" variant:
     a. Remove some glass pane geometry (leave hole)
     b. Add jagged glass fragments at edges:
        - Small triangulated polygons at random positions along frame
        - Use AppendTriangulatedPolygon with irregular vertices
     c. Or: use a "broken glass" material instead of geometry
        (more practical for real-time rendering)

Recommendation: Use material-based approach (broken glass texture with alpha)
rather than geometric fragments. The material approach:
  - Set glass MaterialID to a "broken_glass" material with alpha cutout
  - Add a few geometric shards as AppendBox at random angles along edges
  - 3-5 shards per broken window, each 5-15cm
```

### 8.3 Structural Damage Indicators

| Element | Implementation | Horror Effect |
|---------|---------------|---------------|
| Rust stains | Material zone below windows/vents (MaterialID 9) | Water damage, neglect |
| Cracks | Decal placement zones on wall surface | Structural failure |
| Missing trim | Skip trim generation for random elements | Decay |
| Graffiti zones | Flat panel areas marked for decal placement | Urban abandonment |
| Sagging cornice | Slight downward rotation on cornice segments | Structural decay |
| Exposed brick | Material zone where stucco has "fallen off" | Reveals underlying layer |
| Broken boards | Some boards in boarded windows offset/rotated more | Attempted entry |
| Dirt/moss | Material zones at base and under cornices | Biological growth |

### 8.4 Horror Damage System

Add an optional `damage` parameter to the facade spec:

```json
{
  "damage": {
    "level": 0.6,
    "seed": 42,
    "effects": {
      "boarded_windows": 0.4,
      "broken_glass": 0.3,
      "missing_trim": 0.2,
      "rust_stains": 0.5,
      "graffiti_zones": 0.2
    }
  }
}
```

Each effect probability is evaluated per-element using `FMath::SRand(Seed + ElementIndex)`:
- `boarded_windows = 0.4` means 40% of windows get boarded up
- `broken_glass = 0.3` means 30% of remaining (non-boarded) windows have broken glass material
- `missing_trim = 0.2` means 20% of trim elements are skipped
- `rust_stains` and `graffiti_zones` mark regions for decal placement (returned in output metadata)

---

## 9. Procedural Placement Algorithm -- Full Pipeline

### 9.1 Input: Wall Face Definition

A facade is generated for a single planar wall face:

```json
{
  "origin": [0, 0, 0],
  "width_axis": [1, 0, 0],
  "up_axis": [0, 0, 1],
  "normal": [0, -1, 0],
  "width": 1200,
  "total_height": 900,
  "wall_thickness": 20
}
```

### 9.2 Pipeline Steps

```
Step 1: VERTICAL SPLIT (Tripartite Division)
  Input: total_height, floor_heights[], zone_config
  Output: array of horizontal zones with Z ranges

  zones = [
    { type: "plinth",       z_min: 0,    z_max: 60   },
    { type: "ground_floor", z_min: 60,   z_max: 420  },
    { type: "belt_course",  z_min: 420,  z_max: 435  },
    { type: "upper_floor",  z_min: 435,  z_max: 735, floor_index: 1 },
    { type: "upper_floor",  z_min: 735,  z_max: 1035, floor_index: 2 },  // (if exists)
    { type: "cornice",      z_min: 1035, z_max: 1065 }
  ]

Step 2: HORIZONTAL SPLIT (Bay Division)
  For each floor zone, compute element positions:

  a. Identify fixed elements (doors at known positions)
  b. Compute window positions using ComputeWindowPositions()
  c. Optionally add pilasters between bays
  d. Store X positions for vertical alignment across floors

Step 3: GENERATE BASE WALL GEOMETRY
  AppendBox for the full wall slab (or swept wall from create_structure)
  This is the "canvas" that openings will be cut from

Step 4: BOOLEAN SUBTRACT ALL OPENINGS
  For each window/door:
    Create cutter box (oversized depth for clean cut)
    Position at computed (X, Z) on the wall face
  Merge all cutters into one UDynamicMesh (optimization from boolean-doors research)
  Single ApplyMeshBoolean subtract

Step 5: APPEND FRAME/TRIM GEOMETRY
  For each window:
    Build frame (jambs + lintel + sill) via AppendBox calls
    Set MaterialID for trim
  For each door:
    Build frame (U-frame for standard, full frame for storefront)
    Optional recess geometry

Step 6: APPEND FACADE TRIM
  Cornice: sweep profile along top edge
  Belt course: sweep profile at floor divisions
  Plinth: sweep profile along base
  Pilasters: AppendBox at bay divisions
  Quoins: alternating blocks at wall edges

Step 7: APPLY HORROR DAMAGE (optional)
  Evaluate damage probabilities per element
  Add boarding geometry to selected windows
  Mark decal zones in output metadata
  Skip trim for selected elements

Step 8: UV AND CLEANUP
  SetMeshUVsFromBoxProjection for architectural tiling
  ComputeSplitNormals for sharp edges
  Return result with metadata (element positions, material slots, decal zones)
```

### 9.3 Vertical Alignment Across Floors

The most important architectural rule: **windows on upper floors must be vertically aligned with windows on the ground floor**.

```
Implementation:
  1. Run ComputeWindowPositions for ground floor first
  2. Store the X positions as "bay centers"
  3. For each upper floor:
     - If floor width == ground floor width: reuse exact X positions
     - If floor width differs (setback): run fresh algorithm but snap to nearest bay center
     - If fewer windows desired: use every Nth bay center
```

---

## 10. Storefront Ground Floor

### 10.1 Storefront vs Residential Ground Floor

| Feature | Residential | Storefront |
|---------|------------|------------|
| Floor height | 300-360cm | 400-500cm |
| Window height | 120-150cm | 240-300cm |
| Window sill | 80-100cm | 0-30cm (near floor) |
| Entry | Single door, recessed | Wide opening, deeply recessed |
| Signage | None or modest | Fascia band above windows |
| Kickplate | None | 30-45cm base panel |
| Glass coverage | 20-30% of wall | 60-80% of wall |

### 10.2 Storefront Configuration

```json
{
  "type": "storefront",
  "height": 450,
  "kickplate_height": 40,
  "signage_height": 60,
  "entry": {
    "position": "center",
    "width": 180,
    "recessed": true,
    "recess_depth": 30
  },
  "plate_glass": {
    "repeat": "fill",
    "panel_width": 200,
    "mullion_width": 8
  }
}
```

---

## 11. Existing Monolith Implementation Assessment

### 11.1 What Already Exists

| Feature | Status | Location |
|---------|--------|----------|
| `create_structure` with openings | Working | MonolithMeshProceduralActions.cpp:1680 |
| Boolean subtract for doors/windows | Working (positioning bug noted) | Lines 1839-2036 |
| Door frame trim | Working | Lines 1905-2000 |
| Window frame trim | Working | Lines 1905-2000 |
| `create_building_shell` | Working | Lines 2049-2164 |
| `create_furniture` door_frame | Working | BuildDoorFrame, line 879 |
| `create_furniture` window_frame | Working | BuildWindowFrame, line 904 |
| InsetPolygon2D helper | Working | Used by building shell |
| Material ID per element | Working | Trim uses MaterialID 2 |
| Swept profiles (pipes) | Working | CreatePipeNetwork |
| Thin-wall sweep | Planned | Thin-wall research doc |

### 11.2 What Needs to Be Built

| Feature | Complexity | Dependencies |
|---------|-----------|--------------|
| `create_facade` action (full pipeline) | High | Steps 1-8 above |
| ComputeWindowPositions algorithm | Low | Pure math |
| Vertical zone splitter | Low | Pure math |
| Bay window geometry builder | Medium | Boolean + append |
| Storefront builder | Medium | Boolean + mullions |
| Cornice/belt sweep profiles | Medium | AppendSimpleSweptPolygon |
| Pilaster/quoin generators | Low | AppendBox |
| Horror damage system | Medium | Random + conditional skip |
| Facade style presets (JSON) | Low | Data files |

---

## 12. Proposed Action Spec: `create_facade`

```json
{
  "action": "create_facade",
  "params": {
    "wall": {
      "origin": [0, 0, 0],
      "width": 1200,
      "height": 900,
      "thickness": 20,
      "facing": "north"
    },
    "style": "residential_victorian",
    "floors": [
      {
        "type": "ground",
        "height": 360,
        "material_id": 1,
        "windows": {
          "variant": "single",
          "width": 100,
          "height": 150,
          "sill_height": 90,
          "spacing": "auto",
          "margin": 100,
          "mullion_pattern": "2x3"
        },
        "doors": [
          {
            "variant": "front_recessed",
            "position": "left_third",
            "width": 100,
            "height": 240,
            "recess": 15,
            "transom": true,
            "transom_height": 40
          }
        ]
      },
      {
        "type": "upper",
        "repeat": 2,
        "height": 270,
        "material_id": 3,
        "windows": {
          "variant": "single",
          "width": 100,
          "height": 150,
          "sill_height": 80,
          "align_to": "ground"
        }
      }
    ],
    "trim": {
      "cornice": { "height": 30, "projection": 20, "profile": "classical" },
      "belt_course": { "height": 15, "projection": 8 },
      "plinth": { "height": 50, "projection": 5 },
      "window_headers": true,
      "window_sills": true,
      "pilasters": false,
      "quoins": true
    },
    "damage": {
      "level": 0.5,
      "seed": 42,
      "boarded_windows": 0.3,
      "broken_glass": 0.2,
      "missing_trim": 0.15
    },
    "save_path": "/Game/Generated/Facades/VictorianFront",
    "location": [0, 0, 0]
  }
}
```

### 12.1 Output Metadata

```json
{
  "success": true,
  "asset_path": "/Game/Generated/Facades/VictorianFront",
  "triangle_count": 2840,
  "material_slots": 8,
  "floors_generated": 3,
  "windows": [
    { "floor": 0, "index": 0, "center": [-300, 0, 195], "boarded": false, "broken": false },
    { "floor": 0, "index": 1, "center": [0, 0, 195], "boarded": true, "broken": false },
    { "floor": 1, "index": 0, "center": [-300, 0, 545], "boarded": false, "broken": true }
  ],
  "doors": [
    { "floor": 0, "index": 0, "center": [-400, 0, 120], "variant": "front_recessed" }
  ],
  "decal_zones": [
    { "type": "rust_stain", "center": [-300, 0, 150], "size": [40, 60] },
    { "type": "graffiti", "center": [200, 0, 150], "size": [120, 80] }
  ]
}
```

---

## 13. Style Presets (JSON Data Files)

Store facade styles as JSON presets under `Config/FacadePresets/`:

### 13.1 residential_victorian.json

```json
{
  "name": "Victorian Residential",
  "ground_floor_height": 360,
  "upper_floor_height": 300,
  "window": {
    "width": 100,
    "height": 160,
    "sill_height": 80,
    "frame_width": 6,
    "mullion_pattern": "2x2",
    "variant": "single"
  },
  "door": {
    "width": 100,
    "height": 240,
    "variant": "front_recessed",
    "recess": 15,
    "transom": true
  },
  "trim": {
    "cornice": { "height": 35, "projection": 25, "profile": "classical" },
    "belt_course": { "height": 12, "projection": 6 },
    "plinth": { "height": 45, "projection": 5 },
    "window_headers": true,
    "window_sills": true,
    "quoins": true
  },
  "materials": {
    "base": "stone_brownstone",
    "upper": "brick_red",
    "trim": "wood_painted_white",
    "glass": "glass_clear"
  }
}
```

### 13.2 commercial_storefront.json

```json
{
  "name": "Commercial Storefront",
  "ground_floor_height": 450,
  "upper_floor_height": 300,
  "ground_floor_type": "storefront",
  "storefront": {
    "kickplate_height": 40,
    "signage_height": 60,
    "plate_glass_width": 200,
    "mullion_width": 8,
    "entry_width": 180,
    "entry_recessed": true,
    "entry_recess_depth": 30
  },
  "window": {
    "width": 120,
    "height": 150,
    "sill_height": 90,
    "frame_width": 5,
    "variant": "single"
  },
  "trim": {
    "cornice": { "height": 25, "projection": 15, "profile": "simple" },
    "belt_course": { "height": 10, "projection": 5 },
    "plinth": { "height": 0 },
    "pilasters": { "width": 20, "projection": 8, "spacing": "per_bay" }
  }
}
```

### 13.3 horror_abandoned.json

```json
{
  "name": "Abandoned Building (Horror)",
  "inherits": "residential_victorian",
  "damage": {
    "level": 0.7,
    "boarded_windows": 0.5,
    "broken_glass": 0.3,
    "missing_trim": 0.3,
    "rust_stains": 0.6,
    "graffiti_zones": 0.3,
    "sagging_cornice": 0.2
  },
  "material_overrides": {
    "base": "concrete_crumbling",
    "upper": "stucco_stained",
    "trim": "wood_rotting",
    "glass": "glass_dirty"
  }
}
```

---

## 14. Implementation Plan

### Phase 1: Core Facade Engine (~16h)

| Task | Hours | Notes |
|------|-------|-------|
| ComputeWindowPositions algorithm | 1h | Pure math, well-defined |
| Vertical zone splitter (tripartite) | 1h | Split total height into zones |
| Horizontal bay computation | 2h | Doors + windows + margins |
| Cross-floor vertical alignment | 1h | Bay center inheritance |
| Boolean subtract all openings (merged cutter) | 2h | Reuse from boolean-doors research |
| Window frame builder (jambs + lintel + sill) | 3h | Extend existing trim code |
| Door variants (recessed, flush, double) | 3h | Extend BuildDoorFrame |
| Action registration + JSON parsing | 3h | Standard Monolith pattern |

### Phase 2: Trim and Profiles (~10h)

| Task | Hours | Notes |
|------|-------|-------|
| Cornice sweep profile | 2h | AppendSimpleSweptPolygon |
| Belt course sweep | 1h | Simple rectangle profile |
| Plinth sweep | 1h | Beveled rectangle profile |
| Pilaster builder | 1.5h | AppendBox at divisions |
| Quoin builder | 1.5h | Alternating blocks |
| Window header profiles | 1.5h | Keystone or flat options |
| Material ID assignment system | 1.5h | Per-element MaterialID |

### Phase 3: Advanced Window Types (~8h)

| Task | Hours | Notes |
|------|-------|-------|
| Mullion subdivision system | 2h | Grid patterns in windows |
| Bay window projection | 3h | 3-panel angled geometry |
| Storefront plate glass + mullions | 2h | Special ground floor |
| Paired/triple window grouping | 1h | Treat group as single bay |

### Phase 4: Horror Damage System (~6h)

| Task | Hours | Notes |
|------|-------|-------|
| Board-up geometry generator | 2h | Horizontal/diagonal boards |
| Broken glass material tagging | 1h | MaterialID swap |
| Random trim omission | 0.5h | Seed-based skip |
| Decal zone computation + output | 1.5h | Metadata for rust/graffiti |
| Sagging/misaligned trim | 1h | Slight transform perturbation |

### Phase 5: Presets and Integration (~5h)

| Task | Hours | Notes |
|------|-------|-------|
| JSON preset loader | 2h | Load + merge with overrides |
| 5 preset styles (Victorian, Colonial, Commercial, Brutalist, Horror) | 2h | JSON data files |
| Integration with create_building_shell | 1h | Auto-facade per shell face |

### Total: ~45h

---

## 15. Performance Estimates

| Operation | Cost | Notes |
|-----------|------|-------|
| Wall slab (1 AppendBox) | <0.1ms | Trivial |
| 6 window cutters merged | ~0.5ms | 6 AppendBox into 1 mesh |
| 1 boolean subtract (all windows) | ~15-30ms | Merged cutter approach |
| Frame geometry (6 windows x 4 pieces) | ~2ms | 24 AppendBox calls |
| Cornice sweep | ~1-3ms | Single sweep operation |
| Belt + plinth sweeps | ~1-2ms | Two sweep operations |
| UV box projection | ~2-5ms | Full mesh |
| ComputeSplitNormals | ~2-5ms | Full mesh |
| **Total per facade** | **~25-50ms** | Completely acceptable |

A building with 4 facades: ~100-200ms total. Well within editor-time budget.

---

## 16. Key Sources

### Academic / Foundational
- [Procedural Modeling of Buildings (Muller et al., ACM TOG 2006)](https://dl.acm.org/doi/10.1145/1141911.1141931) -- CGA shape grammar, the foundational paper
- [Procedural Architectural Facade Modeling (Zweig, Brown University 2013)](https://cs.brown.edu/media/filer_public/40/59/4059db66-c7dd-480e-8b5c-2899208e233e/zweig.pdf) -- CGA implementation as Maya plugin, JSON grammar
- [Inverse Procedural Modeling of Facade Layouts (ACM TOG 2014)](https://dl.acm.org/doi/10.1145/2601097.2601162) -- Deriving grammars from photos
- [Understanding CGA Shape Grammar (Penn State GEOG 497)](https://www.e-education.psu.edu/geogvr/node/891) -- Clear pedagogical overview

### CityEngine Tutorials
- [Tutorial 6: Basic Shape Grammar](https://doc.arcgis.com/en/cityengine/2024.0/tutorials/tutorial-6-basic-shape-grammar.htm) -- Lot to building with split/repeat
- [Tutorial 9: Advanced Shape Grammar](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-9-advanced-shape-grammar.htm) -- Full facade grammar with floor index, tile rows, double-tiles
- [Tutorial 13: Facade Wizard](https://doc.arcgis.com/en/cityengine/latest/tutorials/tutorial-13-facade-wizard.htm) -- Visual facade creation tool

### Game Dev / Practical
- [Building Blocks: Artist Driven Procedural Buildings (GDC Vault)](https://gdcvault.com/play/1012655/Building-Blocks-Artist-Driven-Procedural) -- Modular approach for games
- [Procedural City Part 3: Generating Buildings (Shamus Young)](https://www.shamusyoung.com/twentysidedtale/?p=2968) -- Practical game city generation
- [Citybound: Implementing Procedural Architecture (aeplay)](https://aeplay.org/citybound-devblog/how-im-implementing-procedural-architecture) -- Floor rule distribution system
- [Shadows of Doubt DevBlog 13: Creating Procedural Interiors (ColePowered)](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/) -- Game-specific interior generation
- [Procedural Building Lot (UE Marketplace)](https://www.unrealengine.com/marketplace/en-US/product/procedural-building-lot) -- Style-based system with ground/mezzanine/trim/window/roof sections
- [PCG Modular Building Tutorial (UE5 Forums)](https://forums.unrealengine.com/t/community-tutorial-ue5-pcg-modular-building-tutorial-procedural-content-generation/2679812) -- PCG framework for modular buildings

### Architecture Reference
- [Tripartite Division (Chicago Architecture Center)](https://www.architecture.org/online-resources/architecture-encyclopedia/tripartite-division)
- [Window Placement: Use One Ratio (Fine Homebuilding)](https://www.finehomebuilding.com/2016/09/14/window-placement-use-one-ratio)
- [The Placement of Windows (misfits' architecture)](https://misfitsarchitecture.com/2023/12/10/the-placement-of-windows/)
- [Standard Door Sizes (U Brothers Construction)](https://ubrothersconstruction.com/blog/the-ultimate-guide-to-standard-door-size-heights-frames-and-door-dimensions)
- [Standard Window Sizes (DERCHI)](https://www.derchidoor.com/blogs/what-is-the-standard-size-of-doors-and-windows.html)
- [Cornice (Wikipedia)](https://en.wikipedia.org/wiki/Cornice)
- [Bay Window Dimensions (dimensions.com)](https://www.dimensions.com/collection/bay-windows-bow-windows)

### Existing Monolith Research
- `Docs/plans/2026-03-28-boolean-doors-research.md` -- Cutter positioning bug, merged cutter optimization, trim geometry
- `Docs/plans/2026-03-28-thin-wall-geometry-research.md` -- Sweep profiles for walls, UV box projection, corner handling
- `Docs/plans/2026-03-28-auto-collision-research.md` -- Collision for procedural meshes
- `Docs/plans/2026-03-28-proc-mesh-caching-research.md` -- Caching system for generated geometry
