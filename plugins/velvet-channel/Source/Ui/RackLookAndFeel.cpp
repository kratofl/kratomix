#include "RackLookAndFeel.h"

namespace kratomix::ui
{
namespace
{
juce::Colour knobFill()
{
    return juce::Colour::fromRGB(30, 28, 28);
}

juce::Colour knobEdge()
{
    return juce::Colour::fromRGB(12, 12, 12);
}

juce::Colour accent()
{
    return juce::Colour::fromRGB(241, 194, 124);
}
}

void RackLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                       int x,
                                       int y,
                                       int width,
                                       int height,
                                       float sliderPosProportional,
                                       float rotaryStartAngle,
                                       float rotaryEndAngle,
                                       juce::Slider&)
{
    const auto area = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y), static_cast<float>(width), static_cast<float>(height))
                          .reduced(8.0f, 10.0f);
    const auto knobZone = area.withTrimmedBottom(area.getHeight() * 0.16f);
    const auto diameter = juce::jmin(knobZone.getWidth(), knobZone.getHeight());
    const auto bounds = juce::Rectangle<float>(diameter, diameter)
                            .withCentre({ knobZone.getCentreX(), knobZone.getCentreY() - 4.0f });
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const auto angle = juce::jmap(sliderPosProportional, 0.0f, 1.0f, rotaryStartAngle, rotaryEndAngle);

    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.fillEllipse(bounds.translated(0.0f, 4.0f));

    g.setGradientFill(juce::ColourGradient(knobFill().brighter(0.3f),
                                           centre.x,
                                           bounds.getY(),
                                           knobFill(),
                                           centre.x,
                                           bounds.getBottom(),
                                           false));
    g.fillEllipse(bounds);

    g.setColour(knobEdge());
    g.drawEllipse(bounds, 2.0f);

    const auto ringBounds = bounds.reduced(radius * 0.16f);
    g.setColour(juce::Colour::fromRGB(79, 60, 49));
    g.drawEllipse(ringBounds, 1.0f);

    juce::Path arc;
    arc.addCentredArc(centre.x,
                      centre.y,
                      ringBounds.getWidth() * 0.5f,
                      ringBounds.getHeight() * 0.5f,
                      0.0f,
                      rotaryStartAngle,
                      angle,
                      true);
    g.setColour(accent());
    g.strokePath(arc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const auto pointerLength = radius * 0.55f;
    const auto pointerThickness = 3.0f;
    juce::Path pointer;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength, 1.3f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    g.setColour(juce::Colour::fromRGB(250, 236, 214));
    g.fillPath(pointer);

    g.setColour(juce::Colour::fromRGB(107, 90, 75));
    g.fillEllipse(centre.x - 6.0f, centre.y - 6.0f, 12.0f, 12.0f);
}

void RackLookAndFeel::drawToggleButton(juce::Graphics& g,
                                       juce::ToggleButton& button,
                                       bool shouldDrawButtonAsHighlighted,
                                       bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    auto labelBounds = bounds.removeFromBottom(22.0f);
    auto rockerArea = bounds;
    const auto rockerWidth = juce::jmin(rockerArea.getWidth(), 98.0f);
    const auto rockerHeight = juce::jmin(rockerArea.getHeight(), 82.0f);
    const auto rocker = juce::Rectangle<float>(rockerWidth, rockerHeight).withCentre(rockerArea.getCentre());
    const auto isOn = button.getToggleState();

    g.setColour(juce::Colour::fromRGB(20, 20, 20));
    g.fillRoundedRectangle(rocker, 6.0f);

    g.setColour(juce::Colour::fromRGB(65, 65, 65));
    g.drawRoundedRectangle(rocker, 6.0f, 1.2f);

    auto switchCap = rocker.reduced(10.0f, 8.0f);
    switchCap = isOn ? switchCap.translated(0.0f, -5.0f) : switchCap.translated(0.0f, 5.0f);

    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(58, 58, 58),
                                           switchCap.getCentreX(),
                                           switchCap.getY(),
                                           juce::Colour::fromRGB(23, 23, 23),
                                           switchCap.getCentreX(),
                                           switchCap.getBottom(),
                                           false));
    g.fillRoundedRectangle(switchCap, 4.0f);

    const auto lampBounds = juce::Rectangle<float>(rocker.getCentreX() - 8.0f, rocker.getBottom() - 14.0f, 16.0f, 9.0f);
    g.setColour(isOn ? accent() : juce::Colour::fromRGB(55, 38, 24));
    g.fillRoundedRectangle(lampBounds, 3.0f);

    if (shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown)
    {
        g.setColour(juce::Colours::white.withAlpha(0.08f));
        g.drawRoundedRectangle(rocker.expanded(1.0f), 6.0f, 1.0f);
    }

    g.setColour(juce::Colour::fromRGB(26, 21, 17));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawFittedText(button.getButtonText(), labelBounds.toNearestInt(), juce::Justification::centred, 1);
}
}
