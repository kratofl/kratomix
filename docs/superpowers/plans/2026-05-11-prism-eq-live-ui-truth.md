# Prism EQ Live UI Truth Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Kratomix Prism EQ's graph, analyzer, and dynamic-band display show the actual processing state instead of an approximate visual model.

**Architecture:** The DSP and UI will share one response model built from the same IIR coefficient factory. The audio thread keeps processing with preallocated JUCE processors, while the UI builds read-only response snapshots outside the audio thread. Dynamic EQ will use signed range values and publish detector telemetry so the selected-band controls and graph can show what is moving live.

**Tech Stack:** JUCE C++17, AudioProcessorValueTreeState, `juce::dsp::IIR`, existing Prism graph/analyzer buffering, monorepo `make` workflow.

---

## Scope Check

This plan is the first executable slice of the larger high-end Prism EQ roadmap. It covers the user's highest-priority requirement: a correct live UI that shows frequency response, analyzer activity, and dynamic movement accurately.

Separate plans should cover spectral dynamics, M/S and L/R per-band routing, EQ matching, multi-instance workflows, EQ sketching, expanded filter types and slopes, preset/A-B/undo workflows, and full linear-phase processing. Those systems are independent enough that combining them with this slice would make the work hard to verify.

Do not commit during execution unless the user explicitly asks. If the user later authorizes commits, use concise commit messages after each completed task.

## File Structure

- Create `plugins/prism-eq/Source/Dsp/PrismTypes.h`: shared Prism DSP data types currently embedded in `PrismProcessor.h`.
- Create `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`: shared coefficient factory declaration.
- Create `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`: shared coefficient factory implementation used by DSP and UI response rendering.
- Create `plugins/prism-eq/Source/Dsp/PrismResponseModel.h`: response snapshot API for graph rendering and tests.
- Create `plugins/prism-eq/Source/Dsp/PrismResponseModel.cpp`: exact response magnitude calculation from shared coefficients.
- Modify `plugins/prism-eq/Source/Dsp/PrismProcessor.h`: use `PrismTypes.h`, remove private coefficient factory, add telemetry atomics.
- Modify `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`: use shared coefficient factory, publish dynamic detector telemetry, support signed dynamic range.
- Modify `plugins/prism-eq/Source/Ui/PrismGraph.h`: remove approximate response responsibilities, add helpers for settings snapshots.
- Modify `plugins/prism-eq/Source/Ui/PrismGraph.cpp`: render exact static and live response snapshots, draw better dynamic movement.
- Modify `plugins/prism-eq/Source/PluginEditor.h`: add output meter and live selected-band labels.
- Modify `plugins/prism-eq/Source/PluginEditor.cpp`: expose analyzer controls, output meter, signed range labels, and live dynamic readout.
- Modify `plugins/prism-eq/Source/Parameters.h`: make dynamic range signed.
- Modify `plugins/prism-eq/plugin.cmake`: register new DSP source files.
- Modify `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`: add regression tests for exact response, signed dynamic behavior, telemetry, and UI construction.

### Task 1: Shared Types And Exact Response Model

**Files:**
- Create: `plugins/prism-eq/Source/Dsp/PrismTypes.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`
- Create: `plugins/prism-eq/Source/Dsp/PrismResponseModel.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismResponseModel.cpp`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing tests**

Add this include near the other Prism includes in `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`:

```cpp
#include "Source/Dsp/PrismResponseModel.h"
```

Add this block before the dynamic EQ behavior block:

```cpp
    {
        kratomix::PrismSettings responseSettings;
        auto& bell = responseSettings.bands[0];
        bell.enabled = true;
        bell.type = kratomix::prism::BandType::bell;
        bell.frequency = 1000.0f;
        bell.gainDb = 6.0f;
        bell.q = 1.0f;

        std::array<float, kratomix::prism::maxBands> noDynamicGain {};
        const auto snapshot = kratomix::makePrismResponseSnapshot(responseSettings, 48000.0, noDynamicGain, true);
        const auto centreDb = kratomix::prismResponseGainDbAt(snapshot, 1000.0f);
        const auto farDb = kratomix::prismResponseGainDbAt(snapshot, 10000.0f);

        expect(centreDb > 5.4f && centreDb < 6.6f,
               "Exact response model should show a +6 dB bell near its centre frequency",
               failures);
        expect(std::abs(farDb) < 1.0f,
               "Exact response model should return near-neutral response far from a moderate bell",
               failures);

        noDynamicGain[0] = -4.0f;
        bell.dynamicEnabled = true;
        const auto dynamicSnapshot = kratomix::makePrismResponseSnapshot(responseSettings, 48000.0, noDynamicGain, true);
        const auto dynamicCentreDb = kratomix::prismResponseGainDbAt(dynamicSnapshot, 1000.0f);

        expect(dynamicCentreDb < centreDb - 3.0f,
               "Exact response model should include dynamic gain movement when requested",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: build fails because `Source/Dsp/PrismResponseModel.h` does not exist.

- [ ] **Step 3: Move shared Prism data types into `PrismTypes.h`**

Create `plugins/prism-eq/Source/Dsp/PrismTypes.h`:

```cpp
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
    std::array<PrismBandSettings, prism::maxBands> bands {};
};
}
```

In `plugins/prism-eq/Source/Dsp/PrismProcessor.h`, replace the local definitions of `PrismAnalyzerFrame`, `PrismBandSettings`, and `PrismSettings` with:

```cpp
#include "Dsp/PrismTypes.h"
```

Keep the `PrismProcessor` class in `PrismProcessor.h`.

- [ ] **Step 4: Create the shared coefficient factory**

Create `plugins/prism-eq/Source/Dsp/PrismFilterDesign.h`:

```cpp
#pragma once

