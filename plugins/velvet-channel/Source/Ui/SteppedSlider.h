#pragma once

#include <JuceHeader.h>

#include <functional>

namespace kratomix::ui
{
class SteppedSlider final : public juce::Slider
{
public:
    using Snapper = std::function<double(double)>;

    explicit SteppedSlider(Snapper valueSnapper = {})
        : juce::Slider(juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow),
          snapper(std::move(valueSnapper))
    {
    }

    void setSnapper(Snapper valueSnapper)
    {
        snapper = std::move(valueSnapper);
    }

    double snapValue(double attemptedValue, DragMode dragMode) override
    {
        if (snapper == nullptr)
            return juce::Slider::snapValue(attemptedValue, dragMode);

        return snapper(attemptedValue);
    }

private:
    Snapper snapper;
};
}
