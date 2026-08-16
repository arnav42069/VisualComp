#include "VuMeter.h"
#include "Theme.h"

constexpr float VuMeter::kMarks[VuMeter::kNumMarks][2];

namespace
{
    // GR severity -> colour, inverted relative to Theme::meterColour (0 GR is
    // idle/safe here, more-negative GR is more active/hot) so the needle arc
    // and tip read as a real level-dependent meter rather than a fixed
    // orange accent regardless of how hard the compressor is working.
    juce::Colour grColour(float grDb)
    {
        const float mag = -grDb;   // 0 .. ~20+, positive = more reduction
        if (mag >= 15.0f) return Theme::meterHot;
        if (mag >= 9.0f)  return Theme::meterMid;
        if (mag >= 3.0f)  return Theme::accent;
        return Theme::meterLow;
    }
}

VuMeter::VuMeter(std::atomic<float>& grDb) : gainReductionDb(grDb)
{
    startTimerHz(60);
}

void VuMeter::timerCallback()
{
    const float gr        = gainReductionDb.load(std::memory_order_relaxed);
    const float grClamped = juce::jlimit(-20.0f, 0.0f, gr);
    const float targetAngle = 35.0f + grClamped * (70.0f / 20.0f);
    const float alpha = targetAngle < needleAngleDeg ? 0.42f : 0.09f;
    needleAngleDeg += (targetAngle - needleAngleDeg) * alpha;
    repaint();
}

