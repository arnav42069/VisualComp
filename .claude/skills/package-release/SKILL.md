---
name: package-release
description: Build VisualComp 2.21 (VST3 + Standalone, AU on Mac) in Release, code-sign the Windows binaries if a certificate is configured, and assemble the distributable zip via package.ps1. Use when asked to "package the release", "build the release zip", "cut a release build", or "/package-release".
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
   - This only builds Windows formats (VST3 + Standalone) — AU can't be built here. See "AU (Mac)" below.

2. **Run the packaging script**:
   ```
   powershell -ExecutionPolicy Bypass -File ./package.ps1
   ```
   - Assembles `Build Final\VisualComp 2.21 - Windows and Mac.zip` from whichever of `build\` / `build-pkg\` holds the newer `VisualComp 2.21.vst3` (the script checks both — see CLAUDE.md).
   - Bundles: Windows install/uninstall scripts + VST3 + standalone exe, the user manual PDF, and Mac source + build steps (which now build AU in addition to VST3 — see below).
   - Also drops a loose, unzipped `Build Final\VisualComp 2.21.exe` (same binary as inside the zip) so the user can launch and test it directly without unzipping — this happens on every packaging run, not just on request.
   - Signs the Windows `.exe` and the VST3's inner binary with `signtool` if a certificate is configured (see "Code signing" below); otherwise packages unsigned and prints a yellow warning — that warning is expected and not a failure.
   - The script `throw`s if any expected artifact (vst3 / exe / manual) is missing — surface that error to the user rather than retrying blindly. It also `throw`s if signing env vars are set but `signtool.exe` can't be found (Windows SDK not installed).

3. **Report** the final zip path, size, the loose test-exe path, and whether the build was signed back to the user.

## AU (Mac)

- `CMakeLists.txt`'s `FORMATS` includes `AU` alongside `VST3 Standalone`. This is a **no-op on Windows** — JUCE only builds AU when `CMAKE_SYSTEM_NAME` is `Darwin` — so nothing changes about what this Windows machine produces or what `package.ps1` bundles.
- The effect is entirely in `build-macos/build.sh`: when the user copies the project to an actual Mac and runs it there (Apple's toolchain doesn't run on Windows, so this can't be done from here), it now builds **both** VST3 and AU in one pass and auto-installs each to its standard folder (`~/Library/Audio/Plug-Ins/VST3/` and `.../Components/`). Logic Pro specifically needs the AU, since it doesn't load VST3.
- Don't try to produce a `.component` bundle on this machine or add one to the Windows side of the zip — there is no such thing as an AU build on Windows.

## Code signing (Windows)

- Configured entirely via environment variables read by `package.ps1` — nothing to pass on the command line:
  - `VC2_CODESIGN_PFX` + `VC2_CODESIGN_PASSWORD` — path to a `.pfx`/`.p12` cert and its password, **or**
  - `VC2_CODESIGN_THUMBPRINT` — SHA1 thumbprint of a cert already in the Windows certificate store (e.g. an EV cert on a hardware token / cloud HSM CSP)
  - `VC2_CODESIGN_TIMESTAMP_URL` (optional) — RFC3161 timestamp server, defaults to `http://timestamp.digicert.com`
- If none are set, packaging proceeds unsigned — this is the default/expected state until the user supplies an actual certificate. Don't treat the yellow "packaging unsigned" console line as an error.
- `package.ps1` locates `signtool.exe` via `PATH` or by searching `Windows Kits\10\bin\*\x64\` — it's part of the Windows SDK, which is normally already present alongside the Visual Studio C++ toolchain this project already builds with.
- **Important reality check to relay if the user asks why it's still flagged**: a self-signed certificate only removes "Unknown Publisher" from the UAC prompt — it does **not** stop Windows SmartScreen from flagging the binary. Only a certificate from a real, trusted CA does that, and even then a brand-new certificate can still get flagged until it accumulates enough download reputation with Microsoft. Don't imply that any signing step alone guarantees no more warnings.

## Notes

- The version string lives in `package.ps1` (`$version = '2.21'`) — bump it there (and the CMake `PRODUCT_NAME`, per CLAUDE.md) before packaging a new version.
- Ignore/do not use anything referencing `SimpleCompressor` as a build target, `Source/` as a directory, or a script at `scripts/package_release.ps1` — those don't match this codebase's actual CMake targets (`VisualComp_VST3`/`VisualComp_Standalone`) or layout (`src/`), and appear to be stale leftovers from an earlier/different project.
