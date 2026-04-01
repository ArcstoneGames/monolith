---
name: unreal-blueprints
description: Use when working with Unreal Engine Blueprints via Monolith MCP — reading, creating, modifying, compiling Blueprints. Covers variables, components, functions, nodes, pins, interfaces, graph management, DataTables, structs, enums, templates, layout, timelines, level blueprints, CDO properties, and graph export/import. Triggers on Blueprint, BP, event graph, node, variable, function graph, component, compile, interface, DataTable, struct, enum, template, layout, timeline, level blueprint, CDO.
---

# Unreal Blueprint Workflows

You have access to **Monolith** with **88 Blueprint actions** via `blueprint_query()`.

**Also works on:** Level Blueprints (via map path or `$current`), Widget Blueprints (UWidgetBlueprint inherits UBlueprint).

## Discovery

Always discover available actions first:
```
monolith_discover({ namespace: "blueprint" })
```

## Key Parameter Names

- `asset_path` — the Blueprint asset path (NOT `asset`). For Level BPs: use the map path or `"$current"`
- `graph_name` — graph name (returned by `list_graphs`)
- `entry_point` — entry point for execution flow tracing
- `node_id` — node identifier (from `get_graph_data` or `add_node` response)
- `component_name` — component variable name (from `get_components`)

## Action Reference

### Read Actions (19)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `list_graphs` | `asset_path` | List all event/function/macro graphs |
| `get_graph_summary` | `asset_path`, `graph_name` | Lightweight graph overview — node counts by type |
| `get_graph_data` | `asset_path`, `graph_name`, `node_class_filter`? | Full node topology — pins, connections, positions |
| `get_variables` | `asset_path` | Variables with types, defaults, categories, replication |
| `get_execution_flow` | `asset_path`, `graph_name`, `entry_point` | Trace execution wires from entry to terminal |
| `search_nodes` | `asset_path`, `query` | Find nodes by class name, display name, or comment |
| `get_components` | `asset_path` | Component hierarchy — names, classes, parent-child tree |
| `get_component_details` | `asset_path`, `component_name` | Full property dump for a specific component |
| `get_functions` | `asset_path` | Functions with inputs, outputs, metadata, local vars |
| `get_event_dispatchers` | `asset_path` | Event dispatchers with signature pins |
| `get_parent_class` | `asset_path` | Parent class, blueprint type, status, capabilities |
| `get_interfaces` | `asset_path` | Implemented interfaces (direct + inherited) |
| `get_construction_script` | `asset_path` | Construction script graph data |
| `get_node_details` | `asset_path`, `node_id`, `graph_name`? | Full pin dump for a single node |
| `search_functions` | `query`, `class_filter`?, `pure_only`?, `limit`? | Find BP-callable functions by name/keyword |
| `get_interface_functions` | `interface_class` | Query what functions an interface requires |
| `get_function_signature` | `asset_path`, `function_name` | Read function inputs, outputs, flags, local vars |
| `get_blueprint_info` | `asset_path` | Comprehensive BP overview: graphs, compile_status, counts |
| `get_event_dispatcher_details` | `asset_path`, `dispatcher_name` | Signature pins + referencing nodes |

### CDO Actions (2)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_cdo_properties` | `asset_path`, `category_filter`? | Read UPROPERTY defaults from Blueprint CDO or UObject |
| `set_cdo_property` | `asset_path`, `property_name`, `value` | Write a property on Blueprint CDO or UObject. Uses ImportText — supports all types |

### Discovery & Resolution (1)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `resolve_node` | `node_type`, `function_name`?, `target_class`? | Dry-run node creation — returns resolved type, all pins |

### Variable CRUD (8)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_variable` | `asset_path`, `name`, `type`, `default_value`?, `category`? | Add member variable |
| `remove_variable` | `asset_path`, `name` | Remove member variable |
| `rename_variable` | `asset_path`, `old_name`, `new_name` | Rename variable (updates all refs) |
| `set_variable_type` | `asset_path`, `name`, `type` | Change variable type |
| `set_variable_defaults` | `asset_path`, `name`, flags | Update metadata/flags |
| `add_local_variable` | `asset_path`, `function_name`, `name`, `type` | Add local variable to function |
| `remove_local_variable` | `asset_path`, `function_name`, `name` | Remove local variable |
| `add_replicated_variable` | `asset_path`, `variable_name`, `type`, `replication_condition`? | Add replicated variable with OnRep stub |

**Type strings:** `bool`, `int`, `int64`, `float`, `double`, `string`, `name`, `text`, `byte`, `object:ClassName`, `class:ClassName`, `struct:StructName`, `enum:EnumName`, `exec`, `wildcard`, `array:T`, `set:T`, `map:K:V`

### Component CRUD (6)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_component` | `asset_path`, `component_class`, `name`?, `parent`? | Add SCS component |
| `remove_component` | `asset_path`, `component_name`, `promote_children`? | Remove component |
| `rename_component` | `asset_path`, `component_name`, `new_name` | Rename component |
| `reparent_component` | `asset_path`, `component_name`, `new_parent` | Move in hierarchy |
| `set_component_property` | `asset_path`, `component_name`, `property_name`, `value` | Set property via reflection |
| `duplicate_component` | `asset_path`, `component_name`, `new_name`? | Duplicate component |

