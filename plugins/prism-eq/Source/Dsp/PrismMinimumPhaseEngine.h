#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismFilterDesign.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
class PrismMinimumPhaseEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer);
    void copyDynamicTelemetryTo(PrismAnalyzerFrame& destination) const noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;

    void processNativeBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer);
    void processFilterBlock(juce::dsp::AudioBlock<float>& block,
                            std::array<StereoFilter, prism::maxBands>& targetFilters,
                            std::array<StereoFilter, prism::maxBands>& targetPhaseFilters,
                            double processingSampleRate);
    void updateDynamicGain(const juce::AudioBuffer<float>& mainBuffer,
                           const juce::AudioBuffer<float>* sidechainBuffer,
                           int numSamples);
    void updateFilterCoefficients(double processingSampleRate,
                                  std::array<StereoFilter, prism::maxBands>& targetFilters,
                                  std::array<StereoFilter, prism::maxBands>& targetPhaseFilters);
    void updateDetectorCoefficients();
    static float monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept;
    static float bandLimitedRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples, Filter& filter) noexcept;

    std::array<StereoFilter, prism::maxBands> filters;
    std::array<StereoFilter, prism::maxBands> phaseFilters;
    std::array<StereoFilter, prism::maxBands> filters2x;
    std::array<StereoFilter, prism::maxBands> phaseFilters2x;
    std::array<StereoFilter, prism::maxBands> filters4x;
    std::array<StereoFilter, prism::maxBands> phaseFilters4x;
    std::array<Filter, prism::maxBands> detectorFilters;
    std::array<float, prism::maxBands> dynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicTargetGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorLevelDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorOverThresholdDb {};
    std::array<std::atomic<bool>, prism::maxBands> analyzerDetectorUsingExternalSidechain {};

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;

    PrismSettings settings;
    double sampleRate = 44100.0;
};
}
