#include "PluginEditor.h"

namespace kratomix
{
namespace
{
    constexpr int editorWidth = 940;
    constexpr int editorHeight = 320;
    constexpr float knobStartAngle = juce::MathConstants<float>::pi * 1.2f;
    constexpr float knobEndAngle = juce::MathConstants<float>::pi * 2.8f;

    juce::Colour rackShellColour()
    {
        return juce::Colour::fromRGB(16, 16, 17);
    }

    juce::Colour metalDark()
    {
        return juce::Colour::fromRGB(156, 73, 26);
    }

    juce::Colour metalBright()
    {
        return juce::Colour::fromRGB(242, 146, 58);
    }

    juce::Colour engravingColour()
    {
        return juce::Colour::fromRGB(40, 27, 20);
    }

    juce::Rectangle<int> rackBounds(juce::Rectangle<int> bounds)
    {
        return bounds.reduced(18);
    }

    juce::Rectangle<int> topBandBounds(juce::Rectangle<int> rack)
    {
        return rack.reduced(22, 18).removeFromTop(118);
    }

    juce::Rectangle<int> brandBounds(juce::Rectangle<int> rack)
    {
        auto top = topBandBounds(rack);
        return top.removeFromLeft(250);
    }

    juce::Rectangle<int> meterBounds(juce::Rectangle<int> rack)
    {
        auto top = topBandBounds(rack);
        top.removeFromLeft(262);
        auto meter = top.removeFromLeft(290);
        meter.reduce(12, 6);
        return meter;
    }

    juce::Rectangle<int> bypassBounds(juce::Rectangle<int> rack)
    {
        auto top = topBandBounds(rack);
        return top.removeFromRight(130).withTrimmedTop(10).withTrimmedBottom(16).reduced(6, 0);
    }

