# Kit Scanner UX Research: Scan -> Classify -> Build Pipeline

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** asset-scanning-research, modular-building-research, modular-pieces-generation, proc-mesh-caching

---

## Executive Summary

This document covers the user experience design for the scan-classify-build pipeline -- the "killer feature" that lets users point at a folder of modular meshes and immediately start generating procedural buildings from them via natural language conversation. Research covers: ideal MCP conversation flow, classification review/correction patterns, error handling, kit completeness reporting, multi-kit support, proxy-to-real swap, persistence, and lessons from existing tools (Houdini HDAs, Dungeon Architect themes, Procedural World Lab city kits, TrenchBroom WADs, Unity ProBuilder, UE5 PCG framework, WFC editors).

The core UX principle: **progressive disclosure with graceful degradation**. Scan fast, show results immediately, let the user correct, then build. Never block on missing pieces -- always have a fallback.

---

## 1. The Ideal MCP Conversation Flow

### 1.1 Design Principles from MCP UX Research

The Klavis "Less is More" pattern for MCP servers applies directly here. Key insights from MCP design pattern research (2025-2026):

- **Context window is the scarcest resource.** Don't dump 47 mesh classifications at once. Summarize, then let the user drill in.
- **Progressive disclosure over front-loading.** Show category counts first, details on request.
- **Structured summaries over wall-of-text.** Tables beat paragraphs for scan results.
- **Graceful degradation over hard failures.** Missing pieces get fallback plans, not error messages.
- **Conversational repair over re-invocation.** If the user says "that shelf is actually a wall," update in-place rather than re-scanning.

### 1.2 The Golden Path: 3-Turn Conversation

**Turn 1: Scan command + immediate summary**

```
User: "scan /Game/HorrorKit/Meshes/ as a modular building kit"
```

Response structure (what the LLM presents, assembled from tool output):

```
Scanned 47 meshes in /Game/HorrorKit/Meshes/

  Structure (27 pieces):
    12 wall_solid        200x300cm
     4 wall_window       200x300cm  (opening 80x120 at sill 90cm)
     3 wall_door         200x300cm  (opening 100x230)
     2 wall_corner_outer 200x200x300cm
     1 wall_corner_inner 200x200x300cm
     2 floor_tile        200x200cm
     2 ceiling_tile      200x200cm
     1 stair_straight    200x300cm

  Furniture (14 pieces):
    3 table variants
    4 chair variants
    2 shelf variants
    5 misc props

  Uncertain (6 pieces, flagged for review):
    SM_Panel_Large       -> wall_solid? (confidence: 0.62)
    SM_Shelf_Tall        -> furniture? (confidence: 0.58)
    SM_Frame_Decorative  -> furniture? (confidence: 0.51)
    SM_Arch_001          -> wall_door? (confidence: 0.55)
    SM_Block_Floor       -> floor_tile? (confidence: 0.60)
    SM_Railing_Section   -> prop? (confidence: 0.49)

  Grid: 200cm (detected from wall widths)
  Kit saved: "HorrorKit"

  Coverage: 78% of required piece types
  Missing: wall_t_junction, wall_half, stair_landing, stair_l_turn
  Fallback: GeometryScript will generate missing pieces at build time

  Review the uncertain items? Or start building?
```

**Design rationale:** The response is scannable (indented categories, counts, dimensions). Uncertain items are separated and explicitly flagged. Coverage percentage gives an instant quality read. The question at the end drives the conversation forward -- the user chooses review or build, never gets stuck.

**Turn 2a (optional): Review corrections**

```
User: "SM_Panel_Large is wall_solid. SM_Shelf_Tall is wall_solid too, it's
       a damaged wall section. SM_Arch_001 is wall_door. Accept the rest."
```

Response:

```
Updated 3 classifications:
  SM_Panel_Large       wall_solid (was: uncertain)
  SM_Shelf_Tall        wall_solid (was: furniture)
  SM_Arch_001          wall_door  (was: uncertain)
  SM_Frame_Decorative  furniture  (confirmed)
  SM_Block_Floor       floor_tile (confirmed)
  SM_Railing_Section   prop       (confirmed)

Kit "HorrorKit" updated. Coverage now 82%.
Corrections saved -- future scans of this kit will remember these overrides.
```

**Turn 2b (skip review): Jump to building**

```
User: "generate a 4-building horror block using HorrorKit"
```

**Turn 3: Build with kit**

