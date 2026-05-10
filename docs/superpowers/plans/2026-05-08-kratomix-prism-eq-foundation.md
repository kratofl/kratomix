# Kratomix Prism EQ Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first Prism EQ foundation slice: shared Kratomix UI primitives, a registered Prism EQ plugin, stable v1 parameter IDs, neutral processing, and a graph-led editor shell.

**Architecture:** Keep reusable visual controls in `core/ui`, keep Prism-specific product code inside `plugins/prism-eq`, and keep DSP out of `PluginEditor.*`. This slice does not implement full dynamic EQ, analyzer FFT, or sidechain dynamics yet; it creates the stable host/plugin/parameter foundation they will attach to.

**Tech Stack:** JUCE 8, CMake, existing Kratomix Make layer, C++17, APVTS, simple console behavior tests.

---

## File Structure

- Create `core/ui/RackLookAndFeel.h`, `core/ui/RackLookAndFeel.cpp`, `core/ui/SteppedSlider.h`, `core/ui/VuMeter.h`, and `core/ui/VuMeter.cpp` by moving reusable controls from Velvet Channel without visual behavior changes.
- Modify `plugins/velvet-channel/Source/PluginEditor.h` to include shared UI headers from `core/ui`.
- Modify `plugins/velvet-channel/plugin.cmake` to remove plugin-local UI files from its source list.
- Modify `core/cmake/KratomixPlugin.cmake` so every plugin target and test target can include and compile shared core UI sources.
- Create `plugins/prism-eq/` with `make new-plugin SLUG=prism-eq NAME="Kratomix Prism EQ" CODE=PrEQ`.
- Create `plugins/prism-eq/Source/Parameters.h` for all global and 16-band stable IDs.
- Replace generated Prism processor/editor files with Prism-specific names and neutral behavior.
- Create `plugins/prism-eq/Source/Dsp/PrismProcessor.h` and `PrismProcessor.cpp` for neutral processing, input/output/mix/bypass smoothing, and output meter publication.
- Create `plugins/prism-eq/Source/Ui/PrismGraph.h` and `PrismGraph.cpp` for a graph-led editor shell.
- Replace `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp` with foundation behavior tests.

## Task 1: Shared UI Compile Boundary

