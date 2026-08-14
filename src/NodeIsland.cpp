#include "NodeIsland.h"
#include "Theme.h"

NodeIsland::NodeIsland(VisualCompProcessor& proc) : processor(proc)
{
    qKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    qKnob.setRange(0.15, 10.0);
    qKnob.setSkewFactor(0.4);
    qKnob.setDoubleClickReturnValue(true, 0.9);
    qKnob.setMouseDragSensitivity(700);
    qKnob.onValueChange = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        const float newQ = float(qKnob.getValue());
        const float ratio = (n.q > 0.0001f) ? newQ / n.q : 1.0f;
        n.q = newQ;
        processor.eq.setNode(target, n);
        if (onQRatioChanged) onQRatioChanged(target, ratio);
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(qKnob);

    qLabel.setText("Q", juce::dontSendNotification);
    qLabel.setJustificationType(juce::Justification::centred);
    qLabel.setFont(Theme::label(11.0f));
    qLabel.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    qLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(qLabel);

    // FabFilter Pro-MB style: downward-only (0..-60dB — same value as the
    // graph's "T" marker, see EqPanel::thresholdMarkerPos). Skewed range —
    // see setupThresholdKnobRange() in EqEngine.h.
    thresholdKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    thresholdKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    setupThresholdKnobRange(thresholdKnob);
    thresholdKnob.setDoubleClickReturnValue(true, -20.0);
    thresholdKnob.setMouseDragSensitivity(700);
    thresholdKnob.onValueChange = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        n.thresholdDb = float(thresholdKnob.getValue());
        processor.eq.setNode(target, n);
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(thresholdKnob);

    thresholdLabel.setText("THR", juce::dontSendNotification);
    thresholdLabel.setJustificationType(juce::Justification::centred);
    thresholdLabel.setFont(Theme::label(11.0f));
    thresholdLabel.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    thresholdLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(thresholdLabel);

    // FabFilter Pro-MB style: +/-30dB, clamping how far this band's dynamics
    // can swing the gain (see clampedDynamicGrDb) — and its sign chooses
    // downward vs upward, same as Pro-MB's own Range knob. The range is
    // already symmetric about 0 so no extra skew work is needed to keep
    // 0dB dead-centre. The glow ring fills from centre outward (see
    // AzazelLookAndFeel::drawRotarySlider's "centerFill" property) rather
    // than from one end, matching a bipolar knob's actual travel.
    rangeKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    rangeKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    rangeKnob.setRange(-30.0, 30.0);
    rangeKnob.setDoubleClickReturnValue(true, 0.0);
    rangeKnob.setMouseDragSensitivity(700);
    rangeKnob.getProperties().set("centerFill", true);
    rangeKnob.onValueChange = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        n.rangeDb = float(rangeKnob.getValue());
        // Range's sign directly drives direction, live, every time it's
        // touched — positive is always Upward, negative (or zero) is always
        // Downward. The direction button below still works as a one-off
        // manual override, but the next Range tweak snaps it back in line.
        n.upward = n.rangeDb > 0.0f;
        processor.eq.setNode(target, n);
        directionButton.setToggleState(n.upward, juce::dontSendNotification);
        directionButton.setButtonText(n.upward ? "UP" : "DOWN");
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(rangeKnob);

    rangeLabel.setText("RANGE", juce::dontSendNotification);
    rangeLabel.setJustificationType(juce::Justification::centred);
    rangeLabel.setFont(Theme::label(11.0f));
    rangeLabel.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    rangeLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(rangeLabel);

    // Node's own x-axis position on the graph (20Hz..20kHz, log-skewed about
    // 1kHz to match the graph's own log frequency axis).
    freqKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    freqKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    freqKnob.setRange(20.0, 20000.0);
    freqKnob.setSkewFactorFromMidPoint(1000.0);
    freqKnob.setDoubleClickReturnValue(true, 1000.0);
    freqKnob.setMouseDragSensitivity(700);
    freqKnob.onValueChange = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        n.freqHz = float(freqKnob.getValue());
        processor.eq.setNode(target, n);
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(freqKnob);

    freqLabel.setText("FREQ", juce::dontSendNotification);
    freqLabel.setJustificationType(juce::Justification::centred);
    freqLabel.setFont(Theme::label(11.0f));
    freqLabel.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    freqLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(freqLabel);

    // Node's own y-axis position on the graph. Range matches EqPanel's own
    // kGainRange (+/-18dB); no-op for HPF/LPF/Notch, which stay pinned to
    // 0dB the same way the graph itself refuses to drag them vertically
    // (see EqTypes::isGainlessType).
    gainKnob.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    gainKnob.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    gainKnob.setRange(-18.0, 18.0);
    gainKnob.setDoubleClickReturnValue(true, 0.0);
    gainKnob.setMouseDragSensitivity(700);
    gainKnob.getProperties().set("centerFill", true);
    gainKnob.onValueChange = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        if (EqTypes::isGainlessType(n.type)) return;
        n.gainDb = float(gainKnob.getValue());
        processor.eq.setNode(target, n);
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(gainKnob);

    gainLabel.setText("GAIN", juce::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    gainLabel.setFont(Theme::label(11.0f));
    gainLabel.setColour(juce::Label::textColourId, Theme::text.withAlpha(0.78f));
    gainLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(gainLabel);

    // Manual override for the direction Range's sign already implies (see
    // EqNodeState::upward).
    directionButton.setClickingTogglesState(true);
    directionButton.setColour(juce::TextButton::buttonColourId,   Theme::charcoal);
    directionButton.setColour(juce::TextButton::buttonOnColourId, Theme::accent);
    directionButton.setColour(juce::TextButton::textColourOffId,  Theme::textDim);
    directionButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::black);
    directionButton.onClick = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        n.upward = directionButton.getToggleState();
        processor.eq.setNode(target, n);
        directionButton.setButtonText(n.upward ? "UP" : "DOWN");
        if (onNodeEdited) onNodeEdited(target);
    };
    addAndMakeVisible(directionButton);

    // Toggles EqNodeState::linked — same field the graph's right-click
    // "Link to Compressor" menu item controls. AzazelLookAndFeel's
    // drawButtonBackground ignores buttonColourId/buttonOnColourId for
    // ordinary buttons (only the band-select row's "nodeSelect"-tagged
    // buttons get a solid toggle fill), so — same as directionButton — the
    // on/off state has to read through text colour alone: dim when
    // unlinked, bright accent when linked.
    compButton.setClickingTogglesState(true);
    compButton.setButtonText("COMP");
    compButton.setColour(juce::TextButton::textColourOffId, Theme::textDim);
    compButton.setColour(juce::TextButton::textColourOnId,  Theme::accent);
    compButton.onClick = [this]
    {
        if (target < 0) return;
        auto n = processor.eq.getNode(target);
        n.linked = compButton.getToggleState();
        processor.eq.setNode(target, n);
        if (onNodeEdited) onNodeEdited(target);
        if (onLinkedToggled) onLinkedToggled(target, n.linked);
    };
    addAndMakeVisible(compButton);

    typeButton.setColour(juce::TextButton::buttonColourId,  Theme::charcoal);
    typeButton.setColour(juce::TextButton::textColourOffId, Theme::accent);
    typeButton.onClick = [this] { showTypeMenu(); };
    addAndMakeVisible(typeButton);

    setVisible(false);
}

