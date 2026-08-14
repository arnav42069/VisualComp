#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <functional>
#include <array>

// Small rounded floating popup ("Dynamic Island") that EqPanel shows above
// the currently-selected EQ node. Hosts the node-level controls that used
// to live in (Q) or never existed in (upward/downward, filter type,
// Threshold, Range) the main editor's lower Dynamics pane — see CLAUDE.md's
// docked-panel / context-sensitive-knob patterns for why these are manually
// wired rather than APVTS SliderAttachments (there's no host parameter
// behind them).
//
// Threshold and Range mirror FabFilter Pro-MB: Threshold is downward-only
// (-inf..0dB — the graph's "T" marker is the same value, see EqPanel), and
// Range (+/-30dB) both clamps how far this band's dynamics can swing the
// gain AND (via its sign) chooses downward vs upward — positive Range
// auto-engages Upward, mirrored by the direction button below, which
// remains the manual override. Freq mirrors the node's own x-axis position
// on the graph (EqNodeState::freqHz, 20Hz..20kHz log); Gain mirrors the
// node's y-axis position (EqNodeState::gainDb, +/-18dB, same range as the
// graph's vertical drag — disabled for HPF/LPF/Notch, see
// EqTypes::isGainlessType). Comp toggles
// EqNodeState::linked (whether this band feeds the multiband-aware
// compressor detector) — the same field the graph's right-click "Link to
// Compressor" menu item controls.
class NodeIsland : public juce::Component
{
public:
    static constexpr int kWidth  = 372;
    static constexpr int kHeight = 118;

    explicit NodeIsland(VisualCompProcessor& proc);

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;

    // -1 hides the island; otherwise shows it reflecting node `index`.
    // Switching to a *different* node index re-docks the Island under it
    // (clearing any manual drag from the previous node); re-passing the
    // already-target index (e.g. re-clicking the same node) leaves a manual
    // drag alone.
    void setTargetNode(int index);
    int  targetNode() const { return target; }

    // True once the user has dragged the Island away from its default
    // docked position (see mouseDrag) -- EqPanel::updateIslandBounds() stops
    // auto-recentring it under the node while this is set, so the drag
    // isn't immediately fought/undone by the next poll-timer tick.
    bool wasManuallyPositioned() const { return userMoved; }

    // Re-syncs the displayed Q/threshold/range/direction/type from the
    // processor (called by EqPanel's existing poll timer, mirroring how it
    // refreshes localNodes).
    void refreshFromProcessor();

    // Sync position state to/from processor's persisted state
    void syncPositionsFromProcessor();
    void syncPositionsToProcessor();

    // Fired after a Q-knob drag changes the target node's Q, with the
    // multiplicative ratio applied (newQ / oldQ) — EqPanel uses this to
    // propagate the same ratio across a multi-selection, the same way it
    // already does for the graph's own mouse-wheel Q adjustment.
    std::function<void(int nodeIndex, float qRatio)> onQRatioChanged;

    // Fired after threshold/range/freq/gain knob changes, with the delta or
    // ratio applied — EqPanel uses these to propagate changes across a
    // multi-selection (additive delta for threshold/range/gain, multiplicative
    // ratio for freq, same as graph drag/wheel operations).
    std::function<void(int nodeIndex, float deltaDb)> onThresholdChanged;
    std::function<void(int nodeIndex, float deltaDb)> onRangeChanged;
    std::function<void(int nodeIndex, float freqRatio)> onFreqChanged;
    std::function<void(int nodeIndex, float deltaDb)> onGainChanged;

    // Fired after any knob/button here writes a new value straight to
    // processor.eq (Q, Threshold, Range, Freq, direction, Comp) — EqPanel
    // relays this to the main editor so its Dynamics-pane progress bars can
    // repaint immediately rather than waiting for a poll tick.
    std::function<void(int nodeIndex)> onNodeEdited;

    // Fired when the comp (linked) button is toggled, with the new linked state.
    // EqPanel uses this to snap edges when a node becomes linked.
    std::function<void(int nodeIndex, bool newLinkedState)> onLinkedToggled;

private:
    void showTypeMenu();

    VisualCompProcessor& processor;
    int target = -1;

    // Manual-drag state (see mouseDown/mouseDrag). dragAnchor is the mouse
    // position, relative to this component, at the start of the drag --
    // clicks landing on a child knob/button/label never reach here (they
    // either consume the event themselves or, for the labels, pass through
    // with setInterceptsMouseClicks(false, false) only after already being
    // routed to whichever component is actually under the cursor), so this
    // only fires for clicks on the Island's own background/padding.
    juce::Point<int> dragAnchor;
    bool userMoved = false;

    // Per-node manual position persistence: tracks where the Island was
    // dragged to for each node, so switching away and back re-applies the
    // saved position. Array indexed by node index (0..kMaxEqNodes-1).
    // Uses VisualCompProcessor::IslandPosition to stay in sync with processor state.
    static constexpr int kMaxEqNodes = 8;  // Must match EqEngine.h
    std::array<VisualCompProcessor::IslandPosition, kMaxEqNodes> savedPositions {};

    // Called after dragging to save the current position to the saved array
    void saveCurrentNodePosition();
    // Called when setting a target node to restore its saved position if any
    void restoreSavedPosition();

    juce::Slider     qKnob;
    juce::Label      qLabel;
    juce::Slider     thresholdKnob;
    juce::Label      thresholdLabel;
    juce::Slider     rangeKnob;
    juce::Label      rangeLabel;
    juce::Slider     freqKnob;
    juce::Label      freqLabel;
    juce::Slider     gainKnob;
    juce::Label      gainLabel;
    juce::TextButton directionButton;
    juce::TextButton compButton;
    juce::TextButton typeButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeIsland)
};
