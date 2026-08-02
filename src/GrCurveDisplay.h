#pragma once
#include <JuceHeader.h>
#include <atomic>

// Draws the compressor transfer curve (input dB → output dB) in real time,
// with a moving dot showing the current operating point.
class GrCurveDisplay : public juce::Component, private juce::Timer
{
public:
    GrCurveDisplay(juce::AudioProcessorValueTreeState& apvts,
                   std::atomic<float>& inputLevelDb);
    ~GrCurveDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override { repaint(); }

    // Computes output dB for a given input dB given the current parameters
    static float computeOutputDb(float inDb, float thresh, float ratio, float knee) noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    std::atomic<float>&                 inputLevelDb;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GrCurveDisplay)
};
