# VisualComp: minimal UI refinement implementation brief

## Purpose and authorization

This is a detailed handoff for a GPT-5.6-level implementation model. Implement only the four approved refinements below when the user asks you to execute this brief. Creating this document does not itself authorize a build, commit, push, release, or version bump.

The approved direction is to preserve the existing design, not to implement a new A/B concept. The generated comparison image was inaccurate and is NOT an implementation reference. In particular, do not copy its misplaced Gain Out slider, duplicate menu headings, altered knob silhouette, or removed explanatory captions.

Approved scope:

1. Match the GR needle screen background to the existing waveform screens.
2. Align the existing SC, LIM, and AUTO GAIN buttons, with AUTO GAIN directly beneath Gain Out.
3. Ensure the existing darker chunky knob grooves rotate with the knob while lighting stays fixed.
4. Display the current version immediately after `AZAZEL AUDIO` in the logo dropdown, on the same line.

Do not ask the user to choose these details again. If implementation requires a different visual design decision, explain it and request direction before making that change.

## 1. Required preflight

Work from `C:\Users\arnav\VisualComp` unless the user identifies another checkout.

1. Read `AGENTS.md`, `CLAUDE.md`, and the applicable `visualcomp-ui` skill completely.
2. Inspect `git status --short`, unstaged changes, and staged changes. This checkout already contains substantial staged UI work. Preserve it; do not reset, unstage, overwrite, or claim it as newly implemented work.
3. Read the current version from `CMakeLists.txt`. At preparation time it is `2.42`; this is an observation, not a new version constant to copy into source.
4. Inspect the actual source and current built executable before assuming an old screenshot describes current code.
5. Capture a baseline from the exact executable being tested. Record its absolute path, version, and build timestamp. A differently named VisualComp build from another directory is not valid evidence for this checkout.
6. Determine which requirements already work. Retain working code and verify it instead of rewriting it for the sake of producing a diff.

Useful read-only searches:

```powershell
git status --short
git diff --stat
git diff --cached --stat
rg -n 'VC2_VERSION_STRING|PRODUCT_NAME' CMakeLists.txt
rg -n 'showLogoMenu|addSectionHeader|agX|limX|scX|kUtilRow' src/PluginEditor.cpp
rg -n 'drawRecess|Glass glare|ColourGradient' src/VuMeter.cpp src/WaveformDisplay.cpp src/Theme.h
rg -n 'grip_angle|render_base|surface == 9|def verify' scripts/make_knob_3d.py
```

## 2. Invariants: what must not change

- Preserve the smoked grey-green titanium chassis, existing orange accents, typography, labels, faders, panel geometry, and current visual hierarchy.
- Do not lighten/darken the entire interface, replace screen trim, add a new border system, or redesign button faces.
- Preserve the existing raised knob model: silhouette, depth, height, 24 chunky scalloped grooves, face, pointer, camera angle, diameter, and value arc.
- The current knob tilt is 14.4 degrees. Do not make it more tilted or taller to imitate the generated comparison image.
- Keep main knobs approximately 78 pixels at the existing 100% layout. Do not shrink them or resize modules to solve button alignment.
- Keep compact SC and LIM buttons and the wider AUTO GAIN button. Equal-width buttons were not selected.
- Keep SIDECHAIN and 0 dB CEILING explanatory captions; the mockup omitted them incorrectly.
- Do not change DSP, compression curves, GR measurement/ballistics, EQ behavior, sidechain or limiter behavior, APVTS IDs/ranges, parameter attachments, presets, or audio-thread synchronization.
- Preserve the previously corrected transfer-curve coordinate mapping. The current request is not another transfer-curve algorithm change.
- Keep the existing filmstrip rendering/caching architecture. Do not introduce runtime 3D rendering or a new graphics dependency.
- Do not change the product code `Vc22`, preset folder, or version merely to expose the menu label.
- Do not add new settings, parameters, modes, or user-facing options.

## 3. GR screen background

### Relevant code

