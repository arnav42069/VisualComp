# VisualComp: recessed hardware UI redesign brief

Status: DESIGN OPTIONS AND IMPLEMENTATION INSTRUCTIONS ONLY. Prepared 9 September 2026.

This document proposes a visual redesign inspired by the user's photograph. It does not mean that a direction has been selected or that implementation, compilation, installation, version changes, or publishing have been requested. Read the entire brief before implementing it in a later, explicitly authorized task.

## 1. Objective and recommendation

Make VisualComp feel like a physical audio instrument: a satin metal faceplate, controls seated in machined pockets, black displays behind substantial bezels, readable engraved-looking legends, and one consistent light source. The requested quality is convincing depth, especially on buttons and surrounding surfaces.

**Recommended starting direction: A — Satin Silver, with moderate sculpted depth, dark inset buttons, and the existing Azazel orange.** It has the strongest relationship to the reference while allowing the current knobs and dark signal displays to remain useful. This is a recommendation for the user to select, not an already approved design.

The first implementation should be a surface redesign of the existing layout. Keep every current control, its position, and its behavior. Layout restructuring and new features are separate work.

### Reference interpretation

Reference: the user's `Photo 1.jpg`, supplied in the conversation. Local attachment:

`C:/Users/arnav/.codex/codex-remote-attachments/01a05162-abb4-7890-8ab9-8eaff0dbb834/C16296F9-2908-41CF-98AF-8C01B480A845/1-Photo-1.jpg`

The useful reference is the plugin interface in the middle of the photograph, approximately between vertical pixels 322 and 797 of the supplied 591 × 1280 image. The social-media controls and the video of a person are not design references. The photo is compressed and partially cropped; exact materials, shadow widths, and colors cannot be measured reliably from it. The specifications below are proposed design values, not purported measurements.

Borrow these visible qualities:

- A light, gently shaded silver faceplate with continuous material across sections.
- A large black waveform cavity surrounded by a rounded silver bezel.
- Small dark controls seated in lighter recessed pockets.
- A deep pill-shaped black slider slot with a compact physical handle.
- Small legends that appear etched into the faceplate.
- Highlights and shadows that make neighboring pieces appear to share one physical environment.

The reference mixes **raised** and **recessed** elements. The outer silver display bezel appears raised; the black screen inside it is recessed. Small dark buttons sit inside recesses. Reversing every shadow indiscriminately would lose this relationship.

Do not reproduce the other product's logo, lettering, artwork, exact layout, sample-slicing controls, or decorative imagery. Translate the surface treatment to VisualComp's compressor/EQ interface. Preserve Azazel branding and existing signal meanings.

## 2. Design directions to choose from

All three options use the same control layout and functional behavior. Choose one complete direction for an initial implementation; do not build three themes or add a theme switcher.

| Direction | Appearance | Relationship to the reference | Main tradeoff |
| --- | --- | --- | --- |
| **A — Satin Silver (recommended)** | Warm light silver chassis; dark recessed buttons and screens; existing metallic knobs; orange active details | Closest to the reference's silver body and obvious depth | Requires a careful split between dark text on metal and light text on screens |
| **B — Smoked Titanium** | Medium gray metal chassis; charcoal pockets; quieter highlights; orange active details | Keeps the sculpted hardware feeling with less overall brightness | Mid-gray surfaces need deliberate text contrast and sufficiently distinct recesses |
| **C — Graphite Console** | Warm charcoal chassis; near-black recesses; narrow silver edge highlights; orange active details | Transfers the depth vocabulary while retaining the current dark identity | Furthest from the photo's light appearance; weak shadows can disappear into the body |

### Independent design choices

These choices refine the selected direction. Defaults form a coherent recommended recipe; alternatives are for discussion, not instructions to implement every combination.

| Choice | Recommended | Alternative | Consequence |
| --- | --- | --- | --- |
| Depth strength | Moderate sculpted recesses | Crisp shallow machining | Moderate is closer to the photo; shallow is calmer in a dense interface |
| Button face | Charcoal cap seated inside a recessed well | Faceplate-colored inset face | Charcoal makes button boundaries and labels easier to read |
| Screen surround | Thin raised metal lip, then a dark inner wall | Flush milled aperture | Raised lip more closely evokes the photo; flush takes less visual space |
| Typography treatment | Solid readable lettering with very slight etched relief on static legends | Flat printed legends | Etching adds tactility; flat lettering is the safest at small sizes |
| Active indication | Compact indicator plus a visibly seated active face | Restrained tinted face | Compact indicators keep orange scarce; tint can help very small selected controls |
| Faceplate texture | Extremely fine horizontal brushing | Smooth satin finish | Brushing relates to existing knobs; satin reduces visual noise |
| Knob artwork | Keep the current filmstrip | New matching filmstrip as a separate later task | Keeping it reduces scope and preserves the existing rendering and interaction work |
| Layout | Existing compact layout | Later layout revision | Surface work can be evaluated without also moving familiar controls |

