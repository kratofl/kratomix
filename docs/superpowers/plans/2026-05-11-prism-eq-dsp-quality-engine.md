# Prism EQ DSP Quality Engine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Upgrade Kratomix Prism EQ's DSP quality path with robust high-frequency coefficient design, musical shelf behavior, optional oversampling for fast dynamic movement, and a real linear-phase processing mode with correct latency reporting.

**Architecture:** This plan assumes the live UI truth slice has introduced shared `PrismTypes`, `PrismFilterDesign`, and `PrismResponseModel` files. The minimum-phase realtime engine stays IIR-based, but coefficient generation becomes guarded and testable. Oversampling is added as an optional quality path for the minimum-phase engine, while linear phase is a separate FIR/convolution engine that reports latency to the host and crossfades safely when modes change.

**Tech Stack:** JUCE C++17, `juce::dsp::IIR`, `juce::dsp::Oversampling`, `juce::dsp::Convolution`, AudioProcessorValueTreeState, existing monorepo `make` workflow.

---

## Scope Check

This is the second executable slice after `docs/superpowers/plans/2026-05-11-prism-eq-live-ui-truth.md`.

Included:

- Stable high-frequency coefficient design tests and guards.
- Shelf resonance limiting so extreme shelf Q values do not create unintended resonant spikes.
- Optional oversampling for the minimum-phase dynamic EQ path.
- Real linear-phase FIR/convolution processing for static EQ response.
- Host latency reporting for oversampling and linear-phase paths.

Excluded from this plan:

- Spectral dynamics.
- M/S and L/R per-band routing.
- EQ matching.
- Multi-instance workflows.
- EQ sketching.
- New filter types beyond the current six.
- Preset browser, undo/redo, and A/B state management.

Do not commit during execution unless the user explicitly asks. If commits are authorized later, commit after each task with the suggested concise message.

## Prerequisite

Complete Task 1 from `docs/superpowers/plans/2026-05-11-prism-eq-live-ui-truth.md` first. This plan depends on these files:

- `plugins/prism-eq/Source/Dsp/PrismTypes.h`
- `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`
- `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`
- `plugins/prism-eq/Source/Dsp/PrismResponseModel.h`
- `plugins/prism-eq/Source/Dsp/PrismResponseModel.cpp`

## File Structure

- Modify `plugins/prism-eq/Source/Parameters.h`: add `qualityMode`, extend `phaseMode`, keep all existing parameter IDs stable.
- Modify `plugins/prism-eq/Source/Dsp/PrismTypes.h`: add quality mode enum use, quality setting, and engine latency fields.
- Modify `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`: expose coefficient safety helpers and shelf-Q mapping.
- Modify `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`: implement high-frequency guards and shelf resonance limiting.
- Create `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.h`: own IIR filters, detector filters, optional oversampling, and dynamic gain state.
- Create `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.cpp`: process minimum-phase EQ at native, 2x, or 4x rate.
- Create `plugins/prism-eq/Source/Dsp/PrismLatency.h`: calculate latency for quality and phase modes.
- Create `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.h`: own FIR/convolution processing state.
- Create `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.cpp`: build linear-phase FIR from response snapshots and process with convolution.
- Modify `plugins/prism-eq/Source/Dsp/PrismProcessor.h`: delegate EQ work to minimum-phase and linear-phase engines.
- Modify `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`: route processing by phase and quality mode, update host latency.
- Modify `plugins/prism-eq/Source/PluginProcessor.cpp`: call `setLatencySamples` when DSP latency changes.
- Modify `plugins/prism-eq/Source/PluginEditor.cpp`: expose quality and linear-phase mode only as real working controls.
- Modify `plugins/prism-eq/plugin.cmake`: register new DSP files.
- Modify `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`: add coefficient, shelf, oversampling, linear-phase, and latency regression tests.

### Task 1: Quality Parameters And Latency Surface

**Files:**
- Modify: `plugins/prism-eq/Source/Parameters.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismTypes.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismLatency.h`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing parameter and latency tests**

In `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`, add these parameter checks near the existing global parameter checks:

```cpp
    expectParameter(processor.parameters, "qualityMode", failures);
    expect(kratomix::prism::qualityModeChoices().contains("Native"),
           "Quality choices should include native processing",
           failures);
    expect(kratomix::prism::qualityModeChoices().contains("2x"),
           "Quality choices should include 2x oversampling",
           failures);
    expect(kratomix::prism::qualityModeChoices().contains("4x"),
           "Quality choices should include 4x oversampling",
           failures);
    expect(kratomix::prism::phaseModeChoices().contains("Linear Phase"),
           "Phase mode choices should include real linear phase processing",
           failures);
```

