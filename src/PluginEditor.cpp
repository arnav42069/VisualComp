#include "PluginEditor.h"
#include "Presets.h"
#include "Theme.h"
#include "LogoSvg.h"

namespace
{
    constexpr int kWidth  = 960;
    constexpr int kHeight = 648;       // +28 vs. the previous build, for the
                                       // multiband-indicator strip in the Dynamics pane

    constexpr int kTitleH = 60;
    constexpr int kStripH = 48;
    constexpr int kHeadH  = kTitleH + kStripH;        // 108

    constexpr int kRightColW   = 280;
    constexpr int kLevelMeterW = 56;                  // reserved for the dB/LUFS meter strip
    constexpr int kContentW    = kWidth - kRightColW; // 680

    // Curve/GR (transfer curve + gain-reduction meter) column width when
    // expanded. kWidth/kRightColW/kContentW above describe that expanded
    // layout unchanged; when collapsed (the default — see curveGrVisible),
    // this width is subtracted from the total window in VisualCompEditor's
    // constructor/resized()/toggleCurveGrPanel(), the same setSize()-delta
    // mechanism the docked EQ panel already uses via kEqPanelW below.
    constexpr int kCurveGrColW = kRightColW - kLevelMeterW - 6;   // 218

    // Smart Master+ trimmed to ~half its original ~43px total side padding
    // around the button text (was 130), and a matching 5px is added past
    // Curve/GR's right edge when collapsed (see totalEditorWidth()) so it
    // isn't flush against the window edge either.
    constexpr int kSmartMasterW = 109;
    constexpr int kCurveGrRightPad = 5;

    constexpr int kWaveY = kHeadH + 4;                // 112
    constexpr int kWaveH = 232;

    constexpr int kCtrlY = kWaveY + kWaveH + 6;       // 350
    constexpr int kCtrlH = kHeight - kCtrlY;          // 298

    constexpr int kCtrlTopStripH = 30;   // band-selector button row, top of the Dynamics pane

    constexpr int kVUH    = 285;
    constexpr int kCurveY = kHeadH + kVUH;            // 393
    constexpr int kCurveH = kHeight - kCurveY;        // 255 (+50% vs. previous 170)

    constexpr int kFaderW = 80;
    constexpr int kFaderM = 5;

    constexpr int kKnobAreaX = kFaderM + kFaderW + kFaderM;                            // 90
    constexpr int kKnobAreaW = kContentW - kKnobAreaX - kFaderM - kFaderW - kFaderM;   // 500

    // Knob-column slots — Threshold/Knee/Ratio/Attack/Release are all
    // always full-width now (Q used to share this row and narrow
    // Attack/Release to make room for it; Q moved to node-level editing —
    // see EqPanel's mouse wheel / right-click submenu and NodeIsland's
    // Dynamic Island knob).
    constexpr int kSlotThresholdW = 100, kSlotKneeW = 100, kSlotRatioW = 100;
    constexpr int kSlotAttackW = 100, kSlotReleaseW = 100;
    constexpr int kSlotThresholdX = kKnobAreaX;
    constexpr int kSlotKneeX      = kSlotThresholdX + kSlotThresholdW;
    constexpr int kSlotRatioX     = kSlotKneeX + kSlotKneeW;
    constexpr int kSlotAttackX    = kSlotRatioX + kSlotRatioW;
    constexpr int kSlotReleaseX   = kSlotAttackX + kSlotAttackW;

    constexpr int kMixSz    = 48;
    constexpr int kKnobRowY = kCtrlTopStripH + 8;    // label row, pushed below the new strip
    constexpr int kKnobLblH = 20;

    constexpr int kEqPanelW = 520;   // docked EQ panel width when open (2x its base 260)

    // Genres offered by the Auto-Analyze wizard, with a sensible default LUFS
    // target for each (short-term / integrated streaming-style targets).
    struct GenreInfo { const char* name; float defaultLufs; };
    static const GenreInfo kGenres[] =
    {
        { "Pop",                  -9.0f },
        { "Hip-Hop / Trap",       -7.0f },
        { "EDM / Dance",          -6.0f },
        { "Rock / Metal",         -8.0f },
        { "Acoustic / Folk",     -14.0f },
        { "Classical / Orchestral", -18.0f },
        { "Podcast / Spoken Word", -16.0f },
    };
    constexpr int kNumGenres = int(sizeof(kGenres) / sizeof(kGenres[0]));
}

//==============================================================================
// HelpOverlay
//==============================================================================

void HelpOverlay::OverlayLookAndFeel::drawButtonBackground(juce::Graphics& g,
                                                           juce::Button& b,
                                                           const juce::Colour& colour,
                                                           bool isHighlighted, bool isDown)
{
    auto r = b.getLocalBounds().toFloat();
    auto c = colour;
    if (isDown)            c = c.darker(0.25f);
    else if (isHighlighted) c = c.brighter(0.12f);

    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillRoundedRectangle(r.translated(0.0f, 1.5f), 3.0f);
    g.setColour(c);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.drawRoundedRectangle(r, 3.0f, 1.0f);
}

juce::Font HelpOverlay::OverlayLookAndFeel::getTextButtonFont(juce::TextButton&, int height)
{
    return Theme::label(juce::jmin(18.0f, float(height) * 0.56f));
}

HelpOverlay::HelpOverlay()
{
    setLookAndFeel(&overlayLaf);
    setAlwaysOnTop(true);

    nextButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    nextButton.setColour(juce::TextButton::textColourOffId, juce::Colours::black);
    nextButton.onClick = [this] { showStep(current + 1); };
    addAndMakeVisible(nextButton);

    skipButton.setColour(juce::TextButton::buttonColourId, Theme::charcoal);
    skipButton.setColour(juce::TextButton::textColourOffId, Theme::text);
    skipButton.onClick = [this] { finish(); };
    addAndMakeVisible(skipButton);

    setVisible(false);
}

HelpOverlay::~HelpOverlay()
{
    setLookAndFeel(nullptr);
}

void HelpOverlay::startAt(int stepIndex)
{
    if (steps.empty()) return;
    setVisible(true);
    toFront(true);
    showStep(juce::jlimit(0, int(steps.size()) - 1, stepIndex));
}

void HelpOverlay::showStep(int index)
{
    if (index >= int(steps.size())) { finish(); return; }
    current = index;
    nextButton.setButtonText(current == int(steps.size()) - 1 ? "DONE" : "NEXT");
    skipButton.setVisible(current < int(steps.size()) - 1);
    resized();
    repaint();
}

void HelpOverlay::finish()
{
    setVisible(false);
    if (onFinished) onFinished();
}

void HelpOverlay::mouseUp(const juce::MouseEvent&)
{
    // Clicking anywhere on the dimmed area advances the tour.
    showStep(current + 1);
}

juce::Rectangle<int> HelpOverlay::computeBubble() const
{
    constexpr int bw = 404, pad = 14;
    if (steps.empty()) return getLocalBounds().withSizeKeepingCentre(bw, 130);

    // Height follows the body text so nothing ever runs under the buttons
    juce::GlyphArrangement ga;
    ga.addJustifiedText(Theme::label(15.0f, juce::Font::plain),
                        steps[size_t(current)].body,
                        0.0f, 0.0f, float(bw - 32), juce::Justification::topLeft);
    const int textH = int(std::ceil(ga.getBoundingBox(0, -1, true).getHeight()));
    const int bh = juce::jmax(122, 14 + 22 + 6 + textH + 14 + 26 + 14);

    const auto target = steps[size_t(current)].target;
    if (target.isEmpty())
        return getLocalBounds().withSizeKeepingCentre(bw, bh);

    int x = target.getCentreX() - bw / 2;
    int y = target.getBottom() + pad;
    if (y + bh > getHeight() - 8)          // no room below — place above
        y = target.getY() - pad - bh;
    y = juce::jlimit(8, juce::jmax(8, getHeight() - bh - 8), y);
    x = juce::jlimit(8, juce::jmax(8, getWidth() - bw - 8), x);
    return { x, y, bw, bh };
}

void HelpOverlay::resized()
{
    const auto b = computeBubble();
    constexpr int btnW = 74, btnH = 26;
    nextButton.setBounds(b.getRight() - 14 - btnW, b.getBottom() - 14 - btnH, btnW, btnH);
    skipButton.setBounds(b.getRight() - 14 - btnW * 2 - 8, b.getBottom() - 14 - btnH, btnW, btnH);
}

void HelpOverlay::paint(juce::Graphics& g)
{
    if (steps.empty()) return;
    const auto& step = steps[size_t(current)];

    // Dim everything, punching a hole around the spotlighted control
    juce::Path dim;
    dim.addRectangle(getLocalBounds());
    if (!step.target.isEmpty())
    {
        dim.setUsingNonZeroWinding(false);
        dim.addRoundedRectangle(step.target.expanded(4).toFloat(), 3.0f);
    }
    g.setColour(juce::Colours::black.withAlpha(0.76f));
    g.fillPath(dim);

    if (!step.target.isEmpty())
    {
        g.setColour(Theme::accent.withAlpha(0.30f));
        g.drawRoundedRectangle(step.target.expanded(6).toFloat(), 4.0f, 4.0f);
        g.setColour(Theme::accent);
        g.drawRoundedRectangle(step.target.expanded(4).toFloat(), 3.0f, 1.6f);
    }

    // Bubble
    const auto b = computeBubble();
    g.setColour(juce::Colours::black.withAlpha(0.55f));
    g.fillRoundedRectangle(b.toFloat().translated(0.0f, 3.0f), 5.0f);
    g.setColour(Theme::bg);
    g.fillRoundedRectangle(b.toFloat(), 5.0f);
    g.setColour(Theme::accent);
    g.drawRoundedRectangle(b.toFloat(), 5.0f, 1.4f);
    g.fillRect(b.getX() + 1, b.getY() + 1, 4, b.getHeight() - 2);

    auto inner = b.reduced(16, 14);

    // Title row, with the step counter on the right
    auto titleRow = inner.removeFromTop(22);
    g.setColour(Theme::accent);
    g.setFont(Theme::mono(12.0f, juce::Font::bold));
    g.drawText(juce::String(current + 1) + " / " + juce::String(int(steps.size())),
               titleRow, juce::Justification::topRight, false);
    g.setColour(Theme::text);
    g.setFont(Theme::label(16.0f));
    g.drawText(step.title, titleRow.withTrimmedRight(46),
               juce::Justification::topLeft, false);

    inner.removeFromTop(6);
    inner.removeFromBottom(26 + 14);      // buttons + gap
    g.setColour(Theme::text.withAlpha(0.88f));
    g.setFont(Theme::label(12.5f, juce::Font::plain));
    g.drawFittedText(step.body, inner, juce::Justification::topLeft, 8, 1.0f);
}

//==============================================================================
// AzazelLookAndFeel
//==============================================================================

AzazelLookAndFeel::AzazelLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId,       Theme::accent);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId,  Theme::accentDim);
    setColour(juce::Label::textColourId,               Theme::textDim);
    setColour(juce::PopupMenu::backgroundColourId,     Theme::bgDeep);
    setColour(juce::PopupMenu::textColourId,           Theme::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, Theme::accentDim);
    setColour(juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);
    setColour(juce::PopupMenu::headerTextColourId,            Theme::accent);
}

void AzazelLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                         int x, int y, int width, int height,
                                         float sliderPos,
                                         float startAngle, float endAngle,
                                         juce::Slider& slider)
{
    // Knob components span their whole parameter box so the user can drag
    // anywhere inside it; `topInset` keeps the dial clear of the label row.
    const int inset = int(slider.getProperties().getWithDefault("topInset", 0));
    y      += inset;
    height -= inset;

    const float cx   = x + width  * 0.5f;
    const float cy   = y + height * 0.5f;
    const float maxR = juce::jmin(width, height) * 0.5f * 0.93f;
    if (maxR <= 4.0f) return;

    const float valueAngle = startAngle + (endAngle - startAngle) * sliderPos;
    const bool  simpleTicks = slider.getProperties().contains("simpleTicks");

    // Ambient seat shadow
    {
        juce::ColourGradient s(juce::Colour(0x50000000), cx, cy,
                               juce::Colour(0x00000000), cx + maxR * 1.14f, cy, true);
        g.setGradientFill(s);
        g.fillEllipse(cx - maxR * 1.14f, cy - maxR * 1.14f, maxR * 2.28f, maxR * 2.28f);
    }

    // Graduation ticks
    {
        constexpr int N = 22;
        for (int i = 0; i <= N; ++i)
        {
            const float frac  = float(i) / float(N);
            const float angle = startAngle + frac * (endAngle - startAngle);
            const bool  isMaj = (i % 4 == 0) || (i == N);
            if (simpleTicks && !isMaj) continue;
            const float inner = isMaj ? maxR * 0.78f : maxR * 0.85f;
            const float sa = std::sin(angle), ca = -std::cos(angle);
            g.setColour(isMaj ? Theme::text.withAlpha(0.75f)
                              : Theme::textDim.withAlpha(0.40f));
            g.drawLine(cx + inner * sa, cy + inner * ca,
                       cx + maxR * sa,  cy + maxR * ca,
                       isMaj ? 1.5f : 0.7f);
        }
    }

    // Value arc
    {
        const float arcR = maxR * 0.90f;
        juce::Path track, active;
        track.addCentredArc(cx, cy, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.setColour(Theme::accentDeep);
        g.strokePath(track, juce::PathStrokeType(2.2f));

        // Bipolar knobs (e.g. Range) fill from their centre value outward to
        // whichever side they're currently on, never from the dial's start —
        // a plain start-to-value fill would misleadingly always show "some"
        // glow even sitting at the neutral centre value.
        const bool centerFill = slider.getProperties().contains("centerFill");
        const float zeroAngle = centerFill
            ? startAngle + (endAngle - startAngle) * float(slider.valueToProportionOfLength(0.0))
            : startAngle;

        if (centerFill ? (std::abs(valueAngle - zeroAngle) > 0.001f)
                       : (valueAngle > startAngle + 0.001f))
        {
            const float a0 = juce::jmin(zeroAngle, valueAngle);
            const float a1 = juce::jmax(zeroAngle, valueAngle);
            active.addCentredArc(cx, cy, arcR, arcR, 0.0f, a0, a1, true);
            g.setColour(Theme::accent.withAlpha(0.28f));
            g.strokePath(active, juce::PathStrokeType(5.0f));
            g.setColour(Theme::accent);
            g.strokePath(active, juce::PathStrokeType(2.2f));
        }
    }

    // Machined bezel
    const float bezelOuter = maxR * 0.72f;
    const float bezelInner = maxR * 0.60f;
    {
        juce::ColourGradient ch(
            juce::Colour(0xff54514c), cx - bezelOuter * 0.40f, cy - bezelOuter * 0.52f,
            juce::Colour(0xff121110), cx + bezelOuter * 0.40f, cy + bezelOuter * 0.52f, true);
        ch.addColour(0.22, juce::Colour(0xff6b675f));
        ch.addColour(0.45, juce::Colour(0xff232220));
        ch.addColour(0.70, juce::Colour(0xff43403b));
        g.setGradientFill(ch);
        g.fillEllipse(cx - bezelOuter, cy - bezelOuter, bezelOuter * 2.0f, bezelOuter * 2.0f);

        g.setColour(juce::Colour(0xff8b857a).withAlpha(0.40f));
        g.drawEllipse(cx - bezelOuter + 0.7f, cy - bezelOuter + 0.7f,
                      (bezelOuter - 0.7f) * 2.0f, (bezelOuter - 0.7f) * 2.0f, 0.7f);
        g.setColour(juce::Colour(0xff000000).withAlpha(0.75f));
        g.drawEllipse(cx - bezelOuter, cy - bezelOuter, bezelOuter * 2.0f, bezelOuter * 2.0f, 0.9f);

        g.setColour(juce::Colour(0xff080807));
        g.fillEllipse(cx - bezelInner, cy - bezelInner, bezelInner * 2.0f, bezelInner * 2.0f);
    }

    // Body
    const float bodyR = maxR * 0.565f;
    {
        for (int i = 4; i >= 1; --i)
        {
            const float sz = bodyR + float(i) * 1.3f;
            g.setColour(juce::Colour(0xff000000).withAlpha(0.14f * float(5 - i) / 4.0f));
            g.fillEllipse(cx - sz + 1.1f, cy - sz + 1.6f, sz * 2.0f, sz * 2.0f);
        }

        juce::ColourGradient bd(
            juce::Colour(0xff34322e), cx - bodyR * 0.32f, cy - bodyR * 0.42f,
            juce::Colour(0xff060605), cx + bodyR * 0.32f, cy + bodyR * 0.48f, true);
        bd.addColour(0.38, juce::Colour(0xff171614));
        bd.addColour(0.66, juce::Colour(0xff0c0b0a));
        g.setGradientFill(bd);
        g.fillEllipse(cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);

        for (int i = 1; i <= 6; ++i)
        {
            const float rr = bodyR * (0.14f + float(i) * 0.14f);
            g.setColour(juce::Colour(0xff000000).withAlpha(0.06f + float(i) * 0.008f));
            g.drawEllipse(cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 0.5f);
            g.setColour(Theme::text.withAlpha(0.015f));
            g.drawEllipse(cx - rr + 0.4f, cy - rr + 0.4f,
                          rr * 2.0f - 0.8f, rr * 2.0f - 0.8f, 0.35f);
        }

        g.setColour(juce::Colour(0xff000000).withAlpha(0.85f));
        g.drawEllipse(cx - bodyR + 0.3f, cy - bodyR + 0.3f,
                      (bodyR - 0.3f) * 2.0f, (bodyR - 0.3f) * 2.0f, 1.0f);
        g.setColour(juce::Colour(0xff4a463f).withAlpha(0.50f));
        g.drawEllipse(cx - bodyR + 1.0f, cy - bodyR + 1.0f,
                      (bodyR - 1.0f) * 2.0f, (bodyR - 1.0f) * 2.0f, 0.7f);
    }

    // Glare
    {
        const float gx = cx - bodyR * 0.30f;
        const float gy = cy - bodyR * 0.30f;
        juce::ColourGradient gl(
            juce::Colours::white.withAlpha(0.075f), gx, gy,
            juce::Colours::white.withAlpha(0.000f), gx + bodyR * 0.55f, gy + bodyR * 0.55f, true);
        g.setGradientFill(gl);
        g.fillEllipse(gx - bodyR * 0.44f, gy - bodyR * 0.34f, bodyR * 0.88f, bodyR * 0.68f);
    }

    // Pointer
    {
        const float sa = std::sin(valueAngle), ca = -std::cos(valueAngle);
        const float r0 = bodyR * 0.30f, r1 = bodyR * 0.86f;

        g.setColour(juce::Colour(0x70000000));
        g.drawLine(cx + r0 * sa + 0.8f, cy + r0 * ca + 1.0f,
                   cx + r1 * sa + 0.8f, cy + r1 * ca + 1.0f, 2.4f);
        g.setColour(Theme::accent.withAlpha(0.30f));
        g.drawLine(cx + r0 * sa, cy + r0 * ca, cx + r1 * sa, cy + r1 * ca, 4.2f);
        g.setColour(juce::Colours::white);
        g.drawLine(cx + r0 * sa, cy + r0 * ca, cx + r1 * sa, cy + r1 * ca, 1.9f);

        const float dotR = juce::jlimit(1.8f, 3.4f, bodyR * 0.09f);
        const float dx = cx + bodyR * 0.86f * sa;
        const float dy = cy + bodyR * 0.86f * ca;
        g.setColour(Theme::accent.withAlpha(0.35f));
        g.fillEllipse(dx - dotR * 2.0f, dy - dotR * 2.0f, dotR * 4.0f, dotR * 4.0f);
        g.setColour(juce::Colours::white);
        g.fillEllipse(dx - dotR, dy - dotR, dotR * 2.0f, dotR * 2.0f);
    }
}

void AzazelLookAndFeel::drawLinearSlider(juce::Graphics& g,
                                        int x, int y, int width, int height,
                                        float sliderPos,
                                        float, float,
                                        juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, 0, 0, style, slider);
        return;
    }

    const float trackW = 3.0f;
    const float trackX = x + width * 0.5f - trackW * 0.5f;
    const float trackT = float(y) + 8.0f;
    const float trackB = float(y + height) - 8.0f;
    const float trackH = trackB - trackT;

    g.setColour(juce::Colour(0xff0a0a09));
    g.fillRoundedRectangle(trackX, trackT, trackW, trackH, trackW * 0.5f);
    g.setColour(Theme::text.withAlpha(0.06f));
    g.fillRoundedRectangle(trackX, trackT, trackW * 0.45f, trackH, trackW * 0.5f);

    const auto  range    = slider.getNormalisableRange();
    const float rangeMin = float(range.start), rangeMax = float(range.end);

    // Accent fill from unity to the handle
    {
        const float norm0 = juce::jlimit(0.0f, 1.0f, (0.0f - rangeMin) / (rangeMax - rangeMin));
        const float zeroY = trackT + trackH * (1.0f - norm0);
        const float top   = juce::jmin(zeroY, sliderPos);
        const float bot   = juce::jmax(zeroY, sliderPos);
        if (bot - top > 0.5f)
        {
            g.setColour(Theme::accent.withAlpha(0.22f));
            g.fillRect(trackX - 1.2f, top, trackW + 2.4f, bot - top);
            g.setColour(Theme::accent);
            g.fillRect(trackX, top, trackW, bot - top);
        }
        g.setColour(Theme::text.withAlpha(0.45f));
        g.fillRect(trackX - 3.5f, zeroY - 0.5f, trackW + 7.0f, 1.0f);
    }

    // Scale ticks
    for (float db : { -24.0f, -18.0f, -12.0f, -6.0f, 0.0f, 6.0f, 12.0f, 18.0f, 24.0f })
    {
        if (db < rangeMin || db > rangeMax) continue;
        const float norm  = (db - rangeMin) / (rangeMax - rangeMin);
        const float ty    = trackT + trackH * (1.0f - norm);
        const bool  major = (static_cast<int>(db) % 12 == 0) || db == 0.0f;
        g.setColour(Theme::textDim.withAlpha(major ? 0.75f : 0.35f));
        g.fillRect(trackX + trackW + 2.5f, ty - 0.5f, major ? 5.5f : 3.0f, 1.0f);
    }

    // Cap — slim, low-profile
    const float capH = 8.0f;
    const float capW = (float(width) - 16.0f) * 0.52f;
    const float capX = float(x) + (float(width) - capW) * 0.5f;
    const float capY = juce::jlimit(trackT - capH * 0.5f, trackB - capH * 0.5f,
                                    sliderPos - capH * 0.5f);
    const float capR = 1.6f;

    for (int pass = 3; pass >= 1; --pass)
    {
        g.setColour(juce::Colour(0xff000000).withAlpha(0.05f * float(pass)));
        g.fillRoundedRectangle(capX, capY + float(pass) * 0.8f, capW, capH, capR);
    }
    juce::ColourGradient cg(juce::Colour(0xff3d3b36), capX, capY,
                            juce::Colour(0xff151413), capX, capY + capH, false);
    g.setGradientFill(cg);
    g.fillRoundedRectangle(capX, capY, capW, capH, capR);

    // Lit centre line
    g.setColour(Theme::accent);
    g.fillRect(capX + 3.0f, capY + capH * 0.5f - 0.9f, capW - 6.0f, 1.8f);

    g.setColour(juce::Colour(0xff000000).withAlpha(0.85f));
    g.drawRoundedRectangle(capX, capY, capW, capH, capR, 0.9f);
}

