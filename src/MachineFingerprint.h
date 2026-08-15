// MachineFingerprint.h
// Generate stable hardware fingerprints for Keygen machine registration
#pragma once

#include <string>
#include <cstdint>

class MachineFingerprint
{
public:
    // Generate a unique, stable hardware fingerprint for this machine
    // Combines Windows volume serial, CPU ID, and motherboard info
    // Result is deterministic across reboots
    static std::string generate();

    // Get the SHA-256 hash of the fingerprint (shorter, suitable for IDs)
    static std::string getFingerprint();

private:
    // Windows-specific helpers
    static std::string getVolumeSerialNumber();
    static std::string getCpuId();
    static std::string getMotherboardSerial();

    // Compute SHA-256 hash of raw fingerprint data
    static std::string sha256(const std::string& input);
};
