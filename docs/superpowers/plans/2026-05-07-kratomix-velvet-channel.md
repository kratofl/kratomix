# Kratomix Velvet Channel Implementation Plan

> This file now reflects the approved v1 implementation target used for the current build.

**Goal:** Ship a focused Logic Pro source-track strip with stepped musical controls, preamp-style color, rack-inspired UI, and post-output VU metering.

**Architecture:** `PluginProcessor` owns APVTS and host integration, `WarmthProcessor` owns stepped DSP plus smoothing and metering, and `PluginEditor` owns the neo-orange rack front panel with hardware-style controls.

**Tech Stack:** CMake, C++17, JUCE 8, macOS Audio Unit and Standalone.

---

### v1 Feature Set

- `Input`, `Drive`, `HPF`, `Warmth`, `Presence`, `Air`, `Output`, and `Bypass`.
- Thick, forward source-track voicing.
- Hardware-style stepped behavior for tone controls.
- Central analog-style VU meter driven from post-output signal.
- Fixed-size neo-orange metallic rack UI with black italic `Kratomix` branding.

### Verification Targets

- Build `KratomixVelvetChannelTests`.
- Build `KratomixVelvetChannel_AU`.
- Build `KratomixVelvetChannel_Standalone`.
- Run `auval -v aufx VChn Kmix`.
