#pragma once

#include <JuceHeader.h>

#include <array>
#include <functional>
#include <optional>

#include "Dsp/MultibandTypes.h"
#include "Parameters.h"

namespace kratomix::multiband
{
class MultibandGraph final : public juce::Component,
                             private juce::Timer
{
public:
    MultibandGraph();
    ~MultibandGraph() override;

    void attachState(juce::AudioProcessorValueTreeState& stateToUse);
    void attachAnalyzerReader(std::function<void(MultibandAnalyzerFrame&)> reader);

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    int getSelectedBand() const noexcept { return selectedBand; }

    std::function<void(int)> onSelectedBandChanged;

private:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int analyzerBinCount = 220;

    void timerCallback() override;
    juce::Rectangle<float> graphBounds() const;
    std::array<float, crossoverCount> readCrossoverFrequencies() const;
    int bandIndexForX(float x) const;
    int crossoverHandleAt(float x) const;
    void setSelectedBand(int zeroBasedIndex);
    void updateSpectrum();
    void updateSpectrumLane(const std::array<float, MultibandAnalyzerFrame::sampleCount>& samples,
                            std::array<float, analyzerBinCount>& destination);
    void drawGrid(juce::Graphics& g, juce::Rectangle<float> graph) const;
    void drawBands(juce::Graphics& g, juce::Rectangle<float> graph) const;
    void drawSpectrum(juce::Graphics& g,
                      juce::Rectangle<float> graph,
                      const std::array<float, analyzerBinCount>& values,
                      juce::Colour colour) const;
    void drawDynamics(juce::Graphics& g, juce::Rectangle<float> graph) const;
    void setCrossoverFrequency(int crossoverIndex, float frequency);
    float parameterValue(const juce::String& id, float fallback) const;
    static float frequencyToX(float frequency, juce::Rectangle<float> bounds);
    static float xToFrequency(float x, juce::Rectangle<float> bounds);
    static float analyzerFrequencyForBin(int binIndex);

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize * 2> fftData {};
    std::array<float, analyzerBinCount> inputSpectrum {};
    std::array<float, analyzerBinCount> outputSpectrum {};
    MultibandAnalyzerFrame analyzerFrame;
    std::function<void(MultibandAnalyzerFrame&)> analyzerReader;
    juce::AudioProcessorValueTreeState* state = nullptr;
    juce::RangedAudioParameter* draggedCrossoverParameter = nullptr;
    std::optional<juce::Point<float>> hoverPoint;
    int selectedBand = 0;
    int draggedCrossover = -1;
};
}
