# Procedural Town Generator — Session 2 Handover

**Date:** 2026-03-28
**Status:** SP1-SP10 COMPLETE + Fix Plan v2 COMPLETE. Facade alignment needs rework. Research swarm in progress.

## What Was Done This Session

### Implementation (SP2-SP10)
- **Phase 2:** SP2 (Floor Plans), SP3 (Facades), SP6 (Spatial Registry) — 3 parallel agents
- **Phase 3:** SP4 (Roofs), SP5 (City Block), SP7 (Auto-Volumes), SP10 (Furnishing) — 4 parallel agents
- **Phase 4:** SP8a (Terrain), SP8b (Arch Features), SP9 (Debug View) — 3 parallel agents
- **Total:** 10 implementation agents, 45 new actions, ~14,000 lines C++, 18 JSON presets
- All compiled, all tested individually

### Fix Plan v2 (20 issues from playtesting)
- **8 research agents** deployed covering: doors, windows, stairs, ramps, attachment logic, validation, advanced algorithms, room sizes
- **Plan written + 2 independent reviewers** — both APPROVED WITH CHANGES
- **Phase 1 fixes:** Stairs 70°→32°, fire escape 66°→45°, ramp switchback fix, omit_exterior_walls, stairwell cutouts, door Z clamp, door width 110cm
- **Phase 2 fixes:** Room sizes 4-5x corrected, 10 archetypes with realistic dimensions, per-floor rules, aspect ratios, attachment context system, corridor 3 cells, guaranteed entrance
- **Phase 3:** validate_building action (capsule sweep + BFS + stair angles)

### Testing
- All 46 actions tested individually — 1 bug found + fixed (furniture preset types)
- 2 full city block generations tested
- Daredevil view captures working — can "see" layouts from orthographic top-down

### Known Issues (for next session)
1. **Facade alignment** — DISABLED in orchestrator. Facade is separate mesh actor, walls don't align with building. Needs rework: either operate on building mesh directly, or use modular approach
2. **Corridor door gap** — thin wall pillar at entrance/corridor boundary. Bug investigation agent deployed
3. **Building_01 fallback** — floor plan gen fails for some archetype/footprint combos, falls back to 2-room box
4. **No windows in orchestrator output** — facades disabled, buildings have solid walls

## Research Agents Still Running (7)
1. AI vision upgrade system
2. Facade alignment solutions
3. Corridor door gap bug investigation
4. GitHub proc-gen repos
5. Modular building systems (alternative to boolean approach)
6. UE5 mesh APIs we're not using
7. GeometryScript deep dive (face selection, inset, plane cuts)

## Research Completed This Session (9)
All in `Docs/plans/2026-03-28-*-research.md`:
1. Door clearance + validation
2. Window cut-through (double wall root cause)
3. Stairwell cutouts + cross-floor propagation
4. Attachment logic (auto-orient, wall openings)
5. Proc-gen visual validation (3-tier pipeline)
6. Advanced proc building algorithms (constrained growth, WFC, space syntax)
7. Ramp/stair geometry (IBC standards)
8. Realistic room sizes (9 building types, Neufert reference)
9. Real floor plan patterns (circulation types, adjacency matrices, horror subversion)

## Git Status (Monolith repo)
All pushed to `origin/master`. Key commits:
- `feat: Procedural Town Generator SP2-SP10 — 43 new mesh actions`
- `docs: update SPEC/TODO/TESTING for Procedural Town Generator`
- `fix: Phase 1 critical geometry — stairs, windows, stairwells, ramps`
- `fix: Phase 2a — room sizes (4-5x), per-floor rules, attachment context`
- `fix: Phase 2b — corridor width 3 cells, doors 110cm, exterior entrance`
- `fix: Phase 3 — validate_building action`
- `fix: orchestrator wiring — omit_exterior_walls, descriptor passthrough`
- `fix: disable facades in orchestrator until alignment system is reworked`
- `docs: update all docs/skills/agents for fix plan v2`

## Action Counts
- Mesh: 241 | Total: 684
- 11 sub-projects, 46 town gen actions
- 10 building archetypes, 4 facade styles, 2 block presets, 8 furniture presets

## What To Do Next
1. **Wait for 7 research agents** to complete — they'll inform the facade rework approach
2. **Fix facade system** — most likely switch to pre-cut walls (no boolean) or modular pieces
3. **Fix corridor door gap** — bug investigation will identify root cause
4. **Implement adjacency matrix enforcement** — MUST_NOT connections from floor plan research
5. **Implement public-to-private gradient** — entry→public→semi-private→private
6. **Add horror subversion patterns** — blocked routes, loop breaking, privacy inversion
7. **Upgrade daredevil vision** — multi-angle captures, depth buffer analysis, automated QA

## Critical Reminder
- Facades are DISABLED in the orchestrator — `if (false && !bSkipFacades)` at line ~1459
- `omit_exterior_walls` is commented out — buildings generate solid exterior walls
- The `building_context` attachment system is implemented but not wired into the orchestrator
- `validate_building` action exists but hasn't been tested in the orchestrator pipeline
