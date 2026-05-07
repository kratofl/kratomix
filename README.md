# Kratomix Velvet Channel

Kratomix Velvet Channel is a JUCE-based macOS audio effect plugin for Logic Pro. It starts as a warm channel-strip style processor with input drive, subtle harmonic color, musical EQ shaping, and output trim.

## Controls

- **Input**: level into the color stage.
- **Drive**: harmonic warmth amount.
- **High-pass**: low-end cleanup before tone shaping.
- **Warmth**: low shelf around the body range.
- **Presence**: broad mid lift or cut.
- **Air**: high shelf for top-end openness.
- **Output**: final gain compensation.

## Build

Requirements:

- macOS with Xcode command line tools.
- CMake 3.22 or newer.
- JUCE 8, either fetched by CMake or supplied locally.

Fetch JUCE automatically:

```bash
cmake -S . -B build -G Xcode
cmake --build build --config Release
```

Use a local JUCE checkout:

```bash
cmake -S . -B build -G Xcode -DJUCE_DIR=/path/to/JUCE
cmake --build build --config Release
```

JUCE copies the built AU component after build when possible. If Logic Pro does not show it, validate the AU and restart Logic:

```bash
auval -v aufx VChn Kmix
```

## Project Line

The shared plugin prefix is **Kratomix**. Future plugins can use the same pattern, for example `Kratomix Tape Bloom`, `Kratomix Iron Bus`, or `Kratomix ClearComp`.
