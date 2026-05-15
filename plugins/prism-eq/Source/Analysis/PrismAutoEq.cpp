#include "PrismAutoEq.h"

#include <algorithm>
#include <cmath>

#include <JuceHeader.h>

namespace kratomix::prism
{
namespace
{
constexpr float minFrequency = 20.0f;
constexpr float maxFrequency = 20000.0f;

struct Candidate
{
    int bin = 0;
    float prominenceDb = 0.0f;
};

float averageRange(const std::array<float, autoEqSpectrumBinCount>& spectrumDb, int first, int last) noexcept
{
    first = juce::jlimit(0, autoEqSpectrumBinCount - 1, first);
    last = juce::jlimit(0, autoEqSpectrumBinCount - 1, last);

    if (last < first)
        std::swap(first, last);

    auto total = 0.0f;
    auto count = 0;

    for (int index = first; index <= last; ++index)
    {
        total += spectrumDb[static_cast<size_t>(index)];
        ++count;
    }

    return count > 0 ? total / static_cast<float>(count) : -120.0f;
}

float baselineAround(const std::array<float, autoEqSpectrumBinCount>& spectrumDb, int bin) noexcept
{
    auto total = 0.0f;
    auto count = 0;

    for (int index = bin - 14; index <= bin + 14; ++index)
    {
        if (index < 0 || index >= autoEqSpectrumBinCount)
            continue;

        if (std::abs(index - bin) <= 2)
            continue;

        total += spectrumDb[static_cast<size_t>(index)];
        ++count;
    }

    return count > 0 ? total / static_cast<float>(count) : spectrumDb[static_cast<size_t>(bin)];
}

bool isNearExisting(const std::array<AutoEqSuggestion, maxAutoEqSuggestions>& suggestions,
                    size_t count,
                    float frequency) noexcept
{
    for (size_t index = 0; index < count; ++index)
    {
        const auto existingFrequency = suggestions[index].frequency;
        const auto ratio = frequency > existingFrequency ? frequency / existingFrequency
                                                         : existingFrequency / frequency;
        if (ratio < 1.28f)
            return true;
    }

    return false;
}
}

float autoEqFrequencyForBin(int binIndex) noexcept
{
    const auto proportion = static_cast<float>(juce::jlimit(0, autoEqSpectrumBinCount - 1, binIndex))
                            / static_cast<float>(autoEqSpectrumBinCount - 1);
    return std::pow(10.0f, juce::jmap(proportion, std::log10(minFrequency), std::log10(maxFrequency)));
}

AutoEqResult makeInputAutoEqCurve(const std::array<float, autoEqSpectrumBinCount>& spectrumDb)
{
    AutoEqResult result;

    const auto loudest = *std::max_element(spectrumDb.begin(), spectrumDb.end());
    result.hasSignal = loudest > -72.0f;
    if (! result.hasSignal)
        return result;

    const auto lowAverage = averageRange(spectrumDb, 0, 27);
    const auto midAverage = averageRange(spectrumDb, 66, 150);
    if (lowAverage > -48.0f && lowAverage > midAverage + 12.0f)
    {
        result.suggestions[result.count++] = AutoEqSuggestion {
            BandType::highPass,
            55.0f,
            0.0f,
            0.707f
        };
    }

    std::array<Candidate, 16> candidates {};
    size_t candidateCount = 0;

    for (int bin = 8; bin < autoEqSpectrumBinCount - 8; ++bin)
    {
        const auto current = spectrumDb[static_cast<size_t>(bin)];
        if (current < -66.0f)
            continue;

        if (current < spectrumDb[static_cast<size_t>(bin - 1)] || current < spectrumDb[static_cast<size_t>(bin + 1)])
            continue;

        const auto prominence = current - baselineAround(spectrumDb, bin);
        if (prominence < 8.0f)
            continue;

        if (candidateCount < candidates.size())
        {
            candidates[candidateCount++] = Candidate { bin, prominence };
            continue;
        }

        auto weakestIndex = size_t { 0 };
        for (size_t index = 1; index < candidates.size(); ++index)
            if (candidates[index].prominenceDb < candidates[weakestIndex].prominenceDb)
                weakestIndex = index;

        if (prominence > candidates[weakestIndex].prominenceDb)
            candidates[weakestIndex] = Candidate { bin, prominence };
    }

    std::sort(candidates.begin(), candidates.begin() + static_cast<std::ptrdiff_t>(candidateCount),
              [](const Candidate& lhs, const Candidate& rhs)
              {
                  return lhs.prominenceDb > rhs.prominenceDb;
              });

    for (size_t candidateIndex = 0; candidateIndex < candidateCount && result.count < maxAutoEqSuggestions; ++candidateIndex)
    {
        const auto& candidate = candidates[candidateIndex];
        const auto frequency = autoEqFrequencyForBin(candidate.bin);

        if (isNearExisting(result.suggestions, result.count, frequency))
            continue;

        const auto cutDb = -juce::jlimit(1.5f, 4.5f, candidate.prominenceDb * 0.24f);
        const auto q = juce::jlimit(1.5f, 8.0f, candidate.prominenceDb * 0.33f);
        result.suggestions[result.count++] = AutoEqSuggestion {
            BandType::bell,
            frequency,
            cutDb,
            q
        };
    }

    return result;
}
}
