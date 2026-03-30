# Daredevil Mode: Procedural Building Debug View Research

**Date:** 2026-03-28
**Status:** Research Complete
**Scope:** Techniques for inspecting procedural building interiors via top-down/isometric section views

---

## Summary

Eight approaches investigated for a "Sims-like" building inspection view that hides or clips roofs/ceilings to reveal room interiors. The recommended architecture is a **layered system**: a Material Parameter Collection (MPC) drives a height-based clip in materials (Approach 2), combined with actor visibility toggling (Approach 3) for non-MPC geometry, and an orthographic SceneCapture (Approach 6) for minimap/overview output. All are feasible via Monolith actions with ~40-55h total implementation.

---

## Approach 1: Cutting Planes (Global Clip Plane)

### How It Works
UE has a global clip plane (`r.AllowGlobalClipPlane=1`) originally for planar reflections. When enabled, `USceneCaptureComponent2D` exposes `bEnableClipPlane`, `ClipPlaneBase`, and `ClipPlaneNormal` to clip everything above/below a world-space plane.

### UE 5.7 API (Verified via source_query)
```cpp
// SceneCaptureComponent2D.h lines 126-139
UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
bool bEnableClipPlane;

UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
FVector ClipPlaneBase;

UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category=SceneCapture)
FVector ClipPlaneNormal;
```

### Pros
- Built-in, zero custom shaders needed for SceneCapture output
- Works with any geometry, not just materials you control
- Perfect for capturing a clipped top-down image to render target

### Cons
- **15% BasePass cost** when `r.AllowGlobalClipPlane=1` -- affects ALL rendering, not just the debug view
- Only works for `USceneCaptureComponent2D`, NOT the main viewport
- Known engine stability issues (some users report editor hangs at 45% init)
- Cannot clip the main viewport directly -- only scene captures

### Verdict
**Use for SceneCapture minimap only.** Do NOT enable globally for the main viewport. The 15% perf hit is unacceptable as a permanent setting. Instead, toggle it on only when capturing overview images.

---

## Approach 2: Material-Based Section View (MPC + World Position) -- RECOMMENDED PRIMARY

### How It Works
A Material Parameter Collection (MPC) stores a `SectionHeight` scalar and `SectionEnabled` toggle. All ceiling/roof materials include a shared Material Function that reads the MPC, compares `AbsoluteWorldPosition.Z` against `SectionHeight`, and discards pixels above the threshold (via Opacity Mask or by zeroing opacity).

### Architecture
```
MPC "MPC_SectionView":
  - SectionEnabled (scalar, 0 or 1)
  - SectionHeight (scalar, world Z in cm)
  - SectionColor (vector, highlight tint for cut plane edge)

Material Function "MF_SectionClip":
  - Read MPC values
  - If SectionEnabled > 0.5:
    - PixelZ = AbsoluteWorldPosition.Z
    - If PixelZ > SectionHeight: OpacityMask = 0 (clip)
    - Optional: If PixelZ in range [SectionHeight-5, SectionHeight]: emit SectionColor (cut edge highlight)
```

### UE 5.7 API (Verified)
```cpp
// MaterialParameterCollectionInstance.h line 39
ENGINE_API bool SetScalarParameterValue(FName ParameterName, float ParameterValue);

// Usage in C++:
UMaterialParameterCollectionInstance* MPCInst = World->GetParameterCollectionInstance(MPC);
MPCInst->SetScalarParameterValue(FName("SectionHeight"), 150.0f);
MPCInst->SetScalarParameterValue(FName("SectionEnabled"), 1.0f);
```

### Implementation Pattern (from zoltane.com cutaway views)
Materials receive clipping plane data from an MPC. A "ClippingManager" sets PlaneLocation and PlaneNormal. In each material, pixel distance from the plane is calculated; pixels behind it are hidden. Backface normals can be set to match the clip plane, creating the illusion of a solid cross-section surface.

