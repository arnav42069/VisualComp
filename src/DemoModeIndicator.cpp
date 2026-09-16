// DemoModeIndicator.cpp
#include "DemoModeIndicator.h"
#include "Theme.h"

// ════════════════════════════════════════════════════════════════════════════════
// LicenseCloseButton
// ════════════════════════════════════════════════════════════════════════════════

void LicenseCloseButton::paintButton(juce::Graphics& g, bool isMouseOverButton, bool isButtonDown)
{
    auto bounds = getLocalBounds().toFloat();

    if (isButtonDown)
        g.setColour(Theme::warn.withAlpha(0.85f));
    else if (isMouseOverButton)
        g.setColour(Theme::accent.withAlpha(0.85f));
    else
        g.setColour(Theme::textDim);

    auto x = bounds.reduced(bounds.getWidth() * 0.32f, bounds.getHeight() * 0.32f);
    juce::Path cross;
    cross.startNewSubPath(x.getTopLeft());
    cross.lineTo(x.getBottomRight());
    cross.startNewSubPath(x.getTopRight());
    cross.lineTo(x.getBottomLeft());
    g.strokePath(cross, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

// ════════════════════════════════════════════════════════════════════════════════
// LicenseActivationWindow::Content
// ════════════════════════════════════════════════════════════════════════════════

LicenseActivationWindow::Content::Content(LicenseManager& licenseManager,
                                          std::function<void()> onSuccess,
                                          LicenseActivationWindow& parentWindow)
    : juce::Thread("LicenseVerification"),
      licenseMgr(licenseManager), onSuccessCallback(onSuccess), parent(parentWindow)
{
    // Instruction label
    instructionLabel.setText(
        "VisualComp is in Demo mode. Enter your license key to unlock all features:",
        juce::dontSendNotification);
    instructionLabel.setJustificationType(juce::Justification::topLeft);
    instructionLabel.setFont(Theme::label(14.0f, juce::Font::plain));
    instructionLabel.setColour(juce::Label::textColourId, Theme::text);
    addAndMakeVisible(instructionLabel);

    // Trial status label - shows remaining days
    trialStatusLabel.setJustificationType(juce::Justification::topLeft);
    trialStatusLabel.setFont(Theme::label(13.0f));
    trialStatusLabel.setColour(juce::Label::textColourId, Theme::accent);
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

    // License key text editor — single line, Enter submits (activates) rather
    // than inserting a newline.
    licenseKeyEditor.setMultiLine(false);
    licenseKeyEditor.setReturnKeyStartsNewLine(false);
    licenseKeyEditor.setTextToShowWhenEmpty("Paste license key here...", Theme::textDim);
    licenseKeyEditor.setColour(juce::TextEditor::backgroundColourId, Theme::bgDeep);
    licenseKeyEditor.setColour(juce::TextEditor::textColourId, Theme::text);
    licenseKeyEditor.setColour(juce::TextEditor::outlineColourId, Theme::line);
    licenseKeyEditor.setColour(juce::TextEditor::focusedOutlineColourId, Theme::accent);
    licenseKeyEditor.setColour(juce::CaretComponent::caretColourId, Theme::accent);
    licenseKeyEditor.onReturnKey = [this] { onActivateClicked(); };
    addAndMakeVisible(licenseKeyEditor);

    // Activate button
    activateButton.onClick = [this] { onActivateClicked(); };
    activateButton.setColour(juce::TextButton::buttonColourId, Theme::accent);
    activateButton.setColour(juce::TextButton::textColourOnId, Theme::bg);
    activateButton.setColour(juce::TextButton::textColourOffId, Theme::bg);
    addAndMakeVisible(activateButton);

    // Skip button - continue with trial
    skipButton.onClick = [this] { onSkipClicked(); };
    skipButton.setColour(juce::TextButton::buttonColourId, Theme::charcoal);
    skipButton.setColour(juce::TextButton::textColourOnId, Theme::text);
    skipButton.setColour(juce::TextButton::textColourOffId, Theme::text);
    addAndMakeVisible(skipButton);

    // Help link
    helpLink.setColour(juce::HyperlinkButton::textColourId, Theme::ice);
    addAndMakeVisible(helpLink);

    // Feedback label
    feedbackLabel.setJustificationType(juce::Justification::centred);
    feedbackLabel.setFont(Theme::label(13.0f));
    feedbackLabel.setColour(juce::Label::textColourId, Theme::warn);
    addAndMakeVisible(feedbackLabel);

    setSize(520, 400 - LicenseActivationWindow::kTitleBarHeight);
}

LicenseActivationWindow::Content::~Content()
{
    // Flip the shared flag first so any callAsync/callAfterDelay lambda posted
    // from run() (possibly already sitting in the message queue) knows not to
    // touch `this` once it eventually runs — see the member's own comment.
    aliveFlag->store(false);

    // Ensure the background thread is stopped before destruction
    stopThread(5000);  // Wait up to 5 seconds for thread to stop
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
    // Theme has no dedicated "success" colour; 0xff51cf66 (soft green) is kept
    // deliberately as the one bespoke success colour per the spec, distinct from
    // Theme::warn used for errors.
    feedbackLabel.setColour(juce::Label::textColourId,
                             isError ? Theme::warn : juce::Colour(0xff51cf66));
}

void LicenseActivationWindow::Content::onActivateClicked()
{
    auto licenseKey = licenseKeyEditor.getText().toStdString();

    if (licenseKey.empty())
    {
        updateFeedback("Please enter a license key", true);
        return;
    }

    // Prevent multiple simultaneous verification attempts
    if (isVerifying.exchange(true))
    {
        updateFeedback("Verification in progress...", false);
        return;
    }

    // Store the key and disable buttons
    licenseKeyToVerify = licenseKey.c_str();
    activateButton.setEnabled(false);
    skipButton.setEnabled(false);
    licenseKeyEditor.setEnabled(false);
    updateFeedback("Verifying license...", false);

    // Start background verification thread
    startThread();
}

void LicenseActivationWindow::Content::run()
{
    // This runs on a background thread - safe to make blocking HTTP calls here.
    // Grab a copy of the shared alive-flag now, while `this` is definitely still
    // valid — a plain std::shared_ptr copy is thread-safe from any thread (unlike
    // juce::Component::SafePointer, which must only ever be touched on the
    // message thread).
    auto flag = aliveFlag;

    auto activation = licenseMgr.activateLicense(licenseKeyToVerify.toStdString());

    // Return to message thread to update UI and potentially close window. The
    // whole dialog can legally be closed (X / Continue Trial) while this HTTP
    // call is still in flight, since only this Content's own buttons — not the
    // window's close button — are disabled during verification; `flag` is what
    // lets this lambda detect that `this` has since been destroyed instead of
    // touching freed memory.
    juce::MessageManager::callAsync([this, flag, success = activation.success, errorMsg = activation.errorMessage]()
    {
        if (!flag->load())
            return;

        isVerifying.store(false);
        activateButton.setEnabled(true);
        skipButton.setEnabled(true);
        licenseKeyEditor.setEnabled(true);

        if (success)
        {
            updateFeedback("License activated successfully!", false);

            // Close the window after a short delay. Re-check `flag` when the
            // timer fires too, in case the window was dismissed via another
            // path during the delay.
            juce::Timer::callAfterDelay(800, [this, flag]
            {
                if (!flag->load())
                    return;

                if (onSuccessCallback)
                    onSuccessCallback();
                parent.dismiss();
            });
        }
        else
        {
            juce::String errorDisplay = "License error: " + juce::String(errorMsg);
            updateFeedback(errorDisplay, true);
        }
    });
}

void LicenseActivationWindow::Content::onSkipClicked()
{
    parent.dismiss();
}

// ════════════════════════════════════════════════════════════════════════════════
// LicenseActivationWindow
// ════════════════════════════════════════════════════════════════════════════════

LicenseActivationWindow::LicenseActivationWindow(LicenseManager& licenseManager,
                                                 std::function<void()> onSuccess)
{
    // Custom close (X) button, drawn entirely ourselves — no DocumentWindow chrome.
    closeButton.onClick = [this] { dismiss(); };
    addAndMakeVisible(closeButton);

    content = std::make_unique<Content>(licenseManager, onSuccess, *this);
    addAndMakeVisible(*content);

    // Plain juce::Component has no setResizable() (that's a ResizableWindow/
    // DocumentWindow member) — we simply never give this window a resize
    // border/corner, so it's non-resizable by construction.
    setSize(520, 400);
    centreWithSize(520, 400);

    // Ensure window stays on top of other windows
    setAlwaysOnTop(true);

    // Keep at least a sane chunk of the title bar on-screen if dragged toward
    // an edge, rather than letting it be dragged fully off any monitor.
    titleBarConstrainer.setMinimumOnscreenAmounts(kTitleBarHeight, 60, 60, 60);
}

LicenseActivationWindow::~LicenseActivationWindow() = default;

void LicenseActivationWindow::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    g.fillAll(Theme::bg);

    // Title bar strip
    auto titleBar = bounds.removeFromTop(kTitleBarHeight);
    g.setColour(Theme::bgRaised);
    g.fillRect(titleBar);

    g.setColour(Theme::text);
    g.setFont(Theme::label(13.0f));
    g.drawText("ACTIVATE LICENSE", titleBar.reduced(12, 0),
               juce::Justification::centredLeft, true);

    // 1px border around the whole window. Corners are kept square rather than
    // rounded — this window isn't added to the desktop with a semi-transparent
    // peer flag, so a rounded fill would leave the un-covered corner pixels
    // showing whatever garbage the OS window's backbuffer happens to have
    // instead of true transparency.
    g.setColour(Theme::line);
    g.drawRect(getLocalBounds(), 1);
}

void LicenseActivationWindow::resized()
{
    auto bounds = getLocalBounds();
    auto titleBar = bounds.removeFromTop(kTitleBarHeight);

    closeButton.setBounds(titleBar.removeFromRight(kTitleBarHeight).reduced(6));

    if (content != nullptr)
        content->setBounds(bounds);
}

void LicenseActivationWindow::mouseDown(const juce::MouseEvent& e)
{
    // Only clicks landing in the title bar strip start a drag (clicks on the
    // close button or on Content never reach here — those child components
    // consume their own mouseDown first).
    if (e.y < kTitleBarHeight)
        titleBarDragger.startDraggingComponent(this, e);
}

void LicenseActivationWindow::mouseDrag(const juce::MouseEvent& e)
{
    if (e.getMouseDownPosition().y < kTitleBarHeight)
        titleBarDragger.dragComponent(this, e, &titleBarConstrainer);
}

void LicenseActivationWindow::dismiss()
{
    if (dismissRequested)
        return;
    dismissRequested = true;

    if (onDismissed)
        onDismissed();

    setVisible(false);
    removeFromDesktop();

    // Deleting `this` synchronously here would be unsafe — dismiss() can be
    // called from deep inside a button click or timer callback that is itself
    // a member function still executing on this object. Instead, schedule the
    // actual delete for the next message-loop iteration via a SafePointer,
    // which is always safe on the message thread (unlike the background-thread
    // case in Content::run(), which uses a separate std::atomic-based flag —
    // see that method's comments for why the two mechanisms differ).
    juce::Component::SafePointer<LicenseActivationWindow> safeThis(this);
    juce::MessageManager::callAsync([safeThis]()
    {
        if (safeThis != nullptr)
            delete safeThis.getComponent();
    });
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

    // Accent-orange text for demo indicator. NOTE: this used to be
    // juce::Colour(0xff7a1f) — a 6-digit literal, which juce::Colour parses as
    // 0x00ff7a1f (alpha byte = 0x00), i.e. fully transparent. Theme::accent is
    // already the correct 8-digit 0xffff7a1f.
    g.setColour(Theme::accent.withAlpha(0.9f));
    g.setFont(Theme::label(12.0f));

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
    // Only one activation dialog may ever be open at a time. If one is already
    // live, just bring it forward instead of stacking a second one on top.
    if (activeWindow != nullptr)
    {
        activeWindow->toFront(true);
        return;
    }

    // A window creation may already be scheduled (two fast clicks both landing
    // before the callAfterDelay(1, ...) below has fired) — activeWindow is still
    // null at that point since the window doesn't exist yet, so it alone can't
    // catch this race; dialogPending covers the gap.
    if (dialogPending)
        return;
    dialogPending = true;

    // Defer window creation to avoid blocking the event thread during mouseUp.
    // Using callAfterDelay(1ms) returns control to the message pump first, preventing
    // AppHangB1 hangs that occur when expensive window creation blocks the event thread.
    juce::Component::SafePointer<DemoModeIndicator> safeThis(this);
    juce::Timer::callAfterDelay(1, [safeThis]()
    {
        if (safeThis == nullptr)
            return;

        auto* licenseWindow = new LicenseActivationWindow(
            safeThis->licenseMgr,
            [safeThis]()
            {
                if (safeThis != nullptr && safeThis->onLicenseActivated)
                    safeThis->onLicenseActivated();
            });

        safeThis->dialogPending = false;
        safeThis->activeWindow = licenseWindow;

        // Drop our tracking pointer the moment the window starts tearing itself
        // down (dismiss() fires this synchronously, before the async delete), so
        // a fast repeat click in that gap creates a fresh dialog instead of
        // calling toFront() on one that's already on its way out.
        licenseWindow->onDismissed = [safeThis]()
        {
            if (safeThis != nullptr)
                safeThis->activeWindow = nullptr;
        };

        // Show as a non-blocking top-level window instead of using the blocking
        // enterModalState() which can cause AppHangB1 hangs if any part of the
        // component initialization or layout is slow/blocking.
        // The window will stay on top and can be interacted with normally.
        //
        // A freshly-constructed juce::Component defaults to invisible
        // (visibleFlag == false), and nothing along the construction path above
        // ever flips that flag. addToDesktop() creates the native peer but sets
        // its visibility from isVisible() at that moment (Component::addToDesktop(),
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
