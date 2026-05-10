# Kratomix Prism EQ

Kratomix Prism EQ is a graph-first dynamic equalizer in the Kratomix line. This first foundation slice registers the plugin, establishes stable v1 parameter IDs for 16 bands, provides neutral gain/mix/bypass processing, and introduces the graph-led editor shell.

## Foundation Controls

- **Input**: level into the Prism processing path.
- **Output**: final output trim.
- **Mix**: wet/dry blend for the processing path.
- **Bypass**: soft bypass target.
- **Analyzer Mode, Speed, Range**: stable parameters for the upcoming analyzer view.
- **Gain Scale**: stable parameter for graph response scaling.
- **Bands 01-16**: pre-registered dynamic EQ parameter blocks.

## Build

From the monorepo root:

```bash
make build PLUGIN=prism-eq
make test PLUGIN=prism-eq
make run PLUGIN=prism-eq
make validate PLUGIN=prism-eq
```
