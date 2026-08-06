#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>

struct WaveformBuffer;

class WaveformDisplay : public juce::Component, private juce::Timer
{
public:
    WaveformDisplay(const juce::String& title,
                    juce::Colour        waveColour,
                    WaveformBuffer&     buffer,
                    std::atomic<float>* thresholdParamDb = nullptr,
                    WaveformBuffer*     scBuffer         = nullptr,
                    std::atomic<bool>*  scEnabled        = nullptr,
                    std::atomic<float>* ratioParam       = nullptr,
                    std::atomic<float>* kneeParam        = nullptr,
                    std::atomic<float>* attackParam      = nullptr,
                    std::atomic<float>* releaseParam     = nullptr,
                    bool                clickToPause     = false);

    ~WaveformDisplay() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Left click toggles pause (only when constructed with clickToPause =
    // true — the input display only, per user request); right click always
    // fires onRightClick regardless, so the owning editor can offer an
    // "enlarge" menu on both displays.
    void mouseDown(const juce::MouseEvent&) override;

    bool isPaused() const { return paused; }

    // Fired on right-click (popup-menu trigger); the owning editor builds
    // and shows the themed enlarge/enlarge-both menu, since only it knows
    // about the other display and the shared look-and-feel.
    std::function<void(const juce::MouseEvent&)> onRightClick;

private:
    void timerCallback() override;

    // Fills dispMin/dispMax from historical audio data (full re-init)
    void initialiseDisplayBuffer(int pixW);

    // Incrementally shifts dispMin/dispMax left and appends new columns on the right
    void updateDisplayBuffer(int pixW, int writePos);

    // Incrementally shifts scDispEnv left and appends new columns on the right
    void updateScDisplayBuffer(int pixW, int scWp);

    juce::String        title;
    juce::Colour        waveColour;
    WaveformBuffer&     waveformBuffer;
    std::atomic<float>* thresholdParamDb;

    // Click-to-pause: freezes the trace/fill in place while dynamics
    // overlays (ducking curve, threshold line) keep updating live in
    // paint(), since those read their atomics fresh every frame regardless.
    bool paused          = false;
    bool allowClickPause = false;

    // Pre-computed min/max per pixel column — only touched on the message thread
    std::vector<float> dispMin, dispMax;
    int   lastWritePos = -1;
    float pixelAccum   =  0.0f;
    int   displayWidth =  0;

    // Optional sidechain overlay (input display only)
    WaveformBuffer*    scWaveformBuffer { nullptr };
    std::atomic<bool>* scEnabledAtomic  { nullptr };
    std::vector<float> scDispEnv;
    int   scLastWritePos = -1;
    float scPixelAccum   =  0.0f;

    // Compression params used to shape the SC overlay into a ducking curve
    std::atomic<float>* ratioParam   { nullptr };
    std::atomic<float>* kneeParam    { nullptr };
    std::atomic<float>* attackParam  { nullptr };
    std::atomic<float>* releaseParam { nullptr };

    // ~1.3 s of history at 44100 Hz
    static constexpr int kSamplesToShow = 57344;
    static constexpr int kFrameW        = 3;
    static constexpr int kLabelW        = 42;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDisplay)
};
