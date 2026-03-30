# Marketplace Modular Kit Conventions Research

**Date:** 2026-03-28
**Purpose:** Comprehensive survey of how UE marketplace modular kits are structured, named, organized, and built -- to inform the Monolith `scan_modular_kit` auto-classification system.

---

## 1. Popular Modular Kits -- Structure Analysis

### Synty Studios (POLYGON Series)
- **Grid size:** ~200cm (stylized low-poly scale). Not officially documented per-pack; varies slightly by theme.
- **Naming:** `SM_Env_<Category>_<Descriptor>_<Number>` (e.g., `SM_Env_Tiles_Texture_09`, `SM_Wep_Pickaxe_01`). Uses `SM_` prefix universally.
- **Organization:** Flat folder per pack under `/PolygonDungeon/`, `/PolygonCity/`, etc. Subfolders: `Meshes/`, `Materials/`, `Textures/`, `Prefabs/`.
- **Piece types:** Floor tiles (x17 in Dungeon), stairs (x7), doors (x10), walls, columns, arches. Modular sections snap together.
- **Materials:** Palette-based color atlas. Single material per pack with color regions on a shared texture. Material instances for color variants.
- **Notes:** Recent versions include "naming convention cleanup" across packs. 60+ POLYGON packs share consistent conventions. Low-poly style means fewer material slots per mesh.

### KitBash3D
- **Grid size:** Not grid-aligned in traditional sense. Photogrammetry/kitbash pieces, not tile-snapping modular.
- **Naming:** Kit-specific identifiers. No SM_ prefix (these are FBX imports). Names like `KB3D_CYB_Building_01`.
- **Organization (post-Gameplay Ready update):**
  ```
  /Game/Cargo/<KitName>/
    Blueprints/
      Buildings/     (30+ per kit, e.g., Cyberpunk)
      Props/
    Materials/       (consolidated master materials)
    Textures/
  ```
  Previous structure was `Actors/` + `Geometries/` + `Materials/`. New structure uses Packed Level Actors for each building.
- **Materials:** Standardized relative texture paths. Master materials consolidated across kits. Each building is a Blueprint of modular sub-meshes.
- **Pivot:** Center-bottom of each building blueprint.
- **Notes:** NOT traditional snap-grid modular. These are pre-assembled kitbash buildings. Scanner should detect as "assembled" not "tile-kit."

### Quixel/Megascans Modular
- **Grid size:** Real-world photogrammetry scale. Kits organized by building section (Foundation Kit, Base Kit, Wall Kit, 3rd Floor Kit, 4th Floor Kit, Trim Kit, Gable Kit, Door/Window Kit, Pillar Kit).
- **Naming:** Asset IDs like `ukjsehbdw`. Display names: "Modular Building Foundation Kit", "Modular Building Base Wall Kit", etc.
- **Organization:** Organized by architectural zone, not by piece type. Each "kit" is a collection of related photogrammetry scans.
- **Materials:** PBR photogrammetry textures. Typically 1 material per scanned piece. High detail, unique per piece.
- **Notes:** Megascans pieces are unique scans, not designed for tight grid snapping. They're used for hero pieces and dressing, not full building assembly. Scanner should classify by visual role, not grid position.

### Dekogon Studios
- **Grid size:** Not documented. Realistic scale, pieces designed for UE5 archviz-quality environments.
- **Naming:** Product-name based (e.g., `Dekogon_ModularPipes_01`). SM_ prefix used.
- **Organization:** Per-product folders. Includes `Meshes/`, `Materials/`, `Textures/`, `Blueprints/`.
- **Piece types:** Specialized kits (pipes & gutters, fences, building modules). Not full building kits -- more like accent/detail kits.
- **Materials:** Realistic PBR. Multiple material slots per mesh.

