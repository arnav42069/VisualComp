#include "WaveformDisplay.h"
#include "PluginProcessor.h"
#include "Theme.h"
#include <cstring>   // memmove

WaveformDisplay::WaveformDisplay(const juce::String& t,
                                 juce::Colour        c,
                                 WaveformBuffer&     buf,
                                 std::atomic<float>* thresholdDb,
                                 WaveformBuffer*     scBuffer,
                                 std::atomic<bool>*  scEnabled,
                                 std::atomic<float>* ratio,
                                 std::atomic<float>* knee,
                                 std::atomic<float>* attack,
                                 std::atomic<float>* release,
                                 bool                clickToPause)
    : title(t), waveColour(c), waveformBuffer(buf), thresholdParamDb(thresholdDb),
      scWaveformBuffer(scBuffer), scEnabledAtomic(scEnabled),
      ratioParam(ratio), kneeParam(knee), attackParam(attack), releaseParam(release),
      allowClickPause(clickToPause)
{
    startTimerHz(120);
    if (allowClickPause)
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

//==============================================================================
// mouseDown: left click toggles pause (input display only); right click
// always fires onRightClick so the owning editor can show its enlarge menu.
//==============================================================================

void WaveformDisplay::mouseDown(const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onRightClick)
            onRightClick(e);
        return;
    }

    if (allowClickPause)
    {
        paused = ! paused;
        repaint();
    }
}

//==============================================================================
// resized: force re-initialisation on next timer tick
//==============================================================================

void WaveformDisplay::resized()
{
    displayWidth = 0;
}

//==============================================================================
// timerCallback: called at 120 Hz on the message thread
//==============================================================================

void WaveformDisplay::timerCallback()
{
    const int pixW = getWidth() - 2 * kFrameW - kLabelW;
    if (pixW <= 0) return;

    // While paused, skip the buffer refresh entirely so the trace/fill stay
    // frozen in place -- unless the display was never (or no longer
    // correctly) initialised, e.g. right after construction or a resize,
    // in which case we still need to populate it once so it isn't blank.
    const bool needsInit = (displayWidth != pixW || lastWritePos < 0);

    if (! paused || needsInit)
    {
        const int wp = waveformBuffer.writePos.load(std::memory_order_acquire);

        if (needsInit)
            initialiseDisplayBuffer(pixW);
        else
            updateDisplayBuffer(pixW, wp);

        if (scWaveformBuffer != nullptr && scLastWritePos >= 0 && displayWidth == pixW)
        {
            const int scWp = scWaveformBuffer->writePos.load(std::memory_order_acquire);
            updateScDisplayBuffer(pixW, scWp);
        }
    }

    // paint() always re-reads live params (threshold/ratio/knee/attack/
    // release) every frame regardless of pause state, so the transfer-curve
    // overlay keeps animating even while the trace itself is frozen.
    repaint();
}

//==============================================================================
// initialiseDisplayBuffer: fill the entire pixel buffer from audio history
//==============================================================================

void WaveformDisplay::initialiseDisplayBuffer(int pixW)
{
    displayWidth = pixW;
    dispMin.assign(pixW, 0.0f);
    dispMax.assign(pixW, 0.0f);
    pixelAccum = 0.0f;

    const int bufSize = WaveformBuffer::size;
    const int wp      = waveformBuffer.writePos.load(std::memory_order_acquire);
    lastWritePos = wp;

    const int spp = std::max(1, kSamplesToShow / pixW);

    for (int px = 0; px < pixW; ++px)
    {
        // px=0 = oldest (left edge), px=pixW-1 = newest (right edge)
        const int distFromNewest = pixW - 1 - px;
        float mn = 0.0f, mx = 0.0f;

        for (int s = 0; s < spp; ++s)
        {
            const int offset = distFromNewest * spp + s;
            const int idx    = ((wp - 1 - offset) % bufSize + bufSize) % bufSize;
            const float v    = waveformBuffer.data[static_cast<size_t>(idx)];
            if (v > mx) mx = v;
            if (v < mn) mn = v;
        }
        dispMin[px] = mn;
        dispMax[px] = mx;
    }

    // Initialise SC envelope display buffer if present
    if (scWaveformBuffer != nullptr)
    {
        scDispEnv.assign(pixW, 0.0f);
        scPixelAccum = 0.0f;
        const int scWp = scWaveformBuffer->writePos.load(std::memory_order_acquire);
        scLastWritePos = scWp;

        for (int px = 0; px < pixW; ++px)
        {
            const int distFromNewest = pixW - 1 - px;
            float maxV = 0.0f;
            for (int s = 0; s < spp; ++s)
            {
                const int offset = distFromNewest * spp + s;
                const int idx    = ((scWp - 1 - offset) % bufSize + bufSize) % bufSize;
                const float v    = scWaveformBuffer->data[static_cast<size_t>(idx)];
                if (v > maxV) maxV = v;
            }
            scDispEnv[px] = maxV;
        }
    }
}