Avoid very deep soft shadows on every element. The photograph's softness should inspire the major screen bezels, not make every tiny button look like a padded cushion.

### Decision record

Fill this in when handing the document to an implementing model:

```text
Selected direction: UNSELECTED (recommend A — Satin Silver)
Depth: moderate sculpted (recommended)
Button face: dark inset (recommended)
Screen surround: restrained raised lip over a recessed display (recommended)
Legends: readable solid text, subtle etching only on larger static legends
Knobs: preserve current filmstrip and dimensions
Layout: preserve current bounds and docking behavior
Implementation authorized: NO — current task is documentation only
Build/launch authorized: NO — current task is documentation only
Commit/push authorized: NO
```

An explicit later user instruction can select a direction and authorize implementation. If they say to use the recommendation, use the recipe above. Do not infer permission to implement from receiving or reading this document alone. Ordinary implementation decisions within a selected direction do not require repeated approval.

## 3. Scope and project rules

Read [AGENTS.md](../AGENTS.md), [CLAUDE.md](../CLAUDE.md), and the available VisualComp UI skill before future edits. Inspect the live source and `git status` again: the repository can change after this brief was written.

The light chassis proposed in A and B deliberately departs from the older dark-metal style guidance. Selecting A or B is approval for that visual departure only. It does not waive layout, audio, state, or interaction constraints.

### Preserve without reinterpretation

- DSP, audio routing, sidechain behavior, oversampling, latency, compressor behavior, limiter/clip behavior, and meter calculations.
- Every APVTS parameter ID, range, default, skew, attachment, and host-automation gesture.
- Existing non-APVTS per-band controls and their edit/undo callbacks. Do not attach them to newly invented host parameters.
- Preset contents, metadata, serialization, loading/saving, and the existing `Documents/Azazel Audio/VisualComp 2/Presets` location.
- The SpinLock plus audio-thread `tryLock` pattern in `EqEngine.h`.
- Node linking, multiselection, edge dragging/snapping, bonds, node selection, and Island dragging.
- Current keyboard, wheel, drag, double-click reset, text-field editing, focus, and undo/redo behavior. Main rotary, band-context, and gain-fader value boxes are explicitly noneditable; keep them that way.
- Existing menu commands, help, licensing/demo flows, and test-only controls.
- Current main knob size, cached filmstrip renderer, and local changes to knob assets.

Do not add features, change parameter semantics, convert the editor to a web UI, add a GPU framework, create a theme setting, or install new dependencies for this visual task.

### Existing changes and historical documentation

At brief creation the working tree already contained version/document changes from an earlier build attempt and edits to knob artwork and its generator. They are outside this documentation task. Do not revert, regenerate, stage, or overwrite them merely to get a clean baseline.

Some project prose does not match the inspected source:

| Topic | Current source observation | Instruction for this redesign |
| --- | --- | --- |
| Preset author | `presetAuthorEditor` is an always-visible editable field; the tooltip also exists | Keep and style the field; do not remove it based on older tooltip-only descriptions |
| Dynamics context | Threshold, Knee, Ratio, Attack, and Release each have a separate band-context slider | Apply the same material, dimensions, and text treatment to both members of each pair; preserve normalization, value arcs, and parameter-specific colors |
| Island controls | Q, Threshold, Range, Frequency, and Gain are present | Do not omit GAIN when styling the Island |
| Island placement | `syncPositionsFromProcessor`, `syncPositionsToProcessor`, and `processor.islandPositions` exist, despite older local-only guidance | Make no placement or serialization changes; report the discrepancy separately if relevant |
| Test build | The actual script builds VST3 and Standalone and bumps the version | Do not rely on an older description calling it Standalone-only |

A visual redesign is not a reason to resolve these behavioral/documentation discrepancies. Preserve running behavior and flag any conflict that would require a functional change.

## 4. Visual system: material, depth, and light

### One light direction

Use a fixed light from the **upper left**, matching the existing knob artwork's documented 315-degree lighting convention. Describe positions spatially when implementing; graphics APIs use different angle conventions.

| Surface | Upper/left side | Lower/right side | Result |
| --- | --- | --- | --- |
| Raised metal lip or knob cap | Bright narrow edge | Dark edge and short outside shadow | Projects above the faceplate |
| Recessed well | Dark inner wall/shadow | Light inner edge or reflected rim | Sits below the faceplate |
| Engraved static legend | Dark readable glyph | Very faint light duplicate displaced down/right | Appears cut into the surface |

Do not use the raised-edge recipe for the inside of a recess. An outside drop shadow around a dark rectangle alone makes a floating dark tile, not a cutout.

### Four structural surface roles

Use a small coherent system corresponding to the existing theme's surface hierarchy:

1. **Faceplate:** continuous base material of the instrument.
2. **Well:** shallow cavity containing a button, text field, or group of related controls.
3. **Screen:** deepest near-black cavity containing live signal or readouts.
4. **Cap/lip:** a physical button cap, fader cap, knob, or display bezel above its surrounding well.

