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

    for (auto& gain : analyzerDynamicGainDb)
        gain.store(0.0f, std::memory_order_relaxed);
}

void PrismProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));

    for (auto& filter : filters)
        filter.prepare(spec);

    for (auto& filter : phaseFilters)
        filter.prepare(spec);

    inputGainDbSmoother.reset(sampleRate, 0.02);
    outputGainDbSmoother.reset(sampleRate, 0.02);
    mixSmoother.reset(sampleRate, 0.01);
    wetSmoother.reset(sampleRate, 0.01);

    reset();
    applyTargetsImmediately();
}

void PrismProcessor::reset()
{
    for (auto& filter : filters)
        filter.reset();

    for (auto& filter : phaseFilters)
        filter.reset();

    for (auto& filter : detectorFilters)
        filter.reset();

    dynamicGainDb.fill(0.0f);
    for (auto& gain : analyzerDynamicGainDb)
        gain.store(0.0f, std::memory_order_relaxed);
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
    updateFilterCoefficients();

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

    updateDynamicGain(buffer, sidechainBuffer, buffer.getNumSamples());
    updateFilterCoefficients();

    juce::dsp::AudioBlock<float> block { buffer };
    juce::dsp::ProcessContextReplacing<float> context { block };

    for (size_t index = 0; index < filters.size(); ++index)
    {
        if (settings.bands[index].enabled)
            filters[index].process(context);
    }

    if (settings.phaseMode == prism::PhaseMode::natural)
    {
        for (size_t index = 0; index < phaseFilters.size(); ++index)
        {
            if (settings.bands[index].enabled)
                phaseFilters[index].process(context);
        }
    }

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

    for (size_t index = 0; index < analyzerDynamicGainDb.size(); ++index)
        destination.dynamicGainDb[index] = analyzerDynamicGainDb[index].load(std::memory_order_relaxed);

    destination.sampleRate = sampleRate;
    destination.sidechainActive = analyzerSidechainActive.load(std::memory_order_relaxed);
}

void PrismProcessor::applyTargetsImmediately()
{
    inputGainDbSmoother.setCurrentAndTargetValue(settings.inputGainDb);
    outputGainDbSmoother.setCurrentAndTargetValue(settings.outputGainDb);
    mixSmoother.setCurrentAndTargetValue(settings.mix);
    wetSmoother.setCurrentAndTargetValue(settings.bypassed ? 0.0f : 1.0f);
    updateFilterCoefficients();
}

void PrismProcessor::updateFilterCoefficients()
{
    for (size_t index = 0; index < filters.size(); ++index)
    {
        *filters[index].state = *makeCoefficients(sampleRate, settings.bands[index], dynamicGainDb[index]);
        *phaseFilters[index].state = *Coefficients::makeAllPass(
            sampleRate,
            juce::jlimit(20.0f, 20000.0f, settings.bands[index].frequency),
            juce::jlimit(0.35f, 4.0f, std::sqrt(juce::jmax(0.1f, settings.bands[index].q))));
        detectorFilters[index].coefficients = Coefficients::makeBandPass(
            sampleRate,
            juce::jlimit(20.0f, 20000.0f, settings.bands[index].frequency),
            juce::jlimit(0.1f, 40.0f, settings.bands[index].q));
    }
}

