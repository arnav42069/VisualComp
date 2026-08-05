#include "PluginProcessor.h"
#include "PluginEditor.h"

static inline float fastTanh(float x) noexcept
{
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

static inline float fetSaturate(float x, float drive) noexcept
{
    const float k       = 1.0f + drive * 2.2f;
    const float clipped = fastTanh(x * k) / k;
    const float asym    = drive * 0.018f * clipped * std::abs(clipped);
    return clipped + asym;
}

static inline float tubeSaturate(float x, float drive) noexcept
{
    const float k   = 1.0f + drive * 1.6f;
    const float sat = std::tanh(x * k) / k;
    return sat + drive * 0.025f * sat * sat;
}

VisualCompProcessor::VisualCompProcessor()
    : AudioProcessor(BusesProperties()
          .withInput ("Input",     juce::AudioChannelSet::stereo(), true)
          .withInput ("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output",    juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", createParameterLayout())
{
}

VisualCompProcessor::~VisualCompProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout
VisualCompProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    // Defaults = "Mastering Glue" factory preset: transparent 2:1 bus glue
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"attack", 1}, "Attack",
        juce::NormalisableRange<float>(0.1f, 200.0f, 0.01f, 0.3f), 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"release", 1}, "Release",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.3f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"threshold", 1}, "Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.1f), -10.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"knee", 1}, "Knee",
        juce::NormalisableRange<float>(0.0f, 20.0f, 0.1f), 6.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"ratio", 1}, "Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f, 0.4f), 2.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gainIn", 1}, "Gain In",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"gainOut", 1}, "Gain Out",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"limiter", 1}, "Limiter", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"mix", 1}, "Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 1.0f));
    return { params.begin(), params.end() };
}

void VisualCompProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    envelope          = 0.0f;
    opticalState      = 0.0f;
    tubeMu            = 1.0f;
    inputRmsSmooth    = 0.0f;
    outputRmsSmooth   = 0.0f;
    scEnvelope        = 0.0f;
    autoGainDb        = 0.0f;
    monoMixBuffer.setSize(1, samplesPerBlock * 2);
    scEnvBuf     .setSize(1, samplesPerBlock + 16);

    eq.prepare(sampleRate);
    for (auto& bg : bandGrDb) bg.store(0.0f, std::memory_order_relaxed);
    loudness.prepare(sampleRate);
    const int latency = clipper.prepare(sampleRate, samplesPerBlock);
    setLatencySamples(latency);
}

void VisualCompProcessor::releaseResources() {}

