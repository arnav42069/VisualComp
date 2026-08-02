#include "EqPanel.h"
#include "Theme.h"

const juce::Colour EqPanel::kNodeColours[kMaxEqNodes] =
{
    juce::Colour(0xffff7a4d), juce::Colour(0xff4dc3ff), juce::Colour(0xffb98cff),
    juce::Colour(0xff6ee3a0), juce::Colour(0xffffd24d), juce::Colour(0xffff5da2),
    juce::Colour(0xff7de1ff), juce::Colour(0xffc9a15c)
};

namespace
{
    constexpr float kFreqLo = 20.0f, kFreqHi = 20000.0f;
    constexpr float kGainRange = 18.0f;   // +/- dB shown on the graph
    constexpr float kLeftGutter   = 34.0f;   // dB-axis labels
    constexpr float kBottomMargin = 34.0f;   // freq-axis labels + hint text
    constexpr float kHeaderH      = 40.0f;   // title + MULTIBAND COMP toggle row
    constexpr int   kMultibandBtnW = 150;    // wide enough for "MULTIBAND COMP"
    constexpr int   kMultibandBtnX = 10;

    // Right-click "Q" submenu presets (see EqPanel::showNodeMenu).
    constexpr float kQPresets[] = { 0.3f, 0.5f, 0.71f, 1.0f, 1.4f, 2.0f, 3.0f, 5.0f, 8.0f };
    constexpr int   kNumQPresets = int(sizeof(kQPresets) / sizeof(kQPresets[0]));
}

void EqCloseButton::paintButton(juce::Graphics& g, bool isHighlighted, bool isDown)
{
    getLookAndFeel().drawButtonBackground(g, *this, findColour(buttonColourId),
                                          isHighlighted, isDown);

    const auto r = getLocalBounds().toFloat().reduced(7.0f);
    g.setColour(isHighlighted ? Theme::text : Theme::textDim);
    g.drawLine(r.getX(), r.getY(), r.getRight(), r.getBottom(), 1.6f);
    g.drawLine(r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.6f);
}

EqPanel::EqPanel(VisualCompProcessor& proc) : processor(proc), nodeIsland(proc)
{
    for (int i = 0; i < kMaxEqNodes; ++i)
        localNodes[size_t(i)] = processor.eq.getNode(i);

    closeButton.onClick = [this] { if (onCloseRequested) onCloseRequested(); };
    addAndMakeVisible(closeButton);

    // MULTIBAND: OFF = standard single-band compressor, EQ affects tone only.
    // ON = every *linked* node also acts as its own dynamic-EQ-style
    // compression band (see ParametricEq::applyDynamicBandGain). Clicking
    // it on also links every currently-active node in one motion (per the
    // button's own name); clicking it off leaves existing links alone,
    // since they still matter for the single-compressor frequency-aware
    // detector (see CLAUDE.md).
    multibandButton.setClickingTogglesState(true);
    multibandButton.setColour(juce::TextButton::buttonColourId,   Theme::charcoal);
    multibandButton.setColour(juce::TextButton::buttonOnColourId, Theme::warn);
    multibandButton.setColour(juce::TextButton::textColourOffId,  Theme::textDim);
    multibandButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    multibandButton.setToggleState(processor.multibandEnabled.load(std::memory_order_relaxed),
                                   juce::dontSendNotification);
    multibandButton.onClick = [this]
    {
        const bool on = multibandButton.getToggleState();
        processor.multibandEnabled.store(on, std::memory_order_relaxed);
        if (on)
        {
            for (int i = 0; i < kMaxEqNodes; ++i)
            {
                if (!localNodes[size_t(i)].enabled || localNodes[size_t(i)].linked) continue;
                localNodes[size_t(i)].linked = true;
                pushNode(i);
            }
            repaint();
        }
    };
    addAndMakeVisible(multibandButton);

    nodeIsland.onQRatioChanged = [this](int anchor, float ratio) { applyRelativeQ(anchor, ratio); };
    addChildComponent(nodeIsland);

    startTimerHz(20);
    setInterceptsMouseClicks(true, true);
}

EqPanel::~EqPanel() { stopTimer(); }

void EqPanel::pushNode(int i)
{
    processor.eq.setNode(i, localNodes[size_t(i)]);
}

void EqPanel::setSelectedNode(int i)
{
    selectedNode = i;
    multiSelected.fill(false);
    if (i >= 0) multiSelected[size_t(i)] = true;
    nodeIsland.setTargetNode(i);
    updateIslandBounds();
    repaint();
}

