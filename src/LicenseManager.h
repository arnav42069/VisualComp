// LicenseManager.h
// Manages license verification, persistence, and demo mode logic
#pragma once

#include <JuceHeader.h>
#include "LicenseVerifier.h"
#include <atomic>
#include <ctime>

class LicenseManager
{
public:
    // Demo mode configuration
    static constexpr int TRIAL_DAYS = 30;

    // License state enum
    enum class State
    {
        TrialActive,      // Demo mode: trial period active
        TrialExpired,     // Demo mode: trial period expired (30 days)
        Licensed,         // Full license verified and valid
        OfflineLicense    // License verified offline from persistent storage
    };

    explicit LicenseManager();
    ~LicenseManager() = default;

    // Initialize: loads license from disk if it exists, or starts trial
    void initialize();

    // Attempt to load and verify a license from a file
    bool loadLicenseFromFile(const juce::File& licenseFile);

    // Attempt to verify and save a license key (base64 encoded)
    bool activateLicense(const std::string& base64LicenseKey);

    // Get current license state
    State getState() const { return state.load(); }

    // Get remaining trial days (only valid if state == TrialActive)
    int getRemainingTrialDays() const;

    // Check if the product is in demo mode
    bool isDemoMode() const
    {
        State s = state.load();
        return s == State::TrialActive || s == State::TrialExpired;
    }

    // Get the demo watermark text (e.g., "DEMO - 15 days left")
    juce::String getDemoWatermarkText() const;

    // Get the license file path (where licenses are stored)
    static juce::File getLicenseFilePath();

    // Get the license state directory (creates if needed)
    static juce::File getLicenseStateDirectory();

    // Reset trial period (debug/development only — normally never called)
    void resetTrial();

    // For UI: check if we should show the license dialog on startup
    bool shouldShowLicenseOnStartup() const;

private:
    LicenseVerifier verifier;
    std::atomic<State> state { State::TrialActive };

    // Trial start time (epoch seconds)
    std::atomic<time_t> trialStartTime { 0 };

    // Persistent license key (if offline license was found)
    juce::String persistedLicenseKey;

    // Load trial metadata from disk
    void loadTrialMetadata();

    // Save trial start time to disk
    void saveTrialMetadata();

    // Verify and load a license from XML property tree
    bool verifyAndLoadLicenseData(const juce::ValueTree& data);

    // Get path to the trial metadata file
    static juce::File getTrialMetadataFile();

    // Get path to the license XML file
    static juce::File getLicenseXmlFile();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LicenseManager)
};