//==============================================================================
// updateDisplayBuffer: slide left, append new columns from the right
//==============================================================================

void WaveformDisplay::updateDisplayBuffer(int pixW, int wp)
{
    const int bufSize    = WaveformBuffer::size;
    const int newSamples = (wp - lastWritePos + bufSize) % bufSize;
    if (newSamples == 0) return;

    const float sppF = float(kSamplesToShow) / float(pixW);
    pixelAccum += float(newSamples) / sppF;

    const int shift = int(pixelAccum);
    pixelAccum -= float(shift);

    if (shift == 0) { lastWritePos = wp; return; }

    // More than a full screen of new audio — reinitialise rather than shift
    if (shift >= pixW)
    {
        lastWritePos = wp;
        initialiseDisplayBuffer(pixW);
        return;
    }

    // Slide existing columns left by 'shift' positions
    std::memmove(dispMin.data(), dispMin.data() + shift, size_t(pixW - shift) * sizeof(float));
    std::memmove(dispMax.data(), dispMax.data() + shift, size_t(pixW - shift) * sizeof(float));

    // Fill 'shift' new columns entering from the right
    const int sppI = std::max(1, int(sppF));
    for (int col = 0; col < shift; ++col)
    {
        // col=0 is oldest new column, col=shift-1 is newest
        const int distFromNewest = shift - 1 - col;
        const int px             = pixW - shift + col;

        float mn = 0.0f, mx = 0.0f;
        for (int s = 0; s < sppI; ++s)
        {
            const int offset = distFromNewest * sppI + s;
            const int idx    = ((wp - 1 - offset) % bufSize + bufSize) % bufSize;
            const float v    = waveformBuffer.data[static_cast<size_t>(idx)];
            if (v > mx) mx = v;
            if (v < mn) mn = v;
        }
        dispMin[px] = mn;
        dispMax[px] = mx;
    }

    lastWritePos = wp;
}

//==============================================================================
// updateScDisplayBuffer: slide scDispEnv left, append new columns from the right
//==============================================================================

void WaveformDisplay::updateScDisplayBuffer(int pixW, int scWp)
{
    const int bufSize    = WaveformBuffer::size;
    const int newSamples = (scWp - scLastWritePos + bufSize) % bufSize;
    if (newSamples == 0) return;

    const float sppF = float(kSamplesToShow) / float(pixW);
    scPixelAccum += float(newSamples) / sppF;

    const int shift = int(scPixelAccum);
    scPixelAccum -= float(shift);

    if (shift == 0) { scLastWritePos = scWp; return; }

    if (shift >= pixW)
    {
        scLastWritePos = scWp;
        scDispEnv.assign(pixW, 0.0f);
        scPixelAccum = 0.0f;
        return;
    }

    std::memmove(scDispEnv.data(), scDispEnv.data() + shift,
                 size_t(pixW - shift) * sizeof(float));

    const int sppI = std::max(1, int(sppF));
    for (int col = 0; col < shift; ++col)
    {
        const int distFromNewest = shift - 1 - col;
        const int px             = pixW - shift + col;
        float maxV = 0.0f;
        for (int s = 0; s < sppI; ++s)
        {
            const int offset = distFromNewest * sppI + s;
            const int idx    = ((scWp - 1 - offset) % bufSize + bufSize) % bufSize;
            const float v    = scWaveformBuffer->data[static_cast<size_t>(idx)];
            if (v > maxV) maxV = v;
        }
        scDispEnv[px] = maxV;
    }

    scLastWritePos = scWp;
}