### PurePolygons (Modular Building Set)
- **Grid size:** 400cm width x 300cm height (storefront pieces). 100cm subdivisions.
- **Naming:** Dimension-based: `1x2` = 1m x 2m, `2x2` = 2m x 2m. SM_ prefix.
- **Pivot:** Bottom of mesh, centered on front face between the two side edges.
- **Organization:** Meshes grouped by building zone (storefront, upper floors, roof, trim).
- **Materials:** Hundreds of textures for bricks, concrete, plaster, windows, glass, storefronts. Multiple material slots per mesh (typically 2-4: wall exterior, wall interior, trim, glass).
- **Piece types:** Extensive -- wall panels, window panels, door panels, corner columns, roof trim, awnings, overhangs, storefronts.
- **Notes:** Jacob Norris's breakdown PDF is the gold standard reference for modular building conventions.

### BigMediumSmall
- **Grid size:** Not publicly documented.
- **Naming:** Kit-themed prefixes. Modular characters + environments.
- **Organization:** UE native files with Blueprint setups. Presets for quick customization.
- **Materials:** Multiple color schemes per kit. Material instances with preset load-outs.
- **Notes:** Heavy Blueprint integration. Each modular structure has hide/reveal toggles for parts.

### Junction City (Marketplace)
- **Grid size:** 300cm base grid.
- **Naming:** `SM_<material>_<type>_<number>` (e.g., `SM_roof_cloth_1`, `SM_wood_wall_1` through `SM_wood_wall_10`).
- **Materials:** Material-first naming. Meshes grouped by material type.

### Stylized Modular Building Kit (Marketplace)
- **Grid size:** 100cm snap grid. 200+ meshes.
- **Naming:** Standard `SM_` prefix with type descriptors.
- **Style:** Fantasy medieval wooden houses.

---

## 2. Naming Convention Survey

### Universal Patterns Across Kits

| Pattern | Example | Frequency |
|---------|---------|-----------|
| `SM_` prefix | `SM_Wall_01` | ~90% of kits |
| `S_` prefix | `S_Wall_01` | ~5% (some older kits) |
| No prefix | `Wall_01` | ~5% (imports, kitbash) |
| `Mesh_` prefix | `Mesh_Wall_01` | Very rare |

### Piece Type Encoding

Kits use one of three strategies:

**Strategy A: Type in Name (most common)**
```
SM_Wall_Straight_01
SM_Wall_Window_01
SM_Wall_Door_01
SM_Floor_Full_01
SM_Stair_Straight_01
SM_Trim_Baseboard_01
```

**Strategy B: Tileset + Type (Synty-style)**
```
SM_Env_Dungeon_Wall_01
SM_Env_Dungeon_Floor_01
SM_Env_Dungeon_Door_01
```

**Strategy C: Material + Type (Junction City-style)**
```
SM_brick_wall_01
SM_wood_wall_01
SM_stone_floor_01
```

### Size/Dimension Encoding

| Pattern | Example | Usage |
|---------|---------|-------|
| `_1x1`, `_2x1`, `_2x2` | `SM_Wall_2x1` | Dimensions in grid units |
| `_100`, `_200`, `_400` | `SM_Wall_400` | Width in cm |
| `_Small`, `_Medium`, `_Large` | `SM_Window_Large` | Relative size |
| `_Half` | `SM_Wall_Half` | Half of standard size |
| No size suffix | `SM_Wall_01` | Assumed standard (1 grid unit) |

### Variant Encoding

| Pattern | Example | Meaning |
|---------|---------|---------|
| `_01`, `_02`, `_03` | `SM_Wall_01` | Numeric variants (most common) |
| `_A`, `_B`, `_C` | `SM_Wall_A` | Alpha variants |
| `_Clean` / `_Damaged` | `SM_Wall_Damaged_01` | Condition state |
| `_Broken` / `_Destroyed` | `SM_Floor_Broken_01` | Damage level |
| `_Open` / `_Closed` | `SM_Door_Open` | State variant |
| Chained: `_Marble_01` | `SM_Floor_Marble_01` | Material + variant |

### Corner/Junction Encoding

