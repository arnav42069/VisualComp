// MachineFingerprint.cpp
// Generate stable hardware fingerprints for Keygen machine registration
#include "MachineFingerprint.h"
#include <JuceHeader.h>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#include <windows.h>
#include <intrin.h>
#endif

std::string MachineFingerprint::generate()
{
    std::stringstream ss;

    // Combine multiple hardware identifiers for a stable fingerprint
    // These should remain constant across reboots and OS updates

    std::string volumeSerial = getVolumeSerialNumber();
    if (!volumeSerial.empty())
        ss << "VOL:" << volumeSerial << "|";

    std::string cpuId = getCpuId();
    if (!cpuId.empty())
        ss << "CPU:" << cpuId << "|";

    // Motherboard serial is harder to get reliably, but try it
    std::string mbSerial = getMotherboardSerial();
    if (!mbSerial.empty())
        ss << "MB:" << mbSerial << "|";

    std::string combined = ss.str();

    // If we got nothing, fall back to a system identifier
    if (combined.empty())
    {
        combined = "DEFAULT:" + std::to_string(GetTickCount64());
    }

    DBG("Machine fingerprint raw: " << combined);

    return combined;
}

std::string MachineFingerprint::getFingerprint()
{
    // Return SHA-256 hash of the full fingerprint for a shorter, suitable ID
    std::string raw = generate();
    return sha256(raw);
}

std::string MachineFingerprint::getVolumeSerialNumber()
{
    // Get the serial number of the C: drive (most stable identifier)
    DWORD serialNumber = 0;
    DWORD maxComponentLength = 0;
    DWORD fileSystemFlags = 0;

    if (GetVolumeInformationA(
            "C:\\",                    // Root of C: drive
            nullptr,                   // Volume name buffer
            0,                         // Volume name buffer size
            &serialNumber,             // Serial number
            &maxComponentLength,       // Max component length
            &fileSystemFlags,          // File system flags
            nullptr,                   // File system name buffer
            0                          // File system name buffer size
    ))
    {
        std::stringstream ss;
        ss << std::hex << std::setw(8) << std::setfill('0') << serialNumber;
        return ss.str();
    }

    return "";
}

std::string MachineFingerprint::getCpuId()
{
    // Use CPUID instruction to get CPU identifier
    // This is stable across reboots and OS updates
    int cpuInfo[4] = { -1 };
    unsigned nExIds = 0;

    // Get the max extended ID
    __cpuid(cpuInfo, 0x80000000);
    nExIds = cpuInfo[0];

    // Get CPU brand string (48 bytes from CPUID 0x80000002, 0x80000003, 0x80000004)
    std::string brand;
    if (nExIds >= 0x80000004)
    {
        for (unsigned i = 0x80000002; i <= 0x80000004; ++i)
        {
            __cpuid(cpuInfo, i);
            // Each call returns 16 bytes of brand string
            char* bytes = (char*)cpuInfo;
            for (int j = 0; j < 16; ++j)
                brand += bytes[j];
        }
    }

    // Clean up the brand string (remove nulls and trim)
    brand.erase(brand.find('\0'));
    // Convert to hex for a shorter ID
    if (!brand.empty())
    {
        std::stringstream ss;
        for (char c : brand)
        {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)(unsigned char)c;
        }
        return ss.str();
    }

    // Fallback: get CPU info from CPUID 0x01
    __cpuid(cpuInfo, 1);
    std::stringstream ss;
    ss << std::hex << cpuInfo[0] << cpuInfo[1] << cpuInfo[2] << cpuInfo[3];
    return ss.str();
}

std::string MachineFingerprint::getMotherboardSerial()
{
    // Try to get motherboard serial via WMI
    // This is harder and less reliable than volume serial, but if available, it's stable
    // For now, we'll skip this as it requires WMI initialization which can be slow
    // The volume serial + CPU ID should be sufficient for most cases
    return "";
}

std::string MachineFingerprint::sha256(const std::string& input)
{
    // Use JUCE's MD5 hash as a simple fingerprint hash
    // (SHA256 not easily available in JUCE 7.0.9 without external libs)
    juce::String md5Hash = juce::MD5((const uint8_t*)input.data(), input.size()).toHexString();
    return md5Hash.toStdString();
}
