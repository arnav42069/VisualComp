---
name: testbuild
description: Build just the VisualComp 2.27 Standalone target, drop it into "Build Final\Standalone Test", launch it, then commit and push the working tree to GitHub — a fast test-and-publish loop. Use when asked to "test build", "quick build", "build and run", or "/testbuild".
---

# Test Build

Fast iteration loop: builds only the Standalone target (skips VST3 and the
zip bundling that `/package-release` does), copies the exe to
`Build Final\Standalone Test\`, launches it, and publishes the current
working tree to the GitHub remote so `origin/master` stays in sync with
every test build.

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

4. **Commit and push**, once step 1's build has succeeded (a failed build
   means the tree isn't in a state worth publishing — fix it first, don't
   commit broken source over it):
   ```
   git add -A
   git commit -m "<real, specific summary of what actually changed>"
   git push
   ```
   - Write the commit message yourself, describing the actual changes made
     this session — never a generic "test build" placeholder. Follow
     CLAUDE.md's normal commit-hygiene rules (new commits over amends, no
     `--no-verify`) and end the message with the required
     `Co-Authored-By: Claude Sonnet 5 <noreply@anthropic.com>` trailer.
   - `git push` targets the already-configured `origin` remote
     (`github.com/arnav42069/VisualComp`) on the current branch — don't
     create or repoint remotes here.
   - If the working tree is already clean (nothing to commit) but the local
     branch is still ahead of `origin` — e.g. commits made earlier in the
     session — skip straight to `git push`.
   - If `git push` is rejected because `origin` has moved ahead (needs a
     fetch/merge first), stop and surface that to the user instead of
     force-pushing — `/testbuild` should never need `--force`.

## Notes

- Deliberately narrower than `/package-release`: no VST3 build, no zip, no
  installer files — just the fastest path from a source change to a
  runnable, published exe for manual testing.
- Clear any stale Standalone settings first if a clean UI state is needed to
  test against — see CLAUDE.md's screenshot-workflow note about
  `%APPDATA%\VisualComp 2.27\VisualComp 2.settings`.
