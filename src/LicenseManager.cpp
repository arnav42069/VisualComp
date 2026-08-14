// LicenseManager.cpp
#include "LicenseManager.h"
#include <ctime>
#include <sstream>

LicenseManager::LicenseManager()
    : state(State::TrialActive), trialStartTime(0)
{
}

void LicenseManager::initialize()
{
    // First, try to load a persistent license from disk
    juce::File licenseFile = getLicenseXmlFile();
    if (licenseFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(licenseFile))
        {
            auto vt = juce::ValueTree::fromXml(*xml);
            if (verifyAndLoadLicenseData(vt))
            {
                DBG("License loaded successfully from: " << licenseFile.getFullPathName());
                state.store(State::OfflineLicense);
                return;
            }
        }
    }

    // No valid license found; check trial period
    loadTrialMetadata();

    int remainingDays = getRemainingTrialDays();
    if (remainingDays <= 0)
    {
        state.store(State::TrialExpired);
        DBG("Trial period has expired");
    }
    else
    {
        state.store(State::TrialActive);
        DBG("Trial period active - " << remainingDays << " days remaining");
    }
}

bool LicenseManager::activateLicense(const std::string& base64LicenseKey)
{
    // Verify the license signature
    auto result = verifier.verify(base64LicenseKey);
    if (!result.isValid)
    {
        DBG("License verification failed: " << result.errorMessage);
        return false;
    }

    // Save the license to disk
    juce::File licenseDir = getLicenseStateDirectory();
    if (!licenseDir.createDirectory().wasOk())
    {
        DBG("Failed to create license directory: " << licenseDir.getFullPathName());
        return false;
    }

    juce::File licenseFile = getLicenseXmlFile();

    // Create XML with the license key
    juce::XmlElement licenseXml("License");
    licenseXml.setAttribute("key", base64LicenseKey.c_str());
    licenseXml.setAttribute("activatedAt", static_cast<int>(std::time(nullptr)));
    licenseXml.setAttribute("productName", "VisualComp 2");

    if (licenseFile.replaceWithText(licenseXml.toString(), false, false, "UTF-8"))
    {
        persistedLicenseKey = base64LicenseKey.c_str();
        state.store(State::Licensed);
        DBG("License activated and saved to: " << licenseFile.getFullPathName());
        return true;
    }

    DBG("Failed to save license to disk");
    return false;
}

juce::File LicenseManager::getLicenseStateDirectory()
{
    auto appData = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    return appData.getChildFile("Azazel Audio").getChildFile("VisualComp 2");
}

juce::File LicenseManager::getLicenseFilePath()
{
    return getLicenseStateDirectory().getChildFile("license.key");
}

juce::File LicenseManager::getTrialMetadataFile()
{
    return getLicenseStateDirectory().getChildFile("trial.xml");
}

juce::File LicenseManager::getLicenseXmlFile()
{
    return getLicenseStateDirectory().getChildFile("license.xml");
}

bool LicenseManager::loadLicenseFromFile(const juce::File& licenseFile)
{
    if (!licenseFile.existsAsFile())
    {
        DBG("License file not found: " << licenseFile.getFullPathName());
        return false;
    }

    if (auto xml = juce::parseXML(licenseFile))
    {
        auto vt = juce::ValueTree::fromXml(*xml);
        if (verifyAndLoadLicenseData(vt))
        {
            state.store(State::Licensed);
            return true;
        }
    }

    return false;
}

bool LicenseManager::verifyAndLoadLicenseData(const juce::ValueTree& data)
{
    if (!data.hasProperty("key"))
        return false;

    auto keyStr = data.getProperty("key").toString().toStdString();
    auto result = verifier.verify(keyStr);

    if (!result.isValid)
    {
        DBG("License verification failed: " << result.errorMessage);
        return false;
    }

    persistedLicenseKey = keyStr.c_str();
    return true;
}

void LicenseManager::loadTrialMetadata()
{
    juce::File metadataFile = getTrialMetadataFile();

    if (metadataFile.existsAsFile())
    {
        if (auto xml = juce::parseXML(metadataFile))
        {
            if (auto startTimeStr = xml->getStringAttribute("startTime", "").toRawUTF8())
            {
                std::istringstream iss(startTimeStr);
                time_t loadedTime = 0;
                if (iss >> loadedTime && loadedTime > 0)
                {
                    trialStartTime.store(loadedTime);
                    DBG("Loaded trial start time: " << loadedTime);
                    return;
                }
            }
        }
    }

    // First run: initialize trial period
    time_t now = std::time(nullptr);
    trialStartTime.store(now);
    saveTrialMetadata();
    DBG("Trial period initialized - start time: " << now);
}

void LicenseManager::saveTrialMetadata()
{
    juce::File stateDir = getLicenseStateDirectory();
    if (!stateDir.createDirectory().wasOk())
    {
        DBG("Failed to create state directory for trial metadata");
        return;
    }

    time_t startTime = trialStartTime.load();
    juce::XmlElement trialXml("Trial");
    trialXml.setAttribute("startTime", static_cast<int>(startTime));

    juce::File metadataFile = getTrialMetadataFile();
    if (!metadataFile.replaceWithText(trialXml.toString(), false, false, "UTF-8"))
    {
        DBG("Failed to save trial metadata");
    }
}

int LicenseManager::getRemainingTrialDays() const
{
    time_t startTime = trialStartTime.load();
    if (startTime <= 0)
        return TRIAL_DAYS;  // Safety fallback

    time_t now = std::time(nullptr);
    int elapsedSeconds = static_cast<int>(now - startTime);
    int elapsedDays = elapsedSeconds / (24 * 3600);

    int remaining = TRIAL_DAYS - elapsedDays;
    return (remaining < 0) ? 0 : remaining;
}

juce::String LicenseManager::getDemoWatermarkText() const
{
    State currentState = state.load();

    if (currentState == State::Licensed || currentState == State::OfflineLicense)
        return "UNLOCKED";

    if (currentState == State::TrialExpired)
        return "DEMO - TRIAL EXPIRED";

    int remainingDays = getRemainingTrialDays();
    if (remainingDays <= 0)
        return "DEMO - TRIAL EXPIRED";

    if (remainingDays == 1)
        return "DEMO - 1 day left";

    return "DEMO - " + juce::String(remainingDays) + " days left";
}

void LicenseManager::resetTrial()
{
    time_t now = std::time(nullptr);
    trialStartTime.store(now);
    saveTrialMetadata();
    state.store(State::TrialActive);
    DBG("Trial period reset");
}

bool LicenseManager::shouldShowLicenseOnStartup() const
{
    // Show license dialog if trial is nearly expired (7 days or less)
    State currentState = state.load();
    if (currentState == State::TrialExpired)
        return true;

    if (currentState == State::TrialActive)
    {
        int remainingDays = getRemainingTrialDays();
        return remainingDays <= 7;
    }

    return false;
}
