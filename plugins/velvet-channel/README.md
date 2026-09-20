# Kratomix Velvet Channel

Kratomix Velvet Channel is the first plugin in the Kratomix line. It is a focused source-track strip for Logic Pro with continuous musical shaping, transformer-style harmonic color, a hardware-style bypass, and a central post-output VU meter.

## Controls

- **Input**: level into the color stage.
- **Drive**: continuous harmonic warmth amount with analog-style gain staging.
- **HPF**: continuous low-end cleanup before tone shaping.
- **Warmth**: broad low shelf around the body range.
- **Presence**: broad mid lift or cut.
- **Air**: high shelf for top-end openness with gentle transformer-style smoothing.
- **Output**: final gain compensation.
- **Bypass**: hardware-style output bypass.

## UI

- Fixed rack-style front panel in neo-orange metallic tones.
- Black italic `Kratomix` wordmark.
- Black hardware knobs with engraved labels.
- Central analog-inspired post-output VU meter.

## Build

From the monorepo root:

```bash
make build PLUGIN=velvet-channel
make test PLUGIN=velvet-channel
make run PLUGIN=velvet-channel
make validate PLUGIN=velvet-channel
```
