#pragma once

#include <array>
#include <cmath>
#include <optional>

#include <JuceHeader.h>

namespace kratomix::prism
{
struct AnalyzerPeak
{
    size_t index = 0;
    float levelDb = -120.0f;
};

template <size_t Size>
std::array<float, Size> computeMaskingBins(const std::array<float, Size>& postSpectrum,
                                           const std::array<float, Size>& sidechainSpectrum,
                                           float floorDb,
                                           float competitionWindowDb)
{
    std::array<float, Size> result {};

    for (size_t index = 0; index < Size; ++index)
    {
        const auto postDb = postSpectrum[index];
        const auto sideDb = sidechainSpectrum[index];

        if (postDb < floorDb || sideDb < floorDb)
        {
            result[index] = 0.0f;
            continue;
        }

        const auto distanceDb = std::abs(postDb - sideDb);
        const auto proximity = 1.0f - juce::jlimit(0.0f, 1.0f, distanceDb / juce::jmax(0.1f, competitionWindowDb * 2.0f));
        const auto sharedLevelDb = juce::jmin(postDb, sideDb);
        const auto strength = juce::jlimit(0.0f, 1.0f, (sharedLevelDb - floorDb) / 24.0f);
        result[index] = juce::jlimit(0.0f, 1.0f, proximity * strength);
    }

    return result;
}

template <size_t Size>
std::optional<AnalyzerPeak> findNearestAnalyzerPeak(const std::array<float, Size>& spectrum,
                                                    size_t preferredIndex,
                                                    float floorDb)
{
    if constexpr (Size < 3)
    {
        juce::ignoreUnused(spectrum, preferredIndex, floorDb);
        return std::nullopt;
    }
    else
    {
        preferredIndex = juce::jlimit<size_t>(1, Size - 2, preferredIndex);

        std::optional<AnalyzerPeak> best;
        auto bestDistance = Size;

        for (size_t radius = 0; radius < Size; ++radius)
        {
            const auto first = preferredIndex >= radius ? preferredIndex - radius : 1;
            const auto last = juce::jmin(Size - 2, preferredIndex + radius);

            for (auto index = first; index <= last; ++index)
            {
                const auto level = spectrum[index];
                if (level < floorDb || level < spectrum[index - 1] || level < spectrum[index + 1])
                    continue;

                const auto distance = index > preferredIndex ? index - preferredIndex : preferredIndex - index;
                if (! best.has_value() || distance < bestDistance || (distance == bestDistance && level > best->levelDb))
                {
                    best = AnalyzerPeak { index, level };
                    bestDistance = distance;
                }
            }

            if (best.has_value())
                break;
        }

        return best;
    }
}
}
