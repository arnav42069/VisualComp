#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UndoableParameterAction.h"
#include "WaveformDisplay.h"
#include "VuMeter.h"
#include "GrCurveDisplay.h"
#include "EqPanel.h"
#include "LevelMeter.h"
#include "DemoModeIndicator.h"
#include <functional>
#include <vector>

//==============================================================================
// Slider that supports Ctrl (or Cmd) held for fine adjustment + undo/redo.
// Call setUndoManagerAndParamId() to enable undo tracking for this slider.
//==============================================================================
class DragSlider : public juce::Slider
{
public:
    void setUndoManagerAndParamId(VisualCompUndo::UndoRedoManager* manager,
                                   juce::AudioProcessorValueTreeState* apvts,
                                   const juce::String& paramId)
    {
        undoManager = manager;
        apvtsPtr = apvts;
        parameterID = paramId;
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        baseSensitivity = getMouseDragSensitivity();
        if (e.mods.isCtrlDown() || e.mods.isCommandDown())
            setMouseDragSensitivity(baseSensitivity * 8);

        // Capture starting value for undo action
        if (auto param = apvtsPtr ? apvtsPtr->getParameter(parameterID) : nullptr)
            startValue = param->getValue();

        // Fire callback to allow capturing additional context (e.g., EQ node state)
        if (onMouseDownCallback)
            onMouseDownCallback();

        juce::Slider::mouseDown(e);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        juce::Slider::mouseUp(e);
        setMouseDragSensitivity(baseSensitivity);

        // Create undo action if the value changed
        if (undoManager && apvtsPtr && !parameterID.isEmpty())
        {
            if (auto param = apvtsPtr->getParameter(parameterID))
            {
                float endValue = param->getValue();
                if (std::abs(endValue - startValue) > 1e-5f)
                {
                    auto action = std::make_unique<VisualCompUndo::UndoableParameterAction>(
                        *apvtsPtr, parameterID, endValue, startValue);
                    undoManager->perform(std::move(action));
                }
            }
        }

        // Fire custom undo callback if provided
        if (onUndoEditComplete)
            onUndoEditComplete(startValue, getValue());
    }

public:
    // Callback fired when mouse button is pressed, before any value changes
    std::function<void()> onMouseDownCallback;

    // Callback for custom undo actions (e.g., EQ node edits that aren't APVTS parameters)
    std::function<void(float oldValue, float newValue)> onUndoEditComplete;

private:
    int baseSensitivity = 1000;
    float startValue = 0.0f;
    VisualCompUndo::UndoRedoManager* undoManager = nullptr;
    juce::AudioProcessorValueTreeState* apvtsPtr = nullptr;
    juce::String parameterID;
};

//==============================================================================
// Invisible clickable region (used for the logo menu hot-spot).
//==============================================================================
// Invisible: no highlight of any kind, just a hand cursor and a click.
class ClickZone : public juce::Component
{
public:
    ClickZone() { setMouseCursor(juce::MouseCursor::PointingHandCursor); }

    std::function<void()> onClick;

    void mouseUp(const juce::MouseEvent&) override { if (onClick) onClick(); }
};

//==============================================================================
// Step-by-step onboarding overlay: dims the panel, spotlights one control at a
// time and shows a small bubble with NEXT / SKIP.
//==============================================================================
class HelpOverlay : public juce::Component
{
public:
    struct Step
    {
        juce::Rectangle<int> target;   // empty = no spotlight, centred bubble
        juce::String         title;
        juce::String         body;
    };

    HelpOverlay();
    ~HelpOverlay() override;

    void setSteps(std::vector<Step> s) { steps = std::move(s); }
    void start()               { startAt(0); }
    void startAt(int stepIndex);

    std::function<void()> onFinished;   // called on skip or completion

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    class OverlayLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&,
                                  bool, bool) override;
        juce::Font getTextButtonFont(juce::TextButton&, int height) override;
    };

    void showStep(int index);
    void finish();
    juce::Rectangle<int> computeBubble() const;

    std::vector<Step> steps;
    int  current = 0;

    juce::TextButton nextButton { "NEXT" };
    juce::TextButton skipButton { "SKIP" };
    OverlayLookAndFeel overlayLaf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HelpOverlay)
};

//==============================================================================
// Full-window "enlarge" overlay opened via right-click on either waveform
// display (WaveformDisplay::onRightClick) -- shows a fresh, independently-
// paused large WaveformDisplay for the input, the output, or both, reading
// the same WaveformBuffers as the normal-size displays. Built/torn down on
// demand (see VisualCompEditor::showEnlargeOverlay()), same unique_ptr
// on-demand pattern as eqPanel below, rather than a permanently-live member
// like HelpOverlay, since its content (one display vs two) changes per use.
//==============================================================================
class WaveEnlargeOverlay : public juce::Component
{
public:
    WaveEnlargeOverlay(VisualCompProcessor& proc, bool showInput, bool showOutput);
    ~WaveEnlargeOverlay() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override;   // click on the dimmed backdrop closes it
    bool keyPressed(const juce::KeyPress&) override;

