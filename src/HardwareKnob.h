#pragma once

#include <JuceHeader.h>

namespace HardwareKnob
{

// Draws the project-local photorealistic hardware body into `dest`. The body
// intentionally contains no value indicator; PluginEditor draws the live
// debossed orange inlay above it so parameter behaviour remains continuous.
bool draw(juce::Graphics& g, juce::Rectangle<float> dest,
          juce::Rectangle<float>* fittedBounds = nullptr);

} // namespace HardwareKnob
