# Conway's Game of Life — Terminal Edition

A fully interactive, zero-dependency Game of Life that runs in any terminal.
Written in C with ANSI escape codes for rendering.

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
- Proper terminal cleanup on exit (raw mode restore, cursor show)
