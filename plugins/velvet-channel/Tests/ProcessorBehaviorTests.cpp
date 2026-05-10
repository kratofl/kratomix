#include <JuceHeader.h>

#include "Source/PluginEditor.h"
#include "Source/PluginProcessor.h"
#include "Source/Dsp/WarmthProcessor.h"
#include "Source/Parameters.h"
#include "ui/SteppedSlider.h"

namespace
{
class DummyProcessor final : public juce::AudioProcessor
{
public:
    DummyProcessor()
        : juce::AudioProcessor(BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == layouts.getMainOutputChannelSet();
    }

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Dummy"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

void expect(bool condition, const juce::String& message, int& failures)
{
    if (condition)
        return;

    std::cerr << "FAIL: " << message << '\n';
    ++failures;
}

bool buffersAlmostEqual(const juce::AudioBuffer<float>& lhs,
                        const juce::AudioBuffer<float>& rhs,
                        float tolerance)
{
    if (lhs.getNumChannels() != rhs.getNumChannels() || lhs.getNumSamples() != rhs.getNumSamples())
        return false;

    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
    {
        const auto* left = lhs.getReadPointer(channel);
        const auto* right = rhs.getReadPointer(channel);

        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
        {
            if (std::abs(left[sample] - right[sample]) > tolerance)
                return false;
        }
    }

    return true;
}

juce::AudioBuffer<float> makeSineBuffer(int numChannels, int numSamples)
{
    juce::AudioBuffer<float> buffer(numChannels, numSamples);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer(channel);

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const auto phase = static_cast<float>(sample) / static_cast<float>(numSamples);
            samples[sample] = 0.35f * std::sin(phase * juce::MathConstants<float>::twoPi);
        }
    }

    return buffer;
}

