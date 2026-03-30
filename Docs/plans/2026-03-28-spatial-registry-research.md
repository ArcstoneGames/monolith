# Spatial Registry / Building Coordinate System for Procedural City Blocks

**Date:** 2026-03-28
**Scope:** New Monolith data structure for hierarchical spatial tracking of procedurally generated buildings, rooms, openings, furniture, and street elements.
**Goal:** Give AI agents (both gameplay AI and LLM construction agents) a queryable spatial model of what exists where, persisted alongside the existing ProceduralCache manifest.

---

## 1. Problem Statement

When an AI agent builds a city block procedurally via Monolith actions (`create_structure`, `create_building_shell`, `create_parametric_mesh`, etc.), each call is stateless. The agent must manually track:

- Which rooms belong to which building
- Where doors connect rooms
- Which floor a room is on
- What furniture is in each room
- Where buildings sit on the block
- Where street elements are

The existing ProceduralCache manifest (`Saved/Monolith/ProceduralCache/manifest.json`) tracks individual meshes by param hash but has zero relational or spatial data. It knows "a room mesh exists at /Game/CityBlock/Mesh/SM_CornerShop_Main" but not "that room is on floor 1 of the Corner Shop, connected to the Storage room via a door on the north wall."

The existing blockout layout export (`export_blockout_layout`) captures normalized positions within a volume but has no hierarchy, no adjacency, no opening tracking.

**What we need:** A hierarchical spatial descriptor that captures the full topology of a procedurally generated environment, persists to disk as JSON, and supports spatial queries.

---

## 2. JSON Building Descriptor Schema

### 2.1 Top-Level: Block Descriptor

```json
{
    "version": 1,
    "block_id": "city_block_01",
    "created_utc": "2026-03-28T10:00:00Z",
    "modified_utc": "2026-03-28T10:30:00Z",

    "origin": [0, 0, 0],
    "bounds": { "min": [-5000, -3000, 0], "max": [5000, 3000, 3000] },

    "buildings": [ ... ],
    "streets": [ ... ],
    "street_furniture": [ ... ],

    "metadata": {
        "generator": "monolith_ai",
        "seed": 42,
        "style": "horror_suburban",
        "notes": "Two-story clinic with exam rooms, corner shop, apartments"
    }
}
```

### 2.2 Building

Each building has a world-space origin, footprint, and a hierarchy of floors containing rooms.

```json
{
    "building_id": "corner_shop",
    "label": "Corner Shop",
    "tags": ["commercial", "horror_entry"],

    "origin": [-3000, 0, 0],
    "rotation": 0,
    "footprint": [[0,0], [800,0], [800,600], [0,600]],
    "wall_thickness": 15,

    "shell_asset": "/Game/CityBlock/Mesh/SM_CornerShop_Shell",
    "shell_cache_hash": "3043af33c7b2f77b",

    "floors": [
        {
            "floor_index": 0,
            "floor_z": 0,
            "ceiling_z": 270,
            "floor_height": 270,
            "rooms": [ ... ]
        }
    ],

    "entrances": [
        {
            "type": "door",
            "wall": "south",
            "world_position": [-3150, 300, 0],
            "connects_to_street": "main_street",
            "opening_ref": "corner_shop/F0/main_room/south_door_0"
        }
    ],

    "stairs": []
}
```

### 2.3 Room

Rooms are the fundamental spatial unit. Each room has local coordinates relative to its building origin, and world coordinates computed on the fly (building_origin + local_offset).