### Pros
- **Zero performance cost when disabled** (MPC check is one scalar compare)
- Works in the main viewport -- no SceneCapture needed
- Animatable: smoothly raise/lower the section height
- Can add visual flair: colored cut edges, hatching, X-ray effect
- MPC updates are instant and global (all materials using the MF update simultaneously)

### Cons
- Requires all ceiling/roof materials to include the MF_SectionClip function
- Procedurally generated materials from Monolith need this injected at creation time
- Won't clip meshes using materials you don't control (marketplace assets, etc.)
- Opaque materials need to switch blend mode to Masked (slight shader cost)

### Monolith Integration
```
mesh_query("toggle_section_view", {
  "enabled": true,
  "height": 150,          // cm above floor
  "color": [1, 0.8, 0]   // optional cut-edge highlight color
})
```
The action would:
1. Find or create MPC asset `MPC_SectionView`
2. Get the world's MPC instance
3. Call `SetScalarParameterValue` for height and enabled flag
4. Optionally set vector param for edge color

### Verdict
**Primary recommendation.** Best balance of quality, performance, and flexibility. Pairs naturally with Monolith's proc gen since we control the materials.

---

## Approach 3: Actor Visibility Toggling -- RECOMMENDED SECONDARY

### How It Works
All procedurally generated ceiling/roof actors (or components) are tagged during creation. A debug action iterates tagged actors and calls `SetActorHiddenInGame(true)` or `SetVisibility(false)` on components.

### UE 5.7 API (Verified)
```cpp
// AActor
void SetActorHiddenInGame(bool bNewHidden);

// USceneComponent
void SetVisibility(bool bNewVisibility, bool bPropagateToChildren = false);

// Existing Monolith tagging:
Actor->Tags.Add(FName("Monolith.Ceiling"));
Actor->Tags.Add(FName("Monolith.Roof"));
```

### Existing Monolith Infrastructure
Monolith already has `set_actor_tags` (registered in MonolithMeshBlockoutActions.cpp line 482) and tag-based queries in accessibility/horror actions. The procedural actions (`create_structure`, `create_building`) already generate ceilings with `has_ceiling` parameter.

**Key insight:** The `create_structure` action (line 1711) already has `bCeiling` toggling. We can tag ceiling geometry during creation with `Monolith.Ceiling` and then toggle visibility on all tagged actors.

### Pros
- Simplest implementation (~4h)
- Works with ANY geometry regardless of material
- No shader changes needed
- Instant toggle, zero overhead when not active
- Already has tag infrastructure in place

### Cons
- Binary on/off -- no smooth animation or partial clipping
- Doesn't show a cross-section surface (just removes geometry)
- Requires consistent tagging discipline during proc gen
- Won't work for non-Monolith actors unless manually tagged

### Monolith Integration
```
mesh_query("toggle_ceiling_visibility", {
  "visible": false,
  "tags": ["Monolith.Ceiling", "Monolith.Roof"]  // optional filter
})
```

### Verdict
**Use as secondary/fallback.** Quick to implement, handles edge cases where materials can't be modified. Tag ceilings during proc gen, toggle visibility on demand.

---

## Approach 4: Section Box Volume

### How It Works
A box volume defines a 3D region; anything outside is clipped. Like Revit's section box. Implemented via either:
- A post-process material that reads custom depth/stencil to mask regions
- Per-material world-position bounds checking (extension of Approach 2)
- SceneCapture with `ShowOnlyActors`/`HiddenActors` lists

### UE 5.7 API
```cpp
// USceneCaptureComponent2D -- actor filtering
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
TArray<TWeakObjectPtr<AActor>> HiddenActors;

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
TArray<AActor*> ShowOnlyActors;

// Custom Depth Stencil per component
UPROPERTY(EditAnywhere, AdvancedDisplay)
uint8 bRenderCustomDepth : 1;

UPROPERTY(EditAnywhere, AdvancedDisplay)
int32 CustomDepthStencilValue; // 0-255
```

### Implementation
Extend the MPC approach with 6 values (XMin, XMax, YMin, YMax, ZMin, ZMax) instead of just a height threshold. Materials check `AbsoluteWorldPosition` against all 6 bounds.

