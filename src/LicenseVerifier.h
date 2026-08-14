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
    // Ed25519 public key for signature verification (hardcoded for this product)
    static constexpr std::array<uint8_t, 32> ED25519_PUBLIC_KEY{{
        0xef, 0x9b, 0x67, 0x62, 0x72, 0x3f, 0x5d, 0x5c,
        0xa7, 0x29, 0x02, 0x81, 0x1c, 0xd2, 0x14, 0x94,
        0xdf, 0xd2, 0x2a, 0xf5, 0x6d, 0x4d, 0x73, 0xaf,
        0x97, 0x1c, 0xb6, 0xdb, 0xbc, 0x20, 0xe9, 0x8f
    }};

    static constexpr const char* PRODUCT_ID = "7c8bc57b-1356-4f36-b757-cf1511516f29";
    static constexpr const char* POLICY_ID = "b249f536-4179-4021-9567-a67327c2beae";
    static constexpr size_t ED25519_SIGNATURE_BYTES = 64;

    std::optional<std::vector<uint8_t>> decodeBase64(const std::string& encoded);
    VerificationResult verifyPayload(const std::vector<uint8_t>& data);
};
