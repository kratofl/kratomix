#pragma once

#include "Parameters.h"

namespace kratomix::prism
{
inline int prismLatencyFor(PhaseMode phaseMode, QualityMode qualityMode) noexcept
{
    auto latency = 0;

    if (qualityMode == QualityMode::oversample2x)
        latency += 32;
    else if (qualityMode == QualityMode::oversample4x)
        latency += 64;

    if (phaseMode == PhaseMode::linearPhase)
        latency += 2048;

    return latency;
}
}
