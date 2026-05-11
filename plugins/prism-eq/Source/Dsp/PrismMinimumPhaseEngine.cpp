#include "PrismMinimumPhaseEngine.h"

namespace kratomix
{
void PrismMinimumPhaseEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (auto& filter : filters)
        filter.prepare(spec);
    for (auto& filter : phaseFilters)
        filter.prepare(spec);

    juce::dsp::ProcessSpec spec2x { spec.sampleRate * 2.0, spec.maximumBlockSize * 2u, spec.numChannels };
    juce::dsp::ProcessSpec spec4x { spec.sampleRate * 4.0, spec.maximumBlockSize * 4u, spec.numChannels };

    for (auto& filter : filters2x)
        filter.prepare(spec2x);
    for (auto& filter : phaseFilters2x)
        filter.prepare(spec2x);
    for (auto& filter : filters4x)
        filter.prepare(spec4x);
    for (auto& filter : phaseFilters4x)
        filter.prepare(spec4x);

    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels,
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);

    oversampler2x->initProcessing(spec.maximumBlockSize);
    oversampler4x->initProcessing(spec.maximumBlockSize);

    reset();
}

void PrismMinimumPhaseEngine::reset()
{
    for (auto& filter : filters)
        filter.reset();
    for (auto& filter : phaseFilters)
        filter.reset();
    for (auto& filter : filters2x)
        filter.reset();
    for (auto& filter : phaseFilters2x)
        filter.reset();
    for (auto& filter : filters4x)
        filter.reset();
    for (auto& filter : phaseFilters4x)
        filter.reset();
    for (auto& filter : detectorFilters)
        filter.reset();

    if (oversampler2x != nullptr)
        oversampler2x->reset();
    if (oversampler4x != nullptr)
        oversampler4x->reset();

    dynamicGainDb.fill(0.0f);
    for (auto& value : analyzerDynamicGainDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDynamicTargetGainDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorLevelDb)
        value.store(-120.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorOverThresholdDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorUsingExternalSidechain)
        value.store(false, std::memory_order_relaxed);
}

void PrismMinimumPhaseEngine::updateSettings(const PrismSettings& newSettings)
{
    settings = newSettings;
    updateFilterCoefficients(sampleRate, filters, phaseFilters);
    updateFilterCoefficients(sampleRate * 2.0, filters2x, phaseFilters2x);
    updateFilterCoefficients(sampleRate * 4.0, filters4x, phaseFilters4x);
    updateDetectorCoefficients();
}

void PrismMinimumPhaseEngine::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    if (settings.qualityMode == prism::QualityMode::native)
    {
        processNativeBlock(buffer, sidechainBuffer);
        return;
    }

    updateDynamicGain(buffer, sidechainBuffer, buffer.getNumSamples());
    updateDetectorCoefficients();

    auto* oversampler = settings.qualityMode == prism::QualityMode::oversample4x ? oversampler4x.get() : oversampler2x.get();
    if (oversampler == nullptr)
    {
        processNativeBlock(buffer, sidechainBuffer);
        return;
    }

    juce::dsp::AudioBlock<float> block { buffer };
    auto upsampledBlock = oversampler->processSamplesUp(block);

    if (settings.qualityMode == prism::QualityMode::oversample4x)
        processFilterBlock(upsampledBlock, filters4x, phaseFilters4x, sampleRate * 4.0);
    else
        processFilterBlock(upsampledBlock, filters2x, phaseFilters2x, sampleRate * 2.0);

    oversampler->processSamplesDown(block);
}

void PrismMinimumPhaseEngine::processNativeBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    updateDynamicGain(buffer, sidechainBuffer, buffer.getNumSamples());
    updateDetectorCoefficients();

    juce::dsp::AudioBlock<float> block { buffer };
    processFilterBlock(block, filters, phaseFilters, sampleRate);
}

void PrismMinimumPhaseEngine::processFilterBlock(juce::dsp::AudioBlock<float>& block,
                                                std::array<StereoFilter, prism::maxBands>& targetFilters,
                                                std::array<StereoFilter, prism::maxBands>& targetPhaseFilters,
                                                double processingSampleRate)
{
    updateFilterCoefficients(processingSampleRate, targetFilters, targetPhaseFilters);

    juce::dsp::ProcessContextReplacing<float> context { block };

    for (size_t index = 0; index < targetFilters.size(); ++index)
        if (settings.bands[index].enabled)
            targetFilters[index].process(context);

    if (settings.phaseMode == prism::PhaseMode::natural)
        for (size_t index = 0; index < targetPhaseFilters.size(); ++index)
            if (settings.bands[index].enabled)
                targetPhaseFilters[index].process(context);
}

