# Kratomix Velvet Channel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first Kratomix Logic Pro plugin workspace as a JUCE Audio Unit effect.

**Architecture:** CMake creates a JUCE plugin target. `PluginProcessor` handles host state and audio blocks, `WarmthProcessor` handles DSP, and `PluginEditor` handles the compact control UI.

**Tech Stack:** CMake 3.22, C++17, JUCE 8, macOS Audio Unit.

---

### Task 1: Project Scaffold

**Files:**
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/CMakeLists.txt`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/.gitignore`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/README.md`

- [x] **Step 1: Create CMake plugin target**

Set the project name to `KratomixVelvetChannel`, product name to `Kratomix Velvet Channel`, manufacturer code to `Kmix`, plugin code to `VChn`, and formats to `AU VST3 Standalone`.

- [x] **Step 2: Add repository hygiene**

Ignore generated build, Xcode, plugin binary, and macOS metadata files.

- [x] **Step 3: Document build commands**

Document automatic JUCE fetch, local JUCE checkout builds, and AU validation.

### Task 2: Parameters And DSP

**Files:**
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/Parameters.h`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/Dsp/WarmthProcessor.h`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/Dsp/WarmthProcessor.cpp`

- [x] **Step 1: Define stable parameter IDs**

Create input gain, drive, high-pass, warmth, presence, air, and output gain parameters.

- [x] **Step 2: Implement the first tone path**

Apply input gain, gentle saturation, high-pass cleanup, low shelf, presence peak, high shelf, and output gain.

### Task 3: Host Integration And UI

**Files:**
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/PluginProcessor.h`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/PluginProcessor.cpp`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/PluginEditor.h`
- Create: `/Users/kratofl/projects/kratomix/velvet-channel/Source/PluginEditor.cpp`

- [x] **Step 1: Wire APVTS into the processor**

Use JUCE `AudioProcessorValueTreeState` for parameter ownership, automation, and state save/restore.

- [x] **Step 2: Add Logic-compatible bus support**

Support mono and stereo effect layouts with matching input and output channel sets.

- [x] **Step 3: Add a compact channel-strip editor**

Expose all seven controls with slider attachments and restrained Kratomix styling.
