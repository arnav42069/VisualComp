#pragma once
#include <JuceHeader.h>
#include <atomic>

// Vintage needle-style gain-reduction meter.
// Reads gainReductionDb (negative dB) and animates a ballistic needle.
// Needle swings right for 0 dB GR (no compression) and left for heavy GR.
class VuMeter : public juce::Component, private juce::Timer
{
public:
    explicit VuMeter(std::atomic<float>& gainReductionDb);
    ~VuMeter() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override {}

private:
    void timerCallback() override;

    std::atomic<float>& gainReductionDb;

    float needleAngleDeg = 35.0f;  // current needle angle from vertical (+ = right)

    // Scale marks: {dB GR, angle from vertical in degrees}
    static constexpr int kNumMarks = 6;
    // Angles are linear in dB (angle = 35 + dB * 3.5) so they match the needle
    // mapping. Marks every 4 dB keep the enlarged labels clear of each other.
    static constexpr float kMarks[kNumMarks][2] = {
        {  0.0f,  35.0f },
        { -4.0f,  21.0f },
        { -8.0f,   7.0f },
        {-12.0f,  -7.0f },
        {-16.0f, -21.0f },
        {-20.0f, -35.0f }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VuMeter)
};
