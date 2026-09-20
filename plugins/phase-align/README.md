# Kratomix Phase Align

Kratomix Phase Align synchronisiert zwei Aufnahmen oder parallele Signalpfade, die aus derselben Quelle stammen. Das Plugin verschiebt den bearbeiteten Pfad um bis zu 10 ms in beide Richtungen und kann eine umgekehrte Polaritaet korrigieren.

## Logic workflow

1. Setze Kratomix Phase Align auf den Pfad, der verschoben werden soll.
2. Waehle den Vergleichspfad in Logics Side-Chain-Menue.
3. Spiele eine Stelle ab, in der beide Pfade dasselbe Quellsignal enthalten.
4. Druecke `Auto Align` und lasse die Aufnahme kurz weiterlaufen.
5. Hoere `Aligned` und `Difference` ab. Nutze die Sample-Tasten fuer kleine musikalische Abweichungen vom Messergebnis.

Raumanteil, Busbearbeitung und Effekte nach der Aufteilung sollten waehrend der Messung nicht im Sidechain-Signal liegen.

## Controls

- `Auto Align` nimmt beide Pfade einmalig auf und berechnet Delay und Polaritaet ausserhalb des Audiothreads.
- `Signed Offset` zeigt die Korrektur in Samples und Millisekunden. Der Slider deckt -10 bis +10 ms ab.
- `-1`, `-0.1`, `+0.1` und `+1 sample` erlauben grobe und feine Korrekturen.
- `Invert Polarity` dreht die Polaritaet des bearbeiteten Pfads.
- `Original`, `Aligned` und `Difference` schalten zwischen dem neutral zeitkompensierten Pfad, dem Ergebnis und dem Differenzsignal um.
- `Lock Result` wird nach einer erfolgreichen Messung gesetzt. Ein neuer Auto-Align-Lauf loest den Lock automatisch.
- `Correlation` zeigt die laufende Beziehung zwischen dem bearbeiteten Pfad und dem Sidechain-Signal.

Das Plugin meldet eine feste Latenz an den Host. Dadurch funktionieren auch negative Offsets, ohne dass sich Logics Delay-Kompensation beim Einstellen des Sliders aendert.

## Build

```bash
make build PLUGIN=phase-align
make test PLUGIN=phase-align
make run PLUGIN=phase-align
make validate PLUGIN=phase-align
```
