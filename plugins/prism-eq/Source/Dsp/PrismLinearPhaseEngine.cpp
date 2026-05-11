#include "PrismLinearPhaseEngine.h"

namespace kratomix
{
namespace
{
uint64_t mixDigest(uint64_t seed, uint64_t value) noexcept
{
    seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
    return seed;
}

uint64_t floatDigest(float value) noexcept
{
    return static_cast<uint64_t>(std::llround(static_cast<double>(value) * 1000.0));
}
}

void PrismLinearPhaseEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);
    history.setSize(numChannels, impulseSize);
    history.clear();
    impulse.assign(static_cast<size_t>(impulseSize), 0.0f);
    prepared = true;
    activeDigest = 0;
    pendingDigest = 0;
}

void PrismLinearPhaseEngine::reset()
{
    history.clear();
    historyWriteIndex = 0;
}

void PrismLinearPhaseEngine::updateSettings(const PrismSettings& settings)
{
    pendingSettings = settings;
    pendingDigest = makeSettingsDigest(settings);
}

void PrismLinearPhaseEngine::process(juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    rebuildImpulseIfNeeded();

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            auto* historySamples = history.getWritePointer(channel);
            historySamples[historyWriteIndex] = samples[sample];

            auto output = 0.0f;
            auto readIndex = historyWriteIndex;
            for (int tap = 0; tap < impulseSize; ++tap)
            {
                output += impulse[static_cast<size_t>(tap)] * historySamples[readIndex];
                if (--readIndex < 0)
                    readIndex = impulseSize - 1;
            }

            samples[sample] = output;
        }

        if (++historyWriteIndex >= impulseSize)
            historyWriteIndex = 0;
    }
}

void PrismLinearPhaseEngine::rebuildImpulseIfNeeded()
{
    if (activeDigest == pendingDigest)
        return;

    currentSettings = pendingSettings;
    buildImpulseResponse();
    activeDigest = pendingDigest;
}

void PrismLinearPhaseEngine::buildImpulseResponse()
{
    impulse.assign(static_cast<size_t>(impulseSize), 0.0f);

    std::array<float, prism::maxBands> noDynamicGain {};
    const auto response = makePrismResponseSnapshot(currentSettings, sampleRate, noDynamicGain, false);
    std::vector<float> magnitudes(static_cast<size_t>(impulseSize / 2 + 1), 1.0f);

    for (int bin = 0; bin <= impulseSize / 2; ++bin)
    {
        const auto frequency = static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(impulseSize);
        const auto clampedFrequency = juce::jlimit(20.0f, 20000.0f, frequency);
        magnitudes[static_cast<size_t>(bin)] = juce::Decibels::decibelsToGain(prismResponseGainDbAt(response, clampedFrequency));
    }

    for (int sample = 0; sample < impulseSize; ++sample)
    {
        const auto centredSample = static_cast<double>(sample - latencySamples);
        auto value = static_cast<double>(magnitudes[0]);
        value += static_cast<double>(magnitudes[static_cast<size_t>(impulseSize / 2)])
                 * std::cos(juce::MathConstants<double>::pi * centredSample);

        for (int bin = 1; bin < impulseSize / 2; ++bin)
        {
            const auto angle = juce::MathConstants<double>::twoPi
                               * static_cast<double>(bin)
                               * centredSample
                               / static_cast<double>(impulseSize);
            value += 2.0 * static_cast<double>(magnitudes[static_cast<size_t>(bin)]) * std::cos(angle);
        }

        const auto window = 0.5 - 0.5 * std::cos(juce::MathConstants<double>::twoPi
                                                 * static_cast<double>(sample)
                                                 / static_cast<double>(impulseSize - 1));
        impulse[static_cast<size_t>(sample)] = static_cast<float>((value / static_cast<double>(impulseSize)) * window);
    }
}

uint64_t PrismLinearPhaseEngine::makeSettingsDigest(const PrismSettings& settings) noexcept
{
    auto digest = 1469598103934665603ULL;

    for (const auto& band : settings.bands)
    {
        digest = mixDigest(digest, band.enabled ? 1U : 0U);
        digest = mixDigest(digest, static_cast<uint64_t>(band.type));
        digest = mixDigest(digest, floatDigest(band.frequency));
        digest = mixDigest(digest, floatDigest(band.gainDb));
        digest = mixDigest(digest, floatDigest(band.q));
    }

    return digest;
}
}