float EqPanel::freqToX(float freq) const
{
    const auto r = graphArea();
    const float t = std::log(freq / kFreqLo) / std::log(kFreqHi / kFreqLo);
    return r.getX() + t * r.getWidth();
}

float EqPanel::xToFreq(float x) const
{
    const auto r = graphArea();
    const float t = juce::jlimit(0.0f, 1.0f, (x - r.getX()) / r.getWidth());
    return kFreqLo * std::pow(kFreqHi / kFreqLo, t);
}

float EqPanel::gainDbToY(float gainDb) const
{
    const auto r = graphArea();
    const float t = juce::jlimit(-1.0f, 1.0f, gainDb / kGainRange);
    return r.getCentreY() - t * (r.getHeight() * 0.5f);
}

float EqPanel::yToGainDb(float y) const
{
    const auto r = graphArea();
    const float t = juce::jlimit(-1.0f, 1.0f, (r.getCentreY() - y) / (r.getHeight() * 0.5f));
    return t * kGainRange;
}

juce::Rectangle<float> EqPanel::graphArea() const
{
    return getLocalBounds().toFloat()
              .withTrimmedLeft(10.0f + kLeftGutter).withTrimmedRight(10.0f)
              .withTrimmedTop(kHeaderH).withTrimmedBottom(kBottomMargin);
}

namespace
{
    // HPF/LPF/Notch have no meaningful static gain — their node dot always
    // sits on the 0dB line and isn't vertically draggable.
    bool isGainlessType(int type) noexcept
    {
        return type == EqTypes::HighPass || type == EqTypes::LowPass || type == EqTypes::Notch;
    }
}

juce::Point<float> EqPanel::nodePos(int i) const
{
    const auto& n = localNodes[size_t(i)];
    return { freqToX(n.freqHz), gainDbToY(isGainlessType(n.type) ? 0.0f : n.gainDb) };
}

bool EqPanel::thresholdMarkerActive(int i) const
{
    if (i < 0 || i >= kMaxEqNodes) return false;
    const auto& n = localNodes[size_t(i)];
    return n.enabled && n.linked && processor.multibandEnabled.load(std::memory_order_relaxed);
}

juce::Point<float> EqPanel::thresholdMarkerPos(int i) const
{
    const auto& n = localNodes[size_t(i)];
    return { freqToX(n.freqHz), gainDbToY(n.thresholdDb) };
}

int EqPanel::findNodeNear(juce::Point<float> p, float radius) const
{
    int best = -1; float bestD = radius;
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        if (!localNodes[size_t(i)].enabled) continue;
        const float d = nodePos(i).getDistanceFrom(p);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

int EqPanel::findFreeNode() const
{
    for (int i = 0; i < kMaxEqNodes; ++i)
        if (!localNodes[size_t(i)].enabled) return i;
    return -1;
}

float EqPanel::sampleRateForDisplay() const
{
    return float(juce::jmax(44100.0, processor.getSampleRate()));
}

int EqPanel::findEdgeNear(juce::Point<float> p, int& whichEdge, float radius) const
{
    whichEdge = -1;
    const auto r = graphArea();
    const float flagY = r.getY() + 6.0f;
    if (std::abs(p.y - flagY) > radius) return -1;

    const float sr = sampleRateForDisplay();
    int best = -1; float bestD = radius;
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        const auto& n = localNodes[size_t(i)];
        if (!n.enabled || !n.linked) continue;
        const float loX = freqToX(detectorLoHz(n));
        const float hiX = freqToX(detectorHiHz(n, sr));
        const float dLo = std::abs(p.x - loX);
        const float dHi = std::abs(p.x - hiX);
        if (dLo < bestD) { bestD = dLo; best = i; whichEdge = 0; }
        if (dHi < bestD) { bestD = dHi; best = i; whichEdge = 1; }
    }
    return best;
}

void EqPanel::createNodeAt(juce::Point<float> p)
{
    const int freeIdx = findFreeNode();
    if (freeIdx < 0) return;   // all 8 nodes in use
    const float newFreq = xToFreq(p.x);

    bool isLowest = true, isHighest = true;
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        if (i == freeIdx || !localNodes[size_t(i)].enabled) continue;
        if (localNodes[size_t(i)].freqHz < newFreq) isLowest  = false;
        if (localNodes[size_t(i)].freqHz > newFreq) isHighest = false;
    }
    const int defaultType = (isLowest && !isHighest)  ? EqTypes::LowShelf
                           : (isHighest && !isLowest)  ? EqTypes::HighShelf
                                                        : EqTypes::Bell;

    localNodes[size_t(freeIdx)] = EqNodeState{};
    localNodes[size_t(freeIdx)].enabled = true;
    localNodes[size_t(freeIdx)].freqHz  = newFreq;
    localNodes[size_t(freeIdx)].gainDb  = yToGainDb(p.y);
    localNodes[size_t(freeIdx)].type    = defaultType;
    pushNode(freeIdx);

    dragIndex = freeIdx;
    dragEdge  = -1;
    setSelectedNode(freeIdx);
    if (onNodeSelected) onNodeSelected(freeIdx);
    repaint();
}

