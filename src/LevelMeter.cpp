#include "LevelMeter.h"
#include "Theme.h"

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
    startTimerHz(30);
}

LevelMeter::~LevelMeter() { stopTimer(); }

void LevelMeter::mouseUp(const juce::MouseEvent&)
{
    revealed = !revealed;
}

void LevelMeter::timerCallback()
{
    const float target = revealed ? 1.0f : 0.0f;
    revealAmount += (target - revealAmount) * 0.35f;
    if (std::abs(target - revealAmount) < 0.01f) revealAmount = target;

    peakHistory[size_t(peakHistoryPos)] = processor.meterPeakDb.load(std::memory_order_relaxed);
    peakHistoryPos = (peakHistoryPos + 1) % kPeakHoldFrames;

    repaint();
}

juce::Colour LevelMeter::zoneColourForDb(float db, float greenBelow, float yellowBelow)
{
    if (db < greenBelow)  return juce::Colour(0xff5bd67f);
    if (db < yellowBelow) return juce::Colour(0xffe8c34d);
    return Theme::warn;
}

void LevelMeter::drawBar(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb,
                         float floorDb, float ceilDb, float greenBelow, float yellowBelow,
                         float alpha) const
{
    g.setColour(Theme::bgDeep.withAlpha(alpha));
    g.fillRect(bar);
    g.setColour(Theme::line.withAlpha(alpha));
    g.drawRect(bar, 1.0f);

    const float norm = juce::jlimit(0.0f, 1.0f, (valueDb - floorDb) / (ceilDb - floorDb));
    const float fillH = bar.getHeight() * norm;
    const juce::Rectangle<float> fillRect(bar.getX(), bar.getBottom() - fillH, bar.getWidth(), fillH);

    // Segmented colour zones drawn as a gradient so the meter reads as one
    // continuous coloured column rather than hard bands.
    juce::ColourGradient grad(
        Theme::warn.withAlpha(alpha), bar.getX(), bar.getY(),
        juce::Colour(0xff5bd67f).withAlpha(alpha), bar.getX(), bar.getBottom(), false);
    const float yellowNorm = juce::jlimit(0.0f, 1.0f, (yellowBelow - floorDb) / (ceilDb - floorDb));
    const float greenNorm  = juce::jlimit(0.0f, 1.0f, (greenBelow  - floorDb) / (ceilDb - floorDb));
    grad.addColour(juce::jlimit(0.0, 1.0, double(1.0f - yellowNorm)), juce::Colour(0xffe8c34d).withAlpha(alpha));
    grad.addColour(juce::jlimit(0.0, 1.0, double(1.0f - greenNorm)),  juce::Colour(0xff5bd67f).withAlpha(alpha));
    g.setGradientFill(grad);
    g.fillRect(fillRect);

    juce::ignoreUnused(zoneColourForDb);
}

// Draws `text` rotated -90 degrees (reading bottom-to-top), hugging the
// right edge of `bar` with minimal padding -- a narrow vertical label that
// fits the meter strip's tight width instead of a horizontal one below.
void LevelMeter::drawSideLabel(juce::Graphics& g, juce::Rectangle<float> bar, const juce::String& text, float alpha)
{
    constexpr float kPad = 2.0f, kLabelW = 11.0f;
    const juce::Rectangle<float> area(bar.getRight() + kPad, bar.getY(), kLabelW, bar.getHeight());

    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::rotation(-juce::MathConstants<float>::halfPi,
                                                    area.getCentreX(), area.getCentreY()));
    const juce::Rectangle<float> rotated(area.getCentreX() - area.getHeight() * 0.5f,
                                          area.getCentreY() - area.getWidth()  * 0.5f,
                                          area.getHeight(), area.getWidth());
    g.setColour(juce::Colours::white.withAlpha(alpha));
    g.setFont(Theme::mono(8.5f, juce::Font::bold));
    g.drawText(text, rotated, juce::Justification::centred, false);
}

