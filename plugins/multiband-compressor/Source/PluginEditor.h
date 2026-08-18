#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Ui/MultibandGraph.h"
#include "ui/RackLookAndFeel.h"
#include "ui/VuMeter.h"

namespace kratomix
{
class MultibandCompressorAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit MultibandCompressorAudioProcessorEditor(MultibandCompressorAudioProcessor&);
    ~MultibandCompressorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void configureGlobalControls();
    void configureBandControls();
    void configureRotarySlider(juce::Slider& slider, juce::Label& label, const juce::String& text, double doubleClickValue);
    void configureLinearSlider(juce::Slider& slider, juce::Label& label, const juce::String& text, double doubleClickValue);
    void configureCombo(juce::ComboBox& box, const juce::StringArray& choices);
    void rebuildBandAttachments(int zeroBasedBandIndex);
    void setBandControlsEnabled(bool enabled);

    MultibandCompressorAudioProcessor& pluginProcessor;

    ui::RackLookAndFeel rackLookAndFeel;
    multiband::MultibandGraph graph;
    ui::VuMeter outputMeter;

    juce::Label brandLabel;
    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label mixLabel;
    juce::Label analyzerLabel;
    juce::Label lookaheadLabel;
    juce::Label meterLabel;
    juce::Slider inputSlider;
    juce::Slider outputSlider;
    juce::Slider mixSlider;
    juce::ComboBox analyzerBox;
    juce::ComboBox lookaheadBox;
    juce::ToggleButton bypassButton { "BYPASS" };

    std::unique_ptr<SliderAttachment> inputAttachment;
    std::unique_ptr<SliderAttachment> outputAttachment;
    std::unique_ptr<SliderAttachment> mixAttachment;
    std::unique_ptr<ComboBoxAttachment> analyzerAttachment;
    std::unique_ptr<ComboBoxAttachment> lookaheadAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    juce::Component bandPanel;
    juce::Label bandLabel;
    juce::Label modeLabel;
    juce::Label detectorLabel;
    juce::Label frequencyLabel;
    juce::Label widthLabel;
    juce::Label thresholdLabel;
    juce::Label rangeLabel;
    juce::Label ratioLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label kneeLabel;
    juce::Label makeupLabel;
    juce::Label stereoLinkLabel;
    juce::ComboBox modeBox;
    juce::ComboBox detectorBox;
    juce::Slider frequencySlider;
    juce::Slider widthSlider;
    juce::Slider thresholdSlider;
    juce::Slider rangeSlider;
    juce::Slider ratioSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::Slider kneeSlider;
    juce::Slider makeupSlider;
    juce::Slider stereoLinkSlider;
    juce::TextButton deleteButton { "DEL" };
    juce::ToggleButton soloButton { "SOLO" };
    juce::ToggleButton auditionButton { "AUDITION" };

    std::unique_ptr<ComboBoxAttachment> modeAttachment;
    std::unique_ptr<ComboBoxAttachment> detectorAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> widthAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> rangeAttachment;
    std::unique_ptr<SliderAttachment> ratioAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<SliderAttachment> kneeAttachment;
    std::unique_ptr<SliderAttachment> makeupAttachment;
    std::unique_ptr<SliderAttachment> stereoLinkAttachment;
    std::unique_ptr<ButtonAttachment> soloAttachment;
    std::unique_ptr<ButtonAttachment> auditionAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MultibandCompressorAudioProcessorEditor)
};
}
