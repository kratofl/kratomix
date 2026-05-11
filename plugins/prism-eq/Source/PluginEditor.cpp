#include "PluginEditor.h"

namespace
{
constexpr int editorWidth = 1180;
constexpr int editorHeight = 620;
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

void styleCaption(juce::Label& label)
{
    label.setJustificationType(juce::Justification::centredLeft);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 146, 120));
}

void styleComboBox(juce::ComboBox& box)
{
    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour::fromRGB(27, 24, 22));
    box.setColour(juce::ComboBox::textColourId, juce::Colour::fromRGB(250, 231, 202));
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromRGB(74, 52, 36));
    box.setColour(juce::ComboBox::arrowColourId, juce::Colour::fromRGB(250, 231, 202));
}
}

namespace kratomix
{
PrismEqAudioProcessorEditor::PrismEqAudioProcessorEditor(PrismEqAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      pluginProcessor(processorToEdit),
      outputMeter([this] { return pluginProcessor.getOutputLevel(); }),
      bypassAttachment(pluginProcessor.parameters, "bypass", bypassButton),
      outputAttachment(pluginProcessor.parameters, "outputGain", outputSlider),
      analyzerModeAttachment(pluginProcessor.parameters, "analyzerMode", analyzerModeBox),
      phaseModeAttachment(pluginProcessor.parameters, "phaseMode", phaseModeBox),
      qualityModeAttachment(pluginProcessor.parameters, "qualityMode", qualityModeBox),
      analyzerSpeedAttachment(pluginProcessor.parameters, "analyzerSpeed", analyzerSpeedSlider),
      analyzerRangeAttachment(pluginProcessor.parameters, "analyzerRange", analyzerRangeSlider),
      gainScaleAttachment(pluginProcessor.parameters, "gainScale", gainScaleSlider)
{
    brandLabel.setText("Kratomix", juce::dontSendNotification);
    brandLabel.setJustificationType(juce::Justification::centredLeft);
    brandLabel.setFont(juce::FontOptions(28.0f).withName("Snell Roundhand").withStyle("Bold Italic"));
    brandLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(246, 226, 192));
    addAndMakeVisible(brandLabel);

    titleLabel.setText("PRISM EQ", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, accentColour());
    addAndMakeVisible(titleLabel);

    analyzerLabel.setText("VIEW", juce::dontSendNotification);
    styleCaption(analyzerLabel);
    addAndMakeVisible(analyzerLabel);

    analyzerModeBox.addItemList(prism::analyzerModeChoices(), 1);
    styleComboBox(analyzerModeBox);
    addAndMakeVisible(analyzerModeBox);

    phaseLabel.setText("PHASE", juce::dontSendNotification);
    styleCaption(phaseLabel);
    addAndMakeVisible(phaseLabel);

    phaseModeBox.addItemList(prism::phaseModeChoices(), 1);
    styleComboBox(phaseModeBox);
    addAndMakeVisible(phaseModeBox);

    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    styleCaption(qualityLabel);
    addAndMakeVisible(qualityLabel);

    qualityModeBox.addItemList(prism::qualityModeChoices(), 1);
    styleComboBox(qualityModeBox);
    addAndMakeVisible(qualityModeBox);

    liveLabel.setText("LIVE", juce::dontSendNotification);
    styleCaption(liveLabel);
    addAndMakeVisible(liveLabel);

    detectorLabel.setText("DET --.- dB", juce::dontSendNotification);
    styleCaption(detectorLabel);
    addAndMakeVisible(detectorLabel);

    movementLabel.setText("MOVE --.- dB", juce::dontSendNotification);
    styleCaption(movementLabel);
    addAndMakeVisible(movementLabel);

    for (auto* label : { &speedLabel, &rangeLabelGlobal, &scaleLabel })
    {
        styleCaption(*label);
        addAndMakeVisible(*label);
    }

    speedLabel.setText("SPEED", juce::dontSendNotification);
    rangeLabelGlobal.setText("RANGE", juce::dontSendNotification);
    scaleLabel.setText("SCALE", juce::dontSendNotification);

    analyzerSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    analyzerRangeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainScaleSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    for (auto* slider : { &analyzerSpeedSlider, &analyzerRangeSlider, &gainScaleSlider })
    {
        slider->setColour(juce::Slider::thumbColourId, accentColour());
        slider->setColour(juce::Slider::trackColourId, accentColour().withAlpha(0.55f));
        slider->setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(34, 35, 38));
        addAndMakeVisible(*slider);
    }

