---
name: unreal-animation
description: Use when inspecting or editing Unreal animation assets via Monolith MCP — sequences, montages, blend spaces, animation blueprints, notifies, curves, sync markers, skeletons, compression, root motion, pose search. Triggers on animation, montage, ABP, blend space, notify, anim sequence, skeleton, curve, sync marker, root motion, pose search.
---

# Unreal Animation Workflows

You have access to **Monolith** with 67 animation actions (62 animation + 5 PoseSearch) via `animation_query()`.

## Discovery

```
monolith_discover({ namespace: "animation" })
```

## Asset Path Conventions

All asset paths follow UE content browser format (no .uasset extension):

| Location | Path Format | Example |
|----------|------------|---------|
| Project Content/ | `/Game/Path/To/Asset` | `/Game/Animations/ABP_Player` |
| Project Plugins/ | `/PluginName/Path/To/Asset` | `/CarnageFX/Animations/AM_Hit` |
| Engine Plugins | `/PluginName/Path/To/Asset` | `/Niagara/DefaultAssets/SystemAssets/NS_Default` |

## Key Parameter Names

- `asset_path` — the animation asset path
- `machine_name` — state machine name (returned by `get_state_machines`)
- `state_name` — state name within a machine
- `graph_name` — graph name (optional filter for `get_nodes`)

## Action Categories

### Montage Operations
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_montage_info` | `asset_path` | Get full montage details (sections, slots, notifies, length) |
| `add_montage_section` | `asset_path`, `name`, `time` | Add a named section to a montage |
| `delete_montage_section` | `asset_path`, `name` | Remove a section |
| `set_montage_section_link` | `asset_path`, `section`, `next` | Set section playback order |
| `get_montage_slots` | `asset_path` | List all slot tracks and their animations |
| `set_section_time` | `asset_path`, `section`, `time` | Move a section to a specific time |

### Blend Space Operations
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_blendspace_info` | `asset_path` | Get blend space configuration (axes, dimensions, grid) |
| `get_blendspace_samples` | `asset_path` | List all sample points with positions and animations |
| `add_blendspace_sample` | `asset_path`, `animation`, `x`, `y` | Add an animation at X/Y coordinates |
| `remove_blendspace_sample` | `asset_path`, `index` | Remove a sample point by index |
| `set_blendspace_axis` | `asset_path`, `axis`, `name`, `min`, `max` | Configure axis label and range |
| `edit_blendspace_sample` | `asset_path`, `index`, `x`, `y` | Move an existing sample |
| `delete_blendspace_sample` | `asset_path`, `index` | Remove a sample point |

### AnimBP Inspection
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_anim_blueprint_info` | `asset_path` | Overview of the ABP (target skeleton, parent class, graphs) |
| `get_state_machines` | `asset_path` | List all state machines in an ABP |
| `get_state_info` | `asset_path`, `machine_name`, `state_name` | Details of a specific state |
| `get_transitions` | `asset_path`, `machine_name` | Transition rules between states |
| `get_anim_graph_nodes` | `asset_path`, `graph_name` (optional) | Nodes within a specific graph (or all graphs) |
| `get_blend_nodes` | `asset_path` | Blend node trees |
| `get_linked_layers` | `asset_path` | Linked anim layers |
| `get_graphs` | `asset_path` | All graphs in the ABP |
| `get_nodes` | `asset_path`, `graph_name` (optional) | Nodes within a specific graph (or all graphs) |

### Curve Operations
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_curves` | `asset_path` | List all curves on an animation sequence |
| `add_curve` | `asset_path`, `name`, `type` | Add a new curve to a sequence |
| `remove_curve` | `asset_path`, `name` | Remove a curve by name |
| `set_curve_keys` | `asset_path`, `name`, `keys` | Set keyframe data for a curve |
| `get_curve_keys` | `asset_path`, `name` | Get all keyframes for a specific curve |
| `rename_curve` | `asset_path`, `name`, `new_name` | Rename an existing curve |
| `get_curve_data` | `asset_path`, `name` | Get detailed curve data (type, keys, interpolation) |

