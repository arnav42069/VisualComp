// LicenseVerifier.h
// Cryptographic license signature verification
#pragma once

#include <JuceHeader.h>
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

    // Verify a Base64-encoded license key with cryptographic signature
    VerificationResult verify(const std::string& base64LicenseKey);

private:
    // Ed25519 public key for signature verification (hardcoded for this product)
    // This key is used to cryptographically verify all license signatures
    static constexpr std::array<uint8_t, 32> ED25519_PUBLIC_KEY{{
        0xef, 0x9b, 0x67, 0x62, 0x72, 0x3f, 0x5d, 0x5c,
        0xa7, 0x29, 0x02, 0x81, 0x1c, 0xd2, 0x14, 0x94,
        0xdf, 0xd2, 0x2a, 0xf5, 0x6d, 0x4d, 0x73, 0xaf,
        0x97, 0x1c, 0xb6, 0xdb, 0xbc, 0x20, 0xe9, 0x8f
    }};

    // HMAC-SHA256 secret key for payload integrity (hardcoded for this product)
    // Used as fallback validation before full Ed25519 support
    static constexpr std::array<uint8_t, 32> HMAC_SECRET_KEY{{
        0x42, 0x7d, 0x8a, 0x91, 0x2c, 0xb6, 0x5e, 0xd4,
        0x7f, 0x3a, 0x1e, 0x9c, 0x64, 0xbb, 0x5d, 0xf7,
        0x81, 0x23, 0xae, 0x4c, 0x9b, 0x58, 0x2d, 0x16,
        0xc3, 0x7a, 0x42, 0xef, 0x8b, 0x19, 0x54, 0xc2
    }};

    static constexpr const char* PRODUCT_ID = "7c8bc57b-1356-4f36-b757-cf1511516f29";
    static constexpr const char* POLICY_ID = "b249f536-4179-4021-9567-a67327c2beae";
    static constexpr size_t ED25519_SIGNATURE_BYTES = 64;
    static constexpr size_t SHA256_HASH_BYTES = 32;

    std::optional<std::vector<uint8_t>> decodeBase64(const std::string& encoded);
    VerificationResult verifyPayload(const std::vector<uint8_t>& data);

    // Compute HMAC-SHA256 for payload integrity validation
    std::vector<uint8_t> computeHmacSha256(const uint8_t* data, size_t dataLen);

    // Verify that the payload contains required IDs (basic structural validation)
    bool validatePayloadStructure(const std::string& payloadText) const;
};
