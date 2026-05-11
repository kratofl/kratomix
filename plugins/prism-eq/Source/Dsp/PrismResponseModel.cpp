#include "PrismResponseModel.h"

namespace kratomix
{
PrismResponseSnapshot makePrismResponseSnapshot(const PrismSettings& settings,
                                                double sampleRate,
                                                const std::array<float, prism::maxBands>& dynamicGainDb,
                                                bool includeDynamicGain)
{
    PrismResponseSnapshot snapshot;
    snapshot.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (size_t index = 0; index < settings.bands.size(); ++index)
    {
        const auto& band = settings.bands[index];
        snapshot.enabled[index] = band.enabled;

        if (! band.enabled)
            continue;

        const auto movement = includeDynamicGain && band.dynamicEnabled ? dynamicGainDb[index] : 0.0f;
        snapshot.coefficients[index] = makePrismCoefficients(snapshot.sampleRate, band, movement);
    }

    return snapshot;
}

float prismResponseGainDbAt(const PrismResponseSnapshot& snapshot, float frequency)
{
    auto magnitude = 1.0;
    const auto clampedFrequency = juce::jlimit(20.0f, 20000.0f, frequency);

    for (size_t index = 0; index < snapshot.coefficients.size(); ++index)
    {
        if (! snapshot.enabled[index] || snapshot.coefficients[index] == nullptr)
            continue;

        magnitude *= snapshot.coefficients[index]->getMagnitudeForFrequency(clampedFrequency, snapshot.sampleRate);
    }

    return juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -120.0f);
}
}
