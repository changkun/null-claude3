# Round 71 — Mean Field Deviation

## Goal
Add a Mean Field Deviation overlay that compares actual CA dynamics to
mean-field theory predictions, revealing where spatial correlations and
geometry override simple density statistics.

## What was built
- **Overlay #38** (ghost layer) — Toggle key `Ctrl-F`
- Mean-field prediction engine using exact binomial distribution with C(8,k)
  coefficients and active B/S rule mask
- Per-cell comparison of MF prediction vs actual CA outcome
- Zone-specific rule support (per-cell ruleset zones)
- Rolling accuracy tracking with exponential-decay window per cell
- Structural score: spatial clustering of deviations
- Color scheme: deep teal (MF accurate) → amber (deviating) → hot orange
  (correlations dominate) → white-gold (pure structural dynamics)
- Full sidebar panel with accuracy %, mean deviation, correlation fraction,
  structure score, interpretive classification, color legend, dual sparklines

## Algorithm
1. For each cell: compute local density ρ from 8-neighbor count
2. Using ρ as Bernoulli probability, compute P(k neighbors alive) via
   binomial: C(8,k) * ρ^k * (1-ρ)^(8-k)
3. Sum probabilities for birth counts (if dead) or survival counts (if alive)
   using the active B/S rule mask → MF-predicted probability of being alive
4. Compare prediction to actual next-state outcome → deviation
5. Accumulate per-cell hit/miss in ring buffer for rolling accuracy
6. Compute spatial clustering of high-deviation cells → structural score

## Technical details
- ~420 lines added (~21,830 → ~22,250)
- Zero dynamic allocation, all static arrays
- Lazy recomputation via mf_stale flag
- Precomputed binomial coefficients C(8,k)
- Integrated into split-screen system as overlay index 37
- Panel stacks below all existing panels

## Explore/Exploit: 95% explore / 5% exploit
- Entirely new analytical domain: statistical mechanics mean-field theory
- Binomial MF prediction is new mathematical framework for the project
- Exploits existing grid infrastructure, overlay pattern, and zone system

## Connections
- Complements all other overlays: while they measure *what* is happening
  (entropy, vorticity, defects), MF deviation reveals *why* — density
  statistics vs spatial correlations
- Connects to Ising model mean-field theory, Landau theory, and the
  renormalization group program
- Gliders/oscillators glow hot (MF fails), random soups stay dark (MF works)
