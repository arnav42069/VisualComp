// DemoModeIndicator.h
// Visual watermark indicator showing demo mode status and remaining trial time
#pragma once

#include <JuceHeader.h>
#include "LicenseManager.h"

// Small custom-drawn close (X) button used by the license dialog's own title bar.
// Exists purely so we don't rely on any DocumentWindow/DialogWindow chrome.
class LicenseCloseButton : public juce::Button
{
public:
    LicenseCloseButton() : juce::Button("close") {}

    void paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseCloseButton)
};

// Custom dialog for license key entry.
//
// This is a plain top-level juce::Component, NOT a juce::DialogWindow/DocumentWindow —
// it draws its own title bar, border, and close button rather than relying on any
// native/JUCE-drawn chrome, and it is shown non-modally (setVisible + addToDesktop,
// never enterModalState()) to avoid AppHangB1 hangs. See showLicenseActivationDialog()
// in the .cpp for the details of why.
class LicenseActivationWindow : public juce::Component
{
public:
    LicenseActivationWindow(LicenseManager& licenseManager, std::function<void()> onSuccess);
    ~LicenseActivationWindow() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;

    // Hides the window, removes it from the desktop, and schedules `this` for
    // asynchronous deletion on the message thread. Safe to call re-entrantly
    // (e.g. from both the close button and a race with the success timer) —
    // only the first call does anything.
    void dismiss();

    // Fired synchronously at the top of dismiss(), before hiding/deleting —
    // lets the owner (DemoModeIndicator) immediately drop its SafePointer to
    // this window so a fast repeat click can't try to toFront() a window
    // that's already on its way out.
    std::function<void()> onDismissed;

    static constexpr int kTitleBarHeight = 28;

private:
    class Content : public juce::Component, private juce::Thread
    {
    public:
        Content(LicenseManager& licenseManager, std::function<void()> onSuccess, LicenseActivationWindow& parentWindow);
        ~Content() override;
        void resized() override;
        void run() override;  // Background thread for license verification

    private:
        LicenseManager& licenseMgr;
        std::function<void()> onSuccessCallback;
        LicenseActivationWindow& parent;

        juce::Label instructionLabel;
        juce::Label trialStatusLabel;
        juce::TextEditor licenseKeyEditor;
        juce::TextButton activateButton { "Activate License" };
        juce::TextButton skipButton { "Continue Trial" };
        juce::HyperlinkButton helpLink { "Need a license? Visit our site", juce::URL("https://azazelaudio.com/visualcomp") };
        juce::Label feedbackLabel;

        // For background verification
        std::atomic<bool> isVerifying { false };
        juce::String licenseKeyToVerify;

        // Shared with any pending callAsync/callAfterDelay lambdas posted from run()
        // (the background thread). juce::Component::SafePointer is NOT safe to
        // construct/copy off the message thread (it mutates the Component's
        // WeakReference::Master), so we can't use it from the background thread —
        // a plain std::shared_ptr<std::atomic<bool>> is thread-safe to copy/read
        // from anywhere and lets those lambdas detect "this Content has since been
        // destroyed" (e.g. the whole dialog was closed while verification was
        // still in flight) before touching `this`. Flipped false at the very top
        // of ~Content(), before stopThread() blocks for the background thread.
        std::shared_ptr<std::atomic<bool>> aliveFlag { std::make_shared<std::atomic<bool>>(true) };

        void updateFeedback(const juce::String& message, bool isError);
        void onActivateClicked();
        void onSkipClicked();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
    };

    LicenseCloseButton closeButton;
    std::unique_ptr<Content> content;
    juce::ComponentDragger titleBarDragger;
    juce::ComponentBoundsConstrainer titleBarConstrainer;
    bool dismissRequested = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseActivationWindow)
};

class DemoModeIndicator : public juce::Component
{
public:
    explicit DemoModeIndicator(LicenseManager& licenseManager);
    ~DemoModeIndicator() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool hitTest(int x, int y) override;

    // Click handler for showing license activation dialog
    void mouseUp(const juce::MouseEvent& e) override;

    // Callback when license is activated
    std::function<void()> onLicenseActivated;

    // Publicly accessible method to show license activation dialog on startup
    void showLicenseActivationDialog();

private:
    LicenseManager& licenseMgr;

    // Tracks the single live activation dialog, if any. SafePointer so it
    // automatically nulls out once the window deletes itself via dismiss().
    juce::Component::SafePointer<LicenseActivationWindow> activeWindow;

    // Guards against two fast clicks both scheduling a callAfterDelay(1, ...)
    // creation before either has actually constructed the window (activeWindow
    // is still null at that point, so it alone can't prevent the race).
    bool dialogPending = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DemoModeIndicator)
};
