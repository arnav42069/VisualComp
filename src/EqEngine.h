#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <complex>

// Multi-node parametric EQ (up to 8 nodes) plus, for any node marked
// "linked", a parallel bandpass detector used purely to measure that
// band's energy so the compressor's detector can be made frequency-aware
// (see PluginProcessor::processBlock — "multiband-aware" detection).
//
// Coefficients use the standard RBJ Audio EQ Cookbook formulas.
//
// Thread safety: node parameters are edited from the message thread (the
// EQ panel) and read every audio block. A juce::SpinLock guards the node
// array; the GUI takes a (very short, blocking) lock to write, the audio
// thread takes a non-blocking tryLock once per block to snapshot the
// current values into audio-thread-only state (`active`) and rebuild
// coefficients. If the GUI happens to be mid-write when the audio thread
// checks, that block simply reuses the previous coefficients/flags and
// tries again next block — never blocks the audio thread.
namespace EqTypes
{
    enum NodeType { Bell = 0, LowShelf, HighShelf, HighPass, LowPass, Notch, kNumTypes };
    static const char* const kNames[kNumTypes] = { "BELL", "LO SHELF", "HI SHELF", "HPF", "LPF", "NOTCH" };
}

struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float z1 = 0.0f, z2 = 0.0f;

    inline float process(float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x + z2 - a1 * y;
        z2 = b2 * x - a2 * y;
        return y;
    }

    void reset() noexcept { z1 = z2 = 0.0f; }

    void setCoeffs(double b0_, double b1_, double b2_,
                   double a0_, double a1_, double a2_) noexcept
    {
        b0 = float(b0_ / a0_); b1 = float(b1_ / a0_); b2 = float(b2_ / a0_);
        a1 = float(a1_ / a0_); a2 = float(a2_ / a0_);
    }
};

enum class FilterShape { Bell, LowShelf, HighShelf, HighPass, LowPass, BandPass, Notch };

inline void designBiquad(FilterShape shape, float freqHz, float q, float gainDb,
                         double sampleRate, Biquad& bq) noexcept
{
    freqHz = juce::jlimit(20.0f, float(sampleRate * 0.45), freqHz);
    q      = juce::jmax(0.1f, q);

    const double w0     = 2.0 * juce::MathConstants<double>::pi * double(freqHz) / sampleRate;
    const double cosw0  = std::cos(w0), sinw0 = std::sin(w0);
    const double alpha  = sinw0 / (2.0 * double(q));
    const double A      = std::pow(10.0, double(gainDb) / 40.0);

    double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

    switch (shape)
    {
        case FilterShape::Bell:
            b0 = 1 + alpha * A;  b1 = -2 * cosw0;  b2 = 1 - alpha * A;
            a0 = 1 + alpha / A;  a1 = -2 * cosw0;  a2 = 1 - alpha / A;
            break;

        case FilterShape::LowShelf:
        {
            const double sqrtA = std::sqrt(A);
            const double beta  = sqrtA / double(q);
            b0 =    A * ((A + 1) - (A - 1) * cosw0 + beta * sinw0);
            b1 =  2 * A * ((A - 1) - (A + 1) * cosw0);
            b2 =    A * ((A + 1) - (A - 1) * cosw0 - beta * sinw0);
            a0 =        (A + 1) + (A - 1) * cosw0 + beta * sinw0;
            a1 =   -2 * ((A - 1) + (A + 1) * cosw0);
            a2 =        (A + 1) + (A - 1) * cosw0 - beta * sinw0;
            break;
        }
        case FilterShape::HighShelf:
        {
            const double sqrtA = std::sqrt(A);
            const double beta  = sqrtA / double(q);
            b0 =    A * ((A + 1) + (A - 1) * cosw0 + beta * sinw0);
            b1 = -2 * A * ((A - 1) + (A + 1) * cosw0);
            b2 =    A * ((A + 1) + (A - 1) * cosw0 - beta * sinw0);
            a0 =        (A + 1) - (A - 1) * cosw0 + beta * sinw0;
            a1 =    2 * ((A - 1) - (A + 1) * cosw0);
            a2 =        (A + 1) - (A - 1) * cosw0 - beta * sinw0;
            break;
        }
        case FilterShape::HighPass:
            b0 = (1 + cosw0) / 2; b1 = -(1 + cosw0);   b2 = (1 + cosw0) / 2;
            a0 = 1 + alpha;       a1 = -2 * cosw0;      a2 = 1 - alpha;
            break;

        case FilterShape::LowPass:
            b0 = (1 - cosw0) / 2; b1 = 1 - cosw0;       b2 = (1 - cosw0) / 2;
            a0 = 1 + alpha;       a1 = -2 * cosw0;      a2 = 1 - alpha;
            break;

        case FilterShape::BandPass:
            b0 = alpha; b1 = 0; b2 = -alpha;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha;
            break;

        case FilterShape::Notch:
        default:
            b0 = 1; b1 = -2 * cosw0; b2 = 1;
            a0 = 1 + alpha; a1 = -2 * cosw0; a2 = 1 - alpha;
            break;
    }
    bq.setCoeffs(b0, b1, b2, a0, a1, a2);
}

