# PCG for Terrain Modification, Landscape Sculpting, and Ground Surface

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** `2026-03-28-pcg-framework-research.md`, `reference_terrain_adaptive_buildings.md`, `reference_city_block_layout.md`

---

## Executive Summary

PCG **cannot write to landscape heightmaps or weightmaps**. It is a read-only consumer of landscape data. Terrain modification for a procedural horror town requires a separate pipeline that uses UE's landscape editing APIs directly (`FLandscapeEditDataInterface`, `ALandscapeBlueprintBrushBase`, Landscape Splines). PCG then layers on top as a scatter/placement engine informed by the terrain it reads. This document covers all 11 research topics with verified UE 5.7 APIs and a concrete integration plan for Leviathan.

---

## 1. PCG + Landscape Interaction (Read vs Write)

### What PCG Can Do (READ)

PCG has robust **read** access to landscapes via `UPCGLandscapeData`:

- **Height sampling** via `SamplePoint()` / `ProjectPoint()` -- projects points onto landscape surface
- **Normal/slope** -- `bGetHeightOnly = false` returns full normal+tangent per point
- **Layer weights** -- `bGetLayerWeights = true` returns `FPCGLandscapeLayerWeight` (name + weight) per landscape material layer
- **Physical materials** -- `bGetPhysicalMaterial = true` adds PhysicalMaterial attribute
- **Component coordinates** -- `bGetComponentCoordinates = true` adds CoordinateX/Y
- **GPU sampling** -- `bSampleVirtualTextures` for VT-based weight/normal reads
- **Landscape spline data** -- `UPCGLandscapeSplineData` wraps `ULandscapeSplinesComponent` for polyline access
- **Cache** -- `UPCGLandscapeCache` / `FPCGLandscapeCacheEntry` caches per-component height + layer data, serializable

**Key PCG nodes for landscape reads:**
- `Surface Sampler` -- generates point grid on landscape surface
- `Get Landscape Data` -- retrieves landscape as spatial data
- `Normal to Density` -- converts surface normal to 0-1 density (flat=1, cliff=0)
- `Density Filter` -- keeps points within density range (slope filtering)
- `Density Noise` -- adds procedural variation to density
- `Projection` -- projects points onto landscape surface
- `Wait Until Landscape Ready` -- synchronization gate (new in 5.7)
- `Generate Landscape Textures` -- reads grass maps / VT data (replaces deprecated `GenerateGrassMaps`)

### What PCG Cannot Do (WRITE)

PCG has **zero** write access to landscape data. No built-in PCG node can:
- Modify heightmap values
- Paint landscape layer weights
- Create/modify landscape splines
- Flatten terrain
- Carve drainage channels

This is a fundamental architectural limitation -- PCG is a placement engine, not a terrain editor.

### Verified UE 5.7 Heightmap Write API

The **only** way to programmatically modify landscape heightmaps:

```cpp
// Header: Runtime/Landscape/Public/LandscapeEdit.h
struct FLandscapeEditDataInterface : public FLandscapeTextureDataInterface
{
    // Construct for current edit layer
    FLandscapeEditDataInterface(ULandscapeInfo* InLandscape, bool bInUploadTextureChangesToGPU = true);

    // Construct for specific edit layer
    FLandscapeEditDataInterface(ULandscapeInfo* InLandscape, const FGuid& InEditLayerGUID, bool bInUploadTextureChangesToGPU = true);

    // Switch edit layer
    void SetEditLayer(const FGuid& InEditLayerGUID);

    // Write heightmap (uint16 values, 32768 = sea level)
    void SetHeightData(int32 X1, int32 Y1, int32 X2, int32 Y2, const uint16* InData, int32 InStride,
        bool InCalcNormals, const uint16* InNormalData = nullptr, ...);

    // Read heightmap
    void GetHeightData(int32& X1, int32& Y1, int32& X2, int32& Y2, uint16* Data, int32 Stride);

    // Write landscape layer weights
    void SetAlphaData(ULandscapeLayerInfoObject* LayerInfo, const int32 X1, const int32 Y1,
        const int32 X2, const int32 Y2, const uint8* Data, int32 Stride,
        ELandscapeLayerPaintingRestriction PaintingRestriction = None);

    // Read landscape layer weights
    void GetAlphaData(ULandscapeLayerInfoObject* LayerInfo, int32& X1, int32& Y1,
        int32& X2, int32& Y2, uint8* Data, int32 Stride);
};
```

**Important UE 5.7 changes:**
- Non-edit-layer landscapes are **deprecated** (all landscapes use edit layer system now)
- `FLandscapeEditDataInterface` constructor takes optional `FGuid& InEditLayerGUID` to target specific edit layers
- `ULandscapeEditLayerBase` is the base class for all edit layers (Abstract, with `SupportsTargetType`, `NeedsPersistentTextures`, etc.)