Hover and pressed are variations of a control state, not additional material layers. The names are conceptual roles; they do not require four nested components.

### Dimensions at 100% editor zoom

These are initial rendering tokens in logical JUCE pixels, not reasons to change component bounds. Scale through the editor's existing zoom mechanism; do not multiply geometry by zoom a second time.

| Detail | Starting value | Limit or fitting rule |
| --- | --- | --- |
| Small button radius | 3 px | Reduce for very short existing controls; never exceed half their height |
| Readout/author field radius | 4 px | Match nearby dark pockets |
| Display outer radius | 6 px | Fit within existing component space |
| Large group radius | 7 px | Use on real functional groups only |
| Bright/dark edge | 1 px nominal | Keep crisp at supported scale factors |
| Button inset wall | 1–2 px | Prefer 1 px on narrow arrows and band buttons |
| Display inset wall | 3–4 px where margins permit | Do not cover axes, node edges, waveform data, or hit areas |
| Button inner shadow | 2–3 px falloff | Bounded to the inside; no blurred full-component bitmap per paint |
| Display inner shadow | 4–6 px falloff where available | Strongest at top/left, clear of information |
| Physical press displacement | 1 px down | Paint-only offset for cap, label, and indicator together |
| Engraving highlight offset | 0.5–1 px down/right | Static legends only; skip at small sizes if it blurs |
| Indicator | Approximately 3 × 2 px | Use a short bar or dot plus another visible state cue |

Decorative size must yield to the current information and hit geometry. If a bezel cannot fit the current margin, reduce the bezel. Do not silently squeeze a graph or shrink a knob to fit decoration.

## 5. Color tokens and contrast

The following values are starting specifications, not sampled colors from the photograph. Validate their final composited appearance. A neutral base with several low-opacity overlays can produce a very different visible color.

| Semantic role | A — Satin Silver | B — Smoked Titanium | C — Graphite Console |
| --- | --- | --- | --- |
| Faceplate base | `#CACAC5` | `#858883` | `#242623` |
| Upper faceplate light | `#E1E1DC` | `#A2A69F` | `#343731` |
| Lower faceplate shade | `#AFB1AC` | `#6B6F69` | `#1B1D1A` |
| Faceplate primary text | `#202421` | `#101410` | `#EDF0E7` |
| Faceplate secondary text | `#383D38` | `#1C211B` | `#B8C0B2` |
| Well body | `#A3A69F` | `#595E57` | `#171A16` |
| Button cap | `#30352F` | `#292E28` | `#30362D` |
| Button hover cap | `#3C423A` | `#363D33` | `#3C4537` |
| Pressed/latched cap | `#20251F` | `#1B211A` | `#1D241A` |
| Screen background | `#101310` | `#101310` | `#0D100C` |
| Screen/button primary text | `#EDF1E7` | `#EDF1E7` | `#EDF1E7` |
| Screen secondary text | `#ADB7A6` | `#ADB7A6` | `#ADB7A6` |
| Azazel active detail | Existing `Theme::accent`, currently `#FF7A1F` | Same | Same |

Use white/black at controlled alpha for edge lighting; do not use orange as an edge highlight on an idle control. Starting ranges: raised highlight 15–35% white, recess shadow 20–40% black near its edge, recess lower highlight 10–25% white. Use the lower values on small controls and tune against the selected plate. These are rendering ranges, not an instruction to stack all maxima.

### Separate text by its actual background

Do not globally turn `Theme::text` dark when adopting silver. It is used on both dark screens and outer panels. That shortcut would make graphs, menus, and fields unreadable.

Add or adapt a small set of explicit role tokens in `Theme.h`, for example `plateText`, `plateTextSecondary`, `screenText`, `screenTextSecondary`, `buttonText`, and `focusEdge`. These names are **proposed**, not existing APIs. Keep existing compatibility tokens where callers are not being changed, and migrate inspected drawing sites deliberately.

Respect per-component `setColour` overrides, not only LookAndFeel defaults. The Author editor, slider text boxes, labels, menus, and dialog controls have their own color assignments.

- Target at least 4.5:1 contrast for essential labels, values, and button text against their final background. Check actual text/background pairs, including hover, disabled, and active treatments.
- Target at least 3:1 for essential control boundaries/focus indicators against adjacent surfaces. These are project acceptance targets, not a claim of formal accessibility certification.
- Keep primary lettering solid. A faint embossed shadow is decoration, never the only readable version of a label.
- Orange on silver is an indicator color, not a good default for small text. Put orange indicators on a dark backing where needed.
- Keep existing input/output, warning, EQ-node, and meter-ramp meanings. Do not make all traces orange or all bands monochrome.
- Selected state must also be visible through depth, marker shape, contrast, or the existing label; color alone is insufficient.

### Typography