bool VisualCompProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& mainIn  = layouts.getMainInputChannelSet();
    const auto& mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != mainIn) return false;
    if (mainIn != juce::AudioChannelSet::mono() &&
        mainIn != juce::AudioChannelSet::stereo())
        return false;
    const auto& sc = layouts.getChannelSet(true, 1);
    if (!sc.isDisabled() &&
        sc != juce::AudioChannelSet::mono() &&
        sc != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

void VisualCompProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    if (numSamples == 0 || numChannels == 0) return;

    // ── Bypass: pass input unchanged, reset metering ──────────────────────────
    if (bypassed.load(std::memory_order_relaxed))
    {
        gainReductionDb.store(0.0f,   std::memory_order_relaxed);
        currentInputLevelDb.store(-100.0f, std::memory_order_relaxed);
        return;
    }

    // ── Parameter snapshot ────────────────────────────────────────────────────
    const float mixAmt      = apvts.getRawParameterValue("mix")->load();
    const float attackMs    = apvts.getRawParameterValue("attack")->load();
    const float releaseMs   = apvts.getRawParameterValue("release")->load();
    const float thresholdDb = apvts.getRawParameterValue("threshold")->load();
    const float kneeDb      = apvts.getRawParameterValue("knee")->load();
    const float ratio       = apvts.getRawParameterValue("ratio")->load();
    const float gainInDb    = apvts.getRawParameterValue("gainIn")->load();
    const float gainOutDb   = apvts.getRawParameterValue("gainOut")->load();
    const bool  limiterOn   = apvts.getRawParameterValue("limiter")->load() > 0.5f;

    const float inputLinear  = juce::Decibels::decibelsToGain(gainInDb);
    const float outputLinear = juce::Decibels::decibelsToGain(gainOutDb);

    const bool     useSidechain  = sidechainEnabled.load(std::memory_order_relaxed);
    const CompMode mode          = static_cast<CompMode>(compMode.load(std::memory_order_relaxed));
    const bool     useAutoGain   = autoGainEnabled.load(std::memory_order_relaxed);

    // ── Mode-adjusted timing ──────────────────────────────────────────────────
    float effAttack  = attackMs;
    float effRelease = releaseMs;
    switch (mode)
    {
        case CompMode::FET:
            effAttack  = juce::jmin(attackMs,  2.0f);
            effRelease = juce::jmax(releaseMs, 20.0f);
            break;
        case CompMode::Optical:
            effAttack  = juce::jmax(attackMs,  30.0f);
            effRelease = juce::jmax(releaseMs, 100.0f);
            break;
        case CompMode::Tube:
            effAttack  = juce::jmax(attackMs,  50.0f);
            effRelease = juce::jmax(releaseMs, 200.0f);
            break;
        default: break;
    }

    const float attackCoeff  = std::exp(-1.0f / float(currentSampleRate * effAttack  * 0.001));
    const float releaseCoeff = std::exp(-1.0f / float(currentSampleRate * effRelease * 0.001));
    const float fetDrive     = (mode == CompMode::FET)
                             ? 0.28f + gainInDb * 0.006f
                             : 0.18f + gainInDb * 0.003f;

    // ── Auto-gain: measure INPUT RMS before any modification ──────────────────
    if (useAutoGain)
    {
        float sumSq = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i) {
                const float s = buffer.getSample(ch, i) * inputLinear;
                sumSq += s * s;
            }
        const float inRms = sumSq > 0.0f
            ? std::sqrt(sumSq / float(numChannels * numSamples)) : 0.0f;
        const float rmsC = std::exp(-float(numSamples) / float(currentSampleRate * 0.5));
        inputRmsSmooth = rmsC * inputRmsSmooth + (1.0f - rmsC) * inRms;
    }

    // ── Dry capture ───────────────────────────────────────────────────────────
    if (mixAmt < 0.9999f)
    {
        dryBuffer.setSize(numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);
    }

    // ── Input waveform capture (pre-compression) ──────────────────────────────
    monoMixBuffer.setSize(1, numSamples, false, false, true);
    monoMixBuffer.clear();
    const float inScale = inputLinear / float(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
        monoMixBuffer.addFrom(0, 0, buffer, ch, 0, numSamples, inScale);
    inputWaveform.push(monoMixBuffer.getReadPointer(0), numSamples);

    // ── Sidechain bus ─────────────────────────────────────────────────────────
    // Defensive bounds check before touching the sidechain bus: JUCE's
    // Bus::getBusBuffer() does raw pointer arithmetic on the channel-pointer
    // array (buffer.getArrayOfWritePointers() + channelOffset) with NO bounds
    // checking against the buffer we were actually handed. Some hosts (FL
    // Studio observed, when the sidechain input is selected/enabled) report
    // the sidechain bus as active in the layout but call processBlock with a
    // buffer that doesn't actually have channels allocated for it -- calling
    // getBusBuffer() unconditionally then reads channel pointers from past
    // the end of the array and dereferences them, corrupting memory and
    // crashing (typically far from this code, e.g. a null/garbage vtable
    // call). Verify the sidechain bus's channel range actually fits within
    // the buffer we were given before ever calling getBusBuffer() on it.
    const int scChannelOffset = getChannelIndexInProcessBlockBuffer(true, 1, 0);
    const int scChannelCount  = getChannelCountOfBus(true, 1);
    const bool scBufferOk     = scChannelCount > 0
                              && (scChannelOffset + scChannelCount) <= numChannels;
    juce::AudioBuffer<float> scBus = scBufferOk ? getBusBuffer(buffer, true, 1)
                                                 : juce::AudioBuffer<float>();
    const bool hasSC = useSidechain && scBufferOk;

    // ── Sidechain envelope → visualization buffer ─────────────────────────────
    // Always push so sidechainWaveform stays current; display reads it only when SC on.
    {
        scEnvBuf.setSize(1, numSamples, false, false, true);
        auto* envPtr = scEnvBuf.getWritePointer(0);
        // 1 ms attack, 200 ms release — smooth ballistic envelope follower
        const float scAtk = std::exp(-1.0f / float(currentSampleRate * 0.001));
        const float scRel = std::exp(-1.0f / float(currentSampleRate * 0.200));
        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            if (hasSC)
                for (int ch = 0; ch < scBus.getNumChannels(); ++ch)
                    peak = std::max(peak, std::abs(scBus.getSample(ch, i)));
            scEnvelope = (peak > scEnvelope)
                ? scAtk * scEnvelope + (1.0f - scAtk) * peak
                : scRel * scEnvelope + (1.0f - scRel) * peak;
            envPtr[i] = scEnvelope;
        }
        sidechainWaveform.push(envPtr, numSamples);
    }

    float blockPeak = 0.0f;

    // Coefficients for the parametric EQ are only rebuilt when the GUI has
    // actually changed a node (non-blocking; see ParametricEq::maybeUpdateCoefficients).
    eq.maybeUpdateCoefficients();

    // ── Multiband mode ─────────────────────────────────────────────────────────
    // Recompute each *linked* node's own filter once per block using a
    // dynamically-reduced gain (its static gainDb minus that band's own,
    // already-smoothed gain reduction) — this is what turns a linked EQ node
    // into its own independent dynamic-EQ-style compression band. Runs at
    // block rate (not sample rate) since gain reduction only needs to track
    // program material on the same timescale as the attack/release envelope
    // it's derived from, not update every sample.
    const bool multiband = multibandEnabled.load(std::memory_order_relaxed);
    if (multiband)
    {
        const bool forceDemo = debugForceBandGrDemo.load(std::memory_order_relaxed);
        for (int b = 0; b < kMaxEqNodes; ++b)
        {
            if (!eq.isLinked(b))
            {
                bandGrDb[size_t(b)].store(0.0f, std::memory_order_relaxed);
                continue;
            }
            const float bandDetDb = juce::Decibels::gainToDecibels(
                  juce::jmax(1e-6f, eq.bandEnvelopeFor(b)));
            const float bandGr = (forceDemo && b == 0) ? -6.0f
                : clampedDynamicGrDb(eq.upwardFor(b), bandDetDb, eq.thresholdDbFor(b),
                                     eq.kneeDbFor(b), eq.ratioFor(b), eq.rangeDbFor(b));
            eq.applyDynamicBandGain(b, bandGr);
            bandGrDb[size_t(b)].store(bandGr, std::memory_order_relaxed);
        }
    }
    else
    {
        for (auto& bg : bandGrDb) bg.store(0.0f, std::memory_order_relaxed);
    }

    // ── Main processing loop ──────────────────────────────────────────────────
    for (int i = 0; i < numSamples; ++i)
    {
        float chSamples[2] = { 0.0f, 0.0f };
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float raw = buffer.getSample(ch, i) * inputLinear;
            float sat;
            switch (mode)
            {
                case CompMode::FET:  sat = fetSaturate (raw, fetDrive);         break;
                case CompMode::Tube: sat = tubeSaturate(raw, fetDrive * 0.6f);  break;
                default:             sat = raw;                                  break;
            }
            chSamples[ch] = sat;
        }

        // Parametric EQ (tone-shaping) also drives the multiband-aware
        // detector: any node marked "linked" measures its own band's energy
        // here, independent of whether the node's own gain is boosting/cutting.
        float l = chSamples[0];
        float r = (numChannels > 1) ? chSamples[1] : chSamples[0];
        eq.processSample(l, r);
        buffer.setSample(0, i, l);
        if (numChannels > 1) buffer.setSample(1, i, r);

        const float mainPeak = (numChannels > 1) ? juce::jmax(std::abs(l), std::abs(r))
                                                  : std::abs(l);
        blockPeak = std::max(blockPeak, mainPeak);

        float detectPeak = mainPeak;
        if (hasSC)
        {
            float scPeak = 0.0f;
            for (int ch = 0; ch < scBus.getNumChannels(); ++ch)
                scPeak = std::max(scPeak, std::abs(scBus.getSample(ch, i)));
            detectPeak = scPeak;
        }

        float detDb = detectPeak > 1e-7f
            ? juce::Decibels::gainToDecibels(detectPeak) : -140.0f;

        // Single-band mode only: a hot linked EQ band boosts the broadband
        // detector so compression can trigger even when the overall peak is
        // under threshold. In multiband mode each linked band already gets
        // its own independent compression (see above), so this trick is
        // skipped to avoid double-compressing that band's energy.
        if (!multiband && eq.dominantLinkedBand() >= 0)
            detDb = juce::jmax(detDb, eq.dominantLinkedBandDb());

        const float grDb = kneeRatioGrDb(detDb, thresholdDb, kneeDb, ratio);

        switch (mode)
        {
            case CompMode::VCA:
            case CompMode::FET:
            {
                const float c = (grDb < envelope) ? attackCoeff : releaseCoeff;
                envelope = c * envelope + (1.0f - c) * grDb;
                break;
            }
            case CompMode::Optical:
            {
                const float targetLight = juce::jlimit(0.0f, 1.0f, -grDb / 20.0f);
                if (targetLight > opticalState)
                    opticalState = attackCoeff  * opticalState + (1.0f - attackCoeff)  * targetLight;
                else
                    opticalState = releaseCoeff * opticalState + (1.0f - releaseCoeff) * targetLight;
                envelope = -opticalState * 20.0f * (1.0f + opticalState * 0.5f);
                break;
            }
            case CompMode::Tube:
            {
                const float depth  = juce::jlimit(0.0f, 1.0f, -grDb / 20.0f);
                tubeMu             = 1.0f + depth * 1.8f;
                const float tubeGr = grDb * tubeMu;
                const float c      = (tubeGr < envelope) ? attackCoeff : releaseCoeff;
                envelope           = c * envelope + (1.0f - c) * tubeGr;
                break;
            }
        }

        const float compGain = juce::Decibels::decibelsToGain(envelope) * outputLinear;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            const float compressed = buffer.getSample(ch, i) * compGain;
            float out;
            if (limiterOn)
            {
                out = juce::jlimit(-1.0f, 1.0f,
                    (mode == CompMode::Tube)
                        ? tubeSaturate(compressed, 0.08f)
                        : fetSaturate (compressed, 0.08f));
            }
            else
            {
                switch (mode)
                {
                    case CompMode::FET:  out = fetSaturate (compressed, 0.06f); break;
                    case CompMode::Tube: out = tubeSaturate(compressed, 0.10f); break;
                    default:             out = compressed;                       break;
                }
            }
            buffer.setSample(ch, i, out);
        }
    }

    // ── Dry/wet blend ─────────────────────────────────────────────────────────
    if (mixAmt < 0.9999f)
    {
        const float wet = mixAmt, dry = 1.0f - mixAmt;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(ch, i,
                    buffer.getSample(ch, i) * wet + dryBuffer.getSample(ch, i) * dry);
    }

    // ── Auto-gain: measure post-compression OUTPUT (before applying makeup) ───
    if (useAutoGain)
    {
        float sumSq = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i) {
                const float s = buffer.getSample(ch, i);
                sumSq += s * s;
            }
        const float outRms = sumSq > 0.0f
            ? std::sqrt(sumSq / float(numChannels * numSamples)) : 0.0f;

        const float rmsC = std::exp(-float(numSamples) / float(currentSampleRate * 0.5));
        outputRmsSmooth = rmsC * outputRmsSmooth + (1.0f - rmsC) * outRms;

        if (inputRmsSmooth > 1e-6f && outputRmsSmooth > 1e-6f)
        {
            // Target makeup = ratio of input to (pre-makeup) output level, clamped to [0, 24] dB
            const float targetAg = juce::jlimit(0.0f, 24.0f,
                juce::Decibels::gainToDecibels(inputRmsSmooth / outputRmsSmooth));
            // Smooth toward target over ~300 ms to avoid abrupt jumps
            const float agC = std::exp(-float(numSamples) / float(currentSampleRate * 0.3));
            autoGainDb = agC * autoGainDb + (1.0f - agC) * targetAg;
        }
        else if (outputRmsSmooth <= 1e-6f)
        {
            // Silence — slowly decay auto-gain toward 0 so it doesn't latch
            const float agC = std::exp(-float(numSamples) / float(currentSampleRate * 1.0));
            autoGainDb = agC * autoGainDb;
        }

        // Apply auto makeup gain to the buffer
        const float agLinear = juce::Decibels::decibelsToGain(autoGainDb);
        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(ch, i, buffer.getSample(ch, i) * agLinear);
    }
    else
    {
        // When disabled, reset smoothers so re-enabling ramps up cleanly
        autoGainDb      = 0.0f;
        inputRmsSmooth  = 0.0f;
        outputRmsSmooth = 0.0f;
    }

    // ── Final-stage clipping + post-output loudness metering ─────────────────
    {
        const ClipMode cm = static_cast<ClipMode>(clipMode.load(std::memory_order_relaxed));
        for (int i = 0; i < numSamples; ++i)
        {
            float l = buffer.getSample(0, i);
            float r = (numChannels > 1) ? buffer.getSample(1, i) : l;
            clipper.processSample(l, r, cm);
            buffer.setSample(0, i, l);
            if (numChannels > 1) buffer.setSample(1, i, r);
            loudness.pushSample(l, r);
        }
        meterPeakDb.store(loudness.peakDb(), std::memory_order_relaxed);
        meterRmsDb.store(loudness.rmsDb(), std::memory_order_relaxed);
        meterMomLufs.store(loudness.momentaryLufs(), std::memory_order_relaxed);
        meterShortLufs.store(loudness.shortTermLufs(), std::memory_order_relaxed);
    }

    // ── Output waveform capture (post-clip: matches what is actually heard) ──
    monoMixBuffer.clear();
    const float outScale = 1.0f / float(numChannels);
    for (int ch = 0; ch < numChannels; ++ch)
        monoMixBuffer.addFrom(0, 0, buffer, ch, 0, numSamples, outScale);
    outputWaveform.push(monoMixBuffer.getReadPointer(0), numSamples);

    // ── Metering ──────────────────────────────────────────────────────────────
    gainReductionDb.store(envelope, std::memory_order_relaxed);
    const float inputDb = blockPeak > 1e-7f
        ? juce::Decibels::gainToDecibels(blockPeak) : -100.0f;
    currentInputLevelDb.store(inputDb, std::memory_order_relaxed);
    activeEqBand.store(eq.dominantLinkedBand(), std::memory_order_relaxed);
}

void VisualCompProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty("compMode",   compMode.load(std::memory_order_relaxed), nullptr);
    state.setProperty("clipMode",   clipMode.load(std::memory_order_relaxed), nullptr);
    state.setProperty("presetName", currentPresetName, nullptr);
    // eqPanelOpen deliberately not saved — see setStateInformation, it's
    // never read back, so always starts closed.
    state.setProperty("curveGrPanelOpen", curveGrPanelOpen, nullptr);
    // multibandEnabled deliberately not saved — see setStateInformation,
    // it's never read back, so it's always on regardless of what an older
    // saved project had.

    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        const auto n = eq.getNode(i);
        const juce::String p = "eq" + juce::String(i) + "_";
        state.setProperty(p + "enabled", n.enabled, nullptr);
        state.setProperty(p + "linked",  n.linked,  nullptr);
        state.setProperty(p + "freq",    n.freqHz,  nullptr);
        state.setProperty(p + "gain",    n.gainDb,  nullptr);
        state.setProperty(p + "q",       n.q,       nullptr);
        state.setProperty(p + "type",    n.type,    nullptr);
        state.setProperty(p + "attackMs",  n.attackMs,  nullptr);
        state.setProperty(p + "releaseMs", n.releaseMs, nullptr);
        state.setProperty(p + "thresholdDb", n.thresholdDb, nullptr);
        state.setProperty(p + "kneeDb",      n.kneeDb,      nullptr);
        state.setProperty(p + "ratio",       n.ratio,       nullptr);
        state.setProperty(p + "rangeDb",     n.rangeDb,     nullptr);
        state.setProperty(p + "bwLowOct",    n.bwLowOct,    nullptr);
        state.setProperty(p + "bwHighOct",   n.bwHighOct,   nullptr);
        state.setProperty(p + "upward",      n.upward,      nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void VisualCompProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
        {
            auto state = juce::ValueTree::fromXml(*xml);
            apvts.replaceState(state);
            compMode.store(int(state.getProperty("compMode", 0)), std::memory_order_relaxed);
            clipMode.store(int(state.getProperty("clipMode", 0)), std::memory_order_relaxed);
            currentPresetName = state.getProperty("presetName", "Mastering Glue").toString();
            // Deliberately not restored from state — the docked EQ panel is a
            // transient view, not song data, and should always start closed
            // regardless of whether an earlier session (or this same project,
            // last time it was saved) happened to leave it open.
            eqPanelOpen = false;
            curveGrPanelOpen = bool(state.getProperty("curveGrPanelOpen", false));
            // Deliberately not restored from state — multiband mode is always
            // on now (no user-facing toggle any more), so an older project
            // that saved it as off must not be able to turn it back off.
            multibandEnabled.store(true, std::memory_order_relaxed);

            for (int i = 0; i < kMaxEqNodes; ++i)
            {
                const juce::String p = "eq" + juce::String(i) + "_";
                EqNodeState n;
                n.enabled = bool(state.getProperty(p + "enabled", false));
                n.linked  = bool(state.getProperty(p + "linked",  false));
                n.freqHz  = float(state.getProperty(p + "freq",   1000.0));
                n.gainDb  = float(state.getProperty(p + "gain",   0.0));
                n.q       = float(state.getProperty(p + "q",      0.9));
                n.type    = int  (state.getProperty(p + "type",   0));
                n.attackMs  = float(state.getProperty(p + "attackMs",  0.2));
                n.releaseMs = float(state.getProperty(p + "releaseMs", 45.0));
                n.upward      = bool (state.getProperty(p + "upward",       false));
                // Threshold is downward-only (-60..0, the knob's own floor —
                // see setupThresholdKnobRange() in EqEngine.h); clamp in case
                // this is an older project saved with a value outside that
                // (the range has moved a few times: +/-96, then -90..0, now
                // -60..0). rangeDb's fallback (for projects saved before
                // Range existed at all) uses upward's already-loaded
                // direction at max magnitude, so old linked bands keep doing
                // roughly as much as they used to rather than silently going
                // inert at the new default of 0dB.
                n.thresholdDb = juce::jlimit(-60.0f, 0.0f,
                                    float(state.getProperty(p + "thresholdDb", -20.0)));
                n.kneeDb      = float(state.getProperty(p + "kneeDb",        6.0));
                n.ratio       = float(state.getProperty(p + "ratio",         2.0));
                n.rangeDb     = float(state.getProperty(p + "rangeDb", n.upward ? 30.0 : -30.0));
                n.bwLowOct    = float(state.getProperty(p + "bwLowOct",      1.0));
                n.bwHighOct   = float(state.getProperty(p + "bwHighOct",     1.0));
                eq.setNode(i, n);
            }
        }
}

juce::AudioProcessorEditor* VisualCompProcessor::createEditor()
{
    return new VisualCompEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VisualCompProcessor();
}