Add this include:

```cpp
#include "Source/Dsp/PrismLatency.h"
```

Add this latency block before the final sidechain selection test:

```cpp
    {
        expect(kratomix::prismLatencyFor(kratomix::prism::PhaseMode::zeroLatency,
                                         kratomix::prism::QualityMode::native) == 0,
               "Native zero-latency mode should report no added latency",
               failures);
        expect(kratomix::prismLatencyFor(kratomix::prism::PhaseMode::zeroLatency,
                                         kratomix::prism::QualityMode::oversample2x) > 0,
               "2x quality mode should report oversampling latency",
               failures);
        expect(kratomix::prismLatencyFor(kratomix::prism::PhaseMode::linearPhase,
                                         kratomix::prism::QualityMode::native) >= 2048,
               "Linear phase mode should report FIR latency",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: build fails because `QualityMode`, `qualityModeChoices`, and `PrismLatency.h` do not exist yet.

- [ ] **Step 3: Add quality mode and linear phase parameter choices**

In `plugins/prism-eq/Source/Parameters.h`, add this enum after `PhaseMode`:

```cpp
enum class QualityMode
{
    native = 0,
    oversample2x,
    oversample4x
};
```

Change `PhaseMode` to:

```cpp
enum class PhaseMode
{
    zeroLatency = 0,
    natural,
    linearPhase
};
```

Change `globalParameterIds()` from 9 items to 10 items:

```cpp
inline constexpr std::array<const char*, 10> globalParameterIds()
{
    return { "inputGain", "outputGain", "mix", "bypass", "analyzerMode", "analyzerSpeed", "analyzerRange", "gainScale", "phaseMode", "qualityMode" };
}
```

Change `phaseModeChoices()` to:

```cpp
inline juce::StringArray phaseModeChoices()
{
    return { "Zero Latency", "Natural", "Linear Phase" };
}
```

Add:

```cpp
inline juce::StringArray qualityModeChoices()
{
    return { "Native", "2x", "4x" };
}
```

Add a global parameter after `phaseMode`:

```cpp
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { "qualityMode", 1 },
        "Quality Mode",
        qualityModeChoices(),
        static_cast<int>(QualityMode::native)));
```

- [ ] **Step 4: Add latency helper**

Create `plugins/prism-eq/Source/Dsp/PrismLatency.h`:

```cpp
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
```

- [ ] **Step 5: Store quality mode in settings**

In `plugins/prism-eq/Source/Dsp/PrismTypes.h`, add to `PrismSettings`:

```cpp
    prism::QualityMode qualityMode = prism::QualityMode::native;
```

In `plugins/prism-eq/Source/PluginProcessor.cpp`, in `readSettings()`, add:

```cpp
    current.qualityMode = static_cast<prism::QualityMode>(juce::jlimit(
        0,
        static_cast<int>(prism::QualityMode::oversample4x),
        static_cast<int>(std::round(parameters.getRawParameterValue("qualityMode")->load()))));
```

Update the phase-mode clamp to use the new final enum:

```cpp
    current.phaseMode = static_cast<prism::PhaseMode>(juce::jlimit(
        0,
        static_cast<int>(prism::PhaseMode::linearPhase),
        static_cast<int>(std::round(parameters.getRawParameterValue("phaseMode")->load()))));
```

- [ ] **Step 6: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 2: Robust High-Frequency Filter Design

**Files:**
- Modify: `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write failing coefficient stability tests**

In `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`, add:

```cpp
void expectFiniteResponse(const kratomix::PrismIIRCoefficients& coefficients,
                          double sampleRate,
                          const juce::String& label,
                          int& failures)
{
    const std::array<float, 8> probes {
        20.0f, 100.0f, 1000.0f, 6000.0f, 12000.0f, 18000.0f,
        static_cast<float>(sampleRate * 0.43),
        static_cast<float>(sampleRate * 0.47)
    };

    for (const auto frequency : probes)
    {
        if (frequency >= sampleRate * 0.49)
            continue;

        const auto magnitude = coefficients.getMagnitudeForFrequency(frequency, sampleRate);
        expect(std::isfinite(magnitude) && magnitude > 0.0 && magnitude < 1000.0,
               label + " should have finite bounded response at " + juce::String(frequency, 1) + " Hz",
               failures);
    }
}
```

Add this block after the parameter checks:

```cpp
    {
        const std::array<double, 4> sampleRates { 44100.0, 48000.0, 96000.0, 192000.0 };

        for (const auto sampleRateToTest : sampleRates)
        {
            kratomix::PrismBandSettings band;
            band.enabled = true;
            band.type = kratomix::prism::BandType::bell;
            band.frequency = static_cast<float>(sampleRateToTest * 0.45);
            band.gainDb = 18.0f;
            band.q = 30.0f;

            const auto coefficients = kratomix::makePrismCoefficients(sampleRateToTest, band, 0.0f);
            expect(coefficients != nullptr,
                   "High-frequency bell coefficient generation should return coefficients",
                   failures);
            if (coefficients != nullptr)
                expectFiniteResponse(*coefficients, sampleRateToTest, "High-frequency bell", failures);
        }
    }
```

- [ ] **Step 2: Run the test to verify it fails or exposes current behavior**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: the new test may fail on finite/bounded response, or it may pass while proving current behavior. Continue with the guard implementation either way because the guard makes the design explicit and future-proof.

- [ ] **Step 3: Add explicit high-frequency guard helpers**

In `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`, add:

```cpp
namespace prism
{
float safeFilterFrequency(float frequency, double sampleRate) noexcept;
float safeFilterQ(float q) noexcept;
float safeShelfQ(float q) noexcept;
}
```

In `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`, add:

```cpp
float prism::safeFilterFrequency(float frequency, double sampleRate) noexcept
{
    const auto nyquist = static_cast<float>(sampleRate * 0.5);
    const auto upper = juce::jmin(20000.0f, nyquist * 0.475f);
    return juce::jlimit(20.0f, juce::jmax(20.0f, upper), frequency);
}

float prism::safeFilterQ(float q) noexcept
{
    return juce::jlimit(0.1f, 32.0f, q);
}

float prism::safeShelfQ(float q) noexcept
{
    return juce::jlimit(0.35f, 0.95f, q);
}
```

In `makePrismCoefficients`, replace:

```cpp
    const auto frequency = juce::jlimit(20.0f, 20000.0f, band.frequency);
    const auto q = juce::jlimit(0.1f, 40.0f, band.q);
```

with:

```cpp
    const auto frequency = prism::safeFilterFrequency(band.frequency, sampleRate);
    const auto q = prism::safeFilterQ(band.q);
    const auto shelfQ = prism::safeShelfQ(band.q);
```

Use `shelfQ` for shelves:

```cpp
        case prism::BandType::lowShelf:
            return PrismIIRCoefficients::makeLowShelf(sampleRate, frequency, shelfQ, gain);
        case prism::BandType::highShelf:
            return PrismIIRCoefficients::makeHighShelf(sampleRate, frequency, shelfQ, gain);
```

- [ ] **Step 4: Add focused helper tests**

Add this block after the high-frequency test:

```cpp
    {
        expect(kratomix::prism::safeFilterFrequency(30000.0f, 48000.0) < 12000.0f,
               "Safe filter frequency should stay comfortably below Nyquist",
               failures);
        expect(kratomix::prism::safeFilterQ(100.0f) <= 32.0f,
               "Safe filter Q should clamp extreme bell values",
               failures);
        expect(kratomix::prism::safeShelfQ(40.0f) <= 0.95f,
               "Safe shelf Q should prevent high-Q shelf resonance spikes",
               failures);
    }
```

- [ ] **Step 5: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 3: Shelf Resonance Regression Tests