Keep `Theme::uiFontName()` and its current Futura fallback chain. Keep existing units and value formatting. Do not add font downloads or alter the wordmark.

Keep current font sizes and the label/value hierarchy during the first surface pass. The inspected setup uses 17 px main parameter-label fonts and 16 px fader-label fonts; readout and button fonts have their own sizing paths. Do not shrink these labels to imitate the photograph's faint small legends. Existing tiny band selectors and arrows need their own compact treatment. Any later typography refinement must retain readability within the current bounds, especially at small supported zoom.

Reduce extra tracking before reducing the font for long button labels. Preserve mixed case in user preset names and author names. Do not apply an etched duplicate to live values, editable text, axes, tooltips, or small button labels: it reduces clarity.

## 6. Button rendering and interaction states

### Default construction

Draw a shallow recess into the faceplate, then a dark cap seated within it. The cap may have a very narrow raised top edge, but the surrounding cavity should remain visible. A button should still look seated when idle.

Draw order:

1. Establish the outer visual rectangle inside the component's existing bounds.
2. Draw the well base with the selected radius.
3. Clip shadow/highlight strokes or gradients to the well: dark top/left inner wall, restrained lower/right bounce.
4. Draw the cap inside the well, using the appropriate state color.
5. Draw its restrained face highlight or edge, reduced in a pressed state.
6. Draw the state marker, icon, and text with coordinated paint offsets.
7. Draw any focus indication above the surface treatment.

Keep the hit rectangle and JUCE component bounds unchanged. Painting movement must never call `setBounds`, change a value, invoke a callback, or toggle a button.

### State matrix

Treat enabled, hovered, physically pressed, focused, and logically active as separate inputs. A momentary press is not a persistent ON state.

| State | Surface | Legend/indicator | Behavior |
| --- | --- | --- | --- |
| Idle off | Neutral dark cap inside a visible well | Light readable legend; no active indicator | Existing click action |
| Hover off | Slight neutral lift in cap brightness | Legend stays clear; no orange activation | Hover must not appear latched |
| Mouse held | Cap visually seats 1 px deeper; inner shadow tightens | Label and marker shift with cap | Release follows existing JUCE semantics, including release outside |
| Toggle on | Seated dark face stays distinguishable after release | Compact active marker and clear label contrast | Read existing `getToggleState()` |
| Hover on | Retain ON cues with a small neutral hover change | Active marker remains visible | Do not visually cancel ON |
| Pressed on | Additional physical press treatment | Keep active identity until callback changes state | Do not predict state in the painter |
| Keyboard focused | Normal state plus a crisp in-bounds focus cue | Never rely on a diffuse glow alone | Preserve current focus eligibility and ordering |
| Disabled off | Reduced emphasis but readable surface and legend | No hover or pressed emphasis | Respect existing disabled behavior |
| Disabled on, if possible | Disabled treatment while retaining a subdued state marker | Still communicates stored ON state | Do not reset or disguise the actual value |

### Functional roles

- **Momentary actions:** Save, preset arrows, analysis actions, help/navigation controls. Never leave them visually latched unless the current control already exposes a persistent state.
- **Toggles:** MB panel, Curve/GR panel, Auto Gain, Limiter, Sidechain, and similar controls. Use their actual state ownership; a panel toggle is not an audio bypass switch.
- **Bypass:** use the existing warning/red semantic when bypass is active. Make active bypass unambiguous. Do not convert it to orange merely for palette uniformity.
- **Mode/menu controls:** show their current text/state. An open menu and a selected compressor/clip mode are different concepts. Do not create toggle logic for a menu button.
- **Band selectors:** retain node identity colors and unmistakable selection. A stronger selected face is acceptable here because controls are small. Preserve the existing `nodeSelect` branch and parent-drawn identity ring until intentionally restyled together.
- **Preset name:** reads like a recessed display but is still an actionable preset control. Keep hover feedback and existing menu access.
- **Author field:** is editable text, not a button. Keep caret, selection, placeholder, focus, and commit-on-return/focus-loss behavior.

Prefer existing component properties and centralized helpers for visual roles. Do not distribute new tests of button display text throughout multiple painters. Existing special cases must be preserved or carefully mapped to an equivalent role without changing callbacks.

No new animation timer is needed. Implement correct static states first. Any later transition should be cosmetic and must not delay response or parameter changes.

## 7. Component-by-component treatment