```json
{
    "room_id": "main_room",
    "label": "Shop Floor",
    "tags": ["retail", "public", "lit"],
    "function": "retail",

    "local_offset": [0, 0, 0],
    "dimensions": { "width": 800, "depth": 600, "height": 270 },
    "wall_thickness": 15,

    "asset_path": "/Game/CityBlock/Mesh/SM_CornerShop_Main",
    "cache_hash": "3043af33c7b2f77b",
    "actor_name": "SM_CornerShop_Main_0",

    "openings": [
        {
            "opening_id": "south_door_0",
            "type": "door",
            "wall": "south",
            "width": 150,
            "height": 240,
            "offset_x": -150,
            "offset_z": 0,
            "local_position": [-150, 300, 0],
            "connects_to": "exterior",
            "is_entrance": true
        },
        {
            "opening_id": "south_window_0",
            "type": "window",
            "wall": "south",
            "width": 200,
            "height": 150,
            "offset_x": 150,
            "offset_z": 80,
            "local_position": [150, 300, 80],
            "connects_to": "exterior"
        },
        {
            "opening_id": "north_door_0",
            "type": "door",
            "wall": "north",
            "width": 90,
            "height": 220,
            "offset_x": 0,
            "offset_z": 0,
            "local_position": [0, -300, 0],
            "connects_to": "storage_room"
        }
    ],

    "furniture": [
        {
            "furniture_id": "counter_0",
            "type": "counter",
            "asset_path": "/Game/Generated/Parametric/counter/SM_counter_100x100x100_f92a79",
            "local_position": [200, -100, 0],
            "rotation": [0, 0, 0],
            "actor_name": "SM_Counter_0"
        },
        {
            "furniture_id": "shelf_0",
            "type": "shelf",
            "asset_path": "/Game/Generated/Parametric/shelf/SM_shelf_100x100x100_af99ed",
            "local_position": [-300, 0, 0],
            "rotation": [0, 90, 0],
            "actor_name": "SM_Shelf_0"
        }
    ],

    "horror_props": [
        {
            "prop_id": "broken_wall_0",
            "type": "broken_wall",
            "asset_path": "/Game/CityBlock/Mesh/SM_BrokenWall_A",
            "local_position": [350, 200, 0],
            "rotation": [0, 0, 0]
        }
    ]
}
```

### 2.4 Stairwell Connection

Stairs connect floors within a building. They reference the rooms they connect.

```json
{
    "stair_id": "main_stairs",
    "type": "straight",
    "asset_path": "/Game/CityBlock/Mesh/SM_Stairs_Full",
    "local_position": [100, 200, 0],
    "connects_floors": [0, 1],
    "connects_rooms": ["F0/hallway", "F1/hallway"],
    "width": 100,
    "step_count": 15
}
```

### 2.5 Street

Streets are linear or polygonal regions at ground level with their own coordinate system.

```json
{
    "street_id": "main_street",
    "label": "Main Street",
    "type": "road",
    "centerline": [[-5000, -200, 0], [5000, -200, 0]],
    "width": 800,
    "surface": "asphalt",

    "segments": [
        {
            "segment_id": "seg_0",
            "start": [-5000, -200, 0],
            "end": [0, -200, 0],
            "lane_count": 2,
            "has_sidewalk": true,
            "sidewalk_width": 150
        }
    ],

    "intersections": [
        {
            "intersection_id": "int_0",
            "center": [0, -200, 0],
            "connects_streets": ["main_street", "side_alley"],
            "type": "T"
        }
    ]
}
```

### 2.6 Street Furniture

Individual objects placed along streets -- lamp posts, benches, mailboxes, parking spots, dumpsters.

```json
{
    "item_id": "lamp_post_0",
    "type": "lamp_post",
    "world_position": [-3500, -400, 0],
    "rotation": [0, 0, 0],
    "asset_path": "/Game/Generated/Parametric/pillar/SM_pillar_15x15x350_9324eb",
    "actor_name": "LampPost_0",
    "street_ref": "main_street",
    "tags": ["lighting", "metal"]
}
```

---

## 3. Room Adjacency Graph

The JSON descriptor implicitly encodes a graph through opening `connects_to` fields. We extract an explicit adjacency graph for fast queries.

### 3.1 Graph Structure

```
Nodes = rooms (including virtual "exterior" and "street" nodes)
Edges = openings (doors, vents, windows)

Each edge has:
  - type: door | window | vent | stairwell
  - width (traversal constraint)
  - bidirectional: true for all types
  - traversable: true for doors/stairs, false for windows/vents (AI pathfinding)
  - locked: optional gameplay state
```

### 3.2 Example Adjacency for Corner Shop

```
[exterior] --door(150cm)--> [main_room] --door(90cm)--> [storage_room]
                                |
                            door(75cm)
                                |
                                v
                          [bathroom]
[exterior] --window(200cm)--> [main_room]  (non-traversable)
```

