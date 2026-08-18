#include "MultibandProcessor.h"

namespace kratomix
{
namespace
{
float decibelsToGain(float decibels) noexcept
{
    return juce::Decibels::decibelsToGain(decibels);
}

float gainToDecibels(float gain) noexcept
{
    return juce::Decibels::gainToDecibels(gain, -120.0f);
}

float envelopeCoefficient(float timeMs, double sampleRate) noexcept
{
    const auto safeTimeMs = juce::jmax(0.01f, timeMs);
    return std::exp(-1.0f / (0.001f * safeTimeMs * static_cast<float>(sampleRate)));
}

float positiveKneeAmount(float valueDb, float kneeDb) noexcept
{
    if (kneeDb <= 0.001f)
        return juce::jmax(0.0f, valueDb);

    const auto halfKnee = kneeDb * 0.5f;
    if (valueDb <= -halfKnee)
        return 0.0f;
    if (valueDb >= halfKnee)
        return valueDb;

    const auto x = valueDb + halfKnee;
    return (x * x) / (2.0f * kneeDb);
}

bool shouldHearBand(const MultibandBandSettings& band, bool anySoloOrAudition) noexcept
{
    if (! anySoloOrAudition)
        return true;

    return band.solo || band.audition;
}
}

void MultibandBandFilter::prepare(const juce::dsp::ProcessSpec& spec)
{
    processingSampleRate = spec.sampleRate;
    highpass.setType(Filter::Type::highpass);
    lowpass.setType(Filter::Type::lowpass);
    highpass.prepare(spec);
    lowpass.prepare(spec);
    centreSmoother.reset(processingSampleRate, 0.02);
    widthSmoother.reset(processingSampleRate, 0.02);
    centreSmoother.setCurrentAndTargetValue(1000.0f);
    widthSmoother.setCurrentAndTargetValue(2.0f);
    updateCutoffs(1000.0f, 2.0f);
}

void MultibandBandFilter::reset()
{
    highpass.reset();
    lowpass.reset();
    samplesUntilCutoffUpdate = 0;
}

void MultibandBandFilter::setBand(float centreFrequency, float widthOctaves, double sampleRate)
{
    processingSampleRate = sampleRate;
    centreSmoother.setTargetValue(juce::jlimit(20.0f, static_cast<float>(sampleRate * 0.45), centreFrequency));
    widthSmoother.setTargetValue(juce::jlimit(0.25f, 6.0f, widthOctaves));
}

void MultibandBandFilter::updateCutoffs(float centreFrequency, float widthOctaves)
{
    const auto safeCentre = juce::jlimit(20.0f, static_cast<float>(processingSampleRate * 0.45), centreFrequency);
    const auto safeWidth = juce::jlimit(0.25f, 6.0f, widthOctaves);
    const auto halfBandRatio = std::pow(2.0f, safeWidth * 0.5f);
    const auto lowEdge = juce::jlimit(20.0f, static_cast<float>(processingSampleRate * 0.44), safeCentre / halfBandRatio);
    const auto highEdge = juce::jlimit(lowEdge * 1.02f,
                                       static_cast<float>(processingSampleRate * 0.45),
                                       safeCentre * halfBandRatio);

    highpass.setCutoffFrequency(lowEdge);
    lowpass.setCutoffFrequency(highEdge);
}

void MultibandBandFilter::process(const juce::AudioBuffer<float>& source,
                                  juce::AudioBuffer<float>& destination,
                                  int numChannels,
                                  int numSamples) noexcept
{
    const auto channelsToProcess = juce::jmin(numChannels, source.getNumChannels(), destination.getNumChannels());
    const auto samplesToProcess = juce::jmin(numSamples, source.getNumSamples(), destination.getNumSamples());
    destination.clear(0, samplesToProcess);

    for (int sample = 0; sample < samplesToProcess; ++sample)
    {
        const auto centre = centreSmoother.getNextValue();
        const auto width = widthSmoother.getNextValue();
        if (samplesUntilCutoffUpdate-- <= 0)
        {
            updateCutoffs(centre, width);
            samplesUntilCutoffUpdate = 15;
        }

        for (int channel = 0; channel < channelsToProcess; ++channel)
        {
            auto value = highpass.processSample(channel, source.getSample(channel, sample));
            value = lowpass.processSample(channel, value);
            destination.setSample(channel, sample, value);
        }
    }
}

MultibandProcessor::MultibandProcessor()
{
    for (auto* lane : { &inputAnalyzerSamples, &outputAnalyzerSamples, &sidechainAnalyzerSamples })
        for (auto& sample : *lane)
            sample.store(0.0f, std::memory_order_relaxed);

    for (auto* lane : { &dynamicGainTelemetry, &detectorTelemetry, &bandLevelTelemetry })
        for (auto& value : *lane)
            value.store(0.0f, std::memory_order_relaxed);
}

void MultibandProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    preparedChannels = juce::jlimit(1, maxProcessingChannels, static_cast<int>(spec.numChannels));
    preparedBlockSize = static_cast<int>(spec.maximumBlockSize);
    allocateBuffers(preparedChannels, preparedBlockSize);

