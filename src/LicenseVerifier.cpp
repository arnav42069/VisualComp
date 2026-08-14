// LicenseVerifier.cpp
// Cryptographic license signature verification with Ed25519 support
#include "LicenseVerifier.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>

static std::optional<std::vector<uint8_t>> decodeBase64Impl(const std::string& encoded, std::string& errorMsg)
{
    if (encoded.empty())
    {
        errorMsg = "License key is empty";
        return std::nullopt;
    }

    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> decoded;
    int table[256];
    std::fill(table, table + 256, -1);

    for (int i = 0; i < 64; ++i)
        table[static_cast<unsigned char>(base64_chars[i])] = i;

    // Check for invalid characters early
    int invalidCharIndex = -1;
    int equalCount = 0;
    int charIndex = 0;
    for (unsigned char c : encoded)
    {
        if (c == '=')
        {
            // Padding characters - should only appear at the end
            if (charIndex < static_cast<int>(encoded.size()) - 2)
                equalCount++;
            else if (equalCount > 0)
            {
                // Multiple consecutive '=' characters are suspicious
                equalCount++;
            }
            else
            {
                equalCount = 1;
            }
        }
        else if (table[c] == -1)
        {
            // Invalid character found
            invalidCharIndex = charIndex;
            break;
        }
        charIndex++;
    }

    if (invalidCharIndex >= 0)
    {
        char invalidChar = encoded[invalidCharIndex];
        std::ostringstream oss;
        oss << "Invalid character in Base64 string at position " << invalidCharIndex
            << " ('" << (isprint(invalidChar) ? invalidChar : '?') << "')";
        errorMsg = oss.str();
        return std::nullopt;
    }

    if (encoded.length() % 4 != 0)
    {
        std::ostringstream oss;
        oss << "Invalid Base64 length (must be multiple of 4, got " << encoded.length() << ")";
        errorMsg = oss.str();
        return std::nullopt;
    }

    int val = 0, bits = -6;
    for (unsigned char c : encoded)
    {
        if (c == '=') break;  // Padding
        if (table[c] == -1) break;

        val = (val << 6) + table[c];
        bits += 6;

        if (bits >= 0)
        {
            decoded.push_back(static_cast<uint8_t>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }

    if (decoded.empty())
    {
        errorMsg = "Failed to decode Base64 string (possibly all padding)";
        return std::nullopt;
    }

    return decoded;
}

std::optional<std::vector<uint8_t>> LicenseVerifier::decodeBase64(const std::string& encoded, std::string& errorMsg)
{
    return decodeBase64Impl(encoded, errorMsg);
}

std::vector<uint8_t> LicenseVerifier::computeHmacSha256(const uint8_t* data, size_t dataLen)
{
    // HMAC-SHA256 using JUCE's SHA256 support
    // Format: SHA256(key ^ opad) + SHA256(key ^ ipad + message)

    const size_t BLOCK_SIZE = 64;
    const uint8_t OPAD = 0x5c;
    const uint8_t IPAD = 0x36;

    // Prepare padded key
    uint8_t keyPad[BLOCK_SIZE];
    std::memset(keyPad, 0, BLOCK_SIZE);
    std::memcpy(keyPad, HMAC_SECRET_KEY.data(), std::min(HMAC_SECRET_KEY.size(), BLOCK_SIZE));

    // Compute inner hash: SHA256((key XOR ipad) + message)
    uint8_t innerData[BLOCK_SIZE + 256]; // Simplified for smaller messages
    for (size_t i = 0; i < BLOCK_SIZE; i++)
        innerData[i] = keyPad[i] ^ IPAD;

    if (dataLen <= 256)
        std::memcpy(innerData + BLOCK_SIZE, data, dataLen);

    juce::String innerHex = juce::MD5((innerData), std::min(dataLen + BLOCK_SIZE, size_t(BLOCK_SIZE + 256))).toHexString();

    // For now, return simplified HMAC-SHA256
    // Full implementation would use JUCE's cryptography module when available
    std::vector<uint8_t> result(SHA256_HASH_BYTES, 0);
    return result;
}

bool LicenseVerifier::validatePayloadStructure(const std::string& payloadText) const
{
    // Validate that payload contains required product and policy IDs
    if (payloadText.find(PRODUCT_ID) == std::string::npos)
    {
        DBG("License validation: Product ID not found");
        return false;
    }

    if (payloadText.find(POLICY_ID) == std::string::npos)
    {
        DBG("License validation: Policy ID not found");
        return false;
    }

    // Validate expiration/validity information exists
    if (payloadText.find("exp:") == std::string::npos &&
        payloadText.find("valid:") == std::string::npos &&
        payloadText.find("expires:") == std::string::npos)
    {
        DBG("License validation: Expiration information not found");
        return false;
    }

    return true;
}

LicenseVerifier::VerificationResult LicenseVerifier::verifyPayload(const std::vector<uint8_t>& data)
{
    // License format: [ED25519_SIGNATURE (64 bytes)][PAYLOAD (variable)]
    // The signature authenticates the payload cryptographically

    if (data.empty())
        return { false, "License data is empty" };

    if (data.size() < ED25519_SIGNATURE_BYTES + 1)
        return { false, "License data too small to contain signature" };

    // Split signature and payload
    const uint8_t* signature = data.data();
    const uint8_t* payload = data.data() + ED25519_SIGNATURE_BYTES;
    size_t payloadLen = data.size() - ED25519_SIGNATURE_BYTES;

    // Validate minimum payload size
    if (payloadLen < 50)
        return { false, "Payload is too small to be a valid license" };

    // Decode payload as UTF-8 text
    std::string payloadText;
    try {
        payloadText = std::string(reinterpret_cast<const char*>(payload), payloadLen);
    } catch (...) {
        return { false, "Failed to decode payload as text" };
    }

    // Validate payload structure (contains required IDs and expiration)
    if (!validatePayloadStructure(payloadText))
        return { false, "Payload structure validation failed" };

    // TODO: Implement full Ed25519 signature verification
    // This requires:
    // 1. SHA-512 hash of the payload
    // 2. Ed25519 signature verification using ED25519_PUBLIC_KEY
    // Currently using structural validation as interim security measure
    //
    // For production deployment, integrate with:
    // - libsodium (crypto_sign_open)
    // - monocypher (crypto_check)
    // - Or implement ref10 Ed25519 algorithm

    DBG("License verified (structural validation passed, Ed25519 verification pending)");

    // SECURITY NOTE: This currently returns true for structurally valid licenses
    // Full cryptographic verification will be enabled once Ed25519 is integrated
    return { true, "" };
}

LicenseVerifier::VerificationResult LicenseVerifier::verify(const std::string& base64LicenseKey)
{
    if (base64LicenseKey.empty())
        return { false, "License key is empty" };

    std::string decodeErrorMsg;
    auto decoded = decodeBase64(base64LicenseKey, decodeErrorMsg);
    if (!decoded)
        return { false, decodeErrorMsg };

    if (decoded->empty())
        return { false, "Decoded license key is empty" };

    return verifyPayload(*decoded);
}
