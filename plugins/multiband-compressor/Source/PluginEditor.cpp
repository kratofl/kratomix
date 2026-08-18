#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1160;
constexpr int editorHeight = 660;
constexpr float knobStartAngle = juce::MathConstants<float>::pi * 1.2f;
constexpr float knobEndAngle = juce::MathConstants<float>::pi * 2.8f;

juce::Colour shellColour()
{
    return juce::Colour::fromRGB(8, 9, 11);
}

juce::Colour panelColour()
{
    return juce::Colour::fromRGB(18, 19, 22);
}

juce::Colour accentColour()
{
    return juce::Colour::fromRGB(241, 194, 124);
}

juce::Colour captionColour()
{
    return juce::Colour::fromRGB(160, 146, 120);
}

void styleCaption(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, captionColour());
}

void styleButton(juce::Button& button)
{
    button.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(37, 29, 23));
    button.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromRGB(71, 48, 25));
    button.setColour(juce::TextButton::textColourOffId, accentColour());
    button.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
}
}

namespace kratomix
{
MultibandCompressorAudioProcessorEditor::MultibandCompressorAudioProcessorEditor(MultibandCompressorAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      pluginProcessor(processorToEdit),
      outputMeter([this] { return pluginProcessor.getOutputLevel(); })
{
    brandLabel.setText("Kratomix", juce::dontSendNotification);
    brandLabel.setJustificationType(juce::Justification::centredLeft);
    brandLabel.setFont(juce::FontOptions(28.0f).withName("Snell Roundhand").withStyle("Bold Italic"));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(246, 226, 192));
    addAndMakeVisible(brandLabel);

    titleLabel.setText("MULTIBAND COMPRESSOR", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, accentColour());
    addAndMakeVisible(titleLabel);

    versionLabel.setText(juce::String("v") + KRATOMIX_PLUGIN_VERSION_STRING, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    versionLabel.setColour(juce::Label::textColourId, captionColour());
    addAndMakeVisible(versionLabel);

    graph.setComponentID("multibandGraph");
    graph.attachState(pluginProcessor.parameters);
    graph.attachAnalyzerReader([this](MultibandAnalyzerFrame& frame)
    {
        pluginProcessor.copyAnalyzerFrame(frame);
    });
    graph.onSelectedBandChanged = [this](int selectedBand)
    {
        rebuildBandAttachments(selectedBand);
    };
    addAndMakeVisible(graph);

    configureGlobalControls();
    configureBandControls();
    rebuildBandAttachments(graph.getSelectedBand());

    setSize(editorWidth, editorHeight);
}

MultibandCompressorAudioProcessorEditor::~MultibandCompressorAudioProcessorEditor()
{
    for (auto* slider : { &inputSlider, &outputSlider, &mixSlider, &frequencySlider, &widthSlider, &thresholdSlider, &rangeSlider, &ratioSlider, &attackSlider, &releaseSlider, &kneeSlider, &makeupSlider, &stereoLinkSlider })
        slider->setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
}

void MultibandCompressorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(shellColour());

    auto bounds = getLocalBounds().toFloat().reduced(12.0f);
    g.setColour(panelColour());
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(juce::Colour::fromRGB(42, 36, 28));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

    auto top = bounds.removeFromTop(58.0f).reduced(14.0f, 8.0f);
    g.setColour(juce::Colours::black.withAlpha(0.18f));
    g.fillRoundedRectangle(top, 6.0f);

    auto side = getLocalBounds().toFloat().reduced(12.0f).removeFromRight(88.0f).withTrimmedTop(70.0f).reduced(10.0f, 0.0f);
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(side, 6.0f);

    const auto band = bandPanel.getBounds().toFloat();
    if (! band.isEmpty())
    {
        g.setColour(juce::Colour::fromRGB(11, 12, 14).withAlpha(0.96f));
        g.fillRoundedRectangle(band, 6.0f);
        g.setColour(juce::Colour::fromRGB(48, 40, 30));
        g.drawRoundedRectangle(band, 6.0f, 1.0f);
    }
}

void MultibandCompressorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto top = bounds.removeFromTop(46);
    versionLabel.setBounds(top.removeFromRight(80).reduced(0, 8));
    top.removeFromRight(8);
    brandLabel.setBounds(top.removeFromLeft(150));
    titleLabel.setBounds(top.removeFromLeft(245));
    top.removeFromLeft(12);

