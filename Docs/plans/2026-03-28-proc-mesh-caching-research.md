# Procedural Mesh Caching & Reuse System — Research

**Date:** 2026-03-28
**Scope:** MonolithMesh procedural geometry actions (8 actions in Phase 19A-D)
**Goal:** Avoid regenerating identical procedural meshes; auto-save as StaticMesh assets; JSON prefab collections as lightweight alternative to Level Instances.

---

## 1. Current Architecture

### 1.1 Procedural Action Flow

All 8 procedural actions (`create_parametric_mesh`, `create_horror_prop`, `create_structure`, `create_building_shell`, `create_maze`, `create_pipe_network`, `create_fragments`, `create_terrain_patch`) follow this pattern:

```
1. Parse params (type, dimensions, sub-params, seed)
2. NewObject<UDynamicMesh>(Pool)
3. Dispatch to type-specific builder (e.g. BuildChair, BuildBarricade)
4. CleanupMesh (normals / self-union)
5. FinalizeProceduralMesh OR inline save/place/handle logic
```

**Two code paths exist:**
- `CreateParametricMesh` and `CreateHorrorProp` — inline save/place/handle at the end of each function (lines 500-560, 680-720)
- All Phase 19B-D actions — call shared `FinalizeProceduralMesh(Mesh, Params, Result, HandleCategory)` (line 1521)

Both paths do the same three optional steps:
1. Store in handle pool (`Pool->CreateHandle` + copy mesh data)
2. Save to asset (`SaveMeshToAsset` -> temp handle -> `Pool->SaveHandle`)
3. Place in scene (`PlaceMeshInScene` -> spawn `AStaticMeshActor`)

### 1.2 SaveMeshToAsset Pipeline

`SaveMeshToAsset` (line 264) creates a temporary handle, copies the DynamicMesh into it, then delegates to `Pool->SaveHandle` which:

1. Converts `UDynamicMesh` -> `FMeshDescription` via `FDynamicMeshToMeshDescription`
2. `CreatePackage` at the target path
3. `NewObject<UStaticMesh>` into the package
4. `BuildFromMeshDescriptions` (builds render data, collision, etc.)
5. `UPackage::SavePackage` to disk
6. `FAssetRegistryModule::AssetCreated` notification

**Key observation:** The `save_path` parameter is entirely user-specified. There is no automatic naming, no duplicate detection, and no organization convention enforced.

### 1.3 Existing Prefab Support

`create_prefab` (line 804) wraps `ULevelInstanceSubsystem::CreateLevelInstanceFrom` but **blocks MCP** because the engine's dialog cannot be suppressed (bUseSaveAs=true). The code documents this limitation.

`spawn_prefab` (line 896) works fine — loads an existing level asset and spawns it.

This means Level Instances work for *spawning* but not *creation* via MCP. A JSON-based prefab system would bypass this entirely.

---

## 2. Hashing Strategy

### 2.1 What Makes Two Procedural Meshes Identical?

A procedural mesh is fully determined by its **input parameters**. Two calls with the same `{type, dimensions, params, seed}` produce bit-identical geometry (all builders are deterministic given the same `FRandomStream` seed).

Therefore: **hash the canonical parameter set, not the mesh geometry**.

### 2.2 Canonical Parameter Representation

To produce a deterministic hash, we need a canonical JSON string from the generation parameters. The approach:

1. Extract the "identity fields" from the input params (exclude transient fields like `handle`, `save_path`, `overwrite`, `place_in_scene`, `location`, `rotation`, `label`)
2. Sort JSON keys alphabetically (recursive for nested objects)
3. Normalize numeric precision (e.g., round to 2 decimal places to avoid float drift)
4. Serialize to a canonical JSON string
5. Hash with FMD5 or FSHA1

**Identity fields per action:**