void VuMeter::paint(juce::Graphics& g)
{
    const float w = float(getWidth());
    const float h = float(getHeight());

    g.setColour(Theme::bg);
    g.fillRect(0.0f, 0.0f, w, h);

    // === Face panel geometry ===
    const float sidePad    = w * 0.055f;
    const float faceL      = sidePad;
    const float faceR      = w - sidePad;
    const float faceW      = faceR - faceL;
    const float faceT      = h * 0.035f;
    const float labelAreaH = h * 0.17f;
    const float faceB      = h - labelAreaH;
    const float faceH      = faceB - faceT;
    const juce::Rectangle<float> faceRect(faceL, faceT, faceW, faceH);

    // Bezel shadow (soft drop shadow, not an outline)
    g.setColour(juce::Colour(0x55000000));
    g.fillRoundedRectangle(faceRect.expanded(2.5f), 4.0f);

    // Recessed glass face -- sunk into the chassis, no border; depth alone
    // separates it, per the three-surface-depth rule.
    Theme::drawRecess(g, faceRect, 4.0f);

    // === Scale geometry ===
    const float pivX = w * 0.5f;
    const float pivY = faceB - faceH * 0.045f;

    const float maxByWidth  = (faceW * 0.46f) / std::sin(juce::degreesToRadians(35.0f));
    const float maxByHeight = (pivY - faceT - faceH * 0.05f)
                              / std::cos(juce::degreesToRadians(35.0f));
    const float needleLen   = std::min(maxByWidth, maxByHeight) * 0.93f;

    const float tickOuter   = needleLen;
    const float tickInner   = needleLen * 0.875f;
    const float labelRadius = needleLen * 0.735f;

    auto angleToXY = [&](float angleDeg, float radius) -> juce::Point<float>
    {
        const float rad = juce::degreesToRadians(angleDeg);
        return { pivX + radius * std::sin(rad), pivY - radius * std::cos(rad) };
    };

    // === Arc track ===
    {
        juce::Path arcTrack;
        arcTrack.addCentredArc(pivX, pivY, tickOuter, tickOuter, 0.0f,
                               juce::degreesToRadians(-35.0f),
                               juce::degreesToRadians(35.0f), true);
        g.setColour(Theme::textFaint);
        g.strokePath(arcTrack, juce::PathStrokeType(1.2f));
    }

    // === Active arc — tracks the needle, coloured by GR severity so it
    // reads as live signal rather than a static accent decoration ===
    {
        juce::Path live;
        live.addCentredArc(pivX, pivY, tickOuter, tickOuter, 0.0f,
                           juce::degreesToRadians(needleAngleDeg),
                           juce::degreesToRadians(35.0f), true);
        const auto liveCol = grColour(gainReductionDb.load(std::memory_order_relaxed));
        g.setColour(liveCol.withAlpha(0.22f));
        g.strokePath(live, juce::PathStrokeType(6.0f));
        g.setColour(liveCol);
        g.strokePath(live, juce::PathStrokeType(1.8f));
    }

    // === Scale marks ===
    const float fontSize = juce::jlimit(9.0f, 13.0f, needleLen * 0.075f);
    for (int i = 0; i < kNumMarks; ++i)
    {
        const float grVal   = kMarks[i][0];
        const float angDeg  = kMarks[i][1];
        const bool  isMajor = (static_cast<int>(std::abs(grVal)) % 4 == 0);

        const auto inner = angleToXY(angDeg, tickInner);
        const auto outer = angleToXY(angDeg, tickOuter);

        g.setColour(isMajor ? Theme::textMid : Theme::textFaint);
        g.drawLine(inner.x, inner.y, outer.x, outer.y, isMajor ? 1.4f : 0.8f);

        const auto labelPt = angleToXY(angDeg, labelRadius);
        g.setFont(Theme::mono(fontSize, isMajor ? juce::Font::bold : juce::Font::plain));
        g.setColour(isMajor ? Theme::textMid : Theme::textFaint);
        const juce::String lbl = grVal == 0.0f ? "0" : juce::String(int(grVal));
        const float lblW = fontSize * 2.2f;
        const float lblH = fontSize * 1.5f;
        g.drawText(lbl, juce::Rectangle<float>(labelPt.x - lblW * 0.5f,
                                               labelPt.y - lblH * 0.5f, lblW, lblH),
                   juce::Justification::centred, false);
    }

    // Face caption -- letterspaced micro-label sitting directly on the
    // recessed glass, no background box (keeps the needle sweep and pivot
    // clear without drawing another bordered tab on top of the face).
    {
        const juce::Rectangle<int> capArea(int(faceL + 7.0f), int(faceT + 5.0f), 150, 14);
        g.setColour(Theme::textMid);
        g.setFont(Theme::micro(9.5f));
        Theme::drawTracked(g, "Gain Reduction", capArea, juce::Justification::left);
    }

    // === Needle ===
    const auto  tipPt   = angleToXY(needleAngleDeg, needleLen * 0.97f);
    const float needleW = juce::jlimit(1.2f, 2.2f, needleLen * 0.013f);

    const auto needleCol = grColour(gainReductionDb.load(std::memory_order_relaxed));

    g.setColour(juce::Colour(0x40000000));
    g.drawLine(pivX + 1.2f, pivY + 1.2f, tipPt.x + 1.2f, tipPt.y + 1.2f, needleW);
    g.setColour(Theme::text);
    g.drawLine(pivX, pivY, tipPt.x, tipPt.y, needleW);
    const auto litStart = angleToXY(needleAngleDeg, needleLen * 0.80f);
    g.setColour(needleCol.withAlpha(0.35f));
    g.drawLine(litStart.x, litStart.y, tipPt.x, tipPt.y, needleW * 2.6f);
    g.setColour(needleCol);
    g.drawLine(litStart.x, litStart.y, tipPt.x, tipPt.y, needleW);

    // Pivot
    const float pivR = juce::jlimit(3.0f, 8.0f, needleLen * 0.040f);
    g.setColour(juce::Colour(0xff2a2926));
    g.fillEllipse(pivX - pivR, pivY - pivR, pivR * 2.0f, pivR * 2.0f);
    g.setColour(juce::Colour(0xff6e6a61));
    g.fillEllipse(pivX - pivR * 0.5f, pivY - pivR * 0.5f, pivR, pivR);

    // Glass glare
    {
        juce::ColourGradient glare(
            juce::Colours::white.withAlpha(0.055f), faceL, faceT,
            juce::Colours::white.withAlpha(0.000f), faceL, faceT + faceH * 0.45f, false);
        g.setGradientFill(glare);
        g.fillRoundedRectangle(faceRect, 3.0f);
    }

    // === Read-out row below the face ===
    const float lbFont = juce::jlimit(15.0f, 21.0f, w * 0.072f);
    const float grNow  = gainReductionDb.load(std::memory_order_relaxed);

    g.setFont(Theme::mono(lbFont * 1.45f, juce::Font::bold));
    g.setColour(Theme::textHi);
    g.drawText(juce::String(grNow, 1),
               juce::Rectangle<float>(0.0f, faceB + labelAreaH * 0.10f,
                                      w * 0.62f, labelAreaH * 0.52f).toNearestInt(),
               juce::Justification::centredRight, false);
    g.setFont(Theme::label(lbFont * 0.80f));
    g.setColour(Theme::textFaint);
    g.drawText("dB",
               juce::Rectangle<float>(w * 0.64f, faceB + labelAreaH * 0.10f,
                                      w * 0.12f, labelAreaH * 0.52f).toNearestInt(),
               juce::Justification::centredLeft, false);

    // === Activity lamp ===
    {
        const bool  grActive = grNow < -0.3f;
        const float ledR     = juce::jlimit(1.7f, 3.8f, w * 0.012f);
        const float ledCX    = w * 0.845f;
        const float ledCY    = faceB + labelAreaH * 0.30f;

        // Dark socket -- depth alone seats the lamp, no border ring.
        g.setColour(juce::Colour(0xff141412));
        g.fillEllipse(ledCX - ledR - 2.0f, ledCY - ledR - 2.0f,
                      (ledR + 2.0f) * 2.0f, (ledR + 2.0f) * 2.0f);

        if (grActive)
        {
            g.setColour(Theme::warn.withAlpha(0.28f));
            g.fillEllipse(ledCX - ledR * 2.3f, ledCY - ledR * 2.3f, ledR * 4.6f, ledR * 4.6f);
            juce::ColourGradient led(
                Theme::warn.brighter(0.55f), ledCX - ledR * 0.3f, ledCY - ledR * 0.3f,
                Theme::warn.darker(0.45f),   ledCX + ledR * 0.3f, ledCY + ledR * 0.3f, true);
            g.setGradientFill(led);
            g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR * 2.0f, ledR * 2.0f);
            g.setColour(juce::Colours::white.withAlpha(0.45f));
            g.fillEllipse(ledCX - ledR * 0.45f, ledCY - ledR * 0.60f, ledR * 0.7f, ledR * 0.5f);
        }
        else
        {
            g.setColour(Theme::warn.darker(0.85f));
            g.fillEllipse(ledCX - ledR, ledCY - ledR, ledR * 2.0f, ledR * 2.0f);
        }

        g.setFont(Theme::label(13.0f));
        g.setColour(grActive ? Theme::warn.brighter(0.2f) : Theme::textFaint.withAlpha(0.6f));
        g.drawText("ACTIVE", juce::Rectangle<float>(ledCX - 30.0f, ledCY + ledR + 4.0f,
                                                    60.0f, 13.0f),
                   juce::Justification::centred, false);
    }
}
