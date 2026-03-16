---
name: unreal-blueprints
description: Use when working with Unreal Engine Blueprints via Monolith MCP — reading, creating, modifying, compiling Blueprints. Covers variables, components, functions, nodes, pins, interfaces, and graph management. Triggers on Blueprint, BP, event graph, node, variable, function graph, component, compile, interface.
---

# Unreal Blueprint Workflows

You have access to **Monolith** with 55 Blueprint actions via `blueprint_query()`.

## Discovery

Always discover available actions first:
```
monolith_discover({ namespace: "blueprint" })
```

## Key Parameter Names

- `asset_path` — the Blueprint asset path (NOT `asset`)
- `graph_name` — graph name (returned by `list_graphs`)
- `entry_point` — entry point for execution flow tracing
- `node_id` — node identifier (from `get_graph_data` or `add_node` response)
- `component_name` — component variable name (from `get_components`)

## Action Reference

### Read Actions (17)

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
| `get_cdo_properties` | `asset_path`, `category_filter`?, `include_parent_defaults`? | Read all UPROPERTY defaults from Blueprint CDO or UObject asset |
| `get_node_details` | `asset_path`, `node_id`, `graph_name`? | Full pin dump for a single node — faster than fetching entire graph |
| `search_functions` | `query`, `class_filter`?, `pure_only`?, `limit`? | Find BP-callable functions by name/keyword. Static cache — first call ~100ms, then instant |
| `get_interface_functions` | `interface_class` | Query what functions an interface requires — names, params, is_event |

### Discovery & Resolution (1)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `resolve_node` | `node_type`, `function_name`?, `target_class`?, `variable_name`? | Dry-run node creation — returns resolved type, class, all pins with types. No asset modification |

### Variable CRUD (7)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_variable` | `asset_path`, `name`, `type`, `default_value`?, `category`?, flags | Add member variable |
| `remove_variable` | `asset_path`, `name` | Remove member variable |
| `rename_variable` | `asset_path`, `old_name`, `new_name` | Rename variable (updates all refs) |
| `set_variable_type` | `asset_path`, `name`, `type` | Change variable type |
| `set_variable_defaults` | `asset_path`, `name`, flags | Update metadata/flags on a variable |
| `add_local_variable` | `asset_path`, `function_name`, `name`, `type` | Add local variable to a function |
| `remove_local_variable` | `asset_path`, `function_name`, `name` | Remove local variable |

**Type strings:** `bool`, `int`, `int64`, `float`, `double`, `string`, `name`, `text`, `byte`, `object:ClassName`, `class:ClassName`, `struct:StructName`, `enum:EnumName`, `exec`, `wildcard`, `array:T`, `set:T`, `map:K:V`

### Component CRUD (6)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_component` | `asset_path`, `component_class`, `name`?, `parent`? | Add SCS component |
| `remove_component` | `asset_path`, `component_name`, `promote_children`? | Remove component |
| `rename_component` | `asset_path`, `component_name`, `new_name` | Rename component variable |
| `reparent_component` | `asset_path`, `component_name`, `new_parent` | Move component in hierarchy |
| `set_component_property` | `asset_path`, `component_name`, `property_name`, `value` | Set property via reflection |
| `duplicate_component` | `asset_path`, `component_name`, `new_name`? | Duplicate a component |

### Graph Management (9)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_function` | `asset_path`, `name`, `inputs`?, `outputs`?, flags | Create function graph |
| `remove_function` | `asset_path`, `name` | Remove function graph |
| `rename_function` | `asset_path`, `old_name`, `new_name` | Rename function graph |
| `add_macro` | `asset_path`, `name` | Create macro graph |
| `add_event_dispatcher` | `asset_path`, `name`, `params`? | Create event dispatcher |
| `set_function_params` | `asset_path`, `function_name`, `inputs`?, `outputs`? | Set function signature |
| `implement_interface` | `asset_path`, `interface_class` | Add interface to Blueprint |
| `remove_interface` | `asset_path`, `interface_class`, `preserve_functions`? | Remove interface |
| `reparent_blueprint` | `asset_path`, `new_parent_class` | Change Blueprint parent class |

### Node & Pin Operations (6)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_node` | `asset_path`, `node_type`, `graph_name`?, `position`?, type-specific | Add a node to a graph |
| `remove_node` | `asset_path`, `node_id`, `graph_name`? | Remove a node |
| `connect_pins` | `asset_path`, `source_node`, `source_pin`, `target_node`, `target_pin` | Wire two pins together |
| `disconnect_pins` | `asset_path`, `node_id`, `pin_name`, `target_node`?, `target_pin`? | Break pin connections |
| `set_pin_default` | `asset_path`, `node_id`, `pin_name`, `value` | Set pin default value |
| `set_node_position` | `asset_path`, `node_id`, `position` | Move node to [x, y] |