void AzazelLookAndFeel::drawToggleButton(juce::Graphics& g,
                                        juce::ToggleButton& button,
                                        bool isHighlighted, bool isDown)
{
    const auto  bounds = button.getLocalBounds().toFloat();
    const auto  rect   = bounds.reduced(2.0f);
    const float rad    = 2.5f;
    const bool  on     = button.getToggleState();
    const juce::String txt = button.getButtonText();

    const bool isBypass   = txt.startsWithIgnoreCase("BYP");
    const bool isAutoGain = txt.startsWithIgnoreCase("AUTO");
    const juce::Colour lit = isBypass ? Theme::warn : Theme::accent;

    for (int i = 3; i >= 1; --i)
    {
        g.setColour(juce::Colour(0xff000000).withAlpha(0.11f * float(4 - i)));
        g.fillRoundedRectangle(rect.translated(0.0f, float(i) * 0.8f), rad);
    }

    if (on)
    {
        g.setColour(lit.withAlpha(0.20f));
        g.fillRoundedRectangle(rect.expanded(2.5f), rad + 2.0f);
        juce::ColourGradient bd(lit.brighter(0.18f), rect.getX(), rect.getY(),
                                lit.darker(0.32f),   rect.getX(), rect.getBottom(), false);
        g.setGradientFill(bd);
        g.fillRoundedRectangle(rect, rad);
    }
    else
    {
        juce::ColourGradient bd(juce::Colour(0xff33312d), rect.getX(), rect.getY(),
                                juce::Colour(0xff161514), rect.getX(), rect.getBottom(), false);
        g.setGradientFill(bd);
        g.fillRoundedRectangle(rect, rad);
    }

    if (isHighlighted || isDown)
    {
        g.setColour(juce::Colours::white.withAlpha(isDown ? 0.03f : 0.06f));
        g.fillRoundedRectangle(rect, rad);
    }

    g.setColour(juce::Colours::white.withAlpha(on ? 0.16f : 0.09f));
    g.drawLine(rect.getX() + 2.0f, rect.getY() + 1.0f,
               rect.getRight() - 2.0f, rect.getY() + 1.0f, 1.0f);
    g.setColour(on ? lit.darker(0.6f) : juce::Colour(0xff000000).withAlpha(0.8f));
    g.drawRoundedRectangle(rect, rad, 1.0f);

    g.setColour(on ? juce::Colours::black.withAlpha(0.88f) : Theme::text.withAlpha(0.85f));

    if (isAutoGain)
    {
        g.setFont(Theme::label(14.0f));
        const float midY = bounds.getCentreY();
        g.drawText("AUTO", bounds.withBottom(midY + 1.0f),
                   juce::Justification::centredBottom, false);
        g.drawText("GAIN", bounds.withTop(midY - 1.0f),
                   juce::Justification::centredTop, false);
    }
    else
    {
        g.setFont(Theme::label(isBypass ? 18.0f : 16.0f));
        g.drawFittedText(txt, button.getLocalBounds(), juce::Justification::centred, 1);
    }
}

void AzazelLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                            const juce::Colour&,
                                            bool isHighlighted, bool isDown)
{
    const auto  rect = button.getLocalBounds().toFloat().reduced(0.5f);
    const float rad  = 2.0f;

    // Selected EQ-node button (see PluginEditor's bandButtons/"nodeSelect"
    // property): solid orange fill instead of the standard dark gradient,
    // so the single node currently bound to the Dynamics knobs reads
    // unambiguously against the other seven.
    if (button.getToggleState() && button.getProperties().contains("nodeSelect"))
    {
        for (int i = 2; i >= 1; --i)
        {
            g.setColour(juce::Colour(0xff000000).withAlpha(0.13f * float(3 - i)));
            g.fillRoundedRectangle(rect.translated(0.0f, float(i) * 0.9f), rad);
        }
        juce::Colour fill = Theme::accent;
        if (isDown)            fill = fill.darker(0.25f);
        else if (isHighlighted) fill = fill.brighter(0.12f);
        g.setColour(fill);
        g.fillRoundedRectangle(rect, rad);
        g.setColour(juce::Colours::white.withAlpha(0.18f));
        g.drawLine(rect.getX() + 2.0f, rect.getY() + 1.0f,
                   rect.getRight() - 2.0f, rect.getY() + 1.0f, 1.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawRoundedRectangle(rect, rad, 1.0f);
        return;
    }

    for (int i = 2; i >= 1; --i)
    {
        g.setColour(juce::Colour(0xff000000).withAlpha(0.13f * float(3 - i)));
        g.fillRoundedRectangle(rect.translated(0.0f, float(i) * 0.9f), rad);
    }

    juce::ColourGradient bd(juce::Colour(0xff302e2a), rect.getX(), rect.getY(),
                            juce::Colour(0xff141312), rect.getX(), rect.getBottom(), false);
    bd.addColour(0.55, juce::Colour(0xff1f1e1c));
    g.setGradientFill(bd);
    g.fillRoundedRectangle(rect, rad);

    if (isDown)
    {
        g.setColour(juce::Colour(0xff000000).withAlpha(0.30f));
        g.fillRoundedRectangle(rect, rad);
    }
    else if (isHighlighted)
    {
        g.setColour(Theme::accent.withAlpha(0.10f));
        g.fillRoundedRectangle(rect, rad);
    }

    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.drawLine(rect.getX() + 2.0f, rect.getY() + 1.0f,
               rect.getRight() - 2.0f, rect.getY() + 1.0f, 1.0f);
    g.setColour(isHighlighted ? Theme::accentMid : Theme::accentDim);
    g.drawRoundedRectangle(rect, rad, 1.0f);
}

juce::Font AzazelLookAndFeel::getTextButtonFont(juce::TextButton&, int height)
{
    return Theme::label(juce::jmin(18.0f, float(height) * 0.66f));
}

juce::Font AzazelLookAndFeel::getLabelFont(juce::Label& label)
{
    // Every Label this app creates sets its own font explicitly via
    // setFont() (see setupKnob/setupFader/createSliderTextBox below), so
    // this fallback only matters for JUCE-internal Labels (e.g. inside
    // AlertWindow) that don't.
    if (dynamic_cast<juce::Slider*>(label.getParentComponent()) != nullptr)
        return Theme::mono(16.0f, juce::Font::bold);
    return Theme::label(16.0f);
}

juce::Label* AzazelLookAndFeel::createSliderTextBox(juce::Slider& slider)
{
    auto* l = LookAndFeel_V4::createSliderTextBox(slider);
    l->setFont(Theme::mono(slider.getWidth() < 70 ? 12.0f : 16.0f, juce::Font::bold));
    return l;
}

void AzazelLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.fillAll(label.findColour(juce::Label::backgroundColourId));
    if (!label.isBeingEdited())
    {
        g.setColour(label.findColour(juce::Label::textColourId));
        g.setFont(label.getFont());
        g.drawFittedText(label.getText(), label.getLocalBounds(),
                         label.getJustificationType(), 1, 1.0f);
    }
}

void AzazelLookAndFeel::drawPopupMenuBackground(juce::Graphics& g, int width, int height)
{
    g.fillAll(Theme::bgDeep);
    g.setColour(Theme::accentDim);
    g.drawRect(0, 0, width, height, 1);
}

void AzazelLookAndFeel::drawPopupMenuItem(juce::Graphics& g, const juce::Rectangle<int>& area,
                                          bool isSeparator, bool isActive, bool isHighlighted,
                                          bool isTicked, bool hasSubMenu,
                                          const juce::String& text,
                                          const juce::String& shortcutKeyText,
                                          const juce::Drawable*, const juce::Colour*)
{
    if (isSeparator)
    {
        g.setColour(Theme::line);
        g.fillRect(area.reduced(6, 0).withHeight(1).translated(0, area.getHeight() / 2));
        return;
    }

    auto r = area.reduced(1);
    if (isHighlighted && isActive)
    {
        g.setColour(Theme::accent.withAlpha(0.22f));
        g.fillRect(r);
        g.setColour(Theme::accent);
        g.fillRect(r.withWidth(2));
    }

    g.setColour(isActive ? (isTicked ? Theme::accent : Theme::text)
                         : Theme::textDim.withAlpha(0.5f));

    if (isTicked)
    {
        g.setFont(Theme::mono(13.0f, juce::Font::bold));
        g.drawText(juce::String::charToString(0x2022),
                   r.getX() + 7, r.getY(), 12, r.getHeight(),
                   juce::Justification::centredLeft, false);
    }

    g.setFont(Theme::label(15.0f, isTicked ? juce::Font::bold : juce::Font::plain));
    g.drawText(text, r.getX() + 24, r.getY(), r.getWidth() - 40, r.getHeight(),
               juce::Justification::centredLeft, true);

    if (hasSubMenu)
    {
        g.setColour(Theme::accentMid);
        g.drawText(juce::String::charToString(0x203a),
                   r.getRight() - 16, r.getY(), 12, r.getHeight(),
                   juce::Justification::centred, false);
    }

    if (shortcutKeyText.isNotEmpty())
    {
        g.setColour(Theme::textDim);
        g.setFont(Theme::mono(11.0f));
        g.drawText(shortcutKeyText, r.getRight() - 80, r.getY(), 72, r.getHeight(),
                   juce::Justification::centredRight, false);
    }
}

//==============================================================================
// VisualCompEditor
//==============================================================================