### 3.3 In-Memory Representation (C++)

```cpp
struct FSpatialNode
{
    FString NodeId;        // "corner_shop/F0/main_room"
    FString BuildingId;
    int32 FloorIndex;
    FString RoomId;
    FBox BoundsLocal;      // In building-local coords
    FBox BoundsWorld;      // Computed: building_origin + local
    TArray<FString> Tags;
};

struct FSpatialEdge
{
    FString EdgeId;        // "corner_shop/F0/main_room/north_door_0"
    FString FromNode;
    FString ToNode;
    FString OpeningType;   // door, window, vent, stairwell
    float Width;
    bool bTraversable;     // AI can walk through
    FVector WorldPosition;
};

struct FSpatialGraph
{
    TMap<FString, FSpatialNode> Nodes;
    TArray<FSpatialEdge> Edges;

    // Fast lookups
    TMultiMap<FString, int32> NodeToEdgeIndices;
};
```

### 3.4 Graph Queries

The adjacency graph enables these queries without scanning the world:

| Query | Implementation |
|-------|---------------|
| "What room is at position X,Y,Z?" | Iterate nodes, point-in-AABB test on BoundsWorld |
| "What's adjacent to the kitchen?" | NodeToEdgeIndices lookup, filter by traversable |
| "Show me all rooms on floor 2" | Filter nodes by FloorIndex == 2 |
| "Where are the exits?" | Find edges where ToNode == "exterior" |
| "Path from room A to room B" | BFS/Dijkstra on traversable edges |
| "Which rooms can the AI reach from here?" | BFS flood fill on traversable edges |
| "What rooms have windows to exterior?" | Find edges with type=window and ToNode=exterior |
| "Closest safe room to position" | BFS from position's room, filter by tag "safe" |

---

## 4. Coordinate Conventions

### 4.1 Unreal Engine Coordinate System

UE uses left-handed Z-up:
- **X** = forward (north in our convention)
- **Y** = right (east)
- **Z** = up

### 4.2 Building Origin

**Convention: front-left corner at ground level, Z=0.**

Rationale: The existing `create_structure` places rooms with their center at the given location. But for a building descriptor, an origin at (0,0,0) being the front-left ground corner is more intuitive for layout:

```
Building origin (0,0,0) = minimum X, minimum Y, Z=0
                          i.e., the south-west corner at ground level

      N (+X)
      ^
      |
 W ---+---> E (+Y)
      |
      S

Floor 0: Z = 0 to floor_height
Floor 1: Z = floor_height to 2*floor_height
```

All rooms store `local_offset` relative to this building origin. World position = `building.origin + room.local_offset`.

**Important:** The existing `create_structure` uses center-origin for its mesh (walls extend +/- width/2, depth/2 from the placement point). The spatial registry must account for this: when placing a room at local_offset [200, 150, 0] with dimensions 400x300, the mesh placement location should be [200 + 200, 150 + 150, 0] = [400, 300, 0] relative to building origin (center of the room).

Alternatively, we define local_offset as the room's center position, which aligns directly with `create_structure`'s placement convention. **This is simpler and recommended.**

```
room.local_offset = center of room relative to building origin
room.bounds_min = local_offset - dimensions/2
room.bounds_max = local_offset + dimensions/2
```

### 4.3 Room Origin

**Convention: center of room at floor level (Z=0 of that floor).**

This matches `create_structure` behavior exactly -- the mesh is centered at the placement point with Base origin mode. No coordinate translation needed.

### 4.4 Floor Stacking

```
Floor 0: Z range [0, floor_height]
Floor 1: Z range [floor_height, 2 * floor_height]
Floor N: Z range [N * floor_height, (N+1) * floor_height]

Floor slabs are at the bottom of each floor's Z range.
The existing create_building_shell places floor slabs at i * floor_height.
```

### 4.5 Opening Positions

Openings inherit the coordinate convention from `create_structure`:
- `wall: "north"` = the wall at Y = -depth/2 (minimum Y)
- `wall: "south"` = the wall at Y = +depth/2 (maximum Y)
- `wall: "east"` = the wall at X = +width/2 (maximum X)
- `wall: "west"` = the wall at X = -width/2 (minimum X)
- `offset_x` = offset along the wall's run axis from center
- `offset_z` = offset from floor level

