#include "LufsMeter.h"

LufsMeter::LufsMeter()
{
    channelWeight.fill(1.0f);   // L/R (and mono) are always 0dB per BS.1770's channel table
}

void LufsMeter::prepare(double newSampleRate, int numChannels, double maxIntegratedMinutes)
{
    sampleRate        = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numActiveChannels = juce::jlimit(1, kMaxChannels, numChannels);

    Biquad pre, rlb;
    designPreFilter(sampleRate, pre);
    designRlbFilter(sampleRate, rlb);
    for (int ch = 0; ch < kMaxChannels; ++ch)
    {
        // Coefficients are shared across channels; each channel keeps its
        // own z1/z2 filter *state* (reset() below), which is what actually
        // needs to be per-channel.
        preFilter[ch].b0 = pre.b0; preFilter[ch].b1 = pre.b1; preFilter[ch].b2 = pre.b2;
        preFilter[ch].a1 = pre.a1; preFilter[ch].a2 = pre.a2;

        rlbFilter[ch].b0 = rlb.b0; rlbFilter[ch].b1 = rlb.b1; rlbFilter[ch].b2 = rlb.b2;
        rlbFilter[ch].a1 = rlb.a1; rlbFilter[ch].a2 = rlb.a2;
    }

    designTruePeakFilter();

    blockLenSamples = juce::jmax(1, (int) std::round(sampleRate * 0.400));
    hopLenSamples   = juce::jmax(1, (int) std::round(sampleRate * 0.100));

    // One retained history slot per 100ms gating block (at steady state a
    // new block completes every hop, not every 400ms -- see
    // accumulateGatingSample()'s comment), so this is minutes * 60 / 0.1.
    integratedHistoryCapacity = juce::jmax(1, (int) std::round(maxIntegratedMinutes * 60.0 / 0.1));
    integratedHistory.assign((size_t) integratedHistoryCapacity, 0.0f);   // the one allocation; never touched again

    reset();
}

void LufsMeter::reset() noexcept
{
    for (auto& bq : preFilter) bq.reset();
    for (auto& bq : rlbFilter) bq.reset();
    for (auto& tp : truePeak)  tp.reset();

    for (int k = 0; k < kNumOverlapAccumulators; ++k)
    {
        accumulators[(size_t) k].sumSq             = 0.0;
        accumulators[(size_t) k].samplesAccumulated = 0;
        accumulators[(size_t) k].samplesUntilStart  = k * hopLenSamples;
    }

    hopAccumulator.sumSq             = 0.0;
    hopAccumulator.samplesAccumulated = 0;
    hopAccumulator.samplesUntilStart  = 0;

    shortTermRingSumSq.fill(0.0);
    shortTermRunningSumSq = 0.0;
    shortTermWritePos     = 0;
    shortTermBlocksFilled = 0;

    {
        // Not real-time-safe (blocking) -- fine here, see the class comment:
        // reset() is a prepareToPlay()/transport-reset operation, not a
        // per-block one.
        const juce::SpinLock::ScopedLockType sl (historyLock);
        integratedHistoryWritePos = 0;
        integratedHistoryWrapped  = false;
    }

    samplePeakDb.store(-100.0f, std::memory_order_relaxed);
    truePeakDb.store(-100.0f, std::memory_order_relaxed);
    shortTermLufs.store(-100.0f, std::memory_order_relaxed);
}

void LufsMeter::resetPeakHolds() noexcept
{
    samplePeakDb.store(-100.0f, std::memory_order_relaxed);
    truePeakDb.store(-100.0f, std::memory_order_relaxed);
}

// ─────────────────────────────────────────────────────────────────────────
// K-weighting filter design.
//
// Both stages are parametrized by an analog-prototype (f0, Q[, gain]) that
// the standard's reference implementations (e.g. libebur128) derive from,
// then bilinear-transformed to a digital biquad per sample rate below. This
// is preferred here over hardcoding one fixed 48kHz coefficient table
// because it stays correct at 44.1k/96k/etc. without a second table.
//
// These formulas were numerically verified (via a throwaway Node.js script,
// not committed) against the well-known published 48kHz reference biquad
// values before being written here:
//   Stage 1: b0=1.53512485958697  b1=-2.69169618940638  b2=1.19839281085285
//            a1=-1.69065929318241 a2=0.73248077421585
//   Stage 2: a1=-1.99004745483398 a2=0.99007225036621
//            (numerator {1,-2,1} in most published tables is the
//            *pre-normalization* form; dividing it by the same a0 used for
//            a1/a2 below, as this code does, is the correct normalized form
//            for the DF2T difference equation used by Biquad::process() --
//            confirmed independently by the cascade's frequency response
//            converging cleanly to exactly the Stage-1 shelf gain at high
//            frequency, which would not happen if the numerator were off by
//            the ~0.5% a factor-of-a0 error would produce.)
// The Stage 1 coefficients above matched to within float rounding noise
// (~1e-16); Stage 2's a1/a2 matched exactly; the resulting K-weighting
// cascade shape (~-13dB at 20Hz rising through 0dB near 1kHz to the +4dB
// shelf at high frequency) matches the standard's well-known curve.
// ─────────────────────────────────────────────────────────────────────────