    juce::dsp::ProcessSpec filterSpec = spec;
    filterSpec.numChannels = static_cast<juce::uint32>(preparedChannels);
    for (auto* filters : { &audioFilters, &detectorFilters, &sidechainFilters })
        for (auto& filter : *filters)
            filter.prepare(filterSpec);
    lookaheadDelay.prepare(filterSpec);
    dryDelay.prepare(filterSpec);

    inputGainDbSmoother.reset(sampleRate, 0.02);
    outputGainDbSmoother.reset(sampleRate, 0.02);
    mixSmoother.reset(sampleRate, 0.01);
    wetSmoother.reset(sampleRate, 0.01);

    reset();
    updateBandFilters();
    applyTargetsImmediately();
}

void MultibandProcessor::reset()
{
    for (auto* filters : { &audioFilters, &detectorFilters, &sidechainFilters })
        for (auto& filter : *filters)
            filter.reset();
    lookaheadDelay.reset();
    dryDelay.reset();
    analyzerWriteIndex.store(0, std::memory_order_relaxed);
    analyzerSidechainActive.store(false, std::memory_order_relaxed);

    detectorEnvelope.fill(0.0f);
    gainDbState.fill(0.0f);
    bandEnergyState.fill(0.0f);
    sampleGainDb.fill(0.0f);

    for (auto* lane : { &inputAnalyzerSamples, &outputAnalyzerSamples, &sidechainAnalyzerSamples })
        for (auto& sample : *lane)
            sample.store(0.0f, std::memory_order_relaxed);

    for (auto* lane : { &dynamicGainTelemetry, &detectorTelemetry, &bandLevelTelemetry })
        for (auto& value : *lane)
            value.store(0.0f, std::memory_order_relaxed);

    hasProcessedAudio = false;
    outputLevel.store(0.0f);
}

void MultibandProcessor::updateSettings(const MultibandSettings& newSettings)
{
    settings = newSettings;
    settings.mix = juce::jlimit(0.0f, 1.0f, settings.mix);

    for (auto& band : settings.bands)
    {
        band.frequencyHz = juce::jlimit(20.0f, 20000.0f, band.frequencyHz);
        band.widthOctaves = juce::jlimit(0.25f, 6.0f, band.widthOctaves);
        band.ratio = juce::jmax(1.0f, band.ratio);
        band.attackMs = juce::jmax(0.1f, band.attackMs);
        band.releaseMs = juce::jmax(5.0f, band.releaseMs);
        band.stereoLink = juce::jlimit(0.0f, 1.0f, band.stereoLink);
    }

    inputGainDbSmoother.setTargetValue(settings.inputGainDb);
    outputGainDbSmoother.setTargetValue(settings.outputGainDb);
    mixSmoother.setTargetValue(settings.mix);
    wetSmoother.setTargetValue(settings.bypassed ? 0.0f : 1.0f);

    const auto latency = settings.lookaheadMode == multiband::LookaheadMode::fiveMilliseconds
                             ? juce::jmin(maxLookaheadSamples - 1, static_cast<int>(std::round(sampleRate * 0.005)))
                             : 0;
    currentLatencySamples.store(latency, std::memory_order_relaxed);
    lookaheadDelay.setDelay(static_cast<float>(latency));
    dryDelay.setDelay(static_cast<float>(latency));

    updateBandFilters();

    if (! hasProcessedAudio)
        applyTargetsImmediately();
}

