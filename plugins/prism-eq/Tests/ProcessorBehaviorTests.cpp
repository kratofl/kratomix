#include <JuceHeader.h>

#include "Source/Parameters.h"
#include "Source/Dsp/PrismLatency.h"
#include "Source/Dsp/PrismProcessor.h"
#include "Source/Dsp/PrismResponseModel.h"
#include "Source/PluginProcessor.h"
#include "Source/Analysis/PrismAutoEq.h"
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

void expectFiniteResponse(const kratomix::PrismIIRCoefficients& coefficients,
                          double sampleRate,
                          const juce::String& label,
                          int& failures)
{
    const std::array<float, 8> probes {
        20.0f, 100.0f, 1000.0f, 6000.0f, 12000.0f, 18000.0f,
        static_cast<float>(sampleRate * 0.43),
        static_cast<float>(sampleRate * 0.47)
    };

    for (const auto frequency : probes)
    {
        if (frequency >= sampleRate * 0.49)
            continue;

        const auto magnitude = coefficients.getMagnitudeForFrequency(frequency, sampleRate);
        expect(std::isfinite(magnitude) && magnitude > 0.0 && magnitude < 1000.0,
               label + " should have finite bounded response at " + juce::String(frequency, 1) + " Hz",
               failures);
    }
}

float maxResponseDb(const kratomix::PrismIIRCoefficients& coefficients,
                    double sampleRate,
                    float startFrequency,
                    float endFrequency)
{
    auto maxDb = -120.0f;

    for (int index = 0; index <= 240; ++index)
    {
        const auto proportion = static_cast<float>(index) / 240.0f;
        const auto frequency = std::pow(10.0f,
                                        juce::jmap(proportion,
                                                   std::log10(startFrequency),
                                                   std::log10(endFrequency)));
        const auto magnitude = coefficients.getMagnitudeForFrequency(frequency, sampleRate);
        maxDb = juce::jmax(maxDb, juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -120.0f));
    }

    return maxDb;
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