The spatial registry computes world positions:

```
For a north wall opening in a room with local_offset [Rx, Ry, Rz]:
  world_pos = building_origin + [Rx + offset_x, Ry - depth/2, Rz + offset_z]
```

---

## 5. Saving / Loading

### 5.1 File Location

```
Plugins/Monolith/Saved/Monolith/SpatialRegistry/
    city_block_01.json
    city_block_01.graph.json     (optional: precomputed adjacency graph)
    hospital_level.json
    ...
```

Follows existing Monolith convention alongside `ProceduralCache/manifest.json` and `Prefabs/`.

### 5.2 Atomic Write

Use the same atomic write pattern as `FMonolithMeshProceduralCache::SaveManifest()`: write to `.tmp`, then rename. This prevents corruption on crash.

### 5.3 File Size Estimate

A city block with:
- 6 buildings, 15 rooms total, 30 openings, 40 furniture items, 20 street furniture items

Pretty-printed JSON: ~25-40 KB. Compact: ~10-15 KB. Negligible.

A large district with 50 buildings, 200 rooms: ~200 KB pretty, ~80 KB compact. Still fine.

### 5.4 Relationship to ProceduralCache Manifest

The spatial registry and the proc cache manifest are **complementary, not overlapping**:

| Concern | ProceduralCache manifest | Spatial Registry |
|---------|-------------------------|------------------|
| Purpose | Dedup mesh generation (param hash -> asset path) | Track spatial relationships and topology |
| Granularity | Individual mesh | Hierarchical: block -> building -> floor -> room -> opening/furniture |
| Spatial data | None (just asset paths) | Full world coordinates, bounds, adjacency |
| Lifecycle | Grows as meshes are generated, validated on access | Created/updated during block construction, loaded for queries |

They cross-reference via `cache_hash` and `asset_path` fields in the spatial registry pointing to manifest entries.

### 5.5 Save/Load API

```cpp
class FMonolithSpatialRegistry
{
public:
    static FMonolithSpatialRegistry& Get();

    /** Load a block descriptor from disk. Returns false if not found. */
    bool LoadBlock(const FString& BlockId, TSharedPtr<FJsonObject>& OutDescriptor);

    /** Save a block descriptor to disk (atomic write). */
    bool SaveBlock(const FString& BlockId, const TSharedPtr<FJsonObject>& Descriptor);

    /** List all saved block descriptors. */
    TArray<FString> ListBlocks();

    /** Delete a block descriptor. */
    bool DeleteBlock(const FString& BlockId);

    /** Build adjacency graph from a block descriptor. */
    FSpatialGraph BuildGraph(const TSharedPtr<FJsonObject>& Descriptor);

    // --- Queries (operate on a loaded block) ---

    /** Find the room containing a world position. */
    FString FindRoomAtPosition(const TSharedPtr<FJsonObject>& Descriptor, const FVector& WorldPos);

    /** Get all rooms adjacent to a given room (via traversable openings). */
    TArray<FString> GetAdjacentRooms(const FSpatialGraph& Graph, const FString& RoomId);

    /** Get all rooms on a specific floor of a building. */
    TArray<FString> GetRoomsOnFloor(const TSharedPtr<FJsonObject>& Descriptor,
        const FString& BuildingId, int32 FloorIndex);

    /** Find all exits (doors to exterior) for a building. */
    TArray<FString> GetBuildingExits(const FSpatialGraph& Graph, const FString& BuildingId);

    /** Find shortest path between two rooms (BFS on adjacency graph). */
    TArray<FString> FindPath(const FSpatialGraph& Graph, const FString& FromRoom, const FString& ToRoom);

    /** Get all rooms matching a tag (e.g., "safe", "horror", "medical"). */
    TArray<FString> GetRoomsByTag(const TSharedPtr<FJsonObject>& Descriptor, const FString& Tag);

private:
    FString GetRegistryDirectory();
    FString GetBlockPath(const FString& BlockId);
};
```

---

## 6. Spatial Queries — MCP Actions

### 6.1 New Actions (10 total)

