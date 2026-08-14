#include "PluginEditor.h"
#include "Presets.h"
#include "Theme.h"
#include "LogoSvg.h"
#include "UndoableParameterAction.h"

namespace
{
    constexpr int kWidth  = 960;
    constexpr int kHeight = 648;       // +28 vs. the previous build, for the
                                       // multiband-indicator strip in the Dynamics pane

    constexpr int kTitleH = 60;
    constexpr int kStripH = 48;
    constexpr int kHeadH  = kTitleH + kStripH;        // 108

    constexpr int kRightColW   = 280;
    constexpr int kLevelMeterW = 77;                  // reserved for the dB/LUFS meter strip
                                                       // (bars + their side labels, see LevelMeter::paint)
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

    // Shared 7-band tonal-balance split used by both
    // VisualCompEditor::analyzeSmartMasterCapture (measuring the captured
    // excerpt) and runAutoAnalyze (building the correction EQ nodes) so the
    // two never drift out of sync: sub-bass, bass, low-mid, mid, high-mid,
    // presence, air.
    constexpr int   kNumSmartBands = 7;   // must match VisualCompEditor::SmartMasterAnalysis::kNumBands
    constexpr float kSmartBandLo[kNumSmartBands] =
        {   20.0f,  90.0f, 150.0f,  400.0f, 2000.0f, 4000.0f, 8000.0f };
    constexpr float kSmartBandHi[kNumSmartBands] =
        {   90.0f, 150.0f, 400.0f, 2000.0f, 4000.0f, 8000.0f, 20000.0f };
    // Indices (into the band arrays above) given a live multiband dynamics
    // band in the generated master, on top of the static tilt-matching EQ
    // every band gets: gentle low-end glue on the bass band, and a de-esser/
    // harshness tamer on the presence band -- the two moves that show up in
    // almost every genre's mastering-chain guidance.
    constexpr int kSmartBassBand     = 1;
    constexpr int kSmartPresenceBand = 5;
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
// WaveEnlargeOverlay
//==============================================================================

WaveEnlargeOverlay::WaveEnlargeOverlay(VisualCompProcessor& proc, bool showInput, bool showOutput)
{
    // Fresh WaveformDisplay instances reading the same live buffers as the
    // normal-size ones -- own independent pause state, so pausing the small
    // input display and pausing this enlarged one are unrelated. bigInput
    // mirrors inputDisplay's full dynamics-aware constructor args (threshold/
    // sidechain/ratio/knee/attack/release) so its ducking-curve overlay and
    // click-to-pause behave identically; bigOutput mirrors outputDisplay's
    // plain 3-arg form.
    if (showInput)
    {
        bigInput = std::make_unique<WaveformDisplay>(
            "INPUT", Theme::inputCol, proc.inputWaveform,
            proc.apvts.getRawParameterValue("threshold"),
            &proc.sidechainWaveform, &proc.sidechainEnabled,
            proc.apvts.getRawParameterValue("ratio"),
            proc.apvts.getRawParameterValue("knee"),
            proc.apvts.getRawParameterValue("attack"),
            proc.apvts.getRawParameterValue("release"), true);
        addAndMakeVisible(*bigInput);
    }
    if (showOutput)
    {
        bigOutput = std::make_unique<WaveformDisplay>("OUTPUT", Theme::outputCol, proc.outputWaveform);
        addAndMakeVisible(*bigOutput);
    }

    closeButton.setColour(juce::TextButton::buttonColourId, Theme::charcoal);
    closeButton.setColour(juce::TextButton::textColourOffId, Theme::text);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    setAlwaysOnTop(true);
}

void WaveEnlargeOverlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.76f));

    auto drawCard = [&](juce::Component& c)
    {
        auto b = c.getBounds().expanded(4);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(b.toFloat().translated(0.0f, 3.0f), 6.0f);
        g.setColour(Theme::bgDeep);
        g.fillRoundedRectangle(b.toFloat(), 6.0f);
        g.setColour(Theme::line);
        g.drawRoundedRectangle(b.toFloat(), 6.0f, 1.2f);
    };
    if (bigInput)  drawCard(*bigInput);
    if (bigOutput) drawCard(*bigOutput);
}

void WaveEnlargeOverlay::resized()
{
    constexpr int margin = 40, gap = 20, closeH = 30, closeW = 100;
    auto area = getLocalBounds().reduced(margin);
    area.removeFromBottom(gap + closeH);

    const bool both = (bigInput != nullptr && bigOutput != nullptr);
    if (both)
    {
        auto left  = area.removeFromLeft(area.getWidth() / 2 - gap / 2);
        area.removeFromLeft(gap);
        bigInput->setBounds(left.reduced(4));
        bigOutput->setBounds(area.reduced(4));
    }
    else if (bigInput)
    {
        bigInput->setBounds(area.reduced(4));
    }
    else if (bigOutput)
    {
        bigOutput->setBounds(area.reduced(4));
    }

    closeButton.setBounds(getLocalBounds().getCentreX() - closeW / 2,
                           getHeight() - margin - closeH, closeW, closeH);
}