- `src/VuMeter.cpp`: `VuMeter::paint()` paints the analog gain-reduction needle meter.
- `src/WaveformDisplay.cpp`: `WaveformDisplay::paint()` is the screen-background reference.
- `src/Theme.h`: shared `screen`, `surfSunk`, and `drawRecess()`.
- `src/GrCurveDisplay.cpp`: the separate transfer-curve screen; inspect for regression only.

### Observed implementation

Both the waveform and needle meter already use `Theme::drawRecess()`. The shared inset screen color currently resolves to `#101310`. The needle meter additionally paints a `Glass glare` gradient over the face, from white alpha 0.055 at the top to transparent at 45% of the face height. This can brighten the GR screen even though the underlying fill is identical.

### Required implementation

1. Compare the background draw sequence, not just the base color constants.
2. Remove the GR-only broad glare overlay if it causes the observed mismatch. Do not compensate by choosing a different darker base color under the glare.
3. Continue using the same shared recessed background as the waveform screens.
4. Preserve the meter's current face bounds, corner radius, rim, local edge shadows, scale, needle, pivot, labels, and readout row.
5. Do not change the surrounding metal area or make the whole component near-black. Only the interior screen surface is in scope.
6. Avoid changing `Theme::drawRecess()` globally merely to fix this one component.

### Acceptance

- Empty GR interior areas match empty waveform interior areas in the same screenshot.
- Sample several pixels away from rims, text, needle, grid, and colored traces. For flat interior samples, aim for no more than 1 RGB code value difference per channel; compare equivalent regions, not shadowed edge pixels.
- No broad GR-only white wash remains.
- Needle behavior and meter geometry are unchanged.

## 4. Bottom utility-button alignment

### Relevant code

`src/PluginEditor.cpp`, `VisualCompEditor::resized()`, the utility strip anchored to Gain Out.

### Exact desired structure

From left to right:

```text
[ SC ] SIDECHAIN    [ LIM ] 0 dB CEILING    [ AUTO GAIN ]
                                               |
                                  same horizontal centre
                                               |
                                      GAIN OUT fader
```

The diagram describes horizontal alignment only: the fader remains ABOVE AUTO GAIN in the actual interface.

### Required implementation

1. Use a single shared utility-row Y coordinate and height for all three buttons.
2. Keep compact widths and existing captions. Current constants are SC = 40, LIM = 44, AUTO GAIN = `kFaderW` (currently 96), caption gap = 5, group gap = 20, SIDECHAIN caption width = 68, and ceiling caption width = 84. Preserve these unless a demonstrated clipping issue requires a minimal correction.
3. Calculate AUTO GAIN from the Gain Out module bounds. Its horizontal centre must equal the fader/module centre, not the centre of the last rotary knob or the whole button group.
4. Derive the LIM group to the left of AUTO GAIN and the SC group to the left of LIM. Use equal gaps BETWEEN complete groups (button plus caption), not equal raw button-to-button distances.
5. Align captions vertically with their buttons; preserve current font and wording.
6. Verify label justification and visible face/text alignment as well as component bounds. Fix the actual cause if bounds already match.
7. Do not add button outlines. Preserve current clean faces and functional hover, pressed, toggled, and focus feedback.

Current geometry already largely expresses this requirement:

```cpp
const int uy = kCtrlY + kUtilRowY, uh = kUtilRowH;
const int agX = ox + gainOutX;
const int limX = agX - groupGap - limCapW - capGap - limW;
const int scX = limX - groupGap - scCapW - capGap - scW;
```

Do not replace correct geometry with unrelated pixel nudges. If it passes the checks below in the fresh executable, report it as verified existing behavior.

### Dock and scale rules

- `resized()` applies `+ox` once to main-content X coordinates.
- `paint()` already translates the main-content coordinate system by `ox`; do not double-translate component bounds there.
- Preserve the right-column `cshift` behavior. Do not re-anchor these buttons to the entire window's right edge.
- Check EQ dock open/closed and Curve/GR dock open/closed, plus 50%, 70%, 100%, 150%, and 200% supported zoom.