void EqPanel::selectOnly(int i) { setSelectedNode(i); }

void EqPanel::toggleMultiSelect(int i)
{
    if (i < 0) return;
    multiSelected[size_t(i)] = !multiSelected[size_t(i)];
    if (multiSelected[size_t(i)])
    {
        selectedNode = i;   // newly-added node becomes the drag/edit anchor
    }
    else if (selectedNode == i)
    {
        selectedNode = -1;
        for (int k = 0; k < kMaxEqNodes; ++k)
            if (multiSelected[size_t(k)]) { selectedNode = k; break; }
    }
    nodeIsland.setTargetNode(selectedNode);
    updateIslandBounds();
    repaint();
}

void EqPanel::applyRelativeGainFreq(int anchor, float deltaGainDb, float freqRatio)
{
    int count = 0;
    for (int i = 0; i < kMaxEqNodes; ++i) if (multiSelected[size_t(i)]) ++count;
    if (count < 2) return;

    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        if (i == anchor || !multiSelected[size_t(i)] || !localNodes[size_t(i)].enabled) continue;
        auto& n = localNodes[size_t(i)];
        n.freqHz = juce::jlimit(kFreqLo, kFreqHi, n.freqHz * freqRatio);
        if (!isGainlessType(n.type))
            n.gainDb = juce::jlimit(-kGainRange, kGainRange, n.gainDb + deltaGainDb);
        pushNode(i);
    }
}

void EqPanel::applyRelativeQ(int anchor, float qRatio)
{
    int count = 0;
    for (int i = 0; i < kMaxEqNodes; ++i) if (multiSelected[size_t(i)]) ++count;
    if (count < 2) return;

    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        if (i == anchor || !multiSelected[size_t(i)] || !localNodes[size_t(i)].enabled) continue;
        auto& n = localNodes[size_t(i)];
        n.q = juce::jlimit(0.15f, 10.0f, n.q * qRatio);
        pushNode(i);
    }
    repaint();
}

void EqPanel::updateIslandBounds()
{
    if (selectedNode < 0) { nodeIsland.setVisible(false); return; }

    const auto p = nodePos(selectedNode);
    const auto r = graphArea();

    int x = int(p.x - NodeIsland::kWidth * 0.5f);
    x = juce::jlimit(int(r.getX()), juce::jmax(int(r.getX()), int(r.getRight()) - NodeIsland::kWidth), x);

    constexpr int gap = 14;
    int y = int(p.y) - NodeIsland::kHeight - gap;
    if (y < r.getY())
        y = int(p.y) + gap;   // not enough room above -- flip below the node
    y = juce::jlimit(int(r.getY()), juce::jmax(int(r.getY()), int(r.getBottom()) - NodeIsland::kHeight), y);

    nodeIsland.setBounds(x, y, NodeIsland::kWidth, NodeIsland::kHeight);
    nodeIsland.toFront(false);
}

void EqPanel::mouseDown(const juce::MouseEvent& e)
{
    dragMoved     = false;
    dragEdge      = -1;
    dragThreshold = false;
    const auto p = e.position;

    // A click that hits nothing deselects whatever node/Island is active,
    // rather than leaving the last selection (and its Dynamic Island) stuck
    // open until something else happens to change it.
    auto deselect = [this]
    {
        setSelectedNode(-1);
        if (onNodeSelected) onNodeSelected(-1);
    };

    if (!graphArea().contains(p)) { dragIndex = -1; deselect(); return; }

    if (e.mods.isRightButtonDown())
    {
        const int hit = findNodeNear(p);
        if (hit >= 0) showNodeMenu(hit);
        dragIndex = -1;
        return;
    }

    // The "T" threshold marker only exists for the already-selected node, so
    // it takes priority over a plain node/edge hit-test at the same spot.
    if (selectedNode >= 0 && thresholdMarkerActive(selectedNode)
        && thresholdMarkerPos(selectedNode).getDistanceFrom(p) < 10.0f)
    {
        dragIndex     = selectedNode;
        dragThreshold = true;
        return;
    }

    int whichEdge = -1;
    const int edgeHit = findEdgeNear(p, whichEdge);
    if (edgeHit >= 0)
    {
        dragIndex = edgeHit;
        dragEdge  = whichEdge;
        selectOnly(edgeHit);
        if (onNodeSelected) onNodeSelected(edgeHit);
        return;
    }

    const int hit = findNodeNear(p);
    if (hit < 0) { dragIndex = -1; deselect(); return; }   // empty space: double-click creates, single click deselects

    dragIndex = hit;
    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
        toggleMultiSelect(hit);
    else
        selectOnly(hit);
    if (onNodeSelected) onNodeSelected(hit);
}

