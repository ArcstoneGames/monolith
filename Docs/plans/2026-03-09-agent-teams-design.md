# Monolith Phase 7: Agent Teams for Unreal Engine

**Version:** 1.0
**Date:** 2026-03-09
**Engine:** Unreal Engine 5.7
**Monolith:** v0.7.0+
**Claude Code:** v2.1.32+ with `CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS=1`

---

## 1. Overview

Agent Teams for Unreal Engine is a coordinated multi-agent system built on three layers:

1. **Claude Code Agent Teams** (experimental, already exists) — team lead + teammates with shared task list and mailboxes
2. **Monolith MCP Plugin** (already exists, v0.7.0) — 177 actions across 9 domains providing full editor access
3. **New deliverables** (this phase) — team lead skill, specialist agent definitions, quality gate hooks, shared blackboard convention, and a new `MonolithTeams` C++ module

A user describes a high-level UE goal. The Team Lead decomposes it into a dependency-aware task DAG, dynamically spawns 2-6 specialist agents (C++, Blueprint, Material, Animation, Niagara), coordinates their work through Monolith's MCP tools, validates results, and reports back.

### Bundling Strategy

Everything ships **inside the Monolith plugin** as an opt-in feature:

- **`MonolithTeams` C++ module** — bundled in `Source/MonolithTeams/`, registered in `.uplugin` but **disabled by default** (`"EnabledByDefault": false`). Users enable it in Editor Preferences > Plugins > Monolith.
- **Agent definitions, hooks, skill, CLAUDE.md template** — shipped in `Templates/AgentTeams/` inside the plugin directory. Users run a one-time setup to copy these into their project (same pattern as `Templates/.mcp.json.example`).
- **Zero impact for users who don't want Agent Teams** — the module doesn't load unless explicitly enabled, and no project files are modified until the user runs setup.

---

## 2. Architecture

```
User
  |
  v
[Team Lead Agent]  (Opus 4.6, orchestrator skill)
  |  Decomposes goal -> task DAG
  |  Spawns teammates via natural language prompts
  |  Validates via TaskCompleted hooks
  |
  +---> [C++ Agent]        (Opus 4.6, source.query + editor.query + Read/Write/Edit)
  +---> [Blueprint Agent]  (Opus 4.6, blueprint.query + project.query + Read)
  +---> [Material Agent]   (Opus 4.6, material.query + project.query + Read)
  +---> [Animation Agent]  (Opus 4.6, animation.query + project.query + Read)
  +---> [Niagara Agent]    (Opus 4.6, niagara.query + material.query + Read)
  |
  v
[Monolith MCP Server]  (embedded HTTP, port 9316)
  |  All teammates connect independently
  |  Each loads project .mcp.json
  |
  v
[Unreal Editor]  (running, serving Monolith)
```

### What Each Layer Provides

| Layer | Provides | Already Exists? |
|-------|----------|----------------|
| Claude Code Agent Teams | Team lead, teammates, shared task list, mailboxes, TeammateIdle/TaskCompleted hooks | Yes (experimental) |
| Monolith MCP | 177 actions: blueprint, material, animation, niagara, editor, config, project, source | Yes (v0.7.0) |
| `.claude/agents/*.md` | Custom agent definitions with tool restrictions, skills, model selection | Yes (Claude Code feature) |
| Team Lead Skill | Orchestration logic, task decomposition templates, dependency rules | **BUILD** |
| Quality Gate Hooks | TaskCompleted validation scripts per agent type | **BUILD** |
| Shared Blackboard | `blackboard.json` file convention for cross-agent state | **BUILD** |
| `MonolithTeams` module | `teams.query` MCP actions: blackboard, validate_task, compile_and_wait, dependency_graph | **BUILD** |
| Agent team CLAUDE.md | Project-level instructions loaded by all teammates | **BUILD** |

---

## 3. Deliverables

### 3.1 MonolithTeams C++ Module (New Monolith Module)

A new module following the established pattern (RegisterActions + static handlers). Namespace: `teams`.

**Actions (10):**

