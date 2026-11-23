# Agent 30 — explicit factory reload instructions

Updated only `docs/INSTALL_AND_QUICKSTART.md` and `docs/PRESET_GUIDE.md`.

- Explain that an old project may retain an old parameter recipe while displaying the same factory name.
- Give the reliable action: Library → FACTORY LIBRARY → desired entry → LOAD / RELOAD (or Enter / double-click), then play fresh notes and save if desired.
- Explain why an Audition click on an already-loaded row may not reload the recipe.
- Clarify that REVERT restores the sound active when Library opened, potentially the older recipe.
- Keep the documented native VST3 processing-queue-only Program limitation and raw MIDI address mapping unchanged.
- Correct the installation guide’s two stale current-revision references from 2026.11.3 to 2026.11.4. Historical revision references remain unchanged.

Checked the existing loadSelected / return-key / selection routes and agent 27’s source audit. `git diff --check` passes. No tests added or run for these prose-only edits. No commit or push.

Packaging note: the already-running native builds at e32728b contain the earlier installation-guide text. These edits update the working tree only; root must describe the actual bundled guide/source accurately when publishing.