These would register under the `mesh` namespace alongside existing procedural actions.

| Action | Description | Key Params |
|--------|-------------|------------|
| `save_block_descriptor` | Save a complete block descriptor JSON | `block_id`, `descriptor` (or `buildings[]` + `streets[]` to build it) |
| `load_block_descriptor` | Load and return a saved block descriptor | `block_id` |
| `list_block_descriptors` | List all saved spatial registries | (none) |
| `register_room` | Add/update a room in an existing block | `block_id`, `building_id`, `floor`, `room` object |
| `register_street_furniture` | Add street furniture to a block | `block_id`, `items[]` |
| `query_room_at_position` | Find which room contains a world position | `block_id`, `position: [x,y,z]` |
| `query_adjacent_rooms` | Get rooms connected to a given room | `block_id`, `room_path` (e.g., "corner_shop/F0/main_room") |
| `query_rooms_by_filter` | Filter rooms by floor, building, tags, function | `block_id`, `building_id?`, `floor?`, `tags?`, `function?` |
| `query_building_exits` | Find all exterior doors for a building | `block_id`, `building_id` |
| `query_path_between_rooms` | BFS shortest path between two rooms | `block_id`, `from`, `to`, `traversable_only?` |

### 6.2 Integration With Existing Actions

The real power comes from existing procedural actions **automatically registering** in the spatial registry. Two approaches:

**Approach A: Explicit Registration (Recommended for V1)**

The AI agent calls `create_structure`, gets the result, then calls `register_room` with the spatial metadata. This is simple, requires no changes to existing actions, and gives the agent full control over the hierarchy.

```
1. create_building_shell(footprint, floors=2, ...) -> shell_asset
2. create_structure(type: room, dimensions: {400,300,270}, openings: [...]) -> room_asset
3. register_room(block_id: "city_block_01", building_id: "clinic",
                 floor: 0, room: { room_id: "exam_room", local_offset: [200, 150, 0],
                 dimensions: ..., openings: ..., asset_path: room_asset })
```

**Approach B: Auto-Registration (V2)**

Modify `FinalizeProceduralMesh` to accept an optional `spatial_context` parameter:

```json
{
    "spatial_context": {
        "block_id": "city_block_01",
        "building_id": "clinic",
        "floor": 0,
        "room_id": "exam_room",
        "local_offset": [200, 150, 0],
        "function": "medical"
    }
}
```

When present, `FinalizeProceduralMesh` calls `FMonolithSpatialRegistry::Get().RegisterRoom(...)` automatically. This is convenient but couples procedural generation to the spatial system.

**Recommendation:** Start with Approach A. The AI agent already orchestrates multi-step generation workflows (as seen in the CityBlock manifest entries). Adding `register_room` calls is trivial for the agent. V2 can add the `spatial_context` shortcut.

### 6.3 Example Agent Workflow

```
// Phase 1: Define block
save_block_descriptor({
    block_id: "city_block_01",
    origin: [0, 0, 0],
    buildings: [],
    streets: [{
        street_id: "main_street",
        centerline: [[-5000, -200, 0], [5000, -200, 0]],
        width: 800
    }]
})

// Phase 2: Build structures
create_building_shell(footprint: [[0,0],[800,0],[800,600],[0,600]], floors: 1, ...)
create_structure(type: room, dimensions: {800,600,270}, openings: [...], ...)
create_structure(type: room, dimensions: {400,350,270}, openings: [...], ...)

// Phase 3: Register in spatial registry
register_room({ block_id: "city_block_01", building_id: "corner_shop",
    floor: 0, room: { room_id: "main_room", ... openings with connects_to ... }})
register_room({ block_id: "city_block_01", building_id: "corner_shop",
    floor: 0, room: { room_id: "storage", ... }})

// Phase 4: Query
query_adjacent_rooms({ block_id: "city_block_01",
    room_path: "corner_shop/F0/main_room" })
// Returns: ["corner_shop/F0/storage", "exterior"]

query_room_at_position({ block_id: "city_block_01", position: [-2800, 100, 135] })
// Returns: { building: "corner_shop", floor: 0, room: "main_room" }
```

---

## 7. Street Coordinate System