| Action | Identity Fields |
|--------|----------------|
| `create_parametric_mesh` | `type`, `dimensions`, `params` |
| `create_horror_prop` | `type`, `dimensions`, `params`, `seed` |
| `create_structure` | `type`, `dimensions`, `wall_thickness`, `floor_thickness`, `has_ceiling`, `has_floor`, `openings` |
| `create_building_shell` | `footprint`, `floors`, `floor_height`, `wall_thickness`, `floor_thickness`, `stairwell_cutout` |
| `create_maze` | `algorithm`, `grid_size`, `cell_size`, `wall_height`, `wall_thickness`, `seed`, `merge_walls` |
| `create_pipe_network` | `path_points`, `radius`, `segments`, `miter_limit`, `ball_joints`, `joint_radius_scale` |
| `create_fragments` | `source_handle` (resolved to source hash), `fragment_count`, `noise`, `seed`, `gap_width` |
| `create_terrain_patch` | `size`, `resolution`, `amplitude`, `frequency`, `octaves`, `persistence`, `lacunarity`, `seed` |

### 2.3 Recommended Hash Implementation

```cpp
// Proposed helper — sits in MonolithMeshProceduralActions or a new MonolithMeshCache utility
static FString ComputeParamHash(const FString& ActionName, const TSharedPtr<FJsonObject>& Params)
{
    // 1. Build canonical object with only identity fields
    TSharedPtr<FJsonObject> Canonical = MakeShared<FJsonObject>();
    Canonical->SetStringField(TEXT("_action"), ActionName);

    // Copy identity fields (exclude handle, save_path, overwrite, place_in_scene,
    // location, rotation, label)
    static const TSet<FString> ExcludeKeys = {
        TEXT("handle"), TEXT("save_path"), TEXT("overwrite"),
        TEXT("place_in_scene"), TEXT("location"), TEXT("rotation"), TEXT("label")
    };

    for (const auto& Pair : Params->Values)
    {
        if (!ExcludeKeys.Contains(Pair.Key))
        {
            Canonical->SetField(Pair.Key, Pair.Value);
        }
    }

    // 2. Serialize to sorted canonical JSON
    FString CanonicalStr = SortedJsonSerialize(Canonical); // recursive key-sort

    // 3. MD5 hash -> 32-char hex string
    FMD5 Md5;
    Md5.Update((uint8*)TCHAR_TO_UTF8(*CanonicalStr), CanonicalStr.Len());

    uint8 Digest[16];
    Md5.Final(Digest);

    return BytesToHex(Digest, 16).ToLower(); // e.g. "a3f8c2e1..."
}
```

**Why MD5?** Not for security — purely for content addressing. MD5 is fast, 32-char hex is compact, and collision probability across procedural mesh params is effectively zero. UE already ships `FMD5` in Core. `FSHA1` or CityHash64 are also fine alternatives. The key point is determinism and speed.

**Sorted JSON serialization** is critical. UE's `FJsonObject::Values` is a `TMap<FString, ...>` which does NOT guarantee iteration order. We must extract keys, sort them, and serialize in that order. This is a ~20-line helper.

### 2.4 Float Normalization

Floating point representation can vary (`100.0` vs `100.00000001`). Two options:

**Option A — Quantize before hashing:** Round all numeric values to N decimal places (e.g., 2 for cm-scale dimensions). Simple, robust, loses sub-mm precision but that's irrelevant for blockout meshes.

**Option B — Use integer representation:** Multiply by 100, cast to int. E.g., `45.5 cm -> 4550`. More deterministic but requires knowing the scale of each field.

**Recommendation:** Option A with 2 decimal places. The procedural builders themselves don't produce meaningfully different geometry from `45.001 cm` vs `45.00 cm`.

---

## 3. Manifest / Registry

### 3.1 JSON Manifest File

A single manifest file maps parameter hashes to saved asset paths. Location follows existing Monolith convention:

```
Plugins/Monolith/Saved/Monolith/ProceduralCache/manifest.json
```

Schema:

```json
{
    "version": 1,
    "entries": {
        "a3f8c2e1d4b5...": {
            "asset_path": "/Game/Generated/Parametric/Chair/SM_Chair_45x45x90_a3f8c2",
            "action": "create_parametric_mesh",
            "type": "chair",
            "dimensions": { "width": 45, "depth": 45, "height": 90 },
            "triangle_count": 312,
            "created_utc": "2026-03-28T14:30:00Z",
            "params_json": "{ ... full canonical params ... }"
        },
        "b7e2f1a9c3d6...": {
            "asset_path": "/Game/Generated/Horror/Barricade/SM_Barricade_120x10x200_s42_b7e2f1",
            "action": "create_horror_prop",
            "type": "barricade",
            "dimensions": { "width": 120, "depth": 10, "height": 200 },
            "seed": 42,
            "triangle_count": 456,
            "created_utc": "2026-03-28T14:31:00Z",
            "params_json": "{ ... }"
        }
    }
}
```