//==============================================================================
// paint: draw purely from the pre-computed display buffer
//==============================================================================

void WaveformDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();

    // === Panel background ===
    g.setColour(Theme::bg);
    g.fillRect(bounds);

    // Inset border (dark outer, subtle lighter inner highlight)
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRect(bounds, 1);
    g.setColour(Theme::line.withAlpha(0.55f));
    g.drawRect(bounds.reduced(1), 1);

    // === Inner screen: dark oscilloscope style ===
    const auto screen = bounds.reduced(kFrameW);
    g.setColour(Theme::bgDeep);
    g.fillRect(screen);

    const float waveX   = float(screen.getX() + kLabelW);
    const float waveEnd = float(screen.getRight());
    const int   pixW    = int(waveEnd - waveX);
    const float cy      = float(screen.getY()) + screen.getHeight() * 0.5f;
    const float hh      = screen.getHeight() * 0.44f;
    const float scTop   = float(screen.getY());
    const float scBot   = float(screen.getBottom());
    auto clampY = [&](float y) { return juce::jlimit(scTop, scBot, y); };

    // === dB axis labels + grid lines ===
    struct Mark { float dB; };
    static constexpr Mark kMarks[] = { {0.0f}, {-6.0f}, {-12.0f}, {-20.0f} };

    g.setFont(Theme::mono(14.0f));
    for (const auto& m : kMarks)
    {
        const float amp  = juce::Decibels::decibelsToGain(m.dB);
        const float yPos = clampY(cy - amp * hh);
        const float yNeg = clampY(cy + amp * hh);

        // Grid line
        g.setColour(Theme::text.withAlpha(m.dB == 0.0f ? 0.09f : 0.035f));
        g.drawHorizontalLine(int(yPos), waveX, waveEnd);
        if (m.dB < 0.0f)
            g.drawHorizontalLine(int(yNeg), waveX, waveEnd);

        // Tick
        g.setColour(Theme::textDim);
        g.drawHorizontalLine(int(yPos), waveX - 4.0f, waveX);
        if (m.dB < 0.0f)
            g.drawHorizontalLine(int(yNeg), waveX - 4.0f, waveX);

        // Label — positive side
        if (yPos > scTop + 4.0f && yPos < scBot - 4.0f)
        {
            g.setColour(Theme::textDim.withAlpha(0.95f));
            g.drawText(m.dB == 0.0f ? "0" : juce::String(int(m.dB)),
                       screen.getX(), int(yPos) - 9, kLabelW - 6, 18,
                       juce::Justification::centredRight, false);
        }
        // Label — negative mirror (dimmer)
        if (m.dB < 0.0f && yNeg > scTop + 4.0f && yNeg < scBot - 4.0f)
        {
            g.setColour(Theme::textDim.withAlpha(0.60f));
            g.drawText(juce::String(int(m.dB)),
                       screen.getX(), int(yNeg) - 9, kLabelW - 6, 18,
                       juce::Justification::centredRight, false);
        }
    }

    // Centre line
    g.setColour(Theme::text.withAlpha(0.06f));
    g.drawHorizontalLine(int(cy), waveX, waveEnd);

    // === Waveform from pre-computed display buffer ===
    // Each column is fixed once written — no inter-frame shape variation.
    if (pixW > 0 && displayWidth == pixW && static_cast<int>(dispMin.size()) == pixW)
    {
        // --- Filled envelope polygon (max forward pass, min reverse pass) ---
        {
            juce::Path fill;
            fill.startNewSubPath(waveX, clampY(cy - dispMax[0] * hh));
            for (int px = 1; px < pixW; ++px)
                fill.lineTo(waveX + float(px), clampY(cy - dispMax[px] * hh));
            for (int px = pixW - 1; px >= 0; --px)
                fill.lineTo(waveX + float(px), clampY(cy - dispMin[px] * hh));
            fill.closeSubPath();

            g.setColour(waveColour.withAlpha(0.42f));
            g.fillPath(fill);
        }

        // --- Bright anti-aliased top-edge trace ---
        {
            juce::Path topEdge;
            topEdge.startNewSubPath(waveX, clampY(cy - dispMax[0] * hh));
            for (int px = 1; px < pixW; ++px)
                topEdge.lineTo(waveX + float(px), clampY(cy - dispMax[px] * hh));

            g.setColour(waveColour.brighter(0.55f));
            g.strokePath(topEdge,
                         juce::PathStrokeType(1.5f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }

        // --- Dimmer bottom-edge trace (mirrors the top) ---
        {
            juce::Path botEdge;
            botEdge.startNewSubPath(waveX, clampY(cy - dispMin[0] * hh));
            for (int px = 1; px < pixW; ++px)
                botEdge.lineTo(waveX + float(px), clampY(cy - dispMin[px] * hh));

            g.setColour(waveColour.brighter(0.25f).withAlpha(0.55f));
            g.strokePath(botEdge,
                         juce::PathStrokeType(1.0f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
        }
    }

    // === Sidechain overlay — ducking curve (input display only, when SC enabled) ===
    if (scWaveformBuffer != nullptr
        && scEnabledAtomic != nullptr
        && scEnabledAtomic->load(std::memory_order_relaxed)
        && pixW > 0 && displayWidth == pixW
        && static_cast<int>(scDispEnv.size()) == pixW)
    {
        const float screenH = float(screen.getHeight());

        // Read compression params (fall back to sensible defaults)
        const float thresh    = thresholdParamDb ? thresholdParamDb->load(std::memory_order_relaxed) : 0.0f;
        const float ratio     = ratioParam   ? ratioParam->load(std::memory_order_relaxed)   : 4.0f;
        const float knee      = kneeParam    ? kneeParam->load(std::memory_order_relaxed)    : 0.0f;
        const float attackMs  = attackParam  ? attackParam->load(std::memory_order_relaxed)  : 80.0f;
        const float releaseMs = releaseParam ? releaseParam->load(std::memory_order_relaxed) : 40.0f;

        // Time constants in the pixel domain (~1.3 s of history at 44100 Hz)
        const float samplesPerPixel = float(kSamplesToShow) / float(pixW);
        const float atkCoeff = std::exp(-samplesPerPixel / std::max(1.0f, attackMs  * 0.001f * 44100.0f));
        const float relCoeff = std::exp(-samplesPerPixel / std::max(1.0f, releaseMs * 0.001f * 44100.0f));

        // Build ducking fill curve: apply compression gain reduction + ballistic smoothing
        std::vector<float> duckFill(pixW, 0.0f);
        float grSmooth = 0.0f;
        for (int px = 0; px < pixW; ++px)
        {
            const float scLinear = scDispEnv[px];
            float target = 0.0f;
            if (scLinear > 1e-6f)
            {
                const float inputDb = juce::Decibels::gainToDecibels(scLinear);
                const float diff    = inputDb - thresh;
                float grDb = 0.0f;
                if (knee > 0.0f && diff > -knee * 0.5f && diff < knee * 0.5f)
                {
                    const float kf = (diff + knee * 0.5f) / knee;
                    grDb = (1.0f / ratio - 1.0f) * kf * kf * knee * 0.5f;
                }
                else if (diff > knee * 0.5f)
                {
                    grDb = diff * (1.0f / ratio - 1.0f);
                }
                target = juce::jlimit(0.0f, 1.0f, -grDb / 24.0f);
            }
            // Attack when gain reduction increases, release when it decreases
            grSmooth = (target > grSmooth)
                     ? atkCoeff * grSmooth + (1.0f - atkCoeff) * target
                     : relCoeff * grSmooth + (1.0f - relCoeff) * target;
            duckFill[px] = grSmooth;
        }

        // Filled region from top down to the ducking ceiling
        {
            juce::Path fillPath;
            fillPath.startNewSubPath(waveX, scTop);
            for (int px = 0; px < pixW; ++px)
                fillPath.lineTo(waveX + float(px), clampY(scTop + duckFill[px] * screenH));
            fillPath.lineTo(waveEnd, scTop);
            fillPath.closeSubPath();
            g.setColour(Theme::ice.withAlpha(0.10f));
            g.fillPath(fillPath);
        }

        // Cool ceiling line showing the exact ducking boundary
        {
            juce::Path linePath;
            linePath.startNewSubPath(waveX, clampY(scTop + duckFill[0] * screenH));
            for (int px = 1; px < pixW; ++px)
                linePath.lineTo(waveX + float(px), clampY(scTop + duckFill[px] * screenH));
            g.setColour(Theme::ice.withAlpha(0.90f));
            g.strokePath(linePath, juce::PathStrokeType(1.5f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }
    }

    // === Threshold line (input display only) ===
    if (thresholdParamDb != nullptr)
    {
        const float thDb  = thresholdParamDb->load(std::memory_order_relaxed);
        const float thAmp = juce::Decibels::decibelsToGain(thDb);
        const float yPos  = clampY(cy - thAmp * hh);
        const float yNeg  = clampY(cy + thAmp * hh);

        // Subtle glow
        g.setColour(Theme::warn.withAlpha(0.09f));
        g.fillRect(juce::Rectangle<float>(waveX, yPos - 1.5f, waveEnd - waveX, 3.0f));
        g.fillRect(juce::Rectangle<float>(waveX, yNeg - 1.5f, waveEnd - waveX, 3.0f));

        // Dashed threshold line
        const juce::Colour lineCol = Theme::warn.withAlpha(0.90f);
        g.setColour(lineCol);
        for (float fx = waveX; fx < waveEnd; fx += 13.0f)
        {
            const float seg = std::min(fx + 8.0f, waveEnd);
            g.fillRect(juce::Rectangle<float>(fx, yPos - 0.4f, seg - fx, 0.8f));
            g.fillRect(juce::Rectangle<float>(fx, yNeg - 0.4f, seg - fx, 0.8f));
        }

        // Label chip — dark card
        const juce::String thLabel = juce::String(thDb, 1) + " dB";
        constexpr int lblW = 78, lblH = 20;
        const float   lblX = waveEnd - float(lblW) - 4.0f;
        g.setColour(juce::Colour(0xdd000000));
        g.fillRect(juce::Rectangle<float>(lblX - 1, yPos - float(lblH) - 1,
                                          float(lblW) + 2, float(lblH) + 2));
        g.setColour(Theme::warn.brighter(0.35f));
        g.setFont(Theme::mono(14.0f, juce::Font::bold));
        g.drawText(thLabel,
                   int(lblX), int(yPos) - lblH - 1, lblW, lblH,
                   juce::Justification::centredRight, false);
    }

    // === Title tab === bottom-right corner, no outline, half the footprint
    // and half the opacity of the original top-left badge — a subtle stamp
    // rather than a prominent chip.
    const auto badge = juce::Rectangle<int>(int(waveEnd) - 46 - 4, screen.getBottom() - 11 - 4, 46, 11);
    g.setColour(Theme::bg.withAlpha(0.5f));
    g.fillRect(badge);
    g.setColour(waveColour.withAlpha(0.5f));
    g.setFont(Theme::label(7.5f));
    g.drawText(title, badge, juce::Justification::centred, false);

    // Sidechain state stamp, so screenshots/manuals read unambiguously.
    // Anchored independently of the (now bottom-right, subtle) title badge
    // above, at the top-left corner where the title used to sit.
    if (scEnabledAtomic != nullptr && scEnabledAtomic->load(std::memory_order_relaxed))
    {
        const auto stamp = juce::Rectangle<int>(int(waveX) + 4, screen.getY() + 4, 122, 22);
        g.setColour(Theme::ice.withAlpha(0.55f));
        g.drawRect(stamp, 1);
        g.setColour(Theme::ice);
        g.setFont(Theme::label(14.0f));
        g.drawText("SC DUCKING", stamp, juce::Justification::centred, false);
    }

    // Paused stamp — bottom-left, clear of both the top-left SC stamp and
    // the bottom-right title badge. Click-to-pause is input-only.
    if (paused)
    {
        const auto stamp = juce::Rectangle<int>(int(waveX) + 4, screen.getBottom() - 22 - 4, 78, 22);
        g.setColour(Theme::accent.withAlpha(0.60f));
        g.drawRect(stamp, 1);
        g.setColour(Theme::accent);
        g.setFont(Theme::label(14.0f));
        g.drawText("PAUSED", stamp, juce::Justification::centred, false);
    }
}
