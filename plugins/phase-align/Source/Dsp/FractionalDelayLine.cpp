#include "FractionalDelayLine.h"

#include <algorithm>
#include <cmath>

namespace kratomix::phase_align
{
void FractionalDelayLine::prepare(int numChannels, int maximumDelaySamples, double sampleRate)
{
    maximumDelay = std::max(2, maximumDelaySamples);
    const auto storageSize = maximumDelay + 8;
    storage.assign(static_cast<size_t>(std::max(1, numChannels)),
                   std::vector<float>(static_cast<size_t>(storageSize), 0.0f));
    writePosition = 0;
    delay.reset(sampleRate, 0.02);
    delay.setCurrentAndTargetValue(2.0);
}

void FractionalDelayLine::reset() noexcept
{
    for (auto& channel : storage)
        std::fill(channel.begin(), channel.end(), 0.0f);

    writePosition = 0;
    delay.setCurrentAndTargetValue(delay.getTargetValue());
}

void FractionalDelayLine::setDelaySamples(double delaySamples, bool immediately) noexcept
{
    const auto clampedDelay = std::clamp(delaySamples, 2.0, static_cast<double>(maximumDelay));

    if (immediately)
        delay.setCurrentAndTargetValue(clampedDelay);
    else
        delay.setTargetValue(clampedDelay);
}

void FractionalDelayLine::process(juce::AudioBuffer<float>& buffer,
                                  float polarity,
                                  int numSamplesToProcess) noexcept
{
    const auto numChannels = std::min(buffer.getNumChannels(), static_cast<int>(storage.size()));
    const auto numSamples = numSamplesToProcess < 0
                                ? buffer.getNumSamples()
                                : std::min(buffer.getNumSamples(), numSamplesToProcess);

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto currentDelay = delay.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto& channelStorage = storage[static_cast<size_t>(channel)];
            channelStorage[static_cast<size_t>(writePosition)] = buffer.getSample(channel, sample);
            buffer.setSample(channel, sample, interpolate(channel, currentDelay) * polarity);
        }

        writePosition = wrapIndex(writePosition + 1);
    }
}

float FractionalDelayLine::interpolate(int channel, double delaySamples) const noexcept
{
    auto readPosition = static_cast<double>(writePosition) - delaySamples;
    const auto storageSize = static_cast<double>(storage.front().size());

    while (readPosition < 0.0)
        readPosition += storageSize;

    const auto centreIndex = static_cast<int>(std::floor(readPosition));
    const auto fraction = static_cast<float>(readPosition - std::floor(readPosition));
    const auto& channelStorage = storage[static_cast<size_t>(channel)];

    const auto before = channelStorage[static_cast<size_t>(wrapIndex(centreIndex - 1))];
    const auto centre = channelStorage[static_cast<size_t>(wrapIndex(centreIndex))];
    const auto after = channelStorage[static_cast<size_t>(wrapIndex(centreIndex + 1))];
    const auto afterNext = channelStorage[static_cast<size_t>(wrapIndex(centreIndex + 2))];

    const auto coefficientBefore = -fraction * (fraction - 1.0f) * (fraction - 2.0f) / 6.0f;
    const auto coefficientCentre = (fraction + 1.0f) * (fraction - 1.0f) * (fraction - 2.0f) / 2.0f;
    const auto coefficientAfter = -(fraction + 1.0f) * fraction * (fraction - 2.0f) / 2.0f;
    const auto coefficientAfterNext = (fraction + 1.0f) * fraction * (fraction - 1.0f) / 6.0f;

    return before * coefficientBefore
        + centre * coefficientCentre
        + after * coefficientAfter
        + afterNext * coefficientAfterNext;
}

int FractionalDelayLine::wrapIndex(int index) const noexcept
{
    const auto storageSize = static_cast<int>(storage.front().size());

    while (index < 0)
        index += storageSize;

    return index % storageSize;
}
}