inline FilterShape shapeForNodeType(int nodeType) noexcept
{
    switch (nodeType)
    {
        case EqTypes::LowShelf:  return FilterShape::LowShelf;
        case EqTypes::HighShelf: return FilterShape::HighShelf;
        case EqTypes::HighPass:  return FilterShape::HighPass;
        case EqTypes::LowPass:   return FilterShape::LowPass;
        case EqTypes::Notch:     return FilterShape::Notch;
        default:                 return FilterShape::Bell;
    }
}

// Soft-knee compressor curve: dB level in, dB gain reduction (<= 0) out.
// Shared by the audio-thread multiband detector (PluginProcessor, one call
// per linked node using that node's own threshold/knee/ratio) and the
// GUI-thread EQ panel (previewing a node's configured dynamic range).
inline float kneeRatioGrDb(float detDb, float thresholdDb, float kneeDb, float ratio) noexcept
{
    const float halfKnee = kneeDb * 0.5f;
    if (kneeDb > 0.0f && detDb >= (thresholdDb - halfKnee) && detDb <= (thresholdDb + halfKnee))
    {
        const float x = detDb - thresholdDb + halfKnee;
        return x * x * (1.0f / ratio - 1.0f) / (2.0f * kneeDb);
    }
    if (detDb > thresholdDb + halfKnee)
        return thresholdDb + (detDb - thresholdDb) / ratio - detDb;
    return 0.0f;
}

// Upward-compression counterpart to kneeRatioGrDb: boosts material *below*
// threshold instead of cutting material above it. Mirrors the same
// knee/ratio math (algebraically continuous with it at the knee edges) but
// triggers below thresholdDb - halfKnee and always returns >= 0 (a boost to
// ADD, rather than a reduction). Selected per-node via EqNodeState::upward.
inline float upwardGrDb(float detDb, float thresholdDb, float kneeDb, float ratio) noexcept
{
    const float halfKnee = kneeDb * 0.5f;
    if (kneeDb > 0.0f && detDb >= (thresholdDb - halfKnee) && detDb <= (thresholdDb + halfKnee))
    {
        const float x = thresholdDb + halfKnee - detDb;
        return x * x * (1.0f / ratio - 1.0f) / (-2.0f * kneeDb);
    }
    if (detDb < thresholdDb - halfKnee)
        return thresholdDb + (detDb - thresholdDb) / ratio - detDb;
    return 0.0f;
}

// FabFilter Pro-MB-style "Range": clamps kneeRatioGrDb/upwardGrDb's output to
// +/- its magnitude (Pro-MB: "limits the maximum amount of applied gain
// change"). Shared by the audio-thread multiband detector and the GUI-thread
// graph preview so they can never disagree about how far a band can swing.
inline float clampedDynamicGrDb(bool upward, float detDb, float thresholdDb, float kneeDb,
                                float ratio, float rangeDb) noexcept
{
    const float maxGr = std::abs(rangeDb);
    return upward ? juce::jmin(maxGr, upwardGrDb(detDb, thresholdDb, kneeDb, ratio))
                  : juce::jmax(-maxGr, kneeRatioGrDb(detDb, thresholdDb, kneeDb, ratio));
}

