// DemoModeIndicator.cpp
#include "DemoModeIndicator.h"
#include "Theme.h"

// ════════════════════════════════════════════════════════════════════════════════
// LicenseActivationWindow::Content
// ════════════════════════════════════════════════════════════════════════════════

LicenseActivationWindow::Content::Content(LicenseManager& licenseManager,
                                          std::function<void()> onSuccess,
                                          LicenseActivationWindow& parentWindow)
    : licenseMgr(licenseManager), onSuccessCallback(onSuccess), parent(parentWindow)
{
    // Instruction label
    instructionLabel.setText(
        "VisualComp is in Demo mode. Enter your license key to unlock all features:",
        juce::dontSendNotification);
    instructionLabel.setJustificationType(juce::Justification::topLeft);
    instructionLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffffff));
    addAndMakeVisible(instructionLabel);

    // Trial status label - shows remaining days
    trialStatusLabel.setJustificationType(juce::Justification::topLeft);
    trialStatusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff7a1f));
    auto remainingDays = licenseMgr.getRemainingTrialDays();
    if (remainingDays > 0)
    {
        trialStatusLabel.setText(
            remainingDays == 1
                ? "Demo expires in 1 day"
                : "Demo expires in " + juce::String(remainingDays) + " days",
            juce::dontSendNotification);
    }
    else
    {
        trialStatusLabel.setText("Demo period has expired", juce::dontSendNotification);
    }
    addAndMakeVisible(trialStatusLabel);

    // License key text editor
    licenseKeyEditor.setMultiLine(true);
    licenseKeyEditor.setReturnKeyStartsNewLine(true);
    licenseKeyEditor.setTextToShowWhenEmpty("Paste license key here...", juce::Colours::grey);
    licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff2a2a2a));
    licenseKeyEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffffffff));
    licenseKeyEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff555555));
    addAndMakeVisible(licenseKeyEditor);

    // Activate button
    activateButton.onClick = [this] { onActivateClicked(); };
    activateButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff7a1f));
    activateButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
    addAndMakeVisible(activateButton);

    // Skip button - continue with trial
    skipButton.onClick = [this] { onSkipClicked(); };
    skipButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff444444));
    skipButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xffffffff));
    addAndMakeVisible(skipButton);

    // Help link
    helpLink.setColour(juce::HyperlinkButton::textColourId, juce::Colour(0xff7a9fff));
    addAndMakeVisible(helpLink);

    // Feedback label
    feedbackLabel.setJustificationType(juce::Justification::centred);
    feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
    addAndMakeVisible(feedbackLabel);

    setSize(520, 400);
}

void LicenseActivationWindow::Content::resized()
{
    auto bounds = getLocalBounds().reduced(16);

    instructionLabel.setBounds(bounds.removeFromTop(28));
    bounds.removeFromTop(6);

    trialStatusLabel.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(12);

    licenseKeyEditor.setBounds(bounds.removeFromTop(100));
    bounds.removeFromTop(12);

    feedbackLabel.setBounds(bounds.removeFromTop(24));
    bounds.removeFromTop(8);

    helpLink.setBounds(bounds.removeFromTop(20));
    bounds.removeFromTop(8);

    auto buttonArea = bounds.removeFromBottom(36);
    auto buttonWidth = (buttonArea.getWidth() - 8) / 2;
    skipButton.setBounds(buttonArea.removeFromLeft(buttonWidth));
    buttonArea.removeFromLeft(8);
    activateButton.setBounds(buttonArea);
}

void LicenseActivationWindow::Content::updateFeedback(const juce::String& message, bool isError)
{
    feedbackLabel.setText(message, juce::dontSendNotification);
    if (isError)
    {
        feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xffff6b6b));
    }
    else
    {
        feedbackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff51cf66));
    }
}

void LicenseActivationWindow::Content::onActivateClicked()
{
    auto licenseKey = licenseKeyEditor.getText().toStdString();

    if (licenseKey.empty())
    {
        updateFeedback("Please enter a license key", true);
        return;
    }

    // Try to activate the license
    auto activation = licenseMgr.activateLicense(licenseKey);
    if (activation.success)
    {
        updateFeedback("License activated successfully!", false);

        // Close the window after a short delay
        juce::Timer::callAfterDelay(800, [this]
        {
            if (onSuccessCallback)
                onSuccessCallback();
            parent.closeButtonPressed();
        });
    }
    else
    {
        // Use the detailed error message from the license verification
        juce::String errorDisplay = "License error: " + juce::String(activation.errorMessage);
        updateFeedback(errorDisplay, true);
    }
}