juce::Slider* sliderAt(juce::AudioProcessorEditor& editor, int childIndex)
{
    return dynamic_cast<juce::Slider*>(editor.getChildComponent(childIndex));
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;

    {
        DummyProcessor processor;
        juce::AudioProcessorValueTreeState state(processor, nullptr, "Parameters", kratomix::createParameterLayout());

        expect(state.getParameter("bypass") != nullptr,
               "Parameter layout should expose a bypass parameter",
               failures);
    }

    {
        expect(kratomix::ui::isSharedUiHeader,
               "SteppedSlider should come from the shared core UI header",
               failures);
        kratomix::ui::SteppedSlider slider([](double value) { return std::round(value); });
        expect(std::abs(slider.snapValue(2.7, juce::Slider::notDragging) - 3.0) < 1.0e-6,
               "Shared SteppedSlider should be available from core UI include paths",
               failures);
    }

    {
        kratomix::WarmthProcessor processor;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 256;
        spec.numChannels = 2;

        processor.prepare(spec);

        kratomix::WarmthSettings settingsA;
        settingsA.drive = 2.6f;
        settingsA.highPassHz = 20.0f;
        settingsA.warmthDb = 0.0f;
        settingsA.presenceDb = 0.0f;
        settingsA.airDb = 0.0f;

        kratomix::WarmthSettings settingsB = settingsA;
        settingsB.drive = 2.9f;

        auto bufferA = makeSineBuffer(2, 256);
        auto bufferB = bufferA;

        processor.updateSettings(settingsA);
        processor.process(bufferA);

        processor.reset();
        processor.prepare(spec);
        processor.updateSettings(settingsB);
        processor.process(bufferB);

        expect(buffersAlmostEqual(bufferA, bufferB, 1.0e-6f),
               "Drive values inside one hardware step should render the same output",
               failures);
    }

    {
        kratomix::WarmthProcessor processor;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 256;
        spec.numChannels = 2;

        processor.prepare(spec);

        kratomix::WarmthSettings settings;
        settings.drive = 3.0f;
        settings.highPassHz = 20.0f;
        settings.warmthDb = 0.0f;
        settings.presenceDb = 0.0f;
        settings.airDb = 0.0f;

        auto buffer = makeSineBuffer(2, 256);

        processor.updateSettings(settings);
        processor.process(buffer);

        expect(processor.getVuLevel() > 0.01f,
               "Post-output VU meter should react to audible output",
               failures);
    }

    {
        kratomix::WarmthProcessor processor;

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 256;
        spec.numChannels = 2;

        processor.prepare(spec);

        kratomix::WarmthSettings settings;
        settings.drive = 6.0f;
        settings.highPassHz = 80.0f;
        settings.warmthDb = 4.0f;
        settings.presenceDb = 3.0f;
        settings.airDb = 2.0f;
        settings.outputGainDb = -4.0f;
        settings.bypassed = true;

        auto buffer = makeSineBuffer(2, 256);
        const auto dryReference = buffer;

        processor.updateSettings(settings);
        processor.process(buffer);

        expect(buffersAlmostEqual(buffer, dryReference, 1.0e-6f),
               "Bypassed processing should leave the signal unchanged",
               failures);
    }

    {
        kratomix::VelvetChannelAudioProcessor processor;
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());

        auto* input = sliderAt(*editor, 0);
        auto* drive = sliderAt(*editor, 2);
        auto* highPass = sliderAt(*editor, 4);
        auto* warmth = sliderAt(*editor, 6);
        auto* presence = sliderAt(*editor, 8);
        auto* air = sliderAt(*editor, 10);
        auto* output = sliderAt(*editor, 12);

        expect(input != nullptr && drive != nullptr && highPass != nullptr
                   && warmth != nullptr && presence != nullptr && air != nullptr && output != nullptr,
               "Editor should expose all sliders in the expected order",
               failures);

        if (input != nullptr && drive != nullptr && highPass != nullptr
            && warmth != nullptr && presence != nullptr && air != nullptr && output != nullptr)
        {
            expect(warmth->isDoubleClickReturnEnabled(),
                   "Warmth slider should support double-click reset",
                   failures);
            expect(presence->isDoubleClickReturnEnabled(),
                   "Presence slider should support double-click reset",
                   failures);
            expect(air->isDoubleClickReturnEnabled(),
                   "Air slider should support double-click reset",
                   failures);
            expect(output->isDoubleClickReturnEnabled(),
                   "Output slider should support double-click reset",
                   failures);

            expect(std::abs(warmth->getDoubleClickReturnValue()) < 1.0e-6,
                   "Warmth should reset to 0 dB",
                   failures);
            expect(std::abs(presence->getDoubleClickReturnValue()) < 1.0e-6,
                   "Presence should reset to 0 dB",
                   failures);
            expect(std::abs(air->getDoubleClickReturnValue()) < 1.0e-6,
                   "Air should reset to 0 dB",
                   failures);
            expect(std::abs(output->getDoubleClickReturnValue()) < 1.0e-6,
                   "Output should reset to 0 dB",
                   failures);
            expect(std::abs(highPass->getDoubleClickReturnValue() - 20.0) < 1.0e-6,
                   "HPF should reset to its neutral minimum",
                   failures);
            expect(std::abs(drive->getDoubleClickReturnValue()) < 1.0e-6,
                   "Drive should reset to zero color",
                   failures);

            expect(! warmth->getTextFromValue(3.0).containsIgnoreCase("db"),
                   "Warmth textbox should display plain numbers without dB",
                   failures);
            expect(! output->getTextFromValue(-3.2).containsIgnoreCase("db"),
                   "Output textbox should display plain numbers without dB",
                   failures);
            expect(! highPass->getTextFromValue(35.0).containsIgnoreCase("hz"),
                   "HPF textbox should display plain numbers without Hz",
                   failures);

            expect(std::abs(warmth->getValueFromText("2.6") - 3.0) < 1.0e-6,
                   "Warmth textbox input should accept bare numbers and snap to a step",
                   failures);
            expect(std::abs(output->getValueFromText("-4.2") + 4.2) < 1.0e-6,
                   "Output textbox input should accept bare numbers",
                   failures);
        }
    }

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed\n";
    return 1;
}
