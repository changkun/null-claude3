# Round 22: Topological Surface Modes

## Goal
Replace the binary flat/torus toggle with 5 selectable grid topologies that change the fundamental geometry of space: flat, torus, Klein bottle, Möbius strip, and projective plane. Each non-orientable surface creates qualitatively different dynamics — gliders re-enter mirrored, symmetry breaks spontaneously, and familiar patterns behave in unfamiliar ways.

## What Was Built

### 5 Topology Modes
- **Flat** (default): Finite boundaries, no wrapping — cells at edges have fewer neighbors
- **Torus** (∞): Standard wrapping — left↔right and top↔bottom, preserving orientation
- **Klein bottle (𝕂)**: Left↔right normal wrap, but top↔bottom wraps with horizontal flip (x → W-1-x). A glider exiting the top re-enters from the bottom *mirrored*
- **Möbius strip (𝕄)**: Left↔right wraps with vertical flip (y → H-1-y), top↔bottom are open boundaries. The single twisted identification creates a one-sided surface
- **Projective plane (ℙ)**: Both identifications are flipped — left↔right with vertical flip AND top↔bottom with horizontal flip. The most topologically exotic option

### Implementation Details
- Central `topo_map(int *nx, int *ny)` function replaces `wrap_coord()` — returns validity and applies coordinate transform in-place
- All neighbor lookups (main simulation, portal coupling, census border checks) route through `topo_map`
- Genetic search mini-simulation stays flat intentionally (topology-independent fitness)
- `w` key cycles through all 5 topologies (was binary toggle)
- HUD shows topology symbol and name (𝕂Klein, 𝕄Möbius, ℙproj, ∞torus)
- Help bar updated: `[w]topo`
- Save/load backward-compatible: old wrap_mode=0→flat, wrap_mode=1→torus; invalid values clamped

### Mathematical Notes
- Klein bottle and projective plane are non-orientable — there is no consistent "left" and "right" across the surface
- On a Klein bottle, a glider gun aimed upward will have its output collide with the mirrored stream coming from below
- The projective plane has no boundary at all (like a torus) but with both identifications twisted — the most "alien" geometry
- Möbius strip retains open top/bottom edges, making it a bounded non-orientable surface

## Key Binding
- `w`: Cycle topology: flat → torus → Klein → Möbius → projective → flat

## Files Modified
- `life.c`: Replaced `wrap_mode` boolean with `topology` enum, added `topo_map()`, updated all call sites, HUD, help text, save/load, key handler
