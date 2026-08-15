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

    // Step 1: Validate with Keygen API to get license ID
    // This may fail with "must have exactly 1 associated machine" error,
    // but we'll extract the license ID from the response anyway
    auto validationResult = validateWithKeygen(trimmedKey);

    // If we got a license ID (validation succeeded or partially succeeded),
    // proceed to register the machine
    if (!validationResult.licenseId.empty())
    {
        DBG("Extracted license ID: " << validationResult.licenseId);

        // Step 2: Generate machine fingerprint and register it under the license
        std::string machineFingerprint = MachineFingerprint::getFingerprint();
        DBG("Registering machine fingerprint: " << machineFingerprint);

        auto machineResult = registerMachine(validationResult.licenseId, trimmedKey, machineFingerprint);

        if (machineResult.isValid)
        {
            DBG("Machine registered successfully. Machine ID: " << machineResult.machineId);

            // Step 3: Retry validation now that machine is registered
            DBG("Retrying license validation after machine registration");
            auto retryResult = validateWithKeygen(trimmedKey);

            if (retryResult.isValid)
            {
                retryResult.machineId = machineResult.machineId;
                retryResult.licenseId = validationResult.licenseId;
                DBG("License validation succeeded on retry");
                return retryResult;
            }
            else
            {
                // If retry still fails, return the retry error
                DBG("License validation still failed on retry: " << retryResult.errorMessage);
                return retryResult;
            }
        }
        else
        {
            // Machine registration failed
            DBG("Machine registration failed: " << machineResult.errorMessage);
            return { false, machineResult.errorMessage };
        }
    }

    // If we couldn't extract a license ID, return the original validation error
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
    //
    // IMPORTANT: We extract the license ID even if validation fails, because
    // Keygen may reject validation if the machine isn't registered yet ("must have exactly 1 associated machine").
    // We then proceed to register the machine and retry validation.

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

    // ALWAYS try to extract license ID from data object (present even if validation temporarily fails)
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
                    // Validation failed, but we might still proceed if we have a license ID
                    // (machine registration will happen, then validation will be retried)

                    // Try to get error detail if present
                    if (metaObject.hasProperty("detail"))
                    {
                        juce::String detail = metaObject.getProperty("detail", juce::var("")).toString();
                        if (detail.isNotEmpty())
                        {
                            DBG("Validation failed with detail: " << detail);
                            result.errorMessage = detail.toStdString();
                            // Don't return yet if we have a license ID - let caller proceed to machine registration
                            if (!result.licenseId.empty())
                            {
                                DBG("But we have a license ID, proceeding to machine registration");
                                return result;  // Return with isValid=false but with licenseId set
                            }
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
                            if (!result.licenseId.empty())
                            {
                                DBG("But we have a license ID, proceeding to machine registration");
                                return result;
                            }
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

LicenseVerifier::VerificationResult LicenseVerifier::registerMachine(const std::string& licenseId,
                                                                    const std::string& licenseKey,
                                                                    const std::string& machineFingerprint)
{
    try
    {
        // Machines are created at the TOP-LEVEL machines collection, with the owning
        // license attached via a JSON:API relationship. The nested
        // /licenses/{licenseId}/machines path is read-only and returns 404 on POST.
        // POST /v1/accounts/{accountId}/machines
        std::string urlString = std::string(KEYGEN_API_BASE) + "/" + KEYGEN_ACCOUNT_ID + "/machines";
        juce::URL url(urlString);

        DBG("Registering machine with Keygen");
        DBG("API Endpoint: " << urlString);
        DBG("License ID: " << licenseId);
        DBG("Machine fingerprint: " << machineFingerprint);

        // Build the JSON:API request body:
        // { "data": { "type": "machines",
        //             "attributes": { "fingerprint": "...", "platform": "windows" },
        //             "relationships": { "license": { "data": { "type": "licenses", "id": "..." } } } } }
        auto attributesObject = juce::var(new juce::DynamicObject());
        attributesObject.getDynamicObject()->setProperty("fingerprint", juce::String(machineFingerprint));
        attributesObject.getDynamicObject()->setProperty("platform", juce::String("windows"));

        auto licenseDataObject = juce::var(new juce::DynamicObject());
        licenseDataObject.getDynamicObject()->setProperty("type", juce::String("licenses"));
        licenseDataObject.getDynamicObject()->setProperty("id", juce::String(licenseId));

        auto licenseRelationship = juce::var(new juce::DynamicObject());
        licenseRelationship.getDynamicObject()->setProperty("data", licenseDataObject);

        auto relationshipsObject = juce::var(new juce::DynamicObject());
        relationshipsObject.getDynamicObject()->setProperty("license", licenseRelationship);

        auto dataObject = juce::var(new juce::DynamicObject());
        dataObject.getDynamicObject()->setProperty("type", juce::String("machines"));
        dataObject.getDynamicObject()->setProperty("attributes", attributesObject);
        dataObject.getDynamicObject()->setProperty("relationships", relationshipsObject);

        auto bodyObject = juce::var(new juce::DynamicObject());
        bodyObject.getDynamicObject()->setProperty("data", dataObject);

        juce::String jsonBody = juce::JSON::toString(bodyObject);
        DBG("Keygen machine registration request body: " << jsonBody);

        // Creating a machine requires authentication. Authenticate as the license
        // itself using the end user's key, so no admin token has to ship in the binary.
        juce::String headers = "Content-Type: application/vnd.api+json\r\n"
                               "Accept: application/vnd.api+json\r\n"
                               "Authorization: License " + juce::String(licenseKey);

        // Use JUCE 7.0.9 API: withPOSTData() on URL, then InputStreamOptions for headers/timeout
        auto stream = url.withPOSTData(jsonBody).createInputStream(
            juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                .withExtraHeaders(headers)
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
    // Expected conflict response (already exists): HTTP 409 with conflict detail
    // Expected error response: various error formats
    //
    // We treat "machine already exists" as success since it means the machine
    // is already registered with the license (idempotent operation)

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
            if (firstError.isObject())
            {
                // Check for conflict/duplicate machine (treat as success - already registered)
                if (firstError.hasProperty("status"))
                {
                    juce::String status = firstError.getProperty("status", juce::var("")).toString();
                    if (status == "409")  // HTTP 409 Conflict
                    {
                        DBG("Machine already registered with this license (409 Conflict)");
                        // Try to extract the machine ID from the conflict error or meta
                        if (firstError.hasProperty("detail"))
                        {
                            juce::String detail = firstError.getProperty("detail", juce::var("")).toString();
                            DBG("Conflict detail: " << detail);
                            // Don't fail on 409 - machine is already registered
                            result.isValid = true;
                            result.machineId = "existing";  // Placeholder, will retry validation
                            return result;
                        }
                        result.isValid = true;
                        result.machineId = "existing";
                        return result;
                    }
                }

                // Check for other detailed errors
                if (firstError.hasProperty("detail"))
                {
                    juce::String detail = firstError.getProperty("detail", juce::var("Unknown error")).toString();
                    DBG("Machine registration error: " << detail);

                    // Check if error mentions the machine already existing
                    juce::String detailLower = detail.toLowerCase();
                    if (detailLower.contains("already") && detailLower.contains("machine"))
                    {
                        DBG("Machine already associated with license");
                        result.isValid = true;
                        result.machineId = "existing";
                        return result;
                    }

                    result.errorMessage = detail.toStdString();
                    return result;
                }
            }
        }
    }

    if (jsonValue.hasProperty("meta"))
    {
        auto metaObject = jsonValue.getProperty("meta", juce::var());
        if (metaObject.isObject())
        {
            if (metaObject.hasProperty("detail"))
            {
                juce::String detail = metaObject.getProperty("detail", juce::var("")).toString();
                if (detail.isNotEmpty())
                {
                    DBG("Machine registration error detail: " << detail);

                    // Check if it's an "already exists" message
                    juce::String detailLower = detail.toLowerCase();
                    if (detailLower.contains("already"))
                    {
                        result.isValid = true;
                        result.machineId = "existing";
                        return result;
                    }

                    result.errorMessage = detail.toStdString();
                    return result;
                }
            }
        }
    }

    DBG("Unexpected machine registration response format: " << responseBody);
    result.errorMessage = "Invalid response from license service";
    return result;
}
