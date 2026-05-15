#pragma once

#include <array>
#include <cstddef>

#include "Parameters.h"

namespace kratomix::prism
{
inline constexpr int autoEqSpectrumBinCount = 240;
inline constexpr size_t maxAutoEqSuggestions = 6;

struct AutoEqSuggestion
{
    BandType type = BandType::bell;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
};

struct AutoEqResult
{
    std::array<AutoEqSuggestion, maxAutoEqSuggestions> suggestions {};
    size_t count = 0;
    bool hasSignal = false;
};

AutoEqResult makeInputAutoEqCurve(const std::array<float, autoEqSpectrumBinCount>& spectrumDb);
float autoEqFrequencyForBin(int binIndex) noexcept;
}