    std::function<void()> onClose;

private:
    std::unique_ptr<WaveformDisplay> bigInput, bigOutput;
    juce::TextButton closeButton { "CLOSE" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveEnlargeOverlay)
};

//==============================================================================
class AzazelLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AzazelLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider&) override;

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;

    // Opt-in via the button's "microCaps" property (see setupTextButton):
    // renders letterspaced micro-caps, like a hardware control's silkscreen
    // legend, instead of the default plain button label. Buttons that show
    // a user-facing VALUE rather than a control's name (the preset name
    // button) don't set the property and fall back to the base LAF's
    // ordinary text draw.
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;

    void drawLabel(juce::Graphics&, juce::Label&) override;
    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int height) override;

    // A slider's numeric read-out Label is created here (not sized via
    // getLabelFont() — LookAndFeel_V2::createSliderTextBox never calls
    // setFont(), so the base font is whatever this sets it to once, up
    // front). Narrow knob-row slots (Attack/Release/Q sharing the row
    // while an EQ node is linked to the compressor) get a smaller size so
    // the value fits without ellipsizing.
    juce::Label* createSliderTextBox(juce::Slider&) override;

    void drawPopupMenuBackground(juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem(juce::Graphics&, const juce::Rectangle<int>& area,
                           bool isSeparator, bool isActive, bool isHighlighted,
                           bool isTicked, bool hasSubMenu,
                           const juce::String& text,
                           const juce::String& shortcutKeyText,
                           const juce::Drawable* icon,
                           const juce::Colour* textColour) override;
};

//==============================================================================
// UndoRedoNotification: Brief toast notification for undo/redo feedback
//==============================================================================
class UndoRedoNotification : public juce::Component, private juce::Timer
{
public:
    UndoRedoNotification()
    {
        setInterceptsMouseClicks(false, false);
    }

    void showNotification(const juce::String& actionName, bool isUndo)
    {
        notificationText = (isUndo ? "Undo: " : "Redo: ") + actionName;
        alpha = 1.0f;
        startTimer(50);  // 50ms refresh for fade animation
    }

    void paint(juce::Graphics& g) override
    {
        if (alpha < 0.01f)
            return;

        // Semi-transparent dark background rounded rectangle
        g.setColour(juce::Colour(0xff000000).withAlpha(0.8f * alpha));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);

        // Orange accent border
        g.setColour(juce::Colour(0xff7a1f).withAlpha(0.7f * alpha));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.5f);

        // Text
        g.setColour(juce::Colours::white.withAlpha(0.95f * alpha));
        g.setFont(juce::Font(11.0f));
        g.drawText(notificationText, getLocalBounds(), juce::Justification::centred, true);
    }

private:
    void timerCallback() override
    {
        alpha -= 0.08f;  // Fade out over ~600ms (50ms * 12 steps)
        repaint();

        if (alpha <= 0.0f)
            stopTimer();
    }

    juce::String notificationText;
    float alpha = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UndoRedoNotification)
};