juce::Component* findChildComponentWithId(juce::Component& component, const juce::String& componentId)
{
    if (component.getComponentID() == componentId)
        return &component;

    for (int index = 0; index < component.getNumChildComponents(); ++index)
        if (auto* found = findChildComponentWithId(*component.getChildComponent(index), componentId))
            return found;

    return nullptr;
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
    expectParameter(processor.parameters, "qualityMode", failures);
    expect(kratomix::prism::qualityModeChoices().contains("Native"),
           "Quality choices should include native processing",
           failures);
    expect(kratomix::prism::qualityModeChoices().contains("2x"),
           "Quality choices should include 2x oversampling",
           failures);
    expect(kratomix::prism::qualityModeChoices().contains("4x"),
           "Quality choices should include 4x oversampling",
           failures);
    expect(kratomix::prism::phaseModeChoices().contains("Linear Phase"),
           "Phase mode choices should include real linear phase processing",
           failures);
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
    if (auto* rangeParameter = processor.parameters.getParameter("band01DynamicRange"))
    {
        expect(rangeParameter->convertFrom0to1(0.0f) < -29.0f,
               "Dynamic range should support negative movement",
               failures);
        expect(rangeParameter->convertFrom0to1(1.0f) > 29.0f,
               "Dynamic range should support positive movement",
               failures);
    }

    {
        const std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };

        for (const auto sampleRateToTest : sampleRates)
        {
            kratomix::PrismBandSettings band;
            band.enabled = true;
            band.type = kratomix::prism::BandType::bell;
            band.frequency = static_cast<float>(sampleRateToTest * 0.45);
            band.gainDb = 18.0f;
            band.q = 30.0f;

            const auto coefficients = kratomix::makePrismCoefficients(sampleRateToTest, band, 0.0f);
            expect(coefficients != nullptr,
                   "High-frequency bell coefficient generation should return coefficients",
                   failures);
            if (coefficients != nullptr)
                expectFiniteResponse(*coefficients, sampleRateToTest, "High-frequency bell", failures);
        }
    }

    {
        expect(kratomix::prism::safeFilterFrequency(30000.0f, 48000.0) < 12000.0f,
               "Safe filter frequency should stay comfortably below Nyquist",
               failures);
        expect(kratomix::prism::safeFilterQ(100.0f) <= 32.0f,
               "Safe filter Q should clamp extreme bell values",
               failures);
        expect(kratomix::prism::safeShelfQ(40.0f) <= 0.95f,
               "Safe shelf Q should prevent high-Q shelf resonance spikes",
               failures);
    }

    {
        kratomix::PrismBandSettings lowShelf;
        lowShelf.enabled = true;
        lowShelf.type = kratomix::prism::BandType::lowShelf;
        lowShelf.frequency = 180.0f;
        lowShelf.gainDb = 6.0f;
        lowShelf.q = 40.0f;

        const auto lowShelfCoefficients = kratomix::makePrismCoefficients(96000.0, lowShelf, 0.0f);
        expect(lowShelfCoefficients != nullptr,
               "Low shelf coefficients should be created",
               failures);
        if (lowShelfCoefficients != nullptr)
        {
            const auto maximumDb = maxResponseDb(*lowShelfCoefficients, 96000.0, 20.0f, 2000.0f);
            expect(maximumDb <= 7.25f,
                   "High-Q low shelf should not create an unintended resonant spike above the requested gain",
                   failures);
        }

        kratomix::PrismBandSettings highShelf;
        highShelf.enabled = true;
        highShelf.type = kratomix::prism::BandType::highShelf;
        highShelf.frequency = 9000.0f;
        highShelf.gainDb = 6.0f;
        highShelf.q = 40.0f;

        const auto highShelfCoefficients = kratomix::makePrismCoefficients(96000.0, highShelf, 0.0f);
        expect(highShelfCoefficients != nullptr,
               "High shelf coefficients should be created",
               failures);
        if (highShelfCoefficients != nullptr)
        {
            const auto maximumDb = maxResponseDb(*highShelfCoefficients, 96000.0, 1000.0f, 20000.0f);
            expect(maximumDb <= 7.25f,
                   "High-Q high shelf should not create an unintended resonant spike above the requested gain",
                   failures);
        }
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
    expect(editor != nullptr && editor->getWidth() >= 1040,
           "DSP quality controls should fit in the Prism editor width",
           failures);
    expect(editor != nullptr && editor->getHeight() >= 520,
           "Prism editor should leave room for graph and controls",
           failures);
    expect(editor != nullptr && editor->getNumChildComponents() >= 8,
           "Prism editor should expose graph, global controls, output meter, and selected-band controls",
           failures);
    if (editor != nullptr)
        expect(findChildComponentWithId(*editor, "prismAutoRefineButton") != nullptr,
               "Prism editor should expose the input Auto Refine action",
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
        kratomix::PrismProcessor engineProcessor;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 2048;
        spec.numChannels = 2;
        engineProcessor.prepare(spec);

        kratomix::PrismSettings settings;
        auto& band = settings.bands[0];
        band.enabled = true;
        band.type = kratomix::prism::BandType::bell;
        band.frequency = 1000.0f;
        band.gainDb = 6.0f;
        band.q = 1.0f;
        engineProcessor.updateSettings(settings);

        auto engineBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(engineBuffer);
        engineProcessor.process(engineBuffer, nullptr);

        expect(rmsLevel(engineBuffer) > dryRms * 1.45f,
               "Minimum-phase engine extraction should preserve bell boost behavior",
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
            dynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->convertTo0to1(-12.0f));
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
        kratomix::PrismSettings responseSettings;
        auto& bell = responseSettings.bands[0];
        bell.enabled = true;
        bell.type = kratomix::prism::BandType::bell;
        bell.frequency = 1000.0f;
        bell.gainDb = 6.0f;
        bell.q = 1.0f;

        std::array<float, kratomix::prism::maxBands> noDynamicGain {};
        const auto snapshot = kratomix::makePrismResponseSnapshot(responseSettings, 48000.0, noDynamicGain, true);
        const auto centreDb = kratomix::prismResponseGainDbAt(snapshot, 1000.0f);
        const auto farDb = kratomix::prismResponseGainDbAt(snapshot, 10000.0f);

        expect(centreDb > 5.4f && centreDb < 6.6f,
               "Exact response model should show a +6 dB bell near its centre frequency",
               failures);
        expect(std::abs(farDb) < 1.0f,
               "Exact response model should return near-neutral response far from a moderate bell",
               failures);

        noDynamicGain[0] = -4.0f;
        bell.dynamicEnabled = true;
        const auto dynamicSnapshot = kratomix::makePrismResponseSnapshot(responseSettings, 48000.0, noDynamicGain, true);
        const auto dynamicCentreDb = kratomix::prismResponseGainDbAt(dynamicSnapshot, 1000.0f);

        expect(dynamicCentreDb < centreDb - 3.0f,
               "Exact response model should include dynamic gain movement when requested",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor upwardDynamicProcessor;
        disableSidechainForProcessorTest(upwardDynamicProcessor);
        upwardDynamicProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(upwardDynamicProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 0.0f));

        const auto prefix = kratomix::prism::bandPrefix(1);
        upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicEnabled")->setValueNotifyingHost(1.0f);
        upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->convertTo0to1(9.0f));
        upwardDynamicProcessor.parameters.getParameter(prefix + "Threshold")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "Threshold")->convertTo0to1(-48.0f));
        upwardDynamicProcessor.parameters.getParameter(prefix + "Attack")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "Attack")->convertTo0to1(0.1f));

        auto upwardBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(upwardBuffer);
        juce::MidiBuffer upwardMidi;
        upwardDynamicProcessor.processBlock(upwardBuffer, upwardMidi);
        upwardDynamicProcessor.processBlock(upwardBuffer, upwardMidi);
        const auto upwardRms = rmsLevel(upwardBuffer);

        expect(upwardRms > dryRms * 1.15f,
               "A positive dynamic range value should lift matching audio when above threshold",
               failures);

        kratomix::PrismAnalyzerFrame upwardFrame;
        upwardDynamicProcessor.copyAnalyzerFrame(upwardFrame);
        expect(upwardFrame.dynamicGainDb[0] > 2.0f,
               "Analyzer frame should expose upward dynamic gain movement",
               failures);
        expect(upwardFrame.detectorLevelDb[0] > -60.0f,
               "Analyzer frame should expose detector level for live UI readout",
               failures);
        expect(upwardFrame.detectorOverThresholdDb[0] > 0.0f,
               "Analyzer frame should expose detector amount above threshold",
               failures);
    }

    {
        kratomix::PrismEqAudioProcessor oversampledProcessor;
        disableSidechainForProcessorTest(oversampledProcessor);
        oversampledProcessor.prepareToPlay(48000.0, 512);

        oversampledProcessor.parameters.getParameter("qualityMode")->setValueNotifyingHost(
            oversampledProcessor.parameters.getParameter("qualityMode")->convertTo0to1(
                static_cast<float>(kratomix::prism::QualityMode::oversample2x)));

        kratomix::prism::PrismGraph graph;
        graph.attachState(oversampledProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        auto oversampledBuffer = makeSineBuffer(2, 512, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(oversampledBuffer);
        juce::MidiBuffer oversampledMidi;
        oversampledProcessor.processBlock(oversampledBuffer, oversampledMidi);

        expect(rmsLevel(oversampledBuffer) > dryRms * 1.35f,
               "2x oversampled quality mode should preserve audible EQ behavior",
               failures);
        expect(oversampledProcessor.getLatencySamples() == kratomix::prism::prismLatencyFor(
                   kratomix::prism::PhaseMode::zeroLatency,
                   kratomix::prism::QualityMode::oversample2x),
               "Processor should report oversampling latency to the host",
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
        kratomix::PrismEqAudioProcessor linearProcessor;
        disableSidechainForProcessorTest(linearProcessor);
        linearProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(linearProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        linearProcessor.parameters.getParameter("phaseMode")->setValueNotifyingHost(
            linearProcessor.parameters.getParameter("phaseMode")->convertTo0to1(
                static_cast<float>(kratomix::prism::PhaseMode::linearPhase)));

        auto linearBuffer = makeSineBuffer(2, 4096, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(linearBuffer);
        juce::MidiBuffer linearMidi;
        linearProcessor.processBlock(linearBuffer, linearMidi);
        linearProcessor.processBlock(linearBuffer, linearMidi);

        expect(rmsLevel(linearBuffer) > dryRms * 1.20f,
               "Linear phase mode should apply audible EQ gain",
               failures);
        expect(linearProcessor.getLatencySamples() >= 2048,
               "Linear phase mode should report convolution latency",
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
        band.dynamicRangeDb = -12.0f;
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
        std::array<float, 240> quietSpectrum {};
        quietSpectrum.fill(-92.0f);

        const auto quietResult = kratomix::prism::makeInputAutoEqCurve(quietSpectrum);
        expect(! quietResult.hasSignal,
               "Input Auto Refine should reject noise-floor spectra",
               failures);
        expect(quietResult.count == 0,
               "Input Auto Refine should not create bands without signal",
               failures);

        std::array<float, 240> resonantSpectrum {};
        resonantSpectrum.fill(-46.0f);
        resonantSpectrum[132] = -24.0f;
        resonantSpectrum[133] = -23.0f;
        resonantSpectrum[134] = -25.0f;

        const auto resonantResult = kratomix::prism::makeInputAutoEqCurve(resonantSpectrum);
        expect(resonantResult.hasSignal,
               "Input Auto Refine should accept active input spectra",
               failures);
        expect(resonantResult.count == 1,
               "Input Auto Refine should collapse nearby resonance bins into one suggestion",
               failures);
        if (resonantResult.count > 0)
        {
            const auto& suggestion = resonantResult.suggestions[0];
            expect(suggestion.type == kratomix::prism::BandType::bell,
                   "Input Auto Refine should use bell cuts for narrow resonances",
                   failures);
            expect(suggestion.gainDb < -1.0f && suggestion.gainDb >= -4.6f,
                   "Input Auto Refine resonance suggestions should be conservative cuts",
                   failures);
            expect(suggestion.q >= 1.5f && suggestion.q <= 8.0f,
                   "Input Auto Refine should keep resonance Q in a musical range",
                   failures);
            expect(suggestion.frequency > 700.0f && suggestion.frequency < 1400.0f,
                   "Input Auto Refine should map resonance bin to the expected input frequency area",
                   failures);
        }

        std::array<float, 240> rumbleSpectrum {};
        rumbleSpectrum.fill(-58.0f);
        for (size_t index = 0; index < 28; ++index)
            rumbleSpectrum[index] = -31.0f;

        const auto rumbleResult = kratomix::prism::makeInputAutoEqCurve(rumbleSpectrum);
        expect(rumbleResult.count > 0 && rumbleResult.suggestions[0].type == kratomix::prism::BandType::highPass,
               "Input Auto Refine should add a high-pass suggestion for obvious low-end rumble",
               failures);

        for (size_t index = 0; index < rumbleResult.count; ++index)
            expect(rumbleResult.suggestions[index].gainDb <= 0.0f,
                   "Input Auto Refine should not create automatic boosts in V1",
                   failures);
    }

    {
        expect(kratomix::prism::prismLatencyFor(kratomix::prism::PhaseMode::zeroLatency,
                                                kratomix::prism::QualityMode::native) == 0,
               "Native zero-latency mode should report no added latency",
               failures);
        expect(kratomix::prism::prismLatencyFor(kratomix::prism::PhaseMode::zeroLatency,
                                                kratomix::prism::QualityMode::oversample2x) > 0,
               "2x quality mode should report oversampling latency",
               failures);
        expect(kratomix::prism::prismLatencyFor(kratomix::prism::PhaseMode::linearPhase,
                                                kratomix::prism::QualityMode::native) >= 2048,
               "Linear phase mode should report FIR latency",
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

    {
        kratomix::PrismEqAudioProcessor autoRefineProcessor;

        kratomix::prism::PrismGraph graph;
        graph.attachState(autoRefineProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);

        const auto band01 = kratomix::prism::bandPrefix(1);
        const auto band03 = kratomix::prism::bandPrefix(3);
        autoRefineProcessor.parameters.getParameter(band01 + "Enabled")->setValueNotifyingHost(1.0f);
        autoRefineProcessor.parameters.getParameter(band01 + "DynamicEnabled")->setValueNotifyingHost(1.0f);
        autoRefineProcessor.parameters.getParameter(band01 + "DynamicRange")->setValueNotifyingHost(
            autoRefineProcessor.parameters.getParameter(band01 + "DynamicRange")->convertTo0to1(-9.0f));
        autoRefineProcessor.parameters.getParameter(band03 + "Enabled")->setValueNotifyingHost(1.0f);

        expect(graph.hasActiveBands(),
               "Auto Refine graph API should detect an active EQ curve before replacing it",
               failures);

        std::array<float, kratomix::prism::autoEqSpectrumBinCount> spectrum {};
        spectrum.fill(-46.0f);
        spectrum[132] = -24.0f;
        spectrum[133] = -23.0f;
        spectrum[134] = -25.0f;

        const auto applyResult = graph.applyInputAutoRefineForSpectrum(spectrum);
        expect(applyResult == kratomix::prism::AutoRefineApplyResult::refined,
               "Auto Refine should report that a usable input curve was applied",
               failures);
        expect(autoRefineProcessor.parameters.getRawParameterValue(band01 + "Enabled")->load() > 0.5f,
               "Auto Refine should write the first suggestion into band 01",
               failures);
        expect(std::abs(autoRefineProcessor.parameters.getRawParameterValue(band01 + "Type")->load()
                        - static_cast<float>(kratomix::prism::BandType::bell)) < 1.0e-6f,
               "Auto Refine should write resonance suggestions as bell bands",
               failures);
        expect(autoRefineProcessor.parameters.getRawParameterValue(band01 + "Gain")->load() < -1.0f,
               "Auto Refine should write conservative cut gain into band 01",
               failures);
        expect(autoRefineProcessor.parameters.getRawParameterValue(band01 + "DynamicEnabled")->load() < 0.5f,
               "Auto Refine should reset generated bands to static mode",
               failures);
        expect(std::abs(autoRefineProcessor.parameters.getRawParameterValue(band01 + "DynamicRange")->load()) < 1.0e-6f,
               "Auto Refine should reset generated band dynamic range",
               failures);
        expect(autoRefineProcessor.parameters.getRawParameterValue(band03 + "Enabled")->load() < 0.5f,
               "Auto Refine should disable old bands beyond the generated starter curve",
               failures);

        spectrum.fill(-90.0f);
        const auto noSignalResult = graph.applyInputAutoRefineForSpectrum(spectrum);
        expect(noSignalResult == kratomix::prism::AutoRefineApplyResult::noSignal,
               "Auto Refine should report no signal without changing the current curve",
               failures);
        expect(autoRefineProcessor.parameters.getRawParameterValue(band01 + "Enabled")->load() > 0.5f,
               "No-signal Auto Refine should leave the current curve untouched",
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