### Bone Track Inspection
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_bone_tracks` | `asset_path` | List all bone tracks in a sequence |
| `get_bone_track_data` | `asset_path`, `bone` | Get transform data for a specific bone track |
| `get_animation_statistics` | `asset_path` | Frame count, length, bone count, compressed size, etc. |
| `set_bone_track_keys` | `asset_path`, `bone`, `keys` | Set keyframes for a bone track |
| `add_bone_track` | `asset_path`, `bone` | Add a new bone track |
| `remove_bone_track` | `asset_path`, `bone` | Remove a bone track |

### Sync Markers
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_sync_markers` | `asset_path` | List all sync markers on a sequence |
| `add_sync_marker` | `asset_path`, `name`, `time` | Add a sync marker at a specific time |
| `remove_sync_marker` | `asset_path`, `name` | Remove a sync marker by name |

### Root Motion
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_root_motion_info` | `asset_path` | Get root motion settings and statistics |
| `extract_root_motion` | `asset_path` | Extract and report root motion transform data per frame |

### Animation Compression
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_compression_settings` | `asset_path` | Current compression codec and settings |
| `apply_compression` | `asset_path`, `codec`, `settings` | Apply a compression codec to a sequence |

### Notify Editing
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `set_notify_time` | `asset_path`, `notify`, `time` | Move a notify to a specific time |
| `set_notify_duration` | `asset_path`, `notify`, `duration` | Set duration of a notify state |

### Skeleton Operations
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_skeleton_info` | `asset_path` | Bone hierarchy, sockets, virtual bones |
| `add_virtual_bone` | `asset_path`, `source`, `target` | Create a virtual bone between two bones |
| `remove_virtual_bones` | `asset_path`, `bones` | Remove virtual bones |
| `get_socket_info` | `asset_path` | List all sockets with transforms and parent bones |
| `add_socket` | `asset_path`, `name`, `parent_bone`, `transform` | Add a socket to the skeleton |
| `get_skeletal_mesh_info` | `asset_path` | Mesh details, LODs, materials |

### Batch & Modifiers
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `batch_get_animation_info` | `asset_paths` | Get summary info for multiple animations in one call |
| `run_animation_modifier` | `asset_path`, `modifier` | Run an animation modifier blueprint on a sequence |

### PoseSearch
| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_pose_search_schema` | `asset_path` | Inspect a PoseSearch schema (channels, features) |
| `get_pose_search_database` | `asset_path` | Get database config (schema ref, sequences, settings) |
| `add_database_sequence` | `asset_path`, `sequence`, `settings` | Add an animation sequence to a PoseSearch database |
| `remove_database_sequence` | `asset_path`, `sequence` | Remove a sequence from the database |
| `get_database_stats` | `asset_path` | Build stats, pose count, memory usage |

## Common Workflows

### Inspect an ABP's state machines
```
animation_query({ action: "get_anim_blueprint_info", params: { asset_path: "/Game/Animations/ABP_Player" } })
animation_query({ action: "get_state_machines", params: { asset_path: "/Game/Animations/ABP_Player" } })
animation_query({ action: "get_transitions", params: { asset_path: "/Game/Animations/ABP_Player", machine_name: "Locomotion" } })
```

### Get nodes with optional graph filter
```
animation_query({ action: "get_nodes", params: { asset_path: "/Game/Animations/ABP_Player" } })
animation_query({ action: "get_nodes", params: { asset_path: "/Game/Animations/ABP_Player", graph_name: "EventGraph" } })
```

