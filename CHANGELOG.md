# Changelog

Development history organized by round. Each round adds one or two major
features. The project grew from ~790 lines (Round 1) to ~14,457 lines
(Round 52).

## Round 52 — Topological Persistence Barcode

Persistent homology meets cellular automata. Press `:` to track connected
components across generations and visualize their lifetimes as a
persistence barcode. Each component is matched frame-to-frame via spatial
overlap; when it appears that's its "birth," when it vanishes its "death."
Grid cells are colored by component age: white/cyan (newborn) → green
(young) → gold/amber (ancient, persistent). Sidebar panel shows live
feature count, total born/died, mean and max lifetime, β₀ sparkline,
and a barcode diagram of the top 7 longest-lived features — long bars =
structurally stable (still lifes, oscillator cores), short bars =
ephemeral noise. Alive features display an arrow tip (▶), dead features
a terminator (│). Resets automatically on grid clear/randomize. Builds
on existing topological feature map infrastructure (round 34) with
temporal tracking. +504 lines.

## Round 51 — Hamiltonian Energy Landscape

Ising spin model analogy for the automaton. Press `;` to visualize
per-cell Hamiltonian energy H = -s·Σs_neighbors where alive=+1, dead=-1.
Deep blue = energy wells (stable, aligned still lifes), cyan = shallow
wells, yellow/orange = mild frustration at boundaries, bright red =
strongly frustrated domain walls. Sidebar shows total energy H,
magnetization m, susceptibility χ (peaks at phase transitions), domain
wall count/density, energy sparkline, and phase classification
(Ordered/Critical/Disordered/Paramagnetic). Explains *why* certain
structures are stable from a statistical mechanics perspective — still
lifes sit in deep energy minima, chaotic regions are frustrated
high-energy states. +385 lines.

## Round 50 — Spacetime Kymograph Mode

First visualization treating time as a spatial dimension. Press `K` to
replace the normal grid view with a spacetime diagram: X-axis = space
(one scan row), Y-axis = time (scrolling upward, newest at bottom).
Half-block rendering (▀) packs 2 generations per terminal row. Thermal
gradient maps cell age through blue → cyan → green → yellow → red →
white. ↑/↓ moves the scan row (buffer resets for clean transition),
←/→ pans horizontally. 256-deep ring buffer with zero allocations.
Info panel shows scan row, buffer depth, row-population sparkline
(▁▂▃▅▇█), and key hints. Classic Wolfram-style spacetime diagram —
gliders trace diagonal worldlines, oscillators create vertical stripes,
chaotic regions appear as textured noise. +290 lines.

## Round 49 — Percolation Analysis Overlay

Cluster connectivity analysis via flood fill. Toggle with `|` — colors
each connected cluster uniquely with the largest cluster highlighted in
gold. Detects spanning clusters (edge-to-edge connectivity in H/V
directions) and displays percolation order parameter P∞ (fraction of
cells in largest cluster). Sidebar shows density relative to the square
lattice critical threshold p_c ≈ 0.593, spanning status, P∞ sparkline,
and phase classification (Supercritical/Near-critical/Subcritical/Dilute).
Full split-screen support as overlay index 21. +362 lines.

## Round 48 — Split-Screen Dual Overlay Comparison

Side-by-side comparison mode for any two of the 20 analysis overlays.
Toggle with `)` — divides the viewport vertically with a thin divider.
TAB cycles the right panel's overlay through all 21 options (None + 20
overlays); backtick cycles the left panel. Initializes intelligently:
left = current active overlay, right = next in list. Status bar shows
`SPLIT:LeftName|RightName` indicator. Overlay stats panels hidden in
split mode to keep the view clean. Works at zoom==1 (default); higher
zooms render normally. Pure rendering composition — no new analysis math,
just a UI layer that multiplies the value of all existing overlays by
enabling direct comparison (e.g., entropy vs. vorticity, wave mechanics
vs. ergodicity). +256 lines.

## Round 47 — Ergodicity Metric

