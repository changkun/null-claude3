# Round 75 — Interface Roughness Analyzer Overlay

## Goal
Add an interface roughness analyzer that measures the scaling exponents of cluster boundaries to detect KPZ universality class and other surface growth physics.

## What was built

**Overlay #41** — Interface Roughness Analyzer (`Ctrl-V`, ghost code 42)

### Components

1. **State declarations** (~65 lines) — Per-cell boundary flags, column height profile, multi-scale interface width array, roughness exponent α with R² fit quality, fractal dimension, growth exponent β, sparkline histories.

2. **Compute function** `ir_compute()` (~200 lines) — Eight-phase algorithm:
   - Phase 1: Identify boundary cells (alive with ≥1 dead von Neumann neighbor)
   - Phase 2: Build column height profile h(x) = topmost boundary y per column
   - Phase 3: Compute interface width w(ℓ) at 7 window scales (2,4,8,16,32,64,128)
   - Phase 4: Log-log regression for roughness exponent α (w ~ ℓ^α)
   - Phase 5: Classify universality class (SMOOTH/KPZ/EDW-WILK/ANOMALOUS)
   - Phase 6: Per-cell roughness values for rendering
   - Phase 7: Sparkline history update
   - Phase 8: Growth exponent β estimation from log(w) vs log(t)

3. **Grid rendering** (~50 lines) — Boundary cells colored by local height deviation: cyan (smooth) → amber (moderate) → red (rough) → white-hot (extreme). Interior alive cells dim warm glow. Nearby dead cells get faint ambient glow for context.

4. **Sidebar panel** (10 rows) — Shows:
   - Roughness exponent α with universality classification
   - Interface width w, boundary count, R² of log-log fit
   - Fractal dimension d_f = 2−α and growth exponent β
   - Multi-scale w(ℓ) values at different window sizes
   - Color scale legend with toggle hint
   - Two sparklines: w(t) width history and α(t) exponent history

5. **Keyboard handler** — `Ctrl-V` toggle with flash message

6. **Main loop integration** — Recomputes every 4 generations when running

7. **Panel stacking** — Integrated into percolation panel row offset

### Algorithm

```
For each frame (every 4 generations):
  1. Scan grid marking boundary cells (alive + ≥1 dead 4-neighbor)
  2. Build height profile: h[x] = topmost boundary y in column x
  3. Compact valid heights into contiguous array (skip empty columns)
  4. For each scale ℓ ∈ {2,4,8,16,32,64,128}:
     a. Slide overlapping windows (stride ℓ/2) across height profile
     b. In each window: compute local mean, then variance
     c. w(ℓ) = sqrt(mean(variance_per_window))
  5. Log-log regression: log(w) = α·log(ℓ) + const
     - Slope = roughness exponent α
     - R² measures fit quality
  6. Classify: α<0.15→SMOOTH, 0.15-0.42→KPZ, 0.42-0.58→EDW-WILK, >0.58→ANOMALOUS
  7. Per-cell: boundary cells → |y - ⟨h⟩| / max_dev (normalized deviation)
  8. β from temporal log-log: log(w_total) vs log(t)
```

### Integration points in life.c

- State declarations: after su overlay block (~line 1877)
- Overlay table: index 41, `{ "Roughness", 'V'-64 }`
- Ghost code: `if (ir_mode) return 41;` in split_detect_current
- Stale flag: `ir_stale = 1;` in grid_step
- Compute: `case 41: if (ir_stale) ir_compute();` in split_ensure_computed
- Cell color: after su_mode block with ghost code 42
- Sidebar: after su panel, before percolation panel
- Keyboard: `key == 22` (Ctrl-V) after su handler
- Main loop: recompute every 4 generations
- Panel stacking: added to percolation panel row offset
- Demo reset: `ir_mode = 0;` in demo reset section

## Visualization

- **Heatmap**: Boundary cells: cyan → amber → red → white-hot (by local roughness)
- **Interior**: Dim warm glow for alive cells, faint ambient for nearby dead
- **Sidebar**: α exponent, classification, w, boundary count, d_f, β, w(ℓ) values, sparklines
- **Refresh**: Every 4 generations (lazy via stale flag)

## Connections to other rounds

- **Fractal Dimension (R30)**: Measures bulk structure via box-counting; roughness analyzes *boundary* geometry specifically. d_f complements D_box.
- **Topology (R38)**: Counts components (β₀) and holes (β₁); roughness measures *shape* of component boundaries.
- **Percolation (R49)**: Identifies spanning clusters; roughness characterizes the *geometry* of cluster edges.
- **Susceptibility (R74)**: χ diverges at phase transitions; roughness exponent α characterizes the universality class of the transition.
- **Correlation Length (R43)**: ξ measures spatial correlations; α measures interface correlations.
- **Mean Field (R71)**: MF predicts smooth boundaries; α>0 reveals fluctuation-driven roughening beyond MF.

## Technical details

- ~350 lines of new C code
- Zero dynamic allocation (all static arrays)
- Multi-scale windowed variance computation for robust α estimation
- R² reported for fit quality assessment
- Handles sparse boundaries (columns with no boundary cells skipped)
- Growth exponent β tracked from rolling w(t) history

## Explore vs exploit

**67% explore / 33% exploit.** Novel direction: first overlay to analyze *boundary geometry* and non-equilibrium surface growth physics. Exploits existing boundary detection patterns and the established overlay integration framework. The KPZ universality class connection is entirely new territory for this project.
