---
name: unreal-combograph
description: Use when working with ComboGraph plugin via Monolith MCP — creating and editing combo graphs, nodes, edges, effects, cues, and scaffolding combo abilities. Triggers on combo, combo graph, combo node, combo edge, combo ability, combo montage, attack chain, hit sequence.
---

# Unreal ComboGraph Workflows

You have access to **Monolith** with **13 ComboGraph actions** across 5 categories via `combograph_query()`.

## Discovery

Always discover available actions first:
```
monolith_discover({ namespace: "combograph" })
```

## Key Parameter Names

- `asset_path` — the ComboGraph asset path (e.g., `/Game/Combos/CG_LightAttack`)
- `graph_name` — optional graph name within a multi-graph asset
- `node_id` — node identifier within a combo graph
- `montage_path` — animation montage asset path for combo nodes
- `ability_path` — gameplay ability asset path (for linking)
- `save_path` — destination path for new assets

## Action Reference

### Read (4)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `list_combo_graphs` | `path_filter`? | List all ComboGraph assets in project |
| `get_combo_graph_info` | `asset_path` | Read full graph structure: nodes, edges, entry points, effects |
| `get_combo_node_effects` | `asset_path`, `node_id` | Read gameplay effects applied by a specific combo node |
| `validate_combo_graph` | `asset_path` | Lint graph: orphan nodes, missing montages, broken edges, unreachable nodes |

### Create (4)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `create_combo_graph` | `save_path`, `graph_name`? | Create a new ComboGraph asset |
| `add_combo_node` | `asset_path`, `montage_path`, `node_name`? | Add a node to the combo graph with an animation montage |
| `add_combo_edge` | `asset_path`, `source_node`, `target_node`, `input_type`? | Add a transition edge between two combo nodes |
| `set_combo_node_effects` | `asset_path`, `node_id`, `effects` | Set gameplay effects applied when a combo node activates |
| `set_combo_node_cues` | `asset_path`, `node_id`, `cues` | Set gameplay cues triggered by a combo node |

### Scaffold (3)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `create_combo_ability` | `save_path`, `combo_graph_path`, `ability_name`? | Create a Gameplay Ability that drives a ComboGraph |
| `link_ability_to_combo_graph` | `ability_path`, `combo_graph_path` | Link an existing ability to a ComboGraph asset |
| `scaffold_combo_from_montages` | `save_path`, `montages`, `graph_name`? | Scaffold a complete combo graph from an ordered list of montages |

### Layout

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `layout_combo_graph` | `asset_path`, `horizontal_spacing`?, `vertical_spacing`? | Auto-layout nodes left-to-right (BFS tree). Asset must have been opened in editor at least once |

## Key Technical Notes

1. **Precompiled plugin (reflection only)** — ComboGraph is a marketplace plugin distributed as precompiled binaries. MonolithComboGraph interacts with it via UObject reflection and the Asset Registry, NOT by linking against ComboGraph's C++ API directly. All property access uses `FindPropertyByName` / `FProperty::GetValue_InContainer`. This means Monolith works with any ComboGraph version as long as the reflected property names haven't changed.

2. **EdGraph sync** — ComboGraph assets contain both a runtime graph (UComboGraph) and an editor graph (UEdGraph). When modifying nodes or edges programmatically, both graphs must stay in sync. The action handlers manage this automatically — they update the runtime graph and reconstruct the EdGraph representation so the ComboGraph editor shows correct state.