VisualCompEditor::VisualCompEditor(VisualCompProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      inputDisplay ("INPUT",  Theme::inputCol,  p.inputWaveform,
                    p.apvts.getRawParameterValue("threshold"),
                    &p.sidechainWaveform, &p.sidechainEnabled,
                    p.apvts.getRawParameterValue("ratio"),
                    p.apvts.getRawParameterValue("knee"),
                    p.apvts.getRawParameterValue("attack"),
                    p.apvts.getRawParameterValue("release")),
      outputDisplay("OUTPUT", Theme::outputCol, p.outputWaveform),
      vuMeter(p.gainReductionDb),
      curveDisplay(p.apvts, p.currentInputLevelDb),
      levelMeter(p),
      gainInAtt   (p.apvts, "gainIn",    gainInFader),
      gainOutAtt  (p.apvts, "gainOut",   gainOutFader),
      thresholdAtt(p.apvts, "threshold", thresholdKnob),
      kneeAtt     (p.apvts, "knee",      kneeKnob),
      ratioAtt    (p.apvts, "ratio",     ratioKnob),
      attackAtt   (p.apvts, "attack",    attackKnob),
      releaseAtt  (p.apvts, "release",   releaseKnob),
      mixAtt      (p.apvts, "mix",       mixKnob),
      limiterAtt  (p.apvts, "limiter",   limiterButton)
{
    setLookAndFeel(&laf);

    setupFader(gainInFader,  gainInFaderLabel,  "GAIN IN",  "gainIn");
    setupFader(gainOutFader, gainOutFaderLabel, "GAIN OUT", "gainOut");

    setupToggle(limiterButton, "LIM");
    limiterLabel.setText("0 dB CEILING", juce::dontSendNotification);
    limiterLabel.setJustificationType(juce::Justification::centred);
    limiterLabel.setFont(Theme::label(12.0f));
    limiterLabel.setColour(juce::Label::textColourId, Theme::textDim);
    limiterLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(limiterLabel);

    setupToggle(sidechainButton, "SC");
    sidechainButton.onClick = [this]
    {
        audioProcessor.sidechainEnabled.store(sidechainButton.getToggleState(),
                                              std::memory_order_relaxed);
    };
    sidechainLabel.setText("SIDECHAIN", juce::dontSendNotification);
    sidechainLabel.setJustificationType(juce::Justification::centred);
    sidechainLabel.setFont(Theme::label(12.0f));
    sidechainLabel.setColour(juce::Label::textColourId, Theme::textDim);
    sidechainLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(sidechainLabel);

    setupToggle(autoGainButton, "AUTO GAIN");
    autoGainButton.onClick = [this]
    {
        audioProcessor.autoGainEnabled.store(autoGainButton.getToggleState(),
                                             std::memory_order_relaxed);
    };

    setupToggle(bypassButton, "BYPASS");
    bypassButton.onClick = [this]
    {
        audioProcessor.bypassed.store(bypassButton.getToggleState(),
                                      std::memory_order_relaxed);
    };

    // Mode
    {
        static const char* names[] = { "VCA", "FET", "OPTICAL", "TUBE" };
        currentModeName = names[juce::jlimit(0, 3,
            audioProcessor.compMode.load(std::memory_order_relaxed))];
    }
    setupTextButton(modeButton, currentModeName);
    modeButton.onClick = [this] { showModeMenu(); };

    // Presets
    setupTextButton(presetButton, audioProcessor.currentPresetName);
    presetButton.onClick = [this] { showPresetMenu(); };
    setupTextButton(presetPrev, juce::String::charToString(0x2039));
    presetPrev.onClick = [this] { stepPreset(-1); };
    setupTextButton(presetNext, juce::String::charToString(0x203a));
    presetNext.onClick = [this] { stepPreset(1); };
    setupTextButton(presetSave, "SAVE");
    presetSave.onClick = [this] { saveUserPreset(); };

    // EQ toggle (opens the docked parametric EQ panel)
    setupTextButton(eqButton, "EQ");
    eqButton.setClickingTogglesState(true);
    eqButton.onClick = [this] { toggleEqPanel(); };

    // Clip mode (cycled: Soft / Brickwall / Off)
    setupTextButton(clipModeButton,
        OutputClipper::modeName(static_cast<ClipMode>(audioProcessor.clipMode.load())));
    clipModeButton.onClick = [this] { cycleClipMode(); };

    // Auto-Analyze (next to the preset controls)
    setupTextButton(autoAnalyzeButton, "Smart Master+");
    autoAnalyzeButton.onClick = [this] { showAutoAnalyzeGenreStep(); };

    // Curve/GR toggle — Transfer Curve and Gain Reduction meter are hidden by
    // default (see vuMeter/curveDisplay setVisible below) and only rendered
    // while this is on.
    setupTextButton(curveGrButton, "CURVE/GR");
    curveGrButton.setClickingTogglesState(true);
    curveGrButton.onClick = [this] { toggleCurveGrPanel(); };

    // Docked parametric EQ panel — created up front, shown/hidden by toggleEqPanel()
    eqPanel = std::make_unique<EqPanel>(audioProcessor);
    eqPanel->onCloseRequested = [this] { toggleEqPanel(); };
    // Clicking a node directly in the graph selects it for editing here too
    // (always selects, unlike the numbered buttons' click-again-to-deselect).
    eqPanel->onNodeSelected = [this](int i)
    {
        selectedBand = i;
        refreshBandButtons();
    };
    addChildComponent(*eqPanel);
    eqPanelVisible = audioProcessor.eqPanelOpen;
    // Documentation aid, mirrors VC2_TOUR_STEP: forces the EQ panel open on
    // launch so its screenshot can be captured reproducibly. Unset in normal use.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_EQ_OPEN", {}).isNotEmpty())
        eqPanelVisible = true;
    eqButton.setToggleState(eqPanelVisible, juce::dontSendNotification);
    eqPanel->setVisible(eqPanelVisible);

    // Documentation aid, same pattern as VC2_FORCE_EQ_OPEN: seeds a few demo
    // EQ nodes (one linked) so the band-selector row and Q knob can be
    // screenshotted reproducibly without needing to click on the docked EQ
    // panel (synthetic clicks don't reach the window on Windows). Inert
    // unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_DEMO_EQ", {}).isNotEmpty())
    {
        EqNodeState n0; n0.enabled = true; n0.type = EqTypes::LowShelf;  n0.freqHz = 120.0f;  n0.gainDb = 3.0f;  n0.linked = true;
        EqNodeState n1; n1.enabled = true; n1.type = EqTypes::Bell;      n1.freqHz = 2500.0f; n1.gainDb = -4.0f;
        EqNodeState n2; n2.enabled = true; n2.type = EqTypes::HighShelf; n2.freqHz = 9000.0f; n2.gainDb = 2.0f;
        audioProcessor.eq.setNode(0, n0);
        audioProcessor.eq.setNode(1, n1);
        audioProcessor.eq.setNode(2, n2);
    }

    // Documentation aid, same pattern as VC2_FORCE_EQ_OPEN: switches on
    // multiband mode and pins node 0's bandGrDb to a fixed demo value (see
    // VisualCompProcessor::debugForceBandGrDemo) so the per-band gain-
    // reduction dip can be screenshotted reproducibly with no signal
    // flowing. Inert unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_MULTIBAND", {}).isNotEmpty())
    {
        audioProcessor.multibandEnabled.store(true, std::memory_order_relaxed);
        audioProcessor.debugForceBandGrDemo.store(true, std::memory_order_relaxed);
    }

    // Logo artwork + hot-spot
    if (auto xml = juce::XmlDocument::parse(juce::String(kAzazelLogoSvg)))
    {
        logoDrawable = juce::Drawable::createFromSVG(*xml);
        if (logoDrawable != nullptr)
            logoDrawable->replaceColour(juce::Colours::white, Theme::text);
    }
    logoZone.onClick = [this] { showLogoMenu(); };
    addAndMakeVisible(logoZone);

    // Mix knob
    mixKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    mixKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    mixKnob.setMouseDragSensitivity(600);
    mixKnob.setRange(0.0, 1.0);
    mixKnob.getProperties().set("simpleTicks", true);
    mixKnob.onValueChange = [this] { repaint(0, 0, getWidth(), kTitleH); };
    addAndMakeVisible(mixKnob);

    setupKnob(thresholdKnob, thresholdLabel, "THRESHOLD", " dB", "threshold");
    setupKnob(kneeKnob,      kneeLabel,      "KNEE",      " dB", "knee");
    setupKnob(ratioKnob,     ratioLabel,     "RATIO",     ":1",  "ratio");
    setupKnob(attackKnob,    attackLabel,    "ATTACK",    " ms", "attack");
    setupKnob(releaseKnob,   releaseLabel,   "RELEASE",   " ms", "release");

    // Band-selector row: clicking a button swaps Attack/Release below to
    // that EQ node's own state (see selectBand/refreshBandButtons). Hidden
    // until an EQ node is enabled.
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        auto& b = bandButtons[size_t(i)];
        b.setButtonText(juce::String(i + 1));
        b.setColour(juce::TextButton::textColourOffId, Theme::textDim);
        b.setColour(juce::TextButton::textColourOnId,  juce::Colours::white);
        // Opts this button into AzazelLookAndFeel's solid-orange selected
        // style (see drawButtonBackground) instead of the standard dark
        // gradient every other TextButton gets.
        b.getProperties().set("nodeSelect", true);
        b.onClick = [this, i] { selectBand(i); };
        b.setVisible(false);
        addAndMakeVisible(b);
    }

    // Band-context knobs share their on-screen slot with the global
    // Attack/Release/Q controls but are wired straight to the selected EQ
    // node's state rather than an APVTS parameter — a SliderAttachment can
    // only ever bind to one fixed parameter, so context-sensitive editing
    // needs a second, manually-wired slider shown/hidden via setVisible().
    auto setupBandKnob = [this](DragSlider& knob, float lo, float hi, float skew, float defVal)
    {
        knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
        knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 24);
        knob.setTextBoxIsEditable(false);
        knob.setNumDecimalPlacesToDisplay(1);
        knob.setMouseDragSensitivity(1000);
        knob.getProperties().set("topInset", kKnobLblH + 4);
        knob.setRange(lo, hi);
        knob.setSkewFactor(skew);
        knob.setDoubleClickReturnValue(true, defVal);
        knob.setVisible(false);
        addAndMakeVisible(knob);
    };
    setupBandKnob(bandAttackKnob,  0.1f,  200.0f,  0.3f, 0.2f);
    setupBandKnob(bandReleaseKnob, 1.0f,  2000.0f, 0.3f, 45.0f);
    // FabFilter Pro-MB style: Threshold is downward-only (-inf..0 — -96
    // stands in for -inf). Direction (downward/upward) and how far the band
    // can swing now live on the Range knob instead (see NodeIsland) — that's
    // what used to make a positive Threshold "auto-engage" upward and feel
    // weird turning through the crossover; Threshold no longer carries that
    // dual role.
    setupBandKnob(bandThresholdKnob, -96.0f, 0.0f, 0.5f, -10.0f);
    setupBandKnob(bandKneeKnob,        0.0f, 20.0f, 1.0f,   6.0f);
    setupBandKnob(bandRatioKnob,       1.0f, 20.0f, 0.4f,   2.0f);
    bandAttackKnob.setTextValueSuffix(" ms");
    bandReleaseKnob.setTextValueSuffix(" ms");
    bandThresholdKnob.setTextValueSuffix(" dB");
    bandKneeKnob.setTextValueSuffix(" dB");
    bandRatioKnob.setTextValueSuffix(":1");
    bandAttackKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.attackMs = float(bandAttackKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandReleaseKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.releaseMs = float(bandReleaseKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandThresholdKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.thresholdDb = float(bandThresholdKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandKneeKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.kneeDb = float(bandKneeKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandRatioKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.ratio = float(bandRatioKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };

    // Documentation aid, same pattern as VC2_FORCE_EQ_OPEN: selects a given
    // EQ node (1-based, matching the on-screen numbering) on launch, so the
    // per-node Threshold/Knee/Ratio/Attack/Release knobs and the selected-
    // node button styling can be screenshotted reproducibly without needing
    // a click (synthetic clicks don't reach the window on Windows). Inert
    // unless set.
    {
        const auto forced = juce::SystemStats::getEnvironmentVariable("VC2_FORCE_BAND_SELECT", {});
        if (forced.isNotEmpty())
        {
            selectedBand = juce::jlimit(0, kMaxEqNodes - 1, forced.getIntValue() - 1);
            eqPanel->setSelectedNode(selectedBand);
        }
    }

    refreshBandButtons();

    addAndMakeVisible(inputDisplay);
    addAndMakeVisible(outputDisplay);
    addChildComponent(vuMeter);
    addChildComponent(curveDisplay);
    addAndMakeVisible(levelMeter);

    // Hidden by default — only shown while curveGrButton is toggled on.
    curveGrVisible = audioProcessor.curveGrPanelOpen;
    // Documentation aid, same pattern as VC2_FORCE_EQ_OPEN: forces the
    // Transfer Curve / Gain Reduction column open on launch. Inert unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_CURVE_GR", {}).isNotEmpty())
        curveGrVisible = true;
    curveGrButton.setToggleState(curveGrVisible, juce::dontSendNotification);
    vuMeter.setVisible(curveGrVisible);
    curveDisplay.setVisible(curveGrVisible);

    // Onboarding overlay sits above everything
    helpOverlay.onFinished = [] { markHelpSeen(); };
    addChildComponent(helpOverlay);

    setSize(totalEditorWidth(), kHeight);
    setResizable(false, false);

    if (audioProcessor.editorScale != 1.0f)
        setScaleFactor(audioProcessor.editorScale);

    startTimerHz(30);   // keeps the band-selector row synced with the EQ panel

    // First run: show the tour once the window has settled
    if (!hasSeenHelp())
    {
        juce::Component::SafePointer<VisualCompEditor> safe(this);
        juce::Timer::callAfterDelay(500, [safe]
        {
            if (safe != nullptr) safe->startHelpTour();
        });
    }
}

VisualCompEditor::~VisualCompEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void VisualCompEditor::timerCallback()
{
    // Pick up out-of-band EQ changes (docked panel edits, Auto-Analyze) so
    // the band row and any context-swapped knobs stay in sync.
    refreshBandButtons();
}

void VisualCompEditor::setupKnob(DragSlider& knob, juce::Label& label,
                                 const juce::String& text, const juce::String& suffix,
                                 const juce::String& paramId)
{
    knob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 96, 24);
    knob.setTextBoxIsEditable(false);          // whole box is a drag target
    knob.setTextValueSuffix(suffix);
    knob.setNumDecimalPlacesToDisplay(1);
    knob.setMouseDragSensitivity(1000);
    knob.getProperties().set("topInset", kKnobLblH + 4);
    knob.onValueChange = [this] { repaint(0, kCtrlY, kContentW, kCtrlH); };

    if (auto* par = audioProcessor.apvts.getParameter(paramId))
        knob.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));

    addAndMakeVisible(knob);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(Theme::label(17.0f));
    label.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    label.setInterceptsMouseClicks(false, false);   // clicks fall through to the knob
    addAndMakeVisible(label);
}