| Component/family | Intended treatment | Details that must survive |
| --- | --- | --- |
| Chassis and header | Continuous selected metal, subtle brushing, restrained section seams | Azazel logo, current header content, demo/license indicators, actual dimensions |
| Logo area | Existing white mark on a small dark metal nameplate if A/B requires contrast | Existing artwork/viewBox and logo click target; no new wordmark |
| Preset name | Dark recessed readout in the existing header slot | Mixed-case names, long-name handling, tooltip and menu access |
| Author field | Dark inset text field matching readout material | Readable caret, selected text, placeholder, keyboard behavior |
| Header and utility buttons | Common well/cap construction with role-aware active cues | All current labels and actions; narrow buttons remain legible |
| Input/output waveforms | Near-black screen with restrained metal rim and inner shadow | Trace mapping, axes, threshold overlays, enlargement, visible plot area |
| Main dynamics module | Shared metal control bed with subtle boundaries, not seven floating cards | Shared title/value baselines and all seven modules |
| Five main rotaries | Preserve artwork; blend existing base/shadow with faceplate | Full-size filmstrip, value arcs, ticks, drag regions, global/band context |
| Gain In/Out | Black recessed slots, existing physical cap silhouette, consistent light | Current travel, unity position, cap index, noneditable value readouts, both endpoints |
| Global Mix / EQ Mix | Same knob material at existing small sizes | Distinct parameters; never couple their values or attachments |
| Band selectors | Compact seated controls, persistent identity and selection cues | Visible-band filtering, contiguous centering, selection binding |
| EQ/analyzer | Black inset graph in a matching metal surround | Nodes, grid, edge flags, threshold markers, context menus, all graph hit tests |
| Node Island | Shallow panel/pocket over the graph, coherent with faceplate | Q/THR/RANGE/FREQ/GAIN, direction/link/type controls, background dragging |
| Level meter | Existing centered dB/LUFS groups with recessed channels | Equal geometry, peak hold, reveal behavior, approximate-LUFS wording |
| Gain reduction and curve | Same screen material as waveform/EQ with smaller rims | Existing data, labels, collapse/expand behavior |
| Menus, tooltips, dialogs, overlays | Legible dark surfaces with matching edge treatment | Selected rows, text entry, focus, help navigation, alert/license behavior |

### Knobs require special care

The current raster filmstrip is intentional. Its frame includes shadow/transparent margin outside the visible metal. The approximately 78 px nominal main-dial rendering box is not the visible metal diameter. Do not change `KnobStrip` fractions to force a visual match.

Keep `resources/knob-azazel-192x61.png`, its 61-frame mapping, 270-degree sweep, resampling cache, and fallback behavior. The artwork and generator already have local edits at the time of this brief. Do not regenerate them as a side effect of a chassis redesign.

Do not rotate a complete filmstrip frame: it rotates the lighting along with the pointer. Retain the existing frame selection. Keep the value arc procedural, including bipolar center fill and existing color mapping. Preserve small-size tick density rules.

Inspect the existing art on the selected silver/titanium surface before deciding that it needs replacement. If a dark baked fringe becomes visible, record that finding for a separate asset revision; do not hide it with an oversized halo or heavy extra shadow. The existing knob already supplies a shadow.

### Display depth must not alter plot geometry

Add recess treatment to existing available margins. Preserve waveform/curve drawing rectangles and EQ frequency/dB mapping. A thicker rim cannot simply cover the outer graph pixels: users interact near the graph edges. Do not independently inset the painter while leaving mouse-to-frequency conversion unchanged.

The Island is a separate existing component over the graph. Its background must remain draggable; decorative children must not intercept clicks. Keep captions that currently pass clicks through doing so.

## 8. Repository map for the implementing model

Search by symbol; line numbers will move. These are inspected files, not suggestions to create a new architecture.

| File and search anchors | Purpose | Expected visual work |
| --- | --- | --- |
| [Theme.h](../src/Theme.h): `drawRecess`, `drawRaised`, `surfSunk`, `uiFontName`, `drawTracked` | Colors, typography and shared surface drawing | Introduce explicit plate/screen roles and reusable bounded relief helpers |
| [PluginEditor.h](../src/PluginEditor.h): `AzazelLookAndFeel`, `DragSlider`, `Attachment`, `ButtonAttachment` | Rendering declarations, control ownership, gestures and attachments | Add rendering declarations only where needed; retain lifetimes and interaction classes |
| [PluginEditor.cpp](../src/PluginEditor.cpp): `drawButtonBackground`, `drawButtonText`, `drawToggleButton` | Shared button rendering | Apply consistent well/cap/state recipes; cover the separate selected-node branch |
| Same file: `drawRotarySlider`, `drawLinearSlider`, `createSliderTextBox`, `drawLabel` | Rotary/fader drawing and value labels | Surface integration and background-aware text; preserve numeric mapping |
| Same file: `AzazelLookAndFeel::AzazelLookAndFeel`, popup-menu drawing methods | Default control/menu colors | Audit the full palette, including menus and editor fields |
| Same file: `rebuildChassisTexture`, `paint`, `resized`, `totalEditorWidth` | Chassis, panels and layout | Change material rendering; keep geometry ownership and coordinate spaces |
| Same file: `setupTextButton`, `setupKnob`, `setupFader`, `refreshBandButtons`, `presetAuthorEditor` | Per-control overrides and context switching | Audit explicit colors and both global/band versions of controls |
| [WaveformDisplay.cpp](../src/WaveformDisplay.cpp): `paint` | Input/output screen painting | Display relief and dark-surface text; no data-path changes |
| [EqPanel.cpp](../src/EqPanel.cpp): `paint`, `EqCloseButton::paintButton`, `eqMixKnob` | Analyzer, custom node drawing and EQ controls | Include custom painters that bypass normal buttons |
| [NodeIsland.cpp](../src/NodeIsland.cpp): `paint`, `resized` | Floating band controls | Surface and text consistency only |
| [LevelMeter.cpp](../src/LevelMeter.cpp), [VuMeter.cpp](../src/VuMeter.cpp), [GrCurveDisplay.cpp](../src/GrCurveDisplay.cpp) | Meter and curve surfaces | Match screen treatment while preserving scales and ballistics |
| [KnobStrip.h](../src/KnobStrip.h), [KnobStrip.cpp](../src/KnobStrip.cpp) | Filmstrip/cache/fallback implementation | Read for integration constraints; normally no changes |
| [DemoModeIndicator.cpp](../src/DemoModeIndicator.cpp), editor help/alert painters | Special overlays | Check readability on the new surrounding surfaces |

