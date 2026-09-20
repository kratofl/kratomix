# Kratomix Multiband Compressor

Kratomix Multiband Compressor is a graph-led dynamics processor for shaping up to six freely placed frequency ranges with musical compression, expansion-style range movement, and Kratomix analyzer feedback. Frequencies outside active bands remain unprocessed.

## Controls

- **Input, Output, Mix, Bypass**: global gain staging, wet/dry blend, and output bypass.
- **Analyzer**: input, output, combined, or gain-reduction graph view.
- **Lookahead**: off or 5 ms response mode with host latency reporting.
- **Dynamic bands**: double-click the graph to add a band. Drag the band point horizontally to set frequency and vertically to set threshold. Drag either edge or use the mouse wheel to set width.
- **Band controls**: editable value fields for frequency, width, threshold, range, ratio, attack, release, knee, output, and stereo link, plus solo, audition, and mode.

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

AU validation should be confirmed on macOS before release.
