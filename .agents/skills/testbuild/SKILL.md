---
name: testbuild
description: Increment VisualComp by 0.01, build and install the VST3 plus the Standalone target, drop the exe into "Build Final\Standalone Test", launch it, then commit and push the working tree to GitHub. Use when asked to "test build", "quick build", "build and run", or "/testbuild".
---

# Test Build

Fast iteration loop: builds the VST3 target first (copying it to
`C:\Program Files\Common Files\VST3\` via CMake's `VST3_COPY_DIR`), then
builds Standalone (still skipping zip/installer bundling), copies the exe to
`Build Final\Standalone Test\`, launches it, and publishes the current
working tree to the GitHub remote so `origin/master` stays in sync with
every test build.

## Workflow

1. **Run the test-build script**. It increments the version by 0.01, then
   builds the VST3 and Standalone targets:
   ```
   powershell -ExecutionPolicy Bypass -File ./testbuild.ps1
   ```
   - The VST3 is installed into `C:\Program Files\Common Files\VST3\` on
     every successful run. If its DLL is locked, close the loaded plugin
     instance in FL Studio and retry.

2. The script copies the new executable to the test folder and launches it:
   - Copies the newest `.exe` in `build\VisualComp_artefacts\Release\Standalone\`
     to `Build Final\Standalone Test\` (creating that folder if needed) and
     launches it via `Start-Process`.
   - Copies `Z:\Azazel Audio Store\src\assets\audio\future-bass-bypassed.wav`
     beside the test executable and launches Standalone with a test-only
     top-left **PLAY** button. The audio remains silent until that button is
     clicked, then loops as the plugin input. This never affects the installed
     VST3 or normal Standalone launches.
   - Throws if no Standalone build output exists yet — surface that error
     rather than retrying blindly.

3. **Report** the copied exe's path back to the user (it's already running,
   satisfying AGENTS.md's "always run the exe after a build" rule on its own
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
     AGENTS.md's normal commit-hygiene rules (new commits over amends, no
     `--no-verify`) and end the message with the required
     `Co-Authored-By: Codex Sonnet 5 <noreply@anthropic.com>` trailer.
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

- Deliberately narrower than `/package-release`: no zip or installer files;
  it still refreshes the installed VST3 alongside the runnable test exe.
- Clear any stale Standalone settings first if a clean UI state is needed to
  test against — see AGENTS.md's screenshot-workflow note about
  `%APPDATA%\VisualComp 2.27\VisualComp 2.settings`.