void NodeIsland::setTargetNode(int index)
{
    if (index != target) userMoved = false;   // new node: re-dock under it by default
    target = index;
    setVisible(index >= 0);
    if (index >= 0) refreshFromProcessor();
}

void NodeIsland::mouseDown(const juce::MouseEvent& e)
{
    // Only reached for clicks that land on the Island's own background --
    // every knob/button consumes its own clicks, so this is naturally
    // "anywhere that's not a button or input".
    dragAnchor = e.getPosition();
}

void NodeIsland::mouseDrag(const juce::MouseEvent& e)
{
    auto* parent = getParentComponent();
    if (parent == nullptr) return;

    auto pos = getPosition() + (e.getPosition() - dragAnchor);
    const auto bounds = parent->getLocalBounds();
    pos.x = juce::jlimit(bounds.getX(), juce::jmax(bounds.getX(), bounds.getRight() - getWidth()),  pos.x);
    pos.y = juce::jlimit(bounds.getY(), juce::jmax(bounds.getY(), bounds.getBottom() - getHeight()), pos.y);
    setTopLeftPosition(pos);
    userMoved = true;
}

void NodeIsland::refreshFromProcessor()
{
    if (target < 0) return;
    const auto n = processor.eq.getNode(target);
    if (!qKnob.isMouseButtonDown())
        qKnob.setValue(n.q, juce::dontSendNotification);
    if (!thresholdKnob.isMouseButtonDown())
        thresholdKnob.setValue(n.thresholdDb, juce::dontSendNotification);
    if (!rangeKnob.isMouseButtonDown())
        rangeKnob.setValue(n.rangeDb, juce::dontSendNotification);
    if (!freqKnob.isMouseButtonDown())
        freqKnob.setValue(n.freqHz, juce::dontSendNotification);
    // HPF/LPF/Notch have no static gain to edit -- pin the knob at 0dB and
    // disable it outright (rather than merely ignoring drags in
    // onValueChange) so it can't visually drift away from 0 mid-drag only to
    // snap back on the next poll tick once released.
    const bool gainEditable = !EqTypes::isGainlessType(n.type);
    gainKnob.setEnabled(gainEditable);
    if (!gainKnob.isMouseButtonDown())
        gainKnob.setValue(gainEditable ? n.gainDb : 0.0f, juce::dontSendNotification);
    directionButton.setToggleState(n.upward, juce::dontSendNotification);
    directionButton.setButtonText(n.upward ? "UP" : "DOWN");
    compButton.setToggleState(n.linked, juce::dontSendNotification);
    typeButton.setButtonText(EqTypes::kNames[juce::jlimit(0, EqTypes::kNumTypes - 1, n.type)]);
}

