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
        expectParameter(processor.parameters, kratomix::multiband::bandFrequencyIds[idx], failures);
        expectParameter(processor.parameters, kratomix::multiband::bandWidthIds[idx], failures);
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
        expect(band01Enabled->load() < 0.5f, "A new instance should start without processing bands", failures);
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
        const auto original = buffer;
        const auto dryRms = rmsLevel(buffer, 2048);
        dsp.process(buffer);
        const auto wetRms = rmsLevel(buffer, 2048);

        expect(std::abs(juce::Decibels::gainToDecibels(wetRms / dryRms, -120.0f)) < 1.0f,
               "An instance without active bands should preserve steady sine level within 1 dB",
               failures);
        expect(buffersAlmostEqual(buffer, original, 1.0e-6f),
               "Inactive free bands should leave every sample unchanged",
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
        band.frequencyHz = 1000.0f;
        band.widthOctaves = 1.5f;
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

        dsp.reset();
        dsp.updateSettings(settings);
        auto lowDry = makeSineBuffer(2, 512, 100.0f, spec.sampleRate, 0.35f);
        auto lowProcessed = lowDry;
        for (int block = 0; block < 16; ++block)
        {
            lowProcessed = makeSineBuffer(2, 512, 100.0f, spec.sampleRate, 0.35f);
            dsp.process(lowProcessed);
        }

        const auto lowDifferenceDb = juce::Decibels::gainToDecibels(rmsLevel(lowProcessed) / rmsLevel(lowDry), -120.0f);
        expect(std::abs(lowDifferenceDb) < 1.0f,
               "Compression around 1 kHz should leave a 100 Hz tone essentially untouched",
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
        band.frequencyHz = 1000.0f;
        band.widthOctaves = 1.5f;
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
        band.frequencyHz = 1000.0f;
        band.widthOctaves = 1.5f;
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
        auto* frequencyControl = editor != nullptr ? findChildComponentWithId(*editor, "frequency") : nullptr;
        expect(frequencyControl != nullptr && ! frequencyControl->isEnabled(),
               "Band controls should wait for the user to create or select a band",
               failures);
    }

    {
        kratomix::MultibandCompressorAudioProcessor graphProcessor;
        kratomix::multiband::MultibandGraph graph;
        graph.attachState(graphProcessor.parameters);
        graph.setBounds(0, 0, 900, 420);

        expect(graph.getSelectedBand() < 0,
               "An empty graph should start without a selected band",
               failures);
        expect(graph.createBandAt({ 450.0f, 210.0f }),
               "Double-click workflow should be able to create a free dynamic band",
               failures);
        expect(graph.getSelectedBand() == 0,
               "A newly created band should be selected",
               failures);
        expect(graphProcessor.parameters.getRawParameterValue(kratomix::multiband::bandEnabledIds[0])->load() >= 0.5f,
               "Creating a graph band should enable an available band slot",
               failures);

        graph.setSelectedBandFrequency(2400.0f);
        graph.setSelectedBandWidth(1.25f);
        expect(std::abs(graphProcessor.parameters.getRawParameterValue(kratomix::multiband::bandFrequencyIds[0])->load() - 2400.0f) < 2.0f,
               "Dragging a free band should update its centre frequency",
               failures);
        expect(std::abs(graphProcessor.parameters.getRawParameterValue(kratomix::multiband::bandWidthIds[0])->load() - 1.25f) < 0.02f,
               "Band edge dragging or wheel input should update band width",
               failures);
        expect(graph.boundsForBand(0).getWidth() > 20.0f,
               "A free band should expose a visible frequency range",
               failures);
        expect(graph.createBandAt({ 680.0f, 210.0f }) && graph.getSelectedBand() == 1,
               "Additional dynamic bands should be created in the next free slot",
               failures);
        expect(graph.deleteSelectedBand(),
               "The selected dynamic band should be removable",
               failures);
    }

    {
        kratomix::MultibandCompressorAudioProcessor sourceProcessor;
        setParameter(sourceProcessor.parameters, kratomix::multiband::bandEnabledIds[0], 1.0f);
        setParameter(sourceProcessor.parameters, kratomix::multiband::bandFrequencyIds[0], 3210.0f);
        setParameter(sourceProcessor.parameters, kratomix::multiband::bandWidthIds[0], 1.75f);
        setParameter(sourceProcessor.parameters, kratomix::multiband::bandThresholdIds[0], -31.0f);

        juce::MemoryBlock savedState;
        sourceProcessor.getStateInformation(savedState);

        kratomix::MultibandCompressorAudioProcessor restoredProcessor;
        restoredProcessor.setStateInformation(savedState.getData(), static_cast<int>(savedState.getSize()));
        std::unique_ptr<juce::AudioProcessorEditor> restoredEditor(restoredProcessor.createEditor());
        auto* frequency = dynamic_cast<juce::Slider*>(findChildComponentWithId(*restoredEditor, "frequency"));
        auto* width = dynamic_cast<juce::Slider*>(findChildComponentWithId(*restoredEditor, "width"));

        expect(std::abs(restoredProcessor.parameters.getRawParameterValue(kratomix::multiband::bandFrequencyIds[0])->load() - 3210.0f) < 2.0f,
               "Free-band centre frequency should survive state restoration",
               failures);
        expect(frequency != nullptr && frequency->isEnabled() && std::abs(frequency->getValue() - 3210.0) < 2.0,
               "Reopened editor should select the restored active band and show its frequency",
               failures);
        expect(width != nullptr && width->isEnabled() && std::abs(width->getValue() - 1.75) < 0.02,
               "Reopened editor should show the restored band width",
               failures);
        expect(frequency != nullptr && frequency->getParentComponent() != nullptr
                   && frequency->getParentComponent()->getLocalBounds().contains(frequency->getBounds()),
               "Frequency control should fit inside the selected-band panel",
               failures);
        expect(width != nullptr && width->getParentComponent() != nullptr
                   && width->getParentComponent()->getLocalBounds().contains(width->getBounds()),
               "Width control should fit inside the selected-band panel",
               failures);
    }

    {
        kratomix::MultibandCompressorAudioProcessor legacySource;
        setParameter(legacySource.parameters, kratomix::multiband::crossoverFrequencyIds[0], 200.0f);
        setParameter(legacySource.parameters, kratomix::multiband::bandEnabledIds[0], 1.0f);
        auto legacyState = legacySource.parameters.copyState();

        for (int childIndex = legacyState.getNumChildren() - 1; childIndex >= 0; --childIndex)
        {
            const auto child = legacyState.getChild(childIndex);
            const auto id = child.getProperty("id").toString();
            if (id.startsWith("band") && (id.endsWith("Frequency") || id.endsWith("Width")))
                legacyState.removeChild(childIndex, nullptr);
        }

        juce::MemoryBlock legacyData;
        std::unique_ptr<juce::XmlElement> legacyXml(legacyState.createXml());
        juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyData);

        kratomix::MultibandCompressorAudioProcessor migratedProcessor;
        migratedProcessor.setStateInformation(legacyData.getData(), static_cast<int>(legacyData.getSize()));
        const auto migratedFrequency = migratedProcessor.parameters.getRawParameterValue(kratomix::multiband::bandFrequencyIds[0])->load();
        const auto migratedWidth = migratedProcessor.parameters.getRawParameterValue(kratomix::multiband::bandWidthIds[0])->load();

        expect(std::abs(migratedFrequency - std::sqrt(20.0f * 200.0f)) < 2.0f,
               "Legacy crossover state should migrate to a free-band centre frequency",
               failures);
        expect(std::abs(migratedWidth - std::log2(200.0f / 20.0f)) < 0.03f,
               "Legacy crossover state should migrate to a free-band octave width",
               failures);
    }

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    return 1;
}
