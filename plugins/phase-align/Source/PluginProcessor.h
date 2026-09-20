#pragma once

#include <JuceHeader.h>

#include "Dsp/PhaseAlignProcessor.h"
#include "Parameters.h"

namespace kratomix
{
class PhaseAlignAudioProcessor final : public juce::AudioProcessor,
                                       private juce::AsyncUpdater
{
public:
    PhaseAlignAudioProcessor();
    ~PhaseAlignAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Kratomix Phase Align"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    bool requestAutoAlign();
    void nudgeOffsetBySamples(float samples);
    float getOffsetSamples() const noexcept;
    float getCorrelation() const noexcept;
    float getConfidence() const noexcept;
    phase_align::AnalysisStatus getAnalysisStatus() const noexcept;
    void copyAnalysisFrame(phase_align::AnalysisFrame& destination) const noexcept;

    juce::AudioProcessorValueTreeState parameters;

private:
    phase_align::Settings readSettings() const noexcept;
    void handleAsyncUpdate() override;
    void setParameterPlainValue(const char* id, float value);

    phase_align::PhaseAlignProcessor phaseProcessor;
    double processingSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseAlignAudioProcessor)
};
}
