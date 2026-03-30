# Real Architectural Floor Plan Patterns for Procedural Building Generation

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Affects:** `MonolithMeshFloorPlanGenerator`, Building Archetype JSONs, `create_building` action
**Companion:** `2026-03-28-realistic-room-sizes-research.md` (room dimensions & grid cells)

## Table of Contents

1. [Why Proc-Gen Buildings Feel Wrong](#1-why-proc-gen-buildings-feel-wrong)
2. [Universal Architectural Principles](#2-universal-architectural-principles)
3. [Circulation Pattern Taxonomy](#3-circulation-pattern-taxonomy)
4. [Room Adjacency Rules (The Connectivity Graph)](#4-room-adjacency-rules)
5. [Room Proportions by Function](#5-room-proportions-by-function)
6. [Building Type: Residential House](#6-residential-house)
7. [Building Type: Two-Story House](#7-two-story-house)
8. [Building Type: Victorian Mansion](#8-victorian-mansion)
9. [Building Type: Apartment Building](#9-apartment-building)
10. [Building Type: Office Building](#10-office-building)
11. [Building Type: Hospital / Clinic](#11-hospital--clinic)
12. [Building Type: Police Station](#12-police-station)
13. [Building Type: School](#13-school)
14. [Building Type: Church / Chapel](#14-church--chapel)
15. [Building Type: Restaurant / Bar](#15-restaurant--bar)
16. [Building Type: Warehouse](#16-warehouse)
17. [Building Type: Retail Store](#17-retail-store)
18. [Horror Subversion Patterns](#18-horror-subversion-patterns)
19. [Horror Game Case Studies](#19-horror-game-case-studies)
20. [Open Floor Plan Datasets](#20-open-floor-plan-datasets)
21. [Implementation Recommendations](#21-implementation-recommendations)
22. [Sources](#22-sources)

---

## 1. Why Proc-Gen Buildings Feel Wrong

Research and community discussion consistently identify these tells that mark a building as procedurally generated rather than architect-designed:

### The Dead Giveaways

1. **Corridor waste** -- Real architects minimize hallway area. Proc-gen often wastes 30-40% of floor area on corridors that go nowhere. Real buildings keep corridors under 15-20% of total area.

2. **No public-to-private gradient** -- Real buildings transition from public entry spaces through semi-public gathering areas to fully private bedrooms/offices. Proc-gen just puts rooms anywhere.

3. **Doors in wrong places** -- Real doors are placed to preserve wall space for furniture. A bedroom door goes to one side, not dead center. Kitchen doors avoid the work triangle. Proc-gen places doors at arbitrary wall positions.

4. **Rooms accessible only through other rooms** -- In real residential design, you almost never access a bedroom through another bedroom. Every bedroom must reach a bathroom without passing through another bedroom (IRC code requirement). Proc-gen creates "captive rooms" constantly.

5. **No plumbing logic** -- Real bathrooms cluster on shared "wet walls" to minimize plumbing runs. Kitchens back up to bathrooms. Proc-gen scatters wet rooms randomly.

6. **Wrong room proportions** -- Kitchens are wider than deep (counter space). Hallways are narrow and long. Bedrooms are roughly square. Living rooms are slightly rectangular. Proc-gen makes all rooms similar rectangles.

7. **No structural grid** -- Real buildings align to a structural bay system. Walls don't just go anywhere; they follow a grid. Residential: 3-5m bays. Commercial: 6-9m bays. Proc-gen has no underlying grid.

8. **Windows on interior walls** -- Windows can only go on exterior walls. This sounds obvious but many generators place them randomly or forget to place them at all.

9. **No entry sequence** -- Real buildings have a deliberate transition: exterior -> porch/stoop -> threshold -> vestibule/foyer -> circulation -> rooms. Proc-gen drops you straight into a corridor.

10. **Repetitive/homogeneous** -- Every floor looks the same. Every room feels the same size. Real buildings have spatial variety -- a double-height living room next to a cozy bedroom, a narrow hallway opening into a grand entry.

---

## 2. Universal Architectural Principles

These principles apply across ALL building types and are the single most important thing to encode in the generator.

### 2.1 The Public-to-Private Gradient

Every building organizes spaces along a spectrum from fully public to fully private:

```
EXTERIOR
  |
  v
PUBLIC ZONE:     entry, lobby, reception, waiting
  |
  v
SEMI-PUBLIC:     living room, dining room, conference room, common areas
  |
  v
SEMI-PRIVATE:    hallway, corridor (transition/circulation)
  |
  v
PRIVATE ZONE:    bedrooms, bathrooms, offices, cells, storage
  |
  v
SERVICE ZONE:    mechanical, utility, kitchen, laundry (servant spaces)
```

In residential architecture specifically:
- **Public**: entry, foyer, living room, dining room
- **Semi-private**: kitchen (used by family but visible to guests), family room
- **Private**: bedrooms, master bath, study
- **Service**: laundry, utility, garage, storage

### 2.2 Served vs. Servant Spaces (Louis Kahn)

Louis Kahn's fundamental distinction: spaces are either "served" (inhabited, the purpose of the building) or "servant" (supporting the served spaces):

- **Served**: living room, bedroom, dining room, office, classroom, nave
- **Servant**: kitchen, bathroom, hallway, closet, mechanical room, stairwell, laundry

Key rules for servant spaces:
- They cluster together (shared plumbing walls, shared HVAC ducts)
- They get pushed to edges or grouped into cores
- They are smaller and less prominently placed than served spaces
- Kitchens and bathrooms share walls (the "wet wall" principle)

### 2.3 Entry Sequence

Every building has a deliberate transition from outside to inside:

```
Exterior -> Threshold (porch/stoop/canopy) -> Buffer (vestibule/airlock) ->
Reception (foyer/lobby) -> Distribution (hall/corridor) -> Destination (rooms)
```

- **Residential**: porch -> front door -> foyer/entry hall -> hallway -> rooms
- **Commercial**: sidewalk -> vestibule -> lobby/reception -> elevator/corridor -> offices
- **Institutional**: approach -> main entrance -> atrium/lobby -> corridor -> departments
- **Religious**: path -> narthex -> nave -> chancel/sanctuary

The entry should NEVER open directly into a private space (bedroom, bathroom) or a service space (kitchen, utility). It should open into the most public space in the gradient.

### 2.4 Circulation Must Not Bisect Rooms

A critical rule: public circulation paths (how you get from A to B) must not cut through the middle of a room's usable area. Traffic should flow along room edges, through dedicated hallways, or through transition zones. When a living room serves as both a gathering space AND a through-way, furniture must be arranged so the traffic path runs along one side, not diagonally through the seating area.

### 2.5 Structural Bay System

Real buildings are built on a structural grid:

| Building Type | Typical Bay Spacing | Notes |
|---------------|-------------------|-------|
| Residential (wood frame) | 3.0-4.5m (10-15') | Dictated by joist/rafter span |
| Residential (masonry) | 3.0-5.0m (10-16') | Load-bearing walls |
| Small commercial | 6.0-7.5m (20-25') | Steel or concrete frame |
| Office building | 7.5-9.0m (25-30') | Open-plan optimization |
| Industrial/warehouse | 9.0-15.0m (30-50') | Long-span steel |
| Parking structure | 8.0-9.0m (26-30') | Car spacing drives it |

Room dimensions should snap to multiples of the bay spacing. Interior walls typically fall on grid lines or at half-bay positions.

### 2.6 The Loop Principle

In good residential design, the primary living spaces form a LOOP rather than a tree. You should be able to walk from the kitchen to the dining room to the living room to the entry and back to the kitchen without retracing your steps. This loop:
- Enables flow during entertaining
- Provides multiple routes (avoiding "dead-end-rooms")
- Creates better natural light and air circulation
- Feels more spacious than a tree layout

Exceptions: bedrooms and bathrooms are INTENTIONALLY dead-ends for privacy.

### 2.7 Fenestration Rhythm

Windows follow structural bays:
- Window spacing equals or exceeds window width (minimum 1:1 solid-to-void ratio)
- Ideal: window width = 0.5 to 0.75 of bay width
- Windows stack vertically across floors (same column above and below)
- Residential: 3, 5, or 7 bays across the facade (odd numbers for symmetry)
- The space between windows should be at least the window width (shutter clearance) but not more than 1.5x window width

---

## 3. Circulation Pattern Taxonomy

Every building uses one or more of these circulation patterns:

### 3.1 Hub-and-Spoke

```
        [Room]
          |
[Room]--[HUB]--[Room]
          |
        [Room]
```

- **How it works**: A central space (foyer, lobby, hall) with rooms radiating off it
- **Where used**: Small houses, mansions (main hall), museums, horror games (Spencer Mansion)
- **Feel**: Orientation is easy (always return to center). Can feel exposed in the hub.
- **Horror value**: HIGH -- player must cross exposed hub repeatedly; threats can appear from any spoke
- **For proc-gen**: Easy to implement. Hub is the entry/foyer. Spokes are rooms or wing corridors.

### 3.2 Double-Loaded Corridor

```
[Room][Room][Room][Room]
---------CORRIDOR--------
[Room][Room][Room][Room]
```

- **How it works**: A straight corridor with rooms on both sides
- **Where used**: Hotels, apartments, hospitals, schools, office buildings, dormitories
- **Feel**: Efficient but can feel institutional/oppressive when long
- **Horror value**: VERY HIGH -- long sightlines, many doors that could open, nowhere to hide
- **For proc-gen**: The most common institutional pattern. Building width = 12-18m (40-60'). Corridor down the center, rooms 4-6m deep on each side.

### 3.3 Single-Loaded Corridor

```
[Room][Room][Room][Room]
---------CORRIDOR--------
         (exterior wall / windows)
```

- **How it works**: Rooms on one side, windows/exterior on the other
- **Where used**: Luxury apartments, European housing, hospital wards (for light/views), schools
- **Feel**: More generous, more natural light, but uses more floor area
- **Horror value**: MODERATE -- exterior windows provide orientation but also vulnerability
- **For proc-gen**: Building width = 8-10m. Good for hospital wards and luxury buildings.

### 3.4 Racetrack / Loop

```
[Room][Room][Room]
  |              |
[Corr]        [Corr]
  |              |
[Room][Room][Room]
```

- **How it works**: Corridor forms a continuous loop, rooms on outside or inside of loop
- **Where used**: Large office floors (around a central core), department stores, hospitals
- **Feel**: Easy wayfinding, always moving forward, never trapped
- **Horror value**: MODERATE -- predictable but player can be chased in circles; threats from ahead AND behind
- **For proc-gen**: Works for larger buildings. Central core contains stairs, elevators, restrooms.

### 3.5 Enfilade (Room-Through-Room)

```
[Room]--[Room]--[Room]--[Room]--[Room]
```

- **How it works**: Rooms connect directly through aligned doorways, no corridor
- **Where used**: Historic palaces, mansions, art galleries, row houses
- **Feel**: Grand, ceremonial, processional. Each room is a discovery.
- **Horror value**: HIGH -- forces player through rooms sequentially; can't skip ahead; doors create choke points
- **For proc-gen**: Align doors on a single axis for the full effect. Historically, doors are on the wall nearest the exterior (window wall opposite).

### 3.6 Grid / Open Plan

```
[--|--|--]
[--|--|--]
[--|--|--]
```

- **How it works**: Open floor with a regular structural grid; partitions create spaces
- **Where used**: Modern offices, retail, warehouses, factory floors
- **Feel**: Flexible, visible, navigable but can be disorienting without landmarks
- **Horror value**: LOW-MODERATE in open state, HIGH when cluttered (warehouse with shelving rows)
- **For proc-gen**: Good for commercial/industrial. Place structural columns on grid, partition as needed.

### 3.7 Tree / Branching

```
[Entry]--[Hall]--[Branch]--[Room]
                    |
                  [Room]
                    |
                  [Room]
```

- **How it works**: Hierarchical branches from main to secondary to tertiary circulation
- **Where used**: Modern houses, apartment complexes, suburban developments
- **Feel**: Clear hierarchy, good privacy, but dead ends are common
- **Horror value**: HIGH for dead ends (player gets trapped), LOW for navigation (clear retreat path)
- **For proc-gen**: Natural fit for residential. Entry -> main hall -> bedroom wing hallway -> individual rooms.

---

## 4. Room Adjacency Rules

These are the connectivity rules that make buildings feel real. Encoded as an adjacency matrix, they define which rooms MUST connect, SHOULD connect, MAY connect, and MUST NOT connect.

### 4.1 Universal Adjacency Rules

**MUST be adjacent / directly connected:**
- Entry/foyer <-> primary living space (living room or lobby)
- Kitchen <-> dining room (or combined open plan)
- Bedroom <-> bathroom (accessible without passing through another bedroom)
- Corridor <-> every room it serves
- Stairwell <-> corridor on each floor it connects

**SHOULD be adjacent (preferred):**
- Kitchen <-> utility/laundry (shared plumbing wall)
- Kitchen <-> pantry
- Garage <-> kitchen or mudroom (grocery path)
- Master bedroom <-> master bathroom (en suite)
- Entry <-> coat closet
- Office/study <-> living area (work from home)

**MAY be adjacent (optional):**
- Living room <-> dining room (open plan)
- Kitchen <-> living room (open plan)
- Bedroom <-> walk-in closet
- Bathroom <-> laundry (shared plumbing)

**MUST NOT be adjacent / directly connected:**
- Bathroom <-> kitchen (direct opening forbidden by most codes)
- Bedroom <-> bedroom (no pass-through bedrooms -- each must have independent corridor access)
- Bedroom <-> kitchen (not directly connected in the hierarchy)
- Entry <-> bedroom (privacy violation)
- Entry <-> bathroom (privacy violation)
- Mechanical room <-> bedroom (noise)

### 4.2 The Hierarchy Tree

The procedural generation paper by Lopes et al. (2010) establishes this hierarchy:

```
Outside
  |
  +-- Entry / Foyer
        |
        +-- Living Room (hub)
        |     |
        |     +-- Kitchen
        |     |     |
        |     |     +-- Pantry
        |     |     +-- Laundry
        |     |     +-- Dining Room (optional: may connect direct to Living)
        |     |
        |     +-- Hallway / Corridor
        |           |
        |           +-- Bedroom 1 (Master)
        |           |     +-- En Suite Bathroom
        |           |     +-- Walk-in Closet
        |           |
        |           +-- Bedroom 2
        |           +-- Bedroom 3
        |           +-- Hall Bathroom (shared)
        |
        +-- Garage / Mudroom
        +-- Guest Bathroom / Powder Room
```

Optional edges are then added: kitchen<->dining, dining<->living (creating the loop).

### 4.3 The Wet Wall Principle

Bathrooms and kitchens cluster along shared plumbing walls:

```
     Plumbing Wall
          |
[Kitchen] | [Bathroom]
          |
   or     |
          |
[Bath 1]  | [Bath 2]   (back-to-back bathrooms)
          |
```

Rules:
- Place all wet rooms (kitchen, bathrooms, laundry) within 1-2 rooms of a common plumbing chase
- In multi-story buildings, stack wet rooms vertically (bathroom above bathroom, kitchen above kitchen)
- In apartments, back-to-back units share a plumbing wall between their bathrooms/kitchens

### 4.4 Commercial Building Adjacency

For offices, hospitals, schools, and institutional buildings:

**Core elements that cluster together:**
- Stairs + elevators + restrooms + mechanical shaft = THE CORE
- The core goes in the CENTER of the floor plate (most efficient)
- Everything else wraps around the core

**Commercial hierarchy:**
```
Exterior Entry
  |
  +-- Lobby / Reception
        |
        +-- Elevator Lobby
        |     |
        |     +-- CORE: Stairs, Restrooms, Mech Shaft
        |
        +-- Main Corridor (loop around core)
              |
              +-- Offices (perimeter = daylight)
              +-- Conference Rooms
              +-- Open Plan Areas
              +-- Break Room / Kitchen
```

---

## 5. Room Proportions by Function

This table captures the characteristic WIDTH:DEPTH ratio that makes each room type "feel right." These are distinct from dimensions (covered in the companion room sizes research).

| Room Type | Typical Aspect Ratio | Shape Character | Why |
|-----------|---------------------|-----------------|-----|
| Living room | 1:1.2 to 1:1.5 | Slightly rectangular | Furniture groupings need width; depth for TV distance |
| Kitchen (galley) | 1:2.0 to 1:2.5 | Long and narrow | Counter runs along both walls |
| Kitchen (open) | 1:1.0 to 1:1.3 | Nearly square | Island in center, movement all around |
| Dining room | 1:1.0 to 1:1.4 | Nearly square to slightly rectangular | Table centered, chairs all around |
| Master bedroom | 1:1.0 to 1:1.3 | Nearly square | Bed centered, walk space on 3 sides |
| Secondary bedroom | 1:1.0 to 1:1.2 | Nearly square | Bed against wall, desk, closet |
| Full bathroom | 1:1.5 to 1:2.0 | Rectangular | Fixtures line up along one wall |
| Half bath (powder room) | 1:1.0 to 1:1.5 | Small square or narrow | Toilet + sink side by side |
| Hallway | 1:3.0 to 1:10.0 | Very long, narrow | Pure circulation; 0.9-1.2m wide |
| Foyer/entry | 1:1.0 to 1:1.5 | Square-ish | Transitional; coat closet, shoe space |
| Home office / study | 1:1.0 to 1:1.3 | Nearly square | Desk + bookshelf arrangement |
| Walk-in closet | 1:1.5 to 1:2.0 | Rectangular | Rods and shelves on long walls |
| Laundry | 1:1.5 to 1:2.0 | Rectangular | Machines side by side against wall |
| Garage (1 car) | 1:2.0 | Long rectangular | Car-shaped |
| Garage (2 car) | 1:1.3 | Wide rectangular | Two car widths |

### Aspect Ratio Constraint for Proc-Gen

From the Lopes et al. algorithm: **constrain aspect ratio to 1.0-2.0** (max 2.5 for hallways/kitchens). Any room with aspect ratio > 2.5 looks wrong. Any room with aspect ratio < 1.0 is just rotated.

For the 50cm grid system: after placing a room, validate `max(W,H) / min(W,H) <= 2.0` for habitable rooms, `<= 4.0` for corridors.

---

## 6. Residential House

### Canonical Patterns

**Pattern: Ranch (Single-Story, 3-Bed)**
```
Entry sequence: Front porch -> Entry door -> Foyer/Entry hall

Circulation type: Modified hub (foyer as hub) + bedroom wing corridor
Overall shape: Rectangular or L-shaped
Total area: 1,300-1,800 sqft (120-170 m2)

Layout flow:
  [Garage]--[Mudroom]--[Kitchen]--[Dining]
                           |          |
                        [Pantry]   [Living Room]--[Front Entry]
                                      |
                                   [Hallway]
                                   /   |   \
                            [Bed1] [Bath] [Bed2]
                              |
                           [Master Bath]
```

Key rules:
- Front door opens to foyer or directly to living room (never kitchen, never bedroom)
- Living room is the FIRST room visitors see
- Kitchen-dining-living forms the "great room" loop (especially in open plan)
- Bedrooms cluster in a "wing" off a short hallway
- Master bedroom at the END of the wing (most private position)
- Hall bathroom between the secondary bedrooms
- Garage connects to kitchen via mudroom (grocery drop-off path)
- Laundry near bedrooms OR near kitchen (two common schools)

**Pattern: L-Shaped Ranch**
```
           [Bedroom Wing]
                |
[Living/Kitchen]---[Entry]
```

The L-shape separates public and private zones. The public wing (living, kitchen, dining) forms one arm. The private wing (bedrooms, bathrooms) forms the other. The entry/foyer sits at the junction.

Advantages for horror:
- Long bedroom wing corridor
- The junction point is a natural surveillance/ambush position

**Pattern: Colonial / Center-Hall**
```
[Living]  [Entry/Hall]  [Dining]
    |          |            |
    +-------[Hallway]-------+
                |
            [Kitchen]--[Family Room]
```

A symmetrical plan with the entry hall in the dead center. Living room on one side, dining room on the other. Kitchen and family room in the back.

### What Makes It Feel Real

1. The entry connects to the most public room, not a corridor
2. Kitchen, dining, living form a loop or L (not dead ends)
3. Bedrooms are all on the same side/wing (not scattered)
4. At least one bathroom is reachable from public spaces without entering private zones (powder room)
5. Exterior walls have windows; interior walls don't
6. Room sizes vary -- living room is biggest, bathrooms are smallest
7. Hallway to bedrooms is short (under 6m / 20') -- just enough to serve 2-3 doors

---

## 7. Two-Story House

### Canonical Pattern

```
GROUND FLOOR:
  [Garage]--[Mudroom]--[Kitchen]--[Pantry]
                           |
                    [Dining Room]
                           |
                    [Living Room]--[Front Entry/Foyer]
                           |               |
                    [Family Room]    [STAIRCASE]--[Powder Room]
                                         |
                                    [Coat Closet]

UPPER FLOOR:
                    [STAIRCASE/Landing]
                           |
                    [Upper Hallway]
                    /    |    |    \
              [Master] [Bed2] [Bed3] [Laundry]
                |               |
          [Master Bath]    [Hall Bath]
                |
          [Walk-in Closet]
```

Key rules:
- Staircase near center of plan, accessible from foyer
- All bedrooms upstairs (separation of public/private by FLOOR)
- Upper hallway is short -- stairs at center means rooms can "pinwheel" off the landing
- Stack wet rooms vertically: powder room below, hall bath above
- Staircase foot is visible from the front entry (welcoming gesture)
- Master bedroom gets the most distant position from stairs (most private)
- Corner stairs create dead zones; central stairs are better

### Stair Placement Rules

- Place stairs where they serve both floors efficiently (near center)
- Minimum stair width: 0.9m (36"); comfortable: 1.1m (42")
- Stairs on exterior walls can have a window at the landing (natural light)
- The stair footprint on the upper floor determines hallway layout
- Real staircases need 3.0-4.0m of linear run for a standard flight (2.7m floor height)
- Switchback (U-shaped) stairs fit in approximately 2.0m x 3.5m footprint

---

## 8. Victorian Mansion

### Canonical Pattern

This is the most horror-relevant residential type (Spencer Mansion, Baker House).

```
GROUND FLOOR:
                    [Tower/Turret]
                          |
  [East Wing]--[Grand Hall/Foyer]--[West Wing]
       |              |                 |
  [Library]    [Grand Staircase]   [Drawing Room]
       |              |                 |
  [Study]      [Dining Room]       [Parlor/Music Room]
                      |
               [Butler's Pantry]--[Kitchen]
                                       |
                                [Service Stair]--[Servants' Hall]
                                                      |
                                                [Servants' Quarters]

UPPER FLOOR:
  [East Wing Bedrooms]--[Gallery/Landing]--[West Wing Bedrooms]
       |                       |                    |
  [Bed 1 + Bath]      [Grand Staircase]     [Bed 2 + Bath]
  [Bed 3 + Bath]              |              [Master Suite]
                        [Service Stair]
```

Key rules:
- Symmetrical facade but asymmetrical interior (wings have different functions)
- Grand central hallway with monumental staircase
- DUAL circulation: main stairs (family/guests) and service stairs (servants)
- Wings separate functions: library/study wing vs. parlor/music wing
- Kitchen is far from entry (servants prepare, butler serves)
- Service corridor runs behind main rooms (hidden circulation)
- Tower/turret provides vertical circulation or a special room
- Upper floor gallery overlooks the grand hall (double-height space)

### Horror Application

Victorian mansions are the gold standard for horror buildings because:
- The dual circulation (main + service stairs) means the player can be flanked
- Long wings with multiple rooms create committed exploration paths
- The service corridor is a "secret" parallel path for enemies
- Double-height spaces (grand hall, gallery) create vertical sight lines
- Asymmetric wings mean the player can't predict the layout
- Towers and turrets create vertical dead-end traps

---

## 9. Apartment Building

### Canonical Patterns

**Pattern: Double-Loaded Corridor**
```
[Unit A1][Unit A2][Unit A3][Unit A4]
-----------CORRIDOR-----------[CORE: Stairs/Elev/Trash]
[Unit B1][Unit B2][Unit B3][Unit B4]
```

- Building width: 12-18m (40-60')
- Corridor width: 1.5-1.8m (5-6')
- Unit depth: 5-7m (16-23') per side
- Each unit: living/kitchen facing exterior, bedrooms interior or exterior
- Core (stairs + elevator + trash chute + mechanical) at corridor end or center
- Most common in US mid-rise construction

**Pattern: Point Access Block (European)**
```
        [Unit A]
           |
[Unit D]--[CORE]--[Unit B]
           |
        [Unit C]
```

- A single stair/elevator core serves 2-6 units per floor
- No corridor at all (units open directly off the stair landing)
- Much more common in Europe
- Higher quality: every unit gets corner windows, cross-ventilation

**Pattern: Single-Loaded Corridor (Gallery Access)**
```
[Unit][Unit][Unit][Unit]
---------CORRIDOR---------
    (exterior / open to air)
```

- Corridor is exterior (balcony-like gallery)
- All units have through-ventilation
- Common in tropical climates and European social housing
- Less common in US

### Within Each Unit (Typical 2BR Apartment)
```
[Entry]--[Living/Dining]--[Kitchen]
              |
         [Hallway]
          /      \
    [Bedroom 1] [Bedroom 2]
         |           |
    [Closet]    [Hall Bath]
         |
    [En Suite]
```

Key rules for apartments:
- Entry opens to living area, not bedroom
- Kitchen near entry (shorter plumbing runs, easier grocery delivery)
- Bedrooms at the back (furthest from corridor noise)
- Bathrooms cluster on shared plumbing wall
- Living room gets the best exterior wall (largest windows)

### Horror Application

Apartment buildings work for horror because:
- Long double-loaded corridors with many identical doors (uncanny)
- Repetitive unit layouts create disorientation across floors
- The corridor is a gauntlet with no cover
- Stairwells are vertical traps (dark, echoing, enclosed)
- Individual units are dead-end spaces (only one way out to corridor)
- Sounds through thin walls (neighbor sounds = ambiguity about threat source)

---

## 10. Office Building

### Canonical Pattern: Core-and-Shell

```
        [Perimeter Offices]
        [     ...          ]
[Open]  [      CORE        ]  [Open]
[Plan]  [  Stairs Elev     ]  [Plan]
[Area]  [  Restrooms       ]  [Area]
        [  Mech Shaft      ]
        [Perimeter Offices ]
        [     ...          ]
```

Floor plate: 20-40m wide, any length
Core: stairs + 2 elevators minimum + restrooms + mechanical shaft
Corridor: racetrack loop around core
Perimeter offices: 3.5-4.5m deep, 3-4m wide (one structural bay)
Open plan: between core and perimeter

### Layout Rules

- Core is ALWAYS centrally located (minimizes walking distance)
- Restrooms are ALWAYS in or adjacent to the core (plumbing stacks)
- Window offices are on the perimeter (status + daylight)
- Interior offices/conference rooms are "second ring"
- Open plan fills the rest
- Break room / kitchen near core (plumbing access)
- Conference rooms near lobby/reception (visitor convenience)
- Building width driven by daylight: 15-18m from window to core for natural light to reach open plan

### Structural Bay

- Typical: 7.5m x 7.5m or 9m x 9m grid
- Office partitions align to bay grid
- Interior columns are rarely visible in finished offices (within walls or in open plan)

### Horror Application

Offices work for horror because:
- Cubicle farms create maze-like environments with low visibility
- Glass-walled conference rooms create fishbowl exposure
- The racetrack corridor means threats can come from either direction
- Fluorescent lighting failure creates instant darkness
- Dropped ceilings hide overhead threats
- The core (elevator/stairs) becomes the key escape route that can be blocked

---

## 11. Hospital / Clinic

### Canonical Pattern

**Ground Floor:**
```
[Emergency Dept]--[Imaging/Radiology]--[Main Lobby/Reception]
       |                  |                      |
[Trauma Rooms]    [CT/MRI/X-Ray]        [Registration]
       |                  |                      |
[Emergency Corridor]------+                [Elevators/Stairs]
       |                                        |
[Operating Theatre]--[ICU]--[Recovery]   [Outpatient Clinics]
```

**Upper Floors (Nursing Unit):**
```
[Patient Room][Patient Room][Patient Room]
----------CORRIDOR-----------[Nursing Station]--[Meds]
[Patient Room][Patient Room][Patient Room]     [Clean Supply]
                                               [Soiled Utility]
                                               [Staff Lounge]
```

### Key Rules (the "Golden Triangle")

The critical adjacency in any hospital: **Emergency <-> Imaging <-> OR <-> ICU**. These four departments MUST be adjacent or connected by short, direct corridors.

Ward layout rules:
- Nursing station at CENTER of unit with clear sightlines to all patient rooms
- Clean supply and soiled utility on OPPOSITE ends of nursing station
- Medication room adjacent to nursing station (security)
- Staff lounge near nursing station but acoustically separated
- Corridor minimum 2.4m wide (stretcher passing)
- Patient rooms: 12+ m2 for single, 18-20 m2 for ICU
- Each patient room has its own bathroom (en suite, not shared)

### Corridor Organization

- Double-loaded corridor most common for nursing units
- Racetrack loop for larger facilities (> 30 beds per unit)
- Radial layout most efficient for nurse walking distance but harder to build

### Horror Application (Abandoned Hospital)

Hospitals are EXTREMELY effective for horror because:
- Long, wide corridors (designed for stretchers) feel institutional and cold
- Nursing stations create panopticon-like central viewpoints
- Patient rooms are all dead-ends off the corridor
- The OR/ICU areas have restricted access (locked doors, airlocks) creating zones
- Basements contain morgue, mechanical, and other creepy service spaces
- Radiology rooms are lined with lead (claustrophobic, sound-deadening)
- The "golden triangle" layout means all scary departments are clustered together

---

## 12. Police Station

### Canonical Pattern

**Security Zoning (Three Zones):**

```
ZONE 1 - PUBLIC:
  [Main Entry]--[Lobby/Waiting]--[Reception Desk]
       |                              |
  [Community Room]            [Interview Rooms]
       |                              |
  [Public Restrooms]          [Visitor Check-in]

ZONE 2 - SEMI-RESTRICTED (Staff):
  [Reception]--[Bullpen/Patrol Area]--[Offices]
                      |                    |
               [Dispatch Center]    [Break Room]
                      |                    |
               [Locker Room]       [Armory/Storage]
                      |
               [Report Writing]

ZONE 3 - RESTRICTED (Secure):
  [Sally Port (vehicle bay)]--[Booking Area]--[Holding Cells]
                                    |
                             [Processing]--[Interrogation]
                                    |
                             [Evidence Room]
```

### Key Rules

- Security zones have ONE-WAY progression: public can only access Zone 1
- Sally port (enclosed vehicle bay) is the secure prisoner entry -- vehicles drive in, doors close, then prisoners are moved to booking
- Booking area has direct access to holding cells (prisoner path minimized)
- Evidence room is maximum security (limited access, no windows, climate controlled)
- Dispatch center has windows/monitors overlooking both bullpen and booking
- Interview rooms are on the PUBLIC side (civilians come here)
- Interrogation rooms are on the RESTRICTED side (prisoners brought here)
- Front desk/reception controls the boundary between public and staff zones
- Armory is interior, no exterior walls (security)

### Horror Application (Abandoned Police Station -- RE2)

- The three security zones create a layered depth of exploration
- The sally port is a dramatic set piece (vehicle bay, heavy doors)
- Holding cells are claustrophobic dead-ends
- The evidence room is always in the "deepest" position (most restricted = most scary)
- Dispatch center with dead monitors and silent radios = atmospheric gold
- The one-way security flow means the player must penetrate deeper and deeper

---

## 13. School

### Canonical Pattern

```
[Main Entry]--[Admin Office/Reception]--[Principal's Office]
       |                                       |
[Main Corridor]----[Corridor]--[Corridor]--[Corridor]
   |     |     |        |          |           |
[Class][Class][Class] [Labs]   [Library]   [Cafeteria]
[Class][Class][Class] [Labs]   [Media]     [Kitchen]
                                              |
                               [Gymnasium]--[Locker Rooms]
                                              |
                               [Auditorium]--[Stage]
```

### Layout Rules

- Main entry leads to administration (visitor control)
- Double-loaded corridors for classroom wings
- Classrooms: 7.0m x 7.5m typical (sufficient for 25-30 students)
- Corridor width: 2.4-3.0m (student volume)
- Cafeteria and gymnasium are the LARGEST rooms (require clear spans)
- Cafeteria adjacent to kitchen (obvious but generators miss this)
- Library is centrally located (accessible from all wings)
- Science labs cluster together (shared gas/water lines)
- Restrooms at corridor junctions (not dead ends)
- Multiple fire exits at corridor ends
- Admin offices near main entry (first point of contact)

### Horror Application (Abandoned School)

- Repetitive classroom corridors are deeply uncanny
- Lockers lining corridors add visual noise and potential hiding spots
- Gymnasium is a large open space with terrible acoustics
- Stage/auditorium has backstage areas with catwalks (vertical horror)
- Cafeteria kitchen is industrial and unsettling
- Basement/boiler room access from service corridors

---

## 14. Church / Chapel

### Canonical Pattern

```
[Approach/Path]--[NARTHEX (vestibule)]--[NAVE (main hall)]--[CHANCEL/SANCTUARY]
                       |                     |                      |
                  [Bell Tower]          [Side Aisles]          [Altar]
                       |                     |                      |
                  [Baptistry]           [Transept Arms]         [Sacristy]
                                             |
                                    [Side Chapels]
```

### Layout Rules (Latin Cross Plan)

- Entry through narthex (buffer between sacred and profane)
- Nave is the long central space (congregation sits here)
- Nave proportions: extremely long, 1:3 to 1:5 aspect ratio
- Side aisles separated by columns/arcades
- Transept arms cross the nave creating a cross shape
- Crossing (intersection of nave + transept) often has dome or tower
- Sanctuary/chancel is ELEVATED (stepped up from nave)
- Orientation: traditionally altar faces east, entry faces west

### Horror Application

- The nave's extreme length creates dramatic perspective
- Columns and arcades create hiding spots along side aisles
- The crypt/undercroft beneath the chancel is a classic horror space
- Bell tower is a vertical dead-end with dramatic height
- Stained glass windows create colored light patterns that shift with time
- The sacristy (behind the altar) is a hidden room most people don't know exists

---

## 15. Restaurant / Bar

### Canonical Pattern

```
[Street Entry]--[Host Stand/Waiting]--[DINING ROOM]
                       |                    |
                   [Bar Area]          [Server Station]
                       |                    |
                   [Bar Storage]       [KITCHEN]
                                           |
                              [Prep]--[Cook Line]--[Dish Wash]
                                           |
                              [Walk-in Cooler]--[Dry Storage]
                                           |
                              [Office]--[Employee Area]
                                           |
                              [Receiving/Loading Dock]
```

### Layout Rules

- Kitchen takes 30-40% of total area
- Dining takes 40-60% of total area
- Bar near entry or waiting area (captures walk-in traffic)
- Kitchen-to-dining path must be SHORT and DIRECT (server efficiency)
- Server station is the kitchen-dining boundary (POS, water, ice)
- Back of house (kitchen, storage, office) is NEVER visible from dining
- Walk-in cooler + dry storage near receiving dock (delivery path)
- Restrooms accessible from dining without passing through kitchen
- Bar back wall often shares plumbing with kitchen

---

## 16. Warehouse

### Canonical Pattern

```
[LOADING DOCKS - Receiving]
          |
   [Receiving Area]
          |
   [STORAGE (Racking)]     [STORAGE (Racking)]     [STORAGE (Racking)]
   [      Aisle      ]     [      Aisle      ]     [      Aisle      ]
   [STORAGE (Racking)]     [STORAGE (Racking)]     [STORAGE (Racking)]
          |
   [Picking / Staging]
          |
   [LOADING DOCKS - Shipping]

   [Office Area]--[Break Room]--[Restrooms]  (typically one corner, mezzanine, or partitioned area)
```

### Layout Rules

- U-shaped flow (most common): receiving and shipping on SAME wall
- I-shaped flow: receiving and shipping on OPPOSITE walls (for high-throughput)
- Storage takes 22-27% of cubic volume
- Aisles: 3.0-3.6m wide for forklifts
- Office space is typically 5-10% of total area, partitioned in a corner
- Loading docks: one door per 930-1400 m2 of floor space
- Clear height: 6-10m for racking
- Office mezzanine over receiving area is common (supervisor overlook)

### Horror Application

- Tall racking creates canyon-like aisles with no sightlines
- Forklifts and machinery create environmental hazards
- Loading dock doors are massive, dramatic entry/exit points
- Office mezzanine provides an elevated vantage point (or a trap)
- The vast open volume creates echoing acoustics
- Dark corners and dead-end aisles between racking

---

## 17. Retail Store

### Canonical Pattern

```
[Street Entry]
      |
[SALES FLOOR]
[  Display  ]  [  Display  ]  [  Display  ]
[   Area    ]  [   Area    ]  [   Area    ]
      |
[Checkout / POS Counter]
      |
[Back Wall / Feature Display]
      |            |
[Stockroom]  [Manager Office]
      |            |
[Receiving]  [Employee Break]--[Restrooms]
```

### Layout Rules

- Entry -> decompression zone (3-5m open area, no product)
- Sales floor takes 60-70% of total area
- Checkout near exit (anti-theft, natural flow endpoint)
- Power wall: first wall customers see when they turn right (highest-value display)
- Stockroom: 15-25% of total area
- Restrooms and employee areas are BACK OF HOUSE (invisible to customers)
- Receiving dock at rear or side (delivery trucks)

### Common Floor Patterns

- **Grid**: aisles in parallel rows (grocery stores, pharmacies)
- **Loop/Racetrack**: perimeter path with island displays (IKEA, Target)
- **Free-Flow**: organic arrangement of fixtures (boutiques, galleries)
- **Herringbone**: diagonal aisles (hardware stores)

---

## 18. Horror Subversion Patterns

The key insight: horror buildings start with REAL architecture, then SUBVERT specific expectations. The subversion is what creates dread. A building that was never realistic can't be uncanny.

### 18.1 Subversion: Blocked Routes

Take a normal double-loaded corridor. Block 60-80% of the doors. Now the player sees many doors but can only enter a few. The locked doors create ambiguity (what's behind them?) and the few open doors become committed choices.

Real buildings: most doors are accessible.
Horror: most doors are locked, blocked, or broken.

### 18.2 Subversion: Loop Breaking

Take a normal racetrack office layout. Collapse one section of the loop. Now the player must go around the LONG way. Or find an alternative route (through a vent, through a hole in the wall). The broken loop creates tension because the short path is visible but inaccessible.

### 18.3 Subversion: Privacy Inversion

In real buildings, bedrooms are private and living rooms are public. In horror, the "private" spaces have been invaded (something is in the bedroom, the bathroom is covered in blood). This works BECAUSE the player understands the normal privacy gradient.

### 18.4 Subversion: Impossible Depth

The building is deeper than it should be. The hallway goes on too long. The basement is three floors deep when the building only has one floor above ground. Silent Hill uses this constantly -- entering a ground-level building and descending endlessly.

### 18.5 Subversion: Dual Circulation Reveal

The player learns the main circulation pattern, feels safe. Then they discover the SERVICE circulation (the servant stairs, the utility corridor, the maintenance tunnels). Now every room has a second entrance they didn't know about. This is the Spencer Mansion's core trick.

### 18.6 Subversion: Asymmetric Wings

A building that LOOKS symmetric from outside (two identical wings) but is ASYMMETRIC inside. One wing is normal. The other wing is Wrong. This exploits the expectation of symmetry.

### 18.7 Subversion: The Room That Shouldn't Exist

A room that doesn't fit the building's function: a laboratory under a mansion, a shrine in a police station, an operating theater in a school basement. The wrongness comes from violating the building type's expected room list.

### 18.8 Quantifiable Horror Metrics

From space syntax research applied to horror:
- **Dead-end ratio**: % of rooms with only one exit. Normal building: 20-30%. Horror: 40-60%.
- **Integration value**: Low integration = deep, hard-to-reach spaces. Horror buildings have lower average integration than normal buildings.
- **Corridor L:W ratio**: Normal: 1:3 to 1:5. Horror: 1:8 to 1:15 (unnervingly long).
- **Isovist area**: The visible floor area from any point. Normal: varies. Horror: alternates between very large (exposed) and very small (claustrophobic).
- **Step depth from entry**: How many rooms deep you must go. Normal: 2-4 steps to any room. Horror: 6-10+ steps to key rooms.

---

## 19. Horror Game Case Studies

### Spencer Mansion (Resident Evil)

- **Layout**: Hub-and-spoke. Grand hall is the hub. East and west wings are spokes.
- **Circulation**: DUAL -- main stairs AND service/hidden passages
- **Key trick**: Initial layout seems Victorian-normal, then reveals hidden labs underneath
- **Subversions**: Puzzle locks replace normal doors; enfilade sequences force committed paths; the underground lab violates the mansion's architectural grammar completely
- **What the player learns**: The building has a "real" layer (mansion) and a "hidden" layer (lab). This layering is the core architectural horror.

### Silent Hill 2 (Apartments, Hospital)

- **Layout**: Double-loaded corridor. Many locked doors. A few accessible rooms.
- **Circulation**: Linear with occasional branches
- **Key trick**: Orderly building structure (apartment floor plan, hospital ward) with most doors locked. Player has the map showing all rooms but can only enter a few.
- **Subversions**: Impossible depth (descending endlessly), rooms that change between visits, furniture blockades that make no architectural sense
- **What the player learns**: The building's ORDER is a lie. The floor plan promises regularity that doesn't exist.

### Control (The Oldest House)

- **Layout**: Brutalist office building. Core-and-shell with racetrack corridors.
- **Circulation**: Shifts dynamically. Rooms move. Corridors reconfigure.
- **Key trick**: The architecture is REAL brutalist style (clean lines, concrete, structural honesty) but the spaces are impossible (larger inside than outside, geometry shifts)
- **Subversions**: The building itself is alive and hostile. Normal brutalist architecture becomes alien because it violates spatial expectations WHILE looking architecturally correct.

### Resident Evil 2 (Police Station / Art Museum)

- **Layout**: Repurposed building (art museum converted to police station)
- **Circulation**: Hub (main hall with goddess statue) + wings
- **Key trick**: The building has TWO architectural identities. The art museum layout (grand halls, galleries) conflicts with the police station function (booking, cells, evidence). This creates a sense that the building itself is wrong.
- **Subversions**: The underground parking/sewer system adds a third layer below.

---

## 20. Open Floor Plan Datasets

For training data and reference, these datasets contain real architectural floor plans:

| Dataset | Size | Content | Format | License |
|---------|------|---------|--------|---------|
| **ResPlan** | 17,000 plans | Residential (apartments to multi-wing homes) | Vector/Graph | Research |
| **CubiCasa5K** | 5,000 plans | Finnish residential (80+ object categories) | Image + annotation | Open |
| **CubiGraph5K** | 5,000 plans | Same as CubiCasa5K + room adjacency graphs | Graph (adjacency list) | Open |
| **FloorPlanCAD** | 15,000+ plans | Residential to commercial (CAD drawings) | SVG | Research |
| **MSD (Modified Swiss Dwellings)** | 5,300 plans | Multi-apartment building complexes (18,900 apts) | Graph | Research |
| **MLSTRUCT-FP** | 954 plans | Large-scale floor plans with wall annotations | Image + JSON | Open |
| **RPLAN** | 80,000+ plans | Chinese residential apartments | Image | Research |

### Key Insight from Datasets

CubiGraph5K's adjacency data confirms:
- 1 = rooms sharing a wall (adjacent)
- 2 = rooms connected by a doorway (accessible)

This matches the adjacency matrix structure recommended in Section 4: rooms can be adjacent without being connected, and the connection graph is a subset of the adjacency graph.

---

## 21. Implementation Recommendations

### 21.1 Adjacency Matrix as JSON

Encode building type adjacency rules as a JSON matrix within archetype definitions:

```json
{
  "adjacency": {
    "entry": {
      "living_room": "MUST",
      "coat_closet": "SHOULD",
      "powder_room": "MAY",
      "kitchen": "MAY_NOT",
      "bedroom": "MUST_NOT",
      "bathroom": "MUST_NOT"
    },
    "living_room": {
      "dining_room": "SHOULD",
      "kitchen": "MAY",
      "hallway": "MUST",
      "bedroom": "MUST_NOT"
    },
    "kitchen": {
      "dining_room": "MUST",
      "pantry": "SHOULD",
      "laundry": "SHOULD",
      "bathroom": "MUST_NOT",
      "bedroom": "MUST_NOT"
    },
    "hallway": {
      "bedroom": "MUST",
      "bathroom": "MUST"
    },
    "bedroom": {
      "bathroom": "SHOULD",
      "closet": "SHOULD",
      "bedroom": "MUST_NOT"
    }
  }
}
```

### 21.2 Circulation Pattern Selection

Map building types to circulation patterns:

| Building Type | Primary Pattern | Secondary Pattern |
|---------------|----------------|-------------------|
| Ranch house | Hub (foyer) + tree (bedroom wing) | Loop (kitchen-dining-living) |
| Two-story house | Hub (foyer) per floor | Tree (bedroom hallway) |
| Victorian mansion | Hub (grand hall) + enfilade (wings) | Dual circulation (servant stairs) |
| Apartment unit | Tree (entry -> hall -> rooms) | -- |
| Apartment building | Double-loaded corridor | Point access (European) |
| Office building | Racetrack (around core) | Grid (open plan) |
| Hospital | Double-loaded corridor (wards) | Racetrack (large departments) |
| Police station | Tree (security zones) | Hub (reception boundary) |
| School | Double-loaded corridor (wings) | Hub (main intersection) |
| Church | Enfilade (narthex -> nave -> sanctuary) | -- |
| Restaurant | Free-flow (dining) + linear (kitchen) | -- |
| Warehouse | Grid (racking aisles) | Linear (receive -> store -> ship) |
| Retail | Loop/racetrack (sales floor) | Grid (stockroom) |

### 21.3 Validation Rules for "Feels Real"

After generating a floor plan, validate these rules (fail = regenerate):

1. **Entry connects to public room** -- entry must connect to living room, lobby, or foyer (not kitchen, bedroom, bathroom)
2. **No captive bedrooms** -- every bedroom must reach a bathroom without passing through another bedroom
3. **Wet wall clustering** -- all bathrooms/kitchens within 2 rooms of each other on the adjacency graph
4. **Aspect ratio bounds** -- no habitable room with aspect ratio > 2.5; no corridor with aspect ratio < 2.0
5. **Loop exists** -- at least one loop in the connectivity graph for buildings with 4+ rooms (except pure-tree types like apartment units)
6. **Windows on exterior only** -- no windows on walls shared between two rooms
7. **Private zone depth** -- bedrooms must be at least 2 steps from entry in the connectivity graph
8. **Room size hierarchy** -- living/gathering room is the largest room; bathrooms are among the smallest
9. **Corridor efficiency** -- corridor area < 20% of total floor area
10. **Structural grid alignment** -- room walls align to grid lines (within 0.5m tolerance)

### 21.4 Horror Modifiers

Apply these as post-processing on a "normal" building to make it scary:

```json
{
  "horror_modifiers": {
    "door_lock_ratio": 0.6,        // 60% of doors locked/blocked
    "loop_break_chance": 0.3,       // 30% chance to collapse a loop
    "impossible_depth_floors": 2,   // basement goes 2 extra floors deep
    "service_reveal": true,         // add hidden service corridors
    "asymmetric_wing": true,        // one wing diverges from expected layout
    "wrong_room_type": "laboratory",// add a room that doesn't belong
    "dead_end_ratio_target": 0.5,   // target 50% dead-end rooms
    "corridor_stretch_factor": 1.5  // corridors 50% longer than normal
  }
}
```

### 21.5 Priority Order for Implementation

1. **Adjacency matrix enforcement** -- highest impact, prevents the worst "generated" feel
2. **Public-to-private gradient** -- entry connects to public space, bedrooms are deep
3. **Wet wall clustering** -- bathrooms near each other, near kitchens
4. **Room proportion constraints** -- aspect ratios by room type
5. **Loop creation** -- at least one circulation loop in living spaces
6. **Structural grid snapping** -- room walls align to bay spacing
7. **Entry sequence** -- proper exterior -> threshold -> reception -> circulation flow
8. **Horror modifiers** -- applied AFTER a "normal" building is generated

---

## 22. Sources

### Architectural Reference
- [Building Advisor: Circulation Key to a Successful Floor Plan](https://buildingadvisor.com/design/floor-plans/circulation-key-to-a-successful-floor-plan/)
- [Building Advisor: Fine-Tuning a Floor Plan](https://buildingadvisor.com/design/floor-plans/fine-tuning-a-floor-plan/)
- [Houseplans.com: Go with the Flow -- Circulation Patterns](https://www.houseplans.com/blog/go-with-the-flow-how-to-analyze-the-circulation-patterns)
- [Houseplans.com: Watch That Layout -- Avoid Dead Ends](https://www.houseplans.com/blog/watch-that-layout-avoid-dead-ends)
- [Archisoup: Architecture Circulation Diagrams](https://www.archisoup.com/architecture-circulation-diagram)
- [Archisoup: Adjacency Diagrams in Architecture](https://www.archisoup.com/adjacency-diagrams)
- [Standard Room Sizes (sqft.expert)](https://sqft.expert/blogs/standard-sizes-of-rooms-architects)
- [Clawson Architects: Architecture by Numbers](https://www.clawsonarchitects.com/blog-entry/2012/02/architecture-by-numbers.html)
- [Neufert Architects' Data (reference book)](https://books.google.com/books/about/Architects_Data.html?id=sEugDwAAQBAJ)
- [Coohom: Architectural Standards for Foyer Layouts](https://www.coohom.com/article/architectural-standards-for-smart-foyer-layouts)
- [Christine Franck: Avoiding Fenestration Fiascoes](https://christinefranck.com/2012/11/30/avoiding-fenestration-fiascos/)
- [Digitzo: Window Rhythm in Architecture](https://digitzo.com/window-rhythm-in-architecture-for-better-proportion/)

### Circulation and Space Syntax
- [Vaia: Circulation Patterns](https://www.vaia.com/en-us/explanations/architecture/architectural-analysis/circulation-patterns/)
- [Space Syntax Online: Spatial Form Analysis](https://www.spacesyntax.online/applying-space-syntax/building-methods/spatial-form-analysis/)
- [Hillier & Hanson: Building Circulation Typology and Space Syntax Predictive Measures](https://www.researchgate.net/publication/281839434_Building_circulation_typology_and_Space_Syntax_predictive_measures)
- [Incite Architecture: Public, Semi-Public, and Private Spaces](http://incitearchitecture.com/news/2016/3/31/public-semi-public-and-private-spaces)
- [Misfits Architecture: Served and Servant Spaces](https://misfitsarchitecture.com/2022/11/20/architecture-myths-33-served-and-servant-spaces/)

### Building Types
- [BuiltXSDC: Hospital Floor Plan Guide 2025](https://www.builtxsdc.com/blog/hospital-floor-plan-guide-2025-codes-best-practices)
- [Officer.com: 9 Steps to Building Your Most Effective Police Station](https://www.officer.com/command-hq/supplies-services/architects-designers/article/21133021/)
- [Architecture Professor: Double-Loaded Corridor](https://thearchitectureprofessor.com/2020/07/16/2-10-the-double-loaded-corridor-with-an-exterior-lightcourt/)
- [Stora Enso: Office Around Central Core](https://buildingconcepts.storaenso.com/en/find-your-concept/office-around-central-core_18m-wide)
- [Coohom: Core and Shell Design for Office Buildings](https://www.coohom.com/article/core-and-shell-design-for-office-buildings)
- [Mecalux: Warehouse Layout Design](https://www.mecalux.com/warehouse-manual/warehouse-design/warehouse-layout)
- [Restaurant HQ: How to Design a Restaurant Floor Plan](https://www.therestauranthq.com/startups/restaurant-floor-plan/)
- [FitSmallBusiness: 9 Retail Store Layouts](https://fitsmallbusiness.com/planning-your-store-layout/)
- [Coohom: School Building Layout Floor Plans](https://www.coohom.com/article/school-building-layout-floor-plans)
- [Coohom: Gothic Victorian Mansion Floor Plans](https://www.coohom.com/article/gothic-victorian-mansion-floor-plans)

### Horror and Level Design
- [Level Design Book: Circulation](https://book.leveldesignbook.com/process/layout/flow/circulation)
- [Level Design Book: Typology](https://book.leveldesignbook.com/process/layout/typology)
- [GameHaunt: Deconstructing the Level Design of Iconic Horror Mansions](https://gamehaunt.com/deconstructing-the-level-design-of-iconic-horror-mansions/)
- [World of Level Design: Horror Level Design Part 1](https://www.worldofleveldesign.com/categories/level_design_tutorials/horror-fear-level-design/part1-survival-horror-level-design-cliches.php)
- [Game Developer: Real Buildings That Inspired Control](https://www.gamedeveloper.com/art/the-real-buildings-that-inspired-i-control-i-s-oldest-house)
- [Game Developer: Level Design in Procedural Generation](https://www.gamedeveloper.com/design/level-design-in-procedural-generation)
- [Spencer Mansion (Resident Evil Wiki)](https://residentevil.fandom.com/wiki/Spencer_Mansion)
- [NYFA: Architecture of Fear -- Haunted Houses](https://www.nyfa.edu/student-resources/haunted-houses-game-design-nyfa/)

### Procedural Generation
- [Lopes et al.: A Novel Algorithm for Real-time Procedural Generation of Building Floor Plans (2010)](https://arxiv.org/abs/1211.5842)
- [Freiknecht: Constrained Growth Method for Procedural Floor Plan Generation](https://graphics.tudelft.nl/~rafa/myPapers/bidarra.GAMEON10.pdf)
- [ResPlan: 17,000 Residential Floor Plans Dataset](https://arxiv.org/html/2508.14006v1)
- [CubiCasa5K Dataset](https://github.com/CubiCasa/CubiCasa5k)
- [CubiGraph5K: Organizational Graph for Floor Plans](https://github.com/luyueheng/CubiGraph5K)
- [FloorPlanCAD Dataset](https://floorplancad.github.io/)
- [MSD: Benchmark Dataset for Floor Plan Generation](https://caspervanengelenburg.github.io/msd-eccv24-page/)
- [MLSTRUCT-FP Dataset](https://github.com/MLSTRUCT/MLSTRUCT-FP)

### Structural Reference
- [Quora: Typical Column Spacings in Multi-Storey Buildings](https://www.quora.com/What-are-the-typical-column-spacings-in-multi-storey-concrete-buildings)
- [Rhino Steel Buildings: Bay Spacing](https://www.rhinobldg.com/blog/what-is-a-steel-building-bay)

### Room Proportions
- [Laurel Bern: Perfect Architectural Proportions](https://laurelberninteriors.com/perfect-architectural-proportions-the-no-fail-formula/)
- [Houzz: Key Measurements -- Hallway Design](https://www.houzz.com/magazine/key-measurements-hallway-design-fundamentals-stsetivw-vs~25890141)
- [Catch Architecture: Room Size Guide S-M-L](https://catcharchitecture.com/room-sizes-guide-s-m-l)
