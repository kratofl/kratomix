#pragma once

#include <JuceHeader.h>

namespace kratomix::controls
{
inline juce::String formatDrive(float value)
{
    return juce::String(value, 1);
}

inline juce::String formatFrequency(float value)
{
    return juce::String(juce::roundToInt(value));
}

inline juce::String formatFrequencyWithUnits(float value)
{
    return formatFrequency(value) + " Hz";
}

inline juce::String formatWarmth(float value)
{
    return juce::String(value, 1);
}

inline juce::String formatWarmthWithUnits(float value)
{
    return formatWarmth(value) + " dB";
}

inline juce::String formatPresence(float value)
{
    return juce::String(value, 1);
}

inline juce::String formatPresenceWithUnits(float value)
{
    return formatPresence(value) + " dB";
}

inline juce::String formatAir(float value)
{
    return juce::String(value, 1);
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