void EqPanel::mouseDrag(const juce::MouseEvent& e)
{
    if (dragIndex < 0) return;
    dragMoved = true;
    auto& n = localNodes[size_t(dragIndex)];

    if (dragThreshold)
    {
        // Vertical-only: frequency stays locked to the node's own position.
        // Downward-only (<=0), matching the knob's own -96..0 range; also
        // clamped to the graph's +/-kGainRange dB window like everything
        // else drawn on it (see yToGainDb) — the Threshold knob itself is
        // the only way to reach values beyond what the graph can show.
        n.thresholdDb = juce::jmin(0.0f, yToGainDb(e.position.y));
        pushNode(dragIndex);
        repaint();
        return;
    }

    if (dragEdge >= 0)
    {
        const float edgeHz = juce::jlimit(kFreqLo, kFreqHi, xToFreq(e.position.x));
        if (dragEdge == 0)
        {
            const float loHz = juce::jmin(n.freqHz * 0.98f, edgeHz);
            n.bwLowOct = juce::jlimit(0.05f, 8.0f, std::log2(n.freqHz / juce::jmax(kFreqLo, loHz)));
        }
        else
        {
            const float hiHz = juce::jmax(n.freqHz * 1.02f, edgeHz);
            n.bwHighOct = juce::jlimit(0.05f, 8.0f, std::log2(juce::jmin(kFreqHi, hiHz) / n.freqHz));
        }
        pushNode(dragIndex);
        repaint();
        return;
    }

    const float oldFreq = n.freqHz;
    const float oldGain = n.gainDb;

    n.freqHz = juce::jlimit(kFreqLo, kFreqHi, xToFreq(e.position.x));
    if (!isGainlessType(n.type))
        n.gainDb = juce::jlimit(-kGainRange, kGainRange, yToGainDb(e.position.y));
    pushNode(dragIndex);

    const float freqRatio   = (oldFreq > 0.0f) ? n.freqHz / oldFreq : 1.0f;
    const float deltaGainDb = n.gainDb - oldGain;
    applyRelativeGainFreq(dragIndex, deltaGainDb, freqRatio);

    updateIslandBounds();
    repaint();
}

void EqPanel::mouseUp(const juce::MouseEvent&)
{
    dragIndex     = -1;
    dragEdge      = -1;
    dragThreshold = false;
}

void EqPanel::mouseDoubleClick(const juce::MouseEvent& e)
{
    const auto p = e.position;
    if (!graphArea().contains(p)) return;
    if (findNodeNear(p) >= 0) return;   // double-click on an existing node: no-op, not a re-create

    int whichEdge = -1;
    if (findEdgeNear(p, whichEdge) >= 0) return;   // double-click on an edge flag: no-op

    createNodeAt(p);
}

void EqPanel::mouseWheelMove(const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int hit = findNodeNear(e.position, 16.0f);
    if (hit < 0) return;
    auto& n = localNodes[size_t(hit)];
    const float factor = 1.0f + wheel.deltaY * 0.6f;
    n.q = juce::jlimit(0.15f, 10.0f, n.q * factor);
    pushNode(hit);
    applyRelativeQ(hit, factor);
    repaint();
}

