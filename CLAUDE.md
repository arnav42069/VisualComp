# CLAUDE.md

Instructions for working in this repo. Read this before making changes.

## What this is

VisualComp — a JUCE compressor/EQ plugin (VST3 + Standalone) branded **Azazel Audio**
("by Arnav Singh"). The repo folder is still named `SimpleCompressor` from an earlier
project name; that has not been renamed and doesn't need to be. The version number is
deliberately not repeated here: as of 2026-08-06 it increments on every
`/package-release` run (see `bump-version.ps1`, under "Building" below), so a specific
number hardcoded in this file would go stale within a build or two — check
`CMakeLists.txt`'s `VC2_VERSION_STRING` for the actual current version.

- PRODUCT_NAME / plugin code: `Vc22`. Bumping the version string renames the built `.vst3`
  and its plugin ID — that's the mechanism for making FL Studio (or any host) rescan it as
  a new plugin, and it sidesteps FL's file lock since the new filename isn't the one FL has
  open.
- Palette lives in `src/Theme.h`: background `#1d1d1b`, accent `#ff7a1f`. UI font is Futura
  via `Theme::uiFontName()`.
- Logo: original full wordmark (with "audio" script) is
  `C:\Users\Arnav\Documents\Azazelaudio_white.svg`. The current mark-only crop is
  `C:\Users\Arnav\SimpleCompressor\azazel-logo FINAL white.svg` — treat this as the
  authoritative source for the embedded copy in `src/LogoSvg.h`. As of 2026-08-01 the
  embed's `viewBox` was fixed to match this file (`0 0 2177.7 732.7`); the prior box
  (`-14 -14 2210 632`) clipped the descender tails hanging below the A's — path data was
  unchanged, only the viewBox was wrong. If re-cropping/editing the SVG, always verify by
  rendering it (e.g. headless Chrome screenshot of a throwaway HTML) before committing —
  viewBox math is easy to get wrong blind.
- App icon (as of 2026-08-02): `resources/azazel-icon.png` (1024x1024 master) and
  `resources/azazel-icon.ico` (16..256px), wired into the build via `ICON_BIG` in
  `CMakeLists.txt`'s `juce_add_plugin` call — JUCE's own `juceaide` tool regenerates the
  actual `.ico` from that PNG at build time for both the Standalone `.exe` and the VST3
  bundle's `Plugin.ico`/`desktop.ini`, so the checked-in `.ico` itself is a reference copy,
  not something the build reads directly. The mark is just the first "A" glyph of the
  `azazel-logo FINAL white.svg` wordmark (isolated by rendering only that one `<path>` via
  headless Chrome, then cropping to its non-transparent bounding box with Pillow) — a full
  6-letter wordmark doesn't stay legible at 16x16, a single bold glyph does. Composited on
  a dark (`#1d1d1b`) rounded square with a thin accent-orange (`#ff7a1f`) border ring.

## Architecture

- `src/EqEngine.h` — 8-node parametric EQ (Bell/LoShelf/HiShelf/HPF/LPF/Notch), RBJ cookbook
  biquads (Notch uses the standard RBJ notch coefficients, gain-independent like HPF/LPF).
  Thread safety: GUI writes nodes under a `juce::SpinLock` (blocking, brief); the audio
  thread takes a non-blocking `tryLock` once per block to snapshot into an audio-thread-only
  `active` copy, and reuses stale coefficients if the lock is contended — it must never
  block.
