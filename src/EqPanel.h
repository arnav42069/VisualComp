#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "NodeIsland.h"
#include <array>
#include <functional>
#include <vector>

// Docked multi-node parametric EQ panel. Extends to the left of the main
// plugin body when toggled from the header EQ button, matching the full
// height of the rest of the interface.
//
// Interaction:
//   - Double-click empty graph space -> activates the next free node at
//     that frequency/gain.
//   - Drag a node's centre ball -> left/right changes frequency (log
//     scale), up/down changes gain. This node's detector edges (if linked)
//     move with it, keeping their octave offsets from the new frequency.
//   - Drag a linked node's low/high edge flag (top of the graph) -> resizes
//     that side of the band's detector passband independently; the other
//     edge and the centre frequency are untouched.
//   - Right-click a node -> menu: filter type, Q preset, LINK TO COMPRESSOR
//     toggle, remove.
//   - Mouse wheel over a node -> adjusts Q (tone-shaping filter width;
//     independent of the linked detector's low/high edges).
//   - Ctrl/Cmd+Click a node -> toggles it into/out of a multi-selection
//     (plain click resets the selection to just that node). Dragging the
//     most-recently-clicked node's centre (freq/gain), or turning its Q via
//     wheel/Dynamic Island, applies the same relative change (dB delta for
//     gain, multiplicative ratio for freq/Q) to every other selected node,
//     each clamped to its own valid range. Right-click actions (type/link/
//     remove) always apply to just the clicked node.
//   - Clicking a node shows a floating "Dynamic Island" (see NodeIsland.h)
//     above it with an interactive Q knob, an Upward/Downward compression
//     toggle, and a filter-type picker.

// Draws its own vector X rather than relying on a Unicode glyph (Futura, the
// UI font, has no glyph for U+2715 and would render an empty box).
class EqCloseButton : public juce::TextButton
{
public:
    void paintButton(juce::Graphics&, bool shouldDrawButtonAsHighlighted,
                     bool shouldDrawButtonAsDown) override;
};

class EqPanel : public juce::Component, private juce::Timer
{
public:
    explicit EqPanel(VisualCompProcessor& proc);
    ~EqPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

    static const juce::Colour kNodeColours[kMaxEqNodes];

    std::function<void()>    onCloseRequested;
    std::function<void(int)> onNodeSelected;   // fired when a node is clicked directly in the graph
    // Fired whenever a node's state changes from *any* source this panel
    // knows about — graph drags (incl. the "T" threshold marker), the
    // right-click menu, and (relayed through nodeIsland) the Dynamic
    // Island's own knobs/buttons. Lets the main editor repaint its
    // Dynamics-pane progress bars immediately instead of waiting for its
    // own polling timer to notice.
    std::function<void(int)> onNodeEdited;

    // Mirrors the main editor's band-selector-row selection so a click on a
    // numbered button (1-8) also highlights the corresponding node here.
    // Resets any multi-selection down to just this one node.
    void setSelectedNode(int i);

private:
    void timerCallback() override;

