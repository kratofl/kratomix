#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

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
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text);

    VelvetChannelAudioProcessor& processor;

    juce::Slider inputSlider;
    juce::Slider driveSlider;
    juce::Slider highPassSlider;
    juce::Slider warmthSlider;
    juce::Slider presenceSlider;
    juce::Slider airSlider;
    juce::Slider outputSlider;

    juce::Label inputLabel;
    juce::Label driveLabel;
    juce::Label highPassLabel;
    juce::Label warmthLabel;
    juce::Label presenceLabel;
    juce::Label airLabel;
    juce::Label outputLabel;

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
