#pragma once
#include <JuceHeader.h>
#include "EqEngine.h"

// Post-output metering: fast peak/RMS ballistics for the vertical dB meter,
// plus an approximate LUFS estimate (K-weighting pre-filter per the general
// shape described in ITU-R BS.1770, continuous exponential-average windows
// rather than the standard's discrete gated blocks). This is intentionally
// labelled "approx" in the UI — it is close enough to be useful for gain
// staging and the Auto-Analyze wizard, but is not a certified loudness
// measurement and should not be relied on for delivery-spec compliance.
class LoudnessMeter
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        // K-weighting approximation: a high shelf (~+4 dB above ~1.5 kHz)
        // followed by a high-pass around 60 Hz, applied per channel.
        designBiquad(FilterShape::HighShelf, 1500.0f, 0.707f, 4.0f, sr, shelfL);
        designBiquad(FilterShape::HighShelf, 1500.0f, 0.707f, 4.0f, sr, shelfR);
        designBiquad(FilterShape::HighPass,  60.0f,   0.707f, 0.0f, sr, hpL);
        designBiquad(FilterShape::HighPass,  60.0f,   0.707f, 0.0f, sr, hpR);
        reset();
    }

    void reset()
    {
        shelfL.reset(); shelfR.reset(); hpL.reset(); hpR.reset();
        peakEnv = 0.0f; rmsMeanSq = 0.0f;
        momentaryMeanSq = 0.0f; shortTermMeanSq = 0.0f;
    }

    // Call once per sample with the final (post-clip) stereo signal.
    inline void pushSample(float l, float r) noexcept
    {
        const float peak = juce::jmax(std::abs(l), std::abs(r));
        const float atk = std::exp(-1.0f / float(sr * 0.001));    // 1 ms
        const float rel = std::exp(-1.0f / float(sr * 0.300));    // 300 ms
        peakEnv = (peak > peakEnv) ? atk * peakEnv + (1.0f - atk) * peak
                                   : rel * peakEnv + (1.0f - rel) * peak;

        const float ms = 0.5f * (l * l + r * r);
        const float rmsC = std::exp(-1.0f / float(sr * 0.300));
        rmsMeanSq = rmsC * rmsMeanSq + (1.0f - rmsC) * ms;

        // K-weighted path for LUFS estimate
        float kl = hpL.process(shelfL.process(l));
        float kr = hpR.process(shelfR.process(r));
        const float kms = 0.5f * (kl * kl + kr * kr);

        const float momC = std::exp(-1.0f / float(sr * 0.400));   // ~400 ms momentary
        const float stC  = std::exp(-1.0f / float(sr * 3.000));   // ~3 s short-term
        momentaryMeanSq = momC * momentaryMeanSq + (1.0f - momC) * kms;
        shortTermMeanSq = stC  * shortTermMeanSq + (1.0f - stC)  * kms;
    }

    float peakDb() const
    {
        return peakEnv > 1e-9f ? juce::Decibels::gainToDecibels(peakEnv) : -100.0f;
    }

    float rmsDb() const
    {
        return rmsMeanSq > 1e-9f ? 10.0f * std::log10(rmsMeanSq) : -100.0f;
    }

    // Approximate momentary / short-term LUFS (see class comment).
    float momentaryLufs() const
    {
        return momentaryMeanSq > 1e-9f ? -0.691f + 10.0f * std::log10(momentaryMeanSq) : -100.0f;
    }

    float shortTermLufs() const
    {
        return shortTermMeanSq > 1e-9f ? -0.691f + 10.0f * std::log10(shortTermMeanSq) : -100.0f;
    }

private:
    double sr = 44100.0;
    Biquad shelfL, shelfR, hpL, hpR;
    float  peakEnv = 0.0f;
    float  rmsMeanSq = 0.0f;
    float  momentaryMeanSq = 0.0f;
    float  shortTermMeanSq = 0.0f;
};
