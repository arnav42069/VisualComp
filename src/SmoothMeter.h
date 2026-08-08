#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <cmath>

//==============================================================================
// SmoothMeterValue: Lock-free, smooth-interpolating meter value
//==============================================================================
// Atomic double-buffered meter values with fast-tracking attack and slow
// decay release for visual smoothness without blocking the audio thread.
class SmoothMeterValue
{
public:
    explicit SmoothMeterValue(float attackMs = 5.0f, float releaseMs = 100.0f)
        : attackMs(attackMs), releaseMs(releaseMs)
    {
        displayValue.store(floorDb, std::memory_order_release);
        targetValue.store(floorDb, std::memory_order_release);
    }

    // Audio thread: write new peak/rms value (non-blocking)
    void update(float newValue) noexcept
    {
        targetValue.store(juce::jlimit(floorDb, 0.0f, newValue), std::memory_order_release);
    }

    // UI thread: fetch the current smoothly-interpolated display value
    float getDisplayValue() const noexcept
    {
        return displayValue.load(std::memory_order_acquire);
    }

    // UI thread: update internal state (call this from a high-freq timer)
    // deltaTimeMs: milliseconds since last update
    void updateSmoothing(float deltaTimeMs) noexcept
    {
        const float target = targetValue.load(std::memory_order_acquire);
        float current = displayValue.load(std::memory_order_acquire);

        if (target > current)
        {
            // Fast attack
            const float attackCoeff = 1.0f - std::exp(-deltaTimeMs / (attackMs * 0.5f));
            current = current + (target - current) * attackCoeff;
        }
        else if (target < current)
        {
            // Slower release
            const float releaseCoeff = 1.0f - std::exp(-deltaTimeMs / releaseMs);
            current = current + (target - current) * releaseCoeff;
        }

        displayValue.store(current, std::memory_order_release);
    }

    // Peak-hold: maintain a 3-second rolling max (for peak hold display)
    float getPeakHold() const noexcept
    {
        return peakHold.load(std::memory_order_acquire);
    }

    void updatePeakHold(float deltaTimeMs) noexcept
    {
        const float current = displayValue.load(std::memory_order_acquire);
        float peak = peakHold.load(std::memory_order_acquire);

        // Update peak if current is higher
        if (current > peak)
            peak = current;

        // Decay peak very slowly (3-second hold)
        const float decayCoeff = 1.0f - std::exp(-deltaTimeMs / 3000.0f);
        peak = peak + (floorDb - peak) * decayCoeff;

        peakHold.store(peak, std::memory_order_release);
    }

    // Reset to floor
    void reset() noexcept
    {
        displayValue.store(floorDb, std::memory_order_release);
        targetValue.store(floorDb, std::memory_order_release);
        peakHold.store(floorDb, std::memory_order_release);
    }

    // Configuration
    void setAttackMs(float ms) noexcept { attackMs = ms; }
    void setReleaseMs(float ms) noexcept { releaseMs = ms; }

    static constexpr float floorDb = -100.0f;

private:
    // Audio thread writes to targetValue, UI thread reads and smooths into displayValue
    std::atomic<float> targetValue    { floorDb };
    std::atomic<float> displayValue   { floorDb };
    std::atomic<float> peakHold       { floorDb };

    float attackMs, releaseMs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SmoothMeterValue)
};

//==============================================================================
// Lock-free ring buffer for passing meter updates from audio to UI thread
//==============================================================================
struct MeterUpdate
{
    float peakDb, rmsDb, momLufs, shortLufs;
};

class MeterUpdateQueue
{
public:
    static constexpr int capacity = 128;

    void push(const MeterUpdate& update) noexcept
    {
        const int writeIdx = (writePos.load(std::memory_order_relaxed)) % capacity;
        buffer[writeIdx] = update;
        writePos.fetch_add(1, std::memory_order_release);
    }

    // Returns true if an update was available and popped into `out`
    bool pop(MeterUpdate& out) noexcept
    {
        int read = readPos.load(std::memory_order_acquire);
        int write = writePos.load(std::memory_order_acquire);

        if (read >= write) return false;

        out = buffer[read % capacity];
        readPos.fetch_add(1, std::memory_order_release);
        return true;
    }

    void clear() noexcept
    {
        readPos.store(0, std::memory_order_release);
        writePos.store(0, std::memory_order_release);
    }

private:
    std::array<MeterUpdate, capacity> buffer = {};
    std::atomic<int> readPos { 0 }, writePos { 0 };
};
