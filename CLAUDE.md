# CLAUDE.md

Instructions for working in this repo. Read this before making changes.

## What this is

VisualComp 2.2 — a JUCE compressor/EQ plugin (VST3 + Standalone) branded **Azazel Audio**
("by Arnav Singh"). The repo folder is still named `SimpleCompressor` from an earlier
project name; that has not been renamed and doesn't need to be.

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

## Architecture

- `src/EqEngine.h` — 8-node parametric EQ (Bell/LoShelf/HiShelf/HPF/LPF/Notch), RBJ cookbook
  biquads (Notch uses the standard RBJ notch coefficients, gain-independent like HPF/LPF).
  Thread safety: GUI writes nodes under a `juce::SpinLock` (blocking, brief); the audio
  thread takes a non-blocking `tryLock` once per block to snapshot into an audio-thread-only
  `active` copy, and reuses stale coefficients if the lock is contended — it must never
  block.
- The EQ also drives "multiband-aware" compression: any node marked `linked` gets its own
  bandpass detector envelope; `dominantLinkedBand()` / `dominantLinkedBandDb()` feed the
  strongest one into the single shared compressor's detected level. With **MULTIBAND COMP**
  (the toggle in `EqPanel`, `multibandEnabled`) off, this is **one compressor with a
  frequency-aware detector**, not N independent per-band compressors — a deliberate,
  documented simplification; don't describe it as true multiband in UI copy or docs.
  Clicking the toggle on also auto-links every currently-enabled node (so the button does
  what its name says in one click); turning it off does not unlink anything, since links
  still feed the frequency-aware detector either way. With it on, each linked node
  additionally carries its own `thresholdDb`/`kneeDb`/`ratio`/`rangeDb`/`upward` (alongside
  the existing `attackMs`/`releaseMs`/`q`) and runs `kneeRatioGrDb()` (downward) or
  `upwardGrDb()` (upward — boosts material *below* threshold instead of cutting *above* it;
  same knee/ratio math, mirrored), clamped by `clampedDynamicGrDb()`, independently via
  `ParametricEq::applyDynamicBandGain` — a real per-band dynamic-EQ-style compression stage
  layered on top of the broadband compressor, editable through the main editor's
  Threshold/Knee/Ratio/Attack/Release knobs once a node is selected (see
  `VisualCompEditor::selectedBand`/`refreshBandButtons()`), plus a Threshold/Range pair on
  the graph's "Dynamic Island" popup (`NodeIsland`). FabFilter Pro-MB's model, not this
  plugin's own invention: **Threshold** is downward-only (-inf..0dB, `-96` standing in for
  -inf — same value as the graph's draggable "T" marker, `EqPanel::thresholdMarkerPos`).
  **Range** (+/-30dB) both clamps how far this band's dynamics can swing the gain and, via
  its sign, chooses downward vs upward (positive auto-engages `upward`, mirrored in the
  Island's Up/Down button, which remains the manual override) — this replaced an earlier,
  confusing design where Threshold itself went positive to mean "upward" and had no ceiling
  at all. It still is not N fully independent compressors sharing nothing (there's still the
  one broadband stage underneath), so keep the "not true multiband" framing for the OFF
  case, but the ON case is now genuinely per-band in its own dynamics.
- Q is edited per-node, not via a global knob: mouse wheel over a node in `EqPanel`,
  the right-click context menu's Q submenu, or the interactive knob on the "Dynamic Island"
  (`src/NodeIsland.h/.cpp`) — a small floating popup `EqPanel` shows above whichever node is
  currently selected, also hosting the Upward/Downward toggle and a filter-type picker.
  `EqPanel` supports Ctrl/Cmd+Click multi-selection: dragging or wheel/Island-editing the
  most-recently-clicked node propagates the same relative change (dB delta for gain/additive
  fields, multiplicative ratio for freq/Q) to the rest of the selected set, each clamped to
  its own range. Right-click actions (type/link/remove) are always single-node.
- `src/ClipEngine.h` — 3 output clip modes (Soft/Brickwall/Off) sharing one fixed lookahead
  delay line regardless of mode, so switching modes live never changes plugin latency or
  causes clicks.
- `src/LoudnessMeter.h` — peak/RMS ballistics plus an **approximate** LUFS (shelf+highpass
  K-weighting shape, continuous EMA rather than BS.1770's discrete gated blocks). Always
  label this "approx" in UI/docs, not delivery-spec certified.
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
- APVTS `SliderAttachment`/`ButtonAttachment` permanently bind one `Slider`/`Button` to one
  host parameter — they cannot be redirected to a different backing value at runtime. Any
  context-sensitive knob (e.g. a knob whose meaning changes with UI selection state) needs
  a second, manually-wired, non-APVTS slider shown/hidden via `setVisible()`, not a reused
  attachment.

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
  `%APPDATA%\VisualComp 2.2\VisualComp 2.settings` — note the folder carries the current
  product name but the filename still doesn't have ".2" on it. A screenshot script must
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

## General conventions

- Don't add new APVTS parameters for UI-only or per-node state; use custom `ValueTree`
  properties (see above) so host automation lanes never change shape between versions.
- Match existing patterns for thread safety (SpinLock + tryLock snapshot) instead of
  introducing new synchronization primitives.
- After any UI change, build and take a screenshot (see above) before declaring the work
  done — don't rely on compilation success alone as evidence a layout change looks right.
