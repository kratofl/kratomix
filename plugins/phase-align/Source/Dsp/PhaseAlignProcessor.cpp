#include "PhaseAlignProcessor.h"

#include <algorithm>
#include <cmath>

namespace kratomix::phase_align
{
PhaseAlignProcessor::PhaseAlignProcessor(std::function<void()> analysisCompleted)
    : Thread("Kratomix Phase Align analysis"), completionCallback(std::move(analysisCompleted))
{
    for (auto* plot : { &movingPlot, &referencePlot })
        for (auto& sample : *plot)
            sample.store(0.0f, std::memory_order_relaxed);
    startThread(juce::Thread::Priority::low);
}

PhaseAlignProcessor::~PhaseAlignProcessor()
{
    signalThreadShouldExit();
    analysisEvent.signal();
    stopThread(2000);
}

void PhaseAlignProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    preparedChannels = std::clamp(static_cast<int>(spec.numChannels), 1, 2);
    preparedBlockSize = std::max(1, static_cast<int>(spec.maximumBlockSize));
    maximumCorrectionSamples = static_cast<int>(std::ceil(sampleRate * 0.010));
    baseLatencySamples = maximumCorrectionSamples + 2;
    const auto maximumDelay = baseLatencySamples + maximumCorrectionSamples + 2;
    movingDelay.prepare(preparedChannels, maximumDelay, sampleRate);
    referenceDelay.prepare(preparedChannels, maximumDelay, sampleRate);
    movingDelay.setDelaySamples(static_cast<double>(baseLatencySamples), true);
    referenceDelay.setDelaySamples(static_cast<double>(baseLatencySamples), true);
    delayedReference.setSize(preparedChannels, preparedBlockSize, false, true, false);
    reset();
}

void PhaseAlignProcessor::reset() noexcept
{
    movingDelay.reset();
    referenceDelay.reset();
    delayedReference.clear();
    capturePosition.store(0, std::memory_order_relaxed);
    captureState.store(0, std::memory_order_release);
    liveCorrelation.store(0.0f, std::memory_order_relaxed);
    status.store(static_cast<int>(AnalysisStatus::idle), std::memory_order_release);
}

void PhaseAlignProcessor::updateSettings(const Settings& newSettings) noexcept
{
    settings = newSettings;
    settings.offsetMs = std::clamp(settings.offsetMs, -10.0f, 10.0f);
}

void PhaseAlignProcessor::process(juce::AudioBuffer<float>& mainBuffer,
                                  const juce::AudioBuffer<float>* sidechainBuffer) noexcept
{
    const auto numSamples = mainBuffer.getNumSamples();
    const auto numChannels = std::min(mainBuffer.getNumChannels(), preparedChannels);
    if (numSamples <= 0 || numChannels <= 0)
        return;
    jassert(numSamples <= preparedBlockSize);

    if (captureState.load(std::memory_order_acquire) == 1)
    {
        if (sidechainBuffer != nullptr && sidechainBuffer->getNumChannels() > 0)
            capture(mainBuffer, *sidechainBuffer);
        else
        {
            status.store(static_cast<int>(AnalysisStatus::noSidechain), std::memory_order_release);
            captureState.store(3, std::memory_order_release);
        }
    }

    delayedReference.clear();
    if (sidechainBuffer != nullptr && sidechainBuffer->getNumChannels() > 0)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            if (sidechainBuffer->getNumChannels() == 1)
                delayedReference.copyFrom(channel, 0, *sidechainBuffer, 0, 0, numSamples);
            else if (numChannels == 1)
            {
                auto* destination = delayedReference.getWritePointer(0);
                const auto* left = sidechainBuffer->getReadPointer(0);
                const auto* right = sidechainBuffer->getReadPointer(1);
                for (int sample = 0; sample < numSamples; ++sample)
                    destination[sample] = 0.5f * (left[sample] + right[sample]);
            }
            else
                delayedReference.copyFrom(channel, 0, *sidechainBuffer, channel, 0, numSamples);
        }
    }

    const auto aligned = ! settings.bypassed && settings.auditionMode != AuditionMode::original;
    const auto offsetSamples = aligned ? static_cast<double>(settings.offsetMs) * sampleRate * 0.001 : 0.0;
    const auto polarity = aligned && settings.invertPolarity ? -1.0f : 1.0f;
    movingDelay.setDelaySamples(static_cast<double>(baseLatencySamples) + offsetSamples);
    referenceDelay.setDelaySamples(static_cast<double>(baseLatencySamples));
    movingDelay.process(mainBuffer, polarity);
    referenceDelay.process(delayedReference, 1.0f, numSamples);

    if (sidechainBuffer != nullptr && sidechainBuffer->getNumChannels() > 0)
    {
        double product = 0.0;
        double movingEnergy = 0.0;
        double referenceEnergy = 0.0;
        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto* moving = mainBuffer.getReadPointer(channel);
            const auto* reference = delayedReference.getReadPointer(channel);
            for (int sample = 0; sample < numSamples; ++sample)
            {
                product += static_cast<double>(moving[sample]) * reference[sample];
                movingEnergy += static_cast<double>(moving[sample]) * moving[sample];
                referenceEnergy += static_cast<double>(reference[sample]) * reference[sample];
            }
        }

        const auto denominator = std::sqrt(movingEnergy * referenceEnergy);
        const auto blockCorrelation = denominator > 1.0e-12 ? static_cast<float>(product / denominator) : 0.0f;
        liveCorrelation.store(liveCorrelation.load(std::memory_order_relaxed) * 0.85f
                                  + blockCorrelation * 0.15f,
                              std::memory_order_relaxed);

        if (! settings.bypassed && settings.auditionMode == AuditionMode::difference)
            for (int channel = 0; channel < numChannels; ++channel)
                for (int sample = 0; sample < numSamples; ++sample)
                    mainBuffer.setSample(channel, sample,
                                         0.5f * (mainBuffer.getSample(channel, sample)
                                                 - delayedReference.getSample(channel, sample)));
    }
    else
    {
        liveCorrelation.store(0.0f, std::memory_order_relaxed);
    }
}