**Files:**
- Modify: `plugins/velvet-channel/Tests/ProcessorBehaviorTests.cpp`
- Create: `core/ui/RackLookAndFeel.h`
- Create: `core/ui/RackLookAndFeel.cpp`
- Create: `core/ui/SteppedSlider.h`
- Create: `core/ui/VuMeter.h`
- Create: `core/ui/VuMeter.cpp`
- Modify: `plugins/velvet-channel/Source/PluginEditor.h`
- Modify: `plugins/velvet-channel/plugin.cmake`
- Modify: `core/cmake/KratomixPlugin.cmake`
- Delete: `plugins/velvet-channel/Source/Ui/RackLookAndFeel.h`
- Delete: `plugins/velvet-channel/Source/Ui/RackLookAndFeel.cpp`
- Delete: `plugins/velvet-channel/Source/Ui/SteppedSlider.h`
- Delete: `plugins/velvet-channel/Source/Ui/VuMeter.h`
- Delete: `plugins/velvet-channel/Source/Ui/VuMeter.cpp`
- Test: `plugins/velvet-channel/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing compile test**

Add this include near the other includes in `plugins/velvet-channel/Tests/ProcessorBehaviorTests.cpp`:

```cpp
#include "ui/SteppedSlider.h"
```

Add this test block after the bypass parameter test:

```cpp
    {
        expect(kratomix::ui::isSharedUiHeader,
               "SteppedSlider should come from the shared core UI header",
               failures);
        kratomix::ui::SteppedSlider slider([](double value) { return std::round(value); });
        expect(std::abs(slider.snapValue(2.7, juce::Slider::notDragging) - 3.0) < 1.0e-6,
               "Shared SteppedSlider should be available from core UI include paths",
               failures);
    }
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make test PLUGIN=velvet-channel`

Expected: compile failure because `ui/SteppedSlider.h` is not yet available through shared core include paths.

- [ ] **Step 3: Move reusable UI into core**

Move the existing reusable UI code into `core/ui` with unchanged namespace `kratomix::ui` and unchanged behavior. Add this sentinel to `core/ui/SteppedSlider.h` so tests prove the shared header is in use:

```cpp
inline constexpr bool isSharedUiHeader = true;
```

Update Velvet Channel includes from:

```cpp
#include "Ui/RackLookAndFeel.h"
#include "Ui/SteppedSlider.h"
#include "Ui/VuMeter.h"
```

to:

```cpp
#include "ui/RackLookAndFeel.h"
#include "ui/SteppedSlider.h"
#include "ui/VuMeter.h"
```

Update `core/cmake/KratomixPlugin.cmake` so plugin and test targets compile these shared sources and include `core`:

```cmake
get_filename_component(KRATOMIX_CORE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(KRATOMIX_CORE_UI_SOURCES
    "${KRATOMIX_CORE_DIR}/ui/RackLookAndFeel.cpp"
    "${KRATOMIX_CORE_DIR}/ui/VuMeter.cpp")
```

Append `${KRATOMIX_CORE_DIR}` to each target's include directories. Add `${KRATOMIX_CORE_UI_SOURCES}` to `target_sources` for plugin and test targets.

- [ ] **Step 4: Run the test to verify it passes**

Run: `make test PLUGIN=velvet-channel`

Expected: the test executable builds and prints `All tests passed`.

- [ ] **Step 5: Check status instead of committing**

Run: `git status --short`

Expected: shared UI files added under `core/ui`, plugin-local UI files deleted, and no build artifacts tracked.

## Task 2: Prism Scaffold And Stable Parameters

**Files:**
- Create: `plugins/prism-eq/`
- Create: `plugins/prism-eq/Source/Parameters.h`
- Modify: `plugins/prism-eq/Source/PluginProcessor.h`
- Modify: `plugins/prism-eq/Source/PluginProcessor.cpp`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Generate the Prism scaffold**

Run: `make new-plugin SLUG=prism-eq NAME="Kratomix Prism EQ" CODE=PrEQ`

Expected: `plugins/prism-eq` is created and registered through its `plugin.cmake`.

- [ ] **Step 2: Write the failing parameter tests**

Replace `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp` with a test that checks these behaviors:

```cpp
#include <JuceHeader.h>

#include "Source/Parameters.h"
#include "Source/PluginProcessor.h"

namespace
{
void expect(bool condition, const juce::String& message, int& failures)
{
    if (! condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

void expectParameter(juce::AudioProcessorValueTreeState& state, const juce::String& id, int& failures)
{
    expect(state.getParameter(id) != nullptr, "Missing parameter: " + id, failures);
}
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    int failures = 0;

    kratomix::PrismEqAudioProcessor processor;

    for (const auto* id : kratomix::prism::globalParameterIds())
        expectParameter(processor.parameters, id, failures);

    for (int index = 1; index <= kratomix::prism::maxBands; ++index)
    {
        const auto prefix = kratomix::prism::bandPrefix(index);
        expectParameter(processor.parameters, prefix + "Enabled", failures);
        expectParameter(processor.parameters, prefix + "Type", failures);
        expectParameter(processor.parameters, prefix + "Frequency", failures);
        expectParameter(processor.parameters, prefix + "Gain", failures);
        expectParameter(processor.parameters, prefix + "Q", failures);
        expectParameter(processor.parameters, prefix + "DynamicEnabled", failures);
        expectParameter(processor.parameters, prefix + "DynamicRange", failures);
        expectParameter(processor.parameters, prefix + "Threshold", failures);
        expectParameter(processor.parameters, prefix + "Attack", failures);
        expectParameter(processor.parameters, prefix + "Release", failures);
        expectParameter(processor.parameters, prefix + "SidechainSource", failures);
        expectParameter(processor.parameters, prefix + "Solo", failures);
    }

    expect(processor.parameters.getRawParameterValue("band01Enabled")->load() < 0.5f,
           "Band 01 should be disabled by default",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("inputGain")->load()) < 1.0e-6f,
           "Input gain should default to neutral",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("outputGain")->load()) < 1.0e-6f,
           "Output gain should default to neutral",
           failures);
    expect(std::abs(processor.parameters.getRawParameterValue("mix")->load() - 1.0f) < 1.0e-6f,
           "Mix should default to fully wet",
           failures);

    if (failures == 0)
    {
        std::cout << "All tests passed\n";
        return 0;
    }

    std::cerr << failures << " test(s) failed\n";
    return 1;
}
```

- [ ] **Step 3: Run the test to verify it fails**

Run: `make test PLUGIN=prism-eq`

Expected: compile failure or missing parameter failure because the scaffold still exposes only template parameters.

- [ ] **Step 4: Implement the parameter layout**

Create `plugins/prism-eq/Source/Parameters.h` with:

```cpp
#pragma once

#include <JuceHeader.h>

#include <array>
#include <memory>
#include <vector>

namespace kratomix::prism
{
inline constexpr int maxBands = 16;

enum class BandType
{
    bell = 0,
    lowShelf,
    highShelf,
    highPass,
    lowPass,
    notch
};

enum class SidechainSource
{
    main = 0,
    external
};

inline juce::String bandPrefix(int oneBasedIndex)
{
    return "band" + juce::String(oneBasedIndex).paddedLeft('0', 2);
}

inline constexpr std::array<const char*, 8> globalParameterIds()
{
    return { "inputGain", "outputGain", "mix", "bypass", "analyzerMode", "analyzerSpeed", "analyzerRange", "gainScale" };
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
}
```

Update `PluginProcessor` to use `kratomix::prism::createParameterLayout()` and expose class name `PrismEqAudioProcessor`.

- [ ] **Step 5: Run the test to verify it passes**

Run: `make test PLUGIN=prism-eq`

Expected: the test executable builds and prints `All tests passed`.

## Task 3: Neutral Processing Foundation

**Files:**
- Create: `plugins/prism-eq/Source/Dsp/PrismProcessor.h`
- Create: `plugins/prism-eq/Source/Dsp/PrismProcessor.cpp`
- Modify: `plugins/prism-eq/Source/PluginProcessor.h`
- Modify: `plugins/prism-eq/Source/PluginProcessor.cpp`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing neutral processing test**

Add this helper to the Prism test file:

```cpp
juce::AudioBuffer<float> makeRampBuffer(int channels, int samples)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, static_cast<float>(sample + 1) * 0.001f);
    return buffer;
}

