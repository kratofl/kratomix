#include "PluginEditor.h"

#include <algorithm>

namespace
{
const auto background = juce::Colour::fromRGB(16, 18, 21);
const auto panel = juce::Colour::fromRGB(27, 30, 34);
const auto grid = juce::Colour::fromRGB(48, 53, 59);
const auto text = juce::Colour::fromRGB(224, 226, 222);
const auto muted = juce::Colour::fromRGB(137, 145, 148);
const auto amber = juce::Colour::fromRGB(232, 174, 92);
const auto cyan = juce::Colour::fromRGB(87, 190, 202);
}

namespace kratomix
{
AlignmentDisplay::AlignmentDisplay(PhaseAlignAudioProcessor& processorToUse)
    : processor(processorToUse)
{
    setComponentID("phaseAlignDisplay");
}

void AlignmentDisplay::paint(juce::Graphics& graphics)
{
    const auto bounds = getLocalBounds().toFloat();
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(bounds, 7.0f);
    graphics.setColour(grid);
    for (int division = 1; division < 4; ++division)
    {
        const auto x = bounds.getX() + bounds.getWidth() * static_cast<float>(division) / 4.0f;
        graphics.drawVerticalLine(static_cast<int>(x), bounds.getY() + 10.0f, bounds.getBottom() - 10.0f);
    }
    graphics.drawHorizontalLine(static_cast<int>(bounds.getCentreY()), bounds.getX() + 8.0f, bounds.getRight() - 8.0f);

    phase_align::AnalysisFrame frame;
    processor.copyAnalysisFrame(frame);
    if (! frame.valid)
    {
        graphics.setColour(muted);
        graphics.setFont(juce::FontOptions(13.0f));
        graphics.drawText("Play both paths, then press Auto Align",
                          getLocalBounds(), juce::Justification::centred);
        return;
    }

    auto peak = 1.0e-4f;
    for (size_t index = 0; index < frame.moving.size(); ++index)
        peak = std::max({ peak, std::abs(frame.moving[index]), std::abs(frame.reference[index]) });

    const auto makePath = [&](const auto& samples)
    {
        juce::Path path;
        for (int index = 0; index < phase_align::AnalysisFrame::sampleCount; ++index)
        {
            const auto x = juce::jmap(static_cast<float>(index), 0.0f,
                                     static_cast<float>(phase_align::AnalysisFrame::sampleCount - 1),
                                     bounds.getX() + 8.0f, bounds.getRight() - 8.0f);
            const auto y = bounds.getCentreY()
                           - samples[static_cast<size_t>(index)] / peak * bounds.getHeight() * 0.42f;
            if (index == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }
        return path;
    };

    graphics.setColour(cyan.withAlpha(0.85f));
    graphics.strokePath(makePath(frame.reference), juce::PathStrokeType(1.5f));
    graphics.setColour(amber.withAlpha(0.9f));
    graphics.strokePath(makePath(frame.moving), juce::PathStrokeType(1.5f));
}

PhaseAlignAudioProcessorEditor::PhaseAlignAudioProcessorEditor(PhaseAlignAudioProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit),
      pluginProcessor(processorToEdit),
      alignmentDisplay(processorToEdit)
{
    titleLabel.setText("KRATOMIX  PHASE ALIGN", juce::dontSendNotification);
    titleLabel.setColour(juce::Label::textColourId, text);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    addAndMakeVisible(titleLabel);
    addAndMakeVisible(alignmentDisplay);

    offsetCaption.setText("SIGNED OFFSET", juce::dontSendNotification);
    offsetCaption.setColour(juce::Label::textColourId, muted);
    offsetCaption.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    addAndMakeVisible(offsetCaption);

    offsetReadout.setColour(juce::Label::textColourId, text);
    offsetReadout.setJustificationType(juce::Justification::centredRight);
    offsetReadout.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    addAndMakeVisible(offsetReadout);

    offsetSlider.setComponentID("phaseAlignOffset");
    offsetSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    offsetSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 88, 26);
    offsetSlider.setTextValueSuffix(" ms");
    offsetSlider.setDoubleClickReturnValue(true, 0.0);
    offsetSlider.setColour(juce::Slider::trackColourId, amber);
    addAndMakeVisible(offsetSlider);

    configureNudgeButton(coarseDownButton, "-1 sample", -1.0f);
    configureNudgeButton(fineDownButton, "-0.1", -0.1f);
    configureNudgeButton(fineUpButton, "+0.1", 0.1f);
    configureNudgeButton(coarseUpButton, "+1 sample", 1.0f);

    autoAlignButton.setButtonText("AUTO ALIGN");
    autoAlignButton.setComponentID("phaseAlignAuto");
    autoAlignButton.setColour(juce::TextButton::buttonColourId, amber.darker(0.35f));
    autoAlignButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(24, 20, 14));
    autoAlignButton.onClick = [this] { pluginProcessor.requestAutoAlign(); };
    addAndMakeVisible(autoAlignButton);

    polarityButton.setButtonText("Invert Polarity");
    polarityButton.setComponentID("phaseAlignPolarity");
    lockButton.setButtonText("Lock Result");
    lockButton.setComponentID("phaseAlignLock");
    bypassButton.setButtonText("Bypass");
    bypassButton.setComponentID("phaseAlignBypass");
    for (auto* button : { &polarityButton, &lockButton, &bypassButton })
    {
        button->setColour(juce::ToggleButton::textColourId, text);
        addAndMakeVisible(*button);
    }

    auditionModeBox.addItemList(phase_align::auditionModeChoices(), 1);
    auditionModeBox.setComponentID("phaseAlignAuditionMode");
    auditionModeBox.setColour(juce::ComboBox::backgroundColourId, panel.brighter(0.08f));
    auditionModeBox.setColour(juce::ComboBox::textColourId, text);
    addAndMakeVisible(auditionModeBox);

