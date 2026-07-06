# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP8266 test project for IoT/MCU experimentation. The project is currently in its initial setup phase — no source code, build system, or tests have been established yet.

## Design Principles

This project uses the user's global rules as the primary source of truth for development standards. Key documents loaded automatically into context:

| Document | Purpose |
|----------|---------|
| `agent-constraints.md` | Worker limits, anti-patterns, push requirements |
| `core-directives.md` | Branching from main, tech strategy, artifact workflow, 7 rules |
| `tech-strategy.md` | Language/runtime golden paths, golden paths, infrastructure decisions |
| `code-quality.md` | SOLID, DRY, type safety, performance checklist, quality gates |
| `security.md` | Security checklist, OWASP Top 10, severity classification |

## Tech Strategy

When adding code to this project, follow the **Golden Paths** from `tech-strategy.md`:

- **C/C++** for embedded/MCU code (ESP8266 SDK, Arduino framework)
- **Python** for tooling, scripting, and testing (uv, Ruff, msgspec per golden paths)
- All commits branched from `main` — never commit directly to main

## Development Workflow

### Branching
- Always branch from `main` for any work

### Planning Flow (before touching code)
1. PR-FAQ → PRD → ADR → Design Spec → Plan → Implementation
2. Store artifacts in `./artifacts/`
3. Ephemeral notes in `./scratchpad/`

### Pre-Commit Checks
Before committing, all quality gates must pass:
- Build succeeds
- Tests pass
- Linter passes

### Commits
- Atomic, descriptive messages per complete working change
- Append `Co-Authored-By: Claude <noreply@anthropic.com>` to commit messages

## Project Structure

```
./
├── .claude/              # Claude Code configuration
│   └── settings.local.json
├── artifacts/            # ADRs, design specs, plans (create when planning)
├── scratchpad/           # Working notes, exploration output (ephemeral)
├── CLAUDE.md             # This file
└── ...                   # Source code TBD
```

## Decision Hierarchy

When rules conflict: `Security > Tech Strategy > Core Directives > Skill Conventions > Local Judgment`