void MultibandProcessor::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = juce::jmin(buffer.getNumChannels(), preparedChannels);
    if (numSamples == 0 || numChannels == 0)
        return;

    jassert(numSamples <= preparedBlockSize);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
        detectorBuffer.copyFrom(channel, 0, buffer, channel, 0, numSamples);
    }

    if (settings.bypassed)
    {
        publishAnalyzerSamples(dryBuffer, buffer, sidechainBuffer, numSamples);
        outputLevel.store(buffer.getRMSLevel(0, 0, numSamples));
        return;
    }

    hasProcessedAudio = true;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto inputGain = decibelsToGain(inputGainDbSmoother.getNextValue());

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto value = buffer.getSample(channel, sample) * inputGain;
            buffer.setSample(channel, sample, value);
            detectorBuffer.setSample(channel, sample, value);
        }
    }

    processLookahead(buffer, numChannels, numSamples);

    const auto sidechainAvailable = sidechainBuffer != nullptr
                                    && sidechainBuffer->getNumChannels() > 0
                                    && sidechainBuffer->getNumSamples() > 0;

    for (int bandIndex = 0; bandIndex < multiband::maxBands; ++bandIndex)
    {
        const auto index = static_cast<size_t>(bandIndex);
        audioFilters[index].process(buffer, audioBands[index], numChannels, numSamples);
        detectorFilters[index].process(detectorBuffer, detectorBands[index], numChannels, numSamples);

        if (sidechainAvailable)
            sidechainFilters[index].process(*sidechainBuffer, sidechainBands[index], numChannels, numSamples);
        else
            sidechainBands[index].clear(0, numSamples);
    }

    const auto anySoloOrAudition = std::any_of(settings.bands.begin(), settings.bands.end(), [](const auto& band)
    {
        return band.enabled && (band.solo || band.audition);
    });

    double energy = 0.0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int bandIndex = 0; bandIndex < multiband::maxBands; ++bandIndex)
        {
            const auto& band = settings.bands[static_cast<size_t>(bandIndex)];
            const auto magnitude = detectorMagnitudeForBand(bandIndex, sample, detectorBands, sidechainBands, sidechainAvailable);
            const auto attack = envelopeCoefficient(band.attackMs, sampleRate);
            const auto release = envelopeCoefficient(band.releaseMs, sampleRate);
            auto& envelope = detectorEnvelope[static_cast<size_t>(bandIndex)];
            envelope = (magnitude > envelope ? attack : release) * envelope
                       + (1.0f - (magnitude > envelope ? attack : release)) * magnitude;

            const auto detectorDb = gainToDecibels(envelope);
            const auto targetGainDb = computeDynamicGainDb(band, detectorDb);
            const auto gainSmoothing = envelopeCoefficient(targetGainDb < gainDbState[static_cast<size_t>(bandIndex)] ? band.attackMs : band.releaseMs, sampleRate);
            gainDbState[static_cast<size_t>(bandIndex)] = gainSmoothing * gainDbState[static_cast<size_t>(bandIndex)]
                                                          + (1.0f - gainSmoothing) * targetGainDb;
            sampleGainDb[static_cast<size_t>(bandIndex)] = gainDbState[static_cast<size_t>(bandIndex)];

            detectorTelemetry[static_cast<size_t>(bandIndex)].store(detectorDb, std::memory_order_relaxed);
            dynamicGainTelemetry[static_cast<size_t>(bandIndex)].store(gainDbState[static_cast<size_t>(bandIndex)], std::memory_order_relaxed);
        }

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto sum = anySoloOrAudition ? 0.0f : buffer.getSample(channel, sample);

            for (int bandIndex = 0; bandIndex < multiband::maxBands; ++bandIndex)
            {
                const auto& band = settings.bands[static_cast<size_t>(bandIndex)];
                if (! band.enabled || ! shouldHearBand(band, anySoloOrAudition))
                    continue;

                const auto gain = decibelsToGain(sampleGainDb[static_cast<size_t>(bandIndex)] + band.makeupDb);
                const auto detectorUsesExternal = sidechainAvailable && band.detectorSource == multiband::DetectorSource::external;
                const auto& audibleBand = band.audition
                                              ? (detectorUsesExternal ? sidechainBands[static_cast<size_t>(bandIndex)]
                                                                      : detectorBands[static_cast<size_t>(bandIndex)])
                                              : audioBands[static_cast<size_t>(bandIndex)];
                const auto bandSample = audibleBand.getSample(channel, sample);
                sum += anySoloOrAudition ? bandSample * gain : bandSample * (gain - 1.0f);
                bandEnergyState[static_cast<size_t>(bandIndex)] = bandEnergyState[static_cast<size_t>(bandIndex)] * 0.995f
                                                                  + std::abs(bandSample) * 0.005f;
            }

            buffer.setSample(channel, sample, sum);
        }
    }

    for (int sample = 0; sample < numSamples; ++sample)
    {
        const auto outputGain = decibelsToGain(outputGainDbSmoother.getNextValue());
        const auto wet = mixSmoother.getNextValue() * wetSmoother.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            const auto dry = dryBuffer.getSample(channel, sample);
            const auto processed = buffer.getSample(channel, sample) * outputGain;
            const auto mixed = dry + (processed - dry) * wet;
            buffer.setSample(channel, sample, mixed);
            energy += static_cast<double>(mixed) * static_cast<double>(mixed);
        }
    }

    for (int bandIndex = 0; bandIndex < multiband::maxBands; ++bandIndex)
        bandLevelTelemetry[static_cast<size_t>(bandIndex)].store(gainToDecibels(bandEnergyState[static_cast<size_t>(bandIndex)]), std::memory_order_relaxed);

    publishAnalyzerSamples(dryBuffer, buffer, sidechainBuffer, numSamples);

    const auto divisor = static_cast<double>(juce::jmax(1, numSamples * numChannels));
    outputLevel.store(static_cast<float>(std::sqrt(energy / divisor)));
}

