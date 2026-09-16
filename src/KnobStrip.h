#pragma once
#include <JuceHeader.h>

//==============================================================================
/*  The rotary knob filmstrip (resources/knob-azazel-192x61.png, embedded via
    juce_add_binary_data), and the resampling cache that draws it at whatever
    size a knob happens to be.

    The asset is ray-traced from a lathed solid with 24 chunky grip cuts by
    scripts/make_knob_3d.py,
    with an editable mesh in resources/knob-raised-3d.obj. The renderer proves
    its own output: the grip geometry rotates with the short position mark,
    while the camera and upper-left light stay fixed. The smooth top's lighting
    is unchanged across frames; the grooves and their shadows are re-rendered.
    That split makes the knob read as a physical object in a lit room, and is why
    nothing here ever applies a rotation transform to the image.

    One 192 px master ships; every on-screen size is derived from it at runtime
    and cached. See boxDownsample() in the .cpp for why that isn't left to
    Graphics::drawImage.
*/
namespace KnobStrip
{
    /** Frames in the embedded strip. */
    constexpr int kFrames = 61;

    /** The strip is baked for JUCE's default rotary sweep (1.25pi .. 2.75pi,
        i.e. 270 deg), which every rotary in this plugin uses -- none of them
        call setRotaryParameters. A knob that did would get an etched pointer
        disagreeing with its own graduation ticks, so draw() asserts on it. */
    constexpr float kSweepRadians = juce::MathConstants<float>::pi * 1.5f;

    /** Where the bezel's outer edge sits, as a fraction of the frame's half
        width. Chrome drawn outside the knob (ticks, value arcs) should be
        placed off this rather than off the frame edge, since the frame also
        carries the ambient shadow and a clear margin beyond it. */
    constexpr float kBezelOuterFrac = 53.0f / 64.0f;

    // Option D: extend the sidewall, keeping the base and bevel shape unchanged.
    constexpr float kHeightExtension = 35.09f;
    constexpr float kShoulderHeight = 22.0f + kHeightExtension;
    constexpr float kTopHeight = 29.0f + kHeightExtension;
    // Orthographic projection of the 64.09-unit-high top at a 10.08-degree tilt.
    // Keep these synchronized with make_knob_3d.py's model and camera.
    constexpr float kTiltSin = 0.175023059f;
    constexpr float kTopOffsetYFrac = (3.0f - kTopHeight * kTiltSin) / 64.0f;
    constexpr float kTopYScale = 0.984564335f;
    constexpr float kArcRadiusFrac = 36.5f / 64.0f;

    /** The live value ring belongs to the panel outside the silver bezel,
        rather than to the raised top cap. Its centre follows the lower knob
        seat, not the elevated cap's upward projection. */
    constexpr float kOuterArcRadiusFrac = 57.5f / 64.0f;
    constexpr float kOuterArcOffsetYFrac = 2.0f / 64.0f;

    /** ...and where the ambient shadow finally fades out. Anything drawn
        inside this radius will be sitting in the knob's own shadow. */
    constexpr float kShadowOuterFrac = 59.0f / 64.0f;

    /** Draw a fully procedural smooth black knob when the filmstrip asset is
        unavailable. It matches the pale seat, charcoal bevel, and short
        position mark of the embedded reference-style artwork. */
    void drawMetallicFallback (juce::Graphics& g, juce::Rectangle<float> dest, float pos);

    /** Draws the frame nearest `pos` (0 = min, 1 = max) into `dest`.

        Returns false if the asset could not be decoded, so the caller can fall
        back to drawing something rather than nothing.
    */
    bool draw (juce::Graphics& g, juce::Rectangle<float> dest, float pos);
}
