# Kratomix Velvet Channel Design

## Goal

Create the first Kratomix plugin workspace as a JUCE-based macOS Audio Unit effect for Logic Pro, tuned for source tracks with a thick, forward preamp-style color and a rack-inspired interface.

## Product

The shared plugin line prefix is **Kratomix**. This plugin is named **Kratomix Velvet Channel** and lives at `/Users/kratofl/Projects/kratomix/velvet-channel`.

## Architecture

The project uses CMake and JUCE. `PluginProcessor` owns host integration, parameter state, bus validation, preset serialization, meter access, and block processing. `WarmthProcessor` owns stepped control mapping, smoothing, bypass behavior, tone processing, and VU feed generation. `PluginEditor` owns the fixed rack-style front panel, hardware-like controls, and VU presentation.

## DSP

The signal path is:

1. Bypass crossfade.
2. Input trim.
3. Saturating drive stage with asymmetric warmth.
4. Stepped high-pass filter.
5. Stepped low shelf warmth.
6. Stepped presence bell.
7. Stepped high shelf air.
8. Output trim.
9. Post-output VU feed.

`Drive`, `HPF`, `Warmth`, `Presence`, and `Air` are intentionally quantized to musical hardware-style steps. Input and output remain continuous. Gain and wet-state changes are smoothed to avoid clicks during playback and automation.

## UI

The first version uses a fixed-size rack panel around `940 x 320 px`. The front plate is neo-orange metallic with brushed shading, dark screws, black hardware knobs, engraved labels, a black italic `Kratomix` wordmark, and a central analog-style VU meter.

## Build And Verification

The project configures with CMake and JUCE 8 and should build the test target plus AU and Standalone targets on macOS. Logic Pro support is through the AU target. Validation uses the standalone test executable and `auval -v aufx VChn Kmix`.