### Height Read API

```cpp
// Runtime/Landscape/Classes/LandscapeProxy.h
TOptional<float> ALandscapeProxy::GetHeightAtLocation(FVector Location,
    EHeightfieldSource HeightFieldSource = EHeightfieldSource::Complex) const;
```

### Landscape Spline Deformation API

```cpp
// Runtime/Landscape/Private/LandscapeSplineRaster.cpp
bool ULandscapeInfo::ApplySplines(TSet<TObjectPtr<ULandscapeComponent>>* OutModifiedComponents,
    bool bMarkPackageDirty);
```

This modifies heightmap AND weightmaps based on spline control point properties (width, falloff, raise/lower amounts, layer painting).

### Blueprint Brush API (Non-Destructive)

```cpp
// Runtime/Landscape/Public/LandscapeBlueprintBrushBase.h
class ALandscapeBlueprintBrushBase : public AActor, public ILandscapeEditLayerRenderer
{
    bool AffectHeightmap;
    bool AffectWeightmap;
    bool AffectVisibilityLayer;
    TArray<FName> AffectedWeightmapLayers;

    // Override to render heightmap/weightmap modifications
    virtual UTextureRenderTarget2D* RenderLayer(const FLandscapeBrushParameters& InParameters);

    // Trigger landscape update
    void RequestLandscapeUpdate(bool bInUserTriggered = false);
};
```

Blueprint Brushes are the **recommended** non-destructive approach. They render to edit layers via render targets and compose with the landscape's edit layer stack. The Landmass plugin (ships with UE) provides `CustomBrush_Landmass` (plateau/hill/erosion) and `CustomBrush_LandmassRiver` (spline-based carving).

---

## 2. Flat Zones for Buildings (Building Pads)

### Problem
Procedurally placed buildings need flat ground. Terrain under building footprints must be leveled to the building's target elevation.

### Approach A: FLandscapeEditDataInterface (Destructive, Direct)

1. For each building footprint, determine target Z elevation (average of terrain samples, or explicit from city layout)
2. Convert world-space footprint to landscape component coordinates
3. Read existing heightmap via `GetHeightData()`
4. Write flattened values via `SetHeightData()` with cosine-blended falloff at edges
5. Call `UpdateAllComponents()` to refresh collision + rendering

**Height encoding:** UE landscape heights are `uint16`, where `32768 = 0.0` world units relative to landscape origin. Scale factor = `LandscapeScale.Z / 128.0` (typically 100.0/128.0 = 0.78125 cm per unit).

```
TargetHeight_uint16 = FMath::RoundToInt((TargetWorldZ - LandscapeOrigin.Z) / ScaleFactor + 32768.0f);
```

### Approach B: Landscape Blueprint Brush (Non-Destructive, Preferred)

1. Spawn `ALandscapeBlueprintBrushBase` subclass per building cluster or block
2. In `RenderLayer()`, draw flat rectangles at building footprint locations into the height render target
3. Use cosine falloff at pad edges for smooth blending
4. Brush lives on a dedicated edit layer ("ProceduralBuildings") that can be toggled

**Advantages:** Non-destructive, revertible, stacks with other edit layers, supports iteration.

### Approach C: Landscape Splines (Semi-Automated)

Create closed-loop landscape splines around building footprints with "Raise/Lower" properties set to flatten. Call `ULandscapeInfo::ApplySplines()`. Less precise for rectangular buildings.

### Recommendation for Leviathan

**Approach B** (Blueprint Brush) on a dedicated edit layer. One brush actor per city block, rendering all building pads for that block. Monolith MCP spawns and configures the brush programmatically.

### Edge Blending

Building pad edges need smooth falloff to avoid visible cliff edges:
- **Cosine blend** over 2-4m distance: `alpha = 0.5 * (1 + cos(PI * distance / falloffWidth))`
- **Terrain slope compensation**: steeper terrain needs wider falloff
- Use landscape layer painting at pad edge to transition from grass/dirt to foundation material

---

## 3. Road Cuts (Lowering Terrain Along Road Paths)

### Landscape Splines (Native Solution)

UE's landscape spline system is purpose-built for roads:

- Control points define road centerline
- Per-point properties: width, falloff distance, raise/lower amount
- `ULandscapeInfo::ApplySplines()` modifies heightmap AND paints road layer
- Automatic cosine-blended falloff on each side

**5.7 UI Note:** The "Deform Landscape to Splines" button location changed in 5.7 (community reports confusion finding it). The API still works programmatically.