| Action | Description |
|--------|-------------|
| `teams.blackboard_get` | Get a value from the shared blackboard by key |
| `teams.blackboard_set` | Set a key-value pair in the shared blackboard |
| `teams.blackboard_list` | List all blackboard entries, optional prefix filter |
| `teams.blackboard_clear` | Clear all entries or entries matching a prefix |
| `teams.validate_cpp` | Run compilation check, return structured errors/warnings |
| `teams.validate_material` | Run material.validate + shader compile check |
| `teams.validate_blueprint` | Check for broken references, uncompiled state |
| `teams.compile_and_wait` | Trigger Live Coding compile, block until done, return structured result |
| `teams.get_asset_dependencies` | Query Asset Registry for hard+soft dependencies of an asset |
| `teams.get_asset_referencers` | Query Asset Registry for all assets referencing a given asset |

**Blackboard storage:** SQLite table in existing `ProjectIndex.db`:
```sql
CREATE TABLE IF NOT EXISTS blackboard (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT DEFAULT (datetime('now')),
    source TEXT DEFAULT ''
);
```

**Files created:**
- `Source/MonolithTeams/MonolithTeams.Build.cs`
- `Source/MonolithTeams/Public/MonolithTeamsModule.h`
- `Source/MonolithTeams/Private/MonolithTeamsModule.cpp`
- `Source/MonolithTeams/Public/MonolithTeamsActions.h`
- `Source/MonolithTeams/Private/MonolithTeamsActions.cpp`
- `Source/MonolithTeams/Public/MonolithBlackboard.h`
- `Source/MonolithTeams/Private/MonolithBlackboard.cpp`

**Dependencies:** MonolithCore, MonolithIndex (for SQLite access), AssetRegistry, UnrealEd, LiveCoding

### 3.2 Custom Agent Definitions (`.claude/agents/`)

Six agent definition files placed in the Leviathan project's `.claude/agents/` directory:

**`ue-team-lead.md`** — The orchestrator
```yaml
name: ue-team-lead
description: Orchestrates Unreal Engine agent teams. Decomposes goals into tasks, spawns specialists, validates results.
model: opus
skills:
  - ue-team-orchestrator
tools: Read, Grep, Glob, Bash, Agent
mcpServers:
  - monolith
```

**`ue-cpp.md`** — C++ Gameplay Programmer
```yaml
name: ue-cpp
description: C++ specialist for Unreal Engine. Writes gameplay classes, components, GAS abilities, replication.
model: opus
skills:
  - unreal-cpp
  - unreal-build
tools: Read, Write, Edit, Grep, Glob, Bash
mcpServers:
  - monolith
```

**`ue-blueprint.md`** — Blueprint Visual Scripter
```yaml
name: ue-blueprint
description: Blueprint specialist. Reads and analyzes BP graphs, variables, execution flow. Advises on BP architecture.
model: opus
skills:
  - unreal-blueprints
  - unreal-debugging
tools: Read, Grep, Glob, Bash
mcpServers:
  - monolith
```

**`ue-material.md`** — Material/Shader Artist
```yaml
name: ue-material
description: Material specialist. Builds PBR and Substrate material graphs, validates, creates Custom HLSL nodes.
model: opus
skills:
  - unreal-materials
tools: Read, Grep, Glob
mcpServers:
  - monolith
```

**`ue-animation.md`** — Animation Specialist
```yaml
name: ue-animation
description: Animation specialist. Montages, blend spaces, ABP state machines, notifies, skeletons.
model: opus
skills:
  - unreal-animation
tools: Read, Grep, Glob
mcpServers:
  - monolith
```

**`ue-niagara.md`** — Niagara VFX Specialist
```yaml
name: ue-niagara
description: Niagara VFX specialist. Creates particle systems, configures emitters/modules/renderers, GPU sim, scalability.
model: opus
skills:
  - unreal-niagara
tools: Read, Grep, Glob
mcpServers:
  - monolith
```

