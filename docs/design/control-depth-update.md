# Control depth update

- Preserve the existing 24-groove knob design, charcoal top, silver mounting rim,
  external value arc, dark panel, parameter mapping, and control footprints.
- Increase knob total model height from 49.3 to 64.09 units (+30%). Extend the
  straight grip wall; do not thicken the silver base or inflate the diameter.
- Reduce camera tilt from 14.4 to 10.08 degrees off panel normal (-30%), equivalent
  to 79.92 degrees camera elevation. Use orthographic projection, not image rotation.
- Regenerate the filmstrip and editable OBJ with `scripts/make_knob_3d.py`.
  Keep the projection constants in `src/KnobStrip.h` synchronized.
- Gain In/Out: the requested notch is the horizontal orange insert, not MIX's
  rotary marker. Retain the wide low cap, short orange index, and engraved channel.
  Superseding selection: restore the earlier low-profile fader cap styling.
  Remove the added silver extrusion, camera foreshortening, and orange lens
  bevel. Use the original vertical dark gradient, soft shadow, machined grooves,
  and flat short orange index. Do not apply the knob camera to the gain faders.
- Keep the fader index on its existing value position. Shadows fall down/right;
  highlights come from upper-left, matching the ray-traced knob's fixed light.
- Shared raised buttons use the same upper-left highlight/down-right shadow cue.
- No DSP, automation, preset, layout, version, or publishing changes.

Validation: renderer assertions cover all 61 pointer angles, fixed lighting,
rotating groove geometry, and transparent frame edges. Build both Release targets
and inspect the running Standalone at its normal size.