### Programmatic Road Cut Pipeline

1. **Input:** Road polyline from city block layout (centerline + width)
2. **Create landscape spline** segments programmatically via `ULandscapeSplinesComponent`
3. **Set control point properties:**
   - Width = road width (e.g., 800cm for 2-lane)
   - Falloff = 200-400cm for road shoulders
   - Raise/Lower = calculated from desired road grade
   - Layer: paint "Asphalt" landscape layer
4. **Call `ApplySplines()`** to deform heightmap + paint layers
5. **PCG reads** the modified landscape for road furniture scatter (guardrails, signs, debris)

### Grade/Slope for Roads

Real-world road grades for a horror town:
- **Main roads:** max 6-8% grade (comfortable driving)
- **Residential:** up to 12-15% grade
- **Alleys:** up to 20% (barely passable, horror-appropriate)
- **Stairs replacing roads:** >25% grade (Silent Hill vibes)

Implementation: sample terrain height at road spline points, compute required grade, lower/raise terrain to achieve target grade with smoothing.

### Drainage Gutters

Road cross-section should crown (highest at center, sloping 2% to edges). This is below landscape resolution for most setups -- handle via:
- Road mesh with crowned cross-section placed via spline mesh
- Gutter meshes at road edges
- PCG scatters drain grates at low points

---

## 4. Parking Lots

### Geometry

Parking lots are flat areas (building pad approach from Section 2) with:
- Asphalt ground surface (landscape layer painting or ground plane mesh)
- Line markings (decals or procedural material)
- Curbs (edge meshes)
- Light poles, concrete stops, drainage

### Implementation Approaches

**A. Landscape-Based (Integrated)**
1. Flatten terrain under parking lot footprint (building pad technique)
2. Paint "Asphalt" landscape layer via `SetAlphaData()`
3. Scatter line marking decals via PCG
4. Place curb/bollard/light meshes via PCG

**B. Ground Plane Mesh (Floating)**
1. Spawn flat static mesh at parking lot elevation
2. Apply asphalt material with procedural line markings
3. Use RVT (Runtime Virtual Texture) blending to merge edges with landscape
4. PCG scatters props on mesh surface

**C. Hybrid (Recommended)**
1. Flatten landscape + paint base asphalt layer
2. Use RVT decal planes for line markings (snap to ground, blend with terrain)
3. PCG scatters all props and details

### Procedural Line Markings

Marketplace reference: "Procedural Road Markings" (120 decals with age/crack params). For custom generation:
- Standard parking space: 270cm wide x 550cm deep (US)
- Handicap space: 370cm wide + 150cm access aisle
- Angled parking: 60-degree common, saves space
- Material approach: world-aligned UV projection with stripe pattern + noise for wear

### Horror Parking Lots

- Cracked/heaved asphalt (displacement map or mesh deformation)
- Missing/flickering light poles (one working light = classic horror)
- Abandoned cars (PCG scatter with horror weight)
- Oil stains, blood stains (decal scatter)
- Overgrown weeds through cracks (foliage in crack decal regions)
- Flooded sections (puddle decals at low points)

---

## 5. Yards and Gardens (Residential Ground Cover)

### PCG-Driven Yard Generation

PCG excels here -- this is pure scatter/placement:

1. **Define yard zones** from spatial registry (lot footprint minus building footprint minus driveway)
2. **Surface Sampler** on landscape within yard bounds
3. **Layer-based filtering:** only scatter on "Grass" layer (read via `bGetLayerWeights`)
4. **Slope filtering:** Normal to Density -> Density Filter (keep flat areas)

### Yard Elements (PCG Scatter Categories)

| Element | Approach | Horror Variant |
|---------|----------|---------------|
| Lawn grass | Landscape grass type (auto from layer) | Overgrown, dead patches |
| Garden beds | PCG volume + flower/shrub scatter | Dead plants, thorny weeds |
| Fences | Spline mesh along lot boundary | Broken/leaning sections |
| Trees | PCG scatter with exclusion zones | Dead/bare trees, single creepy tree |
| Garden path | Decal strip or spline mesh | Cracked, overgrown |
| Lawn furniture | PCG point scatter | Overturned, rusted |
| Garden shed | Single placement per yard | Door ajar, dark interior |
| Mailbox | At lot front edge | Crooked, number missing |
| Kids' toys | PCG scatter (small objects) | Abandoned, weathered |

### Landscape Layer Painting for Yards

Paint landscape layers per zone:
- **Yard interior:** "Grass" layer (green/brown variants)
- **Near foundation:** "Dirt" layer (bare earth band along building)
- **Walkway:** "Concrete" or "Gravel" layer
- **Driveway:** "Asphalt" or "Concrete" layer

