# VisualComp Gain Fader Redesign Instructions

## Design status

- Selected design: **Option B — Soft convex**
- Selection approved by the user for specification on 2026-09-07.
- This document is an implementation specification only. Do not modify source code or build the plugin until the user explicitly authorizes implementation.
- Apply the design to both **Gain In** and **Gain Out** faders.
- Design comparison: [`docs/design/slider-redesign-option-b.png`](docs/design/slider-redesign-option-b.png)
- Original visual guide: `C:/Users/arnav/AppData/Local/Temp/codex-clipboard-37652ef5-8ef8-4ea6-a1a9-55f8b51633e2.png`

## Intent

Create a compact, wide, low-profile fader cap that reads as a real satin-black hardware part at VisualComp's normal UI size. The face must be gently convex, with a broad rounded bevel and a recessed horizontal channel containing a short orange indicator. The control must remain dark and restrained; orange is reserved for the indicator and existing value feedback.

## Implementation location and scope

Implement the cap inside `AzazelLookAndFeel::drawLinearSlider()` in `src/PluginEditor.cpp`.

Preserve all existing behavior outside the cap-rendering block:

- `juce::Slider::LinearVertical` interaction and APVTS attachments.
- Current Gain In/Out ranges, defaults, text boxes, units, drag sensitivity, travel, and double-click reset.
- Existing vertical recessed slot, unity-to-value accent fill, tick positions, and value-to-pixel calculation.
- `sliderPos` as the exact vertical centre of the indicator; do not introduce additional travel clamping or dead zones.
- Existing editor layout and fader component bounds.

Replace only the cap drawing, beginning at the `// ---- Cap:` section and ending before the function's final closing brace.

## Exact geometry

Use the existing variables `cx`, `pos`, `width`, `trackH`, `capX`, and `capY` where applicable.

```cpp
const float capW = juce::jlimit(28.0f, 72.0f, float(width) * 0.66f);
const float capH = juce::jmin(18.0f, trackH * 0.24f);
const float capX = cx - capW * 0.5f;
const float capY = juce::jlimit(trackT - capH * 0.5f,
                                trackB - capH * 0.5f,
                                pos - capH * 0.5f);
const auto cap = juce::Rectangle<float>(capX, capY, capW, capH);
const float outerRadius = juce::jmin(5.5f, capH * 0.31f);
```

The target width-to-height ratio is approximately 4:1 at full size. Do not add a perspective drop, silver sled, lower metal face, lathe lines, or separate base plate.

Create these nested regions:

```cpp
const auto face = cap.reduced(1.15f, 1.05f);
const auto channel = juce::Rectangle<float>(
    face.getX() + face.getWidth() * 0.12f,
    pos - 2.35f,
    face.getWidth() * 0.76f,
    4.70f);
const float channelRadius = 2.15f;
const float indicatorW = face.getWidth() * 0.53f;
const auto indicator = juce::Rectangle<float>(
    cx - indicatorW * 0.5f,
    pos - 0.75f,
    indicatorW,
    1.50f);
```

## Layer order and colours

Draw every layer in this order. Use JUCE-native vector drawing; do not introduce a raster asset for the fader.

1. **Contact shadow**
   - Light direction: upper-left, approximately 315 degrees.
   - Shadow falls mainly lower-right.
   - Draw four rounded rectangles behind `cap`, with expansion from 0.6 to 2.4 px.
   - Translate each pass by approximately `(0.65 px, 0.85–1.55 px)`.
   - Use black with combined alpha no greater than 0.22 at the darkest centre.
   - Keep the shadow tight enough that it does not touch the scale marks.

2. **Outer cap body**
   - Fill `cap` with a diagonal linear gradient from upper-left to lower-right.
   - Upper-left: `0xff4a4946`.
   - Midtone near 45%: `0xff302f2c`.
   - Lower-right: `0xff151513`.
   - Fill using `outerRadius`.

3. **Rounded bevel and convex face**
   - Fill `face` with a radial gradient whose centre is at 27% width and 20% height.
   - Highlight centre: `0xff53524e`.
   - Midtone near 52%: `0xff353430`.
   - Edge colour: `0xff1b1a18`.
   - This gradient must create a soft crown, not a glossy dome.
   - Draw a 0.8 px upper-left edge highlight using white at alpha `0.12`.
   - Draw a 0.9 px outer edge using black at alpha `0.78`.

4. **Recessed horizontal channel**
   - Fill `channel` with `0xff080807`.
   - Add a subtle upper inner shadow using black at alpha `0.85`.
   - Add a faint lower bounce line using `0xff5a554c` at alpha `0.11`.
   - Do not outline the full channel in bright grey.

5. **Orange indicator**
   - The indicator is an inset material strip, not a free-floating glow bar.
   - Draw one restrained under-glow using `Theme::accent` at alpha `0.18`, expanded by `(1.2 px, 0.9 px)`.
   - Fill `indicator` with `Theme::accent` at full opacity and radius `0.7 px`.
   - Add a 0.55 px top highlight using `Theme::accent.brighter(0.30f)`.
   - The glow must remain within or immediately adjacent to the recessed channel.

## Material and lighting rules

- Material: satin charcoal polymer or lightly textured Bakelite, not polished piano black.
- Keep the surface dark enough to contrast with the current panel while retaining visible upper-left curvature.
- Use broad gradients rather than narrow specular streaks.
- Keep the lower-right edge darker to establish thickness.
- Avoid texture noise at this control size; it aliases and muddies the cap.
- The cap must remain legible when rendered at 1x UI scale and on HiDPI displays.

## Interaction states

- Default: use the colours and alpha values above.
- Hover: brighten only the face gradient by approximately 5%; do not enlarge the cap or indicator.
- Mouse-down: translate the face and channel downward by `0.55 px`; keep the outer cap and contact shadow fixed so the control reads as pressed.
- Disabled: reduce the complete cap opacity to 55%, while retaining the value position.
- Parameter changes move the complete cap vertically and keep the orange indicator centred exactly at `sliderPos`.

## Explicit exclusions

- No silver rim, silver sled, or metallic bottom ring.
- No 3D camera projection or POV angle on the faders.
- No orange perimeter, value arc, or large halo.
- No change to knob assets, knob elevation, toggle buttons, DSP, parameter ranges, layout constants, or preset state.
- No state-dependent change to cap size.

## Verification

After implementation is authorized:

1. Build Release targets with:

   ```powershell
   cmake --build C:\Users\arnav\VisualComp\build-vs --config Release --target VisualComp_VST3 VisualComp_Standalone
   ```

2. Launch the resulting Standalone executable.
3. Inspect Gain In and Gain Out at minimum, 0 dB, and maximum values.
4. Confirm the orange indicator remains centred on the JUCE `sliderPos` at every value.
5. Confirm full slider travel remains usable and the cap never clips at either endpoint.
6. Check that scale marks remain visible beside the cap.
7. Inspect at 100%, 125%, 150%, and 200% UI scaling for clean bevels and a crisp 1.5 px indicator.
8. Confirm hover and press states do not alter layout or reported parameter values.
9. Run `git diff --check`.

## Acceptance criteria

- Both gain faders visually match Option B in the comparison sheet.
- The cap is wide, dark, low, softly convex, and clearly raised from the panel.
- Upper-left lighting and lower-right contact shadow agree with the rest of VisualComp.
- The recessed channel is visible without becoming a high-contrast border.
- The orange indicator is crisp at normal size and has only a restrained local glow.
- No fader behavior, range, reset value, value display, or travel changes.
