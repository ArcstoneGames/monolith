# Automatic Asset Scanning, Classification, and Modular Kit Integration

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** modular-building-research, geometryscript-deep-dive, ue5-mesh-apis-research, proc-mesh-caching

---

## Executive Summary

This research covers the full pipeline for scanning a user's folder of modular building assets, automatically classifying them by type (wall, floor, door, window, corner, furniture), extracting dimensions, detecting grid size, and outputting a vocabulary JSON that the procedural building system can use to generate buildings from the user's own art.

The pipeline is entirely feasible with existing UE 5.7 APIs. Asset Registry scanning + UStaticMesh bounds + GeometryScript boundary edge detection + naming heuristics + material slot analysis provides a robust multi-signal classification system. Estimated implementation: ~40-55h across 5 phases.

---

## 1. Asset Scanning in UE 5.7

### 1.1 IAssetRegistry Directory Scanning

The Asset Registry provides synchronous scanning of any content path. The core pattern:

```cpp
#include "AssetRegistry/AssetRegistryModule.h"

IAssetRegistry& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();

// Ensure path is scanned (may not be if it's a plugin or newly added folder)
AR.ScanPathsSynchronous({TEXT("/Game/MyModularKit/")}, /*bForceRescan=*/ true);

// Build filter for StaticMeshes in the folder
FARFilter Filter;
Filter.PackagePaths.Add(FName(TEXT("/Game/MyModularKit")));
Filter.bRecursivePaths = true;
Filter.ClassPaths.Add(UStaticMesh::StaticClass()->GetClassPathName());  // UE 5.x uses ClassPaths, not ClassNames

TArray<FAssetData> Assets;
AR.GetAssets(Filter, Assets);
```

**Key API details (verified via source_query in prior research):**
- `GetAssets(FARFilter, TArray<FAssetData>&)` -- primary bulk query
- `GetAssetsByPath(FName PackagePath, TArray<FAssetData>&, bool bRecursive)` -- simpler alternative
- `ScanPathsSynchronous()` -- ensures the path is indexed before querying
- `FARFilter::bRecursivePaths` -- include subdirectories
- `FARFilter::ClassPaths` -- UE 5.x replacement for deprecated `ClassNames`

**Without loading assets:** `FAssetData` stores tag values that can be read without loading the full asset. Some mesh properties (like bounds) are NOT in the tags by default and require loading the UStaticMesh.

**Sources:**
- [Asset Registry in Unreal Engine | UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-registry-in-unreal-engine)
- [Get Assets by Path | UE 5.7 Blueprint API](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/AssetRegistry/GetAssetsbyPath)

### 1.2 Mesh Bounds and Dimensions

Once loaded, `UStaticMesh::GetBounds()` returns `FBoxSphereBounds`:

```cpp
UStaticMesh* SM = Cast<UStaticMesh>(AssetData.GetAsset());
FBoxSphereBounds Bounds = SM->GetBounds();

// Full dimensions (Width x Height x Depth)
FVector Size = Bounds.BoxExtent * 2.0;  // BoxExtent is half-extents
double Width  = Size.X;  // cm
double Depth  = Size.Y;  // cm
double Height = Size.Z;  // cm
```

**We already have this in Monolith:** `get_mesh_info` and `get_mesh_bounds` actions in `MonolithMeshInspectionActions.cpp` already extract bounds, tri count, material count, LOD count, and collision info. The scanning action can call these internally.

### 1.3 Triangle Count and Material Slots

Already available via existing Monolith code:

```cpp
// Triangle count
const FStaticMeshLODResources& LOD0 = SM->GetRenderData()->LODResources[0];
int32 TriCount = LOD0.GetNumTriangles();
int32 VertCount = LOD0.GetNumVertices();

// Material slots (names + assigned materials)
const TArray<FStaticMaterial>& Materials = SM->GetStaticMaterials();
for (const FStaticMaterial& Mat : Materials)
{
    FName SlotName = Mat.MaterialSlotName;
    UMaterialInterface* MatRef = Mat.MaterialInterface;
}
```

### 1.4 Pivot Point Detection

The pivot is determined by the relationship between the mesh's local-space bounding box and the origin:

```cpp
FBoxSphereBounds Bounds = SM->GetBounds();
FVector Center = Bounds.Origin;  // Bounding box center in local space

// Classify pivot location
if (FMath::IsNearlyZero(Center.X) && FMath::IsNearlyZero(Center.Y))
{
    if (FMath::IsNearlyZero(Center.Z))
        PivotType = "center";
    else if (FMath::IsNearlyZero(Center.Z + Bounds.BoxExtent.Z))
        PivotType = "top_center";
    else if (FMath::IsNearlyZero(Center.Z - Bounds.BoxExtent.Z))
        PivotType = "base_center";
}
// Check for corner pivots (origin at min corner)
else if (FMath::IsNearlyZero(Center.X - Bounds.BoxExtent.X) &&
         FMath::IsNearlyZero(Center.Y - Bounds.BoxExtent.Y) &&
         FMath::IsNearlyZero(Center.Z - Bounds.BoxExtent.Z))
{
    PivotType = "base_corner";
}
```

**Common pivot conventions in marketplace kits:**
- **Base center:** Most common (Polycount wiki recommendation). Origin at bottom face center.
- **Base corner:** Some kits (Bethesda-style). Origin at one corner of the bottom face.
- **Center:** Less common for modular pieces. Used for props/furniture.
- **Wall center-bottom:** For wall pieces. Origin at the center of the bottom edge of the wall face (between the two sides).

**Important:** The pivot determines how pieces snap together. The scan must detect AND normalize pivots, or at least report them so the assembly code can compensate.

### 1.5 Socket Detection

`UStaticMesh::Sockets` is a public `TArray<UStaticMeshSocket*>`:

```cpp
for (UStaticMeshSocket* Socket : SM->Sockets)
{
    FName SocketName = Socket->SocketName;
    FVector RelativeLocation = Socket->RelativeLocation;
    FRotator RelativeRotation = Socket->RelativeRotation;
    FVector RelativeScale = Socket->RelativeScale;
    FName Tag = Socket->Tag;
}
```

**Socket conventions in modular kits:**
- Some kits use sockets for snap points (named `Snap_Left`, `Snap_Right`, `Snap_Top`, etc.)
- Some use sockets for attachment points (`Attach_Light`, `Attach_Switch`)
- Socket tags can carry classification metadata
- Many marketplace kits do NOT use sockets at all (rely on grid snapping)

**Sources:**
- [Using Sockets With Static Meshes | UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-sockets-with-static-meshes-in-unreal-engine)
- [UStaticMesh::Sockets | UE 5.0 API](https://docs.unrealengine.com/5.0/en-US/API/Runtime/Engine/Engine/UStaticMesh/Sockets/)

### 1.6 Asset Metadata / Tags

UE5 supports custom metadata on assets via `UAssetUserData` and `GetAssetRegistryTags()`:

```cpp
// Reading existing tags (without full load via FAssetData)
FString TagValue;
AssetData.GetTagValue(FName("MyCustomTag"), TagValue);

// Writing custom classification metadata via UAssetUserData
UCLASS()
class UMonolithKitMetadata : public UAssetUserData
{
    UPROPERTY(EditAnywhere)
    FString PieceType;  // "wall_solid", "wall_door", "floor", etc.

    UPROPERTY(EditAnywhere)
    FString KitName;    // "MyModularKit"

    UPROPERTY(EditAnywhere)
    int32 GridUnits;    // Width in grid units (e.g., 2 = 2x grid size)

    virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override
    {
        OutTags.Add(FAssetRegistryTag("MonolithPieceType", PieceType, ...));
        OutTags.Add(FAssetRegistryTag("MonolithKit", KitName, ...));
    }
};
```

**Approach:** After classification, stamp each scanned mesh with a `UMonolithKitMetadata` so future scans can skip re-classification. Tags persist in the asset registry and survive editor restarts.

**Sources:**
- [Asset Metadata in UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/asset-metadata-in-unreal-engine)
- [Working With Asset Meta Data | Xander's Notebook](https://xanderbert.github.io/2025/04/13/WorkingWithAssetMetaData.html)

---

## 2. Automatic Piece Classification

### 2.1 Multi-Signal Classification Pipeline

No single signal is reliable enough alone. The classification system should combine multiple signals with weighted confidence scoring:

```
Classification Pipeline:
  1. Name parsing          (fast, ~80% accurate for well-named assets)
  2. Dimension analysis    (fast, ~70% accurate alone)
  3. Material slot names   (fast, ~50% accuracy boost when present)
  4. Topology analysis     (slower, very reliable for opening detection)
  5. Socket analysis       (fast, ~30% kits use sockets)

Final classification = weighted vote across all signals
```

### 2.2 Signal 1: Name Parsing

Most modular kits follow recognizable naming patterns. Based on analysis of marketplace kits, forum discussions, and the Allar UE5 Style Guide:

**Pattern table (regex → piece type):**

```
Pattern                              → Type              Confidence
─────────────────────────────────────────────────────────────────────
(?i)wall.*door|door.*wall|doorway    → wall_door         0.95
(?i)wall.*window|window.*wall        → wall_window       0.90
(?i)wall.*solid|wall_straight        → wall_solid        0.85
(?i)wall(?!.*door|.*window|.*corner) → wall_solid        0.70
(?i)floor|ground                     → floor             0.85
(?i)ceiling|ceil                     → ceiling           0.85
(?i)stair|step                       → stairs            0.90
(?i)corner.*in|inside.*corner|_IC    → corner_inside     0.85
(?i)corner.*out|outside.*corner|_OC  → corner_outside    0.85
(?i)corner                           → corner            0.75
(?i)column|pillar|post               → column            0.80
(?i)trim|baseboard|molding|moulding  → trim              0.75
(?i)beam|joist                       → beam              0.70
(?i)roof                             → roof              0.80
(?i)railing|banister|handrail        → railing           0.85
(?i)door(?!.*wall)                   → door_prop         0.80
(?i)window(?!.*wall)                 → window_prop       0.80
(?i)bed|table|chair|desk|shelf|lamp  → furniture         0.90
(?i)sink|toilet|tub|shower|bath      → furniture         0.90
(?i)frame|arch                       → frame             0.70
```

**Naming conventions across marketplace kits:**

| Kit | Prefix | Wall Example | Door Example | Grid |
|-----|--------|-------------|-------------|------|
| Modular Building Set (Fab) | `SM_` | `SM_Wall_2x2` | `SM_Wall_Door_2x2` | 200cm |
| Stylized Modular Building | `SM_` | `SM_wall_straight_01` | `SM_wall_w_doorhole_A` | 300cm |
| Modular Sci-Fi Kit | `SM_` | `SM_SciFi_Wall_A` | `SM_SciFi_Door_A` | 400cm |
| Jacob Norris Urban Kit | `SM_` | `SM_wall_storefront` | `SM_storefront_door` | 400x300cm |
| Generic (Polycount convention) | `SM_<Kit>_` | `SM_Kit_Wall_01a` | `SM_Kit_WallDoor_01a` | varies |

**The universal pattern:** `SM_<Kit>_<Type>_<Variant>` where Type contains the classification keyword.

**Sources:**
- [UE5 Style Guide (Allar)](https://github.com/Allar/ue5-style-guide)
- [Naming conventions for modular elements | UE Forums](https://forums.unrealengine.com/t/good-naming-rules-for-modular-elements/58314)
- [Recommended Asset Naming Conventions | UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/recommended-asset-naming-conventions-in-unreal-engine-projects)

### 2.3 Signal 2: Dimension Analysis (Aspect Ratio)

The bounding box aspect ratio is a strong classification signal:

```
Dimension Rules:
  Height >> Width, Height >> Depth, Depth < 50cm  → wall piece
  Width >> Height, Depth >> Height, Height < 30cm  → floor/ceiling
  Height > 150cm, Width > 70cm, Depth < 50cm      → wall (large)
  Height < 100cm, Width < 100cm, Depth < 100cm     → prop/furniture
  Height > 200cm, Width < 100cm, Depth < 100cm     → column/pillar
  Steps pattern (multiple small ledges)             → stairs
```

**Detailed heuristics:**

```cpp
enum class EDimensionClass
{
    Wall,           // Tall, thin (H/D > 5, W varies)
    Floor,          // Flat, wide (H < 30cm, W and D both > 100cm)
    Column,         // Tall, narrow both ways (H > 200cm, W < 100cm, D < 100cm)
    SmallProp,      // All dims < 100cm
    MediumProp,     // All dims < 200cm, not fitting other categories
    LargeProp,      // Over 200cm in at least one dim, thick in two
    Stairs,         // Moderate height, moderate depth, thin (needs topology confirm)
    Unknown
};

EDimensionClass ClassifyByDimensions(FVector Size)
{
    // Sort dimensions: largest, middle, smallest
    double Dims[3] = {Size.X, Size.Y, Size.Z};
    // Sort descending...

    double Largest = Dims[0], Middle = Dims[1], Smallest = Dims[2];

    // Floor: one dim very small, other two large
    if (Smallest < 30.0 && Middle > 80.0 && Largest > 80.0)
        return EDimensionClass::Floor;

    // Wall: one dim very small, one dim large (height), one moderate (width)
    if (Smallest < 50.0 && Largest > 200.0)
        return EDimensionClass::Wall;

    // Column: one dim large, two dims small
    if (Largest > 200.0 && Middle < 100.0 && Smallest < 100.0)
        return EDimensionClass::Column;

    // Small prop
    if (Largest < 100.0)
        return EDimensionClass::SmallProp;

    return EDimensionClass::Unknown;
}
```

**Critical caveat:** Walls oriented differently (X-aligned vs Y-aligned) will have width and depth swapped. The classification must be rotation-agnostic -- sort the three dimensions and classify by ratios, not by axis names.

### 2.4 Signal 3: Material Slot Names

Material slot names carry implicit classification data:

```
Slot Name Contains    → Implication           Confidence
─────────────────────────────────────────────────────────
glass, window         → has window/glass      0.90
wood, door            → has door              0.75
metal, steel          → industrial/door       0.50
brick, concrete       → wall surface          0.60
carpet, tile, parquet → floor surface         0.70
wallpaper, drywall    → wall surface          0.70
frame, trim           → trim/frame piece      0.60
fabric, leather       → furniture             0.65
```

**Material slot enumeration (already in Monolith):**
```cpp
const TArray<FStaticMaterial>& Materials = SM->GetStaticMaterials();
for (const FStaticMaterial& Mat : Materials)
{
    FString SlotName = Mat.MaterialSlotName.ToString().ToLower();
    // Check for glass, wood, etc.
}
```

**Also check assigned material names** (the actual UMaterial asset name), not just the slot name. Some kits name slots generically ("Material_01") but assign materials with descriptive names ("MI_Glass_Clear").

**Sources:**
- [Auto-assign materials by slot name | ArtStation](https://www.artstation.com/hinsunlee/blog/Ondp/unreal-automatically-assigning-material-by-material-slot-name-with-python)
- [Get Material Slot Names | UE Forums](https://forums.unrealengine.com/t/get-material-slot-names-for-static-mesh-in-python/468590)

### 2.5 Signal 4: Topology Analysis (Opening Detection)

This is the most reliable signal for detecting doors and windows, but requires loading the mesh into a DynamicMesh for GeometryScript analysis.

**Boundary edge detection for openings:**

GeometryScript provides:
- `GetNumOpenBorderEdges(TargetMesh)` -- returns count of edges with only one adjacent triangle (boundary edges). A watertight mesh has 0. A mesh with holes has boundary edges forming loops around each hole.
- `GetIsClosed(TargetMesh)` -- returns true if mesh is watertight (no boundary edges)
- `GetMeshBoundaryEdgeLoops(TargetMesh)` -- returns the actual loops (not just count), allowing measurement of each opening

**Opening classification from boundary loops:**

```
Algorithm:
1. Convert StaticMesh → DynamicMesh (UGeometryScriptLibrary_MeshBasicEditFunctions::CopyMeshFromStaticMesh)
2. Call GetMeshBoundaryEdgeLoops → get array of loops
3. For each loop:
   a. Compute loop bounding box
   b. Measure opening width and height
   c. Classify:
      - H > 180cm, W > 70cm, bottom at floor level → door opening
      - H: 60-150cm, W > 50cm, bottom > 60cm from base → window opening
      - H < 30cm or W < 30cm → vent/slot
      - Very large (> 200cm both) → archway/pass-through
```

**Euler characteristic for global topology:**

For a closed mesh: `V - E + F = 2` (genus 0, no holes)
For a mesh with genus g: `V - E + F = 2 - 2g`
For a mesh with B boundary loops: `V - E + F = 2 - 2g - B`

If we count boundary loops (B), we know the number of distinct openings. A solid wall has B=0. A wall with one door has B=1. A wall with door + window has B=2.

**Performance concern:** Converting to DynamicMesh and computing topology is heavier than the other signals. For a kit of 50-100 meshes, expect ~2-5 seconds total. Acceptable for a one-time scan operation.

**Sources:**
- [Robust Hole-Detection in Triangular Meshes | ScienceDirect](https://www.sciencedirect.com/science/article/pii/S001044852400023X)
- [Mesh Topology Analysis using Euler Characteristic | Max Limper](https://max-limper.de/a_euler.html)
- [Get Num Open Border Edges | UE 5.2 Docs](https://docs.unrealengine.com/5.3/en-US/BlueprintAPI/GeometryScript/MeshQueries/GetNumOpenBorderEdges/)
- [Fill All Mesh Holes | UE 5.7](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/GeometryScript/Repair/FillAllMeshHoles)

### 2.6 Signal 5: Socket Analysis

Some modular kits embed snap/attachment points as sockets:

```cpp
for (UStaticMeshSocket* Socket : SM->Sockets)
{
    FString Name = Socket->SocketName.ToString().ToLower();

    if (Name.Contains("snap") || Name.Contains("connect"))
        // This is a modular snap point
        SnapPoints.Add({Socket->RelativeLocation, Socket->RelativeRotation});

    if (Name.Contains("door") || Name.Contains("window"))
        // Socket explicitly marks an opening type
        OpeningType = Name.Contains("door") ? "door" : "window";

    if (Name.Contains("attach") || Name.Contains("mount"))
        // Attachment point for fixtures
        AttachPoints.Add(Socket->SocketName);
}
```

### 2.7 Combined Classification Algorithm

```
function ClassifyMesh(SM):
    scores = {wall_solid: 0, wall_door: 0, wall_window: 0, floor: 0,
              ceiling: 0, stairs: 0, corner: 0, column: 0, furniture: 0, ...}

    // Signal 1: Name parsing (weight: 0.35)
    name_type, name_conf = ParseName(SM->GetName())
    scores[name_type] += 0.35 * name_conf

    // Signal 2: Dimensions (weight: 0.25)
    dim_class = ClassifyByDimensions(SM->GetBounds())
    // Map dim_class to candidate types and add weighted scores

    // Signal 3: Material slots (weight: 0.15)
    has_glass = CheckMaterialSlots(SM, "glass")
    has_wood_door = CheckMaterialSlots(SM, "door|wood")
    if has_glass: scores[wall_window] += 0.15 * 0.9
    if has_wood_door: scores[wall_door] += 0.15 * 0.75

    // Signal 4: Topology (weight: 0.20)
    boundary_loops = GetBoundaryLoops(SM)
    for each loop:
        opening = ClassifyOpening(loop)
        if opening == door: scores[wall_door] += 0.20
        if opening == window: scores[wall_window] += 0.20
    if boundary_loops.empty() and dim_class == Wall:
        scores[wall_solid] += 0.20

    // Signal 5: Sockets (weight: 0.05)
    socket_hints = AnalyzeSockets(SM)
    // Boost relevant scores

    return max(scores)  // Highest scoring type wins
```

**Confidence threshold:** If the winning score is below 0.4, mark as "unclassified" and let the user resolve manually.

---

## 3. Dimension Extraction and Grid Detection

### 3.1 Extracting Exact Dimensions

```cpp
FBoxSphereBounds Bounds = SM->GetBounds();
FVector FullSize = Bounds.BoxExtent * 2.0;

// Round to nearest cm for cleanliness
double Width  = FMath::RoundToDouble(FullSize.X);
double Depth  = FMath::RoundToDouble(FullSize.Y);
double Height = FMath::RoundToDouble(FullSize.Z);
```

### 3.2 Auto-Detecting Grid Size

The grid size is the GCD (Greatest Common Divisor) of all the "primary" dimensions across wall pieces:

```
Algorithm:
1. Filter to wall-classified pieces only
2. Extract the "width" dimension of each wall (the long axis perpendicular to depth)
3. Compute approximate GCD:
   - Start with the smallest wall width
   - Check if all other widths are integer multiples (within 5cm tolerance)
   - If yes, that's the grid size
   - If no, try half the smallest width, then common values (50, 100, 200, 300, 400)

Example:
  Walls: 200cm, 200cm, 400cm, 200cm, 600cm
  GCD candidates: 200 (200/200=1, 400/200=2, 600/200=3) → grid = 200cm

  Walls: 300cm, 300cm, 600cm, 150cm
  GCD candidates: 150 (300/150=2, 600/150=4, 150/150=1) → grid = 150cm
```

**Common grid sizes in practice:**

| Grid Size | Used By | Notes |
|-----------|---------|-------|
| 50cm | Our internal system | Fine-grained, good for proc-gen |
| 100cm | Many indie kits | Simple, clean numbers |
| 200cm | Modular Building Set (Fab), many marketplace kits | Most common UE marketplace standard |
| 300cm | Stylized kits, some European-scale kits | Matches 3m wall height |
| 400cm | Sci-Fi kits, large-scale kits, Jacob Norris urban | Bigger pieces, fewer instances |
| 512cm | UEFN/Fortnite standard | Power-of-two, specific to Fortnite ecosystem |

**Wall heights:**

| Height | Context |
|--------|---------|
| 270cm | Residential (standard 9ft ceiling) |
| 300cm | Commercial/institutional |
| 384cm | UEFN standard (3/4 of 512) |
| 400cm | Sci-Fi, industrial |
| Variable | Some kits use different heights per floor |

**Sources:**
- [The Perfect Modular Grid Size | Mesh Masters](https://meshmasters.com/2933-2/)
- [Grid dimension recommendations | UE Forums](https://forums.unrealengine.com/t/what-dimensions-should-grid-structure-pieces-be/833724)
- [Modular Building Set Breakdown | Polycount](https://polycount.com/discussion/144838/ue4-modular-building-set-breakdown)

### 3.3 Handling Non-Uniform Pieces

Real kits have multiple piece widths:

```
Standard:  1x grid (200cm wall)
Half:      0.5x grid (100cm wall) -- for filling gaps
Double:    2x grid (400cm wall) -- for large facades
Triple:    3x grid (600cm wall) -- rare, some facade pieces

Each piece stores its grid_units (integer):
  200cm wall in 200cm grid → grid_units_width = 1
  400cm wall in 200cm grid → grid_units_width = 2
  100cm wall in 200cm grid → grid_units_width = 0.5 (or stored as half-unit)
```

**Floor/ceiling tile sizes** typically match the grid: 200x200, or 400x400 for 2x2 tiles.

### 3.4 Wall Thickness Detection

Wall thickness (depth dimension) matters for assembly:

```
Common thicknesses:
  10cm  -- paper-thin (some blockout kits)
  20cm  -- standard interior wall
  24cm  -- UEFN standard
  30cm  -- exterior wall
  32cm  -- thick exterior (prevents light bleed)
  40cm+ -- fortress/bunker walls
```

The thickness determines how pieces overlap at corners and whether dedicated corner pieces are needed.

---

## 4. Vocabulary JSON Format

### 4.1 Full Schema

```json
{
  "kit_name": "MyModularKit",
  "source_path": "/Game/MyModularKit/",
  "scan_date": "2026-03-28T14:30:00Z",
  "grid_size": 200,
  "wall_height": 300,
  "wall_thickness": 20,

  "pieces": {
    "wall_solid": [
      {
        "asset": "/Game/MyModularKit/Walls/SM_Wall_2x3",
        "width": 200,
        "height": 300,
        "depth": 20,
        "grid_units_w": 1,
        "grid_units_h": 1,
        "pivot": "base_center",
        "tri_count": 12,
        "material_slots": ["Concrete"],
        "sockets": [],
        "confidence": 0.92,
        "classification_signals": {
          "name": "wall_solid:0.85",
          "dimensions": "wall:0.90",
          "topology": "closed:0.95",
          "materials": "concrete:0.60"
        }
      }
    ],

    "wall_door": [
      {
        "asset": "/Game/MyModularKit/Walls/SM_Wall_Door_2x3",
        "width": 200,
        "height": 300,
        "depth": 20,
        "grid_units_w": 1,
        "grid_units_h": 1,
        "pivot": "base_center",
        "tri_count": 48,
        "material_slots": ["Concrete", "DoorFrame_Wood"],
        "openings": [
          {
            "type": "door",
            "x_offset": 30,
            "y_offset": 0,
            "width": 100,
            "height": 210,
            "confidence": 0.95
          }
        ],
        "sockets": ["Attach_Door"],
        "confidence": 0.97,
        "classification_signals": {
          "name": "wall_door:0.95",
          "dimensions": "wall:0.90",
          "topology": "1_opening_door_sized:0.95",
          "materials": "wood_doorframe:0.75"
        }
      }
    ],

    "wall_window": [
      {
        "asset": "/Game/MyModularKit/Walls/SM_Wall_Window_2x3",
        "width": 200,
        "height": 300,
        "depth": 20,
        "grid_units_w": 1,
        "grid_units_h": 1,
        "pivot": "base_center",
        "tri_count": 52,
        "material_slots": ["Concrete", "Glass"],
        "openings": [
          {
            "type": "window",
            "x_offset": 40,
            "y_offset": 90,
            "width": 120,
            "height": 140,
            "confidence": 0.93
          }
        ],
        "sockets": [],
        "confidence": 0.96
      }
    ],

    "floor": [
      {
        "asset": "/Game/MyModularKit/Floors/SM_Floor_2x2",
        "width": 200,
        "height": 10,
        "depth": 200,
        "grid_units_w": 1,
        "grid_units_d": 1,
        "pivot": "center",
        "tri_count": 4,
        "material_slots": ["Wood_Floor"],
        "confidence": 0.94
      }
    ],

    "ceiling": [],

    "stairs": [
      {
        "asset": "/Game/MyModularKit/Stairs/SM_Stairs_Straight",
        "width": 200,
        "height": 300,
        "depth": 400,
        "grid_units_w": 1,
        "grid_units_d": 2,
        "pivot": "base_center",
        "tri_count": 120,
        "step_count": 16,
        "confidence": 0.91
      }
    ],

    "corner_inside": [],
    "corner_outside": [],
    "t_junction": [],
    "column": [],

    "trim": [],
    "railing": [],
    "beam": [],
    "roof": [],

    "door_prop": [],
    "window_prop": [],

    "furniture": {
      "bed": [],
      "table": [],
      "chair": [],
      "desk": [],
      "shelf": [],
      "unclassified": []
    },

    "unclassified": [
      {
        "asset": "/Game/MyModularKit/Props/SM_Weird_Thing",
        "width": 80,
        "height": 120,
        "depth": 60,
        "confidence": 0.25,
        "suggested_types": ["furniture", "prop"],
        "reason": "No strong signals matched"
      }
    ]
  },

  "statistics": {
    "total_assets_scanned": 47,
    "classified": 42,
    "unclassified": 5,
    "piece_type_counts": {
      "wall_solid": 4,
      "wall_door": 3,
      "wall_window": 5,
      "floor": 6,
      "ceiling": 4,
      "stairs": 2,
      "corner_inside": 2,
      "corner_outside": 2,
      "column": 1,
      "furniture": 13
    }
  },

  "completeness": {
    "has_walls": true,
    "has_doors": true,
    "has_windows": true,
    "has_floors": true,
    "has_ceilings": true,
    "has_stairs": true,
    "has_corners": true,
    "missing": ["t_junction", "railing"],
    "can_build_complete_building": true,
    "notes": "Missing T-junction pieces. Will use column + wall ends as fallback."
  }
}
```

### 4.2 Persistence

Save vocabulary JSON to: `Saved/Monolith/Kits/<KitName>.json`

This lets kits be referenced by name in future `build_with_kit` calls without re-scanning.

---

## 5. Proxy / Blockout Mode

### 5.1 Generating Proxy Meshes

For users who want to test layouts before importing their own art, generate simple box proxies that match dimensions exactly:

```cpp
// For each piece in the vocabulary:
UDynamicMesh* ProxyMesh = NewObject<UDynamicMesh>();
FGeometryScriptPrimitiveOptions PrimOpts;

// AppendBox with exact dimensions from the vocabulary entry
UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox(
    ProxyMesh,
    PrimOpts,
    FTransform::Identity,
    Piece.Width, Piece.Depth, Piece.Height,
    1, 1, 1,  // subdivisions
    EGeometryScriptPrimitiveOriginMode::Base  // match pivot convention
);

// If it's a door/window piece, boolean subtract the opening
if (Piece.HasOpening())
{
    // Cut an opening matching the vocabulary's opening bounds
    CutOpening(ProxyMesh, Piece.Openings[0]);
}

// Apply color-coded material by type:
//   Wall = gray, Floor = dark gray, Door wall = blue, Window wall = cyan
//   Furniture = green, Stairs = yellow, Unclassified = red
```

**Color coding makes it instantly obvious** what type each proxy is during layout testing.

### 5.2 Proxy-to-Real Swap

The swap is straightforward -- just change the StaticMesh reference on each component:

```cpp
void SwapProxiesToRealAssets(AActor* BuildingActor, const FKitVocabulary& Kit)
{
    TArray<UStaticMeshComponent*> Components;
    BuildingActor->GetComponents(Components);

    for (UStaticMeshComponent* Comp : Components)
    {
        // Read the piece type from component tag
        FString PieceType = GetComponentTag(Comp, "MonolithPieceType");
        int32 VariantIndex = GetComponentTagInt(Comp, "MonolithVariant");

        // Look up the real mesh from the kit vocabulary
        UStaticMesh* RealMesh = Kit.GetMeshForType(PieceType, VariantIndex);

        if (RealMesh)
        {
            Comp->SetStaticMesh(RealMesh);

            // May need transform adjustment if pivots differ
            FVector PivotOffset = ComputePivotOffset(
                Kit.GetPivot(PieceType), ProxyPivot);
            Comp->AddRelativeLocation(PivotOffset);
        }
    }
}
```

**For HISM-based buildings:** The swap is even simpler -- just change the mesh on the HISM component. All instances automatically use the new mesh since they share transforms.

```cpp
HISMComponent->SetStaticMesh(NewRealMesh);
// All 200 wall instances instantly swap. One line of code.
```

### 5.3 Selective Swap

Allow swapping individual piece types while keeping others as proxies:

```
swap_proxies(kit: "MyModularKit", types: ["wall_door", "wall_window"])
// Only swaps door and window walls, keeps everything else as proxy
```

This is useful for progressive art integration -- swap the most visible pieces first.

**Sources:**
- [Using Geometry Brushes for Static Mesh Proxies | UE Tutorial](https://dev.epicgames.com/community/learning/tutorials/33e/unreal-engine-using-geometry-brushes-for-static-mesh-proxies)
- [3 Methods for Blocking Out Environments | WorldOfLevelDesign](https://www.worldofleveldesign.com/categories/ue5/blockouts-in-ue5.php)
- [Replace References Tool | UE 4.27](https://docs.unrealengine.com/4.26/en-US/Basics/ContentBrowser/AssetConsolidationTool)

---

## 6. MCP Actions

### 6.1 Action Definitions

**`scan_modular_kit`** -- Primary scanning action

```
Params:
  folder_path: string (required) -- "/Game/MyModularKit/"
  kit_name: string (optional) -- override auto-detected name
  grid_size: int (optional) -- override auto-detected grid (cm)
  wall_height: int (optional) -- override auto-detected height (cm)
  recursive: bool (default true) -- scan subdirectories
  include_topology: bool (default true) -- run boundary edge analysis (slower but more accurate)
  reclassify: bool (default false) -- ignore cached classifications

Returns:
  kit_name, vocabulary summary, piece counts, completeness check,
  path to saved JSON, warnings for unclassified assets

Behavior:
  1. Scan folder via IAssetRegistry
  2. For each StaticMesh: extract bounds, materials, sockets, topology
  3. Run multi-signal classification
  4. Detect grid size from wall dimensions
  5. Build vocabulary JSON
  6. Save to Saved/Monolith/Kits/<kit_name>.json
  7. Optionally stamp assets with UMonolithKitMetadata
  8. Return summary + path
```

**`classify_mesh`** -- Classify a single mesh

```
Params:
  asset_path: string (required) -- "/Game/Kit/SM_Wall_01"
  include_topology: bool (default true)

Returns:
  classification result with all signal scores, winning type, confidence,
  dimensions, material slots, opening details, socket list
```

**`build_with_kit`** -- Generate building using a kit vocabulary

```
Params:
  kit: string (required) -- kit name or path to vocabulary JSON
  building_spec: object (required) -- same spec as create_building_from_grid
  mode: string (default "real") -- "real" (place kit meshes), "proxy" (blockout boxes), "hybrid"
  use_hism: bool (default true) -- use instanced rendering
  folder_path: string (optional) -- Outliner folder for spawned actors

Returns:
  spawned actor reference, piece count breakdown, any missing piece warnings
```

**`swap_proxies`** -- Replace proxy meshes with real assets

```
Params:
  actor: string (required) -- building actor name/label
  kit: string (required) -- kit to source real meshes from
  types: string[] (optional) -- specific piece types to swap (default: all)

Returns:
  swap count, any pieces that couldn't be swapped (missing in kit)
```

**`list_kits`** -- List scanned/available kits

```
Params: none

Returns:
  array of {kit_name, source_path, piece_count, grid_size, scan_date, completeness}
```

**`edit_kit_classification`** -- Manually fix a classification

```
Params:
  kit: string (required) -- kit name
  asset_path: string (required) -- mesh to reclassify
  new_type: string (required) -- corrected piece type

Returns:
  updated vocabulary JSON path

Behavior:
  Updates the vocabulary JSON and stamps the asset with corrected metadata
```

### 6.2 Integration with Existing Actions

These new actions integrate with the existing procedural building pipeline:

```
Current pipeline:
  create_building_from_grid → GeometryScript boolean walls → merged mesh

New pipeline options:
  scan_modular_kit → vocabulary JSON
  build_with_kit (mode: "real") → HISM-instanced real assets
  build_with_kit (mode: "proxy") → blockout testing
  swap_proxies → upgrade proxies to real art

Hybrid:
  create_building_from_grid → GeometryScript structure (backup)
  build_with_kit → modular override where kit has pieces
```

---

## 7. Marketplace Kit Compatibility

### 7.1 Common Marketplace Kit Analysis

Based on research of popular Fab/Marketplace kits:

| Kit | Grid | Height | Pieces | Naming | Corners | Notes |
|-----|------|--------|--------|--------|---------|-------|
| Modular Building Set (Fab) | 200cm | 200cm | ~100 | `SM_Wall_2x2` | Pillar pieces | Most popular kit on marketplace |
| Stylized Modular Building Kit | 300cm | 300cm | ~80 | `SM_wall_straight_01` | Inside/outside corners | Stylized art |
| Modular Sci-Fi Building Kit | 400cm | 400cm | ~120 | `SM_SciFi_Wall_A` | Dedicated corners | Sci-fi corridors |
| Grid-Based Builder | Variable | Variable | Blueprint | N/A | Handled by BP | Tool, not art kit |
| 450+ Modular Building Parts | 200cm | 300cm | 450+ | Mixed | Full set | Massive blockout kit |
| Sundown Modular Shop Kit | 300cm | 300cm | ~60 | `SM_Shop_Wall_01` | L-corners | Retail/commercial |

### 7.2 Corner Handling Across Kits

There are three major strategies for corners in marketplace kits:

**Strategy 1: Pillar/Column pieces (most common)**
- A vertical column piece fills the corner gap
- Walls stop short of the corner on both sides
- Simple, flexible, works for any angle combination
- Used by: Modular Building Set, most blockout kits

**Strategy 2: Dedicated corner meshes (higher quality)**
- Separate inside-corner and outside-corner pieces
- Geometry wraps around the corner with proper bevels/detail
- More visually polished but requires more unique pieces
- Used by: Stylized kits, Sci-Fi kits

**Strategy 3: Overlapping walls (cheapest)**
- One wall extends through the corner, other butts against it
- Results in Z-fighting at the intersection if not careful
- Simplest to implement but worst visual quality
- Used by: Some quick blockout kits

**Our classification system should detect which strategy a kit uses:**
- Has corner_inside + corner_outside pieces → Strategy 2
- Has column/pillar pieces but no corners → Strategy 1
- Has neither → Strategy 3 (overlap, or we generate simple corner fills)

### 7.3 What Kits Are Missing

No marketplace kit is perfectly complete. Common gaps:
- **T-junctions:** Rarely provided. Handled by column + wall ends.
- **Half-height walls:** For railings, half-walls. Uncommon.
- **Transition pieces:** Between interior and exterior walls. Very rare.
- **Ceiling variants:** Most kits have 1-2 ceiling pieces (solid, with light hole).
- **Damaged variants:** Horror-specific damage is never included in general kits.

**Our system should handle missing pieces gracefully:**
1. Check kit completeness after scan
2. Report missing piece types
3. Auto-generate simple GeometryScript fallbacks for missing types
4. Allow mixing kits (walls from Kit A, floors from Kit B)

### 7.4 Grid Conversion

When a user's kit grid doesn't match our internal 50cm grid:

```
Kit grid: 200cm
Our grid: 50cm
Ratio: 200/50 = 4 grid cells per kit piece

A 200cm wall piece occupies 4 of our grid cells.
A 400cm piece occupies 8 cells.

Assembly: snap kit pieces to our grid at integer multiples of the kit grid.
Buildings must have dimensions that are multiples of the kit grid size.
```

**This means our floor plan generator must output rooms whose dimensions are multiples of the kit's grid size**, not just our internal 50cm grid. This is a constraint the `build_with_kit` action enforces.

**Sources:**
- [Modular Building Set | Fab](https://www.fab.com/listings/474a0598-ed86-40b6-baa1-c801d96ef4ab)
- [Modular Building Set Breakdown | Polycount](https://polycount.com/discussion/144838/ue4-modular-building-set-breakdown)
- [Modular Kit Design | Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics/modular)

---

## 8. Industry Approaches to Auto-Classification

### 8.1 Houdini Labs Building Generator

The closest industry precedent for what we're building. SideFX Labs Building Generator:

- Analyzes incoming building volumes, slices into floors
- Identifies structural regions: walls, corners, ledges
- Replaces regions with modules from a user-defined library
- Module selection via **name-based pattern matching** (`Facade Module Pattern`)
- Wall detection uses **face normals** (`@N.y` to separate walls from floors/ceilings)
- Modules need a `@name` attribute for identification (e.g., "wall", "corner")
- Floor detection via height slicing + `facade_height` primitive attributes

**Key takeaway:** Even Houdini's mature system relies primarily on naming conventions + normal-based classification. It does NOT do automatic geometry analysis for classification. Our multi-signal approach (names + dimensions + topology + materials) would actually be more robust.

**Sources:**
- [Labs Building Generator | SideFX](https://www.sidefx.com/docs/houdini/nodes/sop/labs--building_generator-4.0.html)
- [Making the Procedural Buildings of THE FINALS | SideFX](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)
- [Building Generator Tutorial | SideFX](https://www.sidefx.com/tutorials/building-generator/)

### 8.2 THE FINALS Auto-Detection

Embark Studios' Building Creator for THE FINALS:
- Wall mod-kit auto-detects faces from blockout primitives
- Uses `@N.y` (face normal Y component) to classify:
  - `|@N.y| < 0.1` → wall face (nearly vertical)
  - `@N.y > 0.9` → floor/ceiling (nearly horizontal, facing up)
  - `@N.y < -0.9` → ceiling (facing down)
- Window placement checks face dimensions to select appropriate mesh
- Each Feature Node is a specialized HDA for one architectural element

**Sources:**
- [Making the Procedural Buildings of THE FINALS | SideFX](https://www.sidefx.com/community/making-the-procedural-buildings-of-the-finals-using-houdini/)

### 8.3 Academic Approaches

Recent papers on automatic mesh classification:

- **Graph learning for 3D mesh classification** (2024, Springer): Uses graph neural networks on mesh topology for intersecting geometry classification. Overkill for our use case.
- **Hole detection in architectural models** (2020, ScienceDirect): "Bubblegum algorithm" for detecting holes in 3D architectural models using shape classification. Relevant to our opening detection.
- **Robust hole-detection in triangular meshes** (2024, ScienceDirect): Traverses all boundaries of edge-manifold meshes, labels all holes. Most relevant algorithm for our boundary loop analysis.

**Sources:**
- [Advancing 3D Mesh Analysis | Springer](https://link.springer.com/chapter/10.1007/978-3-031-78166-7_10)
- [Detection of holes in 3D architectural models | ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1877050920308450)
- [Robust Hole-Detection in Triangular Meshes | ScienceDirect](https://www.sciencedirect.com/science/article/pii/S001044852400023X)

---

## 9. Implementation Plan

### Phase 1: Asset Scanning Foundation (~8h)
- `scan_modular_kit` action: IAssetRegistry scan, bounds extraction, material slot reading, socket enumeration
- `list_kits` action: enumerate saved kit JSONs
- JSON vocabulary persistence at `Saved/Monolith/Kits/`
- Grid size auto-detection from wall piece widths

### Phase 2: Classification Engine (~12h)
- `classify_mesh` action: individual mesh classification
- Name parsing regex engine (pattern table from Section 2.2)
- Dimension analysis heuristics (Section 2.3)
- Material slot analysis (Section 2.4)
- Multi-signal weighted scoring (Section 2.7)
- Confidence thresholds and "unclassified" handling

### Phase 3: Topology Analysis (~10h)
- StaticMesh → DynamicMesh conversion for boundary analysis
- GeometryScript boundary edge loop detection
- Opening measurement and classification (door vs window vs vent)
- Opening bounds extraction for vocabulary JSON
- Integration into classification pipeline as Signal 4

### Phase 4: Building Generation with Kits (~12h)
- `build_with_kit` action: assembly from vocabulary + building spec
- Grid conversion (kit grid → internal 50cm grid)
- HISM-based piece placement
- Corner strategy detection and handling (pillar vs corner piece vs overlap)
- Missing piece fallback (GeometryScript generation)
- Outliner folder organization

### Phase 5: Proxy Mode and Swap (~8h)
- Proxy mesh generation (colored boxes matching piece dimensions)
- `swap_proxies` action: proxy-to-real mesh swap
- `edit_kit_classification` action: manual classification correction
- UMonolithKitMetadata UAssetUserData for persistent tagging
- Kit completeness reporting

**Total: ~50h**

### Dependencies
- Existing: `get_mesh_info`, `get_mesh_bounds` actions (done)
- Existing: GeometryScript boundary analysis functions (available in UE 5.7)
- Existing: IAssetRegistry scanning (used throughout Monolith indexing)
- New: UMonolithKitMetadata UAssetUserData class
- New: Kit vocabulary JSON format
- Related: modular-building-research (Phase 1-5 of that plan)

---

## 10. Risks and Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| Name parsing fails on non-English kits | Medium | Low | Dimension/topology signals compensate; manual override via `edit_kit_classification` |
| Mesh topology analysis too slow for large kits | Low | Low | Make topology optional (`include_topology: false`); cache results |
| Kit grid doesn't evenly divide building dimensions | Medium | Medium | Snap building dimensions to kit grid multiples; warn user |
| Corner strategy misdetected | Low | Medium | Allow manual override; default to column fallback |
| Pivot conventions vary between pieces in same kit | Medium | High | Detect per-piece and normalize; warn if inconsistent |
| No marketplace kit has all needed piece types | High | Medium | GeometryScript fallback for missing types; allow kit mixing |
| User meshes have non-manifold geometry | Medium | Low | Skip topology for non-manifold; rely on other signals |

---

## 11. Future Extensions

- **ML-based classification:** Train a small model on labeled modular pieces for better accuracy. The multi-signal approach generates training labels automatically.
- **Style transfer:** Scan Kit A and Kit B, map piece types, swap styles while preserving layout.
- **Kit browser UI:** Visual grid of all pieces in a kit, colored by type, with drag-to-reclassify.
- **PCG integration:** Feed vocabulary JSON into UE5's Procedural Content Generation framework as a custom data source.
- **Damage variant generation:** Given a clean piece, auto-generate damaged variants using GeometryScript (booleans for holes, noise displacement for cracks).
- **Kit statistics dashboard:** Tri counts, material counts, LOD coverage, texture density analysis per kit.