Use `FLandscapeEditDataInterface::SetAlphaData()` with yard polygon rasterized to landscape coordinates.

### Horror Yard Variations (Decay Parameter 0-1)

- **0.0 (maintained):** Green grass, trimmed bushes, flowers
- **0.3 (neglected):** Patchy grass, some weeds, peeling fence paint
- **0.6 (abandoned):** Overgrown, dead patches, broken fence, junk scattered
- **1.0 (derelict):** Waist-high weeds, collapsed fence, trees dead/fallen, trash

---

## 6. Terrain Debris (Rocks, Rubble, Broken Concrete)

### PCG Scatter Pipeline

This is PCG's core competency:

```
Surface Sampler -> Normal to Density -> Density Filter -> Density Noise
    -> Static Mesh Spawner (debris meshes)
```

### Debris Categories for Horror Town

| Category | Meshes | PCG Rules |
|----------|--------|-----------|
| Natural rocks | 5-8 rock meshes, 3 sizes | Slope-biased, cluster near terrain features |
| Rubble/concrete | Broken slab pieces | Near buildings, roads, walls |
| Trash/litter | Cans, bottles, papers | Wind-drift bias (accumulate in corners) |
| Broken glass | Small mesh scatters | Near windows, storefronts |
| Vehicle parts | Hubcaps, bumpers | Near roads, parking lots |
| Construction debris | Rebar, pipes, boards | Near damaged buildings |
| Organic | Fallen branches, dead leaves | Under trees, in gutters |

### PCG Terrain-Aware Scattering

Key PCG techniques for ground debris:

1. **Slope-based density:** More debris in flat areas and concavities, less on steep slopes
2. **Layer-based filtering:** Different debris on different ground types (concrete debris on asphalt, rocks on dirt)
3. **Wind accumulation:** Bias small debris toward walls, fences, corners using distance-to-actor
4. **Gutter accumulation:** Higher density along road edges, in drainage channels
5. **Building proximity:** More rubble within 5-10m of damaged buildings

### Collision-Aware Scatter

Reference existing Monolith `collision_aware_scatter` action -- extend to use PCG Density from landscape normal data for terrain-adaptive debris placement.

---

## 7. Puddles and Water Features

### Puddle Approaches in UE5

**A. Decal Puddles (Simplest, Best for Scatter)**
- Flat decal actors with puddle material (reflective, transparent)
- PCG scatter at terrain low points
- Marketplace: "Puddles" pack (72 unique stickers)
- Material: screen-space reflections + normal perturbation for ripples

**B. Landscape Material Puddles (Height-Based)**
- Landscape material with puddle layer that activates in low areas
- Use world-position-based threshold: below certain Z -> blend to wet/puddle
- Rain intensity parameter drives puddle coverage
- No additional geometry needed

**C. Nanite Displacement Puddles (UE 5.5+)**
- Displacement-based water surface embedded in ground mesh
- Tutorial reference for realistic water puddles via Nanite displacement

**D. Water Body (Large Puddles / Drainage)**
- `AWaterBody` for larger standing water (flooded lots, drainage ponds)
- PCG + Water System integration documented in UE 5.4+ tutorials
- Interacts with landscape (carves terrain, applies water layer)

### Low-Point Detection for Puddle Placement

Algorithm for finding natural water accumulation points:

1. Sample landscape height on grid (e.g., 200cm spacing)
2. For each sample, compare to 8 neighbors
3. Points where all neighbors are higher = local minimum (puddle candidate)
4. Expand puddle region outward until rim height reached
5. Puddle size proportional to catchment area

PCG implementation:
```
Surface Sampler (high density) -> Get Landscape Data (height)
    -> Custom Blueprint Node (local minimum detection)
    -> Filter to low points -> Spawn puddle decals/meshes
```

### Storm Drains and Gutters

- Spawn drain grate meshes at road low points (intersection of road + terrain low)
- PCG scatter along road edges at regular intervals (every 30-50m)
- Gutters: spline mesh along road edges
- Horror: clogged drains, overflow, mysterious sounds from drains

### Horror Water Features

- Standing water in basements, parking garages (fog + reflection)
- Flooded streets (WaterBody along road splines where grade dips)
- Dripping from damaged roofs (Niagara particle at roof edges)
- Sewage leaks (colored water + particle effects near damaged infrastructure)
- Puddles that reflect things that aren't there (screen-space reflection trick)

---

## 8. Ground Texturing (Landscape Layer Painting)

### Programmatic Layer Painting Pipeline

Using `FLandscapeEditDataInterface::SetAlphaData()`:

1. **Define zones** from spatial registry (road, sidewalk, yard, building pad, parking, alley)
2. **Rasterize zones** to landscape coordinate grid
3. **For each zone, paint primary layer:**
   - Road -> "Asphalt" layer
   - Sidewalk -> "Concrete" layer
   - Yard -> "Grass" layer
   - Building foundation -> "Concrete" or "Dirt" layer
   - Parking lot -> "Asphalt" layer
   - Alley -> "Dirt" or "Gravel" layer
4. **Edge blending:** cosine falloff at zone boundaries
5. **Dirt transition:** always paint "Dirt" layer in 50-100cm band at building-to-ground transition

### Landscape Material Layer Setup

Recommended layers for horror town:

| Layer | Physical Material | Usage |
|-------|------------------|-------|
| Grass | Grass | Yards, parks, medians |
| Dirt | Dirt | Bare ground, building perimeter, unpaved |
| Asphalt | Asphalt | Roads, parking lots |
| Concrete | Concrete | Sidewalks, foundations, slabs |
| Gravel | Gravel | Alleys, driveways, paths |
| Mud | Mud | Wet/low areas, drainage |
| Sand | Sand | Construction sites, fill areas |

Each layer material should have:
- Base color + normal + roughness
- **Age/damage parameter** (0-1): fresh -> cracked -> heavily damaged
- World-aligned UV projection for tiling without seams
- Macro variation (large-scale noise to break tiling)

### RVT (Runtime Virtual Texture) Integration

For detail that goes beyond landscape layer resolution:
- Road lane markings
- Crosswalk patterns
- Parking space lines
- Drain grate details
- Foundation cracks

RVT allows mesh planes to blend seamlessly with landscape material. Place RVT-compatible meshes and they composite into the landscape's virtual texture.

### PCG-Driven Painting (Indirect)

While PCG can't paint layers directly, it can:
1. **Read** landscape layers and adjust scatter accordingly
2. **Place RVT decal meshes** that visually modify ground appearance
3. **Drive a post-process** that generates painting data (PCG output -> custom tool -> SetAlphaData)

### Horror Ground Texturing

- Blood stains (decals, not layer painting -- need precise placement)
- Oil/chemical spills (decals with emission)
- Scorch marks (decal + slight height depression)
- Tire tracks (decals along paths)
- Drag marks (decals from doors/dumpsters)
- Overgrown cracks (grass/weed meshes in asphalt crack decals)

---

## 9. Elevation Variation (Hills, Slopes, Drainage)

### Terrain Generation for Horror Town

**A. External Heightmap (Recommended for Base Terrain)**

Generate base terrain in World Machine / Gaea / World Creator:
- Import 16-bit PNG heightmap via `ALandscapeProxy::Import()`
- Resolution: 2017x2017 for 2km x 2km town (1m/pixel)
- Include natural drainage patterns, gentle hills

**B. Procedural In-Engine**

For runtime or fully procedural approach:
1. Multi-octave Perlin noise for base terrain
2. Hydraulic erosion pass for natural drainage channels
3. Flatten building zones
4. Cut road grades

### Elevation Design for Horror Town

Real-world small town terrain characteristics:
- **Gentle overall slope:** 2-5% grade across town (natural drainage)
- **Hill neighborhoods:** houses on 5-15% slopes, streets switchback
- **Drainage low line:** creek or drainage channel at town's lowest point
- **Flood zone:** low-lying areas near drainage (gas station, warehouse district)
- **Ridge/high point:** often where church, mansion, or water tower sits

### Drainage Channel Generation

1. Define drainage path from town layout (low point polyline)
2. Use landscape spline with lowered profile (V-shaped cross section)
3. Apply "Mud" layer along channel bottom
4. PCG scatter: tall grass, trash, standing water at wide spots
5. Bridges where roads cross drainage (pre-placed or procedural)

### Sloped Street Handling

For streets on slopes:
- Max playable slope for FPS: 26.6 degrees (50% grade) -- uncomfortable above 15%
- Stairs at >25% grade
- Switchback roads on steep terrain
- Retaining walls at grade changes (Section 10)

---

## 10. Retaining Walls

### When Retaining Walls Are Needed

- Building pad at different elevation than adjacent terrain (>50cm difference)
- Road cut through hill (exposed cut face)
- Parking lot below grade
- Stepped terrain (terraced neighborhood)
- Drainage channel banks

### Generation Approaches

**A. GeometryScript Mesh (Current Monolith Approach)**

Reference existing `reference_terrain_adaptive_buildings.md` research:
- Generate wall mesh segments following terrain contour
- Height = elevation difference at each point
- Cap mesh at top, footing at base
- MaterialID for concrete face, dirt-stained base, cap