    auto placeTopSlider = [](juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto slot = area.removeFromLeft(96);
        label.setBounds(slot.removeFromLeft(36).reduced(0, 8));
        slider.setBounds(slot.reduced(3, 8));
    };

    placeTopSlider(top, inputLabel, inputSlider);
    placeTopSlider(top, outputLabel, outputSlider);
    placeTopSlider(top, mixLabel, mixSlider);
    analyzerLabel.setBounds(top.removeFromLeft(58).reduced(0, 8));
    analyzerBox.setBounds(top.removeFromLeft(136).reduced(0, 8));
    lookaheadLabel.setBounds(top.removeFromLeft(72).reduced(0, 8));
    lookaheadBox.setBounds(top.removeFromLeft(78).reduced(0, 8));

    bounds.removeFromTop(14);
    auto outputArea = bounds.removeFromRight(86);
    meterLabel.setBounds(outputArea.removeFromTop(24));
    bypassButton.setBounds(outputArea.removeFromBottom(90).reduced(0, 4));
    outputMeter.setBounds(outputArea.reduced(4, 8));

    auto bandArea = bounds.removeFromBottom(168);
    bounds.removeFromBottom(10);
    graph.setBounds(bounds);
    bandPanel.setBounds(bandArea.reduced(0, 2));

    auto panel = bandPanel.getLocalBounds().reduced(12, 10);
    auto left = panel.removeFromLeft(118);
    bandLabel.setBounds(left.removeFromTop(22));
    deleteButton.setBounds(left.removeFromTop(30).reduced(0, 3));
    soloButton.setBounds(left.removeFromTop(30).reduced(0, 3));
    auditionButton.setBounds(left.removeFromTop(30).reduced(0, 3));

    auto modeArea = panel.removeFromLeft(128);
    modeLabel.setBounds(modeArea.removeFromTop(18));
    modeBox.setBounds(modeArea.removeFromTop(30));
    modeArea.removeFromTop(8);
    detectorLabel.setBounds(modeArea.removeFromTop(18));
    detectorBox.setBounds(modeArea.removeFromTop(30));

    const auto sliderWidth = juce::jmax(68, panel.getWidth() / 10);
    auto placeBandSlider = [sliderWidth](juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto slot = area.removeFromLeft(sliderWidth).reduced(5, 0);
        label.setBounds(slot.removeFromTop(18));
        slider.setBounds(slot.removeFromTop(118));
    };

    placeBandSlider(panel, frequencyLabel, frequencySlider);
    placeBandSlider(panel, widthLabel, widthSlider);
    placeBandSlider(panel, thresholdLabel, thresholdSlider);
    placeBandSlider(panel, rangeLabel, rangeSlider);
    placeBandSlider(panel, ratioLabel, ratioSlider);
    placeBandSlider(panel, attackLabel, attackSlider);
    placeBandSlider(panel, releaseLabel, releaseSlider);
    placeBandSlider(panel, kneeLabel, kneeSlider);
    placeBandSlider(panel, makeupLabel, makeupSlider);
    placeBandSlider(panel, stereoLinkLabel, stereoLinkSlider);
}

void MultibandCompressorAudioProcessorEditor::configureGlobalControls()
{
    configureLinearSlider(inputSlider, inputLabel, "IN", 0.0);
    configureLinearSlider(outputSlider, outputLabel, "OUT", 0.0);
    configureLinearSlider(mixSlider, mixLabel, "MIX", 1.0);
    mixSlider.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0), 0); };
    mixSlider.valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.0, 1.0, text.getDoubleValue() / 100.0); };

    analyzerLabel.setText("VIEW", juce::dontSendNotification);
    styleCaption(analyzerLabel);
    addAndMakeVisible(analyzerLabel);
    configureCombo(analyzerBox, multiband::analyzerModeChoices());
    addAndMakeVisible(analyzerBox);

    lookaheadLabel.setText("LOOK", juce::dontSendNotification);
    styleCaption(lookaheadLabel);
    addAndMakeVisible(lookaheadLabel);
    configureCombo(lookaheadBox, multiband::lookaheadModeChoices());
    addAndMakeVisible(lookaheadBox);

    bypassButton.setLookAndFeel(&rackLookAndFeel);
    addAndMakeVisible(bypassButton);

    meterLabel.setText("OUT", juce::dontSendNotification);
    meterLabel.setJustificationType(juce::Justification::centred);
    meterLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    meterLabel.setColour(juce::Label::textColourId, captionColour());
    addAndMakeVisible(meterLabel);
    addAndMakeVisible(outputMeter);

    inputAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::inputGainId, inputSlider);
    outputAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::outputGainId, outputSlider);
    mixAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::mixId, mixSlider);
    analyzerAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, multiband::analyzerModeId, analyzerBox);
    lookaheadAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, multiband::lookaheadModeId, lookaheadBox);
    bypassAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters, multiband::bypassId, bypassButton);
}

