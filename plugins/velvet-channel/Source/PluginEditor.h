#pragma once

#include <JuceHeader.h>

#include "ControlValues.h"
#include "PluginProcessor.h"
#include "ui/RackLookAndFeel.h"
#include "ui/SteppedSlider.h"
#include "ui/VuMeter.h"

namespace kratomix
{
class VelvetChannelAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit VelvetChannelAudioProcessorEditor(VelvetChannelAudioProcessor&);
    ~VelvetChannelAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void configureSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         std::function<juce::String(double)> formatter,
                         std::function<double(const juce::String&)> parser,
                         double doubleClickValue);

    VelvetChannelAudioProcessor& audioProcessor;
    ui::RackLookAndFeel rackLookAndFeel;

    ui::SteppedSlider inputSlider;
    ui::SteppedSlider driveSlider;
    ui::SteppedSlider highPassSlider;
    ui::SteppedSlider warmthSlider;
    ui::SteppedSlider presenceSlider;
    ui::SteppedSlider airSlider;
    ui::SteppedSlider outputSlider;
    juce::ToggleButton bypassButton { "BYPASS" };
    ui::VuMeter vuMeter;

    juce::Label inputLabel;
    juce::Label driveLabel;
    juce::Label highPassLabel;
    juce::Label warmthLabel;
    juce::Label presenceLabel;
    juce::Label airLabel;
    juce::Label outputLabel;

    ButtonAttachment bypassAttachment;
    SliderAttachment inputAttachment;
    SliderAttachment driveAttachment;
    SliderAttachment highPassAttachment;
    SliderAttachment warmthAttachment;
    SliderAttachment presenceAttachment;
    SliderAttachment airAttachment;
    SliderAttachment outputAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VelvetChannelAudioProcessorEditor)
};
}
