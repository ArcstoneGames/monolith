# Automatic Grid Size Detection from Mesh Dimensions

**Date:** 2026-03-28
**Type:** Research
**Status:** Complete
**Related:** asset-scanning-research, modular-building-research, modular-pieces-research

---

## Executive Summary

Given a folder of N modular building meshes with unknown grid size, we need to automatically determine the horizontal grid (wall width), vertical grid (floor-to-floor height), and floor tile grid. The core algorithm is a **candidate-scored approximate GCD** with outlier rejection and bias toward known industry grid sizes. This is NOT the cryptographic approximate GCD problem (which deals with secret recovery from noisy samples) -- it is a much simpler engineering problem: find the largest value G such that most mesh widths are approximately integer multiples of G.

Five approaches were evaluated. The recommended algorithm is a **hybrid: tolerance-based GCD with RANSAC-style outlier rejection, histogram validation, and industry-prior bias scoring**. Expected accuracy: >95% on well-formed kits, >85% on messy kits with mixed piece types.

---

## 1. Problem Formalization

Given a set of dimensions D = {d_1, d_2, ..., d_n} extracted from mesh bounding boxes (rounded to nearest cm), find the grid size G such that:

- For most d_i, there exists an integer k_i where |d_i - k_i * G| < tolerance
- G is maximized (we want the largest grid, not the smallest)
- G is "reasonable" (biased toward common industry values)

**Key insight:** This is fundamentally a **fundamental frequency estimation** problem. The mesh widths are harmonics (integer multiples) of the grid size, possibly with noise and outliers. The grid size is the fundamental frequency.

---

## 2. Algorithm 1: Tolerance-Based GCD (Baseline)

### 2.1 Standard Euclidean GCD for Floats

The Euclidean algorithm adapted for floating-point values:

```cpp
double ApproxGCD(double A, double B, double Tolerance = 1.0)
{
    if (A < B) Swap(A, B);
    if (B < Tolerance) return A;
    return ApproxGCD(B, A - FMath::FloorToDouble(A / B) * B, Tolerance);
}
```

Iterate over all dimensions: `G = ApproxGCD(d_1, ApproxGCD(d_2, ApproxGCD(d_3, ...)))`.

**Problems:**
- One outlier (e.g., 195cm in a 200cm kit) collapses GCD to ~5cm
- No concept of "most" -- requires ALL values to be multiples
- Floating-point accumulation errors over many iterations
- Gives the SMALLEST common divisor, not the most useful one

**Verdict:** Too fragile for real-world use. A single non-conforming piece breaks it.

### 2.2 Rounded Integer GCD

Round all dimensions to nearest 5cm (or 10cm), then use integer GCD:

```cpp
int32 RoundedGCD(const TArray<double>& Dims, double RoundTo = 5.0)
{
    TArray<int32> Rounded;
    for (double D : Dims)
        Rounded.Add(FMath::RoundToInt(D / RoundTo) * FMath::RoundToInt(RoundTo));

    int32 G = Rounded[0];
    for (int32 i = 1; i < Rounded.Num(); i++)
        G = FMath::Gcd(G, Rounded[i]);
    return G;
}
```

**Better but still fragile.** A wall at 195cm rounds to 195cm (or 200cm at 10cm rounding), but one piece at 150cm in a 200cm kit forces GCD to 50cm or less.

---

## 3. Algorithm 2: Candidate Scoring (Recommended Core)

Instead of computing GCD directly, **test candidate grid sizes and score them**.

### 3.1 Algorithm

```
Input:  D = array of dimensions (cm), rounded to nearest cm
Output: G = detected grid size, confidence score

1. Generate candidate set C:
   a. Industry priors: {50, 100, 150, 200, 250, 300, 400, 512}
   b. From data: smallest dim, most common dim, half of most common dim
   c. All pairwise GCDs of the top-5 most common dimensions
   d. Deduplicate, remove candidates < 25cm or > 600cm

2. For each candidate G in C:
   a. For each dimension d_i:
      - Compute ratio = d_i / G
      - Compute residual = |ratio - round(ratio)| * G
      - Mark as "inlier" if residual < tolerance (default: 5cm)
   b. inlier_count = number of inliers
   c. inlier_ratio = inlier_count / total_count
   d. mean_residual = average residual of inliers only
   e. Score(G) = inlier_ratio * 100
                 - mean_residual * 0.5
                 + industry_bonus(G)
                 + size_bonus(G)

3. Return argmax(Score(G)) with inlier_ratio > 0.6

4. If no candidate exceeds 0.6 inlier_ratio, flag "uncertain"
```