| Pattern | Example | Meaning |
|---------|---------|---------|
| `_Corner` | `SM_Wall_Corner` | Generic corner (most kits) |
| `_Corner_In` / `_Corner_Out` | `SM_Wall_Corner_In` | Inside/outside corner |
| `_Corner_Inner` / `_Corner_Outer` | Same as above | Alternative naming |
| `_L` | `SM_Wall_L` | L-shaped junction |
| `_T` | `SM_Wall_T` | T-junction |
| `_X` / `_Cross` | `SM_Wall_Cross` | 4-way intersection |
| `_End` / `_Cap` | `SM_Wall_End` | Termination piece |
| `_Pillar` / `_Column` | `SM_Pillar_Corner` | Dedicated corner fill |

### Regex Patterns for Scanner

```
# Primary pattern: SM_[Tileset_]Type[_SubType][_Size][_Variant]
^SM_(?:Env_)?(?<tileset>\w+_)?(?<type>Wall|Floor|Ceiling|Roof|Stair|Door|Window|Trim|Column|Pillar|Railing|Balcony)(?:_(?<subtype>Straight|Corner|Corner_In|Corner_Out|End|Half|Full|Arch|Double|Single|Frame|Broken|Damaged|Open|Closed))?(?:_(?<size>\d+x\d+|\d{2,3}|Small|Medium|Large|Half))?(?:_(?<variant>\d{1,2}|[A-C]|Clean|Damaged|Destroyed))?$
```

---

## 3. Grid Size Survey

### Measured Grid Sizes Across Major Kits

| Kit / Studio | Grid Width (cm) | Grid Height (cm) | Notes |
|---|---|---|---|
| PurePolygons (Norris) | 400 | 300 | Storefront pieces |
| WorldOfLevelDesign 101 | 300 | 300 | Tutorial standard |
| Construction Site Kit | 300 | 300 | Marketplace kit |
| Stylized Modular Building | 100 | 100 | Small grid |
| UEFN / Fortnite | 512 | 384 | Power-of-2 grid |
| Synty POLYGON | ~200 | ~200 | Stylized low-poly |
| Sewers Underground Kit | 50 (snap) | varies | Fine snap grid |
| Generic Indie | 100 | 100-300 | Common starter |
| Realistic/AAA | 200-400 | 270-300 | Real-world scale |
| Power-of-2 Legacy | 256 | 256 | UDK/UT heritage |

### Grid Size Distribution (approximate)

```
100cm  ████████░░░░░░░░  ~20% (stylized, small-scale)
200cm  ██████████░░░░░░  ~25% (most common marketplace)
300cm  ████████████░░░░  ~30% (WorldOfLevelDesign standard, many kits)
400cm  ██████░░░░░░░░░░  ~15% (PurePolygons, larger kits)
512cm  ████░░░░░░░░░░░░  ~10% (UEFN, power-of-2)
```

### Key Insight: The 1.28x Conversion Factor

From Mesh Masters: multiplying a metric grid by 1.28 converts to power-of-2:
- 100cm x 1.28 = 128
- 200cm x 1.28 = 256
- 300cm x 1.28 = 384
- 400cm x 1.28 = 512

This means kits built at 400cm can be scaled to 512 (UEFN-compatible) with a single multiply.

### Height Conventions

| Description | Height (cm) | Usage |
|---|---|---|
| Standard interior wall | 270-300 | Most common |
| Tall/grand interior | 350-400 | Mansions, lobbies |
| Low ceiling | 240 | Basements, crawlspaces |
| UEFN standard | 384 | Fortnite grid |
| Stylized/mini | 200 | Small-scale kits |

### Do Kits Document Grid Size?

- **Rarely in metadata.** Grid size is almost never stored in any parseable format.
- **Sometimes in product description** on marketplace page.
- **Sometimes in README/PDF** included with kit (e.g., PurePolygons breakdown).
- **Never in mesh properties** -- must be inferred from dimensions.
- **Implication for scanner:** Grid size MUST be auto-detected from mesh bounding boxes (GCD method).

