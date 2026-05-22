#include <JuceHeader.h>

#include "Source/Dsp/MultibandProcessor.h"
#include "Source/Parameters.h"
#include "Source/PluginEditor.h"
#include "Source/PluginProcessor.h"

namespace
{
void expect(bool condition, const juce::String& message, int& failures)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

void expectParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id, int& failures)
{
    expect(state.getParameter(id) != nullptr, "Missing parameter: " + id, failures);
}

void setParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id, float plainValue)
{
    if (auto* parameter = state.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::AudioBuffer<float> makeSineBuffer(int channels, int samples, float frequency, double sampleRate, float amplitude)
{
    juce::AudioBuffer<float> buffer(channels, samples);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
            data[sample] = amplitude * std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(sample) / static_cast<float>(sampleRate));
    }

    return buffer;
}

float rmsLevel(const juce::AudioBuffer<float>& buffer, int startSample = 0)
{
    double energy = 0.0;
    auto count = 0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = startSample; sample < buffer.getNumSamples(); ++sample)
        {
            const auto value = buffer.getSample(channel, sample);
            energy += static_cast<double>(value) * static_cast<double>(value);
            ++count;
        }
    }

    return static_cast<float>(std::sqrt(energy / static_cast<double>(juce::jmax(1, count))));
}

bool buffersAlmostEqual(const juce::AudioBuffer<float>& lhs,
                        const juce::AudioBuffer<float>& rhs,
                        float tolerance)
{
    if (lhs.getNumChannels() != rhs.getNumChannels() || lhs.getNumSamples() != rhs.getNumSamples())
        return false;

    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
            if (std::abs(lhs.getSample(channel, sample) - rhs.getSample(channel, sample)) > tolerance)
                return false;

    return true;
}

