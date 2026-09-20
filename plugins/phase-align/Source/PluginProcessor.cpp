#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr auto stateId = "Parameters";
}

namespace kratomix
{
PhaseAlignAudioProcessor::PhaseAlignAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, stateId, phase_align::createParameterLayout()),
      phaseProcessor([this] { triggerAsyncUpdate(); })
{
}

PhaseAlignAudioProcessor::~PhaseAlignAudioProcessor()
{
    cancelPendingUpdate();
}

void PhaseAlignAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    processingSampleRate = sampleRate;
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = static_cast<juce::uint32>(std::max(1, getMainBusNumOutputChannels()));
    phaseProcessor.prepare(spec);
    setLatencySamples(phaseProcessor.getLatencySamples());
}

void PhaseAlignAudioProcessor::releaseResources()
{
    phaseProcessor.reset();
}

bool PhaseAlignAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if ((mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        || mainInput != mainOutput)
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

void PhaseAlignAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainBuffer = getBusBuffer(buffer, false, 0);
    phaseProcessor.updateSettings(readSettings());
    if (auto* sidechainBus = getBus(true, 1); sidechainBus != nullptr && sidechainBus->isEnabled())
    {
        auto sidechainBuffer = getBusBuffer(buffer, true, 1);
        phaseProcessor.process(mainBuffer, &sidechainBuffer);
    }
    else
    {
        phaseProcessor.process(mainBuffer, nullptr);
    }
}

juce::AudioProcessorEditor* PhaseAlignAudioProcessor::createEditor()
{
    return new PhaseAlignAudioProcessorEditor(*this);
}

void PhaseAlignAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (const auto state = parameters.copyState(); state.isValid())
    {
        std::unique_ptr<juce::XmlElement> xml(state.createXml());
        copyXmlToBinary(*xml, destData);
    }
}

void PhaseAlignAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr && xml->hasTagName(parameters.state.getType()))
        parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

bool PhaseAlignAudioProcessor::requestAutoAlign()
{
    setParameterPlainValue(phase_align::lockId, 0.0f);
    return phaseProcessor.requestAnalysis();
}

void PhaseAlignAudioProcessor::nudgeOffsetBySamples(float samples)
{
    const auto offset = parameters.getRawParameterValue(phase_align::offsetId)->load();
    const auto deltaMs = samples * 1000.0f / static_cast<float>(processingSampleRate);
    setParameterPlainValue(phase_align::offsetId, juce::jlimit(-10.0f, 10.0f, offset + deltaMs));
}

float PhaseAlignAudioProcessor::getOffsetSamples() const noexcept
{
    return parameters.getRawParameterValue(phase_align::offsetId)->load()
           * static_cast<float>(processingSampleRate) * 0.001f;
}

float PhaseAlignAudioProcessor::getCorrelation() const noexcept { return phaseProcessor.getCorrelation(); }
float PhaseAlignAudioProcessor::getConfidence() const noexcept { return phaseProcessor.getConfidence(); }
phase_align::AnalysisStatus PhaseAlignAudioProcessor::getAnalysisStatus() const noexcept
{
    return phaseProcessor.getAnalysisStatus();
}
void PhaseAlignAudioProcessor::copyAnalysisFrame(phase_align::AnalysisFrame& destination) const noexcept
{
    phaseProcessor.copyAnalysisFrame(destination);
}

phase_align::Settings PhaseAlignAudioProcessor::readSettings() const noexcept
{
    phase_align::Settings current;
    current.offsetMs = parameters.getRawParameterValue(phase_align::offsetId)->load();
    current.invertPolarity = parameters.getRawParameterValue(phase_align::polarityId)->load() >= 0.5f;
    current.auditionMode = static_cast<phase_align::AuditionMode>(juce::jlimit(
        0, static_cast<int>(phase_align::AuditionMode::difference),
        static_cast<int>(std::round(parameters.getRawParameterValue(phase_align::auditionModeId)->load()))));
    current.bypassed = parameters.getRawParameterValue(phase_align::bypassId)->load() >= 0.5f;
    return current;
}

void PhaseAlignAudioProcessor::handleAsyncUpdate()
{
    phase_align::PhaseAlignmentResult result;
    if (! phaseProcessor.consumeAnalysisResult(result) || ! result.valid)
        return;
    const auto correctionMs = result.correctionSamples * 1000.0f / static_cast<float>(processingSampleRate);
    setParameterPlainValue(phase_align::offsetId, juce::jlimit(-10.0f, 10.0f, correctionMs));
    setParameterPlainValue(phase_align::polarityId, result.invertPolarity ? 1.0f : 0.0f);
    setParameterPlainValue(phase_align::lockId, 1.0f);
}

void PhaseAlignAudioProcessor::setParameterPlainValue(const char* id, float value)
{
    if (auto* parameter = parameters.getParameter(id))
        parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new kratomix::PhaseAlignAudioProcessor();
}
