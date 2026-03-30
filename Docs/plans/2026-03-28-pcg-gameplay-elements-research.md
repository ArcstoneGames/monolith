# PCG for Gameplay Elements -- Items, Enemies, Objectives, Navigation

**Date:** 2026-03-28
**Status:** Research Complete
**Engine:** Unreal Engine 5.7 (PCG Production-Ready)
**Context:** How PCG + Monolith spatial registry can handle gameplay-relevant placement for survival horror FPS

---

## Table of Contents

1. [Item/Loot Placement](#1-itemloot-placement)
2. [Enemy Spawn Points](#2-enemy-spawn-points)
3. [Objective Placement](#3-objective-placement)
4. [AI Patrol Paths](#4-ai-patrol-paths)
5. [Cover Positions](#5-cover-positions)
6. [Barricades and Obstacles](#6-barricades-and-obstacles)
7. [Safe Rooms](#7-safe-rooms)
8. [Jump Scares](#8-jump-scares)
9. [Smart Object Placement](#9-smart-object-placement)
10. [Procedural Locks and Keys](#10-procedural-locks-and-keys)
11. [Difficulty Scaling](#11-difficulty-scaling)
12. [NavMesh Integration](#12-navmesh-integration)
13. [Unified Architecture](#13-unified-architecture)
14. [Proposed MCP Actions](#14-proposed-mcp-actions)
15. [Implementation Plan](#15-implementation-plan)
16. [Sources](#16-sources)

---

## 1. Item/Loot Placement

### Industry Approaches

**Left 4 Dead AI Director:**
Upon loading a map, the Director populates it with items by selecting from pre-placed spawn points. The Director has limited positional control -- it mostly limits the number of similar items spawning close together. It can reactively change items based on team performance; e.g., spawning health kits nearby when the team is low on health. The key insight: **spawn points are hand-placed, item selection is procedural**.

**Resident Evil 4 DDA:**
RE4 tracks the player's currently favored weaponry and reduces ammo drops for it while increasing drops for underused weapons. Low on healing items? High chance of getting them. Low ammo? High chance of ammo drops. This inventory-aware reactive system operates independently of the difficulty rank (1-10) that controls enemy aggression.

**Dead Space:**
Uses a "breadcrumb" system -- items drop from killed enemies based on what the player needs most (health if hurt, ammo for equipped weapon if low). Stomp/loot corpses for randomized secondary drops.

### Spawn Table Design for Room Types

Each room type in the spatial registry should have an associated **loot table** with weighted entries:

```
FLootTable:
  room_type: "bathroom"
  entries:
    - { item: "health_syringe", weight: 3.0, max_per_room: 2 }
    - { item: "bandage", weight: 5.0, max_per_room: 3 }
    - { item: "note_collectible", weight: 1.0, max_per_room: 1 }
    - { item: "ammo_pistol", weight: 0.5, max_per_room: 1 }
  total_items: { min: 0, max: 3 }
  empty_probability: 0.3
```

Room type -> loot table mapping (thematic):

| Room Type | Primary Items | Secondary Items | Notes |
|-----------|--------------|-----------------|-------|
| Bathroom | Health items, bandages | Notes | Medical cabinet location |
| Kitchen | Food/health, melee weapons (knife) | Keys | Drawers, cabinets |
| Office | Notes, keys, keycards | Ammo (desk drawer) | Documents, lore |
| Bedroom | Health, notes, flashlight batteries | Hidden ammo | Under pillows, drawers |
| Storage/Closet | Ammo, tools, crafting materials | Keys | Shelves, boxes |
| Garage/Workshop | Melee weapons, tools, fuel | Ammo | Workbenches |
| Hallway | Nothing (usually) | Rare: ammo, note | Tension space, not reward |
| Safe Room | Save point, storage chest | Guaranteed health | Always has resources |
| Medical Room | Health kits, syringes, first aid | Notes | Primary health source |
| Security Room | Ammo, keycards, weapons | Map/notes | Weapon cache |

### PCG Integration

PCG's weighted Static Mesh Spawner is ideal here. Each entry has a Weight property (default 1); a mesh with weight 2 appears twice as often as one with weight 1. Data Assets can drive mesh selection to keep PCG graphs clean.

**Approach:** Use PCG graphs with room-type-aware parameters:
1. Spatial registry provides room AABB + room_type
2. PCG graph receives room bounds as input volume
3. Loot table Data Asset selected by room_type
4. Surface Sampler finds valid placement surfaces (shelves, tables, floors)
5. Density filter + Self Pruning prevents clustering
6. Static Mesh Spawner places items from weighted table

**Spawn Point Strategy (Hybrid L4D):**
- **Pre-placed spawn points:** Level designer marks key locations (medicine cabinet, desk drawer, shelf)
- **Procedural selection:** At generation time, select which spawn points are active based on loot table
- **Reactive adjustment:** AI Director can activate dormant spawn points if player is struggling
- This hybrid approach is more reliable than pure surface scatter for indoor items

### Hospice Considerations

- Items should be visually distinct (high contrast, subtle glow/outline)
- Never place critical items in hard-to-reach locations (high shelves, behind physics objects)
- Guarantee minimum health items per floor (configurable floor_min_health parameter)
- Accessibility mode: items have subtle audio cue when player is within 3m


---

## 2. Enemy Spawn Points

### Industry Approaches

**Alien Isolation Director:**
The Director posts "job requests" to a blackboard: `{GoalType: Search, Location: Medbay_Lab_A, Priority: 0.75, Duration: 90s}`. The Xenomorph polls the blackboard, picks up a job, executes. The menace gauge measures tension (not just proximity) -- factors include walking distance, line of sight, motion tracker reading, and how quickly the alien can reach the player. Once menace peaks, the Director sends the alien elsewhere (cooldown timers prevent immediate return).

**Left 4 Dead:**
Four intensity states: Build-Up, Sustained Peak, Peak Fade, Relax. Hordes spawn when intensity is in Build-Up. The Director never spawns zombies the player can see (visibility check). Spawns happen behind corners, in rooms the player hasn't entered, or off-screen. Special Infected have minimum/maximum spawn distances and cooldowns per type.

**RE2 Remake Mr. X:**
Uses real navmesh pathfinding. Speed scales inversely with distance (walks when far, jogs when close). Cannot enter designated safe rooms. Has defined "leash range" -- despawns if too far from player for too long.

### Placement Rules for Horror

**Hard constraints (MUST enforce):**
- Never spawn in safe rooms or within 10m of safe room doors
- Never spawn where player has direct line of sight (visibility cull)
- Never spawn on the same floor as the player if player just entered building (grace period)
- Never spawn blocking the only exit from a room (solvability)
- Minimum distance from player spawn: 15m (prevent instant death on load)

**Soft constraints (SHOULD enforce, can violate for drama):**
- Prefer dark areas (light level < 0.3 threshold from lightmap/runtime query)
- Prefer behind corners (dot product of spawn-to-player and corridor direction < -0.5)
- Prefer behind doors the player hasn't opened
- Prefer near room transitions (doorways, stairwells) for ambush potential
- Prefer vent openings, ceiling openings, windows (entrance variety)
- Avoid placing more than 2 enemies in rooms < 20 sqm (claustrophobia, not unfairness)

**Spatial Queries via EQS:**
UE5.7 EQS is the right tool for runtime enemy placement decisions:
- `UEnvQueryGenerator_OnCircle` -- ring of test points around target area
- `UEnvQueryTest_Trace` -- line-of-sight checks
- `UEnvQueryTest_Distance` -- distance from player
- `UEnvQueryTest_PathfindingBatch` -- navmesh reachability
- Custom test: `UEnvQueryTest_LightLevel` -- query lightmap or dynamic light at point
- Custom test: `UEnvQueryTest_RoomType` -- check spatial registry room classification

### Spawn Point Categories

| Category | Placement | Use Case |
|----------|-----------|----------|
| Closet Spawn | Inside closets/cabinets | Burst-out scare when player opens |
| Vent Spawn | Near vent openings | Crawl-out entrance animation |
| Corner Spawn | Behind L-shaped corridor turns | Ambush on rounding corner |
| Door Spawn | Behind closed doors | Reveal when door opens |
| Ceiling Spawn | Rooms with damaged ceiling | Drop-down entrance |
| Floor Spawn | Rooms with floor grates/holes | Rise-up entrance |
| Patrol Start | Corridor endpoints | Begin patrol route |
| Dormant Spawn | Anywhere valid on navmesh | Activated by Director trigger |

### PCG for Spawn Point Distribution

PCG generates spawn point markers (empty actors with metadata), NOT actual enemies:
1. Room bounds from spatial registry
2. Surface Sampler on floor navmesh
3. Filter by room type (no spawns in safe rooms)
4. Filter by light level (prefer dark areas via lightmap density query)
5. Density control per difficulty preset
6. Place `AEnemySpawnPoint` actors with metadata (category, max_enemy_type, activation_trigger)
7. Runtime: AI Director selects which spawn points to activate


---

## 3. Objective Placement

### Objective Types for Survival Horror

| Objective Type | Examples | Placement Constraints |
|---------------|----------|----------------------|
| Key Item | Keys, keycards, fuses | Must be reachable before locked door; thematic room |
| Puzzle Piece | Valve handle, circuit board | Near puzzle mechanism or in adjacent area |
| Switch/Lever | Power switches, gate controls | On walls near the thing they control |
| Collectible Note | Lore documents, photos | Desks, shelves, pinboards; everywhere |
| Weapon Upgrade | Weapon parts, blueprints | Security rooms, workshops, hidden areas |
| Save Item | Typewriter ribbons, ink | Near save points, occasionally scattered |
| Quest Trigger | Radio, computer terminal | Fixed location per objective design |

### Critical Path Placement Algorithm

Objectives that gate progression (keys, fuses, quest items) must be placed with **solvability guarantees**. The spatial registry's adjacency graph enables this:

1. Build **reachability graph** from room adjacency data
2. Identify **locked transitions** (doors requiring keys/items)
3. Place key for lock N in a room reachable WITHOUT key N (but possibly requiring keys 1..N-1)
4. Verify full path from start to end is solvable via BFS on unlocked graph
5. Place optional objectives (collectibles, upgrades) in side rooms off critical path

This is a simplified version of the metazelda/Dormans approach (see Section 10).

### PCG for Non-Critical Objectives

Collectibles, notes, and optional pickups can use PCG scatter:
- Notes: prefer desks, shelves, pinboards (surface type metadata)
- Collectibles: spread evenly across all rooms (1 per N rooms)
- Upgrades: prefer security/workshop/storage rooms
- PCG graph receives room list, filters by type, places markers at valid surface positions


---

## 4. AI Patrol Paths

### Patrol Path Generation from Room Graph

The spatial registry's adjacency graph is the foundation for patrol route generation. AI enemies don't need hand-crafted waypoint splines when the building topology IS the graph.

**Algorithm: Room-Graph Patrol Generation**

```
1. Select patrol "territory" (set of connected rooms, usually 4-8)
2. Build subgraph from spatial registry adjacency
3. Generate Hamiltonian-ish path (visit each room at least once)
4. For each room transition:
   a. Query door position from opening data
   b. Generate navmesh path between door centers
   c. Add "inspect points" inside rooms (corners, behind furniture via EQS)
5. Loop path or reverse at endpoints
6. Add randomized dwell times per room (5-30s)
7. Add idle behavior at inspect points (look around, interact with Smart Object)
```

**Patrol Types:**

| Type | Description | Horror Use |
|------|-------------|------------|
| Fixed Loop | Visit rooms A->B->C->D->A, repeat | Predictable guard the player can learn/avoid |
| Random Walk | Pick random adjacent room, go there | Unpredictable, anxiety-inducing |
| Weighted Random | Bias toward rooms near player (menace) | Stalker behavior |
| Perimeter | Walk building exterior/corridor loop | Security guard pattern |
| Investigate | Temporarily leave route toward stimulus | Responds to noise/light |
| Escalating | Start with small territory, expand over time | Alien Isolation approach |

**UE5 Implementation:**
- Use `UNavigationSystemV1::FindPathSync()` for segment pathfinding
- `FNavigationPath` provides waypoints along navmesh
- StateTree task: `USTTask_PatrolRoomGraph` with room list as parameter
- EQS for inspect point selection within rooms (prefer corners, furniture proximity)
- AIPerception triggers Investigation patrol on hearing/sight stimulus

### PCG for Patrol Visualization

Generate debug spline actors showing patrol routes for level design review:
1. Query spatial registry for room connections
2. Build patrol route from algorithm
3. Spawn debug spline through room centers/doors
4. Color-code by patrol type (red=aggressive, yellow=fixed, blue=random)


---

## 5. Cover Positions

### Cover System for Survival Horror

Cover in survival horror is contextual -- not a waist-high wall shooter. Cover means:
- **Player cover:** Behind furniture, around corners, inside closets (hiding)
- **AI cover:** Behind obstacles for ranged enemies (less common in horror)
- **Environmental barriers:** Tables, desks, overturned shelves that block line of sight

### Cover Point Generation

**UE5 CoverGenerator approach (Deams51/CoverGenerator-UE4):**
The cover generator analyzes level geometry to find possible cover points. Cover points provide info: crouched vs standing cover, lean left/right/over availability. These points integrate with EQS for AI to query optimal tactical positions.

**PCG-based cover point generation:**
1. Trace downward from grid of points to find floor surfaces
2. For each floor point, trace horizontally in 8 directions
3. Points where horizontal trace hits geometry within 0.5-1.5m = potential cover
4. Classify: crouch cover (hit height 0.6-1.0m), standing cover (hit height > 1.5m)
5. Filter: must have at least one open side (not enclosed on all sides)
6. Place `ACoverPoint` actor with metadata (cover_type, open_directions)

**Room-Aware Cover:**
Spatial registry room types inform cover density:

| Room Type | Cover Density | Cover Style |
|-----------|--------------|-------------|
| Hallway/Corridor | Low | Doorframe recesses, corners |
| Office | Medium | Desks, filing cabinets |
| Storage | High | Shelving units, crates |
| Open Area (lobby, gym) | Low-Medium | Columns, reception desks |
| Kitchen | Medium | Counters, tables |
| Bathroom | Very Low | Stalls only |

### EQS Cover Queries

For AI that uses cover (human enemies, armed survivors):
- `UEnvQueryGenerator_SimpleGrid` around AI location
- `UEnvQueryTest_Trace` from each point toward player (must fail = covered)
- `UEnvQueryTest_Trace` from each point to side of player (should succeed = can lean-shoot)
- `UEnvQueryTest_Distance` from player (prefer 5-15m for ranged combat)
- Custom: `UEnvQueryTest_CoverQuality` scoring height, open sides, material penetrability


---

## 6. Barricades and Obstacles

### Obstacle Types

| Type | Function | Gameplay Impact |
|------|----------|----------------|
| Overturned furniture | Visual storytelling + partial block | Forces crouch or alternate path |
| Collapsed ceiling/rubble | Hard block | Requires alternate route or tool |
| Locked door | Progression gate | Requires key/item |
| Boarded-up passage | Soft block | Can break through (melee/tool, makes noise) |
| Flooded area | Movement penalty zone | Slow movement, splashing alerts enemies |
| Fire/hazard | Damage zone | Timer-based or requires extinguisher |
| Broken elevator | Vertical block | Must find stairs/alternate route |
| Furniture wall (barricade) | Semi-permanent block | Player can push/dismantle |

### Procedural Barricade Placement

**Algorithm:**
1. From spatial registry, identify all room transitions (doors/openings)
2. Classify each as: critical_path, alternate_path, dead_end, shortcut
3. **Critical path:** never fully block (may add soft obstacle requiring interaction)
4. **Alternate paths:** 30-50% chance of hard block (rubble, locked without key)
5. **Dead ends:** Low block rate (these are exploration reward areas)
6. **Shortcuts:** 60-80% chance of partial block (boards, furniture)
7. Place obstacle meshes at transition points
8. Update navmesh modifiers (blocked = NavArea_Null, slowdown = cost modifier)

**Horror-Specific Rules:**
- Corridors should have periodic obstacle clusters that break sightlines (every 15-20m)
- Obstacles that make noise when interacted with (breaking boards, pushing furniture) serve dual purpose: path clearing + enemy alert
- Some obstacles should be AI-permeable but player-blocking (enemies can climb over rubble the player cannot)
- Progressive degradation: buildings further from safe zones have more obstacles

### PCG Obstacle Scatter

For non-structural obstacles (debris, loose objects, minor clutter):
1. PCG Surface Sampler on corridor/room floors
2. Density gradient: higher near walls, lower in center (natural settling)
3. Physics-settled placement (Hyper Props Spawner pattern: spawn above surface, simulate settle)
4. Metadata tags for interactability (pushable, breakable, static)
5. NavMesh modifier volumes auto-generated around obstacle clusters


---

## 7. Safe Rooms

### Design Principles (from Industry Analysis)

**Core Function:** Safe rooms are emotional sanctuaries -- quiet places where players reflect, plan, and regain confidence. The drama of safe rooms is the hope they represent.

**Resident Evil Rules:**
- Enemies cannot enter safe rooms (hard boundary)
- Distinctive music/ambiance change on entry (immediate audio feedback)
- Item storage box present (manage inventory)
- Save mechanism present (typewriter/tape recorder)
- Typically single entrance/exit (defensible)
- Warm lighting, relative cleanliness (visual contrast with horror areas)

**Silent Hill 4 Subversion:**
Room 302 starts safe but progressively becomes unsafe -- monsters appear, hauntings accumulate. This is the gold standard for subverting safe room expectations.

**SOMA Safe Mode:**
Monsters remain present but non-lethal. This is ideal for hospice patients -- tension without punishment.

### Safe Room PCG Rules

Safe rooms need **different PCG parameters** than normal rooms:

```
FSafeRoomConfig:
  enemy_spawn: false          # Absolute: no enemy spawn points
  enemy_entry: false          # AI navigation: navmesh blocked at doorway
  lighting: "warm"            # PostProcess preset: warm, bright
  debris_density: 0.1         # Minimal clutter (clean, organized)
  guaranteed_items:
    - save_point: 1
    - storage_chest: 1
    - health_item: { min: 1, max: 3 }
  ambient_sound: "safe_room"  # Distinct calm music
  tension_zone: "sanctuary"   # classify_zone_tension override
  door_type: "heavy"          # Visually distinct door (metal, reinforced)
```

**Placement Rules:**
- One safe room per floor (minimum), two for floors > 500 sqm
- Never more than 8 rooms of traversal from the previous safe room
- Always on or very near the critical path (never hidden in dead ends)
- Must be accessible without solving puzzles or fighting enemies to reach
- Safe room doors should be visually distinct (different material, light above door)

### Safe Room Detection

When generating buildings, mark rooms as safe_room in the spatial registry:
1. Floor plan generator designates safe rooms during layout phase
2. Spatial registry stores `is_safe_room: true` flag
3. All subsequent PCG passes check this flag
4. Enemy spawn, obstacle, and scare trigger PCG graphs skip safe rooms
5. Item/loot PCG uses safe_room-specific loot table (generous)
6. Volume auto-generation uses safe_room reverb/PP presets


---

## 8. Jump Scares

### Scare Trigger Design

**Fundamental Principle:** Jump scares work best when the player is cognitively loaded -- thinking about what to do next, anticipating a reward, or in the middle of a puzzle. Exploiting cognitive load is key. Randomization of timing/positioning prevents pattern recognition across playthroughs.

**Scare Categories:**

| Category | Trigger | Example |
|----------|---------|---------|
| Corner Turn | Player rounds L-shaped corridor | Monster/corpse revealed |
| Door Open | Player opens door | Something falls/lunges |
| Item Pickup | Player picks up key item | Sound/event behind them |
| Backtrack | Player returns through cleared area | Previously empty space now occupied |
| Window | Player passes window | Face/shadow in glass |
| Vent | Player near vent | Sound + movement |
| Light | Player enters dark area | Flickering reveal |
| Scripted | One-time event at fixed location | Setpiece scare (ceiling collapse, phone rings) |

### Procedural Scare Trigger Placement

**Algorithm:**
1. Identify scare-eligible positions from spatial registry:
   - L-shaped corridor turns (query opening adjacency for 90-degree transitions)
   - Door openings between rooms
   - Windows (opening type = window in building descriptor)
   - Room centers for backtrack scares
2. Score each position:
   - Distance from last scare trigger (+score if far, -score if close; min 3 rooms between scares)
   - Light level at position (+score for darker areas)
   - Room type weight (corridors > open rooms for scares)
   - Near critical path (+score; player MUST pass through)
3. Select top N positions (N = difficulty-scaled scare count)
4. Assign scare type based on position geometry:
   - Corner position -> corner_turn scare
   - Door position -> door_open scare
   - Window position -> window scare
   - Open room + has been traversed -> backtrack scare
5. Place `ATriggerVolume` with scare metadata
6. Some scares are "one-shot" (fire once, disable), others are "cooldown" (can re-trigger after N minutes)

### Scare Budget System

Constant scares desensitize the player. Use a **scare budget** per floor:

```
FScareBudget:
  max_scares_per_floor: { easy: 2, normal: 4, hard: 6 }
  min_rooms_between_scares: 3
  scare_cooldown_seconds: 120    # Minimum time between any two scares
  scare_intensity_curve: "build_release"  # Start mild, peak mid-floor, ease off before safe room
  false_alarm_ratio: 0.3         # 30% of triggers are just creepy sounds, no danger
```

The false alarm ratio is critical -- if every scare is lethal, the player learns to dread triggers. If some are just atmosphere, uncertainty amplifies fear.

### Hospice Considerations

- Scare intensity should be separately configurable from combat difficulty
- Option to disable jump scares entirely (atmosphere-only mode)
- Audio jump scares should have configurable volume cap
- Visual scares should have optional screen flash reduction
- Never trigger scares during accessibility pauses or menu transitions


---

## 9. Smart Object Placement

### Smart Objects Overview (UE 5.7)

Smart Objects represent activities that AI can use through a reservation system. Each Smart Object contains slots with context for interactions. Key components:
- `USmartObjectComponent` -- added to actors to make them Smart Objects
- `USmartObjectDefinition` -- defines available slots and behaviors
- `FSmartObjectSlotDefinition` -- individual interaction slot with transform offset
- `USmartObjectSubsystem` -- runtime management, reservation, queries
- `FSmartObjectClaimHandle` -- reservation ticket

### Smart Object Types for Horror AI

| SO Type | AI Behavior | Placement Location |
|---------|------------|-------------------|
| Patrol Point | Stop, look around, idle anim | Room corners, corridor junctions |
| Search Point | Investigate area thoroughly | Under desks, behind furniture, in closets |
| Vent Entry/Exit | Climb into/out of vent | Near vent openings |
| Door Interact | Open/close/bang on door | At door openings |
| Ambush Point | Wait hidden, attack when player near | Behind doors, in dark corners |
| Feeding Point | Horror idle (eating corpse, etc.) | Dead end rooms, away from player path |
| Break Object | Smash furniture, tear down barrier | Near barricades, furniture clusters |
| Wander Point | Slow patrol idle, look around | Anywhere on navmesh |

### Procedural Smart Object Placement

Smart Objects should be placed alongside the geometry that motivates them:
1. **Furniture spawn** -> add Search SO slots to desks, shelves, cabinets
2. **Door placement** -> add Door Interact SO at each door opening
3. **Vent opening** -> add Vent Entry/Exit SO at each vent mesh
4. **Corner detection** -> add Ambush SO at blind corners
5. **Patrol route generation** -> add Patrol Point SO at each room inspect point

**Implementation:**
```cpp
// After spawning furniture via PCG/Monolith
void AddSmartObjectToActor(AActor* FurnitureActor, USmartObjectDefinition* SearchDef)
{
    USmartObjectComponent* SOComp = NewObject<USmartObjectComponent>(FurnitureActor);
    SOComp->SetDefinitionAsset(SearchDef);
    FurnitureActor->AddInstanceComponent(SOComp);
    SOComp->RegisterComponent();
}
```

Smart Object Definitions should be Data Assets referenced by type:
- `DA_SO_SearchFurniture` -- search desk/shelf behavior
- `DA_SO_PatrolPoint` -- idle/lookAround at patrol stop
- `DA_SO_AmbushCorner` -- hide and wait behavior
- `DA_SO_DoorInteract` -- open/close/bash door
- `DA_SO_VentTraversal` -- enter/exit vent system

### PCG Smart Object Scatter

PCG can place Smart Object actors alongside props:
1. Same PCG graph that places furniture ALSO places SO markers
2. Use Actor Spawner node (not Static Mesh Spawner) for SO actors
3. SO actors reference appropriate Definition asset
4. Density: 1-2 SOs per room for patrol, 1 per searchable furniture piece


---

## 10. Procedural Locks and Keys

### The Solvability Problem

The central challenge: every lock must have its key reachable BEFORE the lock. If key B is behind lock A, the player must be able to reach key A first. This must be **guaranteed by construction**, not validated after the fact.

### Algorithms

**Metazelda (Tom Coxon):**
Guarantees solvability by tracking key-levels. Key-level N = set of rooms accessible with N keys but not N-1. The Nth key MUST appear in a key-level M where M < N. This is enforced structurally during generation.

GitHub: `tcoxon/metazelda` -- Java library, procedural Zelda-like dungeon missions.

**Dormans Cyclic Generation:**
Generates dungeons by composing cycles -- circular loops of linked rooms. Lock-and-key is one cycle type (enter lock room, find key elsewhere, return to unlock). Cycles can nest: a lock-and-key cycle contains a sub-cycle where finding the key requires solving another puzzle. Uses Ludoscope transformational grammar.

Key insight: **cycles create non-linear exploration within linear progression**. The player must find the key (exploration freedom) but the lock gates forward progress (linear structure).

**Nystrom Rooms-and-Mazes:**
Robert Nystrom's algorithm generates rooms and maze corridors, then stitches them together. Lock-and-key is layered on top: after connectivity is established, doors are locked and keys are placed in rooms that are reachable from the start without passing through the locked door.

**Mission Graph Approach (recommended for Monolith):**
1. Define mission as a directed graph: Start -> [rooms] -> Goal
2. Insert lock nodes on edges between room clusters
3. For each lock, place corresponding key in a room reachable from Start without passing through that lock
4. Verify via BFS: from Start, can we reach key1 without passing lock1? Then key2 without lock2? Etc.
5. Spatial registry adjacency graph provides the reachability data

### Lock Types for Horror

| Lock Type | Key Type | Gameplay |
|-----------|----------|----------|
| Locked door | Physical key | Classic; find key in nearby rooms |
| Electronic lock | Keycard/ID badge | Tech-themed areas (hospital, office) |
| Power gate | Fuse/generator fuel | Environmental puzzle; restore power |
| Combination lock | Code (from note/env) | Exploration reward for reading notes |
| Barricade | Crowbar/tool | Tool-based progression |
| Broken elevator | Parts/power | Multi-step fetch quest |
| Flooded passage | Pump handle/valve | Environmental change |
| Chemical lock | Specific reagent | Lab/medical themed |

### Implementation for Monolith

The spatial registry already supports adjacency queries and BFS pathfinding. Lock-and-key generation extends this:

```
FLockKeyConfig:
  total_locks: { min: 3, max: 6 }  # Per building/floor
  lock_types: ["door", "keycard", "power", "barricade"]
  key_distance: { min: 2, max: 5 }  # Rooms between key and its lock
  red_herring_keys: 0.2              # 20% extra keys that don't unlock anything (exploration reward)
  backtrack_tolerance: 3              # Max rooms of backtracking to reach a key
```

**Generation Algorithm:**
1. Get room graph from spatial registry (rooms as nodes, openings as edges)
2. Identify critical path (BFS shortest from entrance to objective)
3. Select N edges on critical path as lock positions
4. For each lock (in reverse order -- deepest first):
   a. Compute reachable rooms from start WITHOUT this lock
   b. Place key in a reachable room (preferring thematic match: keycard near office, fuse near electrical)
   c. Verify all previously placed keys are still reachable
5. Place non-critical locks on side paths (reward exploration)
6. Update spatial registry with lock/key metadata
7. PCG places physical lock/key meshes at designated positions


---

## 11. Difficulty Scaling

### Difficulty Parameters

A unified difficulty system should control PCG density parameters across all gameplay elements:

```
FDifficultyProfile:
  name: "normal"

  # Items
  item_density_multiplier: 1.0      # 1.0 = baseline
  health_item_multiplier: 1.0
  ammo_multiplier: 1.0
  guaranteed_floor_health: 2        # Min health items per floor

  # Enemies
  enemy_density_multiplier: 1.0
  enemy_aggression: 0.5             # 0=passive, 1=maximum
  patrol_territory_size: 6          # Rooms per patrol route
  respawn_enabled: false

  # Obstacles
  obstacle_density: 0.5             # Corridor blockage rate
  breakable_ratio: 0.6              # % of obstacles that are breakable

  # Scares
  scare_count_multiplier: 1.0
  scare_intensity: 0.7              # Visual/audio intensity
  false_alarm_ratio: 0.3

  # Locks/Keys
  lock_count: 4
  key_distance: 3                   # Rooms between key and lock

  # Pacing
  safe_room_frequency: 8            # Max rooms between safe rooms
  director_menace_ceiling: 0.7      # Max tension before forced relief
  director_relief_duration: 30      # Seconds of guaranteed safety
```

### Dynamic Difficulty Adjustment (DDA)

**RE4 Model (recommended):**
Track a hidden internal rank (1-10) based on:
- Deaths (rank -1 per death)
- Damage taken per encounter (high = rank down)
- Headshot accuracy (high = rank up)
- Health kit usage rate (high = rank down)
- Time between save points (long stretches without saving = rank down slightly)
- Ammo efficiency (rank up if conservative)

**Reactive Item Drops (RE4 style):**
- Low health + no health items = increase health drop chance by 2x
- Low ammo for equipped weapon = increase ammo drop for that weapon
- Player favoring one weapon = reduce that ammo, increase others
- Never drop below "survival minimum" -- always at least 1 health item per 3 rooms

**Director-Controlled Parameters:**
The AI Director adjusts PCG parameters at runtime:
- Menace gauge high for too long -> spawn extra ammo in next room
- Player at critical health -> next enemy spawn point disabled
- Player breezing through -> reduce relief duration, increase patrol territory
- Multiple deaths at same encounter -> reduce enemy count or move spawn points

### Hospice Accessibility Profile

```
FDifficultyProfile:
  name: "hospice_comfort"

  item_density_multiplier: 2.0      # Generous resources
  health_item_multiplier: 3.0       # Abundant healing
  ammo_multiplier: 2.0
  guaranteed_floor_health: 5

  enemy_density_multiplier: 0.5     # Fewer enemies
  enemy_aggression: 0.2             # Very passive
  patrol_territory_size: 3          # Small patrol areas

  obstacle_density: 0.2             # Fewer blocks
  breakable_ratio: 0.9              # Almost all breakable

  scare_count_multiplier: 0.5       # Fewer scares
  scare_intensity: 0.3              # Gentle scares
  false_alarm_ratio: 0.6            # Most scares are just atmosphere

  lock_count: 2                     # Simpler progression
  key_distance: 2                   # Keys near their locks

  safe_room_frequency: 5            # Frequent safe rooms
  director_menace_ceiling: 0.4      # Very low tension cap
  director_relief_duration: 60      # Long relief periods
```


---

## 12. NavMesh Integration

### The Problem

PCG-placed objects MUST not break navigation. Known issues in UE5:
- PCG Instanced Static Meshes don't always trigger navmesh rebuilds
- Procedural meshes may be ignored by navmesh if collision is misconfigured
- Runtime-placed obstacles need dynamic navmesh updates
- `bCanEverAffectNavigation` must be true on spawned components

### Solution Architecture

**Build-Time (Editor):**
1. PCG generates all static gameplay elements (furniture, obstacles, cover points)
2. Auto-collision system ensures all meshes have proper collision (see auto-collision research)
3. Full navmesh rebuild after PCG generation completes
4. Validate: flood-fill from every door to ensure full connectivity
5. Validate: capsule sweep through every doorway (84cm player capsule width)

**Runtime (Dynamic Elements):**
1. Use `bUseDynamicNavMesh = true` for runtime nav updates
2. Enemy spawns/deaths: no nav impact (AI has separate nav query)
3. Barricade destruction: mark navmesh dirty at barricade location
4. Door state changes: use NavLinkProxy for openable doors (bidirectional when open, blocked when locked)

### NavMesh Modifiers for Gameplay

| Element | NavMesh Treatment |
|---------|-------------------|
| Standard floor | Default walkable |
| Obstacle (hard) | NavArea_Null (blocked) |
| Obstacle (breakable) | NavArea_Null initially; remove on destruction |
| Slow zone (water/debris) | Custom NavArea with cost 3.0 |
| Safe room boundary | NavArea_Null for enemy nav filter (enemies can't enter) |
| Crawl space | NavArea_LowHeight (crouch only) |
| Ladder/drop | NavLinkProxy (one-way or bidirectional) |
| Locked door | NavArea_Null until unlocked, then default |

### PCG + NavMesh Pipeline

```
1. Generate building geometry (Monolith create_structure/create_building)
2. Generate collision (auto-collision system)
3. Place gameplay elements via PCG (furniture, obstacles, items)
4. Place volume modifiers (auto_volumes_for_building)
5. Rebuild navmesh (build_navmesh action)
6. Validate connectivity (flood-fill from entrance to all rooms)
7. Fix violations (remove obstacles blocking only path to rooms)
8. Final navmesh rebuild if fixes applied
```

### Safe Room NavMesh Isolation

For enemy AI, safe rooms should be unreachable via navmesh:
- Place `ANavModifierVolume` covering safe room interior
- Set `AreaClass = UNavArea_Null` with a **custom nav filter** for enemies only
- Player navigation uses default filter (can enter safe rooms)
- Enemy StateTree/BT uses filtered pathfinding that excludes safe room nav areas
- Alternatively: use NavAgentProperties with separate supported agents for player vs enemy


---

## 13. Unified Architecture

### Three-Layer System

The gameplay element placement system operates in three layers:

```
Layer 1: SPATIAL REGISTRY (data)
  - Room graph with adjacency
  - Room types, sizes, connections
  - Lock/key dependency graph
  - Safe room flags
  Input: Building generation output
  Output: Structured spatial data for all other layers

Layer 2: PCG PLACEMENT (generation)
  - Static element scatter (furniture, obstacles, items, spawn points)
  - Rule-based filtering (room type, difficulty, light level)
  - Weighted random selection from loot/spawn tables
  - NavMesh-aware placement (don't block paths)
  Input: Spatial registry data + difficulty profile + Data Asset tables
  Output: Placed actors with metadata

Layer 3: AI DIRECTOR (runtime)
  - Dynamic activation of dormant spawn points
  - Reactive item drop adjustments
  - Menace gauge pacing control
  - DDA parameter tuning
  - Scare trigger activation/cooldown
  Input: Player state + placed element metadata + difficulty profile
  Output: Runtime modifications to placed elements
```

### Data Flow

```
Building Generation
       |
       v
Spatial Registry (rooms, adjacency, types)
       |
       +---> Lock/Key Generator ---> locked doors + key placements
       |
       +---> Safe Room Designator ---> safe room flags
       |
       v
PCG Gameplay Pass
       |
       +---> Item Scatter (loot tables per room type)
       +---> Enemy Spawn Points (placement rules, EQS validation)
       +---> Cover Points (geometry analysis)
       +---> Obstacle Placement (corridor blockage, nav modifiers)
       +---> Scare Triggers (corner/door/window positions)
       +---> Smart Object Placement (patrol/search/ambush/vent)
       +---> Patrol Route Generation (room graph traversal)
       |
       v
Volume Generation (auto_volumes)
       |
       v
NavMesh Build + Validation
       |
       v
AI Director Config (menace thresholds, pacing curves, DDA params)
       |
       v
RUNTIME: Director modulates all placed elements
```

### Integration with Existing Monolith Systems

| Existing System | Integration Point |
|----------------|-------------------|
| Spatial Registry | Room data source for all placement decisions |
| Auto-Collision | Ensures placed meshes have proper collision for navmesh |
| Auto-Volumes | NavMesh bounds, audio volumes, PP volumes after placement |
| Proc Mesh Caching | Cache PCG output meshes for re-use |
| classify_zone_tension | Tension scoring feeds scare trigger intensity |
| analyze_room_acoustics | Audio analysis informs Smart Object audio behaviors |
| predict_player_paths | Critical path prediction for item/objective placement |


---

## 14. Proposed MCP Actions

### New Actions (16 total)

**Gameplay Setup (5 actions):**
1. `populate_building_gameplay` -- master action: runs full gameplay element pass on a building
   - Params: `building_id`, `difficulty_profile`, `seed`
   - Calls all sub-actions in correct order
   - Returns summary of all placed elements

2. `place_items_in_rooms` -- scatter items based on room-type loot tables
   - Params: `building_id`, `loot_table` (DA ref or JSON), `difficulty_multiplier`
   - Returns placed item list with positions

3. `place_enemy_spawn_points` -- generate spawn point markers
   - Params: `building_id`, `enemy_types[]`, `density`, `placement_rules`
   - Returns spawn point list with categories and metadata

4. `place_scare_triggers` -- generate jump scare trigger volumes
   - Params: `building_id`, `scare_budget`, `intensity_curve`
   - Returns trigger list with scare types

5. `generate_lock_key_progression` -- create lock/key dependency graph
   - Params: `building_id`, `num_locks`, `lock_types[]`, `key_distance`
   - Returns lock/key mapping with room assignments

**AI Setup (4 actions):**
6. `generate_patrol_routes` -- create AI patrol paths from room graph
   - Params: `building_id`, `num_patrols`, `patrol_types[]`, `territory_size`
   - Returns patrol route data (room sequences + navmesh paths)

7. `place_smart_objects` -- spawn Smart Object actors alongside furniture/geometry
   - Params: `building_id`, `so_definitions{}` (type -> DA mapping)
   - Returns placed SO list

8. `place_cover_points` -- analyze geometry for tactical cover positions
   - Params: `building_id`, `cover_density`, `min_cover_height`
   - Returns cover point list with classification

9. `configure_ai_director` -- set up Director parameters for a building/level
   - Params: `menace_config`, `pacing_curve`, `dda_params`
   - Returns Director config applied

**Obstacle/Environment (3 actions):**
10. `place_barricades` -- generate corridor obstacles and blockages
    - Params: `building_id`, `obstacle_density`, `breakable_ratio`, `critical_path_protection`
    - Returns obstacle list with nav modifier data

11. `designate_safe_rooms` -- mark rooms as safe zones with appropriate rules
    - Params: `building_id`, `safe_room_count`, `max_distance_between`
    - Returns safe room list with modified PCG parameters

12. `place_objectives` -- place quest items, collectibles, and progression items
    - Params: `building_id`, `objective_manifest[]`, `placement_rules`
    - Returns objective placement map

**Validation (3 actions):**
13. `validate_gameplay_layout` -- verify solvability, connectivity, difficulty balance
    - Params: `building_id`
    - Returns validation report (solvable path, item coverage, enemy distribution)

14. `validate_navmesh_connectivity` -- flood-fill verification of all rooms reachable
    - Params: `building_id`, `capsule_radius` (default 42cm)
    - Returns connectivity report (blocked rooms, narrow passages)

15. `visualize_gameplay_elements` -- debug visualization of all placed elements
    - Params: `building_id`, `show_items`, `show_spawns`, `show_patrols`, `show_scares`, `show_locks`
    - Returns actor count (debug lines/markers spawned)

**Query (1 action):**
16. `query_gameplay_elements` -- query placed elements by type, room, or region
    - Params: `building_id`, `element_type`, `room_filter`, `bounds_filter`
    - Returns matching element list


---

## 15. Implementation Plan

### Dependencies

- **Required (P0):** Spatial Registry (room graph, adjacency queries, BFS pathfinding)
- **Required (P0):** Auto-Collision (meshes need collision for navmesh)
- **Required (P0):** Auto-Volumes (navmesh bounds, audio volumes)
- **Helpful (P1):** classify_zone_tension (tension scoring for scare placement)
- **Helpful (P1):** predict_player_paths (critical path for item/objective placement)
- **Optional (P2):** PCG framework integration (for scatter; can use manual spawn initially)

### Phase 1: Core Data + Lock/Key (24-32h)

- Difficulty profile Data Asset schema
- Loot table Data Asset schema (room_type -> weighted item list)
- Lock/key generation algorithm on spatial registry graph
- `generate_lock_key_progression` action
- `designate_safe_rooms` action
- `validate_gameplay_layout` (solvability check)
- Unit tests for lock/key solvability guarantee

### Phase 2: Item + Enemy Placement (28-36h)

- `place_items_in_rooms` with room-type loot tables
- `place_enemy_spawn_points` with placement rules (light, visibility, room type)
- `place_cover_points` with geometry analysis
- Spawn point metadata system (category, activation state, cooldown)
- EQS integration for light-level and visibility queries
- NavMesh validation for placed elements

### Phase 3: AI Integration (24-32h)

- `generate_patrol_routes` from room graph
- `place_smart_objects` alongside furniture/geometry
- `configure_ai_director` with menace/pacing/DDA params
- Smart Object Definition Data Assets (patrol, search, ambush, door, vent)
- Patrol route debug visualization

### Phase 4: Scares + Obstacles (20-28h)

- `place_scare_triggers` with scare budget system
- `place_barricades` with critical path protection
- Scare type classification (corner, door, window, backtrack)
- Obstacle navmesh modifier integration
- False alarm system for scare triggers

### Phase 5: Master Action + Validation (16-24h)

- `populate_building_gameplay` orchestration action
- `validate_navmesh_connectivity` with flood-fill
- `visualize_gameplay_elements` debug view
- `query_gameplay_elements` query action
- DDA parameter tuning pass
- Hospice accessibility profile preset

### Phase 6: PCG Graph Integration (20-28h)

- Convert item scatter to PCG graph (weighted mesh spawner + room type filter)
- Convert obstacle scatter to PCG graph (density gradient + physics settle)
- PCG-driven Smart Object co-placement with furniture
- Runtime PCG generation mode for Director-triggered item spawns
- PCGEx integration for constraint-based placement rules

### Total Estimate: ~132-180h (6 phases)

### Priority Matrix

| Priority | Actions | Rationale |
|----------|---------|-----------|
| P0 | lock_key, safe_rooms, validate_layout | Solvability is non-negotiable |
| P0 | place_items, place_enemy_spawns | Core gameplay loop |
| P1 | patrol_routes, smart_objects, cover_points | AI needs these for believable behavior |
| P1 | barricades, validate_navmesh | Navigation integrity |
| P2 | scare_triggers, ai_director_config | Polish layer |
| P2 | populate_building (master), visualize, query | Workflow efficiency |
| P3 | PCG graph conversion | Performance optimization for large-scale generation |


---

## 16. Sources

### AI Director / Pacing Systems
- [The Director - Left 4 Dead Wiki](https://left4dead.fandom.com/wiki/The_Director)
- [The AI Director: How L4D2's Adaptive System Works](https://xengamer.com/content/the-ai-director-how-left-4-dead-2s-adaptive)
- [The Perfect Organism: The AI of Alien: Isolation](https://www.gamedeveloper.com/design/the-perfect-organism-the-ai-of-alien-isolation)
- [Revisiting the AI of Alien: Isolation](https://www.gamedeveloper.com/design/revisiting-the-ai-of-alien-isolation)
- [The Illusion of Intelligence: Alien Isolation AI Breakdown](https://medium.com/@aetosdios27/the-illusion-of-intelligence-a-technical-breakdown-of-alien-isolations-ai-b2d7c9927d02)
- [Steam Guide: Understanding the AI Director](https://steamcommunity.com/sharedfiles/filedetails/?id=147309463)

### Lock/Key and Dungeon Generation
- [Metazelda: Procedural Zelda-like Dungeon Generator](https://github.com/tcoxon/metazelda)
- [Introduction to Procedural Lock and Key Dungeon Generation](https://shaggydev.com/2021/12/17/lock-key-dungeon-generation/)
- [Unexplored's Secret: Cyclic Dungeon Generation](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-)
- [Dungeon Generation in Unexplored (Boris the Brave)](https://www.boristhebrave.com/2021/04/10/dungeon-generation-in-unexplored/)
- [Rooms and Mazes (Robert Nystrom)](https://journal.stuffwithstuff.com/2014/12/21/rooms-and-mazes/)
- [Graph Rewriting for Procedural Level Generation](https://www.boristhebrave.com/2021/04/02/graph-rewriting/)
- [Procedural Dungeon Generation Algorithm (Gamedeveloper)](https://www.gamedeveloper.com/programming/procedural-dungeon-generation-algorithm)
- [GraphDungeonGenerator (Zelda-1 style)](https://github.com/amidos2006/GraphDungeonGenerator)
- [Procedural Puzzle Generation: A Survey (PDF)](https://www.academia.edu/101157774/Procedural_Puzzle_Generation_A_Survey)
- [On Graph Grammars and Games (2024 arXiv)](https://arxiv.org/pdf/2403.07607)

### Safe Room Design
- [The Safe Room: How Game Designers Create Horror](https://thegamesedge.com/the-safe-room-how-game-designers-create-horror/)
- [Safe Rooms In Horror Games (PekoeBlaze)](https://pekoeblaze.wordpress.com/2021/07/05/safe-rooms-in-horror-games/)
- [Unsafe Rooms (EGM)](https://egmnow.com/unsafe-rooms/)
- [From Nemesis to Xenomorph: Fear and Safety in Horror Games](https://www.gtogg.com/News/en/From_Nemesis_to_Xenomorph_the_concept_of_fear_and_safety_in_horror_games/)
- [The Use and Misuse of Safe Spaces in Survival Horror (BCMCR)](https://bcmcr.org/research/the-use-and-misuse-of-safe-spaces-in-survival-horror-video-games/)

### Jump Scare / Horror Level Design
- [Creating Horror through Level Design (Gamedeveloper)](https://www.gamedeveloper.com/design/creating-horror-through-level-design-tension-jump-scares-and-chase-sequences)
- [A Lack of Fright: Jump Scare Horror Game Design](https://www.gamedeveloper.com/design/a-lack-of-fright-examining-jump-scare-horror-game-design)
- [Horror Level Design Part 3: Environment and Story](https://www.worldofleveldesign.com/categories/level_design_tutorials/horror-fear-level-design/part3-survival-horror-level-design-story-environment.php)

### Cover / Tactical Placement
- [CoverGenerator-UE4 (Deams51)](https://github.com/Deams51/CoverGenerator-UE4)
- [EQS Quick Start (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-quick-start-in-unreal-engine)
- [EQS User Guide (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/environment-query-system-user-guide-in-unreal-engine)
- [AI FPS Tactical Shooter (mtrebi)](https://github.com/mtrebi/AI_FPS)

### UE5 PCG Framework
- [PCG Overview (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [PCG Node Reference (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-framework-node-reference-in-unreal-engine)
- [PCG Generation Modes (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-pcg-generation-modes-in-unreal-engine)
- [A Tech Artist's Guide to PCG](https://dev.epicgames.com/community/learning/knowledge-base/KP2D/unreal-engine-a-tech-artists-guide-to-pcg)
- [PCGExtendedToolkit (PCGEx)](https://github.com/PCGEx/PCGExtendedToolkit)
- [Hyper Procedural Props Spawner](https://gamesbyhyper.com/product/hyper-procedural-props-spawner/)
- [PCG Basics: First Scatter System (2026)](https://medium.com/@sarah.hyperdense/pcg-basics-your-first-procedural-scatter-system-in-ue5-fab626e1d6f0)

### Smart Objects
- [Smart Objects Overview (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---overview)
- [Smart Objects Quick Start (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/smart-objects-in-unreal-engine---quick-start)
- [Smart Objects and You in UE5 (Ostap Leonov)](https://bigm227.medium.com/smart-objects-and-you-in-ue5-pt-1-what-is-smart-object-a9d3e579a077)

### NavMesh + Procedural Content
- [NavMesh Modification Overview (UE 5.7)](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-how-to-modify-the-navigation-mesh-in-unreal-engine)
- [PCG ISM Not Updating NavMesh (Forum)](https://forums.unrealengine.com/t/pcg-instanced-static-mesh-not-updating-nav-mesh/2667327)
- [NPC Navigation Through Destructible Geometry](https://www.stevestreeting.com/2025/09/05/ue-npc-navigation-through-destructible-geometry/)

### Difficulty / DDA
- [Dynamic Difficulty Adjustment: Concepts and Applications (IntechOpen)](https://www.intechopen.com/chapters/1228576)
- [Game Changers: Dynamic Difficulty (Gamedeveloper)](https://www.gamedeveloper.com/design/game-changers-dynamic-difficulty)
- [RE4 Difficulty System (Fandom Wiki)](https://residentevil.fandom.com/wiki/Difficulty)

### Other Procedural Tools
- [Dungeon Architect](https://dungeonarchitect.dev/)
- [ProceduralDungeon UE Plugin (BenPyton)](https://github.com/BenPyton/ProceduralDungeon)
- [Procedural Room Layout Tool (RAGE)](https://theragegames.itch.io/procedural-environment-asset-placement)
- [Hierarchical Procedural Decoration (Thesis PDF)](https://www.diva-portal.org/smash/get/diva2:1479952/FULLTEXT01.pdf)

### Existing Monolith Research (Cross-References)
- `reference_pcg_framework_research.md` -- PCG architecture, API, verdict
- `reference_spatial_registry_research.md` -- Room graph, adjacency, BFS
- `reference_auto_volumes_research.md` -- NavMesh/Audio/PP volume generation
- `reference_horror_ai_archetypes.md` -- 12 enemy AI archetypes, Director systems
- `reference_horror_ai_systems.md` -- AI in 11 horror games, menace gauge, DDA
- `reference_utility_ai_research.md` -- Utility AI, StateTree integration
- `reference_door_clearance_research.md` -- Player capsule 42cm radius, clearance
- `reference_procgen_validation_research.md` -- Connectivity validation, capsule sweeps
