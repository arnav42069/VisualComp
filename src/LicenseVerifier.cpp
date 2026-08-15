// LicenseVerifier.cpp
// License key validation via Keygen API
#include "LicenseVerifier.h"
#include "MachineFingerprint.h"
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
    auto validationResult = validateWithKeygen(trimmedKey);

    // If validation succeeded and we got a license ID, register this machine
    if (validationResult.isValid && !validationResult.licenseId.empty())
    {
        DBG("License validated successfully. License ID: " << validationResult.licenseId);

        // Generate machine fingerprint and register
        std::string machineFingerprint = MachineFingerprint::getFingerprint();
        DBG("Registering machine fingerprint: " << machineFingerprint);

        auto machineResult = registerMachine(validationResult.licenseId, machineFingerprint);

        if (machineResult.isValid)
        {
            DBG("Machine registered successfully. Machine ID: " << machineResult.machineId);
            validationResult.machineId = machineResult.machineId;
            return validationResult;
        }
        else
        {
            // Machine registration failed, but license was valid
            // Return the error from registration
            DBG("Machine registration failed: " << machineResult.errorMessage);
            return { false, machineResult.errorMessage };
        }
    }

    return validationResult;
}

LicenseVerifier::VerificationResult LicenseVerifier::validateWithKeygen(const std::string& licenseKey)
{
    try
    {
        // Build URL using the correct Keygen API format with Account ID
        // Endpoint: POST /v1/accounts/{accountId}/licenses/actions/validate-key
        std::string urlString = std::string(KEYGEN_API_BASE) + "/" + KEYGEN_ACCOUNT_ID + "/licenses/actions/validate-key";
        juce::URL url(urlString);

        DBG("Validating license with Keygen: " << licenseKey);
        DBG("API Endpoint: " << urlString);

        // Build the JSON request body: { "meta": { "key": "..." } }
        auto metaObject = juce::var(new juce::DynamicObject());
        metaObject.getDynamicObject()->setProperty("key", juce::String(licenseKey));

        auto bodyObject = juce::var(new juce::DynamicObject());
        bodyObject.getDynamicObject()->setProperty("meta", metaObject);

        juce::String jsonBody = juce::JSON::toString(bodyObject);
        DBG("Keygen request body: " << jsonBody);

        // Use JUCE 7.0.9 API: withPOSTData() on URL, then InputStreamOptions for headers/timeout
        // This sends raw JSON POST body without form-encoding
        auto stream = url.withPOSTData(jsonBody).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders("Content-Type: application/vnd.api+json\r\nAccept: application/vnd.api+json")
                .withConnectionTimeoutMs(5000)
        );

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
    // Parse JSON response from Keygen using JSON:API format
    // Expected successful response: {"data":{"id":"..."},"meta":{"valid":true}}
    // Expected error response: {"meta":{"code":"...","detail":"..."}}

    if (responseBody.isEmpty())
        return { false, "Empty response from license service" };

    auto jsonValue = juce::JSON::parse(responseBody);

    if (jsonValue.isVoid())
    {
        DBG("Failed to parse Keygen response as JSON");
        DBG("Raw response was: " << responseBody);
        return { false, "Invalid response format from license service" };
    }

    VerificationResult result { false, "" };

    // Try to extract license ID from data object (present on successful validation)
    if (jsonValue.hasProperty("data"))
    {
        auto dataObject = jsonValue.getProperty("data", juce::var());
        if (dataObject.isObject() && dataObject.hasProperty("id"))
        {
            juce::String licenseId = dataObject.getProperty("id", juce::var("")).toString();
            result.licenseId = licenseId.toStdString();
            DBG("Extracted license ID: " << result.licenseId);
        }
    }

    // Check for JSON:API meta object (the correct location for Keygen responses)
    if (jsonValue.hasProperty("meta"))
    {
        auto metaObject = jsonValue.getProperty("meta", juce::var());
        if (metaObject.isObject())
        {
            // Check for validation result
            if (metaObject.hasProperty("valid"))
            {
                bool isValid = metaObject.getProperty("valid", false);
                DBG("Keygen meta.valid: " << (isValid ? "true" : "false"));

                if (isValid)
                {
                    DBG("License validated successfully");
                    result.isValid = true;
                    return result;
                }
                else
                {
                    // Try to get error detail if present
                    if (metaObject.hasProperty("detail"))
                    {
                        juce::String detail = metaObject.getProperty("detail", juce::var("")).toString();
                        if (detail.isNotEmpty())
                        {
                            DBG("Validation failed with detail: " << detail);
                            result.errorMessage = detail.toStdString();
                            return result;
                        }
                    }
                    if (metaObject.hasProperty("code"))
                    {
                        juce::String code = metaObject.getProperty("code", juce::var("")).toString();
                        if (code.isNotEmpty())
                        {
                            DBG("Validation failed with code: " << code);
                            result.errorMessage = ("License validation failed: " + code).toStdString();
                            return result;
                        }
                    }
                    // Key exists but is not valid (revoked, expired, etc.)
                    result.errorMessage = "License key is not valid or has been revoked";
                    return result;
                }
            }
            else
            {
                DBG("Meta object present but no 'valid' property found");
            }
        }
        else
        {
            DBG("Meta property is not an object");
        }
    }

    // Check for error responses in meta.errors array (alternative format)
    if (jsonValue.hasProperty("errors"))
    {
        auto errors = jsonValue.getProperty("errors", juce::var());
        if (errors.isArray() && errors.size() > 0)
        {
            auto firstError = errors[0];
            if (firstError.isObject() && firstError.hasProperty("detail"))
            {
                juce::String detail = firstError.getProperty("detail", juce::var("Unknown error")).toString();
                DBG("Keygen error detail: " << detail);
                result.errorMessage = detail.toStdString();
                return result;
            }
        }
        result.errorMessage = "License key is invalid";
        return result;
    }

    // Fallback: check for top-level valid property (non-JSON:API format, shouldn't happen with Keygen)
    if (jsonValue.hasProperty("valid"))
    {
        bool isValid = jsonValue.getProperty("valid", false);

        if (isValid)
        {
            DBG("License validated successfully (top-level format)");
            result.isValid = true;
            return result;
        }
        else
        {
            result.errorMessage = "License key is not valid or has been revoked";
            return result;
        }
    }

    // Unexpected response format - log it for debugging
    DBG("Unexpected Keygen response format: " << responseBody);
    result.errorMessage = "Invalid response from license service";
    return result;
}

