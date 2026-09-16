# VisualComp control redesign

## Status and authorization

Camera selection: **18° off top-down / 72° elevation**.
Implementation authorization for this revision: **GIVEN 2026-09-07**.

The user selected 18° off top-down (72° elevation) and authorized implementation using
the detailed knob model already used in the project and repeated in the latest supplied
reference. The implementation includes matched Gain In/Out perspective, the ring fix,
and subtle contact shadows for raised controls. Version bump, packaging, commit and push
remain separate workflows.

## Durable reference material

- [Knob appearance reference](docs/design/visualcomp-redesign/knob-reference.png):
  user attachment `codex-clipboard-e2153126-7374-4b1a-8ec2-f6fe9a00fb19.png`.
  Its 2°–10° labels are explicitly rejected as camera evidence. Use its appearance,
  materials, grooves and proportions as inspiration, not its angle labels.
- [Knob detail and toggle reference](docs/design/visualcomp-redesign/toggle-reference.png):
  user attachment `codex-clipboard-d4bcead4-fc71-4e87-b877-22434905f87f.png`.
  Use the detailed knob and the recessed OFF/ON orange illumination as visual targets.
  Its printed 15° label is also not a calibrated camera measurement.
- [Interactive camera study](docs/design/visualcomp-redesign/camera-study.html):
  deterministic geometric study with one shared 12-groove mesh, fixed materials,
  fixed lighting, fixed scale, exact orthographic camera rotation, 78 px samples,
  and a matching gain-fader view. It is an angle-selection aid, not a replacement
  for the reference's final photorealistic material detail or an approved production asset.
  Selecting an angle in the preview is local exploration; record the user's reply here.
- [Selected detailed-control reference](docs/design/visualcomp-redesign/selected-control-reference.png):
  latest user attachment. Use its knob and fader material language; ignore its embedded
  5° labels because the user's explicit selection for implementation is 18° / 72°.

These files were added for design review only and are not compiled into the plugin.

## Decisions to record

| Decision | Current state |
| --- | --- |
| Knob appearance | Realistic dark convex crown, twelve deep rounded vertical grip grooves, bright attached machined silver base |
| Camera tilt from top-down | 18° |
| Camera elevation above panel | 72° |
| Gain In / Gain Out POV | Match the selected knob camera; preserve existing control travel and layout |
| Rotary outer glow | Absent; retain neutral scale ticks/track and readable value/pointer |
| Toggle appearance | Reference 2: dark cap in a recessed socket, warm orange illumination around the cap when ON |
| Minimum shared toggle scope | Soft Clip and LIM |
| Additional toggle scope | Existing shared utility-toggle renderer retained |
| Final detailed render approved | Existing detailed model retained at selected camera |
| Implement/build authorized | YES — 2026-09-07 |

Camera candidates (all are tilt **away from perpendicular**):

| Choice | Tilt from top-down | Elevation above panel | Visual direction |
| --- | ---: | ---: | --- |
| A | 5° | 85° | Almost overhead; least axial height displacement |
| B | 10° | 80° | Shallow dimensionality; initial preview position, not approval |
| C | 15° | 75° | More visible near sidewall |
| D | 20° | 70° | Stronger raised-hardware appearance |
| E | 25° | 65° | Pronounced sidewall and foreshortening |
| F | 30° | 60° | Most oblique option in this study |

The camera slider also includes 0° as a true top-down baseline. A tapered skirt's
shoulder can remain visible at 0°; visibility alone does not establish the angle.

## Camera and model contract

1. Panel lies in XY; +Z is outward from the panel. Tilt is `theta` from +Z,
   with the camera toward negative Y, no roll or yaw. Elevation is `90 - theta`.
2. Orthographic view direction from target to camera is `(0, -sin(theta), cos(theta))`.
   Projected screen coordinates are `x = X` and `y = Y*cos(theta) + Z*sin(theta)`
   before a common scale/translation. Document these conventions in the renderer.
3. Change the camera only when comparing angles. Hold mesh, height/diameter ratio,
   taper, groove count/depth, crown curvature, pointer position, materials, texture
   seed, light rig, orthographic scale and output resolution constant.
4. A planar calibration circle has projected minor/major axis ratio `cos(theta)`.
   A vertical separation H projects as `H*sin(theta)`. Verify using calibration geometry
   or projected landmarks, not measurements of the convex crown silhouette alone.
5. Retain source geometry and camera metadata beside final renders. A filename or
   generative prompt claiming an angle is not proof. Do not flatten, skew, stretch,
   or relabel the old PNG to simulate a different camera.
6. Use the current geometry study only to choose a camera. The final render must match
   the supplied reference's material/detail quality; do not silently adopt a simpler model.

## Knob appearance contract

- Twelve evenly spaced deep vertical grip recesses with rounded ends and softened
  edges, following the tapered body. Avoid sharp gear teeth or shallow decorative lines.
- A tall physical body and softly convex dark crown. Camera angle controls projected
  height; do not shorten or elongate the model to force a preferred silhouette.
- Restrained fine dark material texture, bevel highlights and machining detail that
  survive reduction to the plugin's existing approximately 78 px main-control size.
- Bright silver upper-facing base shoulder, darker machined vertical lower edge,
  physically attached and concentric with the body. No floating crescent or second rim.
- Short orange inlay in a dark recessed slot. Keep it legible at minimum, middle and
  maximum values, including the smaller Mix knob. No outer glowing progress arc.
- Fixed panel lighting from the upper-left (the existing 315° UI light convention),
  with soft contact shadows. Lights do not spin with the parameter.
- Preserve existing main knob diameter, parameter labels, units, numeric values,
  neutral graduations and compact module alignment.

## Gain faders

