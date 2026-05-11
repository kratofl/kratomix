#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismResponseModel.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
class PrismLinearPhaseEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& settings);
    void process(juce::AudioBuffer<float>& buffer);
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static constexpr int impulseSize = 4096;

    void rebuildImpulseIfNeeded();
    void buildImpulseResponse();
    static uint64_t makeSettingsDigest(const PrismSettings& settings) noexcept;

    PrismSettings currentSettings;
    PrismSettings pendingSettings;
    juce::AudioBuffer<float> history;
    std::vector<float> impulse;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int historyWriteIndex = 0;
    int latencySamples = impulseSize / 2;
    uint64_t activeDigest = 0;
    uint64_t pendingDigest = 1;
    bool prepared = false;
};
}