**Files:**
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`
- Modify: `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`

- [ ] **Step 1: Write shelf overshoot tests**

Add this helper in `ProcessorBehaviorTests.cpp` near `expectFiniteResponse`:

```cpp
float maxResponseDb(const kratomix::PrismIIRCoefficients& coefficients,
                    double sampleRate,
                    float startFrequency,
                    float endFrequency)
{
    auto maxDb = -120.0f;

    for (int index = 0; index <= 240; ++index)
    {
        const auto proportion = static_cast<float>(index) / 240.0f;
        const auto frequency = std::pow(10.0f,
                                        juce::jmap(proportion,
                                                   std::log10(startFrequency),
                                                   std::log10(endFrequency)));
        const auto magnitude = coefficients.getMagnitudeForFrequency(frequency, sampleRate);
        maxDb = juce::jmax(maxDb, juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -120.0f));
    }

    return maxDb;
}
```

Add this test block:

```cpp
    {
        kratomix::PrismBandSettings lowShelf;
        lowShelf.enabled = true;
        lowShelf.type = kratomix::prism::BandType::lowShelf;
        lowShelf.frequency = 180.0f;
        lowShelf.gainDb = 6.0f;
        lowShelf.q = 40.0f;

        const auto lowShelfCoefficients = kratomix::makePrismCoefficients(96000.0, lowShelf, 0.0f);
        expect(lowShelfCoefficients != nullptr,
               "Low shelf coefficients should be created",
               failures);
        if (lowShelfCoefficients != nullptr)
        {
            const auto maximumDb = maxResponseDb(*lowShelfCoefficients, 96000.0, 20.0f, 2000.0f);
            expect(maximumDb <= 7.25f,
                   "High-Q low shelf should not create an unintended resonant spike above the requested gain",
                   failures);
        }

        kratomix::PrismBandSettings highShelf;
        highShelf.enabled = true;
        highShelf.type = kratomix::prism::BandType::highShelf;
        highShelf.frequency = 9000.0f;
        highShelf.gainDb = 6.0f;
        highShelf.q = 40.0f;

        const auto highShelfCoefficients = kratomix::makePrismCoefficients(96000.0, highShelf, 0.0f);
        expect(highShelfCoefficients != nullptr,
               "High shelf coefficients should be created",
               failures);
        if (highShelfCoefficients != nullptr)
        {
            const auto maximumDb = maxResponseDb(*highShelfCoefficients, 96000.0, 1000.0f, 20000.0f);
            expect(maximumDb <= 7.25f,
                   "High-Q high shelf should not create an unintended resonant spike above the requested gain",
                   failures);
        }
    }
```

- [ ] **Step 2: Run test to verify current shelf behavior**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: if Task 2 already clamps shelf Q, tests pass. If not, tests fail with a shelf resonant spike message.

- [ ] **Step 3: Tune shelf guard if needed**

If the shelf overshoot test fails, lower `safeShelfQ` upper bound in `PrismFilterDesign.cpp`:

```cpp
float prism::safeShelfQ(float q) noexcept
{
    return juce::jlimit(0.35f, 0.82f, q);
}
```

- [ ] **Step 4: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 4: Minimum-Phase Engine Boundary

**Files:**
- Create: `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.cpp`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Add behavior-preservation test**

Add this block before the existing dynamic EQ behavior test:

```cpp
    {
        kratomix::PrismProcessor engineProcessor;
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 48000.0;
        spec.maximumBlockSize = 2048;
        spec.numChannels = 2;
        engineProcessor.prepare(spec);

        kratomix::PrismSettings settings;
        auto& band = settings.bands[0];
        band.enabled = true;
        band.type = kratomix::prism::BandType::bell;
        band.frequency = 1000.0f;
        band.gainDb = 6.0f;
        band.q = 1.0f;
        engineProcessor.updateSettings(settings);

        auto engineBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(engineBuffer);
        engineProcessor.process(engineBuffer, nullptr);

        expect(rmsLevel(engineBuffer) > dryRms * 1.45f,
               "Minimum-phase engine extraction should preserve bell boost behavior",
               failures);
    }
```

- [ ] **Step 2: Run current tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: PASS before extraction. This creates a baseline before moving code.

- [ ] **Step 3: Create engine header**

Create `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.h`:

```cpp
#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismFilterDesign.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
class PrismMinimumPhaseEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& newSettings);
    void process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer);
    void copyDynamicTelemetryTo(PrismAnalyzerFrame& destination) const noexcept;

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using Coefficients = juce::dsp::IIR::Coefficients<float>;
    using StereoFilter = juce::dsp::ProcessorDuplicator<Filter, Coefficients>;

    void updateDynamicGain(const juce::AudioBuffer<float>& mainBuffer,
                           const juce::AudioBuffer<float>* sidechainBuffer,
                           int numSamples);
    void updateFilterCoefficients();
    static float monoSampleAt(const juce::AudioBuffer<float>& buffer, int sample) noexcept;
    static float bandLimitedRmsDb(const juce::AudioBuffer<float>& buffer, int numSamples, Filter& filter) noexcept;

    std::array<StereoFilter, prism::maxBands> filters;
    std::array<StereoFilter, prism::maxBands> phaseFilters;
    std::array<Filter, prism::maxBands> detectorFilters;
    std::array<float, prism::maxBands> dynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicTargetGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorLevelDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorOverThresholdDb {};
    std::array<std::atomic<bool>, prism::maxBands> analyzerDetectorUsingExternalSidechain {};

    PrismSettings settings;
    double sampleRate = 44100.0;
};
}
```

- [ ] **Step 4: Move minimum-phase implementation**

Create `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.cpp` by moving these responsibilities out of `PrismProcessor.cpp`:

```cpp
#include "PrismMinimumPhaseEngine.h"

