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
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyPressed(const juce::KeyPress& key) override;

    int getSelectedBand() const noexcept { return selectedBand; }
    bool createBandAt(juce::Point<float> point);
    bool selectBandAt(juce::Point<float> point);
    bool deleteSelectedBand();
    void setSelectedBandFrequency(float frequency);
    void setSelectedBandWidth(float widthOctaves);
    void setSelectedBandThreshold(float thresholdDb);
    juce::Rectangle<float> boundsForBand(int zeroBasedIndex) const;
    float thresholdYForBand(int zeroBasedIndex) const;

    std::function<void(int)> onSelectedBandChanged;

private:
    enum class DragMode
    {
        none,
        body,
        threshold,
        leftEdge,
        rightEdge
    };

    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int analyzerBinCount = 220;

    void timerCallback() override;
    juce::Rectangle<float> graphBounds() const;
    int bandAt(juce::Point<float> point) const;
    DragMode dragModeAt(juce::Point<float> point, int zeroBasedBandIndex) const;
    void updateMouseCursor(juce::Point<float> point);
    void setSelectedBand(int zeroBasedIndex);
    void beginDragGesture();
    void endDragGesture();
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
    float parameterValue(const juce::String& id, float fallback) const;
    void setParameterValue(const juce::String& id, float plainValue);
    bool bandEnabled(int zeroBasedIndex) const;
    float bandFrequency(int zeroBasedIndex) const;
    float bandWidth(int zeroBasedIndex) const;
    float bandThreshold(int zeroBasedIndex) const;
    static float thresholdToY(float thresholdDb, juce::Rectangle<float> bounds);
    static float yToThreshold(float y, juce::Rectangle<float> bounds);
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
    std::array<juce::RangedAudioParameter*, 2> draggedParameters {};
    std::optional<juce::Point<float>> hoverPoint;
    juce::Point<float> dragStartPosition;
    float dragStartFrequency = 1000.0f;
    float dragStartThreshold = -24.0f;
    int selectedBand = -1;
    DragMode dragMode = DragMode::none;
};
}