void MultibandCompressorAudioProcessorEditor::configureBandControls()
{
    addAndMakeVisible(bandPanel);

    bandLabel.setJustificationType(juce::Justification::centredLeft);
    bandLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    bandLabel.setColour(juce::Label::textColourId, accentColour());
    bandPanel.addAndMakeVisible(bandLabel);

    for (auto* button : { static_cast<juce::Button*>(&deleteButton),
                          static_cast<juce::Button*>(&soloButton),
                          static_cast<juce::Button*>(&auditionButton) })
    {
        styleButton(*button);
        bandPanel.addAndMakeVisible(*button);
    }

    for (auto* label : { &modeLabel, &detectorLabel, &frequencyLabel, &widthLabel, &thresholdLabel, &rangeLabel, &ratioLabel, &attackLabel, &releaseLabel, &kneeLabel, &makeupLabel, &stereoLinkLabel })
    {
        styleCaption(*label);
        bandPanel.addAndMakeVisible(*label);
    }

    modeLabel.setText("MODE", juce::dontSendNotification);
    detectorLabel.setText("SIDE CHAIN", juce::dontSendNotification);
    frequencyLabel.setText("FREQ", juce::dontSendNotification);
    widthLabel.setText("WIDTH", juce::dontSendNotification);
    thresholdLabel.setText("THRESHOLD", juce::dontSendNotification);
    rangeLabel.setText("RANGE", juce::dontSendNotification);
    ratioLabel.setText("RATIO", juce::dontSendNotification);
    attackLabel.setText("ATTACK", juce::dontSendNotification);
    releaseLabel.setText("RELEASE", juce::dontSendNotification);
    kneeLabel.setText("KNEE", juce::dontSendNotification);
    makeupLabel.setText("OUTPUT", juce::dontSendNotification);
    stereoLinkLabel.setText("ST LINK", juce::dontSendNotification);

    configureCombo(modeBox, multiband::bandModeChoices());
    configureCombo(detectorBox, multiband::detectorSourceChoices());
    bandPanel.addAndMakeVisible(modeBox);
    bandPanel.addAndMakeVisible(detectorBox);

    configureRotarySlider(frequencySlider, frequencyLabel, "frequency", 1000.0);
    configureRotarySlider(widthSlider, widthLabel, "width", 2.0);
    configureRotarySlider(thresholdSlider, thresholdLabel, "threshold", -24.0);
    configureRotarySlider(rangeSlider, rangeLabel, "range", -6.0);
    configureRotarySlider(ratioSlider, ratioLabel, "ratio", 2.0);
    configureRotarySlider(attackSlider, attackLabel, "attack", 20.0);
    configureRotarySlider(releaseSlider, releaseLabel, "release", 120.0);
    configureRotarySlider(kneeSlider, kneeLabel, "knee", 6.0);
    configureRotarySlider(makeupSlider, makeupLabel, "output", 0.0);
    configureRotarySlider(stereoLinkSlider, stereoLinkLabel, "stereoLink", 1.0);

    ratioSlider.textFromValueFunction = [](double value) { return juce::String(value, value < 10.0 ? 2 : 1) + ":1"; };
    frequencySlider.textFromValueFunction = [](double value) { return juce::String(static_cast<int>(std::round(value))); };
    widthSlider.textFromValueFunction = [](double value) { return juce::String(value, 2); };
    stereoLinkSlider.textFromValueFunction = [](double value) { return juce::String(std::round(value * 100.0), 0); };
    stereoLinkSlider.valueFromTextFunction = [](const juce::String& text) { return juce::jlimit(0.0, 1.0, text.getDoubleValue() / 100.0); };

    deleteButton.setComponentID("deleteBand");
    deleteButton.onClick = [this]
    {
        graph.deleteSelectedBand();
    };
}