### Graph Management (14)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_function` | `asset_path`, `name`, `inputs`?, `outputs`?, `replication`?, `reliable`? | Create function graph |
| `remove_function` | `asset_path`, `name` | Remove function graph |
| `rename_function` | `asset_path`, `old_name`, `new_name` | Rename function graph |
| `add_macro` | `asset_path`, `name` | Create macro graph |
| `remove_macro` | `asset_path`, `macro_name` | Remove macro graph |
| `rename_macro` | `asset_path`, `old_name`, `new_name` | Rename macro graph |
| `add_event_dispatcher` | `asset_path`, `name`, `params`? | Create event dispatcher |
| `remove_event_dispatcher` | `asset_path`, `dispatcher_name` | Remove dispatcher |
| `set_event_dispatcher_params` | `asset_path`, `dispatcher_name`, `params` | Set dispatcher signature |
| `set_function_params` | `asset_path`, `function_name`, `inputs`?, `outputs`? | Set function signature |
| `implement_interface` | `asset_path`, `interface_class` | Add interface to Blueprint |
| `remove_interface` | `asset_path`, `interface_class` | Remove interface |
| `scaffold_interface_implementation` | `asset_path`, `interface_class` | Add interface + create stub functions |
| `reparent_blueprint` | `asset_path`, `new_parent_class` | Change parent class |

### Node & Pin Operations (7)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_node` | `asset_path`, `node_type`, `graph_name`?, `position`?, `replication`?, `reliable`? | Add a node to a graph |
| `remove_node` | `asset_path`, `node_id`, `graph_name`? | Remove a node |
| `connect_pins` | `asset_path`, `source_node`, `source_pin`, `target_node`, `target_pin` | Wire two pins (case-insensitive matching, shows available pins on error) |
| `disconnect_pins` | `asset_path`, `node_id`, `pin_name` | Break pin connections |
| `set_pin_default` | `asset_path`, `node_id`, `pin_name`, `value` | Set pin default value |
| `set_node_position` | `asset_path`, `node_id`, `position` | Move node to [x, y] |
| `promote_pin_to_variable` | `asset_path`, `node_id`, `pin_name`, `variable_name`? | Promote pin to member variable |

#### `add_node` Supported Types (~25)

| node_type | Extra Params | Notes |
|-----------|-------------|-------|
| `CallFunction` | `function_name`, `target_class`? | Also aliased as `call`, `function` |
| `VariableGet` / `VariableSet` | `variable_name` | Also `get` / `set` |
| `CustomEvent` | `event_name`, `replication`?, `reliable`? | Also `event`. Supports server/client/multicast RPC |
| `Branch` | — | Also `branch` |
| `Sequence` | — | Also `sequence` |
| `MacroInstance` | `macro_name`, `macro_blueprint`? | Also `macro` |
| `SpawnActorFromClass` | `actor_class` | Also `spawn` |
| `DynamicCast` | `cast_class` | Also `cast` |
| `Self` | — | UK2Node_Self |
| `Return` | — | UK2Node_FunctionResult (function graphs only) |
| `MakeStruct` | `struct_type` | e.g., `"Vector"`, `"Transform"` |
| `BreakStruct` | `struct_type` | Decompose struct to pins |
| `SwitchOnEnum` | `enum_type` | Creates case pins per enum value |
| `SwitchOnInt` | — | Integer switch |
| `SwitchOnString` | — | String switch |
| `FormatText` | `format`? | Set `"Hello {Name}"` to create arg pins |
| `MakeArray` | `num_entries`? | Default 1 entry |
| `Select` | — | Data routing by index |
| `ForEachLoop` | — | Engine macro (auto-resolved) |
| `ForLoop` / `ForLoopWithBreak` | — | Engine macros |
| `DoOnce` / `FlipFlop` / `Gate` | — | Engine macros |
| `IsValid` | — | Routes to KismetSystemLibrary::IsValid |
| `Delay` / `RetriggerableDelay` | — | Latent, routes to KismetSystemLibrary |
| *(any UK2Node_ class)* | — | Generic fallback — tries `UK2Node_<name>` |

### Compile & Create (6)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `compile_blueprint` | `asset_path` | Full compile — errors include `node_id` and `graph_name` |
| `validate_blueprint` | `asset_path` | Lint — unused vars, disconnected nodes, unimplemented interfaces |
| `create_blueprint` | `save_path`, `parent_class`, `blueprint_type`? | Create new Blueprint |
| `duplicate_blueprint` | `asset_path`, `new_path` | Duplicate to new path |
| `get_dependencies` | `asset_path`, `direction`? | Asset dependencies |
| `save_asset` | `asset_path` | Save any loaded asset to disk |

