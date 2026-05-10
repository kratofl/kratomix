# Prism EQ Masking Analyzer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add sidechain-based masking visualization, hover peak readout, and a prepared phase mode control to Kratomix Prism EQ.

**Architecture:** Keep analyzer interpretation in a small Prism UI helper so it can be tested without rendering. Keep host-visible controls in `Parameters.h`, graph rendering in `PrismGraph`, and leave realtime DSP unchanged in this slice. The masking feature uses existing Post and Sidechain analyzer lanes; it does not add multi-instance communication.

**Tech Stack:** JUCE C++17, AudioProcessorValueTreeState, existing Prism graph/analyzer buffering, monorepo `make` workflow.

---

### Task 1: Parameter Surface

**Files:**
- Modify: `plugins/prism-eq/Source/Parameters.h`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing test**

Add checks that `phaseMode` exists, defaults to zero latency, and analyzer choices include `Masking` at the end:

```cpp
expectParameter(processor.parameters, "phaseMode", failures);
expect(std::abs(processor.parameters.getRawParameterValue("phaseMode")->load()) < 1.0e-6f,
       "Phase mode should default to zero latency",
       failures);
expect(kratomix::prism::analyzerModeChoices().contains("Masking"),
       "Analyzer modes should include sidechain masking",
       failures);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target KratomixPrismEqTests && ./build/KratomixPrismEqTests_artefacts/Release/KratomixPrismEqTests`

Expected: FAIL with missing `phaseMode` and missing `Masking`.

- [ ] **Step 3: Implement the parameter changes**

Append `phaseMode` to `globalParameterIds()`, add `phaseModeChoices()` returning `Zero Latency` and `Natural`, append `Masking` to `analyzerModeChoices()`, and create an `AudioParameterChoice` for `phaseMode` defaulting to `Zero Latency`.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target KratomixPrismEqTests && ./build/KratomixPrismEqTests_artefacts/Release/KratomixPrismEqTests`

Expected: PASS.

### Task 2: Analyzer Feature Helpers

**Files:**
- Create: `plugins/prism-eq/Source/Ui/AnalyzerFeatures.h`
- Test: `plugins/prism-eq/Tests/ProcessorBehaviorTests.cpp`

- [ ] **Step 1: Write the failing test**

Add tests for pure helper behavior:

```cpp
std::array<float, 8> post { -90.0f, -58.0f, -24.0f, -18.0f, -23.0f, -70.0f, -80.0f, -82.0f };
std::array<float, 8> side { -92.0f, -61.0f, -28.0f, -19.0f, -27.0f, -71.0f, -80.0f, -82.0f };
const auto mask = kratomix::prism::computeMaskingBins(post, side, -60.0f, 8.0f);
expect(mask[3] > 0.9f, "Masking helper should mark strong overlapping bins", failures);
expect(mask[0] < 0.01f, "Masking helper should ignore noise floor bins", failures);

const auto peak = kratomix::prism::findNearestAnalyzerPeak(post, 2, -60.0f);
expect(peak.has_value() && peak->index == 3, "Peak helper should find the nearest local maximum", failures);
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target KratomixPrismEqTests && ./build/KratomixPrismEqTests_artefacts/Release/KratomixPrismEqTests`

Expected: FAIL because `AnalyzerFeatures.h` and helpers do not exist.

- [ ] **Step 3: Implement helper functions**

Create a header-only helper with `AnalyzerPeak`, `computeMaskingBins`, and `findNearestAnalyzerPeak`. Use no allocation in rendering paths; the helpers operate on fixed-size arrays or `std::array` templates.

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target KratomixPrismEqTests && ./build/KratomixPrismEqTests_artefacts/Release/KratomixPrismEqTests`

Expected: PASS.

### Task 3: Graph Rendering And Hover Readout

**Files:**
- Modify: `plugins/prism-eq/Source/Ui/PrismGraph.h`
- Modify: `plugins/prism-eq/Source/Ui/PrismGraph.cpp`

- [ ] **Step 1: Add graph state for hover**

Track `hoverPoint`, clear it on mouse exit, update it in `mouseMove`, and call `setInterceptsMouseClicks(true, true)` if needed so the graph receives move events.

- [ ] **Step 2: Render masking mode**

When `analyzerMode` is `Masking`, draw Post and Sidechain spectra plus restrained rose vertical fills from `computeMaskingBins(postSpectrum, sidechainSpectrum, -72.0f, 9.0f)`. Only draw strong masking if `analyzerFrame.sidechainActive` is true.

- [ ] **Step 3: Render hover readout**

When the cursor is inside the graph, map cursor x to a spectrum bin, call `findNearestAnalyzerPeak` on the most relevant visible lane, and draw a compact label with cursor frequency and peak level. Do not create or move bands from hover.

- [ ] **Step 4: Build**

Run: `cmake --build build --target KratomixPrismEqTests`

Expected: build succeeds.

### Task 4: Editor Control

**Files:**
- Modify: `plugins/prism-eq/Source/PluginEditor.h`
- Modify: `plugins/prism-eq/Source/PluginEditor.cpp`

- [ ] **Step 1: Add a phase mode combo box**

Place a compact `phaseModeBox` near the analyzer mode selector. It uses `phaseModeChoices()` and an APVTS `ComboBoxAttachment` bound to `phaseMode`.

- [ ] **Step 2: Keep layout compact**

Ensure the top bar does not overlap at the current editor size. Labels stay short: `Mode` for analyzer and `Phase` for phase mode.

- [ ] **Step 3: Build**

Run: `cmake --build build --target KratomixPrismEqTests`

Expected: build succeeds.

### Task 5: Full Verification

**Files:**
- No code changes.

- [ ] **Step 1: Run plugin tests**

Run: `make test PLUGIN=prism-eq`

Expected: PASS.

- [ ] **Step 2: Build and install plugin**

Run: `make build PLUGIN=prism-eq`

Expected: PASS.

- [ ] **Step 3: Validate AU**

Run: `auval -v aufx PrD2 Kmix`

Expected: `AU VALIDATION SUCCEEDED`.

- [ ] **Step 4: Check worktree**

Run: `git status --short`

Expected: only intentional source, docs, and test changes are listed.
