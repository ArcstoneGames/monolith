# PCG for Vehicles, Large Props, and Environmental Clutter

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready)
**Context:** Making procedurally generated town blocks feel lived-in (or abandoned) for FPS survival horror. Covers placement of vehicles, mailboxes, trash cans, street furniture, yard items, playground equipment, commercial signage, utility infrastructure, construction barriers, and horror-specific props.
**Dependencies:** City block layout (`2026-03-28-city-block-layout-research.md`), spatial registry (`2026-03-28-spatial-registry-research.md`), PCG framework (`2026-03-28-pcg-framework-research.md`), auto-volumes (`2026-03-28-auto-volumes-research.md`)

---

## Table of Contents

1. [Prop Taxonomy](#1-prop-taxonomy)
2. [Vehicle Placement](#2-vehicle-placement)
3. [Driveway Generation](#3-driveway-generation)
4. [Per-Lot Props (Mailboxes, Trash, Yard Items)](#4-per-lot-props)
5. [Street Furniture](#5-street-furniture)
6. [Playground and Park Equipment](#6-playground-and-park-equipment)
7. [Commercial Signage](#7-commercial-signage)
8. [Construction and Barriers](#8-construction-and-barriers)
9. [Utility Infrastructure](#9-utility-infrastructure)
10. [Horror Props and Environmental Storytelling](#10-horror-props-and-environmental-storytelling)
11. [Density Control System](#11-density-control-system)
12. [Collision and Clearance](#12-collision-and-clearance)
13. [PCG Integration Architecture](#13-pcg-integration-architecture)
14. [Hierarchical Decoration Pipeline](#14-hierarchical-decoration-pipeline)
15. [Data-Driven Prop Library](#15-data-driven-prop-library)
16. [Existing Monolith Integration](#16-existing-monolith-integration)
17. [Proposed MCP Actions](#17-proposed-mcp-actions)
18. [Effort Estimate](#18-effort-estimate)
19. [Sources](#19-sources)

---

## 1. Prop Taxonomy

All town-dressing props fall into a hierarchy of placement contexts. Each context determines WHERE props can go, what rules govern spacing, and what metadata drives selection.

### 1.1 Placement Context Hierarchy

```
Town
  Block
    Lot (residential, commercial, industrial, park, parking)
      Building Perimeter
        Front yard / setback zone
        Side yard
        Backyard
        Driveway / parking pad
        Porch / stoop
      Building Interior (covered by existing furnishing system)
    Street Edge
      Sidewalk furniture zone (60-120cm from curb)
      Curb zone (0-60cm from curb edge)
      On-street parking lane (parallel or angled)
    Street Surface
      Travel lanes
      Intersection zones
    Median / Island
    Alley
    Park / Open Space
```

### 1.2 Prop Categories

| Category | Placement Context | Spacing Rule | Per-Lot? | Horror Variant |
|----------|------------------|--------------|----------|----------------|
| **Vehicles** | Driveway, street parking, lot | Per-space | Yes (1-2) | Wrecked, abandoned, burned |
| **Mailboxes** | Front yard at road edge | 1 per residential lot | Yes | Overflowing, knocked over |
| **Trash/Recycling** | Side yard or street edge | 1-2 per building | Yes | Overturned, spilling |
| **Street Lamps** | Sidewalk furniture zone | 30-45m interval | No | Flickering, broken, dark |
| **Fire Hydrants** | Curb zone | 90-180m, prefer corners | No | Leaking, damaged |
| **Benches** | Sidewalk, park | 20-50m near commercial | No | Broken slats, graffiti |
| **Newspaper Boxes** | Sidewalk near commercial | Cluster of 1-3 | No | Faded, empty |
| **Bus Stops** | Sidewalk | 200-400m | No | Shattered glass shelter |
| **Phone Booths** | Sidewalk | Rare (1-2 per block) | No | Receiver dangling |
| **Signs (street)** | Intersections, curb | Per intersection | No | Bent, missing, graffiti |
| **Signs (commercial)** | Building facade | 1+ per commercial | Yes | Broken neon, faded |
| **Playground** | Park zone | 1 per park | No | Rusty, creaking |
| **Traffic Cones** | Street, parking | Clusters | No | Scattered, knocked |
| **Barriers** | Street, sidewalk | Variable | No | Toppled, weathered |
| **Caution Tape** | Anywhere | Variable | No | Torn, flapping |
| **AC Units** | Building side/roof | 1+ per commercial | Yes | Dripping, rusted |
| **Satellite Dishes** | Roof, wall | 0-2 per residential | Yes | Tilted, broken |
| **Transformers** | Alley, utility poles | Per 4-8 buildings | No | Sparking, humming |
| **Dumpsters** | Alley, commercial rear | 1 per commercial | Yes | Overflowing, burned |
| **Lawn Mower** | Front/back yard | 0-1 per residential | Yes | Abandoned mid-job |
| **Garden Hose** | Side yard, faucet | 0-1 per residential | Yes | Running, coiled mess |
| **Patio Furniture** | Backyard, porch | 0-1 set per residential | Yes | Overturned, scattered |
| **BBQ Grill** | Backyard | 0-1 per residential | Yes | Cold, rusted |
| **Swimming Pool** | Backyard (large lots) | 0-1 per qualifying lot | Yes | Green water, drained |
| **Missing Person Posters** | Poles, walls, windows | Clusters | No | Key horror prop |
| **Police Tape** | Doors, lots, streets | Variable | No | Key horror prop |
| **Quarantine Signs** | Doors, fences | Variable | No | Key horror prop |
| **Boarded Windows** | Building facade | Variable | Yes | Key horror prop |

---

## 2. Vehicle Placement

### 2.1 Vehicle Types and Contexts

| Vehicle Type | Where | Frequency | Horror Variant |
|-------------|-------|-----------|----------------|
| Sedan/SUV | Driveway, street parking | Common (60%) | Flat tires, broken windows, abandoned |
| Pickup truck | Driveway, street | Common (25%) | Loaded with supplies/debris |
| Police car | Street, at crime scene | Rare (5%) | Lights still flashing, doors open |
| Ambulance | Street, hospital | Very rare (2%) | Abandoned mid-call |
| Van | Street, commercial | Uncommon (8%) | Sliding door open, sinister |
| Wrecked car | Intersection, curb | Horror-only | Crashed into pole/building |
| Burned car | Street, lot | Horror-only | Charred shell, smoke |

### 2.2 Driveway Vehicle Placement Algorithm

```
PlaceVehicleInDriveway(lot, building_descriptor):
  1. Find driveway pad (from lot geometry, see Section 3)
  2. If no driveway, skip (apartments use street parking)
  3. Roll occupancy: P(has_car) = 0.6 * (1.0 - horror_decay)
     - horror_decay 0.0 = lived-in, 1.0 = abandoned
  4. Select vehicle class based on building type:
     - house_small: sedan 60%, pickup 30%, SUV 10%
     - house_large: SUV 40%, sedan 30%, pickup 20%, van 10%
     - commercial: van 40%, pickup 30%, sedan 20%, truck 10%
  5. Orient vehicle: heading toward street, centered on driveway
  6. Apply horror damage:
     - decay < 0.3: clean, normal
     - decay 0.3-0.6: dusty, flat tire (20%), cracked windshield (15%)
     - decay 0.6-0.8: moss, all windows broken, doors open (30%)
     - decay > 0.8: burned (15%), wrecked (20%), missing wheels (25%)
```

### 2.3 Street Parking Placement

Three parking configurations based on street type:

**Parallel Parking (residential streets):**
- Space: 600cm long x 250cm wide (one lane width from curb)
- Spacing: 650cm center-to-center (600cm car + 50cm gap)
- Typical: 4-6 spaces per block face
- Occupancy: 40-70% (decreases with horror_decay)
- Constraint: No parking within 600cm of intersection, 300cm of hydrant, 450cm of driveway

**Perpendicular Parking (commercial lots):**
- Space: 250cm wide x 550cm deep
- Row spacing: 700cm (550cm depth + 150cm aisle buffer per side, shared)
- Aisle width: 730cm (two-way) or 360cm (one-way angled)
- Fill rate: 30-80% based on time-of-day and building type

**Angled Parking (45-60 degree, downtown):**
- Space: 250cm wide measured perpendicular to curb, 490-520cm deep
- More efficient than perpendicular (gentler turn angle)
- Common in commercial districts

### 2.4 Vehicle Placement Constraints

- NEVER block road travel lanes (minimum 300cm clear lane width)
- NEVER block driveways (450cm clearance zone)
- NEVER within 300cm of fire hydrant
- NEVER within 600cm of intersection corner
- NEVER within 150cm of another parked vehicle (side gap)
- For horror: wrecked vehicles CAN violate lane constraints (crashed into things)

---

## 3. Driveway Generation

### 3.1 Driveway Types

| Type | Width | Where | Material |
|------|-------|-------|----------|
| Single-car | 300cm | Residential small | Concrete/asphalt |
| Double-car | 500cm | Residential large | Concrete |
| Carport | 350cm | Budget residential | Gravel/concrete |
| Commercial | 600-800cm | Commercial lot entry | Asphalt |
| Shared | 350cm | Duplex/townhouse | Concrete |

### 3.2 Generation Algorithm

```
GenerateDriveway(lot, street_edge):
  1. Find garage/parking location on building footprint
     - Prefer: side of building facing street
     - If detached garage: connect garage to street
  2. Compute connection point on street edge:
     - Perpendicular from building parking face to nearest street edge
     - Snap to lot boundary (don't cross neighbor's lot)
  3. Generate driveway geometry:
     - Straight path from street to garage/pad
     - Width based on driveway type (from building descriptor)
     - Slight flare at street end (apron: +50cm each side over last 100cm)
  4. Cut into terrain if landscape present
  5. Material: concrete default, gravel for rural/poor
  6. Horror variants:
     - Cracks (displacement map or decal)
     - Weeds growing through (foliage scatter on driveway surface)
     - Oil stains (decal)
     - Broken/missing sections
```

### 3.3 Integration with Lot Layout

The driveway is generated during lot subdivision as part of the lot's infrastructure:

```
Lot = {
  footprint: Polygon2D,
  building_footprint: Polygon2D,
  setbacks: { front, side_left, side_right, rear },
  driveway: {
    type: "single" | "double" | "carport" | "none",
    side: "left" | "right" | "center",
    width: float,
    street_connection_point: Vector2D,
    building_connection_point: Vector2D
  },
  parking_pad: Box2D  // Where the car sits
}
```

---

## 4. Per-Lot Props

Props that are unique to each lot and relate to the building's identity.

### 4.1 Mailboxes

**Placement rules:**
- 1 per residential lot (apartments get cluster mailbox at entry)
- Position: 150-200cm from curb edge, at end of walkway or driveway
- Height: 100-120cm (USPS regulation: 41-45 inches to bottom of box)
- Facing: toward street
- Style varies by neighborhood wealth:
  - Poor: cheap metal box on post
  - Middle: decorative post-mounted
  - Wealthy: brick column with slot
  - Commercial: wall-mounted slot or none

**Horror variants:**
- Overflowing with mail (hasn't been collected)
- Knocked off post (tilted 30-45 degrees)
- Missing entirely (just the post remains)
- Spray-painted X marks (quarantine/cleared markers)

### 4.2 Trash and Recycling Bins

**Placement rules:**
- 1-2 per building (trash + recycling for residential)
- Position: side of building near street, OR at curb on "trash day"
- Spacing from building: 50-100cm from wall
- Not blocking sidewalk or driveway
- Commercial: dumpster in rear/alley access

**Horror variants:**
- Overturned, contents spilling
- Dragged partway into street
- Missing lid
- Animal damage (torn bags scattered)

### 4.3 Yard Items (Residential)

Placed in appropriate yard zones based on building type and wealth. Controlled by lot descriptors.

**Front Yard:**
- Garden gnomes, decorative rocks, bird bath (wealth: middle)
- Lawn sign (for sale, political, neighborhood watch)
- American flag / flagpole
- Children's toys (bikes, wagons) if family household
- Newspaper (on walkway or porch)

**Side Yard:**
- Garden hose (coiled near faucet point on building)
- Wheelbarrow, garden tools
- Firewood stack
- AC unit (on concrete pad, against building)

**Backyard:**
- Patio furniture set (table + 2-4 chairs)
- BBQ grill (near patio/deck)
- Swimming pool (only lots > 2000cm x 3000cm backyard)
- Trampoline (suburban, family)
- Tool shed (far corner of yard)
- Clothesline (between two posts or building-to-post)

**Porch/Stoop:**
- Rocking chair, bench
- Potted plants
- Welcome mat
- Jack-o-lanterns (seasonal)
- Packages (delivered, sitting on step)

### 4.4 Per-Lot Selection Algorithm

```
SelectLotProps(lot, building, household, horror_decay):
  props = []

  // Required props (always present unless horror removes them)
  if building.type == RESIDENTIAL:
    props.add(MAILBOX, position=compute_mailbox_pos(lot))
    props.add(TRASH_BIN, position=compute_trash_pos(lot, building))

  // Optional props based on household profile
  budget = household.wealth * (1.0 - horror_decay * 0.5)

  // Front yard
  if budget > 0.3 and random(seed) < 0.4:
    props.add(random_yard_decoration(wealth=budget))
  if household.has_children and random(seed) < 0.5:
    props.add(random_children_toy(), position=front_yard_random())

  // Backyard
  if budget > 0.4 and random(seed) < 0.6:
    props.add(PATIO_SET, position=near_back_door(building))
  if budget > 0.5 and random(seed) < 0.3:
    props.add(BBQ_GRILL, position=patio_area(building))
  if budget > 0.8 and lot.backyard_area > MIN_POOL_AREA and random(seed) < 0.15:
    props.add(SWIMMING_POOL, position=backyard_center(lot))

  // Apply horror damage to selected props
  for prop in props:
    prop.damage = compute_damage(horror_decay, prop.type)

  return props
```

---

## 5. Street Furniture

### 5.1 Placement Zones

Street furniture lives in the **sidewalk furniture zone**: a strip 60-120cm from the curb edge, running parallel to the street. This is the standard municipal placement zone.

```
|  Road  |  Curb  |  Furniture  |  Pedestrian  |  Building  |
|        | 15-20  |   60-120    |   120-180    |   setback  |
|        |  cm    |     cm      |     cm       |            |
```

### 5.2 Spacing Rules (Based on US Municipal Standards)

| Item | Interval | Placement Rule | Notes |
|------|----------|---------------|-------|
| Street lamp | 30-45m | Both sides, staggered | Residential: one side only on narrow streets |
| Fire hydrant | 90-180m | Prefer within 15m of intersection | NFPA 24 standard |
| Bench | 20-50m | Near commercial, transit | Not residential-only streets |
| Newspaper box | N/A | Clusters of 1-3 near commercial | Becoming rare (horror: anachronistic detail) |
| Trash can (public) | 30-60m | Near benches, transit, commercial | Steel mesh or concrete |
| Street sign | Per intersection | All 4 corners | Stop, yield, street name, one-way |
| Parking meter | Per space | Only in commercial zones | Or pay station per block |
| Bus stop | 200-400m | One per route per block | Shelter or pole+sign |
| Phone booth | Very rare | 0-1 per commercial block | Period piece for horror |
| Utility pole | 30-50m | One side of street | With power lines (optional) |
| Tree (street) | 8-15m | Furniture zone or tree pit | Tree grate in commercial |
| Bollard | Variable | Protecting pedestrian zones | Concrete or steel |

### 5.3 Spline-Based Street Furniture Placement

Street furniture is best placed along the street spline (or sidewalk spline) at computed intervals:

```
PlaceStreetFurniture(street_spline, street_type, horror_decay):
  total_length = street_spline.length

  // Phase 1: Mandatory items at fixed intervals
  place_at_intervals(STREET_LAMP, interval=3500, offset_from_curb=80,
                     stagger=true, skip_probability=horror_decay * 0.3)

  // Phase 2: Infrastructure items at computed positions
  place_fire_hydrants(interval=12000, prefer_intersections=true,
                      clearance_from_driveway=450)

  // Phase 3: Amenity items (commercial streets only)
  if street_type in [COMMERCIAL, MIXED_USE]:
    place_at_intervals(BENCH, interval=3000, near_storefronts=true)
    place_at_intervals(PUBLIC_TRASH, interval=4000, near_benches=true)
    place_newspaper_boxes(max_per_block=2, near_intersections=true)

  // Phase 4: Signage at intersections
  for intersection in street_spline.intersections:
    place_intersection_signs(intersection, street_type)

  // Phase 5: Horror modifications
  for item in placed_items:
    apply_horror_variant(item, horror_decay)
```

---

## 6. Playground and Park Equipment

### 6.1 Park Layout

Parks are a lot type (from city block subdivision). A park lot contains:

```
ParkLayout:
  playground_zone: Box2D  // 15m x 15m minimum
  open_field: Box2D       // Remaining area
  path_network: Spline[]  // Connecting entry points
  benches: along paths
  trees: scattered in open areas

PlaygroundEquipment (1 set per playground zone):
  - Swing set: 400x200cm footprint, 300cm height
  - Slide: 300x150cm footprint, 250cm height
  - Sandbox: 300x300cm footprint, 50cm height
  - See-saw: 350x80cm footprint, 100cm height
  - Monkey bars: 400x200cm footprint, 220cm height
  - Merry-go-round: 250cm diameter, 80cm height
  - Spring riders: 80x80cm each, cluster of 2-4
```

### 6.2 Horror Playground

The abandoned playground is a cornerstone horror set piece. Horror decay transforms:

| Equipment | Normal | Decay 0.3-0.6 | Decay 0.6-1.0 |
|-----------|--------|---------------|---------------|
| Swing | Moving gently in wind | One chain broken, seat hanging | All broken, frame leaning |
| Slide | Shiny, functional | Rust patches, graffiti | Collapsed section, sharp edges |
| Sandbox | Clean sand, toys | Weeds, cat-proof cover missing | Overgrown, filled with debris |
| See-saw | Balanced, painted | One side stuck down, peeling | Snapped at pivot |
| Merry-go-round | Colorful | Creaking, one handle broken | Spinning slowly by itself (horror trigger) |

The "swing gently moving on its own" and "merry-go-round slowly spinning" are classic horror visual cues. These should be physics-enabled or animated Blueprint actors rather than static meshes.

---

## 7. Commercial Signage

### 7.1 Sign Types

| Type | Position | Size | Horror |
|------|----------|------|--------|
| Fascia sign | Above storefront | 60-120cm tall, full width | Faded, letters missing |
| Projecting sign | Perpendicular to wall | 60x90cm | Hanging by one bracket |
| A-frame/sandwich | Sidewalk near door | 60x100cm | Fallen over, weathered |
| Neon sign | Window or above door | Variable | Flickering, partial glow |
| Menu board | Near restaurant door | 50x80cm | Stained, outdated menu |
| OPEN/CLOSED | Window or door | 30x15cm | Always says CLOSED now |
| Awning with text | Above storefront | Full width, 100cm drop | Torn, faded, sagging |
| Marquee | Theater/entertainment | Large, with letters | Random letters remaining |

### 7.2 Placement

```
PlaceCommercialSignage(building, facade, horror_decay):
  if building.type not in [COMMERCIAL, MIXED_USE]: return

  // Every commercial building gets a main sign
  sign = generate_sign(building.business_type)
  place_on_facade(sign, facade.front, above_door=true)

  // Optional secondary signs
  if random(seed) < 0.4:
    place_projecting_sign(facade.front)
  if random(seed) < 0.3 and building.business_type in [RESTAURANT, CAFE]:
    place_a_frame(sidewalk, near_door=true)
  if random(seed) < 0.2:
    place_neon(facade.front, in_window=true)

  // Horror: sign text generation
  // Pre-made sign meshes with business names avoid runtime text rendering
  // For variety: 20-30 pre-made sign meshes per business type
  // Horror damage applied per mesh variant (clean, faded, broken, missing letters)
```

---

## 8. Construction and Barriers

### 8.1 Types

| Type | Footprint | Placement |
|------|-----------|-----------|
| Traffic cone | 30cm diameter | Road, parking, sidewalk |
| Jersey barrier | 370x60x80cm | Road block, building perimeter |
| Sawhorse barrier | 120x60x100cm | Sidewalk, road |
| Chain-link fence section | 300x180cm | Lot perimeter, road block |
| Caution tape | Spline-based | Between any two attachment points |
| Road work sign | 60x60cm on stand | Road edge |
| Sandbag wall | Variable | Defensive positions |
| Concrete block | 60x60x60cm | Road barriers |

### 8.2 Horror Barrier Scenarios

Barriers serve dual purpose in horror: environmental storytelling AND gameplay blocking.

```
BarrierScenarios:
  ROAD_BLOCK:
    Jersey barriers across road + police car(s) + police tape
    Props: flares (burned out), traffic cones scattered
    Blocks player vehicle but not foot traffic

  QUARANTINE_ZONE:
    Chain-link fence + caution tape + quarantine signs
    Props: hazmat suit (discarded), medical waste
    Blocks both vehicle and foot traffic

  CONSTRUCTION_SITE:
    Sawhorse barriers + cones + equipment
    Props: porta-potty, dumpster, scaffolding
    Partially accessible (squeeze through)

  EVACUATION_ROUTE:
    Sandbags + directional signs (pointing to safety)
    Props: abandoned personal items (suitcases, bags)
    Narrative: people fled this direction

  MILITARY_CHECKPOINT:
    Jersey barriers in chicane pattern + sandbags
    Props: abandoned weapons, radios, MRE wrappers
    Maximum blocking -- forces player to find alternate route
```

---

## 9. Utility Infrastructure

### 9.1 Per-Building Utility

| Item | Position | Rule |
|------|----------|------|
| AC unit (window) | Window, upper floor | 0-2 per residential (apartment) |
| AC unit (ground) | Side yard, concrete pad | 1 per residential (house) |
| AC unit (roof) | Flat roof commercial | 1+ per commercial |
| Satellite dish | Roof or wall | 0-1 per residential |
| TV antenna | Roof | 0-1 per residential (older homes) |
| Gas meter | Side of building | 1 per building, near street |
| Electric meter | Side of building | 1 per building, near street |
| Water spigot | Side of building | 1-2 per building |

### 9.2 Infrastructure (Shared)

| Item | Position | Interval |
|------|----------|----------|
| Utility pole | Furniture zone / alley | 30-50m |
| Transformer (pole) | On utility pole | Per 4-8 buildings |
| Transformer (pad) | Ground, fenced | Per 8-16 buildings |
| Manhole cover | Street surface | Per intersection + ~60m |
| Storm drain | Curb, at low points | Per 30-60m or at intersections |
| Fire escape | Building side/rear | Per multi-story commercial |
| Roof water tank | Flat roof | Older commercial buildings |

### 9.3 Horror Utility

- Transformers sparking, humming loudly (audio trigger zone)
- Downed power lines (danger zone, gameplay hazard)
- Gas meter leaking (audio + visual cue for puzzle)
- Water spigot running (sound attracts enemies)
- Satellite dishes all pointing wrong direction (subtle wrongness)

---

## 10. Horror Props and Environmental Storytelling

### 10.1 Environmental Storytelling Categories

Based on game design theory (Gamedeveloper.com, Don Carson), environmental storytelling props fall into:

**Cause-and-Effect Vignettes** -- staged areas suggesting past events:
- Crashed car + broken fence + torn-up lawn = someone crashed into a house
- Open door + scattered belongings + running car = panicked escape
- Blood trail + dragged furniture + barricaded door = someone tried to hide

**Implicit Narrative** -- individual props suggesting ongoing situation:
- Missing person posters (clustered on poles, walls, fences)
- Police tape (across doors, around lots)
- Quarantine signs (official-looking, on doors)
- Evacuation notices (on poles, bulletin boards)
- "DO NOT ENTER" spray paint on buildings
- Red X marks on doors (searched/cleared markers, like Katrina)
- Body bags (along curbs, in parking lots) -- extreme horror only

**Behavioral Traces** -- evidence of past inhabitants:
- Newspapers piling up at doors
- Overflowing mailboxes
- Pets (dead or alive) in yards
- Forgotten laundry on clotheslines
- Half-eaten meals visible through windows
- Cars with doors left open, keys in ignition

### 10.2 Horror Prop Density Curve

Horror props should follow a **tension curve** rather than uniform distribution:

```
horror_prop_density(distance_from_horror_center):
  // Dense at epicenter, sparse at edges
  // Creates "something happened HERE" narrative

  normalized = distance / max_horror_radius
  if normalized < 0.2:
    return HIGH  // Epicenter: police tape, barriers, bodies, damage
  elif normalized < 0.5:
    return MEDIUM  // Near zone: missing posters, abandoned cars, broken signs
  elif normalized < 0.8:
    return LOW  // Transition: subtle wrongness, empty streets, one or two odd props
  else:
    return MINIMAL  // Edge: almost normal, just... too quiet
```

### 10.3 Horror Vignette Templates

Pre-designed scene templates that can be placed at specific locations:

```json
{
  "vignette_templates": [
    {
      "name": "abandoned_evacuation",
      "footprint": [600, 400],
      "props": [
        {"type": "car", "variant": "doors_open", "position": [0, 0]},
        {"type": "suitcase", "count": 3, "scatter_radius": 200},
        {"type": "child_toy", "count": 1, "near": "suitcase"},
        {"type": "shoes", "count": 2, "scatter_radius": 100}
      ],
      "horror_level": 0.4
    },
    {
      "name": "police_incident",
      "footprint": [800, 600],
      "props": [
        {"type": "police_car", "count": 2, "angle": "angled_to_curb"},
        {"type": "police_tape", "spline": "around_perimeter"},
        {"type": "evidence_markers", "count": 5, "scatter_radius": 300},
        {"type": "body_outline", "count": 1, "on_ground": true}
      ],
      "horror_level": 0.7
    },
    {
      "name": "barricaded_house",
      "footprint": "per_building",
      "props": [
        {"type": "boarded_windows", "apply_to": "all_ground_floor"},
        {"type": "furniture_barricade", "at": "front_door"},
        {"type": "written_sign", "text_pool": ["STAY AWAY", "GOD HELP US", "THEY COME AT NIGHT"]},
        {"type": "generator", "at": "side_yard", "has_audio": true}
      ],
      "horror_level": 0.6
    }
  ]
}
```

### 10.4 Condemned: Criminal Origins Design Insight

Condemned's developers hired surveyors to photograph real abandoned buildings, condemned structures, and neglected urban areas to template their visual design. This approach of studying real-world abandonment patterns is directly applicable:

- Real abandonment follows predictable patterns: windows break bottom-up, graffiti appears on accessible surfaces first, vegetation follows moisture patterns, structural failure follows load paths
- The "uncanny valley of abandonment" -- too clean looks fake, too destroyed looks cartoonish. The sweet spot is 60-70% decay with specific items appearing untouched (like a child's drawing still on the fridge)

---

## 11. Density Control System

### 11.1 Three-Axis Density Model

Prop density is controlled by three orthogonal axes:

```
PropDensity(location):
  zone_density = ZoneDensity(location.zone_type)     // Suburban vs Urban vs Rural
  decay_density = DecayDensity(location.horror_decay) // Normal vs Abandoned
  story_density = StoryDensity(location.narrative)     // Quiet vs Active scene

  final_density = zone_density * decay_density * story_density
```

### 11.2 Zone Density Profiles

| Zone Type | Vehicles | Street Furniture | Yard Items | Signage | Clutter |
|-----------|----------|-----------------|------------|---------|---------|
| **Suburban** (Leviathan default) | Sparse (1 per lot) | Sparse (lamps + hydrants only) | Moderate (tidy) | Minimal | Low |
| **Urban/Commercial** | Dense (full street parking) | Dense (all categories) | N/A | High | Medium |
| **Rural** | Very sparse | Minimal | Moderate (farm equipment) | Minimal | Low |
| **Industrial** | Moderate (trucks, vans) | Minimal | N/A | Warning signs | High (pallets, drums) |

### 11.3 Horror Decay Density Modifier

```
DecayDensityModifier(horror_decay, prop_category):
  // Normal props DECREASE with decay (people left)
  if prop_category in [VEHICLE_NORMAL, YARD_ITEM_TIDY, SIGNAGE_CLEAN]:
    return 1.0 - horror_decay * 0.7  // 30% remain at full decay

  // Abandoned props INCREASE with decay
  if prop_category in [VEHICLE_ABANDONED, TRASH_SCATTERED, BARRIER]:
    return horror_decay * 0.8

  // Infrastructure stays constant but gets damaged
  if prop_category in [HYDRANT, LAMP, UTILITY]:
    return 1.0  // Always present, damage via variant selection

  // Horror-specific props scale with decay
  if prop_category in [MISSING_POSTER, POLICE_TAPE, QUARANTINE]:
    return horror_decay
```

### 11.4 Per-Block Density Budget

To prevent performance issues, each block has a prop budget:

| Block Size | Max Static Mesh Instances | Max Actors (with logic) | Target Draw Calls |
|-----------|--------------------------|------------------------|-------------------|
| Small (residential, 4-6 lots) | 200-400 | 10-20 | 50-100 |
| Medium (mixed, 8-12 lots) | 400-800 | 20-40 | 100-200 |
| Large (commercial) | 800-1500 | 40-60 | 200-400 |

HISM (Hierarchical Instanced Static Mesh) is critical here -- all identical props should be instanced. A single HISM component for "mailbox_standard" handles all 6 mailboxes on a residential block for 1 draw call instead of 6.

---

## 12. Collision and Clearance

### 12.1 Clearance Rules

Props must not block:

| Zone | Minimum Clear Width | Notes |
|------|-------------------|-------|
| Road travel lane | 300cm | Absolute minimum for single lane |
| Sidewalk pedestrian path | 120cm | ADA minimum, prefer 150cm |
| Building entrance | 150cm radius | Must reach every door |
| Driveway | 300cm wide | Full driveway width |
| Fire hydrant | 300cm radius | Fire code clearance |
| Intersection sight triangle | 450cm from corner | No tall props blocking sight |
| Stairs/ramps | 100cm wide | No props on access paths |

### 12.2 Collision Avoidance Strategy

Three-tier approach:

**Tier 1: Zone-Based Exclusion**
Define exclusion zones where NO props may be placed. These are computed from block geometry before any prop placement:

```
ExclusionZones:
  - Travel lanes (road center to parking edge)
  - Pedestrian clear path (sidewalk minus furniture zone)
  - Driveway footprints (+ 50cm buffer)
  - Building entrance zones (150cm radius from doors)
  - Intersection corners (sight triangle)
  - Fire hydrant clearance circles (300cm radius)
```

**Tier 2: Self-Pruning / Spacing**
Props of the same type maintain minimum spacing. PCG's Self Pruning node handles this natively -- each point's bounds exclude overlapping neighbors.

**Tier 3: Post-Placement Validation**
After all props placed, sweep for violations:
- Capsule trace from each door to street (must be clear)
- NavMesh test if available (all placed props must not sever navmesh connectivity)
- AABB overlap test between all placed props (no interpenetration)

### 12.3 PCG Implementation

```
PCG Collision-Free Placement Graph:

Surface Sampler (sidewalk furniture zone)
  -> Density Filter (by zone type)
  -> Difference (subtract exclusion volumes)
  -> Self Pruning (min radius per prop type)
  -> Attribute Set (assign prop category)
  -> Static Mesh Spawner (weighted per category)
  -> [Output: HISM instances with no collisions]
```

---

## 13. PCG Integration Architecture

### 13.1 Why PCG for Town Dressing

From our PCG framework research, PCG is a **placement and distribution engine** -- exactly what prop scattering needs. It does NOT generate geometry (that's GeometryScript's job), but it excels at:

- Point generation on surfaces with density control
- Spline-based interval placement (street furniture)
- Difference/exclusion (avoid roads, doors)
- Self-pruning (no overlap)
- Weighted random mesh selection via Data Assets
- Deterministic seeded output (reproducible)
- HISM spawning (automatic instancing)
- Hierarchical generation (block-level, then lot-level, then detail-level)

### 13.2 PCG Graph Architecture

Three PCG subgraphs, executed in order:

**Subgraph 1: Street-Level Props** (runs per street segment)
```
Input: Street spline, street type, horror_decay
  -> Spline Sampler (furniture zone offset)
  -> Interval placement (lamps, hydrants, signs)
  -> Difference (subtract driveways, intersections)
  -> Self Pruning (per-type minimum spacing)
  -> Weighted Mesh Spawner (per prop category)
  -> [Output: street furniture HISM]

  Parallel branch:
  -> Parking space computation (interval along curb)
  -> Occupancy roll (per space)
  -> Vehicle Mesh Spawner (weighted by zone)
  -> [Output: vehicle HISM + actor spawns for animated]
```

**Subgraph 2: Lot-Level Props** (runs per lot)
```
Input: Lot polygon, building descriptor, household, horror_decay
  -> Zone subdivision (front yard, side, back, driveway)
  -> Per-zone point generation
  -> Required props (mailbox, trash at fixed positions)
  -> Optional props (yard items, probability-weighted)
  -> Self Pruning (within lot)
  -> Mesh Spawner (per category)
  -> [Output: lot prop HISM]
```

**Subgraph 3: Horror Overlay** (runs per block, after normal props)
```
Input: Block bounds, horror_decay, narrative_points
  -> Horror vignette placement (template spawning at key locations)
  -> Horror prop scatter (missing posters, tape, signs)
  -> Damage overlay (modify existing prop variants)
  -> [Output: horror props actors + modified HISM]
```

### 13.3 Programmatic PCG Graph Construction via MCP

Using the verified UE 5.7 PCG C++ API:

```cpp
// Create graph programmatically
UPCGGraph* Graph = NewObject<UPCGGraph>(Outer);

// Add spline sampler
UPCGNode* SamplerNode = Graph->AddNodeOfType(UPCGSplineSamplerSettings::StaticClass());
auto* SamplerSettings = Cast<UPCGSplineSamplerSettings>(SamplerNode->GetSettings());
SamplerSettings->Mode = EPCGSplineSamplingMode::SubdivisionCount;

// Add self-pruning
UPCGNode* PruneNode = Graph->AddNodeOfType(UPCGSelfPruningSettings::StaticClass());

// Add mesh spawner
UPCGNode* SpawnNode = Graph->AddNodeOfType(UPCGStaticMeshSpawnerSettings::StaticClass());

// Connect: Sampler -> Prune -> Spawn
Graph->AddEdge(SamplerNode, TEXT("Out"), PruneNode, TEXT("In"));
Graph->AddEdge(PruneNode, TEXT("Out"), SpawnNode, TEXT("In"));

// Assign to component and generate
PCGComponent->SetGraph(Graph);
PCGComponent->GenerateLocal(true);
```

### 13.4 Hybrid: Monolith MCP + PCG

The recommended approach is NOT to build all prop placement as PCG graphs. Instead:

| Approach | Use For |
|----------|---------|
| **PCG graphs** (programmatic) | Mass scatter: street lamps, trees, grass, generic clutter |
| **Monolith direct placement** | Precise props: mailboxes (1 per lot at exact location), driveway vehicles, building-attached utilities |
| **PCG + Monolith hybrid** | Horror overlay: PCG scatters missing posters/tape, Monolith places vignette actors |

This matches our earlier PCG research verdict: PCG for scatter, Monolith for precision.

---

## 14. Hierarchical Decoration Pipeline

Based on the Dahl & Rinde thesis on hierarchical procedural decoration of game environments, and Shadows of Doubt's priority-based room furnishing:

### 14.1 Priority-Ordered Placement

Props are placed in strict priority order. Higher-priority items get first pick of positions, lower-priority items work around them:

```
Priority Order (exterior):
  1. Building itself (already placed)
  2. Driveways and walkways (infrastructure)
  3. Fences and property boundaries
  4. Required per-lot props (mailbox, trash, utilities)
  5. Street infrastructure (hydrants, lamps, signs)
  6. Vehicles (driveway first, then street parking)
  7. Optional per-lot props (yard items)
  8. Street amenities (benches, newspaper boxes)
  9. Vegetation (trees, shrubs, grass)
  10. Horror overlay props
  11. Micro-clutter (leaves, trash, debris)
```

### 14.2 Slot-Based vs Scatter-Based

Two placement strategies, used for different prop types:

**Slot-Based** (precise, deterministic):
- Each slot has a fixed position, orientation, and accepted prop types
- Slots defined by lot/building geometry
- Example: mailbox slot at road edge, AC unit slot on building side
- Best for: required props, building-attached items, infrastructure

**Scatter-Based** (probabilistic, density-driven):
- Points distributed by PCG in valid zones
- Filtered by exclusion, self-pruned for spacing
- Weighted random mesh selection
- Best for: clutter, vegetation, repeated items, horror overlay

### 14.3 The "Vignette" System

Between slot-based and scatter-based, there's a middle ground: **vignettes**. Pre-designed arrangements of 3-8 props that tell a micro-story:

```
Vignette examples:
  "porch_evening": rocking_chair + side_table + glass + book + porch_light_on
  "kid_play": bicycle_on_side + chalk_drawing_decal + jump_rope
  "yard_work": lawn_mower_mid_yard + grass_clippings_decal + water_bottle
  "horror_flee": open_door + dropped_grocery_bag + scattered_items + running_shoe
  "horror_fight": overturned_table + broken_glass + blood_decal + weapon
```

Vignettes are placed as units at qualifying positions, then individual props within the vignette are offset from the vignette center per the template.

---

## 15. Data-Driven Prop Library

### 15.1 Prop Definition Format

```json
{
  "prop_id": "mailbox_standard",
  "category": "per_lot_required",
  "placement_context": ["front_yard_road_edge"],
  "mesh_variants": [
    {"asset": "/Game/Props/Mailbox/SM_Mailbox_Standard", "weight": 5, "condition": "normal"},
    {"asset": "/Game/Props/Mailbox/SM_Mailbox_Overflowing", "weight": 3, "condition": "decay > 0.3"},
    {"asset": "/Game/Props/Mailbox/SM_Mailbox_Knocked", "weight": 2, "condition": "decay > 0.6"},
    {"asset": "/Game/Props/Mailbox/SM_Mailbox_Missing", "weight": 1, "condition": "decay > 0.8"}
  ],
  "footprint": [40, 40],
  "height": 120,
  "facing": "toward_street",
  "per_lot": true,
  "max_per_lot": 1,
  "clearance_radius": 50,
  "blocks_nav": false,
  "materials": ["M_Metal_Painted", "M_Wood_Post"],
  "horror_audio": null,
  "tags": ["residential", "street_edge"]
}
```

### 15.2 Prop Library Organization

```
/Game/Props/
  Vehicles/
    SM_Car_Sedan_Clean, SM_Car_Sedan_Dusty, SM_Car_Sedan_Wrecked, ...
    SM_Car_Pickup_Clean, SM_Car_Pickup_Loaded, ...
    SM_Car_Police, SM_Car_Ambulance, SM_Car_Van, ...
  StreetFurniture/
    SM_StreetLamp_Standard, SM_StreetLamp_Broken, SM_StreetLamp_Dark, ...
    SM_Hydrant_Clean, SM_Hydrant_Damaged, ...
    SM_Bench_Wood, SM_Bench_Metal, SM_Bench_Broken, ...
    SM_NewspaperBox, SM_PhoneBooth, SM_BusStop_Shelter, ...
  Residential/
    SM_Mailbox_Standard, SM_Mailbox_Brick, SM_Mailbox_Cluster, ...
    SM_TrashBin_Green, SM_TrashBin_Blue, SM_TrashBin_Overturned, ...
    SM_PatioSet_Standard, SM_BBQGrill, SM_LawnMower, ...
    SM_GardenHose_Coiled, SM_Wheelbarrow, SM_Clothesline, ...
  Commercial/
    SM_Sign_Fascia_*, SM_Sign_Projecting_*, SM_Sign_AFrame, ...
    SM_Dumpster_Clean, SM_Dumpster_Overflowing, SM_Dumpster_Burned, ...
    SM_ACUnit_Roof, SM_ACUnit_Ground, SM_ACUnit_Window, ...
  Barriers/
    SM_TrafficCone, SM_JerseyBarrier, SM_Sawhorse, ...
    SM_ChainLinkFence_Section, SM_Sandbag_Stack, ...
  Horror/
    SM_MissingPoster_A-F, SM_QuarantineSign, SM_BoardedWindow, ...
    SM_BodyBag, SM_HazmatSuit_Discarded, SM_GasMask, ...
    SM_RedX_DoorDecal, SM_SprayPaint_Warning, ...
  Utility/
    SM_UtilityPole, SM_Transformer_Pole, SM_Transformer_Pad, ...
    SM_SatelliteDish, SM_TVAntenna, SM_GasMeter, SM_ElectricMeter, ...
    SM_ManholeC over, SM_StormDrain, ...
  Playground/
    SM_SwingSet, SM_Slide, SM_Sandbox, SM_Seesaw, ...
    SM_SwingSet_Rusty, SM_Slide_Collapsed, ... (horror variants)
```

### 15.3 Data Asset for PCG Integration

PCG's Static Mesh Spawner uses weight-based selection. Define a `UPCGMeshSelectorWeighted` Data Asset per prop category:

```
DA_StreetLamps:
  Entries:
    - SM_StreetLamp_Standard, Weight: 5
    - SM_StreetLamp_Ornate, Weight: 2
    - SM_StreetLamp_Modern, Weight: 1

DA_StreetLamps_Horror:
  Entries:
    - SM_StreetLamp_Broken, Weight: 4
    - SM_StreetLamp_Dark, Weight: 3
    - SM_StreetLamp_Flickering, Weight: 2
    - SM_StreetLamp_Standard, Weight: 1  // Some still work
```

Select between normal/horror Data Assets based on `horror_decay` threshold.

---

## 16. Existing Monolith Integration

### 16.1 Existing Actions That Apply

| Action | Module | Relevance |
|--------|--------|-----------|
| `scatter_on_surface` | MonolithMeshContextPropActions | Floor/ground scatter of small props |
| `scatter_on_walls` | MonolithMeshContextPropActions | Wall-mounted props (signs, meters) |
| `create_horror_prop` | MonolithMeshProceduralActions | Barricades, debris, horror set pieces |
| `create_parametric_mesh` | MonolithMeshProceduralActions | Simple prop geometry (fences, poles) |
| `spawn_volume` | MonolithMeshVolumeActions | Blocking/trigger volumes |
| `place_decal` | MonolithMeshDecalActions | Ground markings, stains, graffiti |
| `auto_volumes_for_building` | MonolithMeshAutoVolumeActions | NavMesh, audio for buildings |
| `create_building_shell` | MonolithMeshProceduralActions | Building geometry (props attach to) |
| `create_city_block` (proposed) | MonolithMeshCityBlockActions | Block subdivision that defines lots |

### 16.2 Gaps / New Actions Needed

| Need | Current Coverage | Gap |
|------|-----------------|-----|
| Vehicle placement | None | New: `place_vehicles` |
| Driveway generation | None | New: `generate_driveway` |
| Street furniture | None (scatter exists but not interval-based) | New: `place_street_furniture` |
| Per-lot prop assignment | None | New: `furnish_lot_exterior` |
| Horror vignette spawning | `create_horror_prop` covers individual items | New: `place_horror_vignette` |
| Commercial signage | None | New: part of `furnish_lot_exterior` |
| Playground | None | New: `generate_park` |
| Barrier scenario | `create_horror_prop` partial | New: `place_barrier_scenario` |
| Prop library management | None | New: `register_prop_library` / `list_prop_library` |
| Full block dressing | None (piecemeal only) | New: `dress_city_block` (orchestrator) |

---

## 17. Proposed MCP Actions

### 17.1 Core Actions (8 new)

**`dress_city_block`** -- Master orchestrator
```json
{
  "action": "dress_city_block",
  "params": {
    "block_id": "string (from spatial registry)",
    "horror_decay": 0.5,
    "zone_type": "suburban",
    "prop_library": "/Game/Props/PropLibrary_Suburban",
    "vehicle_density": 0.6,
    "include_horror_vignettes": true,
    "seed": 42
  }
}
```
Calls sub-actions in priority order: driveways -> infrastructure -> per-lot -> street -> vehicles -> horror.

**`place_vehicles`** -- Vehicle placement
```json
{
  "action": "place_vehicles",
  "params": {
    "block_id": "string",
    "parking_type": "parallel|perpendicular|angled|driveway",
    "occupancy": 0.6,
    "horror_decay": 0.5,
    "vehicle_weights": {"sedan": 5, "pickup": 3, "suv": 2},
    "seed": 42
  }
}
```

**`generate_driveway`** -- Per-lot driveway
```json
{
  "action": "generate_driveway",
  "params": {
    "lot_id": "string",
    "type": "single|double|carport",
    "material": "concrete|asphalt|gravel",
    "horror_decay": 0.5
  }
}
```

**`place_street_furniture`** -- Interval-based along street spline
```json
{
  "action": "place_street_furniture",
  "params": {
    "street_spline_actor": "string (actor name)",
    "street_type": "residential|commercial|mixed",
    "horror_decay": 0.5,
    "include": ["lamps", "hydrants", "signs", "benches", "trash_cans"],
    "seed": 42
  }
}
```

**`furnish_lot_exterior`** -- All per-lot props
```json
{
  "action": "furnish_lot_exterior",
  "params": {
    "lot_id": "string",
    "building_type": "house_small|house_large|commercial|apartment",
    "wealth": 0.5,
    "horror_decay": 0.5,
    "household": {"has_children": true, "occupation": "blue_collar"},
    "seed": 42
  }
}
```

**`place_horror_vignette`** -- Spawn a horror scene template
```json
{
  "action": "place_horror_vignette",
  "params": {
    "template": "abandoned_evacuation|police_incident|barricaded_house|military_checkpoint",
    "location": [0, 0, 0],
    "rotation": 0,
    "horror_decay": 0.7,
    "seed": 42
  }
}
```

**`place_barrier_scenario`** -- Gameplay-blocking barrier setups
```json
{
  "action": "place_barrier_scenario",
  "params": {
    "type": "road_block|quarantine|construction|evacuation|military",
    "location": [0, 0, 0],
    "width": 2000,
    "blocks_vehicle": true,
    "blocks_foot": false,
    "horror_decay": 0.5
  }
}
```

**`generate_park`** -- Park/playground generation
```json
{
  "action": "generate_park",
  "params": {
    "lot_id": "string",
    "include_playground": true,
    "include_paths": true,
    "horror_decay": 0.5,
    "seed": 42
  }
}
```

### 17.2 Support Actions (4 new)

**`register_prop_library`** -- Register a set of prop meshes with metadata
```json
{
  "action": "register_prop_library",
  "params": {
    "name": "suburban_residential",
    "props": [
      {"id": "mailbox", "meshes": ["/Game/Props/SM_Mailbox"], "category": "per_lot", ...}
    ]
  }
}
```

**`list_prop_libraries`** -- Show registered libraries

**`set_horror_decay`** -- Apply horror decay to an already-dressed block
```json
{
  "action": "set_horror_decay",
  "params": {
    "block_id": "string",
    "horror_decay": 0.8,
    "regenerate": true
  }
}
```

**`validate_block_clearance`** -- Check all props respect clearance rules
```json
{
  "action": "validate_block_clearance",
  "params": {
    "block_id": "string",
    "check_nav": true,
    "check_doors": true,
    "check_roads": true
  }
}
```

---

## 18. Effort Estimate

### Phase 1: Foundation (24-32h)
- Prop library data format + JSON schema
- `register_prop_library` / `list_prop_libraries`
- Lot exterior zone computation (front/side/back/driveway zones from lot geometry)
- Exclusion zone computation from block geometry
- Integration with spatial registry

### Phase 2: Street-Level (20-28h)
- `place_street_furniture` (spline-interval + exclusion + self-pruning)
- `place_vehicles` (parallel/perpendicular parking, driveway)
- `generate_driveway` (geometry + terrain cut)
- Street sign placement at intersections

### Phase 3: Lot-Level (20-28h)
- `furnish_lot_exterior` (per-lot required + optional props)
- Commercial signage system
- Utility infrastructure (meters, AC, dishes)
- Yard items by household profile

### Phase 4: Horror and Vignettes (16-24h)
- `place_horror_vignette` (template system)
- `place_barrier_scenario` (road blocks, quarantine zones)
- Horror decay variant selection
- Horror prop density curve
- Environmental storytelling micro-scenes

### Phase 5: Parks and Special (12-16h)
- `generate_park` (playground equipment, paths, benches)
- Parking lot generation (commercial)
- Horror playground variants

### Phase 6: Orchestration and Validation (12-16h)
- `dress_city_block` (master orchestrator)
- `set_horror_decay` (re-dress with new decay)
- `validate_block_clearance` (collision/nav checking)
- Performance budgeting (HISM enforcement, draw call limits)

**Total: ~104-144h (13-18 working days)**

### Dependencies
- Spatial registry operational (lot/block/building IDs)
- City block layout generating lots with setbacks
- Building descriptors with door/window positions
- Auto-collision working for proc gen meshes
- Prop mesh assets (can use placeholders initially)

---

## 19. Sources

### UE5 PCG Framework
- [PCG Development Guides (UE 5.7 Official)](https://dev.epicgames.com/documentation/en-us/unreal-engine/pcg-development-guides)
- [PCG Framework Node Reference (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG City Streets Tutorial](https://dev.epicgames.com/community/learning/tutorials/VxP9/unreal-engine-you-won-t-believe-how-easy-city-streets-can-be-in-ue5-using-pcg)
- [Procedural Road Generation in UE5](https://dev.epicgames.com/community/learning/tutorials/9dpd/procedural-road-generation-in-unreal-engine-5-pcg)
- [PCG Scatter Without Collision Tutorial](https://dev.epicgames.com/community/learning/tutorials/7BMa/unreal-engine-procedurally-scatter-items-without-collision-using-pcg-in-ue-5-2)
- [A Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [PCGEx Extended Toolkit (MIT, GitHub)](https://github.com/PCGEx/PCGExtendedToolkit)

### Procedural City Generation
- [Jean-Paul Software: First We Make Manhattan (PCG city, 2025)](https://jeanpaulsoftware.com/2025/02/25/first-we-make-manhattan-then-we-make-berlin/)
- [Shadows of Doubt DevBlog 13: Procedural Interiors](https://colepowered.com/shadows-of-doubt-devblog-13-creating-procedural-interiors/)
- [Shadows of Doubt DevBlog 15: Moving in Citizens](https://colepowered.itch.io/shadows/devlog/78044/shadows-of-doubt-devblog-15-moving-in-the-citizens)
- [Martin Devans: Procedural Generation For Dummies](https://martindevans.me/game-development/2015/12/11/Procedural-Generation-For-Dummies/)
- [Procedural City Generator (Fab)](https://www.fab.com/listings/09667242-f2f9-4d3a-b439-b762b142f9d2)

### Vehicle and Parking
- [Procedural Parking Lot Generator in UE5 (80.lv)](https://80.lv/articles/procedural-parking-lot-generator-in-unreal-engine-5)
- [ArtStation: Procedural Parking Lot (GeometryScript + Spline)](https://www.artstation.com/artwork/vD829E)
- [On-Street Parking Design (VTA)](https://www.vta.org/cdt/parking-design-home-page/street-parking-design)
- [Parking Space Dimensions (Wikipedia)](https://en.wikipedia.org/wiki/Parking_space)

### Environmental Storytelling
- [Environmental Storytelling in Video Games (GameDesignSkills)](https://gamedesignskills.com/game-design/environmental-storytelling/)
- [What You Give Is What You Get: Environmental Storytelling (Gamedeveloper.com)](https://www.gamedeveloper.com/design/what-you-give-is-what-you-get-environmental-storytelling-in-games)
- [Environmental Storytelling: Theme Park Lessons (Don Carson)](https://www.gamedeveloper.com/design/environmental-storytelling-creating-immersive-3d-worlds-using-lessons-learned-from-the-theme-park-industry)
- [Condemned: Criminal Origins (abandoned building reference photography)](https://en.wikipedia.org/wiki/Condemned:_Criminal_Origins)

### Academic / Thesis
- [Hierarchical Procedural Decoration of Game Environments (Dahl & Rinde, Chalmers)](https://www.diva-portal.org/smash/get/diva2:1479952/FULLTEXT01.pdf)
- [Procedural Generation of Indoor Environments (Chalmers thesis)](https://www.cse.chalmers.se/~uffe/xjobb/Lars%20Rinde%20o%20Alexander%20Dahl-Procedural%20Generation%20of%20Indoor%20Environments.pdf)
- [Rule-Based Layout Solving for Procedural Interiors (ResearchGate)](https://www.researchgate.net/publication/228922424_Rule-based_layout_solving_and_its_application_to_procedural_interior_generation)
- [Endless City Driver: Realistic Populated Virtual 3D City (Springer)](https://link.springer.com/chapter/10.1007/978-3-030-37869-1_15)

### Municipal Standards (Real-World Reference)
- [NFPA 24: Fire Hydrant Spacing](https://www.nfpa.org/)
- [USPS Mailbox Installation Requirements](https://www.usps.com/)
- [ADA Sidewalk Width Requirements](https://www.ada.gov/)
- [On-Street Angled Parking Specs (Albuquerque DPM)](https://documents.cabq.gov/planning/development-process-manual/DPM-Chapter23-Sec3-8-Proposed.pdf)

### Existing Monolith Research (Internal)
- `2026-03-28-city-block-layout-research.md` -- Lot subdivision, street geometry, setbacks
- `2026-03-28-pcg-framework-research.md` -- PCG API, nodes, programmatic graph construction
- `2026-03-28-spatial-registry-research.md` -- Block/building/room ID hierarchy
- `2026-03-28-auto-volumes-research.md` -- NavMesh/audio/PP volume auto-generation
- `2026-03-28-facade-window-research.md` -- Building facade for sign attachment
- `2026-03-28-attachment-logic-research.md` -- Exterior feature attachment system