#include <JuceHeader.h>

#include "Dsp/PrismTypes.h"

namespace kratomix
{
using PrismIIRCoefficients = juce::dsp::IIR::Coefficients<float>;

PrismIIRCoefficients::Ptr makePrismCoefficients(double sampleRate,
                                                const PrismBandSettings& band,
                                                float dynamicGainDb);
}
```

Create `plugins/prism-eq/Source/Dsp/PrismFilterDesign.cpp`:

```cpp
#include "PrismFilterDesign.h"

namespace kratomix
{
namespace
{
float decibelsToGain(float decibels) noexcept
{
    return juce::Decibels::decibelsToGain(decibels);
}
}

PrismIIRCoefficients::Ptr makePrismCoefficients(double sampleRate,
                                                const PrismBandSettings& band,
                                                float dynamicGainDb)
{
    const auto frequency = juce::jlimit(20.0f, 20000.0f, band.frequency);
    const auto q = juce::jlimit(0.1f, 40.0f, band.q);
    const auto gain = decibelsToGain(band.gainDb + dynamicGainDb);

    switch (band.type)
    {
        case prism::BandType::lowShelf:
            return PrismIIRCoefficients::makeLowShelf(sampleRate, frequency, q, gain);
        case prism::BandType::highShelf:
            return PrismIIRCoefficients::makeHighShelf(sampleRate, frequency, q, gain);
        case prism::BandType::highPass:
            return PrismIIRCoefficients::makeHighPass(sampleRate, frequency, q);
        case prism::BandType::lowPass:
            return PrismIIRCoefficients::makeLowPass(sampleRate, frequency, q);
        case prism::BandType::notch:
            return PrismIIRCoefficients::makeNotch(sampleRate, frequency, q);
        case prism::BandType::bell:
        default:
            return PrismIIRCoefficients::makePeakFilter(sampleRate, frequency, q, gain);
    }
}
}
```

- [ ] **Step 5: Create the response model**

Create `plugins/prism-eq/Source/Dsp/PrismResponseModel.h`:

```cpp
#pragma once

#include <array>

#include <JuceHeader.h>

#include "Dsp/PrismFilterDesign.h"
#include "Dsp/PrismTypes.h"

namespace kratomix
{
struct PrismResponseSnapshot
{
    std::array<PrismIIRCoefficients::Ptr, prism::maxBands> coefficients {};
    std::array<bool, prism::maxBands> enabled {};
    double sampleRate = 44100.0;
};

PrismResponseSnapshot makePrismResponseSnapshot(const PrismSettings& settings,
                                                double sampleRate,
                                                const std::array<float, prism::maxBands>& dynamicGainDb,
                                                bool includeDynamicGain);

float prismResponseGainDbAt(const PrismResponseSnapshot& snapshot, float frequency);
}
```

Create `plugins/prism-eq/Source/Dsp/PrismResponseModel.cpp`:

```cpp
#include "PrismResponseModel.h"