### Acceptance

- All button tops, bottoms, and text baselines align, allowing at most one physical pixel from display scaling/rounding.
- AUTO GAIN centre matches Gain Out centre within one physical pixel.
- SC is left of LIM; LIM is left of AUTO GAIN.
- Captions remain readable and do not overlap buttons, module dividers, or window edges.
- Toggling every button still changes its original function.

## 5. Rotating darker knob grooves

### Relevant code and assets

- `scripts/make_knob_3d.py`: current 3D geometry/lighting bake and verification.
- `resources/knob-azazel-192x61.png`: production embedded strip.
- `resources/knob-contact-sheet.png`: visual review sheet.
- `resources/knob-raised-3d.obj`: editable mesh.
- `src/KnobStrip.h` and `src/KnobStrip.cpp`: decode, downsample, cache, frame selection.
- `src/PluginEditor.cpp`: `AzazelLookAndFeel::drawRotarySlider()`.
- `CMakeLists.txt`: binary-data embedding.

### Existing behavior to preserve and verify first

The current generator already passes `grip_angle` into intersection and shadow rendering. It darkens concave groove walls with `rgb[surface == 9] *= 0.72`. The build loop derives grip phase from each pointer angle and renders geometry under a fixed camera/light. Do not darken grooves again simply because this brief says 'darker': the current darker treatment is the target.

### Required behavior

- Turning a parameter rotates the body grooves together with the pointer, around the knob's physical central axis.
- Camera, chassis shadow orientation, and world-space light remain fixed. Top-face shading must not revolve like a sticker.
- Do not rotate the completed filmstrip cell/bitmap. That incorrectly rotates perspective, lighting, and shadows.
- Keep the 61-frame, 192-pixel-cell strip and existing 270-degree sweep unless separately approved. Smoother/higher-frame animation was not selected.
- Preserve normal parameter mapping, bipolar arcs, threshold coloring, host automation, and selected-band behavior.

### Important verification trap

Twenty-four identical grooves repeat every 15 degrees. Some widely spaced frames can look identical even when rotation works. Inspect adjacent frames and track a groove over a small angular change; do not infer failure from matching distant frames or a five-image contact sheet alone. Temporal aliasing at some speeds is not proof that geometry is stationary.

### Workflow

1. Inspect current strip frames and runtime frame selection.
2. Test actual dragging and parameter automation on the newly built executable. Confirm the embedded strip, not an old disk image, is displayed.
3. If current code and asset pass, leave them unchanged.
4. If regeneration is necessary, use `scripts/make_knob_3d.py`, not the older `make_knob_filmstrip.py` recipe in historical documentation. Inspect the script's current CLI before running it.
5. Preserve the existing model parameters and material treatment. Rebuild the binary-data target through the normal application build after changing the PNG.
6. Run the generator's existing verification and inspect the strip at main-knob, Island, and Mix sizes. Do not weaken assertions to force a pass.

### Acceptance

- Adjacent body frames show moving grooves; pointer/body angular motion is consistent.
- Fixed-light top-face check, groove-rotation geometry check, periodicity check, pointer-angle check, and transparent-edge check pass.
- The current pointer-error threshold is less than 0.6 degrees; preserve it.
- No new halos, clipped shadows, flickering lighting, diameter changes, or perspective changes.
- No added disk I/O or geometry baking in paint callbacks; preserve existing decode/downsample caches.
- Provide runtime motion evidence (short capture or successive states plus an observed drag), not just a single screenshot. If interactive verification is unavailable, explicitly state that limit.

## 6. Inline version in logo dropdown

### Relevant code

`src/PluginEditor.cpp`, `VisualCompEditor::showLogoMenu()`.

### Required appearance

```text
Zoom (100%)             >
-------------------------
AZAZEL AUDIO  <version>
Show Help
Visit azazelaudio.com
```