- The EQ also drives "multiband-aware" compression: any node marked `linked` gets its own
  bandpass detector envelope; `dominantLinkedBand()` / `dominantLinkedBandDb()` feed the
  strongest one into the single shared compressor's detected level. As of 2026-08-02
  multiband dynamics (`VisualCompProcessor::multibandEnabled`) are **always on** — the old
  "MULTIBAND COMP" toggle button in `EqPanel`'s header was removed, and the flag is no
  longer restored from saved state (see `setStateInformation`), only ever forced `true`. The
  main editor's **MB** button (was **EQ**) still just opens/closes the docked panel. Nodes
  created via double-click on the graph default to `linked = true` (also toggleable per-node
  via `EqNodeState::linked`, either the graph's right-click "Link/Unlink" menu item or the
  Dynamic Island's **COMP** button). Every linked node additionally carries its own
  `thresholdDb`/`kneeDb`/`ratio`/`rangeDb`/`upward` (alongside the existing
  `attackMs`/`releaseMs`/`q`) and runs `kneeRatioGrDb()` (downward) or `upwardGrDb()`
  (upward — boosts material *below* threshold instead of cutting *above* it; same
  knee/ratio math, mirrored), clamped by `clampedDynamicGrDb()`, independently via
  `ParametricEq::applyDynamicBandGain` — a real per-band dynamic-EQ-style compression stage
  layered on top of the broadband compressor, editable through the main editor's
  Threshold/Knee/Ratio/Attack/Release knobs once a node is selected (see
  `VisualCompEditor::selectedBand`/`refreshBandButtons()`), plus a Threshold/Range pair on
  the graph's "Dynamic Island" popup (`NodeIsland`). FabFilter Pro-MB's model, not this
  plugin's own invention: **Threshold** is downward-only, 0..-60dB, with a logarithmic taper
  (`setupThresholdKnobRange()` in `EqEngine.h`, shared by `NodeIsland`'s knob and the main
  editor's `bandThresholdKnob` — they must stay identical since both edit the same value)
  skewed so 50% knob rotation lands on -20dB, a musically useful "sweet spot": finer control
  across the typical -20..0dB working range, the -40..-60dB tail compressed into the other
  half — same value as the graph's draggable "T" marker, `EqPanel::thresholdMarkerPos`,
  which clamps to the same -60dB floor. **Range** (+/-30dB) both clamps how far this band's
  dynamics can swing the gain and, via its sign, chooses downward vs upward (positive
  auto-engages `upward`, mirrored in the Island's Up/Down button, which remains the manual
  override). The Dynamic Island (`src/NodeIsland.h/.cpp`) also has **Freq** (the node's own
  x-axis position, 20Hz..20kHz) and the aforementioned **Comp** link toggle alongside
  Q/Threshold/Range/Up-Down/filter-type — `NodeIsland::onNodeEdited`/`EqPanel::onNodeEdited`
  relay every one of these edits up to `VisualCompEditor` so the Dynamics-pane progress bars
  repaint immediately, rather than waiting on the ~30Hz polling timer. It still is not N
  fully independent compressors sharing nothing (there's still the one broadband stage
  underneath), so don't describe it as true multiband in UI copy or docs — but it is
  genuinely per-band in its own dynamics.
- Q is edited per-node, not via a global knob: mouse wheel over a node in `EqPanel`,
  the right-click context menu's Q submenu, or the interactive knob on the "Dynamic Island"
  (`src/NodeIsland.h/.cpp`) — a small floating popup, also hosting the Upward/Downward toggle
  and a filter-type picker. As of 2026-08-03 the Island is docked to a fixed spot at the
  bottom of the graph (`EqPanel::updateIslandBounds()`), 5px above the frequency axis,
  sliding only left/right to stay centred under the selected node — it no longer flips
  above/below the node depending on the node's own vertical position, which was the previous
  behavior. As of 2026-08-06 the Island is also freely draggable: clicking anywhere on its own
  background (not a knob/button — those consume their own clicks first, and the Q/Threshold/
  Range/Freq labels pass clicks through via `setInterceptsMouseClicks(false, false)`) and
  dragging repositions it (`NodeIsland::mouseDown`/`mouseDrag`, clamped to stay within
  `EqPanel`'s own bounds). Once dragged, `NodeIsland::wasManuallyPositioned()` goes true and
  `EqPanel::updateIslandBounds()` — which otherwise re-centres the Island under the node on
  every 60Hz poll tick — stops doing so (only re-clamping into bounds on resize) until a
  *different* node is selected, at which point `NodeIsland::setTargetNode()` resets the flag
  and it re-docks under the newly-selected node by default. Re-clicking the same
  already-selected node (e.g. to start dragging it on the graph) does not reset the flag.
  `EqPanel` supports Ctrl/Cmd+Click multi-selection: dragging or wheel/Island-editing
  the most-recently-clicked node propagates the same relative change (dB delta for gain/additive
  fields, multiplicative ratio for freq/Q) to the rest of the selected set, each clamped to
  its own range. Right-click actions (type/link/remove) are always single-node.
- Detector edges (`EqNodeState::bwLowOct`/`bwHighOct`, shown as the shaded band strip +
  draggable triangle flags in `EqPanel::paint()`) can be grabbed anywhere along the graph's
  full height (`EqPanel::findEdgeNear()`), not just near the flag itself, and a plain click
  (no drag needed) jumps the border straight to the clicked frequency via
  `EqPanel::moveEdgeTo()`. Dragging an edge within 100Hz of another linked node's edge
  (`EqPanel::trySnapEdge()`) snaps to it and, as of 2026-08-03, forms a lasting bond
  (`EqPanel::snapPartner`/`linkEdges()`/`unlinkEdge()`) rather than a one-time nudge: dragging
  either bonded edge moves both together (`propagateJunctions()`), and moving a node's centre
  frequency carries any junction it participates in along with it so the bond survives the
  move instead of drifting apart. A new node snaps its default edges to a nearby linked node
  immediately on creation (`EqPanel::createNodeAt()`). Dragging an edge far enough from its
  partner (outside the 100Hz radius) breaks the bond. This bonding state is EqPanel-local UI
  state, not persisted with the session (rebuilding it from scratch on next launch is fine
  since it's just a drag convenience, not a parameter).
- `src/ClipEngine.h` — 3 output clip modes (Soft/Brickwall/Off) sharing one fixed lookahead
  delay line regardless of mode, so switching modes live never changes plugin latency or
  causes clicks.
- `src/LoudnessMeter.h` — peak/RMS ballistics plus an **approximate** LUFS (shelf+highpass
  K-weighting shape, continuous EMA rather than BS.1770's discrete gated blocks). Always
  label this "approx" in UI/docs, not delivery-spec certified.
- `src/LevelMeter.h/.cpp` — the dB/LUFS meter strip (right edge of the interface). Its
  geometry (`kBarW`/`kLabelPad`/`kLabelW`/`kGroupW`/`kMidGap`/`kSidePad` and the derived
  `kPreferredWidth`) is **public**, because `paint()` centres the dB+LUFS pair in whatever
  bounds it is handed and its guarantee of equal left/right outer padding only lands on
  `kSidePad` if the editor reserves exactly `kPreferredWidth`. The editor used to hard-code
  77 against a strip that needs 86, so the revealed LUFS group overhung its right edge while
  dB kept its left pad; `kLevelMeterW` now reads the constant. The dB-only and dB+LUFS
  positions are both computed from `getLocalBounds()` (`soloX`/`pairX`, cross-faded by
  `revealAmount`) — there is no tuned X offset. As of
  2026-08-03 each bar's unit label is a vertical (rotated `-90°`) label immediately to its
  right (`LevelMeter::drawSideLabel()`) rather than centred text below the bar, freeing the
  space below for a numeric peak-hold readout above the bars: the highest `meterPeakDb`
  sampled over the trailing 3 seconds (a 90-slot ring buffer at the 30Hz timer rate, see
  `peakHistory`/`kPeakHoldFrames`), not an ordinary decaying peak-hold line. `kLevelMeterW` in
  `PluginEditor.cpp` grew 56→77px to fit the side labels (which shrinks the adjacent Curve/GR
  column's width, `kCurveGrColW`, by the same amount — it's derived from `kLevelMeterW`, see
  that constant's own comment). `VC2_FORCE_METER_REVEAL` (screenshot-automation only, inert
  unless set) starts the LUFS bar already revealed, since synthetic clicks can't reach the
  app to toggle it live.
- New processor state (per-node EQ params, clip mode, panel-open flag) is stored as custom
  `ValueTree` properties, NOT new APVTS host-automatable parameters, so existing DAW
  automation lanes stay valid across versions. Persisted in both project state and user
  `.vcpreset` files.
- Docked EQ side panel pattern: the editor grows/shrinks via `setSize()` by `kEqPanelW` at
  runtime. In `paint()`, ALL "main content" drawing is wrapped in a single
  `g.addTransform(AffineTransform::translation(ox, 0))` rather than adding `ox` to every
  literal coordinate; `resized()`, by contrast, must manually add `+ox` to every
  main-content component's x. **Gotcha:** inside that `ox`-transformed `paint()` block,
  never read back a component's `getBounds()` (already has `+ox` baked in from `resized()`)
  — recompute geometry with the same literal (no-`ox`) formula used elsewhere, or it will
  double-shift.
- The Curve/GR column (transfer curve + gain-reduction meter) uses the same docked-panel
  `setSize()`-delta pattern, mirrored onto the *right* edge instead of the left: collapsed
  by default (`curveGrVisible = false`), it contributes 0 extra width; toggling it on adds
  `kCurveGrColW`. Unlike the EQ panel's `ox` (which shifts *everything* right), this only
  affects things anchored to the right edge — `kWidth`/`kContentW`/`kRightColW` still
  describe the fully-expanded layout unchanged, and header controls positioned via
  `kWidth - ...` (mixKnob, bypass, clip mode, the CURVE/GR button itself) must additionally
  subtract `cshift = curveGrVisible ? 0 : kCurveGrColW` or they hang off the narrower
  collapsed window — everything anchored to `kContentW` (waveforms, Dynamics pane, the level
  meter's left edge) needs no such adjustment since the content area's own width never
  changes. `totalEditorWidth()` is the one place both panels' deltas combine; use it rather
  than recomputing `kWidth + ...` inline, so the two docked panels can't drift out of sync.
- Rotary knobs are a **baked filmstrip**, not procedural drawing, as of 2026-08-25.
  `resources/knob-azazel-192x61.png` (192px cells, 61 frames, -135deg..+135deg) is compiled in
  via `juce_add_binary_data(VisualCompData ...)` in `CMakeLists.txt` — the repo's first and
  only binary-data target. **Gotcha:** JUCE's symbol mangler *deletes* hyphens rather than
  converting them, so the symbol is `BinaryData::knobazazel192x61_png`, not
  `knob_azazel_192x61_png`; check `build/juce_binarydata_VisualCompData/JuceLibraryCode/
  BinaryData.h` if a new asset won't resolve. `src/KnobStrip.h/.cpp` decodes the master once
  into a process-wide function-local static (~9MB resident, shared by every plugin instance)
  and area-averages it down to each requested dial size, caching per size (~2MB each, three
  sizes live in the UI: 78px dials, 49px Island, 45px Mix) — a hand-written premultiplied box
  filter, because JUCE's `highResamplingQuality` is only bilinear and aliases the grain badly
  at the 4x reduction Mix needs. `AzazelLookAndFeel::drawRotarySlider` now draws only the
  graduation ticks (first, so the baked shadow falls across them), the blit, and the value arc
  on top; the arc stays procedural since it's dynamically coloured and centre-filled for
  bipolar knobs. The strip assumes JUCE's default 270deg sweep and asserts it. Regenerate with
  `python scripts/make_knob_filmstrip.py --size 192 --frames 61 --light-deg 315 --accent
  ff7a1f --brush 0.052` — that `--brush` is deliberately deeper than the 0.030 concept strips
  to survive the runtime downsample. `--verify` proves the world/object reference-frame split
  numerically; run it after any change to the renderer.
- APVTS `SliderAttachment`/`ButtonAttachment` permanently bind one `Slider`/`Button` to one
  host parameter — they cannot be redirected to a different backing value at runtime. Any
  context-sensitive knob (e.g. a knob whose meaning changes with UI selection state) needs
  a second, manually-wired, non-APVTS slider shown/hidden via `setVisible()`, not a reused
  attachment.

## UI layout & visual design

The existing 78px main rotary knobs are the visual source of truth for VisualComp's
control design.

### Main knob size is a layout invariant

**Why it survives compaction (as of 2026-08-26):** `AzazelLookAndFeel::drawRotarySlider`
sizes the dial as `maxR = jmin(width, height) * 0.5f * 0.93f`. A main knob's slot is
100px wide (98 after `placeKnob`'s inset) and its dial region is `kKnobDialH`, so as
long as `kKnobDialH >= 98` the `jmin` picks **width** and the dial is width-limited —
every pixel of height past that is dead space, and removing it cannot shrink the knob.
That is what let `kKnobDialH` go 226 -> 100 (module 274 -> 138, `kCtrlH` 340 -> 214,
window 648 -> 522 tall) with `maxR` unchanged at 45.57. It is also why widening the gain
faders 80 -> 96px required widening the window 960 -> 992 rather than narrowing the knob
slots. If `kKnobDialH` is ever taken below the slot width the invariant flips and the
knob starts shrinking silently.

**The shared module constants** (`PluginEditor.cpp` anonymous namespace) — all seven
control-row modules (5 rotaries + GAIN IN + GAIN OUT) are laid out from these and
nothing else, which is what makes their title baselines / control centres / value
baselines / accent bars line up by construction rather than by tuning:
`kKnobSlotW`, `kKnobLblH`, `kKnobLblGap`, `kKnobDialH`, `kKnobValueH`,
`kKnobTopInset` (= label + gap; knobs honour it via their `topInset` property so the
dial clears the label, faders instead start below it because a linear `sliderPos` is
positional and a draw-time inset would make the travel dead), and `kModuleH`.
Row stack: `kCtrlTopStripH` -> `kKnobRowY` -> `kBarRowY`/`kBarRowH` -> `kUtilRowY`/
`kUtilRowH` -> `kCtrlBotPad`, with `kCtrlH` and `kHeight` **derived from** that stack
rather than being magic numbers the rows are carved out of. The right column derives
from `kRightColY`/`kRightColBottom`/`kRightColH` (these replaced `kGainOutSectionY/H`,
which tracked a full-height Gain Out column that no longer exists — both faders are
control-row modules now, and AUTO GAIN / LIM / SC moved to the `kUtilRowY` strip).
`kLevelMeterW` is `LevelMeter::kPreferredWidth`, not a hand-typed number.

- Do NOT reduce the 78px main rotary knob diameter merely to make the editor shorter.
- When compacting the lower control area, remove EMPTY/BLACK SPACE around the knobs
  rather than scaling the knobs down.
- The goal is shorter control MODULES, not smaller knobs.
- First reduce:
  - unused top padding above the knob
  - title-to-knob spacing
  - knob-to-value spacing
  - unused space below the value
  - bottom/accent-line padding
  - unnecessary panel top/bottom padding
- Keep all main rotary-control modules vertically consistent.
- The controls should feel tightly packaged like physical rack hardware, not uniformly
  scaled-down UI.

Conceptually:

CURRENT:
+---------------------+
|      THRESHOLD      |
|                     |
|                     |
|       ( KNOB )      |
|                     |
|                     |
|       -8.5 dB       |
|                     |
+---------------------+

TARGET:
+---------------------+
|      THRESHOLD      |
|       ( KNOB )      |
|       -8.5 dB       |
+---------------------+

The knob remains approximately the same physical size.

### Knob graduations

- Keep the existing baked knob filmstrip and current knob rendering architecture.
- Do not regenerate the filmstrip merely to change graduation marks.
- `AzazelLookAndFeel::drawRotarySlider()` owns the procedural graduation/tick drawing.
- Use approximately 36-48 subtle graduation marks around the active 270-degree arc.
- Preserve the existing value arc and parameter accent colours.
- Major/minor graduation variation is allowed if subtle.
- Do not put ticks through the inactive/dead arc.

### Visual language

VisualComp should resemble compact premium analog mastering hardware with modern
precision metering.

Use:
- dark brushed/aged metal
- subtle machining/brushing texture
- recessed panel surfaces
- restrained bevels
- soft highlights and shadows
- subtle tonal imperfections
- the existing orange accent language

Do not make the surface look heavily rusted, distressed, steampunk, glossy, or neon.

Expensive background textures should be cached rather than procedurally regenerated
on every repaint. The brushed-metal chassis backdrop is `VisualCompEditor::chassisTexture`,
rebuilt by `rebuildChassisTexture(w, h)` from `resized()` only when the window size actually
changes and blitted with one `drawImageAt` — it used to redraw several hundred primitives on
every repaint, including for the whole duration of a knob drag. Anything in it must be a pure
function of (w, h); the machined section bevels stay in `paint()` instead because they follow
the `ox`-shifted content and the texture (drawn before that transform) does not.

### Gain In / Gain Out faders

GAIN IN and GAIN OUT should look like physical studio/mastering faders rather than
generic JUCE sliders.

Prefer:
- wider recessed slots/tracks
- wider dimensional metallic fader caps
- visible centre/index line
- subtle scale graduations
- highlights/shadows consistent with the rotary knobs

They can become wider but should NOT remain unnecessarily tall. Their total section
height should align with the compact main rotary-control modules.

### Lower control-row geometry

The area below the waveform should be substantially shorter than the current layout.

Achieve this primarily through removal of unused vertical space.

Do not apply a blanket 50% scale transform to the contents.

Keep the 78px main knobs approximately unchanged while tightening the containing
panels around them.

The Gain In/Out sections, level-meter area and any other components sharing this row
should be vertically aligned to the same compact layout system.

### Level meter layout

When only the dB meter is visible:
- centre the dB meter mathematically within its available meter region
- do not use a hand-tuned X offset

When both dB and LUFS meters are visible:
- dB is on the left
- LUFS is on the right
- both have equal width and equal height
- use a consistent gap between them
- treat them as one centred group
- left outer padding and right outer padding must be equal

Calculate the group bounds from the parent bounds rather than positioning the two
meters independently.

The meter height should be reduced to fit the new compact lower control row, with
internal ticks, labels and fills recalculated for the new bounds rather than clipped.

### EQ / analyser height

When reducing the lower editor height, reduce the EQ/analyser vertical bounds to suit
the compact layout where necessary.

Recalculate graph/grid/label geometry for its new bounds rather than cropping the old
rendering.

Do not compromise node interaction or DSP behaviour.

### Layout implementation

Before introducing new coordinates, inspect `VisualCompEditor::resized()` and all
related component bounds.

Prefer shared geometry and calculated bounds over unrelated magic numbers.

Where practical, define/reuse shared values for:
- lower control-row height
- knob module height
- knob diameter
- label/value heights
- gain fader dimensions
- meter dimensions
- meter gap
- section padding

Be particularly careful with the existing EQ-panel `ox` translation and Curve/GR
`cshift` layout rules documented above. Do not create a second conflicting bounds path.

UI/layout work must not change DSP, APVTS parameter IDs/ranges, attachments,
automation, presets, sidechain behaviour, limiter behaviour, or compressor behaviour.

## Building

- Claude builds; the user does not build manually.
  `cmake --build build --config Release --target VisualComp_VST3` auto-installs to
  `C:\Program Files\Common Files\VST3\`.
- After every build (Standalone target included), launch the resulting
  `build\VisualComp_artefacts\Release\Standalone\VisualComp <version>.exe` (e.g. via
  `Start-Process`, left running rather than killed) and report its path — a standing
  habit, not a one-off request. This is separate from the screenshot workflow below,
  which launches its own throwaway instance and kills it after capturing.
- FL Studio locks the plugin binary while an instance is loaded — closing the loaded
  instance (not necessarily all of FL) before a rebuild is usually enough.
- `package.ps1` checks both `build` and `build-pkg` and picks whichever has the newer
  `VisualComp <version>.vst3` — don't assume one directory.
- Every `package.ps1` run also drops a loose, unzipped `Build Final\VisualComp <version>.exe`
  (in addition to the zip) so the user has a consistent, always-current copy to launch and
  test without unzipping — this is a standing behavior, not a one-off request.
- `testbuild.ps1` (`/testbuild` skill) is the fast path: Standalone-only build, no VST3, no
  zip. Copies the newest exe from `build\VisualComp_artefacts\Release\Standalone\` to
  `Build Final\Standalone Test\` and launches it. Use this for quick iteration;
  `/package-release` is for an actual release cut.
- Whenever a new `.cpp`/`.h` pair is added, update `CMakeLists.txt`'s `target_sources`.
- **Bumping the version is automated as of 2026-08-06** by `bump-version.ps1` (repo
  root), run automatically as step 1 of `/package-release` (see that skill's
  "Auto-versioning" section) — every packaged release now gets a fresh
  `PRODUCT_NAME`/plugin ID on its own, which is the mechanism that makes FL Studio (or
  any host) detect it as a new plugin on rescan instead of reusing a stale, locked
  one. As of 2026-08-06 (later revised same day) it adds 0.01 to the version as a
  real decimal number (`2.21` → `2.22` → `2.23`), not a string-integer bump on the
  trailing component — that earlier approach broke on single-digit fractions (e.g.
  `2.9` → `2.10`, a jump of +0.91, not +0.01). It rewrites
  every place that needs to match: `CMakeLists.txt`'s `VC2_VERSION_STRING` (drives
  `PRODUCT_NAME` and, see below, the `DIST_DIR` post-build block) and its `project()`
  version, `package.ps1`'s `$version`, both `.claude/skills/*/SKILL.md` docs, the two
  `installer/Install|Uninstall VisualComp <version>.bat` filenames + their internal
  title text, `installer/install.ps1`/`uninstall.ps1` (the latter additionally keeps
  the outgoing version in its "earlier releases" legacy-cleanup list, rather than
  losing track of it — see the script), `installer/README.txt`, `docs/manual.html`'s
  version *stamps* (title/cover/footer — NOT its dedicated per-release changelog
  section, which is deliberately left frozen as history; see "Screenshots & manual"
  below for the PDF regeneration step, which the script also runs), `About
  VisualComp <version>.md` (repo root — renamed, not just edited), and
  `build-macos/build.sh` + `build-macos/README.txt` (both hand-authored, hardcode the
  version in comments/`PLUGIN_NAME`/install-path text — these were gitignored until
  2026-08-06 so a prior bump silently left them at "2.1" for two full release cycles
  with nothing to catch it; they're tracked now). Before this script existed, missing
  one of these places didn't necessarily fail loudly: `package.ps1`'s manual-PDF path
  did exactly that silently for a full release cycle (2.1→2.2) until the 2.2→2.21 bump
  turned it into a hard `throw` instead once the stale-named PDF was deleted (see
  below) — CMake's `DIST_DIR` post-build step (next paragraph) failed the same way, as
  a build *error* rather than a silent staleness, since it's a required `COMMAND`, not
  an optional copy. Scripting the whole checklist closes off that entire class of bug;
  if a new place needs to track the version in the future, add it to
  `bump-version.ps1` rather than relying on this checklist being followed by hand.
- **`DIST_DIR` post-build step** (`CMakeLists.txt`, `if(WIN32 AND VC2_INSTALL_PLUGIN)`
  block): every `VisualComp_VST3` build refreshes a full `VisualComp <version> - Win and
  Mac/` distribution folder at the repo root (Windows VST3 + a Mac build-scripts folder +
  the manual PDF + the About.md + a README) — this is a *second*, separate distribution
  mechanism from `package.ps1`/`Build Final/`, entirely undocumented until 2026-08-03. Its
  paths are driven by the same `VC2_VERSION_STRING`, so it stays in sync automatically now
  — but a prior version bump (2.1→2.2) updated `PRODUCT_NAME` and missed this block, which
  then kept silently regenerating a folder still full of hardcoded "2.1" paths throughout
  the entire 2.2 cycle, until deleting the stale 2.1 manual PDF turned that staleness into
  a hard build failure. The `VisualComp <version> - Win and Mac[.zip| - Copy]` folders this
  produces are exactly the "frozen snapshot" folders `.gitignore`'d and described under
  Version Control below as "one-off copies" — they are not one-off in origin (this command
  regenerates the *current* version's folder on every build), but old *previous-version*
  copies left on disk alongside it (e.g. a lingering `VisualComp 2.1 - Win and Mac/` after
  the version moves to 2.21) are exactly that: stale one-offs, safe to ignore, not to be
  deleted without asking first.
- **AU (Mac only)**: `CMakeLists.txt`'s `FORMATS` is `VST3 AU Standalone` (as of
  2026-08-06). AU is an Apple-only plugin format — JUCE's own platform filtering
  (`_juce_get_platform_plugin_kinds` only appends `AU` when `CMAKE_SYSTEM_NAME STREQUAL
  "Darwin"`) makes this a confirmed no-op on Windows, so it doesn't change anything this
  machine builds or what `package.ps1` bundles. The only place it takes effect is
  `build-macos/build.sh`, which now builds and auto-installs both VST3 and AU in one pass
  when run on an actual Mac (`~/Library/Audio/Plug-Ins/VST3/` and `.../Components/` — Logic
  Pro needs the AU since it doesn't load VST3). There is no way to produce a `.component`
  bundle on Windows; don't try.
- **Code signing (Windows)**: `package.ps1` signs the packaged `.exe` and the VST3's inner
  binary with `signtool` if a certificate is configured via env vars — `VC2_CODESIGN_PFX` +
  `VC2_CODESIGN_PASSWORD` (a `.pfx`/`.p12` file), or `VC2_CODESIGN_THUMBPRINT` (a cert
  already in the Windows cert store, e.g. an EV cert on a hardware token/cloud HSM).
  `VC2_CODESIGN_TIMESTAMP_URL` optionally overrides the default RFC3161 timestamp server.
  None of these are set as of 2026-08-06 (no certificate owned yet), so packaging proceeds
  unsigned with a yellow console warning — that's expected, not a bug. `signtool.exe` is
  located via `PATH` or by searching `Windows Kits\10\bin\*\x64\`. A self-signed cert only
  removes "Unknown Publisher" from the UAC prompt; it does **not** stop Windows SmartScreen
  flagging the binary — only a real CA-issued certificate does that, and even then a new
  certificate can still get flagged until it earns download reputation. Verified working via
  a throwaway self-signed test cert (signed, `Get-AuthenticodeSignature` confirmed a real
  signature chain that correctly reports untrusted-root — the expected result for a
  non-CA cert); the test cert was deleted from the store afterward, nothing shipped uses it.

## Version control

- As of 2026-08-02 this folder is a local git repo (no remote configured). Commit
  history is how "versions" of the source are maintained going forward — commit when
  the user asks, following the standard commit-hygiene rules (new commits over amends,
  no `--no-verify`, never push without being asked).
- `.gitignore` excludes: `build/`, `build-pkg/`, `build-macos/`, `Build Final/` (all
  regenerable via CMake / `package.ps1` / `testbuild.ps1`), `*.zip` release archives,
  `.claude/settings.local.json` (per-user, not shared config), and the older frozen
  snapshot folders `VisualComp 2/`, `VisualComp 3/`, `VisualComp 2.1 - Win and Mac/`,
  `VisualComp 2.1 - Win and Mac - Copy/` (one-off copies made on request, not the living
  source tree — that's `src/` plus this file and the build/installer scripts at repo
  root). Don't add these back without checking with the user first; they're large and/or
  redundant with what's already tracked.
- git identity is set locally (repo-level `user.name`/`user.email`, not `--global`) since
  this machine had none configured.

## Screenshots & manual

- Capture via the Standalone build + `PrintWindow` with flag `2` (`PW_RENDERFULLCONTENT`).
  Synthetic mouse clicks do not reach the app (Windows foreground restrictions) — drive UI
  state via env vars instead (e.g. `VC2_TOUR_STEP=<n>`, `VC2_FORCE_EQ_OPEN=1`); both are
  inert unless set.
- The first capture after a fresh build sometimes returns a bogus tiny window due to
  timing — a longer settle sleep (~3s) before `GetWindowRect` fixes it; harmless to just
  recapture.
- The Standalone build persists its host state (APVTS + custom processor state, i.e. the
  equivalent of a DAW project's plugin chunk) at
  `%APPDATA%\VisualComp <version>\VisualComp 2.settings` — the folder carries the
  current `PRODUCT_NAME` (so it moves every time `bump-version.ps1` runs, see
  "Building" above) but the filename itself still doesn't have the point-release
  number on it, just the fixed "VisualComp 2". A screenshot script must
  delete this exact file before each launch, or stale state (e.g. the EQ panel having been
  left open in a previous run) silently leaks into the next capture — the window opens
  wider/narrower than the script assumes, and a size-based crop then grabs the wrong
  region, which looks like a rendering bug but isn't.
- Tall/narrow crops (e.g. a docked side panel) will blow up hugely if allowed to render at
  full page width in the manual — always cap with an inline `max-width` sized so the
  resulting height stays reasonable.
- Manual full-bleed black requires `@page{margin:0}` plus background/padding on each
  `.page` div. After any content addition (especially figures), verify no page overflows
  the printable height via injected JS + `--dump-dom`.
- Never edit manual HTML via PowerShell `Get-Content`/`Set-Content` without an explicit
  UTF-8 encoding — PS 5.1's default codepage corrupts em dashes and other non-ASCII chars.
- The manual's source is `docs/manual.html`; the shippable artifact is
  `docs/VisualComp <version> - User Manual.pdf`, rendered via headless Chrome
  (`--print-to-pdf`). `package.ps1` expects that exact versioned filename and `throw`s if it's
  missing — as of 2026-08-03 it derives the name from `$version` (previously hardcoded to
  `"2.1"`, which silently went stale across the 2.1→2.2 bump: the packaged zip kept shipping
  the old 2.1 manual for an entire version). Regenerate the PDF (and delete the previous
  version's, so a stale one doesn't linger in `docs/`) any time `manual.html` changes, before
  running `/package-release`. The `getBoundingClientRect().height` overflow check (previous
  bullet) needs the instrumented copy to live inside `docs/` (not a scratch dir) so its
  `img/*` relative paths resolve — otherwise broken-image placeholders under-report each
  figure's true height and the check passes when the real render wouldn't.
- Figures whose source screenshot has a much taller/narrower aspect ratio than the figure's
  own `max-width` implies (e.g. a full-height side panel crop dropped into a wide figure slot)
  can blow a page's height budget even at a seemingly-small `max-width` — the fix is usually a
  smaller `max-width` and/or a wider source crop (include more surrounding UI) rather than
  guessing at CSS; re-measure after any image swap, not just after text edits, since swapping
  an image changes a page's height too.

## Presets

- `src/Presets.h`: `FactoryPreset` values must stay inside the APVTS parameter ranges
  (threshold -60..0, knee 0..20, ratio 1..20, attack 0.1..200ms, release 1..2000ms, gain
  -24..24, mix 0..1, mode 0=VCA/1=FET/2=Opt/3=Tube). `clipMode` matches `ClipEngine.h`
  (0=Soft, 1=Brickwall, 2=Off) — mastering-facing presets should default to Brickwall,
  deliberate-colour presets (tape/tube/parallel crush) to Soft, and presets meant to stay
  maximally clean to Off.
- User presets live at `Documents\Azazel Audio\VisualComp 2\Presets\*.vcpreset` —
  deliberately still versioned "VisualComp 2", not "2.1", so presets survive point
  releases. Don't rename that folder.
- As of 2026-08-06 user presets (not factory ones) carry an **Author** field, aimed at
  producers sharing preset packs. `VisualCompEditor::saveUserPreset()` first shows a themed
  `AlertWindow` prompt (same "modal, then continue" shape as the Auto-Analyze wizard below)
  asking for an author name — pre-filled with whichever name was typed last
  (`VisualCompProcessor::currentPresetAuthor`) so a whole pack doesn't need retyping it every
  save, blank the first time — then hands off to the existing `FileChooser` Save flow
  (`launchSavePresetFileChooser()`). The name is written into the `.vcpreset` XML as a
  `presetAuthor` property (same custom-`ValueTree`-property pattern as `compMode`/`clipMode`,
  not an APVTS parameter) and also persisted in the processor's own session state
  (`getStateInformation`/`setStateInformation`) alongside `presetName`, so it survives a DAW
  project reload. There's no room for an always-visible "by X" label in the pixel-packed
  preset header row (see `resized()`'s preset-strip block), so on load it surfaces instead as
  a tooltip on the preset name button (`VisualCompEditor::setPresetAuthor()`,
  `presetButton.setTooltip(...)`) — this needed a `juce::TooltipWindow` member added to
  `VisualCompEditor` (`tooltipWindow`), since `setTooltip()` text alone renders nothing
  without one live in the component hierarchy. Factory presets and Smart Master+-generated
  presets explicitly clear it (`setPresetAuthor({})`) so a stale author from a
  previously-loaded user preset doesn't linger. `VC2_FORCE_SAVE_PRESET_DIALOG` (screenshot
  automation only, inert unless set) opens the author prompt on launch.

## General conventions

- Don't add new APVTS parameters for UI-only or per-node state; use custom `ValueTree`
  properties (see above) so host automation lanes never change shape between versions.
- Match existing patterns for thread safety (SpinLock + tryLock snapshot) instead of
  introducing new synchronization primitives.
- After any UI change, build and take a screenshot (see above) before declaring the work
  done — don't rely on compilation success alone as evidence a layout change looks right.
