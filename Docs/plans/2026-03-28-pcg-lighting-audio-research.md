# PCG for Lighting, Audio, and Atmospheric Systems in Horror

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready, MegaLights Beta)
**Context:** How PCG can drive the atmospheric layer -- lights, sound, fog, particles, post-process -- that makes procedural horror environments actually scary

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Street Light Placement](#2-street-light-placement)
3. [Interior Lighting](#3-interior-lighting)
4. [Flickering Light System](#4-flickering-light-system)
5. [Horror Shadow Design](#5-horror-shadow-design)
6. [Audio Emitters Per Room](#6-audio-emitters-per-room)
7. [Audio Occlusion & Attenuation](#7-audio-occlusion--attenuation)
8. [Music & Tension Zones](#8-music--tension-zones)
9. [Post-Process Volumes](#9-post-process-volumes)
10. [Fog Volumes](#10-fog-volumes)
11. [Particle Emitters (Dust, Smoke, Fireflies)](#11-particle-emitters)
12. [Day/Night Cycle Integration](#12-daynight-cycle-integration)
13. [Emergency Lighting](#13-emergency-lighting)
14. [Performance Budgets](#14-performance-budgets)
15. [Implementation Architecture](#15-implementation-architecture)
16. [Monolith MCP Integration](#16-monolith-mcp-integration)
17. [Effort Estimate](#17-effort-estimate)
18. [Sources](#18-sources)

---

## 1. Executive Summary

PCG is a **placement and distribution engine** -- it generates point clouds with attributes, then spawns actors/meshes at those points. It cannot create geometry, but it is ideal for atmospheric element placement because:

- Lights, sounds, particles, and volumes ARE actors that need intelligent spatial distribution
- PCG's density/exclusion/distance rules naturally produce good atmospheric layouts
- Point attributes (density, color, custom metadata) map directly to light intensity, sound volume, fog density
- Runtime generation mode enables dynamic atmosphere (day/night, tension-driven changes)

**Key limitation:** PCG cannot directly spawn non-mesh components (point lights, audio components). The workaround is **Blueprint actor templates** -- BP actors containing the desired components (mesh + light, mesh + audio, etc.) spawned via PCG's SpawnActor node. This is the industry-standard approach.

**Existing Monolith coverage:** 5 lighting analysis actions, 14 audio/acoustic actions, spawn_volume (audio/post_process), place_light, set_light_properties, classify_zone_tension. The PCG layer would GENERATE the initial placements that these actions then analyze and tune.

---

## 2. Street Light Placement

### The Problem

Street lights need regular intervals along roads (15-30m real-world = 1500-3000cm UE), with working/broken variation for horror. PCG is ideal for this -- it's literally a spline-sampling problem.

### PCG Approach

```
Road Spline Input
  -> Spline Sampler (distance: 2000cm, jitter: +-200cm)
  -> Attribute Noise (float "working" 0.0-1.0)
  -> Branch on Attribute:
     working > 0.7  -> Spawn BP_StreetLight_Working
     working > 0.3  -> Spawn BP_StreetLight_Flickering
     working <= 0.3 -> Spawn BP_StreetLight_Broken
```

### Blueprint Actor Templates

**BP_StreetLight_Working:**
- StaticMeshComponent: lamp post mesh
- PointLightComponent: warm white (3200K), intensity 5000 lux, attenuation 1500cm
- Shadows enabled, Mobility: Stationary

**BP_StreetLight_Flickering:**
- Same mesh
- PointLightComponent: same base settings
- Timeline component driving SetIntensity with randomized keyframes
- Bool "bIsFlickering" exposed to PCG as actor override

**BP_StreetLight_Broken:**
- Broken mesh variant (glass shattered, bent pole)
- NO light component (dark)
- Optional: sparking Niagara system (low particle count, intermittent)

### Horror Ratio

For survival horror: **70-80% broken, 10-15% flickering, 10-15% working**. This creates pools of light that the player gravitates toward, with vast dark stretches between them. The `working` threshold in the PCG graph controls this ratio directly.

### Intersection Handling

Street intersections need brighter pools. PCG can use a **Volume exclusion + override** approach:
- Place PCG volumes at intersections with higher `working` threshold (0.5 instead of 0.7)
- Or use PCG's Difference node to exclude intersection areas from the spline sampler, then add a separate Surface Sampler for intersection lights

### Alignment

Lights should face the road. PCG's Transform Points node can align rotation to the spline tangent, then add 90-degree offset to face perpendicular to the road. The SpawnActor node preserves point rotation.

---

## 3. Interior Lighting

### Room-Type Light Recipes

Each room type in the spatial registry has a canonical lighting recipe:

| Room Type | Fixture Types | Working % (Horror) | Color Temp | Notes |
|-----------|--------------|-------------------|------------|-------|
| Kitchen | Overhead fluorescent, under-cabinet | 30% | 4000K cool white | Buzzing sound when working |
| Bathroom | Vanity mirror, ceiling | 20% | 3500K | Mirror light = high scare potential |
| Bedroom | Bedside lamp, overhead | 40% | 2700K warm | Table lamp = only working light |
| Hallway | Ceiling flush-mount, every 3m | 15% | 3500K | Long dark stretches, one working at end |
| Office | Fluorescent panel, desk lamp | 25% | 4000K | Some panels hanging at angle |
| Lobby | Chandelier, wall sconces | 35% | 3000K | Chandelier swinging = horror set piece |
| Basement | Single bare bulb, pull chain | 10% | 2700K | THE classic horror light |
| Stairwell | Wall-mount, per landing | 20% | 3500K | One light per 2 floors working |
| Hospital Room | Overhead fluorescent (2x4 panel) | 30% | 5000K daylight | Clinical, harsh when lit |
| Morgue/Lab | Fluorescent, examination lamp | 15% | 5500K | Cold, institutional |

### PCG Interior Light Graph

```
Spatial Registry Room Data (from JSON)
  -> For Each Room:
     -> Get Room Type + Bounds
     -> Room-Type Light Template Lookup (DataTable)
     -> Ceiling Surface Sample (density from template)
     -> Filter by Working % (Attribute Noise + room seed)
     -> Spawn BP_CeilingLight_{Type} or BP_TableLamp or BP_WallSconce
     -> Pass "working" attribute to spawned actor
```

### Light Placement Rules

1. **Overhead lights:** Center of room, or grid pattern for rooms > 4m in any dimension
2. **Table/desk lamps:** Placed on furniture surfaces (requires furniture placement to happen first)
3. **Wall sconces:** Along corridors at regular intervals, alternating sides
4. **Bare bulbs:** Center of small rooms (basement, closet), slightly off-center for unease

### Fixture-on-Furniture Dependency

Table lamps and desk lamps require furniture to be placed first. The pipeline must be:
1. Room geometry (create_structure / create_building)
2. Furniture placement (scatter_props / create_furniture)
3. Interior lighting (PCG graph reads furniture locations as anchor points)

PCG can use **Get Actor Data** node to read existing furniture actors and sample their surfaces for lamp placement.

---

## 4. Flickering Light System

### Architecture: PCG Tags, Blueprint Drives

PCG's role is to **mark** which lights flicker and set flicker parameters. The actual flickering is a Blueprint/Timeline concern.

### Flicker Parameter Space

Each flickering light BP exposes these as PCG-settable attributes:

| Parameter | Range | Horror Meaning |
|-----------|-------|---------------|
| `FlickerSpeed` | 0.1 - 5.0 Hz | Slow = dying, Fast = electrical fault |
| `FlickerDepth` | 0.0 - 1.0 | 0 = stable, 1 = full off periods |
| `FlickerPattern` | Enum | Regular/Random/Dying/Surge/Morse |
| `OnDuration` | 0.1 - 10.0s | Time between flicker bouts |
| `OffDuration` | 0.0 - 5.0s | How long it stays dark |
| `SoundCue` | SoftObjectPath | Electrical buzz, fluorescent hum |

### Flicker Patterns for Horror

1. **Dying:** Gradually decreasing intensity over 30-60s, occasional bright surge, then dark. One-shot, not looping. Triggers player anxiety about losing light source.
2. **Random:** Perlin noise-driven intensity. Unpredictable. Good for background unease.
3. **Surge:** Sudden full-bright flash (0.1s), then slow fade back to dim. Associated with electrical events, monster proximity.
4. **Morse:** Rhythmic pattern suggesting intelligence. Use sparingly -- extremely unsettling.
5. **Sync:** Multiple lights flicker in unison, suggesting a shared power source failing. PCG sets same seed for lights on same circuit.

### Implementation

```cpp
// In BP_FlickeringLight, Construction Script or BeginPlay:
// Read PCG-set attributes
FlickerSpeed = GetAttributeFloat("FlickerSpeed", 2.0f);
FlickerDepth = GetAttributeFloat("FlickerDepth", 0.5f);
// ... etc

// Timeline or Tick drives intensity:
float Noise = FMath::PerlinNoise1D(GetWorld()->GetTimeSeconds() * FlickerSpeed + Seed);
float Intensity = FMath::Lerp(BaseIntensity * (1.0f - FlickerDepth), BaseIntensity, Noise);
LightComponent->SetIntensity(Intensity);
```

### Synced Flickering (Circuit Groups)

PCG can assign a `CircuitGroup` integer attribute to lights sharing a power run. All lights with the same CircuitGroup use the same random seed, creating synchronized flicker. When one goes out, they all go out -- classic horror power failure.

---

## 5. Horror Shadow Design

### Shadow Strategy

Horror relies on shadows more than light. The key principles:

1. **Long shadows:** Low-angle light sources (near floor level, or wall-mounted low) create elongated shadows from furniture and architectural elements
2. **Moving shadows:** Swinging lights (pendulum BP) create sweeping shadow movement
3. **False shadows:** Static mesh "shadow cookies" (gobo lights) project shadow patterns without a physical source
4. **Player shadow:** The player's own shadow, cast by a light behind them, walking ahead of them down a corridor
5. **Monster shadows:** Cast before the monster is visible -- shadow rounds a corner before the creature does

### PCG Shadow Light Placement

PCG places **shadow-inducing lights** -- these are lights positioned specifically to cast dramatic shadows from existing geometry:

```
Room Geometry Analysis
  -> Find tall objects (shelving, pillars, window frames)
  -> For each tall object:
     -> Place low spotlight aimed at base (casts upward shadow on ceiling)
     -> Or place backlight (behind object relative to player approach direction)
  -> Corridor Analysis:
     -> Place single light at far end (everything in corridor casts toward player)
```

### Shadow Performance Concerns

With Lumen + Virtual Shadow Maps:
- VSM uses 16K x 16K virtual pages, cached between frames
- Stationary lights: shadow maps cached, only update when objects move
- Movable lights: full shadow update every frame -- expensive
- **Recommendation:** Most horror lights should be **Stationary** with occasional **Movable** for swinging/flickering critical moments

### Gobo Lights (Shadow Cookies)

Spotlights with IES profiles or light function materials can project:
- Window blind shadows (horizontal bars)
- Tree branch shadows (organic patterns)
- Chain-link fence shadows
- Cross/crucifix shadows (specific to horror)

PCG places gobo spotlights aimed at walls/floors in rooms with windows, using the window direction from the spatial registry to orient correctly.

---

## 6. Audio Emitters Per Room

### Room-Type Ambient Sound Map

Each room type has characteristic ambient sounds:

| Room Type | Primary Ambient | Secondary Sounds | Horror Layer |
|-----------|----------------|-----------------|-------------|
| Kitchen | Refrigerator hum (60Hz) | Dripping faucet, settling pipes | Something in the fridge, scratching from inside walls |
| Bathroom | Dripping water, pipes | Toilet running, vent fan | Mirror reflection sound (high pitched), water in tub |
| Bedroom | Clock ticking, wind outside | Creaking bed springs, closet | Breathing from under bed, whispers |
| Hallway | Wind draft, distant sounds | Footsteps echo, door creaks | Second set of footsteps behind player |
| Basement | Water heater, boiler | Settling foundation, rats | Low rumble, dragging sounds |
| Office | HVAC hum, fluorescent buzz | Paper rustle, chair creak | Phone ringing (no one there), typing sounds |
| Hospital | Heart monitor beep, PA system | IV drip, ventilator | Flatline tone, wheelchair rolling |
| Stairwell | Echo, wind | Handrail creak | Footsteps above/below (no one there) |

### PCG Audio Placement

```
Spatial Registry Room Data
  -> For Each Room:
     -> Room Type -> Audio Template Lookup
     -> Spawn BP_AmbientSoundEmitter at room center
        Attributes: sound_cue, volume, attenuation_radius, loop=true
     -> For secondary sounds:
        -> Random positions within room bounds
        -> Lower volume, smaller attenuation
        -> Some one-shot with random interval triggers
     -> Horror layer:
        -> Triggered by tension system, not always active
        -> PCG places the POTENTIAL emitters, gameplay activates them
```

### Blueprint Audio Actor

**BP_AmbientSoundEmitter:**
- AudioComponent: configurable SoundCue/MetaSoundSource
- Attenuation settings exposed (min/max distance, falloff)
- Auto-activates on BeginPlay
- Volume driven by room size (larger room = louder to fill space)
- `bHorrorLayer` flag: if true, only plays when tension > threshold

### Soundscape Plugin Integration

UE5's built-in **Soundscape** plugin (by Epic's Dan Reynolds) procedurally generates ambient audio at runtime based on nearby environment:
- Builds "Sound Palettes" from "Sound Colors"
- ~90% of ambient sounds can be spatially generated at runtime with no manual placement
- Integrates with PCG -- PCG places biome/room-type markers, Soundscape reads them

**Recommendation:** Use Soundscape for OUTDOOR ambient (wind, insects, distant sounds). Use explicit PCG-placed emitters for INTERIOR per-room sounds where specific spatial positioning matters for horror (dripping from a specific corner, scratching from a specific wall).

---

## 7. Audio Occlusion & Attenuation

### Audio Volumes Per Room

The auto-volumes research (already completed) covers AAudioVolume generation per room. Key recap:

- One AAudioVolume per room, sized to room bounds
- Reverb preset mapped from room size:
  - Tiny (< 3x3m): Bathroom reverb (short, bright)
  - Small (3-5m): Room reverb (medium decay)
  - Medium (5-10m): Hall reverb (longer decay, some diffusion)
  - Large (> 10m): Warehouse/Cathedral (long tail, spacious)
- FInteriorSettings for sound isolation between rooms:
  - `ExteriorVolume`: 0.3 (sounds from outside are quiet)
  - `ExteriorLPF`: 0.7 (outside sounds are muffled)
  - `InteriorVolume`: 1.0
  - `InteriorLPF`: 1.0

### PCG Enhancement to Audio Volumes

PCG adds DETAIL that auto-volumes can't:

1. **Per-wall occlusion markers:** PCG can tag wall segments with material type (concrete = high occlusion, wood = medium, glass = low, broken = none). Existing `analyze_sound_propagation` action reads these.

2. **Door state affecting occlusion:** PCG-placed door actors have `bOpen`/`bClosed` state. Open doors reduce occlusion between adjacent rooms. The audio system reads door state at runtime.

3. **Broken window occlusion holes:** PCG marks broken windows. Sound leaks through broken windows more than intact ones. Existing `find_sound_paths` action already traces through geometry.

### Sound Attenuation Presets

| Sound Type | Min Distance | Max Distance | Falloff |
|-----------|-------------|-------------|---------|
| Room ambient (hum) | 50cm | 500cm | Natural |
| Point source (drip) | 10cm | 300cm | Natural |
| Horror layer (whisper) | 0cm | 200cm | Logarithmic |
| Emergency alarm | 100cm | 3000cm | Natural |
| Distant outdoor | 1000cm | 10000cm | Linear |

---

## 8. Music & Tension Zones

### Zone Architecture

PCG places trigger volumes that the music/tension system reads:

| Zone Type | Trigger | Audio Response | PP Response |
|-----------|---------|---------------|-------------|
| Safe Room | Room tagged "safe" in spatial registry | Calm ambient, music fades to gentle | Warm color grade, slight bloom |
| Exploration | Default corridors, open rooms | Low tension drone, environmental | Neutral with slight desaturation |
| Danger Zone | Near monster spawn, dead ends | Tension stinger, heartbeat rises | Vignette increases, desaturation |
| Chase | Monster actively pursuing | Full combat/chase music | Heavy vignette, chromatic aberration |
| Aftermath | Post-encounter, 30s cooldown | Relief stinger, slow fade to calm | Slowly normalizing |

### PCG Tension Mapping

```
Spatial Registry + Horror Metrics
  -> classify_zone_tension (existing action) per room
  -> PCG reads tension classification as attribute
  -> Spawn TriggerVolume at room bounds
     Attribute: tension_level (0.0-1.0)
  -> Blueprint on TriggerVolume:
     OnBeginOverlap -> Update Music Manager with tension_level
     Blends between zones using fade time from PCG attribute
```

### Music Manager (Not PCG, but PCG-Fed)

The Music Manager is a gameplay system that PCG feeds data into:
- Maintains current tension state (0-1 float, smoothed)
- Crossfades between music layers based on tension
- Uses Quartz for beat-synced transitions
- Horror stingers triggered by specific events (door opens, light breaks, monster spotted)

PCG's contribution: placing the tension trigger volumes that DEFINE where zones begin and end.

### Adaptive Tension from AI Director

The tension zones PCG places are the BASELINE. The AI Director can dynamically modify them:
- If player has been in high tension too long, forcefully reduce (rubber banding)
- If player is coasting, spawn a scare event in a "safe" zone
- PCG provides the terrain, the AI Director provides the weather

---

## 9. Post-Process Volumes

### Per-Building Mood Presets

Already designed in auto-volumes research. Key mapping:

| Building Type | Mood | PP Settings |
|--------------|------|-------------|
| Residential | Warm decay | Slight warm tint, low saturation (0.85), mild bloom |
| Hospital/Clinic | Clinical cold | Blue-white tint, high saturation (1.1), harsh contrast |
| Police Station | Institutional | Neutral, slight cyan tint, normal contrast |
| Abandoned | Desaturated | Very low saturation (0.6), slight green tint (mold), grain |
| Basement | Oppressive | Heavy vignette, warm tint, very low exposure |
| Outdoor Night | Cold void | Blue tint, high contrast, some film grain |

### PCG Post-Process Placement

PCG places PP volumes at two granularities:

1. **Building-level:** One PP volume encompassing the entire building, with the building-type mood. Lower priority.
2. **Room-level:** Override PP volumes for specific high-impact rooms. Higher priority. Examples:
   - Red-tinted PP in a room with blood/gore
   - Complete desaturation in a "liminal" horror room
   - Warm golden PP in a "memory" safe room

### Tension-Driven PP

The tension system (from music/tension zones) also drives PP:

| Tension Level | Vignette | Saturation | Grain | Chromatic Ab |
|--------------|----------|-----------|-------|-------------|
| 0.0 (Safe) | 0.2 | 1.0 | 0.0 | 0.0 |
| 0.3 (Uneasy) | 0.3 | 0.9 | 0.1 | 0.0 |
| 0.5 (Tense) | 0.4 | 0.8 | 0.2 | 0.1 |
| 0.7 (Dread) | 0.5 | 0.7 | 0.3 | 0.2 |
| 1.0 (Panic) | 0.7 | 0.5 | 0.5 | 0.5 |

These are driven by a Material Parameter Collection (MPC) or direct PP modification at runtime, NOT by PCG. PCG places the baseline PP volumes; the tension system modulates on top.

### UE 5.7 API

```cpp
// Spawning a PostProcessVolume programmatically
APostProcessVolume* PPV = World->SpawnActor<APostProcessVolume>();
PPV->bUnbound = false;
PPV->Settings.bOverride_ColorSaturation = true;
PPV->Settings.ColorSaturation = FVector4(0.85f, 0.85f, 0.85f, 1.0f);
PPV->Settings.bOverride_VignetteIntensity = true;
PPV->Settings.VignetteIntensity = 0.4f;
PPV->BlendWeight = 1.0f;
PPV->Priority = 1; // higher overrides lower
```

Already supported by existing `spawn_volume` action with `type: "post_process"`.

---

## 10. Fog Volumes

### UE 5.7 Fog Systems

Three fog systems available:

1. **ExponentialHeightFog:** Global, one per level. Good for outdoor baseline.
2. **Local Fog Volumes (UE 5.3+):** Per-area volumetric fog. The key tool for procedural horror fog.
3. **Niagara particle fog:** Sprite-based, fully dynamic, more expensive.

### Local Fog Volumes for Horror

Local Fog Volumes (introduced UE 5.3, production in 5.7) are the ideal solution:
- Placed as actors with box extent
- Interact with Volumetric Fog and lights (light shafts through fog)
- Height falloff built-in
- Density, height, color all configurable
- NO performance cost beyond volumetric fog itself (they're just density injectors)
- Can exist without ExponentialHeightFog as of UE 5.3

### Per-Room Fog Density

| Room Type | Fog Density | Color | Height Falloff | Notes |
|-----------|------------|-------|---------------|-------|
| Basement | 0.3 (heavy) | Gray-green | From floor, 150cm | Thick at floor, clear at head height |
| Bathroom | 0.1 (light) | White | From floor, 50cm | Steam residue |
| Hallway | 0.05 (wisp) | Gray | Full height | Barely visible, creates depth |
| Outdoor Street | 0.02 (ambient) | Blue-gray | Full height | Distance fog, obscures far end |
| Morgue | 0.15 (cold) | Blue-white | From floor, 100cm | Refrigeration mist |
| Sewer | 0.4 (dense) | Yellow-green | From water level, 80cm | Noxious |

### PCG Fog Graph

```
Spatial Registry Room Data
  -> For Each Room:
     -> Room Type -> Fog Template Lookup
     -> If fog_density > 0:
        -> Spawn ALocalFogVolume at room center
           Extent = room bounds
           Density = template density * decay_factor
           HeightFalloff from template
           FogColor from template
     -> Outdoor areas:
        -> ExponentialHeightFog handles global
        -> Local volumes at specific atmospheric locations (near water, vents, grates)
```

### Horror Fog Tricks

1. **Knee-level fog:** Height falloff stops at ~80cm. Player can see above but not their feet. Classic horror. Hides ground-level threats.
2. **Fog that thickens when monster is near:** Runtime system increases Local Fog Volume density via the tension system. PCG places the volumes; gameplay modulates density.
3. **Light shafts through fog:** Local Fog Volumes interact with Volumetric Fog. A single light source shining through dense fog creates dramatic god rays. PCG positions fog+light combos intentionally.
4. **Fog obscuring exits:** Dense fog at corridor ends makes the player uncertain what's ahead. PCG places fog volumes at corridor dead ends and intersections.

---

## 11. Particle Emitters

### PCG + Niagara Integration

UE5 supports direct PCG-to-Niagara integration. The approach:

1. PCG generates point data with attributes
2. Points are passed to a Niagara system as spawn locations
3. Niagara reads PCG attributes (density, color, lifetime) per-point

Official Epic tutorial confirms this workflow: "Integrate Niagara FX with PCG Points in UE5"

### Ambient Particle Types for Horror

| Effect | Niagara System | Spawn Conditions | Performance |
|--------|---------------|-----------------|-------------|
| Dust motes | NS_DustMotes | Abandoned rooms, density from decay_factor | 50-200 particles/room, GPU sim |
| Floating ash | NS_Ash | Fire-damaged areas, outdoor near burn sites | 100-300 particles/area |
| Fireflies | NS_Fireflies | Outdoor overgrown areas, night only | 20-50 particles/area |
| Dripping water | NS_WaterDrip | Damaged ceiling, bathroom, basement | 1-5 drip points/room |
| Steam/vapor | NS_SteamVent | Kitchen, bathroom, mechanical rooms | 50-100 particles/vent |
| Smoke wisps | NS_SmokeWisp | Recently burned, candle residue | 20-50 particles/source |
| Spores | NS_Spores | Biological horror areas, decay zones | 30-100 particles/zone |
| Embers | NS_Embers | Near fire sources, electrical sparks | 10-30 particles/source |
| Insects | NS_Insects | Near corpses, decay, garbage | 20-50 particles/source |
| Falling debris | NS_FallingDebris | Damaged ceilings, earthquakes | Event-triggered, 10-30 |

### PCG Particle Placement

```
Room Data + Decay Factor
  -> Surface Sample (ceiling for drips, floor for dust)
  -> Attribute from decay_factor:
     decay > 0.7: dust + debris + insects
     decay > 0.4: dust + occasional drip
     decay > 0.1: light dust only
     decay = 0.0: no ambient particles
  -> Spawn BP_AmbientParticles at each point
     Niagara system selected from decay tier
     Particle count scaled by room volume
```

### Performance Notes

- GPU sim for dust motes (thousands of simple sprites = cheap on GPU)
- CPU sim for drips (physics-based, low count)
- Use Niagara's **Scalability** settings: distance cull at 3000cm, screen-size fade
- LOD: particles simplify at distance (reduce count, increase size)
- Budget: ~500-1000 active particles per visible room, ~5000 total on screen

---

## 12. Day/Night Cycle Integration

### Not PCG-Driven, but PCG-Aware

The day/night cycle is a level-wide system (SunSky + Directional Light rotation), NOT something PCG generates. But PCG-placed lights need to RESPOND to time of day:

### Time-of-Day Light States

| Time | Street Lights | Interior Lights | Outdoor Fog | Ambient Sound |
|------|--------------|----------------|-------------|---------------|
| Day (6:00-18:00) | Off | Mixed (some rooms dark) | Light haze | Birds, traffic, distant activity |
| Dusk (18:00-20:00) | Turning on (staggered) | More lights on | Thickening | Insects beginning, wind picking up |
| Night (20:00-4:00) | On (working ones) | Few remaining | Dense | Crickets, wind, distant howls |
| Dawn (4:00-6:00) | Turning off | Most off | Thinning | Birds returning, fog lifting |

### PCG Implementation

PCG-spawned light BPs check a global `TimeOfDay` variable:
- **Street lights:** `bShouldBeOn = (TimeOfDay > 18.0 || TimeOfDay < 6.0) && bWorking`
- **Interior lights:** Always governed by room state, not time (abandoned buildings don't have working timers)
- **Fog volumes:** Density multiplier from TimeOfDay curve asset

This is Blueprint logic in the spawned actors, NOT PCG graph logic. PCG places them once; they respond to time changes at runtime.

### Horror Time Design

For horror, the game likely runs at a FIXED time (perpetual night, or locked at dusk/early night). In that case:
- PCG places lights for a specific time configuration
- No runtime time-of-day changes needed
- Simpler, more art-directable

If dynamic time IS used, PCG generates for worst-case (night) and lights deactivate during day rather than the reverse.

---

## 13. Emergency Lighting

### Emergency Light Types

| Fixture | Mesh | Light | Color | Behavior |
|---------|------|-------|-------|----------|
| EXIT sign | Flat panel mesh | RectLight, low intensity | Green (US) or Red (non-US) | Always on (battery), or flickering |
| Emergency strip | Floor-level LED strip | Series of point lights or emissive mat | Green/white | Guides toward exits |
| Red alarm light | Rotating beacon | SpotLight with rotation | Red | Strobing, triggered by alarm state |
| Fire alarm strobe | Wall-mount box | PointLight, high intensity | White | 1Hz flash, accessibility critical |
| Backup fluorescent | Ceiling panel | RectLight | Cool white, dimmer than normal | Activates when main power fails |

### PCG Placement Rules

**EXIT signs:**
- One above every exterior door
- One at every stairwell entry
- One at every corridor junction pointing toward nearest exit
- PCG reads door/stairwell data from spatial registry

**Emergency strips:**
- Along corridor floors on both sides
- Continuous spline-sampled, not individual meshes
- 10cm from wall, 5cm above floor

**Red alarm lights:**
- One per corridor intersection
- One per stairwell landing
- Ceiling mounted, rotating when active

**Fire alarm strobes:**
- Per IBC/ADA: one per room if alarm system is active
- Wall mounted, 200cm height (ADA compliant)
- PCG places them; gameplay system activates them

### Horror Emergency Light Design

Emergency lighting creates some of the best horror atmosphere:
- **Red alarm strobes** create rhythmic light/dark cycles that disorient
- **EXIT signs** become ironic (the exit is NOT safe)
- **Emergency strips** create false sense of safety (follow the lights... into a trap)
- **Power failure sequence:** Lights go out -> emergency backup kicks in (dim, red) -> pure emergency lighting only. PCG pre-places both normal and emergency lights; gameplay toggles which set is active.

### Power State System

PCG places lights in **two layers:**
1. **Normal power:** Full lighting, most working
2. **Emergency power:** Emergency fixtures only, activated when normal power fails

A gameplay event (`OnPowerFailure`) toggles between layers:
```
Normal Power: Street lights + interior fixtures
  -> Power Failure Event
Emergency Power: EXIT signs + alarm strobes + emergency strips + backup fluorescents
  -> Power Restore Event (optional, or never -- horror stays dark)
```

---

## 14. Performance Budgets

### Light Budget

**With MegaLights (UE 5.7 Beta):**
- MegaLights traces a fixed number of rays per pixel toward important light sources
- **Fixed budget = constant performance** regardless of light count
- Quality degrades gracefully as light count increases (more noise, less accuracy)
- Orders of magnitude more shadow-casting lights than traditional VSM alone
- **Recommended:** Enable MegaLights for Leviathan. It was designed for exactly this use case (many local lights in interiors).

**Without MegaLights (Lumen + VSM only):**
- ~5-7 overlapping dynamic shadowed lights per view is the practical limit for 60fps
- Shadow-casting spotlights cheaper than point lights (cone vs sphere)
- Stationary lights with baked shadows are nearly free at runtime
- Movable lights: each shadow-casting movable light costs ~0.5-2ms GPU

**Leviathan Budget (with MegaLights):**

| Category | Budget | Notes |
|----------|--------|-------|
| Shadow-casting local lights in view | 30-50 | MegaLights handles efficiently |
| Non-shadow-casting local lights | 100+ | Very cheap, just shading cost |
| Movable (animated) lights | 5-8 | Swinging, flickering with shadow movement |
| Stationary lights | Unlimited (practical) | Shadows baked, no runtime shadow cost |
| Directional light | 1 | Moon/sun |
| Sky light | 1 | Ambient |

**Recommendations:**
- Street lights: **Stationary** (shadows don't need to move, light itself doesn't move)
- Flickering lights: **Stationary** with intensity-only animation (no shadow flicker -- cheaper). If shadow flicker is needed for specific horror moments, promote to Movable temporarily.
- Swinging lights: **Movable** (shadow direction changes)
- Emergency strobes: **Stationary** with emissive material pulse (no actual light component needed for small strobes -- fake it with emissive)

### Audio Budget

| Parameter | Default | Recommended | Notes |
|-----------|---------|-------------|-------|
| Max Concurrent Channels | 32 | 48-64 | PC target, adjustable in Project Settings |
| Ambient emitters per room | 1-3 | -- | Primary + 1-2 secondary |
| Total ambient emitters in range | ~15-20 | -- | Managed by Sound Concurrency |
| Horror layer sounds | 3-5 simultaneous | -- | High priority, preempt ambient |
| Music channels | 2-4 | -- | Layered stems for tension crossfade |

**Sound Concurrency settings:**
- Ambient sounds: MaxCount = 8 per type, Resolution = StopFarthest
- Horror stingers: MaxCount = 2, Resolution = StopOldest, Priority = High
- Music: MaxCount = 1 per stem, Resolution = FadeOut

### Fog Budget

- Local Fog Volumes: Near-zero cost (they just inject density into the volumetric fog grid)
- The cost is in **Volumetric Fog itself**, not the number of volumes
- Budget: `r.VolumetricFog.GridPixelSize` (default 8, increase to 16 for performance)
- `r.VolumetricFog.GridSizeZ` (default 128, reduce to 64 for performance)
- ~20-30 Local Fog Volumes per visible area is fine

### Particle Budget

| Budget Item | Target |
|------------|--------|
| Active Niagara systems in view | 15-25 |
| Total particles on screen | 5000-10000 |
| GPU particle systems | Preferred for ambient (dust, ash) |
| CPU particle systems | For physics-driven (drips, debris) |
| Per-system max particles | Dust: 200, Drips: 5, Embers: 30 |

### Total Atmospheric Performance

Rough GPU cost estimate for full atmospheric layer:

| System | GPU Cost (ms) | Notes |
|--------|--------------|-------|
| MegaLights (30-50 local lights) | 2-4ms | Fixed budget |
| Volumetric Fog + Local Fog Volumes | 1-2ms | Grid resolution dependent |
| Ambient Niagara particles | 0.5-1ms | GPU sim, sprite rendering |
| Post-Process (multiple volumes) | 0.3-0.5ms | Blended, single pass |
| Audio (CPU) | 0.5-1ms CPU | Negligible GPU |
| **Total atmospheric layer** | **~4-8ms GPU** | At 60fps = 16.6ms budget, this is 25-50% |

This is significant. Optimization levers:
1. MegaLights quality settings (ray count per pixel)
2. Volumetric fog grid resolution
3. Particle count + distance culling
4. Non-shadow-casting lights for distant/unimportant fixtures
5. Emissive materials instead of actual lights for very small fixtures (LED strips, small indicators)

---

## 15. Implementation Architecture

### Pipeline Order

The atmospheric layer runs AFTER geometry and furniture, BEFORE gameplay systems:

```
Phase 1: Geometry
  create_structure / create_building / create_city_block
  |
Phase 2: Collision + NavMesh + Blocking Volumes
  auto_collision -> spawn_volumes -> build_navmesh
  |
Phase 3: Furniture & Props
  scatter_props / create_furniture
  |
Phase 4: ATMOSPHERIC LAYER (this research)
  |
  4a. Interior Lights (depends on room types + furniture for table lamps)
  4b. Exterior Lights (depends on road splines + building positions)
  4c. Audio Emitters (depends on room types)
  4d. Audio Volumes (depends on room bounds) -- from auto-volumes
  4e. Post-Process Volumes (depends on building type + room type) -- from auto-volumes
  4f. Fog Volumes (depends on room types + decay_factor)
  4g. Particles (depends on room types + decay_factor)
  4h. Tension Zones (depends on horror metrics)
  4i. Emergency Lights (depends on door/stairwell positions)
  |
Phase 5: Gameplay
  Monster spawns, triggers, interactive objects
```

### PCG Graph Architecture

**Option A: One Monolithic PCG Graph**
- Single graph handles all atmospheric elements
- Pro: Single generation pass, shared spatial data
- Con: Complex, hard to debug, can't regenerate just lights

**Option B: Separate PCG Graphs Per System (RECOMMENDED)**
- `PCG_InteriorLights` -- room-based light fixture placement
- `PCG_ExteriorLights` -- street lights, exterior fixtures
- `PCG_AmbientAudio` -- per-room sound emitter placement
- `PCG_FogVolumes` -- per-room fog density
- `PCG_AmbientParticles` -- dust, drips, atmospheric particles
- `PCG_TensionZones` -- trigger volumes for music/PP zones
- `PCG_EmergencyLights` -- EXIT signs, strobes, alarm lights
- Pro: Modular, can regenerate individual systems
- Con: Multiple generation passes

### Data Flow: Spatial Registry -> PCG

The spatial registry (from connected-room-assembly research) provides per-room data as JSON. PCG needs this as input:

```
Spatial Registry JSON
  -> Monolith MCP reads registry
  -> Converts room data to PCG-compatible format:
     Per room: {center, extent, type, floor, decay_factor, tension_score, doors[], windows[]}
  -> Either:
     A) Feed directly to PCG via UPCGComponent parameters
     B) Write to a temporary Data Asset that PCG reads
     C) Spawn marker actors that PCG's GetActorData reads
```

Option C (marker actors) is simplest and most PCG-native. Monolith spawns invisible "room marker" actors with UProperties matching the room data. PCG's GetActorData node reads them.

### Monolith-Owned vs PCG-Owned

**Monolith should OWN the atmospheric placement logic**, using PCG as an execution backend:

```
Monolith MCP "atmosphere_for_building" action:
  1. Read spatial registry for building
  2. Compute light/audio/fog/particle parameters per room
  3. Spawn room marker actors with computed attributes
  4. Trigger PCG graph generation (per-system graphs)
  5. PCG reads markers, spawns actual actors
  6. Monolith validates results (light coverage check, audio coverage)
  7. Return summary to user
```

This keeps the intelligence in Monolith (horror-aware, tunable via MCP) and uses PCG as the efficient spawning mechanism.

### Alternative: Skip PCG, Use Direct Spawning

For the initial implementation, Monolith could skip PCG entirely and just spawn actors directly via `SpawnActor`. This is simpler:

```cpp
// Direct spawn approach (no PCG dependency)
for (const auto& Room : Building.Rooms)
{
    FActorSpawnParameters Params;
    // Spawn light at room center
    APointLight* Light = World->SpawnActor<APointLight>(RoomCenter, FRotator::ZeroRotator, Params);
    Light->SetIntensity(LightTemplates[Room.Type].Intensity);
    // ... etc
}
```

**Pros of direct spawning:**
- No PCG graph authoring needed
- Full C++ control
- Faster to implement
- Works offline (no PCG editor dependency)

**Cons of direct spawning:**
- No PCG editor mode integration (can't manually adjust with PCG tools)
- No PCG's built-in randomization/density controls
- Harder to hand-tune individual placements
- No runtime regeneration via PCG subsystem

**Recommendation:** Start with direct spawning for Phase 1 (Monolith-native). Add PCG graph backend as Phase 2 optimization for users who want to hand-tune atmospheric elements in the PCG editor.

---

## 16. Monolith MCP Integration

### New MCP Actions

| Action | Category | Params | Description |
|--------|----------|--------|-------------|
| `atmosphere_for_building` | ProcGen | `building_name`, `preset`?, `decay`? | Full atmospheric pass for one building |
| `atmosphere_for_block` | ProcGen | `block_name`, `preset`? | Atmospheric pass for entire city block |
| `place_room_lights` | Lighting | `room_data`, `template`?, `working_pct`? | Place light fixtures for one room |
| `place_street_lights` | Lighting | `spline_actor` or `points`, `spacing`?, `working_pct`? | Place street lights along road |
| `place_room_audio` | Audio | `room_data`, `sound_map`? | Place ambient emitters for one room |
| `place_room_fog` | Atmosphere | `room_data`, `density`?, `color`? | Place local fog volume for room |
| `place_room_particles` | Atmosphere | `room_data`, `decay`?, `effects`? | Place ambient particle emitters |
| `place_emergency_lights` | Lighting | `building_name`, `types`? | Place EXIT signs, strobes, strips |
| `set_power_state` | Lighting | `building_name` or `block_name`, `state` | Toggle normal/emergency/blackout |
| `create_tension_zones` | Horror | `building_name`, `method`? | Auto-generate tension trigger volumes |
| `get_atmosphere_summary` | Analysis | `building_name` or `region` | Report: light count, audio emitters, fog density, particle systems |

### Integration with Existing Actions

| Existing Action | How Atmosphere Integrates |
|----------------|--------------------------|
| `create_structure` | Add `atmosphere: true` param to auto-run atmospheric pass |
| `create_building` | Same |
| `create_city_block` | Same |
| `classify_zone_tension` | Feeds tension data to atmosphere system |
| `suggest_light_placement` | Already suggests placement; new system EXECUTES it |
| `create_audio_volume` | Used by atmosphere system for per-room reverb |
| `spawn_volume` | Used for PP volumes and tension trigger volumes |
| `place_light` | Used internally by room light placement |
| `scatter_props` | Furniture must be placed BEFORE table lamp placement |

### Blueprint Actor Library

Required BP actor templates to create:

| Blueprint | Components | PCG-Settable Attributes |
|-----------|-----------|------------------------|
| BP_CeilingLight_Fluorescent | Mesh + RectLight + AudioComp (buzz) | working, flicker_speed, flicker_depth |
| BP_CeilingLight_Bulb | Mesh + PointLight | working, intensity, color_temp |
| BP_WallSconce | Mesh + SpotLight | working, intensity |
| BP_TableLamp | Mesh + PointLight | working, intensity, color_temp |
| BP_StreetLight | Mesh + PointLight | working, flicker_pattern |
| BP_ExitSign | Mesh + RectLight (green/red) | powered |
| BP_AlarmStrobe | Mesh + PointLight | active, flash_rate |
| BP_EmergencyStrip | SplineMesh + emissive material | powered |
| BP_AmbientSoundEmitter | AudioComponent | sound_cue, volume, attenuation |
| BP_HorrorSoundEmitter | AudioComponent | sound_cue, trigger_tension |
| BP_AmbientParticles | NiagaraComponent | system, count_multiplier |
| BP_SwingingLight | Mesh + PointLight + PhysicsConstraint | swing_amplitude, working |
| BP_CandleLight | Mesh + PointLight + Niagara (flame) | lit |
| BP_GoboLight | SpotLight + LightFunction | pattern, rotation |

### Preset System

Genre presets (already designed in genre-presets research) extend to atmosphere:

```json
{
  "name": "survival_horror_residential",
  "lighting": {
    "working_pct": 0.25,
    "color_temp_k": 2700,
    "flicker_pct": 0.4,
    "shadow_casting": true,
    "emergency_lighting": true
  },
  "audio": {
    "ambient_per_room": true,
    "horror_layer": true,
    "tension_zones": true
  },
  "fog": {
    "indoor_density": 0.05,
    "basement_density": 0.3,
    "outdoor_density": 0.02
  },
  "particles": {
    "dust_in_abandoned": true,
    "drips_in_wet_rooms": true,
    "insects_near_decay": true
  },
  "post_process": {
    "desaturation": 0.85,
    "vignette": 0.3,
    "grain": 0.1
  }
}
```

---

## 17. Effort Estimate

### Phase 1: Direct Spawning (No PCG) -- RECOMMENDED START

| Task | Hours | Depends On |
|------|-------|-----------|
| Blueprint actor library (14 BPs) | 16-20h | -- |
| Room light placement logic | 10-14h | Spatial registry |
| Street light placement logic | 6-8h | Road spline data |
| Room audio emitter placement | 8-10h | Spatial registry |
| Room fog volume placement | 4-6h | Spatial registry |
| Room particle placement | 6-8h | Spatial registry |
| Emergency light placement | 6-8h | Door/stairwell data |
| Tension zone generation | 6-8h | classify_zone_tension |
| Power state system | 4-6h | Light placement |
| atmosphere_for_building action | 8-10h | All above |
| atmosphere_for_block action | 4-6h | atmosphere_for_building |
| get_atmosphere_summary action | 4-6h | -- |
| Testing + tuning | 10-14h | All |
| **Phase 1 Total** | **~92-124h** | |

### Phase 2: PCG Graph Backend (Optional)

| Task | Hours |
|------|-------|
| PCG graphs (7 separate graphs) | 20-28h |
| Room marker actor system | 6-8h |
| PCG generation triggering from Monolith | 8-10h |
| PCG attribute passthrough to BPs | 6-8h |
| Testing PCG vs direct spawn parity | 8-10h |
| **Phase 2 Total** | **~48-64h** |

### Phase 3: Runtime & Polish

| Task | Hours |
|------|-------|
| Music/tension manager integration | 10-14h |
| Runtime PP modulation (tension-driven) | 6-8h |
| Runtime fog density modulation | 4-6h |
| Day/night response (if applicable) | 8-10h |
| Soundscape plugin integration (outdoor) | 6-8h |
| Performance profiling + optimization | 8-12h |
| **Phase 3 Total** | **~42-58h** |

### Grand Total: ~182-246h across 3 phases

Phase 1 alone delivers a fully functional atmospheric system. Phases 2-3 are polish/optimization.

---

## 18. Sources

### Official UE Documentation
- [MegaLights in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/megalights-in-unreal-engine) -- Fixed budget lighting, beta in 5.7
- [Virtual Shadow Maps](https://dev.epicgames.com/documentation/en-us/unreal-engine/virtual-shadow-maps-in-unreal-engine) -- 16K virtual pages, cached
- [Local Fog Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine) -- Per-area volumetric fog
- [Volumetric Fog](https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine) -- Global volumetric system
- [Ambient Sound Actor](https://dev.epicgames.com/documentation/en-us/unreal-engine/ambient-sound-actor-user-guide-in-unreal-engine) -- Sound placement
- [Ambient Zones](https://dev.epicgames.com/documentation/en-us/unreal-engine/ambient-zones-in-unreal-engine) -- Audio volume reverb
- [Sound Concurrency](https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-concurrency-reference-guide) -- Channel management
- [Post Process Effects](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-effects-in-unreal-engine) -- PP volume settings
- [Soundscape Quick Start](https://dev.epicgames.com/documentation/en-us/unreal-engine/soundscape-quick-start) -- Procedural ambient audio
- [PCG Development Guides](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides) -- PCG framework docs

### Community Tutorials
- [Horror Game Flickering Lights UE5](https://dev.epicgames.com/community/learning/tutorials/ZZ8D/how-to-make-a-horror-game-in-unreal-engine-5-flickering-lights-part-8) -- Timeline-based flickering
- [Horror Environment with PCG and Landmass](https://dev.epicgames.com/community/learning/tutorials/DlzR/how-to-create-a-horror-environment-and-atmosphere-in-unreal-engine-5-using-pcg-and-landmass) -- PCG for horror atmosphere
- [Integrate Niagara FX with PCG Points](https://dev.epicgames.com/community/learning/tutorials/8Xw2/unreal-engine-integrate-niagara-fx-with-pcg-points-in-ue5) -- PCG-to-Niagara pipeline
- [Dust with Niagara](https://dev.epicgames.com/community/learning/tutorials/8O02/how-to-make-dust-using-niagara-in-unreal-engine-5) -- Ambient particle creation
- [Fog, Mist and Dust with Niagara](https://dev.epicgames.com/community/learning/tutorials/Gekq/unreal-engine-how-to-make-fog-mist-and-dust-with-niagara-particles) -- Volumetric particle effects
- [PCG Spawn Actor with Blueprint Variables](https://dev.epicgames.com/community/learning/tutorials/1mVL/unreal-engine-ue5-pcg-tutorial-control-spawned-actors-with-blueprint-variables) -- Passing attributes to spawned BPs
- [Passing Values from PCG to Actor Blueprints](https://dev.epicgames.com/community/learning/tutorials/9d7z/unreal-engine-passing-values-from-unreal-pcg-to-actor-blueprints) -- PCG attribute pipeline
- [Custom PCG Nodes](https://blueshift-interactive.com/2025/09/03/how-to-create-custom-pcg-nodes/) -- C++ and BP custom node creation
- [Local Fog Volumes Tutorial](https://dev.epicgames.com/community/learning/tutorials/owyG/unreal-engine-local-fog-volumes) -- Local fog volume setup
- [MegaLights in UE 5.5](https://dev.epicgames.com/community/learning/tutorials/V2kD/unreal-engine-megalights-in-ue-5-5) -- MegaLights overview

### Forum Discussions
- [PCG Add Light in Static Mesh](https://forums.unrealengine.com/t/ue5-2-pcg-add-light-in-static-mesh/1256533) -- Workaround: Blueprint actor templates
- [PCG Spawn Actor with Attributes](https://forums.unrealengine.com/t/pcg-spawn-actor-node-choose-actor-with-attribute-parameter/1751595) -- Attribute-driven actor selection
- [Optimizing Many Dynamic Lights](https://forums.unrealengine.com/t/optimizing-many-dynamic-light-sources-in-ue-5-4/1941916) -- Performance discussion
- [VSM and Local Lights Performance](https://forums.unrealengine.com/t/virtual-shadow-and-local-lights-performance/677724) -- Shadow budget
- [Max Audio Channels for PC](https://forums.unrealengine.com/t/max-audio-channels-for-games-on-pc/380864) -- Audio channel limits
- [Horror Lighting in Devil of Plague](https://forums.unrealengine.com/t/lighting-shadows-in-horror-games-how-we-used-unreal-engine-5-to-build-devil-of-plague/2351092) -- Horror lighting techniques

### Articles & External
- [MegaLights Performance Boost](https://www.pcgamer.com/hardware/Unreal-Engine-5-5-Mega-Lights/) -- Orders of magnitude more lights
- [Gothic Horror in UE5](https://www.exp-points.com/marcin-wiech-creating-a-gothic-horror-in-unreal-engine) -- Atmosphere design principles
- [Horror Vibes Scene Design](https://80.lv/articles/designing-a-scene-with-horror-vibes-in-unreal-engine-5) -- Blue tones, orange contrast, lit fog
- [Ambient and Procedural Sound Design Course](https://dev.epicgames.com/community/learning/courses/qR/unreal-engine-ambient-and-procedural-sound-design) -- Epic's official audio design course
- [Dynamic Ambient Sound System](https://conradolaje.com/making-a-dynamic-ambient-sound-system-in-ue5/) -- Community approach to procedural audio
- [Procedural Towns with PCG and Blueprints](https://dev.epicgames.com/community/learning/tutorials/dXR7/unreal-engine-create-amazing-procedural-towns-in-ue5-with-pcg-blueprints-actors-and-cargo) -- PCG town generation with actor spawning

### Prior Monolith Research
- `2026-03-28-pcg-framework-research.md` -- PCG architecture, API, verdict
- `2026-03-28-auto-volumes-research.md` -- Audio/PP/Nav volumes for proc buildings
- `2026-03-28-spatial-registry-research.md` -- Room data that feeds the atmospheric system
- `2026-03-28-horror-ai-systems-research.md` -- Horror design patterns from shipped games