Per-cell time-averaged density (EMA) compared against global spatial average
density. Deviation map shows ergodic regions (green, where time and space
averages converge) versus non-ergodic regions (magenta, persistent local bias
from still lifes, oscillators, or absorbing boundaries). Stats panel displays
ergodicity index (0–1), fraction of ergodic cells, equilibration generation,
phase classification (Broken → Ergodic), and sparkline history. Toggle with
`(`. Key insight: Conway's Life is fundamentally non-ergodic — frozen
structures create permanently biased cells visible as magenta clusters against
a green chaotic background.

## Round 46 — Vorticity Detection

Discrete curl of a velocity field derived from temporal density changes.
Estimates local flow via activity-weighted center-of-mass shifts, then
computes ω = ∂vy/∂x − ∂vx/∂y. Blue = clockwise, dark = irrotational,
red = counterclockwise. Detects vortex centers as local |ω| maxima.
Stats panel with net circulation Γ, vortex count, and sparkline. Toggle
with `*`.

## Round 45 — Wave Mechanics

Damped 2D wave equation overlay. Cell births emit positive impulses,
deaths negative — propagating via discretized wave equation with 5-point
Laplacian stencil. Cyan = positive amplitude, dark = zero, orange =
negative. Reveals standing waves near oscillators, expanding wavefronts,
and interference patterns. Toggle with `~`.

## Round 44 — Entropy Production Rate

Temporal derivative of local Shannon entropy (dS/dt) overlay. First overlay
to capture a temporal thermodynamic quantity rather than a spatial snapshot.
Blue = entropy decreasing (self-organization), red = entropy increasing
(dissolution), gray = equilibrium. EMA-smoothed for stable tracking. Stats
panel shows global dS/dt, ordering/disordering fractions, thermodynamic
phase classification, and sparkline history. Toggle with `=`.

## Round 43 — Spatial Correlation Length

Two-point correlation function C(r) and correlation length ξ overlay.
Measures how spatially correlated cell states are at different distances,
extracting the characteristic scale of order. Per-cell heatmap from
uncorrelated (violet) to strongly correlated (white). Stats panel shows ξ,
C(1), R² fit quality, C(r) sparkline, and phase classification (disordered /
short-range / near-critical / long-range). Toggle with `&`.

## Round 42 — Auto-Demo Mode

Guided tour through 10 curated scenes showcasing pattern + overlay
combinations. Press `D` to start, any key to exit.

## Round 41 — Cell Probe Inspector

Unified per-cell metric display. Click any cell with `?` active to see all
14+ analysis values at once.

## Round 40 — Kolmogorov Complexity

LZ77-style compression of local neighborhoods to estimate algorithmic
complexity. Per-cell heatmap from compressible (blue) to incompressible (red).

## Round 39 — Renormalization Group Flow

Real-space RG via majority-rule block decimation at 2×, 4×, 8× scales.
Criticality score detects scale-invariant (critical) configurations.

## Round 38 — Topological Feature Map

Connected component labeling (β₀) and enclosed hole detection (β₁) via
4-connected flood fill. Each component uniquely colored.

## Round 37 — Composite Complexity Index

Fuses entropy, Lyapunov, surprise, and frequency into a per-cell
edge-of-chaos score with concave edge-boost transform.

## Round 36 — Mutual Information Network

Inter-region coupling map. Grid partitioned into 20×10 blocks, MI computed
for all ~20K block pairs. Top-40 couplings rendered as Bresenham lines.

## Round 35 — Prediction Surprise Field

Per-cell transition surprisal based on accumulated 32-frame statistics.
Highlights where outcomes defy the learned distribution.

## Round 34 — Causal Light Cone

Backward/forward dependency cones from a selected cell. Visualizes
relativistic-style information propagation limits.

## Round 33 — Phase-Space Attractor

Takens delay embedding of population time series. Auto-tuned delay τ,
correlation dimension D₂ via Grassberger-Procaccia.

## Round 32 — Information Flow Field

Transfer entropy causal vectors between cells. Arrow glyphs show net
information flow direction with source/sink/vorticity detection.

## Round 31 — Wolfram Class Detector

Real-time automaton classification into Wolfram Classes I–IV based on
population dynamics, entropy, and Lyapunov analysis.

## Round 30 — Fractal Dimension Analyzer