Each agent's markdown body contains the full system prompt with:
- Role definition and ownership boundaries
- Monolith tool usage patterns (which namespace.actions to use)
- Handoff manifest format (YAML listing API surface, asset paths, parameter names)
- Communication protocol (NEED_CPP_API, BROKEN_REF, COMPLETED report formats)
- Quality standards and constraints
- UE 5.7 specific notes

### 3.3 Team Lead Skill

A new Claude Code skill at `Skills/ue-team-orchestrator/ue-team-orchestrator.md`:

Contains:
- **Feature archetype templates** — pre-built task DAGs for common UE feature types:
  - Gameplay Ability (GAS): C++ ability class -> compile -> BP wiring + animation montage + VFX + UI cooldown
  - Weapon System: C++ weapon class -> compile -> BP weapon actor + materials + animations + VFX
  - UI Feature: C++ data model -> compile -> Widget Blueprint + data binding
  - Environment Prop: Material -> Niagara VFX -> Level placement
  - Character System: C++ component -> compile -> ABP + blend spaces + montages + BP wiring
- **Dependency chain rules** — the universal UE build order
- **Spawn prompt templates** — per-specialist prompts with Monolith tool instructions
- **Blackboard conventions** — key naming patterns (`assets.{type}.{name}`, `classes.{name}`, `params.{system}.{name}`)
- **Quality gate criteria** — what to check per agent type
- **Communication protocol** — structured message formats for task assignment, rejection, unblock, handoff

### 3.4 Quality Gate Hooks

Shell scripts placed in `.claude/hooks/`:

**`task-completed-validate.sh`** — TaskCompleted hook that:
1. Reads task metadata from stdin JSON
2. Determines agent type from `teammate_name`
3. Runs domain-specific validation:
   - **C++ agent**: calls `teams.compile_and_wait` via curl to Monolith, checks for zero errors
   - **Material agent**: calls `teams.validate_material` via curl
   - **Blueprint agent**: calls `teams.validate_blueprint` via curl
   - **Animation agent**: verifies referenced assets exist via `project.query/search`
   - **Niagara agent**: verifies material assignments are non-null
4. Exit 0 (accept) or exit 2 + stderr feedback (reject)

**`teammate-idle-check.sh`** — TeammateIdle hook that:
1. Reads task list for uncompleted tasks assigned to this teammate
2. If uncompleted tasks exist: exit 2 with "You have N uncompleted tasks" on stderr
3. If all done: exit 0

Configuration in `.claude/settings.json`:
```json
{
  "env": {
    "CLAUDE_CODE_EXPERIMENTAL_AGENT_TEAMS": "1"
  },
  "hooks": {
    "TaskCompleted": [
      {
        "hooks": [{
          "type": "command",
          "command": ".claude/hooks/task-completed-validate.sh"
        }]
      }
    ],
    "TeammateIdle": [
      {
        "hooks": [{
          "type": "command",
          "command": ".claude/hooks/teammate-idle-check.sh"
        }]
      }
    ]
  }
}
```

### 3.5 AI-Driven Setup Guide

Instead of a bash setup script or CLAUDE.md template, we ship `Templates/AgentTeams/AGENT-TEAMS-SETUP.md` — a comprehensive guide written **for the AI**. The AI becomes the installer.

**User workflow:** The user tells their AI:
> "Read `Plugins/Monolith/Templates/AgentTeams/AGENT-TEAMS-SETUP.md`"

The AI reads the guide, then walks the user through setup interactively:
1. Confirms prerequisites (Monolith version, Claude Code, experimental feature awareness)
2. Asks which agents to install (all 6 or a subset)
3. Asks about quality gate hooks
4. Copies templates, configures settings — with user approval at each step
5. Shows the user what CLAUDE.md sections it wants to append, only proceeds if approved
6. Gives a summary and verification steps

This approach:
- Never overwrites the user's existing CLAUDE.md
- Lets the user choose which components they want
- Adapts to the user's project (the AI has context about what domains they use)
- No bash script dependency — works on any OS, any MCP client