void PrismMinimumPhaseEngine::copyDynamicTelemetryTo(PrismAnalyzerFrame& destination) const noexcept
{
    for (size_t index = 0; index < analyzerDynamicGainDb.size(); ++index)
    {
        destination.dynamicGainDb[index] = analyzerDynamicGainDb[index].load(std::memory_order_relaxed);
        destination.dynamicTargetGainDb[index] = analyzerDynamicTargetGainDb[index].load(std::memory_order_relaxed);
        destination.detectorLevelDb[index] = analyzerDetectorLevelDb[index].load(std::memory_order_relaxed);
        destination.detectorOverThresholdDb[index] = analyzerDetectorOverThresholdDb[index].load(std::memory_order_relaxed);
        destination.detectorUsingExternalSidechain[index] = analyzerDetectorUsingExternalSidechain[index].load(std::memory_order_relaxed);
    }
}

void PrismMinimumPhaseEngine::updateFilterCoefficients(double processingSampleRate,
                                                       std::array<StereoFilter, prism::maxBands>& targetFilters,
                                                       std::array<StereoFilter, prism::maxBands>& targetPhaseFilters)
{
    for (size_t index = 0; index < targetFilters.size(); ++index)
    {
        *targetFilters[index].state = *makePrismCoefficients(processingSampleRate, settings.bands[index], dynamicGainDb[index]);
        *targetPhaseFilters[index].state = *Coefficients::makeAllPass(
            processingSampleRate,
            prism::safeFilterFrequency(settings.bands[index].frequency, processingSampleRate),
            juce::jlimit(0.35f, 4.0f, std::sqrt(juce::jmax(0.1f, settings.bands[index].q))));
    }
}

void PrismMinimumPhaseEngine::updateDetectorCoefficients()
{
    for (size_t index = 0; index < detectorFilters.size(); ++index)
        detectorFilters[index].coefficients = Coefficients::makeBandPass(
            sampleRate,
            prism::safeFilterFrequency(settings.bands[index].frequency, sampleRate),
            prism::safeFilterQ(settings.bands[index].q));
}

void PrismMinimumPhaseEngine::updateDynamicGain(const juce::AudioBuffer<float>& mainBuffer,
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
                targetDb = band.dynamicRangeDb * juce::jlimit(0.0f, 1.0f, overThresholdDb / 24.0f);

            analyzerDetectorLevelDb[index].store(detectorDb, std::memory_order_relaxed);
            analyzerDetectorOverThresholdDb[index].store(juce::jmax(0.0f, overThresholdDb), std::memory_order_relaxed);
            analyzerDetectorUsingExternalSidechain[index].store(useExternalSidechain, std::memory_order_relaxed);
        }
        else
        {
            analyzerDetectorLevelDb[index].store(-120.0f, std::memory_order_relaxed);
            analyzerDetectorOverThresholdDb[index].store(0.0f, std::memory_order_relaxed);
            analyzerDetectorUsingExternalSidechain[index].store(false, std::memory_order_relaxed);
        }

        analyzerDynamicTargetGainDb[index].store(targetDb, std::memory_order_relaxed);

        auto& currentDb = dynamicGainDb[index];
        const auto movingAwayFromNeutral = std::abs(targetDb) > std::abs(currentDb);
        const auto timeMs = juce::jmax(0.1f, movingAwayFromNeutral ? band.attackMs : band.releaseMs);
        const auto coefficient = std::exp(-blockSeconds / (timeMs * 0.001f));
        currentDb = targetDb + (currentDb - targetDb) * coefficient;
        analyzerDynamicGainDb[index].store(currentDb, std::memory_order_relaxed);
    }
}

float PrismMinimumPhaseEngine::monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
        return 0.0f;

    const auto clampedSample = juce::jlimit(0, buffer.getNumSamples() - 1, sample);
    auto value = 0.0f;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        value += buffer.getSample(channel, clampedSample);

    return value / static_cast<float>(buffer.getNumChannels());
}

float PrismMinimumPhaseEngine::bandLimitedRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples, Filter& filter) noexcept
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
}
