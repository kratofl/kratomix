#pragma once

#include <array>

#include "Parameters.h"

namespace kratomix
{
struct MultibandAnalyzerFrame
{
    static constexpr int sampleCount = 2048;

    std::array<float, sampleCount> input {};
    std::array<float, sampleCount> output {};
    std::array<float, sampleCount> sidechain {};
    std::array<float, multiband::maxBands> dynamicGainDb {};
    std::array<float, multiband::maxBands> detectorLevelDb {};
    std::array<float, multiband::maxBands> bandLevelDb {};
    double sampleRate = 44100.0;
    float outputLevel = 0.0f;
    bool sidechainActive = false;
};

struct MultibandBandSettings
{
    bool enabled = true;
    bool solo = false;
    bool audition = false;
    float thresholdDb = -24.0f;
    float rangeDb = -6.0f;
    float ratio = 2.0f;
    float attackMs = 20.0f;
    float releaseMs = 120.0f;
    float kneeDb = 6.0f;
    float makeupDb = 0.0f;
    multiband::BandMode mode = multiband::BandMode::compress;
    multiband::DetectorSource detectorSource = multiband::DetectorSource::internal;
    float stereoLink = 1.0f;
};

struct MultibandSettings
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;
    bool bypassed = false;
    multiband::AnalyzerMode analyzerMode = multiband::AnalyzerMode::inputOutput;
    multiband::LookaheadMode lookaheadMode = multiband::LookaheadMode::off;
    std::array<float, multiband::crossoverCount> crossoverFrequencies = multiband::defaultCrossoverFrequencies;
    std::array<MultibandBandSettings, multiband::maxBands> bands {};
};
}