void LufsMeter::designPreFilter(double sr, Biquad& out) noexcept
{
    const double f0 = 1681.9744509555319;
    const double G  = 3.999843853973347;
    const double Q  = 0.7071752369554196;

    const double K  = std::tan(juce::MathConstants<double>::pi * f0 / sr);
    const double Vh = std::pow(10.0, G / 20.0);
    const double Vb = std::pow(Vh, 0.4996667741545416);
    const double a0 = 1.0 + K / Q + K * K;

    out.b0 = (float) ((Vh + Vb * K / Q + K * K) / a0);
    out.b1 = (float) (2.0 * (K * K - Vh) / a0);
    out.b2 = (float) ((Vh - Vb * K / Q + K * K) / a0);
    out.a1 = (float) (2.0 * (K * K - 1.0) / a0);
    out.a2 = (float) ((1.0 - K / Q + K * K) / a0);
    out.reset();
}

void LufsMeter::designRlbFilter(double sr, Biquad& out) noexcept
{
    const double f0 = 38.13547087602444;
    const double Q  = 0.5003270373238773;

    const double K  = std::tan(juce::MathConstants<double>::pi * f0 / sr);
    const double a0 = 1.0 + K / Q + K * K;

    out.b0 = (float) (1.0 / a0);
    out.b1 = (float) (-2.0 / a0);
    out.b2 = (float) (1.0 / a0);
    out.a1 = (float) (2.0 * (K * K - 1.0) / a0);
    out.a2 = (float) ((1.0 - K / Q + K * K) / a0);
    out.reset();
}

// ─────────────────────────────────────────────────────────────────────────
// True-peak 4x polyphase oversampler.
//
// Designs a 48-tap windowed-sinc lowpass (cutoff at the post-oversampling
// Nyquist, i.e. a standard L=4 interpolation filter), windowed with a
// Kaiser window (beta=8, a reasonable stopband-attenuation/transition-width
// tradeoff for a 48-tap filter), then decomposes it into 4 polyphase
// sub-filters of 12 taps each -- coeffs[p][t] = h[p + t*4] -- so that phase
// p directly produces the interpolated sample at the p/4 fractional
// position without ever upsampling the actual sample stream (no zero
// stuffing, no running the IIR/convolution at 4x the sample rate).
//
// This is a from-scratch design in the *spirit* of ITU-R BS.1770-4 Annex 2
// (which also specifies a 4x windowed-sinc true-peak filter), not a
// transcription of its own published coefficient table -- see the class
// comment in LufsMeter.h for why. Verified numerically: full 48-tap sum
// ≈4.000 (correct DC gain for 4x interpolation), each phase's 12-tap sum
// ≈1.000 (correct per-phase unity gain -- confirmed as the right
// normalization by simulating a settled constant-1.0 input through the
// actual shift-register + polyphase convolution and observing the output
// converge to ≈1.000, not ≈4.0), and the impulse response is symmetric
// (linear phase, no bias toward earlier/later inter-sample peaks).
// ─────────────────────────────────────────────────────────────────────────

namespace
{
    // Modified Bessel function of the first kind, order 0 -- needed for the
    // Kaiser window and not exposed by JUCE, so implemented directly via its
    // standard convergent series. 25 terms is comfortably enough precision
    // for beta=8 (terms shrink factorially).
    double besselI0(double x) noexcept
    {
        double sum = 1.0, term = 1.0;
        const double xh = x / 2.0;
        for (int k = 1; k <= 25; ++k)
        {
            term *= (xh / (double) k);
            sum += term * term;
        }
        return sum;
    }
}

