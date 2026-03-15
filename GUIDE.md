# User Guide — Theoretical Background

This guide explains the scientific theory behind each analysis mode in the
Life cellular automaton explorer. Each section covers what the mode measures,
why it matters, its formal mathematical description with all symbols defined,
how the implementation works, and where to read more.

## Table of Contents

- [Cellular Automata](#cellular-automata)
- [Entropy Heatmap](#entropy-heatmap)
- [Lyapunov Sensitivity](#lyapunov-sensitivity)
- [Fourier Spectrum](#fourier-spectrum)
- [Fractal Dimension](#fractal-dimension)
- [Frequency Analysis](#frequency-analysis)
- [Wolfram Class Detector](#wolfram-class-detector)
- [Information Flow Field](#information-flow-field)
- [Phase-Space Attractor](#phase-space-attractor)
- [Causal Light Cone](#causal-light-cone)
- [Prediction Surprise](#prediction-surprise)
- [Mutual Information Network](#mutual-information-network)
- [Composite Complexity Index](#composite-complexity-index)
- [Topological Feature Map](#topological-feature-map)
- [Renormalization Group Flow](#renormalization-group-flow)
- [Kolmogorov Complexity](#kolmogorov-complexity)
- [Temperature Field](#temperature-field)
- [Dual-Species Ecosystem](#dual-species-ecosystem)
- [Genetic Rule Explorer](#genetic-rule-explorer)
- [References](#references)

---

## Cellular Automata

A cellular automaton (CA) is a discrete dynamical system consisting of a
regular grid of cells, each in one of a finite number of states. At each time
step, every cell updates its state simultaneously according to a fixed rule
that depends only on the cell's current state and the states of its neighbors.

The **Life-like** family uses a 2D grid with two states (alive/dead) and the
Moore neighborhood (8 surrounding cells). Rules are specified in **B/S notation**:
B lists the neighbor counts that cause a dead cell to be born, and S lists the
counts that let a living cell survive. Conway's Game of Life is B3/S23 — a dead
cell with exactly 3 alive neighbors is born, and a living cell with 2 or 3
alive neighbors survives; all others die.

Despite the simplicity of these rules, Life-like automata can exhibit the full
spectrum of dynamical behavior: convergence to fixed points, periodic
oscillation, chaotic turbulence, and complex emergent computation. This is what
Martin Gardner first brought to public attention in 1970 [1] and what Stephen
Wolfram systematically classified in 2002 [2].

### Formulation

```
σ(x, y, t+1) = f(σ(x, y, t), n(x, y, t))

where:
  σ(x, y, t) ∈ {0, 1}       — state of cell at position (x, y) at generation t
                                (0 = dead, 1 = alive)
  n(x, y, t)                 — number of alive cells in the Moore neighborhood:
                                n = Σ σ(x+dx, y+dy, t)  for (dx, dy) ∈ {-1,0,1}²\{(0,0)}
  f(σ, n)                    — transition function defined by B/S bitmasks:
                                f(0, n) = 1  if  n ∈ B   (birth)
                                f(1, n) = 1  if  n ∈ S   (survival)
                                f(σ, n) = 0  otherwise    (death)
  B ⊆ {0,1,...,8}            — birth set (neighbor counts that create a cell)
  S ⊆ {0,1,...,8}            — survival set (neighbor counts that sustain a cell)

Grid dimensions:
  W = 400                    — grid width (cells)
  H = 200                    — grid height (cells)

Implementation uses double buffering: grid[] holds generation t,
next_grid[] holds generation t+1. All cells update synchronously.
```

| Preset | B | S | Character |
|--------|---|---|-----------|
| Conway | {3} | {2,3} | Classic — gliders, oscillators, still lifes |
| HighLife | {3,6} | {2,3} | Famous replicator pattern |
| Day & Night | {3,6,7,8} | {3,4,6,7,8} | Symmetric — inverted patterns work too |
| Seeds | {2} | {} | Explosive cascading births, all die |
| Diamoeba | {3,5,6,7,8} | {5,6,7,8} | Organic amoeba-like blobs |
| Morley | {3,6,8} | {2,4,5} | Mix of stable and chaotic |
| 2x2 | {3,6} | {1,2,5} | Block-splitting dynamics |
| Maze | {3} | {1,2,3,4,5} | Maze-like corridor growth |
| Coral | {3} | {4,5,6,7,8} | Slow coral-reef growth |
| Anneal | {4,6,7,8} | {3,5,6,7,8} | Noisy regions smooth into blobs |

## Entropy Heatmap

**Toggle: `i`**

### What it measures

Shannon entropy of the local neighborhood around each cell, quantifying the
spatial disorder at cellular resolution.

### Why it matters

Entropy quantifies **disorder** or **uncertainty** in a region. A neighborhood
where all cells are alive (or all dead) has entropy 0 — it is perfectly ordered
and fully predictable. A neighborhood that is 50% alive has maximum entropy
(H = 1.0 bit) — it contains the most "information" in the Shannon sense.

In cellular automata, entropy gradients reveal the boundary between ordered
regions (still lifes, oscillators) and disordered regions (chaotic growth).
These boundaries are often where the most interesting dynamics occur —
gliders emerge from chaos, stable structures nucleate from noise.

### Formulation

```
H(x, y) = -p · log₂(p) - (1 - p) · log₂(1 - p)

where:
  H(x, y) ∈ [0.0, 1.0]     — Shannon entropy of cell (x, y)'s neighborhood (bits)
  p = k / 9                  — fraction of alive cells in the 3×3 Moore neighborhood
  k                          — count of alive cells among the 9 cells in the
                                neighborhood (center cell + 8 adjacent)
  log₂(·)                   — base-2 logarithm; computed as ln(·) × log₂(e)
  log₂(e) = 1.4426950409   — conversion constant from natural log to base-2

Boundary cases:
  H = 0  when p = 0 or p = 1  (all dead or all alive — no uncertainty)
  H = 1  when p = 0.5          (maximum uncertainty — 4 or 5 alive out of 9)

Refresh interval: every 4 generations.
Color map: blue (H ≈ 0) → cyan → green → yellow → red → white (H ≈ 1).
```

### Background

Claude Shannon introduced information entropy in 1948 as a measure of the
average information content in a message [3]. The connection between Shannon
entropy and thermodynamic entropy was established by Jaynes (1957), showing
that statistical mechanics can be derived from information-theoretic principles.
In the context of cellular automata, local entropy serves as a spatial
complexity measure — high entropy regions are disordered, low entropy regions
are structured.

## Lyapunov Sensitivity

**Toggle: `L`**

### What it measures

The local Lyapunov exponent — how fast a small perturbation (a single bit
flip) diverges from the unperturbed system. Positive exponent means chaos
(perturbations grow exponentially), zero means edge of chaos, and negative
means stability (perturbations die out).

### Why it matters

Lyapunov exponents are the standard tool for quantifying **sensitive dependence
on initial conditions** — the defining property of chaos. In a cellular
automaton, a positive local Lyapunov exponent means that flipping one cell in
that region will eventually change the state of many distant cells. A negative
exponent means the flip is absorbed — the system is robust to perturbation
there.

This tells you something fundamentally different from entropy: a region can
have high entropy (disordered) but low Lyapunov sensitivity (the disorder is
stable), or low entropy (ordered) but high sensitivity (a single flip destroys
the order).

### Formulation

```
Algorithm: Shadow-grid perturbation divergence

1. Copy grid → shadow_grid
2. Flip shadow_grid[y][x] for ~N/P randomly chosen cells
3. Evolve both grid and shadow_grid for T steps using identical rules
4. For each cell (x, y), compute local Hamming distance:
     d(x, y) = Σ |grid[y+j][x+i] - shadow[y+j][x+i]|
               for (i, j) ∈ [-2, +2]²
5. Normalize: d_norm(x, y) = d(x, y) / 25
6. Update EMA: λ(x, y) ← α · λ(x, y) + (1 - α) · d_norm(x, y)

where:
  shadow_grid              — perturbed copy of the main grid
  N                        — total number of cells (W × H = 80,000)
  P = 80                   — perturbation sparsity (flip 1 in ~80 cells)
  T = 16                   — shadow evolution horizon (generations)
  d(x, y)                  — Hamming distance in 5×5 window around cell (x, y)
  25                       — window area (5 × 5 cells)
  α = 0.82                 — EMA decay factor (controls temporal smoothing)
  λ(x, y) ∈ [0.0, 1.0]   — local sensitivity estimate for cell (x, y)
                              λ ≈ 0: perturbation absorbed (stable)
                              λ ≈ 0.5: edge of chaos
                              λ ≈ 1: perturbation fills neighborhood (chaotic)

Color map: blue (λ ≈ 0, stable) → yellow (λ ≈ 0.5, edge) → red (λ ≈ 1, chaotic).
```

### Background

Lyapunov exponents were formalized by Aleksandr Lyapunov in his 1892 doctoral
thesis on stability of motion. The modern computational approach — measuring
divergence of nearby trajectories in phase space — was developed by Wolf et al.
(1985) [4]. Applying Lyapunov analysis to cellular automata was pioneered by
Wolfram (1984) and Langton (1990) [5], who used sensitivity as a key criterion
for identifying edge-of-chaos dynamics.

## Fourier Spectrum

**Toggle: `u`**

### What it measures

The 2D spatial frequency spectrum of the grid's density field, computed via
Fast Fourier Transform (FFT). Low frequencies represent large-scale spatial
structure (big clusters, gradients), high frequencies represent fine-grained
detail (isolated cells, noise).

### Why it matters

Fourier analysis reveals **periodic spatial structures** that are invisible in
real space. A grid full of evenly spaced blinkers produces sharp spectral peaks
at the blinker spacing frequency. A random grid produces a flat spectrum. A
grid at a critical phase transition produces a power-law spectrum (1/f noise),
indicating scale-invariant spatial correlations.

The spectral entropy (entropy of the radial power distribution) distinguishes
concentrated spectra (dominated by one frequency — periodic spatial patterns)
from flat spectra (all frequencies present equally — noise or chaos).

### Formulation

```
2D Discrete Fourier Transform (Cooley-Tukey radix-2 FFT):

  F(kx, ky) = Σ_{x=0}^{W'-1} Σ_{y=0}^{H'-1} σ(x, y) · e^{-2πi(kx·x/W' + ky·y/H')}

where:
  σ(x, y) ∈ {0, 1}        — cell state at position (x, y)
  F(kx, ky)                — complex Fourier coefficient at frequency (kx, ky)
  W' = 512                  — FFT width (next power of 2 above W = 400)
  H' = 256                  — FFT height (next power of 2 above H = 200)
  kx ∈ [0, W'-1]           — horizontal frequency index
  ky ∈ [0, H'-1]           — vertical frequency index
  e                         — Euler's number (≈ 2.71828)
  i                         — imaginary unit (√-1)

Power spectrum (log-scaled):
  P(kx, ky) = log(1 + Re(F)² + Im(F)²)

Radial power profile:
  P_rad(r) = mean{ P(kx, ky) : r ≤ √(kx² + ky²) < r + 1 }

where:
  r ∈ {0, 1, ..., R-1}    — radial frequency bin index
  R = 128                   — number of radial bins (FFT_RADIAL_BINS)

Spectral entropy:
  H_spec = -Σ_{r=1}^{R-1} p_r · log₂(p_r)

where:
  p_r = P_rad(r) / Σ P_rad  — normalized radial power at bin r
  DC bin (r = 0) is excluded from entropy calculation

Dominant wavelength:
  λ_dom = W' / r_peak      — spatial wavelength of strongest frequency component
  r_peak = argmax(P_rad)   — radial bin with maximum power

The FFT is computed by applying 1D Cooley-Tukey along rows, then columns.
Zero-padding from 400×200 to 512×256 ensures power-of-2 dimensions.
```

### Background

The Fast Fourier Transform algorithm was published by Cooley and Tukey in 1965
[6], though Gauss had used similar methods as early as 1805. The application of
Fourier analysis to spatial patterns in physics is standard — from X-ray
crystallography (Bragg, 1913) to turbulence analysis (Kolmogorov, 1941). In
cellular automata, the spatial power spectrum distinguishes periodic structures
from chaotic or critical configurations.

## Fractal Dimension

**Toggle: `F`**

### What it measures

The box-counting (Minkowski-Bouligand) fractal dimension D_box of the set of
alive cells. This quantifies how the pattern fills space across scales.

### Why it matters

Integer dimensions describe regular geometry: a line has dimension 1, a filled
square has dimension 2. Fractal objects have **non-integer dimensions** — a
coastline might have D ≈ 1.25, meaning it is "more than a line but less than a
plane." In cellular automata:

- D_box ≈ 0: scattered isolated cells
- D_box ≈ 1.0: linear filaments, glider streams
- D_box ≈ 1.5: branching structures, coastline-like boundaries
- D_box ≈ 2.0: space-filling, dense random field

The fractal dimension is a hallmark of **self-similar structure** — patterns
that look statistically similar at different magnifications. This occurs
naturally at critical phase transitions and in Class IV automata.

### Formulation

```
Box-counting dimension:

  D_box = -lim_{ε→0} [log N(ε) / log ε]

Estimated via linear regression over discrete scales:

  log N(εᵢ) = a + D_box · log(1/εᵢ)     for i = 1, ..., K

where:
  ε                         — box side length (cells)
  N(ε)                      — number of ε×ε boxes containing ≥ 1 alive cell
  K = 7                     — number of scales (FRACTAL_N_SCALES)
  εᵢ ∈ {2, 4, 8, 16, 32, 64, 128}  — box sizes at each scale
  a                         — regression intercept
  D_box                     — fractal dimension (negative slope of regression)

Goodness of fit:
  R² = 1 - SS_res / SS_tot

where:
  SS_res = Σᵢ (yᵢ - ŷᵢ)²  — residual sum of squares
  SS_tot = Σᵢ (yᵢ - ȳ)²   — total sum of squares
  yᵢ = log N(εᵢ)           — observed log-count at scale i
  ŷᵢ = a + b · log(1/εᵢ)  — predicted log-count from regression
  ȳ = mean(yᵢ)             — mean of observed log-counts

Per-cell detail metric:
  detail(x, y) = (1 - fill_ratio) × (1 - scale_index / K)

where:
  fill_ratio                — fraction of cells alive in the box containing (x, y)
  scale_index               — index of the current rendering scale
```

### Background

Benoit Mandelbrot popularized fractal geometry and the concept of fractal
dimension in his 1982 book [7]. The box-counting method is described in detail
by Falconer (2003). In the context of cellular automata, fractal dimension has
been used to characterize the spatial structure of critical and chaotic
configurations, with Class IV automata often producing fractal patterns with
D_box between 1.4 and 1.8.

## Frequency Analysis

**Toggle: `f`**

### What it measures

The oscillation period of each cell, detected by autocorrelation over the last
64 frames of simulation history.

### Why it matters

Many Game of Life patterns are **periodic**: blinkers have period 2, pulsars
have period 3, pentadecathlons have period 15. Frequency analysis reveals the
temporal structure hidden in what appears to be random flickering. It
distinguishes still lifes (period 1) from oscillators (period 2–32) from
chaotic cells (no detectable period).

This is complementary to entropy, which measures spatial disorder at a single
instant. Frequency analysis measures **temporal regularity** across many
generations.

### Formulation

```
Autocorrelation-based period detection:

For each cell (x, y), extract binary state history:
  h(t) = σ(x, y, t)       for t = t_now - D + 1, ..., t_now

Test candidate periods τ = 1, 2, ..., τ_max:
  match(τ) = |{ t : h(t) = h(t - τ),  t ∈ [t_now - D + τ + 1, t_now] }|
  total(τ) = D - τ

Accept period τ if:
  match(τ) × 100 > total(τ) × θ

where:
  h(t) ∈ {0, 1}            — cell's alive/dead state at generation t
  D = 64                    — history depth (frames of timeline to examine)
  τ ∈ {1, 2, ..., 32}     — candidate period (τ_max = D/2 = 32)
  θ = 85                   — match threshold (percent; must exceed 85%)
  match(τ)                 — number of frames where state repeats after τ steps
  total(τ)                 — number of testable frame pairs for period τ

Period assignment:
  period(x, y) = min{ τ : match(τ) × 100 > total(τ) × θ }
               = 255 if no τ passes threshold (chaotic)
               = 0   if cell was never alive in the window (dead)

Special case for τ = 1 (still life):
  Verified by checking that no state change occurred in the entire window.

Color map:
  period 1      → ice blue (still life)
  period 2      → emerald green (blinkers, toads)
  period 3      → gold (pulsars)
  period 4–12   → orange → coral
  period 13–32  → magenta
  period 255    → hot red (chaotic)

Refresh interval: every 8 generations.
```

### Background

Autocorrelation-based period detection is a standard technique in signal
processing, described in any DSP textbook (e.g., Oppenheim & Willsky, 1996).
The classification of Game of Life patterns by period is a foundational
activity in the Life community, catalogued extensively by resources like the
LifeWiki. Periodic structures are the building blocks of more complex
constructions (oscillator-based logic gates, period-doubling cascades).

## Wolfram Class Detector

**Toggle: `C`**

### What it measures

Automatically classifies the automaton's current behavior into one of Stephen
Wolfram's four classes:

| Class | Behavior | Example |
|-------|----------|---------|
| I | Death — all cells die | Empty grid |
| II | Periodic — stable oscillation | Blinkers, blocks |
| III | Chaotic — aperiodic, high entropy | Random-looking turbulence |
| IV | Complex — edge of chaos, long transients | Gliders, guns, computers |

### Why it matters

Wolfram's classification (2002) is the most influential framework for
understanding cellular automaton behavior [2]. Class IV is the most
scientifically interesting — it is where **emergent computation** happens, where
simple local rules produce complex global behavior, where structures like
gliders carry information across space. Langton (1990) showed that Class IV
dynamics occur at a phase transition between order (Class II) and chaos
(Class III), called the **edge of chaos** [5].

### Formulation

```
Feature extraction from current grid state:

  1. Entropy:   E = mean{ H(x, y) }   sampled every 2 cells
  2. Lyapunov:  L = mean divergence from 4 perturbation probes
                    on 80×60 subwindow, evolved 8 steps each
  3. Fractal:   D = quick box-counting at ε ∈ {4, 16, 64}

Classification via weighted scoring:

  Class I score:
    +5.0  if density < 0.001
    +2.0  if E < 0.1
    +1.5  if L < 0.05

  Class II score:
    +1.0  if 0.01 < density < 0.4
    +1.5  if 0.05 < E < 0.5
    +1.5  if autocorrelation > 0.5
    +1.5  if population variance < 0.01

  Class III score:
    +2.0  if E > 0.6
    +2.0  if L > 0.3
    +0.5  if autocorrelation < 0.2

  Class IV score:
    +2.0  if 0.2 < E < 0.6
    +1.5  if 0.1 < L < 0.2

  Classified as: argmax(score_I, score_II, score_III, score_IV)

where:
  E                        — mean local Shannon entropy across sampled cells
  L                        — mean Lyapunov sensitivity from probe perturbations
  D                        — estimated fractal dimension
  density                  — fraction of alive cells (population / (W × H))
  autocorrelation          — population time-series autocorrelation at lag 1
```

### Background

Wolfram introduced his four-class taxonomy in a series of papers in the 1980s,
culminating in *A New Kind of Science* (2002) [2]. The connection between Class
IV and computation was formalized by Cook (2004), who proved that Rule 110 (a
1D Class IV automaton) is Turing-complete. Langton's λ parameter (1990) [5]
provided the first quantitative framework for predicting which rules produce
which class of behavior.

## Information Flow Field

**Toggle: `O`**

### What it measures

**Transfer entropy** between each cell and its cardinal neighbors, producing a
vector field of causal information flow. Unlike correlation (which is
symmetric), transfer entropy measures **directional causation** — it tells you
that cell A is causing changes in cell B, but not vice versa.

### Why it matters

In a cellular automaton, information propagates at a finite speed (at most one
cell per generation). Transfer entropy makes this propagation visible. You can
literally see information flowing along glider streams, radiating from emitters,
and being absorbed at collision sites. The flow field reveals the **causal
architecture** of the automaton — which regions are producers of information
and which are consumers.

Vorticity (curl of the flow field) detects rotational information currents —
swirling causal patterns that arise from oscillator interactions and
interference.

### Formulation

```
Transfer entropy from source X to target Y:

  TE(X→Y) = Σ p(y', y, x) · log₂[ p(y' | y, x) / p(y' | y) ]

summed over all (y', y, x) ∈ {0, 1}³

where:
  X                         — source cell's state time series
  Y                         — target cell's state time series
  x = X(t) ∈ {0, 1}       — source state at time t
  y = Y(t) ∈ {0, 1}       — target state at time t
  y' = Y(t+1) ∈ {0, 1}   — target state at time t+1
  p(y', y, x)              — joint probability estimated from W_flow frames
  p(y' | y, x)             — conditional: probability of future target state
                               given current target and source states
  p(y' | y)                — conditional: probability of future target state
                               given current target state alone
  log₂(·)                  — base-2 logarithm; computed via ln(·) × 1.4426950409

Joint probabilities are estimated by counting occurrences in a sliding window.

Block-level flow vectors (computed on B×B cell blocks):

  TE is computed in 8 directions (N, NE, E, SE, S, SW, W, NW) for each block.

  vx(bx, by) = Σ TE_dir · cos(θ_dir)    — horizontal flow component
  vy(bx, by) = Σ TE_dir · sin(θ_dir)    — vertical flow component

where:
  B = 4                     — spatial block size (FLOW_BLOCK, cells per side)
  W_flow = 16               — temporal window (FLOW_WINDOW, frames)
  (bx, by)                  — block coordinates
  TE_dir                    — transfer entropy in direction dir
  θ_dir                     — angle of direction dir (0°=E, 90°=S, etc.)

Vorticity (discrete curl):
  ω(bx, by) = ∂vy/∂x - ∂vx/∂y
             ≈ [vy(bx+1,by) - vy(bx-1,by)] / 2
             - [vx(bx,by+1) - vx(bx,by-1)] / 2

Color: HSV hue = atan2(vy, vx), brightness = |v|.
Arrow glyphs: rendered at block centers showing net flow direction.
Refresh interval: every 4 generations.
```

### Background

Transfer entropy was introduced by Thomas Schreiber in 2000 [8] as a
model-free, non-parametric measure of directed information transfer between
time series. It generalizes Granger causality (1969) to nonlinear systems.
Lizier et al. (2008, 2012) applied transfer entropy to cellular automata,
showing that it correctly identifies information-carrying structures (gliders,
domain walls) and distinguishes them from the background.

## Phase-Space Attractor

**Toggle: `A`**

### What it measures

The **dynamical attractor** of the system, reconstructed from the 1D population
time series via Takens delay embedding. The trajectory in delay coordinates
reveals whether the dynamics are periodic, quasi-periodic, or chaotic.

### Why it matters

Takens' theorem (1981) [9] guarantees that the topology of the underlying
dynamical attractor can be reconstructed from a single scalar time series, by
plotting (x(t), x(t+τ)) in 2D (or higher-dimensional embeddings). This is one
of the most powerful results in nonlinear dynamics — it means we can visualize
the "shape" of the dynamics from just the population count.

| Attractor shape | Dynamics |
|-----------------|----------|
| Single dot | Fixed point — population converged |
| Closed loop | Limit cycle — periodic population oscillation |
| Torus (thick ring) | Quasi-periodic — multiple incommensurate frequencies |
| Space-filling cloud | Strange attractor — deterministic chaos |

The **correlation dimension D₂** of the attractor gives a fractal dimension
of the dynamics in phase space. D₂ < 1 for periodic, D₂ ≈ 1–2 for
low-dimensional chaos, D₂ > 2 for high-dimensional chaos.

### Formulation

```
Takens delay embedding (d = 2):

  Trajectory: { (P(t), P(t + τ)) }  for t = 1, ..., N - τ

where:
  P(t)                      — total alive population at generation t
  τ                         — embedding delay (auto-tuned, see below)
  d = 2                     — embedding dimension (fixed for 2D display)
  N                         — trajectory length, up to 4096 points (ATTR_MAX_PTS)

Delay auto-tuning via autocorrelation:

  R(k) = (1/M) Σ_{t=1}^{M} [P(t) - P̄] · [P(t+k) - P̄]

  τ = min{ k ∈ [1, 30] : R(k) ≤ 0 }      (first zero-crossing)
      or min{ k : R(k) ≤ R(0) / e }       (first 1/e decay, fallback)

where:
  R(k)                      — autocorrelation at lag k
  P̄                        — mean population over the trajectory
  M = N - k                 — number of valid pairs at lag k
  e ≈ 2.71828               — Euler's number

Phase portrait rasterization:
  Normalize trajectory to [0, 1]² and bin into B_a × B_a grid.
  Density map: count(bx, by) = number of trajectory points in bin (bx, by).
  Color: log-scaled density (dark blue → cyan → yellow → white).

where:
  B_a = 80                  — bins per axis (ATTR_BINS)

Correlation dimension D₂ (Grassberger-Procaccia algorithm):

  C(r) = (2 / (N'·(N'-1))) · |{ (i,j) : ||xᵢ - xⱼ|| < r,  i < j }|

  D₂ = log[C(r_L)] - log[C(r_S)]
       ────────────────────────────
       log(r_L) - log(r_S)

where:
  C(r) ∈ [0, 1]           — correlation integral (fraction of pairs within distance r)
  xᵢ = (P(tᵢ), P(tᵢ+τ)) — embedded point i
  ||·||                    — Euclidean distance in 2D
  N'                       — number of embedded points
  r_S = 0.05               — small radius (normalized)
  r_L = 0.20               — large radius (normalized)
  D₂ ∈ [0.0, 3.0]        — correlation dimension (clamped)
```

### Background

The delay embedding theorem was proved by Floris Takens in 1981 [9], building
on earlier work by Whitney (1936). The Grassberger-Procaccia algorithm for
correlation dimension was published in 1983 [10]. Together, these tools enabled
the experimental discovery of strange attractors in physical systems (weather,
fluid turbulence, heart rhythms, brain dynamics) from time series measurements
alone.

## Causal Light Cone

**Toggle: `9`**

### What it measures

The **backward light cone** (past dependencies) and **forward light cone**
(future influence) of a selected cell. In a cellular automaton with nearest-
neighbor rules, information propagates at most one cell per generation. The
light cone shows which cells *actually* contributed to the selected cell's
state (backward) and which cells it will *actually* influence (forward).

### Why it matters

The concept comes from special relativity, where the light cone defines the
causal structure of spacetime. In a CA, the situation is analogous: the
maximum speed of information is 1 cell/generation (the "speed of light"), and
the light cone separates causally connected events from causally disconnected
ones.

The **fill ratio** (actual influence / theoretical maximum) reveals how much
of the theoretical cone is actually used. A glider fills a narrow wedge of
its forward cone; an explosion fills a large fraction; a still life fills
almost none.

### Formulation

```
Backward light cone (actual causal ancestry):

  Algorithm: BFS through timeline history

  1. Initialize frontier F₀ = { (x₀, y₀) }   at generation t₀
  2. For each step backwards t = t₀-1, t₀-2, ..., t₀-D_max:
       F_{t} = { (x', y') ∈ Moore(F_{t+1}) : σ(x', y', t) = 1 }
       cone_back ← cone_back ∪ F_{t}

where:
  (x₀, y₀)                — selected cell position
  t₀                       — current generation
  D_max = 128              — maximum backward depth (CONE_MAX_DEPTH)
  Moore(F)                 — union of Moore neighborhoods of all cells in set F
  σ(x', y', t)            — cell state at position (x', y') at generation t
  F_{t}                    — frontier set at generation t (alive ancestors)

Forward light cone (theoretical maximum):

  cone_fwd = { (x, y) : max(|x - x₀|, |y - y₀|) ≤ d }

where:
  d                        — forward depth (adjustable with +/-)
  max(|Δx|, |Δy|)        — Chebyshev distance (Moore neighborhood metric)
  The theoretical cone is a diamond (square rotated 45°) expanding by
  1 cell per generation in each direction.

Fill ratio:
  ρ = |cone_back| / |cone_fwd|

where:
  |cone_back|              — number of cells in actual backward cone
  |cone_fwd|               — number of cells in theoretical forward cone
  ρ ∈ [0, 1]              — fraction of cone actually used by causal influence
```

### Background

The light cone concept in cellular automata was formalized by Wolfram (2002)
[2] and is directly analogous to the Minkowski light cone in special relativity
(Einstein, 1905). In the CA context, the discrete speed limit creates a well-
defined causal structure that can be computed exactly, unlike continuous physical
systems where signal propagation speeds are harder to bound.

## Prediction Surprise

**Toggle: `!`**

### What it measures

Per-cell **surprisal** (self-information): how unexpected the cell's actual
transition was, given the learned statistics of its neighborhood configuration.

### Why it matters

Surprisal identifies the **frontier of unpredictability** — cells where what
actually happens is not what the system's own recent history would predict.
This is different from entropy (which measures spatial disorder) and Lyapunov
(which measures perturbation sensitivity). A random field has high entropy but
low surprise (random *is* the prediction). Surprise highlights specific events
that violate learned patterns.

In practice, high-surprise cells cluster at:
- Leading edges of gliders (the glider tip is "surprising" given local history)
- Phase boundaries between ordered and disordered regions
- The first few generations after a rule mutation
- Collision sites where two structures interact unpredictably

### Formulation

```
Surprisal (self-information):

  I(x, y) = -log₂ P(outcome | config)

where:
  I(x, y) ∈ [0, I_max]   — surprisal in bits for cell (x, y)
  I_max = 10.0             — maximum surprisal cap (bits)
  outcome ∈ {0, 1}        — actual next state of cell (x, y)
  config                   — 9-bit encoding of Moore neighborhood + center state

Neighborhood encoding:
  config = Σ_{i=0}^{8} σᵢ · 2ⁱ    (9-bit hash, 512 possible configurations)

where:
  σ₀ = σ(x, y, t)         — center cell state
  σ₁..σ₈                  — states of 8 Moore neighbors (fixed enumeration order)

Transition statistics (sliding window):
  For each config c ∈ {0, ..., 511}:
    count_alive[c]  — times config c led to alive outcome in last W_s frames
    count_total[c]  — times config c was observed in last W_s frames

  P(alive | c) = clamp(count_alive[c] / count_total[c],  ε,  1 - ε)
  P(dead  | c) = 1 - P(alive | c)

where:
  W_s = 32                  — sliding window size (SURP_WINDOW, frames)
  ε = 0.001                — probability clipping bound (prevents log(0))
  Hash table size = 8192   — SURP_HASH_SIZE (power of 2, with masking)

  If count_total[c] = 0 (unseen configuration):
    I(x, y) = 1.0 bit     — default surprise for unknown neighborhoods

Color map: dark blue (I ≈ 0, predictable) → cyan → yellow → red/white (I ≈ 10, surprising).
Refresh interval: every 4 generations.
```

### Background

Surprisal (self-information) was defined by Shannon (1948) [3] as the
fundamental unit of information: the surprise of an event is the logarithm of
one over its probability. Bayesian surprise in dynamical systems was further
developed by Itti and Baldi (2009). In the cellular automata context, surprisal
identifies cells that are not well-predicted by their recent local statistics —
these are the cells where "interesting things are happening."

## Mutual Information Network

**Toggle: `@`**

### What it measures

**Mutual information** between coarse-grained regions of the grid, revealing
long-range coupling structure. The grid is divided into blocks, and MI is
computed between all block-pair population time series over the 256-frame
history.

### Why it matters

All the other analyzers operate at the single-cell or local-neighborhood level.
Mutual information reveals **mesoscale structure** — how distant regions of the
grid are statistically coupled over time. Two blocks with high MI are
synchronized: knowing the state of one tells you about the state of the other.

This reveals:
- **Glider communication channels** — blocks connected by glider streams
- **Oscillator entrainment** — distant oscillators that have phase-locked
- **Zone boundary effects** — coupling across rule boundaries
- **Portal coupling** — wormhole-mediated long-range correlation

The **clustering coefficient** of the MI network measures how much the coupling
forms triangles (cliques) vs isolated pairs, indicating whether the system has
hierarchical modular structure.

### Formulation

```
Mutual information between blocks A and B:

  MI(A; B) = Σ_{a=0}^{Q-1} Σ_{b=0}^{Q-1} p(a, b) · log₂[ p(a, b) / (p(a) · p(b)) ]

where:
  A, B                     — two grid blocks identified by index
  a, b ∈ {0, ..., Q-1}   — quantized population bins for blocks A, B
  Q = 8                    — number of quantization bins (MI_BINS)
  p(a, b)                  — joint probability: fraction of frames where block A
                              is in bin a AND block B is in bin b
  p(a)                     — marginal probability: fraction of frames where
                              block A is in bin a
  p(b)                     — marginal probability for block B
  log₂(·)                 — base-2 logarithm (MI measured in bits)

Grid partitioning:
  Block size: B_w × B_h = 20 × 20 cells per block
  Grid of blocks: G_w × G_h = 20 × 10 blocks (MI_GW × MI_GH)
  Total blocks: N_blk = 200 (MI_NBLK)

where:
  B_w = 20                  — block width in cells (MI_BLK_W)
  B_h = 20                  — block height in cells (MI_BLK_H)
  G_w = W / B_w = 400/20 = 20  — blocks across grid width
  G_h = H / B_h = 200/20 = 10  — blocks across grid height

Block population quantization:
  pop(block, t) = count of alive cells in block at generation t
  bin(block, t) = floor(pop(block, t) / max_pop × (Q - 1))

where:
  max_pop = B_w × B_h = 400  — maximum possible population per block

Network construction:
  Compute MI for all N_blk × (N_blk - 1) / 2 ≈ 19,900 block pairs.
  Keep top K = 40 strongest couplings (MI_TOP_N).
  Coupling threshold: MI > 0.1 bits.

Clustering coefficient:
  C = (3 × triangles) / (connected triples)

where:
  triangles               — number of closed triangles in the MI network
                             (three blocks all mutually coupled above threshold)
  connected triples       — number of paths of length 2

Rendered as Bresenham lines between block centers.
Color: dark blue (low MI) → cyan → green → yellow → magenta → white (high MI).
Refresh interval: every 8 generations. Requires ≥ 16 frames of history.
```

### Background

Mutual information is a core concept in information theory, defined by Shannon
(1948) [3] and thoroughly treated by Cover and Thomas (2005) [11]. Its
application to complex systems as a network analysis tool was developed in the
context of neuroscience (Tononi et al., 1994), climate science (Donges et al.,
2009), and cellular automata (Lizier et al., 2008). The MI network provides a
model-free measure of coupling that works for nonlinear and non-Gaussian
relationships, unlike correlation.

## Composite Complexity Index

**Toggle: `#`**

### What it measures

A per-cell **edge-of-chaos score** (0.0–1.0) that fuses four existing
analyzers into a single complexity metric.

### Why it matters

The "edge of chaos" is the phase transition between ordered (Class II) and
chaotic (Class III) dynamics. This is where **computation** happens in cellular
automata — where structures are stable enough to carry information but dynamic
enough to process it. Langton (1990) [5] identified this transition as the
region where complex behavior (Class IV) emerges.

The composite score brings this concept down to **per-cell spatial resolution**.
The concave term `4·raw·(1-raw)` peaks at raw = 0.5, specifically rewarding
cells that show **balanced moderate signals** across all four metrics — the
hallmark of edge-of-chaos dynamics. Pure chaos scores high on all four
components (raw ≈ 1.0) but the concave transform pushes its score down. Pure
order scores low on everything (raw ≈ 0.0) and is also pushed down. Only the
balanced middle gets the highest score.

The gold band (0.4–0.7) marks cells where gliders, long transients, and
localized computation tend to live.

### Formulation

```
Composite complexity score:

  raw(x, y) = w_E · E(x,y) + w_L · L(x,y) + w_S · S(x,y) + w_F · F(x,y)

  C(x, y) = α · [4 · raw · (1 - raw)] + (1 - α) · raw

where:
  C(x, y) ∈ [0.0, 1.0]   — composite complexity score for cell (x, y)
  raw ∈ [0.0, 1.0]        — weighted linear combination of component scores
  E(x, y) ∈ [0, 1]        — normalized Shannon entropy (from entropy overlay)
  L(x, y) ∈ [0, 1]        — normalized Lyapunov sensitivity
  S(x, y) ∈ [0, 1]        — normalized prediction surprisal (capped at 1.0)
  F(x, y) ∈ [0, 1]        — normalized frequency score:
                              1.0 if chaotic (period = 255)
                              (period - 1) / 31 if periodic
                              0.0 if still or dead
  w_E = 0.30               — entropy weight
  w_L = 0.25               — Lyapunov weight
  w_S = 0.25               — surprise weight
  w_F = 0.20               — frequency weight
  α = 0.70                 — concave edge-boost blend factor

The concave term 4·raw·(1-raw):
  = 0.0 at raw = 0  (dead/simple)
  = 1.0 at raw = 0.5 (maximum edge-of-chaos score)
  = 0.0 at raw = 1  (fully chaotic)

Classification thresholds:
  C < 0.2    → simple (dead or fully ordered)
  0.4 ≤ C ≤ 0.7  → edge-of-chaos band (complex)
  C > 0.8    → fully chaotic

Color map: deep blue (C ≈ 0) → teal-green → bright gold (C ≈ 0.5) → red (C ≈ 1).
Refresh interval: every 4 generations.
```

### Background

The edge-of-chaos hypothesis was introduced by Langton (1990) [5] and
elaborated by Kauffman (1993). The idea that computation requires a balance
between order and chaos goes back to Turing (1952) and was formalized in the
cellular automata context by Wolfram (2002) [2]. The composite index used here
is inspired by the "multi-information" complexity measures of Tononi et al.
(1994) and the "statistical complexity" of Crutchfield and Young (1989).

## Topological Feature Map

**Toggle: `$`**

### What it measures

**Betti numbers** of the pattern: β₀ (number of connected components) and β₁
(number of enclosed holes).

- **β₀**: each isolated cluster of alive cells is a separate connected
  component. A grid with 50 separate blocks has β₀ = 50.
- **β₁**: each dead-cell region completely surrounded by alive cells is a
  "hole." A beehive has β₁ = 1 (one hole). A pond has β₁ = 1.

### Why it matters

Betti numbers provide a **purely geometric** lens on the pattern, fundamentally
different from all information-theoretic overlays. They detect:

- **Percolation transitions** — when many small components suddenly merge into
  one giant connected cluster. This is a classic phase transition studied
  extensively in statistical physics.
- **Topological genus** — how many holes structures contain. Complex structures
  like the eater and the pond have characteristic topological signatures.
- **Fragmentation dynamics** — whether the system is coalescing (β₀ decreasing)
  or fragmenting (β₀ increasing) over time.

The sparkline history of β₀ and β₁ reveals topological phase transitions that
are invisible to other analyzers.

### Formulation

```
Connected component labeling (β₀):

  Algorithm: BFS flood fill with 4-connectivity

  1. For each unvisited alive cell (x, y):
       n_comp ← n_comp + 1
       BFS from (x, y) visiting {up, down, left, right} neighbors
       Label all reachable alive cells with component ID = n_comp
  2. β₀ = n_comp

where:
  4-connectivity           — neighbors are {(x±1,y), (x,y±1)} only
                              (not diagonal — this matches the standard
                              topological definition on a square lattice)
  n_comp ≤ 1024           — maximum tracked components (TOPO_MAX_COMPONENTS)

Component coloring:
  hue(c) = (c × 137) mod 360

where:
  c                        — component index (1-based)
  137°                    — golden angle (≈ 360° / φ², where φ = golden ratio)
                              provides maximum visual separation between
                              adjacent component colors

Enclosed hole detection (β₁):

  Algorithm: BFS flood fill of dead cells

  1. For each unvisited dead cell (x, y):
       BFS from (x, y) visiting {up, down, left, right} dead neighbors
       If the region does NOT touch any grid boundary
         AND region size < (W × H) / 2:
           n_holes ← n_holes + 1
           Mark all cells in region as "hole"
  2. β₁ = n_holes

where:
  boundary                 — cells at x = 0, x = W-1, y = 0, or y = H-1
  W × H / 2 = 40,000     — maximum hole size (prevents labeling the
                              entire dead background as a "hole")

History tracking:
  β₀ and β₁ stored in ring buffers of length L_hist = 64 (TOPO_HIST_LEN)
  for sparkline display.

Holes highlighted in magenta/violet overlay.
Refresh interval: every 4 generations.
```

### Background

Betti numbers are fundamental invariants in algebraic topology, named after
Enrico Betti (1871) and formalized by Poincaré (1895). Their computational
application to spatial data is covered by Edelsbrunner and Harer (2010) [12].
Percolation theory — the study of connected components in random lattices — was
founded by Broadbent and Hammersley (1957) and is directly relevant to
understanding how cellular automaton patterns fragment and coalesce.

## Renormalization Group Flow

**Toggle: `%`**

### What it measures

How spatial structure distributes across **length scales**, via real-space
renormalization group (RG) analysis. The grid is coarse-grained at three scales
(2×2, 4×4, 8×8 blocks) using majority-rule decimation, and the density retained
at each scale reveals where structure lives.

### Why it matters

The renormalization group is one of the deepest ideas in 20th-century physics.
It explains **universality** — why completely different physical systems (magnets,
fluids, polymers) exhibit the same behavior near phase transitions. The key
insight: at a critical point, the system looks statistically the same at all
scales (scale invariance).

In a cellular automaton:
- **Fine-dominated** patterns (cyan) have structure only at small scales — dust,
  isolated cells, noise. Under coarse-graining, the density drops rapidly.
- **Coarse-dominated** patterns (magenta) have large-scale coherent structure
  that persists under coarse-graining.
- **Scale-invariant** patterns (white) retain similar density at all scales —
  this is the hallmark of **criticality**, the phase transition between order
  and chaos.

The **criticality score** (0.0–1.0) measures the flatness of the density
spectrum across scales. A score near 1.0 indicates scale-invariant (critical)
behavior.

### Formulation

```
Real-space renormalization group via majority-rule block decimation:

For each scale sₖ ∈ {s₁, s₂, s₃}:

  1. Partition grid into non-overlapping sₖ × sₖ blocks
  2. For each block B at position (bx, by):
       alive_count = Σ σ(x, y)  for (x, y) ∈ B
       σ_coarse(bx, by, k) = 1  if alive_count > sₖ² / 2   (majority rule)
                            = 0  otherwise
  3. ρₖ = mean{ σ_coarse(bx, by, k) }   — coarse-grained density at scale k

where:
  K = 3                     — number of RG scales (RG_N_SCALES)
  s₁ = 2, s₂ = 4, s₃ = 8  — block sizes at each scale (cells per side)
  sₖ² / 2                  — majority threshold:
                              s₁: > 2 of 4 cells
                              s₂: > 8 of 16 cells
                              s₃: > 32 of 64 cells
  σ_coarse(bx, by, k)      — coarse-grained "super-cell" state at scale k
  ρₖ ∈ [0, 1]             — density retained at scale k

Per-cell scale classification:
  Δₖ = ρₖ - ρₖ₊₁          — density drop from scale k to scale k+1

  dominant scale = argmax(Δ₁, Δ₂, Δ₃)

  Classification:
    Δ₁ largest  → fine-dominated   (cyan)   — structure at scale 2, lost at 4
    Δ₂ largest  → meso-dominated   (yellow) — structure at scale 4, lost at 8
    Δ₃ largest  → coarse-dominated (magenta) — structure persists to scale 8
    All Δ similar → scale-invariant (white)  — critical behavior

Per-cell scale invariance:
  inv(x, y) = 1 - √(var(ρ₁, ρ₂, ρ₃))

where:
  var(·)                   — sample variance of the three scale densities
  inv ∈ [0, 1]            — 1.0 = perfectly scale-invariant, 0.0 = one scale dominates

Global criticality score:
  C_rg = 1 - max |ρᵢ - ρⱼ|    for all pairs i, j ∈ {1, 2, 3}

Color map: cyan (fine) → yellow (meso) → magenta (coarse) → white (invariant).
Refresh interval: every 4 generations.
```

### Background

The renormalization group was invented by Leo Kadanoff (1966) [13] as a
conceptual framework for understanding critical phenomena in the Ising model,
and developed into a quantitative tool by Kenneth Wilson (1971) [14], who
received the 1982 Nobel Prize in Physics for this work. The real-space
majority-rule decimation used here follows Kadanoff's original block-spin
approach. Israeli and Goldenfeld (2006) applied RG methods to cellular
automata, showing that coarse-graining can reduce complex rules to simpler
effective rules.

## Kolmogorov Complexity

**Toggle: `^`**

### What it measures

The **algorithmic complexity** of each cell's local neighborhood — how
compressible its pattern is. A region with a simple, regular pattern (e.g., a
row of alternating cells) has low Kolmogorov complexity. A region with no
discernible pattern has high complexity.

### Why it matters

Kolmogorov complexity (1965) [15] is the gold standard measure of the
"intrinsic information content" of a string — it is the length of the shortest
program that produces the string. Unlike Shannon entropy (which depends on a
probability distribution), Kolmogorov complexity is a property of the
individual object.

In cellular automata, Kolmogorov complexity distinguishes:
- **Ordered regions** (still lifes, oscillators): highly compressible, low K
- **Random regions** (chaotic noise): incompressible, high K
- **Complex regions** (gliders, guns): moderate K — structured but not trivially
  so. This is the most interesting regime.

The relationship between complexity and computation is deep: Turing showed that
deciding whether a string has low Kolmogorov complexity is undecidable. In
practice, compression algorithms provide upper bounds.

### Formulation

```
LZ77-style sliding window compression as Kolmogorov complexity estimator:

For each cell (x, y), extract neighborhood block:
  block = { σ(x + dx, y + dy) : dx ∈ [-B_k/2, B_k/2), dy ∈ [-B_k/2, B_k/2) }
  Serialized as a bitstring s of length B_k² bits (row-major order).

Compression algorithm:
  Initialize: pos = 0, output_bits = 0

  While pos < |s|:
    Search for longest match in s[max(0, pos-W_k) .. pos-1]:
      match_len = length of longest substring starting at pos
                  that appears in the search window
      match_off = offset to the match position

    If match_len ≥ M_k:
      Emit back-reference: 1 bit (flag=1) + 5 bits (offset) + 4 bits (length)
      output_bits += 10
      pos += match_len
    Else:
      Emit literal: 1 bit (flag=0) + 1 bit (raw value)
      output_bits += 2
      pos += 1

  K_est(x, y) = output_bits / input_bits

where:
  B_k = 16                  — neighborhood block size (KC_BLOCK, cells per side)
  B_k² = 256               — input size in bits
  W_k = 64                  — search window size (KC_WINDOW, bits lookback)
  M_k = 3                   — minimum match length (KC_MIN_MATCH)
  |s| = 256                — bitstring length (16 × 16)
  K_est ∈ [0.0, 1.0]      — normalized complexity estimate (compression ratio)
                              0.0 = fully compressible (ordered)
                              1.0 = incompressible (random)

Classification thresholds:
  K_est < 0.3   → simple / ordered
  0.3 ≤ K_est ≤ 0.7  → structured / moderate complexity
  K_est > 0.7   → complex / random

Color map: blue (K ≈ 0, compressible) → gold (K ≈ 0.5, structured) → red (K ≈ 1, incompressible).
History ring buffer: L_hist = 64 frames (KC_HIST_LEN).
Refresh interval: every 4 generations.
```

### Background

Kolmogorov complexity was independently introduced by Ray Solomonoff (1960),
Andrey Kolmogorov (1965) [15], and Gregory Chaitin (1966). The LZ77 compression
algorithm used as a practical estimator was published by Ziv and Lempel in 1977
[16]. The connection between compression and complexity in cellular automata was
explored by Zenil et al. (2012) and Gauvrit et al. (2014), who showed that
compression-based complexity measures correlate with other measures of
interesting dynamics.

## Temperature Field

**Toggle: `X`**

### What it measures

A per-cell stochastic noise field that introduces **thermal fluctuations** into
the deterministic CA rules. Each cell has a local temperature T ∈ [0, 1] plus a
global temperature offset. At each time step, a cell's deterministic
birth/death decision is flipped with probability T.

### Why it matters

Deterministic cellular automata are isolated systems — they cannot undergo true
**phase transitions** because there is no external control parameter to tune.
Adding temperature creates a system analogous to the Ising model in statistical
mechanics, where the competition between deterministic rules (energy
minimization) and stochastic noise (entropy maximization) drives the system
through ordered, critical, and disordered phases.

At low temperature, the CA rules dominate — still lifes are stable, oscillators
are regular. At high temperature, noise dominates — cells flip randomly. At a
critical intermediate temperature, the system sits at the **phase transition**
between order and chaos, exhibiting long-range correlations and scale-invariant
fluctuations.

By painting hot and cold regions, you can create **thermal gradients** that
drive the system into different phases in different spatial regions, with the
phase boundary visible as a line of enhanced fluctuations.

### Formulation

```
Stochastic cellular automaton with temperature:

  σ_det(x, y, t+1) = f(σ(x, y, t), n(x, y, t))    — deterministic rule output

  T_eff(x, y) = clamp(T_global + T_local(x, y),  0,  1)

  σ(x, y, t+1) = σ_det  with probability  1 - T_eff
                = 1 - σ_det  with probability  T_eff

Equivalently:
  if rand() < T_eff(x, y):
    σ(x, y, t+1) = 1 - σ_det(x, y, t+1)       — flip the deterministic outcome
  else:
    σ(x, y, t+1) = σ_det(x, y, t+1)            — keep the deterministic outcome

where:
  σ_det                    — next state from deterministic B/S rules
  T_global ∈ [0.0, 1.0]  — global temperature offset (adjustable with {/})
  T_local(x, y) ∈ [0.0, 1.0]  — per-cell painted temperature (temp_grid)
  T_eff ∈ [0.0, 1.0]     — effective temperature after clamping
  clamp(v, lo, hi)        — max(lo, min(v, hi))
  rand() ∈ [0.0, 1.0)   — uniform random number

Phase regimes:
  T_eff ≈ 0.0  → deterministic (CA rules dominate, ordered phase)
  T_eff ≈ 0.5  → critical (phase transition, long-range correlations)
  T_eff ≈ 1.0  → fully stochastic (random flipping, disordered phase)

Controls:
  Left-click: paint T_local = T_brush (hot)
  Right-click: paint T_local = 0 (cold)
  +/-: adjust T_brush
  {/}: adjust T_global
  Scroll: adjust brush radius
```

### Background

The connection between cellular automata and statistical mechanics was explored
by Domany and Kinzel (1984) and Wolfram (1983). Stochastic cellular automata
are equivalent to interacting particle systems in probability theory (Liggett,
1985). The Ising model, which the temperature field emulates, was solved exactly
in 2D by Onsager (1944) and exhibits one of the best-understood critical phase
transitions in physics.

## Dual-Species Ecosystem

**Toggle: `a`**

### What it measures

Two coexisting populations (species A and B) on the same grid, each following
its own B/S rules, with a tunable **inter-species interaction coefficient** α
ranging from -1.0 (hostile) to +1.0 (cooperative).

### Why it matters

Single-species cellular automata are "monochrome" — all cells are identical.
Adding a second species with different rules and a coupling parameter creates a
system analogous to **predator-prey dynamics**, **competitive exclusion**, and
**mutualism** in ecology.

| α value | Ecological analogy |
|---------|-------------------|
| -1.0 | Strong competition / predation — cross-species neighbors hurt |
| 0.0 | Neutral coexistence — species ignore each other |
| +1.0 | Full mutualism — cross-species neighbors help equally |

This creates emergent behaviors impossible in single-species CA:
- Territorial boundaries and frontiers
- Population oscillations (Lotka-Volterra cycles)
- Competitive exclusion (one species drives the other to extinction)
- Symbiotic structures (mixed-species configurations that neither could form alone)

### Formulation

```
Dual-species neighbor counting and update:

For cell (x, y) with species s ∈ {A, B}:

  n_same(x, y) = count of alive Moore neighbors with species = s
  n_cross(x, y) = count of alive Moore neighbors with species ≠ s

  n_eff(x, y) = n_same + α · n_cross

  Survival: σ(x, y, t+1) = 1  if  round(n_eff) ∈ S_s
  Death:    σ(x, y, t+1) = 0  otherwise

For dead cell (x, y) (birth arbitration):

  For each candidate species s ∈ {A, B}:
    n_same_s = count of alive neighbors with species s
    n_cross_s = count of alive neighbors with species ≠ s
    n_eff_s = n_same_s + α · n_cross_s

    born_s = (round(n_eff_s) ∈ B_s)

  If born_A and born_B:
    species = A  if n_same_A > n_same_B    (majority wins)
    species = B  if n_same_B > n_same_A
    species = random(A, B)  if tie
  Else if born_A: species = A
  Else if born_B: species = B

where:
  α ∈ [-1.0, +1.0]        — inter-species interaction coefficient
                              α < 0: hostile (cross-species neighbors reduce n_eff)
                              α = 0: neutral (species ignore each other)
                              α > 0: cooperative (cross-species neighbors help)
  n_same                    — count of same-species alive neighbors (0–8)
  n_cross                   — count of other-species alive neighbors (0–8)
  n_eff                     — effective weighted neighbor count
  round(·)                 — rounding to nearest integer
  B_s ⊆ {0,...,8}         — birth set for species s
  S_s ⊆ {0,...,8}         — survival set for species s

Default species rules:
  Species A: B_A = {3}, S_A = {2, 3}           — Conway (B3/S23)
             birth bitmask = 0x008, survival bitmask = 0x00C
  Species B: B_B = {3, 6}, S_B = {2, 3}       — HighLife (B36/S23)
             birth bitmask = 0x048, survival bitmask = 0x00C

Interaction step: α adjusted with {/} in increments of 0.1.
Brush species toggled with 6. Randomize assigns 50/50 species.
```

### Background

The dual-species model is inspired by the **Lotka-Volterra equations** (Lotka,
1925 [17]; Volterra, 1926), which describe predator-prey population dynamics in
continuous time. Discrete-space multi-species models on lattices were studied by
Tainaka (1988) and Szabó and Fáth (2007) in the context of evolutionary game
theory. The competitive exclusion principle (Gause, 1934) predicts that two
species competing for the same niche cannot stably coexist — this explorer lets
you test whether spatial structure (which the mean-field Lotka-Volterra model
ignores) can sustain coexistence.

## Genetic Rule Explorer

**Toggle: `G`**

### What it measures

Automated search for "interesting" cellular automaton rules via a **genetic
algorithm** that evolves a population of candidate rulesets across generations.

### Why it matters

The space of Life-like rules is vast: 2⁹ possible birth conditions × 2⁹
possible survival conditions = 262,144 distinct rules. Most are boring (all
cells die immediately, or the grid fills instantly). The interesting rules —
the ones with gliders, oscillators, complex transients — are rare. A genetic
algorithm can search this space efficiently by breeding and mutating rules that
show promising behavior.

### Formulation

```
Genetic algorithm for rule-space search:

Genome representation:
  g = (B, S)               — 18-bit genome: 9 birth bits + 9 survival bits
  B ⊆ {0,...,8}           — birth set encoded as 9-bit bitmask
  S ⊆ {0,...,8}           — survival set encoded as 9-bit bitmask
  Total search space: 2¹⁸ = 262,144 possible rules

Population:
  P = { g₁, g₂, ..., g_N }    with N = 20 (GENPOP_SIZE)

Fitness evaluation (gene_evaluate):

  1. Initialize W_g × H_g grid with R-pentomino at center + 10% random fill
  2. Evolve for T_g steps using candidate rule g
  3. Record population time series pop(t) for t = 1, ..., T_g

  fitness(g) = f_density + f_variance + f_period + f_longevity + f_complexity

where:
  W_g = 80                  — evaluation grid width (GENEVAL_W)
  H_g = 60                  — evaluation grid height (GENEVAL_H)
  T_g = 200                 — evaluation steps (GENEVAL_STEPS)

  f_density:
    +20.0  if 5% < final_density < 50%
    0      otherwise

  f_variance:
    cv = stddev(pop) / mean(pop)       — coefficient of variation
    +30.0 × (1 - |cv - 0.15| / 0.5)   if 0.02 < cv < 0.5
    0      otherwise

  f_period:
    For each detected period p ∈ {1, ..., 12}:
      +5.0 per matching period (up to +60.0 total)

  f_longevity:
    valid = count of steps where 0 < pop(t) < W_g × H_g
    +15.0 × (valid / T_g)

  f_complexity:
    distinct = |{ pop(t) : t ∈ [T_g - 50, T_g] }|
    +0.5 × distinct          (up to +20.0)

Evolution operators:

  Selection (elitism):
    Sort population by fitness descending.
    Keep top E = 5 (GENBEST_SIZE) as elite.

  Crossover (uniform):
    For each new offspring:
      Select parents p₁, p₂ randomly from top E
      For each bit position i ∈ {0, ..., 17}:
        offspring[i] = p₁[i] with probability 0.5
                     = p₂[i] with probability 0.5

  Mutation:
    For each offspring:
      Flip m randomly chosen bits, where m ∈ [1, strength]
      strength scales with generation number

Press G to breed next generation, 1-5 to load a discovered rule.
```

### Background

Genetic algorithms were introduced by John Holland (1975) [18] as a
computational model of adaptation inspired by Darwinian natural selection.
Applying genetic algorithms to search the space of cellular automaton rules
was pioneered by Mitchell, Crutchfield, and Hraber (1994), who evolved rules
for density classification tasks. The fitness function used here favors
"interesting" behavior in the Wolfram Class IV sense — rules that produce
sustained complex dynamics without dying out or filling the grid.

---

## References

[1] M. Gardner, "Mathematical Games: The fantastic combinations of John
Conway's new solitaire game 'life'," *Scientific American*, vol. 223,
no. 4, pp. 120–123, October 1970.
Available: https://web.stanford.edu/class/sts145/Library/life.pdf

[2] S. Wolfram, *A New Kind of Science*. Wolfram Media, 2002.
Available: https://www.wolframscience.com/nks/

[3] C. E. Shannon, "A mathematical theory of communication," *The Bell
System Technical Journal*, vol. 27, no. 3, pp. 379–423, 1948.
DOI: [10.1002/j.1538-7305.1948.tb01338.x](https://doi.org/10.1002/j.1538-7305.1948.tb01338.x)

[4] A. Wolf, J. B. Swift, H. L. Swinney, and J. A. Vastano, "Determining
Lyapunov exponents from a time series," *Physica D: Nonlinear Phenomena*,
vol. 16, no. 3, pp. 285–317, 1985.
DOI: [10.1016/0167-2789(85)90011-9](https://doi.org/10.1016/0167-2789(85)90011-9)

[5] C. G. Langton, "Computation at the edge of chaos: Phase transitions
and emergent computation," *Physica D: Nonlinear Phenomena*, vol. 42,
no. 1–3, pp. 12–37, 1990.
DOI: [10.1016/0167-2789(90)90064-V](https://doi.org/10.1016/0167-2789(90)90064-V)

[6] J. W. Cooley and J. W. Tukey, "An algorithm for the machine
calculation of complex Fourier series," *Mathematics of Computation*,
vol. 19, no. 90, pp. 297–301, 1965.
DOI: [10.1090/S0025-5718-1965-0178586-1](https://doi.org/10.1090/S0025-5718-1965-0178586-1)

[7] B. B. Mandelbrot, *The Fractal Geometry of Nature*. W. H. Freeman,
1982. ISBN: 978-0-7167-1186-5.

[8] T. Schreiber, "Measuring information transfer," *Physical Review
Letters*, vol. 85, no. 2, pp. 461–464, 2000.
DOI: [10.1103/PhysRevLett.85.461](https://doi.org/10.1103/PhysRevLett.85.461)

[9] F. Takens, "Detecting strange attractors in turbulence," in *Lecture
Notes in Mathematics*, vol. 898, Springer, 1981, pp. 366–381.
DOI: [10.1007/BFb0091924](https://doi.org/10.1007/BFb0091924)

[10] P. Grassberger and I. Procaccia, "Characterization of strange
attractors," *Physical Review Letters*, vol. 50, no. 5, pp. 346–349,
1983.
DOI: [10.1103/PhysRevLett.50.346](https://doi.org/10.1103/PhysRevLett.50.346)

[11] T. M. Cover and J. A. Thomas, *Elements of Information Theory*,
2nd ed. Wiley, 2005. ISBN: 978-0-471-24195-9.
DOI: [10.1002/047174882X](https://doi.org/10.1002/047174882X)

[12] H. Edelsbrunner and J. L. Harer, *Computational Topology: An
Introduction*. American Mathematical Society, 2010.
ISBN: 978-0-8218-4925-5.
DOI: [10.1090/mbk/069](https://doi.org/10.1090/mbk/069)

[13] L. P. Kadanoff, "Scaling laws for Ising models near T_c," *Physics
Physique Fizika*, vol. 2, no. 6, pp. 263–272, 1966.
DOI: [10.1103/PhysicsPhysiqueFizika.2.263](https://doi.org/10.1103/PhysicsPhysiqueFizika.2.263)

[14] K. G. Wilson, "Renormalization group and critical phenomena. I.
Renormalization group and the Kadanoff scaling picture," *Physical
Review B*, vol. 4, no. 9, pp. 3174–3183, 1971.
DOI: [10.1103/PhysRevB.4.3174](https://doi.org/10.1103/PhysRevB.4.3174)

[15] A. N. Kolmogorov, "Three approaches to the quantitative definition
of information," *International Journal of Computer Mathematics*,
vol. 2, no. 1–4, pp. 157–168, 1968.
DOI: [10.1080/00207166808803030](https://doi.org/10.1080/00207166808803030)

[16] J. Ziv and A. Lempel, "A universal algorithm for sequential data
compression," *IEEE Transactions on Information Theory*, vol. 23,
no. 3, pp. 337–343, 1977.
DOI: [10.1109/TIT.1977.1055714](https://doi.org/10.1109/TIT.1977.1055714)

[17] A. J. Lotka, *Elements of Physical Biology*. Williams & Wilkins,
1925. Reprinted as *Elements of Mathematical Biology*, Dover, 1956.

[18] J. H. Holland, *Adaptation in Natural and Artificial Systems*.
University of Michigan Press, 1975. Reprinted by MIT Press, 1992.
DOI: [10.7551/mitpress/1090.001.0001](https://doi.org/10.7551/mitpress/1090.001.0001)
