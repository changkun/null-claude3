# Round 9: Emitters & Absorbers — Open Dissipative Cellular Automata

## Goal
Transform the CA from a closed conservative system into an open dissipative one by adding placeable **emitters** (cell sources) and **absorbers** (cell sinks) that continuously inject and remove cells, creating steady-state flow patterns.

## What Was Built

### Emitters (Sources)
- Placeable point sources that spawn live cells at configurable intervals
- 4 emission patterns: dot, cross, random 3x3, glider
- Configurable fire rate (every 1-20 generations)
- Glider emitters automatically vary direction based on position
- Respects kaleidoscope symmetry when placing

### Absorbers (Sinks)
- Placeable circular kill zones that continuously remove all cells within radius
- Configurable radius (1-10 cells)
- Scroll wheel adjusts radius while in emit mode
- Creates stable "death zones" that structure flow

### Emit Mode (`e` key)
- Left-click places emitter at cursor position
- Right-click places absorber at cursor position
- Middle-click removes nearby emitters/absorbers (radius 3)
- `[`/`]` cycles emission pattern (dot/cross/rand3/glider)
- `+`/`-` adjusts emission rate
- Scroll wheel adjusts absorber radius
- Works with all zoom levels via viewport coordinate translation
- Mutually exclusive with zone-paint mode

### Visual Feedback
- Emitters: bright cyan/white pulsing markers (◉) that pulse with fire rate
- Absorbers: dark red/purple markers (○) at center, with radius ring visible in emit mode
- Status bar shows: pattern, rate, absorber radius, count of emitters/absorbers
- Markers render correctly at all 3 zoom levels (1x, 2x half-block, 4x quarter-block)

### Integration
- Sources/sinks fire after each `grid_step()`, maintaining steady-state
- Double-clear (`c` twice) removes all emitters and absorbers
- Emitter/absorber state persists across grid clear (first `c`)
- Up to 32 emitters and 32 absorbers simultaneously

## Emergent Behaviors Enabled
- **Glider rivers**: Glider emitter → absorber creates persistent stream
- **Standing waves**: Balanced source/sink pairs with oscillating patterns
- **Zone boundary turbulence**: Emitter output hitting zone rule boundaries
- **Symmetric flow fields**: 8-fold emitter placement creates mandala-like flows
- **Population homeostasis**: Sources replace what sinks consume, stabilizing population

## Controls Summary
- `e` — Toggle emitter/absorber placement mode
- Left-click — Place emitter (in emit mode)
- Right-click — Place absorber (in emit mode)
- Middle-click — Remove nearby source/sink
- `[`/`]` — Cycle emission pattern
- `+`/`-` — Adjust emission rate
- Scroll wheel — Adjust absorber radius
- `c` twice — Clear everything including emitters/absorbers

## Technical Details
- ~200 lines of new code added
- Forward declarations used for grid_set/grid_unset dependency
- Source/sink application happens post-step, pre-histogram
- Marker rendering integrated into cell_color with new return types (4=emitter, 5=absorber)
- Half-block and quarter-block renderers updated to treat markers as solid cells