    graph.attachState(pluginProcessor.parameters);
    graph.attachAnalyzerReader([this](PrismAnalyzerFrame& frame)
    {
        pluginProcessor.copyAnalyzerFrame(frame);
    });
    graph.onSelectedBandChanged = [this](int selectedBand)
    {
        rebuildBandAttachments(selectedBand);
    };
    addAndMakeVisible(graph);

    outputSlider.setSliderStyle(juce::Slider::LinearVertical);
    outputSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 22);
    outputSlider.setDoubleClickReturnValue(true, 0.0);
    outputSlider.setColour(juce::Slider::thumbColourId, accentColour());
    outputSlider.setColour(juce::Slider::trackColourId, accentColour().withAlpha(0.55f));
    outputSlider.setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(34, 35, 38));
    outputSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(250, 231, 202));
    outputSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    outputSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(28, 23, 21, 160));
    outputSlider.textFromValueFunction = [](double value)
    {
        return juce::String(value, 1);
    };
    outputSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.getDoubleValue();
    };
    addAndMakeVisible(outputSlider);

    bypassButton.setLookAndFeel(&lookAndFeel);
    addAndMakeVisible(bypassButton);

    meterLabel.setText("OUT", juce::dontSendNotification);
    meterLabel.setJustificationType(juce::Justification::centred);
    meterLabel.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    meterLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(160, 146, 120));
    addAndMakeVisible(meterLabel);
    addAndMakeVisible(outputMeter);

    configureBandControls();
    rebuildBandAttachments(0);

    setSize(editorWidth, editorHeight);
    startTimerHz(20);
}

void PrismEqAudioProcessorEditor::paint(juce::Graphics& g)
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

    auto side = getLocalBounds().toFloat().reduced(12.0f).removeFromRight(96.0f).withTrimmedTop(70.0f).reduced(10.0f, 0.0f);
    g.setColour(juce::Colours::black.withAlpha(0.22f));
    g.fillRoundedRectangle(side, 6.0f);

    auto band = bandPanel.getBounds().toFloat();
    if (! band.isEmpty())
    {
        g.setColour(juce::Colour::fromRGB(11, 12, 14).withAlpha(0.96f));
        g.fillRoundedRectangle(band, 6.0f);
        g.setColour(juce::Colour::fromRGB(48, 40, 30));
        g.drawRoundedRectangle(band, 6.0f, 1.0f);
    }
}

void PrismEqAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto top = bounds.removeFromTop(46);
    brandLabel.setBounds(top.removeFromLeft(150));
    titleLabel.setBounds(top.removeFromLeft(120));
    top.removeFromLeft(12);
    analyzerLabel.setBounds(top.removeFromLeft(38).reduced(0, 8));
    analyzerModeBox.setBounds(top.removeFromLeft(140).reduced(0, 8));
    top.removeFromLeft(10);
    phaseLabel.setBounds(top.removeFromLeft(46).reduced(0, 8));
    phaseModeBox.setBounds(top.removeFromLeft(118).reduced(0, 8));
    top.removeFromLeft(10);
    qualityLabel.setBounds(top.removeFromLeft(54).reduced(0, 8));
    qualityModeBox.setBounds(top.removeFromLeft(74).reduced(0, 8));
    top.removeFromLeft(10);
    liveLabel.setBounds(top.removeFromLeft(36).reduced(0, 8));
    detectorLabel.setBounds(top.removeFromLeft(82).reduced(0, 8));
    movementLabel.setBounds(top.removeFromLeft(92).reduced(0, 8));

    bounds.removeFromTop(14);

    auto outputArea = bounds.removeFromRight(90);
    meterLabel.setBounds(outputArea.removeFromTop(24));
    bypassButton.setBounds(outputArea.removeFromBottom(92).reduced(0, 4));
    outputSlider.setBounds(outputArea.removeFromBottom(210).reduced(8, 10));
    outputMeter.setBounds(outputArea.reduced(4, 8));

    auto bandArea = bounds.removeFromBottom(148);
    bounds.removeFromBottom(10);

    graph.setBounds(bounds.reduced(0, 0));

    auto analyzerControlArea = bandArea.removeFromTop(24);
    speedLabel.setBounds(analyzerControlArea.removeFromLeft(52));
    analyzerSpeedSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    rangeLabelGlobal.setBounds(analyzerControlArea.removeFromLeft(58));
    analyzerRangeSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    scaleLabel.setBounds(analyzerControlArea.removeFromLeft(54));
    gainScaleSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    bandArea.removeFromTop(6);

    bandPanel.setBounds(bandArea.reduced(0, 2));

    auto panelBounds = bandPanel.getLocalBounds().reduced(12, 10);
    auto left = panelBounds.removeFromLeft(118);
    bandLabel.setBounds(left.removeFromTop(22));
    dynamicModeLabel.setBounds(left.removeFromTop(18));
    dynamicModeBox.setBounds(left.removeFromTop(28));
    deleteButton.setBounds(left.removeFromTop(28).reduced(0, 3));

    auto typeArea = panelBounds.removeFromLeft(120);
    typeLabel.setBounds(typeArea.removeFromTop(18));
    bandTypeBox.setBounds(typeArea.removeFromTop(28));
    sourceLabel.setBounds(typeArea.removeFromTop(20).translated(0, 8));
    sidechainSourceBox.setBounds(typeArea.removeFromTop(28).translated(0, 8));

    const auto sliderWidth = juce::jmax(82, panelBounds.getWidth() / 7);
    auto placeSlider = [sliderWidth](juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
    {
        auto slot = area.removeFromLeft(sliderWidth).reduced(5, 0);
        label.setBounds(slot.removeFromTop(18));
        slider.setBounds(slot.removeFromTop(78));
    };

    placeSlider(panelBounds, frequencyLabel, frequencySlider);
    placeSlider(panelBounds, gainLabel, gainSlider);
    placeSlider(panelBounds, qLabel, qSlider);
    placeSlider(panelBounds, rangeLabel, dynamicRangeSlider);
    placeSlider(panelBounds, thresholdLabel, thresholdSlider);
    placeSlider(panelBounds, attackLabel, attackSlider);
    placeSlider(panelBounds, releaseLabel, releaseSlider);
}