### Pros
- Most flexible -- clip to any box region
- Great for isolating a single room or floor
- Natural for archviz workflows

### Cons
- More complex MPC setup (6 params vs 1)
- Higher per-pixel cost (6 comparisons vs 1)
- Less intuitive for users than simple height slider

### Verdict
**V2 feature.** Start with height-only clip, expand to full section box later.

---

## Approach 5: Orthographic Camera -- RECOMMENDED FOR OVERVIEW

### How It Works
Switch the viewport or spawn a camera with orthographic projection, positioned directly above the building looking down. Combined with ceiling hiding, this gives a perfect floor-plan view.

### UE 5.7 API (Verified)
```cpp
// SceneCaptureComponent2D.h lines 39-72 (verified via source_query)
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection)
TEnumAsByte<ECameraProjectionMode::Type> ProjectionType; // Set to Orthographic

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection)
float OrthoWidth; // World units visible horizontally

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Projection)
bool bAutoCalculateOrthoPlanes; // Auto near/far

// Orthographic tiling for large scenes (lines 115-124)
bool bEnableOrthographicTiling; // Render in NxM tiles
int32 NumXTiles;
int32 NumYTiles;
```

### Editor Viewport
The editor viewport supports orthographic via the viewport menu (Top/Front/Side views). Programmatically, `FEditorViewportClient` has `SetViewportType()` and `SetViewMode()` members, but these are editor-only and not easily controllable from Monolith's runtime API.

### For Monolith: Use SceneCapture Instead
Rather than hijacking the editor viewport, spawn a transient `ASceneCapture2D` actor:
1. Position above building center at sufficient height
2. Set `ProjectionType = ECameraProjectionMode::Orthographic`
3. Set `OrthoWidth` to cover building extents
4. Point straight down (rotation = -90 pitch)
5. Set `HiddenActors` to include ceiling actors (from Approach 3 tags)
6. Capture to render target, save as PNG

### Pros
- True architectural floor plan view
- Combines perfectly with ceiling hiding
- OrthoWidth auto-scales to building size
- Tiling support for very large scenes

### Cons
- SceneCapture has overhead (full scene render)
- Static image, not interactive
- Editor viewport ortho toggle is not easily automated via MCP

### Verdict
**Use for overview capture.** Combine with Approach 3 (hide ceilings) for clean floor plan output.

---

## Approach 6: Scene Capture to Render Target -- RECOMMENDED FOR MINIMAP

### How It Works
A persistent or on-demand `USceneCaptureComponent2D` renders a top-down view of the building to a `UTextureRenderTarget2D`. This texture can be displayed in a UMG widget (minimap), saved as PNG, or used in materials.

### Existing Monolith Pattern
Monolith already has scene capture infrastructure in:
- `MonolithEditorActions.cpp`: `HandleCaptureScenePreview` (lines 362-373)
- `MonolithMeshLightingCapture.cpp`: `CreateLightingCapture` helper (lines 38-80)

Both create transient `USceneCaptureComponent2D` + `UTextureRenderTarget2D`, configure show flags, capture, read pixels, and save to PNG. The pattern is proven and reusable.

### Implementation (Extend Existing Pattern)
```cpp
// Pseudocode for capture_floor_plan action
USceneCaptureComponent2D* Capture = CreateTransientCapture(World, RT);
Capture->ProjectionType = ECameraProjectionMode::Orthographic;
Capture->OrthoWidth = BuildingExtents.X; // auto-fit to building
Capture->SetWorldRotation(FRotator(-90, 0, 0)); // look straight down
Capture->SetWorldLocation(BuildingCenter + FVector(0, 0, 5000));

// Hide ceiling actors
for (AActor* CeilingActor : CeilingActors)
    Capture->HiddenActors.Add(CeilingActor);

// Show flags: unlit for clean floor plan
Capture->ShowFlags.SetLighting(false);
Capture->ShowFlags.SetPostProcessing(false);

Capture->TextureTarget = RT;
Capture->CaptureScene();
// Read back and save PNG (reuse existing RenderAndSaveCapture helper)
```

