# Kratomix Velvet Channel Design

## Goal

Create the first Kratomix plugin workspace: a JUCE-based macOS Audio Unit effect for Logic Pro that adds warm harmonic color and simple musical EQ shaping.

## Product

The shared plugin line prefix is **Kratomix**. This plugin is named **Kratomix Velvet Channel** and lives at `/Users/kratofl/projects/kratomix/velvet-channel`.

## Architecture

The project uses CMake and JUCE. `PluginProcessor` owns host integration, parameter state, bus validation, preset serialization, and block processing. `WarmthProcessor` owns all tone processing so the DSP can evolve independently from UI and host code. `PluginEditor` provides a compact channel-strip control surface for the first version.

## DSP

The signal path is:

1. Input gain.
2. Gentle waveshaping with mild even-order color.
3. High-pass filter.
4. Low shelf warmth.
5. Broad presence band.
6. High shelf air.
7. Output gain.

The first version prioritizes stable, musical defaults and a clear extension point for future oversampling, metering, presets, or component-model refinements.

## Build And Verification

The project should configure with CMake and JUCE 8, then build AU, VST3, and Standalone targets on macOS. Logic Pro support is through the AU target. AU validation uses `auval -v aufx VChn Kmix`.