bool buffersAlmostEqual(const juce::AudioBuffer<float>& lhs,
                        const juce::AudioBuffer<float>& rhs,
                        float tolerance)
{
    if (lhs.getNumChannels() != rhs.getNumChannels() || lhs.getNumSamples() != rhs.getNumSamples())
        return false;
    for (int channel = 0; channel < lhs.getNumChannels(); ++channel)
        for (int sample = 0; sample < lhs.getNumSamples(); ++sample)
            if (std::abs(lhs.getSample(channel, sample) - rhs.getSample(channel, sample)) > tolerance)
                return false;
    return true;
}
```

Add this behavior block after the parameter checks:

```cpp
    processor.prepareToPlay(48000.0, 128);
    juce::MidiBuffer midi;
    auto buffer = makeRampBuffer(2, 128);
    const auto before = buffer;
    processor.processBlock(buffer, midi);

    expect(buffersAlmostEqual(buffer, before, 1.0e-6f),
           "Default Prism processing should be neutral with all bands disabled",
           failures);
    expect(processor.getOutputLevel() > 0.0f,
           "Output meter should react to processed audio",
           failures);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make test PLUGIN=prism-eq`

Expected: compile failure because `getOutputLevel()` and Prism processing foundation do not exist yet.

- [ ] **Step 3: Implement neutral processing**

Create `PrismProcessor` with `prepare`, `reset`, `updateSettings`, `process`, and `getOutputLevel`. For this slice, it applies smoothed input gain, output gain, mix, and bypass only. It does not apply EQ filters yet because all v1 bands are still only parameterized.

```cpp
struct PrismSettings
{
    float inputGainDb = 0.0f;
    float outputGainDb = 0.0f;
    float mix = 1.0f;
    bool bypassed = false;
};
```

`PluginProcessor::processBlock` should read these global parameters, update `PrismProcessor`, and process the main buffer. Add `float getOutputLevel() const noexcept`.

- [ ] **Step 4: Run the test to verify it passes**

Run: `make test PLUGIN=prism-eq`

Expected: the test executable builds and prints `All tests passed`.

## Task 4: Graph-Led Editor Shell

**Files:**
- Create: `plugins/prism-eq/Source/Ui/PrismGraph.h`
- Create: `plugins/prism-eq/Source/Ui/PrismGraph.cpp`
- Modify: `plugins/prism-eq/Source/PluginEditor.h`
- Modify: `plugins/prism-eq/Source/PluginEditor.cpp`
- Modify: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`
- Modify: `plugins/prism-eq/plugin.cmake`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing editor test**

Add this block to the Prism test file:

```cpp
    std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
    expect(editor != nullptr, "Prism editor should be constructible", failures);
    expect(editor != nullptr && editor->getWidth() >= 900, "Prism editor should be graph-led and wide", failures);
    expect(editor != nullptr && editor->getHeight() >= 520, "Prism editor should leave room for graph and controls", failures);
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `make test PLUGIN=prism-eq`

Expected: fails because the generated editor is too small and still template-shaped.

- [ ] **Step 3: Implement the graph-led editor shell**

Create `PrismGraph` as a `juce::Component` that paints:

```cpp
g.fillAll(juce::Colour::fromRGB(12, 13, 15));
g.setColour(juce::Colour::fromRGB(47, 49, 54));
```

Draw logarithmic vertical grid lines for common frequencies and horizontal gain lines. In `PluginEditor`, set size to `1040 x 620`, paint a dark Kratomix frame, add a top title strip, the graph component, and a right output strip. Use shared Kratomix UI colors and avoid any non-Kratomix product text.

- [ ] **Step 4: Run the test to verify it passes**

Run: `make test PLUGIN=prism-eq`

Expected: the test executable builds and prints `All tests passed`.

## Task 5: Repository Verification

**Files:**
- No new files unless earlier task verification reveals necessary fixes.

- [ ] **Step 1: Run focused plugin tests**

Run: `make test PLUGIN=velvet-channel`

Expected: `All tests passed`.

Run: `make test PLUGIN=prism-eq`

Expected: `All tests passed`.

- [ ] **Step 2: Run full test target**

Run: `make test`

Expected: all registered plugin tests pass.

- [ ] **Step 3: Run status check**

Run: `git status --short`

Expected: source/docs changes only. No generated build artifacts, plugin binaries, Xcode derived files, or local metadata should appear.
