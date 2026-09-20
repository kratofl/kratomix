#pragma once

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <functional>

#include "FractionalDelayLine.h"
#include "PhaseAlignmentDetector.h"

namespace kratomix::phase_align
{
enum class AuditionMode { original, aligned, difference };
enum class AnalysisStatus { idle, capturing, analyzing, ready, lowConfidence, noSidechain };

struct Settings
{
    float offsetMs = 0.0f;
    bool invertPolarity = false;
    AuditionMode auditionMode = AuditionMode::aligned;
    bool bypassed = false;
};

struct AnalysisFrame
{
    static constexpr int sampleCount = 256;
    std::array<float, sampleCount> moving {};
    std::array<float, sampleCount> reference {};
    bool valid = false;
};

class PhaseAlignProcessor final : private juce::Thread
{
public:
    explicit PhaseAlignProcessor(std::function<void()> analysisCompleted);
    ~PhaseAlignProcessor() override;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset() noexcept;
    void updateSettings(const Settings& newSettings) noexcept;
    void process(juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>* sidechainBuffer) noexcept;
    bool requestAnalysis() noexcept;
    bool consumeAnalysisResult(PhaseAlignmentResult& destination) noexcept;
    int getLatencySamples() const noexcept;
    float getCorrelation() const noexcept;
    float getConfidence() const noexcept;
    AnalysisStatus getAnalysisStatus() const noexcept;
    void copyAnalysisFrame(AnalysisFrame& destination) const noexcept;

private:
    static constexpr int captureSampleCount = 32768;
    using CaptureStorage = std::array<float, captureSampleCount>;
    using PlotStorage = std::array<std::atomic<float>, AnalysisFrame::sampleCount>;

    void run() override;
    void capture(const juce::AudioBuffer<float>& mainBuffer,
                 const juce::AudioBuffer<float>& sidechainBuffer) noexcept;
    void publishPreview(const PhaseAlignmentResult& result) noexcept;
    void publishUnavailable(AnalysisStatus newStatus) noexcept;
    static float monoSample(const juce::AudioBuffer<float>& buffer, int sample) noexcept;

    std::function<void()> completionCallback;
    FractionalDelayLine movingDelay;
    FractionalDelayLine referenceDelay;
    juce::AudioBuffer<float> delayedReference;
    juce::WaitableEvent analysisEvent;
    CaptureStorage movingCapture {};
    CaptureStorage referenceCapture {};
    PlotStorage movingPlot;
    PlotStorage referencePlot;
    std::atomic<int> capturePosition { 0 };
    std::atomic<int> captureState { 0 };
    std::atomic<bool> resultReady { false };
    std::atomic<bool> resultValid { false };
    std::atomic<float> resultCorrection { 0.0f };
    std::atomic<bool> resultPolarity { false };
    std::atomic<float> resultConfidence { 0.0f };
    std::atomic<float> resultCorrelation { 0.0f };
    std::atomic<float> liveCorrelation { 0.0f };
    std::atomic<int> status { static_cast<int>(AnalysisStatus::idle) };
    std::atomic<bool> previewValid { false };
    Settings settings;
    double sampleRate = 48000.0;
    int preparedChannels = 0;
    int preparedBlockSize = 0;
    int baseLatencySamples = 0;
    int maximumCorrectionSamples = 0;
};
}