### Pros
- Proven pattern already in codebase
- Full control over what's visible (HiddenActors, ShowOnlyActors)
- Output usable as minimap texture, documentation image, or debug overlay
- Can combine with clip plane for SceneCapture-only clipping

### Cons
- One-shot capture, not real-time interactive
- Full scene render cost per capture
- Doesn't help with interactive viewport inspection

### Verdict
**Implement as `mesh_query("capture_floor_plan", {...})`.** Low effort given existing helpers.

---

## Approach 7: UE Viewport Modes (Wireframe, Unlit, etc.)

### How It Works
Toggle the editor viewport's rendering mode to wireframe, unlit, or other debug visualizations.

### UE 5.7 View Modes (Verified via source_query)
```cpp
// EngineBaseTypes.h lines 961-1066
enum EViewModeIndex : int {
    VMI_BrushWireframe = 0,    // Wireframe w/ brushes
    VMI_Wireframe = 1,         // CSG Wireframe
    VMI_Unlit = 2,             // Unlit
    VMI_Lit = 3,               // Lit (default)
    VMI_Lit_DetailLighting = 4,// Detail Lighting
    VMI_LightingOnly = 5,      // Lighting Only
    VMI_LightComplexity = 6,   // Light Complexity
    VMI_ShaderComplexity = 8,  // Shader Complexity
    VMI_CollisionPawn = 15,    // Player Collision
    VMI_CollisionVisibility = 16, // Visibility Collision
    VMI_LODColoration = 18,    // Mesh LOD Coloration
    // ... 35+ modes total
};
```

### Monolith Integration
For SceneCapture, viewport mode is controlled via `ShowFlags`:
```cpp
Capture->ShowFlags.SetLighting(false);    // = VMI_Unlit
Capture->ShowFlags.SetWireframe(true);    // = VMI_Wireframe
```

For the editor viewport itself, there's no clean MCP-accessible API. The `FEditorViewportClient` is editor-internal. However, **console commands** work:
```
viewmode wireframe
viewmode unlit
viewmode lit
```

### Pros
- Wireframe mode shows all room outlines clearly
- Unlit mode removes shadow confusion
- Already built into the engine

### Cons
- Affects the ENTIRE viewport, not selective
- No partial transparency or section cutting
- Editor viewport commands not easily automated via Monolith MCP
- Console commands are brittle across engine versions

### Verdict
**Useful supplementary feature** for the SceneCapture path. Set `ShowFlags` on the capture component for clean floor plan renders. Not practical as primary debug view.

---

## Approach 8: LOD-Based Floor Plan Outlines

### How It Works
At a far distance (or explicitly triggered), replace building meshes with simplified floor-plan outline geometry. Essentially an "overview LOD" that shows only walls as 2D lines.

### Implementation
Generate a secondary static mesh during proc gen that contains only wall outlines (2D polylines extruded to thin quads). Store as LOD1 or as a separate tagged actor. Toggle between detail and overview modes.

### Pros
- Very lightweight rendering
- Clean architectural aesthetic
- Could work as a persistent minimap representation

### Cons
- Requires generating additional geometry during proc gen
- Significant implementation effort for marginal benefit over other approaches
- Doesn't show interior details (furniture, items, etc.)

### Verdict
**Defer.** Nice-to-have but low priority compared to approaches 2, 3, and 6.

---

## Room Highlighting (Flash a Room Yellow)

### Approach A: MPC-Based Highlight
Extend the section view MPC with per-room highlight:
```
MPC "MPC_RoomHighlight":
  - HighlightCenter (vector, world XY of room center)
  - HighlightExtents (vector, half-size of room bounds)
  - HighlightColor (vector, e.g. yellow)
  - HighlightIntensity (scalar, 0-1, animate for pulse/flash)
```

