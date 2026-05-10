# Kratomix Prism EQ Design

## Goal

Create **Kratomix Prism EQ**, a graph-first dynamic equalizer for Logic Pro-first macOS plugin work. The plugin should feel like a modern surgical EQ workspace while staying inside the Kratomix brand: dark, restrained, readable, amber-accented, and focused on direct audio control instead of marketing copy.

The first version should be a real usable graphical dynamic EQ. Advanced spectral editing features can follow after the core graph, analyzer, sidechain, and dynamic band workflow are stable.

## Product

- Product name: `Kratomix Prism EQ`
- Plugin slug: `prism-eq`
- CMake target: `KratomixPrismEq`
- Bundle ID: `com.kratomix.prismeq`
- Plugin code: `PrD2`
- Primary format target: Audio Unit for Logic Pro
- Additional project formats: VST3 and Standalone, matching the current Kratomix plugin template

The plugin must not use third-party product, hardware, plugin, or manufacturer names as product identity in source, comments, docs, UI text, or metadata.

## V1 Scope

V1 includes:

- Large logarithmic frequency graph from 20 Hz to 20 kHz.
- 16 pre-registered EQ bands with stable parameter IDs.
- Only active bands shown as graph nodes.
- Double-click empty graph space to create the next free band at the clicked frequency and gain.
- Draggable band nodes.
- Per-band filter types: Bell, Low Shelf, High Shelf, High-pass, Low-pass, and Notch.
- Per-band dynamic EQ mode with range, threshold, attack, release, and detector source.
- External sidechain trigger support for dynamic bands.
- Analyzer overlays for Pre, Post, and Sidechain signals.
- Sidechain-based masking overlay that highlights frequency areas where the processed signal and sidechain overlap.
- Hover peak readout in the analyzer for fast resonance and frequency identification.
- Phase mode control with a zero-latency default and a higher-quality natural mode target.
- Selected-band floating control panel.
- Global input gain, output gain, mix, gain scale, bypass, and output metering.

V1 excludes:

- Spectral dynamics.
- EQ matching.
- Freehand curve sketching.
- Multi-instance communication.
- Additional filter shapes beyond the six v1 types.
- Full linear-phase convolution processing.
- Automatic multi-instance track discovery.

## Architecture

### Shared Kratomix Core UI

Move reusable UI pieces out of `plugins/velvet-channel/Source/Ui` and into a shared `core/ui` area when they are not coupled to Velvet Channel's rack layout. The shared layer should contain only generic Kratomix UI primitives:

- Palette and typography tokens.
- Shared look-and-feel helpers.
- Knob, toggle, meter, and compact control styling.
- Generic graph/grid and curve-rendering helpers when they are not Prism-specific.

Velvet Channel keeps its plugin-specific rack layout and product-specific paint code. Prism EQ uses the shared style tokens and creates its own graph-led layout.

The CMake helper in `core/cmake/KratomixPlugin.cmake` should support shared core source/include registration cleanly, rather than requiring every plugin to copy shared UI files into its own source list.

### Prism Processor And DSP

`plugins/prism-eq/Source/PluginProcessor.*` owns:

- JUCE host integration.
- Main input/output bus validation.
- Optional external sidechain input bus.
- AudioProcessorValueTreeState construction and serialization.
- Realtime block processing coordination.
- Analyzer and meter snapshot access for the editor.

`plugins/prism-eq/Source/Dsp/` owns:

- Band state extraction from parameters.
- Filter coefficient generation.
- Filter processing.
- Dynamic detector processing.
- Dynamic gain smoothing.
- Sidechain detector routing.
- Bypass and mix crossfade.
- Output metering.
- Analyzer feed generation.

No DSP, detector, sidechain, or analyzer logic should live in `PluginEditor.*`.

### Prism UI

`plugins/prism-eq/Source/Ui/` owns Prism-specific UI components:

