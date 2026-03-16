# Round 54 — Metric Correlation Matrix

**Explore/Exploit: Pure exploit**

## Goal
Add a real-time 16×16 heatmap showing pairwise Pearson correlations among
all scalar metrics, revealing which overlays capture redundant vs.
independent information.

## What was built
- **Toggle**: `'` (single quote) key
- **16×16 heatmap grid**: Each cell colored by Pearson correlation
  coefficient between two metrics over a sliding window
- **Color scheme**: Blue (r = −1, anti-correlated) → white (r = 0,
  uncorrelated) → red (r = +1, strongly correlated)
- **Sliding window**: Ring buffer of 64 frames, recording all 16 metrics
  via `pp_read_metric()` every 2 generations
- **Sidebar panel** (~26 rows):
  - Window fill counter (N/64 frames)
  - Column headers (first letter of each metric name)
  - Row labels (first 5 chars of metric name)
  - 16×16 colored heatmap cells (2-char wide blocks)
  - Separator line
  - ▲ Most correlated pair with r value
  - ▼ Most anti-correlated pair with r value
  - ◇ Most independent pair (closest to r = 0)
- **Status bar indicator**: `CORR:N/64` in red

## Key design decisions
- Pure exploitation of Round 53's `pp_read_metric()` unified scalar
  interface — zero new analysis math, just standard Pearson correlation
- Minimum 4 samples required before computing (statistical minimum)
- Standard deviation check (`< 1e-12`) prevents divide-by-zero for
  constant metrics — assigns r = 0 instead
- Records every 2 generations, matching phase portrait cadence
- `cm_stale` flag avoids redundant recomputation
- Three-pass computation: means → standard deviations → covariances,
  all over the ring buffer with correct index wrapping

## Integration points
- Split-screen overlay table: index 24
- `split_set_overlay` / `split_detect_current` / `split_ensure_computed`
- `demo_reset_overlays` clears `cm_mode`
- Panel stacks below all 24 existing overlay panels on the right side

## What it reveals
- Whether Lyapunov exponent always tracks entropy, or they diverge near
  phase transitions
- Which metrics are redundant (high positive r) — candidates for removal
  or grouping
- Which metrics are truly independent (r ≈ 0) — the most informative
  subset for characterizing system state
- Anti-correlated pairs reveal inverse relationships (e.g., does high
  energy production correlate with low ergodicity?)
- The matrix is a meta-analysis tool: it turns the overlay suite itself
  into the object of study

## Lines
14,822 → 15,137 (+315 lines)

## Key insight
The correlation matrix is a second-order observatory — it doesn't observe
the automaton directly, but observes the *observers*. By revealing
statistical dependencies among all 16 scalar metrics, it exposes which
analysis overlays are measuring the same underlying phenomenon with
different labels, and which are capturing genuinely orthogonal aspects of
the dynamics. This is essential for understanding the information-theoretic
efficiency of the overlay suite: if two metrics are always correlated at
r > 0.95, one of them is nearly redundant.