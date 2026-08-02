#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Vertical, colour-coded dB meter (peak + RMS ballistics). Clicking it
// reveals a second bar to its right showing an approximate LUFS reading
// (momentary + short-term), animated in over ~150 ms. Bounds are fixed
// width for both states — only the second bar's visibility/slide animates,
// so the rest of the layout never has to reflow.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    explicit LevelMeter(VisualCompProcessor& proc);
    ~LevelMeter() override;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    static juce::Colour zoneColourForDb(float db, float greenBelow, float yellowBelow);
    void drawBar(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb,
                float floorDb, float ceilDb, float greenBelow, float yellowBelow,
                const juce::String& label, float alpha) const;

    VisualCompProcessor& processor;
    bool  revealed     = false;
    float revealAmount = 0.0f;   // eased 0..1

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
