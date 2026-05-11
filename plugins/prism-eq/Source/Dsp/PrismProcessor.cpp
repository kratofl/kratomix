#include "PrismProcessor.h"

namespace kratomix
{
namespace
{
float decibelsToGain(float decibels) noexcept
{
    return juce::Decibels::decibelsToGain(decibels);
}
}

PrismProcessor::PrismProcessor()
{
    for (auto* lane : { &preAnalyzerSamples, &postAnalyzerSamples, &sidechainAnalyzerSamples })
        for (auto& sample : *lane)
            sample.store(0.0f, std::memory_order_relaxed);
}

void PrismProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));
    minimumPhaseEngine.prepare(spec);
    linearPhaseEngine.prepare(spec);

    inputGainDbSmoother.reset(sampleRate, 0.02);
    outputGainDbSmoother.reset(sampleRate, 0.02);
    mixSmoother.reset(sampleRate, 0.01);
    wetSmoother.reset(sampleRate, 0.01);

    reset();
    applyTargetsImmediately();
}

void PrismProcessor::reset()
{
    minimumPhaseEngine.reset();
    linearPhaseEngine.reset();
    analyzerWriteIndex.store(0, std::memory_order_relaxed);
    analyzerSidechainActive.store(false, std::memory_order_relaxed);

    for (auto* lane : { &preAnalyzerSamples, &postAnalyzerSamples, &sidechainAnalyzerSamples })
        for (auto& sample : *lane)
            sample.store(0.0f, std::memory_order_relaxed);

    hasProcessedAudio = false;
    outputLevel.store(0.0f);
}

void PrismProcessor::updateSettings(const PrismSettings& newSettings)
{
    settings = newSettings;
    settings.mix = juce::jlimit(0.0f, 1.0f, settings.mix);

    inputGainDbSmoother.setTargetValue(settings.inputGainDb);
    outputGainDbSmoother.setTargetValue(settings.outputGainDb);
    mixSmoother.setTargetValue(settings.mix);
    wetSmoother.setTargetValue(settings.bypassed ? 0.0f : 1.0f);
    currentLatencySamples.store(prism::prismLatencyFor(settings.phaseMode, settings.qualityMode), std::memory_order_relaxed);
    minimumPhaseEngine.updateSettings(settings);
    linearPhaseEngine.updateSettings(settings);

    if (! hasProcessedAudio)
        applyTargetsImmediately();
}

void PrismProcessor::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    if (buffer.getNumSamples() == 0)
        return;

    jassert(buffer.getNumSamples() <= dryBuffer.getNumSamples());
    jassert(buffer.getNumChannels() <= dryBuffer.getNumChannels());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());

    hasProcessedAudio = true;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto inputGain = decibelsToGain(inputGainDbSmoother.getNextValue());

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.getWritePointer(channel)[sample] *= inputGain;
    }

    if (settings.phaseMode == prism::PhaseMode::linearPhase)
        linearPhaseEngine.process(buffer);
    else
        minimumPhaseEngine.process(buffer, sidechainBuffer);

    double energy = 0.0;

    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto outputGain = decibelsToGain(outputGainDbSmoother.getNextValue());
        const auto wet = mixSmoother.getNextValue() * wetSmoother.getNextValue();

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            auto* samples = buffer.getWritePointer(channel);
            const auto dry = dryBuffer.getSample(channel, sample);
            const auto processed = samples[sample] * outputGain;
            const auto mixed = dry + (processed - dry) * wet;
            samples[sample] = mixed;
            energy += static_cast<double>(mixed) * static_cast<double>(mixed);
        }
    }

    publishAnalyzerSamples(dryBuffer, buffer, sidechainBuffer, buffer.getNumSamples());

    const auto divisor = static_cast<double>(juce::jmax(1, buffer.getNumSamples() * buffer.getNumChannels()));
    outputLevel.store(static_cast<float>(std::sqrt(energy / divisor)));
}

float PrismProcessor::getOutputLevel() const noexcept
{
    return outputLevel.load();
}

int PrismProcessor::getCurrentLatencySamples() const noexcept
{
    return currentLatencySamples.load(std::memory_order_relaxed);
}

void PrismProcessor::copyAnalyzerFrame(PrismAnalyzerFrame& destination) const noexcept
{
    const auto writeIndex = analyzerWriteIndex.load(std::memory_order_acquire);

    for (int sample = 0; sample < PrismAnalyzerFrame::sampleCount; ++sample)
    {
        const auto sourceIndex = (writeIndex + sample) % PrismAnalyzerFrame::sampleCount;
        destination.pre[static_cast<size_t>(sample)] = preAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
        destination.post[static_cast<size_t>(sample)] = postAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
        destination.sidechain[static_cast<size_t>(sample)] = sidechainAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
    }

    minimumPhaseEngine.copyDynamicTelemetryTo(destination);

    destination.sampleRate = sampleRate;
    destination.sidechainActive = analyzerSidechainActive.load(std::memory_order_relaxed);
}

void PrismProcessor::applyTargetsImmediately()
{
    inputGainDbSmoother.setCurrentAndTargetValue(settings.inputGainDb);
    outputGainDbSmoother.setCurrentAndTargetValue(settings.outputGainDb);
    mixSmoother.setCurrentAndTargetValue(settings.mix);
    wetSmoother.setCurrentAndTargetValue(settings.bypassed ? 0.0f : 1.0f);
    minimumPhaseEngine.updateSettings(settings);
    linearPhaseEngine.updateSettings(settings);
}

void PrismProcessor::publishAnalyzerSamples(const juce::AudioBuffer<float>& preBuffer,
                                            const juce::AudioBuffer<float>& postBuffer,
                                            const juce::AudioBuffer<float>* sidechainBuffer,
                                            int numSamples) noexcept
{
    auto writeIndex = analyzerWriteIndex.load(std::memory_order_relaxed);
    const auto sidechainAvailable = sidechainBuffer != nullptr
                                    && sidechainBuffer->getNumChannels() > 0
                                    && sidechainBuffer->getNumSamples() > 0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        preAnalyzerSamples[static_cast<size_t>(writeIndex)].store(monoSampleAt(preBuffer, sample), std::memory_order_relaxed);
        postAnalyzerSamples[static_cast<size_t>(writeIndex)].store(monoSampleAt(postBuffer, sample), std::memory_order_relaxed);
        sidechainAnalyzerSamples[static_cast<size_t>(writeIndex)].store(
            sidechainAvailable ? monoSampleAt(*sidechainBuffer, juce::jmin(sample, sidechainBuffer->getNumSamples() - 1)) : 0.0f,
            std::memory_order_relaxed);

        writeIndex = (writeIndex + 1) % PrismAnalyzerFrame::sampleCount;
    }

    analyzerSidechainActive.store(sidechainAvailable, std::memory_order_relaxed);
    analyzerWriteIndex.store(writeIndex, std::memory_order_release);
}

float PrismProcessor::monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return 0.0f;

    const auto clampedSample = juce::jlimit(0, buffer.getNumSamples() - 1, sample);
    auto value = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        value += buffer.getSample(channel, clampedSample);

    return value / static_cast<float>(buffer.getNumChannels());
}

}