`Theme::drawRecess` currently serves some screens and meters; it is not the only background implementation. `drawButtonBackground` does not cover ToggleButtons. `drawButtonText` has a normal-text fallback and a `microCaps` path. A three-function recolor will not complete this redesign.

Search hardcoded `juce::Colour` values and explicit `setColour` calls in the affected painters. Classify them as material, text, signal, or state before editing. Do not bulk replace every dark gray or orange literal: some are deliberate signal or warning colors.

Prefer a few small helpers within the existing theme/LookAndFeel arrangement. If a new `.h/.cpp` pair is truly needed, add it to `CMakeLists.txt` `target_sources`. Do not introduce a framework, a generalized theme engine, or a hierarchy of decorative components.

## 9. Layout invariants and regression traps

The source currently uses logical base dimensions below. These are a verification snapshot, not a second source of layout constants.

| EQ dock | Curve/GR dock | Base width × height at 100% |
| --- | --- | --- |
| Closed | Closed | 809 × 522 |
| Closed | Open | 992 × 522 |
| Open | Closed | 1329 × 522 |
| Open | Open | 1512 × 522 |

`totalEditorWidth()` includes the collapsed right padding. Preserve its formula rather than recomputing width from the table.

- `kKnobSlotW = 100`, `kKnobDialH = 100`, `kKnobTopInset = 18`, `kModuleH = 138`, `kFaderW = 96` in the inspected source. Preserve the shared slot/module system. Do not copy these numbers into another geometry implementation.
- `maxR = min(width, height) * 0.5 * 0.93` in the rotary painter; preserving slot width and usable dial height preserves the existing dial footprint. Do not narrow slots for a bezel.
- Main-content `paint()` uses one `AffineTransform::translation(ox, 0)`. `resized()` adds `+ox` to actual component bounds. Do not add a second offset in painting.
- Within the already-translated paint block, component `getBounds()` includes `ox`. Reading it there without converting coordinates can double-shift new decorations. Reuse the existing unshifted formula or perform an explicit documented conversion.
- Right-anchored controls also use `cshift` when Curve/GR is hidden. Preserve this adjustment independently of EQ's `ox`.
- Keep `LevelMeter::kPreferredWidth` as the authority for the strip width. One meter is centered by itself; two form one centered pair, dB left and LUFS right, with equal external padding.
- Preserve the fader's existing `sliderPos` coordinate space. JUCE already accounts for thumb indentation. Adding another inset or clamping to a new range can create dead travel at the ends.
- Labels above rotary drag regions deliberately pass clicks through. Preserve those flags and component stacking.
- Keep the editor's existing zoom mechanism. The current menu offers 50%, 70%, 100%, 150%, and 200%; verify those actual options before testing. Do not assume only Windows DPI scaling matters.

## 10. Rendering and performance rules

Static material belongs in the existing `chassisTexture` cache, rebuilt through the established size-change path. Keep fixed seeds for texture generation. Do not regenerate grain randomly during animation, meter updates, or knob dragging.

The inspected `paint()` includes a fallback cache rebuild if the image is missing or incorrectly sized; do not confuse that existing guard with permission to rebuild every frame. Dynamic state must not be baked into the chassis cache. Panels whose position follows `ox` must remain in their appropriate coordinate space.

- Use a bounded number of gradients, fills, paths and strokes for button relief. JUCE-native drawing is sufficient.
- Use scoped graphics state when clipping or transforming; later text and neighboring controls must not inherit a clip or offset accidentally.
- Do not introduce file I/O, new or repeated asset decoding, full-window blur, per-pixel material generation, component mutation, or new audio-thread work in paint callbacks. Preserve the filmstrip renderer's existing one-time lazy master decoding and per-size cache initialization; this rule does not request a rewrite of that architecture.
- Do not allocate a new shadow image for each control on each frame. If a mask/cache is needed, prepare and invalidate it outside repeated painting, keyed to actual size/style needs.
- Preserve existing LookAndFeel ownership and destructor cleanup. Never give a component a pointer to a temporary LookAndFeel object.
- Check the material at 150% and 200% for blurred edges or aliased grain. Do not add a new high-resolution cache system preemptively; make a bounded rendering adjustment only if visual evidence requires it.
- Do not increase UI timer rates or change smoothing/ballistics to make a new surface appear more animated.