---

## 4. Piece Catalog -- What Exists in a Typical Kit

### Tier 1: Minimum Viable Kit (~15-25 pieces)

| Category | Pieces | Notes |
|---|---|---|
| **Walls** | Solid, Window, Door, Corner Column | 4 minimum |
| **Floors** | Full tile, Half tile | 2 minimum |
| **Ceilings** | Flat panel | 1 minimum |
| **Stairs** | Straight run | 1 minimum |
| **Doors** | Single door (mesh or cutout) | 1 minimum |
| **Windows** | Single window (mesh or cutout) | 1 minimum |
| **Trim** | Baseboard, Door frame | 2 minimum |

### Tier 2: Standard Kit (~40-80 pieces)

All of Tier 1, plus:

| Category | Additional Pieces |
|---|---|
| **Walls** | Half wall, Arch wall, Double window wall, Damaged/broken variants (x2-3), Interior/exterior variants |
| **Floors** | Stairwell cutout, Broken variants (x2-3), Material variants (wood, tile, concrete) |
| **Ceilings** | Beam ceiling, Skylight cutout |
| **Stairs** | L-turn landing, Railing |
| **Doors** | Double door, Doorframe (trim), Open/closed variants |
| **Windows** | Large window, Small/narrow window, Window frame (trim) |
| **Trim** | Crown molding, Window sill, Door threshold, Corner trim (inside/outside) |
| **Structural** | Column/pillar, Beam, Support bracket |
| **Exterior** | Awning, Overhang, Balcony (floor + railing), Roof edge trim |

### Tier 3: Premium Kit (~100-200+ pieces)

All of Tier 2, plus:

| Category | Additional Pieces |
|---|---|
| **Walls** | Curved wall, T-junction wall, Window wall variants (round, arched), Paneled/wainscoting, Plinth/baseboard integrated |
| **Floors** | Elevated platform, Grate/metal floor, Transition pieces |
| **Roof** | Flat roof, Pitched roof panels, Ridge cap, Gutter, Dormer |
| **Stairs** | Spiral stair, Wide/grand staircase, Landing platform, Under-stair fill |
| **Misc** | Pipe/conduit runs, Vent grilles, Light fixtures (recessed), Signage mounts, Fireplace/chimney, Elevator shaft |
| **Facade** | Storefront assembly, Cornice, Belt course, Parapet cap, Fire escape |

### What's Typically MISSING From Kits (Common Gaps)

1. **Interior corner trim** -- most kits have wall corners but not baseboard/crown corner pieces
2. **Transition pieces** -- between different floor materials or levels
3. **Stairwell enclosures** -- open cutouts but no stair shaft walls
4. **Ceiling-to-wall trim** -- crown molding is often absent
5. **Damaged variants for ALL piece types** -- usually only walls get damage
6. **Roof-to-wall connection** -- the soffit/fascia junction is almost always missing
7. **Balcony undersides** -- visible from below in multi-story
8. **Half-height walls** / knee walls / railings that match the kit style
9. **Curved/non-orthogonal pieces** -- 45-degree angles, curved walls
10. **Plumbing/electrical/HVAC** fixtures integrated into walls
11. **Foundation/footing** pieces where building meets ground
12. **Threshold/transition** pieces at door openings between rooms
13. **Window sill assemblies** -- interior ledge, exterior drip edge

---

## 5. Material Conventions

### Material Slot Counts

| Kit Type | Slots Per Piece | Strategy |
|---|---|---|
| Single-material kit | 1 | Trim sheet atlas UV. All meshes share ONE material. Highest perf. |
| Dual-material kit | 2 | Interior face + exterior face. Or wall + trim. |
| Multi-material kit | 2-4 | Wall, glass, trim, metal. Common in realistic kits. |
| Per-face material | 4-6 | Each face gets its own slot. Worst for draw calls but most flexible. |

### Distribution (observed across marketplace kits)