- GAIN IN and GAIN OUT share the selected camera elevation and light direction.
- Maintain their current width, travel, recessed vertical slots, readable scale and
  value readouts. Use a raised dark cap, silver sled/base and recessed orange index.
- The visual index must stay exactly at JUCE's `sliderPos`; any projected front-face
  depth must not offset the apparent value or change drag/hit behavior.

## Toggle illumination

- Follow reference 2: the light originates behind the dark cap inside its recessed
  socket. Bright warm-orange inner edges fade into a restrained amber spill, with
  somewhat more light visible along the lower inside lip.
- OFF retains the dark textured face, bevel and cavity shading; orange light is absent.
- ON retains exactly the same cap size and bezel dimensions. Do not shrink the face
  on activation to expose more orange. A press can have a small temporary depth motion.
- Avoid filling the whole face solid orange, a flat neon border, or a large exterior halo.
- Use one reusable style for Soft Clip and LIM; follow the pending scope decision for
  AUTO GAIN and SC. Preserve Bypass's existing warning/semantic treatment.
- Preserve text contrast and distinct hover, press, disabled and keyboard-focus states.
  Do not rely exclusively on emitted color to convey state.
- Soft Clip remains the existing clip-mode menu selector; styling must not turn it into
  a binary parameter or collapse the Soft / Brickwall / Off behavior.

## Existing source and corrective work, after approval

Read `AGENTS.md` and `CLAUDE.md` first. The source references below describe the
inspected working tree; locate by symbol if line positions change.

1. `CMakeLists.txt`, `juce_add_binary_data(VisualCompData)`: currently embeds
   `resources/knob-hardware-2p5deg-chroma.png`. Replace its reference only with the
   approved, verified camera render. Keep source metadata and final assets in the repo.
2. `src/HardwareKnob.cpp`, `loadKeyedBody()` and `draw()`: currently keys green,
   crops with padding, downsamples to 384 px and caches the image. Preserve efficient
   cached rendering; prepare decoded assets outside repaint. Prefer clean alpha for
   a new deterministic render; preserve antialiased edges and avoid green fringes.
3. `src/PluginEditor.cpp`, `AzazelLookAndFeel::drawRotarySlider()`, ratio family:
   the raster is the primary path. The later `kViewOffTopDeg` only changes the
   procedural fallback; it cannot modify the visible raster's camera.
4. Remove the independent `collarCy` / `collarEdge` / `collarSilver` overlay (currently
   around lines 898–927) when integrating the corrected base. The existing raster
   already contains silver metal; the overlay likely causes the detached bright smile.
5. Replace hardcoded raster aspect and crown anchor guesses with metadata from the
   same render. Project the live pointer onto the crown using the actual fitted image
   bounds, including crop padding. `HardwareKnob::draw()` can fit inside its destination;
   assuming the whole destination is always filled can misalign overlays.
6. Define shared selected-camera data for the asset metadata, procedural fallback and
   `drawLinearSlider()` faders. Avoid unrelated duplicate degree constants.
7. Preserve the existing no-active-arc branch for the hardware knob family. Dynamic
   Island's legacy filmstrip rendering is outside this change unless explicitly added.
8. `drawToggleButton()` currently changes face inset between OFF and ON. Use fixed
   cap geometry with independently drawn cavity illumination. Soft Clip and LIM already
   share this ToggleButton path; inspect their callbacks/attachments before changes.
9. Preserve DSP, parameter IDs/ranges, APVTS attachments, undo, automation, sidechain,
   clip modes, preset storage, docked EQ `ox` translation and Curve/GR `cshift` rules.
10. Preserve the unrelated uncommitted working-tree changes. Version stays centralized
    in the existing CMake/package workflow; design work introduces no version literal.

## Future implementation sequence

1. Record selected angle and glow scope from the user's answer.
2. Prepare a detailed final knob/fader reference at that verified camera, plus native-size
   crops and OFF/ON toggle samples. Iterate on appearance without changing the camera.
3. Record explicit implementation approval for that concrete design.
4. Integrate the approved asset/metadata, correct ring and pointer placement, synchronize
   faders, and refine shared toggle painting using the source checklist above.
5. Run the appropriate Release VST3 + Standalone validation and capture the actual UI
   after implementation is authorized. Preserve/restore standalone settings for captures.
   Follow the existing launch convention after the build; do not invoke release or
   push workflows unless separately requested.

## Acceptance checks

- Confirm camera metadata and calibration ratios for the selected angle. For previews,
  verify that each tile uses the same mesh/material inputs and differs in actual projection.
- Review enlarged and native approximately 78 px main knobs, plus smaller Mix, at 1x/2x
  display scaling. Base ring must remain attached with no added smile or chroma halo.
- Check pointer at minimum, midpoint and maximum; verify debossed trough alignment
  and that the pointer stays on the crown. Never rotate a raster with baked world lighting.
- Both gain faders share POV and light; index alignment is correct at minimum, unity
  and maximum. Labels and value readouts keep their existing space.
- Compare toggle OFF/ON geometry directly. Check hover/press/disabled/focus and
  preserve actual clip-mode/menu behavior and every existing parameter binding.
- Check editor with EQ and Curve/GR panels open and closed. Confirm compact alignment,
  no clipping, and no change in the knob's intended size or parameter interaction.
- Verify performance: no per-paint image decoding, new large allocations or disk I/O.
- Compilation alone does not close visual review; inspect actual plugin screenshots.

## Review log

- 2026-09-07 implementation pass: user selected 18° off top-down / 72° elevation and
  authorized the build using the current detailed knob model. Integrate the new 18°
  sibling raster, remove the duplicate silver collar overlay, match Gain In/Out to 18°,
  keep toggle cap geometry fixed across OFF/ON, and add subtle contact shadows.