- Frequency graph component.
- Band node hit testing and dragging.
- Response curve renderer.
- Analyzer overlay renderer.
- Selected-band floating panel.
- Output strip and meter presentation.
- Analyzer/global controls.

The editor should communicate with the processor through APVTS attachments, explicit parameter updates, and read-only display snapshots. UI rendering must not require locks or allocation in `processBlock`.

## Parameters

Parameter IDs must be stable from the first release. Bands are pre-registered even when inactive.

Global parameters:

- `inputGain`
- `outputGain`
- `mix`
- `bypass`
- `analyzerMode`
- `analyzerSpeed`
- `analyzerRange`
- `gainScale`
- `phaseMode`

Each band index from `01` through `16` uses:

- `bandNNEnabled`
- `bandNNType`
- `bandNNFrequency`
- `bandNNGain`
- `bandNNQ`
- `bandNNDynamicEnabled`
- `bandNNDynamicRange`
- `bandNNThreshold`
- `bandNNAttack`
- `bandNNRelease`
- `bandNNSidechainSource`
- `bandNNSolo`

Recommended defaults:

- All bands disabled.
- Bell as the default type for newly created bands.
- Newly created band gain from clicked graph position, clamped to the visible gain range.
- Frequency from clicked graph position using logarithmic mapping.
- Neutral global gain and mix.
- Bypass off.
- Dynamic mode off for newly created bands.

## DSP

The signal path is:

1. Clear output channels that do not have matching input channels.
2. Copy dry signal for mix and bypass handling.
3. Apply smoothed input gain.
4. Feed the Pre analyzer.
5. Process the 16-band EQ chain, skipping inactive bands.
6. For dynamic bands, run the detector from either the main signal or external sidechain.
7. Apply smoothed per-band dynamic gain offset according to dynamic range, threshold, attack, and release.
8. Feed the Post analyzer.
9. Apply smoothed output gain.
10. Apply mix and soft bypass crossfade.
11. Update output meters.

Dynamic EQ behavior is range-based. The static band gain defines the normal EQ curve, while dynamic range defines how far that band can move in response to its detector. Negative dynamic range reduces energy when the detector crosses threshold. Positive dynamic range expands or lifts when the detector crosses threshold.

If external sidechain is selected but no sidechain bus is available or active, the detector should fall back to the main signal in a predictable way and expose that state to the UI.

All realtime paths must avoid allocation, locks, file operations, and network operations. Parameter changes that affect audible gain or filter movement need smoothing to avoid zipper noise. Denormal protection remains required in `processBlock`.

## Analyzer

V1 analyzer modes:

- Pre.
- Post.
- Sidechain.
- Pre + Post.
- Pre + Post + Sidechain.
- Masking.

The analyzer should use a realtime-safe buffering strategy. `processBlock` may write audio into preallocated lock-free or wait-free buffers. FFT and visual smoothing should run outside the audio thread where possible.

The graph should support:

- Separate visual colors for Pre, Post, and Sidechain.
- A masking overlay derived from Post and Sidechain spectra when sidechain input is active.
- Adjustable range.
- Adjustable speed.
- Hover readout for nearest analyzer peak, including frequency and level.
- Freeze can be deferred if it complicates the v1 analyzer path.

The masking overlay is sidechain-based for V1. It should not attempt automatic multi-instance communication. A frequency region is considered masked when both the processed signal and sidechain signal have meaningful energy in nearby analyzer bins and their levels are close enough to compete. The UI should display these regions as restrained red/rose vertical fills behind the EQ curve, not as a separate product workflow.

Hover peak readout should be passive and should not create or move bands. When the cursor is inside the graph, the UI shows the frequency at the cursor and the closest significant peak from the currently visible analyzer lanes. This is a readout for resonance finding and manual EQ decisions; automatic peak-to-band creation can be added later.

Phase mode starts with two choices:

- `Zero Latency`: current realtime IIR processing, no added latency.
- `Natural`: higher-quality response target for future filter refinement without adding linear-phase latency in V1.

