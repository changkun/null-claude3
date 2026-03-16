# Round 59: Anomaly Detector

## Goal
Add a real-time anomaly detector that monitors all 16 scalar metrics and automatically flags phase transitions, bifurcations, and rare dynamical events via statistical deviation tracking.

## What was built
- **Anomaly Detector** (toggle `8`): Statistical watchdog, not a grid overlay
- **Three modes**: off → watch → watch+auto-pause → off
- **Sliding window statistics**: 128-frame ring buffer tracking all 16 metrics
- **Z-score computation**: Running mean/variance per metric, real-time deviation scoring
- **Alert system**: Up to 4 simultaneous alerts with metric name, direction (↑/↓), Z-score
- **Red alert bar**: Persistent flash bar on status line when anomalies detected
- **Auto-pause**: Mode 2 halts simulation when any metric exceeds critical threshold (3.5σ)
- **Adjustable threshold**: +/− keys change alert sensitivity (1.0σ–5.0σ, default 2.0σ)
- **Status indicator**: Green when quiet (shows threshold + event count), red when alerting (shows top anomaly)
- **Auto-expiry**: Alerts fade after ~60 frames (~1 second)

## Technical details
- `AD_WINDOW = 128` frame sliding window
- Mean/variance computed from full window each update (not incremental Welford — simpler for ring buffer)
- Z-score per metric: `(current - mean) / σ`
- Alert insertion sorted by Z-score magnitude (most extreme first)
- Records every 2 generations (same cadence as other meta-overlays)
- ~100 lines of new code

## Design decisions
- **Not a grid overlay**: The anomaly detector is purely a status bar / alert system. It doesn't color cells because it monitors global scalar metrics, not spatial patterns. This is the first non-spatial analysis tool.
- **Reuses pp_read_metric()**: No new metric computation — purely exploits the existing 16-metric infrastructure.
- **Three-mode cycle**: Simple watch mode for passive monitoring, auto-pause for catching rare events during long unattended runs.

## Key insight
This is the first **meta-tool** — it doesn't analyze the automaton directly but monitors the other analyzers. It answers "when should I pay attention?" rather than "what is happening?" This turns the 28 existing overlays from passive displays into an active discovery system: run the simulation, let the watchdog flag interesting moments, then switch to the appropriate overlay to understand what happened.

## Explore/exploit balance
Pure exploitation (explore=19%). Zero new analysis math. Wires existing metric infrastructure (pp_read_metric, the 16 scalar metrics, flash system, +/- key chain) into a practical discovery tool. The only novel concept is applying statistical process control to cellular automaton observation.
