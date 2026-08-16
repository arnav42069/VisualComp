---
name: testbuild
description: Build just the VisualComp 2.26 Standalone target, drop it into "Build Final\Standalone Test", and launch it for a fast test loop. Use when asked to "test build", "quick build", "build and run", or "/testbuild".
---

# Test Build

Fast iteration loop: builds only the Standalone target (skips VST3 and the
zip bundling that `/package-release` does), copies the exe to
`Build Final\Standalone Test\`, and launches it.

## Workflow

1. **Build the Standalone target only** (Claude builds — the user does not
   build manually; see CLAUDE.md):
   ```
   cmake --build build --config Release --target VisualComp_Standalone
   ```
   - If the build fails because the plugin DLL is locked, FL Studio has an
     instance loaded — ask the user to close that instance (not necessarily
     all of FL) and retry. (Standalone-only builds don't touch the VST3, so
     this is rare here, but the SharedCode lib is still shared.)

2. **Run the test-build script**:
   ```
   powershell -ExecutionPolicy Bypass -File ./testbuild.ps1
   ```
   - Copies the newest `.exe` in `build\VisualComp_artefacts\Release\Standalone\`
     to `Build Final\Standalone Test\` (creating that folder if needed) and
     launches it via `Start-Process`.
   - Throws if no Standalone build output exists yet — surface that error
     rather than retrying blindly.

3. **Report** the copied exe's path back to the user (it's already running,
   satisfying CLAUDE.md's "always run the exe after a build" rule on its own
   — no separate launch step needed).

## Notes

- Deliberately narrower than `/package-release`: no VST3 build, no zip, no
  installer files — just the fastest path from a source change to a runnable
  exe for manual testing.
- Clear any stale Standalone settings first if a clean UI state is needed to
  test against — see CLAUDE.md's screenshot-workflow note about
  `%APPDATA%\VisualComp 2.26\VisualComp 2.settings`.
