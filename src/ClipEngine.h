#pragma once
#include <JuceHeader.h>

// Final-stage output clipping/limiting, selectable per preset or by the
// user. Three modes:
//
//   Soft       - tanh-style saturation. Colours the signal audibly as it
//                approaches 0 dBFS; a deliberate "glue" character.
//   Brickwall  - a short-lookahead transparent limiter: a few milliseconds
//                of delay let the gain-reduction envelope see peaks
//                slightly before they arrive, so the limiter can react
//                without the gain-reduction itself being audible. True
//                hard 0 dB ceiling, tuned for mastering.
//   Off        - no processing.
//
// All three modes run the signal through the same fixed lookahead delay
// line (only the processing on the delayed sample differs), so switching
// modes live never changes the plugin's reported latency or causes a
// timing jump.
enum class ClipMode { Soft = 0, Brickwall, Off, kNumModes };

class OutputClipper
{
public:
    int prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = sampleRate;
        lookaheadSamples = juce::jmax(1, int(sampleRate * 0.003));   // 3 ms, fixed regardless of mode
        delayLine.assign(size_t(lookaheadSamples) * 2 /*ch*/, 0.0f);
        writePos = 0;
        gainEnv  = 1.0f;
        return lookaheadSamples;
    }

    int getLatencySamples() const { return lookaheadSamples; }

    void reset()
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        writePos = 0;
        gainEnv  = 1.0f;
    }

    // Processes one stereo sample in place. `mode` may change block-to-block;
    // the lookahead delay itself is always applied so latency stays constant.
    inline void processSample(float& l, float& r, ClipMode mode) noexcept
    {
        const size_t base = size_t(writePos) * 2;
        delayLine[base]     = l;
        delayLine[base + 1] = r;

        const float peak = juce::jmax(std::abs(l), std::abs(r));
        const float target = (mode == ClipMode::Brickwall && peak > 0.999f) ? (0.999f / peak) : 1.0f;

        const float atkC = std::exp(-1.0f / float(sr * 0.0004));   // ~0.4 ms
        const float relC = std::exp(-1.0f / float(sr * 0.060));    // 60 ms
        gainEnv = (target < gainEnv)
            ? atkC * gainEnv + (1.0f - atkC) * target
            : relC * gainEnv + (1.0f - relC) * target;

        const int readPos = (writePos + 1) % lookaheadSamples;
        const size_t rbase = size_t(readPos) * 2;
        float dl = delayLine[rbase];
        float dr = delayLine[rbase + 1];
        writePos = readPos;

        switch (mode)
        {
            case ClipMode::Soft:
                l = softClipSample(dl);
                r = softClipSample(dr);
                break;

            case ClipMode::Brickwall:
                l = juce::jlimit(-1.0f, 1.0f, dl * gainEnv);
                r = juce::jlimit(-1.0f, 1.0f, dr * gainEnv);
                break;

            case ClipMode::Off:
            default:
                l = dl;
                r = dr;
                break;
        }
    }

    static juce::String modeName(ClipMode m)
    {
        switch (m)
        {
            case ClipMode::Soft:      return "SOFT CLIP";
            case ClipMode::Brickwall: return "BRICKWALL";
            case ClipMode::Off:       default: return "OFF";
        }
    }

private:
    static inline float softClipSample(float x) noexcept
    {
        constexpr float drive = 1.6f;
        const float y = std::tanh(x * drive) / std::tanh(drive);
        return juce::jlimit(-1.0f, 1.0f, y);
    }

    double sr = 44100.0;
    int    lookaheadSamples = 128;
    std::vector<float> delayLine;
    int    writePos = 0;
    float  gainEnv  = 1.0f;
};