### 7.1 Street-Local Coordinates

For placing objects along a street, we define a parametric coordinate along the centerline:

```
t = 0.0 at street start
t = 1.0 at street end
lateral_offset = distance from centerline (+Y = right when facing direction of travel)
```

Street furniture positions can be specified either as:
- World coordinates: `[x, y, z]` (absolute)
- Street-parametric: `{ street_id, t, lateral_offset }` (resolved to world coords)

### 7.2 Street Furniture Categories

| Category | Examples | Placement Rules |
|----------|----------|----------------|
| Lighting | Lamp posts, neon signs | Sidewalk edge, regular spacing (15-25m) |
| Seating | Benches, bus stops | Sidewalk, near intersections/buildings |
| Barriers | Bollards, fences, barricades | Road/sidewalk boundary |
| Utilities | Mailboxes, fire hydrants, dumpsters | Sidewalk, near building entrances |
| Parking | Cars, parking meters | Road edge, parallel or perpendicular |
| Vegetation | Trees, planters | Sidewalk edge, tree pits |
| Horror | Blood stains, debris, abandoned items | Freeform, registered for AI awareness |

### 7.3 Street Furniture Placement Zones

The street descriptor can define placement zones for automated distribution:

```json
{
    "street_id": "main_street",
    "placement_zones": [
        {
            "zone_id": "north_sidewalk",
            "type": "sidewalk",
            "bounds": { "t_start": 0.0, "t_end": 1.0, "lateral_min": 250, "lateral_max": 400 },
            "allowed_categories": ["lighting", "seating", "utilities", "vegetation"]
        },
        {
            "zone_id": "parking_lane",
            "type": "parking",
            "bounds": { "t_start": 0.2, "t_end": 0.8, "lateral_min": -400, "lateral_max": -300 },
            "allowed_categories": ["parking"]
        }
    ]
}
```

---

## 8. Visualization — Debug Geometry

### 8.1 MCP Action: `visualize_spatial_registry`

Renders the spatial graph as debug draw commands in the editor viewport.

| Element | Visual | Color |
|---------|--------|-------|
| Room bounds | Wireframe box | Green (normal), Yellow (safe room), Red (horror zone) |
| Door connections | Line from room center to door position to adjacent room center | White |
| Window openings | Small wireframe quad at opening position | Cyan |
| Vent openings | Small wireframe quad | Orange |
| Stair connections | Vertical line between connected floors | Magenta |
| Building bounds | Wireframe box (outer) | Blue |
| Street centerlines | Polyline | Gray |
| Street furniture | Point markers with type label | Various |
| Room labels | Text at room center | White |

### 8.2 Implementation

Use `DrawDebugBox`, `DrawDebugLine`, `DrawDebugString` from `DrawDebugHelpers.h`. These persist for a configurable duration (default: 30 seconds) or until `FlushPersistentDebugLines()`.

```cpp
// Pseudocode for room visualization
for (const auto& Room : Rooms)
{
    FBox WorldBounds = Room.GetWorldBounds(BuildingOrigin);
    DrawDebugBox(World, WorldBounds.GetCenter(), WorldBounds.GetExtent(),
        FColor::Green, /*bPersistent=*/true, Duration);
    DrawDebugString(World, WorldBounds.GetCenter(), Room.Label, nullptr,
        FColor::White, Duration);
}

// Door connections
for (const auto& Edge : Graph.Edges)
{
    if (Edge.OpeningType == "door")
    {
        FVector FromCenter = Nodes[Edge.FromNode].BoundsWorld.GetCenter();
        FVector ToCenter = Nodes[Edge.ToNode].BoundsWorld.GetCenter();
        DrawDebugLine(World, FromCenter, Edge.WorldPosition, FColor::White, true, Duration);
        DrawDebugLine(World, Edge.WorldPosition, ToCenter, FColor::White, true, Duration);
    }
}
```

### 8.3 Visualization Parameters

```json
{
    "block_id": "city_block_01",
    "show_rooms": true,
    "show_doors": true,
    "show_windows": false,
    "show_furniture": false,
    "show_streets": true,
    "show_labels": true,
    "duration": 60.0,
    "filter_building": "clinic",
    "filter_floor": 1,
    "color_by": "function"
}
```

