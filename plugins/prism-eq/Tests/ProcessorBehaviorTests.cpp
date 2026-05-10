#include <JuceHeader.h>

#include "Source/Parameters.h"
#include "Source/Dsp/PrismProcessor.h"
#include "Source/PluginProcessor.h"
#include "Source/Ui/AnalyzerFeatures.h"
#include "Source/Ui/PrismGraph.h"

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

juce::AudioBuffer<float> makeRampBuffer(int channels, int samples)
{
    juce::AudioBuffer<float> buffer(channels, samples);

    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, static_cast<float>(sample + 1) * 0.001f);

    return buffer;
}

juce::AudioBuffer<float> makeSineBuffer(int channels, int samples, float frequency, double sampleRate)
{
    juce::AudioBuffer<float> buffer(channels, samples);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* data = buffer.getWritePointer(channel);

        for (int sample = 0; sample < samples; ++sample)
            data[sample] = 0.1f * std::sin(juce::MathConstants<float>::twoPi * frequency * static_cast<float>(sample) / static_cast<float>(sampleRate));
    }

    return buffer;
}

float rmsLevel(const juce::AudioBuffer<float>& buffer)
{
    double energy = 0.0;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            energy += static_cast<double>(buffer.getSample(channel, sample)) * static_cast<double>(buffer.getSample(channel, sample));

    const auto divisor = static_cast<double>(juce::jmax(1, buffer.getNumChannels() * buffer.getNumSamples()));
    return static_cast<float>(std::sqrt(energy / divisor));
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

void disableSidechainForProcessorTest(kratomix::PrismEqAudioProcessor& processor)
{
    processor.disableNonMainBuses();
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;

    kratomix::PrismEqAudioProcessor processor;

    for (const auto* id : kratomix::prism::globalParameterIds())
        expectParameter(processor.parameters, id, failures);

    expectParameter(processor.parameters, "phaseMode", failures);
    if (auto* phaseMode = processor.parameters.getRawParameterValue("phaseMode"))
        expect(std::abs(phaseMode->load()) < 1.0e-6f,
               "Phase mode should default to zero latency",
               failures);
    if (auto* analyzerMode = processor.parameters.getRawParameterValue("analyzerMode"))
        expect(std::abs(analyzerMode->load() - 4.0f) < 1.0e-6f,
               "Analyzer mode should default to Pre + Post + Sidechain",
               failures);
    expect(kratomix::prism::analyzerModeChoices().contains("Masking"),
           "Analyzer modes should include sidechain masking",
           failures);
    if (auto* frequencyParameter = processor.parameters.getParameter("band01Frequency"))
    {
        const auto displayText = frequencyParameter->getText(frequencyParameter->convertTo0to1(1936.45f), 16);
        expect(! displayText.contains("."),
               "Frequency parameter display should not include decimal places",
               failures);
    }

    for (int index = 1; index <= kratomix::prism::maxBands; ++index)
    {
        const auto prefix = kratomix::prism::bandPrefix(index);
        expectParameter(processor.parameters, prefix + "Enabled", failures);
        expectParameter(processor.parameters, prefix + "Type", failures);
        expectParameter(processor.parameters, prefix + "Frequency", failures);
        expectParameter(processor.parameters, prefix + "Gain", failures);
        expectParameter(processor.parameters, prefix + "Q", failures);
        expectParameter(processor.parameters, prefix + "DynamicEnabled", failures);
        expectParameter(processor.parameters, prefix + "DynamicRange", failures);
        expectParameter(processor.parameters, prefix + "Threshold", failures);
        expectParameter(processor.parameters, prefix + "Attack", failures);
        expectParameter(processor.parameters, prefix + "Release", failures);
        expectParameter(processor.parameters, prefix + "SidechainSource", failures);
        expectParameter(processor.parameters, prefix + "Solo", failures);
    }

    expect(processor.parameters.getRawParameterValue("band01Enabled")->load() < 0.5f,
           "Band 01 should be disabled by default",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("inputGain")->load()) < 1.0e-6f,
           "Input gain should default to neutral",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("outputGain")->load()) < 1.0e-6f,
           "Output gain should default to neutral",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("mix")->load() - 1.0f) < 1.0e-6f,
           "Mix should default to fully wet",
           failures);

    disableSidechainForProcessorTest(processor);
    processor.prepareToPlay(48000.0, 128);
    juce::MidiBuffer midi;
    auto buffer = makeRampBuffer(2, 128);
    const auto before = buffer;
    processor.processBlock(buffer, midi);

    expect(buffersAlmostEqual(buffer, before, 1.0e-6f),
           "Default Prism processing should be neutral with all bands disabled",
           failures);
    expect(processor.getOutputLevel() > 0.0f,
           "Output meter should react to processed audio",
           failures);

    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    expect(editor != nullptr,
           "Prism editor should be constructible",
           failures);
    expect(editor != nullptr && editor->getWidth() >= 900,
           "Prism editor should be graph-led and wide",
           failures);
    expect(editor != nullptr && editor->getHeight() >= 520,
           "Prism editor should leave room for graph and controls",
           failures);

    {
        kratomix::prism::PrismGraph graph;
        graph.attachState(processor.parameters);
        graph.setBounds(0, 0, 900, 460);

        const auto created = graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));
        expect(created,
               "Graph should create a band at the requested point",
               failures);
        expect(processor.parameters.getRawParameterValue("band01Enabled")->load() > 0.5f,
               "Graph-created band should enable the first inactive band",
               failures);
        expect(std::abs(processor.parameters.getRawParameterValue("band01Frequency")->load() - 1000.0f) < 5.0f,
               "Graph-created band should store the clicked frequency",
               failures);
        expect(std::abs(processor.parameters.getRawParameterValue("band01Gain")->load() - 6.0f) < 0.2f,
               "Graph-created band should store the clicked gain",
               failures);

        graph.dragSelectedBandTo(graph.pointForFrequencyAndGain(2000.0f, -3.0f));
        expect(std::abs(processor.parameters.getRawParameterValue("band01Frequency")->load() - 2000.0f) < 10.0f,
               "Dragging a selected node should update frequency",
               failures);
        expect(std::abs(processor.parameters.getRawParameterValue("band01Gain")->load() + 3.0f) < 0.2f,
               "Dragging a selected node should update gain",
               failures);

        const auto clicked = graph.selectBandAt(graph.pointForFrequencyAndGain(500.0f, 4.0f));
        expect(! clicked,
               "Clicking empty graph space should not create a band",
               failures);
        expect(processor.parameters.getRawParameterValue("band02Enabled")->load() < 0.5f,
               "Empty-space clicks should leave inactive band slots disabled",
               failures);

        const auto doubleClicked = graph.createBandAt(graph.pointForFrequencyAndGain(500.0f, 4.0f));
        expect(doubleClicked,
               "Double-click graph action should create a selectable band",
               failures);
        expect(processor.parameters.getRawParameterValue("band02Enabled")->load() > 0.5f,
               "Double-click-created band should use the next inactive slot",
               failures);
        expect(std::abs(processor.parameters.getRawParameterValue("band02Frequency")->load() - 500.0f) < 5.0f,
               "Double-click-created band should store the clicked frequency",
               failures);

        expect(graph.keyPressed(juce::KeyPress(juce::KeyPress::deleteKey)),
               "Delete key should be handled when a graph band is selected",
               failures);
        expect(processor.parameters.getRawParameterValue("band02Enabled")->load() < 0.5f,
               "Delete key should remove the selected graph band",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor eqProcessor;
        disableSidechainForProcessorTest(eqProcessor);
        eqProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(eqProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        auto eqBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(eqBuffer);
        juce::MidiBuffer eqMidi;
        eqProcessor.processBlock(eqBuffer, eqMidi);
        const auto wetRms = rmsLevel(eqBuffer);

        expect(wetRms > dryRms * 1.45f,
               "A graph-created +6 dB bell band should audibly boost matching audio",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor analyzerProcessor;
        disableSidechainForProcessorTest(analyzerProcessor);
        analyzerProcessor.prepareToPlay(48000.0, 512);

        auto analyzerBuffer = makeSineBuffer(2, 512, 1000.0f, 48000.0);
        juce::MidiBuffer analyzerMidi;
        analyzerProcessor.processBlock(analyzerBuffer, analyzerMidi);

        kratomix::PrismAnalyzerFrame frame;
        analyzerProcessor.copyAnalyzerFrame(frame);

        auto hasPreSignal = false;
        auto hasPostSignal = false;

        for (int sample = 0; sample < kratomix::PrismAnalyzerFrame::sampleCount; ++sample)
        {
            hasPreSignal = hasPreSignal || std::abs(frame.pre[static_cast<size_t>(sample)]) > 1.0e-5f;
            hasPostSignal = hasPostSignal || std::abs(frame.post[static_cast<size_t>(sample)]) > 1.0e-5f;
        }

        expect(hasPreSignal,
               "Analyzer frame should expose recent pre-EQ audio for the graph",
               failures);
        expect(hasPostSignal,
               "Analyzer frame should expose recent post-EQ audio for the graph",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor dynamicProcessor;
        disableSidechainForProcessorTest(dynamicProcessor);
        dynamicProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(dynamicProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 0.0f));

        const auto prefix = kratomix::prism::bandPrefix(1);
        dynamicProcessor.parameters.getParameter(prefix + "DynamicEnabled")->setValueNotifyingHost(1.0f);
        dynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->setValueNotifyingHost(
            dynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->convertTo0to1(12.0f));
        dynamicProcessor.parameters.getParameter(prefix + "Threshold")->setValueNotifyingHost(
            dynamicProcessor.parameters.getParameter(prefix + "Threshold")->convertTo0to1(-48.0f));
        dynamicProcessor.parameters.getParameter(prefix + "Attack")->setValueNotifyingHost(
            dynamicProcessor.parameters.getParameter(prefix + "Attack")->convertTo0to1(0.1f));
        dynamicProcessor.parameters.getParameter(prefix + "Release")->setValueNotifyingHost(
            dynamicProcessor.parameters.getParameter(prefix + "Release")->convertTo0to1(50.0f));

        auto dynamicBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(dynamicBuffer);
        juce::MidiBuffer dynamicMidi;
        dynamicProcessor.processBlock(dynamicBuffer, dynamicMidi);
        dynamicProcessor.processBlock(dynamicBuffer, dynamicMidi);
        const auto dynamicRms = rmsLevel(dynamicBuffer);

        expect(dynamicRms < dryRms * 0.82f,
               "A dynamic range band should reduce matching audio when above threshold",
               failures);

        kratomix::PrismAnalyzerFrame dynamicFrame;
        dynamicProcessor.copyAnalyzerFrame(dynamicFrame);
        expect(dynamicFrame.dynamicGainDb[0] < -3.0f,
               "Analyzer frame should expose current dynamic gain so the graph can show moving dynamic bands",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor positiveRangeProcessor;
        disableSidechainForProcessorTest(positiveRangeProcessor);
        positiveRangeProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(positiveRangeProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 0.0f));

        const auto prefix = kratomix::prism::bandPrefix(1);
        positiveRangeProcessor.parameters.getParameter(prefix + "DynamicEnabled")->setValueNotifyingHost(1.0f);
        positiveRangeProcessor.parameters.getParameter(prefix + "DynamicRange")->setValueNotifyingHost(
            positiveRangeProcessor.parameters.getParameter(prefix + "DynamicRange")->convertTo0to1(12.0f));
        positiveRangeProcessor.parameters.getParameter(prefix + "Threshold")->setValueNotifyingHost(
            positiveRangeProcessor.parameters.getParameter(prefix + "Threshold")->convertTo0to1(-48.0f));
        positiveRangeProcessor.parameters.getParameter(prefix + "Attack")->setValueNotifyingHost(
            positiveRangeProcessor.parameters.getParameter(prefix + "Attack")->convertTo0to1(0.1f));

        auto dynamicBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(dynamicBuffer);
        juce::MidiBuffer dynamicMidi;
        positiveRangeProcessor.processBlock(dynamicBuffer, dynamicMidi);
        positiveRangeProcessor.processBlock(dynamicBuffer, dynamicMidi);
        const auto dynamicRms = rmsLevel(dynamicBuffer);

        expect(dynamicRms < dryRms * 0.82f,
               "A positive dynamic range value should reduce matching peaks in Dynamic mode",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor zeroLatencyProcessor;
        disableSidechainForProcessorTest(zeroLatencyProcessor);
        zeroLatencyProcessor.prepareToPlay(48000.0, 2048);

        kratomix::PrismEqAudioProcessor naturalProcessor;
        disableSidechainForProcessorTest(naturalProcessor);
        naturalProcessor.prepareToPlay(48000.0, 2048);

        for (auto* testProcessor : { &zeroLatencyProcessor, &naturalProcessor })
        {
            kratomix::prism::PrismGraph graph;
            graph.attachState(testProcessor->parameters);
            graph.setBounds(0, 0, 900, 460);
            graph.createBandAt(graph.pointForFrequencyAndGain(1200.0f, 6.0f));
        }

        naturalProcessor.parameters.getParameter("phaseMode")->setValueNotifyingHost(
            naturalProcessor.parameters.getParameter("phaseMode")->convertTo0to1(1.0f));

        auto zeroLatencyBuffer = makeSineBuffer(2, 2048, 1200.0f, 48000.0);
        auto naturalBuffer = zeroLatencyBuffer;
        juce::MidiBuffer phaseMidi;
        zeroLatencyProcessor.processBlock(zeroLatencyBuffer, phaseMidi);
        naturalProcessor.processBlock(naturalBuffer, phaseMidi);

        expect(! buffersAlmostEqual(zeroLatencyBuffer, naturalBuffer, 1.0e-5f),
               "Natural phase mode should change the processed waveform compared with zero latency mode",
               failures);
    }

    {
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 2048;
        spec.numChannels = 2;

        kratomix::PrismSettings settings;
        auto& band = settings.bands[0];
        band.enabled = true;
        band.frequency = 1000.0f;
        band.gainDb = 0.0f;
        band.q = 4.0f;
        band.dynamicEnabled = true;
        band.dynamicRangeDb = 12.0f;
        band.thresholdDb = -48.0f;
        band.attackMs = 0.1f;
        band.releaseMs = 50.0f;
        band.sidechainSource = kratomix::prism::SidechainSource::external;

        auto guitarWithLowSidechain = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        auto guitarWithMatchingSidechain = guitarWithLowSidechain;
        const auto dryRms = rmsLevel(guitarWithLowSidechain);
        auto lowSidechain = makeSineBuffer(2, 2048, 200.0f, 48000.0);
        auto matchingSidechain = makeSineBuffer(2, 2048, 1000.0f, 48000.0);

        kratomix::PrismProcessor lowDetectorProcessor;
        lowDetectorProcessor.prepare(spec);
        lowDetectorProcessor.updateSettings(settings);
        lowDetectorProcessor.process(guitarWithLowSidechain, &lowSidechain);
        lowDetectorProcessor.process(guitarWithLowSidechain, &lowSidechain);

        kratomix::PrismProcessor matchingDetectorProcessor;
        matchingDetectorProcessor.prepare(spec);
        matchingDetectorProcessor.updateSettings(settings);
        matchingDetectorProcessor.process(guitarWithMatchingSidechain, &matchingSidechain);
        matchingDetectorProcessor.process(guitarWithMatchingSidechain, &matchingSidechain);

        const auto lowSidechainRms = rmsLevel(guitarWithLowSidechain);
        const auto matchingSidechainRms = rmsLevel(guitarWithMatchingSidechain);

        expect(lowSidechainRms > dryRms * 0.94f,
               "External sidechain energy away from the selected band should not strongly duck that guitar band",
               failures);
        expect(matchingSidechainRms < lowSidechainRms * 0.86f,
               "External sidechain energy near the selected band should duck that guitar frequency more strongly",
               failures);
    }

    {
        std::array<float, 8> post { -90.0f, -58.0f, -24.0f, -18.0f, -23.0f, -70.0f, -80.0f, -82.0f };
        std::array<float, 8> side { -92.0f, -61.0f, -28.0f, -19.0f, -27.0f, -71.0f, -80.0f, -82.0f };

        const auto mask = kratomix::prism::computeMaskingBins(post, side, -60.0f, 8.0f);
        expect(mask[3] > 0.9f,
               "Masking helper should mark strong overlapping bins",
               failures);
        expect(mask[0] < 0.01f,
               "Masking helper should ignore noise floor bins",
               failures);

        const auto peak = kratomix::prism::findNearestAnalyzerPeak(post, 2, -60.0f);
        expect(peak.has_value() && peak->index == 3,
               "Peak helper should find the nearest local maximum",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor selectionProcessor;

        expect(selectionProcessor.getBusCount(true) >= 2,
               "Prism EQ should expose an optional sidechain input bus",
               failures);
        expect(selectionProcessor.getBus(true, 1) != nullptr && selectionProcessor.getBus(true, 1)->isEnabled(),
               "Prism EQ sidechain bus should be enabled by default so Logic exposes the host sidechain selector",
               failures);

        kratomix::prism::PrismGraph graph;
        graph.attachState(selectionProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(750.0f, 3.0f));

        auto selectedBand = 0;
        graph.onSelectedBandChanged = [&selectedBand](int bandIndex)
        {
            selectedBand = bandIndex;
        };
        graph.selectBandAt(graph.pointForFrequencyAndGain(750.0f, 3.0f));

        expect(graph.getSelectedBand() == 1,
               "Graph should expose the selected band for the editor panel",
               failures);
        expect(selectedBand == 1,
               "Graph should notify the editor when a band is selected",
               failures);
    }

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed\n";
    return 1;
}
