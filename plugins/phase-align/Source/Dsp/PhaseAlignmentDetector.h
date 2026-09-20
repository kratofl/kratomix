#pragma once

namespace kratomix::phase_align
{
struct PhaseAlignmentResult
{
    bool valid = false;
    float correctionSamples = 0.0f;
    bool invertPolarity = false;
    float confidence = 0.0f;
    float correlation = 0.0f;
};

class PhaseAlignmentDetector
{
public:
    static PhaseAlignmentResult analyze(const float* moving,
                                        const float* reference,
                                        int numSamples,
                                        double sampleRate,
                                        int maximumCorrectionSamples);
};
}