namespace kratomix
{
void PrismMinimumPhaseEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    for (auto& filter : filters)
        filter.prepare(spec);
    for (auto& filter : phaseFilters)
        filter.prepare(spec);

    reset();
}

void PrismMinimumPhaseEngine::reset()
{
    for (auto& filter : filters)
        filter.reset();
    for (auto& filter : phaseFilters)
        filter.reset();
    for (auto& filter : detectorFilters)
        filter.reset();

    dynamicGainDb.fill(0.0f);
    for (auto& value : analyzerDynamicGainDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDynamicTargetGainDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorLevelDb)
        value.store(-120.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorOverThresholdDb)
        value.store(0.0f, std::memory_order_relaxed);
    for (auto& value : analyzerDetectorUsingExternalSidechain)
        value.store(false, std::memory_order_relaxed);
}

void PrismMinimumPhaseEngine::updateSettings(const PrismSettings& newSettings)
{
    settings = newSettings;
    updateFilterCoefficients();
}

void PrismMinimumPhaseEngine::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    updateDynamicGain(buffer, sidechainBuffer, buffer.getNumSamples());
    updateFilterCoefficients();

    juce::dsp::AudioBlock<float> block { buffer };
    juce::dsp::ProcessContextReplacing<float> context { block };

    for (size_t index = 0; index < filters.size(); ++index)
        if (settings.bands[index].enabled)
            filters[index].process(context);

    if (settings.phaseMode == prism::PhaseMode::natural)
        for (size_t index = 0; index < phaseFilters.size(); ++index)
            if (settings.bands[index].enabled)
                phaseFilters[index].process(context);
}
}
```

Move the existing `updateDynamicGain`, `updateFilterCoefficients`, `monoSampleAt`, and `bandLimitedRmsDb` implementations from `PrismProcessor.cpp` into this new file and update coefficient creation to call `makePrismCoefficients(...)`.

- [ ] **Step 5: Delegate from `PrismProcessor`**

In `plugins/prism-eq/Source/Dsp/PrismProcessor.h`, remove these members:

```cpp
    std::array<StereoFilter, prism::maxBands> filters;
    std::array<StereoFilter, prism::maxBands> phaseFilters;
    std::array<Filter, prism::maxBands> detectorFilters;
    std::array<float, prism::maxBands> dynamicGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicGainDb {};
```

Add:

```cpp
#include "Dsp/PrismMinimumPhaseEngine.h"
```

and a private member:

```cpp
    PrismMinimumPhaseEngine minimumPhaseEngine;
```

In `PrismProcessor::prepare`, call:

```cpp
    minimumPhaseEngine.prepare(spec);
```

In `reset()`, call:

```cpp
    minimumPhaseEngine.reset();
```

In `updateSettings()`, call:

```cpp
    minimumPhaseEngine.updateSettings(settings);
```

In `process()`, replace the direct filter loop with:

```cpp
    minimumPhaseEngine.process(buffer, sidechainBuffer);
```

In `copyAnalyzerFrame`, call:

```cpp
    minimumPhaseEngine.copyDynamicTelemetryTo(destination);
```

- [ ] **Step 6: Register the new engine**

In `plugins/prism-eq/plugin.cmake`, add:

```cmake
    Source/Dsp/PrismMinimumPhaseEngine.h
    Source/Dsp/PrismMinimumPhaseEngine.cpp
