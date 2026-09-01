#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// Vertical, colour-coded dB meter (peak + RMS ballistics). Clicking it
// reveals a second bar to its right showing an approximate LUFS reading,
// animated in over ~150 ms. Bounds are fixed width for both states — only
// the second bar's visibility/slide animates, so the rest of the layout
// never has to reflow. The pair occupies two equal cells with identical
// internal padding; unit labels overlay the tops of their bars, while live
// values with units sit below and a three-second peak readout sits above.
class LevelMeter : public juce::Component, private juce::Timer
{
public:
    // ---------------------------------------------------------------------
    // Strip geometry. Published rather than kept private because the editor
    // has to reserve exactly this width for the component (kLevelMeterW in
    // PluginEditor.cpp is kPreferredWidth): paint() centres the dB+LUFS pair
    // in whatever bounds it is given. Equal cell widths plus equal outer
    // padding keep both meters balanced against every edge when revealed.
    // ---------------------------------------------------------------------
    static constexpr float kBarW     = 24.0f;   // the bargraph column itself
    static constexpr float kCellW    = 42.0f;   // one equal meter division
    static constexpr float kMidGap   = 6.0f;    // between the two divisions
    static constexpr float kSidePad  = 5.0f;    // pair -> strip edge, both sides
    static constexpr int   kPreferredWidth =
        int (2.0f * kCellW + kMidGap + 2.0f * kSidePad);                // 100

    explicit LevelMeter(VisualCompProcessor& proc);
    ~LevelMeter() override;

    void paint(juce::Graphics&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;

    // Draws one meter channel as a recessed, segmented LED-style column:
    // each lit segment is coloured via Theme::meterColour() at that
    // segment's own dB position (the classic fixed-ramp hardware bargraph
    // look), unlit segments are dim. No alpha parameter -- callers wanting
    // a faded-in channel (the LUFS bar) wrap the call in a Graphics
    // transparency layer instead, so every helper below can just paint
    // opaque colours.
    void drawChannel(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb,
                     float floorDb, float ceilDb) const;
    static void drawOverlayLabel(juce::Graphics& g, juce::Rectangle<float> bar,
                                 const juce::String& text);
    static void drawBottomValue(juce::Graphics& g, juce::Rectangle<float> cell,
                                float value, const juce::String& unit);
    // Prints a hardware-meter-style scale directly on the bar: a tick line
    // for each of `levels` that falls inside [floorDb, ceilDb], with the
    // 0 dB line singled out as a brighter, wider reference mark plus its
    // own numeral. Shared by the dB and LUFS bars, each with their own set
    // of common reference levels (see the kDbNotches/kLufsNotches
    // definitions in the .cpp).
    static void drawNotches(juce::Graphics& g, juce::Rectangle<float> bar,
                            float floorDb, float ceilDb,
                            const float* levels, int numLevels);

    VisualCompProcessor& processor;
    bool  revealed     = false;
    float revealAmount = 0.0f;   // eased 0..1

    // Rolling history of the instantaneous peak reading, sampled once per
    // timer tick (120Hz), so the peak-hold readout can show the true max over
    // the trailing window rather than an ordinary decaying peak-hold.
    static constexpr int kPeakHoldFrames = 360;   // 3s @ 120Hz
    std::array<float, kPeakHoldFrames> peakHistory;
    int peakHistoryPos = 0;
    bool peakHistoryWrapped = false;   // True after first complete cycle through buffer

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelMeter)
};