namespace kratomix
{
PrismResponseSnapshot makePrismResponseSnapshot(const PrismSettings& settings,
                                                double sampleRate,
                                                const std::array<float, prism::maxBands>& dynamicGainDb,
                                                bool includeDynamicGain)
{
    PrismResponseSnapshot snapshot;
    snapshot.sampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;

    for (size_t index = 0; index < settings.bands.size(); ++index)
    {
        const auto& band = settings.bands[index];
        snapshot.enabled[index] = band.enabled;

        if (! band.enabled)
            continue;

        const auto movement = includeDynamicGain && band.dynamicEnabled ? dynamicGainDb[index] : 0.0f;
        snapshot.coefficients[index] = makePrismCoefficients(snapshot.sampleRate, band, movement);
    }

    return snapshot;
}

float prismResponseGainDbAt(const PrismResponseSnapshot& snapshot, float frequency)
{
    auto magnitude = 1.0;
    const auto clampedFrequency = juce::jlimit(20.0f, 20000.0f, frequency);

    for (size_t index = 0; index < snapshot.coefficients.size(); ++index)
    {
        if (! snapshot.enabled[index] || snapshot.coefficients[index] == nullptr)
            continue;

        magnitude *= snapshot.coefficients[index]->getMagnitudeForFrequency(clampedFrequency, snapshot.sampleRate);
    }

    return juce::Decibels::gainToDecibels(static_cast<float>(magnitude), -120.0f);
}
}
```

- [ ] **Step 6: Make the DSP processor use the shared coefficient factory**

In `plugins/prism-eq/Source/Dsp/PrismProcessor.h`, remove this private declaration:

```cpp
    static Coefficients::Ptr makeCoefficients(double sampleRate, const PrismBandSettings& band, float dynamicGainDb);
```

In `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`, add this include:

```cpp
#include "PrismFilterDesign.h"
```

In `updateFilterCoefficients()`, replace:

```cpp
        *filters[index].state = *makeCoefficients(sampleRate, settings.bands[index], dynamicGainDb[index]);
```

with:

```cpp
        *filters[index].state = *makePrismCoefficients(sampleRate, settings.bands[index], dynamicGainDb[index]);
```

Delete the old `PrismProcessor::makeCoefficients(...)` function at the bottom of `PrismProcessor.cpp`.

- [ ] **Step 7: Register the new files in CMake**

In `plugins/prism-eq/plugin.cmake`, add the new files to `KRATOMIX_PLUGIN_SOURCES` next to the existing DSP files:

```cmake
    Source/Dsp/PrismTypes.h
    Source/Dsp/PrismFilterDesign.h
    Source/Dsp/PrismFilterDesign.cpp
    Source/Dsp/PrismResponseModel.h
    Source/Dsp/PrismResponseModel.cpp
```

- [ ] **Step 8: Run tests to verify the exact response model**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 2: Exact Graph Response Rendering

**Files:**
- Modify: `plugins/prism-eq/Source/Ui/PrismGraph.h`
- Modify: `plugins/prism-eq/Source/Ui/PrismGraph.cpp`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Add a focused regression test for graph response consistency**

In `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`, add this block after the response model test from Task 1:

```cpp
    {
        kratomix::PrismEqAudioProcessor graphResponseProcessor;
        disableSidechainForProcessorTest(graphResponseProcessor);
        graphResponseProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(graphResponseProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 6.0f));

        kratomix::PrismSettings settings;
        auto& band = settings.bands[0];
        band.enabled = true;
        band.type = kratomix::prism::BandType::bell;
        band.frequency = graphResponseProcessor.parameters.getRawParameterValue("band01Frequency")->load();
        band.gainDb = graphResponseProcessor.parameters.getRawParameterValue("band01Gain")->load();
        band.q = graphResponseProcessor.parameters.getRawParameterValue("band01Q")->load();

        std::array<float, kratomix::prism::maxBands> noDynamicGain {};
        const auto snapshot = kratomix::makePrismResponseSnapshot(settings, 48000.0, noDynamicGain, true);

        expect(kratomix::prismResponseGainDbAt(snapshot, 1000.0f) > 5.0f,
               "Graph-created bands should be representable by the exact response model",
               failures);
    }
```

- [ ] **Step 2: Run the test**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: PASS. This verifies that the response model can represent graph-created APVTS state before the graph renderer is replaced.

- [ ] **Step 3: Add graph helpers for APVTS snapshots**

In `plugins/prism-eq/Source/Ui/PrismGraph.h`, add these private declarations:

```cpp
    PrismSettings readSettingsSnapshot() const;
    std::array<float, maxBands> dynamicGainSnapshot() const;