void VisualCompEditor::setupFader(DragSlider& fader, juce::Label& label,
                                  const juce::String& text, const juce::String& paramId)
{
    fader.setSliderStyle(juce::Slider::LinearVertical);
    fader.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 20);
    fader.setTextBoxIsEditable(false);
    fader.setTextValueSuffix(" dB");
    fader.setNumDecimalPlacesToDisplay(1);
    fader.setMouseDragSensitivity(500);

    if (auto* par = audioProcessor.apvts.getParameter(paramId))
        fader.setDoubleClickReturnValue(true, par->convertFrom0to1(par->getDefaultValue()));

    addAndMakeVisible(fader);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(Theme::label(16.0f));
    label.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    label.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(label);
}

void VisualCompEditor::setupTextButton(juce::TextButton& b, const juce::String& text)
{
    b.setButtonText(text);
    b.setColour(juce::TextButton::textColourOffId, Theme::accent);
    b.setColour(juce::TextButton::textColourOnId,  Theme::accent.brighter(0.2f));
    addAndMakeVisible(b);
}

void VisualCompEditor::setupToggle(juce::ToggleButton& b, const juce::String& text)
{
    b.setButtonText(text);
    addAndMakeVisible(b);
}

//==============================================================================
// Mode
//==============================================================================

void VisualCompEditor::showModeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    menu.addSectionHeader("DETECTOR CIRCUIT");
    menu.addItem(1, "VCA  -  Bus / Mix",             true, currentModeName == "VCA");
    menu.addItem(2, "FET  -  Drums / Bass / Vocals", true, currentModeName == "FET");
    menu.addItem(3, "Optical  -  Vocals / Acoustic", true, currentModeName == "OPTICAL");
    menu.addItem(4, "Tube  -  Lead Vox / Mastering", true, currentModeName == "TUBE");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(modeButton),
        [this](int result) { if (result > 0) applyMode(result - 1); });
}

void VisualCompEditor::applyMode(int modeIndex)
{
    audioProcessor.compMode.store(modeIndex, std::memory_order_relaxed);
    static const char* names[] = { "VCA", "FET", "OPTICAL", "TUBE" };
    currentModeName = names[juce::jlimit(0, 3, modeIndex)];
    modeButton.setButtonText(currentModeName);
    repaint();
}

//==============================================================================
// Clipping mode + docked EQ panel
//==============================================================================

void VisualCompEditor::cycleClipMode()
{
    const int next = (audioProcessor.clipMode.load(std::memory_order_relaxed) + 1)
                     % static_cast<int>(ClipMode::kNumModes);
    audioProcessor.clipMode.store(next, std::memory_order_relaxed);
    clipModeButton.setButtonText(OutputClipper::modeName(static_cast<ClipMode>(next)));
}

int VisualCompEditor::totalEditorWidth() const
{
    return kWidth + (eqPanelVisible ? kEqPanelW : 0)
                  - (curveGrVisible ? 0 : kCurveGrColW - kCurveGrRightPad);
}

void VisualCompEditor::toggleEqPanel()
{
    eqPanelVisible = !eqPanelVisible;
    audioProcessor.eqPanelOpen = eqPanelVisible;
    eqButton.setToggleState(eqPanelVisible, juce::dontSendNotification);
    eqPanel->setVisible(eqPanelVisible);
    setSize(totalEditorWidth(), kHeight);
    resized();
    repaint();
}

void VisualCompEditor::toggleCurveGrPanel()
{
    curveGrVisible = !curveGrVisible;
    audioProcessor.curveGrPanelOpen = curveGrVisible;
    curveGrButton.setToggleState(curveGrVisible, juce::dontSendNotification);
    vuMeter.setVisible(curveGrVisible);
    curveDisplay.setVisible(curveGrVisible);
    setSize(totalEditorWidth(), kHeight);
    resized();
    repaint();
}

//==============================================================================
// Band-selector row (Attack/Release context switching)
//==============================================================================

void VisualCompEditor::selectBand(int i)
{
    selectedBand = (selectedBand == i) ? -1 : i;   // click again to return to the global compressor
    // EqPanel owns node selection/multi-selection independently (see its
    // onNodeSelected callback below) — this is the one place a band-row
    // click pushes selectedBand back into it, so a Ctrl+Click
    // multi-selection made directly in the graph isn't clobbered by
    // refreshBandButtons()'s periodic (30Hz timer-driven) re-sync.
    if (eqPanel != nullptr) eqPanel->setSelectedNode(selectedBand);
    refreshBandButtons();
}

void VisualCompEditor::refreshBandButtons()
{
    bool layoutDirty = false;   // enabled-node count changed -> re-run resized()
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        const auto n = audioProcessor.eq.getNode(i);
        if (bandButtons[size_t(i)].isVisible() != n.enabled) layoutDirty = true;
        bandButtons[size_t(i)].setVisible(n.enabled);
    }

    if (selectedBand >= 0 && !audioProcessor.eq.getNode(selectedBand).enabled)
        selectedBand = -1;

    const bool bandCtx = selectedBand >= 0;
    attackKnob.setVisible(!bandCtx);
    releaseKnob.setVisible(!bandCtx);
    bandAttackKnob.setVisible(bandCtx);
    bandReleaseKnob.setVisible(bandCtx);
    thresholdKnob.setVisible(!bandCtx);
    kneeKnob.setVisible(!bandCtx);
    ratioKnob.setVisible(!bandCtx);
    bandThresholdKnob.setVisible(bandCtx);
    bandKneeKnob.setVisible(bandCtx);
    bandRatioKnob.setVisible(bandCtx);

    const juce::String bandNum = juce::String(selectedBand + 1);
    attackLabel.setText(bandCtx ? "ATK N" + bandNum : "ATTACK", juce::dontSendNotification);
    releaseLabel.setText(bandCtx ? "REL N" + bandNum : "RELEASE", juce::dontSendNotification);
    thresholdLabel.setText(bandCtx ? "THR N" + bandNum : "THRESHOLD", juce::dontSendNotification);
    kneeLabel.setText(bandCtx ? "KNEE N" + bandNum : "KNEE", juce::dontSendNotification);
    ratioLabel.setText(bandCtx ? "RATIO N" + bandNum : "RATIO", juce::dontSendNotification);

    if (bandCtx && !bandAttackKnob.isMouseButtonDown())
        bandAttackKnob.setValue(audioProcessor.eq.getNode(selectedBand).attackMs, juce::dontSendNotification);
    if (bandCtx && !bandReleaseKnob.isMouseButtonDown())
        bandReleaseKnob.setValue(audioProcessor.eq.getNode(selectedBand).releaseMs, juce::dontSendNotification);
    if (bandCtx && !bandThresholdKnob.isMouseButtonDown())
        bandThresholdKnob.setValue(audioProcessor.eq.getNode(selectedBand).thresholdDb, juce::dontSendNotification);
    if (bandCtx && !bandKneeKnob.isMouseButtonDown())
        bandKneeKnob.setValue(audioProcessor.eq.getNode(selectedBand).kneeDb, juce::dontSendNotification);
    if (bandCtx && !bandRatioKnob.isMouseButtonDown())
        bandRatioKnob.setValue(audioProcessor.eq.getNode(selectedBand).ratio, juce::dontSendNotification);

    // Selected-node styling: solid orange fill (AzazelLookAndFeel, gated on
    // the "nodeSelect" property set at construction) with crisp white text
    // for the one button currently bound to the Dynamics knobs.
    for (int i = 0; i < kMaxEqNodes; ++i)
        bandButtons[size_t(i)].setToggleState(i == selectedBand, juce::dontSendNotification);

    // The band-selector row centers itself on the enabled-node count, which
    // needs a full resized() (not just a repaint) when it changes.
    if (layoutDirty) resized();

    repaint(0, kCtrlY, kContentW, kCtrlH);
}

//==============================================================================
// Presets
//==============================================================================

juce::File VisualCompEditor::getUserPresetDir()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                   .getChildFile("Azazel Audio")
                   .getChildFile("VisualComp 2")
                   .getChildFile("Presets");
    dir.createDirectory();
    return dir;
}

void VisualCompEditor::setPresetName(const juce::String& name)
{
    audioProcessor.currentPresetName = name;
    presetButton.setButtonText(name);
}

void VisualCompEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);

    juce::String category;
    juce::PopupMenu sub;
    for (int i = 0; i < kNumFactoryPresets; ++i)
    {
        const auto& fp = kFactoryPresets[i];
        if (category != fp.category)
        {
            if (category.isNotEmpty()) menu.addSubMenu(category, sub);
            sub = {};
            sub.setLookAndFeel(&laf);
            category = fp.category;
        }
        sub.addItem(1000 + i, fp.name, true, audioProcessor.currentPresetName == fp.name);
    }
    if (category.isNotEmpty()) menu.addSubMenu(category, sub);

    auto userFiles = getUserPresetDir().findChildFiles(juce::File::findFiles, false, "*.vcpreset");
    userFiles.sort();
    if (!userFiles.isEmpty())
    {
        menu.addSeparator();
        menu.addSectionHeader("MY PRESETS");
        for (int i = 0; i < userFiles.size(); ++i)
            menu.addItem(2000 + i, userFiles[i].getFileNameWithoutExtension(), true,
                         audioProcessor.currentPresetName
                             == userFiles[i].getFileNameWithoutExtension());
    }

    menu.addSeparator();
    menu.addItem(1, "Save Preset As...");
    menu.addItem(2, "Open Presets Folder");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(presetButton)
                                                 .withMinimumWidth(240),
        [this, userFiles](int result)
        {
            if (result >= 2000)      loadUserPreset(userFiles[result - 2000]);
            else if (result >= 1000) applyFactoryPreset(result - 1000);
            else if (result == 1)    saveUserPreset();
            else if (result == 2)    getUserPresetDir().revealToUser();
        });
}

void VisualCompEditor::applyFactoryPreset(int index)
{
    if (index < 0 || index >= kNumFactoryPresets) return;
    const auto& fp = kFactoryPresets[index];

    auto setParam = [this](const char* id, float value)
    {
        if (auto* par = audioProcessor.apvts.getParameter(id))
            par->setValueNotifyingHost(par->convertTo0to1(value));
    };
    setParam("threshold", fp.threshold);
    setParam("knee",      fp.knee);
    setParam("ratio",     fp.ratio);
    setParam("attack",    fp.attackMs);
    setParam("release",   fp.releaseMs);
    setParam("gainIn",    fp.gainIn);
    setParam("gainOut",   fp.gainOut);
    setParam("mix",       fp.mix);
    setParam("limiter",   fp.limiter ? 1.0f : 0.0f);

    applyMode(fp.mode);
    audioProcessor.clipMode.store(fp.clipMode, std::memory_order_relaxed);
    clipModeButton.setButtonText(OutputClipper::modeName(static_cast<ClipMode>(fp.clipMode)));
    setPresetName(fp.name);
}

