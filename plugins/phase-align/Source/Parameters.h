#pragma once

#include <JuceHeader.h>

namespace kratomix::phase_align
{
inline constexpr auto offsetId = "offsetMs";
inline constexpr auto polarityId = "polarity";
inline constexpr auto auditionModeId = "auditionMode";
inline constexpr auto lockId = "lock";
inline constexpr auto bypassId = "bypass";

inline juce::StringArray auditionModeChoices()
{
    return { "Original", "Aligned", "Difference" };
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Parameter = juce::RangedAudioParameter;
    std::vector<std::unique_ptr<Parameter>> layout;

    layout.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { offsetId, 1 }, "Offset",
        juce::NormalisableRange<float>(-10.0f, 10.0f, 0.0001f), 0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("ms")
            .withStringFromValueFunction([](float value, int) { return juce::String(value, 4); })
            .withValueFromStringFunction([](const juce::String& text) { return text.getFloatValue(); })));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { polarityId, 1 }, "Invert Polarity", false));
    layout.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { auditionModeId, 1 }, "Audition", auditionModeChoices(), 1));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { lockId, 1 }, "Lock Result", false));
    layout.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { bypassId, 1 }, "Bypass", false));

    return { layout.begin(), layout.end() };
}
}
