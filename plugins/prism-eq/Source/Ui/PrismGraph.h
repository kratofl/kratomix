#pragma once

#include <JuceHeader.h>

#include <optional>

#include "Dsp/PrismProcessor.h"

namespace kratomix::prism
{
class PrismGraph final : public juce::Component,
                         private juce::Timer
{
public:
    PrismGraph();

    void attachState(juce::AudioProcessorValueTreeState& stateToUse);
    void attachAnalyzerReader(std::function<void(PrismAnalyzerFrame&)> reader);

    void paint(juce::Graphics& g) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    bool keyPressed(const juce::KeyPress& key) override;

    bool createBandAt(juce::Point<float> point);
    bool selectBandAt(juce::Point<float> point);
    bool selectOrCreateBandAt(juce::Point<float> point);
    void dragSelectedBandTo(juce::Point<float> point);
    bool deleteSelectedBand();
    juce::Point<float> pointForFrequencyAndGain(float frequency, float gainDb) const;
    int getSelectedBand() const noexcept { return selectedBand; }
    void clearSelection();

    std::function<void(int)> onSelectedBandChanged;

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int analyzerBinCount = 240;

    void timerCallback() override;
    juce::Rectangle<float> graphBounds() const;
    static float frequencyToX(float frequency, juce::Rectangle<float> bounds);
    static float xToFrequency(float x, juce::Rectangle<float> bounds);
    static float gainToY(float gainDb, juce::Rectangle<float> bounds);
    static float yToGain(float y, juce::Rectangle<float> bounds);
    float responseGainAt(float frequency, bool includeDynamicGain) const;
    bool hasDynamicBands() const;
    void setSelectedBand(int oneBasedIndex);
    void updateSpectrum();
    void updateSpectrumLane(const std::array<float, PrismAnalyzerFrame::sampleCount>& samples,
                            std::array<float, analyzerBinCount>& destination);
    void drawAnalyzerLane(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          const std::array<float, analyzerBinCount>& values,
                          juce::Colour colour,
                          float rangeDb) const;
    void drawMaskingOverlay(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void drawHoverReadout(juce::Graphics& g, juce::Rectangle<float> bounds, int analyzerMode) const;
    static float frequencyForAnalyzerBin(int binIndex);
    static int analyzerBinForFrequency(float frequency);

    void setParameterValue(const juce::String& id, float plainValue);
    float parameterValue(const juce::String& id, float fallback) const;
    bool bandEnabled(int oneBasedIndex) const;
    juce::Point<float> bandPoint(int oneBasedIndex) const;

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData {};
    std::array<float, analyzerBinCount> preSpectrum {};
    std::array<float, analyzerBinCount> postSpectrum {};
    std::array<float, analyzerBinCount> sidechainSpectrum {};
    PrismAnalyzerFrame analyzerFrame;
    std::function<void(PrismAnalyzerFrame&)> analyzerReader;

    juce::AudioProcessorValueTreeState* state = nullptr;
    int selectedBand = 0;
    std::optional<juce::Point<float>> hoverPoint;
};
}