juce::Component* findChildComponentWithId(juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int index = 0; index < component.getNumChildComponents(); ++index)
        if (auto* found = findChildComponentWithId(*component.getChildComponent(index), componentId))
            return found;

    return nullptr;
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;

    kratomix::MultibandCompressorAudioProcessor processor;

    for (const auto* id : kratomix::multiband::globalParameterIds)
        expectParameter(processor.parameters, id, failures);

    for (const auto* id : kratomix::multiband::crossoverFrequencyIds)
        expectParameter(processor.parameters, id, failures);

    for (int index = 0; index < kratomix::multiband::maxBands; ++index)
    {
        const auto idx = static_cast<size_t>(index);
        expectParameter(processor.parameters, kratomix::multiband::bandEnabledIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandThresholdIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandRangeIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandRatioIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandAttackIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandReleaseIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandKneeIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandMakeupIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandModeIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandDetectorSourceIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandStereoLinkIds[idx], failures);
    }

    if (auto* band01Enabled = processor.parameters.getRawParameterValue(kratomix::multiband::bandEnabledIds[0]))
        expect(band01Enabled->load() >= 0.5f, "Band 1 should be enabled by default", failures);
    if (auto* band05Enabled = processor.parameters.getRawParameterValue(kratomix::multiband::bandEnabledIds[4]))
        expect(band05Enabled->load() < 0.5f, "Band 5 should be reserved but disabled by default", failures);

    {
        kratomix::MultibandProcessor dsp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 4096;
        spec.numChannels = 2;
        dsp.prepare(spec);

        kratomix::MultibandSettings settings;
        for (auto& band : settings.bands)
            band.enabled = false;
        dsp.updateSettings(settings);

        auto buffer = makeSineBuffer(2, 4096, 1000.0f, spec.sampleRate, 0.25f);
        const auto dryRms = rmsLevel(buffer, 2048);
        dsp.process(buffer);
        const auto wetRms = rmsLevel(buffer, 2048);

        expect(std::abs(juce::Decibels::gainToDecibels(wetRms / dryRms, -120.0f)) < 1.0f,
               "Neutral fixed crossover should preserve steady sine level within 1 dB",
               failures);
    }

    {
        processor.disableNonMainBuses();
        setParameter(processor.parameters, kratomix::multiband::bypassId, 1.0f);
        setParameter(processor.parameters, kratomix::multiband::inputGainId, 12.0f);
        setParameter(processor.parameters, kratomix::multiband::outputGainId, -12.0f);
        processor.prepareToPlay(48000.0, 512);

        auto buffer = makeSineBuffer(2, 512, 1000.0f, 48000.0, 0.25f);
        const auto dry = buffer;
        juce::MidiBuffer midi;
        processor.processBlock(buffer, midi);

        expect(buffersAlmostEqual(buffer, dry, 1.0e-6f),
               "Bypass should leave the main signal unchanged",
               failures);
        setParameter(processor.parameters, kratomix::multiband::bypassId, 0.0f);
    }

    {
        kratomix::MultibandProcessor dsp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        dsp.prepare(spec);

        kratomix::MultibandSettings settings;
        for (auto& band : settings.bands)
            band.enabled = false;
        auto& band = settings.bands[2];
        band.enabled = true;
        band.thresholdDb = -42.0f;
        band.rangeDb = -12.0f;
        band.ratio = 8.0f;
        band.attackMs = 0.1f;
        band.releaseMs = 50.0f;
        dsp.updateSettings(settings);

        auto dry = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.35f);
        auto processed = dry;

        for (int block = 0; block < 16; ++block)
        {
            processed = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.35f);
            dsp.process(processed);
        }

        expect(rmsLevel(processed) < rmsLevel(dry) * 0.88f,
               "Compression should reduce a band above threshold",
               failures);
    }

    {
        kratomix::MultibandProcessor dsp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        dsp.prepare(spec);

        kratomix::MultibandSettings settings;
        for (auto& band : settings.bands)
            band.enabled = false;
        auto& band = settings.bands[2];
        band.enabled = true;
        band.mode = kratomix::multiband::BandMode::expand;
        band.thresholdDb = -20.0f;
        band.rangeDb = 10.0f;
        band.ratio = 4.0f;
        band.attackMs = 0.1f;
        band.releaseMs = 80.0f;
        dsp.updateSettings(settings);

        auto dry = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.01f);
        auto processed = dry;

        for (int block = 0; block < 16; ++block)
        {
            processed = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.01f);
            dsp.process(processed);
        }

        expect(rmsLevel(processed) > rmsLevel(dry) * 1.15f,
               "Positive expand range should lift signal below threshold",
               failures);
    }

    {
        kratomix::MultibandProcessor dsp;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 512;
        spec.numChannels = 2;
        dsp.prepare(spec);

        kratomix::MultibandSettings settings;
        for (auto& band : settings.bands)
            band.enabled = false;
        auto& band = settings.bands[2];
        band.enabled = true;
        band.detectorSource = kratomix::multiband::DetectorSource::external;
        band.thresholdDb = -40.0f;
        band.rangeDb = -12.0f;
        band.ratio = 8.0f;
        band.attackMs = 0.1f;
        band.releaseMs = 80.0f;
        dsp.updateSettings(settings);

        auto quietMain = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.01f);
        auto noSidechain = quietMain;
        dsp.process(noSidechain, nullptr);

        dsp.reset();
        dsp.updateSettings(settings);
        auto withSidechain = quietMain;
        auto loudSidechain = makeSineBuffer(2, 512, 1000.0f, spec.sampleRate, 0.45f);
        for (int block = 0; block < 12; ++block)
        {
            withSidechain = quietMain;
            dsp.process(withSidechain, &loudSidechain);
        }

        expect(rmsLevel(withSidechain) < rmsLevel(noSidechain) * 0.9f,
               "External sidechain should drive gain reduction when selected",
               failures);
    }

    {
        kratomix::MultibandCompressorAudioProcessor latencyProcessor;
        latencyProcessor.disableNonMainBuses();
        setParameter(latencyProcessor.parameters, kratomix::multiband::lookaheadModeId, 1.0f);
        latencyProcessor.prepareToPlay(48000.0, 512);
        auto buffer = makeSineBuffer(2, 512, 1000.0f, 48000.0, 0.1f);
        juce::MidiBuffer midi;
        latencyProcessor.processBlock(buffer, midi);

        expect(latencyProcessor.getLatencySamples() == 240,
               "5 ms lookahead should report 240 samples at 48 kHz",
               failures);
    }

    {
        kratomix::MultibandCompressorAudioProcessor editorProcessor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(editorProcessor.createEditor());
        expect(editor != nullptr,
               "Editor should be constructible",
               failures);
        expect(editor != nullptr && findChildComponentWithId(*editor, "multibandGraph") != nullptr,
               "Editor should expose the graph component",
               failures);
        expect(editor != nullptr && findChildComponentWithId(*editor, "threshold") != nullptr,
               "Editor should expose the selected-band threshold control",
               failures);
    }

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    return 1;
}
