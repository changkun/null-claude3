# Life — Cellular Automaton Explorer

A fully interactive, zero-dependency cellular automaton explorer for the terminal.
12,000+ lines of pure C. No libraries. No frameworks. Just ANSI escape codes and `math.h`.

Supports Conway's Game of Life and 9 other Life-like rulesets, 20+ scientific
analysis overlays, time-travel replay, multi-rule zones, wormhole portals,
dual-species ecosystems, genetic rule evolution, and more — all at 60 fps in
your terminal.

## Table of Contents

- [Quick Start](#quick-start)
- [Features at a Glance](#features-at-a-glance)
- [Controls](#controls)
  - [Keyboard](#keyboard)
  - [Mouse](#mouse)
- [Rule Presets](#rule-presets)
  - [Interactive Rule Editor](#interactive-rule-editor)
- [Simulation Features](#simulation-features)
  - [Multi-Rule Zones](#multi-rule-zones)
  - [Emitters & Absorbers](#emitters--absorbers)
  - [Wormhole Portals](#wormhole-portals)
  - [Dual-Species Ecosystem](#dual-species-ecosystem)
  - [Temperature Field](#temperature-field)
  - [Pattern Stamp Tool](#pattern-stamp-tool)
  - [Genetic Rule Explorer](#genetic-rule-explorer)
- [Scientific Analysis Overlays](#scientific-analysis-overlays)
  - [Entropy Heatmap](#entropy-heatmap)
  - [Lyapunov Sensitivity](#lyapunov-sensitivity)
  - [Fourier Spectrum](#fourier-spectrum)
  - [Fractal Dimension](#fractal-dimension)
  - [Frequency Analysis](#frequency-analysis)
  - [Wolfram Class Detector](#wolfram-class-detector)
  - [Information Flow Field](#information-flow-field)
  - [Phase-Space Attractor](#phase-space-attractor)
  - [Causal Light Cone](#causal-light-cone)
  - [Prediction Surprise](#prediction-surprise)
  - [Mutual Information Network](#mutual-information-network)
  - [Composite Complexity Index](#composite-complexity-index)
  - [Topological Feature Map](#topological-feature-map)
  - [Renormalization Group Flow](#renormalization-group-flow)
  - [Kolmogorov Complexity](#kolmogorov-complexity)
  - [Spatial Correlation Length](#spatial-correlation-length)
  - [Entropy Production Rate](#entropy-production-rate)
  - [Wave Mechanics](#wave-mechanics)
  - [Vorticity Detection](#vorticity-detection)
  - [Percolation Analysis](#percolation-analysis)
  - [Cell Probe Inspector](#cell-probe-inspector)
- [Visualization](#visualization)
  - [Heatmap & Ghost Trails](#heatmap--ghost-trails)
  - [Signal Tracer](#signal-tracer)
  - [Pattern Census](#pattern-census)
  - [Population Sparkline](#population-sparkline)
  - [Minimap](#minimap)
  - [Auto-Demo Mode](#auto-demo-mode)
- [Time-Travel Replay](#time-travel-replay)
- [File I/O](#file-io)
  - [Save & Load](#save--load)
  - [RLE Import/Export](#rle-importexport)
  - [Screenshot Capture](#screenshot-capture)
- [Build Options](#build-options)
- [Requirements](#requirements)
- [Architecture](#architecture)
- [Further Reading](#further-reading)
- [License](#license)

---

## Quick Start

```bash
make        # compile
./life      # run
```

Press `SPACE` to play/pause, `r` to randomize, `d` to draw with the mouse,
`h` for heatmap mode. Press `D` for a guided demo tour. `q` to quit.

Load an RLE pattern file:

```bash
./life pattern.rle
```

## Features at a Glance

| Category | Features |
|----------|----------|
| **Simulation** | 10 rule presets, live mutation, multi-rule zones, emitters/absorbers, wormhole portals, dual-species ecosystems, temperature field |
| **Analysis** | Shannon entropy, Lyapunov exponents, 2D FFT, fractal dimension, frequency detection, Wolfram classification, transfer entropy, phase-space attractors, causal light cones, prediction surprise, mutual information, composite complexity, topological features, renormalization group, Kolmogorov complexity, spatial correlation length, anomaly detector |
| **Interaction** | Mouse drawing, zoom/pan, kaleidoscope symmetry, pattern stamps, genetic rule search, time-travel replay |
| **Visualization** | 24-bit true-color heatmaps, ghost trails, signal tracer, population sparkline, minimap, auto-demo mode |
| **I/O** | Binary save/load, RLE import/export, PPM screenshots, timeline animation export |

## Controls

### Keyboard

#### Core

| Key | Action |
|-----|--------|
| `SPACE` / `p` | Play / Pause |
| `s` | Step one generation (while paused) |
| `r` | Randomize grid |
| `c` | Clear grid (press twice to also clear zones/portals) |
| `+` / `-` | Speed up / slow down (20ms–1000ms) |
| `q` / `ESC` | Quit |

#### Drawing & Placement

| Key | Action |
|-----|--------|
| `d` | Toggle draw mode (mouse painting) |
| `k` | Cycle symmetry: none → 2-fold → 4-fold → 8-fold |
| `S` | Toggle stamp mode (place classic patterns) |
| `e` | Cycle emitter/absorber placement mode |
| `W` | Toggle wormhole portal placement |

#### Navigation

| Key | Action |
|-----|--------|
| `z` / `x` | Zoom in / out (1× → 2× → 4×) |
| Arrow keys | Pan viewport |
| `0` | Re-center viewport |
| `n` | Toggle minimap overlay |

#### Rules & Simulation

| Key | Action |
|-----|--------|
| `[` / `]` | Cycle rule presets (or zone/stamp/emitter selection) |
| `m` | Mutate — randomly flip one B/S bit |
| `b` | Toggle rule editor overlay |
| `j` | Toggle zone-paint mode |
| `w` | Cycle topology: flat → torus → Klein bottle → Möbius → projective |
| `a` | Toggle dual-species ecosystem mode |
| `6` | Switch brush species (A ↔ B) |
| `{` / `}` | Adjust cross-species interaction (-1.0 to +1.0) |
| `X` | Toggle temperature field |

#### Visualization

| Key | Action |
|-----|--------|
| `h` | Toggle heatmap (cell-age coloring + ghost trails) |
| `g` | Toggle population sparkline |
| `y` | Toggle population dynamics dashboard |
| `t` | Toggle timeline scrubber bar |
| `T` | Cycle signal tracer: off → accumulate → frozen → clear |
| `v` | Toggle pattern census overlay |
| `D` | Auto-demo mode (guided tour) |

#### Analysis Overlays

| Key | Action |
|-----|--------|
| `i` | Entropy heatmap (local Shannon entropy) |
| `L` | Lyapunov sensitivity (perturbation divergence) |
| `u` | 2D Fourier spectrum |
| `f` | Frequency analysis (oscillation period detection) |
| `F` | Fractal dimension (box-counting) |
| `C` | Wolfram class detector (I/II/III/IV) |
| `G` | Genetic rule explorer |
| `O` | Information flow (transfer entropy vectors) |
| `A` | Phase-space attractor (Takens embedding) |
| `9` | Causal light cone |
| `!` | Prediction surprise (per-cell surprisal) |
| `@` | Mutual information network |
| `#` | Composite complexity index |
| `$` | Topological feature map (β₀ components, β₁ holes) |
| `%` | Renormalization group flow |
| `^` | Kolmogorov complexity (LZ77 compression) |
| `&` | Spatial correlation length (two-point correlation ξ) |
| `?` | Cell probe inspector (all metrics for clicked cell) |
| `8` | Anomaly detector (statistical watchdog for all 16 metrics) |

#### File I/O

| Key | Action |
|-----|--------|
| `P` | Screenshot (PPM) |
| `Ctrl-P` | Dump full 256-frame timeline as image sequence |
| `Ctrl-S` | Save state to `.life` file |
| `Ctrl-O` | Load most recent save |
| `Ctrl-E` | Export grid as RLE file |

#### Replay

| Key | Action |
|-----|--------|
| `<` / `,` | Rewind (enters replay mode) |
| `>` / `.` | Fast-forward |
| `SPACE` | Resume live simulation from current point (branch) |

### Mouse

| Action | Effect |
|--------|--------|
| Left-click / drag | Place cells (or stamps, emitters, portals) |
| Right-click / drag | Erase cells (or cancel placement) |
| Scroll wheel | Zoom, rotate stamps, adjust portal/absorber radius |

## Rule Presets

| Ruleset | Rule | Character |
|---------|------|-----------|
| Conway | B3/S23 | The classic — gliders, oscillators, still lifes |
| HighLife | B36/S23 | Famous for its replicator pattern |
| Day & Night | B3678/S34678 | Symmetric — patterns work inverted too |
| Seeds | B2/S | Explosive — every cell dies, but births cascade |
| Diamoeba | B35678/S5678 | Organic amoeba-like blobs |
| Morley | B368/S245 | Mix of stable structures and chaos |
| 2x2 | B36/S125 | Block-splitting dynamics |
| Maze | B3/S12345 | Grows maze-like corridors |
| Coral | B3/S45678 | Slow coral-reef growth |
| Anneal | B4678/S35678 | Annealing — noisy regions smooth into blobs |

Press `m` to mutate any ruleset — randomly flips one birth or survival bit.
The status bar shows `(mutant)` when the active rule doesn't match any preset.

### Interactive Rule Editor

Press `b` to open the rule editor overlay — click individual B/S bits to
toggle them, or click any preset name to load it instantly. The simulation
keeps running so you see each change in real time.

## Simulation Features

### Multi-Rule Zones

Press `j` to enter zone-paint mode. Paint different rulesets onto regions of
the grid. Each cell uses its zone's B/S rules during simulation, creating
emergent behavior at zone boundaries.

- `[`/`]` cycles the zone brush through all 10 rulesets
- `1`–`5` loads preset zone layouts (split, quadrants, rings, stripes, checkerboard)
- Works with kaleidoscope symmetry for mandala-like zone patterns

### Emitters & Absorbers

Press `e` to cycle through placement modes. Emitters are point sources that
spawn cells at configurable intervals (dot, cross, random, glider patterns).
Absorbers are circular kill zones that remove all cells within radius.

Place emitters on one side and absorbers on the other to create rivers of cells
through multi-rule zones.

### Wormhole Portals

Press `W` to enter portal placement mode. Left-click to place entrance (cyan),
click again for exit (magenta). Up to 8 portal pairs. Cells near portals see
extra neighbors from the paired endpoint, enabling:

- Glider tunneling between distant regions
- Cross-zone rule bleeding
- Oscillation disruption from non-local influence
- Emergent boundary effects at portal perimeters

### Dual-Species Ecosystem

Press `a` to toggle ecosystem mode — two populations (Red/Blue) coexist with
independent B/S rules and tunable inter-species interaction.

| Interaction | Behavior |
|-------------|----------|
| -1.0 | Hostile — cross-species neighbors count negatively |
| 0.0 | Neutral — species ignore each other |
| +1.0 | Cooperative — full mutualism |

Adjust with `{`/`}`. Press `6` to switch brush species.

### Temperature Field

Press `X` to toggle stochastic noise. Paint hot/cold regions with the mouse.
Cells in hot regions have a probability of spontaneous birth/death, enabling
phase-transition experiments.

### Pattern Stamp Tool

Press `S` to enter stamp mode. Library of 20 classic patterns:

| Category | Patterns |
|----------|----------|
| Still lifes | Block, Beehive, Loaf, Boat, Tub |
| Oscillators | Blinker, Toad, Beacon, Pentadecathlon, Clock |
| Spaceships | Glider, LWSS, MWSS, HWSS |
| Methuselahs | R-pentomino, Diehard, Acorn, Pi-heptomino |
| Other | Gosper Gun, Pulsar |

`[`/`]` to cycle patterns, scroll to rotate, left-click to place.

### Genetic Rule Explorer

Press `G` to evolve interesting rulesets via genetic algorithm. 20 candidate
rules per generation are scored for "interestingness" (population stability,
oscillation, complexity). Press `G` to breed the next generation, `1`–`5` to
load a discovered rule.

## Scientific Analysis Overlays

The explorer includes 15+ information-theoretic, dynamical systems, topological,
and algorithmic analysis overlays. Each recomputes at configurable intervals
(4–16 generations) to maintain 60 fps rendering.

### Entropy Heatmap

Toggle: `i`. Shannon entropy of each cell's 3×3 Moore neighborhood. Blue (low
entropy, uniform) through red/white (high entropy, disordered). Reveals the
boundary between order and chaos at cellular resolution.

### Lyapunov Sensitivity

Toggle: (via entropy overlay). Measures perturbation growth by maintaining a
shadow grid with a single flipped bit and tracking divergence over time.
Positive Lyapunov exponent = chaos; zero = edge; negative = stability.

### Fourier Spectrum

Toggle: `u`. 2D FFT of grid density, showing radial power distribution across
spatial frequencies. Reveals periodic spatial structures invisible in real space.

### Fractal Dimension

Toggle: `F`. Box-counting method at scales 2–128, computing D_box via
log-log regression. D_box ≈ 1.0 for lines, ≈ 1.5 for coastlines, ≈ 2.0 for
space-filling patterns. Includes R² goodness-of-fit.

### Frequency Analysis

Toggle: `f`. Per-cell oscillation period detection via autocorrelation over
the last 64 frames.

| Color | Period |
|-------|--------|
| Ice blue | Still life (never changes) |
| Emerald | Period 2 (blinkers, toads) |
| Gold | Period 3 (pulsars) |
| Orange–coral | Period 4–12 |
| Magenta | Period 13–32 |
| Hot red | Chaotic (no clear period) |

### Wolfram Class Detector

Toggle: `C`. Automatically classifies the automaton's behavior:

- **Class I** — death (population → 0)
- **Class II** — periodic (stable oscillation)
- **Class III** — chaotic (aperiodic, high entropy)
- **Class IV** — complex (edge of chaos, long transients)

### Information Flow Field

Toggle: `O`. Transfer entropy between each cell and its cardinal neighbors,
producing directional causal influence vectors. Arrow glyphs show net
information flow direction, color-coded by angle.

- **Sources** — regions exporting information (glider launchers, emitters)
- **Sinks** — regions absorbing information (absorbers, colliders)
- **Vorticity** — rotational flow patterns

### Phase-Space Attractor

Toggle: `A`. Reconstructs the dynamical attractor from population time series
via Takens delay embedding: `(pop(t), pop(t+τ))`.

| Shape | Dynamics |
|-------|----------|
| Single dot | Fixed point (dead or stable) |
| Closed loop | Periodic orbit |
| Thick ring | Quasi-periodic |
| Space-filling cloud | Strange attractor / chaos |

Includes correlation dimension D₂ via Grassberger-Procaccia algorithm.

### Causal Light Cone

Toggle: `9`. Click a cell to trace its backward (past dependencies) and
forward (future influence) light cones. `+`/`-` adjust cone depth.

### Prediction Surprise

Toggle: `!`. Accumulates transition statistics over 32 frames, then colors
each cell by surprisal: `-log₂ P(outcome | neighborhood)`.

- **Dark blue** — perfectly predictable (still lifes, oscillators)
- **Red/white** — high surprise (glider edges, phase boundaries)

### Mutual Information Network

Toggle: `@`. Partitions the grid into 20×10 blocks, computes MI between all
block-pair population time series over 256 frames. Top-40 strongest couplings
rendered as colored lines. Reveals long-range synchronization invisible to
local analyzers.

### Composite Complexity Index

Toggle: `#`. Fuses entropy, Lyapunov, surprise, and frequency into a per-cell
edge-of-chaos score (0.0–1.0) with concave edge-boost:

```
raw = 0.30·entropy + 0.25·lyapunov + 0.25·surprise + 0.20·frequency
score = 0.7·(4·raw·(1-raw)) + 0.3·raw
```

Gold band at 0.4–0.7 marks where gliders and complex structures live.

### Topological Feature Map

Toggle: `$`. Connected component labeling and enclosed hole detection via
4-connected flood fill.

- **β₀** — number of distinct connected components (each uniquely colored)
- **β₁** — number of enclosed holes (highlighted in magenta)

Reveals percolation transitions and fragmentation dynamics.

### Renormalization Group Flow

Toggle: `%`. Real-space RG via majority-rule block decimation at 2×, 4×, 8×
scales.

| Color | Meaning |
|-------|---------|
| Cyan | Fine-dominated (dust, noise) |
| Yellow | Meso-dominated (clusters) |
| Magenta | Coarse-dominated (spanning patterns) |
| White | Scale-invariant (critical behavior) |

Criticality score near 1.0 = scale-invariant = edge of chaos.

### Kolmogorov Complexity

Toggle: `^`. LZ77-style compression of local neighborhoods. Blue =
compressible (ordered), gold = structured, red = incompressible (random).

### Spatial Correlation Length

Toggle: `&`. Two-point correlation function C(r) reveals the characteristic
spatial scale ξ (xi) of order. Short ξ = disordered (random), long ξ =
near-critical (large-scale structure). Per-cell heatmap: violet = uncorrelated,
cyan = moderate, white = strongly correlated. Panel shows ξ, C(r) sparkline,
and phase classification.

### Entropy Production Rate

Toggle: `=`. Local time-derivative of Shannon entropy (dS/dt). Blue = entropy
decreasing (self-organization), gray = equilibrium, red = entropy increasing
(dissolution). EMA-smoothed. Stats panel with global dS/dt, ordering/disordering
fractions, thermodynamic phase classification.

### Wave Mechanics

Toggle: `~`. Treats cell births/deaths as impulse sources propagating via a
damped 2D wave equation. Cyan = positive amplitude, dark = zero, orange =
negative. Reveals standing waves near oscillators, expanding wavefronts at
chaos boundaries, and interference patterns between active regions.

### Vorticity Detection

Toggle: `*`. Computes the discrete curl of a velocity field estimated from
temporal density changes. Blue = clockwise rotation, dark = irrotational,
red = counterclockwise. Detects vortex centers as local maxima of |ω|. Stats
panel with max/mean vorticity, net circulation (Γ), vortex count, and sparkline.

### Percolation Analysis

Toggle: `|`. Flood-fill cluster connectivity analysis. Each connected cluster of
live cells gets a unique color; the largest cluster is highlighted in gold. Detects
spanning clusters — whether the largest cluster connects opposite edges (horizontal
and/or vertical). Displays the percolation order parameter P∞ (fraction of cells in
the largest cluster), site density relative to the critical threshold p_c ≈ 0.593
for square lattice percolation, and phase classification (Supercritical when
spanning, Near-critical, Subcritical, or Dilute). Sparkline tracks P∞ over time.

### Cell Probe Inspector

Toggle: `?`. Click any cell to see all 18+ metrics at once in a unified panel:
entropy, temperature, Lyapunov, Fourier, fractal, surprisal, mutual info,
Kolmogorov, correlation, complexity, frequency, flow, RG, topology, entropy
production, wave amplitude, vorticity.

## Visualization

### Heatmap & Ghost Trails

Press `h` to toggle. Cells are colored by age using a 24-bit thermal gradient
(blue → cyan → green → yellow → red → white). Ghost trails mark where cells
recently died with a 5-frame gray-blue fade.

### Signal Tracer

Press `T` to cycle: off → accumulate → frozen → clear. Alive cells build up
trail intensity over time using a purple → magenta → pink palette. Glider
streams become visible rivers, emitter outputs become flow fields.

### Pattern Census

Press `v`. Real-time structure recognition via bitmask matching with
dead-border verification:

| Category | Patterns |
|----------|----------|
| Still lifes (green) | Block, Beehive, Loaf, Boat, Tub, Ship, Barge, Long Boat |
| Oscillators (amber) | Blinker, Toad, Beacon (both phases each) |

Updates every 16 generations.

### Population Sparkline

Press `g`. Unicode block element graph (▁▂▃▅▇█) with density-based coloring
(red → yellow → green). Shows population trend over recent history.

### Minimap

Press `n` or zoom in. Quarter-block thumbnail of the full 400×200 grid with
a yellow viewport rectangle showing your current view.

### Auto-Demo Mode

Press `D`. Guided tour through 10 curated scenes showcasing pattern + overlay
combinations. Press any key to exit and explore on your own.

## Time-Travel Replay

The simulation records every generation into a 256-frame ring buffer.

| Key | Action |
|-----|--------|
| `<` / `,` | Rewind (enters replay mode) |
| `>` / `.` | Fast-forward |
| `s` | Step forward one frame |
| `SPACE` | Resume live from current point (branch) |
| `t` | Toggle timeline scrubber bar |

The status bar shows `⏪ REPLAY` when browsing history. Press `SPACE` at any
point to branch — restore that state and resume live simulation from there.

## File I/O

### Save & Load

`Ctrl-S` saves full state to auto-numbered `.life` files (`save_001.life`, etc.).
`Ctrl-O` loads the most recent save. Everything is preserved: grid, ages,
zones, emitters, absorbers, portals, ruleset, symmetry, wrapping.

### RLE Import/Export

`Ctrl-E` exports the current grid as RLE (Run Length Encoded), the standard
Game of Life pattern exchange format. Pass an `.rle` file as a command-line
argument to import.

### Screenshot Capture

| Key | Output |
|-----|--------|
| `P` | Single frame → `frame_0001.ppm` |
| `Ctrl-P` | Full 256-frame timeline → `frame_0001.ppm` through `frame_0256.ppm` |

Files use binary PPM (P6) format. Convert with:

```bash
# Animated GIF
convert -delay 5 frame_*.ppm animation.gif

# Video
ffmpeg -framerate 30 -i frame_%04d.ppm output.mp4
```

## Build Options

```bash
make                # Optimised release build (-O2)
make debug          # Debug build (-g -O0, no optimisation)
make sanitize       # AddressSanitizer + UBSan build
make install        # Install to /usr/local/bin
make uninstall      # Remove from /usr/local/bin
make clean          # Remove build artifacts
make help           # Show all targets and variables
```

Override compiler or install prefix:

```bash
make CC=clang
make install PREFIX=/opt
```

## Requirements

- C99 compiler (gcc, clang)
- POSIX terminal with 24-bit color support (most modern terminals)
- `math.h` / `-lm` (the only library dependency)

No other dependencies. No ncurses. No SDL. No external libraries.

## Architecture

The entire application is a single C file (`life.c`, ~10,800 lines) organized
into modular subsystems separated by ASCII section headers:

| Subsystem | Responsibility |
|-----------|---------------|
| Grid & state | 400×200 double-buffered cell array, ghost trails, signal tracer |
| Rule engine | B/S notation, 10 presets, live mutation, multi-rule zones |
| Simulation | Neighbor counting, state update, emitters, absorbers, portals |
| Analysis | 15 scientific overlay compute functions |
| Rendering | ANSI escape output buffer, heatmaps, Unicode blocks, UI panels |
| Input | Raw terminal mode, keyboard dispatch, SGR mouse protocol |
| File I/O | Binary `.life` format, RLE import/export, PPM screenshots |
| Timeline | 256-frame ring buffer, replay, branching |

Design choices:

- **Zero allocation** — all state in static arrays, no `malloc`
- **Double-buffered rendering** — pre-built output buffer, single `write()` per frame
- **Lazy recomputation** — `stale` flags prevent redundant overlay computation
- **Single file** — no build complexity, trivial to compile and distribute

## Further Reading

See **[GUIDE.md](GUIDE.md)** for the theoretical background behind each
analysis mode — what it measures, why it matters, how it works, and academic
sources with validated DOI links.

## License

[MIT](LICENSE) — Changkun Ou
