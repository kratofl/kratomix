#pragma once

#include <JuceHeader.h>

#include "Parameters.h"

#include <array>

namespace kratomix
{
struct PrismAnalyzerFrame
{
    static constexpr int sampleCount = 2048;

    std::array<float, sampleCount> pre {};
    std::array<float, sampleCount> post {};
    std::array<float, sampleCount> sidechain {};
    std::array<float, prism::maxBands> dynamicGainDb {};
    double sampleRate = 44100.0;
    bool sidechainActive = false;
};

struct PrismBandSettings
{
    bool enabled = false;
    prism::BandType type = prism::BandType::bell;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    bool dynamicEnabled = false;
    float dynamicRangeDb = 0.0f;
    float thresholdDb = -24.0f;
    float attackMs = 20.0f;
    float releaseMs = 120.0f;
    prism::SidechainSource sidechainSource = prism::SidechainSource::main;
    bool solo = false;
};

struct PrismSettings
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;
    bool bypassed = false;
    prism::PhaseMode phaseMode = prism::PhaseMode::zeroLatency;
    std::array<PrismBandSettings, prism::maxBands> bands {};
};

class PrismProcessor
{
public:
    PrismProcessor();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer = nullptr);
    float getOutputLevel() const noexcept;
    void copyAnalyzerFrame(PrismAnalyzerFrame& destination) const noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;
    using AnalyzerStorage = std::array<std::atomic<float>, PrismAnalyzerFrame::sampleCount>;

    void applyTargetsImmediately();
    void updateDynamicGain(const juce::AudioBuffer<float>& mainBuffer,
                           const juce::AudioBuffer<float>* sidechainBuffer,
                           int numSamples);
    void updateFilterCoefficients();
    void publishAnalyzerSamples(const juce::AudioBuffer<float>& preBuffer,
                                const juce::AudioBuffer<float>& postBuffer,
                                const juce::AudioBuffer<float>* sidechainBuffer,
                                int numSamples) noexcept;
    static float monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept;
    static float blockRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;
    static float bandLimitedRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples, Filter& filter) noexcept;
    static Coefficients::Ptr makeCoefficients(double sampleRate, const PrismBandSettings& band, float dynamicGainDb);

    juce::SmoothedValue<float> inputGainDbSmoother;
    juce::SmoothedValue<float> outputGainDbSmoother;
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> wetSmoother;
    juce::AudioBuffer<float> dryBuffer;
    std::array<StereoFilter, prism::maxBands> filters;
    std::array<StereoFilter, prism::maxBands> phaseFilters;
    std::array<Filter, prism::maxBands> detectorFilters;
    std::array<float, prism::maxBands> dynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicGainDb {};

    AnalyzerStorage preAnalyzerSamples;
    AnalyzerStorage postAnalyzerSamples;
    AnalyzerStorage sidechainAnalyzerSamples;
    std::atomic<int> analyzerWriteIndex { 0 };
    std::atomic<bool> analyzerSidechainActive { false };

    std::atomic<float> outputLevel { 0.0f };
    PrismSettings settings;
    double sampleRate = 44100.0;
    bool hasProcessedAudio = false;
};
}