### 3.2 Why Not Use Asset Registry Tags Instead?

UE 5.7 supports custom asset registry tags via `GetAssetRegistryTags(FAssetRegistryTagsContext)` override. We could subclass `UStaticMesh` or use `UAssetUserData` to attach a "MonolithParamHash" tag to each saved mesh, then query the Asset Registry for it.

**Pros of AR tags:**
- Integrated with UE's existing search/filter system
- Survives asset moves/renames (tag travels with asset)
- Queryable without loading the asset

**Cons of AR tags:**
- Requires subclassing `UStaticMesh` or attaching user data — changes the asset type downstream
- Tag queries (`IAssetRegistry::GetAssetsByTagValues`) require the asset to be scanned/registered
- Slower than a simple JSON lookup for hit/miss checks
- More complex to implement, harder to debug

**Cons of JSON manifest:**
- Falls out of sync if assets are deleted/moved outside Monolith
- Requires periodic validation pass

**Recommendation:** Use the JSON manifest as primary (fast, simple, follows existing Monolith patterns like Patterns/AcousticProfiles/Presets directories). Add a `validate_cache` action that scans manifest entries and removes stale ones where the asset no longer exists (`FPackageName::DoesPackageExist`). Optionally, also store the hash as `UMetaData` on the asset for disaster recovery / manual browsing.

### 3.3 Staleness Detection

When the manifest says hash X maps to `/Game/Generated/Foo`, verify the asset still exists before returning it:

```cpp
if (FPackageName::DoesPackageExist(CachedPath))
{
    // Cache hit — return existing asset path
}
else
{
    // Asset was deleted — remove stale entry, regenerate
    Manifest.Remove(Hash);
}
```

This is already the pattern used in `SaveHandle` (line 171 of HandlePool).

---

## 4. Folder Organization

### 4.1 Proposed Structure

```
/Game/Generated/
    Parametric/
        Chair/
            SM_Chair_45x45x90_a3f8c2.uasset
            SM_Chair_60x60x100_e1d4b5.uasset
        Table/
            SM_Table_120x75x75_c9f2a1.uasset
        ...
    Horror/
        Barricade/
            SM_Barricade_120x10x200_s42_b7e2f1.uasset
        Coffin/
            SM_Coffin_60x200x50_s0_d3e4f5.uasset
        ...
    Structure/
        Room/
            SM_Room_400x600x300_f2a1b3.uasset
        Corridor/
            ...
    BuildingShell/
        SM_Building_3F_300h_a1b2c3.uasset
    Maze/
        SM_Maze_8x8_RB_s42_d4e5f6.uasset
    Pipe/
        SM_Pipe_r10_s12_e7f8a9.uasset
    Terrain/
        SM_Terrain_2000x2000_a4_f0.01_b1c2d3.uasset
```

### 4.2 Naming Convention

```
SM_{Type}_{DimensionSummary}[_s{Seed}]_{HashPrefix6}.uasset
```

- `SM_` prefix — standard UE convention for StaticMesh
- `{Type}` — lowercase type name (Chair, Barricade, Room, etc.)
- `{DimensionSummary}` — `{W}x{D}x{H}` in cm, integers
- `_s{Seed}` — only for seeded types (horror props, maze, terrain, fragments)
- `_{HashPrefix6}` — first 6 chars of the param hash for uniqueness

This produces human-readable names that sort well and are unique. The hash suffix handles cases where two meshes have the same type and dimensions but differ in sub-params.

### 4.3 Auto-Path Generation

When the user calls `create_parametric_mesh` WITHOUT specifying `save_path`, the cache system generates the path automatically:

```cpp
FString AutoSavePath = FString::Printf(TEXT("/Game/Generated/%s/%s/SM_%s_%dx%dx%d_%s"),
    *Category,  // "Parametric", "Horror", "Structure", etc.
    *Type,      // "Chair", "Barricade", "Room", etc.
    *Type,
    FMath::RoundToInt(Width), FMath::RoundToInt(Depth), FMath::RoundToInt(Height),
    *Hash.Left(6));
```

If the user DOES specify `save_path`, use that instead but still register in the manifest.

---

## 5. Integration Into create_* Workflow

### 5.1 Modified Flow

```
1. Parse params
2. Compute param hash
3. Check manifest for hash
   a. HIT + asset exists → return cached path, skip generation
   b. HIT + asset missing → remove stale entry, continue to generate
   c. MISS → continue to generate
4. Build mesh (existing code, unchanged)
5. CleanupMesh
6. Determine save_path (user-specified OR auto-generated)
7. SaveMeshToAsset
8. Register in manifest (hash -> path + metadata)
9. Handle pool / place in scene (existing code)
10. Return result with cache_hit: false, save_path, hash
```

On cache hit:

```
1. Parse params
2. Compute param hash
3. Manifest hit, asset confirmed to exist
4. Skip mesh generation entirely
5. If handle requested: load StaticMesh -> convert to DynamicMesh -> store in handle pool
6. If place_in_scene: spawn actor from cached asset
7. Return result with cache_hit: true, save_path, hash
```

### 5.2 New Optional Parameters

Add to all 8 procedural actions:

| Param | Type | Default | Description |
|-------|------|---------|-------------|
| `use_cache` | boolean | `true` | Check cache before generating. Set `false` to force regeneration. |
| `auto_save` | boolean | `true` | Auto-save to `/Game/Generated/...` even without explicit `save_path`. |

### 5.3 Where to Put the Code

**Option A — Modify `FinalizeProceduralMesh`:** Add cache-check at the START of each action, and cache-register at the end of `FinalizeProceduralMesh`. Problem: `CreateParametricMesh` and `CreateHorrorProp` don't use `FinalizeProceduralMesh` — they have inline logic.

**Option B (recommended) — New `FMonolithMeshProceduralCache` class:** Encapsulates manifest I/O, hash computation, cache lookup, and auto-path generation. Each action calls:

```cpp
// At the top:
FString Hash = FMonolithMeshProceduralCache::ComputeHash(TEXT("create_parametric_mesh"), Params);
FString CachedPath;
if (FMonolithMeshProceduralCache::TryGetCached(Hash, CachedPath))
{
    // Build result from cached asset, handle pool load if needed
    return BuildCacheHitResult(Hash, CachedPath, Params);
}

// ... existing generation code ...

// At the end (in FinalizeProceduralMesh or inline):
FMonolithMeshProceduralCache::Register(Hash, SavePath, Metadata);
```

Also refactor `CreateParametricMesh` and `CreateHorrorProp` to use `FinalizeProceduralMesh` for consistency (they should have used it from the start).

### 5.4 New MCP Actions

| Action | Description |
|--------|-------------|
| `list_cached_meshes` | List all entries in the manifest with metadata (type, dimensions, path, age) |
| `clear_cache` | Delete all cached assets and clear manifest (or filter by type/age) |
| `validate_cache` | Check all manifest entries against disk, remove stale ones |
| `get_cache_stats` | Counts by type, total disk size estimate, hit/miss ratio if tracked |

---

## 6. JSON Prefab System

### 6.1 Motivation

Level Instances have two problems for MCP workflows:
1. `CreateLevelInstanceFrom` triggers a blocking Save As dialog
2. They're heavyweight — a full sub-level with streaming, world partition, etc.

For "spawn these 5 meshes as a group at this location," a lightweight JSON definition is better.

### 6.2 Prefab JSON Schema