```
Generating 4-building block with HorrorKit (200cm grid)...

  Building 1: 2-story house (6 rooms)
    -> 48 wall pieces, 12 floor tiles, 8 ceiling tiles, 2 doors, 3 windows
  Building 2: 1-story clinic (4 rooms)
    -> 32 wall pieces, 8 floor tiles, 4 ceiling tiles, 3 doors, 2 windows
  Building 3: 2-story apartment (8 rooms)
    -> 64 wall pieces, 16 floor tiles, 12 ceiling tiles, 4 doors, 6 windows
  Building 4: 1-story store (2 rooms)
    -> 18 wall pieces, 4 floor tiles, 2 ceiling tiles, 1 door, 2 windows

  Generated pieces (fallback): 4 wall_t_junctions, 2 stair_landings
  Total actors: 4 buildings, 287 instances (HISM)
  Placed in: /HorrorBlock/ outliner folder
```

### 1.3 MCP Tool Output Format

The `scan_modular_kit` action returns structured JSON. The LLM formats it conversationally. The JSON structure should be designed for easy LLM summarization:

```json
{
  "kit_name": "HorrorKit",
  "scan_path": "/Game/HorrorKit/Meshes/",
  "total_meshes": 47,
  "grid_size_cm": 200,
  "grid_detection_confidence": 0.92,
  "categories": {
    "wall_solid": { "count": 12, "dimensions": "200x300cm", "pieces": [...] },
    "wall_window": { "count": 4, "dimensions": "200x300cm", "opening": {...}, "pieces": [...] },
    ...
  },
  "uncertain": [
    { "asset": "SM_Panel_Large", "predicted": "wall_solid", "confidence": 0.62, "reason": "tall+flat but name unclear" },
    ...
  ],
  "coverage": {
    "percentage": 78,
    "present": ["wall_solid", "wall_window", "wall_door", ...],
    "missing": ["wall_t_junction", "wall_half", ...],
    "fallback_plan": { "wall_t_junction": "geometry_script", "wall_half": "geometry_script", ... }
  },
  "kit_path": "Saved/Monolith/ModularKits/HorrorKit.json"
}
```

**Key design decision:** The tool returns the full structured data. The LLM decides how to present it based on conversation context. If the user asked for a quick scan, summarize. If they asked for details on windows, expand that section. This follows the MCP principle of letting the model interpret structured data rather than the tool formatting for humans.

### 1.4 Progressive Detail Levels

The LLM should present information at increasing detail based on user interest:

| Level | Trigger | Content |
|-------|---------|---------|
| L0 - Summary | Default scan result | Category counts, grid, coverage % |
| L1 - Category | "show me the wall pieces" | Individual piece names, dims, material count |
| L2 - Piece | "tell me about SM_Wall_Window_02" | Full dims, material slots, socket info, opening size, tri count, pivot, thumbnail |
| L3 - Debug | "why was SM_Shelf classified as furniture?" | All 5 signal scores, individual weights, what would change the classification |

This maps directly to the Klavis Strata pattern -- the LLM navigates down the information hierarchy based on user intent, never dumping everything at once.

---

## 2. Classification Review and Correction UX

### 2.1 Confidence-Based Routing (Human-in-the-Loop Pattern)

Research on HITL UX patterns reveals a well-established three-tier routing model:

| Confidence | Action | UX |
|-----------|--------|-----|
| > 0.80 | Auto-accept | Shown in summary, no flag |
| 0.50 - 0.80 | Flag for review | Listed in "Uncertain" section with predicted type |
| < 0.50 | Unclassified | Listed as "Unknown" -- user must assign |

**Threshold tuning:** These thresholds should be adjustable per-kit. A kit with very consistent naming (all starting with `SM_Wall_`, `SM_Floor_`) will have high confidence across the board. A kit with generic names (`SM_Piece_001`) will have low confidence. The 0.50/0.80 defaults are based on common ML review dashboard patterns.

**Confidence display:** Show as a simple descriptor (high/medium/low) not a raw number. Users don't know what 0.62 means. "SM_Panel_Large -> probably wall_solid (medium confidence, dimensions match but name doesn't)" is more useful than "0.62."

### 2.2 Correction Actions

The `edit_kit_classification` action supports several correction modes:

```
// Single correction
User: "SM_Shelf_Tall is actually wall_solid"
-> edit_kit_classification({ kit: "HorrorKit", asset: "SM_Shelf_Tall", new_type: "wall_solid" })

// Batch correction
User: "all the SM_Panel pieces are wall_solid"
-> edit_kit_classification({ kit: "HorrorKit", pattern: "SM_Panel*", new_type: "wall_solid" })

// Accept all uncertain as predicted
User: "accept all uncertain"
-> edit_kit_classification({ kit: "HorrorKit", accept_uncertain: true })

// Reclassify with reason (helps learning)
User: "SM_Shelf_Tall is a damaged wall, not furniture"
-> edit_kit_classification({ kit: "HorrorKit", asset: "SM_Shelf_Tall", new_type: "wall_solid",
                             note: "damaged wall variant" })
```

