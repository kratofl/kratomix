#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto stateId = "Parameters";
}

namespace kratomix
{
MultibandCompressorAudioProcessor::MultibandCompressorAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, stateId, multiband::createParameterLayout())
{
}

void MultibandCompressorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(juce::jmax(1, getTotalNumOutputChannels()));

    multibandProcessor.prepare(spec);
}

void MultibandCompressorAudioProcessor::releaseResources()
{
    multibandProcessor.reset();
}

bool MultibandCompressorAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    const auto mainInput = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    if (mainInput != mainOutput)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sidechain = layouts.getChannelSet(true, 1);
        if (! sidechain.isDisabled()
            && sidechain != juce::AudioChannelSet::mono()
            && sidechain != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void MultibandCompressorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    multibandProcessor.updateSettings(readSettings());

    const auto latency = multibandProcessor.getCurrentLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples(latency);

    if (auto* sidechainBus = getBus(true, 1); sidechainBus != nullptr && sidechainBus->isEnabled())
    {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        multibandProcessor.process(mainBuffer, &sidechainBuffer);
    }
    else
    {
        multibandProcessor.process(mainBuffer, nullptr);
    }
}

juce::AudioProcessorEditor* MultibandCompressorAudioProcessor::createEditor()
{
    return new MultibandCompressorAudioProcessorEditor(*this);
}

void MultibandCompressorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void MultibandCompressorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

float MultibandCompressorAudioProcessor::getOutputLevel() const noexcept
{
    return multibandProcessor.getOutputLevel();
}

void MultibandCompressorAudioProcessor::copyAnalyzerFrame(MultibandAnalyzerFrame& destination) const noexcept
{
    multibandProcessor.copyAnalyzerFrame(destination);
}

MultibandSettings MultibandCompressorAudioProcessor::readSettings() const
{
    MultibandSettings current;

    current.inputGainDb = parameters.getRawParameterValue(multiband::inputGainId)->load();
    current.outputGainDb = parameters.getRawParameterValue(multiband::outputGainId)->load();
    current.mix = parameters.getRawParameterValue(multiband::mixId)->load();
    current.bypassed = parameters.getRawParameterValue(multiband::bypassId)->load() >= 0.5f;
    current.analyzerMode = static_cast<multiband::AnalyzerMode>(juce::jlimit(
        0,
        static_cast<int>(multiband::AnalyzerMode::gainReduction),
        static_cast<int>(std::round(parameters.getRawParameterValue(multiband::analyzerModeId)->load()))));
    current.lookaheadMode = static_cast<multiband::LookaheadMode>(juce::jlimit(
        0,
        static_cast<int>(multiband::LookaheadMode::fiveMilliseconds),
        static_cast<int>(std::round(parameters.getRawParameterValue(multiband::lookaheadModeId)->load()))));

    for (int index = 0; index < multiband::crossoverCount; ++index)
        current.crossoverFrequencies[static_cast<size_t>(index)] = parameters.getRawParameterValue(multiband::crossoverFrequencyIds[static_cast<size_t>(index)])->load();

    for (int index = 0; index < multiband::maxBands; ++index)
    {
        auto& band = current.bands[static_cast<size_t>(index)];
        band.enabled = parameters.getRawParameterValue(multiband::bandEnabledIds[static_cast<size_t>(index)])->load() >= 0.5f;
        band.solo = parameters.getRawParameterValue(multiband::bandSoloIds[static_cast<size_t>(index)])->load() >= 0.5f;
        band.audition = parameters.getRawParameterValue(multiband::bandAuditionIds[static_cast<size_t>(index)])->load() >= 0.5f;
        band.thresholdDb = parameters.getRawParameterValue(multiband::bandThresholdIds[static_cast<size_t>(index)])->load();
        band.rangeDb = parameters.getRawParameterValue(multiband::bandRangeIds[static_cast<size_t>(index)])->load();
        band.ratio = parameters.getRawParameterValue(multiband::bandRatioIds[static_cast<size_t>(index)])->load();
        band.attackMs = parameters.getRawParameterValue(multiband::bandAttackIds[static_cast<size_t>(index)])->load();
        band.releaseMs = parameters.getRawParameterValue(multiband::bandReleaseIds[static_cast<size_t>(index)])->load();
        band.kneeDb = parameters.getRawParameterValue(multiband::bandKneeIds[static_cast<size_t>(index)])->load();
        band.makeupDb = parameters.getRawParameterValue(multiband::bandMakeupIds[static_cast<size_t>(index)])->load();
        band.mode = static_cast<multiband::BandMode>(juce::jlimit(
            0,
            static_cast<int>(multiband::BandMode::expand),
            static_cast<int>(std::round(parameters.getRawParameterValue(multiband::bandModeIds[static_cast<size_t>(index)])->load()))));
        band.detectorSource = static_cast<multiband::DetectorSource>(juce::jlimit(
            0,
            static_cast<int>(multiband::DetectorSource::external),
            static_cast<int>(std::round(parameters.getRawParameterValue(multiband::bandDetectorSourceIds[static_cast<size_t>(index)])->load()))));
        band.stereoLink = parameters.getRawParameterValue(multiband::bandStereoLinkIds[static_cast<size_t>(index)])->load();
    }

    return current;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kratomix::MultibandCompressorAudioProcessor();
}