```json
{
    "name": "HospitalBedWithGurney",
    "version": 1,
    "description": "Hospital bed flanked by gurney and IV stand",
    "origin": "center_bottom",
    "elements": [
        {
            "mesh_path": "/Game/Generated/Parametric/Bed/SM_Bed_100x200x55_a1b2c3",
            "relative_location": [0, 0, 0],
            "relative_rotation": [0, 0, 0],
            "scale": [1, 1, 1],
            "material_overrides": {
                "0": "/Game/Materials/M_Fabric_White"
            },
            "label": "HospitalBed"
        },
        {
            "mesh_path": "/Game/Generated/Horror/Gurney/SM_Gurney_70x190x90_s0_d4e5f6",
            "relative_location": [150, 0, 0],
            "relative_rotation": [0, 15, 0],
            "scale": [1, 1, 1],
            "material_overrides": {},
            "label": "Gurney"
        }
    ],
    "tags": ["horror", "hospital", "furniture_group"],
    "bounding_box": { "min": [-60, -100, 0], "max": [220, 100, 90] }
}
```

### 6.3 Storage Location

```
Plugins/Monolith/Saved/Monolith/Prefabs/
    HospitalBedWithGurney.json
    CorridorSection_A.json
    ...
```

### 6.4 MCP Actions

| Action | Description |
|--------|-------------|
| `create_mesh_prefab` | Define a prefab from a list of mesh paths + relative transforms. Saves JSON to Prefabs/. |
| `spawn_mesh_prefab` | Load a prefab JSON, spawn all elements at a world location with a shared root offset. Returns actor names. |
| `list_mesh_prefabs` | List all saved prefabs with element counts and tags. |
| `capture_mesh_prefab` | Select actors in the scene, capture their mesh paths and relative transforms into a new prefab definition. |

### 6.5 Spawning Implementation

```cpp
// For each element in the prefab:
// 1. Load the StaticMesh from mesh_path
// 2. Spawn AStaticMeshActor at (WorldOrigin + relative_location), (WorldRotation + relative_rotation)
// 3. Apply scale
// 4. Apply material overrides per slot index
// 5. Set actor label to "{PrefabName}_{ElementLabel}"
// 6. Optionally: create a folder in the World Outliner to group them
```

**Grouping option:** Use `AActor::SetFolderPath` to organize spawned actors under a common outliner folder (e.g., `/Prefabs/HospitalBedWithGurney`). This is lightweight and doesn't require Level Instances.

### 6.6 Integration with Caching

Prefab elements reference asset paths. When spawning a prefab, if a referenced mesh doesn't exist:
1. Check the cache manifest — the path might have been generated but deleted
2. If the prefab was authored with param hashes, regenerate the mesh on the fly
3. Report missing meshes in the result

For this, prefab elements can optionally include `generation_params`:

```json
{
    "mesh_path": "/Game/Generated/Parametric/Chair/SM_Chair_45x45x90_a3f8c2",
    "generation_params": {
        "action": "create_parametric_mesh",
        "type": "chair",
        "dimensions": { "width": 45, "depth": 45, "height": 90 }
    },
    "relative_location": [0, 0, 0],
    ...
}
```

If `mesh_path` is missing but `generation_params` is present, the system calls the appropriate create action to regenerate it. This makes prefabs self-healing and portable.

---

## 7. Asset Registry Metadata (Optional Enhancement)

### 7.1 UMetaData Approach (No Subclassing Required)

UE provides `UMetaData` that can be attached to any `UObject` without subclassing:

```cpp
UMetaData* Meta = NewObject<UMetaData>(StaticMesh);
Meta->SetValue(TEXT("MonolithParamHash"), Hash);
Meta->SetValue(TEXT("MonolithAction"), ActionName);
Meta->SetValue(TEXT("MonolithParams"), CanonicalParamsJson);
StaticMesh->AddAssetUserData(Meta);  // not quite — UMetaData isn't UAssetUserData
```

Actually, `UMetaData` in UE is the `UPackage::GetMetaData()` system which stores key-value pairs at the package level. This works but is not queryable through the Asset Registry without loading.

### 7.2 UAssetUserData Approach

A better fit: create a `UMonolithProceduralMeshUserData : UAssetUserData` with UPROPERTY fields marked `AssetRegistrySearchable`:

```cpp
UCLASS()
class UMonolithProceduralMeshUserData : public UAssetUserData
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, AssetRegistrySearchable)
    FString ParamHash;

    UPROPERTY(EditAnywhere, AssetRegistrySearchable)
    FString GeneratorAction;

    UPROPERTY(EditAnywhere)
    FString CanonicalParamsJson;
};
```