`color_by` options:
- `"function"` -- retail=blue, medical=green, residential=yellow, storage=gray, horror=red
- `"building"` -- each building gets a distinct hue
- `"floor"` -- gradient from blue (ground) to red (top)
- `"connectivity"` -- rooms with more connections are brighter

---

## 9. Integration With Horror AI Systems

The spatial registry is directly useful for horror gameplay AI:

### 9.1 AI Director Integration

The horror AI director (see prior research: `reference_horror_ai_systems.md`, `reference_horror_ai_archetypes.md`) can query the spatial registry to:

- **Choose ambush rooms:** Rooms with single entry, low visibility, near player path
- **Plan patrol routes:** BFS through traversable edges, prefer loops
- **Place horror events:** Rooms tagged "horror" or with specific furniture (gurneys, cages)
- **Track player location:** `query_room_at_position(player_pos)` -> know which room, building, floor
- **Calculate escape routes:** `query_path_between_rooms(player_room, nearest_exit)` -> tension metric (fewer exits = more tension)
- **Zone control:** Assign AI territories based on building/floor boundaries

### 9.2 Encounter Design Integration

The existing `design_encounter` action (Phase 21) can consume the spatial registry to automatically:

1. Score rooms for encounter suitability (exits, size, horror props present)
2. Generate patrol routes that respect door connectivity
3. Place safe rooms at graph chokepoints (rooms with high betweenness centrality)
4. Validate pacing by measuring graph distance between encounter zones

### 9.3 Accessibility Integration

The existing accessibility actions (`validate_path_width`, `validate_navigation_complexity`) can use the adjacency graph to:

- Verify all rooms are reachable (graph connectivity check)
- Validate that safe rooms are within N doors of any encounter zone
- Check that rest points exist at regular graph-distance intervals
- Ensure exits are clearly reachable (no dead-end rooms without windows)

### 9.4 Sound Propagation

The `find_sound_paths` and `can_ai_hear_from` actions can use the adjacency graph to approximate sound propagation through doors (high transmission) vs walls (low transmission) vs windows (medium transmission).

---

## 10. Implementation Estimate

| Component | Effort | Priority |
|-----------|--------|----------|
| **FMonolithSpatialRegistry** singleton (load/save/list/delete JSON) | 3-4h | P0 |
| Block descriptor JSON schema (validation + defaults) | 2-3h | P0 |
| `save_block_descriptor`, `load_block_descriptor`, `list_block_descriptors` | 3-4h | P0 |
| `register_room`, `register_street_furniture` | 3-4h | P0 |
| Adjacency graph builder (from descriptor JSON) | 3-4h | P0 |
| `query_room_at_position` (AABB point containment) | 1-2h | P0 |
| `query_adjacent_rooms` (graph lookup) | 1h | P0 |
| `query_rooms_by_filter` (tag/floor/building filter) | 1-2h | P0 |
| `query_building_exits` (edge filter) | 1h | P1 |
| `query_path_between_rooms` (BFS) | 2-3h | P1 |
| `visualize_spatial_registry` (debug draw) | 4-5h | P1 |
| `spatial_context` auto-registration in FinalizeProceduralMesh (V2) | 3-4h | P2 |
| Graph metrics (betweenness centrality, dead-end detection) | 3-4h | P2 |
| Street parametric coordinate resolution | 2-3h | P2 |
| Integration with encounter design actions | 4-5h | P2 |

**Total: ~36-48h across P0-P2**

P0 alone (core registry + 7 actions): ~18-24h

---

## 11. Phased Rollout

### Phase 1 -- Core Registry (P0, ~18-24h)

- `FMonolithSpatialRegistry` class: singleton, JSON I/O, directory management
- Block descriptor JSON schema with validation
- `save_block_descriptor` / `load_block_descriptor` / `list_block_descriptors` actions
- `register_room` / `register_street_furniture` actions
- Adjacency graph builder
- `query_room_at_position` / `query_adjacent_rooms` / `query_rooms_by_filter`

Deliverable: An AI agent can build a city block, register all rooms, and query "what's adjacent to the kitchen?" or "show me all rooms on floor 2."