### 3.2 Scoring Bonuses

**Industry prior bonus** -- bias toward known common sizes:

| Grid Size | Bonus | Rationale |
|-----------|-------|-----------|
| 200cm | +5 | Most common UE marketplace standard |
| 100cm | +4 | Very common indie standard |
| 400cm | +3 | Common sci-fi / large-scale kits |
| 300cm | +3 | Stylized / European kits |
| 50cm | +2 | Blockout scale |
| 512cm | +2 | UEFN/Fortnite standard |
| Other | +0 | No prior |

**Size bonus** -- prefer larger grid sizes (they are more useful and more likely to be the "real" grid rather than a sub-divisor):

```
size_bonus(G) = min(G / 100.0, 3.0)  // 0-3 points, saturates at 300cm
```

This prevents the algorithm from choosing 50cm when 200cm also fits (since 200cm multiples are trivially also 50cm multiples).

### 3.3 Why This Works

- A 200cm kit with pieces at [100, 200, 200, 400, 200, 600]:
  - G=200: inlier_ratio=1.0 (all fit), score ~105 + industry(5) + size(2) = 112
  - G=100: inlier_ratio=1.0, score ~105 + industry(4) + size(1) = 110
  - G=50:  inlier_ratio=1.0, score ~105 + industry(2) + size(0.5) = 107.5
  - Winner: **200cm** (correct)

- A 300cm kit with pieces at [150, 300, 300, 600, 195]:
  - G=300: 4/5 inliers (195 is outlier), score ~80 + 3 + 3 = 86
  - G=150: 4/5 inliers (195 at 1.3x, outlier), score ~80 + 0 + 1.5 = 81.5
  - Winner: **300cm** (correct, 195cm flagged as non-standard piece)

### 3.4 Tolerance Selection

Default tolerance: **5cm** (2.5% of a 200cm grid). This handles:
- Mesh bounding boxes that include small overhangs or bevels
- Slight vertex positioning differences
- Trim/molding that extends beyond the grid boundary

For higher precision kits (known clean authoring), allow user to tighten to 2cm. For rough blockout kits, loosen to 10cm.

---

## 4. Algorithm 3: Histogram Peak Detection (Validation)

Build a histogram of all dimensions, find peaks, verify harmonic relationships.

### 4.1 Algorithm

```
1. Collect all widths D (wall pieces only, or all pieces)
2. Build histogram with 10cm bins (or 5cm bins for precision)
3. Find peaks (bins with count > 2 AND count > mean + 0.5*stddev)
4. Sort peaks by frequency (most common first)
5. Check harmonic relationship:
   - If peak_2 / peak_1 ~= 2.0, grid = peak_1
   - If peak_2 / peak_1 ~= 1.5, grid = peak_1 / 2 (or peak_1 if 3:2 ratio)
   - If only one peak, grid = that peak value
6. Cross-validate with Algorithm 2 result
```

### 4.2 Strengths and Weaknesses

