# Tuxels — Chronological Log

Append-only. Never rewrite history. Dated entries in ISO-8601.

---

## 2026-04-20

### Session start — M0 bootstrap

- SCOPE.md had been written 2026-04-17 (pre-existing). Read in full this session.
- User asked to "get started" with basic Photoshop functionality (layers, masking, brushes, blending), and explicitly asked for a workflow that survives auto-compaction.
- Environment check: Ubuntu 24.04, g++ 13.3.0, no Qt6 or CMake installed system-wide. Ubuntu 24.04 has Qt 6.4.2, CMake 3.28.3, lcms2 2.14, libpng 1.6.43, libtiff 4.5, libwebp 1.3 in apt repos.
- User confirmed: install via `sudo apt`, minimal MVP scope.
- Plan file written and approved: `/home/james/.claude/plans/modular-singing-teacup.md` (Milestone M0, 10 steps S1..S10).
- Persistent memory files saved: `project_tuxels.md` (project overview), `feedback_workflow.md` (doc-update discipline), `MEMORY.md` (index).
- Tasks #1..#10 created in task list, one per step.
- **S1 started**: `.gitignore`, `docs/STATUS.md`, `docs/NEXT.md`, `docs/LOG.md` (this file), `docs/ARCHITECTURE.md`, `docs/BUILD.md` created. `sudo apt install …` running in background.