```

- [ ] **Step 7: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 5: Optional Oversampling Path

**Files:**
- Modify: `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismMinimumPhaseEngine.cpp`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write oversampling behavior tests**

Add this block before the linear phase tests:

```cpp
    {
        kratomix::PrismEqAudioProcessor oversampledProcessor;
        disableSidechainForProcessorTest(oversampledProcessor);
        oversampledProcessor.prepareToPlay(48000.0, 512);

        oversampledProcessor.parameters.getParameter("qualityMode")->setValueNotifyingHost(
            oversampledProcessor.parameters.getParameter("qualityMode")->convertTo0to1(
                static_cast<float>(kratomix::prism::QualityMode::oversample2x)));

        kratomix::prism::PrismGraph graph;
        graph.attachState(oversampledProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        auto buffer = makeSineBuffer(2, 512, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(buffer);
        juce::MidiBuffer midi;
        oversampledProcessor.processBlock(buffer, midi);

        expect(rmsLevel(buffer) > dryRms * 1.35f,
               "2x oversampled quality mode should preserve audible EQ behavior",
               failures);
        expect(oversampledProcessor.getLatencySamples() == kratomix::prism::prismLatencyFor(
                   kratomix::prism::PhaseMode::zeroLatency,
                   kratomix::prism::QualityMode::oversample2x),
               "Processor should report oversampling latency to the host",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: FAIL because quality mode is not routed through oversampling and latency is not updated.

- [ ] **Step 3: Add oversampling members**

In `PrismMinimumPhaseEngine.h`, add:

```cpp
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler2x;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler4x;
    juce::AudioBuffer<float> oversamplingWorkBuffer;
    juce::dsp::ProcessSpec nativeSpec {};
```

- [ ] **Step 4: Prepare oversamplers**

In `PrismMinimumPhaseEngine::prepare`, add after `sampleRate = spec.sampleRate;`:

```cpp
    nativeSpec = spec;
    oversamplingWorkBuffer.setSize(static_cast<int>(spec.numChannels), static_cast<int>(spec.maximumBlockSize));

    oversampler2x = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels,
        1,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);
    oversampler4x = std::make_unique<juce::dsp::Oversampling<float>>(
        spec.numChannels,
        2,
        juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
        true);

    oversampler2x->initProcessing(spec.maximumBlockSize);
    oversampler4x->initProcessing(spec.maximumBlockSize);
```

- [ ] **Step 5: Split native processing into a helper**

In `PrismMinimumPhaseEngine.h`, add:

```cpp
    void processNativeBlock(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer);
```

Rename the current `process(...)` body to `processNativeBlock(...)`.

Replace `process(...)` with:

```cpp
void PrismMinimumPhaseEngine::process(juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechainBuffer)
{
    if (settings.qualityMode == prism::QualityMode::native)
    {
        processNativeBlock(buffer, sidechainBuffer);
        return;
    }

    auto* oversampler = settings.qualityMode == prism::QualityMode::oversample4x ? oversampler4x.get() : oversampler2x.get();
    if (oversampler == nullptr)
    {
        processNativeBlock(buffer, sidechainBuffer);
        return;
    }

    juce::dsp::AudioBlock<float> block { buffer };
    auto upsampledBlock = oversampler->processSamplesUp(block);
    juce::AudioBuffer<float> upsampledBuffer(
        upsampledBlock.getChannelPointer(0),
        static_cast<int>(upsampledBlock.getNumChannels()),
        static_cast<int>(upsampledBlock.getNumSamples()));

    processNativeBlock(upsampledBuffer, nullptr);
    oversampler->processSamplesDown(block);
}
```

This first oversampling slice processes the main EQ path at the oversampled rate and keeps sidechain detection on the native-rate block. A later sidechain-specific slice can oversample detector filtering too if the audible tests require it.

- [ ] **Step 6: Report latency from processor**

In `PrismProcessor`, add a public getter:

```cpp
int getCurrentLatencySamples() const noexcept;
```

Add a private member:

```cpp
std::atomic<int> currentLatencySamples { 0 };
```

In `updateSettings`, after storing settings:

```cpp
    currentLatencySamples.store(prism::prismLatencyFor(settings.phaseMode, settings.qualityMode), std::memory_order_relaxed);
```

Implement:

```cpp
int PrismProcessor::getCurrentLatencySamples() const noexcept
{
    return currentLatencySamples.load(std::memory_order_relaxed);
}
```

In `PluginProcessor::processBlock`, after `prismProcessor.updateSettings(readSettings());`, add:

```cpp
    const auto latency = prismProcessor.getCurrentLatencySamples();
    if (latency != getLatencySamples())
        setLatencySamples(latency);
```

- [ ] **Step 7: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 6: Linear-Phase FIR Engine

**Files:**
- Create: `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.cpp`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write linear-phase behavior and latency tests**

Add this block after the existing natural phase comparison:

```cpp
    {
        kratomix::PrismEqAudioProcessor linearProcessor;
        disableSidechainForProcessorTest(linearProcessor);
        linearProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(linearProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        linearProcessor.parameters.getParameter("phaseMode")->setValueNotifyingHost(
            linearProcessor.parameters.getParameter("phaseMode")->convertTo0to1(
                static_cast<float>(kratomix::prism::PhaseMode::linearPhase)));

        auto linearBuffer = makeSineBuffer(2, 4096, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(linearBuffer);
        juce::MidiBuffer linearMidi;
        linearProcessor.processBlock(linearBuffer, linearMidi);
        linearProcessor.processBlock(linearBuffer, linearMidi);

        expect(rmsLevel(linearBuffer) > dryRms * 1.20f,
               "Linear phase mode should apply audible EQ gain",
               failures);
        expect(linearProcessor.getLatencySamples() >= 2048,
               "Linear phase mode should report convolution latency",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: FAIL because linear phase is only a parameter choice and there is no FIR engine.

- [ ] **Step 3: Create linear phase engine header**

Create `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.h`:

```cpp
#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismResponseModel.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
class PrismLinearPhaseEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();
    void updateSettings(const PrismSettings& settings);
    void process(juce::AudioBuffer<float>& buffer);
    int getLatencySamples() const noexcept { return latencySamples; }

private:
    static constexpr int fftOrder = 13;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int impulseSize = 4096;

    void rebuildImpulseIfNeeded();
    juce::AudioBuffer<float> buildImpulseResponse() const;

    juce::dsp::Convolution convolution;
    PrismSettings currentSettings;
    PrismSettings pendingSettings;
    double sampleRate = 44100.0;
    int numChannels = 2;
    int latencySamples = impulseSize / 2;
    bool prepared = false;
    bool impulseDirty = true;
};
}
```

- [ ] **Step 4: Create linear phase engine implementation**

Create `plugins/prism-eq/Source/Dsp/PrismLinearPhaseEngine.cpp`:

```cpp
#include "PrismLinearPhaseEngine.h"

namespace kratomix
{
void PrismLinearPhaseEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    numChannels = static_cast<int>(spec.numChannels);
    convolution.prepare(spec);
    prepared = true;
    impulseDirty = true;
    rebuildImpulseIfNeeded();
}

void PrismLinearPhaseEngine::reset()
{
    convolution.reset();
}

void PrismLinearPhaseEngine::updateSettings(const PrismSettings& settings)
{
    pendingSettings = settings;
    impulseDirty = true;
}

void PrismLinearPhaseEngine::process(juce::AudioBuffer<float>& buffer)
{
    if (! prepared)
        return;

    rebuildImpulseIfNeeded();
    juce::dsp::AudioBlock<float> block { buffer };
    juce::dsp::ProcessContextReplacing<float> context { block };
    convolution.process(context);
}

void PrismLinearPhaseEngine::rebuildImpulseIfNeeded()
{
    if (! impulseDirty)
        return;

    currentSettings = pendingSettings;
    auto impulse = buildImpulseResponse();
    convolution.loadImpulseResponse(std::move(impulse),
                                    sampleRate,
                                    juce::dsp::Convolution::Stereo::yes,
                                    juce::dsp::Convolution::Trim::no,
                                    juce::dsp::Convolution::Normalise::no);
    impulseDirty = false;
}

juce::AudioBuffer<float> PrismLinearPhaseEngine::buildImpulseResponse() const
{
    juce::AudioBuffer<float> impulse(numChannels, impulseSize);
    impulse.clear();

    std::array<float, prism::maxBands> noDynamicGain {};
    const auto response = makePrismResponseSnapshot(currentSettings, sampleRate, noDynamicGain, false);

    juce::dsp::FFT fft(fftOrder);
    std::vector<float> fftData(static_cast<size_t>(fftSize * 2), 0.0f);

    for (int bin = 0; bin <= fftSize / 2; ++bin)
    {
        const auto frequency = static_cast<float>(bin) * static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        const auto magnitudeDb = prismResponseGainDbAt(response, juce::jlimit(20.0f, 20000.0f, frequency));
        const auto magnitude = juce::Decibels::decibelsToGain(magnitudeDb);
        const auto linearPhase = -juce::MathConstants<float>::twoPi * static_cast<float>(bin) * static_cast<float>(latencySamples) / static_cast<float>(fftSize);

        fftData[static_cast<size_t>(bin * 2)] = magnitude * std::cos(linearPhase);
        fftData[static_cast<size_t>(bin * 2 + 1)] = magnitude * std::sin(linearPhase);

        if (bin > 0 && bin < fftSize / 2)
        {
            const auto mirror = fftSize - bin;
            fftData[static_cast<size_t>(mirror * 2)] = fftData[static_cast<size_t>(bin * 2)];
            fftData[static_cast<size_t>(mirror * 2 + 1)] = -fftData[static_cast<size_t>(bin * 2 + 1)];
        }
    }

    fft.performRealOnlyInverseTransform(fftData.data());

    for (int channel = 0; channel < numChannels; ++channel)
        for (int sample = 0; sample < impulseSize; ++sample)
            impulse.setSample(channel, sample, fftData[static_cast<size_t>(sample)] / static_cast<float>(fftSize));

    return impulse;
}
}
```

- [ ] **Step 5: Route linear phase in `PrismProcessor`**

In `PrismProcessor.h`, include:

```cpp
#include "Dsp/PrismLinearPhaseEngine.h"
```

Add private member:

```cpp
    PrismLinearPhaseEngine linearPhaseEngine;
```

In `prepare`, call:

```cpp
    linearPhaseEngine.prepare(spec);
```

In `reset`, call:

```cpp
    linearPhaseEngine.reset();
```

In `updateSettings`, call:

```cpp
    linearPhaseEngine.updateSettings(settings);
```

In `process`, replace the minimum-phase call:

```cpp
    minimumPhaseEngine.process(buffer, sidechainBuffer);
```

with:

```cpp
    if (settings.phaseMode == prism::PhaseMode::linearPhase)
        linearPhaseEngine.process(buffer);
    else
        minimumPhaseEngine.process(buffer, sidechainBuffer);
```

Set latency with `prismLatencyFor(settings.phaseMode, settings.qualityMode)` as done in Task 5.

- [ ] **Step 6: Register linear phase files**

In `plugins/prism-eq/plugin.cmake`, add:

```cmake
    Source/Dsp/PrismLinearPhaseEngine.h
    Source/Dsp/PrismLinearPhaseEngine.cpp
```

- [ ] **Step 7: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 7: Editor Controls For Real DSP Modes

**Files:**
- Modify: `plugins/prism-eq/Source/PluginEditor.h`
- Modify: `plugins/prism-eq/Source/PluginEditor.cpp`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Add editor construction test**

Extend the editor construction block:

```cpp
    expect(editor != nullptr && editor->getWidth() >= 1040,
           "DSP quality controls should fit in the Prism editor width",
           failures);
```

- [ ] **Step 2: Add quality combo box members**

In `PluginEditor.h`, add:

```cpp
    juce::Label qualityLabel;
    juce::ComboBox qualityModeBox;
    ComboBoxAttachment qualityModeAttachment;
```

Initialize attachment in the constructor initializer list:

```cpp
      qualityModeAttachment(pluginProcessor.parameters, "qualityMode", qualityModeBox)
```

- [ ] **Step 3: Configure quality controls**

In `PluginEditor.cpp`, after phase controls:

```cpp
    qualityLabel.setText("QUALITY", juce::dontSendNotification);
    styleCaption(qualityLabel);
    addAndMakeVisible(qualityLabel);

    qualityModeBox.addItemList(prism::qualityModeChoices(), 1);
    styleComboBox(qualityModeBox);
    addAndMakeVisible(qualityModeBox);
```

In `resized()`, after phase controls:

```cpp
    top.removeFromLeft(12);
    qualityLabel.setBounds(top.removeFromLeft(66).reduced(0, 8));
    qualityModeBox.setBounds(top.removeFromLeft(96).reduced(0, 8));
```

- [ ] **Step 4: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 8: Full Verification

**Files:**
- No code changes.

- [ ] **Step 1: Run Prism tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

- [ ] **Step 2: Build Prism plugin formats**

Run:

```bash
make build PLUGIN=prism-eq
```

Expected: build completes for AU, VST3, and Standalone targets.

- [ ] **Step 3: Validate the Audio Unit**

Run:

```bash
make validate PLUGIN=prism-eq
```

Expected: `AU VALIDATION SUCCEEDED`.

- [ ] **Step 4: Check worktree**

Run:

```bash
git status --short
```

Expected: only intentional Prism EQ source, tests, CMake, and plan files are listed, plus unrelated pre-existing user changes.

## Self-Review

- Screenshot coverage: high-frequency coefficient stability is covered by Tasks 2 and 3; shelf resonance behavior is covered by Task 3; true linear phase is covered by Task 6; oversampling is covered by Task 5.
- Product naming: The plan uses Kratomix Prism EQ terminology and does not add third-party product identity to code, comments, docs, or UI text.
- Scope control: Spectral dynamics, M/S routing, EQ matching, instance workflows, and sketching stay outside this plan because they are independent feature systems.
- Placeholder scan: The plan uses concrete paths, commands, code snippets, and expected outputs.
- Type consistency: `QualityMode`, extended `PhaseMode`, `PrismSettings`, `PrismMinimumPhaseEngine`, `PrismLinearPhaseEngine`, and `prismLatencyFor` are introduced before later tasks use them.
