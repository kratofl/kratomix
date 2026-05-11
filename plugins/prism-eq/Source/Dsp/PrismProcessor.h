#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismLatency.h"
#include "Dsp/PrismLinearPhaseEngine.h"
#include "Dsp/PrismMinimumPhaseEngine.h"
#include "Dsp/PrismTypes.h"

#include <array>

namespace kratomix
{
class PrismProcessor
{
public:
    PrismProcessor();

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer = nullptr);
    float getOutputLevel() const noexcept;
    int getCurrentLatencySamples() const noexcept;
    void copyAnalyzerFrame(PrismAnalyzerFrame& destination) const noexcept;

private:
    using AnalyzerStorage = std::array<std::atomic<float>, PrismAnalyzerFrame::sampleCount>;

    void applyTargetsImmediately();
    void publishAnalyzerSamples(const juce::AudioBuffer<float>& preBuffer,
                                const juce::AudioBuffer<float>& postBuffer,
                                const juce::AudioBuffer<float>* sidechainBuffer,
                                int numSamples) noexcept;
    static float monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept;

    juce::SmoothedValue<float> inputGainDbSmoother;
    juce::SmoothedValue<float> outputGainDbSmoother;
    juce::SmoothedValue<float> mixSmoother;
    juce::SmoothedValue<float> wetSmoother;
    juce::AudioBuffer<float> dryBuffer;
    PrismMinimumPhaseEngine minimumPhaseEngine;
    PrismLinearPhaseEngine linearPhaseEngine;

    AnalyzerStorage preAnalyzerSamples;
    AnalyzerStorage postAnalyzerSamples;
    AnalyzerStorage sidechainAnalyzerSamples;
    std::atomic<int> analyzerWriteIndex { 0 };
    std::atomic<bool> analyzerSidechainActive { false };

    std::atomic<float> outputLevel { 0.0f };
    std::atomic<int> currentLatencySamples { 0 };
    PrismSettings settings;
    double sampleRate = 44100.0;
    bool hasProcessedAudio = false;
};
}
