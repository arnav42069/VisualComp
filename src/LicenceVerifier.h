// LicenseVerifier.h
#pragma once

#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>

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

    VerificationResult verify(const std::string& base64LicenseKey);

private:
    static constexpr std::array<uint8_t, 32> ED25519_PUBLIC_KEY{{
        0xef, 0x9b, 0x67, 0x62, 0x72, 0x3f, 0x5d, 0x5c,
        0xa7, 0x29, 0x02, 0x81, 0x1c, 0xd2, 0x14, 0x94,
        0xdf, 0xd2, 0x2a, 0xf5, 0x6d, 0x4d, 0x73, 0xaf,
        0x97, 0x1c, 0xb6, 0xdb, 0xbc, 0x20, 0xe9
    }};

    static constexpr const char* PRODUCT_ID = "7c8bc57b-1356-4f36-b757-cf1511516f29";
    static constexpr const char* POLICY_ID = "b249f536-4179-4021-9567-a67327c2beae";
    static constexpr size_t ED25519_SIGNATURE_BYTES = 64;

    std::optional<std::vector<uint8_t>> decodeBase64(const std::string& encoded);
    VerificationResult verifyPayload(const std::vector<uint8_t>& data);
};

// LicenseVerifier.cpp
#include "LicenseVerifier.h"
#include <monocypher.h>
#include <algorithm>
#include <cstring>

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
    if (data.size() < ED25519_SIGNATURE_BYTES)
        return { false, "Invalid license data size" };

    size_t payload_size = data.size() - ED25519_SIGNATURE_BYTES;
    const uint8_t* payload = data.data();
    const uint8_t* signature = data.data() + payload_size;

    if (crypto_check(const_cast<uint8_t*>(signature),
                     ED25519_PUBLIC_KEY.data(),
                     payload,
                     payload_size) != 0)
    {
        return { false, "Cryptographic signature verification failed" };
    }

    std::string payload_text(reinterpret_cast<const char*>(payload), payload_size);

    if (payload_text.find(PRODUCT_ID) == std::string::npos)
        return { false, "Product ID not found in license" };

    if (payload_text.find(POLICY_ID) == std::string::npos)
        return { false, "Policy ID not found in license" };

    return { true, "" };
}

LicenseVerifier::VerificationResult LicenseVerifier::verify(const std::string& base64LicenseKey)
{
    auto decoded = decodeBase64(base64LicenseKey);
    if (!decoded)
        return { false, "Failed to decode Base64 license key" };

    return verifyPayload(*decoded);
}