void WaveEnlargeOverlay::mouseUp(const juce::MouseEvent& e)
{
    // A click that lands on this component itself (not one of the child
    // WaveformDisplay/CLOSE-button components, which consume it first)
    // means the dimmed backdrop was clicked -- dismiss.
    if (e.eventComponent == this && onClose) onClose();
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

    // Machined bezel radii, declared up front so the graduation ticks below
    // can reach flush against the cog-wheel rim instead of floating short of
    // it with a visible gap.
    const float bezelOuter  = maxR * 0.72f;
    const float bezelValley = maxR * 0.645f;   // tooth root radius (grooves a finger catches on)
    const float bezelInner  = maxR * 0.60f;

    // Graduation ticks — elongated all the way in to the bezel rim (rather
    // than stopping short of it) so they read as one continuous dial face.
    {
        constexpr int N = 22;
        for (int i = 0; i <= N; ++i)
        {
            const float frac  = float(i) / float(N);
            const float angle = startAngle + frac * (endAngle - startAngle);
            const bool  isMaj = (i % 4 == 0) || (i == N);
            if (simpleTicks && !isMaj) continue;
            const float sa = std::sin(angle), ca = -std::cos(angle);
            g.setColour(isMaj ? Theme::text.withAlpha(0.75f)
                              : Theme::textDim.withAlpha(0.40f));
            g.drawLine(cx + bezelOuter * sa, cy + bezelOuter * ca,
                       cx + maxR * sa,       cy + maxR * ca,
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

    // Machined bezel — a cog-wheel/knurled rim instead of a plain disc, so
    // there's something for a finger to actually grip and turn, like a real
    // hardware compressor's knob. Same gradient/stroke colours as the old
    // plain-circle bezel; only the outline shape changed to alternate
    // between an outer tooth radius and an inner valley radius. (Radii
    // declared once, above, for the graduation ticks.)
    {
        constexpr int kTeeth = 18;
        juce::Path gear;
        for (int t = 0; t < kTeeth * 2; ++t)
        {
            // Offset by valueAngle so the tooth pattern turns with the knob,
            // like a real machined dial rather than a static ring the
            // pointer sweeps past.
            const float ang = (float(t) / float(kTeeth * 2)) * juce::MathConstants<float>::twoPi + valueAngle;
            const float rad = (t % 2 == 0) ? bezelOuter : bezelValley;
            const float sa = std::sin(ang), ca = -std::cos(ang);
            const juce::Point<float> pnt(cx + rad * sa, cy + rad * ca);
            if (t == 0) gear.startNewSubPath(pnt); else gear.lineTo(pnt);
        }
        gear.closeSubPath();

        juce::ColourGradient ch(
            juce::Colour(0xff54514c), cx - bezelOuter * 0.40f, cy - bezelOuter * 0.52f,
            juce::Colour(0xff121110), cx + bezelOuter * 0.40f, cy + bezelOuter * 0.52f, true);
        ch.addColour(0.22, juce::Colour(0xff6b675f));
        ch.addColour(0.45, juce::Colour(0xff232220));
        ch.addColour(0.70, juce::Colour(0xff43403b));
        g.setGradientFill(ch);
        g.fillPath(gear);

        g.setColour(juce::Colour(0xff8b857a).withAlpha(0.40f));
        g.strokePath(gear, juce::PathStrokeType(0.7f));
        g.setColour(juce::Colour(0xff000000).withAlpha(0.75f));
        g.strokePath(gear, juce::PathStrokeType(0.9f));

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

    // Pointer — stretched all the way out to the cogwheel rim's outer tooth
    // radius (rather than stopping short inside the body) so it reads as one
    // continuous needle against the bezel; no accent-coloured halo behind it,
    // just a plain drop shadow under the white line/dot.
    {
        const float sa = std::sin(valueAngle), ca = -std::cos(valueAngle);
        const float r0 = bodyR * 0.30f, r1 = bezelOuter;

        g.setColour(juce::Colour(0x70000000));
        g.drawLine(cx + r0 * sa + 0.8f, cy + r0 * ca + 1.0f,
                   cx + r1 * sa + 0.8f, cy + r1 * ca + 1.0f, 2.4f);
        g.setColour(juce::Colours::white);
        g.drawLine(cx + r0 * sa, cy + r0 * ca, cx + r1 * sa, cy + r1 * ca, 1.9f);

        const float dotR = juce::jlimit(1.8f, 3.4f, bodyR * 0.09f);
        const float dx = cx + r1 * sa;
        const float dy = cy + r1 * ca;
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

    // JUCE's own sliderPos spans the full [y, y+height] region passed in
    // here, but the track (and the cap below) are inset 8px top/bottom —
    // clamp once so the accent fill can never reach past where the cap
    // itself is able to travel.
    const float clampedPos = juce::jlimit(trackT, trackB, sliderPos);

    // Accent fill from unity to the handle
    {
        const float norm0 = juce::jlimit(0.0f, 1.0f, (0.0f - rangeMin) / (rangeMax - rangeMin));
        const float zeroY = trackT + trackH * (1.0f - norm0);
        const float top   = juce::jmin(zeroY, clampedPos);
        const float bot   = juce::jmax(zeroY, clampedPos);
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
    const float capY = clampedPos - capH * 0.5f;
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
                    p.apvts.getRawParameterValue("release"), true),
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

    // Initialize license/demo mode system
    audioProcessor.initializeLicenseManager();

    // Wire undo/redo into parameter knobs
    gainInFader.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "gainIn");
    gainOutFader.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "gainOut");
    thresholdKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "threshold");
    kneeKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "knee");
    ratioKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "ratio");
    attackKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "attack");
    releaseKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "release");
    mixKnob.setUndoManagerAndParamId(&audioProcessor.undoRedoManager, &audioProcessor.apvts, "mix");

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
    presetButton.setTooltip(audioProcessor.currentPresetAuthor.isNotEmpty()
        ? ("Preset by " + audioProcessor.currentPresetAuthor) : juce::String());
    presetButton.onClick = [this] { showPresetMenu(); };
    setupTextButton(presetPrev, juce::String::charToString(0x2039));
    presetPrev.onClick = [this] { stepPreset(-1); };
    setupTextButton(presetNext, juce::String::charToString(0x203a));
    presetNext.onClick = [this] { stepPreset(1); };
    setupTextButton(presetSave, "SAVE");
    presetSave.onClick = [this] { saveUserPreset(); };

    // Preset Author — free text, always visible in presetButton's old strip
    // slot; kept in sync with audioProcessor.currentPresetAuthor both ways
    // (see setPresetAuthor()).
    presetAuthorEditor.setFont(Theme::label(13.0f, juce::Font::plain));
    presetAuthorEditor.setColour(juce::TextEditor::backgroundColourId, Theme::bgDeep);
    presetAuthorEditor.setColour(juce::TextEditor::textColourId,       Theme::text);
    presetAuthorEditor.setColour(juce::TextEditor::outlineColourId,    Theme::line);
    presetAuthorEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    presetAuthorEditor.setColour(juce::TextEditor::highlightColourId,  Theme::accentDim);
    presetAuthorEditor.setColour(juce::CaretComponent::caretColourId,  Theme::accent);
    presetAuthorEditor.setJustification(juce::Justification::centredLeft);
    presetAuthorEditor.setTextToShowWhenEmpty("Author", Theme::textDim);
    presetAuthorEditor.setText(audioProcessor.currentPresetAuthor, false);
    presetAuthorEditor.setSelectAllWhenFocused(true);
    presetAuthorEditor.onFocusLost = [this] { setPresetAuthor(presetAuthorEditor.getText().trim()); };
    presetAuthorEditor.onReturnKey = [this]
    {
        setPresetAuthor(presetAuthorEditor.getText().trim());
        presetAuthorEditor.giveAwayKeyboardFocus();
    };
    addAndMakeVisible(presetAuthorEditor);

    // MB toggle (opens the docked parametric EQ panel, always multiband now)
    setupTextButton(eqButton, "MB");
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
    setupTextButton(curveGrButton, "GR");
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
    // Graph drags (incl. the "T" threshold marker) and Dynamic Island edits
    // both land here so the Dynamics-pane progress bars (which read straight
    // off bandThresholdKnob etc., not off the EQ panel) repaint immediately
    // instead of waiting for the 30Hz timerCallback() poll below.
    eqPanel->onNodeEdited = [this](int i)
    {
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        if (i == selectedBand) refreshBandButtons();
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

    // Demo mode watermark indicator
    addAndMakeVisible(demoModeIndicator);

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
    // FabFilter Pro-MB style: Threshold is downward-only (0..-60dB). Direction
    // (downward/upward) and how far the band can swing now live on the Range
    // knob instead (see NodeIsland) — that's what used to make a positive
    // Threshold "auto-engage" upward and feel weird turning through the
    // crossover; Threshold no longer carries that dual role. Range/skew below
    // get overridden immediately after by setupThresholdKnobRange() — see
    // EqEngine.h.
    setupBandKnob(bandThresholdKnob, -60.0f, 0.0f, 0.5f, -20.0f);
    setupThresholdKnobRange(bandThresholdKnob);
    setupBandKnob(bandKneeKnob,        0.0f, 20.0f, 1.0f,   6.0f);
    setupBandKnob(bandRatioKnob,       1.0f, 20.0f, 0.4f,   2.0f);
    bandAttackKnob.setTextValueSuffix(" ms");
    bandReleaseKnob.setTextValueSuffix(" ms");
    bandThresholdKnob.setTextValueSuffix(" dB");
    bandKneeKnob.setTextValueSuffix(" dB");
    bandRatioKnob.setTextValueSuffix(":1");
    bandAttackKnob.onMouseDownCallback = [this]
    {
        if (selectedBand >= 0)
            oldBandNodeStateAtMouseDown = audioProcessor.eq.getNode(selectedBand);
    };
    bandAttackKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.attackMs = float(bandAttackKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandAttackKnob.onUndoEditComplete = [this](float oldValue, float newValue)
    {
        if (selectedBand < 0) return;
        if (std::abs(newValue - oldValue) < 1e-5f) return;
        auto newState = audioProcessor.eq.getNode(selectedBand);
        auto action = std::make_unique<VisualCompUndo::UndoableEqNodeEdit>(
            audioProcessor.eq, selectedBand, newState, oldBandNodeStateAtMouseDown);
        audioProcessor.undoRedoManager.perform(std::move(action));
    };
    bandReleaseKnob.onMouseDownCallback = [this]
    {
        if (selectedBand >= 0)
            oldBandNodeStateAtMouseDown = audioProcessor.eq.getNode(selectedBand);
    };
    bandReleaseKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.releaseMs = float(bandReleaseKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandReleaseKnob.onUndoEditComplete = [this](float oldValue, float newValue)
    {
        if (selectedBand < 0) return;
        if (std::abs(newValue - oldValue) < 1e-5f) return;
        auto newState = audioProcessor.eq.getNode(selectedBand);
        auto action = std::make_unique<VisualCompUndo::UndoableEqNodeEdit>(
            audioProcessor.eq, selectedBand, newState, oldBandNodeStateAtMouseDown);
        audioProcessor.undoRedoManager.perform(std::move(action));
    };
    bandThresholdKnob.onMouseDownCallback = [this]
    {
        if (selectedBand >= 0)
            oldBandNodeStateAtMouseDown = audioProcessor.eq.getNode(selectedBand);
    };
    bandThresholdKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.thresholdDb = float(bandThresholdKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandThresholdKnob.onUndoEditComplete = [this](float oldValue, float newValue)
    {
        if (selectedBand < 0) return;
        if (std::abs(newValue - oldValue) < 1e-5f) return;
        auto newState = audioProcessor.eq.getNode(selectedBand);
        auto action = std::make_unique<VisualCompUndo::UndoableEqNodeEdit>(
            audioProcessor.eq, selectedBand, newState, oldBandNodeStateAtMouseDown);
        audioProcessor.undoRedoManager.perform(std::move(action));
    };
    bandKneeKnob.onMouseDownCallback = [this]
    {
        if (selectedBand >= 0)
            oldBandNodeStateAtMouseDown = audioProcessor.eq.getNode(selectedBand);
    };
    bandKneeKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.kneeDb = float(bandKneeKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandKneeKnob.onUndoEditComplete = [this](float oldValue, float newValue)
    {
        if (selectedBand < 0) return;
        if (std::abs(newValue - oldValue) < 1e-5f) return;
        auto newState = audioProcessor.eq.getNode(selectedBand);
        auto action = std::make_unique<VisualCompUndo::UndoableEqNodeEdit>(
            audioProcessor.eq, selectedBand, newState, oldBandNodeStateAtMouseDown);
        audioProcessor.undoRedoManager.perform(std::move(action));
    };
    bandRatioKnob.onMouseDownCallback = [this]
    {
        if (selectedBand >= 0)
            oldBandNodeStateAtMouseDown = audioProcessor.eq.getNode(selectedBand);
    };
    bandRatioKnob.onValueChange = [this]
    {
        if (selectedBand < 0) return;
        auto n = audioProcessor.eq.getNode(selectedBand);
        n.ratio = float(bandRatioKnob.getValue());
        audioProcessor.eq.setNode(selectedBand, n);
        audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
        repaint(0, kCtrlY, kContentW, kCtrlH);
    };
    bandRatioKnob.onUndoEditComplete = [this](float oldValue, float newValue)
    {
        if (selectedBand < 0) return;
        if (std::abs(newValue - oldValue) < 1e-5f) return;
        auto newState = audioProcessor.eq.getNode(selectedBand);
        auto action = std::make_unique<VisualCompUndo::UndoableEqNodeEdit>(
            audioProcessor.eq, selectedBand, newState, oldBandNodeStateAtMouseDown);
        audioProcessor.undoRedoManager.perform(std::move(action));
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

    inputDisplay.onRightClick  = [this](const juce::MouseEvent&) { showEnlargeMenu(inputDisplay, true); };
    outputDisplay.onRightClick = [this](const juce::MouseEvent&) { showEnlargeMenu(outputDisplay, false); };

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
    curveGrButton.setButtonText("GR " + juce::String::charToString(curveGrVisible ? 0x2039 : 0x203a));
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

    // Click-anywhere-to-deselect (see mouseDown()): registering on `this`
    // with nested==true reports mouse-down events from every descendant
    // component too, without stealing them from whatever normally handles
    // the click.
    addMouseListener(this, true);

    // Show license activation dialog if trial is expiring soon (7 days or less)
    if (audioProcessor.licenseManager.shouldShowLicenseOnStartup())
    {
        juce::Component::SafePointer<VisualCompEditor> safe(this);
        juce::Timer::callAfterDelay(300, [safe]
        {
            if (safe != nullptr)
                safe->demoModeIndicator.showLicenseActivationDialog();
        });
    }

    // First run: show the tour once the window has settled
    if (!hasSeenHelp())
    {
        juce::Component::SafePointer<VisualCompEditor> safe(this);
        juce::Timer::callAfterDelay(500, [safe]
        {
            if (safe != nullptr) safe->startHelpTour();
        });
    }

    // Documentation aid, same pattern as VC2_FORCE_EQ_OPEN: opens the
    // "Save Preset" author-name prompt on launch so it can be screenshotted
    // reproducibly (synthetic clicks don't reach the window on Windows).
    // Inert unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_SAVE_PRESET_DIALOG", {}).isNotEmpty())
    {
        juce::Component::SafePointer<VisualCompEditor> safe(this);
        juce::Timer::callAfterDelay(700, [safe]
        {
            if (safe != nullptr) safe->saveUserPreset();
        });
    }

    // Documentation aid, same pattern as VC2_FORCE_SAVE_PRESET_DIALOG: marks
    // the EQ dirty and pops the "Replace your EQ?" confirm dialog on launch
    // so it can be screenshotted reproducibly. Inert unless set.
    if (juce::SystemStats::getEnvironmentVariable("VC2_FORCE_EQ_CONFIRM_DIALOG", {}).isNotEmpty())
    {
        juce::Component::SafePointer<VisualCompEditor> safe(this);
        juce::Timer::callAfterDelay(700, [safe]
        {
            if (safe == nullptr) return;
            safe->audioProcessor.eqDirtySincePreset.store(true, std::memory_order_relaxed);
            safe->confirmAndApplyEq([] {});
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
    if (waitingForSmartMasterCapture) pollSmartMasterCapture();
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
    // Arrow points the direction the click will move it: right (closed, "expand
    // this way") when hidden, left ("collapse back") when open.
    curveGrButton.setButtonText("GR " + juce::String::charToString(curveGrVisible ? 0x2039 : 0x203a));
    vuMeter.setVisible(curveGrVisible);
    curveDisplay.setVisible(curveGrVisible);
    setSize(totalEditorWidth(), kHeight);
    resized();
    repaint();
}

//==============================================================================
// Band-selector row (Attack/Release context switching)
//==============================================================================

void VisualCompEditor::mouseDown(const juce::MouseEvent& e)
{
    if (selectedBand < 0) return;   // already showing the default compressor -- nothing to do

    // Walk up from the actual clicked component. Anything inside the EQ
    // panel (graph + Dynamic Island) manages selection itself already; the
    // band-context knobs and the numbered selector buttons are editing/
    // choosing the current selection, not abandoning it -- deselecting on
    // mouseDown for those would cancel a drag or fight selectBand()'s own
    // toggle-off logic (which runs on mouseUp). Everything else -- faders,
    // header buttons, waveform displays, blank space -- drops back to the
    // default broadband compressor knobs.
    for (auto* c = e.originalComponent; c != nullptr; c = c->getParentComponent())
    {
        if (c == eqPanel.get()) return;
        if (c == &bandThresholdKnob || c == &bandKneeKnob || c == &bandRatioKnob
            || c == &bandAttackKnob || c == &bandReleaseKnob) return;
        for (auto& b : bandButtons) if (c == &b) return;
    }

    selectedBand = -1;
    if (eqPanel != nullptr) eqPanel->setSelectedNode(-1);
    refreshBandButtons();
}

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

// Author now has its own always-visible editable field in the preset strip
// (presetAuthorEditor, presetButton's old slot); the tooltip is kept too as
// a low-effort second surface. Guard against clobbering text the user is
// actively typing when this is called from elsewhere (preset load, factory
// preset apply) while the field happens to have focus.
void VisualCompEditor::setPresetAuthor(const juce::String& author)
{
    audioProcessor.currentPresetAuthor = author;
    presetButton.setTooltip(author.isNotEmpty() ? ("Preset by " + author) : juce::String());
    if (!presetAuthorEditor.hasKeyboardFocus(false) && presetAuthorEditor.getText() != author)
        presetAuthorEditor.setText(author, false);
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

void VisualCompEditor::showEnlargeMenu(WaveformDisplay& source, bool sourceIsInput)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel(&laf);
    menu.addItem(1, sourceIsInput ? "Enlarge Input Display" : "Enlarge Output Display");
    menu.addItem(2, "Enlarge Both Displays");

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(source)
                                                 .withMinimumWidth(200),
        [this, sourceIsInput](int result)
        {
            if (result == 1)      showEnlargeOverlay(sourceIsInput, !sourceIsInput);
            else if (result == 2) showEnlargeOverlay(true, true);
        });
}

void VisualCompEditor::showEnlargeOverlay(bool showInput, bool showOutput)
{
    if (!showInput && !showOutput) return;
    enlargeOverlay = std::make_unique<WaveEnlargeOverlay>(audioProcessor, showInput, showOutput);
    enlargeOverlay->onClose = [this] { enlargeOverlay.reset(); };
    addAndMakeVisible(*enlargeOverlay);
    enlargeOverlay->setBounds(getLocalBounds());
    enlargeOverlay->toFront(true);
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
    setPresetAuthor("Arnav Singh");   // all factory presets are credited to Arnav Singh

    // Factory presets carry no authored EQ curve of their own (see
    // FactoryPreset in Presets.h) -- treat them as a flat/empty EQ position,
    // same as any other preset "position" the user might switch to.
    confirmAndApplyEq([this]
    {
        for (int i = 0; i < kMaxEqNodes; ++i)
            audioProcessor.eq.setNode(i, EqNodeState{});
    });
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
    // Ask for an author first (blank input field, pre-filled with whatever
    // was typed last so a whole preset pack doesn't need retyping it every
    // save), then hand off to the file chooser for the name/location --
    // same AlertWindow-then-continue shape as the Auto-Analyze/Smart
    // Master+ wizard below, just with fewer steps.
    auto* aw = new juce::AlertWindow("Save Preset",
        "Credit yourself so producers sharing this pack know who made it (optional).",
        juce::AlertWindow::NoIcon);
    aw->setLookAndFeel(&laf);
    aw->addTextEditor("author", audioProcessor.currentPresetAuthor, "Author");
    aw->addButton("Next",   1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    wizardWindow.reset(aw);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this](int result)
        {
            if (result != 1 || wizardWindow == nullptr) { wizardWindow.reset(); return; }
            const juce::String author = wizardWindow->getTextEditorContents("author").trim();
            wizardWindow.reset();
            launchSavePresetFileChooser(author);
        }), false);
}

void VisualCompEditor::launchSavePresetFileChooser(const juce::String& author)
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Save preset", getUserPresetDir().getChildFile("My Preset.vcpreset"), "*.vcpreset");

    fileChooser->launchAsync(
        juce::FileBrowserComponent::saveMode
            | juce::FileBrowserComponent::canSelectFiles
            | juce::FileBrowserComponent::warnAboutOverwriting,
        [this, author](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file == juce::File{}) return;
            file = file.withFileExtension(".vcpreset");

            auto state = audioProcessor.apvts.copyState();
            state.setProperty("compMode",
                audioProcessor.compMode.load(std::memory_order_relaxed), nullptr);
            state.setProperty("clipMode",
                audioProcessor.clipMode.load(std::memory_order_relaxed), nullptr);
            state.setProperty("presetAuthor", author, nullptr);

            // EQ nodes -- mirrors VisualCompProcessor::getStateInformation's
            // eqN_ property write loop, so a saved preset also captures the
            // EQ curve, not just the compressor knobs.
            for (int i = 0; i < kMaxEqNodes; ++i)
            {
                const auto n = audioProcessor.eq.getNode(i);
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
                if (xml->writeTo(file))
                {
                    setPresetName(file.getFileNameWithoutExtension());
                    setPresetAuthor(author);
                }
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
            // Blank for presets saved before this field existed -- clears
            // any stale tooltip left over from whatever was loaded before.
            setPresetAuthor(state.getProperty("presetAuthor", "").toString());
            confirmAndApplyEq([this, state] { applyEqFromState(state); });
        }
    }
}

void VisualCompEditor::confirmAndApplyEq(std::function<void()> applyFn,
                                          std::function<void()> onSettled)
{
    if (!audioProcessor.eqDirtySincePreset.load(std::memory_order_relaxed))
    {
        if (applyFn) applyFn();
        if (onSettled) onSettled();
        return;
    }

    // The live EQ has hand-edits since the last preset load/generation --
    // ask before silently discarding them. Same AlertWindow-then-continue
    // shape as saveUserPreset()'s author prompt.
    auto* aw = new juce::AlertWindow("Replace your EQ?",
        "This preset comes with its own EQ curve, but you've made changes to the "
        "EQ since the last preset was loaded or generated. Keep your current EQ, "
        "or replace it with the preset's?",
        juce::AlertWindow::QuestionIcon);
    aw->setLookAndFeel(&laf);
    aw->addButton("Replace EQ", 1, juce::KeyPress(juce::KeyPress::returnKey));
    aw->addButton("Keep Mine",  0, juce::KeyPress(juce::KeyPress::escapeKey));

    wizardWindow.reset(aw);
    aw->enterModalState(true, juce::ModalCallbackFunction::create(
        [this, applyFn, onSettled](int result)
        {
            wizardWindow.reset();
            audioProcessor.eqDirtySincePreset.store(false, std::memory_order_relaxed);
            if (result == 1 && applyFn) applyFn();
            if (onSettled) onSettled();
        }), false);
}

void VisualCompEditor::applyEqFromState(const juce::ValueTree& state)
{
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
        // Same clamp/fallback logic as PluginProcessor::setStateInformation
        // -- see its comment for why.
        n.thresholdDb = juce::jlimit(-60.0f, 0.0f,
                            float(state.getProperty(p + "thresholdDb", -20.0)));
        n.kneeDb      = float(state.getProperty(p + "kneeDb",        6.0));
        n.ratio       = float(state.getProperty(p + "ratio",         2.0));
        n.rangeDb     = float(state.getProperty(p + "rangeDb", n.upward ? 30.0 : -30.0));
        n.bwLowOct    = float(state.getProperty(p + "bwLowOct",      1.0));
        n.bwHighOct   = float(state.getProperty(p + "bwHighOct",     1.0));
        audioProcessor.eq.setNode(i, n);
    }
}

//==============================================================================
// Auto-Analyze / Smart Master+ wizard
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
    auto* aw = new juce::AlertWindow("Smart Master+ - Step 1 of 3",
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
        "Step 2 of 3 - target loudness in LUFS (approx). Typical masters run "
        "from about -18 LU (classical) to -6 LU (EDM/club).",
        juce::AlertWindow::NoIcon);
    aw->setLookAndFeel(&laf);
    aw->addTextEditor("lufs", juce::String(defaultLufs, 1), "Target LUFS");
    aw->addButton("Start Listening", 1, juce::KeyPress(juce::KeyPress::returnKey));
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
                beginSmartMasterCapture(genre, target);
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

void VisualCompEditor::beginSmartMasterCapture(const juce::String& genre, float targetLufs)
{
    smartMasterGenre       = genre;
    smartMasterTargetLufs  = targetLufs;
    smartMasterProgress    = 0.0;

    audioProcessor.smartMasterCaptureWritePos.store(0, std::memory_order_relaxed);
    audioProcessor.smartMasterCaptureDone.store(false, std::memory_order_relaxed);
    audioProcessor.smartMasterCaptureActive.store(true, std::memory_order_relaxed);

    auto* aw = new juce::AlertWindow("Smart Master+ - Step 3 of 3 - Listening",
        "Play (or let run) at least "
            + juce::String(int(VisualCompProcessor::kSmartMasterCaptureSeconds))
            + " seconds of representative audio through the plugin now. Smart Master+ analyzes "
              "the whole excerpt -- level, dynamics and full frequency spectrum -- rather than "
              "an instant snapshot, then builds EQ and multiband moves from it.",
        juce::AlertWindow::NoIcon);
    aw->setLookAndFeel(&laf);
    aw->addProgressBarComponent(smartMasterProgress);
    aw->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    wizardWindow.reset(aw);
    waitingForSmartMasterCapture = true;
    wizardWindow->enterModalState(true, juce::ModalCallbackFunction::create(
        [this](int result)
        {
            waitingForSmartMasterCapture = false;
            audioProcessor.smartMasterCaptureActive.store(false, std::memory_order_relaxed);
            wizardWindow.reset();
            if (result == 2)   // capture finished -- see pollSmartMasterCapture
                runAutoAnalyze(smartMasterGenre, smartMasterTargetLufs);
            // result == 0 (Cancel) -- just abandon, capture buffer discarded
        }), false);
}

void VisualCompEditor::pollSmartMasterCapture()
{
    const int cap = audioProcessor.smartMasterCapture.getNumSamples();
    const int pos = audioProcessor.smartMasterCaptureWritePos.load(std::memory_order_relaxed);
    smartMasterProgress = cap > 0 ? juce::jlimit(0.0, 1.0, double(pos) / double(cap)) : 0.0;

    // Programmatically closes the still-open AlertWindow once the capture
    // buffer is full; its ModalCallbackFunction (above) picks up result==2
    // and runs the actual analysis.
    if (audioProcessor.smartMasterCaptureDone.load(std::memory_order_relaxed) && wizardWindow != nullptr)
        wizardWindow->exitModalState(2);
}

VisualCompEditor::SmartMasterAnalysis VisualCompEditor::analyzeSmartMasterCapture() const
{
    SmartMasterAnalysis a;
    const auto& cap = audioProcessor.smartMasterCapture;
    const int numCaptured = juce::jmin(audioProcessor.smartMasterCaptureWritePos.load(std::memory_order_relaxed),
                                       cap.getNumSamples());
    if (numCaptured < 1024 || cap.getNumChannels() < 2) return a;

    const double sr = audioProcessor.getSampleRate() > 0.0 ? audioProcessor.getSampleRate() : 44100.0;
    const float* L = cap.getReadPointer(0);
    const float* R = cap.getReadPointer(1);

    // ---- Level / crest factor / integrated loudness over the whole excerpt ----
    // Same K-weighting shape as LoudnessMeter (see its class comment) but
    // accumulated as one true mean over the whole capture rather than a
    // continuously-decaying short-term window, since the excerpt is now a
    // fixed, complete measurement rather than a live reading.
    Biquad shelfL, shelfR, hpL, hpR;
    designBiquad(FilterShape::HighShelf, 1500.0f, 0.707f, 4.0f, sr, shelfL);
    designBiquad(FilterShape::HighShelf, 1500.0f, 0.707f, 4.0f, sr, shelfR);
    designBiquad(FilterShape::HighPass,  60.0f,   0.707f, 0.0f, sr, hpL);
    designBiquad(FilterShape::HighPass,  60.0f,   0.707f, 0.0f, sr, hpR);

    double sumSq = 0.0, kSumSq = 0.0;
    float  peak  = 0.0f;
    for (int i = 0; i < numCaptured; ++i)
    {
        const float l = L[i], r = R[i];
        peak   = juce::jmax(peak, juce::jmax(std::abs(l), std::abs(r)));
        sumSq += 0.5 * (double(l) * l + double(r) * r);

        const float kl = hpL.process(shelfL.process(l));
        const float kr = hpR.process(shelfR.process(r));
        kSumSq += 0.5 * (double(kl) * kl + double(kr) * kr);
    }
    const double meanSq  = sumSq  / double(numCaptured);
    const double kMeanSq = kSumSq / double(numCaptured);
    const float rms = float(std::sqrt(meanSq));
    a.rmsDb          = rms  > 1.0e-9f ? juce::Decibels::gainToDecibels(rms)  : -100.0f;
    a.peakDb         = peak > 1.0e-9f ? juce::Decibels::gainToDecibels(peak) : -100.0f;
    a.crestDb        = a.peakDb - a.rmsDb;
    a.integratedLufs = kMeanSq > 1.0e-9 ? float(-0.691 + 10.0 * std::log10(kMeanSq)) : -100.0f;

    // ---- Welch-averaged magnitude spectrum, reduced to the 7 tonal bands ----
    constexpr int kFftOrder = 12;                 // 4096-point FFT
    constexpr int kFftSize  = 1 << kFftOrder;
    constexpr int kHop      = kFftSize / 2;        // 50% overlap
    if (numCaptured < kFftSize) return a;

    juce::dsp::FFT fft(kFftOrder);
    juce::dsp::WindowingFunction<float> window(size_t(kFftSize),
        juce::dsp::WindowingFunction<float>::hann);

    std::array<double, SmartMasterAnalysis::kNumBands> bandEnergy {};
    std::array<double, SmartMasterAnalysis::kNumBands> bandBins   {};
    std::vector<float> fftBuf(size_t(kFftSize) * 2, 0.0f);

    int numWindows = 0;
    for (int start = 0; start + kFftSize <= numCaptured; start += kHop)
    {
        for (int i = 0; i < kFftSize; ++i)
            fftBuf[size_t(i)] = 0.5f * (L[start + i] + R[start + i]);
        juce::FloatVectorOperations::clear(fftBuf.data() + kFftSize, kFftSize);
        window.multiplyWithWindowingTable(fftBuf.data(), size_t(kFftSize));
        fft.performRealOnlyForwardTransform(fftBuf.data());

        for (int bin = 1; bin < kFftSize / 2; ++bin)
        {
            const float re = fftBuf[size_t(bin * 2)];
            const float im = fftBuf[size_t(bin * 2 + 1)];
            const double mag2 = double(re) * re + double(im) * im;
            const double freq = double(bin) * sr / double(kFftSize);
            for (int b = 0; b < SmartMasterAnalysis::kNumBands; ++b)
            {
                if (freq >= kSmartBandLo[b] && freq < kSmartBandHi[b])
                {
                    bandEnergy[size_t(b)] += mag2;
                    bandBins[size_t(b)]   += 1.0;
                    break;
                }
            }
        }
        ++numWindows;
    }

    if (numWindows > 0)
    {
        std::array<float, SmartMasterAnalysis::kNumBands> rawDb {};
        double meanDb = 0.0; int validBands = 0;
        for (int b = 0; b < SmartMasterAnalysis::kNumBands; ++b)
        {
            const double avgEnergy = bandBins[size_t(b)] > 0.0
                ? bandEnergy[size_t(b)] / bandBins[size_t(b)] : 0.0;
            rawDb[size_t(b)] = avgEnergy > 1.0e-12 ? float(10.0 * std::log10(avgEnergy)) : -120.0f;
            if (rawDb[size_t(b)] > -119.0f) { meanDb += rawDb[size_t(b)]; ++validBands; }
        }
        const float mean = validBands > 0 ? float(meanDb / validBands) : 0.0f;
        // Store each band relative to this excerpt's own mean -- a tilt
        // shape, comparable directly against the (also mean-relative)
        // genre target tilt table in runAutoAnalyze regardless of the
        // excerpt's absolute level.
        for (int b = 0; b < SmartMasterAnalysis::kNumBands; ++b)
            a.bandDb[size_t(b)] = rawDb[size_t(b)] - mean;
    }

    return a;
}

// Builds a full master from the just-captured excerpt: broadband compressor
// (same genre-character table the original heuristic used), a set of EQ
// nodes correcting this excerpt's measured spectral tilt toward the genre's
// reference curve (see kSmartBandLo/Hi and the target-tilt table below --
// distilled from genre mastering-reference-curve guidance: V-shaped pop;
// hip-hop/EDM sub-bass emphasis with a scooped low-mid and crisp top;
// rock/metal midrange-forward with controlled low-mid mud; classical/
// acoustic left close to flat/natural; podcast presence-boosted for
// intelligibility with rumble cut), plus two always-on multiband dynamics
// bands (gentle bass glue, presence de-esser) layered on top -- this is the
// "use plain EQ nodes, multiband comp, whatever is needed" ask, not just a
// broadband-compressor preset.
void VisualCompEditor::runAutoAnalyze(const juce::String& genre, float targetLufs)
{
    const auto analysis = analyzeSmartMasterCapture();
    if (analysis.rmsDb <= -99.0f)
    {
        juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon,
            "Smart Master+", "No usable audio was captured -- make sure audio is actually "
                              "playing through the plugin, then try again.");
        return;
    }

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

    using Tilt = std::array<float, SmartMasterAnalysis::kNumBands>;
    auto targetTiltFor = [](const juce::String& g) -> Tilt
    {
        // sub-bass, bass, low-mid, mid, high-mid, presence, air (relative dB)
        if (g == "Pop")                    return { { 1.5f, 1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 1.5f } };
        if (g == "Hip-Hop / Trap")         return { { 3.5f, 2.0f, -1.5f, -0.5f, 0.5f, 1.5f, 2.0f } };
        if (g == "EDM / Dance")            return { { 3.0f, 2.5f, -2.0f, -0.5f, 0.5f, 1.5f, 2.5f } };
        if (g == "Rock / Metal")           return { { -1.0f, 0.5f, -1.0f, 1.5f, 1.5f, 1.0f, 0.0f } };
        if (g == "Acoustic / Folk")        return { { -2.0f, -0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 1.0f } };
        if (g == "Classical / Orchestral") return { { -1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f } };
        return                                    { { -4.0f, -2.0f, -0.5f, 1.0f, 2.0f, -0.5f, 0.5f } }; // Podcast
    };
    const Tilt target = targetTiltFor(genre);

    // Per-band Q: wider at the extremes (sub-bass/air), a bit narrower
    // through the mids where a genre's character actually lives.
    static constexpr float kBandQ[SmartMasterAnalysis::kNumBands] = { 0.7f, 0.8f, 0.9f, 0.9f, 1.0f, 1.1f, 0.7f };

    // ---- Threshold/gain-out for the broadband compressor, from the whole excerpt ----
    const float thresholdDb = juce::jlimit(-40.0f, -1.0f, (analysis.peakDb + analysis.rmsDb) * 0.5f);
    const float gainOutDb   = (analysis.integratedLufs > -90.0f)
        ? juce::jlimit(-24.0f, 24.0f, targetLufs - analysis.integratedLufs) : 0.0f;

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

    // ---- EQ nodes: one per tonal band, tilt-correcting toward the genre's
    // reference curve; the bass and presence bands are additionally linked
    // into their own multiband compressor for glue/de-essing. Node 7 is left
    // free (only 7 bands are defined). Built into a local array first rather
    // than written straight to audioProcessor.eq, so confirmAndApplyEq below
    // can gate the actual apply behind a "keep current EQ?" prompt if the
    // live EQ has hand-edits since the last preset load/generation.
    std::array<EqNodeState, kMaxEqNodes> nodes;
    int nodesUsed = 0;
    for (int b = 0; b < SmartMasterAnalysis::kNumBands; ++b)
    {
        const bool isDynamicsBand = (b == kSmartBassBand || b == kSmartPresenceBand);
        const float deviation = juce::jlimit(-6.0f, 6.0f, target[size_t(b)] - analysis.bandDb[size_t(b)]);
        const bool  worthCorrecting = std::abs(deviation) > 0.4f;

        EqNodeState n;
        n.enabled = isDynamicsBand || worthCorrecting;
        if (!n.enabled) { nodes[size_t(b)] = n; continue; }

        n.type   = EqTypes::Bell;
        n.freqHz = std::sqrt(kSmartBandLo[b] * kSmartBandHi[b]);
        n.q      = kBandQ[size_t(b)];
        n.gainDb = worthCorrecting ? deviation : 0.0f;
        n.linked = isDynamicsBand;

        if (isDynamicsBand)
        {
            const bool isBass = (b == kSmartBassBand);
            // Threshold anchored to this excerpt's own measured RMS: a bit
            // below it for the bass band (catches above-average low-end
            // buildup for gentle glue), a bit above it for the presence
            // band (catches only harsh/sibilant peaks, not steady content).
            n.thresholdDb = juce::jlimit(-60.0f, 0.0f, analysis.rmsDb + (isBass ? -2.0f : 2.0f));
            n.kneeDb      = isBass ? 6.0f  : 4.0f;
            n.ratio       = isBass ? 1.8f  : 2.5f;
            n.attackMs    = isBass ? 25.0f : 3.0f;
            n.releaseMs   = isBass ? 180.0f : 90.0f;
            n.rangeDb     = -3.0f;     // gentle -- glue/de-ess, not slam
            n.upward      = false;
            n.bwLowOct    = isBass ? 0.8f : 0.5f;
            n.bwHighOct   = isBass ? 0.8f : 0.5f;
        }

        nodes[size_t(b)] = n;
        ++nodesUsed;
    }
    nodes[size_t(SmartMasterAnalysis::kNumBands)] = EqNodeState{};   // node 7, unused

    const juce::String name = "Smart Master+: " + genre + "  " + juce::String(targetLufs, 1) + " LU";

    confirmAndApplyEq(
        [this, nodes]
        {
            for (int i = 0; i < kMaxEqNodes; ++i)
                audioProcessor.eq.setNode(i, nodes[size_t(i)]);
        },
        [this, name, nodesUsed, analysis]
        {
            setPresetName(name);
            setPresetAuthor({});   // generated, not loaded/saved from a user preset file

            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::InfoIcon,
                "Smart Master+ complete",
                "Generated \"" + name + "\" from "
                    + juce::String(int(VisualCompProcessor::kSmartMasterCaptureSeconds))
                    + "s of captured audio (" + juce::String(analysis.crestDb, 1) + " dB crest factor, "
                    + juce::String(analysis.integratedLufs, 1) + " LUFS measured): broadband compression, "
                    + juce::String(nodesUsed) + " spectrum-matching EQ nodes, and bass/presence multiband "
                      "dynamics.\n\nThis is a heuristic starting point, not a mastering-grade AI -- refine "
                      "by ear, especially threshold, ratio and the EQ node gains.");
        });
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

    steps.push_back({ presetButton.getBounds(), "Start with a preset",
        "34 factory presets for drums, vocals, bass, mastering and more. Click the name to browse." });

    steps.push_back({ presetPrev.getBounds().getUnion(presetSave.getBounds()),
        "Step through and save",
        "The arrows cycle presets in order. SAVE keeps your own — credit yourself in the Author "
        "box in between, so a shared pack still says who made it." });

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
        g.drawText("AUTHOR", 182, kTitleH + 4, 140, 14, juce::Justification::centredLeft, false);
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
    // Demo mode watermark indicator — positioned in top-right corner, fixed 24px height
    demoModeIndicator.setBounds(ox + kWidth - cshift - 224, 8, 216, 24);
    // Preset name now lives up here, left of SoftClip — same row/height as
    // bypassButton/clipModeButton. Worst case (Curve/GR collapsed) leaves
    // 290px between logoZone's right edge and clipModeButton's left edge;
    // centred in that gap (rather than flush against the logo) so it sits
    // perfectly between the two, not just clear of both.
    {
        constexpr int presetW = 270;
        const int gapLeft  = logoZone.getRight();
        const int gapRight = clipModeButton.getX();
        const int presetX  = gapLeft + (gapRight - gapLeft - presetW) / 2;
        presetButton.setBounds(presetX, 14, presetW, kTitleH - 28);
    }

    // Preset strip — EQ toggle sits directly to the left of the Mode button.
    // presetButton's old slot here now holds the Author field instead.
    {
        const int sy = kTitleH + 20, sh = 24;
        eqButton         .setBounds(ox + 10,  sy, 36,  sh);
        modeButton       .setBounds(ox + 50,  sy, 104, sh);
        presetPrev       .setBounds(ox + 158, sy, 20,  sh);
        presetAuthorEditor.setBounds(ox + 182, sy, 250, sh);
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

        // Trimmed from 24/32/3/14 to reclaim vertical space for the fader
        // itself (see faderH below) — still legible at this scale, matching
        // other compact controls elsewhere in this UI (e.g. NodeIsland's
        // 26px direction/type buttons).
        constexpr int btnH   = 20;
        constexpr int agBtnH = 28;
        constexpr int gap    = 2;
        constexpr int miniH  = 11;

        const int bottomH  = agBtnH + gap + btnH + miniH + gap + btnH + miniH + 2;
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

    if (enlargeOverlay)
        enlargeOverlay->setBounds(getLocalBounds());
}

bool VisualCompEditor::keyPressed(const juce::KeyPress& key)
{
    // Ctrl+Z: Undo
    if (key.isKeyCode(juce::KeyPress::createFromDescription("ctrl+z").getKeyCode()))
    {
        if (audioProcessor.undoRedoManager.canUndo())
        {
            audioProcessor.undoRedoManager.undo();
            return true;
        }
    }

    // Ctrl+Y or Ctrl+Shift+Z: Redo
    if (key.isKeyCode(juce::KeyPress::createFromDescription("ctrl+y").getKeyCode()) ||
        key.isKeyCode(juce::KeyPress::createFromDescription("ctrl+shift+z").getKeyCode()))
    {
        if (audioProcessor.undoRedoManager.canRedo())
        {
            audioProcessor.undoRedoManager.redo();
            return true;
        }
    }

    // Let the base class handle any other keys
    return false;
}