void MultibandCompressorAudioProcessorEditor::configureRotarySlider(juce::Slider& slider,
                                                                    juce::Label&,
                                                                    const juce::String& componentId,
                                                                    double doubleClickValue)
{
    slider.setComponentID(componentId);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(knobStartAngle, knobEndAngle, true);
    slider.setLookAndFeel(&rackLookAndFeel);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    slider.setTextBoxIsEditable(true);
    slider.setDoubleClickReturnValue(true, doubleClickValue);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(250, 231, 202));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(28, 23, 21, 160));
    bandPanel.addAndMakeVisible(slider);
}

void MultibandCompressorAudioProcessorEditor::configureLinearSlider(juce::Slider& slider,
                                                                    juce::Label& label,
                                                                    const juce::String& text,
                                                                    double doubleClickValue)
{
    label.setText(text, juce::dontSendNotification);
    styleCaption(label);
    addAndMakeVisible(label);

    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 42, 20);
    slider.setDoubleClickReturnValue(true, doubleClickValue);
    slider.setColour(juce::Slider::thumbColourId, accentColour());
    slider.setColour(juce::Slider::trackColourId, accentColour().withAlpha(0.55f));
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(34, 35, 38));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(250, 231, 202));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(28, 23, 21, 160));
    addAndMakeVisible(slider);
}

void MultibandCompressorAudioProcessorEditor::configureCombo(juce::ComboBox& box, const juce::StringArray& choices)
{
    box.addItemList(choices, 1);
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(27, 24, 22));
    box.setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(250, 231, 202));
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(74, 52, 36));
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(250, 231, 202));
}

void MultibandCompressorAudioProcessorEditor::rebuildBandAttachments(int zeroBasedBandIndex)
{
    modeAttachment.reset();
    detectorAttachment.reset();
    frequencyAttachment.reset();
    widthAttachment.reset();
    thresholdAttachment.reset();
    rangeAttachment.reset();
    ratioAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    kneeAttachment.reset();
    makeupAttachment.reset();
    stereoLinkAttachment.reset();
    soloAttachment.reset();
    auditionAttachment.reset();

    if (zeroBasedBandIndex < 0)
    {
        bandLabel.setText("NO BAND", juce::dontSendNotification);
        setBandControlsEnabled(false);
        return;
    }

    const auto index = juce::jlimit(0, multiband::maxBands - 1, zeroBasedBandIndex);
    const auto idx = static_cast<size_t>(index);

    bandLabel.setText(multiband::bandLabel(index).toUpperCase(), juce::dontSendNotification);

    modeAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, multiband::bandModeIds[idx], modeBox);
    detectorAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, multiband::bandDetectorSourceIds[idx], detectorBox);
    frequencyAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandFrequencyIds[idx], frequencySlider);
    widthAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandWidthIds[idx], widthSlider);
    thresholdAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandThresholdIds[idx], thresholdSlider);
    rangeAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandRangeIds[idx], rangeSlider);
    ratioAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandRatioIds[idx], ratioSlider);
    attackAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandAttackIds[idx], attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandReleaseIds[idx], releaseSlider);
    kneeAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandKneeIds[idx], kneeSlider);
    makeupAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandMakeupIds[idx], makeupSlider);
    stereoLinkAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, multiband::bandStereoLinkIds[idx], stereoLinkSlider);
    soloAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters, multiband::bandSoloIds[idx], soloButton);
    auditionAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters, multiband::bandAuditionIds[idx], auditionButton);
    setBandControlsEnabled(true);
}

void MultibandCompressorAudioProcessorEditor::setBandControlsEnabled(bool enabled)
{
    for (auto* component : { static_cast<juce::Component*>(&modeBox),
                             static_cast<juce::Component*>(&detectorBox),
                             static_cast<juce::Component*>(&frequencySlider),
                             static_cast<juce::Component*>(&widthSlider),
                             static_cast<juce::Component*>(&thresholdSlider),
                             static_cast<juce::Component*>(&rangeSlider),
                             static_cast<juce::Component*>(&ratioSlider),
                             static_cast<juce::Component*>(&attackSlider),
                             static_cast<juce::Component*>(&releaseSlider),
                             static_cast<juce::Component*>(&kneeSlider),
                             static_cast<juce::Component*>(&makeupSlider),
                             static_cast<juce::Component*>(&stereoLinkSlider),
                             static_cast<juce::Component*>(&deleteButton),
                             static_cast<juce::Component*>(&soloButton),
                             static_cast<juce::Component*>(&auditionButton) })
        component->setEnabled(enabled);
}
}