### 2.3 Override Persistence and Learning

**Per-kit overrides file:** Corrections are stored in the kit JSON:

```json
{
  "overrides": {
    "SM_Shelf_Tall": {
      "auto_classified": "furniture",
      "user_corrected": "wall_solid",
      "note": "damaged wall section",
      "corrected_at": "2026-03-28T14:30:00Z"
    }
  }
}
```

**Re-scan behavior:** When `scan_modular_kit` is called again for the same path, overrides take precedence over auto-classification. New meshes get auto-classified, but previously-corrected meshes keep their user assignment.

**Cross-kit learning (deferred, not Phase 1):** A pattern like "tall thin flat meshes named Shelf in horror kits are often wall variants" could inform future kits. This is a V2 feature -- for now, overrides are per-kit only. The override data provides training signal if we ever want to improve the classifier.

### 2.4 Comparison: How Other Tools Handle Classification Review

| Tool | Classification | Review UX | Learning |
|------|---------------|-----------|----------|
| Dungeon Architect | Manual theme mapping (drag mesh to marker) | Theme editor graph -- visual, immediate preview | None -- fully manual |
| Procedural World Lab | Manual assignment in "Building Blocks" panel | Dropdown per slot type | None |
| Houdini HDA | Artist promotes parameters manually | Parameter interface editor | None |
| PCG Framework | Manual Static Mesh Spawner population | Drag from content browser into spawner entries | None |
| TrenchBroom | Auto-load by filename from WAD | Name conflicts: last loaded wins | None |
| **Monolith (proposed)** | **Auto-classify with 5-signal pipeline** | **Conversational correction, confidence flags** | **Per-kit overrides, re-scan respects** |

Monolith is the only tool in this space that auto-classifies AND has a correction loop. Every other tool requires fully manual assignment. This is the competitive advantage.

---

## 3. Error Handling and Edge Cases

### 3.1 Design Philosophy: Graceful Degradation, Not Hard Failure

From agentic UX research: "The quality of an AI product is measured not by how often the agent succeeds but by how gracefully it handles failure." Every error case below has a fallback plan AND a clear message.

### 3.2 Edge Case Matrix

| Scenario | Detection | Fallback | User Message |
|----------|-----------|----------|-------------|
| **No door pieces** | Kit coverage check: `wall_door` count = 0 | GeometryScript generates door-wall (wall + boolean cutout) | "Kit has no door pieces. Will generate door openings in solid walls at build time." |
| **No floor pieces** | `floor_tile` count = 0 | Generate simple planar floor at grid size | "No floor pieces found. Will generate flat floors. You can add floor meshes later with swap_proxies." |
| **No ceiling pieces** | `ceiling_tile` count = 0 | Skip ceilings (common in some kits) OR reuse floor flipped | "No ceiling pieces. Buildings will be open-top. Add ceiling meshes or use floor pieces flipped?" |
| **No corner pieces** | `wall_corner_*` count = 0 | Butt-join walls (overlap at corners, hidden by wall thickness) | "No corner pieces. Walls will overlap at corners -- works fine for most visual styles." |
| **Ambiguous grid** | GCD of wall widths produces multiple candidates (e.g., 100cm and 200cm both fit) | Pick most common dimension, flag in response | "Grid could be 100cm or 200cm. Using 200cm (12 of 16 walls match). Override with: edit_kit grid_size 100" |
| **Mixed grid sizes** | Wall widths don't share a clean GCD | Pick dominant grid, classify outliers as "oversized" or "half" pieces | "Most pieces are 200cm grid. 3 pieces are 400cm (double-width). 2 pieces are 100cm (half-width). Using 200cm base grid." |
| **Piece too small** | All dimensions < grid_size * 0.3 | Classify as prop/decoration | "SM_Bracket_Small (15x8x20cm) is too small for a structural piece. Classified as prop." |
| **Piece too large** | One dimension > grid_size * 3 | Could be a room module or multi-cell piece | "SM_Room_Complete (600x400x300cm) is 3x2 grid cells. Treating as a room module -- it will be placed as-is, not tiled." |
| **Non-standard pivot** | Bounding box center offset from origin > 10% of size | Record offset, compensate during placement | "SM_Wall_001 has an off-center pivot (bottom-left instead of center). Will compensate during placement." |
| **Folder is empty** | 0 meshes found at path | Clear error with suggestion | "No static meshes found in /Game/HorrorKit/. Check the path? Or scan a subfolder?" |
| **Folder has non-meshes** | Filter finds Blueprints, Materials, Textures but no StaticMeshes | Ignore non-meshes, report | "Found 23 assets in folder but only 3 are Static Meshes. The rest are materials and textures (ignored)." |
| **Already scanned** | Kit JSON exists for this path | Offer re-scan or load existing | "HorrorKit was scanned before (2 days ago, 47 meshes). Re-scan to check for changes, or use existing?" |
| **Assets modified since scan** | File timestamps newer than kit JSON | Flag stale kit | "3 meshes in HorrorKit were modified since last scan. Re-scan recommended." |

