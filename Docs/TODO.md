# Monolith — TODO

Last updated: 2026-03-27

---

### MonolithMesh Module — 46 Actions, Phases 0-4 Complete (2026-03-27)

- [x] **Phase 0 — Module scaffold:** MonolithMesh module, Build.cs, uplugin entry, MeshCatalogIndexer
- [x] **Phase 1 — Mesh Inspection (12):** `get_mesh_info`, `get_mesh_bounds`, `get_mesh_materials`, `get_mesh_lods`, `get_mesh_collision`, `get_mesh_uvs`, `analyze_skeletal_mesh`, `analyze_mesh_quality`, `compare_meshes`, `get_vertex_data`, `search_meshes_by_size`, `get_mesh_catalog_stats`
- [x] **Phase 2 — Scene Manipulation (8):** `get_actor_info`, `spawn_actor`, `move_actor`, `duplicate_actor`, `delete_actors`, `group_actors`, `set_actor_properties`, `batch_execute`
- [x] **Phase 3 — Spatial Queries (11):** `query_raycast`, `query_multi_raycast`, `query_radial_sweep`, `query_overlap`, `query_nearest`, `query_line_of_sight`, `get_actors_in_volume`, `get_scene_bounds`, `get_scene_statistics`, `get_spatial_relationships`, `query_navmesh`
- [x] **Phase 4 — Level Blockout (15):** `get_blockout_volumes`, `get_blockout_volume_info`, `setup_blockout_volume`, `create_blockout_primitive`, `create_blockout_primitives_batch`, `create_blockout_grid`, `match_asset_to_blockout`, `match_all_in_volume`, `apply_replacement`, `set_actor_tags`, `clear_blockout`, `export_blockout_layout`, `import_blockout_layout`, `scan_volume`, `scatter_props`

#### MonolithMesh — Remaining Work (Phases 5-12)

- [ ] **Phase 5 — Mesh modification (GeometryScript):** Decimate, merge meshes, generate LODs, auto-UV, bake normals. Requires GeometryScripting plugin (optional dep already wired).
- [ ] **Phase 6 — Instanced mesh support:** Foliage/HISM manipulation, instance transforms, bulk operations.
- [ ] **Phase 7 — Procedural mesh generation:** Create meshes from vertex data, spline meshes, landscape queries.
- [ ] **Phase 8 — Level streaming integration:** Streaming level queries, sub-level actor management.
- [ ] **Phase 9 — Physics body operations:** Physics asset inspection/modification, constraint setup.
- [ ] **Phase 10 — Lightmap/UV2 tooling:** Lightmap UV generation, density analysis, overlap detection.
- [ ] **Phase 11 — Mesh merge/proxy:** HLOD proxy generation, mesh merge for draw call reduction.
- [ ] **Phase 12 — Skill file + agent integration:** `unreal-mesh` skill, agent preloading, wiki Tool-Reference update.

---

### Niagara Expansion — 31 New Actions (2026-03-25)

- [x] **Dynamic Inputs (5):** `list_dynamic_inputs`, `get_dynamic_input_tree`, `remove_dynamic_input`, `get_dynamic_input_value`, `get_dynamic_input_inputs`
- [x] **Emitter Management (3):** `rename_emitter`, `get_emitter_property`, `list_available_renderers`
- [x] **Renderer Configuration (3):** `set_renderer_mesh`, `configure_ribbon`, `configure_subuv`
- [x] **Event Handlers (3):** `get_event_handlers`, `set_event_handler_property`, `remove_event_handler`
- [x] **Simulation Stages (3):** `get_simulation_stages`, `set_simulation_stage_property`, `remove_simulation_stage`
- [x] **Module Outputs (1):** `get_module_output_parameters`
- [x] **Niagara Parameter Collections (5):** `create_npc`, `get_npc`, `add_npc_parameter`, `remove_npc_parameter`, `set_npc_default`
- [x] **Effect Types (3):** `create_effect_type`, `get_effect_type`, `set_effect_type_property`
- [x] **Utilities (5):** `get_available_parameters`, `preview_system`, `diff_systems`, `save_emitter_as_template`, `clone_module_overrides`
- [x] **`move_module` rewrite** — Complete rewrite of move_module to preserve input overrides
- [x] **7 bug fixes** — type fallback warning, spawn shape duplicate, NPC namespace mismatch, + 4 others found during testing
- [x] **Full test pass** — 40/40 PASS, 0 FAIL, 3 SKIPPED. Total Niagara actions: 65 → 96.

---

### Material Function Full Suite (2026-03-25)