## 11. Implementation sequence — later, after authorization

Work in small phases. Each phase has an observable completion condition. Keep unrelated worktree edits intact.

### Phase 0: confirm the selected recipe and baseline

Read the decision record and latest user instruction. Inspect current rules, source, build setup, and worktree. Capture the existing running UI if available; mark older screenshots as historical rather than treating them as the current layout. Record the four dock combinations, control inventory, text colors, and current behavior before edits.

Done when the selected appearance and allowed task scope are clear and the current baseline is documented. If only this brief was requested, stop at documentation.

### Phase 1: material roles and one representative button family

Add the minimal semantic tokens/helper drawing needed. Adapt the existing shared button renderer for one ordinary action, one toggle, and the selected-node special case. Keep handlers and control bounds intact. Check idle, hover, held, on, off, disabled, and focused appearances. Do not switch the whole chassis to light while dark-screen text still uses unclassified global tokens.

Done when well/cap lighting is consistent and each state can be identified without guessing.

### Phase 2: chassis and surface-aware lettering

Apply the selected faceplate to the cached material and panel fills. Migrate text according to its actual background. Keep screens, fields, and menus dark and readable. Remove excessive dark vignetting from a silver material; do not simply place light base color underneath the old strong black shading.

Done when the interface reads as one material, with dark screens and properly contrasted labels.

### Phase 3: apply the system across current components

Complete header controls, Author field, utility toggles, band buttons, screen bezels, meter surfaces, EQ panel, Island, and custom overlays. Check per-instance colors and painters that bypass LookAndFeel. Preserve all five main knob context pairs and every Island control.

Done when no old unrelated surface treatment remains in the active UI and no control disappears.

### Phase 4: integrate knobs, faders, and text

Keep the knob asset and size. Check transparency/shadow edges on the new plate. Align surface light with existing cap light. Refine fader surface colors without changing cap travel. Resolve long labels by tightening tracking/padding within the current layout. Verify numerical readout visibility while preserving its current noneditable behavior; verify editing separately in the Author field and existing dialogs.

Done when all controls look seated in the same panel and endpoints, values, and labels are still correct.

### Phase 5: future build and visual verification

Only run this phase when a later implementation/build task authorizes it. This brief authorizes no current build. At that time use the actual current project instructions and toolchain; do not assume the earlier missing-toolchain issue is still present or already resolved.

The repository's Release validation command is:

```powershell
cmake --build build --config Release --target VisualComp_VST3 VisualComp_Standalone
```

This can install the VST3 through the configured post-build step. Read the configuration before executing it. If the user invokes `/testbuild`, use its current skill; that workflow also bumps version, launches a demo-enabled executable, and includes commit/push steps. Do not substitute `/testbuild` for an ordinary visual check without considering those extra effects, and do not rerun a version-bumping script blindly after failure.

For future UI changes, project rules require a build, launch, and screenshot inspection before claiming completion. Follow the current screenshot workflow; existing environment hooks include `VC2_FORCE_EQ_OPEN`, `VC2_FORCE_CURVE_GR`, `VC2_FORCE_BAND_SELECT`, and `VC2_FORCE_METER_REVEAL`. Inspect their current implementations and accepted values before using them. Scope temporary environment changes to the capture process.

Avoid clearing the user's live presets/settings to obtain screenshots. If clean state is required, use an isolated capture setup or back up and restore the exact versioned settings file. Do not delete an entire settings directory. If a build or capture is blocked, report the exact failed check and leave visual approval unclaimed.

## 12. Acceptance checklist for the future implementation

### Appearance

- [ ] Selected direction is recognizable at a glance; A actually has a silver body.
- [ ] Major displays visibly sit below their bezel/faceplate.
- [ ] Buttons are seated in wells; they do not look like floating flat tiles.
- [ ] Raised and inset lighting follow the same upper-left light source with the correct opposite edge treatment.
- [ ] Button states remain distinct after release, with clear active bypass and node selection.
- [ ] Engraved-looking labels remain readable; live values and axes are clean single-pass text.
- [ ] No orange outlines decorate idle panels; signal and active colors remain meaningful.
- [ ] No fringe is introduced by new compositing, duplicate heavy shadow, or changed knob footprint. Document any pre-existing asset fringe exposed by the selected material as an unresolved asset limitation; do not claim it is visually approved or silently regenerate the artwork.
- [ ] Readout, Author editor, popup, tooltip, and dialog text is legible, including selected text and caret.
- [ ] Existing selected EQ-node colors remain recognizable.

