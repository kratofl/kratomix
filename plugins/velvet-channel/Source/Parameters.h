#pragma once

#include <JuceHeader.h>

#include "ControlValues.h"

namespace kratomix
{
namespace ParamID
{
    inline constexpr auto inputGain = "inputGain";
    inline constexpr auto drive = "drive";
    inline constexpr auto highPass = "highPass";
    inline constexpr auto warmth = "warmth";
    inline constexpr auto presence = "presence";
    inline constexpr auto air = "air";
    inline constexpr auto outputGain = "outputGain";
    inline constexpr auto bypass = "bypass";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::inputGain,
        "Input",
        juce::NormalisableRange<float> { -18.0f, 18.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::drive,
        "Drive",
        juce::NormalisableRange<float> { 0.0f, 10.0f },
        3.0f,
        juce::AudioParameterFloatAttributes()
            .withStringFromValueFunction([](float value, int) { return controls::formatDrive(value); })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::highPass,
        "High-pass",
        juce::NormalisableRange<float> { 20.0f, 180.0f },
        35.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("Hz")
            .withStringFromValueFunction([](float value, int) { return controls::formatFrequencyWithUnits(value); })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::warmth,
        "Warmth",
        juce::NormalisableRange<float> { -6.0f, 6.0f },
        3.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) { return controls::formatWarmthWithUnits(value); })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::presence,
        "Presence",
        juce::NormalisableRange<float> { -6.0f, 6.0f },
        1.5f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) { return controls::formatPresenceWithUnits(value); })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::air,
        "Air",
        juce::NormalisableRange<float> { -6.0f, 6.0f },
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction([](float value, int) { return controls::formatAirWithUnits(value); })));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::outputGain,
        "Output",
        juce::NormalisableRange<float> { -18.0f, 18.0f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        ParamID::bypass,
        "Bypass",
        false));

    return { params.begin(), params.end() };
}
}