### Set up montage section flow (intro -> loop -> outro)
```
animation_query({ action: "get_montage_info", params: { asset_path: "/Game/Animations/AM_Attack" } })
animation_query({ action: "add_montage_section", params: { asset_path: "/Game/Animations/AM_Attack", name: "Intro", time: 0.0 } })
animation_query({ action: "add_montage_section", params: { asset_path: "/Game/Animations/AM_Attack", name: "Loop", time: 0.5 } })
animation_query({ action: "add_montage_section", params: { asset_path: "/Game/Animations/AM_Attack", name: "Outro", time: 1.2 } })
animation_query({ action: "set_montage_section_link", params: { asset_path: "/Game/Animations/AM_Attack", section: "Intro", next: "Loop" } })
animation_query({ action: "set_montage_section_link", params: { asset_path: "/Game/Animations/AM_Attack", section: "Loop", next: "Outro" } })
```

### Work with animation curves
```
animation_query({ action: "get_curves", params: { asset_path: "/Game/Animations/AS_Walk" } })
animation_query({ action: "add_curve", params: { asset_path: "/Game/Animations/AS_Walk", name: "FootPlant_L", type: "float" } })
animation_query({ action: "set_curve_keys", params: { asset_path: "/Game/Animations/AS_Walk", name: "FootPlant_L", keys: [{ time: 0.0, value: 1.0 }, { time: 0.3, value: 0.0 }] } })
```

### Inspect animation details and compression
```
animation_query({ action: "get_animation_statistics", params: { asset_path: "/Game/Animations/AS_Walk" } })
animation_query({ action: "get_bone_tracks", params: { asset_path: "/Game/Animations/AS_Walk" } })
animation_query({ action: "get_compression_settings", params: { asset_path: "/Game/Animations/AS_Walk" } })
animation_query({ action: "get_root_motion_info", params: { asset_path: "/Game/Animations/AS_Walk" } })
```

### Work with sync markers
```
animation_query({ action: "get_sync_markers", params: { asset_path: "/Game/Animations/AS_Walk" } })
animation_query({ action: "add_sync_marker", params: { asset_path: "/Game/Animations/AS_Walk", name: "FootDown_L", time: 0.0 } })
animation_query({ action: "add_sync_marker", params: { asset_path: "/Game/Animations/AS_Walk", name: "FootDown_R", time: 0.5 } })
```

### Inspect skeleton structure
```
animation_query({ action: "get_skeleton_info", params: { asset_path: "/Game/Characters/SK_Mannequin" } })
animation_query({ action: "get_socket_info", params: { asset_path: "/Game/Characters/SK_Mannequin" } })
animation_query({ action: "get_skeletal_mesh_info", params: { asset_path: "/Game/Characters/SKM_Mannequin" } })
```

### Batch inspect animations
```
animation_query({ action: "batch_get_animation_info", params: { asset_paths: ["/Game/Animations/AS_Walk", "/Game/Animations/AS_Run", "/Game/Animations/AS_Idle"] } })
```

### PoseSearch database setup
```
animation_query({ action: "get_pose_search_schema", params: { asset_path: "/Game/PoseSearch/Schema_Locomotion" } })
animation_query({ action: "get_pose_search_database", params: { asset_path: "/Game/PoseSearch/DB_Locomotion" } })
animation_query({ action: "add_database_sequence", params: { asset_path: "/Game/PoseSearch/DB_Locomotion", sequence: "/Game/Animations/AS_Walk" } })
animation_query({ action: "get_database_stats", params: { asset_path: "/Game/PoseSearch/DB_Locomotion" } })
```

## Rules

- Editing tools modify assets **live in the editor** — changes are immediate
- The primary asset param is `asset_path` (not `asset`)
- State machine names are returned clean (no newline artifacts)
- `get_nodes` and `get_anim_graph_nodes` accept an optional `graph_name` filter to scope results
- Use `project_query("search", { query: "AM_*" })` to find animation assets first
- ABP reading is read-only — state machine logic must be edited in the BP editor
- PoseSearch actions require the PoseSearch plugin to be enabled
- `batch_get_animation_info` is preferred over multiple individual calls for bulk inspection
