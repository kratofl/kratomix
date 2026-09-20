#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"

namespace kratomix
{
class AlignmentDisplay final : public juce::Component
{
public:
    explicit AlignmentDisplay(PhaseAlignAudioProcessor& processorToUse);
    void paint(juce::Graphics& graphics) override;

private:
    PhaseAlignAudioProcessor& processor;
};

class PhaseAlignAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                             private juce::Timer
{
public:
    explicit PhaseAlignAudioProcessorEditor(PhaseAlignAudioProcessor&);
    ~PhaseAlignAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void configureNudgeButton(juce::TextButton& button, const juce::String& text, float samples);
    static juce::String statusText(phase_align::AnalysisStatus status);

    PhaseAlignAudioProcessor& pluginProcessor;
    AlignmentDisplay alignmentDisplay;
    juce::Label titleLabel;
    juce::Label offsetCaption;
    juce::Label offsetReadout;
    juce::Label correlationLabel;
    juce::Label confidenceLabel;
    juce::Label statusLabel;
    juce::Slider offsetSlider;
    juce::TextButton coarseDownButton;
    juce::TextButton fineDownButton;
    juce::TextButton fineUpButton;
    juce::TextButton coarseUpButton;
    juce::TextButton autoAlignButton;
    juce::ToggleButton polarityButton;
    juce::ToggleButton lockButton;
    juce::ToggleButton bypassButton;
    juce::ComboBox auditionModeBox;

    std::unique_ptr<SliderAttachment> offsetAttachment;
    std::unique_ptr<ButtonAttachment> polarityAttachment;
    std::unique_ptr<ButtonAttachment> lockAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;
    std::unique_ptr<ComboBoxAttachment> auditionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PhaseAlignAudioProcessorEditor)
};
}
