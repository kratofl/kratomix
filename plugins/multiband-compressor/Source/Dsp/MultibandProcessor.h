#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>

#include "Dsp/MultibandTypes.h"

namespace kratomix
{
class MultibandSplitter
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void setCrossovers(const std::array<float, multiband::crossoverCount>& frequencies);
    void split(const juce::AudioBuffer<float>& source,
               std::array<juce::AudioBuffer<float>, multiband::maxBands>& destination,
               int numChannels,
               int numSamples) noexcept;

private:
    using Filter = juce::dsp::LinkwitzRileyFilter<float>;

    std::array<Filter, multiband::crossoverCount> lowpassFilters;
    std::array<Filter, multiband::crossoverCount> highpassFilters;
    std::array<std::array<Filter, multiband::crossoverCount>, multiband::maxBands> allpassFilters;
};

class MultibandProcessor
{
public:
    MultibandProcessor();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const MultibandSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer = nullptr);
    float getOutputLevel() const noexcept;
    int getCurrentLatencySamples() const noexcept;
    void copyAnalyzerFrame(MultibandAnalyzerFrame& destination) const noexcept;

private:
    static constexpr int maxProcessingChannels = 2;
    static constexpr int maxLookaheadSamples = 8192;
    using AnalyzerStorage = std::array<std::atomic<float>, MultibandAnalyzerFrame::sampleCount>;

    void allocateBuffers(int numChannels, int numSamples);
    void applyTargetsImmediately();
    void updateCrossovers();
    void processLookahead(juce::AudioBuffer<float>& audioBuffer, int numChannels, int numSamples);
    float detectorMagnitudeForBand(int bandIndex,
                                   int sample,
                                   const std::array<juce::AudioBuffer<float>, multiband::maxBands>& internalBands,
                                   const std::array<juce::AudioBuffer<float>, multiband::maxBands>& externalBands,
                                   bool sidechainAvailable) const noexcept;
    float computeDynamicGainDb(const MultibandBandSettings& band, float detectorLevelDb) const noexcept;
    void publishAnalyzerSamples(const juce::AudioBuffer<float>& inputBuffer,
                                const juce::AudioBuffer<float>& outputBuffer,
                                const juce::AudioBuffer<float>* sidechainBuffer,
                                int numSamples) noexcept;
    static float monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept;

    juce::SmoothedValue<float> inputGainDbSmoother;
    juce::SmoothedValue<float> outputGainDbSmoother;
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> wetSmoother;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> lookaheadDelay { maxLookaheadSamples };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { maxLookaheadSamples };

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> detectorBuffer;
    std::array<juce::AudioBuffer<float>, multiband::maxBands> audioBands;
    std::array<juce::AudioBuffer<float>, multiband::maxBands> detectorBands;
    std::array<juce::AudioBuffer<float>, multiband::maxBands> sidechainBands;
    MultibandSplitter audioSplitter;
    MultibandSplitter detectorSplitter;
    MultibandSplitter sidechainSplitter;

    AnalyzerStorage inputAnalyzerSamples;
    AnalyzerStorage outputAnalyzerSamples;
    AnalyzerStorage sidechainAnalyzerSamples;
    std::array<std::atomic<float>, multiband::maxBands> dynamicGainTelemetry;
    std::array<std::atomic<float>, multiband::maxBands> detectorTelemetry;
    std::array<std::atomic<float>, multiband::maxBands> bandLevelTelemetry;
    std::atomic<int> analyzerWriteIndex { 0 };
    std::atomic<bool> analyzerSidechainActive { false };
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<int> currentLatencySamples { 0 };

    std::array<float, multiband::crossoverCount> activeCrossovers = multiband::defaultCrossoverFrequencies;
    std::array<float, multiband::maxBands> detectorEnvelope {};
    std::array<float, multiband::maxBands> gainDbState {};
    std::array<float, multiband::maxBands> bandEnergyState {};
    std::array<float, multiband::maxBands> sampleGainDb {};
    MultibandSettings settings;
    double sampleRate = 44100.0;
    int preparedChannels = 0;
    int preparedBlockSize = 0;
    bool hasProcessedAudio = false;
};
}
