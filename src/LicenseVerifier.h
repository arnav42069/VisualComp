// LicenseVerifier.h
// License key validation via Keygen API
#pragma once

#include <JuceHeader.h>
#include <string>
#include <optional>

class LicenseVerifier
{
public:
    struct VerificationResult
    {
        bool isValid;
        std::string errorMessage;
    };

    LicenseVerifier() = default;
    ~LicenseVerifier() = default;

    // Verify a license key string by sending it to Keygen's API
    // Keygen handles all validation and returns whether the key is valid
    VerificationResult verify(const std::string& licenseKeyString);

private:
    // Keygen API endpoint and credentials
    static constexpr const char* KEYGEN_API_URL = "https://api.keygen.sh/v1/licenses/validate";
    static constexpr const char* PRODUCT_ID = "7c8bc57b-1356-4f36-b757-cf1511516f29";
    static constexpr const char* POLICY_ID = "b249f536-4179-4021-9567-a67327c2beae";

    // Perform HTTP POST request to Keygen API to validate the license key
    // Returns true if valid, false otherwise (with errorMessage set)
    VerificationResult validateWithKeygen(const std::string& licenseKey);

    // Parse Keygen API response and extract validation result
    VerificationResult parseKeygenResponse(const juce::String& responseBody);
};
