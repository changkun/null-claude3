# Life-like Cellular Automaton Explorer — Terminal Edition

A fully interactive, zero-dependency cellular automaton explorer that runs in any terminal.
Written in C with ANSI escape codes for rendering. Supports Conway's Game of Life
and 9 other Life-like rulesets, live rule mutation, and kaleidoscope drawing.

![Terminal simulation](https://upload.wikimedia.org/wikipedia/commons/e/e5/Gospers_glider_gun.gif)

## Build & Run

```
make && ./life
```

Requires only `gcc` and a POSIX terminal.

## Controls

| Key | Action |
|-----|--------|
| `SPACE` / `p` | Play / Pause |
| `s` | Step one generation (while paused) |
| `r` | Randomize grid |
| `c` | Clear grid |
| `w` | Toggle toroidal wrapping (∞ indicator) |
| `d` | Toggle draw mode (mouse painting) |
| `g` | Toggle population sparkline graph |
| `h` | Toggle heatmap mode (age coloring + ghost trails) |
| `[` / `]` | Cycle through rule presets (or zone brush in zone mode) |
| `m` | Mutate — randomly flip one birth/survival bit |
| `b` | Toggle rule editor overlay (click B/S bits or preset names) |
| `j` | Toggle zone-paint mode (paint regions with different rulesets) |
| `k` | Cycle symmetry: none → 2-fold → 4-fold → 8-fold (kaleidoscope) |
| `z` / `x` | Zoom in / out (1x → 2x half-block → 4x quarter-block) |
| `n` | Toggle minimap overlay (full-grid thumbnail + viewport rect) |
| `e` | Cycle emitter/absorber mode (off → emitter → absorber) |
| `[` / `]` | Cycle emitter pattern or absorber radius |
| `<` / `,` | Rewind through history (enters replay mode) |
| `>` / `.` | Fast-forward through history |
| `t` | Toggle timeline scrubber bar |
| `T` | Cycle signal tracer: off → accumulate → frozen → clear+off |
| `f` | Toggle frequency analysis overlay (period detection heatmap) |
| `W` | Toggle wormhole portal placement mode |
| `Ctrl-S` | Save state to numbered `.life` file |
| `Ctrl-O` | Load most recent `.life` save |
| `Arrow keys` | Pan viewport across the full 400×200 grid |
| `0` | Re-center viewport |
| `1`–`5` | Load preset pattern |
| `+` / `-` | Speed up / slow down (20ms–1000ms) |
| `q` / `ESC` | Quit |

### Mouse (draw mode)

| Action | Effect |
|--------|--------|
| Left-click / drag | Place cells |
| Right-click / drag | Erase cells |
| Scroll wheel | Zoom in / out |

## Preset Patterns

1. **Glider** — the classic spaceship
2. **Pulsar** — period-3 oscillator
3. **Gosper Glider Gun** — infinite growth, streams gliders
4. **R-pentomino** — chaotic methuselah (stabilizes after 1103 generations)
5. **Acorn** — another methuselah (stabilizes after 5206 generations)

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

Press `m` to mutate any ruleset — randomly flips one birth or survival bit,
creating hybrid rules that may exhibit entirely novel behavior. The status bar
shows `(mutant)` when the active rule doesn't match any preset.

### Interactive Rule Editor

Press `b` to open the **rule editor overlay** — a visual panel where you can click
individual birth and survival bits to toggle them on/off. Click any preset name
to load it instantly. The simulation continues running underneath, so you see
the effect of each change in real time.

- **Birth row**: Click digits 0–8 to toggle which neighbor counts cause cell birth
- **Survive row**: Click digits 0–8 to toggle which neighbor counts let cells survive
- **Presets**: Click any of the 10 preset names to load that ruleset
- Active bits are highlighted (green for birth, blue for survival)
- Combined with zone-paint mode, design exact rules then paint them into regions

## Multi-Rule Zones

Press `j` to enter **zone-paint mode**, where mouse painting assigns different rulesets
to regions of the grid. Each cell remembers its zone, and during simulation, cells use
their zone's birth/survival rules — creating emergent behavior at zone boundaries.

- **Left-click/drag** paints the current zone brush (shown in status bar)
- **Right-click/drag** resets zones back to the default (Conway)
- **`[`/`]`** cycles the zone brush through all 10 rulesets
- **`1`–`5`** loads preset zone layouts (split, quadrants, rings, stripes, checkerboard)
- Symmetry modes work with zone painting for mandala-like zone patterns
- Press `c` twice (once to clear cells, once to clear zones) to fully reset
- Zone tinting is visible when zone mode is active (color-coded backgrounds)

Try painting Conway on the left and Maze on the right, then randomize — watch
gliders from Conway crash into maze corridors! Or use preset `3` (concentric rings)
for a psychedelic multi-rule target pattern.

## Emitters & Absorbers

Press `e` to cycle through **emitter** and **absorber** placement modes. These transform
the automaton from a closed system into an open dissipative one with steady-state flows.

- **Emitters** — point sources that spawn cells at configurable intervals. Four patterns:
  dot, cross, random 3×3, and glider (auto-varies direction by position).
- **Absorbers** — circular kill zones that continuously remove all cells within radius.
  Scroll wheel adjusts radius (1–10 cells).

Place emitters on one side and absorbers on the other to create rivers of cells.
Combine with multi-rule zones for complex flow interactions at rule boundaries.

## Time-Travel Replay

The simulation automatically records every generation into a 256-frame history ring buffer.

| Key | Action |
|-----|--------|
| `<` / `,` | Rewind (enters replay mode) |
| `>` / `.` | Fast-forward |
| `s` | Step forward one frame in replay |
| `SPACE` | Resume live simulation from current point (**branch**) |
| `t` | Toggle timeline scrubber bar |

The status bar shows `⏪ REPLAY` when browsing history. The timeline scrubber displays a
playhead (`●`) showing your position. Pressing SPACE at any point **branches** — it restores
that historical state, truncates the future, and resumes live simulation from there. This
lets you rewind to a critical moment, change parameters, and compare outcomes.

## Signal Tracer

Press `T` to cycle through tracer modes:

1. **Off** (default) — no trail rendering
2. **Accumulate** — alive cells build up trail intensity each generation. Dead cells display
   a purple-to-pink gradient showing where structures have traveled. Glider streams become
   visible rivers, emitter outputs become flow fields, and zone boundaries show collision patterns.
3. **Frozen** — trails remain visible but stop accumulating, letting you observe existing
   trails while the simulation continues evolving underneath
4. Press `T` again to clear all trails and return to off

The tracer uses a distinct purple→magenta→pink palette so it doesn't conflict with the
thermal heatmap (blue→red→white). Combine with 8-fold symmetry and emitters for stunning
mandala-like trail art.

## Frequency Analysis Overlay

Press `f` to toggle the **frequency analysis overlay** — a visualization that reveals the
hidden temporal structure of the automaton by color-coding every cell based on its oscillation
period, detected via autocorrelation over the last 64 frames of timeline history.

| Color | Meaning |
|-------|---------|
| Ice blue | Still life — permanently alive, never changes |
| Emerald green | Period 2 — blinkers, toads, beacons |
| Gold | Period 3 — pulsars |
| Orange | Period 4–5 |
| Coral | Period 6–12 |
| Magenta | Period 13–32 |
| Hot red | Chaotic — toggles without clear periodicity |

Currently-alive cells are rendered brighter; cells in their "off" phase of an oscillation are
rendered dimmer, so you can see the full spatial extent of each oscillating structure. A legend
panel appears in the bottom-left corner.

This mode is especially revealing for patterns like:
- **Gosper Glider Gun** — the gun itself shows as period-30 (magenta), the stream as chaotic (red)
- **Pulsars** — beautiful gold p3 structures
- **R-pentomino aftermath** — still-life debris (blue) surrounded by oscillators (green/gold)

The analysis auto-refreshes every 8 generations while the simulation runs.

## Wormhole Portals

Press `W` to enter **portal placement mode** — create paired wormholes that tunnel cells
between distant grid regions, breaking the fundamental locality assumption of cellular automata.

### Placement
- **Left-click** once to place the entrance (cyan ring), click again to place the exit (magenta ring)
- **Right-click** cancels mid-placement or removes an existing portal under the cursor
- **Scroll wheel** adjusts portal radius (2–6 cells)
- Up to 8 portal pairs can coexist

### How it works
Cells within a portal's radius see **extra neighbors** from the paired endpoint, mapped by
positional offset — a cell 2 units left of entrance A sees neighbors 2 units left of exit B.
Coupling is bidirectional: both endpoints contribute neighbors to each other.

Because portal neighbors are *additive* (on top of the normal 8), cells near portals can have
**>8 effective neighbors**, creating overpopulation pressure under standard rules. This produces:

- **Glider tunneling** — streams enter one portal and emerge from the other
- **Cross-zone bleeding** — connect two different ruleset zones and watch their physics interfere
- **Oscillation disruption** — stable oscillators near portals destabilize from non-local influence
- **Emergent boundary effects** — the portal perimeter itself becomes a site of unusual activity

Portals are saved/loaded with `.life` files and render at all zoom levels with animated
swirling rings (cyan for entrances, magenta for exits). Press `c` twice to clear portals
along with zones and emitters.

## Save & Load

Press `Ctrl-S` to save the full simulation state to a numbered `.life` file (`save_001.life`,
`save_002.life`, etc.). Press `Ctrl-O` to load the most recent save. Everything is preserved:
grid state, cell ages, ghost trails, zones, emitters, absorbers, ruleset, symmetry, and wrapping.

- Saves use a compact binary format with magic header and version byte
- Auto-numbered slots (up to 999) prevent accidental overwrites
- Status bar flashes green confirmation on save/load
- Enables sharing interesting configurations and resuming experiments

## Implementation Details

- Double-buffered grid updates for correct neighbor counting
- Single-write rendering via pre-built output buffer (~60fps)
- SIGWINCH handler for live terminal resize
- Cell-age heatmap with 24-bit true-color thermal gradient (blue → cyan → green → yellow → red → white)
- Ghost trails: fading markers where cells die (5-frame decay with gray-blue tint)
- Legacy color-cycling mode (6-color palette) available via `h` toggle
- SGR extended mouse protocol (1006) for large terminal support
- Population sparkline using Unicode block elements (▁▂▃▅▇█) with red→yellow→green color gradient
- Toroidal wrapping mode — gliders and patterns loop around edges
- Kaleidoscope drawing — 2/4/8-fold symmetric mouse painting with reflections around grid center
- Multi-rule zones — paint different rulesets onto regions; cells use per-zone rules with emergent boundary interactions
- Zoom and pan — 3 zoom levels using Unicode half-block (▀▄) and quarter-block (▘▝▖▗) characters for 2x and 4x density
- Minimap overlay — quarter-block thumbnail of full grid with yellow viewport rectangle, auto-shows when zoomed
- Emitters and absorbers — placeable cell sources (dot, cross, random, glider patterns) and circular kill zones for open dissipative dynamics
- Time-travel replay — 256-frame history ring buffer with rewind/fast-forward, timeline scrubber bar, and branching from any historical point
- Save/load — binary `.life` files preserving full state (grid, zones, emitters, absorbers, settings) with auto-numbered slots and status flash feedback
- Frequency analysis — per-cell oscillation period detection via autocorrelation over timeline history, with ice-blue→emerald→gold→red color spectrum and legend overlay
- Wormhole portals — paired non-local spatial couplings with additive neighbor model, animated ring visualization, positional offset mapping, and bidirectional coupling across up to 8 portal pairs
- Full 400×200 simulation grid with viewport navigation (arrow keys + mouse scroll)
- Proper terminal cleanup on exit (raw mode restore, cursor show)
