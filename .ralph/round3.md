---
round: 3
goal: "Cell-age heatmap rendering with ghost trails"
status: complete
---

## What was built
- **Cell-age heatmap**: Grid stores cell age (0=dead, 1–255=generations alive) instead of binary state. Surviving cells increment age each generation, capped at 255. A thermal gradient maps age to 24-bit true color via interpolated stops: blue (newborn) → cyan → green → yellow → red → white-hot (ancient/stable structures)
- **Ghost trails**: When a cell dies, it leaves a fading `·` marker that decays over 5 frames with a dim gray-blue tint — gliders leave visible streaks, oscillator boundaries shimmer
- **Toggle**: Press `h` to switch between heatmap mode (on by default) and legacy flat-color cycling. Status bar shows `█HEAT` indicator when active
- **Preserved state on resize**: `apply_resize` carries over cell ages and ghost state
- **Right-click erase** triggers ghost trails for visual feedback
- Render buffer enlarged to accommodate true-color ANSI sequences (`\033[48;2;R;G;Bm`)

## Technical notes
- Thermal gradient uses 6 color stops with linear interpolation between them
- Ghost brightness scales from 30–80 with a blue tint (40–100), creating a cool afterglow effect
- Neighbor counting changed from summing raw grid values to `(grid[ny][nx] > 0)` since grid now stores age, not 0/1

## Future directions
- Stamp mode: pick a pattern and stamp it at mouse position
- Save/load state to file
- Rule variants (HighLife, Day & Night, etc.)
- Undo/redo system
- Network multiplayer (shared grid via sockets)
