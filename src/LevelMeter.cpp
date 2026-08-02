#include "LevelMeter.h"
#include "Theme.h"

LevelMeter::LevelMeter(VisualCompProcessor& proc) : processor(proc)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
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
                         const juce::String& label, float alpha) const
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

    g.setColour(Theme::text.withAlpha(0.85f * alpha));
    g.setFont(Theme::mono(8.0f));
    g.drawText(label, bar.withY(bar.getBottom() + 2.0f).withHeight(12.0f),
               juce::Justification::centred, false);
}

void LevelMeter::paint(juce::Graphics& g)
{
    const auto full = getLocalBounds().toFloat();
    const float barW = 20.0f;

    // With the LUFS bar collapsed, the dB bar is the only thing in this
    // segment and should sit centred rather than stranded at the paired-up
    // (left) position it needs once LUFS reveals alongside it.
    const float pairedX   = full.getX() + 2.0f;
    const float centredX  = full.getX() + (full.getWidth() - barW) * 0.5f;
    const float dbX = centredX + (pairedX - centredX) * revealAmount;

    const auto dbBar   = juce::Rectangle<float>(dbX, full.getY() + 2.0f,
                                                barW, full.getHeight() - 20.0f);
    const auto lufsBar = juce::Rectangle<float>(full.getX() + 2.0f + barW + 6.0f, full.getY() + 2.0f,
                                                barW, full.getHeight() - 20.0f);

    const float peakDb = processor.meterPeakDb.load(std::memory_order_relaxed);
    drawBar(g, dbBar, peakDb, -48.0f, 0.0f, -18.0f, -6.0f, "dB", 1.0f);

    if (revealAmount > 0.01f)
    {
        const float stLufs = processor.meterShortLufs.load(std::memory_order_relaxed);
        drawBar(g, lufsBar, stLufs, -36.0f, 0.0f, -18.0f, -10.0f, "LUFS", revealAmount);
    }

    // Hint chevron showing there is more to reveal
    g.setColour(Theme::textDim.withAlpha(0.6f));
    g.setFont(Theme::mono(9.0f));
    g.drawText(revealed ? juce::String::charToString(0x2039) : juce::String::charToString(0x203a),
               int(full.getRight()) - 12, int(full.getY()), 12, 12,
               juce::Justification::centred, false);
}
