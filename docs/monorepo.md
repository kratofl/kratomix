# Kratomix Monorepo

## Structure

- `core/cmake/` holds shared JUCE bootstrap code and the plugin registration helpers.
- `plugins/<slug>/plugin.cmake` is the metadata contract for each plugin.
- `plugins/<slug>/Source/` and `plugins/<slug>/Tests/` stay plugin-specific.
- `templates/effect-plugin/` is the neutral effect template used by the generator.
- `tools/new-plugin.py` creates new plugin folders directly under `plugins/`.

## Make Layer

The root `Makefile` is the public command surface:

```bash
make help
make list
make build [PLUGIN=<slug>|all]
make test [PLUGIN=<slug>|all]
make run PLUGIN=<slug>
make validate PLUGIN=<slug>
make package PLUGIN=<slug>
make release PLUGIN=<slug>
make new-plugin SLUG=<slug> NAME="Kratomix ..." CODE=<FourCC>
```

The defaults are tuned for this machine:

- Generator: `Unix Makefiles`
- Build dir: `build/`
- Build type: `Release`
- Parallel jobs: `4`

## Plugin Metadata Contract

Each `plugin.cmake` defines:

- `KRATOMIX_PLUGIN_SLUG`
- `KRATOMIX_CMAKE_TARGET`
- `KRATOMIX_PRODUCT_NAME`
- `KRATOMIX_BUNDLE_ID`
- `KRATOMIX_PLUGIN_MANUFACTURER_CODE`
- `KRATOMIX_PLUGIN_CODE`
- `KRATOMIX_AU_MAIN_TYPE`
- `KRATOMIX_PLUGIN_FORMATS`
- `KRATOMIX_PLUGIN_SOURCES`
- `KRATOMIX_PLUGIN_TEST_SOURCES`
- `KRATOMIX_PLUGIN_INCLUDE_DIRS`

The root CMake layer uses those values to register the JUCE plugin targets and normalize them behind:

- `plugin-<slug>`
- `plugin-<slug>-au`
- `plugin-<slug>-standalone`
- `test-<slug>`

`core/cmake/print_plugin_metadata.cmake` reads the same file so `make validate` and `make run` do not duplicate AU or product metadata.

## Release Packages

Use `make package PLUGIN=<slug>` to build one plugin and create `dist/<slug>-Release.zip`.
The zip stages only the exact current product-name artifacts declared by `plugins/<slug>/plugin.cmake`, so stale build outputs with old names are ignored.

Use `make release PLUGIN=<slug>` when preparing a GitHub release artifact. It builds the plugin, validates the AU when the plugin provides one, and then writes the zip.