### Phase 2 -- Queries + Visualization (P1, ~8-12h)

- `query_building_exits` / `query_path_between_rooms`
- `visualize_spatial_registry` with debug draw
- Graph traversal utilities (BFS, flood fill)
- Connectivity validation (are all rooms reachable?)

Deliverable: AI can pathfind through the building graph and see the spatial layout visualized in the editor.

### Phase 3 -- Smart Integration (P2, ~10-16h)

- `spatial_context` parameter on procedural actions for auto-registration
- Graph analytics (betweenness centrality for chokepoint detection)
- Street parametric coordinates
- Integration with encounter design and accessibility actions
- Horror AI director spatial awareness queries

Deliverable: Fully integrated system where procedural generation, spatial awareness, and horror AI all share a common spatial model.

---

## 12. Open Questions

1. **Multiple active blocks:** Should the system support having multiple block descriptors loaded simultaneously? For a single level, probably one is enough. But for streaming/world partition, multiple blocks could represent different chunks. **Recommendation:** Support it from the start -- it's just a map of block_id -> descriptor.

2. **Live synchronization with the level:** If an actor is moved in the editor, the spatial registry becomes stale. Options:
   - Accept staleness (registry is the "design intent," scene is the "reality")
   - Add a `sync_from_scene` action that scans placed actors and updates positions
   - Use editor delegates (OnActorMoved) to auto-update -- complex and fragile
   **Recommendation:** Accept staleness for V1, add `sync_from_scene` in V2.

3. **Room-to-room wall sharing:** Two adjacent rooms share a wall. Should the spatial registry track which wall segments are shared? This matters for:
   - Sound propagation (shared walls transmit more)
   - Structural analysis (removing shared walls creates open floor plans)
   **Recommendation:** Not in V1. The adjacency graph captures connectivity. Wall-level detail can be inferred from overlapping room bounds.

4. **Procedural street generation:** The schema supports streets but we have no `create_street` action yet. For V1, streets are manually defined in the descriptor. A future `create_street` action would generate road/sidewalk geometry and auto-register.

5. **Naming convention for room paths:** Using `building_id/F{floor}/room_id` as the canonical path (e.g., `corner_shop/F0/main_room`). This is human-readable and hierarchical. Alternative: flat UUIDs. **Recommendation:** Keep hierarchical paths -- they're more useful for LLM agents that reason about them in natural language.

6. **Descriptor versioning:** Include `"version": 1` in the schema. If the schema changes, bump the version and add migration logic in `LoadBlock`. This is the same pattern used by the ProceduralCache manifest.

---

## 13. Relationship to Existing Systems

| Existing System | Relationship to Spatial Registry |
|----------------|--------------------------------|
| **ProceduralCache manifest** | Cross-referenced by `cache_hash` field. Registry tracks WHERE meshes are placed; manifest tracks WHAT meshes exist. |
| **JSON Prefab system** | Prefabs define reusable mesh groups. The spatial registry could reference a prefab as the source for a room's furniture layout. |
| **Blockout system** | The blockout export/import format captures normalized positions. The spatial registry is a superset with hierarchy and connectivity. Could add an `export_to_blockout` action that converts registry rooms to blockout primitives. |
| **Encounter design** | Encounter actions consume spatial data. With the registry, they can operate on the building graph rather than scanning actors. |
| **Accessibility actions** | Path width, navigation complexity, rest points -- all benefit from knowing room connectivity and dimensions without scanning geometry. |
| **Horror analysis** | Sightlines, hiding spots, ambush points -- all queryable from room bounds and door positions without raycast scanning. |

---

## 14. File Layout Summary

```
Plugins/Monolith/
    Source/MonolithMesh/
        Public/
            MonolithSpatialRegistry.h      (NEW: singleton + graph types)
        Private/
            MonolithSpatialRegistry.cpp    (NEW: JSON I/O, graph builder, queries)
            MonolithSpatialActions.cpp     (NEW: MCP action handlers)

    Saved/Monolith/
        SpatialRegistry/                   (NEW: block descriptor JSON files)
            city_block_01.json
            ...
        ProceduralCache/
            manifest.json                  (EXISTING: unchanged)
```