void EqPanel::showNodeMenu(int i)
{
    juce::PopupMenu menu;
    auto& n = localNodes[size_t(i)];

    juce::PopupMenu typeMenu;
    for (int t = 0; t < EqTypes::kNumTypes; ++t)
        typeMenu.addItem(100 + t, EqTypes::kNames[t], true, n.type == t);
    menu.addSubMenu("Filter Type", typeMenu);

    juce::PopupMenu qMenu;
    int closestQi = 0; float bestDiff = std::abs(kQPresets[0] - n.q);
    for (int qi = 1; qi < kNumQPresets; ++qi)
    {
        const float diff = std::abs(kQPresets[qi] - n.q);
        if (diff < bestDiff) { bestDiff = diff; closestQi = qi; }
    }
    for (int qi = 0; qi < kNumQPresets; ++qi)
        qMenu.addItem(200 + qi, juce::String(kQPresets[qi], 2), true, qi == closestQi);
    menu.addSubMenu("Q", qMenu);

    menu.addItem(1, n.linked ? "Unlink from Compressor" : "Link to Compressor");
    menu.addSeparator();
    menu.addItem(2, "Remove Node");

    menu.showMenuAsync(juce::PopupMenu::Options(), [this, i](int result)
    {
        auto& node = localNodes[size_t(i)];
        if (result >= 100 && result < 100 + EqTypes::kNumTypes)
            node.type = result - 100;
        else if (result >= 200 && result < 200 + kNumQPresets)
            node.q = kQPresets[result - 200];
        else if (result == 1)
            node.linked = !node.linked;
        else if (result == 2)
        {
            node = EqNodeState{};   // disable + reset to defaults
            multiSelected[size_t(i)] = false;
            if (selectedNode == i) setSelectedNode(-1);
        }

        pushNode(i);
        repaint();
    });
}

void EqPanel::timerCallback()
{
    // Pick up out-of-band changes (e.g. an Auto-Analyze preset writing
    // directly into the processor's EQ).
    bool changed = false;
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        const auto latest = processor.eq.getNode(i);
        if (latest.enabled != localNodes[size_t(i)].enabled ||
            latest.linked  != localNodes[size_t(i)].linked)
            changed = true;
        if (!latest.enabled) multiSelected[size_t(i)] = false;
        localNodes[size_t(i)] = latest;
    }

    if (selectedNode >= 0 && !localNodes[size_t(selectedNode)].enabled)
        setSelectedNode(-1);
    else if (selectedNode >= 0)
    {
        nodeIsland.refreshFromProcessor();
        updateIslandBounds();
    }

    // Multiband mode's per-band GR curves (see paint()) animate continuously
    // while active, independent of the dominant-band tracking below.
    if (changed || processor.activeEqBand.load(std::memory_order_relaxed) >= 0
        || processor.multibandEnabled.load(std::memory_order_relaxed))
        repaint();
}

void EqPanel::resized()
{
    closeButton.setBounds(getWidth() - 28, 8, 22, 22);
    multibandButton.setBounds(kMultibandBtnX, 8, kMultibandBtnW, 24);
    updateIslandBounds();
}