**B. Spline Mesh (Flexible)**

1. Define retaining wall path as spline (edge of building pad, road cut edge)
2. Sweep wall cross-section along spline
3. Cross-section profile: vertical face + small cap + slight base batter (2-5 degree lean)
4. Height varies with terrain difference

**C. Modular Pieces (Best Visual Quality)**

- Prebuilt retaining wall segments (straight, corner, end cap)
- HISM placement along boundary
- Height tiers (1m, 2m, 3m wall pieces)
- PCG scatter: moss, stains, cracks on wall face

### Retaining Wall Types for Horror Town

| Type | Appearance | Where |
|------|-----------|-------|
| Concrete block | Gray, modular, functional | Commercial, parking lots |
| Poured concrete | Smooth face, form lines | Major roads, infrastructure |
| Stone/fieldstone | Rustic, irregular | Residential, older areas |
| Railroad tie/timber | Dark wood, deteriorating | Yards, gardens |
| Gabion | Wire cage + rocks | Modern infrastructure |

### Horror Retaining Walls

- Bulging/leaning walls (foundation failure)
- Cracked with vegetation growing through
- Water seepage staining
- Collapsed sections exposing earth
- Graffiti (decals)
- Hidden spaces behind (horror reveal)

---

## 11. Procedural Terrain Features (Horror-Specific)

### Sinkholes

**Generation:**
1. Define sinkhole center + radius
2. Lower heightmap in circular pattern with steep sides
3. Bowl-shaped depression: `depth * cos(PI/2 * distance/radius)` profile
4. Paint "Dirt" layer on exposed sides, "Mud" at bottom
5. Optional: standing water at bottom, broken fence around perimeter

**PCG scatter:** Warning tape, orange cones (if recent), overgrown brush (if old), exposed pipes/utilities at edges.

### Collapsed Ground / Subsidence

**Generation:**
1. Irregular polygon shape (not circular like sinkhole)
2. Uneven depression -- lower heightmap with noisy offset
3. Cracked ground perimeter (decal ring)
4. Tilted terrain inside collapse zone

**Story elements:** Over abandoned mine, broken sewer, unstable fill, buried structure.

### Excavation Sites

**Generation:**
1. Rectangular depression with sloped ramps for vehicle access
2. Flat bottom (building pad technique at lower elevation)
3. Spoil piles adjacent (raised terrain with "Dirt" layer)
4. Fence perimeter

**PCG scatter:** Construction equipment, porta-potties, caution signs, exposed bones/artifacts (horror).

### Cracked/Heaved Ground

Not heightmap-level -- handle via:
- Displacement in landscape material (crack pattern displacement map)
- Decal overlays for surface cracks
- Mesh replacement for severely damaged sections (heaved slabs)

### Terrain Scars

Long linear depressions from:
- Old railroad tracks (removed)
- Buried utilities (trench subsidence)
- Drag marks (something large was dragged)

Generate as shallow landscape splines with narrow width and minimal falloff.

---

## Architecture: Terrain Pipeline for Procedural Horror Town

### Pipeline Stages (Ordered)

```
Stage 1: BASE TERRAIN
  Input:  Heightmap (external) or procedural noise
  Tool:   ALandscapeProxy::Import() or FLandscapeEditDataInterface::SetHeightData()
  Output: Raw landscape with natural elevation variation + drainage

Stage 2: ROAD CUTS
  Input:  Road network polylines from city layout
  Tool:   Landscape Splines + ULandscapeInfo::ApplySplines()
  Output: Terrain deformed along roads, road layer painted

Stage 3: BUILDING PADS
  Input:  Building footprints + target elevations from spatial registry
  Tool:   ALandscapeBlueprintBrushBase (on dedicated edit layer)
  Output: Flat zones under all buildings with smooth edge blending

Stage 4: PARKING/INFRASTRUCTURE PADS
  Input:  Parking lot, plaza, construction site footprints
  Tool:   Same Blueprint Brush (extend building pad to include infrastructure)
  Output: Additional flat zones

Stage 5: TERRAIN FEATURES
  Input:  Horror feature locations (sinkholes, collapses, excavations)
  Tool:   FLandscapeEditDataInterface::SetHeightData() per feature
  Output: Special terrain deformations

Stage 6: GROUND TEXTURING
  Input:  Zone map (road, yard, sidewalk, dirt, etc.)
  Tool:   FLandscapeEditDataInterface::SetAlphaData() per layer
  Output: All landscape layers painted by zone

Stage 7: RETAINING WALLS
  Input:  Elevation differences at zone boundaries
  Tool:   GeometryScript or modular piece placement
  Output: Wall meshes at grade changes

Stage 8: PCG SCATTER
  Input:  Completed landscape (reads height, slope, layers)
  Tool:   PCG graphs (multiple passes)
  Output: Ground debris, vegetation, puddles, props, furniture

Stage 9: RVT DETAIL
  Input:  Detail placement data (line markings, cracks, stains)
  Tool:   RVT-compatible mesh planes + decals
  Output: High-detail ground texturing beyond landscape resolution
```

