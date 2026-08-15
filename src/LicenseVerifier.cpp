// LicenseVerifier.cpp
// License key validation via Keygen API
#include "LicenseVerifier.h"
#include <sstream>

LicenseVerifier::VerificationResult LicenseVerifier::verify(const std::string& licenseKeyString)
{
    if (licenseKeyString.empty())
        return { false, "License key is empty" };

    // Trim whitespace from the key
    std::string trimmedKey = licenseKeyString;
    trimmedKey.erase(0, trimmedKey.find_first_not_of(" \t\n\r"));
    trimmedKey.erase(trimmedKey.find_last_not_of(" \t\n\r") + 1);

    if (trimmedKey.empty())
        return { false, "License key is empty" };

    // Basic format validation: key should contain alphanumeric chars and hyphens
    for (char c : trimmedKey)
    {
        if (!std::isalnum(c) && c != '-')
            return { false, "License key contains invalid characters" };
    }

    // Validate with Keygen API
    return validateWithKeygen(trimmedKey);
}

LicenseVerifier::VerificationResult LicenseVerifier::validateWithKeygen(const std::string& licenseKey)
{
    try
    {
        // Build URL using the correct Keygen API format with Account ID
        // Endpoint: POST /v1/accounts/{accountId}/licenses/actions/validate-key
        std::string urlString = std::string(KEYGEN_API_BASE) + "/" + KEYGEN_ACCOUNT_ID + "/licenses/actions/validate-key";
        juce::URL url(urlString);
        url = url.withParameter("key", licenseKey);

        DBG("Validating license with Keygen: " << licenseKey);
        DBG("API Endpoint: " << urlString);

        // Create input stream (POST request)
        // First parameter: true = POST request, false = GET request
        // Set a timeout of 5 seconds (timeout in ms)
        auto stream = url.createInputStream(true, nullptr, nullptr, "", 5000);

        if (!stream)
        {
            DBG("Failed to connect to Keygen service");
            return { false, "Unable to connect to license service. Please check your internet connection." };
        }

        // Read response
        juce::String responseBody = stream->readEntireStreamAsString();

        if (responseBody.isEmpty())
        {
            DBG("Empty response from Keygen");
            return { false, "Empty response from license service" };
        }

        DBG("Keygen response: " << responseBody);

        // Parse and return result
        return parseKeygenResponse(responseBody);
    }
    catch (const std::exception& e)
    {
        std::string errorMsg = std::string("License validation failed: ") + e.what();
        DBG(errorMsg);
        return { false, errorMsg };
    }
    catch (...)
    {
        DBG("License validation failed with unknown error");
        return { false, "License validation failed. Please try again." };
    }
}

LicenseVerifier::VerificationResult LicenseVerifier::parseKeygenResponse(const juce::String& responseBody)
{
    // Parse JSON response from Keygen
    // Expected successful response: {"valid":true,"key":"..."}
    // Expected error response: {"errors":[{"detail":"..."}]}

    if (responseBody.isEmpty())
        return { false, "Empty response from license service" };

    auto jsonValue = juce::JSON::parse(responseBody);

    if (jsonValue.isVoid())
    {
        DBG("Failed to parse Keygen response as JSON");
        return { false, "Invalid response format from license service" };
    }

    // Check for error responses first
    if (jsonValue.hasProperty("errors"))
    {
        auto errors = jsonValue.getProperty("errors", juce::var());
        if (errors.isArray() && errors.size() > 0)
        {
            auto firstError = errors[0];
            if (firstError.isObject() && firstError.hasProperty("detail"))
            {
                juce::String detail = firstError.getProperty("detail", juce::var("Unknown error")).toString();
                return { false, detail.toStdString() };
            }
        }
        return { false, "License key is invalid" };
    }

    // Check for successful validation
    if (jsonValue.hasProperty("valid"))
    {
        bool isValid = jsonValue.getProperty("valid", false);

        if (isValid)
        {
            juce::String key = jsonValue.getProperty("key", juce::var("")).toString();
            DBG("License validated successfully: " << key);
            return { true, "" };
        }
        else
        {
            // Key exists but is not valid (revoked, expired, etc.)
            return { false, "License key is not valid or has been revoked" };
        }
    }

    // Check nested data structure
    if (jsonValue.hasProperty("data"))
    {
        auto data = jsonValue.getProperty("data", juce::var());
        if (data.isObject() && data.hasProperty("valid"))
        {
            bool isValid = data.getProperty("valid", false);
            if (isValid)
            {
                return { true, "" };
            }
            else
            {
                return { false, "License key is not valid or has been revoked" };
            }
        }
    }

    // Unexpected response format - log it for debugging
    DBG("Unexpected Keygen response format: " << responseBody);
    return { false, "Invalid response from license service" };
}