void NodeIsland::showTypeMenu()
{
    if (target < 0) return;
    const int t = target;
    const auto n = processor.eq.getNode(t);

    juce::PopupMenu menu;
    for (int i = 0; i < EqTypes::kNumTypes; ++i)
        menu.addItem(100 + i, EqTypes::kNames[i], true, n.type == i);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(typeButton),
        [this, t](int result)
        {
            if (result < 100) return;
            auto node = processor.eq.getNode(t);
            node.type = result - 100;
            processor.eq.setNode(t, node);
            if (onNodeEdited) onNodeEdited(t);
            if (t == target) refreshFromProcessor();
        });
}

void NodeIsland::resized()
{
    auto r = getLocalBounds().reduced(9, 8);

    auto knobRow = r.removeFromTop(66);
    auto qArea = knobRow.removeFromLeft(54);
    qLabel.setBounds(qArea.removeFromTop(13));
    qKnob.setBounds(qArea);

    knobRow.removeFromLeft(6);
    auto thrArea = knobRow.removeFromLeft(54);
    thresholdLabel.setBounds(thrArea.removeFromTop(13));
    thresholdKnob.setBounds(thrArea);

    knobRow.removeFromLeft(6);
    auto rangeArea = knobRow.removeFromLeft(54);
    rangeLabel.setBounds(rangeArea.removeFromTop(13));
    rangeKnob.setBounds(rangeArea);

    knobRow.removeFromLeft(6);
    auto freqArea = knobRow.removeFromLeft(54);
    freqLabel.setBounds(freqArea.removeFromTop(13));
    freqKnob.setBounds(freqArea);

    knobRow.removeFromLeft(6);
    auto gainArea = knobRow.removeFromLeft(54);
    gainLabel.setBounds(gainArea.removeFromTop(13));
    gainKnob.setBounds(gainArea);

    knobRow.removeFromLeft(6);
    compButton.setBounds(knobRow.getX(), knobRow.getCentreY() - 13, knobRow.getWidth(), 26);

    r.removeFromTop(6);
    auto dirArea = r.removeFromLeft(90);
    directionButton.setBounds(dirArea.getX(), r.getCentreY() - 13, dirArea.getWidth(), 26);

    r.removeFromLeft(6);
    typeButton.setBounds(r.getX(), r.getCentreY() - 13, r.getWidth(), 26);
}

void NodeIsland::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.fillRoundedRectangle(r.translated(0.0f, 2.0f), 10.0f);

    g.setColour(Theme::bgRaised);
    g.fillRoundedRectangle(r, 10.0f);

    g.setColour(Theme::accent.withAlpha(0.7f));
    g.drawRoundedRectangle(r, 10.0f, 1.4f);
}
