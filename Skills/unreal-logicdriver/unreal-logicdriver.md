---
name: unreal-logicdriver
description: Use when working with Logic Driver Pro plugin via Monolith MCP -- creating and editing state machines, states, transitions, runtime PIE control, JSON spec builds, scaffolding templates, and text visualization. Triggers on state machine, logic driver, SM blueprint, state graph, FSM, state transition, dialogue tree, quest system, game flow.
---

# Unreal Logic Driver Pro Workflows

You have access to **Monolith** with **66 LogicDriver actions** across 10 categories via `logicdriver_query()`.

## Discovery

Always discover available actions first:
```
monolith_discover({ namespace: "logicdriver" })
```

## Key Parameter Names

- `asset_path` -- the state machine Blueprint asset path (e.g., `/Game/StateMachines/SM_EnemyBehavior`)
- `state_machine_path` -- alias for asset_path in some actions
- `node_id` -- node identifier within a state machine graph
- `state_name` -- human-readable state name
- `transition_id` -- transition identifier
- `save_path` -- destination path for new assets
- `spec` -- JSON specification object for `build_sm_from_spec`
- `template` -- scaffold template name (e.g., `hello_world`, `horror_encounter`)
- `format` -- text output format (`ascii`, `mermaid`, `dot`)
- `instance_index` -- runtime SM instance index (default 0)

## Action Reference

### Asset CRUD (8)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `create_state_machine` | `save_path`, `sm_name`? | Create a new state machine Blueprint asset |
| `get_state_machine` | `asset_path` | Read full SM structure: states, transitions, entry points, properties |
| `list_state_machines` | `path_filter`?, `limit`? | List all state machine Blueprint assets in project |
| `delete_state_machine` | `asset_path` | Delete a state machine Blueprint asset |
| `duplicate_state_machine` | `asset_path`, `save_path` | Duplicate an existing SM to a new path |
| `rename_state_machine` | `asset_path`, `new_name` | Rename a state machine asset |
| `validate_state_machine` | `asset_path` | Lint SM: orphan states, missing transitions, unreachable nodes, dead ends |
| `compile_state_machine` | `asset_path` | Compile SM after structural edits -- required before PIE testing |

### Graph Read/Write (20)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_graph_structure` | `asset_path` | Get full graph topology: nodes, edges, nesting |
| `get_state_info` | `asset_path`, `node_id` | Read state node details: class, properties, graph contents |
| `get_transition_info` | `asset_path`, `transition_id` | Read transition details: conditions, priority, color |
| `get_nested_graph` | `asset_path`, `node_id` | Read nested state machine graph within a state |
| `add_state` | `asset_path`, `state_class`, `state_name`? | Add a state node to the graph |
| `remove_state` | `asset_path`, `node_id` | Remove a state node and its transitions |
| `add_transition` | `asset_path`, `source_node`, `target_node`, `condition_class`? | Add a transition between states |
| `remove_transition` | `asset_path`, `transition_id` | Remove a transition edge |
| `set_state_property` | `asset_path`, `node_id`, `property_name`, `value` | Set a UPROPERTY on a state node |
| `get_state_property` | `asset_path`, `node_id`, `property_name` | Read a UPROPERTY value from a state node |
| `set_transition_property` | `asset_path`, `transition_id`, `property_name`, `value` | Set a UPROPERTY on a transition |
| `get_transition_property` | `asset_path`, `transition_id`, `property_name` | Read a UPROPERTY value from a transition |
| `set_entry_state` | `asset_path`, `node_id` | Set which state is the entry point |
| `add_conduit` | `asset_path`, `conduit_name`? | Add a conduit node for conditional branching |
| `add_state_machine_ref` | `asset_path`, `ref_path`, `node_name`? | Add a nested state machine reference |
| `set_node_position` | `asset_path`, `node_id`, `x`, `y` | Set node position in graph editor |
| `get_all_states` | `asset_path` | List all state nodes with IDs and classes |
| `get_all_transitions` | `asset_path` | List all transitions with source/target and conditions |
| `auto_arrange_graph` | `asset_path`, `spacing`? | Auto-layout using IMonolithGraphFormatter (Blueprint Assist bridge) |
| `set_graph_property` | `asset_path`, `property_name`, `value` | Set a top-level graph UPROPERTY |