**add_node node_type values:** `CallFunction` (+ `function_name`, `target_class`?), `VariableGet`/`VariableSet` (+ `variable_name`), `CustomEvent` (+ `event_name`), `Branch`, `Sequence`, `MacroInstance` (+ `macro_name`), `SpawnActorFromClass` (+ `actor_class`)

**add_node aliases:** `"function"`/`"call"` → CallFunction, `"get"` → VariableGet, `"set"` → VariableSet, `"event"` → CustomEvent, `"macro"` → MacroInstance, `"branch"` → Branch, `"sequence"` → Sequence, `"spawn"` → SpawnActorFromClass. CallFunction auto-tries K2_ prefix (e.g. `GetActorLocation` → `K2_GetActorLocation`) and U prefix on class names (e.g. `PrimitiveComponent` → `UPrimitiveComponent`).

### Compile & Create (5)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `compile_blueprint` | `asset_path` | Full compile — returns errors, warnings, status |
| `validate_blueprint` | `asset_path` | Lint without compiling — unused vars, disconnected nodes |
| `create_blueprint` | `save_path`, `parent_class`, `blueprint_type`? | Create new Blueprint asset |
| `duplicate_blueprint` | `asset_path`, `new_path` | Duplicate Blueprint to new path |
| `get_dependencies` | `asset_path`, `direction`? | Asset dependencies (depends_on/referenced_by/both) |

### Batch Operations (4)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `batch_execute` | `asset_path`, `operations`, `compile_on_complete`?, `stop_on_error`? | Run multiple BP operations in one round-trip. `operations`: array of `{op, ...params}`. Returns per-op results. Supports all write actions |
| `add_nodes_bulk` | `asset_path`, `nodes` (array with `temp_id`), `auto_layout`? | Place multiple nodes in one call. Returns `temp_id` → `node_id` mapping for use in connect_pins_bulk |
| `connect_pins_bulk` | `asset_path`, `connections` (array) | Wire multiple pin connections in one call. Per-connection success/failure |
| `set_pin_defaults_bulk` | `asset_path`, `defaults` (array) | Set multiple pin defaults in one call |

**`add_variable` type aliases:** `"integer"` → int, `"vector"` → Vector, `"color"` → LinearColor, `"rotator"` → Rotator, `"transform"` → Transform. Unknown types return error with valid type list.

**`create_blueprint`** supports `skip_save` param to defer the asset save.

## Common Workflows

### Understand a Blueprint's structure
```
blueprint_query({ action: "list_graphs", params: { asset_path: "/Game/Blueprints/BP_Enemy" } })
blueprint_query({ action: "get_variables", params: { asset_path: "/Game/Blueprints/BP_Enemy" } })
blueprint_query({ action: "get_components", params: { asset_path: "/Game/Blueprints/BP_Enemy" } })
blueprint_query({ action: "get_functions", params: { asset_path: "/Game/Blueprints/BP_Enemy" } })
```

### Create a Blueprint from scratch
```
blueprint_query({ action: "create_blueprint", params: { save_path: "/Game/Test/BP_NewActor", parent_class: "Actor" } })
blueprint_query({ action: "add_variable", params: { asset_path: "/Game/Test/BP_NewActor", name: "Health", type: "float", default_value: "100.0" } })
blueprint_query({ action: "add_component", params: { asset_path: "/Game/Test/BP_NewActor", component_class: "StaticMeshComponent", name: "Mesh" } })
blueprint_query({ action: "add_function", params: { asset_path: "/Game/Test/BP_NewActor", name: "TakeDamage", inputs: [{"name": "Amount", "type": "float"}] } })
blueprint_query({ action: "compile_blueprint", params: { asset_path: "/Game/Test/BP_NewActor" } })
```

### Add nodes and wire them
```
blueprint_query({ action: "add_node", params: { asset_path: "/Game/Test/BP_NewActor", node_type: "CallFunction", function_name: "PrintString", position: [200, 0] } })
blueprint_query({ action: "connect_pins", params: { asset_path: "/Game/Test/BP_NewActor", source_node: "K2Node_Event_0", source_pin: "then", target_node: "K2Node_CallFunction_0", target_pin: "execute" } })
```

### Validate before shipping
```
blueprint_query({ action: "validate_blueprint", params: { asset_path: "/Game/Blueprints/BP_Enemy" } })
```

## Tips

- **Use `get_graph_summary` first** to understand structure, then `get_graph_data` with `node_class_filter` for specific node types — avoids large payloads
- **Pin names** in `connect_pins` use exact UE pin names (e.g. `execute`, `then`, `ReturnValue`)
- **Node IDs** come from `get_graph_data` or `add_node` response — they're `GetName()` values like `K2Node_CallFunction_0`
- **Type strings** for variables and pins: `bool`, `float`, `struct:Vector`, `object:Actor`, `array:float`, `map:string:int`
- **Component names** are variable names from `get_components`, not display names
- For C++ parent class analysis, combine with `source_query("get_class_hierarchy", ...)`
- **Always compile** after making structural changes to verify correctness