### 3.3 Error Message Tone

Following Monolith's conversational style, error messages should be:
- **Direct and specific** -- "No floor pieces found" not "An issue was encountered with the kit configuration"
- **Actionable** -- Always suggest what to do next
- **Not alarming** -- Missing pieces are normal, not errors. Frame as "will use fallback" not "MISSING REQUIRED PIECE"
- **Context-aware** -- If the kit has 40 wall variants and zero floors, that's probably an exterior-only kit. Say so.

### 3.4 The "Good Enough" Kit

Not every kit needs every piece type. Define quality tiers:

| Tier | Requirement | Coverage | Build Quality |
|------|-------------|----------|---------------|
| **Full Kit** | wall_solid + wall_door + wall_window + floor + ceiling + corners + stairs | 95-100% | Production-ready |
| **Structural Kit** | wall_solid + wall_door + floor | 60-70% | Playable with GS fallbacks |
| **Wall Kit** | wall_solid only | 30-40% | Exterior shell only, GS interiors |
| **Prop Kit** | furniture/props only | N/A for structure | Populates interiors built with GS |
| **Facade Kit** | wall variants with visual detail but no structural roles | 40-50% | Exterior facades over GS structure |

Detecting which tier a kit falls into shapes the user message. A "Wall Kit" shouldn't get scolded for missing floors -- it should be recognized: "This looks like an exterior wall kit. Great for facades -- interiors will use GeometryScript."

---

## 4. Kit Completeness Report

### 4.1 The Coverage Score

The coverage score is a percentage of "buildable actions" the kit can handle without fallback. Weight by frequency of use:

| Piece Type | Frequency Weight | Notes |
|-----------|-----------------|-------|
| wall_solid | 0.25 | Most-placed piece in any building |
| floor_tile | 0.15 | Every room needs a floor |
| wall_door | 0.12 | Every room needs at least one |
| wall_window | 0.10 | Exterior walls mostly |
| ceiling_tile | 0.10 | Optional but expected |
| wall_corner_outer | 0.08 | Every exterior corner |
| wall_corner_inner | 0.05 | Interior corners |
| stair_straight | 0.05 | Multi-story buildings |
| wall_t_junction | 0.04 | Where interior wall meets exterior |
| wall_half | 0.03 | Odd-width rooms |
| stair_landing | 0.02 | Multi-floor stairs |
| stair_l_turn | 0.01 | L-shaped stairs |

**Formula:** `coverage = sum(weight * has_piece) / sum(all_weights) * 100`

A kit with wall_solid + floor + wall_door + wall_window + ceiling scores: (0.25 + 0.15 + 0.12 + 0.10 + 0.10) / 1.0 = 72%.

### 4.2 Completeness Report Format

```
Kit Completeness: HorrorKit
------------------------------
Coverage: 78% (HIGH)

  PRESENT:
    wall_solid          12 variants  (excellent variety)
    wall_window          4 variants  (good)
    wall_door            3 variants  (good)
    wall_corner_outer    2 variants
    wall_corner_inner    1 variant
    floor_tile           2 variants
    ceiling_tile         2 variants
    stair_straight       1 variant

  MISSING (with fallback):
    wall_t_junction    -> will combine corner + straight wall
    wall_half          -> will scale wall_solid to half width
    stair_landing      -> GeometryScript flat platform
    stair_l_turn       -> GeometryScript L-stair

  VARIETY NOTES:
    wall_solid has 12 variants -- buildings will have excellent visual variety
    floor_tile has only 2 variants -- may look repetitive in large rooms
    stair_straight has only 1 variant -- all stairs will look identical

  ESTIMATED BUILD QUALITY: HIGH
    Structure: kit-quality meshes (art-ready)
    Fallbacks: 4 pieces will be GeometryScript (blockout-quality)
    Recommendation: Add 2-3 more floor variants for best results
```

### 4.3 Variety Analysis

Beyond presence/absence, variety per type matters for visual quality:

| Variants | Rating | Effect |
|----------|--------|--------|
| 1 | Minimal | Obvious repetition |
| 2-3 | Adequate | Noticeable but acceptable |
| 4-6 | Good | Varied appearance |
| 7+ | Excellent | No obvious repetition |

