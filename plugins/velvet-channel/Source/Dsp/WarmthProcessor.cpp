#include "WarmthProcessor.h"

#include "ControlValues.h"

namespace kratomix
{
namespace
{
    constexpr int filterUpdateInterval = 16;

    float decibelsToGain(float db) noexcept
    {
        return juce::Decibels::decibelsToGain(db);
    }

    float shelfQ() noexcept
    {
        return 0.7071f;
    }
}

void WarmthProcessor::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    dryBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));

    highPass.prepare(spec);
    lowShelf.prepare(spec);
    presence.prepare(spec);
    highShelf.prepare(spec);

    inputGainDbSmoother.reset(sampleRate, 0.02);
    driveSmoother.reset(sampleRate, 0.02);
    highPassSmoother.reset(sampleRate, 0.04);
    warmthSmoother.reset(sampleRate, 0.04);
    presenceSmoother.reset(sampleRate, 0.04);
    airSmoother.reset(sampleRate, 0.04);
    outputGainDbSmoother.reset(sampleRate, 0.02);
    wetMixSmoother.reset(sampleRate, 0.01);

    meterReleasePerSample = std::exp(std::log(0.2f) / static_cast<float>(sampleRate * 0.35));
    meterEnvelope = 0.0f;
    vuLevel.store(0.0f);

    reset();
    updateSettings(settings);
    applyTargetsImmediately();
}

void WarmthProcessor::reset()
{
    highPass.reset();
    lowShelf.reset();
    presence.reset();
    highShelf.reset();

    hasProcessedAudio = false;
    meterEnvelope = 0.0f;
    vuLevel.store(0.0f);
}

void WarmthProcessor::updateSettings(const WarmthSettings& newSettings)
{
    settings = newSettings;
    settings.drive = controls::snapDrive(newSettings.drive);
    settings.highPassHz = controls::snapHighPass(newSettings.highPassHz);
    settings.warmthDb = controls::snapWarmth(newSettings.warmthDb);
    settings.presenceDb = controls::snapPresence(newSettings.presenceDb);
    settings.airDb = controls::snapAir(newSettings.airDb);

    inputGainDbSmoother.setTargetValue(settings.inputGainDb);
    driveSmoother.setTargetValue(settings.drive);
    highPassSmoother.setTargetValue(settings.highPassHz);
    warmthSmoother.setTargetValue(settings.warmthDb);
    presenceSmoother.setTargetValue(settings.presenceDb);
    airSmoother.setTargetValue(settings.airDb);
    outputGainDbSmoother.setTargetValue(settings.outputGainDb);
    wetMixSmoother.setTargetValue(settings.bypassed ? 0.0f : 1.0f);

    if (!hasProcessedAudio)
        applyTargetsImmediately();
}

void WarmthProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0)
        return;

    jassert(buffer.getNumSamples() <= dryBuffer.getNumSamples());
    jassert(buffer.getNumChannels() <= dryBuffer.getNumChannels());

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        dryBuffer.copyFrom(channel, 0, buffer, channel, 0, buffer.getNumSamples());

    hasProcessedAudio = true;
    double energy = 0.0;

    for (int startSample = 0; startSample < buffer.getNumSamples(); startSample += filterUpdateInterval)
    {
        const auto chunkSize = std::min(filterUpdateInterval, buffer.getNumSamples() - startSample);

        const auto highPassHz = highPassSmoother.getNextValue();
        const auto warmthDb = warmthSmoother.getNextValue();
        const auto presenceDb = presenceSmoother.getNextValue();
        const auto airDb = airSmoother.getNextValue();

        if (chunkSize > 1)
        {
            highPassSmoother.skip(chunkSize - 1);
            warmthSmoother.skip(chunkSize - 1);
            presenceSmoother.skip(chunkSize - 1);
            airSmoother.skip(chunkSize - 1);
        }

        updateFilterCoefficients(highPassHz, warmthDb, presenceDb, airDb);

        for (int sample = 0; sample < chunkSize; ++sample)
        {
            const auto absoluteSample = startSample + sample;
            const auto inputGain = decibelsToGain(inputGainDbSmoother.getNextValue());
            const auto driveAmount = driveSmoother.getNextValue();

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* samples = buffer.getWritePointer(channel);
                samples[absoluteSample] = saturateSample(samples[absoluteSample] * inputGain, driveAmount);
            }
        }

        juce::dsp::AudioBlock<float> block { buffer };
        auto subBlock = block.getSubBlock(static_cast<size_t>(startSample), static_cast<size_t>(chunkSize));
        juce::dsp::ProcessContextReplacing<float> context { subBlock };

        highPass.process(context);
        lowShelf.process(context);
        presence.process(context);
        highShelf.process(context);

        for (int sample = 0; sample < chunkSize; ++sample)
        {
            const auto absoluteSample = startSample + sample;
            const auto outputGain = decibelsToGain(outputGainDbSmoother.getNextValue());
            const auto wetMix = wetMixSmoother.getNextValue();

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            {
                auto* processedSamples = buffer.getWritePointer(channel);
                const auto* drySamples = dryBuffer.getReadPointer(channel);
                const auto processed = processedSamples[absoluteSample] * outputGain;
                const auto mixed = drySamples[absoluteSample] + (processed - drySamples[absoluteSample]) * wetMix;
                processedSamples[absoluteSample] = mixed;
                energy += static_cast<double>(mixed) * static_cast<double>(mixed);
            }
        }
    }

    const auto blockRms = std::sqrt(energy / static_cast<double>(buffer.getNumSamples() * buffer.getNumChannels()));
    const auto release = std::pow(meterReleasePerSample, static_cast<float>(buffer.getNumSamples()));

    if (blockRms > meterEnvelope)
        meterEnvelope = static_cast<float>(blockRms);
    else
        meterEnvelope *= release;

    vuLevel.store(meterEnvelope);
}

float WarmthProcessor::saturateSample(float sample, float driveAmount) noexcept
{
    const auto normalizedDrive = driveAmount / 10.0f;
    const auto driveGain = 1.0f + normalizedDrive * 6.5f;
    const auto asymmetry = 0.08f * normalizedDrive;
    const auto shaped = std::tanh((sample + asymmetry) * driveGain) / std::tanh(driveGain);

    const auto evenHarmonic = 0.045f * normalizedDrive * shaped * shaped;
    return juce::jlimit(-1.0f, 1.0f, shaped + evenHarmonic);
}

float WarmthProcessor::getVuLevel() const noexcept
{
    return vuLevel.load();
}

void WarmthProcessor::applyTargetsImmediately()
{
    inputGainDbSmoother.setCurrentAndTargetValue(settings.inputGainDb);
    driveSmoother.setCurrentAndTargetValue(settings.drive);
    highPassSmoother.setCurrentAndTargetValue(settings.highPassHz);
    warmthSmoother.setCurrentAndTargetValue(settings.warmthDb);
    presenceSmoother.setCurrentAndTargetValue(settings.presenceDb);
    airSmoother.setCurrentAndTargetValue(settings.airDb);
    outputGainDbSmoother.setCurrentAndTargetValue(settings.outputGainDb);
    wetMixSmoother.setCurrentAndTargetValue(settings.bypassed ? 0.0f : 1.0f);
    updateFilterCoefficients(settings.highPassHz, settings.warmthDb, settings.presenceDb, settings.airDb);
}

void WarmthProcessor::updateFilterCoefficients(float highPassHz, float warmthDb, float presenceDb, float airDb)
{
    const auto cutoff = juce::jlimit(20.0f, 180.0f, highPassHz);
    *highPass.state = *Coefficients::makeHighPass(sampleRate, cutoff, 0.7071f);

    *lowShelf.state = *Coefficients::makeLowShelf(
        sampleRate,
        110.0f,
        shelfQ(),
        decibelsToGain(warmthDb));

    *presence.state = *Coefficients::makePeakFilter(
        sampleRate,
        2800.0f,
        0.9f,
        decibelsToGain(presenceDb));

    *highShelf.state = *Coefficients::makeHighShelf(
        sampleRate,
        11000.0f,
        shelfQ(),
        decibelsToGain(airDb));
}
}