```

In `plugins/prism-eq/Source/Ui/PrismGraph.cpp`, add this include:

```cpp
#include "Dsp/PrismResponseModel.h"
```

Add these helper implementations near `parameterValue(...)`:

```cpp
PrismSettings PrismGraph::readSettingsSnapshot() const
{
    PrismSettings snapshot;

    if (state == nullptr)
        return snapshot;

    snapshot.inputGainDb = parameterValue("inputGain", 0.0f);
    snapshot.outputGainDb = parameterValue("outputGain", 0.0f);
    snapshot.mix = parameterValue("mix", 1.0f);
    snapshot.bypassed = parameterValue("bypass", 0.0f) >= 0.5f;
    snapshot.phaseMode = static_cast<PhaseMode>(juce::jlimit(
        0,
        static_cast<int>(PhaseMode::natural),
        static_cast<int>(std::round(parameterValue("phaseMode", 0.0f)))));

    for (int index = 1; index <= maxBands; ++index)
    {
        const auto prefix = bandPrefix(index);
        auto& band = snapshot.bands[static_cast<size_t>(index - 1)];
        band.enabled = parameterValue(prefix + "Enabled", 0.0f) >= 0.5f;
        band.type = static_cast<BandType>(juce::jlimit(
            0,
            static_cast<int>(BandType::notch),
            static_cast<int>(std::round(parameterValue(prefix + "Type", 0.0f)))));
        band.frequency = parameterValue(prefix + "Frequency", 1000.0f);
        band.gainDb = parameterValue(prefix + "Gain", 0.0f);
        band.q = parameterValue(prefix + "Q", 1.0f);
        band.dynamicEnabled = parameterValue(prefix + "DynamicEnabled", 0.0f) >= 0.5f;
        band.dynamicRangeDb = parameterValue(prefix + "DynamicRange", 0.0f);
        band.thresholdDb = parameterValue(prefix + "Threshold", -24.0f);
        band.attackMs = parameterValue(prefix + "Attack", 20.0f);
        band.releaseMs = parameterValue(prefix + "Release", 120.0f);
        band.sidechainSource = static_cast<SidechainSource>(juce::jlimit(
            0,
            static_cast<int>(SidechainSource::external),
            static_cast<int>(std::round(parameterValue(prefix + "SidechainSource", 0.0f)))));
        band.solo = parameterValue(prefix + "Solo", 0.0f) >= 0.5f;
    }

    return snapshot;
}

std::array<float, maxBands> PrismGraph::dynamicGainSnapshot() const
{
    std::array<float, maxBands> gains {};

    for (size_t index = 0; index < gains.size(); ++index)
        gains[index] = analyzerFrame.dynamicGainDb[index];

    return gains;
}
```

- [ ] **Step 4: Replace the approximate response drawing**

In `PrismGraph::paint`, replace the existing `drawResponse` lambda and calls with this exact snapshot-based version:

```cpp
    const auto settingsSnapshot = readSettingsSnapshot();
    const auto dynamicGains = dynamicGainSnapshot();
    const auto responseSampleRate = analyzerFrame.sampleRate > 0.0 ? analyzerFrame.sampleRate : 44100.0;
    const auto staticResponse = makePrismResponseSnapshot(settingsSnapshot, responseSampleRate, dynamicGains, false);
    const auto liveResponse = makePrismResponseSnapshot(settingsSnapshot, responseSampleRate, dynamicGains, true);

    const auto drawResponse = [&g, graph](const PrismResponseSnapshot& snapshot, juce::Colour colour, float thickness)
    {
        juce::Path responsePath;
        const auto steps = juce::jmax(24, static_cast<int>(graph.getWidth()));

        for (int step = 0; step <= steps; ++step)
        {
            const auto proportion = static_cast<float>(step) / static_cast<float>(steps);
            const auto x = graph.getX() + proportion * graph.getWidth();
            const auto frequency = xToFrequency(x, graph);
            const auto y = gainToY(prismResponseGainDbAt(snapshot, frequency), graph);

            if (step == 0)
                responsePath.startNewSubPath(x, y);
            else
                responsePath.lineTo(x, y);
        }

        g.setColour(colour);
        g.strokePath(responsePath, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    if (hasDynamicBands())
        drawResponse(staticResponse, responseColour().withAlpha(0.36f), 1.4f);
    drawResponse(liveResponse, responseColour(), 2.6f);
```

After this replacement, delete `float responseGainAt(float frequency, bool includeDynamicGain) const;` from the header and remove the old `PrismGraph::responseGainAt(...)` implementation from `PrismGraph.cpp`.

- [ ] **Step 5: Build and test**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 3: Signed Dynamic Range And Detector Telemetry

**Files:**
- Modify: `plugins/prism-eq/Source/Parameters.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.h`
- Modify: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write failing tests for signed dynamic behavior**

Replace the current positive-range dynamic test block in `ProcessorBehaviorTests.cpp` with this behavior:

```cpp
    {
        kratomix::PrismEqAudioProcessor upwardDynamicProcessor;
        disableSidechainForProcessorTest(upwardDynamicProcessor);
        upwardDynamicProcessor.prepareToPlay(48000.0, 2048);

        kratomix::prism::PrismGraph graph;
        graph.attachState(upwardDynamicProcessor.parameters);
        graph.setBounds(0, 0, 900, 460);
        graph.createBandAt(graph.pointForFrequencyAndGain(1000.0f, 0.0f));

        const auto prefix = kratomix::prism::bandPrefix(1);
        upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicEnabled")->setValueNotifyingHost(1.0f);
        upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "DynamicRange")->convertTo0to1(9.0f));
        upwardDynamicProcessor.parameters.getParameter(prefix + "Threshold")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "Threshold")->convertTo0to1(-48.0f));
        upwardDynamicProcessor.parameters.getParameter(prefix + "Attack")->setValueNotifyingHost(
            upwardDynamicProcessor.parameters.getParameter(prefix + "Attack")->convertTo0to1(0.1f));

        auto upwardBuffer = makeSineBuffer(2, 2048, 1000.0f, 48000.0);
        const auto dryRms = rmsLevel(upwardBuffer);
        juce::MidiBuffer upwardMidi;
        upwardDynamicProcessor.processBlock(upwardBuffer, upwardMidi);
        upwardDynamicProcessor.processBlock(upwardBuffer, upwardMidi);
        const auto upwardRms = rmsLevel(upwardBuffer);

        expect(upwardRms > dryRms * 1.15f,
               "A positive dynamic range value should lift matching audio when above threshold",
               failures);

        kratomix::PrismAnalyzerFrame upwardFrame;
        upwardDynamicProcessor.copyAnalyzerFrame(upwardFrame);
        expect(upwardFrame.dynamicGainDb[0] > 2.0f,
               "Analyzer frame should expose upward dynamic gain movement",
               failures);
        expect(upwardFrame.detectorLevelDb[0] > -60.0f,
               "Analyzer frame should expose detector level for live UI readout",
               failures);
        expect(upwardFrame.detectorOverThresholdDb[0] > 0.0f,
               "Analyzer frame should expose detector amount above threshold",
               failures);
    }
