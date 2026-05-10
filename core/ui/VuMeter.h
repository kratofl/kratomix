#pragma once

#include <JuceHeader.h>

#include <functional>

namespace kratomix::ui
{
class VuMeter final : public juce::Component, private juce::Timer
{
public:
    explicit VuMeter(std::function<float()> levelReader);

    void paint(juce::Graphics&) override;

private:
    void timerCallback() override;

    std::function<float()> readLevel;
    float displayedLevel = 0.0f;
};
}
