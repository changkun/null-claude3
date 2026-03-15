# Round 13: Signal Tracer — Persistent Motion Trail Visualization

## Goal
Add a "signal tracer" mode that renders persistent colored trails behind moving structures (gliders, spaceships, expanding wavefronts), creating beautiful flow visualizations that reveal the hidden highways and rivers of information flow across the grid.

## What was built
- **Tracer array** (`tracer[MAX_H][MAX_W]`): Accumulates cell presence over time (0-255 per cell)
- **Three-mode cycle** via `T` key:
  - **Off** (default): No tracer rendering
  - **Accumulate**: Every generation, alive cells increment their tracer value (cap 255). Dead cells show their accumulated trail as colored background
  - **Frozen**: Trails remain visible but stop growing, letting you observe existing trails while simulation continues
  - Pressing `T` from frozen returns to off and clears all tracer data
- **Purple→magenta→pink color gradient**: Distinct from the blue→red→white thermal heatmap
  - Low values (barely visited): dark indigo
  - Medium values: violet/purple
  - High values (heavily visited): bright magenta/hot pink
- **Full zoom integration**: Tracer trails render correctly at all 3 zoom levels (1x solid blocks, 2x half-blocks, 4x quarter-blocks)
- **Minimap integration**: Tracer trails appear on the minimap when tracer mode is active (threshold > 10 to avoid noise)
- **Status bar indicator**: Shows `▓TRACE` (magenta) when accumulating, `▓TRACE:FRZ` (dim purple) when frozen
- **Help bar**: Added `[T]trace` to the compact help line

## Key behaviors
- Tracer only accumulates during live simulation (not during replay)
- Grid clear (`c`) also clears tracer data
- Tracer data is ephemeral (not saved to .life files) — it's a visualization tool, not state
- Ghost trails (5-frame decay) still render on top of tracer when heatmap mode is on
- Zone backgrounds still render when tracer is off

## Impact
- Glider streams from Gosper Guns create visible purple rivers across the grid
- Emitter outputs become flow field visualizations
- Zone boundary interactions show up as collision/refraction patterns
- Symmetric patterns (8-fold + tracer) produce stunning mandala-like trail art
- Makes the difference between static structures and moving structures instantly visible

## Architecture
- Added ~30 lines to `grid_step()` and `cell_color()` — minimal footprint
- `tracer_to_rgb()` implements a 4-stop gradient with smooth interpolation
- Cell color type 6 added for tracer, integrated into all 3 zoom renderers
- No changes to save/load format or timeline system

## Lines changed: ~60 net additions