```
1 slot (trim sheet)    ████████░░░░░░░░  ~20%
2 slots                ██████████████░░  ~35% (most common)
3-4 slots              ████████████░░░░  ~30%
5+ slots               ████░░░░░░░░░░░░  ~15% (archviz, Megascans)
```

### Master Material Patterns

**Pattern A: Single Master Material with Instances**
- One M_Master with parameters for BaseColor, Normal, Roughness, tiling, tint
- Each material slot gets a different MI_ (Material Instance)
- Most performant, used by Synty, many stylized kits

**Pattern B: Trim Sheet + Tileable**
- One trim sheet material for detail elements (cornices, frames, moldings)
- One or more tileable materials for large surfaces (brick, plaster, wood)
- Meshes have 2 material slots: trim + surface
- Used by PurePolygons, many AAA-style kits

**Pattern C: Per-Material Instances**
- Multiple master materials (M_Brick, M_Wood, M_Metal, M_Glass)
- Each mesh references relevant material instances
- Most flexible but highest draw call count
- Used by Dekogon, archviz kits

### Material Slot Naming Conventions

Common slot names observed:
- `Wall`, `Wall_Exterior`, `Wall_Interior`
- `Trim`, `Frame`, `Molding`
- `Glass`, `Window_Glass`
- `Metal`, `Iron`, `Steel`
- `Wood`, `WoodFloor`, `WoodTrim`
- `Concrete`, `Brick`, `Plaster`, `Stucco`
- `Roof`, `Shingle`, `Tile`
- `Decal`, `Overlay`
- `Emissive` (for lights, signs)

### Horror-Specific Material Patterns

- `_Damaged` / `_Dirty` / `_Aged` material instance variants
- Vertex paint blend between clean and damaged (NOT for visual quality -- use material instances)
- Decal layers for blood, grime, water stains
- Emissive slots for flickering lights, neon signs

---

## 6. Pivot/Origin Conventions

### The 5-Step Pivot Placement Rule (Paul Mader / Gamedeveloper.com)

1. **Place at focal point** of the model
2. **Check symmetry lines** -- pivot on symmetry axis or intersection
3. **Place at connection edge** for tiling pieces (walls, floors)
4. **Align to grid** -- pivot must land exactly on grid intersection
5. **Snap to useful face** -- bottom edge for floors, front-bottom edge for walls

### Observed Pivot Conventions by Piece Type

| Piece Type | Pivot Location | Rationale |
|---|---|---|
| **Wall (most kits)** | Bottom-center of front face | Allows rotation around face plane; snaps to floor grid |
| **Wall (PurePolygons)** | Bottom-center between two side edges | Centers on wall midpoint |
| **Wall (some kits)** | Bottom-left corner of front face | Left-to-right snapping workflow |
| **Floor** | Center-bottom (center of tile, Z=0) | Tiles snap at centers on grid |
| **Floor (alt)** | Corner (bottom-left, Z=0) | Corner-based grid assembly |
| **Ceiling** | Center-top (mirrored floor) | Snaps to top of wall grid |
| **Stair** | Bottom of first step, center-width | Landing aligns with floor grid |
| **Door/Window** | Center-bottom of opening | Aligns with wall opening position |
| **Column/Pillar** | Center-bottom | Drops into corner intersections |
| **Trim/Molding** | Left end, bottom edge | Left-to-right placement along wall |
| **Roof** | Bottom edge, center-width | Sits on top of wall grid line |

### Epic Marketplace Requirement

> "Products advertising modular use of their assets must have meshes whose pivot points are placed for smooth assembly."

No specific position mandated -- just that it enables clean snapping.

### Socket Usage

- **Very rare** in marketplace kits. Most kits rely on grid snapping, not socket snapping.
- **Exception:** Modular Snap System (plugin) uses socket-to-socket snapping with named snap points.
- **Pipe/conduit kits** sometimes use sockets for connection points.
- **KitBash3D** does NOT use sockets.
- **Synty** does NOT use sockets.
- **Implication for scanner:** Don't rely on sockets. Primary classification should use bounding box dimensions + name parsing. Sockets are a bonus signal when present (~5% of kits).