### Edit Layer Strategy

| Edit Layer | Priority | Contents |
|-----------|----------|----------|
| Base Terrain | 0 (bottom) | Imported heightmap or procedural base |
| Roads | 1 | Road spline deformations |
| Building Pads | 2 | Flat zones for buildings + parking |
| Horror Features | 3 | Sinkholes, collapses, excavations |
| Detail Sculpt | 4 (top) | Manual artist overrides |

All procedural layers are non-destructive and independently togglable.

---

## Existing Marketplace Tools (Reference)

| Tool | Price | Relevant Features | Limitations |
|------|-------|--------------------|-------------|
| **OmniScape** | Premium | Full city gen, terrain snapping, road splines, building rejection by slope | Closed system, may conflict with Monolith pipeline |
| **Massive World** | Premium | Landscape stamps, PCG biomes, road drawing, non-destructive | Landscape stamp approach, not per-building pad |
| **Fast Nature Maker / PWG** | Premium | Landscape layer-based auto-scatter, inside native terrain editor | Nature focus, not urban/town |
| **Road Creator Pro** | Mid | Road splines, asphalt materials, lane markings | Road-only, no building integration |
| **Procedural Road Markings** | Low | 120 road marking decals with age params | Decals only, not terrain |
| **Puddles** | Low | 72 puddle sticker decals | Decals only |
| **Parking Lot Kit** | Mid | Modules + props + decals for parking areas | Static kit, not procedural |

**Recommendation:** Don't depend on marketplace city generators (conflicts with Monolith pipeline). DO use marketplace decal/material packs for visual quality (Road Markings, Puddles). Build terrain modification pipeline in Monolith.

---

## Proposed MCP Actions

### New Actions for Terrain Pipeline

| Action | Namespace | Description | Priority |
|--------|-----------|-------------|----------|
| `flatten_terrain` | mesh | Flatten landscape under polygon footprint with falloff | P0 |
| `cut_road_terrain` | mesh | Lower terrain along road spline with grade + cross-section | P0 |
| `paint_landscape_layer` | mesh | Paint landscape material layer in polygon region | P0 |
| `create_terrain_feature` | mesh | Create sinkhole/collapse/excavation at location | P1 |
| `generate_retaining_walls` | mesh | Auto-generate retaining walls at grade changes | P1 |
| `scatter_ground_debris` | mesh | PCG-driven debris scatter with terrain awareness | P1 |
| `place_puddles` | mesh | Auto-detect low points, place puddle decals/meshes | P2 |
| `paint_zone_layers` | mesh | Bulk paint all zones (road/yard/sidewalk) from spatial registry | P1 |
| `create_drainage_channel` | mesh | Spline-based drainage channel with terrain deformation | P2 |
| `setup_rvt_ground_detail` | mesh | Configure RVT for ground detail blending | P2 |

### Extended Existing Actions

| Action | Extension |
|--------|-----------|
| `spawn_volume` | Add `terrain_flatten: true` option to auto-flatten under spawned volumes |
| `create_structure` | Add `flatten_terrain: true` option to auto-flatten under buildings |
| `auto_volumes_for_block` | Include terrain preparation as first step |

---

## Performance Considerations

### Landscape Modification Performance

- `SetHeightData()`: ~2-5ms per landscape component (63x63 verts)
- `SetAlphaData()`: ~3-8ms per component per layer (known perf concern, Epic bug report)
- Typical town (2km x 2km, 1m resolution): ~1024 landscape components
- **Full town terrain modification:** 5-15 seconds (acceptable for editor-time generation)
- `ApplySplines()`: ~50-200ms for town road network

### PCG Scatter Performance

- Surface Sampler: GPU-accelerated in 5.7
- 100K debris points: ~200ms to generate, ~50ms to spawn (HISM)
- Landscape cache (`UPCGLandscapeCache`): serializable, avoids re-sampling

### Memory

- Landscape edit layers: each layer adds ~16 bytes/vertex (height) + 1 byte/vertex/layer (weights)
- 5 edit layers on 2017x2017 landscape: ~160MB heightmap + ~40MB weights
- PCG landscape cache: proportional to sampling density

---

## Implementation Estimate