3. **UComboGraphFactory** — New ComboGraph assets are created via `UComboGraphFactory` (discovered via reflection from the ComboGraph plugin's factory registry). If the factory class is not found at runtime, `create_combo_graph` returns an error indicating ComboGraph is not installed.

4. **`#if WITH_COMBOGRAPH` conditional** — `MonolithComboGraph.Build.cs` probes for ComboGraph in project `Plugins/` and engine `Plugins/Marketplace/`. Defines `WITH_COMBOGRAPH=1` or `=0`. When ComboGraph is absent, the module compiles to an empty stub (0 actions registered). Full details in the [Optional Modules wiki](https://github.com/tumourlove/monolith/wiki/Optional-Modules).

5. **Settings toggle** — `bEnableComboGraph` in `UMonolithSettings` (default: true). When disabled, 0 actions registered even if ComboGraph is installed. Check Editor Preferences > Plugins > Monolith.

6. **GAS integration** — `create_combo_ability` and `link_ability_to_combo_graph` create/modify Gameplay Abilities that reference ComboGraph assets. These actions require both ComboGraph AND GameplayAbilities plugins. The combo ability scaffolding wires up `UAbilityTask_RunComboGraph` (or equivalent) inside the ability graph.

## Common Workflows

### Create Combo Graph from Scratch
```
combograph_query({ action: "create_combo_graph", params: {
  save_path: "/Game/Combos/CG_LightAttack"
}})
combograph_query({ action: "add_combo_node", params: {
  asset_path: "/Game/Combos/CG_LightAttack",
  montage_path: "/Game/Animations/AM_Slash_1",
  node_name: "Slash1"
}})
combograph_query({ action: "add_combo_node", params: {
  asset_path: "/Game/Combos/CG_LightAttack",
  montage_path: "/Game/Animations/AM_Slash_2",
  node_name: "Slash2"
}})
combograph_query({ action: "add_combo_edge", params: {
  asset_path: "/Game/Combos/CG_LightAttack",
  source_node: "Slash1",
  target_node: "Slash2",
  input_type: "LightAttack"
}})
```

### Scaffold Combo from Montage List
```
combograph_query({ action: "scaffold_combo_from_montages", params: {
  save_path: "/Game/Combos/CG_HeavyCombo",
  montages: [
    "/Game/Animations/AM_Heavy_1",
    "/Game/Animations/AM_Heavy_2",
    "/Game/Animations/AM_Heavy_3",
    "/Game/Animations/AM_Heavy_Finisher"
  ],
  graph_name: "HeavyCombo"
}})
```

### Add Effects to Combo Nodes
```
combograph_query({ action: "set_combo_node_effects", params: {
  asset_path: "/Game/Combos/CG_LightAttack",
  node_id: "Slash2",
  effects: ["/Game/GAS/Effects/GE_Damage_Light"]
}})
combograph_query({ action: "set_combo_node_cues", params: {
  asset_path: "/Game/Combos/CG_LightAttack",
  node_id: "Slash2",
  cues: ["GameplayCue.Combat.Hit.Light"]
}})
```

### Create Ability from Combo Graph
```
combograph_query({ action: "create_combo_ability", params: {
  save_path: "/Game/GAS/Abilities/GA_LightCombo",
  combo_graph_path: "/Game/Combos/CG_LightAttack",
  ability_name: "LightCombo"
}})
```

### Inspect and Validate
```
combograph_query({ action: "get_combo_graph_info", params: {
  asset_path: "/Game/Combos/CG_LightAttack"
}})
combograph_query({ action: "validate_combo_graph", params: {
  asset_path: "/Game/Combos/CG_LightAttack"
}})
```

## Anti-Patterns to Validate

- **Orphan nodes** — Nodes with no incoming or outgoing edges (except entry nodes). `validate_combo_graph` catches these.
- **Missing montages** — Nodes referencing montage assets that don't exist or have been deleted. Flagged by validation.
- **Circular edges without exit** — Edge loops with no terminal node. Causes infinite combo chains.
- **Unlinked ability** — Combo graph exists but no ability references it. Use `link_ability_to_combo_graph` to connect.
- **Effect on entry node** — Applying damage effects to the first node before the attack animation plays. Usually unintended.

## Tips

- **Use `scaffold_combo_from_montages`** for quick setup — it creates the graph, adds all nodes in sequence, and wires edges automatically. Then customize individual nodes with effects and cues.
- **Validate after editing** — `validate_combo_graph` catches common mistakes before runtime testing.
- **GAS integration** — Always pair combo graphs with abilities via `create_combo_ability` or `link_ability_to_combo_graph` for proper GAS activation flow.
- **ComboGraph editor** — After modifying via MCP, open the asset in the ComboGraph editor to visually verify the graph structure. The EdGraph sync ensures your changes are visible.
