#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>
#include <vector>

namespace kratomix::prism
{
inline constexpr int maxBands = 16;

enum class BandType
{
    bell = 0,
    lowShelf,
    highShelf,
    highPass,
    lowPass,
    notch
};

enum class SidechainSource
{
    main = 0,
    external
};

enum class PhaseMode
{
    zeroLatency = 0,
    natural,
    linearPhase
};

enum class QualityMode
{
    native = 0,
    oversample2x,
    oversample4x
};

inline juce::String bandPrefix(int oneBasedIndex)
{
    return "band" + juce::String(oneBasedIndex).paddedLeft('0', 2);
}

inline constexpr std::array<const char*, 10> globalParameterIds()
{
    return { "inputGain", "outputGain", "mix", "bypass", "analyzerMode", "analyzerSpeed", "analyzerRange", "gainScale", "phaseMode", "qualityMode" };
}

inline juce::StringArray bandTypeChoices()
{
    return { "Bell", "Low Shelf", "High Shelf", "High-pass", "Low-pass", "Notch" };
}

inline juce::StringArray sidechainSourceChoices()
{
    return { "Main", "External" };
}

inline juce::StringArray analyzerModeChoices()
{
    return { "Pre", "Post", "Sidechain", "Pre + Post", "Pre + Post + Sidechain", "Masking" };
}

inline juce::StringArray phaseModeChoices()
{
    return { "Zero Latency", "Natural", "Linear Phase" };
}

inline juce::StringArray qualityModeChoices()
{
    return { "Native", "2x", "4x" };
}

inline juce::NormalisableRange<float> frequencyRange()
{
    juce::NormalisableRange<float> range { 20.0f, 20000.0f, 1.0f };
    range.setSkewForCentre(1000.0f);
    return range;
}

inline juce::NormalisableRange<float> qRange()
{
    juce::NormalisableRange<float> range { 0.1f, 40.0f, 0.01f };
    range.setSkewForCentre(1.0f);
    return range;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Parameter = juce::RangedAudioParameter;
    std::vector<std::unique_ptr<Parameter>> params;

    const auto displayDb = [](float value, int) { return juce::String(value, 1); };
    const auto displayFrequency = [](float value, int) { return juce::String(static_cast<int>(std::round(value))); };
    const auto parseFloat = [](const juce::String& text) { return text.getFloatValue(); };

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "inputGain", 1 },
        "Input",
        juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(displayDb)
            .withValueFromStringFunction(parseFloat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "outputGain", 1 },
        "Output",
        juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(displayDb)
            .withValueFromStringFunction(parseFloat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "mix", 1 },
        "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { "bypass", 1 },
        "Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "analyzerMode", 1 },
        "Analyzer Mode",
        analyzerModeChoices(),
        4));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "analyzerSpeed", 1 },
        "Analyzer Speed",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        0.5f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "analyzerRange", 1 },
        "Analyzer Range",
        juce::NormalisableRange<float> { 24.0f, 120.0f, 1.0f },
        72.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { "gainScale", 1 },
        "Gain Scale",
        juce::NormalisableRange<float> { 0.25f, 2.0f, 0.01f },
        1.0f));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "phaseMode", 1 },
        "Phase Mode",
        phaseModeChoices(),
        0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "qualityMode", 1 },
        "Quality Mode",
        qualityModeChoices(),
        static_cast<int>(QualityMode::native)));

    for (int index = 1; index <= maxBands; ++index)
    {
        const auto prefix = bandPrefix(index);
        const auto labelPrefix = "Band " + juce::String(index) + " ";

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { prefix + "Enabled", 1 },
            labelPrefix + "Enabled",
            false));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { prefix + "Type", 1 },
            labelPrefix + "Type",
            bandTypeChoices(),
            static_cast<int>(BandType::bell)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Frequency", 1 },
            labelPrefix + "Frequency",
            frequencyRange(),
            1000.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction(displayFrequency)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Gain", 1 },
            labelPrefix + "Gain",
            juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Q", 1 },
            labelPrefix + "Q",
            qRange(),
            1.0f));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { prefix + "DynamicEnabled", 1 },
            labelPrefix + "Dynamic Enabled",
            false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "DynamicRange", 1 },
            labelPrefix + "Dynamic Range",
            juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Threshold", 1 },
            labelPrefix + "Threshold",
            juce::NormalisableRange<float> { -90.0f, 0.0f, 0.1f },
            -24.0f,
            juce::AudioParameterFloatAttributes().withLabel("dB")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Attack", 1 },
            labelPrefix + "Attack",
            juce::NormalisableRange<float> { 0.1f, 200.0f, 0.1f },
            20.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { prefix + "Release", 1 },
            labelPrefix + "Release",
            juce::NormalisableRange<float> { 5.0f, 1000.0f, 0.1f },
            120.0f,
            juce::AudioParameterFloatAttributes().withLabel("ms")));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { prefix + "SidechainSource", 1 },
            labelPrefix + "Sidechain Source",
            sidechainSourceChoices(),
            static_cast<int>(SidechainSource::main)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { prefix + "Solo", 1 },
            labelPrefix + "Solo",
            false));
    }

    return { params.begin(), params.end() };
}
}