| Phase | Scope | Hours |
|-------|-------|-------|
| Phase 1: Core terrain APIs | `flatten_terrain`, `paint_landscape_layer`, edit layer setup | 20-28h |
| Phase 2: Road + infrastructure | `cut_road_terrain`, parking lot flattening, spline integration | 16-22h |
| Phase 3: Ground texturing | Zone painting, `paint_zone_layers`, edge blending | 14-18h |
| Phase 4: PCG scatter | Debris, puddles, yard generation PCG graphs | 18-24h |
| Phase 5: Horror features | Sinkholes, collapses, retaining walls, drainage | 16-22h |
| Phase 6: RVT detail | Ground detail pipeline, line markings, decal scatter | 10-14h |
| **Total** | | **94-128h** |

### Dependencies

- Spatial registry (for zone definitions) -- `reference_spatial_registry_research.md`
- City block layout (for road/lot geometry) -- `reference_city_block_layout.md`
- Terrain-adaptive buildings (for building elevation targets) -- `reference_terrain_adaptive_buildings.md`
- Auto-collision fix (for debris scatter) -- `reference_auto_collision_research.md`

---

## Key Technical Risks

1. **Edit layer stacking order** -- procedural edit layers must compose correctly with manual edits. Mitigation: dedicated layer indices, clear priority scheme.

2. **SetAlphaData performance** -- known to be slow at high resolution. Mitigation: batch operations, only modify changed components, background thread possible.

3. **Landscape spline precision** -- spline-based road cuts may not align perfectly with procedural road geometry. Mitigation: use road centerline directly as spline input, verify alignment.

4. **PCG cannot write terrain** -- confirmed limitation. All terrain mods must go through landscape APIs first, then PCG reads results. Not a risk per se, but a hard architectural constraint.

5. **World Partition interaction** -- landscape components load/unload with WP. Terrain mods must target loaded components. Mitigation: process terrain per-partition-cell with loading gates.

---

## Sources

- [Landscape Edit Layers in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-edit-layers-in-unreal-engine)
- [FLandscapeEditDataInterface API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Landscape/FLandscapeEditDataInterface)
- [Landscape Splines in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-splines-in-unreal-engine)
- [Landscape Blueprint Brushes in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-blueprint-brushes-in-unreal-engine)
- [Runtime Virtual Texturing in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/runtime-virtual-texturing-in-unreal-engine)
- [Landscape Flatten Tool](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-flatten-tool-in-unreal-engine)
- [PCG Surface Sampler Basics (March 2026)](https://medium.com/@sarah.hyperdense/pcg-basics-your-first-procedural-scatter-system-in-ue5-fab626e1d6f0)
- [PCG and Landscape Materials Interactions](https://dev.epicgames.com/community/learning/tutorials/qX6K/unreal-engine-5-4-pcg-and-landscape-materials-interactions)
- [Horror Environment with PCG and Landmass](https://dev.epicgames.com/community/learning/tutorials/DlzR/how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass)
- [Procedural Parking Lot Generator in UE5](https://80.lv/articles/procedural-parking-lot-generator-in-unreal-engine-5)
- [Landscape Physical Materials for PCG](https://dev.epicgames.com/community/learning/tutorials/EPoW/unreal-engine-how-to-use-landscape-physical-materials-to-control-pcg)
- [Realistic Water Puddles UE 5.5](https://80.lv/articles/master-nanite-displacement-for-dynamic-water-puddles-in-unreal-engine-5-5)
- [PCG and Water System UE 5.4](https://dev.epicgames.com/community/learning/tutorials/XaRe/unreal-engine-5-4-working-with-pcg-and-water-system)
- [UE5 Landscape Creation with PCG](https://dev.epicgames.com/community/learning/tutorials/m38k/unreal-engine-fab-landscape-creation-in-ue5-4-procedural-content-generation-framework-pcg)
- [OmniScape Procedural City Generator](https://www.fab.com/listings/89685028-7a1e-4859-8fdf-47f9dca8e0be)
- [Massive World PCG Tool](https://www.unrealengine.com/marketplace/en-US/product/massive-world-procedural-generation-with-pcg)
- [A Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [Procedural Road Markings Pack](https://www.unrealengine.com/marketplace/en-US/product/procedural-road-markings)
- [Using PCG with GPU Processing UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)
- [Landscape Materials in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/landscape-materials-in-unreal-engine)
- Engine source: `LandscapeEdit.h`, `LandscapeBlueprintBrushBase.h`, `LandscapeEditLayer.h`, `LandscapeSplineRaster.cpp`, `PCGLandscapeData.h`, `PCGLandscapeCache.h`, `PCGSurfaceSampler.h`