Materials check if `AbsoluteWorldPosition.XY` falls within the room bounds and add `HighlightColor * HighlightIntensity` to emissive.

### Approach B: Overlay Material
UE 5.7 supports `OverlayMaterial` on `UMeshComponent`. Apply a translucent colored overlay to all components within a room's bounds:
```cpp
Component->SetOverlayMaterial(HighlightMI);
```
Use a simple unlit translucent material with animated opacity (sine wave for pulsing).

### Approach C: Post-Process + Custom Stencil
Assign all room floor/wall actors a `CustomDepthStencilValue` (e.g., room 1 = stencil 1, room 2 = stencil 2). A post-process material reads the stencil buffer and applies a color overlay for the target room.

### Recommended: Approach B (Overlay Material)
- Simplest to implement
- No material modification needed on existing assets
- Per-component granularity
- Easy to animate via `SetScalarParameterValueOnMaterials`

### Monolith Integration
```
mesh_query("highlight_room", {
  "room_tag": "Kitchen",        // or room center + extents
  "color": [1, 0.9, 0, 0.3],   // RGBA yellow
  "pulse": true,                 // animate intensity
  "duration": 3.0               // seconds, 0 = permanent until cleared
})

mesh_query("clear_highlights", {})
```

---

## Camera Bookmarks

### UE Editor Bookmarks
The editor supports Ctrl+0 through Ctrl+9 to save viewport camera positions, and 0-9 to recall them. These are stored per-level in `FBookMark` structs in `ULevel::BookMarks`.

### Monolith Implementation
Since editor bookmark API is internal to `FEditorViewportClient`, implement our own bookmark system:

```
mesh_query("save_camera_bookmark", {
  "name": "kitchen_overview",
  "location": [500, 300, 800],
  "rotation": [-90, 0, 0],
  "projection": "orthographic",  // or "perspective"
  "ortho_width": 2000,
  "fov": 60                      // for perspective
})

mesh_query("load_camera_bookmark", {
  "name": "kitchen_overview"
})

mesh_query("list_camera_bookmarks", {})
```

### Storage
Save as JSON in `Saved/Monolith/CameraBookmarks.json`:
```json
{
  "bookmarks": {
    "kitchen_overview": {
      "location": [500, 300, 800],
      "rotation": [-90, 0, 0],
      "projection": "orthographic",
      "ortho_width": 2000
    },
    "hallway_section": {
      "location": [200, 0, 150],
      "rotation": [0, -45, 0],
      "projection": "perspective",
      "fov": 60,
      "section_height": 150
    }
  }
}
```

Bookmarks can store section view state (height, enabled) so loading a bookmark also configures the section view.

### For SceneCapture Actions
Bookmarks feed directly into `capture_floor_plan` camera params, enabling reproducible captures.

---

## Proposed Monolith Actions

### Phase 1: Core Debug View (~20h)

| Action | Namespace | Description | Est |
|--------|-----------|-------------|-----|
| `toggle_section_view` | mesh | MPC-based height clip on/off, set height | 8h |
| `toggle_ceiling_visibility` | mesh | Show/hide tagged ceiling actors | 3h |
| `capture_floor_plan` | mesh | Ortho SceneCapture top-down PNG | 5h |
| `highlight_room` | mesh | Overlay material on room actors | 4h |

### Phase 2: Camera & Polish (~15h)

| Action | Namespace | Description | Est |
|--------|-----------|-------------|-----|
| `save_camera_bookmark` | mesh | Save named camera state to JSON | 3h |
| `load_camera_bookmark` | mesh | Load camera state, apply to viewport/capture | 3h |
| `list_camera_bookmarks` | mesh | Enumerate saved bookmarks | 1h |
| `clear_highlights` | mesh | Remove all overlay materials | 2h |
| `toggle_section_box` | mesh | 3D box clip (extends section view) | 6h |

### Phase 3: Material Integration (~10h)

