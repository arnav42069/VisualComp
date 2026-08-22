#include "LevelMeter.h"
#include "Theme.h"

namespace
{
    // Common dBFS reference levels for the peak bar: 0 = digital ceiling,
    // -1 a typical true-peak safety margin, -3/-6/-9/-12 coarse mixing
    // headroom marks, -18 the SMPTE/EBU alignment reference level, -24 the
    // ATSC/broadcast reference, -36 near the meter floor.
    constexpr float kDbNotches[] = { 0.0f, -1.0f, -3.0f, -6.0f, -9.0f, -12.0f, -18.0f, -24.0f, -36.0f };
    // Common integrated-loudness targets platforms normalize to, so a mix
    // can be eyeballed against them live: -6 (loud EDM/club masters), -9
    // (typical streaming-era pop/hip-hop master), -14 (Spotify/YouTube/
    // Apple Music streaming normalization), -16 (Apple Music alt/podcasts),
    // -18 (some broadcast delivery specs), -23 (EBU R128 broadcast).
    constexpr float kLufsNotches[] = { -6.0f, -9.0f, -14.0f, -16.0f, -18.0f, -23.0f };
}

LevelMeter::LevelMeter(VisualCompProcessor& proc) : processor(proc)
{
    peakHistory.fill(-100.0f);
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    // Screenshot-automation hook only (see CLAUDE.md) -- synthetic clicks
    // can't reach the app, so this is the only way to capture the revealed
    // (LUFS-visible) state. Inert unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_METER_REVEAL", {}).isNotEmpty())
    {
        revealed     = true;
        revealAmount = 1.0f;
    }
    startTimerHz(120);   // 120Hz for ultra-smooth meter animation
}

LevelMeter::~LevelMeter() { stopTimer(); }

void LevelMeter::mouseUp(const juce::MouseEvent&)
{
    revealed = !revealed;
}

void LevelMeter::timerCallback()
{
    constexpr float deltaTimeMs = 1000.0f / 120.0f;   // ~8.33ms per frame at 120Hz

    // Update smooth meter interpolation (attack/release ballistics)
    processor.smoothMeterPeak.updateSmoothing(deltaTimeMs);
    processor.smoothMeterRms.updateSmoothing(deltaTimeMs);
    processor.smoothMeterLufs.updateSmoothing(deltaTimeMs);

    // Update smooth meter peak-hold tracking
    processor.smoothMeterPeak.updatePeakHold(deltaTimeMs);

    const float target = revealed ? 1.0f : 0.0f;
    revealAmount += (target - revealAmount) * 0.35f;
    if (std::abs(target - revealAmount) < 0.01f) revealAmount = target;

    // Sample the smooth display value into peak history for rolling 3-second max
    peakHistory[size_t(peakHistoryPos)] = processor.smoothMeterPeak.getDisplayValue();
    peakHistoryPos = (peakHistoryPos + 1) % kPeakHoldFrames;
    // Mark that we've completed at least one full cycle through the buffer,
    // so paint() knows all slots contain valid data and can iterate the entire array
    if (peakHistoryPos == 0)
        peakHistoryWrapped = true;

    repaint();
}

// Recessed, segmented LED-bargraph channel. Each fixed-height segment is
// coloured from Theme::meterColour() at that segment's OWN dB position (the
// classic fixed-ramp hardware convention -- a segment near the top is red
// whether or not it's currently lit), so the column reads as a real bargraph
// rather than a single flat-coloured stick that happens to change height.
void LevelMeter::drawChannel(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb,
                             float floorDb, float ceilDb) const
{
    Theme::drawRecess(g, bar, 2.0f);

    const float range = ceilDb - floorDb;
    constexpr float kSegH = 3.0f, kGap = 1.0f;

    for (float segBottom = bar.getBottom() - 1.5f; segBottom > bar.getY() + 1.0f; segBottom -= (kSegH + kGap))
    {
        const float segTop    = juce::jmax(bar.getY() + 1.0f, segBottom - kSegH);
        const float segCentre = (segTop + segBottom) * 0.5f;
        const float segDb     = floorDb + (bar.getBottom() - segCentre) / bar.getHeight() * range;

        g.setColour(segDb <= valueDb ? Theme::meterColour(segDb)
                                      : Theme::surfRaised.withAlpha(0.35f));
        g.fillRect(juce::Rectangle<float>(bar.getX() + 1.0f, segTop, bar.getWidth() - 2.0f, segBottom - segTop));
    }
}