// Draws the bar's own current live value, centred directly beneath it --
// distinct from the peak-hold readout above the bars (which shows the
// trailing-3-second max instead of the instantaneous reading).
void LevelMeter::drawBottomValue(juce::Graphics& g, juce::Rectangle<float> bar, float valueDb, float alpha)
{
    const juce::String text = (valueDb <= -99.5f) ? juce::String("--") : juce::String(valueDb, 1);
    g.setColour(juce::Colours::white.withAlpha(alpha));
    g.setFont(Theme::mono(7.5f, juce::Font::bold));
    g.drawText(text, bar.getX() - 4.0f, bar.getBottom() + 1.0f, bar.getWidth() + 8.0f, 10.0f,
               juce::Justification::centred, false);
}

void LevelMeter::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float barW = 20.0f;
    constexpr float kLabelPad = 2.0f, kLabelW = 11.0f;
    const float groupW = barW + kLabelPad + kLabelW;   // bar + its side label, as one unit

    // With the LUFS bar collapsed, the dB bar+label is the only thing in
    // this segment and should sit centred rather than stranded at the
    // paired-up (left) position it needs once LUFS reveals alongside it.
    const float pairedX  = full.getX() + 2.0f;
    const float centredX = full.getX() + (full.getWidth() - groupW) * 0.5f;
    const float dbX = centredX + (pairedX - centredX) * revealAmount;

    // Top strip reserved for the peak-hold readout, bottom strip for each
    // bar's own live-value readout; the bars fill whatever's left.
    constexpr float kTopPad = 12.0f, kBottomPad = 11.0f;
    const auto dbBar   = juce::Rectangle<float>(dbX, full.getY() + kTopPad,
                                                barW, full.getHeight() - kTopPad - kBottomPad);
    const auto lufsBar = juce::Rectangle<float>(full.getX() + 2.0f + groupW + 3.0f, full.getY() + kTopPad,
                                                barW, full.getHeight() - kTopPad - kBottomPad);

    const float peakDb = processor.meterPeakDb.load(std::memory_order_relaxed);
    drawBar(g, dbBar, peakDb, -48.0f, 0.0f, -18.0f, -6.0f, 1.0f);
    drawSideLabel(g, dbBar, "dB", 1.0f);
    drawBottomValue(g, dbBar, peakDb, 1.0f);

    if (revealAmount > 0.01f)
    {
        const float stLufs = processor.meterShortLufs.load(std::memory_order_relaxed);
        drawBar(g, lufsBar, stLufs, -36.0f, 0.0f, -18.0f, -10.0f, revealAmount);
        drawSideLabel(g, lufsBar, "LUFS", revealAmount);
        drawBottomValue(g, lufsBar, stLufs, revealAmount);
    }

    // Peak-hold: highest dB peak seen in the trailing ~3 seconds (a true
    // rolling max, not an ordinary slowly-decaying peak-hold line), shown as
    // a bright readout above the bars.
    float peak3s = -100.0f;
    for (float v : peakHistory) peak3s = juce::jmax(peak3s, v);
    const juce::String peakText = (peak3s <= -99.5f) ? juce::String("--") : juce::String(peak3s, 1);
    g.setColour(juce::Colours::white);
    g.setFont(Theme::mono(9.5f, juce::Font::bold));
    g.drawText(peakText, int(full.getX()), int(full.getY()), int(full.getWidth()) - 13, 11,
               juce::Justification::centred, false);

    // Hint chevron showing there is more to reveal -- top-right, clear of
    // both the peak-hold readout beside it and the per-bar value readouts
    // now occupying the bottom strip.
    g.setColour(Theme::textDim.withAlpha(0.6f));
    g.setFont(Theme::mono(9.0f));
    g.drawText(revealed ? juce::String::charToString(0x2039) : juce::String::charToString(0x203a),
               int(full.getRight()) - 12, int(full.getY()), 12, 12,
               juce::Justification::centred, false);
}
