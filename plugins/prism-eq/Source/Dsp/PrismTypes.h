#pragma once

#include <array>

#include "Parameters.h"

namespace kratomix
{
struct PrismAnalyzerFrame
{
    static constexpr int sampleCount = 2048;

    std::array<float, sampleCount> pre {};
    std::array<float, sampleCount> post {};
    std::array<float, sampleCount> sidechain {};
    std::array<float, prism::maxBands> dynamicGainDb {};
    std::array<float, prism::maxBands> dynamicTargetGainDb {};
    std::array<float, prism::maxBands> detectorLevelDb {};
    std::array<float, prism::maxBands> detectorOverThresholdDb {};
    std::array<bool, prism::maxBands> detectorUsingExternalSidechain {};
    double sampleRate = 44100.0;
    bool sidechainActive = false;
};

struct PrismBandSettings
{
    bool enabled = false;
    prism::BandType type = prism::BandType::bell;
    float frequency = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    bool dynamicEnabled = false;
    float dynamicRangeDb = 0.0f;
    float thresholdDb = -24.0f;
    float attackMs = 20.0f;
    float releaseMs = 120.0f;
    prism::SidechainSource sidechainSource = prism::SidechainSource::main;
    bool solo = false;
};

struct PrismSettings
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;
    bool bypassed = false;
    prism::PhaseMode phaseMode = prism::PhaseMode::zeroLatency;
    prism::QualityMode qualityMode = prism::QualityMode::native;
    std::array<PrismBandSettings, prism::maxBands> bands {};
};
}