The report should flag types with only 1 variant ("all stairs look identical") and celebrate types with many variants ("excellent wall variety").

---

## 5. Multi-Kit Support

### 5.1 The Multi-Kit Use Case

Real projects mix asset sources:
- Horror structural kit for walls and architecture
- Modern furniture pack for interiors
- Custom props for unique story items
- GeometryScript fallback for anything missing

### 5.2 Kit Layering Architecture

```
User: "use HorrorKit for structure, ModernFurniture for interiors"
```

Implementation: `build_with_kit` accepts a kit stack:

```json
{
  "kit_stack": [
    { "kit": "HorrorKit", "roles": ["structure", "exterior"] },
    { "kit": "ModernFurniture", "roles": ["furniture", "props"] },
    { "kit": "__geometry_script__", "roles": ["fallback"] }
  ]
}
```

**Resolution order:**
1. For structural pieces (walls, floors, ceilings, stairs): use first kit in stack that has the piece type
2. For furniture: use first kit with matching furniture category
3. For anything missing: fall through to GeometryScript

**Conflict resolution:** If both kits have `wall_solid`, the first kit in the stack wins. User can override: "use ModernFurniture walls for building 3."

### 5.3 Kit Compatibility Checks

When combining kits, check:

| Check | Severity | Action |
|-------|----------|--------|
| Grid size mismatch | Warning | "HorrorKit is 200cm grid, ModernFurniture is 100cm. Furniture will be placed at 100cm intervals within 200cm rooms." |
| Wall height mismatch | Warning | "HorrorKit walls are 300cm, SteamPunkKit walls are 350cm. Using 300cm (HorrorKit) -- SteamPunkKit pieces may clip at top." |
| Style mismatch | Info only | "Mixing horror and modern furniture -- intentional?" |
| Scale mismatch | Error | "TinyKit pieces are 1/10th scale of HorrorKit. Something's wrong -- check import scale?" |

### 5.4 Kit Registry

List all available kits:

```
User: "list my kits"

Available Kits:
  HorrorKit           47 pieces  200cm grid  78% coverage  scanned 2h ago
  ModernFurniture     23 pieces  100cm grid  props only    scanned 1d ago
  SteamPunkKit        31 pieces  200cm grid  65% coverage  scanned 3d ago
  __geometry_script__  (built-in fallback, generates any piece type)
```

---

## 6. Proxy-to-Real Asset Swap

### 6.1 The Blockout-First Workflow

This maps to the industry-standard level design pipeline:
1. **Blockout Pass:** Generate with simple geometry (fast, cheap, editable)
2. **Playtest Pass:** Walk through, test gameplay, adjust layout
3. **Art Pass:** Swap blockout meshes for final art

Monolith's `swap_proxies` action automates step 3.

### 6.2 Swap Matching Algorithm

When swapping proxies for kit meshes, match by:

1. **Piece type** (primary) -- a proxy `wall_solid` becomes a kit `wall_solid`
2. **Dimensions** (secondary) -- closest dimension match within type
3. **Variant randomization** (tertiary) -- if the kit has 12 wall_solid variants, distribute randomly for visual variety

```
Matching algorithm:
  For each proxy actor:
    1. Read piece_type from actor tag (set during generation)
    2. Find all kit pieces of that type
    3. Filter to pieces within 10% dimension tolerance
    4. If multiple matches: pick randomly (weighted by distance from last use for variety)
    5. If no match: keep proxy, flag in report
```

### 6.3 Grid Size Mismatch Handling

If the proxy was built on a 50cm grid (Monolith default) and the kit is 200cm grid:

- **Option A (recommended): Re-generate at kit grid size.** Fastest, cleanest result.
- **Option B: Scale proxies.** 50cm proxy scaled 4x to match 200cm kit. Loses layout precision.
- **Option C: Tile kit pieces within proxy slots.** Place 4 kit pieces per proxy slot. Only works if kit grid divides evenly into proxy grid.

The swap action should detect the mismatch and recommend Option A:

```
Grid mismatch: proxies are 50cm grid, HorrorKit is 200cm grid.
Recommend: re-generate the layout at 200cm grid for best results.
  -> mesh_query("create_building", { ..., grid_size: 200, kit: "HorrorKit" })
Or force swap anyway (may have alignment issues).
```

### 6.4 Swap Report

```
Swap complete: 287 instances updated

  Swapped (kit mesh):     241 pieces
  Swapped (GS fallback):   38 pieces (wall_t_junction, stair_landing)
  Kept as proxy:             8 pieces (no match found)

  Kept proxies:
    3x floor_half_200x100 -- kit has no half-floor, keep or generate?
    5x wall_angled_30deg  -- kit has no angled walls, keep or generate?
```

---

## 7. Kit Persistence