//==============================================================================
class VisualCompEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VisualCompEditor(VisualCompProcessor&);
    ~VisualCompEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;   // drives the reactive title glow
    // Global click-anywhere-to-deselect: registered on `this` with nested
    // children included (see constructor), so a click landing anywhere in
    // the editor -- other than inside the EQ panel/Dynamic Island (which
    // manage node selection themselves) or the band-context knobs/selector
    // buttons currently editing the selected node -- drops back to the
    // default (unlinked) broadband compressor knobs.
    void mouseDown(const juce::MouseEvent&) override;

    // Handle Ctrl+Z (undo) and Ctrl+Y/Ctrl+Shift+Z (redo) keyboard shortcuts
    bool keyPressed(const juce::KeyPress& key) override;

    VisualCompProcessor& audioProcessor;

    AzazelLookAndFeel laf;

    // Needed for any setTooltip() text (e.g. the preset button's "Preset by
    // <author>", see setPresetAuthor()) to actually pop up on hover -- a
    // Component's tooltip string alone does nothing without one of these
    // live somewhere in its hierarchy.
    juce::TooltipWindow tooltipWindow { this };

    // Demo mode watermark indicator
    DemoModeIndicator demoModeIndicator { audioProcessor.licenseManager };

    // Undo/redo notification feedback
    UndoRedoNotification undoRedoNotification;

    WaveformDisplay inputDisplay;
    WaveformDisplay outputDisplay;
    VuMeter         vuMeter;
    GrCurveDisplay  curveDisplay;
    LevelMeter      levelMeter;

    // Vertical faders
    DragSlider  gainInFader, gainOutFader;
    juce::Label gainInFaderLabel, gainOutFaderLabel;

    // Right-column toggles
    juce::ToggleButton limiterButton;
    juce::Label        limiterLabel;
    juce::ToggleButton sidechainButton;
    juce::Label        sidechainLabel;
    juce::ToggleButton autoGainButton;

    // Header controls
    juce::ToggleButton bypassButton;
    juce::TextButton   modeButton;
    juce::TextButton   eqButton;
    juce::TextButton   presetButton, presetPrev, presetNext, presetSave, autoAnalyzeButton;
    juce::TextButton   demoPlayButton;
    bool               testDemoUiEnabled = false;
    ClickZone          logoZone;

    // Always-visible free-text Author field — occupies presetButton's old
    // slot in the preset strip now that presetButton itself lives in the
    // title bar (see resized()). Wired to audioProcessor.currentPresetAuthor
    // via setPresetAuthor(), same as presetButton's tooltip.
    juce::TextEditor   presetAuthorEditor;

    juce::String currentModeName { "VCA" };

    // Knobs
    DragSlider  thresholdKnob, kneeKnob, ratioKnob, attackKnob, releaseKnob;
    juce::Label thresholdLabel, kneeLabel, ratioLabel, attackLabel, releaseLabel;
    DragSlider  mixKnob;

    // Clipping mode — sits with the other output-stage controls. Clicking it
    // opens a popup rather than cycling: the menu also carries the
    // oversampling factor, which has no control of its own anywhere else in
    // the interface (see showClipModeMenu()).
    juce::TextButton clipModeButton;
    void showClipModeMenu();
    void applyClipMode(int modeIndex);
    void applyOversamplingIndex(int index);

    // Docked parametric EQ panel
    std::unique_ptr<EqPanel> eqPanel;
    bool eqPanelVisible = false;
    void toggleEqPanel();

    // Transfer Curve / Gain Reduction meter column: collapsed (no window
    // space reserved) by default, slides out — growing the window, same
    // docked-panel setSize() mechanism as the EQ panel — only while this
    // toggle is on (see VuMeter/GrCurveDisplay members below).
    juce::TextButton curveGrButton;
    bool curveGrVisible = false;
    void toggleCurveGrPanel();

    // Total editor width for the current eqPanelVisible/curveGrVisible
    // state — the single source of truth both toggles and the constructor
    // pass to setSize(), so the two docked panels' width deltas never drift
    // out of sync with each other.
    int totalEditorWidth() const;

    // Band-selector row above the Dynamics knobs: clicking a band button
    // swaps Attack/Release to that EQ node's own detector timing.
    // Threshold/Knee/Ratio stay global — there is no per-band equivalent in
    // this architecture. Q is no longer edited here — see EqPanel (mouse
    // wheel / right-click Q submenu) and NodeIsland (the Dynamic Island's
    // interactive Q knob).
    std::array<juce::TextButton, kMaxEqNodes> bandButtons;
    int  selectedBand = -1;   // -1 == Attack/Release show the global compressor
    void refreshBandButtons();
    void selectBand(int i);

    DragSlider bandAttackKnob, bandReleaseKnob;

    // Captured at mouseDown for band knob edits: the old EQ node state before
    // the drag starts, used by onUndoEditComplete to create proper undo actions
    EqNodeState oldBandNodeStateAtMouseDown {};

    // Threshold/Knee/Ratio band-context counterparts — same pattern as
    // bandAttackKnob/bandReleaseKnob above: manually wired to the selected
    // EQ node's own dynamics rather than an APVTS parameter, sharing their
    // on-screen slot with thresholdKnob/kneeKnob/ratioKnob. Threshold's
    // range extends into positive dB — see its onValueChange in the .cpp,
    // which auto-engages that node's upward-compression mode when positive.
    DragSlider bandThresholdKnob, bandKneeKnob, bandRatioKnob;

    // Onboarding
    HelpOverlay helpOverlay;

    using Attachment       = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    Attachment gainInAtt, gainOutAtt;
    Attachment thresholdAtt, kneeAtt, ratioAtt, attackAtt, releaseAtt;
    Attachment mixAtt;
    ButtonAttachment limiterAtt;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AlertWindow> wizardWindow;

    // Setup helpers
    void setupKnob(DragSlider& knob, juce::Label& label,
                   const juce::String& text, const juce::String& suffix,
                   const juce::String& paramId);
    void setupFader(DragSlider& fader, juce::Label& label, const juce::String& text,
                    const juce::String& paramId);
    void setupTextButton(juce::TextButton& b, const juce::String& text, bool microCaps = true);
    void setupToggle(juce::ToggleButton& b, const juce::String& text);

    // Mode
    void showModeMenu();
    void applyMode(int modeIndex);

    // Presets
    void showPresetMenu();
    void applyFactoryPreset(int index);
    void stepPreset(int delta);
    void saveUserPreset();
    void launchSavePresetFileChooser(const juce::String& author);
    void loadUserPreset(const juce::File& file);
    void setPresetName(const juce::String& name);
    void setPresetAuthor(const juce::String& author);
    static juce::File getUserPresetDir();

    // Gates any operation that would replace the live EQ (loading a user
    // preset, applying a factory preset, running Smart Master+) behind a
    // "keep current EQ?" confirm dialog, but only if the EQ has actually
    // been hand-edited since the last preset load/generation
    // (audioProcessor.eqDirtySincePreset). If not dirty, applyFn runs
    // immediately with no dialog. Either way the dirty flag is cleared once
    // the outcome is resolved -- both "replace" and "keep current" settle
    // the divergence from a preset context. onSettled (optional) always
    // runs once the outcome is known, whether or not applyFn actually ran --
    // for callers (runAutoAnalyze) with follow-up work that shouldn't race
    // the confirm dialog, e.g. showing a completion message.
    void confirmAndApplyEq(std::function<void()> applyFn,
                            std::function<void()> onSettled = nullptr);
    // Mirrors setStateInformation's per-node EQ property read loop, applying
    // a saved .vcpreset's EQ nodes (property prefix "eqN_...") onto the live
    // ParametricEq. Called (wrapped in confirmAndApplyEq) from loadUserPreset().
    void applyEqFromState(const juce::ValueTree& state);

    // Waveform enlarge overlay -- right-click on either WaveformDisplay
    // (wired via its onRightClick) opens a themed popup menu, same styling
    // as showPresetMenu(), offering "enlarge this display" / "enlarge both";
    // showEnlargeOverlay builds the on-demand WaveEnlargeOverlay accordingly.
    std::unique_ptr<WaveEnlargeOverlay> enlargeOverlay;
    void showEnlargeMenu(WaveformDisplay& source, bool sourceIsInput);
    void showEnlargeOverlay(bool showInput, bool showOutput);

    // Auto-Analyze / Smart Master+ wizard
    void showAutoAnalyzeGenreStep();
    void showAutoAnalyzeLufsStep(const juce::String& genre);
    // Arms the processor's ~9s capture buffer and shows a progress dialog
    // that only completes once that much real audio has actually passed
    // through the plugin -- runAutoAnalyze then analyzes that whole excerpt
    // rather than an instantaneous snapshot. pollSmartMasterCapture (driven
    // by the existing 30Hz timerCallback) advances/closes the dialog.
    void beginSmartMasterCapture(const juce::String& genre, float targetLufs);
    void pollSmartMasterCapture();
    void runAutoAnalyze(const juce::String& genre, float targetLufs);

    // Full analysis of the just-captured Smart Master+ excerpt: crest
    // factor/integrated loudness measured across the whole 8-10s buffer
    // (not the ~1.5s waveform display ring buffer) plus a Welch-averaged
    // FFT spectrum reduced to a handful of broad tonal-balance bands, used
    // to steer the generated EQ nodes toward each genre's reference curve.
    struct SmartMasterAnalysis
    {
        float integratedLufs = -100.0f;
        float rmsDb = -100.0f, peakDb = -100.0f, crestDb = 0.0f;
        static constexpr int kNumBands = 7;   // sub-bass..air, see runAutoAnalyze
        std::array<float, kNumBands> bandDb {};   // per-band level, dB (mean-relative)
    };
    SmartMasterAnalysis analyzeSmartMasterCapture() const;

    double       smartMasterProgress = 0.0;   // bound to the capture dialog's ProgressBar
    juce::String smartMasterGenre;
    float        smartMasterTargetLufs = -9.0f;
    bool         waitingForSmartMasterCapture = false;

    // Zoom
    void applyZoom(float scale);

    // Help / branding menu
    void showLogoMenu();
    void startHelpTour();
    static juce::File settingsFile();
    static bool  hasSeenHelp();
    static void  markHelpSeen();

    // Custom drawing
    std::unique_ptr<juce::Drawable> logoDrawable;
    void drawLogo(juce::Graphics& g, juce::Rectangle<float> area) const;
    void drawTabPanel(juce::Graphics& g, juce::Rectangle<int> r,
                      const juce::String& tabText) const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VisualCompEditor)
};
