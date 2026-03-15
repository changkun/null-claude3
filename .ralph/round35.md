# Round 35: Prediction Surprise Field

## Goal
Build a per-cell surprisal heatmap that shows where the automaton's state transitions are statistically unexpected, revealing the boundary between predictable and chaotic dynamics.

## What Was Built
**Prediction Surprise Field** (`!` key) — a real-time overlay that accumulates transition statistics (neighborhood configuration → next-state outcome) over a 32-frame sliding window, then colors each cell by its information-theoretic surprisal: -log₂ P(actual_outcome | neighborhood).

### Key Features
- **Transition frequency tracking**: Hash table maps 9-bit Moore neighborhood configs to observed outcome frequencies
- **Per-cell surprisal**: Each cell colored by how unexpected its current state is given its neighborhood's track record
- **32-frame sliding history**: Ring buffer of grid snapshots provides temporal depth without excessive memory
- **Statistics panel**: Mean/max surprisal, count of predictable (< 0.1 bits) vs surprising (> 0.8 bits) cells
- **4-segment color gradient**: Dark blue (predictable) → cyan → yellow → red/white (surprising)
- **Auto-refresh every 4 generations**: Responsive without being computationally expensive

### Technical Details
- **Hash table**: 8192-slot table indexed by 9-bit neighborhood config (512 unique configs possible, rest for collision headroom)
- **Spatial subsampling**: Transition stats built from every 2nd cell for performance (O(W×H×frames/4))
- **Surprisal computation**: Full per-cell evaluation using accumulated statistics
- **History recording**: Done in `grid_step()` so frames accumulate regardless of whether surprise mode is active

## Why This Feature
- **Fills a missing dimension**: Entropy measures spatial disorder; Lyapunov measures perturbation sensitivity; surprise measures **temporal unpredictability** — how well the system's own history predicts its next state
- **Complements existing tools**: High-entropy regions aren't necessarily surprising (random noise is high-entropy but predictable as noise). Surprise reveals where the specific outcome defied the learned distribution.
- **Clean exploitation**: Follows the exact analyzer template, reuses neighborhood scanning, zero new dependencies
- **Connects to Wolfram classes**: Class I/II should be low surprise (deterministic/periodic), Class III moderate (statistically predictable chaos), Class IV high and spatially heterogeneous (complex localized surprise at boundaries)

## Architecture
Follows the established pattern:
- `surp_mode` / `surp_stale` / `surp_grid[][]` global state
- `surp_compute()` function with staleness-guarded auto-refresh
- `surp_to_rgb()` color mapping
- `cell_color()` integration (return code 17 for surprise ghost cells)
- HUD indicator with lightning bolt icon (⚡SURP:x.xxxx)
- Overlay panel stacking below existing panels
- `!` key binding (mnemonic: exclamation = surprise)

## Lines Added
~315 lines of C (declarations + compute + color map + cell_color + HUD + panel + key binding + auto-refresh)
