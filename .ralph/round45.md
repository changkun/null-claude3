# Round 45: Wave Mechanics Overlay

**Date**: 2026-03-16
**Explore/Exploit**: 95% explore / 5% exploit

## Goal
Add a wave mechanics overlay that treats cell births/deaths as impulse sources
propagating ripples via a damped 2D wave equation, revealing interference patterns
and information propagation across the automaton.

## What was built

### Wave Mechanics (`~` key)
- **Impulse sources**: Every cell birth emits a positive impulse (+0.5), every death
  emits a negative impulse (-0.5) into the wave field
- **2D wave equation**: Discretized damped wave propagation using 5-point Laplacian
  stencil with velocity Verlet integration
  - `v(t+1) = damping * (v(t) + c² * ∇²u) + impulse`
  - `u(t+1) = u(t) + v(t+1)`
  - Damping = 0.96, wave speed c² = 0.25 (stable for 2D lattice)
- **Color scheme**: Cyan (positive amplitude) → dark (zero) → orange (negative)
  with smooth multi-segment gradients
- **Live cells brightened**: +55 to all channels for visibility
- **Stats panel** (right side, stacking): Energy, max amplitude, estimated dominant
  wavelength (from zero-crossing analysis), propagation speed, source count, color
  gradient legend, energy sparkline
- **Status bar**: `◆≈{energy}E` in teal
- **Probe inspector**: Shows amplitude and velocity at clicked cell
- **Auto-refresh**: Every generation (continuous propagation needed)
- **Soft clamping**: ±2.0 prevents runaway amplitudes

### What it reveals
- **Standing waves** form near oscillators (blinkers, pulsars) — constructive
  interference creates bright nodes
- **Expanding wavefronts** trace the boundary of chaos (R-pentomino, acorn)
- **Interference patterns** at collision boundaries between active regions
- **Information speed**: Ripples propagate at c ≈ 0.5 cells/gen through the medium,
  distinct from the GoL's own information speed of 1 cell/gen

## Technical details
- Zero heap allocation (static arrays for amplitude + velocity fields)
- Boundary condition: fixed (zero at edges) — waves reflect off grid boundaries
- Wavelength estimation via horizontal zero-crossing density in central band
- Energy = sum of kinetic (v²) + potential (u²) across all cells
- Sparkline history: 64-frame buffer with log-scale energy display

## Lines
- Before: 11,639
- After: 12,033
- Added: +394 lines

## Classification
~95% exploration — adds continuous physics (wave equation) on top of the discrete
automaton, a completely new analytical dimension. The project had no continuous-field
overlays before; this bridges discrete CA dynamics with continuum wave mechanics.