void VisualCompEditor::stepPreset(int delta)
{
    int current = -1;
    for (int i = 0; i < kNumFactoryPresets; ++i)
        if (audioProcessor.currentPresetName == kFactoryPresets[i].name) { current = i; break; }

    const int next = (current < 0) ? (delta > 0 ? 0 : kNumFactoryPresets - 1)
                                   : (current + delta + kNumFactoryPresets) % kNumFactoryPresets;
    applyFactoryPreset(next);
}

void VisualCompEditor::saveUserPreset()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save preset", getUserPresetDir().getChildFile("My Preset.vcpreset"), "*.vcpreset");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            file = file.withFileExtension(".vcpreset");

            auto state = audioProcessor.apvts.copyState();
            state.setProperty("compMode",
                audioProcessor.compMode.load(std::memory_order_relaxed), nullptr);
            state.setProperty("clipMode",
                audioProcessor.clipMode.load(std::memory_order_relaxed), nullptr);
            if (auto xml = state.createXml())
                if (xml->writeTo(file))
                    setPresetName(file.getFileNameWithoutExtension());
        });
}

void VisualCompEditor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return;
    if (auto xml = juce::parseXML(file))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid() && state.hasType(audioProcessor.apvts.state.getType()))
        {
            audioProcessor.apvts.replaceState(state);
            applyMode(int(state.getProperty("compMode", 0)));
            const int cm = int(state.getProperty("clipMode", 0));
            audioProcessor.clipMode.store(cm, std::memory_order_relaxed);
            clipModeButton.setButtonText(OutputClipper::modeName(static_cast<ClipMode>(cm)));
            setPresetName(file.getFileNameWithoutExtension());
        }
    }
}

//==============================================================================
// Auto-Analyze wizard
//==============================================================================
// A two-step wizard (genre, then LUFS target) that measures the material
// currently flowing through the plugin's existing input ring buffer and
// generates a starting-point preset from it. This is a heuristic — crest
// factor and measured level driving table-based ratio/attack/release
// choices per genre, plus a gain trim toward the requested loudness — not
// a machine-learned mastering engine. It is honest, useful, and reproducible,
// which matters more here than sounding more sophisticated than it is.

void VisualCompEditor::showAutoAnalyzeGenreStep()
{
    auto* aw = new juce::AlertWindow("Smart Master+ - Step 1 of 2",
        "Choose the closest genre for this material.",
        juce::AlertWindow::NoIcon);
    aw->setLookAndFeel(&laf);

    juce::StringArray names;
    for (int i = 0; i < kNumGenres; ++i) names.add(kGenres[i].name);
    aw->addComboBox("genre", names, "Genre");
    if (auto* cb = aw->getComboBoxComponent("genre"))
        cb->setSelectedItemIndex(0);

    aw->addButton("Next",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    wizardWindow.reset(aw);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this](int result)
        {
            if (result == 1 && wizardWindow != nullptr)
            {
                int idx = 0;
                if (auto* cb = wizardWindow->getComboBoxComponent("genre"))
                    idx = cb->getSelectedItemIndex();
                const juce::String genre = kGenres[juce::jlimit(0, kNumGenres - 1, idx)].name;
                wizardWindow.reset();
                showAutoAnalyzeLufsStep(genre);
            }
            else
            {
                wizardWindow.reset();
            }
        }), false);
}

void VisualCompEditor::showAutoAnalyzeLufsStep(const juce::String& genre)
{
    float defaultLufs = -9.0f;
    for (int i = 0; i < kNumGenres; ++i)
        if (genre == kGenres[i].name) { defaultLufs = kGenres[i].defaultLufs; break; }

    auto* aw = new juce::AlertWindow("Smart Master+ - " + genre,
        "Step 2 of 2 - target loudness in LUFS (approx). Typical masters run "
        "from about -18 LU (classical) to -6 LU (EDM/club).",
        juce::AlertWindow::NoIcon);
    aw->setLookAndFeel(&laf);
    aw->addTextEditor("lufs", juce::String(defaultLufs, 1), "Target LUFS");
    aw->addButton("Analyze", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Back",    2);
    aw->addButton("Cancel",  0, juce::KeyPress(juce::KeyPress::escapeKey));

    wizardWindow.reset(aw);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, genre](int result)
        {
            if (result == 1 && wizardWindow != nullptr)
            {
                const float target = wizardWindow->getTextEditorContents("lufs").getFloatValue();
                wizardWindow.reset();
                runAutoAnalyze(genre, target);
            }
            else if (result == 2)
            {
                wizardWindow.reset();
                showAutoAnalyzeGenreStep();
            }
            else
            {
                wizardWindow.reset();
            }
        }), false);
}

void VisualCompEditor::runAutoAnalyze(const juce::String& genre, float targetLufs)
{
    // Measure the material already captured in the input ring buffer (the
    // same data driving the on-screen input waveform).
    const auto& buf = audioProcessor.inputWaveform.data;
    constexpr int N = WaveformBuffer::size;
    double sumSq = 0.0;
    float  peak  = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        const float s = buf[size_t(i)];
        sumSq += double(s) * double(s);
        peak = juce::jmax(peak, std::abs(s));
    }
    const float rms  = float(std::sqrt(sumSq / double(N)));
    const float rmsDb  = rms  > 1.0e-9f ? juce::Decibels::gainToDecibels(rms)  : -60.0f;
    const float peakDb = peak > 1.0e-9f ? juce::Decibels::gainToDecibels(peak) : -60.0f;
    const float crestDb = peakDb - rmsDb;

    struct Character { float ratio, attackMs, releaseMs; int mode; ClipMode clip; };
    auto characterFor = [](const juce::String& g) -> Character
    {
        if (g == "Pop")                     return { 2.5f, 20.0f, 150.0f, 0, ClipMode::Brickwall };
        if (g == "Hip-Hop / Trap")          return { 3.5f,  5.0f, 120.0f, 1, ClipMode::Brickwall };
        if (g == "EDM / Dance")             return { 4.0f,  3.0f, 100.0f, 1, ClipMode::Brickwall };
        if (g == "Rock / Metal")            return { 3.0f, 10.0f, 150.0f, 1, ClipMode::Brickwall };
        if (g == "Acoustic / Folk")         return { 1.8f, 40.0f, 300.0f, 2, ClipMode::Soft };
        if (g == "Classical / Orchestral")  return { 1.5f, 60.0f, 400.0f, 2, ClipMode::Off };
        return                                     { 2.2f, 25.0f, 300.0f, 2, ClipMode::Brickwall }; // Podcast
    };
    const auto ch = characterFor(genre);

    // Threshold at the midpoint between measured RMS and peak: punchier,
    // higher-crest-factor material gets compressed harder; already-dense
    // material is left mostly alone.
    const float thresholdDb = juce::jlimit(-40.0f, -1.0f, (peakDb + rmsDb) * 0.5f);

    // Trim output gain toward the requested loudness using the plugin's own
    // (approximate) short-term LUFS reading of the current live signal.
    const float currentLufs = audioProcessor.meterShortLufs.load(std::memory_order_relaxed);
    const float gainOutDb = (currentLufs > -90.0f)
        ? juce::jlimit(-24.0f, 24.0f, targetLufs - currentLufs) : 0.0f;

    auto setParam = [this](const char* id, float value)
    {
        if (auto* par = audioProcessor.apvts.getParameter(id))
            par->setValueNotifyingHost(par->convertTo0to1(value));
    };
    setParam("threshold", thresholdDb);
    setParam("ratio",     ch.ratio);
    setParam("attack",    ch.attackMs);
    setParam("release",   ch.releaseMs);
    setParam("knee",      6.0f);
    setParam("gainIn",    0.0f);
    setParam("gainOut",   gainOutDb);
    setParam("mix",       1.0f);
    setParam("limiter",   0.0f);

    applyMode(ch.mode);
    audioProcessor.clipMode.store(static_cast<int>(ch.clip), std::memory_order_relaxed);
    clipModeButton.setButtonText(OutputClipper::modeName(ch.clip));

    const juce::String name = "Smart Master+: " + genre + "  " + juce::String(targetLufs, 1) + " LU";
    setPresetName(name);

    juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
        "Smart Master+ complete",
        "Generated \"" + name + "\" from " + juce::String(crestDb, 1)
            + " dB of measured crest factor in the current input.\n\n"
              "This is a heuristic starting point, not a mastering-grade AI - "
              "refine threshold, ratio, attack and release by ear.");
}

//==============================================================================
// Zoom
//==============================================================================

void VisualCompEditor::applyZoom(float scale)
{
    audioProcessor.editorScale = scale;
    setScaleFactor(scale);
}

//==============================================================================
// Help tour / logo menu
//==============================================================================

juce::File VisualCompEditor::settingsFile()
{
    auto dir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                   .getChildFile("Azazel Audio")
                   .getChildFile("VisualComp 2");
    dir.createDirectory();
    return dir.getChildFile("settings.xml");
}

bool VisualCompEditor::hasSeenHelp()
{
    if (auto xml = juce::parseXML(settingsFile()))
        return xml->getBoolAttribute("helpSeen", false);
    return false;
}

void VisualCompEditor::markHelpSeen()
{
    juce::XmlElement xml("VisualComp2Settings");
    xml.setAttribute("helpSeen", true);
    xml.writeTo(settingsFile());
}

void VisualCompEditor::showLogoMenu()
{
    // The zoom control lives here, at the top of the Azazel menu, rather
    // than as its own header button — the logo hot-spot stays the only
    // visible/interactive mark in the header, everything else is reached
    // through this one (invisible) entry point.
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);

    juce::PopupMenu zoomMenu;
    zoomMenu.setLookAndFeel(&laf);
    for (float s : { 0.5f, 0.7f, 1.0f, 1.5f, 2.0f })
        zoomMenu.addItem(int(s * 100.0f), juce::String(int(s * 100.0f)) + "%",
                        true, std::abs(audioProcessor.editorScale - s) < 0.01f);
    menu.addSubMenu("Zoom  (" + juce::String(int(audioProcessor.editorScale * 100.0f)) + "%)",
                    zoomMenu);

    menu.addSeparator();
    menu.addSectionHeader("AZAZEL AUDIO");
    menu.addItem(1, "Show Help");
    menu.addItem(2, "Visit azazelaudio.com");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(logoZone)
                                                 .withMinimumWidth(200),
        [this](int result)
        {
            if (result == 1)      startHelpTour();
            else if (result == 2) juce::URL("https://azazelaudio.com").launchInDefaultBrowser();
            else if (result > 0)  applyZoom(float(result) / 100.0f);   // zoom submenu ids are % values
        });
}

void VisualCompEditor::startHelpTour()
{
    const int ox = eqPanelVisible ? kEqPanelW : 0;
    const auto waveArea = juce::Rectangle<int>(ox + 5, kWaveY, kContentW - 14, kWaveH);
    const auto knobArea = juce::Rectangle<int>(ox + kKnobAreaX, kCtrlY + 4, kKnobAreaW, kCtrlH - 14);

    std::vector<HelpOverlay::Step> steps;

    steps.push_back({ {}, "Welcome to VisualComp 2",
        "A compressor you can actually see. This 30-second tour covers the essentials. "
        "Click anywhere, or press NEXT, to continue." });

    steps.push_back({ presetButton.getBounds().getUnion(presetSave.getBounds()),
        "Start with a preset",
        "34 factory presets for drums, vocals, bass, mastering and more. "
        "Step through them with the arrows, or click the name to browse. SAVE keeps your own." });

    steps.push_back({ modeButton.getBounds(), "Pick a circuit",
        "VCA is clean, FET is fast and punchy, Optical is smooth, Tube is warm. "
        "Each one changes how the compressor reacts, not just its name." });

    steps.push_back({ waveArea, "Watch it work",
        "Input on the left, compressed output on the right. The dashed red line is your "
        "threshold: anything crossing it is being compressed." });

    steps.push_back({ knobArea, "Drag anywhere, hold Ctrl for fine",
        "Click and drag anywhere inside a control's box to change it. "
        "Hold Ctrl (Cmd on Mac) while dragging for fine adjustment, "
        "or double-click to reset it." });

    steps.push_back({ curveGrButton.getBounds(), "See the gain reduction",
        "CURVE/GR is collapsed by default to keep things tidy. Click it to slide out the "
        "gain-reduction needle and transfer curve — aim for 2-4 dB on a bus, more on a "
        "single track." });

    steps.push_back({ sidechainButton.getBounds(), "External sidechain",
        "Route the key track (usually a kick) to this plugin's sidechain input, then press SC "
        "to see and hear the ducking.\n"
        "FL Studio: plugin wrapper menu > Processing > Connections, then pick your External "
        "input - route that track to this one first.\n"
        "Live / Cubase / REAPER: choose the source in the plugin's sidechain routing." });

    steps.push_back({ logoZone.getBounds(), "Need this again?",
        "Click the Azazel logo any time for Show Help, or to visit azazelaudio.com. "
        "Enjoy the plugin." });

    helpOverlay.setSteps(std::move(steps));
    helpOverlay.setBounds(getLocalBounds());

    // Documentation aid: VC2_TOUR_STEP=<n> opens the tour on a given step so the
    // manual's screenshots can be captured reproducibly. Unset in normal use.
    const auto forced = juce::SystemStats::getEnvironmentVariable("VC2_TOUR_STEP", {});
    helpOverlay.startAt(forced.isNotEmpty() ? forced.getIntValue() - 1 : 0);
}

