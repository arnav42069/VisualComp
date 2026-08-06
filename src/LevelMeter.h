#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Vertical, colour-coded dB meter (peak + RMS ballistics). Clicking it
// reveals a second bar to its right showing an approximate LUFS reading
// (momentary + short-term), animated in over ~150 ms. Bounds are fixed
// width for both states — only the second bar's visibility/slide animates,
// so the rest of the layout never has to reflow. Each bar's unit label sits
// immediately to its right (rotated vertical text, not below the bar, to
// keep the whole strip narrow); a numeric peak-hold readout — the highest dB
// peak seen in the trailing 3 seconds — sits above the bars, and each bar's
// own live numeric value sits directly below it.
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
                float alpha) const;
    static void drawSideLabel(juce::Graphics& g, juce::Rectangle<float> bar, const juce::String& text, float alpha);
    static void drawBottomValue(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb, float alpha);
    // Prints a hardware-meter-style scale directly on the bar: a tick line
    // plus a small number at each of `levels`, for whichever fall inside
    // [floorDb, ceilDb]. Shared by the dB and LUFS bars, each with their own
    // set of common reference levels (see the kDbNotches/kLufsNotches
    // definitions in the .cpp).
    static void drawNotches(juce::Graphics& g, juce::Rectangle<float> bar,
                            float floorDb, float ceilDb,
                            const float* levels, int numLevels, float alpha);

    VisualCompProcessor& processor;
    bool  revealed     = false;
    float revealAmount = 0.0f;   // eased 0..1

    // Rolling history of the instantaneous peak reading, sampled once per
    // timer tick (30Hz), so the peak-hold readout can show the true max over
    // the trailing window rather than an ordinary decaying peak-hold.
    static constexpr int kPeakHoldFrames = 90;   // 3s @ 30Hz
    std::array<float, kPeakHoldFrames> peakHistory;
    int peakHistoryPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