// One EQ node's parameter state.
struct EqNodeState
{
    bool  enabled = false;
    bool  linked  = false;     // feeds the multiband-aware compressor detector
    float freqHz  = 1000.0f;
    float gainDb  = 0.0f;
    float q       = 0.9f;
    int   type    = EqTypes::Bell;
    float attackMs  = 0.2f;    // this band's detector envelope timing, used only when linked
    float releaseMs = 45.0f;
    // This band's own compressor dynamics, used only when linked AND
    // multiband mode is on (see ParametricEq::applyDynamicBandGain) — lets
    // each band compress differently instead of sharing the global knobs.
    float thresholdDb = -10.0f;   // downward-only now: -inf(-96)..0dB, FabFilter Pro-MB style
    float kneeDb      = 6.0f;
    float ratio       = 2.0f;
    // FabFilter Pro-MB-style Range: +/-30dB, clamps how far kneeRatioGrDb/
    // upwardGrDb can swing this band's gain (see clampedDynamicGrDb). Its
    // sign also auto-engages `upward` below (positive = upward), mirroring
    // Pro-MB using this one knob for both ceiling and direction.
    float rangeDb     = 0.0f;
    // Compression direction for this band's own dynamics: false = downward
    // (kneeRatioGrDb, cuts material above threshold), true = upward
    // (upwardGrDb, boosts material below threshold). A positive rangeDb
    // auto-engages this (see NodeIsland's Range knob); the Dynamic Island's
    // Up/Down toggle remains the manual override.
    bool  upward = false;

    // Independent low/high edges of this band's *detector* passband, in
    // octaves below/above freqHz. Used only when linked, to shape what
    // frequency range feeds this band's compressor — separate from the
    // node's own tone-shaping filter (Bell/Shelf/HPF/LPF via q above),
    // which these do not affect. Free-floating: no relation to any other
    // node's edges, so bands may fully overlap or leave gaps between them.
    float bwLowOct  = 1.0f;
    float bwHighOct = 1.0f;
};

static constexpr int kMaxEqNodes = 8;
constexpr float kDetectorEdgeQ = 0.7071f;   // 2-pole Butterworth-ish edge slope

// Detector passband edges for a node (see EqNodeState::bwLowOct/bwHighOct).
inline float detectorLoHz(const EqNodeState& n) noexcept
{
    return juce::jmax(20.0f, n.freqHz * std::pow(2.0f, -juce::jmax(0.05f, n.bwLowOct)));
}

inline float detectorHiHz(const EqNodeState& n, double sampleRate) noexcept
{
    const float lo = detectorLoHz(n);
    const float hi = n.freqHz * std::pow(2.0f, juce::jmax(0.05f, n.bwHighOct));
    return juce::jmax(lo * 1.02f, juce::jmin(float(sampleRate * 0.45), hi));
}