bool PhaseAlignProcessor::requestAnalysis() noexcept
{
    auto expected = 0;
    if (! captureState.compare_exchange_strong(expected, 1, std::memory_order_acq_rel))
        return false;
    capturePosition.store(0, std::memory_order_relaxed);
    resultReady.store(false, std::memory_order_release);
    status.store(static_cast<int>(AnalysisStatus::capturing), std::memory_order_release);
    return true;
}

bool PhaseAlignProcessor::consumeAnalysisResult(PhaseAlignmentResult& destination) noexcept
{
    if (! resultReady.exchange(false, std::memory_order_acq_rel))
        return false;
    destination.valid = resultValid.load(std::memory_order_relaxed);
    destination.correctionSamples = resultCorrection.load(std::memory_order_relaxed);
    destination.invertPolarity = resultPolarity.load(std::memory_order_relaxed);
    destination.confidence = resultConfidence.load(std::memory_order_relaxed);
    destination.correlation = resultCorrelation.load(std::memory_order_relaxed);
    return true;
}

int PhaseAlignProcessor::getLatencySamples() const noexcept { return baseLatencySamples; }
float PhaseAlignProcessor::getCorrelation() const noexcept { return liveCorrelation.load(std::memory_order_relaxed); }
float PhaseAlignProcessor::getConfidence() const noexcept { return resultConfidence.load(std::memory_order_relaxed); }
AnalysisStatus PhaseAlignProcessor::getAnalysisStatus() const noexcept
{
    return static_cast<AnalysisStatus>(status.load(std::memory_order_acquire));
}

void PhaseAlignProcessor::copyAnalysisFrame(AnalysisFrame& destination) const noexcept
{
    for (size_t index = 0; index < destination.moving.size(); ++index)
    {
        destination.moving[index] = movingPlot[index].load(std::memory_order_relaxed);
        destination.reference[index] = referencePlot[index].load(std::memory_order_relaxed);
    }
    destination.valid = previewValid.load(std::memory_order_acquire);
}

void PhaseAlignProcessor::run()
{
    while (! threadShouldExit())
    {
        analysisEvent.wait(20);
        if (threadShouldExit())
            break;

        const auto state = captureState.load(std::memory_order_acquire);
        if (state == 3)
        {
            publishUnavailable(AnalysisStatus::noSidechain);
            continue;
        }
        if (state != 2)
            continue;

        status.store(static_cast<int>(AnalysisStatus::analyzing), std::memory_order_release);
        const auto result = PhaseAlignmentDetector::analyze(movingCapture.data(), referenceCapture.data(),
                                                            captureSampleCount, sampleRate,
                                                            maximumCorrectionSamples);
        resultValid.store(result.valid, std::memory_order_relaxed);
        resultCorrection.store(result.correctionSamples, std::memory_order_relaxed);
        resultPolarity.store(result.invertPolarity, std::memory_order_relaxed);
        resultConfidence.store(result.confidence, std::memory_order_relaxed);
        resultCorrelation.store(result.correlation, std::memory_order_relaxed);
        publishPreview(result);
        status.store(static_cast<int>(result.valid ? AnalysisStatus::ready : AnalysisStatus::lowConfidence),
                     std::memory_order_release);
        captureState.store(0, std::memory_order_release);
        resultReady.store(true, std::memory_order_release);
        if (completionCallback)
            completionCallback();
    }
}

