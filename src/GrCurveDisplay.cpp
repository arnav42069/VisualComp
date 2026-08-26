#include "GrCurveDisplay.h"
#include "Theme.h"

GrCurveDisplay::GrCurveDisplay(juce::AudioProcessorValueTreeState& a,
                               std::atomic<float>& lvl)
    : apvts(a), inputLevelDb(lvl)
{
    startTimerHz(30);
}

float GrCurveDisplay::computeOutputDb(float inDb, float thresh,
                                       float ratio, float knee) noexcept
{
    const float hk = knee * 0.5f;
    if (knee > 0.0f && inDb >= thresh - hk && inDb <= thresh + hk)
    {
        const float x = inDb - thresh + hk;
        return inDb + x * x * (1.0f / ratio - 1.0f) / (2.0f * knee);
    }
    if (inDb > thresh + hk)
        return thresh + (inDb - thresh) / ratio;
    return inDb;
}

void GrCurveDisplay::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds();
    const int  bw = bounds.getWidth();
    const int  bh = bounds.getHeight();

    g.setColour(Theme::bg);
    g.fillRect(bounds);

    // === Plot area ===
    constexpr int kLeftM   = 44;
    constexpr int kRightM  = 8;
    constexpr int kTopM    = 8;
    constexpr int kBottomM = 38;

    const int plotX = bounds.getX() + kLeftM;
    const int plotY = bounds.getY() + kTopM;
    const int plotW = bw - kLeftM - kRightM;
    const int plotH = bh - kTopM - kBottomM;
    const int plotR = plotX + plotW;
    const int plotB = plotY + plotH;

    constexpr float kDbMin = -60.0f, kDbMax = 0.0f;
    constexpr float kDbRange = kDbMax - kDbMin;
    constexpr float kZeroDbHeight = 0.70f;

    auto dbToX = [&](float db) { return plotX + plotW * (db - kDbMin) / kDbRange; };
    auto dbToY = [&](float db)
    {
        const float norm = juce::jlimit(0.0f, 1.0f, (db - kDbMin) / kDbRange);
        const float yNorm = norm <= 0.0f ? 1.0f
                          : norm >= 1.0f ? 0.0f
                          : (norm <= 0.5f
                             ? 1.0f - (0.30f * (norm / 0.5f))
                             : 0.70f - (0.70f * ((norm - 0.5f) / 0.5f)));
        return plotY + plotH * yNorm;
    };

    // Recessed screen -- sunk into the chassis, no outline; depth alone
    // separates the plot from its surroundings.
    Theme::drawRecess(g, juce::Rectangle<float>(float(plotX), float(plotY),
                                                float(plotW), float(plotH)), 3.0f);

    // === Grid -- a quiet reference, must never compete with the curve ===
    g.setFont(Theme::mono(9.5f));
    for (float db : { -60.0f, -40.0f, -20.0f, 0.0f })
    {
        const int gx = int(dbToX(db));
        const int gy = int(dbToY(db));

        g.setColour(Theme::hairline.withAlpha(0.6f));
        g.drawVerticalLine(gx, float(plotY), float(plotB));
        g.drawHorizontalLine(gy, float(plotX), float(plotR));

        g.setColour(Theme::textFaint);
        const juce::String lbl = db == 0.0f ? "0" : juce::String(int(db));
        g.drawText(lbl, gx - 20, plotB + 5, 40, 17, juce::Justification::centred, false);
        g.drawText(lbl, bounds.getX(), gy - 9, kLeftM - 6, 18,
                   juce::Justification::centredRight, false);
    }

    // === Parameters ===
    const float thresh    = apvts.getRawParameterValue("threshold")->load();
    const float ratio     = apvts.getRawParameterValue("ratio")->load();
    const float knee      = apvts.getRawParameterValue("knee")->load();
    const float attackMs  = apvts.getRawParameterValue("attack")->load();
    const float releaseMs = apvts.getRawParameterValue("release")->load();

    // === Unity reference ===
    {
        juce::Path unity;
        unity.startNewSubPath(float(dbToX(kDbMin)), float(dbToY(kDbMin)));
        unity.lineTo(float(dbToX(kDbMax)), float(dbToY(kDbMax)));
        g.setColour(Theme::textFaint.withAlpha(0.55f));
        const float dashes[] = { 3.0f, 3.0f };
        juce::Path dashed;
        juce::PathStrokeType(0.9f).createDashedStroke(dashed, unity, dashes, 2);
        g.fillPath(dashed);
    }

    // === Knee region ===
    if (knee > 0.0f)
    {
        const float x0 = dbToX(thresh - knee * 0.5f);
        const float x1 = dbToX(thresh + knee * 0.5f);
        g.setColour(Theme::surfRaised.withAlpha(0.30f));
        g.fillRect(juce::Rectangle<float>(x0, float(plotY), x1 - x0, float(plotH)));
    }

    // === Threshold marker ===
    {
        const float tx = dbToX(thresh);
        g.setColour(Theme::warn.withAlpha(0.75f));
        g.drawVerticalLine(int(tx), float(plotY), float(plotB));
    }

    // === Transfer curve ===
    {
        juce::Path curve;
        bool started = false;
        for (int px = 0; px < plotW; ++px)
        {
            const float inDb  = kDbMin + kDbRange * float(px) / float(plotW - 1);
            const float outDb = computeOutputDb(inDb, thresh, ratio, knee);
            const float cy    = juce::jlimit(float(plotY), float(plotB), dbToY(outDb));
            const float cx    = float(plotX + px);
            if (!started) { curve.startNewSubPath(cx, cy); started = true; }
            else            curve.lineTo(cx, cy);
        }
        g.setColour(Theme::accent.withAlpha(0.16f));
        g.strokePath(curve, juce::PathStrokeType(5.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
        g.setColour(Theme::accent);
        g.strokePath(curve, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    // === Operating point ===
    {
        const float inDb = juce::jlimit(kDbMin, kDbMax,
                                        inputLevelDb.load(std::memory_order_relaxed));
        const float dx = dbToX(inDb);
        const float dy = juce::jlimit(float(plotY), float(plotB), dbToY(inDb));

        g.setColour(Theme::textHi.withAlpha(0.18f));
        g.fillEllipse(dx - 6.0f, dy - 6.0f, 12.0f, 12.0f);
        g.setColour(Theme::textHi);
        g.fillEllipse(dx - 2.6f, dy - 2.6f, 5.2f, 5.2f);
    }

    // === Axis captions ===
    g.setFont(Theme::label(9.5f));
    g.setColour(Theme::textFaint);
    g.drawText("IN dBFS", plotX, plotB + 20, plotW, 17, juce::Justification::centred, false);

    // === Header label -- letterspaced micro-label sitting directly on the
    // recessed screen, no background tab/border (depth already separates the
    // plot from the chassis, so a bordered box on top of it is redundant). ===
    {
        const juce::Rectangle<int> capArea(plotX + 6, plotY + 5, 160, 12);
        g.setColour(Theme::textMid);
        g.setFont(Theme::micro(9.5f));
        Theme::drawTracked(g, "Transfer Curve", capArea, juce::Justification::left);
    }

    // === Timing read-out — sits under the label, left of the rising curve ===
    g.setFont(Theme::mono(9.5f));
    g.setColour(Theme::textFaint);
    g.drawText("ATK " + juce::String(attackMs, 1) + "  REL " + juce::String(releaseMs, 0),
               plotX + 6, plotY + 20, plotW - 12, 16,
               juce::Justification::centredLeft, false);
}
