#include "PluginEditor.h"

namespace kratomix
{
namespace
{
    constexpr int editorWidth = 760;
    constexpr int editorHeight = 280;

    juce::Colour backgroundColour()
    {
        return juce::Colour::fromRGB(24, 23, 21);
    }

    juce::Colour panelColour()
    {
        return juce::Colour::fromRGB(43, 39, 34);
    }

    juce::Colour accentColour()
    {
        return juce::Colour::fromRGB(214, 159, 91);
    }
}

VelvetChannelAudioProcessorEditor::VelvetChannelAudioProcessorEditor(VelvetChannelAudioProcessor& p)
    : AudioProcessorEditor(&p),
      processor(p),
      inputAttachment(processor.parameters, ParamID::inputGain, inputSlider),
      driveAttachment(processor.parameters, ParamID::drive, driveSlider),
      highPassAttachment(processor.parameters, ParamID::highPass, highPassSlider),
      warmthAttachment(processor.parameters, ParamID::warmth, warmthSlider),
      presenceAttachment(processor.parameters, ParamID::presence, presenceSlider),
      airAttachment(processor.parameters, ParamID::air, airSlider),
      outputAttachment(processor.parameters, ParamID::outputGain, outputSlider)
{
    setSize(editorWidth, editorHeight);

    configureSlider(inputSlider, inputLabel, "Input");
    configureSlider(driveSlider, driveLabel, "Drive");
    configureSlider(highPassSlider, highPassLabel, "HPF");
    configureSlider(warmthSlider, warmthLabel, "Warmth");
    configureSlider(presenceSlider, presenceLabel, "Presence");
    configureSlider(airSlider, airLabel, "Air");
    configureSlider(outputSlider, outputLabel, "Output");
}

void VelvetChannelAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(backgroundColour());

    auto bounds = getLocalBounds().toFloat().reduced(18.0f);
    g.setColour(panelColour());
    g.fillRoundedRectangle(bounds, 7.0f);

    g.setColour(accentColour());
    g.setFont(juce::FontOptions(25.0f, juce::Font::bold));
    g.drawText("Kratomix Velvet Channel", getLocalBounds().reduced(28).removeFromTop(44), juce::Justification::centredLeft);

    g.setColour(juce::Colour::fromRGB(176, 169, 156));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("Warm color, musical EQ, clean gain staging", 30, 58, 360, 24, juce::Justification::centredLeft);
}

void VelvetChannelAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(28);
    area.removeFromTop(72);

    auto controls = area.reduced(4, 0);
    const auto controlWidth = controls.getWidth() / 7;

    juce::Slider* sliders[] = {
        &inputSlider, &driveSlider, &highPassSlider, &warmthSlider, &presenceSlider, &airSlider, &outputSlider
    };

    juce::Label* labels[] = {
        &inputLabel, &driveLabel, &highPassLabel, &warmthLabel, &presenceLabel, &airLabel, &outputLabel
    };

    for (int i = 0; i < 7; ++i)
    {
        auto slot = controls.removeFromLeft(controlWidth).reduced(8, 0);
        labels[i]->setBounds(slot.removeFromTop(24));
        sliders[i]->setBounds(slot);
    }
}

void VelvetChannelAudioProcessorEditor::configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& text)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 76, 22);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accentColour());
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromRGB(81, 74, 65));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromRGB(239, 222, 196));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(239, 232, 218));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, juce::Colour::fromRGB(225, 211, 189));
    addAndMakeVisible(label);
}
}
