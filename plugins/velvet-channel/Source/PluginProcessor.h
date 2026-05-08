#pragma once

#include <JuceHeader.h>

#include "Dsp/WarmthProcessor.h"
#include "Parameters.h"

namespace kratomix
{
class VelvetChannelAudioProcessor final : public juce::AudioProcessor
{
public:
    VelvetChannelAudioProcessor();
    ~VelvetChannelAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kratomix Velvet Channel"; }
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

    float getVuLevel() const noexcept;

    juce::AudioProcessorValueTreeState parameters;

private:
    WarmthSettings readSettings() const;

    WarmthProcessor warmthProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelvetChannelAudioProcessor)
};
}