class ParametricEq
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& f : bandL) f.reset();
        for (auto& f : bandR) f.reset();
        for (auto& d : detectHpL) d.reset();
        for (auto& d : detectHpR) d.reset();
        for (auto& d : detectLpL) d.reset();
        for (auto& d : detectLpR) d.reset();
        for (auto& e : bandEnvelope) e = 0.0f;
        dirty.store(true, std::memory_order_release);
    }

    // ---- GUI-thread API (brief blocking lock) --------------------------------
    void setNode(int i, const EqNodeState& newState)
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        nodes[size_t(i)] = newState;
        dirty.store(true, std::memory_order_release);
    }

    EqNodeState getNode(int i) const
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        return nodes[size_t(i)];
    }

    // GUI-thread only: response curve preview (reads the edited-not-yet-applied state).
    float previewMagnitudeDbAt(float freqHz) const
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        double totalDb = 0.0;
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            const auto& n = nodes[size_t(i)];
            if (!n.enabled) continue;
            totalDb += magnitudeDbForNode(n, freqHz);
        }
        return float(totalDb);
    }

    // GUI-thread only: magnitude response of node i's own shape/freq/Q in
    // isolation, using a caller-supplied gain instead of the node's static
    // gainDb. Used to draw the live per-band gain-reduction dip in multiband
    // mode, independent of whatever static boost/cut the node is set to.
    float previewNodeMagnitudeDbAt(int i, float overrideGainDb, float freqHz) const
    {
        const juce::SpinLock::ScopedLockType sl(lock);
        EqNodeState n = nodes[size_t(i)];
        n.gainDb = overrideGainDb;
        return float(magnitudeDbForNode(n, freqHz));
    }

    // ---- Audio-thread API (never blocks) -------------------------------------
    void maybeUpdateCoefficients()
    {
        if (!dirty.load(std::memory_order_acquire)) return;

        const juce::SpinLock::ScopedTryLockType tl(lock);
        if (!tl.isLocked()) return;   // GUI mid-write — reuse last-good state this block

        active = nodes;
        dirty.store(false, std::memory_order_release);

        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            const auto& n = active[size_t(i)];
            designBiquad(shapeForNodeType(n.type), n.freqHz, n.q, n.gainDb, sr, bandL[size_t(i)]);
            designBiquad(shapeForNodeType(n.type), n.freqHz, n.q, n.gainDb, sr, bandR[size_t(i)]);

            const float loHz = detectorLoHz(n);
            const float hiHz = detectorHiHz(n, sr);
            designBiquad(FilterShape::HighPass, loHz, kDetectorEdgeQ, 0.0f, sr, detectHpL[size_t(i)]);
            designBiquad(FilterShape::HighPass, loHz, kDetectorEdgeQ, 0.0f, sr, detectHpR[size_t(i)]);
            designBiquad(FilterShape::LowPass,  hiHz, kDetectorEdgeQ, 0.0f, sr, detectLpL[size_t(i)]);
            designBiquad(FilterShape::LowPass,  hiHz, kDetectorEdgeQ, 0.0f, sr, detectLpR[size_t(i)]);
            envAttackCoeff[size_t(i)]  = std::exp(-1.0f / (float(sr) * 0.001f * juce::jmax(0.1f, n.attackMs)));
            envReleaseCoeff[size_t(i)] = std::exp(-1.0f / (float(sr) * 0.001f * juce::jmax(1.0f, n.releaseMs)));
        }
    }

    inline void processSample(float& l, float& r) noexcept
    {
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            const auto& n = active[size_t(i)];
            if (n.linked)
            {
                const float dl = detectLpL[size_t(i)].process(detectHpL[size_t(i)].process(l));
                const float dr = detectLpR[size_t(i)].process(detectHpR[size_t(i)].process(r));
                const float e  = 0.5f * (std::abs(dl) + std::abs(dr));
                const float c  = (e > bandEnvelope[size_t(i)]) ? envAttackCoeff[size_t(i)] : envReleaseCoeff[size_t(i)];
                bandEnvelope[size_t(i)] = c * bandEnvelope[size_t(i)] + (1.0f - c) * e;
            }
            if (!n.enabled) continue;
            l = bandL[size_t(i)].process(l);
            r = bandR[size_t(i)].process(r);
        }
    }

    // Audio-thread: index (0..7) of the linked node with the strongest
    // current band energy, or -1 if none linked / all below the noise floor.
    int dominantLinkedBand(float silenceFloor = 0.0008f) const
    {
        int best = -1; float bestE = silenceFloor;
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            if (!active[size_t(i)].linked) continue;
            if (bandEnvelope[size_t(i)] > bestE) { bestE = bandEnvelope[size_t(i)]; best = i; }
        }
        return best;
    }

    // dB level of the strongest linked band right now, or -100 if none linked/active.
    float dominantLinkedBandDb() const
    {
        const int idx = dominantLinkedBand();
        if (idx < 0) return -100.0f;
        return juce::Decibels::gainToDecibels(juce::jmax(1e-6f, bandEnvelope[size_t(idx)]));
    }

    // ---- Multiband mode (audio-thread only) ----------------------------------
    // These read/write `active`/`bandL`/`bandR` directly rather than going
    // through the GUI's SpinLock-guarded `nodes`/dirty-flag path, since they
    // are only ever called from the audio thread (from PluginProcessor,
    // once per block) and must never block.
    bool  isLinked(int i)  const noexcept { return active[size_t(i)].linked; }
    bool  isEnabled(int i) const noexcept { return active[size_t(i)].enabled; }
    float bandEnvelopeFor(int i) const noexcept { return bandEnvelope[size_t(i)]; }

    // This band's own dynamics (see EqNodeState comment above), read from the
    // audio-thread cache — used by the multiband detector to give each linked
    // node its own threshold/knee/ratio instead of sharing the global knobs.
    float thresholdDbFor(int i) const noexcept { return active[size_t(i)].thresholdDb; }
    float kneeDbFor(int i)      const noexcept { return active[size_t(i)].kneeDb; }
    float ratioFor(int i)       const noexcept { return active[size_t(i)].ratio; }
    float rangeDbFor(int i)     const noexcept { return active[size_t(i)].rangeDb; }
    bool  upwardFor(int i)      const noexcept { return active[size_t(i)].upward; }

    // Multiband mode: recomputes node i's own tone-shaping filter using
    // (its static gainDb + grDb) instead of the static gainDb alone, so a
    // loud linked band's boost/cut is dynamically pulled back in real time
    // — i.e. this one node becomes its own dynamic-EQ-style compression
    // band, layered on top of ordinary tone shaping. Called once per block
    // (not per sample) from PluginProcessor; grDb is <= 0 for downward
    // (cut deeper / boost less) and >= 0 for upward (boost more / cut less).
    void applyDynamicBandGain(int i, float grDb) noexcept
    {
        const auto& n = active[size_t(i)];
        if (!n.enabled) return;
        const float dynGainDb = n.gainDb + grDb;
        designBiquad(shapeForNodeType(n.type), n.freqHz, n.q, dynGainDb, sr, bandL[size_t(i)]);
        designBiquad(shapeForNodeType(n.type), n.freqHz, n.q, dynGainDb, sr, bandR[size_t(i)]);
    }