### Timeline Actions (4)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_timeline` | `asset_path`, `timeline_name`?, `auto_play`?, `loop`? | Create timeline node |
| `get_timeline_data` | `asset_path`, `timeline_name`? | Read tracks, keys, settings (all timelines or one) |
| `add_timeline_track` | `asset_path`, `timeline_name`, `track_name`, `track_type`? | Add float/vector/event/color track |
| `set_timeline_keys` | `asset_path`, `timeline_name`, `track_name`, `keys` | Set keyframes: `[{time, value, interp_mode?}]` |

### Struct, Enum & DataTable Actions (5)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `create_user_defined_struct` | `save_path`, `fields` | Create struct with `[{name, type, default_value?}]` |
| `create_user_defined_enum` | `save_path`, `values` | Create enum with `["Value1", "Value2", ...]` |
| `create_data_table` | `save_path`, `row_struct` | Create DataTable for a struct type |
| `add_data_table_row` | `asset_path`, `row_name`, `values` | Add row: `{column: value, ...}` |
| `get_data_table_rows` | `asset_path`, `row_name`? | Read all rows or single row |

### Build from Spec (1)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `build_blueprint_from_spec` | `asset_path`, `graph_name`?, `variables`?, `components`?, `nodes`, `connections`?, `pin_defaults`?, `auto_compile`? | One-shot declarative Blueprint builder |

Nodes use spec IDs (e.g., `"id": "evt"`) that map to real node IDs in connections/pin_defaults.

### Graph Export Actions (3)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `export_graph` | `asset_path`, `graph_name`? | Export graph to JSON (compatible with build_from_spec) |
| `copy_nodes` | `source_asset`, `source_graph`, `node_ids`, `target_asset`, `target_graph` | Copy nodes via T3D (perfect fidelity) |
| `duplicate_graph` | `asset_path`, `graph_name`, `new_name` | Duplicate function/macro within same BP |

### Diff Actions (1)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `compare_blueprints` | `asset_path_a`, `asset_path_b` | Structural diff — variables, components, functions, graphs |

### Template Actions (2)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `list_templates` | — | List available templates with descriptions + params |
| `apply_template` | `template_name`, `asset_path`, `params`? | Apply a template (health_system, timer_loop, interactable_actor) |

### Layout Actions (1)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `auto_layout` | `asset_path`, `graph_name`?, `layout_mode`?, `horizontal_spacing`?, `vertical_spacing`?, `formatter`? | Auto-arrange graph nodes left-to-right |

**Layout modes:** `"all"` (reposition everything), `"new_only"` (only nodes at 0,0), `"selected"` (specific `node_ids`, others pinned)
**Formatter:** `"monolith"` (default) or `"blueprint_assist"` (if BA plugin installed)

### Scaffolding & Events (3)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_event_node` | `asset_path`, `event_name`, `replication`?, `reliable`? | Override events or custom events with RPC support |
| `add_comment_node` | `asset_path`, `text`, `node_ids`?, `color`? | Add comment box |

### Batch Operations (4)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `batch_execute` | `asset_path`, `operations`, `compile_on_complete`? | Run multiple ops in one round-trip |
| `add_nodes_bulk` | `asset_path`, `nodes` (with `temp_id`) | Place multiple nodes, returns ID mapping |
| `connect_pins_bulk` | `asset_path`, `connections` | Wire multiple connections |
| `set_pin_defaults_bulk` | `asset_path`, `defaults` | Set multiple pin defaults |

## Common Workflows

### Build a Blueprint from spec (one call)
```
blueprint_query({ action: "build_blueprint_from_spec", params: {
  asset_path: "/Game/Test/BP_Door",
  nodes: [
    {"id": "evt", "type": "CustomEvent", "event_name": "OnInteract", "position": [0, 0]},
    {"id": "print", "type": "CallFunction", "function_name": "PrintString", "position": [300, 0]}
  ],
  connections: [{"source": "evt", "source_pin": "Then", "target": "print", "target_pin": "execute"}],
  pin_defaults: [{"node_id": "print", "pin_name": "InString", "value": "Door opened!"}],
  auto_compile: true
}})
```

### Create a Server RPC
```
blueprint_query({ action: "add_node", params: {
  asset_path: "/Game/BP_Player", node_type: "CustomEvent",
  event_name: "ServerTakeDamage", replication: "server", reliable: true
}})
```

### Auto-layout a messy graph
```
blueprint_query({ action: "auto_layout", params: {
  asset_path: "/Game/BP_Enemy", graph_name: "EventGraph", layout_mode: "all"
}})
```

### Apply a template
```
blueprint_query({ action: "apply_template", params: {
  template_name: "health_system", asset_path: "/Game/BP_Player", params: { max_health: "200.0" }
}})
```

## Tips

- **Pin names** are now case-insensitive. Wrong names show available pins in the error.
- **Compile errors** include `node_id` and `graph_name` for targeted debugging.
- **Level Blueprints** work with all actions — use the map path or `"$current"` for the loaded level.
- **Widget Blueprints** work with all blueprint_query actions (event graph, variables, functions).
- **Generic K2Node fallback**: any `UK2Node_` subclass name works as `node_type` if no dedicated handler exists.
- Use `get_graph_summary` first for structure, then `get_graph_data` with `node_class_filter` for specifics.
- **Always compile** after structural changes to verify correctness.