//==============================================================================
// Azazel wordmark
//==============================================================================

void VisualCompEditor::drawLogo(juce::Graphics& g, juce::Rectangle<float> a) const
{
    if (logoDrawable == nullptr) return;

    logoDrawable->drawWithin(
        g, a,
        juce::RectanglePlacement(juce::RectanglePlacement::xLeft
                               | juce::RectanglePlacement::yMid),
        1.0f);
}

void VisualCompEditor::drawTabPanel(juce::Graphics& g, juce::Rectangle<int> r,
                                    const juce::String& tabText) const
{
    const auto rf = r.toFloat();

    g.setColour(Theme::bgDeep.withAlpha(0.55f));
    g.fillRect(rf);

    juce::ColourGradient wash(Theme::text.withAlpha(0.028f), rf.getCentreX(), rf.getY(),
                              juce::Colour(0x00000000), rf.getCentreX(), rf.getBottom(), false);
    g.setGradientFill(wash);
    g.fillRect(rf);

    g.setColour(Theme::line);
    g.drawRect(rf, 1.0f);

    if (tabText.isNotEmpty())
    {
        auto font = Theme::label(13.0f);
        const float tw = font.getStringWidthFloat(tabText) + 16.0f;
        const juce::Rectangle<float> tab(rf.getX() + 8.0f, rf.getY() - 1.0f, tw, 15.0f);
        g.setColour(Theme::bg);
        g.fillRect(tab);
        g.setColour(Theme::accentDim);
        g.drawRect(tab, 1.0f);
        g.setColour(Theme::accent);
        g.setFont(font);
        g.drawText(tabText, tab, juce::Justification::centred, false);
    }
}

//==============================================================================
// Paint
//==============================================================================

void VisualCompEditor::paint(juce::Graphics& g)
{
    const int   w  = getWidth();
    const int   h  = getHeight();
    const float fw = float(w);
    const float fh = float(h);

    g.setColour(Theme::bg);
    g.fillRect(0, 0, w, h);

    // Metallic brush + wear
    g.setColour(Theme::text.withAlpha(0.008f));
    for (int yy = 1; yy < h; yy += 2)
        g.drawHorizontalLine(yy, 0.0f, fw);
    {
        juce::Random rng(0x5EED);
        for (int i = 0; i < 110; ++i)
        {
            const float px = rng.nextFloat() * fw;
            const float py = rng.nextFloat() * fh;
            const float sz = 0.6f + rng.nextFloat() * 1.7f;
            g.setColour(rng.nextBool()
                ? Theme::text.withAlpha(0.012f + rng.nextFloat() * 0.020f)
                : juce::Colours::black.withAlpha(0.10f + rng.nextFloat() * 0.16f));
            g.fillEllipse(px, py, sz, sz);
        }
        for (int i = 0; i < 12; ++i)
        {
            const float x1 = rng.nextFloat() * fw, y1 = rng.nextFloat() * fh;
            const float len = 14.0f + rng.nextFloat() * 52.0f;
            const float ang = rng.nextFloat() * juce::MathConstants<float>::twoPi;
            g.setColour(Theme::text.withAlpha(0.007f + rng.nextFloat() * 0.012f));
            g.drawLine(x1, y1, x1 + len * std::cos(ang), y1 + len * std::sin(ang), 0.6f);
        }
    }

    {
        juce::ColourGradient top(juce::Colours::white.withAlpha(0.040f), fw * 0.5f, 0.0f,
                                 juce::Colours::white.withAlpha(0.000f), fw * 0.5f, fh * 0.60f, true);
        g.setGradientFill(top); g.fillRect(0, 0, w, h);
    }
    { juce::ColourGradient v(juce::Colour(0x00000000), 0.f, fh*0.58f, juce::Colour(0x66000000), 0.f, fh, false); g.setGradientFill(v); g.fillRect(0,0,w,h); }
    { juce::ColourGradient v(juce::Colour(0x40000000), 0.f, 0.f, juce::Colour(0x00000000), fw*0.16f, 0.f, false); g.setGradientFill(v); g.fillRect(0,0,w,h); }
    { juce::ColourGradient v(juce::Colour(0x00000000), fw*0.84f, 0.f, juce::Colour(0x40000000), fw, 0.f, false); g.setGradientFill(v); g.fillRect(0,0,w,h); }

    // Everything from here on is "main content" — when the docked EQ panel
    // is open it is pushed right by kEqPanelW (the panel itself is a child
    // component and paints on top of whatever falls under it, so nothing
    // needs to change for x < kEqPanelW; content further right must be
    // translated to stay aligned with the components resized() has moved).
    const int ox = eqPanelVisible ? kEqPanelW : 0;
    g.saveState();
    if (ox != 0) g.addTransform(juce::AffineTransform::translation(float(ox), 0.0f));

    // Header
    {
        juce::ColourGradient hb(juce::Colour(0xff272725), 0.f, 0.f,
                                juce::Colour(0xff191917), 0.f, float(kTitleH), false);
        g.setGradientFill(hb); g.fillRect(0, 0, kWidth, kTitleH);
    }
    g.setColour(Theme::text.withAlpha(0.05f)); g.fillRect(0, 0, kWidth, 1);

    // Wordmark: 2210 x 632 artwork, so 40 px tall renders proportionally wide
    drawLogo(g, juce::Rectangle<float>(12.0f, 10.0f, 120.0f, 40.0f));

    // MIX read-out — cshift mirrors resized()'s mixKnob shift so this stays
    // aligned with it when Curve/GR is collapsed.
    const int cshift = curveGrVisible ? 0 : kCurveGrColW;
    {
        const int   labelX = kWidth - cshift - kMixSz - 70;
        const float mixVal = audioProcessor.apvts.getRawParameterValue("mix")->load() * 100.0f;
        g.setFont(Theme::label(15.0f));
        g.setColour(Theme::text.withAlpha(0.85f));
        g.drawText("MIX", labelX, 8, 62, 19, juce::Justification::centred, false);
        g.setFont(Theme::mono(17.0f, juce::Font::bold));
        g.setColour(Theme::accent);
        g.drawText(juce::String(int(mixVal)) + "%", labelX, 29, 62, 21,
                   juce::Justification::centred, false);
    }

    // Preset strip
    {
        g.setColour(Theme::bgDeep);
        g.fillRect(0, kTitleH, kWidth, kStripH);
        g.setColour(Theme::accent.withAlpha(0.55f));
        g.fillRect(0, kTitleH, kWidth, 1);
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.fillRect(0, kHeadH - 1, kWidth, 1);

        g.setFont(Theme::label(13.0f));
        g.setColour(Theme::textDim);
        g.drawText("MODE",   50,  kTitleH + 4, 104, 14, juce::Justification::centredLeft, false);
        g.drawText("PRESET", 182, kTitleH + 4, 140, 14, juce::Justification::centredLeft, false);
    }

    // Waveform seat
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawRect(3, kWaveY - 2, kContentW - 6, kWaveH + 4, 1);

    // Controls panel (no header tab)
    drawTabPanel(g, juce::Rectangle<int>(3, kCtrlY, kContentW - 6, kCtrlH - 4), {});

    // Band-selector row — a colored ring (matching EqPanel::kNodeColours)
    // frames each enabled node's button, inset within the ring so it reads
    // as an outline (the button itself is a child component painted after
    // this and would otherwise cover a same-size frame entirely). Brighter
    // for the node currently selected for Attack/Release/Q editing;
    // dimmer-highlighted for whichever linked node is dominant and driving
    // the compressor's detector.
    {
        const int domBand = audioProcessor.activeEqBand.load(std::memory_order_relaxed);
        const int rowX = kFaderM, rowY = kCtrlY + 4, rowH = kCtrlTopStripH - 8;
        const int rowW = kContentW - 2 * kFaderM, gap = 3;
        const int btnW = (rowW - (kMaxEqNodes - 1) * gap) / kMaxEqNodes;

        int visibleCount = 0;
        for (int i = 0; i < kMaxEqNodes; ++i)
            if (bandButtons[size_t(i)].isVisible()) ++visibleCount;
        const int totalW = visibleCount > 0 ? visibleCount * btnW + (visibleCount - 1) * gap : 0;
        const int startX = rowX + (rowW - totalW) / 2;

        int slot = 0;
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            if (!bandButtons[size_t(i)].isVisible()) continue;
            const juce::Rectangle<float> b(float(startX + slot * (btnW + gap)), float(rowY),
                                           float(btnW), float(rowH));
            ++slot;
            const auto colour = EqPanel::kNodeColours[i];
            const bool sel = (i == selectedBand), dom = (i == domBand);
            if (sel || dom)
            {
                g.setColour(colour.withAlpha(sel ? 0.20f : 0.12f));
                g.fillRoundedRectangle(b, 3.0f);
            }
            g.setColour(colour.withAlpha(sel ? 0.95f : (dom ? 0.75f : 0.45f)));
            g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, sel ? 1.6f : 1.1f);
        }
    }

    // Knob dividers
    for (int sx : { kSlotKneeX, kSlotRatioX, kSlotAttackX, kSlotReleaseX })
    {
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(sx, kCtrlY + kCtrlTopStripH + 4, 1, kCtrlH - kCtrlTopStripH - 18);
        g.setColour(Theme::line.withAlpha(0.55f));
        g.fillRect(sx + 1, kCtrlY + kCtrlTopStripH + 4, 1, kCtrlH - kCtrlTopStripH - 18);
    }

    // Fader column dividers
    {
        const int faderRightEdge = kFaderM + kFaderW;
        const int gainOutX       = kContentW - kFaderM - kFaderW;
        const int dy = kCtrlY + kCtrlTopStripH + 4, dh = kCtrlH - kCtrlTopStripH - 18;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(faderRightEdge, dy, 1, dh);
        g.fillRect(gainOutX - 1,   dy, 1, dh);
        g.setColour(Theme::line.withAlpha(0.55f));
        g.fillRect(faderRightEdge + 1, dy, 1, dh);
        g.fillRect(gainOutX,           dy, 1, dh);
    }

    // Parameter position bars
    {
        auto drawBar = [&g](int x, int w, float norm)
        {
            const float bx = float(x) + 12.0f;
            const float bw = float(w) - 24.0f;
            const float by = float(kCtrlY + kCtrlH - 20);
            g.setColour(Theme::accentDeep);
            g.fillRect(bx, by, bw, 3.0f);
            g.setColour(Theme::accent.withAlpha(0.30f));
            g.fillRect(bx, by - 1.0f, bw * norm, 5.0f);
            g.setColour(Theme::accent);
            g.fillRect(bx, by, bw * norm, 3.0f);
        };

        const bool bandCtx = selectedBand >= 0;
        DragSlider& thr = bandCtx ? bandThresholdKnob : thresholdKnob;
        DragSlider& kn  = bandCtx ? bandKneeKnob      : kneeKnob;
        DragSlider& rat = bandCtx ? bandRatioKnob     : ratioKnob;
        DragSlider& atk = bandCtx ? bandAttackKnob    : attackKnob;
        DragSlider& rel = bandCtx ? bandReleaseKnob   : releaseKnob;

        // Matches the knob's own outer accent ring exactly — both read from
        // valueToProportionOfLength(), the same figure JUCE uses to place
        // that ring — so the bar is a miniature of the ring, never disagreeing
        // with it, regardless of a knob's skew or (for Threshold in multiband
        // mode) its 0dB-centred range.
        drawBar(kSlotThresholdX, kSlotThresholdW, float(thr.valueToProportionOfLength(thr.getValue())));
        drawBar(kSlotKneeX,      kSlotKneeW,      float(kn.valueToProportionOfLength(kn.getValue())));
        drawBar(kSlotRatioX,     kSlotRatioW,     float(rat.valueToProportionOfLength(rat.getValue())));
        drawBar(kSlotAttackX,  kSlotAttackW,  float(atk.valueToProportionOfLength(atk.getValue())));
        drawBar(kSlotReleaseX, kSlotReleaseW, float(rel.valueToProportionOfLength(rel.getValue())));
    }

    // Right column separators
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.fillRect(kContentW, kHeadH, 1, h - kHeadH);
    g.setColour(Theme::line.withAlpha(0.6f));
    g.fillRect(kContentW + 1, kHeadH, 1, h - kHeadH);

    // The Curve/GR column's internal dividers only exist while it's expanded
    // — when collapsed, the level meter sits directly against the content
    // edge (already drawn above) and there's nothing else to separate.
    if (curveGrVisible)
    {
        g.setColour(juce::Colours::black.withAlpha(0.6f));
        g.fillRect(kContentW, kCurveY - 1, kRightColW, 1);
        g.setColour(Theme::line.withAlpha(0.6f));
        g.fillRect(kContentW, kCurveY, kRightColW, 1);

        // Divider between the GR meter / transfer curve and the level meter strip
        const int meterStripX = kContentW + 2 + kCurveGrColW;
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRect(meterStripX, kHeadH, 1, h - kHeadH);
        g.setColour(Theme::line.withAlpha(0.5f));
        g.fillRect(meterStripX + 1, kHeadH, 1, h - kHeadH);
    }

    g.restoreState();
}