### 7.1 Kit JSON Format

Stored at `Saved/Monolith/ModularKits/{KitName}.json`:

```json
{
  "version": 1,
  "kit_name": "HorrorKit",
  "scan_path": "/Game/HorrorKit/Meshes/",
  "scanned_at": "2026-03-28T14:00:00Z",
  "grid_size_cm": 200,
  "grid_detection": {
    "method": "gcd_of_wall_widths",
    "confidence": 0.92,
    "candidates": [200, 100],
    "selected": 200,
    "reason": "12 of 16 walls match 200cm"
  },
  "pieces": [
    {
      "asset_path": "/Game/HorrorKit/Meshes/SM_Wall_Solid_01",
      "type": "wall_solid",
      "classification_source": "auto",
      "confidence": 0.94,
      "dimensions": { "x": 200, "y": 20, "z": 300 },
      "pivot": "bottom_center",
      "pivot_offset": { "x": 0, "y": 0, "z": 0 },
      "tri_count": 128,
      "material_slots": ["MI_Brick_Damaged", "MI_Trim_Wood"],
      "sockets": [],
      "openings": [],
      "tags": ["damaged", "exterior"]
    },
    ...
  ],
  "overrides": {
    "SM_Shelf_Tall": {
      "auto_classified": "furniture",
      "user_corrected": "wall_solid",
      "note": "damaged wall section",
      "corrected_at": "2026-03-28T14:30:00Z"
    }
  },
  "coverage": {
    "percentage": 82,
    "present": ["wall_solid", "wall_window", "wall_door", ...],
    "missing": ["wall_t_junction", "wall_half"],
    "tier": "structural_kit"
  }
}
```

### 7.2 Staleness Detection

On load, check if source assets have been modified:

```cpp
// For each piece in the kit
FAssetData AssetData = AR.GetAssetByObjectPath(PiecePath);
FDateTime AssetModTime = IFileManager::Get().GetTimeStamp(
    FPackageName::LongPackageNameToFilename(PiecePath, TEXT(".uasset")));

if (AssetModTime > Kit.ScannedAt)
{
    StaleAssets.Add(PiecePath);
}
```

**UX on stale kit:**

```
Loading HorrorKit... 3 meshes modified since last scan.
  SM_Wall_Solid_03  (modified 4h ago)
  SM_Wall_Door_02   (modified 4h ago)
  SM_Floor_01       (modified 1d ago)

Re-scan to update? (overrides will be preserved)
```

### 7.3 Kit Versioning

The kit JSON includes a `version` field for schema changes. When Monolith updates the kit format (adding new classification signals, new piece types), old kit files are migrated on load:

- Version 1: Initial format (current)
- Version 2 (future): Add socket-based snap rules
- Version 3 (future): Add WFC adjacency constraints

Migration is automatic and silent -- no user action required.

---

## 8. Lessons from Existing Tools

### 8.1 Houdini Digital Assets (HDAs)

**What they do well:**
- **Progressive parameter exposure.** HDAs hide internal complexity behind a curated parameter interface. Artists see "Wall Height" and "Window Count," not the 200 nodes inside. Monolith should do the same -- the scan result shows piece types and counts, not classification signal weights.
- **Hide When / Disable When.** Parameters appear/disappear based on context. If the user sets `style: modern`, medieval-only parameters hide. Monolith's kit output should similarly omit irrelevant sections (don't show stair stats for a prop-only kit).
- **Multiparm blocks.** Users can add/remove items in a list. The `edit_kit_classification` action is our version of this -- add/remove/change piece classifications.
- **Live preview.** As you change HDA parameters, the viewport updates. Monolith should ideally show the kit's pieces in-viewport during review (future feature).

**What they don't do:**
- No auto-classification. Users manually wire every input. That's the gap we fill.

### 8.2 Dungeon Architect (Unreal)

**What they do well:**
- **Marker system for asset mapping.** The layout generates abstract markers (Ground, Wall, Door), then the theme engine maps meshes to markers. This is exactly our piece-type system.
- **Theme swapping.** Change the theme asset, get a completely different look with the same layout. Our kit-swap via `swap_proxies` achieves the same thing.
- **Visual theme editor.** Drag-and-drop mesh assignment with live preview. Purely manual but very intuitive for artists.

**What they don't do:**
- No auto-detection of piece types from mesh properties. Fully manual marker assignment.
- No fallback generation for missing markers. If a marker has no mesh mapped, it's just empty.
- No confidence scores or review flow.

**Lesson for Monolith:** Dungeon Architect proves the marker/theme abstraction works. Our innovation is automating the mesh-to-marker mapping step that DA requires manually.

### 8.3 Procedural World Lab / iPCC

