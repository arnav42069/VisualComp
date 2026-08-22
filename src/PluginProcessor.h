#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "EqEngine.h"
#include "ClipEngine.h"
#include "LoudnessMeter.h"
#include "UndoRedoManager.h"
#include "SmoothMeter.h"
#include "LicenseManager.h"

enum class CompMode { VCA = 0, FET, Optical, Tube };

struct WaveformBuffer
{
    static constexpr int size = 65536;
    std::array<float, size> data {};
    std::atomic<int> writePos { 0 };

    void push(const float* samples, int numSamples) noexcept
    {
        int pos = writePos.load(std::memory_order_relaxed);
        for (int i = 0; i < numSamples; ++i)
        {
            data[static_cast<size_t>(pos)] = samples[i];
            pos = (pos + 1) & (size - 1);
        }
        writePos.store(pos, std::memory_order_relaxed);
    }
};

class VisualCompProcessor final : public juce::AudioProcessor,
                                  private juce::AsyncUpdater
{
public:
    VisualCompProcessor();
    ~VisualCompProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    void setDemoAudioPlaying(bool shouldPlay) noexcept { demoAudioPlaying.store(shouldPlay, std::memory_order_release); }
    bool hasDemoAudio() const noexcept { return demoAudioReady.load(std::memory_order_acquire); }

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect()  const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int  getNumPrograms()                            override { return 1; }
    int  getCurrentProgram()                         override { return 0; }
    void setCurrentProgram(int)                      override {}
    const juce::String getProgramName(int)           override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Initialize license/demo mode (safe to call multiple times)
    void initializeLicenseManager()
    {
        if (!licenseMgrInitialized.load())
        {
            licenseManager.initialize();
            licenseMgrInitialized.store(true);
        }
    }

    // ── Public state for GUI access ──
    // License manager (demo mode, trial period, license verification)
    LicenseManager licenseManager;

    // Undo/Redo manager (message thread only, safe to access from editor)
    VisualCompUndo::UndoRedoManager undoRedoManager { 100 };   // Keep 100 undo steps

    juce::AudioProcessorValueTreeState apvts;

    WaveformBuffer inputWaveform;
    WaveformBuffer outputWaveform;
    WaveformBuffer sidechainWaveform;   // sidechain peak envelope (0-1 per sample)

    // Written by audio thread, read by GUI
    std::atomic<float> gainReductionDb     { 0.0f };
    std::atomic<float> currentInputLevelDb { -100.0f };

    // GUI-controlled flags
    std::atomic<bool>  sidechainEnabled  { false };
    std::atomic<int>   compMode          { static_cast<int>(CompMode::VCA) };
    std::atomic<bool>  bypassed          { false };
    std::atomic<bool>  autoGainEnabled   { false };
    std::atomic<int>   clipMode          { static_cast<int>(ClipMode::Soft) };

    // Parametric EQ (multi-node) + multiband-aware detection
    ParametricEq eq;
    std::atomic<int> activeEqBand { -1 };   // GUI read-only: dominant linked band, or -1

    // GUI read-only: each linked node's own current gain reduction (<= 0 dB),
    // written every block while multiband mode is on (0 otherwise/for
    // unlinked nodes) — lets the EQ panel draw a live per-band GR curve.
    std::array<std::atomic<float>, kMaxEqNodes> bandGrDb {};

    // Documentation aid, same pattern as the VC2_FORCE_* editor flags: when
    // set, node 0's bandGrDb is pinned to a fixed demo value every block
    // instead of being computed from (silent, in a screenshot rig) live
    // audio, so the per-band gain-reduction curve can be screenshotted
    // reproducibly. Inert unless set.
    std::atomic<bool> debugForceBandGrDemo { false };

    // Multiband mode: every *linked* EQ node acts as its own dynamic-EQ-style
    // compression band (see ParametricEq::applyDynamicBandGain), on top of
    // the ordinary broadband compressor. As of 2026-08-02 this is always on
    // (no user-facing toggle any more — see setStateInformation, the flag
    // itself is kept only because the DSP branches on it internally).
    std::atomic<bool> multibandEnabled { true };

    // True whenever the live EQ (node freq/gain/Q/type/link/dynamics — any
    // edit that funnels through EqPanel::onNodeEdited or the main editor's
    // band-context Threshold/Knee/Ratio/Attack/Release knobs) has diverged
    // from whatever was last loaded via a preset. Gates the "keep current
    // EQ?" confirm dialog in VisualCompEditor::loadUserPreset()/
    // runAutoAnalyze() — GUI-only bookkeeping, not persisted (see
    // setStateInformation, which never touches it: a fresh DAW project load
    // has nothing to "keep" a confirm dialog about yet).
    std::atomic<bool> eqDirtySincePreset { false };

    // ─── LEGACY: Kept for compatibility, use smoothMeter* below instead ───
    // Post-output metering (peak/RMS dB + approximate LUFS) — GUI read-only
    std::atomic<float> meterPeakDb     { -100.0f };
    std::atomic<float> meterRmsDb      { -100.0f };
    std::atomic<float> meterMomLufs    { -100.0f };
    std::atomic<float> meterShortLufs  { -100.0f };

    // ─── SMOOTH METERS: Low-latency, interpolated meter values ───
    // Lock-free, smooth-interpolating meter displays (attack/release ballistics)
    // Audio thread writes to these atomically; UI reads and calls updateSmoothing()
    // at 120Hz for butter-smooth animation.
    SmoothMeterValue smoothMeterPeak   { 5.0f,   200.0f };   // 5ms attack, 200ms release
    SmoothMeterValue smoothMeterRms    { 10.0f,  300.0f };   // 10ms attack, 300ms release
    SmoothMeterValue smoothMeterLufs   { 10.0f,  400.0f };   // 10ms attack, 400ms release

    // Lock-free queue for batch meter updates (optional, for lower-latency pushes)
    MeterUpdateQueue meterUpdateQueue;

    // Smart Master+ capture buffer: a real 8-10s excerpt of raw (pre-
    // processing) input, captured on demand so the wizard can analyze a full
    // representative passage -- spectrum, crest factor, integrated loudness
    // -- rather than whatever happened to be sitting in the ~1.5s
    // inputWaveform display ring buffer. GUI arms it by setting
    // smartMasterCaptureActive true (after zeroing writePos/done); the audio
    // thread fills smartMasterCapture in processBlock and flips
    // smartMasterCaptureDone once full. Sized once per prepareToPlay call.
    static constexpr double kSmartMasterCaptureSeconds = 9.0;   // middle of the 8-10s ask
    juce::AudioBuffer<float> smartMasterCapture;                 // 2ch, sized in prepareToPlay
    std::atomic<bool> smartMasterCaptureActive   { false };
    std::atomic<int>  smartMasterCaptureWritePos { 0 };          // samples captured so far
    std::atomic<bool> smartMasterCaptureDone     { false };

    // Test-build-only looping input, activated only by VC2_DEMO_AUDIO_FILE.
    // It is fully preloaded during prepareToPlay; processBlock does no I/O.
    juce::AudioBuffer<float> demoAudio;
    double demoReadPosition = 0.0;
    double demoReadIncrement = 1.0;
    std::atomic<bool> demoAudioReady   { false };
    std::atomic<bool> demoAudioPlaying { false };

    // GUI persistence (message thread only)
    juce::String currentPresetName   { "Mastering Glue" };
    // Author of the currently loaded/saved user preset -- blank for factory
    // presets and for user presets saved before this field existed. Doubles
    // as the remembered default the next "Save Preset As..." pre-fills, so a
    // producer saving a whole preset pack doesn't retype their name each time.
    juce::String currentPresetAuthor { "" };
    float        editorScale       { 1.0f };
    bool         eqPanelOpen       { false };
    bool         curveGrPanelOpen  { false };

    // Dynamic Island position persistence per node (8 max nodes)
    struct IslandPosition { int x = 0; int y = 0; bool hasPosition = false; };
    std::array<IslandPosition, 8> islandPositions {};

    // ── Oversampling (final clip/limit stage only) ──────────────────────────
    // The "oversamplingFactor" APVTS parameter is a choice index, 0..3; the
    // actual ratio is 1/2/4/8. Index doubles as JUCE's Oversampling stage
    // count, which is log2 of the ratio.
    static constexpr int kMaxOversamplingStages = 3;    // 2^3 = 8x
    static int oversamplingFactorForIndex(int index) noexcept;

    // Whichever factor the audio thread is currently running at (choice
    // index, not ratio). GUI-readable so the clip-mode popup can tick the
    // live entry; also what computeTotalLatency() reports against.
    std::atomic<int> activeOversamplingIndex { 0 };

    // Latency the host is told about: the oversampler's own filter delay plus
    // the lookahead line divided down to the base rate. Referenced by
    // ClipEngine.h's header comment.
    int computeTotalLatency() const;

private:
    void handleAsyncUpdate() override;   // message-thread setLatencySamples()

    // License manager initialization flag
    std::atomic<bool> licenseMgrInitialized { false };

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Per-mode state
    float envelope      = 0.0f;
    float opticalState  = 0.0f;
    float tubeMu        = 1.0f;

    // Sidechain envelope follower (audio thread only)
    float scEnvelope = 0.0f;

    // Auto-gain RMS smoothers
    float inputRmsSmooth  = 0.0f;
    float outputRmsSmooth = 0.0f;
    float autoGainDb      = 0.0f;

    double currentSampleRate = 44100.0;
    juce::AudioBuffer<float> monoMixBuffer;
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> scEnvBuf;   // scratch buffer for per-sample SC envelope

    OutputClipper  clipper;
    LoudnessMeter  loudness;

    // One prepared Oversampling instance per factor (index 0 = 1x, unused:
    // at 1x the whole stage is bypassed and the clipper runs on the buffer
    // directly). All four are built and initProcessing()'d up front in
    // prepareToPlay so switching factors mid-stream never allocates on the
    // audio thread -- juce::dsp::Oversampling allocates in both its
    // constructor and initProcessing(), so neither can happen in processBlock.
    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>,
               size_t(kMaxOversamplingStages) + 1> oversamplers;
    int preparedBlockCapacity = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualCompProcessor)
};