private:
    double magnitudeDbForNode(const EqNodeState& n, float freqHz) const
    {
        Biquad tmp;
        designBiquad(shapeForNodeType(n.type), n.freqHz, n.q, n.gainDb, sr, tmp);
        const double w  = 2.0 * juce::MathConstants<double>::pi * double(freqHz) / sr;
        const std::complex<double> z1 = std::polar(1.0, -w);
        const std::complex<double> z2 = std::polar(1.0, -2.0 * w);
        const std::complex<double> num = double(tmp.b0) + double(tmp.b1) * z1 + double(tmp.b2) * z2;
        const std::complex<double> den = 1.0 + double(tmp.a1) * z1 + double(tmp.a2) * z2;
        return 20.0 * std::log10(std::max(1.0e-6, std::abs(num / den)));
    }

    juce::SpinLock lock;
    std::array<EqNodeState, kMaxEqNodes> nodes;    // GUI-owned, lock-guarded
    std::array<EqNodeState, kMaxEqNodes> active;   // audio-thread-only cache
    std::atomic<bool> dirty { true };

    std::array<Biquad, kMaxEqNodes> bandL, bandR;
    std::array<Biquad, kMaxEqNodes> detectHpL, detectHpR;   // low-edge highpass
    std::array<Biquad, kMaxEqNodes> detectLpL, detectLpR;   // high-edge lowpass
    std::array<float, kMaxEqNodes>  bandEnvelope {};
    std::array<float, kMaxEqNodes>  envAttackCoeff  { 0.90f, 0.90f, 0.90f, 0.90f, 0.90f, 0.90f, 0.90f, 0.90f };
    std::array<float, kMaxEqNodes>  envReleaseCoeff { 0.9995f, 0.9995f, 0.9995f, 0.9995f, 0.9995f, 0.9995f, 0.9995f, 0.9995f };
    double sr = 44100.0;
};
