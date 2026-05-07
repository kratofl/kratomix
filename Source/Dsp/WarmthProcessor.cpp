#include "WarmthProcessor.h"

namespace kratomix
{
namespace
{
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

    highPass.prepare(spec);
    lowShelf.prepare(spec);
    presence.prepare(spec);
    highShelf.prepare(spec);

    reset();
    updateSettings(settings);
}

void WarmthProcessor::reset()
{
    highPass.reset();
    lowShelf.reset();
    presence.reset();
    highShelf.reset();
}

void WarmthProcessor::updateSettings(const WarmthSettings& newSettings)
{
    settings = newSettings;

    const auto highPassHz = juce::jlimit(20.0f, 180.0f, settings.highPassHz);
    *highPass.state = *Coefficients::makeHighPass(sampleRate, highPassHz, 0.7071f);

    *lowShelf.state = *Coefficients::makeLowShelf(
        sampleRate,
        120.0f,
        shelfQ(),
        decibelsToGain(settings.warmthDb));

    *presence.state = *Coefficients::makePeakFilter(
        sampleRate,
        3200.0f,
        0.8f,
        decibelsToGain(settings.presenceDb));

    *highShelf.state = *Coefficients::makeHighShelf(
        sampleRate,
        12000.0f,
        shelfQ(),
        decibelsToGain(settings.airDb));
}

void WarmthProcessor::process(juce::AudioBuffer<float>& buffer)
{
    if (buffer.getNumSamples() == 0)
        return;

    const auto inputGain = decibelsToGain(settings.inputGainDb);
    const auto outputGain = decibelsToGain(settings.outputGainDb);
    const auto driveAmount = juce::jlimit(0.0f, 10.0f, settings.drive);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);

        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            samples[sample] = saturateSample(samples[sample] * inputGain, driveAmount);
    }

    juce::dsp::AudioBlock<float> block { buffer };
    juce::dsp::ProcessContextReplacing<float> context { block };

    highPass.process(context);
    lowShelf.process(context);
    presence.process(context);
    highShelf.process(context);

    buffer.applyGain(outputGain);
}

float WarmthProcessor::saturateSample(float sample, float driveAmount) noexcept
{
    const auto normalizedDrive = driveAmount / 10.0f;
    const auto gain = 1.0f + normalizedDrive * 5.0f;
    const auto shaped = std::tanh(sample * gain) / std::tanh(gain);

    // Mild even-order bias keeps the color warm without turning the processor into an obvious distortion.
    const auto evenHarmonic = 0.035f * normalizedDrive * shaped * shaped;
    return juce::jlimit(-1.0f, 1.0f, shaped + evenHarmonic);
}
}
