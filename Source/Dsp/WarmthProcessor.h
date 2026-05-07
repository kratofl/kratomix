#pragma once

#include <JuceHeader.h>

namespace kratomix
{
struct WarmthSettings
{
    float inputGainDb = 0.0f;
    float drive = 2.5f;
    float highPassHz = 35.0f;
    float warmthDb = 1.5f;
    float presenceDb = 0.5f;
    float airDb = 1.0f;
    float outputGainDb = 0.0f;
};

class WarmthProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const WarmthSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer);

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;

    static float saturateSample(float sample, float driveAmount) noexcept;

    StereoFilter highPass;
    StereoFilter lowShelf;
    StereoFilter presence;
    StereoFilter highShelf;

    double sampleRate = 44100.0;
    WarmthSettings settings;
};
}