    for (auto* label : { &correlationLabel, &confidenceLabel, &statusLabel })
    {
        label->setColour(juce::Label::textColourId, muted);
        label->setFont(juce::FontOptions(12.0f, juce::Font::bold));
        addAndMakeVisible(*label);
    }
    statusLabel.setJustificationType(juce::Justification::centredRight);

    offsetAttachment = std::make_unique<SliderAttachment>(pluginProcessor.parameters,
                                                          phase_align::offsetId, offsetSlider);
    polarityAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters,
                                                            phase_align::polarityId, polarityButton);
    lockAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters,
                                                        phase_align::lockId, lockButton);
    bypassAttachment = std::make_unique<ButtonAttachment>(pluginProcessor.parameters,
                                                          phase_align::bypassId, bypassButton);
    auditionAttachment = std::make_unique<ComboBoxAttachment>(pluginProcessor.parameters,
                                                              phase_align::auditionModeId,
                                                              auditionModeBox);

    setSize(760, 430);
    startTimerHz(20);
    timerCallback();
}

void PhaseAlignAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(background);
    graphics.setColour(panel);
    graphics.fillRoundedRectangle(getLocalBounds().toFloat().reduced(12.0f), 9.0f);
    graphics.setColour(grid);
    graphics.drawHorizontalLine(58, 24.0f, static_cast<float>(getWidth() - 24));

    graphics.setColour(cyan);
    graphics.fillEllipse(28.0f, 31.0f, 8.0f, 8.0f);
    graphics.setColour(amber);
    graphics.fillEllipse(45.0f, 31.0f, 8.0f, 8.0f);
}

void PhaseAlignAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds().reduced(24);
    auto header = bounds.removeFromTop(28);
    titleLabel.setBounds(header.removeFromLeft(320));
    statusLabel.setBounds(header.removeFromRight(240));
    bounds.removeFromTop(18);
    alignmentDisplay.setBounds(bounds.removeFromTop(155));
    bounds.removeFromTop(14);

    auto captionRow = bounds.removeFromTop(22);
    offsetCaption.setBounds(captionRow.removeFromLeft(160));
    offsetReadout.setBounds(captionRow.removeFromRight(300));
    offsetSlider.setBounds(bounds.removeFromTop(34));
    bounds.removeFromTop(8);

    auto nudgeRow = bounds.removeFromTop(28);
    coarseDownButton.setBounds(nudgeRow.removeFromLeft(92));
    nudgeRow.removeFromLeft(6);
    fineDownButton.setBounds(nudgeRow.removeFromLeft(72));
    nudgeRow.removeFromLeft(6);
    fineUpButton.setBounds(nudgeRow.removeFromLeft(72));
    nudgeRow.removeFromLeft(6);
    coarseUpButton.setBounds(nudgeRow.removeFromLeft(92));
    autoAlignButton.setBounds(nudgeRow.removeFromRight(150));
    bounds.removeFromTop(14);

    auto controlRow = bounds.removeFromTop(30);
    auditionModeBox.setBounds(controlRow.removeFromLeft(150));
    controlRow.removeFromLeft(14);
    polarityButton.setBounds(controlRow.removeFromLeft(140));
    lockButton.setBounds(controlRow.removeFromLeft(120));
    bypassButton.setBounds(controlRow.removeFromLeft(90));
    bounds.removeFromTop(12);

    auto meterRow = bounds.removeFromTop(24);
    correlationLabel.setBounds(meterRow.removeFromLeft(220));
    confidenceLabel.setBounds(meterRow.removeFromLeft(220));
}

void PhaseAlignAudioProcessorEditor::timerCallback()
{
    const auto samples = pluginProcessor.getOffsetSamples();
    const auto milliseconds = pluginProcessor.parameters.getRawParameterValue(phase_align::offsetId)->load();
    offsetReadout.setText(juce::String(samples, 2) + " samples   "
                              + juce::String(milliseconds, 4) + " ms",
                          juce::dontSendNotification);
    correlationLabel.setText("CORRELATION  " + juce::String(pluginProcessor.getCorrelation(), 3),
                             juce::dontSendNotification);
    confidenceLabel.setText("CONFIDENCE  " + juce::String(pluginProcessor.getConfidence() * 100.0f, 0) + "%",
                            juce::dontSendNotification);
    statusLabel.setText(statusText(pluginProcessor.getAnalysisStatus()), juce::dontSendNotification);
    alignmentDisplay.repaint();
}

void PhaseAlignAudioProcessorEditor::configureNudgeButton(juce::TextButton& button,
                                                          const juce::String& label,
                                                          float samples)
{
    button.setButtonText(label);
    button.onClick = [this, samples] { pluginProcessor.nudgeOffsetBySamples(samples); };
    button.setColour(juce::TextButton::buttonColourId, panel.brighter(0.12f));
    addAndMakeVisible(button);
}

juce::String PhaseAlignAudioProcessorEditor::statusText(phase_align::AnalysisStatus status)
{
    switch (status)
    {
        case phase_align::AnalysisStatus::capturing: return "CAPTURING";
        case phase_align::AnalysisStatus::analyzing: return "ANALYZING";
        case phase_align::AnalysisStatus::ready: return "RESULT LOCKED";
        case phase_align::AnalysisStatus::lowConfidence: return "LOW CONFIDENCE";
        case phase_align::AnalysisStatus::noSidechain: return "SELECT A SIDECHAIN";
        case phase_align::AnalysisStatus::idle: return "READY";
    }
    return {};
}
}