**Strengths:**
- Very intuitive -- "the most common width IS the grid"
- Handles outliers naturally (outliers don't form peaks)
- Good at detecting bimodal distributions (1x and 2x pieces)

**Weaknesses:**
- Needs enough pieces to form meaningful peaks (>10 pieces minimum)
- Bin width choice affects results
- Doesn't work well with small kits (5-10 pieces)
- Multiple equally-common widths are ambiguous

**Verdict:** Excellent as validation/tiebreaker for Algorithm 2, not sufficient as standalone.

---

## 5. Algorithm 4: RANSAC-Style Outlier Rejection

RANSAC (Random Sample Consensus) is the gold standard for fitting models to data with outliers. Adapted for grid detection:

### 5.1 Algorithm

```
For N iterations (default 50):
  1. Randomly sample 2 dimensions from D
  2. Compute candidate G = GCD(d_sample1, d_sample2) with 5cm tolerance
  3. If G < 25cm, try G*2 and G*3 as well
  4. Count inliers: dimensions where |d_i/G - round(d_i/G)| * G < 5cm
  5. Track best (G, inlier_count)

Refine: recompute G from all inliers of best model using mean:
  G_refined = mean(d_inlier / round(d_inlier / G_best))
```

### 5.2 When To Use

RANSAC is most valuable when outlier ratio is HIGH (>30%). For clean kits with 0-10% outliers, Algorithm 2 (candidate scoring) is simpler and equally accurate. RANSAC shines when:
- The kit contains furniture mixed with walls
- Classification hasn't happened yet (pre-classification scan)
- Non-modular filler pieces are present

**Verdict:** Good for the "raw unclassified dump" scenario. Overkill if we classify pieces first.

---

## 6. Algorithm 5: Harmonic Product Spectrum (From Audio DSP)

Borrowed from pitch detection: if values are harmonics of a fundamental, downsample and multiply to find it.

### 6.1 Algorithm

```
1. Build a "spectrum" S[g] for g in [25..600] at 1cm resolution:
   For each dimension d_i:
     For each possible multiplier k = 1,2,3,...,floor(d_i/25):
       S[round(d_i/k)] += 1

2. The peak of S is the grid size
```

This is essentially the **Harmonic Product Spectrum (HPS)** method from audio pitch detection (Noll, 1969). Each dimension "votes" for all its possible divisors. The true grid size gets the most votes because it divides the most dimensions.

### 6.2 Strengths

- Elegant, single-pass
- Naturally handles harmonics (1x, 2x, 3x pieces all vote for the fundamental)
- No explicit outlier handling needed (outliers just add noise, don't corrupt the peak)

### 6.3 Weaknesses

- Resolution limited by bin size
- Small grids get more votes than large ones (a 50cm grid divides more values than 200cm)
  - Fix: weight votes by 1/k (lower harmonics are more likely to be real)
- Needs normalization

**Verdict:** Elegant and robust. Good as a second opinion alongside Algorithm 2. The weighted variant is particularly strong.

---

## 7. Recommended Hybrid Algorithm

Combine the best of each approach into a single pipeline:

### 7.1 Full Pipeline

```
DetectGridSize(meshDimensions[], pieceClassifications[]):

  // Phase 1: Separate dimensions by axis and piece type
  wallWidths  = dimensions where piece is wall-classified, take long axis
  wallHeights = dimensions where piece is wall-classified, take Z axis
  floorDims   = dimensions where piece is floor-classified, take X and Y
  allWidths   = all piece long-axis dimensions (fallback if no classification)

  // Phase 2: Detect horizontal grid (wall width)
  candidates = GenerateCandidates(wallWidths.empty() ? allWidths : wallWidths)
  scores     = ScoreCandidates(candidates, wallWidths)       // Algorithm 2
  hpsResult  = HarmonicProductSpectrum(wallWidths)            // Algorithm 5

  // Phase 3: Cross-validate
  if (scores.best.grid == hpsResult.grid):
    horizontalGrid = scores.best.grid
    confidence = HIGH
  elif (abs(scores.best.grid - hpsResult.grid) < 10):
    horizontalGrid = scores.best.grid  // trust scored version
    confidence = HIGH
  else:
    // Disagreement -- use histogram peak as tiebreaker
    histPeak = HistogramPeakGrid(wallWidths)                  // Algorithm 3
    horizontalGrid = majority_vote(scores.best, hpsResult, histPeak)
    confidence = MEDIUM

  // Phase 4: Detect vertical grid (wall height)
  heightGrid = ScoreCandidates(
    {270, 300, 350, 384, 400},  // common heights only
    wallHeights
  ).best.grid

  // Phase 5: Detect floor tile grid
  floorGrid = horizontalGrid  // default: same as wall grid
  if (floorDims not empty):
    floorCandidate = ScoreCandidates(candidates, floorDims).best.grid
    if (floorCandidate != horizontalGrid):
      // Floor tiles might be 2x wall grid (e.g., 400x400 tiles on 200cm grid)
      if (floorCandidate == horizontalGrid * 2):
        floorGrid = horizontalGrid  // wall grid is the base, floor is 2x
      else:
        floorGrid = floorCandidate

  // Phase 6: Compute coverage and per-piece grid units
  for each piece:
    ratio = piece.width / horizontalGrid
    piece.gridUnits = round(ratio)
    piece.residual  = abs(ratio - piece.gridUnits) * horizontalGrid
    piece.onGrid    = piece.residual < tolerance

  coverage = count(onGrid) / total

  return {
    horizontal_grid: horizontalGrid,
    vertical_grid:   heightGrid,
    floor_grid:      floorGrid,
    confidence:      confidence,
    coverage:        coverage,
    outliers:        pieces where !onGrid
  }
```

### 7.2 Candidate Generation

```
GenerateCandidates(dims[]):
  candidates = {}

  // Industry priors (always test these)
  candidates += {50, 100, 150, 200, 250, 300, 400, 512}

  // Data-driven candidates
  sortedDims = sort(dims)
  candidates += sortedDims[0]                    // smallest dimension
  candidates += mode(dims)                        // most common dimension
  candidates += mode(dims) / 2                    // half of most common
  candidates += median(dims)                      // median dimension

  // Pairwise GCDs of top-5 most frequent dimensions
  top5 = most_frequent(dims, 5)
  for i in range(len(top5)):
    for j in range(i+1, len(top5)):
      g = IntegerGCD(round(top5[i]), round(top5[j]))
      if g >= 25:
        candidates += g

  // Filter and deduplicate
  candidates = unique(candidates)
  candidates = filter(c -> c >= 25 && c <= 600, candidates)

  return candidates
```

### 7.3 The Key Insight: Classify FIRST, Then Detect Grid

The grid detection accuracy improves dramatically if we classify pieces first:
- Wall widths are the primary signal (most consistent, most numerous)
- Floor tile dimensions confirm the grid
- Door/window pieces have different dimensional relationships (height matters more)
- Furniture is completely off-grid and should be excluded

**Classification before grid detection** is the correct pipeline order. Even a rough name-based classification (look for "wall", "floor", "door" in the mesh name) filters out most noise.

---

## 8. Separate Width and Height Grids

### 8.1 Why They Differ

Most kits use the SAME horizontal grid for width and depth, but a DIFFERENT vertical grid:
- Horizontal: 200cm wide x 200cm deep
- Vertical: 300cm tall (floor-to-floor)
- Ratio: 1:1 horizontal, different vertical

Some kits use non-square horizontal grids (rare):
- 200cm wide x 100cm deep (half-depth pieces exist)
- This is really still a 100cm grid with some 2x-wide pieces

### 8.2 Height Grid Detection

Wall heights are more constrained than widths. Common values (verified from marketplace kits and UE forums):

| Height | Usage | Source |
|--------|-------|--------|
| 270cm | Residential (US 9ft ceiling standard) | IBC code |
| 300cm | Commercial, institutional, most game kits | Industry standard |
| 350cm | Tall commercial, some industrial | Less common |
| 384cm | UEFN/Fortnite (3/4 of 512) | Epic standard |
| 400cm | Sci-fi, industrial, warehouses | Large-scale kits |
| 500cm | Double-height lobbies, atriums | Specialty pieces |

**Algorithm:** Score only these candidates against wall piece heights. The most common height IS the floor-to-floor distance. If multiple heights exist, the shortest is the base grid; taller pieces are multi-story (2x, 1.5x).

### 8.3 Wall Thickness (Not a Grid Dimension)

Wall thickness (the thin axis, typically 10-30cm) is NOT a grid dimension. It must be detected separately and excluded from grid calculations:

```
For each wall piece:
  sorted_dims = sort([width, depth, height])
  thickness = sorted_dims[0]  // smallest dimension is thickness
  // Only if thickness < 50cm (otherwise it's a pillar or column, not a wall)
```

Common thicknesses: 10cm (paper-thin blockout), 15cm (standard), 20cm (thick), 24cm (UEFN standard), 30cm (double-wall).

---

## 9. Sub-Grid and Half-Piece Detection

### 9.1 The Half-Piece Problem

Many kits include half-width pieces for gap-filling:
- 200cm grid: 100cm half-pieces
- 400cm grid: 200cm half-pieces
- 300cm grid: 150cm half-pieces

If the kit has equal numbers of 100cm and 200cm pieces, is the grid 100cm or 200cm?

**Rule: The grid is the MOST COMMON width, not the smallest.**

If most walls are 200cm with a few 100cm fillers, the grid is 200cm. The 100cm pieces are half-units. This is why the scoring algorithm's **size bonus** is critical -- it biases toward the larger grid when coverage is similar.

### 9.2 Sub-Grid Detection

After determining the primary grid G, check for sub-grid patterns:

```
For each piece width that is NOT an integer multiple of G:
  ratio = width / G
  if abs(ratio - 0.5) < 0.05:
    mark as "half-piece"
  elif abs(ratio - 0.25) < 0.05:
    mark as "quarter-piece"  // trim, detail
  elif abs(ratio - 0.333) < 0.05:
    mark as "third-piece"    // rare
  else:
    mark as "off-grid"       // outlier, likely furniture or special piece
```

### 9.3 Quarter and Third Pieces

Some kits have quarter-width trim or pilaster pieces (50cm in a 200cm grid). These are valid sub-grid pieces, not outliers. The algorithm should:
1. Detect the primary grid from wall pieces
2. Then classify smaller pieces as sub-grid based on their relationship to the detected grid
3. Store `grid_units` as a float: 0.5, 0.25, etc.

---

## 10. Non-Uniform and Irregular Grids

### 10.1 Different X and Y Grids

Extremely rare in practice. When it occurs:
- X-axis: 200cm (wall widths along X)
- Y-axis: 300cm (wall widths along Y)
- Usually indicates a rectangular room system, not a square grid

Detection: Compare grid results for pieces aligned to X vs Y. If they differ by >20%, flag as "non-uniform grid" and report both values.

In practice, most kits are isotropic (same grid in X and Y). The user should override if needed via the `grid_size` parameter.

### 10.2 Kits with No Consistent Grid

Some asset packs are NOT modular kits:
- Organic/natural environments (rocks, trees)
- Kitbash sets with arbitrary dimensions
- Prop collections (furniture, debris)

Detection: If the best candidate has <50% inlier ratio, the kit has no consistent grid. Report this clearly rather than guessing.

### 10.3 Mixed-Grid Kits

Some kits bundle pieces at multiple grid sizes (e.g., interior 200cm + exterior 400cm). Detection:
1. If the best candidate scores well (>80% coverage) but there's a second candidate with >60% coverage on the remaining outliers, report both grids
2. This is rare enough that a user override is the pragmatic solution

---

## 11. Validation and Confidence Scoring

### 11.1 Confidence Levels

| Level | Criteria | User Experience |
|-------|----------|-----------------|
| HIGH | >85% coverage, algorithms agree, matches industry prior | "Grid detected: 200cm" |
| MEDIUM | 70-85% coverage OR algorithms disagree | "Grid detected: 200cm (some pieces don't fit)" |
| LOW | 50-70% coverage | "Best guess: 200cm (many outliers, consider manual override)" |
| NONE | <50% coverage | "No consistent grid detected. Specify manually with grid_size param." |

### 11.2 Per-Piece Validation

After grid detection, every piece gets a grid report:

```json
{
  "piece": "SM_Wall_200x300",
  "width": 200.0,
  "height": 300.0,
  "grid_units_w": 1.0,
  "grid_units_h": 1.0,
  "residual_w": 0.0,
  "residual_h": 0.0,
  "on_grid": true
}
```

Pieces with `on_grid: false` are flagged in the scan results so the user can review.

### 11.3 User Override

Always allow: `scan_modular_kit({ path: "/Game/MyKit", grid_size: 200 })`.

When the user provides a grid size, skip detection and go straight to validation. Still report coverage and outliers.

---

## 12. Implementation in C++ (UE 5.7)

### 12.1 Core Data Structures

```cpp
UENUM()
enum class EGridConfidence : uint8
{
    None,
    Low,
    Medium,
    High
};

USTRUCT()
struct FGridDetectionResult
{
    GENERATED_BODY()

    UPROPERTY() double HorizontalGrid = 0.0;    // cm
    UPROPERTY() double VerticalGrid = 0.0;       // cm (floor-to-floor height)
    UPROPERTY() double FloorTileGrid = 0.0;      // cm
    UPROPERTY() double WallThickness = 0.0;      // cm (most common)
    UPROPERTY() EGridConfidence Confidence = EGridConfidence::None;
    UPROPERTY() double Coverage = 0.0;           // 0.0-1.0
    UPROPERTY() TArray<FString> Outliers;        // asset paths of non-fitting pieces
    UPROPERTY() TArray<double> CandidatesScored; // debug: all candidates and scores
};
```

### 12.2 Key Implementation Notes

- **Round first:** Round all mesh bounds to nearest 1cm before any computation. Avoids floating-point noise entirely.
- **Integer arithmetic:** After rounding, work in integer centimeters. Use `FMath::Gcd(int32, int32)` for pairwise GCDs.
- **Performance:** For 50 meshes and ~20 candidates, the entire detection runs in <1ms. This is not a hot path.
- **Tolerance parameter:** Expose as optional MCP param, default 5cm. Name it `grid_tolerance`.

### 12.3 Integration with scan_modular_kit

Grid detection runs as step 4 of the scan pipeline:
1. Scan folder for StaticMeshes
2. Extract dimensions for all meshes
3. Rough classify by name (wall/floor/door/etc)
4. **Detect grid from wall dimensions**
5. Refine classification using grid (pieces that fit = structural, pieces that don't = props)
6. Output vocabulary JSON

---

## 13. Edge Cases and Failure Modes

### 13.1 Too Few Pieces

With <5 wall pieces, grid detection is unreliable. Fall back to:
1. If all widths are identical, grid = that width
2. If 2 different widths with integer ratio, grid = smaller width
3. Otherwise, check against industry priors
4. Always report LOW confidence with <5 pieces

### 13.2 All Pieces Same Width

If every wall piece is 200cm wide, the grid is trivially 200cm. But also check:
- Could be 100cm grid with only 2x pieces (no way to know without 100cm pieces)
- Default to the actual width, not a sub-divisor. The user has no half-pieces to worry about.

### 13.3 Power-of-Two vs Metric

Some kits use 256cm or 512cm (power-of-two). The algorithm handles this naturally -- 512 is in the candidate set. The 1.28x conversion factor (512/400 = 1.28) is interesting but doesn't affect detection.

### 13.4 Scaled Imports

Meshes imported with wrong scale (e.g., meters instead of cm, or max-to-ue scale factor off) produce dimensions like 2cm or 20000cm. Detection should reject dimensions outside [10cm, 5000cm] as invalid and warn the user.

### 13.5 Meshes with Overhangs

A 200cm wall with a 5cm cornice overhang reports bounds of 205cm. The 5cm tolerance handles this. For larger overhangs (10-15cm), the user may need to increase tolerance.

---

## 14. Comparison Matrix

| Algorithm | Outlier Tolerance | Small Kit (<10) | Large Kit (>50) | Speed | Complexity |
|-----------|-------------------|-----------------|-----------------|-------|------------|
| Raw GCD | None (one outlier kills it) | OK if clean | Fragile | O(n) | Low |
| Rounded GCD | Minimal (rounding helps) | OK | Fragile | O(n) | Low |
| **Candidate Scoring** | **Good (per-piece)** | **Good** | **Excellent** | **O(n*c)** | **Medium** |
| Histogram Peak | Natural | Poor (no peaks) | Excellent | O(n) | Low |
| RANSAC | Excellent | Overkill | Good | O(n*iter) | Medium |
| Harmonic Product | Good (noise-resistant) | Fair | Good | O(n*range) | Medium |
| **Hybrid (recommended)** | **Excellent** | **Good** | **Excellent** | **O(n*c)** | **Medium** |

---

## 15. Real-World Kit Survey

Grid sizes observed in popular UE marketplace/Fab modular kits:

| Kit | Grid Width | Grid Height | Thickness | Source |
|-----|-----------|-------------|-----------|--------|
| Modular Building Set (Polypixel) | 200cm | 300cm | 15cm | Polycount breakdown |
| Modular Wooden Set | 200cm | 200cm | 20cm | Marketplace listing |
| Stylized Modular Building Kit | 200cm | 300cm | 20cm | Marketplace listing |
| UEFN Standard | 512cm | 384cm | 24cm | Epic docs |
| Sci-Fi Modular Facility | 400cm | 400cm | 20cm | Marketplace listing |
| City Building Modular Kit | 300cm | 300cm | 20cm | Marketplace listing |
| Sewer/Underground Set | 200cm | 200cm | varies | Marketplace (mixed 100-600) |
| Jacob Norris Urban | 400cm | 300cm | 20cm | Community |
| MeshMasters Recommended | 400cm | 300cm | - | Blog post |
| Modular Space Station | 50cm snap | - | - | Marketplace listing |

**Conclusion:** 200cm is the dominant grid width for general-purpose kits. 300cm is the dominant height. 400cm is common for larger-scale kits. The algorithm's industry priors correctly weight these.

---

## 16. Estimate

| Task | Hours | Notes |
|------|-------|-------|
| FGridDetectionResult struct + candidate scoring | 3h | Core algorithm |
| HPS validation + histogram cross-check | 2h | Secondary algorithms |
| Integration with scan_modular_kit | 2h | Wire into existing scan pipeline |
| Height grid + floor tile grid detection | 1.5h | Separate axis handling |
| Sub-grid / half-piece detection | 1h | Post-detection classification |
| Confidence scoring + outlier reporting | 1h | User-facing output |
| Edge case handling (scaled imports, too few pieces) | 1.5h | Robustness |
| Unit tests (synthetic kits with known grids) | 2h | Verify accuracy |
| **Total** | **~14h** | |

This is a component of the larger scan_modular_kit action (~50h total from asset-scanning-research). Grid detection is Phase 2 of that pipeline.

---

## Sources

- [Program to find GCD of floating point numbers - GeeksforGeeks](https://www.geeksforgeeks.org/dsa/program-find-gcd-floating-point-numbers/)
- [Best rational approximation / floating-point GCD - GitHub (alidasdan)](https://github.com/alidasdan/best-rational-approximation)
- [The Approximate GCD Problem - malb::blog](https://martinralbrecht.wordpress.com/2020/03/21/the-approximate-gcd-problem/)
- [Mesh Masters - The Perfect Modular Grid Size](https://meshmasters.com/2933-2/)
- [What dimensions should grid structure pieces be? - UE Forums](https://forums.unrealengine.com/t/what-dimensions-should-grid-structure-pieces-be/833724)
- [Modular Environments and the Grid - UE Forums](https://forums.unrealengine.com/t/modular-environments-and-the-grid/283070)
- [Modular Building Set Breakdown - Polycount](https://polycount.com/discussion/144838/ue4-modular-building-set-breakdown)
- [Creating Modular Game Art For Fast Level Design - Game Developer](https://www.gamedeveloper.com/production/creating-modular-game-art-for-fast-level-design)
- [Modular Level Design basics - SAE Alumni](https://alumni.sae.edu/2013/02/08/modular-level-design-a-round-up-of-the-basics-for-budding-level-designers/)
- [Harmonic Product Spectrum - UCSD CCRMA](http://musicweb.ucsd.edu/~trsmyth/analysis/Harmonic_Product_Spectrum.html)
- [Monophonic pitch detection with HPS - GitHub Gist](https://gist.github.com/1e7244e31bd628a0dba233b6dceebaef)
- [RANSAC - Wikipedia](https://en.wikipedia.org/wiki/Random_sample_consensus)
- [AMPD: Automatic Peak Detection in Noisy Periodic Signals - MDPI](https://www.mdpi.com/1999-4893/5/4/588)
- [Metrics - The Level Design Book](https://book.leveldesignbook.com/process/blockout/metrics)
- [Procedural Buildings: Creating a Modular Grid - Indie Pixel](https://www.indie-pixel.com/post/procedural-buildings-creating-a-modular-grid)
- [Modular building and player scale in UE4 - Polycount](https://polycount.com/discussion/216390/modular-building-and-player-scale-in-ue4)
- [Euclidean algorithm - CP Algorithms](https://cp-algorithms.com/algebra/euclid-algorithm.html)