**What they do well:**
- **Multi-stage generation.** Roads -> blocks -> buildings -> interiors, each stage customizable. Artists can tweak at any stage without losing downstream work.
- **Custom mesh support.** Documented as "design buildings from A to Z with your own custom meshes."
- **Grammar-based building generation.** Rules define how pieces combine.
- **City kit ecosystem.** Third-party artists can create compatible kits.

**What they don't do:**
- No auto-scanning. Artists manually assign meshes to building block slots.
- Kit creation requires following a specific format/naming convention.
- No fallback for missing pieces.

**Lesson for Monolith:** The kit ecosystem concept is valuable. If Monolith kits were shareable JSON files, users could share kit definitions for popular marketplace packs. "Here's my HorrorKit.json for the Fab Horror Architecture pack -- just drop it in your ModularKits folder."

### 8.4 TrenchBroom

**What they do well:**
- **Drop-to-load.** Drag a WAD file onto the window, textures are available immediately. Zero configuration for the common case.
- **Auto-read by filename.** Textures match by name, no manual mapping needed.
- **Override order for conflicts.** Last loaded WAD wins name conflicts. Simple, predictable.

**What they don't do:**
- No auto-classification beyond filename.
- No quality/coverage reporting.
- WAD-specific (not general mesh libraries).

**Lesson for Monolith:** The drop-to-scan UX is the gold standard for first interaction. "Point at folder, get results" should be that simple. We should support asset path completion in the conversation -- typing `/Game/` should let the user browse.

### 8.5 UE5 PCG Framework

**What they do well:**
- **Attribute-driven selection.** PCG points carry metadata that drives mesh selection. Our kit pieces carry type/dimensions that drive placement decisions.
- **Weight-based spawning.** Static Mesh Spawner supports weights per mesh entry. Our variant randomization should be similarly weighted.

**What they don't do:**
- No auto-scanning or classification. Manual drag-and-drop into spawner nodes.
- No completeness checking.
- No fallback generation.

### 8.6 WFC Editors

**What they do well:**
- **Adjacency rules from examples.** Some WFC tools auto-extract adjacency constraints from a sample tilemap. Monolith could potentially auto-extract "these wall types were placed adjacent to these floor types" from existing levels.
- **Interactive constraint editing.** GUI tools let users paint which tiles can be neighbors. Our `edit_kit_classification` is a simpler version of this.

**What they don't do:**
- No auto-detection of tile types from mesh properties.
- Adjacency rules are the hard part -- usually manual.

---

## 9. Conversation Flow Variants

### 9.1 Power User: One-Shot Build

```
User: "scan /Game/HorrorKit/ and generate a 3-building block"
```

Monolith scans, auto-accepts all classifications (no review), generates immediately. Report shows scan summary + build result in one response. For users who trust the auto-classifier.

### 9.2 Cautious User: Full Review

```
User: "scan /Game/HorrorKit/"
User: "show me all the uncertain ones with thumbnails"
User: "SM_Panel_Large is wall_solid, SM_Arch_001 is wall_door, rest are correct"
User: "show me the coverage report"
User: "now generate a 2-story house"
```

5-turn conversation. Each step is short and focused. The user maintains full control.

### 9.3 Iterative Builder

```
User: "scan /Game/HorrorKit/ and build a house"
User: "the windows are too high, SM_Wall_Window_02 has sill at 120cm not 90cm"
User: "re-scan with corrected window opening detection"
User: "rebuild the house"
```

User discovers classification errors through building results, corrects, rebuilds. The pipeline should be fast enough that this iterate-correct-rebuild cycle feels lightweight.

### 9.4 Kit Explorer

```
User: "what kits do I have?"
User: "compare HorrorKit and SteamPunkKit"
User: "which one has better coverage for a hospital?"
User: "use SteamPunkKit for structure, HorrorKit for damage overlays"
```

The user is choosing between kits for a specific building type. Monolith should be able to recommend based on coverage + building type requirements.

---

## 10. Implementation Priority

### Phase 1: Core Scan + Classify (20h)
- `scan_modular_kit` action with 5-signal classifier
- Kit JSON persistence
- Grid detection
- Coverage report
- `list_kits` action

### Phase 2: Review + Correction (8h)
- `edit_kit_classification` action
- Override persistence in kit JSON
- Re-scan with override preservation
- Confidence display in scan results

### Phase 3: Build Integration (12h)
- `build_with_kit` action (connects kit pieces to create_building pipeline)
- Piece selection by type + dimensions
- Variant randomization
- Fallback to GeometryScript for missing pieces
- HISM instancing for kit pieces

### Phase 4: Swap + Multi-Kit (10h)
- `swap_proxies` action
- Multi-kit stack support
- Grid mismatch detection
- Swap report