### Node Config (8)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `set_on_state_begin` | `asset_path`, `node_id`, `graph_nodes` | Configure the On State Begin event graph |
| `set_on_state_update` | `asset_path`, `node_id`, `graph_nodes` | Configure the On State Update event graph |
| `set_on_state_end` | `asset_path`, `node_id`, `graph_nodes` | Configure the On State End event graph |
| `set_transition_condition` | `asset_path`, `transition_id`, `condition_class`, `params`? | Set transition evaluation condition |
| `set_transition_priority` | `asset_path`, `transition_id`, `priority` | Set transition evaluation priority (lower = first) |
| `add_state_tag` | `asset_path`, `node_id`, `tag` | Add a gameplay tag to a state node |
| `remove_state_tag` | `asset_path`, `node_id`, `tag` | Remove a gameplay tag from a state node |
| `set_state_color` | `asset_path`, `node_id`, `color` | Set node color in the graph editor |

### Runtime/PIE (7)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `runtime_get_active_states` | `actor_label`?, `component_name`? | Get currently active states on a running SM |
| `runtime_force_state` | `actor_label`, `state_name`, `instance_index`? | Force a running SM to a specific state |
| `runtime_send_event` | `actor_label`, `event_name`, `instance_index`? | Send a named event to a running SM |
| `runtime_get_variables` | `actor_label`, `instance_index`? | Read SM instance variable values at runtime |
| `runtime_set_variable` | `actor_label`, `variable_name`, `value`, `instance_index`? | Set an SM instance variable at runtime |
| `runtime_restart` | `actor_label`, `instance_index`? | Restart a running SM instance from its initial state |
| `runtime_stop` | `actor_label`, `instance_index`? | Stop a running SM instance |

### JSON/Spec (5)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `build_sm_from_spec` | `save_path`, `spec` | **POWER ACTION** -- create a full SM from a JSON specification in one call |
| `export_sm_to_spec` | `asset_path` | Export an existing SM to JSON spec format |
| `import_sm_from_json` | `save_path`, `json_path` | Import SM from a JSON file on disk |
| `export_sm_to_json` | `asset_path`, `json_path` | Export SM to a JSON file on disk |
| `diff_state_machines` | `asset_path_a`, `asset_path_b` | Compare two SMs and return structural differences |

### Scaffolding (7)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `scaffold_hello_world` | `save_path` | Minimal 2-state SM: Entry -> HelloState -> EndState |
| `scaffold_dialogue` | `save_path`, `dialogue_lines`? | Dialogue tree SM with branching responses |
| `scaffold_quest` | `save_path`, `quest_stages`? | Quest progression SM with stages and completion |
| `scaffold_interactable` | `save_path`, `interaction_type`? | Interactable object SM: Idle -> Interact -> Cooldown |
| `scaffold_weapon` | `save_path`, `weapon_type`? | Weapon state SM: Idle -> Fire -> Reload -> Overheat |
| `scaffold_horror_encounter` | `save_path`, `phases`? | Horror encounter SM: Ambient -> Alert -> Chase -> Attack -> Reset |
| `scaffold_game_flow` | `save_path`, `flow_stages`? | Game flow SM: MainMenu -> Loading -> Gameplay -> Pause -> GameOver |

### Discovery (6)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `list_state_classes` | `filter`? | List available state node classes (built-in + project) |
| `list_transition_classes` | `filter`? | List available transition condition classes |
| `list_conduit_classes` | `filter`? | List available conduit classes |
| `get_sm_class_hierarchy` | `class_name` | Get inheritance hierarchy for an SM node class |
| `find_sm_references` | `asset_path` | Find all assets referencing this state machine |
| `get_sm_stats` | `asset_path` | Get statistics: state count, transition count, max depth, complexity |