The UI must not overstate the phase mode. If the processing path is equivalent in this implementation slice, the control may be present and documented as a prepared mode while DSP differences are implemented in a later slice.

Analyzer display quality should be good enough for mixing decisions, but the v1 priority is stability and low audio-thread risk.

## UI Layout

The first screen is the working plugin, not a landing page or instructional screen.

Layout:

- Top bar: Kratomix wordmark, `PRISM EQ`, analyzer mode, copy/menu controls, and compact global controls.
- Center: full-width graph-led EQ editor.
- Right side: slim output meter and output gain.
- Floating selected-band panel near the selected node.
- Bottom bar: analyzer controls, gain scale, and graph range controls.

A/B and undo/redo should not appear as inactive buttons. They can be added in v1 only if implemented as real stateful actions; otherwise they belong to a later release.

Graph layers, back to front:

1. Dark Kratomix background.
2. Frequency and gain grid.
3. Pre analyzer.
4. Post analyzer.
5. Sidechain analyzer.
6. Individual band curves and dynamic movement fills.
7. Total EQ response curve.
8. Active band nodes.
9. Selection affordances and floating panel.

Visual direction:

- Dark charcoal/black graph base.
- Amber total response curve.
- Restrained colored per-band curves.
- Compact hardware-like controls derived from Kratomix shared UI.
- Readable labels and values.
- No marketing copy inside the plugin UI.

## UI Interactions

- Double-click empty graph: create the next free band.
- Drag node horizontally: frequency.
- Drag node vertically: gain for Bell/Shelf/Notch, cutoff-related placement for High-pass and Low-pass.
- Click node: select band and show floating panel.
- Mouse wheel or modifier-drag on selected node: Q or slope behavior.
- Right-click node: open band action/type menu.
- Delete key: disable selected band.
- Option/Alt-drag: finer movement.
- Band solo: audition the selected band region for tuning.
- Bypass/delete/solo actions are available from the selected-band panel.
- Hover graph: show cursor frequency and nearest significant analyzer peak.
- Masking analyzer mode: show sidechain overlap regions when a sidechain signal is present.

The UI should keep text within controls at supported sizes and avoid overlapping labels, nodes, panel controls, and meters.

## Testing

Behavior tests should cover:

- Parameter layout includes all global parameters and 16 stable band blocks.
- Default state is neutral with all bands disabled.
- Band creation maps graph coordinates to frequency and gain correctly.
- The first inactive band is activated by graph creation.
- Disabled bands do not affect audio.
- Bell, shelf, cut, and notch filters change signal in expected directions.
- Dynamic bands react to threshold and range.
- Attack and release smoothing changes dynamic movement over time rather than instantly.
- External sidechain detector changes dynamic response when sidechain input is present.
- Sidechain fallback is predictable when external sidechain is selected but unavailable.
- Masking analyzer mode shows overlap when main and sidechain spectra compete.
- Analyzer hover peak detection returns a nearby peak and ignores noise-floor bins.
- Phase mode parameter is present, automatable, and defaults to zero latency.
- Bypass and mix remain click-safe and gain-stable.
- Analyzer snapshot publishing does not allocate or block in `processBlock`.

Build verification for the finished implementation:

```bash
make build PLUGIN=prism-eq
make test PLUGIN=prism-eq
make validate PLUGIN=prism-eq
```

## Delivery

The implementation should deliver:

- `plugins/prism-eq/` registered through `plugins/prism-eq/plugin.cmake`.
- Shared reusable Kratomix UI code under `core/ui` or an equivalent shared core location.
- Velvet Channel updated to consume shared UI only for components whose visual behavior remains unchanged.
- Prism-specific DSP, UI, parameters, and tests in the Prism plugin tree.
- Documentation updates that list Prism EQ as a Kratomix plugin.

Generated build artifacts, plugin binaries, Xcode derived files, and local machine metadata must not be committed.