void PrismProcessor::updateDynamicGain(const juce::AudioBuffer<float>& mainBuffer,
                                       const juce::AudioBuffer<float>* sidechainBuffer,
                                       int numSamples)
{
    const auto sidechainAvailable = sidechainBuffer != nullptr
                                    && sidechainBuffer->getNumChannels() > 0
                                    && sidechainBuffer->getNumSamples() > 0;
    const auto blockSeconds = static_cast<float>(numSamples / juce::jmax(1.0, sampleRate));

    for (size_t index = 0; index < settings.bands.size(); ++index)
    {
        const auto& band = settings.bands[index];

        auto targetDb = 0.0f;
        if (band.enabled && band.dynamicEnabled && std::abs(band.dynamicRangeDb) > 0.001f)
        {
            const auto useExternalSidechain = band.sidechainSource == prism::SidechainSource::external && sidechainAvailable;
            const auto& detectorBuffer = useExternalSidechain ? *sidechainBuffer : mainBuffer;
            const auto detectorSamples = useExternalSidechain ? juce::jmin(numSamples, sidechainBuffer->getNumSamples()) : numSamples;
            const auto detectorDb = bandLimitedRmsDb(detectorBuffer, detectorSamples, detectorFilters[index]);
            const auto overThresholdDb = detectorDb - band.thresholdDb;

            if (overThresholdDb > 0.0f)
                targetDb = -std::abs(band.dynamicRangeDb) * juce::jlimit(0.0f, 1.0f, overThresholdDb / 24.0f);
        }

        auto& currentDb = dynamicGainDb[index];
        const auto movingAwayFromNeutral = std::abs(targetDb) > std::abs(currentDb);
        const auto timeMs = juce::jmax(0.1f, movingAwayFromNeutral ? band.attackMs : band.releaseMs);
        const auto coefficient = std::exp(-blockSeconds / (timeMs * 0.001f));
        currentDb = targetDb + (currentDb - targetDb) * coefficient;
        analyzerDynamicGainDb[index].store(currentDb, std::memory_order_relaxed);
    }
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

float PrismProcessor::blockRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
{
    if (buffer.getNumChannels() <= 0 || numSamples <= 0)
        return -120.0f;

    double energy = 0.0;
    const auto samplesToRead = juce::jmin(numSamples, buffer.getNumSamples());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < samplesToRead; ++sample)
        {
            const auto value = buffer.getSample(channel, sample);
            energy += static_cast<double>(value) * static_cast<double>(value);
        }

    const auto divisor = static_cast<double>(juce::jmax(1, samplesToRead * buffer.getNumChannels()));
    return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(energy / divisor)), -120.0f);
}

float PrismProcessor::bandLimitedRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples, Filter& filter) noexcept
{
    if (buffer.getNumChannels() <= 0 || numSamples <= 0)
        return -120.0f;

    double energy = 0.0;
    const auto samplesToRead = juce::jmin(numSamples, buffer.getNumSamples());

    for (int sample = 0; sample < samplesToRead; ++sample)
    {
        const auto filtered = filter.processSample(monoSampleAt(buffer, sample));
        energy += static_cast<double>(filtered) * static_cast<double>(filtered);
    }

    const auto divisor = static_cast<double>(juce::jmax(1, samplesToRead));
    return juce::Decibels::gainToDecibels(static_cast<float>(std::sqrt(energy / divisor)), -120.0f);
}

PrismProcessor::Coefficients::Ptr PrismProcessor::makeCoefficients(double sampleRateToUse,
                                                                   const PrismBandSettings& band,
                                                                   float dynamicGainDb)
{
    const auto frequency = juce::jlimit(20.0f, 20000.0f, band.frequency);
    const auto q = juce::jlimit(0.1f, 40.0f, band.q);
    const auto gain = decibelsToGain(band.gainDb + dynamicGainDb);

    switch (band.type)
    {
        case prism::BandType::lowShelf:
            return Coefficients::makeLowShelf(sampleRateToUse, frequency, q, gain);
        case prism::BandType::highShelf:
            return Coefficients::makeHighShelf(sampleRateToUse, frequency, q, gain);
        case prism::BandType::highPass:
            return Coefficients::makeHighPass(sampleRateToUse, frequency, q);
        case prism::BandType::lowPass:
            return Coefficients::makeLowPass(sampleRateToUse, frequency, q);
        case prism::BandType::notch:
            return Coefficients::makeNotch(sampleRateToUse, frequency, q);
        case prism::BandType::bell:
        default:
            return Coefficients::makePeakFilter(sampleRateToUse, frequency, q, gain);
    }
}
}
