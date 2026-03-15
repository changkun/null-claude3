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
| `[` / `]` | Cycle through rule presets (B/S notation) |
| `m` | Mutate — randomly flip one birth/survival bit |
| `k` | Cycle symmetry: none → 2-fold → 4-fold → 8-fold (kaleidoscope) |
| `1`–`5` | Load preset pattern |
| `+` / `-` | Speed up / slow down (20ms–1000ms) |
| `q` / `ESC` | Quit |

### Mouse (draw mode)

| Action | Effect |
|--------|--------|
| Left-click / drag | Place cells |
| Right-click / drag | Erase cells |

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
- Proper terminal cleanup on exit (raw mode restore, cursor show)
