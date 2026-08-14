// DemoModeIndicator.h
// Visual watermark indicator showing demo mode status and remaining trial time
#pragma once

#include <JuceHeader.h>
#include "LicenseManager.h"

// Custom dialog for license key entry
class LicenseActivationWindow : public juce::DialogWindow
{
public:
    LicenseActivationWindow(LicenseManager& licenseManager, std::function<void()> onSuccess);
    ~LicenseActivationWindow() override = default;

    void closeButtonPressed() override;

private:
    class Content : public juce::Component
    {
    public:
        Content(LicenseManager& licenseManager, std::function<void()> onSuccess, LicenseActivationWindow& parentWindow);
        void resized() override;

    private:
        LicenseManager& licenseMgr;
        std::function<void()> onSuccessCallback;
        LicenseActivationWindow& parent;

        juce::Label instructionLabel;
        juce::Label trialStatusLabel;
        juce::TextEditor licenseKeyEditor;
        juce::TextButton activateButton { "Activate License" };
        juce::TextButton skipButton { "Continue Trial" };
        juce::HyperlinkButton helpLink { "Need a license? Visit our site", juce::URL("https://azazelaudio.com/licenses") };
        juce::Label feedbackLabel;

        void updateFeedback(const juce::String& message, bool isError);
        void onActivateClicked();
        void onSkipClicked();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Content)
    };

    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseActivationWindow)
};

class DemoModeIndicator : public juce::Component
{
public:
    explicit DemoModeIndicator(LicenseManager& licenseManager);
    ~DemoModeIndicator() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Click handler for showing license activation dialog
    void mouseUp(const juce::MouseEvent& e) override;

    // Callback when license is activated
    std::function<void()> onLicenseActivated;

    // Publicly accessible method to show license activation dialog on startup
    void showLicenseActivationDialog();

private:
    LicenseManager& licenseMgr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DemoModeIndicator)
};