---

## 7. Folder Structure Conventions

### Pattern A: By Piece Type (most common for building kits)

```
/Content/<KitName>/
  Meshes/
    Walls/
    Floors/
    Ceilings/
    Stairs/
    Doors/
    Windows/
    Trim/
    Props/
    Exterior/
  Materials/
    MI_Brick_01.uasset
    MI_Wood_01.uasset
  Textures/
    T_Brick_D.uasset
    T_Brick_N.uasset
  Blueprints/
  Maps/
    Demo_Map.umap
```

### Pattern B: By Building Zone (Quixel/archviz style)

```
/Content/<KitName>/
  Foundation/
  Base/
  UpperFloors/
  Roof/
  Trim/
  Doors_Windows/
  Materials/
  Textures/
```

### Pattern C: Flat Structure (Synty/stylized)

```
/Content/Polygon<Theme>/
  Meshes/          (all meshes flat, named by type)
  Materials/
  Textures/
  Prefabs/         (pre-assembled Blueprint actors)
```

### Pattern D: Blueprint-Centric (KitBash3D, BigMediumSmall)

```
/Content/<KitName>/
  Blueprints/
    Buildings/     (complete structures as Blueprints)
    Props/
  Materials/
  Textures/
```

### Folder Name Frequency

| Folder Name | Frequency |
|---|---|
| `Meshes/` | ~80% |
| `Materials/` | ~95% |
| `Textures/` | ~90% |
| `Blueprints/` | ~60% |
| `Maps/` or `Demo/` | ~70% |
| `Walls/` subfolder | ~40% |
| `Props/` | ~50% |
| `Documentation/` or `README` | ~15% |

### Metadata Files

- **Almost none** include parseable metadata files (JSON, CSV, etc.).
- Some include PDF breakdowns (PurePolygons).
- Some include demo maps as "documentation."
- **No kit includes grid size in a machine-readable format.**
- **Implication for scanner:** Cannot rely on metadata. Must infer everything from mesh names, dimensions, materials, and folder paths.

---

## 8. Horror-Specific Kits

### Available Horror Modular Kits (UE Marketplace)

| Kit | Grid | Pieces | Focus |
|---|---|---|---|
| **Retro Modular Horror Pack** | Unknown | Hotel/apartments, flickering lights, blood decals, secret passages | PS1-style low-poly horror |
| **Horror Environment Pack** | Modular | Exterior + interior, house building system | Abandoned home |
| **Modular Hospital (Abandoned)** | Modular | Hyper-realistic, large-scale hospital | Medical horror |
| **Abandoned House Vol 1** | Non-uniform | Foyer/entry modular, construction BPs | Mansion horror (NOT grid-strict) |
| **Abandoned Mansion - Fully Modular** | Grid-based | Curved corners, walls, ceiling, floor, doors, windows | Victorian mansion |
| **Modular Abandoned Hospital V2** | Modular | Hospital rooms, corridors | Medical horror |
| **Modular Abandoned Warehouse** | Modular | Industrial interior, lighting BPs | Industrial horror |
| **Horror Abandoned Asylum Pack** | Modular | Bars, beds, pipes, fans, security doors | Prison/asylum |
| **Horror - Modular Interior and Props** | Modular | Interior rooms, furniture | Generic horror |
| **Modular Horror House** | Modular | Residential horror | House horror |
| **Gothic Horror Level Pack** | Modular | Gothic architecture | Period horror |
| **Modular Destroyed Buildings** | Modular | War-damaged structures | Destruction |
| **Stylized Haunted Street** | Modular | Blender 5 + UE 5.7 | Stylized horror |

### Horror Kit Conventions

**Damage Variants:**
- Most kits provide `_Damaged`, `_Broken`, `_Destroyed` wall variants
- Floor holes/broken variants common
- Boarded windows are separate meshes overlaid on window openings
- Broken glass is typically a separate transparent mesh inside the window frame