// Draws `text` rotated -90 degrees (reading bottom-to-top), hugging the
// right edge of `bar` with minimal padding -- a narrow vertical label that
// fits the meter strip's tight width instead of a horizontal one below.
void LevelMeter::drawSideLabel(juce::Graphics& g, juce::Rectangle<float> bar, const juce::String& text)
{
    constexpr float kPad = 2.0f, kLabelW = 11.0f;
    const juce::Rectangle<float> area(bar.getRight() + kPad, bar.getY(), kLabelW, bar.getHeight());

    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                    area.getCentreX(), area.getCentreY()));
    const juce::Rectangle<float> rotated(area.getCentreX() - area.getHeight() * 0.5f,
                                          area.getCentreY() - area.getWidth()  * 0.5f,
                                          area.getHeight(), area.getWidth());
    g.setColour(Theme::textFaint);
    g.setFont(Theme::micro(8.0f));
    Theme::drawTracked(g, text, rotated.toNearestInt(), juce::Justification::centred, 0.8f);
}

// Draws the bar's own current live value, centred directly beneath it --
// distinct from the peak-hold readout above the bars (which shows the
// trailing-3-second max instead of the instantaneous reading).
void LevelMeter::drawBottomValue(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb)
{
    const juce::String text = (valueDb <= -99.5f) ? juce::String("--") : juce::String(valueDb, 1);
    g.setColour(Theme::textMid);
    g.setFont(Theme::mono(7.5f, juce::Font::bold));
    g.drawText(text, bar.getX() - 4.0f, bar.getBottom() + 1.0f, bar.getWidth() + 8.0f, 10.0f,
               juce::Justification::centred, false);
}

// Draws a scale tick for each reference level that falls within the bar's
// range. The 0 dB line is singled out as a brighter, wider reference mark
// with its own numeral -- every other notch is a bare dim tick, relying on
// the side unit label / bottom live value / top peak-hold readout for actual
// numeric precision instead of cramming a number onto every line of a 20px-
// wide column (Design Law: delete redundant encodings).
void LevelMeter::drawNotches(juce::Graphics& g, juce::Rectangle<float> bar,
                             float floorDb, float ceilDb,
                             const float* levels, int numLevels)
{
    for (int i = 0; i < numLevels; ++i)
    {
        const float db = levels[i];
        if (db < floorDb || db > ceilDb) continue;

        const float norm = (db - floorDb) / (ceilDb - floorDb);
        const float y = bar.getBottom() - norm * bar.getHeight();
        const bool  isZero = db > -0.05f && db < 0.05f;

        if (isZero)
        {
            g.setColour(Theme::textHi.withAlpha(0.85f));
            g.drawLine(bar.getX() - 2.0f, y, bar.getRight() + 2.0f, y, 1.2f);
            g.setColour(Theme::textHi);
            g.setFont(Theme::micro(6.5f));
            g.drawText("0", bar.getX() - 1.0f, y - 4.5f, bar.getWidth() + 2.0f, 9.0f,
                       juce::Justification::centred, false);
        }
        else
        {
            g.setColour(Theme::textFaint.withAlpha(0.5f));
            g.drawLine(bar.getX(), y, bar.getRight(), y, 0.6f);
        }
    }
}

