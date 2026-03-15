# Round 27: Stochastic Temperature Field — Phase Transitions in Life

## Goal
Add a temperature parameter (T = 0.0–1.0) that injects probabilistic noise into rule evaluation, transforming the deterministic cellular automaton into a statistical mechanics system. At T=0 rules behave classically; at T>0 each birth/death decision has probability T of flipping, creating spontaneous ignition and random death. Spatial temperature gradients allow coexisting ordered (cold) and chaotic (hot) regions with visible phase boundaries.

## What Was Built

### Data Structures
- `temp_grid[MAX_H][MAX_W]` — per-cell temperature value (0.0–1.0)
- `temp_global` — uniform global temperature offset added to all cells
- `temp_brush` / `temp_brush_radius` — brush temperature and radius for spatial painting

### Core Mechanics — Stochastic Rule Evaluation
- In `grid_step()`, after computing the deterministic birth/death outcome, each decision is flipped with probability `T = temp_global + temp_grid[y][x]`
- Works for both standard Conway/zone rules and ecosystem (multi-species) mode
- T=0.0: no change from previous deterministic behavior
- T>0: dead cells can spontaneously ignite, live cells randomly die
- At critical T (~0.15 for Conway), the system exhibits a phase transition between static order and dynamic chaos

### Visualization
- **Temperature overlay** in `cell_color()`: blue (cold/ordered) → white (mid) → red (hot/chaotic) color wash
  - Alive cells: 50/50 blend of base cell color and temperature wash
  - Dead cells: dim temperature wash (reveals thermal landscape)
- **Overlay panel**: top-right box (shifts below entropy panel if both active) showing:
  - Global temperature, brush temperature, brush radius
  - Mean temperature (T̄), max temperature (T_max), count of hot cells
  - 10-bin temperature distribution histogram with blue→red gradient bars

### Temperature Painting
- `temp_paint()` applies Gaussian-like falloff from brush center
- `temp_paint_sym()` respects the current symmetry setting (2-fold, 4-fold, 8-fold)
- Left-click paints hot zones at brush temperature
- Right-click paints cold zones (resets to T=0)

### Controls
- `X` — Toggle temperature mode (activates draw mode, disables conflicting modes)
- Left-click/drag — Paint hot zones at brush temperature
- Right-click/drag — Paint cold zones (T=0)
- Scroll wheel — Adjust brush radius
- `+`/`-` — Adjust brush temperature (0.05 increments, 0.0–1.0)
- `{`/`}` — Adjust global temperature (0.01 increments, 0.0–1.0)
- `[`/`]` — Adjust brush radius (1–20)
- Double `c` — Clears temperature field along with zones/portals

### Interactions with Other Systems
- **Shannon entropy heatmap** (round 26): entropy visibly increases with temperature — see information theory meet thermodynamics
- **Multi-species ecosystems**: temperature flips both species' birth/death decisions
- **Zone rules**: temperature applies on top of zone-specific rulesets
- **Portals and topology**: fully compatible

## Files Modified
- `life.c`: ~300 lines added (globals, stochastic flip in grid_step for both standard and ecosystem modes, temp_to_rgb color mapping, temp_paint/temp_paint_sym spatial painting, cell_color temperature overlay, overlay panel with histogram, HUD indicator, key bindings for X/{/}/+/-/[/]/scroll/mouse, double-c clear integration, help bar update)
