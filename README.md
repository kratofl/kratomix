# Kratomix

Kratomix is a JUCE-based plugin monorepo for Logic Pro-first audio effects on macOS. The root repo now carries the shared CMake/Make tooling, plugin metadata contract, and template generator for the full Kratomix line.

## Layout

- `core/cmake/`: shared JUCE and plugin registration helpers.
- `plugins/velvet-channel/`: the current production plugin.
- `templates/effect-plugin/`: neutral starter template for future plugins.
- `tools/new-plugin.py`: generator behind `make new-plugin`.
- `mk/*.mk`: root GNU Make command layer.

## Commands

Requirements:

- macOS with Xcode command line tools.
- CMake 3.22 or newer.
- GNU Make.
- JUCE 8, either fetched by CMake or supplied through `JUCE_DIR`.

List registered plugins:

```bash
make list
```

Configure and build everything with the default `Unix Makefiles` generator:

```bash
make build
make test
```

Build, test, run, or validate a single plugin:

```bash
make build PLUGIN=velvet-channel
make test PLUGIN=velvet-channel
make run PLUGIN=velvet-channel
make validate PLUGIN=velvet-channel
```

Use a local JUCE checkout:

```bash
make build JUCE_DIR=/path/to/JUCE
```

Scaffold a new plugin from the shared template:

```bash
make new-plugin SLUG=tape-bloom NAME="Kratomix Tape Bloom" CODE=TBlo
```

## Plugins

- `Kratomix Velvet Channel`: warm source-track strip with stepped EQ color, harmonic drive, hardware-style bypass, and a central VU meter. See [plugins/velvet-channel/README.md](/Users/kratofl/Projects/kratomix/plugins/velvet-channel/README.md).

## Notes

- `plugins/<slug>/plugin.cmake` is the single source of truth for product name, bundle ID, and AU codes.
- The root Make layer is a command wrapper over CMake and the generator script, not a second build system.
