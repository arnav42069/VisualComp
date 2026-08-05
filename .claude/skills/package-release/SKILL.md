---
name: package-release
description: Build VisualComp 2.21 (VST3 + Standalone) in Release and assemble the distributable zip via package.ps1. Use when asked to "package the release", "build the release zip", "cut a release build", or "/package-release".
---

# Package Release

Builds VisualComp 2.21 and assembles the Windows+Mac distributable zip in `Build Final\`.

## Workflow

1. **Build the Release targets** (Claude builds — the user does not build manually; see CLAUDE.md):
   ```
   cmake --build build --config Release --target VisualComp_VST3 --target VisualComp_Standalone
   ```
   - This auto-installs the VST3 to `C:\Program Files\Common Files\VST3\`.
   - If the build fails because the plugin DLL is locked, FL Studio has an instance loaded — ask the user to close that instance (not necessarily all of FL) and retry.

2. **Run the packaging script**:
   ```
   powershell -ExecutionPolicy Bypass -File ./package.ps1
   ```
   - Assembles `Build Final\VisualComp 2.21 - Windows and Mac.zip` from whichever of `build\` / `build-pkg\` holds the newer `VisualComp 2.21.vst3` (the script checks both — see CLAUDE.md).
   - Bundles: Windows install/uninstall scripts + VST3 + standalone exe, the user manual PDF, and Mac source + build steps.
   - Also drops a loose, unzipped `Build Final\VisualComp 2.21.exe` (same binary as inside the zip) so the user can launch and test it directly without unzipping — this happens on every packaging run, not just on request.
   - The script `throw`s if any expected artifact (vst3 / exe / manual) is missing — surface that error to the user rather than retrying blindly.

3. **Report** the final zip path, size, and the loose test-exe path back to the user.

## Notes

- The version string lives in `package.ps1` (`$version = '2.21'`) — bump it there (and the CMake `PRODUCT_NAME`, per CLAUDE.md) before packaging a new version.
- Ignore/do not use anything referencing `SimpleCompressor` as a build target, `Source/` as a directory, or a script at `scripts/package_release.ps1` — those don't match this codebase's actual CMake targets (`VisualComp_VST3`/`VisualComp_Standalone`) or layout (`src/`), and appear to be stale leftovers from an earlier/different project.