//==============================================================================
// Resized
//==============================================================================

void VisualCompEditor::resized()
{
    // Docked EQ panel always sits flush left; everything else ("main
    // content") is pushed right by its width while it's open.
    const int ox = eqPanelVisible ? kEqPanelW : 0;
    if (eqPanel != nullptr)
        eqPanel->setBounds(0, 0, kEqPanelW, kHeight);

    // Header row spans the full width, including the space above the right
    // column — so its right-anchored controls (unlike everything else,
    // which is anchored to kContentW/left) need to shift left by the same
    // amount the window itself shrinks when Curve/GR is collapsed, or
    // they'd hang off the now-narrower right edge. paint()'s MIX read-out
    // and sample-rate text mirror this with the same `cshift`.
    const int cshift = curveGrVisible ? 0 : kCurveGrColW;
    mixKnob.setBounds(ox + kWidth - cshift - kMixSz - 8, 6, kMixSz, kMixSz);
    bypassButton.setBounds(ox + kWidth - cshift - kMixSz - 70 - 104, 14, 100, kTitleH - 28);
    clipModeButton.setBounds(ox + kWidth - cshift - kMixSz - 70 - 104 - 100 - 8, 14, 96, kTitleH - 28);
    logoZone.setBounds(ox + 10, 8, 112, kTitleH - 16);   // matches the drawn wordmark

    // Preset strip — EQ toggle sits directly to the left of the Mode button
    {
        const int sy = kTitleH + 20, sh = 24;
        eqButton         .setBounds(ox + 10,  sy, 36,  sh);
        modeButton       .setBounds(ox + 50,  sy, 104, sh);
        presetPrev       .setBounds(ox + 158, sy, 20,  sh);
        presetButton     .setBounds(ox + 182, sy, 250, sh);
        presetNext       .setBounds(ox + 436, sy, 20,  sh);
        presetSave       .setBounds(ox + 460, sy, 54,  sh);
        // Width trimmed to ~half its old side padding around the text.
        autoAnalyzeButton.setBounds(ox + 518, sy, kSmartMasterW, sh);
        // Collapsed: the right-anchored formula below assumes the expanded
        // window width and overlaps Smart Master+ once the Curve/GR column
        // is gone, so dock it right after that button instead (5px gap).
        // The same 5px is mirrored past Curve/GR's own right edge (see
        // totalEditorWidth()), so it isn't left flush against the window edge.
        if (curveGrVisible)
            curveGrButton.setBounds(ox + kWidth - cshift - 216, sy, 100, sh);
        else
            curveGrButton.setBounds(ox + 518 + kSmartMasterW + 5, sy, 100, sh);
    }

    // Waveforms
    const int waveW = (kContentW - 14) / 2;
    inputDisplay .setBounds(ox + 5,         kWaveY, waveW, kWaveH);
    outputDisplay.setBounds(ox + waveW + 9, kWaveY, waveW, kWaveH);

    // Right column — vuMeter/curveDisplay always keep the same (expanded)
    // geometry; only levelMeter's x-position and the total window width
    // (see totalEditorWidth()) change with curveGrVisible, so when
    // collapsed that column simply sits outside the now-narrower window.
    const int levelMeterX = ox + kContentW + 2 + (curveGrVisible ? kCurveGrColW + 4 : 0);
    vuMeter     .setBounds(ox + kContentW + 2, kHeadH,      kCurveGrColW, kVUH);
    curveDisplay.setBounds(ox + kContentW + 2, kCurveY + 1, kCurveGrColW, kCurveH - 1);
    levelMeter  .setBounds(levelMeterX, kHeadH, kLevelMeterW - 4, kVUH + kCurveH - 1);

    // Controls
    constexpr int textBoxH = 24;

    // Gain In
    {
        const int fx = ox + kFaderM;
        gainInFaderLabel.setBounds(fx, kCtrlY + kKnobRowY, kFaderW, kKnobLblH);
        gainInFader.setBounds(fx, kCtrlY + kKnobRowY + kKnobLblH + 2, kFaderW,
                              kCtrlH - kKnobRowY - kKnobLblH - 28);
    }

    // Band-selector row — spans the full Dynamics-pane width, directly
    // above the Gain In / Threshold / Knee / Ratio / Attack / Release /
    // Gain Out labels. Buttons are sized as if all kMaxEqNodes slots were
    // in play, but only the enabled ones are laid out, centered as a
    // contiguous group — so with e.g. 3 active bands the row reads as
    // "1 2 3" in the middle rather than left-packed with a dead gap on the
    // right. Each button sits inset within the colored ring drawn in
    // paint() (identical geometry, computed without `ox` there since
    // paint()'s content block is already translated by the transform).
    {
        const int rowX = ox + kFaderM, rowY = kCtrlY + 4, rowH = kCtrlTopStripH - 8;
        const int rowW = kContentW - 2 * kFaderM, gap = 3;
        const int btnW = (rowW - (kMaxEqNodes - 1) * gap) / kMaxEqNodes;

        int visibleCount = 0;
        for (int i = 0; i < kMaxEqNodes; ++i)
            if (bandButtons[size_t(i)].isVisible()) ++visibleCount;

        const int totalW = visibleCount > 0 ? visibleCount * btnW + (visibleCount - 1) * gap : 0;
        const int startX = rowX + (rowW - totalW) / 2;

        int slot = 0;
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            if (!bandButtons[size_t(i)].isVisible()) continue;
            const int bx = startX + slot * (btnW + gap);
            bandButtons[size_t(i)].setBounds(bx + 2, rowY + 2, btnW - 4, rowH - 4);
            ++slot;
        }
    }

    // Knobs — the component fills the whole slot so a drag anywhere works;
    // the label sits on top of it and passes clicks through. Attack/Release
    // and their band-context counterparts share a slot (only one of each
    // pair is visible at a time — see refreshBandButtons). All five knob
    // slots are always full-width now (Q used to narrow Attack/Release to
    // share this row; it moved to node-level editing — see EqPanel and
    // NodeIsland), so setupKnob/setupBandKnob's own initial text-box sizing
    // already matches and doesn't need re-applying here.
    const int knobTop = kCtrlY + kKnobRowY;
    const int knobH   = kCtrlH - kKnobRowY - 28;

    auto placeKnob = [&](DragSlider& knob, int x, int w)
    {
        knob.setBounds(ox + x + 1, knobTop, w - 2, knobH);
    };
    auto placeLabel = [&](juce::Label& label, int x, int w)
    {
        label.setBounds(ox + x, knobTop, w, kKnobLblH);
        label.toFront(false);
    };

    placeKnob(thresholdKnob,     kSlotThresholdX, kSlotThresholdW);
    placeKnob(bandThresholdKnob, kSlotThresholdX, kSlotThresholdW);
    placeKnob(kneeKnob,      kSlotKneeX,      kSlotKneeW);
    placeKnob(bandKneeKnob,  kSlotKneeX,      kSlotKneeW);
    placeKnob(ratioKnob,     kSlotRatioX,     kSlotRatioW);
    placeKnob(bandRatioKnob, kSlotRatioX,     kSlotRatioW);
    placeKnob(attackKnob,     kSlotAttackX,  kSlotAttackW);
    placeKnob(bandAttackKnob, kSlotAttackX,  kSlotAttackW);
    placeKnob(releaseKnob,     kSlotReleaseX, kSlotReleaseW);
    placeKnob(bandReleaseKnob, kSlotReleaseX, kSlotReleaseW);

    placeLabel(thresholdLabel, kSlotThresholdX, kSlotThresholdW);
    placeLabel(kneeLabel,      kSlotKneeX,      kSlotKneeW);
    placeLabel(ratioLabel,     kSlotRatioX,     kSlotRatioW);
    placeLabel(attackLabel,    kSlotAttackX,    kSlotAttackW);
    placeLabel(releaseLabel,   kSlotReleaseX,   kSlotReleaseW);

    // Gain Out + toggles
    {
        const int fx = ox + kContentW - kFaderM - kFaderW;
        gainOutFaderLabel.setBounds(fx, kCtrlY + kKnobRowY, kFaderW, kKnobLblH);

        constexpr int btnH   = 24;
        constexpr int agBtnH = 32;
        constexpr int gap    = 3;
        constexpr int miniH  = 14;

        const int bottomH  = agBtnH + gap + btnH + miniH + gap + btnH + miniH + 4;
        const int faderTop = kCtrlY + kKnobRowY + kKnobLblH + 2;
        const int faderH   = kCtrlH - kKnobRowY - kKnobLblH - 8 - bottomH - gap - textBoxH;

        gainOutFader.setBounds(fx, faderTop, kFaderW, faderH + textBoxH);

        int agY = faderTop + faderH + textBoxH + gap;
        autoGainButton.setBounds(fx + 4, agY, kFaderW - 8, agBtnH);

        int limY = agY + agBtnH + gap;
        limiterButton.setBounds(fx + 4, limY, kFaderW - 8, btnH);
        limiterLabel .setBounds(fx, limY + btnH, kFaderW, miniH);

        int scY = limY + btnH + miniH + gap;
        sidechainButton.setBounds(fx + 4, scY, kFaderW - 8, btnH);
        sidechainLabel .setBounds(fx, scY + btnH, kFaderW, miniH);
    }

    helpOverlay.setBounds(getLocalBounds());
}