- [x] **export_function_graph** — Full graph export with connections, properties, switch details (#7)
- [x] **set_function_metadata** — Update description, categories, library exposure
- [x] **delete_function_expression** — Remove expression(s) from function
- [x] **update_material_function** — Recompile cascade to referencing materials
- [x] **create_function_instance** — Create MFI with parent + optional overrides
- [x] **set_function_instance_parameter** — Set param overrides on MFI
- [x] **get_function_instance_info** — Read MFI parent chain + overrides (11 param types)
- [x] **layout_function_expressions** — Auto-arrange function graph
- [x] **rename_function_parameter_group** — Rename param group
- [x] **create_material_function type param** — Support MaterialLayer/MaterialLayerBlend creation

---

### MonolithUI Module (2026-03-22)

- [x] **NEW: MonolithUI module** — IMPLEMENTED (2026-03-22). 42 actions across 8 action classes in the `ui` namespace (`ui_query` tool). Widget blueprint CRUD, slot layout, HUD/menu templates, styling, UMG animation CRUD, binding inspection, settings scaffolding, accessibility.
- [x] **MonolithUI — MCP testing** — DONE (2026-03-22). All 42 actions PASS. Full test results in TESTING.md.
- [ ] **MonolithUI — Phase 3 deprecation warnings** — `GetBindings` and `FMovieSceneBinding::GetName` may generate deprecation warnings in UE 5.7. Audit and update call sites.
- [x] **MonolithUI — agent skill file** — DONE (2026-03-22). `unreal-ui` skill created in `.claude/skills/` and `Plugins/Monolith/Skills/`, added to `interface-architect` agent definition.
- [x] **MonolithUI — bUIEnabled settings toggle** — DONE (2026-03-22). `UMonolithSettings::bUIEnabled` wired to registration gating (confirmed in SPEC.md settings table).

---

### Recently Fixed (2026-03-14)

- [x] **NEW: Niagara `set_system_property`** — ADDED (2026-03-14). Sets a system-level property (WarmupTime, bDeterminism, etc.) via reflection.
- [x] **NEW: Niagara `set_static_switch_value`** — ADDED (2026-03-14). Sets a static switch value on a module.
- [x] **NEW: Niagara `list_module_scripts`** — ADDED (2026-03-14). Searches available Niagara module scripts by keyword. Returns matching script asset paths.
- [x] **NEW: Niagara `list_renderer_properties`** — ADDED (2026-03-14). Lists editable properties on a renderer via reflection. Params: `asset_path`, `emitter`, `renderer`.
- [x] **Niagara DI class name auto-prefix** — FIXED (2026-03-14). `set_module_input_di` and `get_di_functions` now auto-resolve DI class names: both `NiagaraDataInterfaceCurve` and `UNiagaraDataInterfaceCurve` are accepted (U prefix stripped/added as needed).
- [x] **Niagara `get_module_inputs` returns DI curve data** — FIXED (2026-03-14). Now returns actual FRichCurve key data for DataInterface curve inputs, not just the DI class name.
- [x] **LinearColor/vector defaults deserialization** — FIXED (2026-03-14). `get_module_inputs` correctly deserializes LinearColor and vector default values from string-serialized JSON fallback, no longer returns zeroed values.
- [x] **Material `disconnect_expression` targeted disconnection** — FIXED (2026-03-14). Now supports disconnecting a specific connection via optional `input_name`/`output_name` params, instead of always disconnecting all connections on the expression.
- [x] **Blueprint `add_node` aliases and K2_ prefix fallback** — FIXED (2026-03-14). `add_node` now resolves common node class aliases (e.g. `CallFunction`, `VariableGet`) and automatically tries the `K2_` prefix for function call nodes when the bare name doesn't resolve.
- [x] **Niagara `list_renderers` returns `type` short name** — FIXED (2026-03-14). The `type` field now returns the short renderer class name (e.g. `SpriteRenderer`) instead of the full UClass path.

### Recently Fixed

- [x] **Niagara `set_emitter_property` SimTarget — "Data missing please force a recompile"** — FIXED (2026-03-13). Raw field assignment on `SimTarget` skipped `PostEditChangeVersionedProperty`, so `MarkNotSynchronized` was never called and `RequestCompile(false)` saw unchanged hash → skipped compilation. Fix: call `PostEditChangeVersionedProperty` + `RebuildEmitterNodes` + `SynchronizeOverviewGraphWithSystem` after SimTarget change. Same pattern applied to `bLocalSpace` and `bDeterminism`.
- [x] **Niagara `list_emitters` missing emitter GUID** — FIXED (2026-03-13). Added `"id": Handle.GetId().ToString()` to the emitter listing. Users now have a stable round-trip token.
- [x] **Niagara `get_system_diagnostics` — NEW ACTION** — Added (2026-03-13). Returns compile errors, warnings, renderer/SimTarget incompatibility, GPU+dynamic bounds warnings, and per-script stats (op count, registers, compile status). Also added `CalculateBoundsMode` to `set_emitter_property`.

- [x] **Niagara `create_system` + `add_emitter` — emitters don't persist** — FIXED (2026-03-13). Replaced raw `System->AddEmitterHandle()` with `FNiagaraEditorUtilities::AddEmitterToSystem()` which calls `RebuildEmitterNodes` + `SynchronizeOverviewGraphWithSystem` after adding the handle. Also added `SavePackage` call in both `HandleCreateSystem` and `HandleAddEmitter`. Custom emitter names applied post-add via `SetName()`.
- [x] **Niagara `create_system_from_spec` — fails with `failed_steps:1`** — FIXED (2026-03-13). Added synchronous `RequestCompile(true)` + `WaitForCompilationComplete()` after each emitter add in the spec flow, before modules are added. Removed redundant async `RequestCompile(false)` from `HandleAddEmitter`. Also added error message capture — failed sub-operations now report in an `"errors"` array instead of silent `FailCount++`.
- [x] **All MCP tools return stale in-memory objects after asset recreate** — FIXED (2026-03-13). `LoadAssetByPath` now queries `IAssetRegistry::GetAssetByObjectPath()` + `FAssetData::GetAsset()` first (reflects editor ground truth), falling back to `StaticLoadObject` only if the Asset Registry has no record. Prevents stale `RF_Standalone` ghosts from shadowing recreated assets.
- [x] **Niagara `set_module_input_value` — namespace warnings on compile** — FIXED (2026-03-13). `MatchedFullName` was assigned the stripped short name instead of the full `Module.`-prefixed name from `In.GetName()`. Same fix applied to `HandleSetModuleInputBinding`. Both now pass the full name to `FNiagaraParameterHandle::CreateAliasedModuleParameterHandle`.
- [x] **Registry-level required param validation** — ADDED (2026-03-13). `FMonolithToolRegistry::Execute()` now validates required params from schema before dispatching to the handler. Checks all schema keys marked `required: true`, skips `asset_path` (handled by `GetAssetPath()` with aliases). Returns error listing missing + provided keys.
- [x] **Niagara param name aliases** — ADDED (2026-03-13). All module write actions (`set_module_input_value`, `set_module_input_binding`, `set_module_input_di`, `set_curve_value`, `remove_module`, `move_module`, `set_module_enabled`, `get_module_inputs`, `get_module_graph`) now accept `module_name` and `module` as aliases for the canonical `module_node` param. `input_name` accepted as alias for `input`.
- [x] **`set_expression_property` PostEditChange fix** — FIXED (2026-03-13). Was calling `PostEditChange()` (no args), which didn't rebuild the material graph. Now calls `PostEditChangeProperty(FPropertyChangedEvent(Prop))` with the actual property so `MaterialGraph->RebuildGraph()` fires and the editor display updates correctly.
- [x] **Auto-recompile + PostEditChange on 4 material write actions** — FIXED (2026-03-13). `set_material_property`, `create_material`, `delete_expression`, and `connect_expressions` now all call `Mat->PreEditChange(nullptr)` + `Mat->PostEditChange()` to trigger recompile and push changes through the material graph system.
- [x] **`tools/list` embeds per-action param schemas** — IMPLEMENTED (2026-03-13). `FMonolithHttpServer::HandleToolsList()` now builds the `params` property description with per-action param documentation in `*name(type)` format (`*` = required). AI clients can see all param names and types from the MCP tool list without calling `monolith_discover` first.

### Minor

- [x] **Material `validate_material` false positive islands** — FIXED (2026-03-09). Added MP_MaterialAttributes + 6 missing properties to AllMaterialProperties, seeded BFS from UMaterialExpressionMaterialAttributeLayers. 0 false positives on standard materials. Layer-blend materials still have a known limitation (implicit layer system connections not traversable via pin graph).
- [x] **Blueprint `get_execution_flow` matches comments before events** — FIXED (2026-03-09). Two-pass FindEntryNode: Pass 1 checks events/functions (prefers K2Node_Event, K2Node_FunctionEntry), Pass 2 is fuzzy fallback that skips EdGraphNode_Comment.

---

## Unimplemented Features (stubs in code)

- [x] **Niagara `create_module_from_hlsl`** — DONE (2026-03-15). Creates standalone NiagaraScript with CustomHlsl node, typed I/O pins, ParameterMap flow. Bypasses unexported APIs via UPROPERTY reflection + Signature-driven pin creation.

- [x] **Niagara `create_function_from_hlsl`** — DONE (2026-03-15). Same path as module, `ENiagaraScriptUsage::Function` with direct typed pin wiring.

- [ ] **SSE streaming** — DEFERRED. `MonolithHttpServer.cpp` SSE endpoint returns a single event and closes. Comment: "Full SSE streaming will be implemented when we need server-initiated notifications."
  - **File:** `Source/MonolithCore/Private/MonolithHttpServer.cpp` (~line 232)

- [x] **C++ source indexer — native port complete** — DONE (2026-03-15). `MonolithSource` module now runs a native C++ indexer via `UMonolithSourceSubsystem`. The Python tree-sitter indexer (`Scripts/source_indexer/`) is legacy and no longer invoked. New action: `trigger_project_reindex` for incremental project-only C++ re-index. New commandlet: `UMonolithQueryCommandlet` (-run=MonolithQuery) replaces `monolith_offline.py` as the preferred offline access path.

- [x] **Python indexer: capture full class/struct definitions** — FIXED (2026-03-08). Added UE macro preprocessor that strips UCLASS/USTRUCT/UENUM/UINTERFACE, *_API, GENERATED_BODY() before tree-sitter parsing. 62,059 definitions now captured (was near-zero).

- [x] **Source index: ancestor traversal** — FIXED (2026-03-08). Inheritance table now has 37,010 entries across 34,444 classes. AActor→UObject, APawn→AActor, ACharacter→APawn all working.

---

## Feature Improvements

### Platform

- [ ] **Mac/Linux support** — DEFERRED (Windows-only project). All build-related actions are `#if PLATFORM_WINDOWS` guarded. Live Coding is Windows-only. Update system is Windows-only.

### Niagara Module — Improvements

- [ ] **`FindEmitterHandleIndex` should accept numeric index** — `list_emitters` returns `"index"` for each emitter. Allow passing `"0"`, `"1"` etc. as emitter identifier for convenient fallback.
  - **File:** `Source/MonolithNiagara/Private/MonolithNiagaraActions.cpp` (~line 292)

### Animation Module — Wishlist

Priority features identified for future waves:

- [x] **Wave 1 — Read actions (EASY, ~8 actions):** DONE (2026-03-10)
- [x] **Wave 2 — Notify CRUD (EASY, ~4 actions):** DONE (2026-03-10)
- [x] **Wave 3 — Curve CRUD (EASY, ~5 actions):** DONE (2026-03-10)
- [x] **Wave 4 — Skeleton sockets (EASY, ~4 actions):** DONE (2026-03-10) — expanded to 6 actions (added set_blend_space_axis, set_root_motion_settings)
- [x] **Wave 5 — Creation + editing (MODERATE, ~6 actions):** DONE (2026-03-10)
- [x] **Wave 6 — PoseSearch/Motion Matching (MODERATE, ~5 actions):** DONE (2026-03-10)
- [x] **Wave 7 — Anim Modifiers + Composites (MODERATE, ~5 actions):** DONE (2026-03-10)
- [ ] **Wave 8 — IKRig + Control Rig (HARD, ~6 actions):** `get_ikrig_info`, `add_ik_solver`, `get_retargeter_info`, `get_control_rig_info` — requires IKRig/ControlRig module dependencies
- [ ] **Deferred — ABP write ops (HARD):** State machine structural writes (add state/transition) require Blueprint graph mutation, high complexity

---

## Documentation

- [ ] **CI pipeline** — Per Phase 6 plan

---

## Completed

- [x] Core infrastructure (HTTP server, registry, settings, JSON utils, asset utils)
- [x] All 11 domain modules compiling clean on UE 5.7
- [x] SQLite FTS5 project indexer with 14 indexers (Blueprint, Material, Generic, Dependency, Animation, Niagara, DataTable, Level, GameplayTag, Config, Cpp, UserDefinedEnum, UserDefinedStruct, InputAction)
- [x] Python tree-sitter engine source indexer
- [x] Auto-updater via GitHub Releases
- [x] 10 Claude Code skills (including unreal-build and unreal-ui)
- [x] Templates (.mcp.json, CLAUDE.md)
- [x] README, LICENSE, ATTRIBUTION
- [x] HTTP body null-termination fix
- [x] Niagara graph traversal fix (emitter shared graph)
- [x] Niagara emitter lookup hardening (case-insensitive + fallbacks)
- [x] Source DB WAL -> DELETE journal mode fix
- [x] Asset loading 4-tier fallback
- [x] SQL schema creation (BEGIN/END depth tracking for triggers)
- [x] Reindex dispatch fix (FindFunctionByName -> StartFullIndex + UFUNCTION)
- [x] Asset loading crash fix (removed FastGetAsset from background thread)
- [x] Animation `remove_bone_track` — now uses `RemoveBoneCurve(FName)` per bone + child traversal (2026-03-07)
- [x] MonolithIndex `last_full_index` — added `WriteMeta()` call, guarded with `!bShouldStop` (2026-03-07)
- [x] Niagara `move_module` — rewires stack-flow pins only, preserves override inputs (2026-03-07)
- [x] Editor `get_build_errors` — uses `ELogVerbosity` enum instead of substring matching (2026-03-07)
- [x] MonolithIndex SQL injection — all 13 insert methods converted to `FSQLitePreparedStatement` (2026-03-07)
- [x] Animation `LogTemp` -> `LogMonolith` (2026-03-07)
- [x] Editor `CachedLogCapture` dangling pointer — added `ClearCachedLogCapture()` in ShutdownModule (2026-03-07)
- [x] MonolithSource vestigial outer module — flattened structure, deleted stub (2026-03-07)
- [x] Session expiry / reconnection — Removed session tracking entirely. Sessions stored no per-session state and only caused bugs when server restarted. Server is now fully stateless. (2026-03-07)
- [x] Claude tools fail on first invocation — Fixed transport type mismatch in .mcp.json ("http" → "streamableHttp") and fixed MonolithSource stub that wasn't registering actions. (2026-03-07)
- [x] Module enable toggles — settings now checked before registering actions (2026-03-07)
- [x] MCP package CLI cleanup — removed abandoned scaffold (2026-03-07)
- [x] Material action count alignment — skill updated to match C++ reality (2026-03-07)
- [x] Animation action count alignment — skill updated to match C++ reality (2026-03-07)
- [x] Niagara action count alignment — skill updated to match C++ reality (2026-03-07)
- [x] Config action schema documentation — `explain_setting` convenience mode documented in skill (2026-03-07)
- [x] Niagara `reorder_emitters` safety — proper change notifications added (2026-03-07)
- [x] `diff_from_default` INI parsing — rewritten with GConfig/FConfigCacheIni (2026-03-07)
- [x] Config `diff_from_default` enhancement — now compares all 5 config layers (2026-03-07)
- [x] Live Coding trigger action (`editor.live_compile`) — fully implemented (2026-03-07)
- [x] Cross-platform update system — tar/unzip support added (2026-03-07)
- [x] Hot-swap plugin updates — delayed file swap mechanism implemented (2026-03-07)
- [x] Remove phase plan .md files from Source/ — moved to Docs/plans/ (2026-03-07)
- [x] AnimationIndexer — AnimSequence, AnimMontage, BlendSpace indexing (2026-03-07)
- [x] NiagaraIndexer — NiagaraSystem, NiagaraEmitter deep indexing (2026-03-07)
- [x] DataTableIndexer — DataTable row indexing (2026-03-07)
- [x] LevelIndexer — Level/World actor indexing (2026-03-07)
- [x] GameplayTagIndexer — Tag hierarchy indexing (2026-03-07)
- [x] ConfigIndexer — INI config indexing (2026-03-07)
- [x] CppIndexer — C++ symbol indexing (2026-03-07)
- [x] Deep asset indexing — safe game-thread loading strategy implemented (2026-03-07)
- [x] Incremental indexing — delta updates from file change detection (2026-03-07)
- [x] Asset change detection — hooked into Asset Registry callbacks (2026-03-07)
- [x] API reference page — auto-generated API_REFERENCE.md with 119 actions (2026-03-07)
- [x] Contribution guide — CONTRIBUTING.md created (2026-03-07)
- [x] Changelog — CHANGELOG.md created (2026-03-07)
- [x] Clean up MCP/ package — removed abandoned CLI scaffold (2026-03-07)
- [x] `find_callers` / `find_callees` param name fix — `"function"` → `"symbol"` (2026-03-07)
- [x] `read_file` param name fix — `"path"` → `"file_path"` (2026-03-07)
- [x] `read_file` path normalization — forward slash → backslash for DB suffix matching (2026-03-07)
- [x] `get_class_hierarchy` forward-declaration filtering — prefer real definitions over `class X;` (2026-03-07)
- [x] `ExtractMembers` rewrite — brace depth tracking for Allman-style UE code (2026-03-07)
- [x] `get_recent_logs` — accepts both `"max"` and `"count"` param names (2026-03-07)
- [x] `search_config` category filter — changed param read from `"file"` to `"category"` (2026-03-07)
- [x] `get_section` category name resolution — accepts `"Engine"` not just `"DefaultEngine"` (2026-03-07)
- [x] SQLite WAL → DELETE — belt-and-suspenders fix: C++ forces DELETE on open + Python indexer never sets WAL (2026-03-07)
- [x] Source DB ReadOnly → ReadWrite — WAL + ReadOnly silently returns 0 rows on Windows (2026-03-07)
- [x] Reindex absolute path — `FPaths::ConvertRelativePathToFull()` on engine source + shader paths (2026-03-07)
- [x] MonolithHttpServer top-level param merge — params alongside `action` were silently dropped, now merged (2026-03-07)
- [x] UE macro preprocessor — strips UCLASS/USTRUCT/UENUM/UINTERFACE, *_API, GENERATED_BODY() before tree-sitter parsing (2026-03-08)
- [x] Source indexer --clean flag — deletes existing DB before reindexing (2026-03-08)
- [x] Inheritance resolution — 37,010 links across 34,444 classes, full ancestor chains working (2026-03-08)
- [x] Diagnostic counters — definitions/forward_decls/with_base_classes/inheritance_resolved/failed printed after indexing (2026-03-08)
- [x] Preprocessor in ReferenceBuilder — consistent AST for cross-reference extraction (2026-03-08)
- [x] Auto-updater rewrite — tasklist polling, move retry loop 10x3s, errorlevel fix, cmd /c quoting, DelayedExpansion, xcopy /h, rollback rmdir. Windows end-to-end tested v0.4.0→v0.5.0 (2026-03-08)
- [x] Release script `Scripts/make_release.ps1` — sets `"Installed": true` in zip for BP-only users (2026-03-08)
- [x] BP-only support — release zips work without rebuild for Blueprint-only projects (2026-03-08)
- [x] GitHub Wiki — 11 pages: Installation, Tool Reference, Test Status, Auto-Updater, FAQ, Changelog, etc. (2026-03-08)
- [x] Indexer auto-index deferred to `IAssetRegistry::OnFilesLoaded()` — was running too early, only indexing 193/9560 assets (2026-03-09)
- [x] Indexer sanity check — if < 500 assets indexed, skip writing `last_full_index` so next launch retries (2026-03-09)
- [x] Indexer `bIsIndexing` reset in `Deinitialize()` to prevent stuck flag (2026-03-09)
- [x] Index DB changed from WAL to DELETE journal mode (2026-03-09)
- [x] Niagara `trace_parameter_binding` — fixed missing OR fallback for `User.` prefix (2026-03-09)
- [x] Niagara `get_di_functions` — fixed reversed class name pattern, now tries `UNiagaraDataInterface<Name>` (2026-03-09)
- [x] Niagara `batch_execute` — fixed 3 op name mismatches, old names kept as aliases (2026-03-09)
- [x] Niagara actions now accept `asset_path` (preferred) with `system_path` as backward-compat alias (2026-03-09)
- [x] Niagara `duplicate_emitter` accepts `emitter` as alias for `source_emitter` (2026-03-09)
- [x] Niagara `set_curve_value` accepts `module_node` as alias for `module` (2026-03-09)
- [x] NEW: Niagara `list_emitters` action — returns emitter names, index, enabled, sim_target, renderer_count (2026-03-09)
- [x] NEW: Niagara `list_renderers` action — returns renderer class, index, enabled, material (2026-03-09)
- [x] Animation state machine names stripped of `\n` — clean names like "InAir" not "InAir\nState Machine" (2026-03-09)
- [x] Animation `get_state_info` validates required params (machine_name, state_name) (2026-03-09)
- [x] Animation state machine matching changed from fuzzy Contains() to exact match (2026-03-09)
- [x] Animation `get_nodes` now accepts optional `graph_name` filter (2026-03-09)
- [x] NEW: Blueprint `get_graph_summary` — lightweight graph overview (id/class/title + exec connections only, ~10KB vs 172KB) (2026-03-09)
- [x] Blueprint `get_graph_data` now accepts optional `node_class_filter` param (2026-03-09)
- [x] Blueprint `get_variables` now reads default values from CDO (was always empty) (2026-03-09)
- [x] Blueprint indexer CDO fix — same default value extraction fix applied to BlueprintIndexer (2026-03-09)
- [x] Material `export_material_graph` now accepts `include_properties` and `include_positions` params (2026-03-09)
- [x] Material `get_thumbnail` now accepts `save_to_file` param (2026-03-09)
- [x] Niagara `get_compiled_gpu_hlsl` auto-compiles system if HLSL not available (2026-03-09)
- [x] Niagara `User.` prefix stripped in get_parameter_value, trace_parameter_binding, remove_user_parameter, set_parameter_default (2026-03-09)
- [x] Per-action param schemas in `monolith_discover()` output — all 122 actions now have param documentation (2026-03-09)
- [x] Niagara `get_module_inputs` — types now use `PinToTypeDefinition` instead of default Vector4f (2026-03-09)
- [x] Niagara `get_ordered_modules` — usage filter now works with shorthands ("spawn", "update"), returns error on invalid values, returns all stages when omitted (2026-03-09)
- [x] Niagara `get_renderer_bindings` — clean JSON output (name/bound_to/type) instead of raw UE struct dumps (2026-03-09)
- [x] Niagara `get_all_parameters` — added optional `emitter` and `scope` filters (2026-03-09)
- [x] Animation `get_transitions` — cast to `UAnimStateNodeBase*` instead of `UAnimStateNode*`, resolves conduit names. Added from_type/to_type fields (2026-03-09)
- [x] Material `validate_material` — seeds BFS from `UMaterialExpressionCustomOutput` subclasses + `UMaterialExpressionMaterialAttributeLayers`, added MP_MaterialAttributes + 6 missing properties. 0 false positives on standard materials (2026-03-09)
- [x] Blueprint `get_execution_flow` — two-pass FindEntryNode: Pass 1 prefers events/functions, Pass 2 fuzzy fallback skips comments (2026-03-09)
- [x] Blueprint `get_graph_summary` all-graphs mode — returns all graphs when graph_name empty, single graph when specified (2026-03-09)
- [x] **CRITICAL: Hot-swap updater deletes Saved/** — swap script moved entire plugin dir to backup, then only preserved .git/.github. EngineSource.db (1.8GB) and ProjectIndex.db were destroyed on cleanup. Fixed: both static .bat script and C++ template (Windows + Mac/Linux) now preserve Saved/ alongside .git (2026-03-10)
- [x] Material `build_material_graph` class lookup — `FindObject<UClass>(nullptr, ClassName)` always returned null. Changed to `FindFirstObject<UClass>(ClassName, NativeFirst)` with U-prefix fallback. Short names like "Constant", "VectorParameter" now resolve correctly (2026-03-10)
- [x] Material `disconnect_expression` missing material outputs — `disconnect_outputs=true` only iterated other expressions' inputs, never checked material output properties. Added `GetExpressionInputForProperty()` loop over `MaterialOutputEntries` (2026-03-10)
- [x] NEW: Material `create_material` — creates UMaterial asset at path with Opaque/DefaultLit/Surface defaults (2026-03-10)
- [x] NEW: Material `create_material_instance` — creates UMaterialInstanceConstant from parent material with parameter overrides (2026-03-10)
- [x] NEW: Material `set_material_property` — sets material properties (blend_mode, shading_model, etc.) via UMaterialEditingLibrary::SetMaterialUsage (2026-03-10)
- [x] NEW: Material `delete_expression` — deletes expression node by name from material graph (2026-03-10)
- [x] NEW: Material `get_material_parameters` — returns scalar/vector/texture/static_switch parameter arrays with values, works on UMaterial and UMaterialInstanceConstant (2026-03-10)
- [x] NEW: Material `set_instance_parameter` — sets scalar/vector/texture/static_switch parameters on MIC (2026-03-10)
- [x] NEW: Material `recompile_material` — forces material recompile via UMaterialEditingLibrary::RecompileMaterial (2026-03-10)
- [x] NEW: Material `duplicate_material` — duplicates material asset to new path via UEditorAssetLibrary::DuplicateAsset (2026-03-10)
- [x] NEW: Material `get_compilation_stats` — returns sampler count, texture estimates, UV scalars, blend mode, expression count. API corrected for UE 5.7 FMaterialResource (2026-03-10)
- [x] NEW: Material `set_expression_property` — sets properties on expression nodes (e.g., DefaultValue on scalar param) (2026-03-10)
- [x] NEW: Material `connect_expressions` — wires expression outputs to expression inputs or material property inputs. Supports expr-to-expr and expr-to-material-property (2026-03-10)
- [x] **Niagara `get_module_inputs` returns all input types** — IMPLEMENTED (2026-03-11). Now uses engine's `FNiagaraStackGraphUtilities::GetStackFunctionInputs` with `FCompileConstantResolver`. Returns floats, vectors, colors, data interfaces, enums, bools — not just static switch pins. Output uses short names (no `Module.` prefix).
- [x] **Niagara `batch_execute` missing write ops** — FIXED (2026-03-11). Dispatch table now covers all 23 write op types. Previously missing 8: `remove_user_parameter`, `set_parameter_default`, `set_module_input_di`, `set_curve_value`, `reorder_emitters`, `duplicate_emitter`, `set_renderer_binding`, `request_compile`.
- [x] **Niagara `set_module_input_di` validation** — FIXED (2026-03-11). Now validates input exists and is DataInterface type before applying. Rejects nonexistent inputs and non-DI type inputs with descriptive errors. `config` param now accepts JSON object (not just string).
- [x] **Niagara `create_system_from_spec` functional** — FIXED (2026-03-11). Was broken — now uses `UNiagaraSystemFactoryNew::InitializeSystem` for proper system creation.
- [x] **Niagara `FindEmitterHandleIndex` auto-select removed** — FIXED (2026-03-11). Was silently auto-selecting the single emitter when a specific non-matching name was passed. Now requires the name to match if one is provided, returning a clear error instead.
- [x] **Niagara write actions accept both short and prefixed input names** — IMPLEMENTED (2026-03-11). `set_module_input_value`, `set_module_input_binding`, `set_module_input_di`, `set_curve_value` all accept both `InputName` (short) and `Module.InputName` (prefixed) forms.
- [x] **CRASH: `add_virtual_bone` no bone validation** — FIXED (2026-03-10). Added `FReferenceSkeleton::FindBoneIndex()` validation for both source and target bones before calling `AddNewVirtualBone()`. Previously created bogus virtual bones with non-existent bones, causing array OOB crash on skeleton access.
- [x] **`set_notify_time` / `set_notify_duration` reject AnimMontage** — FIXED (2026-03-10). Changed `LoadAssetByPath<UAnimSequence>` to `LoadAssetByPath<UAnimSequenceBase>` so montages and composites are accepted. Also made `set_notify_duration` error message include `(total: N)` to match `set_notify_time`.
- [x] **`remove_virtual_bones` false success for non-existent bones** — FIXED (2026-03-10). Now validates each bone name against actual virtual bones before removal. Returns `not_found` array and errors if all names are invalid.
- [x] **`delete_montage_section` allows deleting last section** — FIXED (2026-03-10). Added guard: if montage has only 1 section remaining, returns error "Cannot delete the last remaining montage section".
- [x] **`add_blendspace_sample` generic error on skeleton mismatch** — FIXED (2026-03-10). Added skeleton comparison before adding sample, returns descriptive error naming both skeletons when they don't match.
- [x] **Animation Waves 1-7: 39 new actions** — IMPLEMENTED (2026-03-10). Total animation module: 62 actions + 5 PoseSearch = 67. Waves: 8 read actions, 4 notify CRUD, 5 curve CRUD, 6 skeleton+blendspace, 6 creation+montage, 5 PoseSearch, 5 modifiers+composites. Build errors fixed: BlendParameters private, GetTargetSkeleton removed, UMirrorDataTable forward-decl, GetBoneAnimationTracks deprecated, OpenBracket FText.
- [x] **Blueprint module upgrade: 6 → 46 actions** — IMPLEMENTED (2026-03-13). Added 40 new write actions across 5 categories: Variable CRUD (7), Component CRUD (6), Graph Management (9), Node & Pin Operations (6), Compile & Create (5). Also expanded Read Actions to 13 (added get_components, get_component_details, get_functions, get_event_dispatchers, get_parent_class, get_interfaces, get_construction_script). Total plugin actions: 177 → 217.
- [x] **Offline CLI (`monolith_offline.py`)** — IMPLEMENTED (2026-03-13). Pure Python (stdlib only) CLI that queries `EngineSource.db` and `ProjectIndex.db` directly without the editor running. 14 actions across 2 namespaces: `source` (9 actions, mirrors `source_query`) and `project` (5 actions, mirrors `project_query`). Read-only, zero footprint, zero dependencies. Fallback for when MCP/editor is unavailable. Location: `Saved/monolith_offline.py`.
- [x] **NEW: MonolithUI module** — IMPLEMENTED (2026-03-22). New module at `Source/MonolithUI/`. 42 actions in `ui` namespace (`ui_query` tool). 8 action classes: FMonolithUIActions (7), FMonolithUISlotActions (3), FMonolithUITemplateActions (8), FMonolithUIStylingActions (6), FMonolithUIAnimationActions (5), FMonolithUIBindingActions (4), FMonolithUISettingsActions (5), FMonolithUIAccessibilityActions (4).
- [x] **NEW: MonolithMesh module** — IMPLEMENTED (2026-03-27). New module at `Source/MonolithMesh/`. 46 actions in `mesh` namespace (`mesh_query` tool). 4 action classes: FMonolithMeshInspectionActions (12), FMonolithMeshSceneActions (8), FMonolithMeshSpatialActions (11), FMonolithMeshBlockoutActions (15). MeshCatalogIndexer added to MonolithIndex.