### Phase 5: Polish (5h)
- Staleness detection
- Kit comparison
- Kit tier detection
- Variety analysis

**Total: ~55h across 5 phases**

---

## 11. Key UX Principles Summary

1. **Scan should take < 5 seconds for 100 meshes.** Users won't wait. Asset Registry queries are fast; classification heuristics must be too. No ML models, no network calls.

2. **Default to auto-accept, flag exceptions.** 80%+ of pieces should classify correctly. Only bother the user with the uncertain 20%.

3. **Never block on missing pieces.** Always have a GeometryScript fallback. Missing pieces are information ("your kit covers 78%"), not errors.

4. **Corrections are first-class.** As easy as saying "that's a wall, not furniture." Persist across re-scans.

5. **Progressive disclosure.** Summary first, details on demand. Don't dump 47 mesh classifications at once.

6. **Kit quality is a spectrum.** A wall-only kit isn't broken -- it's a facade kit. Recognize and message accordingly.

7. **Re-scan is cheap.** Modified assets? Re-scan in seconds. Don't force users to manually track changes.

8. **Shareable kits.** Kit JSON files could become community artifacts. "Here's my classification for the Fab Medieval Pack."

9. **The conversation IS the interface.** No editor panels, no config files to hand-edit. Everything happens in chat. The MCP tool returns structured data; the LLM presents it naturally.

10. **Build fast, correct later.** Generate a building in seconds, walk through it, come back and refine. The proxy-to-swap pipeline means first results are instant, final quality is iterative.

---

## Sources

- [Klavis - Less is More: 4 Design Patterns for MCP Servers](https://www.klavis.ai/blog/less-is-more-mcp-design-patterns-for-ai-agents)
- [MCP Apps Extension - Interactive UIs in Chat](https://blog.modelcontextprotocol.io/posts/2025-11-21-mcp-apps/)
- [MCP UI: How the Next Web Gets Embedded Inside AI Chat](https://www.d4b.dev/blog/2025-09-16-mcp-ui-new-web-in-chat)
- [Agentic Design Patterns - UI/UX & Human-AI Interaction](https://agentic-design.ai/patterns/ui-ux-patterns)
- [Error Recovery and Fallback Strategies in AI Agent Development](https://www.gocodeo.com/post/error-recovery-and-fallback-strategies-in-ai-agent-development)
- [Conversational AI Design Patterns 2026](https://www.ai-agentsplus.com/blog/conversational-ai-design-patterns-2026)
- [Human-in-the-Loop Systems in Machine Learning](https://medium.com/biased-algorithms/human-in-the-loop-systems-in-machine-learning-ca8b96a511ef)
- [Confidence Scores in Machine Learning - Ultralytics](https://www.ultralytics.com/glossary/confidence)
- [HITL Best Practices - Parseur](https://parseur.com/blog/hitl-best-practices)
- [Houdini - Edit an Asset's User Interface](https://www.sidefx.com/docs/houdini/assets/asset_ui.html)
- [Houdini Engine for Unreal](https://github.com/sideeffects/HoudiniEngineForUnreal)
- [Dungeon Architect Documentation](https://docs.dungeonarchitect.dev/unreal/)
- [Dungeon Architect - Placeable Markers](https://docs.dungeonarchitect.dev/unreal/sgf-placeable-markers.html)
- [Procedural World Lab](https://proceduralworldlab.com/)
- [Procedural Building Generator - UE Marketplace](https://www.unrealengine.com/marketplace/en-US/product/procedural-building-generator)
- [TrenchBroom 2025.4 Reference Manual](https://trenchbroom.github.io/manual/latest/)
- [Modular Kit Design - The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics/modular)
- [PCG Overview - UE 5.7 Docs](https://dev.epicgames.com/documentation/en-us/unreal-engine/procedural-content-generation-overview)
- [UE5 Blockout Methods - World of Level Design](https://www.worldofleveldesign.com/categories/ue5/blockouts-in-ue5.php)
- [Level Creation Workflow BSP to Final - World of Level Design](https://www.worldofleveldesign.com/categories/ue4/bsp-03-level-workflow.php)
- [WaveFunctionCollapse - mxgmn](https://github.com/mxgmn/WaveFunctionCollapse)
- [WFC Tips and Tricks - Boris the Brave](https://www.boristhebrave.com/2020/02/08/wave-function-collapse-tips-and-tricks/)
- [Progressive Disclosure Might Replace MCP - MCPJam](https://www.mcpjam.com/blog/claude-agent-skills)
- [AI Tool Overload: Why More Tools Mean Worse Performance](https://www.jenova.ai/en/resources/mcp-tool-scalability-problem)
