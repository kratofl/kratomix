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
    bool bypassed = false;
};

class WarmthProcessor
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const WarmthSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer);
    float getVuLevel() const noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;

    static float saturateSample(float sample, float driveAmount) noexcept;
    void applyTargetsImmediately();
    void updateFilterCoefficients(float highPassHz, float warmthDb, float presenceDb, float airDb);

    StereoFilter highPass;
    StereoFilter lowShelf;
    StereoFilter presence;
    StereoFilter highShelf;

    juce::SmoothedValue<float> inputGainDbSmoother;
    juce::SmoothedValue<float> driveSmoother;
    juce::SmoothedValue<float> highPassSmoother;
    juce::SmoothedValue<float> warmthSmoother;
    juce::SmoothedValue<float> presenceSmoother;
    juce::SmoothedValue<float> airSmoother;
    juce::SmoothedValue<float> outputGainDbSmoother;
    juce::SmoothedValue<float> wetMixSmoother;

    juce::AudioBuffer<float> dryBuffer;

    double sampleRate = 44100.0;
    float meterEnvelope = 0.0f;
    float meterReleasePerSample = 0.0f;
    std::atomic<float> vuLevel { 0.0f };
    bool hasProcessedAudio = false;
    WarmthSettings settings;
};
}