- One section heading only.
- Version immediately follows the brand with a small gap on the same baseline.
- Keep the current heading font, orange color, left alignment, and menu item order.
- Do not right-align the version to the menu edge, add a badge, add a second title, or put the version on a separate row.
- Preserve zoom, Help, URL, and asynchronous menu callbacks.

### Existing implementation

The current source already contains:

```cpp
const auto version = juce::String(JucePlugin_Name)
    .fromLastOccurrenceOf(" ", false, false);
menu.addSectionHeader("AZAZEL AUDIO  " + version);
```

`JucePlugin_Name` comes from the CMake product name, which includes `VC2_VERSION_STRING`. Verify the generated value and retain this working approach. Do not hardcode `2.42` or introduce another independent version value. Do not substitute a three-part version macro without checking that its formatting matches the displayed product version.

### Acceptance

- Open the actual logo dropdown and capture it.
- Exactly one inline version is visible and equals the current CMake product version.
- Heading is neither clipped nor ellipsized at supported zoom levels. Adjust menu sizing only if needed; do not shrink the heading font to hide a sizing issue.
- Do not bump the version merely to test this label.

## 7. Build and validation sequence

These steps apply to the later implementation task, not to creating this document.

1. Make minimal patches only where verified behavior fails the requirements.
2. Review the task-specific diff and ensure no unrelated source or existing staged work was altered.
3. Inspect the current CMake cache and compiler before choosing a build directory. At preparation time, `build-vs` is the working MSVC build tree; `build` has previously contained a different compiler cache. Recheck rather than assuming.
4. Do not run a version-bumping testbuild/package workflow for this refinement unless the user explicitly asks for that workflow.
5. For the existing compatible `build-vs` tree, the expected build is:

```powershell
cmake --build build-vs --config Release --target VisualComp_VST3 VisualComp_Standalone
```

If CMake is absent from PATH, locate the installed Visual Studio CMake executable and invoke that verified path. Do not delete a build tree to resolve a cache mismatch.

6. Wait for actual successful completion of both targets. Report installation separately from compilation: a build configured with `VC2_INSTALL_PLUGIN=OFF` does not update the installed DAW plugin.
7. Launch the exact new standalone from the chosen build tree, using the current product version in its filename. Stop only a confirmed task-owned instance if it locks that target; do not close unrelated versions or DAWs.
8. Follow the repository's approved screenshot workflow. If using `build-vs/capture-raised-knobs.ps1`, inspect it first and verify the executable it launches. Treat that script/path as a local convenience, not a guaranteed tracked dependency.
9. Preserve user settings. Before isolating the exact version-specific settings file for reproducible captures, back it up and restore it afterward. Never remove broad AppData folders or another product version's settings.
10. Capture the whole interface, GR screen close-up, utility row, and open logo menu. Test motion separately. Keep test-only environment hooks inert in ordinary launches.
11. Leave the requested fresh standalone running and report its exact path.
12. Do not commit, stage all files, push, package, or publish unless the user separately requests those actions.

## 8. Definition of done and final handoff

The implementation is complete only when all four requirements have evidence. A feature that already works may be marked 'verified existing' instead of changed.

| Requirement | Required evidence |
| --- | --- |
| Matched GR background | Same-build screenshot and interior color comparison |
| Utility alignment | Bounds/centre checks and dock/zoom visual inspection |
| Rotating grooves | Existing renderer checks plus runtime motion verification |
| Inline version | Actual opened-menu screenshot matching current build version |
| No regressions | Successful VST3/Standalone builds and unchanged interaction checks |

Final response should state:

- What changed, and what was already correct and only verified.
- Files changed for this task, without attributing pre-existing staged changes to this work.
- Exact build commands and results, including whether the VST3 was installed.
- Screenshot/motion evidence paths and any untested conditions.
- The exact executable left running.
- Confirmation that no unrelated design changes, version bump, or Git publication occurred unless separately requested.

Do not claim pixel-perfect matching, animation verification, installation, or successful builds without corresponding evidence. If a necessary design departure or environmental blocker remains, describe it and ask for the narrow decision needed rather than silently expanding scope.
