#include "VuMeter.h"

namespace kratomix::ui
{
namespace
{
juce::Point<float> pointOnArc(juce::Point<float> centre, float radius, float angleRadians)
{
    return { centre.x + std::cos(angleRadians) * radius,
             centre.y + std::sin(angleRadians) * radius };
}
}

VuMeter::VuMeter(std::function<float()> levelReader)
    : readLevel(std::move(levelReader))
{
    startTimerHz(30);
}

void VuMeter::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(3.0f);
    g.setColour(juce::Colour::fromRGB(18, 17, 16));
    g.fillRoundedRectangle(bounds, 7.0f);

    g.setColour(juce::Colour::fromRGB(65, 55, 42));
    g.drawRoundedRectangle(bounds, 7.0f, 1.0f);

    auto face = bounds.reduced(10.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour::fromRGB(248, 236, 205),
                                           face.getCentreX(),
                                           face.getY(),
                                           juce::Colour::fromRGB(225, 206, 174),
                                           face.getCentreX(),
                                           face.getBottom(),
                                           false));
    g.fillRoundedRectangle(face, 4.0f);

    const auto centre = juce::Point<float>(face.getCentreX(), face.getBottom() - 22.0f);
    const auto radius = juce::jmin(face.getWidth() * 0.42f, face.getHeight() * 0.95f);
    const auto startAngle = juce::degreesToRadians(210.0f);
    const auto endAngle = juce::degreesToRadians(330.0f);

    g.setColour(juce::Colour::fromRGB(70, 46, 24));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("VU", face.toNearestInt().removeFromTop(20), juce::Justification::centred);

    for (int index = 0; index <= 8; ++index)
    {
        const auto proportion = static_cast<float>(index) / 8.0f;
        const auto angle = juce::jmap(proportion, 0.0f, 1.0f, startAngle, endAngle);
        const auto inner = pointOnArc(centre, radius - 18.0f, angle);
        const auto outer = pointOnArc(centre, radius, angle);

        g.setColour(index >= 6 ? juce::Colour::fromRGB(140, 42, 26) : juce::Colour::fromRGB(42, 32, 24));
        g.drawLine({ inner, outer }, 1.4f);
    }

    const auto decibels = juce::Decibels::gainToDecibels(displayedLevel, -48.0f);
    const auto normalized = juce::jlimit(0.0f, 1.0f, juce::jmap(decibels, -20.0f, 3.0f, 0.0f, 1.0f));
    const auto needleAngle = juce::jmap(normalized, 0.0f, 1.0f, startAngle, endAngle);
    const auto needleEnd = pointOnArc(centre, radius - 16.0f, needleAngle);

    g.setColour(juce::Colour::fromRGB(131, 22, 18));
    g.drawLine({ centre, needleEnd }, 2.2f);
    g.fillEllipse(centre.x - 5.0f, centre.y - 5.0f, 10.0f, 10.0f);
}

void VuMeter::timerCallback()
{
    const auto target = readLevel != nullptr ? readLevel() : 0.0f;
    displayedLevel = displayedLevel * 0.82f + target * 0.18f;
    repaint();
}
}