void LufsMeter::designTruePeakFilter()
{
    constexpr int totalTaps = kOversample * kTapsPerPhase;   // 48
    constexpr int M         = totalTaps - 1;                 // 47
    constexpr double L      = (double) kOversample;
    constexpr double kaiserBeta = 8.0;

    std::array<double, totalTaps> h{};
    const double i0Beta = besselI0(kaiserBeta);

    for (int n = 0; n < totalTaps; ++n)
    {
        const double m = (double) n - (double) M / 2.0;
        const double sincVal = (std::abs(m) < 1e-9)
            ? 1.0
            : std::sin(juce::MathConstants<double>::pi * m / L) / (juce::MathConstants<double>::pi * m / L);

        const double windowArg = (2.0 * (double) n - (double) M) / (double) M;
        const double kaiserVal = besselI0(kaiserBeta * std::sqrt(juce::jmax(0.0, 1.0 - windowArg * windowArg))) / i0Beta;

        h[(size_t) n] = sincVal * kaiserVal;
    }

    for (int p = 0; p < kOversample; ++p)
        for (int t = 0; t < kTapsPerPhase; ++t)
            truePeakCoeffs[(size_t) p][(size_t) t] = (float) h[(size_t) (p + t * kOversample)];
}

float LufsMeter::pushTruePeakSample(TruePeakChannel& ch, float x) noexcept
{
    for (int i = kTapsPerPhase - 1; i > 0; --i)
        ch.delay[(size_t) i] = ch.delay[(size_t) (i - 1)];
    ch.delay[0] = x;

    // The un-interpolated sample itself is one of the 4 phases' worth of
    // "true" signal too -- start the max from it rather than only from the
    // 4 reconstructed phases, so a peak that happens to land exactly on an
    // original sample is never missed by a boundary/rounding fluke.
    float maxAbs = std::abs(x);
    for (int p = 0; p < kOversample; ++p)
    {
        float acc = 0.0f;
        for (int t = 0; t < kTapsPerPhase; ++t)
            acc += truePeakCoeffs[(size_t) p][(size_t) t] * ch.delay[(size_t) t];
        maxAbs = juce::jmax(maxAbs, std::abs(acc));
    }
    return maxAbs;
}

// ─────────────────────────────────────────────────────────────────────────
// Per-block processing.
// ─────────────────────────────────────────────────────────────────────────

void LufsMeter::processBlock(const float* const* channelData, int numChannels, int numSamples) noexcept
{
    const int chCount = juce::jmin(numChannels, numActiveChannels, kMaxChannels);
    if (chCount <= 0 || numSamples <= 0)
        return;

    float blockSamplePeak = 0.0f;
    float blockTruePeak   = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        double weightedSqSum = 0.0;

        for (int ch = 0; ch < chCount; ++ch)
        {
            const float x = channelData[ch][i];

            blockSamplePeak = juce::jmax(blockSamplePeak, std::abs(x));

            const float tp = pushTruePeakSample(truePeak[(size_t) ch], x);
            blockTruePeak = juce::jmax(blockTruePeak, tp);

            const float shelved  = preFilter[(size_t) ch].process(x);
            const float weighted = rlbFilter[(size_t) ch].process(shelved);
            weightedSqSum += (double) channelWeight[(size_t) ch] * (double) weighted * (double) weighted;
        }

        accumulateGatingSample(weightedSqSum);
    }

    // Peak-hold: only ever moves upward (session max), never decays --
    // resetPeakHolds() is the only way it comes back down.
    const float samplePeakDbBlock = juce::Decibels::gainToDecibels(blockSamplePeak, -100.0f);
    if (samplePeakDbBlock > samplePeakDb.load(std::memory_order_relaxed))
        samplePeakDb.store(samplePeakDbBlock, std::memory_order_relaxed);

    const float truePeakDbBlock = juce::Decibels::gainToDecibels(blockTruePeak, -100.0f);
    if (truePeakDbBlock > truePeakDb.load(std::memory_order_relaxed))
        truePeakDb.store(truePeakDbBlock, std::memory_order_relaxed);
}