### Geometry and interaction

- [ ] All four EQ/Curve-GR dock combinations match the current geometry.
- [ ] Repeated dock toggles produce no double translation or accumulating offsets.
- [ ] Main dial sizes and shared module baselines are unchanged.
- [ ] One-meter and two-meter states remain mathematically centered.
- [ ] Graph-edge nodes, handles, threshold markers, and labels remain visible and reachable.
- [ ] Every rotary reaches minimum/default/maximum; bipolar Range shows zero and both signs correctly.
- [ ] Gain faders reach both endpoints without dead travel; unity indication matches value.
- [ ] All five global/band control pairs work after selection changes, without modifying a hidden global parameter.
- [ ] Island Q, THR, RANGE, FREQ, GAIN, direction/link/type controls and background dragging still work.
- [ ] Node multiselection, linking, edge bonds, and preset/state behavior show no regression.
- [ ] Long preset names, Author entry, Smart Master+, Auto Gain, and compact band selectors remain usable.
- [ ] Existing focus, keyboard, double-click, wheel, drag, and undo/redo paths work.

### Visual evidence and performance

- [ ] Save and inspect full-window screenshots of all four dock states at 100%.
- [ ] Inspect supported 50/70/150/200% zoom for text fit, thin edges, and clipping. Small supported zoom must not introduce new regressions relative to baseline.
- [ ] Inspect representative button crops: off, hover, held, on, disabled, and focused where applicable.
- [ ] Inspect silence, active audio, bypass, a selected band, and both meter reveal states.
- [ ] Compare sustained knob dragging and active metering with baseline; no new per-frame material generation or perceptible repaint stalls.
- [ ] Verify source diff contains no unintended DSP/state/automation or unrelated asset changes.
- [ ] Report build/capture results accurately. A successful compile alone does not verify appearance.

Do not invent extra test frameworks for this task. Use the existing build surface and targeted visual/interaction checks. If full verification is unavailable, list precisely which items remain unverified.

## 13. Common failure modes and concrete corrections

| Failure | Likely cause | Corrective direction |
| --- | --- | --- |
| Looks flat despite having gradients | No consistent separation between plate, well, and cap | Establish the four roles; fix inner-edge polarity before adding stronger effects |
| Looks like soft plastic | Broad white halos and large rounded shadows on every object | Tighten edge falloff, use fewer rounded groups, reduce highlights |
| Silver panel has unreadable displays | Global text colors were inverted | Split plate/screen/button roles and inspect explicit component colors |
| Knobs look pasted on | Extra UI shadow plus baked shadow, or exposed dark fringe | Remove duplicate shadow; preserve artwork and report any genuine asset mismatch |
| ON and pressed look identical | Logical state and pointer state were conflated | Read them separately; retain ON marker after mouse release |
| Tiny text is fuzzy | Etched duplicate and tracking applied indiscriminately | Use clean single-pass text for small legends and values |
| Docked UI shifts twice | Component bounds were used inside an already translated painter | Restore one consistent coordinate space; do not tune a compensating magic offset |
| Graph feels smaller or misses clicks | Decoration inset differs from plot/hit-test mapping | Preserve the plot rectangle; reduce decoration to existing margins |
| Slow while turning a knob | Material generation, large blur, or cache invalidation per frame | Return static work to bounded caches and simple state drawing |
| Some controls still look unrelated | Custom painters/per-component colors were missed | Audit ToggleButtons, node selection, EQ close, fields, menus, Island and overlays |

## 14. Copy-ready handoff prompt

Use this only for a future implementation task; replace the placeholders before sending it.

```text
Redesign VisualComp's visual surfaces using docs/UI_REDESIGN_DEBOSSED_BRIEF.md.

Selected direction: [A — Satin Silver / B — Smoked Titanium / C — Graphite Console]
Selected refinements: [use the recommended recipe, or specify changes]
Authorized work: [state implementation/build scope explicitly]
Commit/push: [only if explicitly requested]

Read the full brief, AGENTS.md, CLAUDE.md, and the current VisualComp UI skill.
Inspect the live source and existing uncommitted changes before editing.
Follow the brief's phased sequence and use its acceptance checklist.

Implement the selected material, recessed wells, seated buttons, consistent
upper-left lighting, and readable plate/screen text roles. Keep current
component bounds, controls, filmstrip artwork, parameter behavior, attachments,
DSP, presets, and audio-thread behavior intact.

Pay particular attention to ToggleButtons versus TextButtons, the selected
node branch, the Author field, five global/band slider pairs, Island GAIN,
dark popup text, and the ox/cshift coordinate rules. Do not remove current
functionality because an older comment describes a different interface.

Verify through the authorized build and screenshot workflow. Show the resulting
UI and state what was actually checked. If a required check fails, diagnose it
and report the remaining limitation instead of claiming success.
```

The deliverable for the current request is this Markdown brief. No plugin source, artwork, build configuration, or installed binaries were changed to create it.