void LevelMeter::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float barW = 19.0f;
    constexpr float kLabelPad = 3.0f, kLabelW = 13.0f;
    const float groupW = barW + kLabelPad + kLabelW;   // bar + its side label, as one unit

    // Shared left/right breathing room: kSidePad is the gap from the strip's
    // own edge to the nearest bar+label group, on both sides -- chosen (with
    // kMidGap, the gap between the two groups) so that once LUFS is fully
    // revealed, the dB group's left padding and the LUFS group's right
    // padding come out exactly equal (2*kSidePad + kMidGap + 2*groupW ==
    // this component's width; see kLevelMeterW in PluginEditor.cpp).
    constexpr float kSidePad = 5.0f, kMidGap = 4.0f;

    // With the LUFS bar collapsed, the dB bar+label is the only thing in
    // this segment and should sit centred rather than stranded at the
    // paired-up (left) position it needs once LUFS reveals alongside it.
    const float pairedX  = full.getX() + kSidePad;
    const float centredX = full.getX() + (full.getWidth() - groupW) * 0.5f;
    const float dbX = centredX + (pairedX - centredX) * revealAmount;

    // Top strip reserved for the peak-hold readout, bottom strip for each
    // bar's own live-value readout; the bars fill whatever's left.
    constexpr float kTopPad = 15.0f, kBottomPad = 14.0f;
    const auto dbBar   = juce::Rectangle<float>(dbX, full.getY() + kTopPad,
                                                barW, full.getHeight() - kTopPad - kBottomPad);
    const auto lufsBar = juce::Rectangle<float>(pairedX + groupW + kMidGap, full.getY() + kTopPad,
                                                barW, full.getHeight() - kTopPad - kBottomPad);

    // dB (peak) bar gets a little headroom above 0 dBFS so the 0 dB
    // reference notch sits clearly mid-scale rather than glued to the top
    // edge, and an over-0dB reading visibly pushes into its own zone.
    constexpr float kDbFloor = -48.0f, kDbCeil = 3.0f;
    const float peakDb = processor.smoothMeterPeak.getDisplayValue();
    drawChannel(g, dbBar, peakDb, kDbFloor, kDbCeil);
    drawNotches(g, dbBar, kDbFloor, kDbCeil, kDbNotches, int(sizeof(kDbNotches) / sizeof(kDbNotches[0])));
    drawSideLabel(g, dbBar, "dB");
    drawBottomValue(g, dbBar, peakDb);

    if (revealAmount > 0.01f)
    {
        // The whole LUFS channel fades in as one group rather than threading
        // an alpha parameter through every draw call by hand.
        g.saveState();
        g.beginTransparencyLayer(revealAmount);

        constexpr float kLufsFloor = -36.0f, kLufsCeil = 0.0f;
        const float stLufs = processor.smoothMeterLufs.getDisplayValue();
        drawChannel(g, lufsBar, stLufs, kLufsFloor, kLufsCeil);
        drawNotches(g, lufsBar, kLufsFloor, kLufsCeil, kLufsNotches, int(sizeof(kLufsNotches) / sizeof(kLufsNotches[0])));
        drawSideLabel(g, lufsBar, "LUFS");
        drawBottomValue(g, lufsBar, stLufs);

        g.endTransparencyLayer();
        g.restoreState();
    }

    // Peak-hold: highest dB peak seen in the trailing ~3 seconds (a true
    // rolling max, not an ordinary slowly-decaying peak-hold line), shown as
    // a bright readout above the bars.
    // During the first 3 seconds, peakHistoryWrapped is false and only the
    // range [0, peakHistoryPos) contains valid samples. After the first
    // wrap-around, all 360 slots are valid, so we iterate the full array.
    float peak3s = -100.0f;
    if (peakHistoryWrapped)
    {
        for (float v : peakHistory) peak3s = juce::jmax(peak3s, v);
    }
    else
    {
        for (int i = 0; i < peakHistoryPos; ++i) peak3s = juce::jmax(peak3s, peakHistory[size_t(i)]);
    }
    const juce::String peakText = (peak3s <= -99.5f) ? juce::String("--") : juce::String(peak3s, 1);
    g.setColour(Theme::textHi);
    g.setFont(Theme::mono(9.5f, juce::Font::bold));
    g.drawText(peakText, int(full.getX()), int(full.getY()), int(full.getWidth()) - 13, 11,
               juce::Justification::centred, false);

    // Hint chevron showing there is more to reveal -- top-right, clear of
    // both the peak-hold readout beside it and the per-bar value readouts
    // now occupying the bottom strip.
    g.setColour(Theme::textFaint.withAlpha(0.6f));
    g.setFont(Theme::mono(9.0f));
    g.drawText(revealed ? juce::String::charToString(0x2039) : juce::String::charToString(0x203a),
               int(full.getRight()) - 12, int(full.getY()), 12, 12,
               juce::Justification::centred, false);
}