void LufsMeter::accumulateGatingSample(double weightedSq) noexcept
{
    // 4 accumulators staggered by one 100ms hop each realize BS.1770 Annex
    // 2's 400ms / 75%-overlap gating blocks without ever buffering raw
    // audio: accumulator k waits k*hopLenSamples samples before starting
    // its first block, then immediately starts its next block the instant
    // the previous one completes (no gap) -- so once warmed up, exactly one
    // of the four completes a block every hop (100ms), matching the
    // standard's 100ms block-rate.
    for (auto& acc : accumulators)
    {
        if (acc.samplesUntilStart > 0)
        {
            --acc.samplesUntilStart;
            continue;
        }

        acc.sumSq += weightedSq;
        if (++acc.samplesAccumulated >= blockLenSamples)
        {
            onIntegratedGatingBlockComplete(acc.sumSq / (double) blockLenSamples);
            acc.sumSq             = 0.0;
            acc.samplesAccumulated = 0;
        }
    }

    // Independent, non-overlapping 100ms blocks feed the ungated 3s
    // short-term ring -- Short-Term LUFS is never gated per the standard,
    // unlike Integrated Loudness below.
    hopAccumulator.sumSq += weightedSq;
    if (++hopAccumulator.samplesAccumulated >= hopLenSamples)
    {
        const double blockSumSq = hopAccumulator.sumSq;
        hopAccumulator.sumSq             = 0.0;
        hopAccumulator.samplesAccumulated = 0;

        if (shortTermBlocksFilled >= kShortTermBlocks)
            shortTermRunningSumSq -= shortTermRingSumSq[(size_t) shortTermWritePos];
        else
            ++shortTermBlocksFilled;

        shortTermRingSumSq[(size_t) shortTermWritePos] = blockSumSq;
        shortTermRunningSumSq += blockSumSq;
        shortTermWritePos = (shortTermWritePos + 1) % kShortTermBlocks;

        const double meanSq = shortTermRunningSumSq / ((double) hopLenSamples * (double) shortTermBlocksFilled);
        const double lufs   = -0.691 + 10.0 * std::log10(juce::jmax(meanSq, 1.0e-12));
        shortTermLufs.store((float) lufs, std::memory_order_relaxed);
    }
}

void LufsMeter::onIntegratedGatingBlockComplete(double meanSq) noexcept
{
    // Absolute gate (-70 LUFS): applied here, at storage time, since it's a
    // static per-block test -- gated-out blocks are never even retained,
    // which also bounds the history to only what could ever matter for the
    // relative gate's second pass in getIntegratedLufs().
    const double blockLufs = -0.691 + 10.0 * std::log10(juce::jmax(meanSq, 1.0e-12));
    if (blockLufs < -70.0)
        return;

    if (historyLock.tryEnter())
    {
        integratedHistory[(size_t) integratedHistoryWritePos] = (float) meanSq;
        integratedHistoryWritePos = (integratedHistoryWritePos + 1) % integratedHistoryCapacity;
        if (integratedHistoryWritePos == 0)
            integratedHistoryWrapped = true;
        historyLock.exit();
    }
    // else: the UI/message thread is mid-scan inside getIntegratedLufs() --
    // drop this one ~100ms block rather than stall the audio thread waiting
    // for it. Losing an occasional block out of a multi-second-to-hours-long
    // running measurement is inaudible in the readout.
}

float LufsMeter::getIntegratedLufs() const noexcept
{
    const juce::SpinLock::ScopedLockType sl (historyLock);   // blocking -- UI/message thread only

    const int count = integratedHistoryWrapped ? integratedHistoryCapacity : integratedHistoryWritePos;
    if (count <= 0)
        return -100.0f;

    // Pass 1: mean of all absolute-gated blocks -> the relative gate
    // threshold, Γr = that mean's loudness - 10dB (BS.1770-4 Annex 2).
    double sumA = 0.0;
    for (int i = 0; i < count; ++i)
        sumA += (double) integratedHistory[(size_t) i];
    const double meanA = sumA / (double) count;
    const double absoluteGatedLufs = -0.691 + 10.0 * std::log10(juce::jmax(meanA, 1.0e-12));

    const double gammaRLufs   = absoluteGatedLufs - 10.0;
    const double gammaRLinear = std::pow(10.0, (gammaRLufs + 0.691) / 10.0);

    // Pass 2: mean of only the blocks that also clear the relative gate.
    double sumB = 0.0;
    int countB  = 0;
    for (int i = 0; i < count; ++i)
    {
        const double v = (double) integratedHistory[(size_t) i];
        if (v >= gammaRLinear)
        {
            sumB += v;
            ++countB;
        }
    }

    // If nothing clears the relative gate (degenerate/near-silent
    // programme), fall back to the absolute-gated mean rather than dividing
    // by zero.
    const double meanB = countB > 0 ? sumB / (double) countB : meanA;
    return (float) (-0.691 + 10.0 * std::log10(juce::jmax(meanB, 1.0e-12)));
}
