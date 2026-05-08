#pragma once

#include <JuceHeader.h>

#include <array>

namespace kratomix::controls
{
inline constexpr std::array<float, 7> driveSteps { 0.0f, 1.5f, 3.0f, 4.5f, 6.0f, 7.5f, 9.0f };
inline constexpr std::array<float, 6> highPassSteps { 20.0f, 35.0f, 50.0f, 80.0f, 120.0f, 160.0f };
inline constexpr std::array<float, 7> warmthSteps { -4.0f, -2.0f, 0.0f, 1.5f, 3.0f, 4.5f, 6.0f };
inline constexpr std::array<float, 7> presenceSteps { -4.0f, -2.0f, 0.0f, 1.5f, 3.0f, 4.5f, 6.0f };
inline constexpr std::array<float, 7> airSteps { -4.0f, -2.0f, 0.0f, 1.0f, 2.5f, 4.0f, 6.0f };

template <size_t NumSteps>
float snapToNearest(float value, const std::array<float, NumSteps>& steps) noexcept
{
    auto closest = steps.front();
    auto bestDistance = std::abs(value - closest);

    for (const auto step : steps)
    {
        const auto distance = std::abs(value - step);

        if (distance < bestDistance)
        {
            bestDistance = distance;
            closest = step;
        }
    }

    return closest;
}

inline float snapDrive(float value) noexcept
{
    return snapToNearest(value, driveSteps);
}

inline float snapHighPass(float value) noexcept
{
    return snapToNearest(value, highPassSteps);
}

inline float snapWarmth(float value) noexcept
{
    return snapToNearest(value, warmthSteps);
}

inline float snapPresence(float value) noexcept
{
    return snapToNearest(value, presenceSteps);
}

inline float snapAir(float value) noexcept
{
    return snapToNearest(value, airSteps);
}

inline juce::String formatDrive(float value)
{
    return juce::String(snapDrive(value), 1);
}

inline juce::String formatFrequency(float value)
{
    return juce::String(juce::roundToInt(snapHighPass(value)));
}

inline juce::String formatFrequencyWithUnits(float value)
{
    return formatFrequency(value) + " Hz";
}

inline juce::String formatWarmth(float value)
{
    return juce::String(snapWarmth(value), 1);
}

inline juce::String formatWarmthWithUnits(float value)
{
    return formatWarmth(value) + " dB";
}

inline juce::String formatPresence(float value)
{
    return juce::String(snapPresence(value), 1);
}

inline juce::String formatPresenceWithUnits(float value)
{
    return formatPresence(value) + " dB";
}

inline juce::String formatAir(float value)
{
    return juce::String(snapAir(value), 1);
}

inline juce::String formatAirWithUnits(float value)
{
    return formatAir(value) + " dB";
}

inline juce::String formatPlainDecibels(float value)
{
    return juce::String(value, 1);
}

inline double parseNumericText(const juce::String& text, double fallback = 0.0)
{
    auto sanitized = text.trim().replaceCharacter(',', '.').retainCharacters("+-0123456789.");

    if (sanitized.isEmpty() || sanitized == "+" || sanitized == "-" || sanitized == ".")
        return fallback;

    return sanitized.getDoubleValue();
}
}