| Action | Namespace | Description | Est |
|--------|-----------|-------------|-----|
| MPC asset creation | material | Create MPC_SectionView if not exists | 2h |
| MF_SectionClip injection | material | Add clip function to proc gen materials | 4h |
| Section edge rendering | material | Colored edge at cut plane | 4h |

### Total: ~45h across 3 phases

---

## Implementation Priority

1. **`toggle_ceiling_visibility`** -- 3h, immediate value, no material changes needed
2. **`capture_floor_plan`** -- 5h, builds on existing SceneCapture pattern
3. **`highlight_room`** -- 4h, big UX win for conversational building review
4. **`toggle_section_view`** -- 8h, most polished result but needs material integration
5. Camera bookmarks -- 7h, nice-to-have
6. Section box -- 6h, V2

---

## Technical Dependencies

- **MPC creation:** Need `material_query("create_mpc", ...)` or do it in C++ at toggle time
- **Proc gen tagging:** `create_structure` and `create_building` must tag ceiling geometry with `Monolith.Ceiling` and `Monolith.Roof` during generation
- **Material injection:** Proc gen materials (wall, floor, ceiling) need MF_SectionClip in their graph
- **Existing helpers reusable:** `MonolithMeshLightingCapture::CreateTransientRT`, `RenderAndSaveCapture`

---

## Key Source References

| Symbol/File | Location | Relevance |
|-------------|----------|-----------|
| `USceneCaptureComponent2D` | `SceneCaptureComponent2D.h:33-321` | ClipPlane, OrthoWidth, HiddenActors |
| `UMaterialParameterCollectionInstance::SetScalarParameterValue` | `MaterialParameterCollectionInstance.h:39` | MPC runtime update |
| `EViewModeIndex` | `EngineBaseTypes.h:961-1066` | 35+ viewport modes |
| `MonolithMeshLightingCapture` | `MonolithMeshLightingCapture.cpp:26-80` | Existing transient RT + capture pattern |
| `MonolithEditorActions::capture_scene_preview` | `MonolithEditorActions.cpp:362-373` | Existing capture action pattern |
| `FMonolithMeshBlockoutActions::SetActorTags` | `MonolithMeshBlockoutActions.cpp:482-488` | Existing tag infrastructure |
| `MonolithMeshProceduralActions::CreateStructure` | `MonolithMeshProceduralActions.cpp:1680-1760` | Ceiling generation code to add tags |

---

## Sources

- [Cutaway Views Tutorial (zoltane.com)](https://zoltane.com/pages/unreal/cutaway-views/) -- MPC + material clip plane implementation
- [UE5 Custom Depth Stencil Demo](https://github.com/droganaida/ue5-postprocess-stencil-demo) -- Stencil masking patterns
- [Architectural Section Tool (UE Marketplace)](https://www.unrealengine.com/marketplace/en-US/product/architectural-section-tool/reviews) -- Commercial reference
- [Using Material Parameter Collections (UE 5.7 Docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/using-material-parameter-collections-in-unreal-engine)
- [Orthographic Camera (UE 5.7 Docs)](https://dev.epicgames.com/documentation/en-us/unreal-engine/orthographic-camera-in-unreal-engine)
- [USceneCaptureComponent2D (UE 5.7 API)](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponent2D)
- [Cross-Section Shader (UE Forums)](https://forums.unrealengine.com/t/cross-section-cut-out-shader/117652)
- [Section Planes Discussion (UE Forums)](https://forums.unrealengine.com/t/section-planes/128771)
- [Global Clip Plane Discussion (UE Forums)](https://forums.unrealengine.com/t/how-to-enable-global-clip-plane/360780)
- [Highlight Material Tutorial (Epic Community)](https://dev.epicgames.com/community/learning/tutorials/l3Py/highlight-any-object-in-unreal-engine-5-with-free-material-2024)
- [Pulsing Emissive Material (UE Forums)](https://forums.unrealengine.com/t/pulsating-emissive-glow/357006)
- [Camera Bookmarks (CBGameDev)](https://www.cbgamedev.com/blog/quick-dev-tip-12-ue4-stored-camera-positions)
