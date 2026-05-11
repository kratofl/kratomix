#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismTypes.h"

namespace kratomix
{
using PrismIIRCoefficients = juce::dsp::IIR::Coefficients<float>;

namespace prism
{
float safeFilterFrequency(float frequency, double sampleRate) noexcept;
float safeFilterQ(float q) noexcept;
float safeShelfQ(float q) noexcept;
}

PrismIIRCoefficients::Ptr makePrismCoefficients(double sampleRate,
                                                const PrismBandSettings& band,
                                                float dynamicGainDb);
}