void PrismEqAudioProcessorEditor::configureBandControls()
{
    addAndMakeVisible(bandPanel);

    bandLabel.setText("Band --", juce::dontSendNotification);
    bandLabel.setJustificationType(juce::Justification::centredLeft);
    bandLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    bandLabel.setColour(juce::Label::textColourId, accentColour());
    bandPanel.addAndMakeVisible(bandLabel);

    for (auto* label : { &typeLabel, &frequencyLabel, &gainLabel, &qLabel, &rangeLabel, &thresholdLabel, &attackLabel, &releaseLabel, &sourceLabel, &dynamicModeLabel })
    {
        styleCaption(*label);
        bandPanel.addAndMakeVisible(*label);
    }

    typeLabel.setText("TYPE", juce::dontSendNotification);
    frequencyLabel.setText("FREQ", juce::dontSendNotification);
    gainLabel.setText("GAIN", juce::dontSendNotification);
    qLabel.setText("Q", juce::dontSendNotification);
    rangeLabel.setText("RANGE", juce::dontSendNotification);
    thresholdLabel.setText("THR", juce::dontSendNotification);
    attackLabel.setText("ATK", juce::dontSendNotification);
    releaseLabel.setText("REL", juce::dontSendNotification);
    sourceLabel.setText("SOURCE", juce::dontSendNotification);
    dynamicModeLabel.setText("MODE", juce::dontSendNotification);

    bandTypeBox.addItemList(prism::bandTypeChoices(), 1);
    sidechainSourceBox.addItemList(prism::sidechainSourceChoices(), 1);
    dynamicModeBox.addItem("Static", 1);
    dynamicModeBox.addItem("Dynamic", 2);

    for (auto* box : { &bandTypeBox, &sidechainSourceBox, &dynamicModeBox })
    {
        styleComboBox(*box);
        bandPanel.addAndMakeVisible(*box);
    }

    frequencySlider.setRange(20.0, 20000.0, 0.01);
    frequencySlider.setSkewFactorFromMidPoint(1000.0);
    gainSlider.setRange(-30.0, 30.0, 0.1);
    qSlider.setRange(0.1, 40.0, 0.01);
    qSlider.setSkewFactorFromMidPoint(1.0);
    dynamicRangeSlider.setRange(0.0, 30.0, 0.1);
    thresholdSlider.setRange(-90.0, 0.0, 0.1);
    attackSlider.setRange(0.1, 200.0, 0.1);
    releaseSlider.setRange(5.0, 1000.0, 0.1);

    for (auto* slider : { &frequencySlider, &gainSlider, &qSlider, &dynamicRangeSlider, &thresholdSlider, &attackSlider, &releaseSlider })
    {
        configureValueSlider(*slider);
        bandPanel.addAndMakeVisible(*slider);
    }

    frequencySlider.textFromValueFunction = [](double value) { return juce::String(static_cast<int>(std::round(value))); };
    gainSlider.textFromValueFunction = [](double value) { return juce::String(value, 1); };
    dynamicRangeSlider.textFromValueFunction = [](double value) { return juce::String(value, 1); };
    thresholdSlider.textFromValueFunction = [](double value) { return juce::String(value, 1); };
    attackSlider.textFromValueFunction = [](double value) { return juce::String(value, 1); };
    releaseSlider.textFromValueFunction = [](double value) { return juce::String(value, 0); };

    deleteButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(37, 29, 23));
    deleteButton.setColour(juce::TextButton::textColourOffId, accentColour());
    deleteButton.onClick = [this]
    {
        graph.deleteSelectedBand();
        rebuildBandAttachments(0);
    };

    bandPanel.addAndMakeVisible(deleteButton);
}