The CLAUDE.md sections cover:
- Monolith MCP tool reference (10 namespaces, 177 actions)
- Agent team coordination rules (compile gate, blackboard protocol, one-agent-per-asset, handoff manifests)
- UE naming conventions (BP_, M_, MI_, MF_, AM_, ABP_, NS_, SM_, SK_, T_, WBP_)
- Build rules (Live Coding default, full rebuild criteria)

### 3.6 Blackboard Convention Document

A `blackboard-conventions.md` in `Skills/ue-team-orchestrator/`:

Key naming patterns:
```
assets.material.{name}     -> /Game/Materials/M_{Name}
assets.niagara.{name}      -> /Game/VFX/{Name}/NS_{Name}
assets.montage.{name}      -> /Game/Animation/AM_{Name}
assets.blueprint.{name}    -> /Game/Blueprints/BP_{Name}
assets.widget.{name}       -> /Game/UI/WBP_{Name}
classes.{name}             -> Module.ClassName
params.{system}.{param}    -> parameter name string
tags.{category}.{name}     -> Gameplay tag string
sockets.{name}             -> Socket name string
delegates.{class}.{name}   -> Delegate signature info
```

---

## 4. Dependency Chain (Universal UE Build Order)

```
C++ Module Compilation (HARD GATE - nothing proceeds without clean compile)
  |
  +-> Blueprint class creation (references C++ base classes)
  |     +-> Widget Blueprint (references BP classes + C++ components)
  |     +-> Blueprint instance placement (references BP classes)
  |
  +-> Animation assets (may reference C++ AnimNotify classes)
  |     +-> Montages (need AnimSequences + AnimNotify classes)
  |     +-> ABP state machines (need montages + C++ variables)
  |
  +-> Material creation (rarely depends on C++ unless custom nodes)
  |     +-> Material Instances (need parent material)
  |     +-> Niagara systems (need materials for renderers)
  |     +-> Level placement (need materials)
  |
  +-> UI widgets (reference C++ function libraries, GAS components)
        +-> HUD integration (reference widgets)
```

---

## 5. Cross-Agent Coordination Protocol

### 5.1 Handoff Manifest Format

Every agent produces this after completing a task:

```yaml
task_id: 3
agent: cpp
status: completed
files_created:
  - Source/Leviathan/Public/Abilities/GA_Dash.h
  - Source/Leviathan/Private/Abilities/GA_Dash.cpp
files_modified: []
blueprint_api_surface:
  functions:
    - name: ActivateDash
      class: UGA_Dash
      params: "float DashDistance, float DashDuration"
      return: void
      specifiers: BlueprintCallable
  delegates:
    - name: OnDashComplete
      class: UGA_Dash
      params: "bool bWasSuccessful"
      type: Dynamic Multicast
  properties:
    - name: DashDistance
      class: UGA_Dash
      type: float
      specifiers: EditDefaultsOnly, BlueprintReadOnly
blackboard_writes:
  classes.GA_Dash: "Leviathan.UGA_Dash"
  tags.ability.dash: "Ability.Dash"
  tags.cooldown.dash: "Cooldown.Dash"
compile_status: clean
warnings: []
```

### 5.2 Task Assignment Format

```yaml
task_id: 4
agent: blueprint
summary: Wire up dash ability to Enhanced Input
scope: /Game/Blueprints/BP_PlayerCharacter
blocked_by: [1, 2, 3]  # C++ tasks
inputs:
  - class: UGA_Dash (see blackboard classes.GA_Dash)
  - input_action: IA_Dash (verify via project.query/search)
expected_outputs:
  - Modified: /Game/Blueprints/BP_PlayerCharacter
acceptance_criteria:
  - BP compiles without errors
  - IA_Dash bound to TryActivateAbilityByClass(UGA_Dash)
  - AbilitySet grants UGA_Dash on possession
```

### 5.3 Error Recovery

