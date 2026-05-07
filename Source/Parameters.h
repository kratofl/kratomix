#pragma once

#include <JuceHeader.h>

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
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::inputGain,
        "Input",
        juce::NormalisableRange<float> { -18.0f, 18.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::drive,
        "Drive",
        juce::NormalisableRange<float> { 0.0f, 10.0f, 0.1f },
        2.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::highPass,
        "High-pass",
        juce::NormalisableRange<float> { 20.0f, 180.0f, 1.0f },
        35.0f,
        juce::AudioParameterFloatAttributes().withLabel("Hz")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::warmth,
        "Warmth",
        juce::NormalisableRange<float> { -6.0f, 6.0f, 0.1f },
        1.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::presence,
        "Presence",
        juce::NormalisableRange<float> { -6.0f, 6.0f, 0.1f },
        0.5f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::air,
        "Air",
        juce::NormalisableRange<float> { -6.0f, 6.0f, 0.1f },
        1.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamID::outputGain,
        "Output",
        juce::NormalisableRange<float> { -18.0f, 18.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    return { params.begin(), params.end() };
}
}