LicenseVerifier::VerificationResult LicenseVerifier::registerMachine(const std::string& licenseId, const std::string& machineFingerprint)
{
    try
    {
        // Build URL to register machine: POST /v1/accounts/{accountId}/machines
        // The machine will be associated with the license via the fingerprint
        std::string urlString = std::string(KEYGEN_API_BASE) + "/" + KEYGEN_ACCOUNT_ID + "/machines";
        juce::URL url(urlString);

        DBG("Registering machine with Keygen");
        DBG("API Endpoint: " << urlString);
        DBG("Machine fingerprint: " << machineFingerprint);

        // Build the JSON request body with machine fingerprint and license association
        auto dataObject = juce::var(new juce::DynamicObject());
        dataObject.getDynamicObject()->setProperty("fingerprint", juce::String(machineFingerprint));
        dataObject.getDynamicObject()->setProperty("licenseId", juce::String(licenseId));

        auto bodyObject = juce::var(new juce::DynamicObject());
        bodyObject.getDynamicObject()->setProperty("data", dataObject);

        juce::String jsonBody = juce::JSON::toString(bodyObject);
        DBG("Keygen machine registration request body: " << jsonBody);

        // Use JUCE 7.0.9 API: withPOSTData() on URL, then InputStreamOptions for headers/timeout
        auto stream = url.withPOSTData(jsonBody).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders("Content-Type: application/vnd.api+json\r\nAccept: application/vnd.api+json")
                .withConnectionTimeoutMs(5000)
        );

        if (!stream)
        {
            DBG("Failed to connect to Keygen service for machine registration");
            return { false, "Unable to connect to license service for machine registration. Please check your internet connection." };
        }

        // Read response
        juce::String responseBody = stream->readEntireStreamAsString();

        if (responseBody.isEmpty())
        {
            DBG("Empty response from Keygen machine registration");
            return { false, "Empty response from license service" };
        }

        DBG("Keygen machine registration response: " << responseBody);

        // Parse and return result
        return parseMachineRegistrationResponse(responseBody);
    }
    catch (const std::exception& e)
    {
        std::string errorMsg = std::string("Machine registration failed: ") + e.what();
        DBG(errorMsg);
        return { false, errorMsg };
    }
    catch (...)
    {
        DBG("Machine registration failed with unknown error");
        return { false, "Machine registration failed. Please try again." };
    }
}

LicenseVerifier::VerificationResult LicenseVerifier::parseMachineRegistrationResponse(const juce::String& responseBody)
{
    // Parse JSON response from Keygen machine registration
    // Expected successful response: {"data":{"id":"<machine-id>"}}
    // Expected error response: various error formats

    if (responseBody.isEmpty())
        return { false, "Empty response from license service" };

    auto jsonValue = juce::JSON::parse(responseBody);

    if (jsonValue.isVoid())
    {
        DBG("Failed to parse machine registration response as JSON");
        DBG("Raw response was: " << responseBody);
        return { false, "Invalid response format from license service" };
    }

    VerificationResult result { false, "" };

    // Try to extract machine ID from data object
    if (jsonValue.hasProperty("data"))
    {
        auto dataObject = jsonValue.getProperty("data", juce::var());
        if (dataObject.isObject() && dataObject.hasProperty("id"))
        {
            juce::String machineId = dataObject.getProperty("id", juce::var("")).toString();
            result.machineId = machineId.toStdString();
            result.isValid = true;
            DBG("Machine registered successfully. Machine ID: " << result.machineId);
            return result;
        }
    }

    // Check for error responses in data or meta
    if (jsonValue.hasProperty("errors"))
    {
        auto errors = jsonValue.getProperty("errors", juce::var());
        if (errors.isArray() && errors.size() > 0)
        {
            auto firstError = errors[0];
            if (firstError.isObject() && firstError.hasProperty("detail"))
            {
                juce::String detail = firstError.getProperty("detail", juce::var("Unknown error")).toString();
                DBG("Machine registration error: " << detail);
                result.errorMessage = detail.toStdString();
                return result;
            }
        }
    }

    if (jsonValue.hasProperty("meta"))
    {
        auto metaObject = jsonValue.getProperty("meta", juce::var());
        if (metaObject.isObject() && metaObject.hasProperty("detail"))
        {
            juce::String detail = metaObject.getProperty("detail", juce::var("")).toString();
            if (detail.isNotEmpty())
            {
                DBG("Machine registration error detail: " << detail);
                result.errorMessage = detail.toStdString();
                return result;
            }
        }
    }

    DBG("Unexpected machine registration response format: " << responseBody);
    result.errorMessage = "Invalid response from license service";
    return result;
}