float MultibandProcessor::getOutputLevel() const noexcept
{
    return outputLevel.load(std::memory_order_relaxed);
}

int MultibandProcessor::getCurrentLatencySamples() const noexcept
{
    return currentLatencySamples.load(std::memory_order_relaxed);
}

void MultibandProcessor::copyAnalyzerFrame(MultibandAnalyzerFrame& destination) const noexcept
{
    const auto writeIndex = analyzerWriteIndex.load(std::memory_order_acquire);

    for (int sample = 0; sample < MultibandAnalyzerFrame::sampleCount; ++sample)
    {
        const auto sourceIndex = (writeIndex + sample) % MultibandAnalyzerFrame::sampleCount;
        destination.input[static_cast<size_t>(sample)] = inputAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
        destination.output[static_cast<size_t>(sample)] = outputAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
        destination.sidechain[static_cast<size_t>(sample)] = sidechainAnalyzerSamples[static_cast<size_t>(sourceIndex)].load(std::memory_order_relaxed);
    }

    for (int band = 0; band < multiband::maxBands; ++band)
    {
        destination.dynamicGainDb[static_cast<size_t>(band)] = dynamicGainTelemetry[static_cast<size_t>(band)].load(std::memory_order_relaxed);
        destination.detectorLevelDb[static_cast<size_t>(band)] = detectorTelemetry[static_cast<size_t>(band)].load(std::memory_order_relaxed);
        destination.bandLevelDb[static_cast<size_t>(band)] = bandLevelTelemetry[static_cast<size_t>(band)].load(std::memory_order_relaxed);
    }

    destination.sampleRate = sampleRate;
    destination.outputLevel = outputLevel.load(std::memory_order_relaxed);
    destination.sidechainActive = analyzerSidechainActive.load(std::memory_order_relaxed);
}

void MultibandProcessor::allocateBuffers(int numChannels, int numSamples)
{
    dryBuffer.setSize(numChannels, numSamples);
    detectorBuffer.setSize(numChannels, numSamples);

    for (auto* collection : { &audioBands, &detectorBands, &sidechainBands })
        for (auto& band : *collection)
            band.setSize(numChannels, numSamples);
}

void MultibandProcessor::applyTargetsImmediately()
{
    inputGainDbSmoother.setCurrentAndTargetValue(settings.inputGainDb);
    outputGainDbSmoother.setCurrentAndTargetValue(settings.outputGainDb);
    mixSmoother.setCurrentAndTargetValue(settings.mix);
    wetSmoother.setCurrentAndTargetValue(settings.bypassed ? 0.0f : 1.0f);
}

void MultibandProcessor::updateBandFilters()
{
    for (int index = 0; index < multiband::maxBands; ++index)
    {
        const auto& band = settings.bands[static_cast<size_t>(index)];
        for (auto* filters : { &audioFilters, &detectorFilters, &sidechainFilters })
            (*filters)[static_cast<size_t>(index)].setBand(band.frequencyHz, band.widthOctaves, sampleRate);
    }
}

