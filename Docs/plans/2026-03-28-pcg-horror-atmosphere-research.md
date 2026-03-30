# PCG Horror Atmosphere -- Debris, Gore, Lighting, Audio, Environmental Storytelling

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready)
**Context:** How to use PCG to dress procedural buildings with horror atmosphere -- decay, abandonment, violence, dread
**Depends on:** PCG Framework Research (`2026-03-28-pcg-framework-research.md`), Spatial Registry, Auto Volumes

---

## Table of Contents

1. [Existing Monolith Foundation](#1-existing-monolith-foundation)
2. [PCG Architecture for Horror Atmosphere](#2-pcg-architecture-for-horror-atmosphere)
3. [Debris Scatter](#3-debris-scatter)
4. [Blood/Gore Placement](#4-bloodgore-placement)
5. [Damage Decals](#5-damage-decals)
6. [Lighting Atmosphere](#6-lighting-atmosphere)
7. [Audio Zones](#7-audio-zones)
8. [Fog/Particle Volumes](#8-fogparticle-volumes)
9. [Environmental Storytelling](#9-environmental-storytelling)
10. [Graffiti/Messages](#10-graffitimessages)
11. [Cobwebs](#11-cobwebs)
12. [Broken Windows](#12-broken-windows)
13. [Decay Progression Parameter](#13-decay-progression-parameter)
14. [Time-of-Day Integration](#14-time-of-day-integration)
15. [Tension Curve Integration](#15-tension-curve-integration)
16. [Industry Case Studies](#16-industry-case-studies)
17. [PCG Node Patterns for Horror](#17-pcg-node-patterns-for-horror)
18. [Performance Considerations](#18-performance-considerations)
19. [Hospice Accessibility](#19-hospice-accessibility)
20. [Proposed MCP Actions](#20-proposed-mcp-actions)
21. [Implementation Plan](#21-implementation-plan)
22. [Sources](#22-sources)

---

## 1. Existing Monolith Foundation

Before designing PCG atmosphere generation, inventory what already exists:

### Already Implemented (Non-PCG, MCP-driven)

**Decal System (Phase 10 -- 4 actions):**
- `place_decals` -- Surface-aligned decal spawning with Poisson-disk scatter in regions
- `place_along_path` -- Catmull-Rom interpolated paths with built-in patterns: `blood_drips`, `footprints`, `drag_marks`
- `analyze_prop_density` -- Density analysis for placed props
- `place_storytelling_scene` -- Parameterized horror vignettes with intensity 0-1:
  - `violence` -- Radial blood splatter from central impact
  - `abandoned_in_haste` -- Scattered personal items, scuff marks, spills
  - `dragged` -- Linear drag trail with blood smears, handprints, scratches
  - `medical_emergency` -- Triage scene with fluid pools, boot prints
  - `corruption` -- Organic growth spreading from focal point (floor + wall elements)

**Context-Aware Props (Phase 18 -- 8 actions):**
- `scatter_on_surface` -- Props ON surfaces (shelf tops, tables) with Poisson disk
- `scatter_on_walls` -- Horizontal traces to find walls, place aligned props
- `scatter_on_ceiling` -- Upward traces for hanging props
- `set_room_disturbance` -- Progressive disorder transforms (0-1 intensity)
- `configure_physics_props` / `settle_props` -- Physics sim for realistic placement
- `create_prop_kit` / `place_prop_kit` -- Reusable prop kit JSON

**Horror Analysis (Phase 6 -- 8 actions):**
- `classify_zone_tension` -- Tension level classification per zone
- `analyze_pacing_curve` -- Encounter spacing and intensity curves
- `find_dark_corners` -- Contiguous dark regions (lighting Phase 7)
- `find_hiding_spots` / `find_ambush_points` / `analyze_sightlines`
- `find_dead_ends` / `analyze_choke_points` / `analyze_escape_routes`

**Horror Design (Phase 15 -- 4 actions):**
- `predict_player_paths` -- Navmesh path prediction
- `evaluate_spawn_point` -- Composite spawn location scoring
- `suggest_scare_positions` -- Optimal scare event positioning
- `evaluate_encounter_pacing` -- Encounter spacing analysis

**Encounter Design (Phase 21 -- 8 actions):**
- `design_encounter` -- Capstone encounter composition
- `evaluate_safe_room` -- Safe room scoring
- `analyze_level_pacing_structure` -- Macro tension mapping
- `generate_scare_sequence` -- Procedural scare event sequences
- `validate_horror_intensity` -- Hospice mode intensity caps
- `generate_hospice_report` -- Full accessibility audit

**Lighting (Phase 7 -- 5 actions):**
- `sample_light_levels` / `find_dark_corners` / `analyze_light_transitions`
- `get_light_coverage` / `suggest_light_placement` (mood-based)

**Audio (Phase 8 -- 14 actions):**
- Full acoustics: room analysis, sound propagation, stealth maps, quiet paths
- `suggest_audio_volumes` -- Auto-suggest reverb zones
- `create_audio_volume` -- Write audio volumes

**Procedural Horror Props:**
- `create_horror_prop` -- GeometryScript generation: `barricade`, `debris_pile`, `cage`, `coffin`, `gurney`, `broken_wall`, `vent_grate`

### Gap Analysis: What PCG Adds

The existing system is **agent-driven** (AI calls individual MCP actions sequentially). PCG adds:

1. **Batch generation** -- One PCG graph pass places hundreds of props vs. sequential MCP calls
2. **Spatial noise** -- Perlin/Voronoi density control that the current Poisson-disk scatter lacks
3. **Attribute-driven selection** -- Weighted random mesh selection from tables
4. **Surface conformance** -- Built-in surface sampling, normal filtering, slope exclusion
5. **Runtime regeneration** -- PCG can regenerate at runtime (OnDemand/AtRuntime modes)
6. **Hierarchical density** -- PCG's DensityFilter + spatial noise creates natural-looking distribution
7. **Seed-based reproducibility** -- Consistent results per seed, variant exploration

The MCP actions remain the **control layer** (what to dress, how intense, where). PCG becomes the **execution layer** (place N items efficiently with distribution rules).

---

## 2. PCG Architecture for Horror Atmosphere

### Master Decay Parameter

Every horror atmosphere PCG graph accepts a single `decay` float parameter (0.0 = pristine, 1.0 = destroyed). This drives:

| Decay Range | Visual State | Density Multiplier |
|---|---|---|
| 0.0 - 0.1 | Pristine, lived-in | 0.0 (no horror dressing) |
| 0.1 - 0.3 | Neglected | 0.2 - 0.4 |
| 0.3 - 0.5 | Abandoned | 0.4 - 0.7 |
| 0.5 - 0.7 | Damaged | 0.7 - 0.9 |
| 0.7 - 0.9 | Destroyed | 0.9 - 1.0 |
| 0.9 - 1.0 | Corrupted/Supernatural | 1.0 (max) + special elements |

### Layer Architecture

Horror atmosphere is composed of **7 independent PCG layers**, each a separate subgraph that can be enabled/disabled independently:

```
Layer 0: Decay Decals     (wall/floor damage, stains, mold, rust)
Layer 1: Debris Scatter   (broken glass, papers, trash, rubble)
Layer 2: Blood/Gore       (splatters, pools, drag marks, handprints)
Layer 3: Lighting         (flickering lights, broken fixtures, emergency lights, candles)
Layer 4: Audio            (ambient emitters: dripping, creaking, wind, distant sounds)
Layer 5: Atmosphere       (local fog, dust motes, smoke, spore particles)
Layer 6: Storytelling     (composed scenes, graffiti, cobwebs, broken windows, barricades)
```

Each layer reads the master `decay` parameter and a per-layer `intensity_override` (default -1 = use decay). Layers can also read a `tension` float from the spatial registry or pacing system.

### PCG Graph Structure

```
[Input: Room/Building Bounds] --> [Surface Sampler] --> [Density Noise (Perlin)]
    --> [Density Remap (decay param)] --> [Normal Filter]
    --> [Attribute Set (select mesh/decal)] --> [Spawn Actor/Static Mesh]
```

For wall-mounted elements:
```
[Input: Wall Mesh] --> [Mesh Sampler] --> [Filter by Normal (vertical)]
    --> [Density Noise] --> [Spawn Decals/Props]
```

For storytelling scenes:
```
[Input: Room Center Points] --> [Select by Attribute (room type)]
    --> [Subgraph: Scene Template] --> [Spawn Actors]
```

---

## 3. Debris Scatter

### Categories and Mesh Tables

| Category | Example Meshes | Decay Threshold | Surface |
|---|---|---|---|
| Paper/Litter | Newspapers, crumpled paper, folders, envelopes | 0.1+ | Floor |
| Broken Glass | Shards, fragments (various sizes) | 0.3+ | Floor (near windows) |
| Overturned Furniture | Chairs, tables, shelves (tilted) | 0.4+ | Floor |
| Rubble | Concrete chunks, plaster, ceiling tiles | 0.5+ | Floor |
| Scattered Items | Shoes, bags, phones, pill bottles, keys | 0.2+ | Floor/surfaces |
| Construction Debris | Pipes, wires, insulation, rebar | 0.6+ | Floor/ceiling |
| Organic Decay | Leaves, dirt piles, dead plants | 0.3+ | Floor/corners |

### PCG Implementation

**Surface Sampling:** Use `Surface Sampler` on floor meshes, then:
1. `Density Noise` (Perlin, scale 200-500cm) creates natural clustering
2. `Density Remap` maps [0,1] noise to [0, decay_param] -- higher decay = more debris
3. `Density Filter` removes points below threshold
4. `Normal Filter` keeps only upward-facing points (floor surfaces)
5. `Attribute Set` from weighted table selects mesh per point
6. `Transform Randomize` for rotation (full 360 yaw, slight pitch/roll for realistic scatter)
7. `Static Mesh Spawner` with weighted mesh table

**Corner Clustering:** Debris naturally accumulates in corners and against walls:
- Use `Distance` node from wall geometry to bias density higher near edges
- Alternatively: `Bounds Modifier` to increase density at room perimeter
- Wind-blown paper clusters against walls (one-directional noise bias)

**Collision-Aware:** Existing `scatter_props` already does overlap checking. PCG equivalent:
- `Self Pruning` node with min distance
- `World Collision Query` (custom C++ node) to reject overlapping placements
- Or use `Bounds Modifier` with collision volumes as exclusion zones

**Weighted Mesh Selection:** PCG Biome plugins demonstrate `Attribute Set Tables`:
- Each entry: StaticMesh + Weight + min/max scale + rotation constraints
- Weight controls probability (paper weight=5, rubble weight=1 at low decay; reversed at high decay)
- The spawner's built-in weighted selection handles this natively

### Performance Notes
- Debris is static -- bake to HISM for rendering efficiency
- LOD 0 within 10m, LOD 1 at 10-30m, culled beyond 30m (interior only)
- Target: 50-200 debris items per room depending on size and decay

---

## 4. Blood/Gore Placement

### Rating-Gated System

**Critical for hospice accessibility.** Blood/gore must be controllable independently:

| Rating Level | Content | Config Key |
|---|---|---|
| 0 (None) | No blood at all | `gore_level=0` |
| 1 (Minimal) | Dark stains only (could be oil/water) | `gore_level=1` |
| 2 (Moderate) | Red blood decals, small pools | `gore_level=2` |
| 3 (Full) | Splatters, drag marks, handprints, large pools | `gore_level=3` |

The PCG graph reads `gore_level` as a parameter and filters the blood layer entirely at level 0, or selects from different mesh/decal tables per level.

### Placement Strategies

**Floor Blood (Decals):**
- Poisson-disk distribution within PCG, density driven by `decay * gore_level_normalized`
- Larger pools near storytelling scene centers (violence pattern)
- Small drips as connecting elements between larger pools
- Aligned to floor normals (trace down from sampled points)

**Wall Blood (Decals):**
- `Mesh Sampler` on wall geometry, filter by vertical normal
- Handprints: placed 90-130cm height (human reach while crouching/stumbling)
- Splatter: placed 50-200cm height, biased toward impact direction
- Smear marks: elongated decals with downward orientation (gravity-pulled)

**Drag Trails:**
- Not suited for PCG scatter -- better as spline-based placement
- Use existing `place_along_path` with `blood_drips` pattern
- PCG can generate the path endpoints; MCP places the trail

**Blood Pool Physics:**
- Blood pools follow floor slope (decal orientation from surface normal)
- Accumulate at low points (height-based density increase)
- Edge drips where floor meets wall (transition decals)

### Render Target Approach (Let Them Come: Onslaught Pattern)

For dense blood (runtime or extreme horror zones), consider the "Splat Manager" approach:
- Single persistent Blueprint actor with a global Niagara system
- Blood areas written to a **global render target mask** via HLSL
- 256-entry ring buffer for blood positions/radii/material types
- Advantages: unlimited blood without individual decal actors, no draw call increase
- UE 5.3+ Niagara Data Channels enable efficient event routing
- **Recommendation:** Evaluate for runtime gore (Phase 3 PCG integration). For editor-time dressing, individual decals are fine and give more art control.

---

## 5. Damage Decals

### Types and Placement Rules

| Decal Type | Surface | Normal Filter | Decay Threshold | Scale Range |
|---|---|---|---|---|
| Bullet holes | Walls, floors | Any | 0.4+ | 5-15cm |
| Cracks (structural) | Walls, ceilings | Vertical/Up | 0.3+ | 30-200cm |
| Water stains | Ceilings, upper walls | Up, Vertical (upper) | 0.2+ | 50-300cm |
| Mold/mildew | Corners, near water | Any (bias corners) | 0.3+ | 20-100cm |
| Rust streaks | Metal surfaces, near pipes | Vertical | 0.2+ | 10-80cm |
| Peeling paint | Walls | Vertical | 0.3+ | 40-200cm |
| Scorch marks | Walls, floors | Any | 0.5+ | 30-150cm |
| Dirt/grime | Lower walls, floors | Any | 0.1+ | 50-300cm |

### PCG Normal-Based Filtering

The key technique for proper decal placement is **surface normal filtering**:

```
[Mesh Sampler (building mesh)] --> [Filter by Normal]
    Wall decals:   dot(normal, up) < 0.3  (vertical surfaces)
    Floor decals:  dot(normal, up) > 0.7  (horizontal surfaces)
    Ceiling decals: dot(normal, up) < -0.7  (inverted horizontal)
```

**Height-based distribution within walls:**
- Water stains: upper 20% of wall height (gravity flow from roof leaks)
- Mold: lower 30% of wall height OR near water stain drip endpoints
- Dirt/grime: lower 50cm gradient (splash zone from foot traffic)
- Bullet holes: 80-180cm height (firefight height)
- Cracks: random, but clustered (Voronoi noise groups cracks into patterns)

### Corner and Edge Detection

Damage concentrates at architectural features:
- **Corners:** Use `Distance` from edge/corner geometry to increase density
- **Near windows:** Radius-based density boost around window openings (weather damage)
- **Near pipes/fixtures:** Rust streaks below mounted objects
- PCG can compute these using `Bounds Modifier` or custom attribute from spatial data

### PCG Geometry Decals (Marketplace Plugin Pattern)

The "PCG Geometry Decals" system demonstrates an alternative approach:
- Stamps **geometry** (not projected decals) directly onto surfaces
- Conforms to terrain via heightmap displacement
- Bakes to Nanite static meshes for runtime
- Better for large-scale damage (crumbling walls, large cracks) where projected decals lose resolution
- **Assessment:** Overkill for interior wall decals. Useful for exterior facade damage where geometry sells better than projection. Consider for Phase 2 exterior dressing.

---

## 6. Lighting Atmosphere

### Horror Lighting Elements

| Element | UE5 Actor/Component | PCG Spawn Method | Decay Threshold |
|---|---|---|---|
| Flickering fluorescent | PointLight + BP_FlickeringLight | SpawnActor (Blueprint) | 0.3+ |
| Broken fixture (dark) | StaticMesh only (no light) | StaticMeshSpawner | 0.4+ |
| Emergency red light | PointLight (red, low intensity) | SpawnActor | 0.5+ |
| Candles | PointLight + flame Niagara | SpawnActor | 0.4+ |
| Fire barrel | PointLight + fire Niagara | SpawnActor (exterior) | 0.5+ |
| Hanging bare bulb | PointLight + SM_Bulb + cable | SpawnActor | 0.3+ |
| Flashlight beam (dropped) | SpotLight + SM_Flashlight | SpawnActor | 0.4+ |
| TV static glow | PointLight (blue flicker) + SM_TV | SpawnActor | 0.3+ |

### Flickering Light Implementation

**Blueprint Actor Pattern:** `BP_FlickeringLight`
- PointLight component with `SetIntensity()` driven by timeline or noise function
- Flicker patterns: regular (fluorescent buzz), random (electrical fault), dying (slow fade + pop)
- Parameters exposed to PCG: flicker_type, base_intensity, flicker_speed, color
- Sound component for electrical buzz (tied to flicker timing)

**Decay-Driven Light Replacement:**
```
decay < 0.3:  All lights working normally (no PCG modification)
decay 0.3-0.5: 30% of lights replaced with flickering variants
decay 0.5-0.7: 60% flickering, 20% dead (mesh only, no light)
decay 0.7-0.9: 20% flickering, 60% dead, 20% replaced with candles/emergency
decay 0.9-1.0: 80% dead, sparse candles/fire barrels, emergency lights only
```

### PCG Integration for Lighting

PCG is excellent for this because it can:
1. **Sample existing light positions** (Input: GetActorsOfClass PointLight) or ceiling mount points
2. **Probabilistically replace** working lights with horror variants based on decay
3. **Add supplemental horror lights** (candles on surfaces, dropped flashlights)
4. Parameters drive the replacement probability curve

**Implementation approach:**
- PCG graph input: room bounds + existing light actor positions
- At each light position, weighted random selects: keep (weight = 1-decay), flicker (weight = decay*0.5), dead (weight = decay*0.3), candle_replacement (weight = decay*0.2)
- Additional pass: scatter candles on flat surfaces (table tops, floor) at decay 0.5+

### Light Color Temperature Shift

As decay increases, shift surviving lights toward horror palette:
- Normal: 4000-5000K (neutral white)
- Decay 0.3+: 3000-3500K (warm/sickly yellow)
- Decay 0.6+: 2500-3000K OR shift toward green-tinted
- Emergency: pure red, 500-1000 lux
- Supernatural (0.9+): desaturated cold blue or unnatural purple

---

## 7. Audio Zones

### Ambient Sound Emitter Categories

| Category | Examples | Placement | Decay Threshold |
|---|---|---|---|
| Water | Dripping, running, pooled splashing | Below stains, near pipes, bathrooms | 0.2+ |
| Structural | Creaking, settling, groaning metal | Ceiling/wall corners, stairwells | 0.3+ |
| Wind | Whistling, howling, drafts | Near windows (esp. broken), vents, doors | 0.2+ |
| Electrical | Buzzing, sparking, humming | Near light fixtures, panels, machinery | 0.3+ |
| Biological | Insects, rats, breathing, heartbeat | Dark corners, vents, walls | 0.4+ |
| Distant | Screams, thuds, sirens, gunshots | Non-directional, low volume, rooms away | 0.5+ |
| Environmental | Rain on roof, thunder, wind against windows | Exterior-facing walls, roofs | 0.0+ (weather) |

### PCG Placement Strategy

**AmbientSound actors** in UE5 have:
- `USoundBase*` (Sound Cue or MetaSound)
- Spatialization settings (3D positioning)
- Attenuation settings (falloff shape/distance)
- Auto-activate on spawn

**PCG spawns Blueprint wrapper actors** (`BP_AmbientHorrorSound`) containing:
- AmbientSound component
- Attenuation preset (small room / large room / hallway)
- Sound Cue with randomized variations
- Volume driven by decay parameter
- Optional trigger volume (only plays when player is near)

**Placement rules:**
- Water drips: spawn below `water_stain` decal positions (cross-layer reference)
- Wind: spawn at broken window positions + vent locations
- Creaking: spawn at ceiling corners, bias toward large rooms
- Electrical: co-locate with flickering lights
- Distant sounds: spawn in adjacent empty rooms, NOT in player's room (spatial separation sells distance)

**Density control:**
- Max 3-5 ambient emitters per room (more causes muddiness)
- PCG `Self Pruning` with large min distance (500-800cm) prevents clustering
- Attenuation radius should not overlap significantly (check with audio volume analysis)

### Integration with Existing Audio Actions

The 14 audio actions already handle room acoustics, reverb zones, and AI hearing. PCG ambient emitters complement by:
- Populating the reverb zones with actual sound sources
- Using `analyze_room_acoustics` output to set appropriate attenuation
- `can_ai_hear_from` validates whether placed sounds would alert AI (intentional design tool)

---

## 8. Fog/Particle Volumes

### Types Available in UE 5.7

**Local Fog Volumes** (since UE 5.5):
- `ALocalFogVolume` actor -- box-shaped localized fog
- Properties: Fog Density, Fog Albedo (color), Fog Emissive, Fog Phase (scattering), Fog Height Falloff
- **Does NOT require Exponential Height Fog** (independent system)
- Can be placed per-room for isolated atmosphere

**Niagara Particle Systems:**
- Dust motes: GPU particles, 100-500 per room, slow drift, lit by scene lights
- Smoke wisps: Low-density particles with world-space turbulence noise
- Spore clouds: Larger particles, slow movement, glow with emissive
- Dripping water: Per-drip emitters below water stains
- Sparks: Near damaged electrical equipment, burst on timer

### PCG Fog Implementation

| Fog Type | Actor | Shape | Density | Decay Threshold |
|---|---|---|---|---|
| Light haze | LocalFogVolume | Room-sized box | 0.05-0.15 | 0.2+ |
| Dense fog bank | LocalFogVolume | Hallway-length box | 0.2-0.5 | 0.5+ |
| Floor fog | LocalFogVolume | Flat wide box (30cm tall) | 0.3-0.6 | 0.4+ |
| Smoke wisps | Niagara (BP_SmokeWisp) | Emitter volume | Particle count | 0.5+ |
| Dust motes | Niagara (BP_DustMotes) | Room-sized | 50-200 particles | 0.1+ |
| Spore cloud | Niagara (BP_SporeCloud) | Localized cluster | 20-50 particles | 0.7+ |

**PCG spawns these as Blueprint actors:**
```
[Room Center Points] --> [Attribute: room_size, decay]
    --> [Filter: decay > threshold] --> [Weighted Select: fog type]
    --> [SpawnActor: BP_LocalFog_Horror]
```

**BP_LocalFog_Horror** reads PCG attributes on spawn:
- `room_width`, `room_depth` set fog volume extents
- `decay` sets fog density (linear remap)
- `fog_type` selects color/behavior preset

### Niagara + PCG Integration

Community tutorials confirm PCG can spawn Niagara systems at point locations. The pattern:
1. PCG generates points (surface sample, density filter)
2. Each point stores attributes (particle type, intensity)
3. `SpawnActor` node spawns a Blueprint containing a NiagaraComponent
4. Blueprint's Construction Script reads PCG attributes to configure the Niagara system

**For dust motes specifically:** A single room-scale Niagara system is more efficient than multiple small emitters. Spawn one `BP_DustMotes` per room, sized to room bounds.

---

## 9. Environmental Storytelling

### Existing System: `place_storytelling_scene`

Already implements 5 parameterized scene patterns (violence, abandoned_in_haste, dragged, medical_emergency, corruption). These are **composed scenes** -- specific arrangements of decals/props that tell a micro-narrative.

### PCG Enhancement: Procedural Scene Selection and Placement

PCG adds the ability to **automatically place storytelling scenes** throughout a building based on room type, decay level, and tension:

**Scene-Room Compatibility Matrix:**

| Scene Pattern | Suitable Room Types | Decay Range | Tension Boost |
|---|---|---|---|
| violence | Hallway, office, bedroom, bathroom | 0.5+ | +0.3 |
| abandoned_in_haste | Office, kitchen, living room, lobby | 0.2+ | +0.1 |
| dragged | Hallway, stairwell, corridor | 0.6+ | +0.4 |
| medical_emergency | Bathroom, clinic room, kitchen | 0.4+ | +0.2 |
| corruption | Basement, utility, bathroom, storage | 0.7+ | +0.5 |

**NEW Scene Patterns to Add:**

| Pattern | Description | Elements |
|---|---|---|
| `last_stand` | Barricaded position with ammo/supplies | Overturned furniture, scattered shells, blood behind barricade |
| `ritual` | Occult/cult activity | Candles in circle, symbols on floor (decals), dark stains |
| `quarantine` | Medical isolation attempt | Plastic sheets (mesh), tape (decals), biohazard signs, body outline |
| `surveillance` | Someone was watching/collecting info | Papers on walls, photos, string connecting points, desk with notes |
| `escape_attempt` | Failed exit | Broken window + glass shards + blood trail leading to window + body outline below |
| `nest` | Something lives here | Organic matter, bones, torn fabric, scratch marks on walls |
| `power_failure` | Electrical disaster | Scorch marks, melted cables, emergency lights, sparking emitter |
| `flood_damage` | Water intrusion | Water stain high on walls, warped floor (slight mesh deformation), mold, pooled water |

### PCG Storytelling Graph

```
[Room Data (from spatial registry)] --> [Filter: has_storytelling_scene = false]
    --> [Probability Gate: decay * 0.4]   // Not every room gets a scene
    --> [Attribute Select: room_type --> compatible_scenes]
    --> [Weighted Random: select scene]
    --> [Subgraph: Execute scene pattern at room center]
```

**Key principle:** Not every room should have a storytelling scene. At decay 0.5, roughly 20% of rooms get one. At decay 1.0, roughly 40%. Empty rooms between scene rooms create pacing contrast.

---

## 10. Graffiti/Messages

### Types

| Type | Content | Surface | Height |
|---|---|---|---|
| Warning graffiti | "DON'T GO", "THEY'RE INSIDE", "RUN" | Walls | 100-180cm |
| Directional arrows | Arrow decals pointing toward/away from danger | Walls, floors | 80-150cm |
| Tally marks | Counting days/kills | Walls (near beds, cells) | 100-150cm |
| Gang/territory marks | Symbols, tags | Exterior walls, doors | 100-200cm |
| Official signage (damaged) | Biohazard, exit, floor numbers | Walls, doors | 150-200cm |
| Personal messages | Names, dates, "I was here" | Walls, floors, furniture | Variable |
| Occult symbols | Pentagrams, sigils, unknown scripts | Walls, floors, ceilings | Variable |

### Placement via PCG

**Wall detection:** PCG `Mesh Sampler` with normal filter (vertical surfaces) provides valid wall points. Additional filtering:
- Height range: 80-200cm above floor (human arm reach)
- Exclude window/door openings (check against spatial registry opening positions)
- Bias toward visible approach sides (face the direction players enter from)

**Decal Designer approach:** The marketplace "Decal Designer" plugin demonstrates "Volume Scatter" that can grime up entire rooms by sampling scene geometry and projecting decals with contextual detail (curvatures, edges). Similar PCG approach:
1. Sample wall surfaces
2. Filter by normal + height
3. Apply density noise (graffiti clusters, not uniform)
4. Spawn `ADecalActor` with randomly selected graffiti material
5. Scale by surface available area (no text hanging off edges)

**Readable messages:** For messages that need to be legible, place at eye-height (140-170cm), on flat wall sections (no corners), with adequate light level (cross-reference lighting data). These should be manually placed or at minimum, MCP-curated rather than pure PCG scatter.

---

## 11. Cobwebs

### The Cobweb Problem

Cobwebs are geometrically tricky because they **span between surfaces** -- they require detecting two nearby anchor points (corners, edges, objects) and stretching geometry between them.

### Approaches

**Approach A: Decal-Based (Recommended for Phase 1)**
- Flat cobweb decals projected into corners
- PCG: Sample corner points (where wall meets wall, wall meets ceiling)
- Normal filter: select points where two surfaces meet at ~90 degrees
- Spawn decal with appropriate orientation (spanning the corner)
- Limitation: looks flat, no 3D depth

**Approach B: Static Mesh Library**
- Pre-made cobweb meshes for common configurations:
  - Corner cobweb (90-degree wall-wall)
  - Ceiling-corner cobweb (wall-ceiling junction)
  - Span cobweb (between two parallel walls < 200cm apart)
  - Object-to-wall cobweb (furniture to adjacent wall)
- PCG selects appropriate mesh based on local geometry analysis
- Better 3D appearance, limited to pre-made configurations

**Approach C: Procedural Cobweb Generation (Houdini/Blender Pattern)**
- Houdini and Blender both have procedural cobweb generators that:
  1. Cast rays from anchor point in multiple directions
  2. Find nearby surfaces
  3. Generate spline strands between anchor and hit points
  4. Add cross-strands (catenary curves)
  5. Generate mesh from strands
- Would require GeometryScript implementation
- Too complex for initial implementation; consider for Phase 3

**Recommended:** Approach B (static mesh library) with PCG placement. Corner detection via:
1. Room data from spatial registry (wall corners are known geometry)
2. PCG samples at corner positions
3. Select from 4-5 cobweb mesh variants
4. Random rotation/scale for variety
5. Decay threshold: 0.3+ (cobwebs accumulate in neglected spaces)

### Density
- 2-6 cobwebs per room (corners + ceiling junctions)
- More in low-traffic rooms (storage, attic, basement)
- Less in high-traffic rooms (hallways, kitchens)

---

## 12. Broken Windows

### Component Elements

A "broken window" is a **composed multi-element feature**:
1. **Glass shards** on the ground below the window (small static meshes, scatter)
2. **Broken glass in frame** (modified window mesh or decal overlay)
3. **Boarding planks** (1-3 wooden planks across the opening)
4. **Draft wind** (audio emitter + slight particle effect)
5. **Light ingress** (if exterior-facing, additional light bleeding in)

### PCG Implementation

**Per-window decision tree:**
```
For each window opening (from spatial registry):
    if decay < 0.3: intact (no modification)
    if decay 0.3-0.5: 20% chance cracked (decal overlay only)
    if decay 0.5-0.7: 40% cracked, 20% broken (shards + partial glass)
    if decay 0.7-0.9: 20% cracked, 50% broken, 20% boarded
    if decay 0.9-1.0: 10% broken, 60% boarded, 30% fully missing
```

**Glass shard scatter:** Below broken windows, Poisson-disk scatter of glass shard meshes:
- 5-15 shards per broken window
- Spread radius: 30-80cm from window base
- Biased toward interior side (broken inward implies external force -- horror)
- Small shards (2-5cm) + medium (5-10cm) + occasional large piece (10-20cm)

**Boarding planks:** 1-3 wooden plank meshes placed across window opening:
- Horizontal or diagonal orientation
- Gaps between planks (player can see through but not pass)
- Nails/screws at endpoints (small detail meshes)
- Planks from INSIDE (someone tried to keep something out -- more terrifying)

**Wind/Draft emitter:** `BP_WindowDraft` spawned at broken window:
- Directional wind particle effect (dust/leaves blowing in)
- Wind ambient sound with attenuation
- Only active on exterior-facing windows

### Integration with Facade System

The existing facade generation (`reference_facade_window_research.md`) already defines window positions. The broken window system operates as a **post-processing pass**:
1. Read window positions from spatial registry or facade data
2. For each window, roll the damage probability based on decay
3. Spawn appropriate damage elements (decals, shards, boarding, audio)

---

## 13. Decay Progression Parameter

### The Master `decay` Parameter (0.0 - 1.0)

This is the single most important design decision. One float controls the entire atmosphere. Crucially, it should be **spatially varying** -- different parts of the same building can have different decay levels.

### Spatial Decay Map

**Per-room decay:** Each room in the spatial registry stores a `decay` float. The building-level `base_decay` is modulated per room:

```
room_decay = clamp(base_decay + room_decay_offset + noise(room_position), 0, 1)
```

**Room type modifiers:**
| Room Type | Decay Modifier | Reason |
|---|---|---|
| Bathroom | +0.15 | Water damage accelerates decay |
| Basement | +0.2 | Below-grade = moisture, neglect |
| Kitchen | +0.1 | Organic matter, appliance failure |
| Attic | +0.15 | Roof leaks, insulation damage |
| Hallway | -0.05 | Slightly better maintained (traffic) |
| Lobby/entrance | -0.1 | First maintained, last to decay |
| Utility/boiler | +0.1 | Industrial wear, no cosmetic care |

**Perlin noise modulation:** Add world-space Perlin noise (scale 500-1000cm) to create organic variation. This prevents the entire floor from feeling uniformly decayed.

### Decay Layers vs. Single Parameter

While `decay` is the master control, specific sub-systems can have independent overrides:
- `gore_level` (0-3): Independent from decay -- a pristine building can have a fresh murder scene
- `supernatural_level` (0-1): Corruption/organic elements independent of physical decay
- `time_since_event` (0-1): How old are the horror elements? Fresh blood vs. dried, new bodies vs. skeletons

### PCG Parameter Pipeline

```
Building base_decay (0.7)
    |
    +-- Room modifiers (bathroom: +0.15 = 0.85, lobby: -0.1 = 0.6)
    |
    +-- Perlin noise per room (+/- 0.1)
    |
    +-- Per-layer override (gore_level=0 disables Layer 2 entirely)
    |
    +-- Per-layer PCG graph reads final decay value
    |
    +-- Density Remap maps decay to spawn density curve
```

---

## 14. Time-of-Day Integration

### PCG and Lighting Conditions

PCG graphs can adjust based on time-of-day, but this primarily affects **what elements are visible/active**, not placement:

| Time | Atmosphere Adjustment |
|---|---|
| Day (bright) | Fog less visible, debris clearly seen, horror subtle |
| Dusk/Dawn | Golden/orange light through windows, long shadows, fog catches light |
| Night | Darkness amplifies everything, candles/emergency lights become primary |
| Overcast | Flat lighting reduces contrast, mold/stains more visible |

### Implementation

**Approach: Conditional activation, not re-generation.**

- PCG places ALL atmosphere elements at author-time
- Blueprint actors respond to time-of-day parameter at runtime:
  - `BP_FlickeringLight`: Adjusts base intensity (brighter at night for contrast)
  - `BP_LocalFog_Horror`: Adjusts density (denser at night)
  - `BP_DustMotes`: Adjusts particle count (more visible in light shafts)
  - `BP_WindowDraft`: Toggles light shaft Niagara emitter based on exterior illumination

**PCG does NOT regenerate per time-of-day.** That would be wasteful. Instead, placed actors have runtime-responsive behavior driven by a global `TimeOfDay` MPC (Material Parameter Collection) or a shared `BP_HorrorAtmosphereManager` actor.

### MPC-Driven Materials

Decal materials can sample an MPC `TimeOfDay` value to:
- Adjust emissive (blood glistens in flashlight, dull in daylight)
- Shift color temperature (stains look different under warm vs. cool light)
- Control wetness/specular (fresh blood is glossy, dried blood is matte -- but this is `time_since_event`, not time-of-day)

---

## 15. Tension Curve Integration

### Connecting Pacing to Atmosphere Density

The existing `analyze_pacing_curve` and `classify_zone_tension` actions output per-zone tension values. PCG should read these to modulate horror dressing density.

### Tension-Atmosphere Mapping

| Tension Level | Atmosphere Behavior | Example |
|---|---|---|
| 0.0 (Safe Room) | Minimal dressing, warm lighting, clean | Player rest area |
| 0.2 (Low) | Light debris, working lights, subtle stains | Transition areas |
| 0.4 (Rising) | Moderate debris, some flickering, scattered papers | Approaching danger |
| 0.6 (High) | Dense dressing, multiple dead lights, blood visible | Pre-encounter |
| 0.8 (Peak) | Maximum density, fog, darkness, audio intensity | Active encounter zone |
| 1.0 (Climax) | Supernatural elements, corruption, extreme atmosphere | Boss/set-piece area |

### Implementation

**Per-room tension tag:** The spatial registry stores a `tension` float per room (from `analyze_level_pacing_structure`). PCG reads this alongside `decay`:

```
effective_density = base_decay * lerp(0.3, 1.0, tension)
```

At tension 0.0 (safe room), density is 30% of what decay alone would suggest. At tension 1.0, full decay density applies.

**Safe Room Exemption:** Rooms flagged as safe rooms by `evaluate_safe_room` get special treatment:
- Layer 2 (blood/gore): completely disabled
- Layer 3 (lighting): all lights working, warm color
- Layer 4 (audio): only ambient/peaceful sounds (rain, distant music)
- Layer 5 (fog): minimal or none
- Layer 6 (storytelling): no horror scenes
- Layer 0-1: light debris only (some decay is atmospheric even in safe spaces)

### Gradient Transitions

Horror dressing should not have hard boundaries. Between a safe room and a danger zone:
- Use PCG's spatial noise to create **gradient transitions**
- Density increases gradually over 5-10 meters
- The player subconsciously registers the environment getting "worse"

This is the Left 4 Dead AI Director principle applied to environment: "the environment itself paces the player through visual density."

---

## 16. Industry Case Studies

### Left 4 Dead -- AI Director + Environment

The L4D Director controls environmental pacing through:
- **Wander density per area:** Each map area has a randomized wanderer count based on length and desired density
- **Mob intervals:** 90-180 seconds on Normal, mob size grows over time
- **Intensity tracking:** Director maintains emotional intensity (0-1) and creates relief periods after peaks
- **Key lesson:** The Director controls *frequency* (pacing), not *difficulty* (amplitude)

For Leviathan's environmental dressing, the equivalent is:
- The **tension curve** controls *density* of horror elements (frequency)
- The **decay parameter** controls *severity* of each element (amplitude)
- These are independent axes, enabling a low-decay-high-tension room (clean but terrifying) or high-decay-low-tension room (ruined but safe)

### Alien: Isolation -- Environmental Detail Density

Sevastopol Station's art direction uses environmental density to communicate danger:
- Working areas: organized, lit, labeled -- functional
- Compromised areas: debris, flickering lights, damaged systems -- danger
- Hive areas: organic corruption, darkness, alien materials -- extreme danger
- The environment is a **continuous gradient** from order to chaos
- Environmental audio changes with area state (machinery vs. silence vs. organic sounds)

### Dead Space -- Integrated Horror Environment

Dead Space Remake's environmental storytelling:
- Opening level: ground "littered with trash and covered in dirt, with debris scattered around"
- Lighting shifts from working systems to emergency red to complete darkness
- Audio: ship sounds degrade from normal operation to alarming mechanical failures
- Blood and bodies placed to create breadcrumb trails guiding the player
- Environmental damage escalates along the critical path

### Silent Hill 2 -- Otherworld Transition

The "Otherworld" represents decay as a spatial mechanic:
- Normal world: neglected but recognizable (decay 0.3)
- Transition: environments gradually corrupt (decay 0.3 -> 0.8 over distance)
- Otherworld: extreme corruption, rust, organic matter, darkness (decay 1.0)
- **Key technique:** The transition IS the horror -- watching the environment change

### Shadows of Doubt (Procedural City)

Procedural building generation with atmosphere:
- Each building has a "cleanliness" parameter similar to our decay
- Interior dressing is procedurally placed based on building type + cleanliness
- Crime scenes are procedural storytelling (victim position, evidence scatter, blood patterns)
- Demonstrates that PCG CAN create convincing atmospheric environments

---

## 17. PCG Node Patterns for Horror

### Pattern 1: Basic Floor Scatter (Debris/Glass/Papers)

```
Surface Sampler (floor mesh, PointsPerSqM=0.5)
    --> Density Noise (Perlin, Scale=300)
    --> Density Remap (In:[0,1], Out:[0, ${decay}])
    --> Density Filter (Min=0.3)
    --> Self Pruning (MinDistance=30)
    --> Attribute Set (MeshTable: debris_table)
    --> Transform Randomize (Rotation: Yaw=360, Pitch=5, Roll=5, Scale=0.7-1.3)
    --> Static Mesh Spawner
```

### Pattern 2: Wall Decal Scatter (Damage/Stains/Mold)

```
Mesh Sampler (wall mesh, PointsPerSqM=0.2)
    --> Filter: dot(Normal, Up) < 0.3   // vertical surfaces only
    --> Filter: Height > 10cm, Height < WallHeight-10cm  // not at floor/ceiling joint
    --> Density Noise (Voronoi, Scale=500)  // clustering
    --> Density Remap (In:[0,1], Out:[0, ${decay}])
    --> Density Filter (Min=0.4)
    --> Attribute Set (DecalTable: wall_damage_table)
    --> SpawnActor (BP_HorrorDecal)
```

### Pattern 3: Lighting Replacement

```
GetActorsInVolume (class: PointLight)
    --> For Each:
        --> Random (seed + actor_index)
        --> Branch: random < decay*0.7 ? Replace : Keep
        --> If Replace:
            --> Weighted Select: Flicker(0.4), Dead(0.3), Emergency(0.2), Candle(0.1)
            --> Delete Original, SpawnActor Replacement
```

Note: This is pseudocode. PCG doesn't natively do per-actor replacement. This would be a **custom C++ PCG node** or handled by the MCP action that calls PCG.

### Pattern 4: Audio Placement

```
Room Center Points (from spatial registry)
    --> Filter: decay > 0.2
    --> For Each Room:
        --> Probability: 0.6 * decay  // not every room gets audio
        --> Select Sound Category by room_type + decay
        --> SpawnActor (BP_AmbientHorrorSound)
        --> Set Attributes: volume = decay * 0.7, attenuation = room_size * 1.5
```

### Pattern 5: Fog Volumes

```
Room Center Points
    --> Filter: decay > 0.3
    --> Probability: decay * 0.5
    --> Select Fog Type:
        if ceiling_height > 400: fog_haze
        if is_hallway: floor_fog
        if is_basement: dense_fog
        default: light_haze
    --> SpawnActor (BP_LocalFog_Horror)
    --> Set Extents to room_bounds * 0.9
```

### Pattern 6: Corner Cobwebs

```
Room Corner Points (from spatial registry: wall-wall junctions)
    --> Filter: decay > 0.3
    --> Probability: 0.3 + decay * 0.4  // 30% at low decay, 70% at high
    --> Height Select: floor_level (low cobwebs) OR ceiling_level (high cobwebs)
    --> Weighted Mesh: corner_cobweb_A(3), corner_cobweb_B(2), ceiling_cobweb(1)
    --> Transform: orient to corner angle
    --> Static Mesh Spawner
```

---

## 18. Performance Considerations

### Budget Per Room

| Element Type | Max Count/Room | Draw Calls | Memory/Instance |
|---|---|---|---|
| Debris meshes (HISM) | 50-200 | 1 per mesh type | 48-88 bytes |
| Decals | 10-30 | 1 per decal | ~4KB (projector) |
| Lights (point/spot) | 3-8 | Variable (Lumen) | ~1KB |
| Audio emitters | 2-5 | 0 (audio thread) | ~2KB |
| Fog volumes | 0-2 | 1-2 (volumetric pass) | ~256 bytes |
| Niagara systems | 1-3 | 1 per system | Variable |
| Cobweb meshes | 2-6 | 1 (HISM) | 48-88 bytes |

### Total Budget Target
- **Per room:** ~60-250 additional actors/instances
- **Per building (20 rooms):** ~1,200-5,000 instances
- **Per city block (8 buildings):** ~10,000-40,000 instances

### Optimization Strategies

1. **HISM for static meshes:** All debris, cobwebs, boarding planks as HISM (no per-instance collision)
2. **LOD and distance culling:** Debris LOD 0 < 10m, LOD 1 < 30m, cull > 30m
3. **Decal pooling:** Max 256 active decals per block (ring buffer, fade out distant)
4. **Audio priority:** Max 8 concurrent ambient emitters per block, distance-priority sorted
5. **Fog volume limit:** Max 4 active local fog volumes per block (merge nearby rooms)
6. **PCG generation batching:** Generate atmosphere per building, not per room (fewer PCG executions)
7. **Bake option:** After PCG generates, option to "bake" results (ClearPCGLink) -- actors become standalone, PCG component removed

### PCG Generation Timing
- Editor-time generation: ~0.5-2 seconds per building (acceptable for MCP workflow)
- Runtime generation: ~50-200ms per building on demand (OnDemand mode)
- Consider streaming: generate atmosphere for loaded buildings only (partition-based)

---

## 19. Hospice Accessibility

### Content Sensitivity Controls

Hospice patients require **granular control** over horror content:

| Setting | Default | Options | Effect |
|---|---|---|---|
| `gore_level` | 2 | 0-3 | Controls blood/gore layer entirely |
| `body_visibility` | true | true/false | Hides body outlines, corpse props |
| `supernatural_elements` | true | true/false | Hides corruption, occult elements |
| `jump_scare_intensity` | 0.7 | 0-1 | Scales audio spikes, flickering speed |
| `darkness_minimum` | 0.15 | 0-1 | Minimum ambient light level |
| `fog_density_max` | 0.5 | 0-1 | Caps fog density |
| `audio_scare_volume` | 0.6 | 0-1 | Max volume for horror sound emitters |

### Implementation
- These settings are read as PCG graph parameters
- The PCG graph filters/modifies layers based on accessibility settings
- `validate_horror_intensity` (existing action) can audit PCG output against hospice caps
- `generate_hospice_report` (existing action) provides full accessibility audit

### Safe Room Guarantee
At least one room per floor must be a fully safe room:
- No horror dressing beyond minimal debris
- Working warm lights
- Peaceful ambient audio
- No fog
- Visual indicator (different wall color/clean state) that signals safety

---

## 20. Proposed MCP Actions

### New PCG Atmosphere Actions (7 core + 3 utility = 10 actions)

| Action | Description | Layer |
|---|---|---|
| `pcg_horror_atmosphere` | Master action: generate all horror layers for a building/room. Reads spatial registry, decay, tension. Spawns PCG component, executes graph, returns placed actor counts. | All |
| `pcg_debris_scatter` | Scatter debris meshes on floor surfaces within volume. Weighted mesh table, Poisson disk + Perlin noise density. | 1 |
| `pcg_blood_dressing` | Place blood/gore decals and props. Rating-gated (gore_level). Floor + wall placement. | 2 |
| `pcg_damage_decals` | Place wall/floor/ceiling damage decals (cracks, stains, mold, rust). Normal-filtered. | 0 |
| `pcg_horror_lighting` | Replace/augment lights with horror variants. Flickering, dead, emergency, candles. | 3 |
| `pcg_ambient_audio` | Place ambient horror sound emitters. Category-based, attenuation-aware. | 4 |
| `pcg_atmosphere_fog` | Place local fog volumes and particle emitters (dust, smoke, spores). | 5 |
| `set_room_decay` | Set/get per-room decay value in spatial registry. Batch set for building. | - |
| `set_tension_map` | Set/get per-room tension value. Import from `analyze_level_pacing_structure`. | - |
| `bake_atmosphere` | Freeze PCG output: ClearPCGLink, convert to standalone actors/HISM. | - |

### Integration Actions (extend existing)

| Action | Extension |
|---|---|
| `place_storytelling_scene` | Add 8 new patterns (last_stand, ritual, quarantine, surveillance, escape_attempt, nest, power_failure, flood_damage) |
| `set_room_disturbance` | Accept `decay` as input alongside disturbance level; unify concepts |
| `create_horror_prop` | Add: `cobweb`, `boarding_planks`, `broken_glass_pile`, `candle_cluster`, `warning_sign` |

---

## 21. Implementation Plan

### Phase 1: Core PCG Atmosphere Pipeline (~28-36h)

1. **PCG Graph Library** (~8h)
   - Create base PCG graph assets for each layer (debris, decals, lighting, audio, fog)
   - Expose `decay`, `tension`, `gore_level` as graph parameters
   - Set up weighted mesh/decal tables with horror asset references

2. **`pcg_horror_atmosphere` Master Action** (~10h)
   - Read spatial registry for room data (bounds, type, decay, tension)
   - Create/configure UPCGComponent on building actor
   - Set parameters, trigger generation
   - Return placed actor inventory
   - Handle `bake_atmosphere` to freeze output

3. **`pcg_debris_scatter` + `pcg_damage_decals`** (~6h)
   - Floor surface sampling with decay-driven density
   - Wall mesh sampling with normal filtering
   - Weighted mesh/decal table selection
   - Self-pruning and overlap prevention

4. **`set_room_decay` + `set_tension_map` Utility Actions** (~4-6h)
   - Spatial registry integration for per-room floats
   - Batch set operations (set_building_decay, import_tension_from_pacing)
   - Perlin noise modulation option

### Phase 2: Specialized Layers (~24-32h)

5. **`pcg_horror_lighting`** (~8h)
   - Blueprint actors: BP_FlickeringLight, BP_EmergencyLight, BP_CandleCluster, BP_DeadFixture
   - Light replacement logic (existing lights -> horror variants)
   - Color temperature shift by decay
   - Integration with existing `suggest_light_placement`

6. **`pcg_ambient_audio`** (~6h)
   - Blueprint actors: BP_AmbientHorrorSound with sound cue library
   - Room-type -> sound category mapping
   - Attenuation sizing from room bounds
   - Max emitter density limiting

7. **`pcg_atmosphere_fog`** (~6h)
   - Blueprint actors: BP_LocalFog_Horror, BP_DustMotes, BP_SmokeWisp
   - Room-size -> fog volume sizing
   - Decay -> density mapping
   - Time-of-day responsive behavior

8. **`pcg_blood_dressing`** (~4-6h)
   - Gore level gating
   - Floor + wall blood decal scatter
   - Integration with storytelling scene blood patterns
   - Hospice mode total disable

### Phase 3: Storytelling and Polish (~20-28h)

9. **New Storytelling Patterns** (~8h)
   - Implement 8 new scene patterns (last_stand, ritual, quarantine, etc.)
   - PCG scene selector subgraph (room type -> compatible scenes -> weighted random)
   - Auto-placement across building rooms

10. **Cobwebs + Broken Windows + Graffiti** (~8h)
    - Cobweb static mesh library (4-5 variants) + corner placement
    - Broken window system (glass shards, boarding, draft emitter)
    - Graffiti decal scatter on vertical surfaces

11. **New Horror Props** (~4h)
    - `cobweb`, `boarding_planks`, `broken_glass_pile`, `candle_cluster`, `warning_sign`
    - Add to `create_horror_prop` action

12. **`bake_atmosphere` + Performance Pass** (~4-6h)
    - ClearPCGLink to freeze output
    - HISM conversion for static meshes
    - Distance-based LOD and culling setup
    - Budget validation against performance targets

### Phase 4: Tension Integration + Hospice (~12-16h)

13. **Tension Curve Pipeline** (~6h)
    - Import `analyze_level_pacing_structure` output into spatial registry
    - PCG reads tension alongside decay
    - Safe room exemption logic
    - Gradient transitions between tension zones

14. **Hospice Accessibility Settings** (~4-6h)
    - All content sensitivity controls as PCG parameters
    - `validate_horror_intensity` integration with PCG output
    - `generate_hospice_report` extension for atmosphere audit
    - Safe room guarantee enforcement

15. **Time-of-Day Responsive Actors** (~2-4h)
    - MPC integration for placed Blueprint actors
    - Fog density, light intensity, particle count runtime adjustment
    - Material parameter drives for decal appearance shift

### Total Estimate: ~84-112h across 4 phases

### Priority Order
1. Phase 1 unlocks the entire pipeline
2. Phase 2 delivers the most visible horror impact (lighting + fog + audio)
3. Phase 3 adds storytelling depth
4. Phase 4 connects everything to the pacing/accessibility systems

---

## 22. Sources

### UE5 PCG Documentation
- [PCG Framework Node Reference (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG Biome Core and Sample Plugins](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-pcg-biome-core-and-sample-plugins-reference-guide-in-unreal-engine)
- [PCG GPU Processing](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-with-gpu-processing-in-unreal-engine)
- [PCG Generation Modes](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine)
- [Graph Parameters (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/PCG/GraphParameters)

### PCG Tutorials and Community
- [How to Create a Horror Environment in UE5 Using PCG and Landmass](https://dev.epicgames.com/community/learning/tutorials/DlzR/how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass)
- [Integrate Niagara FX with PCG Points in UE5](https://dev.epicgames.com/community/learning/tutorials/8Xw2/unreal-engine-integrate-niagara-fx-with-pcg-points-in-ue5)
- [Passing Values from PCG to Actor Blueprints](https://dev.epicgames.com/community/learning/tutorials/9d7z/unreal-engine-passing-values-from-unreal-pcg-to-actor-blueprints)
- [A Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [PCG Basics: First Procedural Scatter System (Hyperdense, 2026)](https://medium.com/@sarah.hyperdense/pcg-basics-your-first-procedural-scatter-system-in-ue5-fab626e1d6f0)
- [3 Ways to Scatter PCG Points on a Mesh](https://forums.unrealengine.com/t/community-tutorial-3-ways-to-scatter-pcg-points-on-a-mesh/2176679)

### PCG Decals and Gore
- [PCG Geometry Decals (Fab)](https://www.fab.com/listings/98539376-5a9d-4fc4-9793-e921e5332056)
- [PCG Geometry Decals Documentation](https://www.munduscreatus.be/pcg-geometry-decals-documentation/)
- [Custom PCG Geometry Decals System (80.lv)](https://80.lv/articles/custom-pcg-geometry-decals-system-for-unreal-engine-5)
- [Optimizing Blood/VFX in UE5 -- Let Them Come: Onslaught](https://www.unrealengine.com/en-US/tech-blog/optimizing-blood-and-vfx-systems-in-ue5-for-let-them-come-onslaught)

### Fog and Atmosphere
- [Volumetric Fog (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine)
- [Local Fog Volumes (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine)
- [Local Fog Volumes Tutorial](https://dev.epicgames.com/community/learning/tutorials/owyG/unreal-engine-local-fog-volumes)

### Horror Level Design
- [3 Level Design Horror Principles -- Alan Wake](https://www.worldofleveldesign.com/categories/level_design_tutorials/alan-wake-horror.php)
- [Create Horror and Bring Fear -- Part 1: Cliches](https://www.worldofleveldesign.com/categories/level_design_tutorials/horror-fear-level-design/part1-survival-horror-level-design-cliches.php)
- [Horror Survival Level Design: Part 3 -- Environment and Story](https://www.worldofleveldesign.com/categories/level_design_tutorials/horror-fear-level-design/part3-survival-horror-level-design-story-environment.php)
- [Creating Horror through Level Design (Gamedeveloper)](https://www.gamedeveloper.com/design/creating-horror-through-level-design-tension-jump-scares-and-chase-sequences)
- [When Buildings Dream: Environmental Storytelling in Horror (Dr. Wedgbury)](https://drwedge.uk/2025/05/04/when-buildings-dream-horror-game-design/)
- [Dead Space -- Storytelling through Level Design](https://www.gamedeveloper.com/design/dead-space---storytelling-through-level-design)

### Environmental Storytelling
- [GDC Vault: Environmental Narrative -- Your World is Your Story](https://www.gdcvault.com/play/1012712/)
- [GDC Vault: What Happened Here? Environmental Storytelling](https://gdcvault.com/play/1012647/What-Happened-Here-Environmental)
- [Environmental Storytelling (Gamedeveloper)](https://www.gamedeveloper.com/design/environmental-storytelling)

### AI Director / Pacing
- [The AI Systems of Left 4 Dead (Mike Booth, Valve)](https://steamcdn-a.akamaihd.net/apps/valve/2009/ai_systems_of_l4d_mike_booth.pdf)
- [The Director -- L4D Wiki](https://left4dead.fandom.com/wiki/The_Director)
- [The Safe Room: How Game Designers Create Horror](https://thegamesedge.com/the-safe-room-how-game-designers-create-horror/)

### Cobweb Generation
- [Free Cobweb Generator for Houdini (80.lv)](https://80.lv/articles/free-cobweb-generator-for-houdini)
- [Cobweb Generator for Blender (Gumroad)](https://vilemduha.gumroad.com/l/ZWBtg)
- [Tutorial: Setting Up a Cobweb Generator in Blender (80.lv)](https://80.lv/articles/tutorial-setting-up-a-cobweb-generator-in-blender)

### Graffiti / Decal Systems
- [Decal Designer: Drag & Drop Graffiti, Dirt & Damage Generator (Fab)](https://www.fab.com/listings/d85387d2-09e8-4511-bf1a-86b58c2c457d)
- [Developing a Procedural Graffiti Generator in UE5 (80.lv)](https://80.lv/articles/developing-a-procedural-graffiti-generator-in-unreal-engine-5)

### Previous Monolith Research
- [PCG Framework Deep Dive](2026-03-28-pcg-framework-research.md)
- [Auto Volumes Research](2026-03-28-auto-volumes-research.md)
- [Spatial Registry Research](2026-03-28-spatial-registry-research.md)
- [Facade Window Research](2026-03-28-facade-window-research.md)
- [Horror AI Systems Research](../../.claude/agent-memory/research-agent/reference_horror_ai_systems.md)
