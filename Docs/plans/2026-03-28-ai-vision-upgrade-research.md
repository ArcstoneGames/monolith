# Enhanced AI Vision System for Procedural Building Validation

**Date:** 2026-03-28
**Scope:** Multi-angle automated capture, geometric analysis, computer vision, VLM review, debug visualization, and G-buffer access for procedural building QA in Monolith/Leviathan.
**Builds on:** `2026-03-28-procgen-validation-research.md` (3-tier validation pipeline)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Multi-Angle Automated Capture System](#2-multi-angle-automated-capture-system)
3. [Automated Geometric Analysis (No ML)](#3-automated-geometric-analysis-no-ml)
4. [Computer Vision Approaches (Lightweight)](#4-computer-vision-approaches-lightweight)
5. [VLM-Based Review](#5-vlm-based-review)
6. [UE5-Native Debug Visualization](#6-ue5-native-debug-visualization)
7. [Depth Buffer / G-Buffer Access](#7-depth-buffer--g-buffer-access)
8. [Contact Sheet Stitching](#8-contact-sheet-stitching)
9. [Proposed Architecture](#9-proposed-architecture)
10. [Effort Estimates](#10-effort-estimates)
11. [Sources](#11-sources)

---

## 1. Executive Summary

This research extends the existing 3-tier validation pipeline with a comprehensive "vision" layer. The key insight from our prior research holds: **programmatic validation catches 80%+ of issues and should always run first**. Vision (both classical CV and VLM) is supplementary but valuable for catching aesthetic and semantic problems that geometry checks miss.

**Six capability areas researched:**

| Area | Feasibility | Priority | Est. Hours |
|------|-------------|----------|------------|
| Multi-angle capture system | Fully feasible, native UE5 APIs | P0 | 20-28h |
| Geometric analysis (no ML) | Fully feasible, existing UE5 APIs | P0 | 16-24h |
| Lightweight CV (OpenCV) | Feasible as Python post-process | P1 | 14-20h |
| VLM-based review | Feasible, $0.06/building | P2 | 12-16h |
| Debug visualization | Fully feasible, standard UE5 | P0 | 18-24h |
| G-buffer / depth access | Fully feasible, native capture source | P0 | 8-12h |

**Total estimated effort:** 88-124 hours across 4 phases.

**Key technical finding:** UE 5.7 `ESceneCaptureSource` enum provides 10 capture modes including depth (`SCS_SceneDepth`), normals (`SCS_Normal`), base color (`SCS_BaseColor`), and scene color with depth in alpha (`SCS_SceneColorSceneDepth`). One `USceneCaptureComponent2D` per buffer type, all positioned identically, captures everything we need in a single frame.

---

## 2. Multi-Angle Automated Capture System

### 2.1 Camera Placement Strategy

For comprehensive building coverage, we need captures from multiple viewpoints. Based on architectural photography conventions and game QA practices:

**Minimum viable set (6 views):**
1. **Top-down orthographic** -- floor plan view, room connectivity, wall completeness
2. **4 cardinal elevations** (N/S/E/W) -- exterior facades, windows, doors, roof
3. **First-person from entrance** -- what the player sees entering

**Extended set (12+ views):**
6. **4 corner perspectives** (NE/NW/SE/SW, 45deg, ~30deg pitch down) -- 3D form, roof, overhangs
7. **Per-room interior perspectives** -- proportions, floor integrity, door passability
8. **Through-door views** -- verify openings are clear
9. **Stairwell views** -- step consistency, angle assessment
10. **Street-level eye-height** -- player experience perspective

**Camera parameter table:**

| View Type | Projection | FOV / OrthoWidth | Height | Pitch |
|-----------|-----------|-------------------|--------|-------|
| Top-down | Orthographic | building_bounds.X * 1.2 | bounds.Z + 500 | -90 |
| Elevation N/S/E/W | Perspective | 60 | bounds.Z * 0.5 | 0 |
| Corner NE/NW/SE/SW | Perspective | 75 | bounds.Z * 0.7 | -25 |
| First-person entrance | Perspective | 90 | 170 (eye height) | 0 |
| Per-room interior | Perspective | 90 | 170 | 0 |
| Through-door | Perspective | 75 | 170 | 0 |
| Stairwell | Perspective | 60 | 170 | -30 |

### 2.2 UE5 SceneCaptureComponent2D Configuration

**Verified API (UE 5.7 source):**

```cpp
// SceneCaptureComponent2D.h -- key properties
UPROPERTY() TObjectPtr<UTextureRenderTarget2D> TextureTarget;
UPROPERTY() TEnumAsByte<ECameraProjectionMode::Type> ProjectionType;
UPROPERTY() float FOVAngle;
UPROPERTY() float OrthoWidth;
UPROPERTY() struct FPostProcessSettings PostProcessSettings;
UPROPERTY() float PostProcessBlendWeight;
UPROPERTY() bool bEnableClipPlane;
UPROPERTY() FVector ClipPlaneBase;
UPROPERTY() FVector ClipPlaneNormal;
UPROPERTY() bool bRenderInMainRenderer; // NEW in 5.7 -- renders as additional pass of main renderer
```

**ESceneCaptureSource enum (verified from EngineTypes.h:531):**

| Enum Value | Captures | RT Format | Use Case |
|------------|----------|-----------|----------|
| `SCS_SceneColorHDR` | Color (HDR) in RGB, Inv Opacity in A | RGBA16F | Primary color capture |
| `SCS_SceneColorHDRNoAlpha` | Color (HDR) in RGB, 0 in A | RGBA16F | Color without alpha artifacts |
| `SCS_FinalColorLDR` | Final Color (LDR) in RGB | RGBA8 | What player sees (post-tone-map) |
| `SCS_SceneColorSceneDepth` | Color (HDR) in RGB, SceneDepth in A | RGBA16F | Color + depth in one RT |
| `SCS_SceneDepth` | SceneDepth in R | R32F | Pure depth buffer |
| `SCS_DeviceDepth` | DeviceDepth in RGB | RGBA8 | Normalized 0-1 depth |
| `SCS_Normal` | Normal in RGB (Deferred only) | RGBA8 | Surface normals |
| `SCS_BaseColor` | BaseColor in RGB (Deferred only) | RGBA8 | Albedo without lighting |
| `SCS_FinalColorHDR` | Final Color (HDR) in Linear WCS | RGBA16F | Linear color |
| `SCS_FinalToneCurveHDR` | Final Color + tone curve in Linear sRGB | RGBA16F | Graded color |

**Optimal capture configuration for validation:**

For each viewpoint, spawn 4 `USceneCaptureComponent2D` instances at the same location/rotation:
1. `SCS_FinalColorLDR` -- visual review (LDR, post-processed, what player sees)
2. `SCS_SceneDepth` -- depth analysis (R32F, linear depth in cm)
3. `SCS_Normal` -- surface orientation (RGB world-space normals)
4. `SCS_BaseColor` -- albedo without lighting (room type identification via material)

**Optimization:** Use `SCS_SceneColorSceneDepth` to get color + depth in a single RT, reducing to 3 captures per viewpoint.

**Performance:** `bRenderInMainRenderer = true` (new in 5.7) renders the capture as an additional pass of the main renderer rather than an independent renderer. This is significantly cheaper for depth/normal/basecolor modes as it reuses existing G-buffer data. For scene color modes, it disables lighting and shadows.

### 2.3 Show Flags for Diagnostic Modes

`USceneCaptureComponent2D` exposes `ShowFlagSettings` (array of `FEngineShowFlagsSetting`) that control rendering for that capture independently of the viewport.

**Wireframe overlay capture:**

```cpp
// Set wireframe mode via show flags
CaptureComponent->ShowFlagSettings.Add(FEngineShowFlagsSetting{TEXT("Wireframe"), true});
CaptureComponent->ShowFlagSettings.Add(FEngineShowFlagsSetting{TEXT("Lighting"), false});
```

**Unlit capture (flat shading for room identification):**

```cpp
CaptureComponent->ShowFlagSettings.Add(FEngineShowFlagsSetting{TEXT("Lighting"), false});
// With bCaptureBaseColorToEmissive = true, basecolor routes to emissive for unlit visibility
```

**Collision visualization:**

```cpp
CaptureComponent->ShowFlagSettings.Add(FEngineShowFlagsSetting{TEXT("Collision"), true});
CaptureComponent->ShowFlagSettings.Add(FEngineShowFlagsSetting{TEXT("CollisionPawn"), true});
```

**Available view modes for diagnostic captures:**
- `Wireframe` -- mesh topology, edge density, face counts
- `Unlit` -- pure color, no lighting artifacts
- `Lighting Only` -- just lighting, catches shadow leaks
- `LightComplexity` -- overdraw/complexity hotspots
- `ShaderComplexity` -- material cost
- `CollisionPawn` / `CollisionVisibility` -- collision mesh vis

### 2.4 Post-Process Material Overrides

Each `USceneCaptureComponent2D` has `PostProcessSettings` with `WeightedBlendables` for custom diagnostic materials.

**Custom depth/normals visualization:**

```cpp
// Add diagnostic PP material to scene capture only
FWeightedBlendable Blendable;
Blendable.Object = DiagnosticMaterialInstance;
Blendable.Weight = 1.0f;
CaptureComponent->PostProcessSettings.WeightedBlendables.Array.Add(Blendable);
CaptureComponent->PostProcessBlendWeight = 1.0f;
```

**Useful diagnostic PP materials:**
- **Edge detection** (Sobel/Laplacian on scene depth) -- highlights geometry edges, reveals gaps
- **Room ID colorization** (custom stencil -> lookup table) -- rooms as solid colors
- **Z-fighting detector** (depth derivative analysis) -- highlights high-frequency depth changes
- **Normal discontinuity** (normal buffer derivative) -- reveals hard edges and face boundaries

### 2.5 Render Target Readback

**Synchronous (simple, blocking):**

```cpp
FTextureRenderTarget2DResource* RTResource = TextureTarget->GameThread_GetRenderTargetResource();
TArray<FColor> Pixels;
RTResource->ReadPixels(Pixels); // Blocks game thread 1-4ms
```

**Asynchronous (recommended for batch capture):**

Using `FRenderCommandFence` to avoid blocking the game thread (from nicholas477.github.io):

```cpp
// Step 1: Issue the capture
CaptureComponent->CaptureScene();

// Step 2: Create fence
FRenderCommandFence ReadbackFence;
ReadbackFence.BeginFence();

// Step 3: Check completion on tick (non-blocking)
if (ReadbackFence.IsFenceComplete())
{
    // Now safe to ReadPixels
    RTResource->ReadPixels(Pixels);
}
```

**Performance comparison:**
- Synchronous `ReadPixels()`: 1-4ms, stalls game thread
- Async with `FRenderCommandFence`: ~40us game thread cost, 1-4ms total latency
- For batch capture of 12 views: sync = 12-48ms stall, async = ~0.5ms + 4ms final readback

### 2.6 Saving to Disk

**Verified API (UE 5.7):**

```cpp
// FImageUtils::ExportRenderTarget2DAsPNG (ImageUtils.cpp:1266)
bool FImageUtils::ExportRenderTarget2DAsPNG(UTextureRenderTarget2D* TexRT, FArchive& Ar);

// Usage:
FBufferArchive Buffer;
FImageUtils::ExportRenderTarget2DAsPNG(RenderTarget, Buffer);
FFileHelper::SaveArrayToFile(Buffer, *FilePath);
```

Also available: `ExportRenderTarget2DAsEXR` for HDR depth data.

**File naming convention:**
```
Saved/Monolith/Validation/{BuildingId}/
    color_topdown.png
    color_elevation_n.png
    color_elevation_e.png
    color_elevation_s.png
    color_elevation_w.png
    color_entrance.png
    depth_topdown.exr
    depth_elevation_n.exr
    normals_topdown.png
    normals_elevation_n.png
    wireframe_topdown.png
    contact_sheet.png          // stitched composite
    validation_report.json     // results
```

---

## 3. Automated Geometric Analysis (No ML)

These checks run purely in UE5 C++ with zero external dependencies. They form Tier 1 + Tier 2 of the validation pipeline.

### 3.1 Ray Grid Wall Analysis

**Purpose:** Detect holes, gaps, and incomplete booleans in walls by casting a grid of rays through each wall face.

**Algorithm:**

```
For each wall_face in building:
    // Create a 2D grid of ray origins on interior side
    grid_spacing = 10cm  // dense enough to catch small holes
    rays_x = wall_face.width / grid_spacing
    rays_z = wall_face.height / grid_spacing

    for x in range(rays_x):
        for z in range(rays_z):
            origin = wall_face.interior_point + (x * grid_spacing, 0, z * grid_spacing)
            direction = wall_face.outward_normal

            hit = LineTraceSingle(origin, origin + direction * wall_thickness * 2)

            if NOT hit.bBlockingHit:
                // Ray passed through wall -- HOLE detected
                record_hole(wall_face, x, z, origin)
            else if hit.Distance < wall_thickness * 0.5:
                // Hit too close -- wall may be too thin
                record_thin_wall(wall_face, x, z, hit.Distance)
```

**Expected results:**
- Door openings: contiguous rectangular region of "holes" (expected, verify against spatial registry)
- Window openings: contiguous region at expected height (expected)
- Unexpected holes: boolean artifacts, missing geometry, face culling errors

**Performance:** 20x30 grid (200cm x 300cm wall at 10cm spacing) = 600 rays. At ~0.01ms/ray = 6ms per wall. 40 walls = 240ms. Acceptable.

### 3.2 Capsule Sweep Door Passability

**Purpose:** Verify every door opening is traversable by the player capsule.

**UE5 API (verified):**

```cpp
FCollisionShape CapsuleShape = FCollisionShape::MakeCapsule(
    42.f,   // Leviathan player capsule radius
    96.f    // Leviathan player capsule half-height
);

FCollisionQueryParams QueryParams;
QueryParams.bTraceComplex = false; // Simple collision is faster
QueryParams.AddIgnoredActor(BuildingActor);

FHitResult Hit;
bool bHit = GetWorld()->SweepSingleByChannel(
    Hit,
    DoorCenter - DoorNormal * 100.f,  // Start 100cm before door
    DoorCenter + DoorNormal * 100.f,  // End 100cm after door
    FQuat::Identity,
    ECC_Pawn,
    CapsuleShape,
    QueryParams
);

if (bHit)
{
    // Door is blocked -- report what's in the way
    Report.AddIssue(EValidationSeverity::Critical,
        FString::Printf(TEXT("Door %s blocked by %s at %s"),
            *Door.Name, *Hit.GetActor()->GetName(),
            *Hit.ImpactPoint.ToString()));
}
```

**Extended checks:**
- Wheelchair accessibility: sweep with 60cm radius (120cm diameter)
- Double doors: two parallel sweeps offset by door width/4
- Angled doors: orient sweep along door normal, not room-to-room

**Performance:** ~0.1ms per door. 20 doors = 2ms.

### 3.3 Floor Continuity Check

**Purpose:** Verify every room has a solid floor with no holes.

**Algorithm:**

```
For each room in building:
    room_bounds = get_room_bounds(room)
    grid_spacing = 25cm

    for x in range(room_bounds.min.x, room_bounds.max.x, grid_spacing):
        for y in range(room_bounds.min.y, room_bounds.max.y, grid_spacing):
            origin = (x, y, room.floor_z + 100)  // 100cm above expected floor
            hit = LineTraceSingle(origin, origin - (0, 0, 200), WorldStatic)

            if NOT hit.bBlockingHit:
                record_floor_hole(room, x, y)
            else if abs(hit.ImpactPoint.Z - room.floor_z) > 5.0:
                // Floor at wrong height
                record_floor_offset(room, x, y, hit.ImpactPoint.Z - room.floor_z)
```

### 3.4 Exterior Wall Continuity

**Purpose:** Verify no gaps in exterior walls by sweeping rays inward from outside the building.

**Algorithm:**

```
For each exterior_face in building:
    // Cast rays from outside inward along a dense grid
    for each ray_origin in face_grid(exterior_face, spacing=15cm):
        direction = exterior_face.inward_normal
        origin = ray_origin + direction * -50  // 50cm outside

        hit = LineTraceSingle(origin, origin + direction * 200)

        if NOT hit.bBlockingHit:
            // Ray passed through exterior -- GAP in wall
            record_exterior_gap(exterior_face, ray_origin)
```

### 3.5 Room Enclosure Validation

**Purpose:** Verify each room is fully enclosed by walls (no open sides).

**Algorithm:**

```
For each room in building:
    room_center = room.center at mid-height
    num_directions = 36  // Every 10 degrees around the room
    expected_walls = room.wall_count

    open_directions = []
    for angle in range(0, 360, 10):
        direction = (cos(angle), sin(angle), 0)
        hit = LineTraceSingle(room_center, room_center + direction * 5000)

        if NOT hit.bBlockingHit OR hit.Distance > room.max_expected_dimension * 2:
            open_directions.append(angle)

    if len(open_directions) > 36 * 0.1:  // More than 10% open
        record_unenclosed_room(room, open_directions)
```

### 3.6 Overlapping Geometry Detection

**Purpose:** Detect two meshes occupying the same space (causes z-fighting, wasted tris).

**Approach -- AABB Broad Phase + Ray Narrow Phase:**

```
// Broad phase: find overlapping bounding boxes
overlapping_pairs = []
for i, actor_a in enumerate(building_actors):
    for j, actor_b in enumerate(building_actors):
        if i >= j: continue
        if actor_a.bounds.Intersects(actor_b.bounds):
            overlapping_pairs.append((actor_a, actor_b))

// Narrow phase: sample overlapping regions
for (actor_a, actor_b) in overlapping_pairs:
    overlap_box = actor_a.bounds.Intersection(actor_b.bounds)
    if overlap_box.volume < threshold: continue  // Trivial overlap

    // Cast rays through overlap region
    for sample_point in grid_points(overlap_box, spacing=20cm):
        hit_a = ray_hits_actor(sample_point, actor_a)
        hit_b = ray_hits_actor(sample_point, actor_b)

        if hit_a AND hit_b AND distance(hit_a, hit_b) < 1.0:
            // Two surfaces within 1cm of each other
            record_overlap(actor_a, actor_b, sample_point)
```

**Academic approaches (from search results):**
- Boundary traversal for edge-manifold meshes to detect holes (Robust Hole-Detection, arxiv 2311.12466)
- Surface overlap detection via distance+angle thresholds (Sandia CUBIT approach)
- Spatial hashing for broad-phase (Teschner et al.) -- O(n) expected time vs O(n^2) brute force
- Graph learning for 3D geometry classification (Springer 2024)

### 3.7 Mesh Topology Validation

**Purpose:** Check mesh health -- closed manifold, no degenerate triangles, correct normals.

**GeometryScript functions (verified available in UE 5.7):**

```cpp
// Check if mesh is closed (no boundary edges)
bool bIsClosed = UGeometryScriptLibrary_MeshQueryFunctions::GetIsClosed(DynamicMesh);

// Get mesh statistics
FGeometryScriptMeshStatistics Stats;
UGeometryScriptLibrary_MeshQueryFunctions::GetMeshStatistics(DynamicMesh, Stats);
// Stats includes: TriangleCount, VertexCount, BoundaryEdgeCount, NonManifoldEdgeCount

// Check genus (topology complexity)
// genus 0 = sphere-like (closed room), genus > 0 = has handles/tunnels
int32 Genus = (2 - Stats.EulerCharacteristic) / 2;  // For closed meshes
```

**Validation checks:**
- `BoundaryEdgeCount > 0` -- mesh has holes (not closed)
- `NonManifoldEdgeCount > 0` -- topology error (shared edges between 3+ faces)
- Degenerate triangles: area < epsilon
- Flipped normals: dot(face_normal, expected_outward) < 0

---

## 4. Computer Vision Approaches (Lightweight)

These run as Python post-processing on captured PNG images. No large ML models -- pure OpenCV / numpy algorithms.

### 4.1 Edge Detection for Wall Alignment

**Purpose:** Detect wall alignment issues, non-orthogonal walls, and structural irregularities from captured elevation images.

**Pipeline:**

```python
import cv2
import numpy as np

def analyze_wall_alignment(image_path: str) -> dict:
    img = cv2.imread(image_path)
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # 1. Gaussian blur to reduce noise
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    # 2. Canny edge detection
    edges = cv2.Canny(blurred, 50, 150)

    # 3. Hough Line Transform to detect straight lines
    lines = cv2.HoughLinesP(edges,
        rho=1,               # 1 pixel resolution
        theta=np.pi/180,     # 1 degree resolution
        threshold=100,        # min votes
        minLineLength=50,     # min line length in pixels
        maxLineGap=10         # max gap between segments
    )

    # 4. Classify lines by angle
    horizontal_lines = []
    vertical_lines = []
    diagonal_lines = []

    for line in lines:
        x1, y1, x2, y2 = line[0]
        angle = np.degrees(np.arctan2(y2 - y1, x2 - x1))

        if abs(angle) < 5 or abs(angle) > 175:
            horizontal_lines.append(line)
        elif 85 < abs(angle) < 95:
            vertical_lines.append(line)
        else:
            diagonal_lines.append(line)

    # 5. Flag issues
    issues = []
    if len(diagonal_lines) > len(horizontal_lines) * 0.1:
        issues.append("Significant non-orthogonal geometry detected")

    # 6. Check vertical line alignment (windows should be on same grid)
    v_positions = sorted([line[0][0] for line in vertical_lines])
    spacings = np.diff(v_positions)
    if len(spacings) > 2:
        spacing_std = np.std(spacings)
        spacing_mean = np.mean(spacings)
        if spacing_std > spacing_mean * 0.3:
            issues.append(f"Irregular vertical spacing (std/mean={spacing_std/spacing_mean:.2f})")

    return {"horizontal": len(horizontal_lines),
            "vertical": len(vertical_lines),
            "diagonal": len(diagonal_lines),
            "issues": issues}
```

### 4.2 Window Pattern Analysis (Template Matching)

**Purpose:** Verify window spacing regularity and detect missing/extra windows.

**Approach:** Use OpenCV template matching on elevation captures. Since our windows are procedurally generated with known dimensions, we can generate a template from the window spec.

```python
def analyze_window_pattern(elevation_image: str, window_template: str) -> dict:
    img = cv2.imread(elevation_image)
    template = cv2.imread(window_template)

    # Multi-scale template matching (windows at different distances)
    detections = []
    for scale in [0.8, 0.9, 1.0, 1.1, 1.2]:
        resized = cv2.resize(template, None, fx=scale, fy=scale)
        result = cv2.matchTemplate(img, resized, cv2.TM_CCOEFF_NORMED)
        threshold = 0.7
        locations = np.where(result >= threshold)
        for pt in zip(*locations[::-1]):
            detections.append((pt[0], pt[1], scale))

    # NMS to remove duplicate detections
    detections = non_max_suppression(detections, overlap_thresh=0.3)

    # Analyze spacing regularity
    if len(detections) >= 2:
        x_positions = sorted([d[0] for d in detections])
        spacings = np.diff(x_positions)
        regularity = 1.0 - (np.std(spacings) / np.mean(spacings)) if np.mean(spacings) > 0 else 0

        return {
            "window_count": len(detections),
            "regularity_score": regularity,  # 1.0 = perfectly even, 0.0 = random
            "spacings": spacings.tolist(),
            "issue": "irregular" if regularity < 0.7 else "ok"
        }
```

**Challenges and mitigations:**
- Scale variation: multi-scale matching handles windows at different depths
- Occlusion: elevation captures minimize occlusion vs perspective
- False positives: NMS + threshold tuning, verified against spatial registry window count
- Edge-based matching more robust than intensity-based for procedural geometry (less texture variation)

### 4.3 Connected Component Analysis on Floor Plans

**Purpose:** Verify room connectivity and segmentation from top-down orthographic captures.

```python
def analyze_floor_plan(floor_plan_image: str) -> dict:
    img = cv2.imread(floor_plan_image, cv2.IMREAD_GRAYSCALE)

    # Threshold to separate walls (dark) from rooms (light)
    _, binary = cv2.threshold(img, 128, 255, cv2.THRESH_BINARY)

    # Connected component analysis
    num_labels, labels, stats, centroids = cv2.connectedComponentsWithStats(binary)

    rooms = []
    for i in range(1, num_labels):  # Skip background (label 0)
        area = stats[i, cv2.CC_STAT_AREA]
        if area > min_room_area_pixels:  # Filter noise
            rooms.append({
                "id": i,
                "area_px": area,
                "centroid": centroids[i].tolist(),
                "bbox": [stats[i, cv2.CC_STAT_LEFT],
                         stats[i, cv2.CC_STAT_TOP],
                         stats[i, cv2.CC_STAT_WIDTH],
                         stats[i, cv2.CC_STAT_HEIGHT]]
            })

    # Verify against expected room count
    issues = []
    if len(rooms) != expected_room_count:
        issues.append(f"Found {len(rooms)} rooms, expected {expected_room_count}")

    # Check for very small rooms (artifacts)
    median_area = np.median([r["area_px"] for r in rooms])
    for room in rooms:
        if room["area_px"] < median_area * 0.1:
            issues.append(f"Room {room['id']} suspiciously small ({room['area_px']}px)")

    return {"room_count": len(rooms), "rooms": rooms, "issues": issues}
```

### 4.4 Depth Buffer Analysis

**Purpose:** Detect floating geometry, Z-fighting, and mesh interpenetration from depth captures.

```python
def analyze_depth_buffer(depth_exr_path: str) -> dict:
    # Load EXR depth (linear depth in cm)
    depth = cv2.imread(depth_exr_path, cv2.IMREAD_UNCHANGED)

    issues = []

    # 1. Z-fighting detection: high-frequency depth oscillation
    # Compute depth gradient magnitude
    grad_x = cv2.Sobel(depth, cv2.CV_64F, 1, 0, ksize=3)
    grad_y = cv2.Sobel(depth, cv2.CV_64F, 0, 1, ksize=3)
    gradient_mag = np.sqrt(grad_x**2 + grad_y**2)

    # Z-fighting appears as noise in flat regions
    flat_regions = gradient_mag < np.percentile(gradient_mag, 50)
    flat_depth_std = np.std(depth[flat_regions])
    if flat_depth_std > z_fighting_threshold:
        issues.append(f"Possible Z-fighting (flat region depth std={flat_depth_std:.3f}cm)")

    # 2. Floating geometry: isolated depth discontinuities
    # Large depth jumps that don't form continuous edges
    depth_laplacian = cv2.Laplacian(depth, cv2.CV_64F)
    extreme_laplacian = np.abs(depth_laplacian) > depth.max() * 0.1
    num_labels, labels = cv2.connectedComponents(extreme_laplacian.astype(np.uint8))

    for label_id in range(1, num_labels):
        region = labels == label_id
        region_area = np.sum(region)
        if 10 < region_area < 500:  # Small isolated discontinuity
            issues.append(f"Possible floating geometry at region {label_id} ({region_area}px)")

    # 3. Depth holes (sky/void visible through building)
    sky_pixels = depth > 100000  # Very far depth = sky
    sky_ratio = np.sum(sky_pixels) / depth.size
    if sky_ratio > 0.01:  # More than 1% sky in a building capture
        issues.append(f"Depth holes detected ({sky_ratio*100:.1f}% sky pixels)")

    return {"issues": issues, "depth_range": (depth.min(), depth.max())}
```

### 4.5 Integration: Python Post-Process Pipeline

All CV analysis runs as a Python script invoked after UE5 captures images to disk:

```python
def validate_building_cv(building_dir: str, spec: dict) -> dict:
    results = {}

    # Wall alignment from elevations
    for direction in ['n', 'e', 's', 'w']:
        img_path = f"{building_dir}/color_elevation_{direction}.png"
        results[f"alignment_{direction}"] = analyze_wall_alignment(img_path)

    # Window pattern from elevations
    for direction in ['n', 'e', 's', 'w']:
        img_path = f"{building_dir}/color_elevation_{direction}.png"
        results[f"windows_{direction}"] = analyze_window_pattern(img_path, spec["window_template"])

    # Room connectivity from floor plan
    results["floor_plan"] = analyze_floor_plan(f"{building_dir}/color_topdown.png")

    # Depth analysis
    for view in ['topdown', 'elevation_n', 'entrance']:
        depth_path = f"{building_dir}/depth_{view}.exr"
        results[f"depth_{view}"] = analyze_depth_buffer(depth_path)

    return results
```

**Dependencies:** OpenCV (`cv2`), numpy. Both already available via pip, no GPU required.

**Performance:** ~200-500ms per building for all CV analysis. Negligible compared to VLM costs.

---

## 5. VLM-Based Review

### 5.1 Updated Benchmark Data (March 2026)

Two key papers inform our VLM strategy:

**VideoGameQA-Bench (NeurIPS 2025):**
- 9 tasks, 4,786 questions, 16 VLMs evaluated
- Sony Interactive Entertainment co-authored
- GPT-4o: 82.8% accuracy on image glitch detection (best)
- Visual unit testing: 53% (poor) -- struggles with precise spatial details
- Visual regression: 45.2% (worst) -- misses subtle changes
- UI testing: 40% (very poor)
- At 5% defect prevalence, 17.8% false positive rate

**"How Far Can VLMs Go for Visual Bug Detection?" (FSE 2026, arxiv 2603.22706):**
- 41 hours gameplay, 19,738 keyframes, 100 videos
- Baseline VLM: precision 0.50, accuracy 0.72
- Judge models (secondary VLM re-evaluation): marginal improvement only
- Metadata-augmented prompting: marginal improvement, increased cost/variance
- **Key conclusion:** "Off-the-shelf VLMs can detect a range of visual bugs, but hybrid approaches separating textual and visual anomaly detection are needed"

### 5.2 Cost Analysis

**Claude vision token calculation (verified from API docs):**
`tokens = (width_px * height_px) / 750`

Images are resized internally to max 1568px on the long side.

| Image Size | Tokens | Claude Sonnet 4.6 Cost (input) | Notes |
|------------|--------|-------------------------------|-------|
| 512x512 | ~349 | ~$0.001 | Fast, low detail |
| 1024x1024 | ~1,398 | ~$0.004 | Good balance |
| 1568x1568 | ~3,277 | ~$0.010 | Max effective res |
| 6 images @ 1024x1024 | ~8,389 | ~$0.025 | One building, 6 views |
| Contact sheet 2048x2048 | ~5,592 | ~$0.017 | Composite image |

**Per-building cost estimates:**

| Strategy | Input Tokens | Output Tokens (~500) | Total Cost | Latency |
|----------|-------------|---------------------|------------|---------|
| 6 individual images + prompt | ~9,000 | ~500 | ~$0.035 | 3-8s |
| 1 contact sheet + prompt | ~6,000 | ~500 | ~$0.026 | 2-5s |
| Batch API (50% off) | ~6,000 | ~500 | ~$0.013 | 24h max |

**Batch validation costs:**
- 10 buildings: ~$0.13-0.35
- 100 buildings: ~$1.30-3.50
- 1000 buildings (nightly): ~$13-35
- With Batch API: ~$6.50-17.50

### 5.3 Optimal Prompt Engineering

Based on NVIDIA's VLM Prompt Engineering Guide (March 2025) and the FSE 2026 findings:

**Best practices for architectural review prompts:**

1. **Structured output format** -- request JSON, not free text. VLMs produce more consistent results with explicit schema.
2. **Provide building spec as context** -- room count, door count, window count, dimensions. This grounds the model.
3. **One-shot example** -- include one example of a known issue with annotation. Improves detection accuracy significantly.
4. **Checklist format** -- enumerate specific things to check. VLMs perform better with explicit task decomposition.
5. **Severity classification** -- define CRITICAL/WARNING/INFO upfront with examples of each.
6. **Contact sheet over individual images** -- cheaper and VLMs can cross-reference views.

**Recommended prompt template:**

```
You are a QA inspector for procedurally generated game buildings.

BUILDING SPEC:
- Rooms: {room_count} ({room_list})
- Floors: {floor_count}, floor height: {floor_height}cm
- Doors: {door_count} (min width: {min_door_width}cm)
- Windows: {window_count}
- Building dimensions: {width}x{depth}x{height}cm

IMAGE LAYOUT (contact sheet):
Row 1: Top-down floor plan | Entrance first-person view
Row 2: North elevation | South elevation
Row 3: East elevation | West elevation

INSPECT FOR:
1. DOORS: Are all {door_count} doors visible and fully cut through walls?
2. WINDOWS: Are all {window_count} windows visible? Evenly spaced per facade?
3. WALLS: Any gaps, holes, or overlapping geometry?
4. STAIRS: If multi-story, are stairs visible and at walkable angle (<45 deg)?
5. FLOORS: Any visible holes in floor surfaces?
6. PROPORTIONS: Do room sizes appear reasonable?
7. ARTIFACTS: Floating geometry, Z-fighting (shimmering surfaces), dark patches?

Respond with JSON:
{
  "overall": "PASS" | "FAIL",
  "confidence": 0.0-1.0,
  "issues": [
    {
      "severity": "CRITICAL" | "WARNING" | "INFO",
      "category": "doors" | "windows" | "walls" | "stairs" | "floors" | "proportions" | "artifacts",
      "location": "describe where in image",
      "description": "what's wrong",
      "suggested_fix": "if obvious"
    }
  ]
}
```

### 5.4 Multi-View Formatting for VLMs

**Contact sheet (preferred):**
Stitch 6 views into a single 2x3 or 3x2 grid image with labels. Cheaper (one image cost), VLM can cross-reference views.

**Individual images (higher accuracy):**
Send each view as a separate image with specific per-view prompts. More tokens but VLM focuses on each view.

**Recommendation:** Use contact sheet for batch/nightly validation, individual images for targeted investigation of flagged buildings.

---

## 6. UE5-Native Debug Visualization

### 6.1 DrawDebugHelpers (Runtime)

**Header:** `DrawDebugHelpers.h`

Available functions for procedural building debug rendering:

```cpp
#include "DrawDebugHelpers.h"

// Room labels
DrawDebugString(World, RoomCenter, RoomName, nullptr, FColor::White, 0.f, true, 1.5f);

// Door markers (green = passable, red = blocked)
DrawDebugBox(World, DoorCenter, DoorHalfExtent, DoorRotation,
    bPassable ? FColor::Green : FColor::Red, true, 0.f, 0, 3.f);

// Wall normals
DrawDebugDirectionalArrow(World, WallCenter, WallCenter + WallNormal * 50.f,
    20.f, FColor::Cyan, true, 0.f, 0, 2.f);

// Capsule sweep path visualization
DrawDebugCapsule(World, SweepStart, HalfHeight, Radius, FQuat::Identity,
    FColor::Yellow, true, 0.f, 0, 1.f);

// Problem areas
DrawDebugSphere(World, ProblemLocation, 25.f, 12, FColor::Red, true, 0.f, 0, 3.f);
```

**Limitation:** `DrawDebug*` functions are editor/development only. Stripped in shipping builds. For our use case (editor-time validation), this is perfect.

**Persistence:** Pass `bPersistentLines = true` and `LifeTime = 0.f` for lines that stay until `FlushPersistentDebugLines()`.

### 6.2 Custom Show Flags with UDebugDrawService

**Purpose:** Add a "Building Validation" toggle in the viewport Show menu that renders all validation overlays.

**Implementation pattern:**

```cpp
// In your editor module header
#include "Debug/DebugDrawService.h"

static TCustomShowFlag<> BuildingValidationShowFlag(
    TEXT("BuildingValidation"),
    false,          // default off
    SFG_Developer,  // group
    FText::FromString(TEXT("Building Validation"))
);

// In StartupModule:
DebugDrawDelegateHandle = UDebugDrawService::Register(
    TEXT("BuildingValidation"),
    FDebugDrawDelegate::CreateRaw(this, &FMyModule::OnDebugDraw)
);

// In ShutdownModule:
UDebugDrawService::Unregister(DebugDrawDelegateHandle);

// Draw callback:
void FMyModule::OnDebugDraw(UCanvas* Canvas, APlayerController* PC)
{
    UWorld* World = PC->GetWorld();

    // Draw room labels
    for (const auto& Room : SpatialRegistry->GetRooms())
    {
        FVector2D ScreenPos;
        if (PC->ProjectWorldLocationToScreen(Room.Center, ScreenPos))
        {
            Canvas->SetDrawColor(FColor::White);
            Canvas->DrawText(GEngine->GetSmallFont(), Room.Name,
                ScreenPos.X, ScreenPos.Y);
        }
    }

    // Draw door markers
    for (const auto& Door : SpatialRegistry->GetDoors())
    {
        DrawDebugBox(World, Door.Center, Door.HalfExtent,
            Door.Rotation, Door.bPassable ? FColor::Green : FColor::Red,
            false, -1.f, 0, 2.f);
    }

    // Draw validation issues
    for (const auto& Issue : LastValidationResult.Issues)
    {
        FColor IssueColor = Issue.Severity == Critical ? FColor::Red :
                            Issue.Severity == Warning ? FColor::Yellow : FColor::Cyan;
        DrawDebugSphere(World, Issue.Location, 30.f, 8, IssueColor,
            false, -1.f, 0, 3.f);
    }
}
```

### 6.3 NavMesh Overlay

**Toggle NavMesh visibility programmatically:**

```cpp
// Console command approach
GEngine->Exec(World, TEXT("show Navigation"));

// Or via RecastNavMesh actor
ARecastNavMesh* NavMesh = Cast<ARecastNavMesh>(
    FNavigationSystem::GetCurrent<UNavigationSystemV1>(World)->GetDefaultNavDataInstance());
if (NavMesh)
{
    NavMesh->bDrawNavMesh = true;
    NavMesh->bDrawFilledPolys = true;
    NavMesh->DrawOffset = 5.f;  // Slight offset for visibility
}
```

**Keyboard shortcut:** Press `P` in editor viewport to toggle NavMesh visibility.

### 6.4 Material Override Mode (Room Type Colorization)

**Purpose:** Replace all materials with solid colors based on room type for at-a-glance floor plan verification.

**Approach 1: Custom Stencil + Post-Process**

```cpp
// Assign custom stencil values per room type
for (auto& [RoomId, Actors] : RoomActorMap)
{
    uint8 StencilValue = GetRoomTypeStencilValue(RoomId); // 1=bedroom, 2=bathroom, etc.
    for (AActor* Actor : Actors)
    {
        for (UMeshComponent* Mesh : Actor->GetComponentsByClass<UMeshComponent>())
        {
            Mesh->SetRenderCustomDepth(true);
            Mesh->SetCustomDepthStencilValue(StencilValue);
        }
    }
}
```

Then a post-process material reads `CustomStencil` and maps to colors via a lookup texture.

**Approach 2: Overlay Material**

```cpp
// UMeshComponent::SetOverlayMaterial -- no material graph changes needed
UMaterialInstanceDynamic* OverlayMID = UMaterialInstanceDynamic::Create(
    OverlayBaseMaterial, this);  // Unlit, translucent, solid color
OverlayMID->SetVectorParameterValue(TEXT("Color"), RoomTypeColor);

for (UMeshComponent* Mesh : RoomActors)
{
    Mesh->SetOverlayMaterial(OverlayMID);
}
```

### 6.5 Collision Visualization

```cpp
// Toggle collision visualization via show flags
FEngineShowFlags& ShowFlags = Viewport->GetClient()->GetEngineShowFlags();
ShowFlags.SetCollisionPawn(true);   // Show pawn collision
ShowFlags.SetCollisionVisibility(true);  // Show visibility collision
```

---

## 7. Depth Buffer / G-Buffer Access

### 7.1 Direct G-Buffer Capture via ESceneCaptureSource

**This is the primary mechanism.** No custom post-process materials needed -- UE 5.7 exposes G-buffer layers directly as capture sources.

| Buffer | CaptureSource | RT Format | Notes |
|--------|--------------|-----------|-------|
| Depth | `SCS_SceneDepth` | R32F | Linear depth in cm, single channel |
| Device Depth | `SCS_DeviceDepth` | RGB8 | 0-1 normalized, easier to visualize |
| World Normal | `SCS_Normal` | RGB8 | **Deferred renderer only** -- we use Lumen (deferred), so OK |
| Base Color | `SCS_BaseColor` | RGB8 | **Deferred renderer only** -- albedo without lighting |
| Color + Depth | `SCS_SceneColorSceneDepth` | RGBA16F | Color in RGB, depth in A -- two-for-one |

**Important:** `SCS_Normal` and `SCS_BaseColor` require the deferred renderer. Leviathan uses Lumen GI (deferred shading) -- confirmed compatible.

### 7.2 Custom Stencil Buffer for Object Identification

**Enable:** Project Settings > Rendering > Postprocessing > Custom Depth-Stencil Pass = "Enabled with Stencil"

**Per-object tagging:**

```cpp
// Tag each building component with a stencil ID
MeshComponent->SetRenderCustomDepth(true);
MeshComponent->SetCustomDepthStencilValue(RoomID);  // 0-255
```

**Reading stencil in PP material:**
- SceneTexture node > CustomStencil
- Each pixel contains the stencil ID of the closest object

**Limitation:** 8-bit stencil = max 255 unique IDs. Fine for per-room tagging within a building (rarely > 50 rooms).

### 7.3 Custom Diagnostic Post-Process Materials

For analyses not covered by direct capture sources:

**Z-Fighting Detector:**

```hlsl
// Post-process material -- detects high-frequency depth oscillation
float Depth = SceneTextureLookup(UV, 1, false).r;  // Scene depth
float DepthDx = ddx(Depth);
float DepthDy = ddy(Depth);
float DepthVariance = abs(DepthDx) + abs(DepthDy);

// In flat regions, high variance = Z-fighting
float FlatMask = step(DepthVariance, FlatThreshold);  // 1 where depth is smooth
float LocalVariance = /* compute in neighborhood */;

float ZFighting = FlatMask * step(ZFightThreshold, LocalVariance);
return lerp(SceneColor, float3(1, 0, 0), ZFighting * 0.8);  // Red overlay
```

**Normal Discontinuity Detector:**

```hlsl
// Highlights sharp normal changes -- reveals face boundaries and topology errors
float3 Normal = SceneTextureLookup(UV, 8, false).rgb;  // WorldNormal
float3 NormalDx = ddx(Normal);
float3 NormalDy = ddy(Normal);
float Discontinuity = length(NormalDx) + length(NormalDy);

return lerp(SceneColor, float3(0, 1, 1), saturate(Discontinuity * Sensitivity));
```

### 7.4 Async Multi-Buffer Capture Pipeline

**Capture all buffers for one viewpoint in parallel:**

```cpp
struct FDiagnosticCapture
{
    USceneCaptureComponent2D* ColorCapture;
    USceneCaptureComponent2D* DepthCapture;
    USceneCaptureComponent2D* NormalCapture;

    UTextureRenderTarget2D* ColorRT;
    UTextureRenderTarget2D* DepthRT;
    UTextureRenderTarget2D* NormalRT;

    void SetupAtViewpoint(FVector Location, FRotator Rotation, float FOV)
    {
        auto ConfigureCapture = [&](USceneCaptureComponent2D* Comp,
                                     UTextureRenderTarget2D* RT,
                                     ESceneCaptureSource Source)
        {
            Comp->SetWorldLocationAndRotation(Location, Rotation);
            Comp->FOVAngle = FOV;
            Comp->TextureTarget = RT;
            Comp->CaptureSource = Source;
            Comp->bCaptureEveryFrame = false;
            Comp->bCaptureOnMovement = false;
            Comp->bRenderInMainRenderer = true;  // Efficient G-buffer reuse
        };

        ConfigureCapture(ColorCapture, ColorRT, SCS_FinalColorLDR);
        ConfigureCapture(DepthCapture, DepthRT, SCS_SceneDepth);
        ConfigureCapture(NormalCapture, NormalRT, SCS_Normal);
    }

    void CaptureAll()
    {
        ColorCapture->CaptureScene();
        DepthCapture->CaptureScene();
        NormalCapture->CaptureScene();
    }
};
```

---

## 8. Contact Sheet Stitching

### 8.1 In-Engine Approach (UE5 Render Target Compositing)

**Create a large render target and draw sub-images into it:**

```cpp
UTextureRenderTarget2D* ContactSheet = NewObject<UTextureRenderTarget2D>();
ContactSheet->InitAutoFormat(2048, 2048);  // 3x2 grid of 682x1024 tiles

UCanvas* Canvas;
FVector2D CanvasSize;
FDrawToRenderTargetContext Context;
UKismetRenderingLibrary::BeginDrawCanvasToRenderTarget(World, ContactSheet,
    Canvas, CanvasSize, Context);

// Draw each capture into its grid cell
int32 TileW = 682, TileH = 1024;
struct FTile { UTextureRenderTarget2D* RT; FString Label; int32 Col, Row; };
TArray<FTile> Tiles = {
    {TopDownRT, "Floor Plan", 0, 0},
    {EntranceRT, "Entrance", 1, 0},
    {NorthRT, "North", 2, 0},
    {SouthRT, "South", 0, 1},
    {EastRT, "East", 1, 1},
    {WestRT, "West", 2, 1}
};

for (const auto& Tile : Tiles)
{
    Canvas->DrawTile(Tile.Col * TileW, Tile.Row * TileH,
        TileW, TileH, 0, 0, 1, 1,
        FLinearColor::White, Tile.RT->GetResource(), BLEND_Opaque);

    // Draw label
    Canvas->DrawText(GEngine->GetSmallFont(), Tile.Label,
        Tile.Col * TileW + 5, Tile.Row * TileH + 5);
}

UKismetRenderingLibrary::EndDrawCanvasToRenderTarget(World, Context);

// Save to disk
FBufferArchive Buffer;
FImageUtils::ExportRenderTarget2DAsPNG(ContactSheet, Buffer);
FFileHelper::SaveArrayToFile(Buffer, *ContactSheetPath);
```

### 8.2 Python Post-Process Approach (Simpler)

```python
from PIL import Image

def create_contact_sheet(image_paths: dict, output_path: str,
                         tile_size=(512, 512), grid=(3, 2)):
    sheet_w = tile_size[0] * grid[0]
    sheet_h = tile_size[1] * grid[1]
    sheet = Image.new('RGB', (sheet_w, sheet_h), (32, 32, 32))

    layout = [
        ("Floor Plan", "color_topdown.png", 0, 0),
        ("Entrance",   "color_entrance.png", 1, 0),
        ("Wireframe",  "wireframe_topdown.png", 2, 0),
        ("North",      "color_elevation_n.png", 0, 1),
        ("East",       "color_elevation_e.png", 1, 1),
        ("South",      "color_elevation_s.png", 2, 1),
    ]

    from PIL import ImageDraw, ImageFont
    draw = ImageDraw.Draw(sheet)

    for label, filename, col, row in layout:
        img_path = image_paths.get(filename)
        if img_path and os.path.exists(img_path):
            tile = Image.open(img_path).resize(tile_size)
            sheet.paste(tile, (col * tile_size[0], row * tile_size[1]))

        # Draw label
        draw.text((col * tile_size[0] + 4, row * tile_size[1] + 4),
                  label, fill=(255, 255, 255))

    sheet.save(output_path)
    return output_path
```

**Recommendation:** Use the Python approach -- simpler, no render thread involvement, works offline, easy to customize layout.

---

## 9. Proposed Architecture

### 9.1 Complete Validation Pipeline

```
                    +-----------------------+
                    | generate_building()   |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | TIER 1: Geometry      |  < 100ms, always runs
                    | - Capsule sweep doors |
                    | - Ray grid walls      |
                    | - Floor continuity    |
                    | - Exterior integrity  |
                    | - Overlap detection   |
                    | - Mesh topology       |
                    +-----------+-----------+
                                |
                    Pass?  -----+-----> Done (80% of buildings)
                    No          |
                    +-----------v-----------+
                    | TIER 2: NavMesh +     |  1-3s, always runs
                    |   Spatial             |
                    | - NavMesh build       |
                    | - Path connectivity   |
                    | - Path width check    |
                    | - Stair angle check   |
                    +-----------+-----------+
                                |
                    Pass?  -----+-----> Done (15% more buildings)
                    No / Batch  |
                    +-----------v-----------+
                    | TIER 3: Vision        |  3-30s, on-demand
                    |                       |
                    | A) Multi-angle capture |
                    |    - 6-12 viewpoints  |
                    |    - Color+Depth+Norm |
                    |    - Contact sheet    |
                    |                       |
                    | B) CV analysis        |
                    |    - Edge alignment   |
                    |    - Window patterns  |
                    |    - Depth anomalies  |
                    |    - Floor plan CCA   |
                    |                       |
                    | C) VLM review         |
                    |    - Claude Sonnet 4  |
                    |    - Contact sheet    |
                    |    - Structured JSON  |
                    +-----------+-----------+
                                |
                    +-----------v-----------+
                    | Validation Report     |
                    | (JSON + annotated PNGs)|
                    +-----------------------+
                                |
                    +-----------v-----------+
                    | Auto-Fix (if possible)|
                    | - Widen doors         |
                    | - Deepen booleans     |
                    | - Flip normals        |
                    | - Spawn nav links     |
                    +-----------------------+
```

### 9.2 Debug Visualization Pipeline

```
Viewport Show Menu
    |
    +-- [x] Building Validation
    |       |
    |       +-- Room labels (Canvas text)
    |       +-- Door markers (green/red boxes)
    |       +-- Wall normals (cyan arrows)
    |       +-- Problem markers (red spheres)
    |       +-- Validation issue annotations
    |
    +-- [x] Room Type Colors
    |       |
    |       +-- Custom stencil per room
    |       +-- PP material color LUT
    |
    +-- [x] NavMesh Overlay
    |       |
    |       +-- Standard 'P' key toggle
    |       +-- DrawOffset = 5 for readability
    |
    +-- [x] Collision Vis
            |
            +-- CollisionPawn show flag
            +-- CollisionVisibility show flag
```

### 9.3 New MCP Actions

| Action | Tier | Description |
|--------|------|-------------|
| `capture_building_views` | 3A | Capture 6-12 views of building to disk (color, depth, normal, wireframe) |
| `create_contact_sheet` | 3A | Stitch captured views into diagnostic contact sheet |
| `validate_door_passability` | 1 | Capsule sweep through all doors, return pass/fail per door |
| `validate_room_connectivity` | 1 | Flood-fill from entrance, report disconnected rooms |
| `validate_wall_integrity` | 1 | Ray grid through walls, report holes and gaps |
| `validate_floor_continuity` | 1 | Ray grid down in rooms, report floor holes |
| `validate_exterior_shell` | 1 | Ray grid inward from exterior, report gaps |
| `validate_mesh_topology` | 1 | Check closed manifold, boundary edges, normals |
| `detect_overlapping_geometry` | 1 | AABB broad phase + ray narrow phase |
| `validate_building` | ALL | Orchestrator: runs Tier 1+2, optional Tier 3 |
| `validate_building_vision` | 3C | Send captures to VLM, return structured report |
| `analyze_captures_cv` | 3B | Run OpenCV analysis on captured images |
| `toggle_validation_overlay` | Viz | Toggle debug visualization show flag |
| `set_room_color_mode` | Viz | Apply room-type color overlay via stencil/materials |
| `fix_building_issues` | Fix | Auto-fix known issue types (widen doors, etc.) |

---

## 10. Effort Estimates

### Phase 1: Core Capture System (P0, 20-28h)

| Task | Hours |
|------|-------|
| Multi-viewpoint camera placement system | 6-8h |
| Multi-buffer capture (color/depth/normal/basecolor) | 4-6h |
| Async readback pipeline | 3-4h |
| PNG/EXR save to disk | 2-3h |
| Contact sheet generation (Python) | 2-3h |
| `capture_building_views` MCP action | 3-4h |

### Phase 2: Geometric Validation (P0, 16-24h)

| Task | Hours |
|------|-------|
| Capsule sweep door passability | 3-4h |
| Ray grid wall/floor/exterior checks | 4-6h |
| Room enclosure validation | 2-3h |
| Overlap detection (AABB + ray narrow phase) | 3-4h |
| Mesh topology checks | 2-3h |
| `validate_building` orchestrator | 2-4h |

### Phase 3: Debug Visualization (P0, 18-24h)

| Task | Hours |
|------|-------|
| Custom show flag + UDebugDrawService registration | 3-4h |
| Room label rendering | 2-3h |
| Door/wall/problem marker rendering | 3-4h |
| Room type colorization (stencil + PP) | 4-6h |
| NavMesh overlay toggle | 1-2h |
| Collision vis toggle | 1-2h |
| Diagnostic PP materials (Z-fight, normal discontinuity) | 4-6h |

### Phase 4: CV + VLM (P1-P2, 26-36h)

| Task | Hours |
|------|-------|
| OpenCV wall alignment analysis | 3-4h |
| Window pattern template matching | 3-4h |
| Connected component floor plan analysis | 3-4h |
| Depth buffer anomaly detection | 3-4h |
| Python validation pipeline integration | 2-3h |
| VLM prompt engineering + testing | 4-6h |
| VLM integration (Claude API call, response parsing) | 4-6h |
| `validate_building_vision` + `analyze_captures_cv` MCP | 4-6h |

**Total: 80-112 hours across 4 phases.**

---

## 11. Sources

### UE5 SceneCapture and G-Buffer
- [Post Process Materials - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/post-process-materials-in-unreal-engine)
- [USceneCaptureComponent2D - UE 5.7 API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/USceneCaptureComponent2D)
- [Scene Capture - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Rendering/SceneCapture)
- [UnrealImageCapture - GitHub](https://github.com/TimmHess/UnrealImageCapture) -- comprehensive tutorial on multi-buffer capture
- [Asynchronously Reading Render Targets Using Fences](https://nicholas477.github.io/blog/2023/reading-rt/)
- [Custom Stencil Notes - Gamedev Guide](https://ikrima.dev/ue4guide/graphics-development/custom-passes-extensions/custom-stencil-notes/)
- [FImageUtils - UE 5.7 API](https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Runtime/Engine/FImageUtils)
- [Viewport Modes - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-modes-in-unreal-engine)

### Debug Visualization
- [Custom Visualization Component and Show Flags - Epic Community Tutorial](https://dev.epicgames.com/community/learning/tutorials/XaE8/unreal-engine-custom-visualization-component-and-show-flags)
- [UDebugDrawService Notes - zomgmoz](https://zomgmoz.tv/unreal/UDebugDrawService)
- [Custom Show Flag Notes - hzfishy](https://notes.hzfishy.fr/Unreal-Engine/Editor-Only/Editor-Customization/Custom-show-flag)
- [Easier Editor Debug Visualization - itsBaffled](https://itsbaffled.github.io/posts/UE/Easier-Editor-Debug-Visualization)
- [Component Visualizers - QuodSoler](https://www.quodsoler.com/blog/unreal-engine-component-visualizers-unleashing-the-power-of-editor-debug-visualization)
- [Viewport Show Flags - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/viewport-show-flags-in-unreal-engine)

### VLM Game QA Research
- [VideoGameQA-Bench (NeurIPS 2025)](https://asgaardlab.github.io/videogameqa-bench/) -- 9 tasks, 4,786 questions, 16 VLMs
- [VideoGameQA-Bench Paper - arxiv](https://arxiv.org/abs/2505.15952)
- [VideoGameQA-Bench - Sony Interactive](https://sonyinteractive.com/en/innovation/research-academia/research/vision-language-models-for-quality-assurance/)
- ["How Far Can VLMs Go for Visual Bug Detection?" (FSE 2026)](https://arxiv.org/abs/2603.22706) -- 41h gameplay, 19,738 keyframes
- [Automated Bug Frame Retrieval from Gameplay Videos](https://arxiv.org/html/2508.04895v1)
- [VideoGameBunny - Towards Vision Assistants for Video Games](https://www.researchgate.net/publication/390616743_Videogamebunny_Towards_Vision_Assistants_for_Video_Games)

### VLM Prompt Engineering
- [VLM Prompt Engineering Guide - NVIDIA Technical Blog](https://developer.nvidia.com/blog/vision-language-model-prompt-engineering-guide-for-image-and-video-understanding/)
- [VLM Prompt Engineering Guide - Edge AI Vision Alliance](https://www.edge-ai-vision.com/2025/03/vision-language-model-prompt-engineering-guide-for-image-and-video-understanding/)
- [VLMs for Engineering Design - Springer AI Review 2025](https://link.springer.com/article/10.1007/s10462-025-11290-y)
- [Systematic Survey of Prompt Engineering on VLMs - GitHub](https://github.com/JindongGu/Awesome-Prompting-on-Vision-Language-Model/)

### Claude Vision Pricing
- [Claude Vision Documentation](https://platform.claude.com/docs/en/build-with-claude/vision) -- token calculation: `tokens = (width * height) / 750`
- [Claude API Pricing](https://platform.claude.com/docs/en/about-claude/pricing)
- [Claude Sonnet 4.6 Pricing Analysis](https://pricepertoken.com/pricing-page/model/anthropic-claude-sonnet-4.6)

### Mesh Validation Algorithms
- [Robust Hole-Detection in Triangular Meshes - arxiv 2311.12466](https://arxiv.org/abs/2311.12466)
- [Detection of Holes in 3D Architectural Models - ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1877050920308450)
- [Finding Surface Overlap - Sandia CUBIT](https://www.sandia.gov/files/cubit/15.4/help_manual/WebHelp/geometry/cleanup_and_defeaturing/finding_surface_overlap.htm)
- [Optimized Spatial Hashing for Collision Detection](https://matthias-research.github.io/pages/publications/tetraederCollision.pdf)
- [Graph Learning for Intersecting 3D Geometry Classification - Springer 2024](https://link.springer.com/chapter/10.1007/978-3-031-78166-7_10)

### Computer Vision / OpenCV
- [OpenCV Template Matching Tutorial](https://docs.opencv.org/4.x/d4/dc6/tutorial_py_template_matching.html)
- [OpenCV Hough Line Transform](https://docs.opencv.org/3.4/d9/db0/tutorial_hough_lines.html)
- [OpenCV Canny Edge Detection](https://docs.opencv.org/4.x/da/d22/tutorial_py_canny.html)
- [OpenCV Connected Component Labeling - PyImageSearch](https://pyimagesearch.com/2021/02/22/opencv-connected-component-labeling-and-analysis/)
- [Edge-Based Template Matching - GitHub](https://github.com/IOL0ol1/Edge-Based-Template-Matching)
- [Floor Plan Room Segmentation (U-Net) - GitHub](https://github.com/ozturkoktay/floor-plan-room-segmentation)
- [DeepFloorplan - GitHub](https://github.com/zlzeng/DeepFloorplan)

### Collision Detection
- [Video Game Physics Part II: Collision Detection - Toptal](https://www.toptal.com/developers/game/video-game-physics-part-ii-collision-detection-for-solid-objects)
- [Collision Detection and Spatial Indexes](https://kortham.net/posts/collision-detect-and-spatial-indexes/)
- [Capsule Trace By Channel - UE 5.7 Documentation](https://dev.epicgames.com/documentation/en-us/unreal-engine/BlueprintAPI/Collision/CapsuleTraceByChannel)