void PrismEqAudioProcessorEditor::rebuildBandAttachments(int oneBasedBandIndex)
{
    bandTypeAttachment.reset();
    sidechainAttachment.reset();
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    dynamicRangeAttachment.reset();
    thresholdAttachment.reset();
    attackAttachment.reset();
    releaseAttachment.reset();
    dynamicModeAttachment.reset();

    if (oneBasedBandIndex <= 0)
    {
        bandLabel.setText("Band --", juce::dontSendNotification);
        setBandControlsEnabled(false);
        return;
    }

    const auto prefix = prism::bandPrefix(oneBasedBandIndex);
    bandLabel.setText("Band " + juce::String(oneBasedBandIndex).paddedLeft('0', 2), juce::dontSendNotification);

    bandTypeAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, prefix + "Type", bandTypeBox);
    sidechainAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, prefix + "SidechainSource", sidechainSourceBox);
    frequencyAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Frequency", frequencySlider);
    gainAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Gain", gainSlider);
    qAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Q", qSlider);
    dynamicRangeAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "DynamicRange", dynamicRangeSlider);
    thresholdAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Threshold", thresholdSlider);
    attackAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Attack", attackSlider);
    releaseAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters, prefix + "Release", releaseSlider);
    dynamicModeAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters, prefix + "DynamicEnabled", dynamicModeBox);

    setBandControlsEnabled(true);
}

void PrismEqAudioProcessorEditor::setBandControlsEnabled(bool shouldBeEnabled)
{
    for (auto* component : { static_cast<juce::Component*>(&bandTypeBox),
                             static_cast<juce::Component*>(&sidechainSourceBox),
                             static_cast<juce::Component*>(&frequencySlider),
                             static_cast<juce::Component*>(&gainSlider),
                             static_cast<juce::Component*>(&qSlider),
                             static_cast<juce::Component*>(&dynamicRangeSlider),
                             static_cast<juce::Component*>(&thresholdSlider),
                             static_cast<juce::Component*>(&attackSlider),
                             static_cast<juce::Component*>(&releaseSlider),
                             static_cast<juce::Component*>(&dynamicModeBox),
                             static_cast<juce::Component*>(&deleteButton) })
        component->setEnabled(shouldBeEnabled);
}

void PrismEqAudioProcessorEditor::configureValueSlider(juce::Slider& slider)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(knobStartAngle, knobEndAngle, true);
    slider.setLookAndFeel(&lookAndFeel);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    slider.setDoubleClickReturnValue(true, 0.0);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(250, 231, 202));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(28, 23, 21, 160));
}

void PrismEqAudioProcessorEditor::timerCallback()
{
    const auto selectedBand = graph.getSelectedBand();
    PrismAnalyzerFrame frame;
    pluginProcessor.copyAnalyzerFrame(frame);

    if (selectedBand <= 0)
    {
        detectorLabel.setText("DET --.- dB", juce::dontSendNotification);
        movementLabel.setText("MOVE --.- dB", juce::dontSendNotification);
        return;
    }

    const auto index = static_cast<size_t>(selectedBand - 1);
    detectorLabel.setText("DET " + juce::String(frame.detectorLevelDb[index], 1) + " dB", juce::dontSendNotification);
    movementLabel.setText("MOVE " + juce::String(frame.dynamicGainDb[index], 1) + " dB", juce::dontSendNotification);
}

}