    juce::Rectangle<int> controlsBounds(juce::Rectangle<int> rack)
    {
        auto body = rack.reduced(22, 18);
        body.removeFromTop(124);
        return body;
    }
}

VelvetChannelAudioProcessorEditor::VelvetChannelAudioProcessorEditor(VelvetChannelAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      inputSlider(),
      driveSlider([](double value) { return controls::snapDrive(static_cast<float>(value)); }),
      highPassSlider([](double value) { return controls::snapHighPass(static_cast<float>(value)); }),
      warmthSlider([](double value) { return controls::snapWarmth(static_cast<float>(value)); }),
      presenceSlider([](double value) { return controls::snapPresence(static_cast<float>(value)); }),
      airSlider([](double value) { return controls::snapAir(static_cast<float>(value)); }),
      outputSlider(),
      vuMeter([this] { return audioProcessor.getVuLevel(); }),
      bypassAttachment(audioProcessor.parameters, ParamID::bypass, bypassButton),
      inputAttachment(audioProcessor.parameters, ParamID::inputGain, inputSlider),
      driveAttachment(audioProcessor.parameters, ParamID::drive, driveSlider),
      highPassAttachment(audioProcessor.parameters, ParamID::highPass, highPassSlider),
      warmthAttachment(audioProcessor.parameters, ParamID::warmth, warmthSlider),
      presenceAttachment(audioProcessor.parameters, ParamID::presence, presenceSlider),
      airAttachment(audioProcessor.parameters, ParamID::air, airSlider),
      outputAttachment(audioProcessor.parameters, ParamID::outputGain, outputSlider)
{
    setSize(editorWidth, editorHeight);

    configureSlider(inputSlider,
                    inputLabel,
                    "INPUT",
                    [](double value) { return controls::formatPlainDecibels(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::parseNumericText(text, 0.0); },
                    0.0);
    configureSlider(driveSlider,
                    driveLabel,
                    "DRIVE",
                    [](double value) { return controls::formatDrive(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::snapDrive(static_cast<float>(controls::parseNumericText(text, 0.0))); },
                    0.0);
    configureSlider(highPassSlider,
                    highPassLabel,
                    "HPF",
                    [](double value) { return controls::formatFrequency(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::snapHighPass(static_cast<float>(controls::parseNumericText(text, 20.0))); },
                    20.0);
    configureSlider(warmthSlider,
                    warmthLabel,
                    "WARMTH",
                    [](double value) { return controls::formatWarmth(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::snapWarmth(static_cast<float>(controls::parseNumericText(text, 0.0))); },
                    0.0);
    configureSlider(presenceSlider,
                    presenceLabel,
                    "PRESENCE",
                    [](double value) { return controls::formatPresence(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::snapPresence(static_cast<float>(controls::parseNumericText(text, 0.0))); },
                    0.0);
    configureSlider(airSlider,
                    airLabel,
                    "AIR",
                    [](double value) { return controls::formatAir(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::snapAir(static_cast<float>(controls::parseNumericText(text, 0.0))); },
                    0.0);
    configureSlider(outputSlider,
                    outputLabel,
                    "OUTPUT",
                    [](double value) { return controls::formatPlainDecibels(static_cast<float>(value)); },
                    [](const juce::String& text) { return controls::parseNumericText(text, 0.0); },
                    0.0);

    bypassButton.setLookAndFeel(&rackLookAndFeel);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(vuMeter);
}

void VelvetChannelAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(rackShellColour());

    const auto rack = rackBounds(getLocalBounds()).toFloat();
    juce::ColourGradient panelGradient(metalBright(), rack.getCentreX(), rack.getY(), metalDark(), rack.getCentreX(), rack.getBottom(), false);
    panelGradient.addColour(0.35, juce::Colour::fromRGB(246, 165, 76));
    panelGradient.addColour(0.7, juce::Colour::fromRGB(201, 96, 36));
    g.setGradientFill(panelGradient);
    g.fillRoundedRectangle(rack, 8.0f);

    for (int x = static_cast<int>(rack.getX()) + 10; x < static_cast<int>(rack.getRight()) - 10; x += 6)
    {
        g.setColour(juce::Colours::white.withAlpha((x % 12 == 0) ? 0.045f : 0.02f));
        g.drawVerticalLine(x, rack.getY() + 6.0f, rack.getBottom() - 6.0f);
    }

    g.setColour(juce::Colour::fromRGB(40, 22, 14));
    g.drawRoundedRectangle(rack.reduced(1.5f), 8.0f, 1.5f);

    const auto screwColour = juce::Colour::fromRGB(31, 28, 27);
    for (const auto corner : { juce::Point<float>(rack.getX() + 18.0f, rack.getY() + 18.0f),
                               juce::Point<float>(rack.getRight() - 18.0f, rack.getY() + 18.0f),
                               juce::Point<float>(rack.getX() + 18.0f, rack.getBottom() - 18.0f),
                               juce::Point<float>(rack.getRight() - 18.0f, rack.getBottom() - 18.0f) })
    {
        g.setColour(screwColour);
        g.fillEllipse(corner.x - 7.0f, corner.y - 7.0f, 14.0f, 14.0f);
        g.setColour(juce::Colour::fromRGB(90, 90, 90));
        g.drawLine(corner.x - 4.0f, corner.y, corner.x + 4.0f, corner.y, 1.2f);
        g.drawLine(corner.x, corner.y - 4.0f, corner.x, corner.y + 4.0f, 1.2f);
    }

    const auto rackInt = rackBounds(getLocalBounds());
    const auto brand = brandBounds(rackInt).toFloat();
    const auto meter = meterBounds(rackInt).toFloat();
    const auto controls = controlsBounds(rackInt).toFloat();

    g.setColour(juce::Colours::black.withAlpha(0.16f));
    g.fillRoundedRectangle(brand.reduced(6.0f, 10.0f), 10.0f);

    juce::Font logoFont(juce::FontOptions(34.0f).withName("Snell Roundhand").withStyle("Bold Italic"));
    g.setFont(logoFont);
    g.setColour(juce::Colours::black.withAlpha(0.92f));
    g.drawText("Kratomix", brand.toNearestInt().reduced(18, 10).removeFromTop(52), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(18.0f, juce::Font::bold));
    g.setColour(engravingColour());
    g.drawText("VELVET CHANNEL", brand.toNearestInt().reduced(18, 10).withTrimmedTop(52).removeFromTop(28), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(12.5f, juce::Font::plain));
    g.drawText("SOURCE PREAMP COLOR", brand.toNearestInt().reduced(18, 10).withTrimmedTop(84).removeFromTop(18), juce::Justification::centredLeft);

    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.setColour(engravingColour().withAlpha(0.78f));
    g.drawText(juce::String("v") + KRATOMIX_PLUGIN_VERSION_STRING,
               rackInt.reduced(32, 24).removeFromRight(82).removeFromBottom(20),
               juce::Justification::centredRight);

    g.setColour(juce::Colours::black.withAlpha(0.12f));
    g.drawRoundedRectangle(meter.expanded(8.0f), 10.0f, 1.0f);
    g.drawLine(controls.getX(), controls.getY() - 10.0f, controls.getRight(), controls.getY() - 10.0f, 1.0f);
}

void VelvetChannelAudioProcessorEditor::resized()
{
    const auto rack = rackBounds(getLocalBounds());
    vuMeter.setBounds(meterBounds(rack));
    auto bypassArea = bypassBounds(rack);
    bypassButton.setBounds(juce::Rectangle<int>(118, 110).withCentre(bypassArea.getCentre()));

    auto controls = controlsBounds(rack).reduced(2, 2);
    const auto controlWidth = controls.getWidth() / 7;

    juce::Slider* sliders[] = {
        &inputSlider, &driveSlider, &highPassSlider, &warmthSlider, &presenceSlider, &airSlider, &outputSlider
    };

    juce::Label* labels[] = {
        &inputLabel, &driveLabel, &highPassLabel, &warmthLabel, &presenceLabel, &airLabel, &outputLabel
    };

    for (int i = 0; i < 7; ++i)
    {
        auto slot = controls.removeFromLeft(controlWidth).reduced(8, 2);
        labels[i]->setBounds(slot.removeFromTop(20));
        sliders[i]->setBounds(slot);
    }
}

void VelvetChannelAudioProcessorEditor::configureSlider(juce::Slider& slider,
                                                        juce::Label& label,
                                                        const juce::String& text,
                                                        std::function<juce::String(double)> formatter,
                                                        std::function<double(const juce::String&)> parser,
                                                        double doubleClickValue)
{
    slider.setRotaryParameters(knobStartAngle, knobEndAngle, true);
    slider.setLookAndFeel(&rackLookAndFeel);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    slider.setTextBoxIsEditable(true);
    slider.setDoubleClickReturnValue(true, doubleClickValue);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour::fromRGB(250, 231, 202));
    slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour::fromRGBA(28, 23, 21, 160));
    slider.setComponentID(text.toLowerCase());
    slider.textFromValueFunction = std::move(formatter);
    slider.valueFromTextFunction = std::move(parser);
    addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, engravingColour());
    addAndMakeVisible(label);
}
}