    juce::Rectangle<float> graphArea() const;
    float xToFreq(float x) const;
    float freqToX(float freq) const;
    float yToGainDb(float y) const;
    float gainDbToY(float gainDb) const;
    juce::Point<float> nodePos(int i) const;
    // Detector edge (bwLowOct/bwHighOct) helpers shared by drag, click-to-jump,
    // and new-node snapping. `edge` is 0 = low, 1 = high.
    float edgeHzOf(int node, int edge) const;
    void  setEdgeHz(int node, int edge, float hz);
    // Snap-junction bonding: once two nodes' edges are bonded, they are kept
    // at the same frequency by every subsequent move (see propagateJunctions)
    // until the edge is dragged far enough from its partner to unbond.
    struct EdgeRef { int node = -1; int edge = -1; };
    std::array<std::array<EdgeRef, 2>, kMaxEqNodes> snapPartner;
    void  linkEdges(int nodeA, int edgeA, int nodeB, int edgeB);
    void  unlinkEdge(int node, int edge);
    // Snaps (node,edge) to the nearest other linked node's edge within
    // snapRadiusHz, bonding them on success (or breaking any existing bond on
    // failure); returns the resulting target frequency (== rawHz if no snap).
    float trySnapEdge(int node, int edge, float rawHz, float snapRadiusHz = 100.0f);
    // Moves (node,edge) to point p's frequency (snapping/bonding as above)
    // and applies it immediately -- used by both the click-to-jump on
    // mouseDown and continued dragging in mouseDrag.
    void  moveEdgeTo(int node, int edge, juce::Point<float> p);
    // After node i's centre frequency changes, pushes any bonded partner
    // edge(s) to match node i's own (now-shifted) edge frequency, so an
    // existing junction survives the move instead of drifting apart.
    void  propagateJunctions(int node);
    // FabFilter Pro-MB-style threshold handle: a small "T" marker at the
    // node's frequency but at thresholdDb's height on the gain axis, shown
    // only for the selected node while it's linked and MULTIBAND COMP is
    // on (i.e. while the Dynamics panel's Threshold knob is actually bound
    // to it). Vertical-drag-only and two-way linked to that knob — see
    // mouseDown/mouseDrag and PluginEditor::bandThresholdKnob.
    juce::Point<float> thresholdMarkerPos(int i) const;
    bool  thresholdMarkerActive(int i) const;
    int   findNodeNear(juce::Point<float> p, float radius = 12.0f) const;
    // Hit-tests a linked node's low/high detector-edge border -- the whole
    // vertical line spanning the graph's height (see paint()'s shaded band
    // strip), not just the small flag triangle at the top, so clicking
    // anywhere near that border grabs it. Returns the node index and sets
    // whichEdge to 0 (low) or 1 (high), or -1/-1 if none hit.
    int   findEdgeNear(juce::Point<float> p, int& whichEdge, float radius = 12.0f) const;
    int   findFreeNode() const;
    void  pushNode(int i);   // writes localNodes[i] to the processor's ParametricEq
    void  showNodeMenu(int i);
    void  createNodeAt(juce::Point<float> p);
    float sampleRateForDisplay() const;   // processor's rate, or a sane default pre-prepare

    // Multi-select helpers (see the class comment above for the Ctrl+Click
    // and relative-edit model). applyRelativeGainFreq/applyRelativeQ/etc.
    // propagate a change on node `anchor` to the rest of multiSelected.
    void  selectOnly(int i);
    void  toggleMultiSelect(int i);
    void  applyRelativeGainFreq(int anchor, float deltaGainDb, float freqRatio);
    void  applyRelativeQ(int anchor, float qRatio);
    void  applyRelativeThreshold(int anchor, float deltaDb);
    void  applyRelativeRange(int anchor, float deltaDb);
    void  updateIslandBounds();
    void  updateSpectrum();

    VisualCompProcessor& processor;
    std::array<EqNodeState, kMaxEqNodes> localNodes;

    int   dragIndex   = -1;
    int   dragEdge    = -1;   // -1 = dragging centre (or nothing); 0/1 = low/high edge of dragIndex
    bool  dragThreshold = false;   // dragging the "T" threshold marker for dragIndex
    bool  dragMoved   = false;
    int   hoverNode   = -1;
    int   selectedNode = -1;   // primary/anchor node; set via setSelectedNode() or a click; -1 = none
    std::array<bool, kMaxEqNodes> multiSelected {};   // Ctrl+Click multi-selection set

    EqCloseButton    closeButton;
    NodeIsland       nodeIsland;

    // EQ-only dry/wet mix, independent of the main editor's overall Mix
    // knob -- blends just this panel's tone-shaping, bound to the "eqMix"
    // APVTS parameter. Compact knob + label in the header, left of
    // closeButton (see resized()).
    juce::Slider     eqMixKnob;
    juce::Label      eqMixLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqMixAttachment;

    // Message-thread FFT of the existing rolling input history: no audio
    // thread allocation, locking, or extra real-time work for the display.
    static constexpr int kSpectrumFftOrder = 10;
    static constexpr int kSpectrumFftSize  = 1 << kSpectrumFftOrder;
    static constexpr int kSpectrumBands    = 48;
    juce::dsp::FFT spectrumFft { kSpectrumFftOrder };
    juce::dsp::WindowingFunction<float> spectrumWindow { kSpectrumFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> spectrumFftBuffer { size_t(kSpectrumFftSize) * 2, 0.0f };
    std::array<float, kSpectrumBands> spectrumDb {};
    int spectrumTick = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqPanel)
};