### Component (3)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `add_sm_component` | `blueprint_path`, `sm_asset_path`, `component_name`? | Add SMInstance component to a Blueprint actor |
| `configure_sm_component` | `blueprint_path`, `component_name`, `properties` | Configure SM component properties (auto-start, tick, etc.) |
| `list_sm_components` | `blueprint_path` | List all SM components on a Blueprint actor |

### Text Graph (2)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `visualize_sm_as_text` | `asset_path`, `format`? | Render SM as text: `ascii`, `mermaid`, or `dot` (Graphviz) |
| `search_sm_content` | `query`, `path_filter`? | Full-text search across SM states, transitions, and properties |

## Key Technical Notes

1. **Precompiled plugin (reflection only)** -- Logic Driver Pro is a marketplace plugin distributed as precompiled binaries. MonolithLogicDriver interacts with it via UObject reflection and the Asset Registry, NOT by linking against Logic Driver's C++ API directly. All property access uses `FindPropertyByName` / `FProperty::GetValue_InContainer`. This means Monolith works with any Logic Driver version as long as the reflected property names haven't changed.

2. **`#if WITH_LOGICDRIVER` conditional** -- `MonolithLogicDriver.Build.cs` probes for Logic Driver Pro in project `Plugins/` and engine `Plugins/Marketplace/`. Defines `WITH_LOGICDRIVER=1` or `=0`. When Logic Driver is absent, the module compiles to an empty stub (0 actions registered). Full details in the [Optional Modules wiki](https://github.com/tumourlove/monolith/wiki/Optional-Modules).

3. **Settings toggle** -- `bEnableLogicDriver` in `UMonolithSettings` (default: true). When disabled, 0 actions registered even if Logic Driver is installed. Check Editor Preferences > Plugins > Monolith.

4. **SM Blueprint architecture** -- Logic Driver state machines are stored as Blueprint assets (`USMBlueprint`) with a compiled runtime class (`USMInstance`). The graph has both a design-time EdGraph representation and a runtime node graph. Programmatic edits update both layers and call `compile_state_machine` to finalize.

5. **State node class hierarchy** -- `USMStateInstance_Base` is the root. Key subclasses: `USMStateInstance` (standard state), `USMStateMachineInstance` (nested SM), `USMConduitInstance` (conditional branch). Transitions use `USMTransitionInstance`.

6. **`build_sm_from_spec` spec format** -- The JSON spec object contains `states` (array of state defs with name, class, properties, event graphs), `transitions` (array with source, target, condition), and optional `entry_state`, `metadata`. This is the fastest way to create complex SMs in a single call.

7. **Blueprint Assist integration** -- `auto_arrange_graph` uses the `IMonolithGraphFormatter` interface. If Blueprint Assist is installed, it reads FBACache for accurate node sizes and uses BA's layout algorithm. Falls back to built-in grid layout without BA.

8. **Runtime actions require PIE** -- All `runtime_*` actions only work during Play-In-Editor sessions. They locate SM instances via actor label and component name in the PIE world.

## Common Workflows

### Create SM from JSON Spec (Fastest)
```
logicdriver_query({ action: "build_sm_from_spec", params: {
  save_path: "/Game/StateMachines/SM_EnemyAI",
  spec: {
    entry_state: "Idle",
    states: [
      { name: "Idle", class: "USMStateInstance" },
      { name: "Patrol", class: "USMStateInstance" },
      { name: "Chase", class: "USMStateInstance" },
      { name: "Attack", class: "USMStateInstance" }
    ],
    transitions: [
      { source: "Idle", target: "Patrol", condition_class: "USMTransitionInstance" },
      { source: "Patrol", target: "Chase", condition_class: "USMTransitionInstance" },
      { source: "Chase", target: "Attack", condition_class: "USMTransitionInstance" },
      { source: "Attack", target: "Idle", condition_class: "USMTransitionInstance" }
    ]
  }
}})
```

### Scaffold + Customize
```
logicdriver_query({ action: "scaffold_horror_encounter", params: {
  save_path: "/Game/StateMachines/SM_GhostEncounter",
  phases: ["Ambient", "Whispers", "Apparition", "Chase", "Vanish"]
}})
logicdriver_query({ action: "set_state_property", params: {
  asset_path: "/Game/StateMachines/SM_GhostEncounter",
  node_id: "Chase",
  property_name: "bAlwaysUpdate",
  value: true
}})
logicdriver_query({ action: "compile_state_machine", params: {
  asset_path: "/Game/StateMachines/SM_GhostEncounter"
}})
```

### Inspect + Validate
```
logicdriver_query({ action: "get_state_machine", params: {
  asset_path: "/Game/StateMachines/SM_EnemyAI"
}})
logicdriver_query({ action: "validate_state_machine", params: {
  asset_path: "/Game/StateMachines/SM_EnemyAI"
}})
logicdriver_query({ action: "visualize_sm_as_text", params: {
  asset_path: "/Game/StateMachines/SM_EnemyAI",
  format: "mermaid"
}})
```

### Add SM Component to Actor
```
logicdriver_query({ action: "add_sm_component", params: {
  blueprint_path: "/Game/Blueprints/BP_Enemy",
  sm_asset_path: "/Game/StateMachines/SM_EnemyAI",
  component_name: "BehaviorSM"
}})
logicdriver_query({ action: "configure_sm_component", params: {
  blueprint_path: "/Game/Blueprints/BP_Enemy",
  component_name: "BehaviorSM",
  properties: { bAutoStart: true, bTickBeforeBeginPlay: false }
}})
```

### Manual State Machine Building
```
logicdriver_query({ action: "create_state_machine", params: {
  save_path: "/Game/StateMachines/SM_Door"
}})
logicdriver_query({ action: "add_state", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  state_class: "USMStateInstance",
  state_name: "Closed"
}})
logicdriver_query({ action: "add_state", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  state_class: "USMStateInstance",
  state_name: "Opening"
}})
logicdriver_query({ action: "add_state", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  state_class: "USMStateInstance",
  state_name: "Open"
}})
logicdriver_query({ action: "add_transition", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  source_node: "Closed",
  target_node: "Opening"
}})
logicdriver_query({ action: "add_transition", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  source_node: "Opening",
  target_node: "Open"
}})
logicdriver_query({ action: "set_entry_state", params: {
  asset_path: "/Game/StateMachines/SM_Door",
  node_id: "Closed"
}})
logicdriver_query({ action: "compile_state_machine", params: {
  asset_path: "/Game/StateMachines/SM_Door"
}})
```

### Runtime PIE Debugging
```
logicdriver_query({ action: "runtime_get_active_states", params: {
  actor_label: "BP_Enemy_1"
}})
logicdriver_query({ action: "runtime_force_state", params: {
  actor_label: "BP_Enemy_1",
  state_name: "Chase"
}})
logicdriver_query({ action: "runtime_send_event", params: {
  actor_label: "BP_Enemy_1",
  event_name: "PlayerDetected"
}})
```

## Anti-Patterns to Validate

- **Orphan states** -- States with no incoming or outgoing transitions (except entry). `validate_state_machine` catches these.
- **Dead-end states** -- Non-terminal states with no outgoing transitions. Causes SM to get stuck.
- **Unreachable states** -- States that can't be reached from the entry point. Flagged by validation.
- **Missing compile** -- Editing graph structure without calling `compile_state_machine` afterward. SM won't reflect changes at runtime.
- **Circular transitions without exit** -- State loops with no terminal or conditional exit path.
- **Unattached SM** -- State machine exists but no actor has an SMInstance component referencing it. Use `add_sm_component` to wire it up.

## Tips

- **Use `build_sm_from_spec`** for complex SMs -- it creates the full graph in one call, much faster than adding states/transitions individually.
- **Use scaffolds** for common patterns -- `scaffold_horror_encounter` and `scaffold_weapon` are tuned for this project's FPS survival horror genre.
- **Always compile after structural edits** -- `compile_state_machine` is required after adding/removing states or transitions.
- **Validate before PIE** -- `validate_state_machine` catches common mistakes before runtime testing.
- **Text visualization** -- `visualize_sm_as_text` with `mermaid` format is great for documentation and quick reviews.
- **Runtime debugging** -- Use `runtime_get_active_states` and `runtime_get_variables` during PIE to inspect live SM behavior without pausing.