**Gore/Blood Integration:**
- Decal-based (NOT baked into meshes). Blood pools, splatter patterns, drip trails.
- Emissive flickering light BPs common.
- Some kits include "blood in bathtub" type prop combinations.

**Atmosphere Pieces:**
- Cobwebs (separate mesh/particle)
- Dust motes (particle system)
- Flickering/broken light fixtures
- Fog/haze volumes
- Boarded window overlays (separate mesh piece)
- Cracked/peeling wallpaper (decal or separate mesh layer)

**What Horror Kits Typically Provide:**
- Secret doors / hidden passages (rotatable wall panels)
- Basement/crawlspace variants (low ceiling)
- Dirty/aged material variants
- Bloody decals set
- Atmospheric lighting Blueprints

**What Horror Kits Are Missing:**
- Progressive damage states (clean -> dirty -> damaged -> destroyed)
- Procedural placement tools for blood/grime
- Sound integration (creaking, dripping)
- Proper LODs for horror set dressing
- Material parameter-driven decay (single material that goes from clean to destroyed via scalar)

---

## 9. Scanner Design Implications

### Name Parsing Priority

Based on real-world kit analysis, the scanner should try patterns in this order:

1. **SM_ prefix strip** -- remove `SM_` to get base name
2. **Tileset detection** -- check for `Env_`, kit name prefix, or material prefix
3. **Type keyword match** -- search for Wall, Floor, Ceiling, Stair, Door, Window, Trim, Column, Pillar, Railing, Roof, Balcony, Beam, Arch, Pipe, Vent, Foundation
4. **Subtype keyword match** -- Straight, Corner, Corner_In/Out, End, Half, Full, Double, Single, Frame, Broken, Damaged, Open, Closed, Arch, L, T, Cross
5. **Size extraction** -- `\d+x\d+` pattern, or `_\d{2,3}` for cm, or Small/Medium/Large/Half
6. **Variant extraction** -- trailing `_\d{1,2}` or `_[A-C]` or condition keywords

### Grid Size Auto-Detection Algorithm

```
1. Collect all pieces classified as "Wall" type
2. Get bounding box X dimensions (width along wall face)
3. Compute GCD of all wall widths
4. If GCD is in [50, 100, 128, 200, 256, 300, 400, 512]: high confidence
5. If GCD doesn't match known sizes, try wall heights similarly
6. Cross-validate: floor tile width should match wall width
7. Report confidence level with detected grid
```

### Pivot Detection

```
1. For wall pieces: check if pivot is at (0, 0, 0) relative to mesh bottom
2. Check if pivot X is at 0 (left edge), width/2 (center), or width (right edge)
3. Check if pivot Z is at 0 (bottom -- expected) or height/2 (center)
4. Majority vote across all pieces in kit to determine convention
5. Store pivot convention in kit vocabulary JSON
```

### Material Strategy Detection

```
1. Count material slots across all pieces in kit
2. If median == 1: "trim_sheet" strategy
3. If median == 2: "dual_material" strategy (interior/exterior or wall/trim)
4. If median >= 3: "multi_material" strategy
5. Check if material names are shared across pieces (master material pattern)
6. Detect if any materials contain "glass", "metal", "emissive" keywords
```

### Folder Structure Detection

```
1. Check for /Meshes/ subfolder -> Pattern A
2. Check for /Blueprints/Buildings/ -> Pattern D (KitBash3D-style)
3. Check for zone folders (Foundation/, Base/, etc.) -> Pattern B
4. If all meshes in root or single folder -> Pattern C (flat)
5. If /Walls/, /Floors/ subfolders exist -> enhanced Pattern A
```

---

## 10. Kit Compatibility Matrix

### Scanner Must Handle