void PhaseAlignProcessor::capture(const juce::AudioBuffer<float>& mainBuffer,
                                  const juce::AudioBuffer<float>& sidechainBuffer) noexcept
{
    auto position = capturePosition.load(std::memory_order_relaxed);
    const auto samplesToCopy = std::min(captureSampleCount - position, mainBuffer.getNumSamples());
    for (int sample = 0; sample < samplesToCopy; ++sample)
    {
        movingCapture[static_cast<size_t>(position + sample)] = monoSample(mainBuffer, sample);
        referenceCapture[static_cast<size_t>(position + sample)] = monoSample(sidechainBuffer, sample);
    }
    position += samplesToCopy;
    capturePosition.store(position, std::memory_order_relaxed);
    if (position >= captureSampleCount)
        captureState.store(2, std::memory_order_release);
}

void PhaseAlignProcessor::publishPreview(const PhaseAlignmentResult& result) noexcept
{
    const auto correction = result.valid ? result.correctionSamples : 0.0f;
    const auto polarity = result.valid && result.invertPolarity ? -1.0f : 1.0f;
    const auto safeFirst = maximumCorrectionSamples + 4;
    const auto safeLast = captureSampleCount - maximumCorrectionSamples - 4;
    auto strongestSample = safeFirst;
    auto strongestMagnitude = 0.0f;
    for (int sample = safeFirst; sample < safeLast; ++sample)
    {
        const auto magnitude = std::abs(referenceCapture[static_cast<size_t>(sample)]);
        if (magnitude > strongestMagnitude)
        {
            strongestMagnitude = magnitude;
            strongestSample = sample;
        }
    }
    const auto first = std::clamp(strongestSample - AnalysisFrame::sampleCount / 2,
                                  safeFirst,
                                  safeLast - AnalysisFrame::sampleCount);
    for (int plotSample = 0; plotSample < AnalysisFrame::sampleCount; ++plotSample)
    {
        const auto referenceIndex = first + plotSample;
        const auto movingPosition = static_cast<float>(referenceIndex) - correction;
        const auto movingIndex = std::clamp(static_cast<int>(std::floor(movingPosition)), 0,
                                            captureSampleCount - 2);
        const auto fraction = movingPosition - static_cast<float>(movingIndex);
        const auto movingSample = (movingCapture[static_cast<size_t>(movingIndex)] * (1.0f - fraction)
                                   + movingCapture[static_cast<size_t>(movingIndex + 1)] * fraction)
                                  * polarity;
        movingPlot[static_cast<size_t>(plotSample)].store(movingSample, std::memory_order_relaxed);
        referencePlot[static_cast<size_t>(plotSample)].store(
            referenceCapture[static_cast<size_t>(referenceIndex)], std::memory_order_relaxed);
    }
    previewValid.store(true, std::memory_order_release);
}

void PhaseAlignProcessor::publishUnavailable(AnalysisStatus newStatus) noexcept
{
    resultValid.store(false, std::memory_order_relaxed);
    resultCorrection.store(0.0f, std::memory_order_relaxed);
    resultPolarity.store(false, std::memory_order_relaxed);
    resultConfidence.store(0.0f, std::memory_order_relaxed);
    resultCorrelation.store(0.0f, std::memory_order_relaxed);
    captureState.store(0, std::memory_order_release);
    status.store(static_cast<int>(newStatus), std::memory_order_release);
    resultReady.store(true, std::memory_order_release);
    if (completionCallback)
        completionCallback();
}

float PhaseAlignProcessor::monoSample(const juce::AudioBuffer<float>& buffer, int sample) noexcept
{
    if (buffer.getNumChannels() <= 1)
        return buffer.getSample(0, sample);
    return 0.5f * (buffer.getSample(0, sample) + buffer.getSample(1, sample));
}
}
