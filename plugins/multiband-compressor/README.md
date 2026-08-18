# Kratomix Multiband Compressor

Kratomix Multiband Compressor is a graph-led dynamics processor for shaping up to six freely placed frequency ranges with musical compression, expansion-style range movement, optional external sidechain detection, and Kratomix analyzer feedback. Frequencies outside active bands remain unprocessed.

## Controls

- **Input, Output, Mix, Bypass**: global gain staging, wet/dry blend, and output bypass.
- **Analyzer**: input, output, combined, or gain-reduction graph view.
- **Lookahead**: off or 5 ms response mode with host latency reporting.
- **Dynamic bands**: double-click the graph to add a band, drag its centre to choose the frequency, and drag either edge or use the mouse wheel to set its width.
- **Band controls**: frequency, width, solo, audition, mode, detector source, threshold, range, ratio, attack, release, knee, output, and stereo link.

## UI

- Large dark spectrum graph using the Kratomix amber accent and restrained colored band regions.
- Freely placed, overlapping band ranges with draggable centre and width handles.
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
