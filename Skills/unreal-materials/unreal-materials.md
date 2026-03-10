---
name: unreal-materials
description: Use when creating, editing, or inspecting Unreal Engine materials via Monolith MCP. Covers PBR setup, graph building, material instances, templates, HLSL nodes, validation, and previews. Triggers on material, shader, PBR, texture, material instance, material graph.
---

# Unreal Material Workflows

You have access to **Monolith** with 25 material actions via `material_query()`.

## Discovery

```
monolith_discover({ namespace: "material" })
```

## Asset Path Conventions

All asset paths follow UE content browser format (no .uasset extension):

| Location | Path Format | Example |
|----------|------------|---------|
| Project Content/ | `/Game/Path/To/Asset` | `/Game/Materials/M_Rock` |
| Project Plugins/ | `/PluginName/Path/To/Asset` | `/CarnageFX/Materials/M_Blood` |
| Engine Plugins | `/PluginName/Path/To/Asset` | `/Niagara/DefaultAssets/SystemAssets/NS_Default` |

## Key Parameter Names

- `asset_path` — the material asset path (NOT `asset`)

## Action Reference (25 actions)

### Read Actions (10)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `get_all_expressions` | `asset_path` | List all expression nodes in a material |
| `get_expression_details` | `asset_path`, `expression` | Inspect a specific node's properties and pins |
| `get_full_connection_graph` | `asset_path` | Complete node/wire topology |
| `export_material_graph` | `asset_path`, `include_properties`?, `include_positions`? | Serialize graph as JSON. Pass `include_properties: false` to reduce payload by ~70% |
| `validate_material` | `asset_path` | Check for broken connections, unused nodes, errors |
| `render_preview` | `asset_path` | Trigger material compilation and preview |
| `get_thumbnail` | `asset_path`, `save_to_file`? | Get material thumbnail image. Use `save_to_file: true` to save to disk instead of inline base64 |
| `get_layer_info` | `asset_path` | Inspect material layer/blend stack |
| `get_material_parameters` | `asset_path` | List all parameter types (scalar, vector, texture, static switch) with values. Works on UMaterial and UMaterialInstanceConstant |
| `get_compilation_stats` | `asset_path` | Sampler count, texture estimates, UV scalars, blend mode, expression count |

### Write Actions (15)

| Action | Key Params | Purpose |
|--------|-----------|---------|
| `create_material` | `asset_path`, `blend_mode`?, `shading_model`? | Create new UMaterial with configurable defaults |
| `create_material_instance` | `asset_path`, `parent_material`, `parameters`? | Create UMaterialInstanceConstant from parent |
| `set_material_property` | `asset_path`, `property`, `value` | Set material properties (blend_mode, shading_model, two_sided, etc.) |
| `build_material_graph` | `asset_path`, `nodes`, `connections` | Create an entire graph from a JSON spec (fastest path) |
| `disconnect_expression` | `asset_path`, `expression` | Remove connections from a node (supports expr→material property) |
| `delete_expression` | `asset_path`, `expression` | Delete expression node by name |
| `create_custom_hlsl_node` | `asset_path`, `code`, `inputs`, `outputs` | Add a Custom HLSL expression |
| `set_expression_property` | `asset_path`, `expression`, `property`, `value` | Set properties on expression nodes |
| `connect_expressions` | `asset_path`, `from`, `to` | Wire expression outputs to inputs or material properties |
| `set_instance_parameter` | `asset_path`, `parameter`, `value`, `type` | Set scalar/vector/texture/static switch on MICs |
| `duplicate_material` | `asset_path`, `destination_path` | Duplicate material asset to new path |
| `recompile_material` | `asset_path` | Force material recompile |
| `import_material_graph` | `asset_path`, `graph` | Deserialize graph from JSON. Mode: "overwrite" or "merge" |
| `begin_transaction` | `asset_path`, `description` | Start an undo group |
| `end_transaction` | `asset_path` | End an undo group |

## PBR Material Workflow

### 1. Create a material and build the full graph in one call
```
material_query({ action: "build_material_graph", params: {
  asset_path: "/Game/Materials/M_Rock",
  create_if_missing: true,
  nodes: [
    { type: "TextureSample", name: "BaseColor", params: { Texture: "/Game/Textures/T_Rock_D" } },
    { type: "TextureSample", name: "Normal", params: { Texture: "/Game/Textures/T_Rock_N", SamplerType: "Normal" } },
    { type: "TextureSample", name: "ORM", params: { Texture: "/Game/Textures/T_Rock_ORM" } }
  ],
  connections: [
    { from: "BaseColor.RGB", to: "Material.BaseColor" },
    { from: "Normal.RGB", to: "Material.Normal" },
    { from: "ORM.R", to: "Material.AmbientOcclusion" },
    { from: "ORM.G", to: "Material.Roughness" },
    { from: "ORM.B", to: "Material.Metallic" }
  ]
}})
```

### 2. Create a material instance with parameter overrides
```
material_query({ action: "create_material_instance", params: {
  asset_path: "/Game/Materials/MI_Rock_Wet",
  parent_material: "/Game/Materials/M_Rock",
  parameters: { Roughness_Scale: 0.3, Tint: [0.8, 0.7, 0.6, 1.0] }
}})
```

### 3. Validate after changes
```
material_query({ action: "validate_material", params: { asset_path: "/Game/Materials/M_Rock" } })
```

## Editing Existing Materials

Always inspect before modifying:
```
material_query({ action: "get_all_expressions", params: { asset_path: "/Game/Materials/M_Skin" } })
material_query({ action: "get_full_connection_graph", params: { asset_path: "/Game/Materials/M_Skin" } })
```

Wrap modifications in transactions for undo support:
```
material_query({ action: "begin_transaction", params: { asset_path: "/Game/Materials/M_Skin", description: "Add emissive" } })
// ... make changes ...
material_query({ action: "end_transaction", params: { asset_path: "/Game/Materials/M_Skin" } })
```

## Rules

- **Graph editing only works on base Materials**, not MaterialInstanceConstants (use `set_instance_parameter` for MICs)
- The primary asset param is `asset_path` (not `asset`)
- Always call `validate_material` after graph changes
- `build_material_graph` is the fastest way to create complex graphs — single JSON spec for all nodes + wires
- Use `export_material_graph` to snapshot a graph before making destructive changes
- Use `get_all_expressions` + `get_full_connection_graph` for inspection. Only use `export_material_graph` for round-tripping. Pass `include_properties: false` to reduce payload by ~70%
- Use `render_preview` or `get_thumbnail` with `save_to_file: true` — inline base64 wastes context window
- There are exactly 25 material actions — use `monolith_discover("material")` to see them all
