# Round 32: Information Flow Field

**Goal:** A transfer entropy-based directional causal influence vector field overlay that reveals how information propagates through the automaton — the first analyzer measuring *directed relationships* rather than cell properties.

**Explore/Exploit:** 30% explore (transfer entropy computation, causal vector field concept), 70% exploit (builds on temporal analysis infrastructure from rounds 26–31).

## What Was Built

A new overlay (`O` key) that computes pairwise transfer entropy between cells and their cardinal neighbors over a 16-frame temporal window, producing a vector field of net information flow.

### Transfer Entropy Computation
For each 4×4 block, the system computes TE(X→Y) for the center cell and its 4 cardinal neighbors. Transfer entropy measures how much X's past reduces uncertainty about Y's future beyond Y's own history — capturing true directional causation rather than mere correlation.

### Vector Field Rendering
- **HSV-colored grid cells**: hue encodes flow direction, brightness encodes magnitude
- **Unicode arrow glyphs** (→↗↑↖←↙↓↘) rendered at block centers on alive cells and flow ghosts
- Net information outflow direction and magnitude computed per block

### Overlay Panel
Right-side panel displaying:
- Mean and max flow magnitude
- Vorticity (curl of the vector field)
- Source and sink counts
- 8-bin direction histogram with color-coded bars
- Dominant direction compass indicator

### Derived Metrics
- **Sources**: regions with high outward TE — information exporters (e.g., glider guns, emitters)
- **Sinks**: regions with high inward TE — information absorbers (e.g., colliders, absorber structures)
- **Vorticity**: rotational information currents, measuring swirling flow patterns
- **Divergence detection**: identifies where information is being created vs. destroyed

## Key Design Decisions

1. **Directional causation over correlation:** Transfer entropy is asymmetric — TE(X→Y) ≠ TE(Y→X) — which distinguishes cause from effect. A glider gun *causes* changes downstream; downstream cells don't cause changes in the gun.
2. **Block-level aggregation:** Computing per-cell TE would be prohibitively expensive. 4×4 blocks with cardinal-neighbor sampling balances spatial resolution with performance.
3. **16-frame temporal window:** Long enough to capture multi-generation causal chains, short enough to track evolving dynamics.
4. **HSV direction encoding:** Maps the circular direction space naturally to hue, with magnitude controlling brightness — perceptually uniform and intuitive.

## Significance

This is the first analyzer in the project that measures *relationships between cells* rather than *properties of cells*. Previous analyzers (entropy, Lyapunov, fractal dimension, frequency) all compute per-cell or per-region statistics. The information flow field reveals the causal *architecture* — showing that cell A influences cell B but not vice versa, and mapping the global pattern of information transport.

## Files Changed
- `life.c`: +~490 lines (transfer entropy state, computation function, vector field rendering, panel overlay, key binding)
- `README.md`: Added `O` key to controls table and Information Flow Field section
