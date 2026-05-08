# AGENTS.md

## Project

This repository contains the **Kratomix** plugin line. It is a JUCE-based monorepo for macOS audio effect plugins targeting Logic Pro first, with shared build tooling and plugin templates under the `Kratomix Core` layer.

Keep all user-facing names, comments, docs, bundle identifiers, and UI text under the Kratomix brand. Do not reference third-party hardware, plugin, or manufacturer names as product identity. If a classic studio hardware sound is used as inspiration, describe it generically as warm channel color, harmonic saturation, transformer-style tone, musical EQ, or analog-style gain staging.

## Repository Layout

- `CMakeLists.txt`: monorepo root that discovers `plugins/*/plugin.cmake`.
- `Makefile` and `mk/*.mk`: shared command layer for configure/build/test/run/validate/scaffold.
- `core/cmake/`: shared JUCE/CMake helpers and plugin metadata tooling.
- `plugins/<slug>/plugin.cmake`: the single source of truth for plugin metadata and target wiring.
- `plugins/<slug>/Source/`: plugin processor, DSP, UI, and parameter code.
- `plugins/<slug>/Tests/`: plugin-specific behavior tests.
- `templates/effect-plugin/`: the neutral template for `make new-plugin`.
- `tools/new-plugin.py`: generator used by `make new-plugin`.
- `docs/superpowers/specs/`: design notes.
- `docs/superpowers/plans/`: implementation plans.

## Engineering Rules

- Keep shared build logic in `core/cmake/`, not inside individual plugin trees.
- Keep plugin-specific DSP and UI behavior inside the owning plugin directory unless it is intentionally being generalized.
- Keep DSP logic out of `PluginEditor.*`.
- Keep host and preset logic out of dedicated DSP processors.
- Register new plugins through `plugins/<slug>/plugin.cmake`; do not add plugin-local Makefiles.
- Use `make new-plugin` for new plugin scaffolds instead of hand-rolling directory layouts.
- Preserve stable parameter IDs once released, because DAW automation and saved sessions depend on them.
- Prefer small, focused files over large mixed-responsibility files.
- Avoid adding dependencies unless they are necessary for audio behavior, build stability, or plugin distribution.
- Use ASCII in source and docs unless an existing file clearly requires otherwise.
- Do not commit generated build artifacts, plugin binaries, Xcode derived files, or local machine metadata.

## Audio Rules

- Prioritize musical defaults over extreme ranges.
- Avoid clipping-prone output changes without gain compensation or clear output control.
- Keep denormal protection in realtime audio paths.
- Do not allocate memory, lock mutexes, or perform file/network operations inside `processBlock`.
- Add smoothing before introducing parameters that can produce zipper noise.
- If adding oversampling, latency, or lookahead, report latency correctly to the host.

## UI Rules

- Keep the first UI compact and channel-strip oriented.
- Expose controls that map directly to audible behavior.
- Avoid marketing copy inside the plugin UI.
- Use restrained Kratomix styling and readable labels.

## Build Commands

Standard workflow from the monorepo root:

```bash
make build
make test
```

Build or test a single plugin:

```bash
make build PLUGIN=velvet-channel
make test PLUGIN=velvet-channel
```

Scaffold a new plugin:

```bash
make new-plugin SLUG=tape-bloom NAME="Kratomix Tape Bloom" CODE=TBlo
```

Validate an Audio Unit:

```bash
make validate PLUGIN=velvet-channel
```

## Git

Before finishing a change, run:

```bash
git status --short
```

Only commit when explicitly asked. Use concise commit messages, for example:

```bash
git commit -m "feat: add monorepo make layer"
```