| Scenario | Recovery |
|----------|----------|
| C++ compile failure | Reject task, feed compiler errors back to C++ Agent. Max 3 retries, then escalate to user |
| Blueprint broken reference | Check if C++ class was renamed. If yes, tell BP Agent the new name. If missing, create new C++ task |
| Material shader error | Reject, provide shader compiler output. Agent simplifies graph |
| Asset conflict (two agents same file) | Should never happen (Lead prevents). If it does, determine canonical version, reject other |
| Agent timeout/hang | TeammateIdle hook detects. Reassign task to fresh instance |
| Wrong architecture | Reject all downstream tasks. Revise plan. Expensive — prevention via specific initial planning |

---

## 6. Windows-Specific Notes

- **Display mode:** `in-process` only (psmux/tmux split panes blocked by isTTY bug, Issue #26244)
- **Teammate cycling:** Shift+Down to cycle, Enter to view, Escape to interrupt
- **Task list toggle:** Ctrl+T
- **MCP connection:** Each teammate connects to `http://localhost:9316/mcp` independently via project `.mcp.json`
- **File paths:** Use forward slashes in all Monolith MCP calls, Windows paths for file system tools

---

## 7. Limitations and Future Work

### Current Limitations
- **No custom agent types for teammates** — all spawn as general-purpose, differentiated only by prompt (Issue #24316)
- **No split panes on Windows** — in-process mode only (Issues #24384, #26244)
- **No session resume with teammates** — team context lost on restart
- **One team per session** — clean up before starting a new team
- **Blueprint write actions missing** — agents can read but not modify BP graphs via Monolith
- **Niagara HLSL module creation stubbed** — Epic APIs not exported
- **No PIE automation** — cannot run automated playtests

### Future Work (Beyond Phase 7)
- Phase 3 Animation expansion (39 additional actions) — unblocks Animation Agent autonomy
- Blueprint WRITE actions (add_node, connect_pins, add_variable) — unblocks Blueprint Agent autonomy
- Material Instance creation/editing — unblocks full material pipeline
- Substrate node support in build_material_graph — UE 5.7 native material system
- ABP state machine WRITE actions — unblocks animation system construction
- PIE automation (start, input, screenshot, stop) — enables automated integration testing
- Custom agent types for teammates (when Issue #24316 lands) — `.claude/agents/` files as teammate types

---

## 8. File Manifest

### Plugin Files (bundled with Monolith)

| File | Location | Type |
|------|----------|------|
| `MonolithTeams.Build.cs` | `Source/MonolithTeams/` | C++ Build config |
| `MonolithTeamsModule.h/cpp` | `Source/MonolithTeams/` | C++ Module |
| `MonolithTeamsActions.h/cpp` | `Source/MonolithTeams/` | C++ Action handlers |
| `MonolithBlackboard.h/cpp` | `Source/MonolithTeams/` | C++ Blackboard store |
| `Monolith.uplugin` update | Plugin root | Add MonolithTeams module (disabled by default) |

### Template Files (shipped in plugin, copied to project on setup)

| File | Plugin Location | Project Destination |
|------|----------------|---------------------|
| `ue-team-lead.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-cpp.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-blueprint.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-material.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-animation.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-niagara.md` | `Templates/AgentTeams/agents/` | `.claude/agents/` |
| `ue-team-orchestrator.md` | `Templates/AgentTeams/skills/ue-team-orchestrator/` | `Skills/ue-team-orchestrator/` (or `~/.claude/skills/`) |
| `blackboard-conventions.md` | `Templates/AgentTeams/skills/ue-team-orchestrator/` | `Skills/ue-team-orchestrator/` (or `~/.claude/skills/`) |
| `task-completed-validate.sh` | `Templates/AgentTeams/hooks/` | `.claude/hooks/` |
| `teammate-idle-check.sh` | `Templates/AgentTeams/hooks/` | `.claude/hooks/` |
| `AGENT-TEAMS-SETUP.md` | `Templates/AgentTeams/` | N/A (AI reads this, walks user through interactive setup) |
| `settings.json.example` | `Templates/AgentTeams/` | `.claude/settings.json` (AI merges during setup) |
| `README.md` | `Templates/AgentTeams/` | N/A (human-readable overview + "tell your AI to read AGENT-TEAMS-SETUP.md") |
