#include "PrismFilterDesign.h"

namespace kratomix
{
namespace
{
float decibelsToGain(float decibels) noexcept
{
    return juce::Decibels::decibelsToGain(decibels);
}
}

float prism::safeFilterFrequency(float frequency, double sampleRate) noexcept
{
    const auto nyquist = static_cast<float>(sampleRate * 0.5);
    const auto upper = juce::jmin(20000.0f, nyquist * 0.475f);
    return juce::jlimit(20.0f, juce::jmax(20.0f, upper), frequency);
}

float prism::safeFilterQ(float q) noexcept
{
    return juce::jlimit(0.1f, 32.0f, q);
}

float prism::safeShelfQ(float q) noexcept
{
    return juce::jlimit(0.35f, 0.95f, q);
}

PrismIIRCoefficients::Ptr makePrismCoefficients(double sampleRate,
                                                const PrismBandSettings& band,
                                                float dynamicGainDb)
{
    const auto frequency = prism::safeFilterFrequency(band.frequency, sampleRate);
    const auto q = prism::safeFilterQ(band.q);
    const auto shelfQ = prism::safeShelfQ(band.q);
    const auto gain = decibelsToGain(band.gainDb + dynamicGainDb);

    switch (band.type)
    {
        case prism::BandType::lowShelf:
            return PrismIIRCoefficients::makeLowShelf(sampleRate, frequency, shelfQ, gain);
        case prism::BandType::highShelf:
            return PrismIIRCoefficients::makeHighShelf(sampleRate, frequency, shelfQ, gain);
        case prism::BandType::highPass:
            return PrismIIRCoefficients::makeHighPass(sampleRate, frequency, q);
        case prism::BandType::lowPass:
            return PrismIIRCoefficients::makeLowPass(sampleRate, frequency, q);
        case prism::BandType::notch:
            return PrismIIRCoefficients::makeNotch(sampleRate, frequency, q);
        case prism::BandType::bell:
        default:
            return PrismIIRCoefficients::makePeakFilter(sampleRate, frequency, q, gain);
    }
}
}
