# Kratomix Multiband Compressor

Kratomix Multiband Compressor is a graph-led dynamics processor for shaping up to six fixed crossover bands with musical compression, expansion-style range movement, optional external sidechain detection, and Kratomix analyzer feedback.

## Controls

- **Input, Output, Mix, Bypass**: global gain staging, wet/dry blend, and output bypass.
- **Analyzer**: input, output, combined, or gain-reduction graph view.
- **Lookahead**: off or 5 ms response mode with host latency reporting.
- **Crossovers**: five draggable graph handles define the six fixed processing bands.
- **Band controls**: enable, solo, audition, mode, detector source, threshold, range, ratio, attack, release, knee, output, and stereo link.

## UI

- Large dark spectrum graph using the Kratomix amber accent and restrained colored band regions.
- Draggable crossover handles and selectable band ranges.
- Bottom selected-band panel for direct dynamics controls.
- Right-side output meter and compact global toolbar.

## Build

From the monorepo root:

```bash
make build PLUGIN=multiband-compressor
make test PLUGIN=multiband-compressor
make run PLUGIN=multiband-compressor
make validate PLUGIN=multiband-compressor
```

AU validation and host-side sidechain behavior should be confirmed on macOS before release.
