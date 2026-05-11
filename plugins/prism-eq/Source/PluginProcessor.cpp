#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto stateId = "Parameters";
constexpr auto bypassId = "bypass";
}

namespace kratomix
{
PrismEqAudioProcessor::PrismEqAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, stateId, prism::createParameterLayout())
{
}

void PrismEqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    prismProcessor.prepare(spec);
}

void PrismEqAudioProcessor::releaseResources()
{
    prismProcessor.reset();
}

bool PrismEqAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void PrismEqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    prismProcessor.updateSettings(readSettings());
    const auto latency = prismProcessor.getCurrentLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples(latency);

    if (auto* sidechainBus = getBus(true, 1); sidechainBus != nullptr && sidechainBus->isEnabled())
    {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        prismProcessor.process(mainBuffer, &sidechainBuffer);
    }
    else
    {
        prismProcessor.process(mainBuffer, nullptr);
    }
}

juce::AudioProcessorEditor* PrismEqAudioProcessor::createEditor()
{
    return new PrismEqAudioProcessorEditor(*this);
}

void PrismEqAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void PrismEqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

float PrismEqAudioProcessor::getOutputLevel() const noexcept
{
    return prismProcessor.getOutputLevel();
}

void PrismEqAudioProcessor::copyAnalyzerFrame(PrismAnalyzerFrame& destination) const noexcept
{
    prismProcessor.copyAnalyzerFrame(destination);
}

PrismSettings PrismEqAudioProcessor::readSettings() const
{
    PrismSettings current;

    current.inputGainDb = parameters.getRawParameterValue("inputGain")->load();
    current.outputGainDb = parameters.getRawParameterValue("outputGain")->load();
    current.mix = parameters.getRawParameterValue("mix")->load();
    current.bypassed = parameters.getRawParameterValue(bypassId)->load() >= 0.5f;
    current.phaseMode = static_cast<prism::PhaseMode>(juce::jlimit(
        0,
        static_cast<int>(prism::PhaseMode::linearPhase),
        static_cast<int>(std::round(parameters.getRawParameterValue("phaseMode")->load()))));
    current.qualityMode = static_cast<prism::QualityMode>(juce::jlimit(
        0,
        static_cast<int>(prism::QualityMode::oversample4x),
        static_cast<int>(std::round(parameters.getRawParameterValue("qualityMode")->load()))));

    for (int index = 1; index <= prism::maxBands; ++index)
    {
        const auto prefix = prism::bandPrefix(index);
        auto& band = current.bands[static_cast<size_t>(index - 1)];

        band.enabled = parameters.getRawParameterValue(prefix + "Enabled")->load() >= 0.5f;
        band.type = static_cast<prism::BandType>(juce::jlimit(
            0,
            static_cast<int>(prism::BandType::notch),
            static_cast<int>(std::round(parameters.getRawParameterValue(prefix + "Type")->load()))));
        band.frequency = parameters.getRawParameterValue(prefix + "Frequency")->load();
        band.gainDb = parameters.getRawParameterValue(prefix + "Gain")->load();
        band.q = parameters.getRawParameterValue(prefix + "Q")->load();
        band.dynamicEnabled = parameters.getRawParameterValue(prefix + "DynamicEnabled")->load() >= 0.5f;
        band.dynamicRangeDb = parameters.getRawParameterValue(prefix + "DynamicRange")->load();
        band.thresholdDb = parameters.getRawParameterValue(prefix + "Threshold")->load();
        band.attackMs = parameters.getRawParameterValue(prefix + "Attack")->load();
        band.releaseMs = parameters.getRawParameterValue(prefix + "Release")->load();
        band.sidechainSource = static_cast<prism::SidechainSource>(juce::jlimit(
            0,
            static_cast<int>(prism::SidechainSource::external),
            static_cast<int>(std::round(parameters.getRawParameterValue(prefix + "SidechainSource")->load()))));
        band.solo = parameters.getRawParameterValue(prefix + "Solo")->load() >= 0.5f;
    }

    return current;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kratomix::PrismEqAudioProcessor();
}
