// LicenseVerifier.cpp
#include "LicenseVerifier.h"
#include <algorithm>
#include <cstring>
#include <sstream>

static std::optional<std::vector<uint8_t>> decodeBase64Impl(const std::string& encoded)
{
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> decoded;
    int table[256];
    std::fill(table, table + 256, -1);

    for (int i = 0; i < 64; ++i)
        table[static_cast<unsigned char>(base64_chars[i])] = i;

    int val = 0, bits = -6;
    for (unsigned char c : encoded)
    {
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
        return std::nullopt;

    return decoded;
}

std::optional<std::vector<uint8_t>> LicenseVerifier::decodeBase64(const std::string& encoded)
{
    return decodeBase64Impl(encoded);
}

LicenseVerifier::VerificationResult LicenseVerifier::verifyPayload(const std::vector<uint8_t>& data)
{
    // For demo purposes: decode the license and check that it contains required fields
    // In production, this would use cryptographic signature verification (Ed25519)

    if (data.empty() || data.size() < 50)
        return { false, "Invalid license data size" };

    std::string payload_text(reinterpret_cast<const char*>(data.data()), data.size());

    if (payload_text.find(PRODUCT_ID) == std::string::npos)
        return { false, "Product ID not found in license" };

    if (payload_text.find(POLICY_ID) == std::string::npos)
        return { false, "Policy ID not found in license" };

    // Check that the license contains a valid timestamp
    if (payload_text.find("exp:") == std::string::npos &&
        payload_text.find("valid:") == std::string::npos)
        return { false, "License validity information not found" };

    return { true, "" };
}

LicenseVerifier::VerificationResult LicenseVerifier::verify(const std::string& base64LicenseKey)
{
    auto decoded = decodeBase64(base64LicenseKey);
    if (!decoded)
        return { false, "Failed to decode Base64 license key" };

    return verifyPayload(*decoded);
}
