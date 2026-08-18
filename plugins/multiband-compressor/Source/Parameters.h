#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>
#include <vector>

namespace kratomix::multiband
{
inline constexpr int maxBands = 6;
inline constexpr int crossoverCount = maxBands - 1;

enum class BandMode
{
    compress = 0,
    expand
};

enum class DetectorSource
{
    internal = 0,
    external
};

enum class AnalyzerMode
{
    input = 0,
    output,
    inputOutput,
    gainReduction
};

enum class LookaheadMode
{
    off = 0,
    fiveMilliseconds
};

inline constexpr const char* inputGainId = "inputGain";
inline constexpr const char* outputGainId = "outputGain";
inline constexpr const char* mixId = "mix";
inline constexpr const char* bypassId = "bypass";
inline constexpr const char* analyzerModeId = "analyzerMode";
inline constexpr const char* lookaheadModeId = "lookaheadMode";

// Retained so released sessions and automation lanes keep their stable parameter IDs.
inline constexpr std::array<const char*, crossoverCount> crossoverFrequencyIds {{
    "crossover01Frequency",
    "crossover02Frequency",
    "crossover03Frequency",
    "crossover04Frequency",
    "crossover05Frequency"
}};

inline constexpr std::array<float, crossoverCount> defaultCrossoverFrequencies {{
    120.0f,
    500.0f,
    2000.0f,
    8000.0f,
    12000.0f
}};

inline constexpr std::array<const char*, maxBands> bandFrequencyIds {{
    "band01Frequency", "band02Frequency", "band03Frequency", "band04Frequency", "band05Frequency", "band06Frequency"
}};

inline constexpr std::array<const char*, maxBands> bandWidthIds {{
    "band01Width", "band02Width", "band03Width", "band04Width", "band05Width", "band06Width"
}};

inline constexpr std::array<float, maxBands> defaultBandFrequencies {{
    80.0f, 250.0f, 1000.0f, 4000.0f, 8000.0f, 14000.0f
}};

inline constexpr std::array<const char*, maxBands> bandEnabledIds {{
    "band01Enabled", "band02Enabled", "band03Enabled", "band04Enabled", "band05Enabled", "band06Enabled"
}};

inline constexpr std::array<const char*, maxBands> bandSoloIds {{
    "band01Solo", "band02Solo", "band03Solo", "band04Solo", "band05Solo", "band06Solo"
}};

inline constexpr std::array<const char*, maxBands> bandAuditionIds {{
    "band01Audition", "band02Audition", "band03Audition", "band04Audition", "band05Audition", "band06Audition"
}};

inline constexpr std::array<const char*, maxBands> bandThresholdIds {{
    "band01Threshold", "band02Threshold", "band03Threshold", "band04Threshold", "band05Threshold", "band06Threshold"
}};

inline constexpr std::array<const char*, maxBands> bandRangeIds {{
    "band01Range", "band02Range", "band03Range", "band04Range", "band05Range", "band06Range"
}};

inline constexpr std::array<const char*, maxBands> bandRatioIds {{
    "band01Ratio", "band02Ratio", "band03Ratio", "band04Ratio", "band05Ratio", "band06Ratio"
}};

inline constexpr std::array<const char*, maxBands> bandAttackIds {{
    "band01Attack", "band02Attack", "band03Attack", "band04Attack", "band05Attack", "band06Attack"
}};

inline constexpr std::array<const char*, maxBands> bandReleaseIds {{
    "band01Release", "band02Release", "band03Release", "band04Release", "band05Release", "band06Release"
}};

inline constexpr std::array<const char*, maxBands> bandKneeIds {{
    "band01Knee", "band02Knee", "band03Knee", "band04Knee", "band05Knee", "band06Knee"
}};

inline constexpr std::array<const char*, maxBands> bandMakeupIds {{
    "band01Makeup", "band02Makeup", "band03Makeup", "band04Makeup", "band05Makeup", "band06Makeup"
}};

inline constexpr std::array<const char*, maxBands> bandModeIds {{
    "band01Mode", "band02Mode", "band03Mode", "band04Mode", "band05Mode", "band06Mode"
}};

inline constexpr std::array<const char*, maxBands> bandDetectorSourceIds {{
    "band01DetectorSource", "band02DetectorSource", "band03DetectorSource", "band04DetectorSource", "band05DetectorSource", "band06DetectorSource"
}};

inline constexpr std::array<const char*, maxBands> bandStereoLinkIds {{
    "band01StereoLink", "band02StereoLink", "band03StereoLink", "band04StereoLink", "band05StereoLink", "band06StereoLink"
}};

inline constexpr std::array<const char*, 6> globalParameterIds {{
    inputGainId,
    outputGainId,
    mixId,
    bypassId,
    analyzerModeId,
    lookaheadModeId
}};

inline juce::String bandLabel(int zeroBasedIndex)
{
    return "Band " + juce::String(zeroBasedIndex + 1);
}

inline juce::StringArray bandModeChoices()
{
    return { "Compress", "Expand" };
}

inline juce::StringArray detectorSourceChoices()
{
    return { "Internal", "External" };
}

inline juce::StringArray analyzerModeChoices()
{
    return { "Input", "Output", "Input + Output", "Gain Reduction" };
}

inline juce::StringArray lookaheadModeChoices()
{
    return { "Off", "5 ms" };
}

inline juce::NormalisableRange<float> frequencyRange()
{
    juce::NormalisableRange<float> range { 20.0f, 20000.0f, 1.0f };
    range.setSkewForCentre(1000.0f);
    return range;
}

inline juce::NormalisableRange<float> ratioRange()
{
    juce::NormalisableRange<float> range { 1.0f, 20.0f, 0.01f };
    range.setSkewForCentre(3.0f);
    return range;
}

inline juce::NormalisableRange<float> widthRange()
{
    juce::NormalisableRange<float> range { 0.25f, 6.0f, 0.01f };
    range.setSkewForCentre(2.0f);
    return range;
}

inline juce::NormalisableRange<float> timeRange(float minimum, float maximum, float centre)
{
    juce::NormalisableRange<float> range { minimum, maximum, 0.1f };
    range.setSkewForCentre(centre);
    return range;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using Parameter = juce::RangedAudioParameter;
    std::vector<std::unique_ptr<Parameter>> params;

    const auto displayDb = [](float value, int) { return juce::String(value, 1); };
    const auto parseFloat = [](const juce::String& text) { return text.getFloatValue(); };
    const auto displayFrequency = [](float value, int) { return juce::String(static_cast<int>(std::round(value))); };
    const auto displayPercent = [](float value, int) { return juce::String(std::round(value * 100.0f), 0); };
    const auto parsePercent = [](const juce::String& text) { return juce::jlimit(0.0f, 1.0f, text.getFloatValue() / 100.0f); };
    const auto displayRatio = [](float value, int) { return juce::String(value, value < 10.0f ? 2 : 1) + ":1"; };

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { inputGainId, 1 },
        "Input",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(displayDb)
            .withValueFromStringFunction(parseFloat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { outputGainId, 1 },
        "Output",
        juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
        0.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("dB")
            .withStringFromValueFunction(displayDb)
            .withValueFromStringFunction(parseFloat)));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { mixId, 1 },
        "Mix",
        juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
        1.0f,
        juce::AudioParameterFloatAttributes()
            .withLabel("%")
            .withStringFromValueFunction(displayPercent)
            .withValueFromStringFunction(parsePercent)));

    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { bypassId, 1 },
        "Bypass",
        false));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { analyzerModeId, 1 },
        "Analyzer",
        analyzerModeChoices(),
        static_cast<int>(AnalyzerMode::inputOutput)));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { lookaheadModeId, 1 },
        "Lookahead",
        lookaheadModeChoices(),
        static_cast<int>(LookaheadMode::off)));

    for (int index = 0; index < crossoverCount; ++index)
    {
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { crossoverFrequencyIds[static_cast<size_t>(index)], 1 },
            "Crossover " + juce::String(index + 1),
            frequencyRange(),
            defaultCrossoverFrequencies[static_cast<size_t>(index)],
            juce::AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction(displayFrequency)
                .withValueFromStringFunction(parseFloat)));
    }

    for (int index = 0; index < maxBands; ++index)
    {
        const auto namePrefix = bandLabel(index) + " ";

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandFrequencyIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Frequency",
            frequencyRange(),
            defaultBandFrequencies[static_cast<size_t>(index)],
            juce::AudioParameterFloatAttributes()
                .withLabel("Hz")
                .withStringFromValueFunction(displayFrequency)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandWidthIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Width",
            widthRange(),
            2.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("oct")
                .withStringFromValueFunction([](float value, int) { return juce::String(value, 2); })
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bandEnabledIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Enabled",
            false));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bandSoloIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Solo",
            false));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID { bandAuditionIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Audition",
            false));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandThresholdIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Threshold",
            juce::NormalisableRange<float> { -90.0f, 0.0f, 0.1f },
            -24.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction(displayDb)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandRangeIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Range",
            juce::NormalisableRange<float> { -24.0f, 24.0f, 0.1f },
            -6.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction(displayDb)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandRatioIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Ratio",
            ratioRange(),
            2.0f,
            juce::AudioParameterFloatAttributes()
                .withStringFromValueFunction(displayRatio)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandAttackIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Attack",
            timeRange(0.1f, 200.0f, 20.0f),
            20.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("ms")
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandReleaseIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Release",
            timeRange(5.0f, 1000.0f, 120.0f),
            120.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("ms")
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandKneeIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Knee",
            juce::NormalisableRange<float> { 0.0f, 24.0f, 0.1f },
            6.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction(displayDb)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandMakeupIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Output",
            juce::NormalisableRange<float> { -18.0f, 18.0f, 0.1f },
            0.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("dB")
                .withStringFromValueFunction(displayDb)
                .withValueFromStringFunction(parseFloat)));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { bandModeIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Mode",
            bandModeChoices(),
            static_cast<int>(BandMode::compress)));

        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID { bandDetectorSourceIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Detector",
            detectorSourceChoices(),
            static_cast<int>(DetectorSource::internal)));

        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID { bandStereoLinkIds[static_cast<size_t>(index)], 1 },
            namePrefix + "Stereo Link",
            juce::NormalisableRange<float> { 0.0f, 1.0f, 0.001f },
            1.0f,
            juce::AudioParameterFloatAttributes()
                .withLabel("%")
                .withStringFromValueFunction(displayPercent)
                .withValueFromStringFunction(parsePercent)));
    }

    return { params.begin(), params.end() };
}
}
