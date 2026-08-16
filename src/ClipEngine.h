#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>

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
// *clip modes* live never changes the plugin's reported latency or causes a
// timing jump. (Changing the oversampling factor does move latency, since
// the lookahead is re-derived at the oversampled rate -- see
// setSampleRate() and VisualCompProcessor::computeTotalLatency().)
enum class ClipMode { Soft = 0, Brickwall, Off, kNumModes };

// ── Brickwall gain envelope: synchronized linear ramp ────────────────────────
//
// The classic exponential (one-pole) attack is what this used to do, and it has
// two problems for a mastering clipper: it never actually *reaches* its target
// (it only asymptotes toward it), so the ceiling is only held by the hard
// jlimit() safety clamp -- which is just clipping, i.e. distortion -- and its
// curve is signal-independent, so a fast transient and a slow swell get the
// same shape.
//
// Instead, the moment a peak is seen at the *input* of the lookahead line, we
// know exactly how many samples remain before that peak reaches the *output*:
// `lookaheadSamples`. So we solve for the straight line that lands on the
// required gain at precisely that sample and walk it one increment per sample.
// The gain reduction is therefore spread over the full lookahead window (the
// gentlest possible slope that still guarantees the ceiling), and it arrives
// exactly on time -- no overshoot to clip, no early duck.
//
// When a *second*, louder peak arrives while a ramp is already in flight, both
// deadlines have to be met at once. The rule is to take the steeper of the two
// slopes AND the lower of the two targets, then recompute how many samples the
// (now steeper) ramp needs to reach that (now lower) target. Taking only the
// steeper slope would land on the old, higher target and stop -- missing the
// new peak; taking only the lower target would use a shallow slope that
// arrives too late for the earlier one. Taking both is provably safe for every
// pending deadline: since the running slope is <= each pending demand's own
// slope and the running target is <= each pending demand's required gain, at
// any deadline the envelope is either still ramping (and so at or below that
// demand's line) or has already landed on the lower target.
class OutputClipper
{
public:
    // Lookahead time. Also the exact duration of every gain-reduction ramp.
    static constexpr double kLookaheadSeconds = 0.0045;   // 4.5 ms
    // Release back to unity once the peak has passed (exponential -- a linear
    // release would be audible as a hard corner at the end of the recovery).
    static constexpr double kReleaseSeconds   = 0.060;    // 60 ms
    static constexpr float  kCeiling          = 0.999f;

    // Allocates the delay line. `maxOversamplingFactor` reserves capacity for
    // the highest oversampled rate this instance will ever be asked to run at,
    // so setSampleRate() can be called later from the audio thread without
    // allocating. Returns the lookahead length in samples at the *base* rate.
    int prepare(double sampleRate, int /*maxBlockSize*/, int maxOversamplingFactor = 1)
    {
        const double maxSr = sampleRate * double(juce::jmax(1, maxOversamplingFactor));
        const int maxFrames = lookaheadSamplesFor(maxSr) + 1;
        delayLine.assign(size_t(maxFrames) * 2 /*ch*/, 0.0f);
        setSampleRate(sampleRate);
        return lookaheadSamples;
    }

    // Re-derives the lookahead length (and release coefficient) for a new
    // sample rate *within the capacity already reserved by prepare()*. Never
    // allocates, so it is safe to call from processBlock when the oversampling
    // factor changes. Resets the delay line, since its contents are meaningless
    // at the new rate.
    //
    // `latencyQuantum` rounds the lookahead *down* to a multiple of itself.
    // Callers running this inside an oversampler pass the oversampling factor,
    // so that getLatencySamples() / factor -- the figure reported to the host
    // for delay compensation -- divides exactly. Without it the host would be
    // told a truncated sample count and the plugin would sit a fraction of a
    // sample early (e.g. 44100 * 4 * 0.0045 = 793 samples; 793 / 4 = 198.25,
    // reported as 198, so 0.25 samples of PDC error). Costs at most
    // factor - 1 samples of lookahead.
    void setSampleRate(double sampleRate, int latencyQuantum = 1) noexcept
    {
        sr = sampleRate;
        lookaheadSamples = lookaheadSamplesFor(sampleRate);
        ringFrames       = lookaheadSamples + 1;

        // Clamp into whatever prepare() reserved (a host raising the rate
        // beyond what we were prepared for shortens the lookahead rather than
        // running off the end of the buffer).
        const int capacityFrames = int(delayLine.size() / 2);
        if (capacityFrames >= 2 && ringFrames > capacityFrames)
        {
            ringFrames       = capacityFrames;
            lookaheadSamples = ringFrames - 1;
        }

        // Quantise *after* the capacity clamp, so the clamp can never push the
        // length back off the quantum boundary.
        if (latencyQuantum > 1)
        {
            const int quantised = (lookaheadSamples / latencyQuantum) * latencyQuantum;
            lookaheadSamples = juce::jmax(latencyQuantum, quantised);
            ringFrames       = lookaheadSamples + 1;
        }

        relCoeff = std::exp(-1.0f / float(juce::jmax(1.0, sampleRate * kReleaseSeconds)));
        reset();
    }

    // Lookahead length in samples *at the rate this instance is running at*.
    // Callers running it inside an oversampler must divide by the oversampling
    // factor before reporting latency to the host.
    int getLatencySamples() const noexcept { return lookaheadSamples; }

    void reset() noexcept
    {
        std::fill(delayLine.begin(), delayLine.end(), 0.0f);
        writePos = 0;

        currentGain          = 1.0f;
        targetGain           = 1.0f;
        rampIncrement        = 0.0f;
        rampSamplesRemaining = 0;
        holdCounter          = 0;
    }