Attach after creating the StaticMesh:

```cpp
auto* UserData = NewObject<UMonolithProceduralMeshUserData>(StaticMesh);
UserData->ParamHash = Hash;
UserData->GeneratorAction = ActionName;
UserData->CanonicalParamsJson = CanonicalStr;
StaticMesh->AddAssetUserData(UserData);
```

Then query:

```cpp
FARFilter Filter;
Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());
Filter.TagsAndValues.Add(TEXT("ParamHash"), Hash);
IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
TArray<FAssetData> Results;
AR.GetAssets(Filter, Results);
```

**Verdict:** Nice to have but not required for V1. The JSON manifest is simpler and sufficient. Add AR tags in V2 if users need Content Browser integration.

---

## 8. Implementation Estimate

| Component | Effort | Priority |
|-----------|--------|----------|
| `FMonolithMeshProceduralCache` (hash, manifest I/O, lookup) | 4-6h | P0 |
| Sorted canonical JSON serializer | 1-2h | P0 |
| Integrate cache into 8 procedural actions | 3-4h | P0 |
| Auto-path generation + folder creation | 1-2h | P0 |
| Refactor CreateParametricMesh/CreateHorrorProp to use FinalizeProceduralMesh | 2-3h | P0 (cleanup) |
| `list_cached_meshes`, `clear_cache`, `validate_cache`, `get_cache_stats` | 3-4h | P1 |
| JSON prefab schema + `create_mesh_prefab` | 2-3h | P1 |
| `spawn_mesh_prefab` (spawn + folder grouping) | 3-4h | P1 |
| `list_mesh_prefabs` + `capture_mesh_prefab` | 2-3h | P1 |
| Self-healing prefabs (regenerate missing meshes) | 2-3h | P2 |
| `UAssetUserData` with AR-searchable tags | 3-4h | P2 |

**Total:** ~26-38h across P0-P2

---

## 9. Open Questions

1. **Cache invalidation on Monolith update:** If a builder function changes (e.g., `BuildChair` gets improved geometry), all cached chairs become stale. Options:
   - Include a `builder_version` field in the hash (bump manually when builder logic changes)
   - Accept that users must `clear_cache(type: "chair")` after updates
   - Track Monolith plugin version in manifest and invalidate on version change

2. **Disk budget:** Should there be a max cache size? Procedural blockout meshes are small (~10-50KB each), so even 1000 cached meshes would be ~50MB. Probably fine without limits.

3. **Source handle hashing for `create_fragments`:** The source mesh is identified by a handle name, which is transient. To make fragment caching work, we'd need to resolve the handle to its own param hash (if it was procedurally generated) or to its asset path (if loaded from disk). This is solvable but adds complexity. Could defer fragment caching to V2.

4. **Prefab vs Level Instance:** The JSON prefab system is intentionally simpler than Level Instances. It doesn't support:
   - Nested prefabs (prefab of prefabs)
   - Per-instance property overrides beyond materials
   - Streaming / LOD per-group
   If these are needed, push users toward actual Level Instances (created manually since MCP can't trigger the dialog).

---

## 10. Recommended Phased Rollout

### Phase 1 — Core Caching (P0, ~12-17h)
- `FMonolithMeshProceduralCache` class with manifest I/O
- Deterministic hash computation with sorted JSON
- Cache check at top of each procedural action
- Auto-save with generated paths when `save_path` not specified
- Manifest registration on save
- Staleness check (`DoesPackageExist`)
- Refactor CreateParametricMesh/CreateHorrorProp to use FinalizeProceduralMesh

### Phase 2 — Cache Management + Prefabs (P1, ~10-14h)
- `list_cached_meshes`, `clear_cache`, `validate_cache`, `get_cache_stats` actions
- JSON prefab schema definition
- `create_mesh_prefab`, `spawn_mesh_prefab`, `list_mesh_prefabs`, `capture_mesh_prefab` actions

### Phase 3 — Self-Healing + AR Tags (P2, ~5-7h)
- Prefab `generation_params` for auto-regeneration
- `UMonolithProceduralMeshUserData` with AssetRegistrySearchable properties
- Builder version tracking for cache invalidation
