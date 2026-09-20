#include <JuceHeader.h>

#include "Source/Dsp/FractionalDelayLine.h"
#include "Source/Dsp/PhaseAlignmentDetector.h"
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

void setParameter(juce::AudioProcessorValueTreeState& state,
                  const juce::String& id,
                  float plainValue)
{
    if (auto* parameter = state.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
}

juce::Component* findChildComponentWithId(juce::Component& component,
                                          const juce::String& componentId)
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

    constexpr int sampleCount = 8192;
    constexpr int knownDelay = 37;
    std::vector<float> reference(static_cast<size_t>(sampleCount), 0.0f);
    std::vector<float> moving(static_cast<size_t>(sampleCount), 0.0f);
    juce::Random random(0x50484153);

    for (auto& sample : reference)
        sample = random.nextFloat() * 2.0f - 1.0f;

    for (int sample = knownDelay; sample < sampleCount; ++sample)
        moving[static_cast<size_t>(sample)] = reference[static_cast<size_t>(sample - knownDelay)];

    const auto detection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        moving.data(), reference.data(), sampleCount, 48000.0, 256);

    expect(detection.valid,
           "Detector should accept related broadband signals",
           failures);
    expect(std::abs(detection.correctionSamples + static_cast<float>(knownDelay)) < 0.1f,
           "Detector should return the signed correction for a known delay",
           failures);
    expect(! detection.invertPolarity,
           "Detector should preserve matching polarity",
           failures);

    constexpr float knownFractionalDelay = 12.35f;
    std::fill(moving.begin(), moving.end(), 0.0f);
    for (int sample = 14; sample < sampleCount; ++sample)
    {
        const auto sourcePosition = static_cast<float>(sample) - knownFractionalDelay;
        const auto sourceIndex = static_cast<int>(std::floor(sourcePosition));
        const auto fraction = sourcePosition - static_cast<float>(sourceIndex);
        moving[static_cast<size_t>(sample)] =
            reference[static_cast<size_t>(sourceIndex)] * (1.0f - fraction)
            + reference[static_cast<size_t>(sourceIndex + 1)] * fraction;
    }

    const auto fractionalDetection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        moving.data(), reference.data(), sampleCount, 48000.0, 256);
    expect(fractionalDetection.valid
               && std::abs(fractionalDetection.correctionSamples + knownFractionalDelay) < 0.2f,
           "Detector should refine a known delay below one sample",
           failures);

    for (auto& sample : moving)
        sample = -sample;

    const auto invertedDetection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        moving.data(), reference.data(), sampleCount, 48000.0, 256);
    expect(invertedDetection.valid && invertedDetection.invertPolarity,
           "Detector should identify inverted polarity",
           failures);

    std::vector<float> filteredMoving(static_cast<size_t>(sampleCount), 0.0f);
    float filterState = 0.0f;
    for (int sample = knownDelay; sample < sampleCount; ++sample)
    {
        filterState += 0.22f
            * (reference[static_cast<size_t>(sample - knownDelay)] - filterState);
        filteredMoving[static_cast<size_t>(sample)] = filterState;
    }

    const auto filteredDetection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        filteredMoving.data(), reference.data(), sampleCount, 48000.0, 256);
    expect(filteredDetection.valid
               && std::abs(filteredDetection.correctionSamples + static_cast<float>(knownDelay)) < 1.0f,
           "Detector should tolerate different spectral shaping",
           failures);

    std::fill(moving.begin(), moving.end(), 0.0f);
    const auto silentDetection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        moving.data(), reference.data(), sampleCount, 48000.0, 256);
    expect(! silentDetection.valid && silentDetection.confidence < 0.15f,
           "Detector should reject silence instead of publishing a correction",
           failures);

    for (auto& sample : moving)
        sample = random.nextFloat() * 2.0f - 1.0f;
    const auto unrelatedDetection = kratomix::phase_align::PhaseAlignmentDetector::analyze(
        moving.data(), reference.data(), sampleCount, 48000.0, 256);
    expect(! unrelatedDetection.valid,
           "Detector should reject unrelated signals",
           failures);

    kratomix::phase_align::FractionalDelayLine delayLine;
    delayLine.prepare(1, 128, 48000.0);
    delayLine.setDelaySamples(7.5, true);

    juce::AudioBuffer<float> rampBuffer(1, 256);
    for (int sample = 0; sample < rampBuffer.getNumSamples(); ++sample)
        rampBuffer.setSample(0, sample, static_cast<float>(sample));

    delayLine.process(rampBuffer, 1.0f);
    expect(std::abs(rampBuffer.getSample(0, 100) - 92.5f) < 1.0e-3f,
           "Fractional delay should interpolate a half-sample position",
           failures);

    kratomix::PhaseAlignAudioProcessor processor;

    for (const auto* id : { "offsetMs", "polarity", "auditionMode", "lock", "bypass" })
        expect(processor.parameters.getParameter(id) != nullptr,
               "Missing parameter: " + juce::String(id),
               failures);

    expect(processor.getBusCount(true) == 2,
           "Processor should expose main and sidechain input buses",
           failures);
    expect(processor.getBus(true, 0) != nullptr && processor.getBus(true, 0)->isEnabled(),
           "Main input bus should be enabled by default",
           failures);
    expect(processor.getBus(true, 1) != nullptr
               && processor.getBus(true, 1)->getName() == "Sidechain"
               && ! processor.getBus(true, 1)->isEnabled(),
           "Sidechain bus should be named Sidechain and disabled by default",
           failures);

    auto monoLayout = processor.getBusesLayout();
    monoLayout.inputBuses.set(0, juce::AudioChannelSet::mono());
    monoLayout.inputBuses.set(1, juce::AudioChannelSet::disabled());
    monoLayout.outputBuses.set(0, juce::AudioChannelSet::mono());
    expect(processor.isBusesLayoutSupported(monoLayout),
           "Processor should support mono main input and output",
           failures);

    auto stereoWithMonoSidechain = processor.getBusesLayout();
    stereoWithMonoSidechain.inputBuses.set(0, juce::AudioChannelSet::stereo());
    stereoWithMonoSidechain.inputBuses.set(1, juce::AudioChannelSet::mono());
    stereoWithMonoSidechain.outputBuses.set(0, juce::AudioChannelSet::stereo());
    expect(processor.isBusesLayoutSupported(stereoWithMonoSidechain),
           "Processor should accept a mono sidechain for a stereo moving path",
           failures);

    processor.prepareToPlay(48000.0, 512);
    const auto fixedLatency = processor.getLatencySamples();
    expect(fixedLatency >= 480,
           "Processor should report enough fixed latency for negative offsets",
           failures);

    juce::AudioBuffer<float> processingBuffer(2, 512);
    juce::MidiBuffer midi;
    for (const auto offset : { -10.0f, 0.0f, 10.0f })
    {
        setParameter(processor.parameters, "offsetMs", offset);
        processingBuffer.clear();
        processor.processBlock(processingBuffer, midi);
        expect(processor.getLatencySamples() == fixedLatency,
               "Reported latency should stay fixed across signed offsets",
               failures);
    }

    for (const auto offset : { -1.0f, 1.0f })
    {
        kratomix::phase_align::PhaseAlignProcessor delayProcessor([] {});
        juce::dsp::ProcessSpec delaySpec { 48000.0, 4096, 1 };
        delayProcessor.prepare(delaySpec);
        kratomix::phase_align::Settings settings;
        settings.offsetMs = offset;
        delayProcessor.updateSettings(settings);

        juce::AudioBuffer<float> impulse(1, 4096);
        impulse.clear();
        impulse.setSample(0, 1024, 1.0f);
        delayProcessor.process(impulse, nullptr);

        auto peakIndex = 0;
        auto peakValue = 0.0f;
        for (int sample = 0; sample < impulse.getNumSamples(); ++sample)
            if (const auto value = std::abs(impulse.getSample(0, sample)); value > peakValue)
            {
                peakValue = value;
                peakIndex = sample;
            }

        const auto expectedDelay = delayProcessor.getLatencySamples()
                                   + static_cast<int>(offset * 48.0f);
        expect(std::abs((peakIndex - 1024) - expectedDelay) <= 1,
               "Signed offset should move around the fixed base delay",
               failures);
    }

    {
        std::atomic<bool> analysisCompleted { false };
        kratomix::phase_align::PhaseAlignProcessor analysisProcessor(
            [&analysisCompleted] { analysisCompleted.store(true); });
        juce::dsp::ProcessSpec analysisSpec { 48000.0, 512, 1 };
        analysisProcessor.prepare(analysisSpec);
        expect(analysisProcessor.requestAnalysis(),
               "Auto Align should accept a one-shot capture request",
               failures);

        juce::Random captureRandom(0x414C4947);
        std::array<float, 32768> captureReference {};
        for (auto& sample : captureReference)
            sample = captureRandom.nextFloat() * 2.0f - 1.0f;

        for (int block = 0; block < 64; ++block)
        {
            juce::AudioBuffer<float> mainBlock(1, 512);
            juce::AudioBuffer<float> sidechainBlock(1, 512);
            for (int sample = 0; sample < 512; ++sample)
            {
                const auto absoluteSample = block * 512 + sample;
                sidechainBlock.setSample(0, sample,
                                         captureReference[static_cast<size_t>(absoluteSample)]);
                mainBlock.setSample(0, sample,
                                    absoluteSample >= knownDelay
                                        ? captureReference[static_cast<size_t>(absoluteSample - knownDelay)]
                                        : 0.0f);
            }
            analysisProcessor.process(mainBlock, &sidechainBlock);
        }

        for (int attempt = 0; attempt < 200 && ! analysisCompleted.load(); ++attempt)
            juce::Thread::sleep(5);

        kratomix::phase_align::PhaseAlignmentResult result;
        expect(analysisCompleted.load() && analysisProcessor.consumeAnalysisResult(result),
               "Auto Align should finish captured analysis off the audio thread",
               failures);
        expect(result.valid
                   && std::abs(result.correctionSamples + static_cast<float>(knownDelay)) < 0.2f,
               "Auto Align should publish the captured path correction",
               failures);
    }

    {
        constexpr int comparisonSamples = 8192;
        juce::AudioBuffer<float> alignedMain(1, comparisonSamples);
        juce::AudioBuffer<float> unalignedMain(1, comparisonSamples);
        juce::AudioBuffer<float> comparisonReference(1, comparisonSamples);
        juce::Random comparisonRandom(0x4E554C4C);
        for (int sample = 0; sample < comparisonSamples; ++sample)
        {
            const auto value = comparisonRandom.nextFloat() * 2.0f - 1.0f;
            comparisonReference.setSample(0, sample, value);
            const auto delayed = sample >= knownDelay
                                     ? comparisonReference.getSample(0, sample - knownDelay)
                                     : 0.0f;
            alignedMain.setSample(0, sample, delayed);
            unalignedMain.setSample(0, sample, delayed);
        }

        kratomix::phase_align::PhaseAlignProcessor alignedProcessor([] {});
        kratomix::phase_align::PhaseAlignProcessor unalignedProcessor([] {});
        juce::dsp::ProcessSpec comparisonSpec { 48000.0, comparisonSamples, 1 };
        alignedProcessor.prepare(comparisonSpec);
        unalignedProcessor.prepare(comparisonSpec);
        kratomix::phase_align::Settings alignedSettings;
        alignedSettings.offsetMs = -static_cast<float>(knownDelay) / 48.0f;
        alignedSettings.auditionMode = kratomix::phase_align::AuditionMode::difference;
        kratomix::phase_align::Settings unalignedSettings = alignedSettings;
        unalignedSettings.offsetMs = 0.0f;
        alignedProcessor.updateSettings(alignedSettings);
        unalignedProcessor.updateSettings(unalignedSettings);
        alignedProcessor.process(alignedMain, &comparisonReference);
        unalignedProcessor.process(unalignedMain, &comparisonReference);

        double alignedEnergy = 0.0;
        double unalignedEnergy = 0.0;
        for (int sample = 2500; sample < comparisonSamples; ++sample)
        {
            alignedEnergy += static_cast<double>(alignedMain.getSample(0, sample))
                             * alignedMain.getSample(0, sample);
            unalignedEnergy += static_cast<double>(unalignedMain.getSample(0, sample))
                               * unalignedMain.getSample(0, sample);
        }
        expect(alignedEnergy < unalignedEnergy * 0.01,
               "Detected correction should deepen the difference null",
               failures);
    }

    setParameter(processor.parameters, "offsetMs", -1.375f);
    setParameter(processor.parameters, "polarity", 1.0f);
    setParameter(processor.parameters, "auditionMode", 2.0f);
    setParameter(processor.parameters, "lock", 1.0f);

    juce::MemoryBlock state;
    processor.getStateInformation(state);

    kratomix::PhaseAlignAudioProcessor restoredProcessor;
    restoredProcessor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    expect(std::abs(restoredProcessor.parameters.getRawParameterValue("offsetMs")->load() + 1.375f) < 1.0e-4f,
           "Signed offset should survive state restoration",
           failures);
    expect(restoredProcessor.parameters.getRawParameterValue("polarity")->load() > 0.5f,
           "Polarity should survive state restoration",
           failures);
    expect(std::abs(restoredProcessor.parameters.getRawParameterValue("auditionMode")->load() - 2.0f) < 1.0e-6f,
           "Audition mode should survive state restoration",
           failures);
    expect(restoredProcessor.parameters.getRawParameterValue("lock")->load() > 0.5f,
           "Lock state should survive state restoration",
           failures);

    std::unique_ptr<juce::AudioProcessorEditor> editor(restoredProcessor.createEditor());
    expect(editor != nullptr,
           "Editor should be constructible",
           failures);
    auto* audition = dynamic_cast<juce::ComboBox*>(findChildComponentWithId(*editor, "phaseAlignAuditionMode"));
    auto* offset = dynamic_cast<juce::Slider*>(findChildComponentWithId(*editor, "phaseAlignOffset"));
    auto* autoAlign = dynamic_cast<juce::TextButton*>(findChildComponentWithId(*editor, "phaseAlignAuto"));
    expect(audition != nullptr && audition->isVisible() && audition->isEnabled()
               && audition->getSelectedId() == 3 && audition->getHeight() >= 24,
           "Restored editor should show the saved audition mode in a usable control",
           failures);
    expect(offset != nullptr && offset->isVisible() && offset->isEnabled()
               && offset->getWidth() >= 180 && offset->getHeight() >= 24,
           "Offset control should remain visible and usable",
           failures);
    expect(autoAlign != nullptr && autoAlign->isVisible() && autoAlign->isEnabled()
               && ! autoAlign->getBounds().isEmpty(),
           "Auto Align action should remain visible and usable",
           failures);

    if (const auto snapshotPath = juce::SystemStats::getEnvironmentVariable(
            "KRATOMIX_PHASE_ALIGN_SNAPSHOT", {});
        snapshotPath.isNotEmpty())
    {
        const auto snapshot = editor->createComponentSnapshot(editor->getLocalBounds());
        if (juce::FileOutputStream output { juce::File(snapshotPath) }; output.openedOk())
            juce::PNGImageFormat().writeImageToStream(snapshot, output);
    }

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    return 1;
}