    // Processes one stereo sample in place. `mode` may change block-to-block;
    // the lookahead delay itself is always applied so latency stays constant.
    inline void processSample(float& l, float& r, ClipMode mode) noexcept
    {
        // 1. Write the incoming (still undelayed) sample into the lookahead
        //    ring. This sample will be *output* lookaheadSamples calls from now.
        const size_t wbase = size_t(writePos) * 2;
        delayLine[wbase]     = l;
        delayLine[wbase + 1] = r;

        // 2. Advance the gain envelope by exactly one sample.
        if (holdCounter > 0)
            --holdCounter;

        if (rampSamplesRemaining > 0)
        {
            if (--rampSamplesRemaining == 0)
            {
                currentGain   = targetGain;   // land exactly on target, no float drift
                rampIncrement = 0.0f;
            }
            else
            {
                currentGain += rampIncrement;
            }
        }
        else if (holdCounter == 0 && currentGain < 1.0f)
        {
            // Release: no ramp in flight and every peak that caused the current
            // reduction has already left the delay line, so recover to unity.
            currentGain = relCoeff * currentGain + (1.0f - relCoeff);
            if (currentGain > 1.0f - 1.0e-5f)
                currentGain = 1.0f;
            targetGain = currentGain;   // idle invariant: target == current
        }

        // 3. Look at the sample just written and, if it needs gain reduction,
        //    schedule/steepen the ramp so we arrive exactly when it does.
        const float peak = juce::jmax(std::abs(l), std::abs(r));
        const float required = (mode == ClipMode::Brickwall && peak > kCeiling)
                                   ? (kCeiling / peak)
                                   : 1.0f;

        if (required < 1.0f)
        {
            // Hold the reduction until this peak has fully exited the lookahead
            // window (+1 so the release cannot start on the peak's own output
            // sample).
            holdCounter = lookaheadSamples + 1;

            if (required < currentGain)
            {
                const float candidateSlope = (required - currentGain) / float(lookaheadSamples);

                // Steepest slope AND lowest target of every deadline still in
                // flight -- see the class comment for why both are needed.
                const float slope     = (rampSamplesRemaining > 0)
                                            ? juce::jmin(candidateSlope, rampIncrement)
                                            : candidateSlope;
                const float newTarget = (rampSamplesRemaining > 0)
                                            ? juce::jmin(required, targetGain)
                                            : required;

                if (slope > -1.0e-12f)
                {
                    // Degenerate (target essentially where we already are) --
                    // snap rather than divide by ~zero.
                    currentGain          = newTarget;
                    targetGain           = newTarget;
                    rampIncrement        = 0.0f;
                    rampSamplesRemaining = 0;
                }
                else
                {
                    targetGain           = newTarget;
                    rampIncrement        = slope;
                    rampSamplesRemaining = juce::jmax(1,
                        int(std::ceil((newTarget - currentGain) / slope)));
                }
            }
        }

        // 4. Read the delayed sample (written lookaheadSamples calls ago) and
        //    apply the envelope that was scheduled for exactly this moment.
        const int readPos = (writePos + 1) % ringFrames;
        const size_t rbase = size_t(readPos) * 2;
        const float dl = delayLine[rbase];
        const float dr = delayLine[rbase + 1];
        writePos = readPos;

        switch (mode)
        {
            case ClipMode::Soft:
                l = softClipSample(dl);
                r = softClipSample(dr);
                break;

            case ClipMode::Brickwall:
                // jlimit is a safety net only: with the synchronized ramp the
                // envelope is already at or below the required gain by now.
                l = juce::jlimit(-1.0f, 1.0f, dl * currentGain);
                r = juce::jlimit(-1.0f, 1.0f, dr * currentGain);
                break;

            case ClipMode::Off:
            default:
                l = dl;
                r = dr;
                break;
        }
    }

    // Current broadband limiter gain reduction, in dB (<= 0).
    float getGainReductionDb() const noexcept
    {
        return juce::Decibels::gainToDecibels(currentGain, -60.0f);
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
    static int lookaheadSamplesFor(double sampleRate) noexcept
    {
        return juce::jmax(1, int(sampleRate * kLookaheadSeconds));
    }

    static inline float softClipSample(float x) noexcept
    {
        constexpr float drive = 1.6f;
        const float y = std::tanh(x * drive) / std::tanh(drive);
        return juce::jlimit(-1.0f, 1.0f, y);
    }

    double sr = 44100.0;

    // The ring holds lookaheadSamples + 1 frames: reading at (writePos + 1)
    // yields the frame written exactly lookaheadSamples calls ago, which is
    // what getLatencySamples() reports. (Sizing it to lookaheadSamples frames
    // -- as this did before -- gives a real delay of lookaheadSamples - 1,
    // one sample short of the reported latency and of the ramp length.)
    int    lookaheadSamples = 128;
    int    ringFrames       = 129;
    std::vector<float> delayLine;
    int    writePos = 0;

    // Linear-ramp gain envelope (see class comment).
    float  currentGain          = 1.0f;   // gain applied to the sample leaving the delay line
    float  targetGain           = 1.0f;   // value the in-flight ramp lands on
    float  rampIncrement        = 0.0f;   // per-sample delta, negative while ramping down
    int    rampSamplesRemaining = 0;      // samples until currentGain == targetGain
    int    holdCounter          = 0;      // blocks release until the peak has left the buffer
    float  relCoeff             = 0.0f;   // one-pole release coefficient
};
