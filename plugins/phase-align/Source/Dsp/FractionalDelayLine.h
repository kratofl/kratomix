#pragma once

#include <JuceHeader.h>

#include <vector>

namespace kratomix::phase_align
{
class FractionalDelayLine
{
public:
    void prepare(int numChannels, int maximumDelaySamples, double sampleRate);
    void reset() noexcept;
    void setDelaySamples(double delaySamples, bool immediately = false) noexcept;
    void process(juce::AudioBuffer<float>& buffer,
                 float polarity,
                 int numSamplesToProcess = -1) noexcept;

private:
    float interpolate(int channel, double delaySamples) const noexcept;
    int wrapIndex(int index) const noexcept;

    std::vector<std::vector<float>> storage;
    juce::LinearSmoothedValue<double> delay;
    int writePosition = 0;
    int maximumDelay = 2;
};
}
