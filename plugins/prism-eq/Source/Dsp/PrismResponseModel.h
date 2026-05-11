#pragma once

#include <array>

#include <JuceHeader.h>

#include "Dsp/PrismFilterDesign.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
struct PrismResponseSnapshot
{
    std::array<PrismIIRCoefficients::Ptr, prism::maxBands> coefficients {};
    std::array<bool, prism::maxBands> enabled {};
    double sampleRate = 44100.0;
};

PrismResponseSnapshot makePrismResponseSnapshot(const PrismSettings& settings,
                                                double sampleRate,
                                                const std::array<float, prism::maxBands>& dynamicGainDb,
                                                bool includeDynamicGain);

float prismResponseGainDbAt(const PrismResponseSnapshot& snapshot, float frequency);
}