void MultibandProcessor::processLookahead(juce::AudioBuffer<float>& audioBuffer, int numChannels, int numSamples)
{
    if (currentLatencySamples.load(std::memory_order_relaxed) <= 0)
        return;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto wetInput = audioBuffer.getSample(channel, sample);
            lookaheadDelay.pushSample(channel, wetInput);
            audioBuffer.setSample(channel, sample, lookaheadDelay.popSample(channel));

            const auto dryInput = dryBuffer.getSample(channel, sample);
            dryDelay.pushSample(channel, dryInput);
            dryBuffer.setSample(channel, sample, dryDelay.popSample(channel));
        }
    }
}

float MultibandProcessor::detectorMagnitudeForBand(int bandIndex,
                                                   int sample,
                                                   const std::array<juce::AudioBuffer<float>, multiband::maxBands>& internalBands,
                                                   const std::array<juce::AudioBuffer<float>, multiband::maxBands>& externalBands,
                                                   bool sidechainAvailable) const noexcept
{
    const auto& band = settings.bands[static_cast<size_t>(bandIndex)];
    const auto& source = sidechainAvailable && band.detectorSource == multiband::DetectorSource::external
                             ? externalBands[static_cast<size_t>(bandIndex)]
                             : internalBands[static_cast<size_t>(bandIndex)];
    auto linkedMagnitude = 0.0f;
    auto averageMagnitude = 0.0f;
    const auto channels = juce::jmax(1, source.getNumChannels());

    for (int channel = 0; channel < source.getNumChannels(); ++channel)
    {
        const auto magnitude = std::abs(source.getSample(channel, juce::jmin(sample, source.getNumSamples() - 1)));
        linkedMagnitude = juce::jmax(linkedMagnitude, magnitude);
        averageMagnitude += magnitude;
    }

    averageMagnitude /= static_cast<float>(channels);
    return averageMagnitude + (linkedMagnitude - averageMagnitude) * band.stereoLink;
}

float MultibandProcessor::computeDynamicGainDb(const MultibandBandSettings& band, float detectorLevelDb) const noexcept
{
    if (! band.enabled)
        return 0.0f;

    const auto ratioAmount = 1.0f - (1.0f / juce::jmax(1.0f, band.ratio));
    const auto rangeLimit = std::abs(band.rangeDb);

    if (band.mode == multiband::BandMode::compress)
    {
        const auto overThresholdDb = positiveKneeAmount(detectorLevelDb - band.thresholdDb, band.kneeDb);
        const auto movement = juce::jmin(rangeLimit, overThresholdDb * ratioAmount);
        return band.rangeDb >= 0.0f ? movement : -movement;
    }

    const auto underThresholdDb = positiveKneeAmount(band.thresholdDb - detectorLevelDb, band.kneeDb);
    const auto movement = juce::jmin(rangeLimit, underThresholdDb * ratioAmount);
    return band.rangeDb >= 0.0f ? movement : -movement;
}

void MultibandProcessor::publishAnalyzerSamples(const juce::AudioBuffer<float>& inputBuffer,
                                                const juce::AudioBuffer<float>& outputBuffer,
                                                const juce::AudioBuffer<float>* sidechainBuffer,
                                                int numSamples) noexcept
{
    auto writeIndex = analyzerWriteIndex.load(std::memory_order_relaxed);
    const auto sidechainAvailable = sidechainBuffer != nullptr
                                    && sidechainBuffer->getNumChannels() > 0
                                    && sidechainBuffer->getNumSamples() > 0;

    for (int sample = 0; sample < numSamples; ++sample)
    {
        inputAnalyzerSamples[static_cast<size_t>(writeIndex)].store(monoSampleAt(inputBuffer, sample), std::memory_order_relaxed);
        outputAnalyzerSamples[static_cast<size_t>(writeIndex)].store(monoSampleAt(outputBuffer, sample), std::memory_order_relaxed);
        sidechainAnalyzerSamples[static_cast<size_t>(writeIndex)].store(
            sidechainAvailable ? monoSampleAt(*sidechainBuffer, juce::jmin(sample, sidechainBuffer->getNumSamples() - 1)) : 0.0f,
            std::memory_order_relaxed);

        writeIndex = (writeIndex + 1) % MultibandAnalyzerFrame::sampleCount;
    }

    analyzerSidechainActive.store(sidechainAvailable, std::memory_order_relaxed);
    analyzerWriteIndex.store(writeIndex, std::memory_order_release);
}

float MultibandProcessor::monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept
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
