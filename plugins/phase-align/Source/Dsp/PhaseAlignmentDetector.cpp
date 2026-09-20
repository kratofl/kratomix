#include "PhaseAlignmentDetector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
std::vector<float> makeDetectionSignal(const float* source, int numSamples, double sampleRate)
{
    std::vector<float> filtered(static_cast<size_t>(numSamples), 0.0f);

    const auto highPassCoefficient = static_cast<float>(
        std::exp(-2.0 * 3.14159265358979323846 * 80.0 / sampleRate));
    const auto lowPassCoefficient = static_cast<float>(
        std::exp(-2.0 * 3.14159265358979323846 * std::min(10000.0, sampleRate * 0.4) / sampleRate));

    float previousInput = 0.0f;
    float highPassed = 0.0f;
    float lowPassed = 0.0f;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto input = source[sample];
        highPassed = highPassCoefficient * (highPassed + input - previousInput);
        previousInput = input;
        lowPassed += (1.0f - lowPassCoefficient) * (highPassed - lowPassed);
        filtered[static_cast<size_t>(sample)] = lowPassed;
    }

    return filtered;
}

float normalizedCorrelation(const std::vector<float>& moving,
                            const std::vector<float>& reference,
                            int correction)
{
    const auto numSamples = static_cast<int>(moving.size());
    const auto firstMovingSample = std::max(0, -correction);
    const auto lastMovingSample = std::min(numSamples, numSamples - correction);

    double product = 0.0;
    double movingEnergy = 0.0;
    double referenceEnergy = 0.0;

    for (int sample = firstMovingSample; sample < lastMovingSample; ++sample)
    {
        const auto movingSample = static_cast<double>(moving[static_cast<size_t>(sample)]);
        const auto referenceSample = static_cast<double>(reference[static_cast<size_t>(sample + correction)]);
        product += movingSample * referenceSample;
        movingEnergy += movingSample * movingSample;
        referenceEnergy += referenceSample * referenceSample;
    }

    const auto denominator = std::sqrt(movingEnergy * referenceEnergy);
    if (denominator <= 1.0e-12)
        return 0.0f;

    return static_cast<float>(product / denominator);
}
}

namespace kratomix::phase_align
{
PhaseAlignmentResult PhaseAlignmentDetector::analyze(const float* moving,
                                                      const float* reference,
                                                      int numSamples,
                                                      double sampleRate,
                                                      int maximumCorrectionSamples)
{
    PhaseAlignmentResult result;

    if (moving == nullptr || reference == nullptr || numSamples < 256 || sampleRate <= 0.0)
        return result;

    const auto maximumCorrection = std::clamp(maximumCorrectionSamples, 1, numSamples / 4);
    const auto filteredMoving = makeDetectionSignal(moving, numSamples, sampleRate);
    const auto filteredReference = makeDetectionSignal(reference, numSamples, sampleRate);
    std::vector<float> correlations(static_cast<size_t>(maximumCorrection * 2 + 1), 0.0f);

    auto bestIndex = 0;
    auto bestMagnitude = -std::numeric_limits<float>::infinity();

    for (int correction = -maximumCorrection; correction <= maximumCorrection; ++correction)
    {
        const auto correlation = normalizedCorrelation(filteredMoving, filteredReference, correction);
        const auto index = correction + maximumCorrection;
        correlations[static_cast<size_t>(index)] = correlation;

        if (const auto magnitude = std::abs(correlation); magnitude > bestMagnitude)
        {
            bestMagnitude = magnitude;
            bestIndex = index;
        }
    }

    const auto bestCorrection = bestIndex - maximumCorrection;
    auto fractionalCorrection = static_cast<float>(bestCorrection);

    if (bestIndex > 0 && bestIndex + 1 < static_cast<int>(correlations.size()))
    {
        const auto left = std::abs(correlations[static_cast<size_t>(bestIndex - 1)]);
        const auto centre = std::abs(correlations[static_cast<size_t>(bestIndex)]);
        const auto right = std::abs(correlations[static_cast<size_t>(bestIndex + 1)]);
        const auto curvature = left - 2.0f * centre + right;

        if (std::abs(curvature) > 1.0e-6f)
            fractionalCorrection += std::clamp(0.5f * (left - right) / curvature, -0.5f, 0.5f);
    }

    const auto signedCorrelation = correlations[static_cast<size_t>(bestIndex)];
    result.valid = bestMagnitude >= 0.15f;
    result.correctionSamples = result.valid ? fractionalCorrection : 0.0f;
    result.invertPolarity = result.valid && signedCorrelation < 0.0f;
    result.confidence = std::clamp(bestMagnitude, 0.0f, 1.0f);
    result.correlation = signedCorrelation;
    return result;
}
}