```

Add this parameter assertion near the existing parameter checks:

```cpp
    if (auto* rangeParameter = processor.parameters.getParameter("band01DynamicRange"))
    {
        expect(rangeParameter->convertFrom0to1(0.0f) < -29.0f,
               "Dynamic range should support negative movement",
               failures);
        expect(rangeParameter->convertFrom0to1(1.0f) > 29.0f,
               "Dynamic range should support positive movement",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: FAIL because `band01DynamicRange` still starts at `0.0f`, positive dynamic range still ducks, and detector telemetry fields are not populated.

- [ ] **Step 3: Make dynamic range signed**

In `plugins/prism-eq/Source/Parameters.h`, replace the dynamic range parameter range:

```cpp
            juce::NormalisableRange<float> { 0.0f, 30.0f, 0.1f },
```

with:

```cpp
            juce::NormalisableRange<float> { -30.0f, 30.0f, 0.1f },
```

Keep the default at `0.0f`.

- [ ] **Step 4: Add telemetry storage to the processor**

In `plugins/prism-eq/Source/Dsp/PrismProcessor.h`, add these private members next to `analyzerDynamicGainDb`:

```cpp
    std::array<std::atomic<float>, prism::maxBands> analyzerDynamicTargetGainDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorLevelDb {};
    std::array<std::atomic<float>, prism::maxBands> analyzerDetectorOverThresholdDb {};
    std::array<std::atomic<bool>, prism::maxBands> analyzerDetectorUsingExternalSidechain {};
```

In the constructor and `reset()`, initialize these arrays:

```cpp
    for (auto& gain : analyzerDynamicTargetGainDb)
        gain.store(0.0f, std::memory_order_relaxed);
    for (auto& level : analyzerDetectorLevelDb)
        level.store(-120.0f, std::memory_order_relaxed);
    for (auto& amount : analyzerDetectorOverThresholdDb)
        amount.store(0.0f, std::memory_order_relaxed);
    for (auto& flag : analyzerDetectorUsingExternalSidechain)
        flag.store(false, std::memory_order_relaxed);
```

- [ ] **Step 5: Publish telemetry in `copyAnalyzerFrame`**

In `PrismProcessor::copyAnalyzerFrame`, extend the existing loop over `analyzerDynamicGainDb`:

```cpp
    for (size_t index = 0; index < analyzerDynamicGainDb.size(); ++index)
    {
        destination.dynamicGainDb[index] = analyzerDynamicGainDb[index].load(std::memory_order_relaxed);
        destination.dynamicTargetGainDb[index] = analyzerDynamicTargetGainDb[index].load(std::memory_order_relaxed);
        destination.detectorLevelDb[index] = analyzerDetectorLevelDb[index].load(std::memory_order_relaxed);
        destination.detectorOverThresholdDb[index] = analyzerDetectorOverThresholdDb[index].load(std::memory_order_relaxed);
        destination.detectorUsingExternalSidechain[index] = analyzerDetectorUsingExternalSidechain[index].load(std::memory_order_relaxed);
    }
```

- [ ] **Step 6: Change dynamic target calculation**

In `PrismProcessor::updateDynamicGain`, replace this target calculation:

```cpp
            if (overThresholdDb > 0.0f)
                targetDb = -std::abs(band.dynamicRangeDb) * juce::jlimit(0.0f, 1.0f, overThresholdDb / 24.0f);
```

with:

```cpp
            if (overThresholdDb > 0.0f)
                targetDb = band.dynamicRangeDb * juce::jlimit(0.0f, 1.0f, overThresholdDb / 24.0f);

            analyzerDetectorLevelDb[index].store(detectorDb, std::memory_order_relaxed);
            analyzerDetectorOverThresholdDb[index].store(juce::jmax(0.0f, overThresholdDb), std::memory_order_relaxed);
            analyzerDetectorUsingExternalSidechain[index].store(useExternalSidechain, std::memory_order_relaxed);
```

After the dynamic-band `if` block and before smoothing, add:

```cpp
        if (! (band.enabled && band.dynamicEnabled && std::abs(band.dynamicRangeDb) > 0.001f))
        {
            analyzerDetectorLevelDb[index].store(-120.0f, std::memory_order_relaxed);
            analyzerDetectorOverThresholdDb[index].store(0.0f, std::memory_order_relaxed);
            analyzerDetectorUsingExternalSidechain[index].store(false, std::memory_order_relaxed);
        }

        analyzerDynamicTargetGainDb[index].store(targetDb, std::memory_order_relaxed);
```

- [ ] **Step 7: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 4: Live Dynamic Graph And Selected-Band Readout

**Files:**
- Modify: `plugins/prism-eq/Source/Ui/PrismGraph.cpp`
- Modify: `plugins/prism-eq/Source/PluginEditor.h`
- Modify: `plugins/prism-eq/Source/PluginEditor.cpp`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Add editor construction coverage for live readouts**

Extend the existing editor construction block in `ProcessorBehaviorTests.cpp`:

```cpp
    expect(editor != nullptr && editor->getNumChildComponents() >= 8,
           "Prism editor should expose graph, global controls, output meter, and selected-band controls",
           failures);
```

- [ ] **Step 2: Improve dynamic band drawing**

In `PrismGraph::paint`, inside the active-band node loop, replace the current dynamic line block:

```cpp
            if (dynamicEnabled)
            {
                const auto currentGain = parameterValue(prefix + "Gain", 0.0f) + dynamicGain;
                const auto currentPoint = pointForFrequencyAndGain(parameterValue(prefix + "Frequency", 1000.0f), currentGain);
                g.setColour(juce::Colour::fromRGB(95, 211, 186).withAlpha(0.55f));
                g.drawLine(point.x, point.y, currentPoint.x, currentPoint.y, 1.4f);
                g.fillEllipse(currentPoint.x - 3.5f, currentPoint.y - 3.5f, 7.0f, 7.0f);
            }
```

with:

```cpp
            if (dynamicEnabled)
            {
                const auto baseGain = parameterValue(prefix + "Gain", 0.0f);
                const auto rangeDb = parameterValue(prefix + "DynamicRange", 0.0f);
                const auto currentGain = baseGain + dynamicGain;
                const auto rangePoint = pointForFrequencyAndGain(parameterValue(prefix + "Frequency", 1000.0f), baseGain + rangeDb);
                const auto currentPoint = pointForFrequencyAndGain(parameterValue(prefix + "Frequency", 1000.0f), currentGain);
                const auto upward = rangeDb >= 0.0f;
                const auto movementColour = upward ? juce::Colour::fromRGB(120, 196, 255)
                                                   : juce::Colour::fromRGB(95, 211, 186);

                g.setColour(movementColour.withAlpha(0.18f));
                g.drawLine(point.x, point.y, rangePoint.x, rangePoint.y, 3.0f);
                g.setColour(movementColour.withAlpha(0.70f));
                g.drawLine(point.x, point.y, currentPoint.x, currentPoint.y, 1.8f);
                g.fillEllipse(currentPoint.x - 4.0f, currentPoint.y - 4.0f, 8.0f, 8.0f);
            }
```

- [ ] **Step 3: Add live readout labels to the editor header**

In `plugins/prism-eq/Source/PluginEditor.h`, add these private members:

```cpp
    juce::Label liveLabel;
    juce::Label detectorLabel;
    juce::Label movementLabel;
    juce::Timer liveReadoutTimer;
```

If `juce::Timer` cannot be used directly as a member with a callback, make `PrismEqAudioProcessorEditor` inherit privately from `juce::Timer`:

```cpp
class PrismEqAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
```

and add:

```cpp
    void timerCallback() override;
```

In the constructor, add:

```cpp
    liveLabel.setText("LIVE", juce::dontSendNotification);
    styleCaption(liveLabel);
    addAndMakeVisible(liveLabel);

    detectorLabel.setText("DET --.- dB", juce::dontSendNotification);
    styleCaption(detectorLabel);
    addAndMakeVisible(detectorLabel);

    movementLabel.setText("MOVE --.- dB", juce::dontSendNotification);
    styleCaption(movementLabel);
    addAndMakeVisible(movementLabel);

    startTimerHz(20);
```

In `resized()`, place the labels after the phase controls:

```cpp
    top.removeFromLeft(12);
    liveLabel.setBounds(top.removeFromLeft(42).reduced(0, 8));
    detectorLabel.setBounds(top.removeFromLeft(92).reduced(0, 8));
    movementLabel.setBounds(top.removeFromLeft(104).reduced(0, 8));
```

Add `timerCallback()`:

```cpp
void PrismEqAudioProcessorEditor::timerCallback()
{
    const auto selectedBand = graph.getSelectedBand();
    PrismAnalyzerFrame frame;
    pluginProcessor.copyAnalyzerFrame(frame);

    if (selectedBand <= 0)
    {
        detectorLabel.setText("DET --.- dB", juce::dontSendNotification);
        movementLabel.setText("MOVE --.- dB", juce::dontSendNotification);
        return;
    }

    const auto index = static_cast<size_t>(selectedBand - 1);
    detectorLabel.setText("DET " + juce::String(frame.detectorLevelDb[index], 1) + " dB", juce::dontSendNotification);
    movementLabel.setText("MOVE " + juce::String(frame.dynamicGainDb[index], 1) + " dB", juce::dontSendNotification);
}
```

- [ ] **Step 4: Build and test**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 5: Analyzer Controls And Output Meter Visibility

**Files:**
- Modify: `plugins/prism-eq/Source/PluginEditor.h`
- Modify: `plugins/prism-eq/Source/PluginEditor.cpp`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Add controls to the editor**

In `PluginEditor.h`, include the shared meter:

```cpp
#include "ui/VuMeter.h"
```

Add members:

```cpp
    juce::Label speedLabel;
    juce::Label rangeLabelGlobal;
    juce::Label scaleLabel;
    juce::Slider analyzerSpeedSlider;
    juce::Slider analyzerRangeSlider;
    juce::Slider gainScaleSlider;
    ui::VuMeter outputMeter;

    SliderAttachment analyzerSpeedAttachment;
    SliderAttachment analyzerRangeAttachment;
    SliderAttachment gainScaleAttachment;
```

Initialize the new members in the constructor initializer list after `phaseModeAttachment`:

```cpp
      outputMeter([this] { return pluginProcessor.getOutputLevel(); }),
      analyzerSpeedAttachment(pluginProcessor.parameters, "analyzerSpeed", analyzerSpeedSlider),
      analyzerRangeAttachment(pluginProcessor.parameters, "analyzerRange", analyzerRangeSlider),
      gainScaleAttachment(pluginProcessor.parameters, "gainScale", gainScaleSlider)
```

- [ ] **Step 2: Configure analyzer sliders**

In the constructor body after the phase combo setup, add:

```cpp
    for (auto* label : { &speedLabel, &rangeLabelGlobal, &scaleLabel })
    {
        styleCaption(*label);
        addAndMakeVisible(*label);
    }

    speedLabel.setText("SPEED", juce::dontSendNotification);
    rangeLabelGlobal.setText("RANGE", juce::dontSendNotification);
    scaleLabel.setText("SCALE", juce::dontSendNotification);

    analyzerSpeedSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerSpeedSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    analyzerRangeSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    analyzerRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainScaleSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    gainScaleSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);

    for (auto* slider : { &analyzerSpeedSlider, &analyzerRangeSlider, &gainScaleSlider })
    {
        slider->setColour(juce::Slider::thumbColourId, accentColour());
        slider->setColour(juce::Slider::trackColourId, accentColour().withAlpha(0.55f));
        slider->setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGB(34, 35, 38));
        addAndMakeVisible(*slider);
    }

    addAndMakeVisible(outputMeter);
```

- [ ] **Step 3: Place controls in the compact layout**

In `resized()`, after `graph.setBounds(...)`, lay out the bottom controls above the selected-band panel:

```cpp
    auto analyzerControlArea = bandArea.removeFromTop(24);
    speedLabel.setBounds(analyzerControlArea.removeFromLeft(52));
    analyzerSpeedSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    rangeLabelGlobal.setBounds(analyzerControlArea.removeFromLeft(58));
    analyzerRangeSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    scaleLabel.setBounds(analyzerControlArea.removeFromLeft(54));
    gainScaleSlider.setBounds(analyzerControlArea.removeFromLeft(120).reduced(4, 6));
    bandArea.removeFromTop(6);
```

Replace the current output area layout:

```cpp
    meterLabel.setBounds(outputArea.removeFromTop(24));
    bypassButton.setBounds(outputArea.removeFromBottom(110).reduced(0, 4));
    outputSlider.setBounds(outputArea.reduced(8, 10));
```

with:

```cpp
    meterLabel.setBounds(outputArea.removeFromTop(24));
    bypassButton.setBounds(outputArea.removeFromBottom(92).reduced(0, 4));
    outputSlider.setBounds(outputArea.removeFromBottom(210).reduced(8, 10));
    outputMeter.setBounds(outputArea.reduced(4, 8));
```

- [ ] **Step 4: Make gain scale affect graph display**

In `PrismGraph.cpp`, replace the fixed gain range constants:

```cpp
constexpr float minGainDb = -18.0f;
constexpr float maxGainDb = 18.0f;
```

with:

```cpp
constexpr float baseVisibleGainDb = 18.0f;
```

Change `gainToY` and `yToGain` signatures in the header and source to accept `visibleGainDb`:

```cpp
static float gainToY(float gainDb, juce::Rectangle<float> bounds, float visibleGainDb);
static float yToGain(float y, juce::Rectangle<float> bounds, float visibleGainDb);
float visibleGainRangeDb() const;
```

Implement:

```cpp
float PrismGraph::visibleGainRangeDb() const
{
    const auto scale = juce::jlimit(0.25f, 2.0f, parameterValue("gainScale", 1.0f));
    return baseVisibleGainDb / scale;
}
```

Replace `gainToY(gain, graph)` calls with `gainToY(gain, graph, visibleGainRangeDb())`, and replace `yToGain(point.y, graph)` with `yToGain(point.y, graph, visibleGainRangeDb())`.

Implement the new mappers:

```cpp
float PrismGraph::gainToY(float gainDb, juce::Rectangle<float> bounds, float visibleGainDb)
{
    const auto range = juce::jmax(3.0f, visibleGainDb);
    const auto normalized = juce::jmap(juce::jlimit(-range, range, gainDb), -range, range, 1.0f, 0.0f);
    return bounds.getY() + normalized * bounds.getHeight();
}

float PrismGraph::yToGain(float y, juce::Rectangle<float> bounds, float visibleGainDb)
{
    const auto range = juce::jmax(3.0f, visibleGainDb);
    const auto normalized = juce::jlimit(0.0f, 1.0f, (y - bounds.getY()) / juce::jmax(1.0f, bounds.getHeight()));
    return juce::jmap(normalized, 1.0f, 0.0f, -range, range);
}
```

- [ ] **Step 5: Run tests**

Run:

```bash
make test PLUGIN=prism-eq
```

Expected: `100% tests passed, 0 tests failed out of 1`.

### Task 6: Full Verification

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

- [ ] **Step 4: Check repository state**

Run:

```bash
git status --short
```

Expected: only intentional Prism EQ source, test, plan, and CMake changes are listed, plus any unrelated pre-existing user changes.

## Self-Review

- Spec coverage: This plan covers exact live response rendering, dynamic movement visibility, signed dynamic range behavior, analyzer controls, output metering, and selected-band live telemetry. These directly serve the user's requirement that Prism EQ clearly show what happens to frequencies live.
- Known gaps for separate plans: spectral dynamics, M/S and L/R routing, EQ Match, Instance List, EQ Sketch, expanded filter slopes/types, preset workflow, A/B and undo/redo, Character modes, and true linear-phase processing.
- Placeholder scan: The plan contains concrete file paths, code snippets, commands, and expected results.
- Type consistency: Shared types move to `PrismTypes.h`; `PrismProcessor`, `PrismGraph`, and response-model tests all consume the same `PrismSettings`, `PrismAnalyzerFrame`, and `PrismBandSettings` types.