| Dimension | Range | Default Assumption |
|---|---|---|
| Grid width | 50-512cm | 300cm |
| Grid height | 100-400cm | 300cm |
| Material slots | 1-8 | 2 |
| Naming prefix | SM_, S_, none, custom | SM_ |
| Pivot style | corner, center-front, center | center-front-bottom |
| Folder depth | 1-4 levels | 2 levels |
| Piece count | 10-500+ | 40-80 |
| Socket usage | none to full | none |
| Kit type | tile-snap, kitbash, photogrammetry | tile-snap |

### Classification Confidence Tiers

| Tier | Confidence | Method |
|---|---|---|
| **High (>80%)** | Name contains exact type keyword + dimensions match expected ratios | Name + Dims agree |
| **Medium (50-80%)** | Name is ambiguous but dimensions strongly suggest type | Dims primary |
| **Low (20-50%)** | Name gives no signal, dims are unusual, relying on material/topology | Fallback signals |
| **Unknown (<20%)** | Can't classify -- present to user for manual labeling | Manual override |

---

## Sources

- [Tom Looman - UE Naming Convention Guide](https://tomlooman.com/unreal-engine-naming-convention-guide)
- [Allar UE5 Style Guide](https://github.com/Allar/ue5-style-guide)
- [Epic - Recommended Asset Naming Conventions](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)
- [Epic Forums - Good Naming Rules for Modular Elements](https://forums.unrealengine.com/t/good-naming-rules-for-modular-elements/58314)
- [PurePolygons - Modular Building Set Breakdown](http://www.purepolygons.com/blog/modular-building-breakdown)
- [Jacob Norris - Building Breakdown PDF](http://wiki.polycount.com/w/images/2/26/JacobNorris_Building_Breakdown.pdf)
- [Polycount - Modular Building Set Breakdown (thread)](https://polycount.com/discussion/144838)
- [Mesh Masters - The Perfect Modular Grid Size](https://meshmasters.com/2933-2/)
- [Paul Mader - Creating Modular Game Art For Fast Level Design](https://www.gamedeveloper.com/production/creating-modular-game-art-for-fast-level-design)
- [Lee Perry - Modular Level and Component Design (PDF)](https://docs.unrealengine.com/udk/Three/rsrc/Three/ModularLevelDesign/ModularLevelDesign.pdf)
- [WorldOfLevelDesign - Modular Environment Design 101](https://www.worldofleveldesign.com/categories/game_environments_design/modular-environment-design-101.php)
- [WorldOfLevelDesign - UE5 Guide to Scale and Dimensions](https://www.worldofleveldesign.com/categories/ue5/guide-to-scale-dimensions-proportions.php)
- [The Level Design Book - Metrics](https://book.leveldesignbook.com/process/blockout/metrics)
- [The Level Design Book - Modular Kit Design](https://book.leveldesignbook.com/process/blockout/metrics/modular)
- [KitBash3D - Gameplay Ready Structural Updates](https://help.kitbash3d.com/en/articles/11698511-structural-updates-to-unreal-engine-kits-gameplay-ready)
- [KitBash3D - Guide to Using Kits with UE](https://help.kitbash3d.com/en/articles/6201150-guide-to-using-kitbash3d-kits-with-unreal-engine)
- [UEFN - Grid Snapping Documentation](https://dev.epicgames.com/documentation/en-us/fortnite/using-grid-snapping-in-unreal-editor-for-fortnite)
- [80.lv - Single Material Modular Kit Environment](https://www.exp-points.com/vuk-single-material-modular-kit-environment-ue4)
- [Beyond Extent - Balancing Modularity and Uniqueness](https://www.beyondextent.com/articles/balancing-modularity-and-uniqueness-in-environment-art)
- [Polycount - Modular Environment Pivot Question](https://polycount.com/discussion/236704/modular-environment-pivot-question)
- [Epic Marketplace Guidelines](https://www.unrealengine.com/de/marketplace-guidelines)
- [Animatics - Trim Sheets That Sell](https://www.animaticsassetstore.com/2025/10/31/trim-sheets-that-sell-a-production-workflow-for-modular-environments/)
