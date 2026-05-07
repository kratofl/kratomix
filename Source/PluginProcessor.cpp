#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace kratomix
{
VelvetChannelAudioProcessor::VelvetChannelAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
}

void VelvetChannelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(getTotalNumOutputChannels());

    warmthProcessor.prepare(spec);
}

void VelvetChannelAudioProcessor::releaseResources()
{
    warmthProcessor.reset();
}

bool VelvetChannelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    const auto mainInput = layouts.getMainInputChannelSet();

    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return mainInput == mainOutput;
}

void VelvetChannelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (auto channel = getTotalNumInputChannels(); channel < getTotalNumOutputChannels(); ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    warmthProcessor.updateSettings(readSettings());
    warmthProcessor.process(buffer);
}

juce::AudioProcessorEditor* VelvetChannelAudioProcessor::createEditor()
{
    return new VelvetChannelAudioProcessorEditor(*this);
}

void VelvetChannelAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void VelvetChannelAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

WarmthSettings VelvetChannelAudioProcessor::readSettings() const
{
    WarmthSettings current;

    current.inputGainDb = parameters.getRawParameterValue(ParamID::inputGain)->load();
    current.drive = parameters.getRawParameterValue(ParamID::drive)->load();
    current.highPassHz = parameters.getRawParameterValue(ParamID::highPass)->load();
    current.warmthDb = parameters.getRawParameterValue(ParamID::warmth)->load();
    current.presenceDb = parameters.getRawParameterValue(ParamID::presence)->load();
    current.airDb = parameters.getRawParameterValue(ParamID::air)->load();
    current.outputGainDb = parameters.getRawParameterValue(ParamID::outputGain)->load();

    return current;
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kratomix::VelvetChannelAudioProcessor();
}