void LicenseActivationWindow::Content::onSkipClicked()
{
    parent.closeButtonPressed();
}

// ════════════════════════════════════════════════════════════════════════════════
// LicenseActivationWindow
// ════════════════════════════════════════════════════════════════════════════════

LicenseActivationWindow::LicenseActivationWindow(LicenseManager& licenseManager,
                                                 std::function<void()> onSuccess)
    : juce::DialogWindow("Activate License", juce::Colour(0xff1d1d1b), true, false)
{
    // Create content with fixed size (520x400)
    setContentOwned(new Content(licenseManager, onSuccess, *this), true);
    setResizable(false, false);

    // Explicitly set the window size to match the content size to avoid layout issues
    setSize(520, 400);

    // Center the window on screen after sizing
    centreWithSize(520, 400);

    // Ensure window stays on top of other windows
    setAlwaysOnTop(true);
}

void LicenseActivationWindow::closeButtonPressed()
{
    juce::DialogWindow::closeButtonPressed();
}

// ════════════════════════════════════════════════════════════════════════════════
// DemoModeIndicator
// ════════════════════════════════════════════════════════════════════════════════

DemoModeIndicator::DemoModeIndicator(LicenseManager& licenseManager)
    : licenseMgr(licenseManager)
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
}

void DemoModeIndicator::paint(juce::Graphics& g)
{
    if (!licenseMgr.isDemoMode())
    {
        // No watermark for licensed product
        return;
    }

    auto bounds = getLocalBounds().toFloat();
    juce::String text = licenseMgr.getDemoWatermarkText();

    // Semi-transparent dark background
    g.setColour(juce::Colour(0, 0, 0).withAlpha(0.4f));
    g.fillRect(bounds);

    // Accent-orange text for demo indicator
    auto accentColour = juce::Colour(0xff7a1f);
    g.setColour(accentColour.withAlpha(0.9f));

    auto font = juce::Font(12.0f, juce::Font::bold);
    g.setFont(font);

    g.drawText(text,
               bounds.reduced(4.0f),
               juce::Justification::centred,
               true);
}

void DemoModeIndicator::resized()
{
    // Small fixed-height indicator
}

void DemoModeIndicator::mouseUp(const juce::MouseEvent&)
{
    if (licenseMgr.isDemoMode())
    {
        showLicenseActivationDialog();
    }
}

void DemoModeIndicator::showLicenseActivationDialog()
{
    // Defer window creation to avoid blocking the event thread during mouseUp.
    // Using callAfterDelay(1ms) returns control to the message pump first, preventing
    // AppHangB1 hangs that occur when expensive window creation blocks the event thread.
    juce::Timer::callAfterDelay(1, [this]()
    {
        auto* licenseWindow = new LicenseActivationWindow(
            licenseMgr,
            [this]()
            {
                if (onLicenseActivated)
                    onLicenseActivated();
            });

        // Show as a non-blocking top-level window instead of using the blocking
        // enterModalState() which can cause AppHangB1 hangs if any part of the
        // component initialization or layout is slow/blocking.
        // The window will stay on top and can be interacted with normally.
        //
        // A freshly-constructed juce::Component defaults to invisible
        // (visibleFlag == false), and DialogWindow's ctor here is called with
        // addToDesktop=false, so nothing else along the construction path ever
        // flips that flag. addToDesktop() creates the native peer but sets its
        // visibility from isVisible() at that moment (Component::addToDesktop(),
        // "peer->setVisible(isVisible())") — without an explicit setVisible(true)
        // first, the peer is created hidden and toFront() alone does not reveal
        // it (it only reorders z-order), so the dialog would silently never
        // appear on screen.
        licenseWindow->setVisible(true);
        licenseWindow->addToDesktop(
            juce::ComponentPeer::windowHasDropShadow |
            juce::ComponentPeer::windowIsTemporary);
        licenseWindow->toFront(true);
    });
}