Box-counting method at scales 2–128 with log-log regression for D_box
and R² goodness-of-fit.

## Round 29 — 2D Fourier Spectrum

Spatial FFT of grid density with radial power distribution display.

## Round 28 — Lyapunov Sensitivity Map

Shadow grid perturbation tracking for divergence visualization. Positive
exponent = chaos, zero = edge, negative = stability.

## Round 27 — Temperature Field

Stochastic noise field with paintable hot/cold regions. Enables
phase-transition experiments in cellular automata.

## Round 26 — Shannon Entropy Heatmap

Per-cell local Shannon entropy of 3×3 neighborhoods with distribution panel.

## Round 25 — Population Dynamics Dashboard

Per-species population graphing overlay with statistics.

## Round 24 — RLE Import/Export

Standard Game of Life Run Length Encoded pattern format support. Import via
command-line argument, export via `Ctrl-E`.

## Round 23 — Spaceship Detection

Extended pattern census with glider and spaceship recognition.

## Round 22 — Topological Surface Modes

Flat, torus, Klein bottle, Möbius strip, and projective plane boundary
conditions.

## Round 21 — Braille Ultra-Density Rendering

8× zoom using Braille Unicode characters for maximum information density.

## Round 20 — Genetic Rule Explorer

Genetic algorithm evolving 20 candidate rulesets per generation toward
"interesting" behavior (population stability, oscillation, complexity).

## Round 19 — Pattern Census Overlay

Real-time structure recognition via bitmask matching with dead-border
verification. Identifies 14 pattern templates (8 still lifes + 6 oscillator
phases).

## Round 18 — PPM Screenshot Capture

Single-frame and full-timeline PPM export with auto-numbered filenames.

## Round 17 — Pattern Stamp Tool

Library of 20 classic patterns with 4 rotations each. Preview overlay,
integration with symmetry and species modes.

## Round 16 — Dual-Species Ecosystem

Red and Blue populations with independent B/S rules and tunable inter-species
interaction coefficient (-1.0 hostile to +1.0 cooperative).

## Round 15 — Wormhole Portals

Paired non-local spatial couplings with additive neighbor model. Up to 8
portal pairs with animated ring visualization.

## Round 14 — Frequency Analysis

Per-cell oscillation period detection via autocorrelation over 64 frames.
Color spectrum from ice blue (still) to hot red (chaotic).

## Round 13 — Signal Tracer

Persistent motion trail visualization with purple → magenta → pink palette.
Accumulate, freeze, and clear modes.

## Round 12 — Interactive Rule Editor

Clickable B/S bit grid and preset name buttons. Live simulation underneath.

## Round 11 — Save/Load

Binary `.life` files with magic header, version byte, and auto-numbered slots.

## Round 10 — Time-Travel Replay

256-frame history ring buffer with rewind, fast-forward, and branching from
any historical point. Timeline scrubber bar.

## Round 9 — Emitters & Absorbers

Point cell sources (4 patterns) and circular kill zones for open dissipative
dynamics.

## Round 8 — Multi-Rule Zones

Per-cell ruleset assignment with preset zone layouts. Emergent boundary
behavior between different rule regions.

## Round 7 — Minimap Overlay

Quarter-block thumbnail of full grid with yellow viewport rectangle.

## Round 6 — Zoom & Pan

3 zoom levels (1×, 2× half-block, 4× quarter-block) with arrow key and
mouse navigation.

## Round 5 — Kaleidoscope Drawing

2/4/8-fold symmetric mouse painting with reflections around grid center.

## Round 4 — Rule Mutation

Live B/S bit flipping to explore the rule space. Status bar mutant indicator.

## Round 3 — Cell-Age Heatmap & Ghost Trails

24-bit true-color thermal gradient for cell age. 5-frame fading markers
where cells die.

## Round 2 — Mouse Painting, Sparkline, Toroidal Wrapping

Interactive drawing mode, population sparkline with Unicode blocks, and
toroidal (wrapping) boundary conditions.

## Round 1 — Foundation

Core Game of Life simulation with 10 rule presets, 5 starting patterns,
keyboard controls, ANSI rendering. ~790 lines.
