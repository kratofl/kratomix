# Kratomix

Kratomix is a JUCE-based plugin monorepo for Logic Pro-first audio effects on macOS. The root repo now carries the shared CMake/Make tooling, plugin metadata contract, and template generator for the full Kratomix line.

## Kratomix Prism EQ

![Kratomix Prism EQ editor](plugins/prism-eq/Docs/prism-eq-editor.png)

Kratomix Prism EQ is a graph-first dynamic equalizer with live pre/post/sidechain analyzer views, signed dynamic band movement, quality modes, oversampling, and a linear-phase processing path.

## Download & Install

Download the latest release from the [Releases](https://github.com/kratofl/kratomix/releases) page.

Each plugin ships as a separate macOS `.pkg` installer.

**Installation:**
1. Download the `.pkg` for the plugin.
2. Open the installer and follow the prompts.
3. Restart your DAW and run a plugin scan if the host does not rescan automatically.

Installers place plugin bundles in the standard systemwide macOS audio locations:

```text
/Library/Audio/Plug-Ins/Components/
/Library/Audio/Plug-Ins/VST3/
```

Updates use the same installer flow. A newer package replaces the previous plugin bundle at the same systemwide path.

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

Create local installer packages:

```bash
make installer VERSION=1.2.3
make installer VERSION=1.2.3 PLUGIN=velvet-channel
```

Create and publish a GitHub release:

```bash
make release VERSION=1.2.3
make release VERSION=1.2.3 PLUGIN=prism-eq
make release VERSION=1.2.3 PRERELEASE=1
```

`make release` requires a clean git working tree, creates versioned `.pkg` files in `dist/`, creates and pushes the release tag, and publishes the packages with GitHub CLI. If `PLUGIN` is omitted, every registered plugin is released.

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
- `Kratomix Prism EQ`: graph-first dynamic EQ foundation with stable 16-band parameters, neutral processing, and a graph-led editor shell. See [plugins/prism-eq/README.md](/Users/kratofl/Projects/kratomix/plugins/prism-eq/README.md).

## License

MIT. See [LICENSE](LICENSE).

## Notes

- `plugins/<slug>/plugin.cmake` is the single source of truth for product name, bundle ID, and AU codes.
- The root Make layer is a command wrapper over CMake and the generator script, not a second build system.
