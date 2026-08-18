#pragma once

#include <JuceHeader.h>

#include "PluginProcessor.h"
#include "Ui/PrismGraph.h"
#include "ui/RackLookAndFeel.h"
#include "ui/VuMeter.h"

namespace kratomix
{
class PrismEqAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit PrismEqAudioProcessorEditor(PrismEqAudioProcessor&);
    ~PrismEqAudioProcessorEditor() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void configureBandControls();
    void rebuildBandAttachments(int oneBasedBandIndex);
    void setBandControlsEnabled(bool shouldBeEnabled);
    void configureValueSlider(juce::Slider& slider);
    void triggerAutoRefine();
    void applyAutoRefineNow();
    void timerCallback() override;

    PrismEqAudioProcessor& pluginProcessor;

    ui::RackLookAndFeel lookAndFeel;
    prism::PrismGraph graph;

    juce::Label brandLabel;
    juce::Label titleLabel;
    juce::Label versionLabel;
    juce::Label analyzerLabel;
    juce::ComboBox analyzerModeBox;
    juce::Label phaseLabel;
    juce::ComboBox phaseModeBox;
    juce::Label qualityLabel;
    juce::ComboBox qualityModeBox;
    juce::Label liveLabel;
    juce::Label detectorLabel;
    juce::Label movementLabel;
    juce::TextButton autoRefineButton { "REFINE" };
    juce::Label autoRefineStatusLabel;
    juce::Label speedLabel;
    juce::Label rangeLabelGlobal;
    juce::Label scaleLabel;
    juce::Slider analyzerSpeedSlider;
    juce::Slider analyzerRangeSlider;
    juce::Slider gainScaleSlider;
    juce::Label meterLabel;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Slider outputSlider;
    ui::VuMeter outputMeter;

    ButtonAttachment bypassAttachment;
    SliderAttachment outputAttachment;
    std::unique_ptr<ComboBoxAttachment> analyzerModeAttachment;
    std::unique_ptr<ComboBoxAttachment> phaseModeAttachment;
    std::unique_ptr<ComboBoxAttachment> qualityModeAttachment;
    SliderAttachment analyzerSpeedAttachment;
    SliderAttachment analyzerRangeAttachment;
    SliderAttachment gainScaleAttachment;

    juce::Component bandPanel;
    juce::Label bandLabel;
    juce::Label typeLabel;
    juce::Label frequencyLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label rangeLabel;
    juce::Label thresholdLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label sourceLabel;
    juce::Label dynamicModeLabel;
    juce::ComboBox bandTypeBox;
    juce::ComboBox sidechainSourceBox;
    juce::ComboBox dynamicModeBox;
    juce::Slider frequencySlider;
    juce::Slider gainSlider;
    juce::Slider qSlider;
    juce::Slider dynamicRangeSlider;
    juce::Slider thresholdSlider;
    juce::Slider attackSlider;
    juce::Slider releaseSlider;
    juce::TextButton deleteButton { "DEL" };

    std::unique_ptr<ComboBoxAttachment> bandTypeAttachment;
    std::unique_ptr<ComboBoxAttachment> sidechainAttachment;
    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> qAttachment;
    std::unique_ptr<SliderAttachment> dynamicRangeAttachment;
    std::unique_ptr<SliderAttachment> thresholdAttachment;
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    std::unique_ptr<ComboBoxAttachment> dynamicModeAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PrismEqAudioProcessorEditor)
};
}