void EqPanel::paint(juce::Graphics& g)
{
    g.fillAll(Theme::bg);

    // Header: MULTIBAND COMP toggle top-left, title alongside it, close top-right.
    const int titleX = kMultibandBtnX + kMultibandBtnW + 10;
    g.setFont(Theme::label(15.0f));
    g.setColour(Theme::accent);
    g.drawText("PARAMETRIC EQ", titleX, 8, getWidth() - titleX - 40, 24,
               juce::Justification::centredLeft, false);
    g.setColour(Theme::accent.withAlpha(0.55f));
    g.fillRect(0, int(kHeaderH) - 8, getWidth(), 1);

    const auto r = graphArea();
    g.setColour(Theme::bgDeep);
    g.fillRect(r);
    g.setColour(Theme::line);
    g.drawRect(r, 1.0f);

    // Frequency gridlines (X-axis) — faint unlabeled lines fill out the grid,
    // labeled decade lines (incl. the 20Hz low edge) get a clear tick label
    // along the bottom axis.
    g.setFont(Theme::mono(9.5f));
    for (float f : { 50.0f, 200.0f, 500.0f, 2000.0f, 5000.0f, 20000.0f })
    {
        const float x = freqToX(f);
        g.setColour(Theme::text.withAlpha(0.05f));
        g.drawVerticalLine(int(x), r.getY(), r.getBottom());
    }
    for (float f : { 20.0f, 100.0f, 1000.0f, 10000.0f })
    {
        const float x = freqToX(f);
        g.setColour(Theme::text.withAlpha(0.16f));
        g.drawVerticalLine(int(x), r.getY(), r.getBottom());
        g.setColour(Theme::text.withAlpha(0.75f));
        const juce::String lbl = f >= 1000.0f ? juce::String(int(f / 1000.0f)) + "kHz" : juce::String(int(f)) + "Hz";
        const auto just = (f <= kFreqLo) ? juce::Justification::centredLeft : juce::Justification::centred;
        const float lblX = (f <= kFreqLo) ? x : x - 22.0f;
        g.drawText(lbl, lblX, r.getBottom() + 3.0f, 44.0f, 13.0f, just, false);
    }
    // dB gridlines (Y-axis), with the value labeled in the left-hand gutter.
    for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
    {
        const float y = gainDbToY(db);
        g.setColour(Theme::text.withAlpha(db == 0.0f ? 0.22f : 0.12f));
        g.drawHorizontalLine(int(y), r.getX(), r.getRight());
        g.setColour(Theme::text.withAlpha(0.75f));
        const juce::String lbl = (db > 0.0f ? "+" : "") + juce::String(int(db)) + "dB";
        g.drawText(lbl, 0.0f, y - 6.0f, kLeftGutter - 4.0f, 12.0f,
                   juce::Justification::centredRight, false);
    }

    // Response curve
    {
        juce::Path curve;
        const int steps = 96;
        for (int s = 0; s <= steps; ++s)
        {
            const float t = float(s) / float(steps);
            const float freq = kFreqLo * std::pow(kFreqHi / kFreqLo, t);
            const float db = processor.eq.previewMagnitudeDbAt(freq);
            const float x = r.getX() + t * r.getWidth();
            const float y = gainDbToY(db);
            if (s == 0) curve.startNewSubPath(x, y); else curve.lineTo(x, y);
        }
        g.setColour(Theme::accent.withAlpha(0.18f));
        g.strokePath(curve, juce::PathStrokeType(4.0f));
        g.setColour(Theme::accent);
        g.strokePath(curve, juce::PathStrokeType(1.6f));
    }

    // FabFilter Pro-MB-style dynamic gain-reduction curves: in multiband
    // mode each linked node runs its own independent detector/compressor
    // (see ParametricEq::applyDynamicBandGain), so its live reduction is a
    // real, isolated dip at that node's own shape/freq/Q — drawn separately
    // from the static response curve above, which never reflects dynamic
    // gain. Skipped outside multiband mode, where a linked node's level only
    // informs the one shared broadband compressor rather than being pulled
    // down on its own (see CLAUDE.md — this plugin is not true multiband).
    if (processor.multibandEnabled.load(std::memory_order_relaxed))
    {
        const float zeroY = gainDbToY(0.0f);
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            const auto& n = localNodes[size_t(i)];
            if (!n.enabled || !n.linked) continue;
            const auto colourRange = kNodeColours[i];

            // Offset range — this band's configured dynamic reach: for a
            // downward band, the reduction it would apply to a full-scale
            // (0 dBFS) hit; for an upward band, the boost it would apply to
            // near-silence — using its own threshold/knee/ratio either way.
            // Static (not level-reactive), so it always shows what this band
            // COULD do, independent of the live curve below. Drawn first so
            // the reactive curve sits on top.
            const float maxGrDb = clampedDynamicGrDb(n.upward, n.upward ? -100.0f : 0.0f,
                                                     n.thresholdDb, n.kneeDb, n.ratio, n.rangeDb);
            if (std::abs(maxGrDb) > 0.05f)
            {
                juce::Path rangePath;
                const int steps = 64;
                for (int s = 0; s <= steps; ++s)
                {
                    const float t = float(s) / float(steps);
                    const float freq = kFreqLo * std::pow(kFreqHi / kFreqLo, t);
                    const float db = processor.eq.previewNodeMagnitudeDbAt(i, maxGrDb, freq);
                    const float x = r.getX() + t * r.getWidth();
                    const float y = gainDbToY(db);
                    if (s == 0) rangePath.startNewSubPath(x, y); else rangePath.lineTo(x, y);
                }
                juce::Path dashed;
                const float dashLengths[] = { 3.0f, 3.0f };
                juce::PathStrokeType(1.0f).createDashedStroke(dashed, rangePath, dashLengths, 2);
                g.setColour(colourRange.withAlpha(0.35f));
                g.strokePath(dashed, juce::PathStrokeType(1.0f));
            }

            const float grDb = processor.bandGrDb[size_t(i)].load(std::memory_order_relaxed);
            if (std::abs(grDb) < 0.05f) continue;   // negligible reduction/boost — nothing to show

            juce::Path dip;
            const int steps = 64;
            for (int s = 0; s <= steps; ++s)
            {
                const float t = float(s) / float(steps);
                const float freq = kFreqLo * std::pow(kFreqHi / kFreqLo, t);
                const float db = processor.eq.previewNodeMagnitudeDbAt(i, grDb, freq);
                const float x = r.getX() + t * r.getWidth();
                const float y = gainDbToY(db);
                if (s == 0) dip.startNewSubPath(x, y); else dip.lineTo(x, y);
            }

            juce::Path fill(dip);
            fill.lineTo(r.getRight(), zeroY);
            fill.lineTo(r.getX(),     zeroY);
            fill.closeSubPath();

            const auto colour = kNodeColours[i];
            g.setColour(colour.withAlpha(0.16f));
            g.fillPath(fill);
            g.setColour(colour.withAlpha(0.85f));
            g.strokePath(dip, juce::PathStrokeType(1.4f));
        }
    }

    // Detector edge flags — for every linked node (regardless of multiband
    // mode: even the single-compressor "frequency-aware detector" case uses
    // these edges, see CLAUDE.md), a faint shaded strip shows the frequency
    // range feeding that band's compressor detector, with a draggable flag
    // at each edge. Independent per node — strips may fully overlap or
    // leave gaps, there is no shared crossover between them.
    {
        const float flagY = r.getY() + 6.0f;
        const float srDisp = sampleRateForDisplay();
        for (int i = 0; i < kMaxEqNodes; ++i)
        {
            const auto& n = localNodes[size_t(i)];
            if (!n.enabled || !n.linked) continue;
            const auto colour = kNodeColours[i];
            const float loX = freqToX(detectorLoHz(n));
            const float hiX = freqToX(detectorHiHz(n, srDisp));

            g.setColour(colour.withAlpha(0.06f));
            g.fillRect(juce::Rectangle<float>(loX, r.getY(), hiX - loX, r.getHeight()));

            for (float x : { loX, hiX })
            {
                juce::Path flag;
                flag.addTriangle(x - 4.0f, flagY - 5.0f, x + 4.0f, flagY - 5.0f, x, flagY + 4.0f);
                g.setColour(colour.withAlpha(0.85f));
                g.fillPath(flag);
            }
        }
    }

    const int domBand = processor.activeEqBand.load(std::memory_order_relaxed);

    // Nodes
    for (int i = 0; i < kMaxEqNodes; ++i)
    {
        const auto& n = localNodes[size_t(i)];
        if (!n.enabled) continue;
        const auto p = nodePos(i);
        const auto colour = kNodeColours[i];
        const bool isActive = (i == domBand);
        const float radius = 9.0f;

        // Offset-range stem — a FabFilter Pro-MB-style handle showing how far
        // this band's own dynamics could pull its gain: down toward a
        // full-scale hit for a downward band, or up toward near-silence for
        // an upward one. Drawn behind the node ball so the ball still reads
        // as sitting at its static position.
        if (n.linked && processor.multibandEnabled.load(std::memory_order_relaxed))
        {
            const float maxGrDb = clampedDynamicGrDb(n.upward, n.upward ? -100.0f : 0.0f,
                                                     n.thresholdDb, n.kneeDb, n.ratio, n.rangeDb);
            if (std::abs(maxGrDb) > 0.05f)
            {
                const float staticDb = isGainlessType(n.type) ? 0.0f : n.gainDb;
                const float yFloor = gainDbToY(juce::jlimit(-kGainRange, kGainRange, staticDb + maxGrDb));
                g.setColour(colour.withAlpha(0.55f));
                g.drawLine(p.x, p.y, p.x, yFloor, 1.6f);
                g.setColour(colour.withAlpha(0.8f));
                g.fillEllipse(p.x - 2.2f, yFloor - 2.2f, 4.4f, 4.4f);
            }
        }

        // Halo — always present so a node reads clearly against the curve,
        // brighter when it's the dominant linked band driving the compressor.
        g.setColour(colour.withAlpha(isActive ? 0.40f : 0.16f));
        g.fillEllipse(p.x - radius * 1.8f, p.y - radius * 1.8f, radius * 3.6f, radius * 3.6f);

        // Drop shadow
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(p.x - radius + 1.0f, p.y - radius + 1.5f, radius * 2.0f, radius * 2.0f);

        // Glass-bead body — light-to-dark radial gradient, FabFilter-style.
        {
            juce::ColourGradient body(colour.brighter(0.9f), p.x - radius * 0.4f, p.y - radius * 0.4f,
                                       colour.darker(0.5f),   p.x + radius * 0.5f, p.y + radius * 0.6f, true);
            body.addColour(0.45, colour);
            g.setGradientFill(body);
            g.fillEllipse(p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f);
        }
        g.setColour(colour.darker(0.6f).withAlpha(0.8f));
        g.drawEllipse(p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        const bool darkText = colour.getPerceivedBrightness() > 0.6f;
        g.setColour(darkText ? juce::Colours::black.withAlpha(0.75f) : juce::Colours::white.withAlpha(0.9f));
        g.setFont(Theme::mono(9.5f, juce::Font::bold));
        g.drawText(juce::String(i + 1),
                   juce::Rectangle<float>(p.x - radius, p.y - radius, radius * 2.0f, radius * 2.0f),
                   juce::Justification::centred, false);

        if (n.linked)
        {
            g.setColour(juce::Colours::white.withAlpha(0.85f));
            g.drawEllipse(p.x - radius - 2.5f, p.y - radius - 2.5f,
                          (radius + 2.5f) * 2.0f, (radius + 2.5f) * 2.0f, 1.4f);
        }

        // Selected-node ring — mirrors the main editor's band-selector row so
        // it's clear at a glance which node the Dynamics knobs (or a click
        // here) are currently bound to. A Ctrl+Click multi-selection draws a
        // dimmer ring on every selected node, with a brighter/thicker one
        // reserved for the primary/anchor node (see the class comment above
        // for what "primary" drives — drag/wheel relative-edit source, and
        // the Dynamic Island's target).
        if (i == selectedNode)
        {
            g.setColour(Theme::accent);
            g.drawEllipse(p.x - radius - 4.5f, p.y - radius - 4.5f,
                          (radius + 4.5f) * 2.0f, (radius + 4.5f) * 2.0f, 1.8f);
        }
        else if (multiSelected[size_t(i)])
        {
            g.setColour(Theme::accent.withAlpha(0.55f));
            g.drawEllipse(p.x - radius - 3.5f, p.y - radius - 3.5f,
                          (radius + 3.5f) * 2.0f, (radius + 3.5f) * 2.0f, 1.3f);
        }
    }

    // Threshold ("T") marker — see thresholdMarkerPos/mouseDown/mouseDrag.
    // Only for the selected node, since that's the one the Dynamics panel's
    // Threshold knob is actually bound to right now.
    if (selectedNode >= 0 && thresholdMarkerActive(selectedNode))
    {
        const int i = selectedNode;
        const auto colour = kNodeColours[i];
        const auto nodeP  = nodePos(i);
        const auto tP     = thresholdMarkerPos(i);
        const float tRadius = 8.0f;

        juce::Path stem;
        stem.startNewSubPath(nodeP.x, nodeP.y);
        stem.lineTo(tP.x, tP.y);
        juce::Path dashedStem;
        const float dashLengths[] = { 3.0f, 3.0f };
        juce::PathStrokeType(1.4f).createDashedStroke(dashedStem, stem, dashLengths, 2);
        g.setColour(colour.withAlpha(0.5f));
        g.strokePath(dashedStem, juce::PathStrokeType(1.4f));

        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillEllipse(tP.x - tRadius + 1.0f, tP.y - tRadius + 1.5f, tRadius * 2.0f, tRadius * 2.0f);

        juce::ColourGradient body(colour.brighter(0.6f), tP.x - tRadius * 0.4f, tP.y - tRadius * 0.4f,
                                   colour.darker(0.6f),   tP.x + tRadius * 0.5f, tP.y + tRadius * 0.6f, true);
        g.setGradientFill(body);
        g.fillEllipse(tP.x - tRadius, tP.y - tRadius, tRadius * 2.0f, tRadius * 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.85f));
        g.drawEllipse(tP.x - tRadius, tP.y - tRadius, tRadius * 2.0f, tRadius * 2.0f, 1.2f);

        const bool darkText = colour.getPerceivedBrightness() > 0.6f;
        g.setColour(darkText ? juce::Colours::black.withAlpha(0.8f) : juce::Colours::white.withAlpha(0.95f));
        g.setFont(Theme::mono(9.0f, juce::Font::bold));
        g.drawText("T", juce::Rectangle<float>(tP.x - tRadius, tP.y - tRadius, tRadius * 2.0f, tRadius * 2.0f),
                   juce::Justification::centred, false);
    }

    g.setColour(Theme::textDim.withAlpha(0.7f));
    g.setFont(Theme::label(9.0f));
    g.drawText("Double-click: add node   Drag: freq/gain   Ctrl+Click: multi-select   "
               "Wheel: Q   Right-click: type/Q/link/remove",
               10, getHeight() - 16, getWidth() - 20, 14, juce::Justification::centredLeft, false);
}
