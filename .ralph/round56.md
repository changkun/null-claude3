# Round 56 — Recurrence Plot Overlay

**Explore/Exploit: ~60% explore / 40% exploit**

## Goal
Add a real-time recurrence plot — a classic nonlinear dynamics visualization
that reveals hidden temporal structure by building a 64×64 recurrence matrix
from the system's 16-metric state vectors over time.

## What was built
- **Toggle**: `o` key (overlay index 26)
- **Data collection**: Records all 16 metrics via `pp_read_metric()` into a
  64-step ring buffer every 2 generations (same cadence as correlation matrix)
- **Distance matrix**: 64×64 pairwise Euclidean distances in normalized
  16-dimensional metric space (each metric normalized to [0,1] range)
- **Recurrence threshold**: ε = 10% of max distance (configurable via
  `rp_thresh`)

### RQA Statistics
- **RR%** — Recurrence Rate: fraction of matrix entries below threshold
- **DET%** — Determinism: fraction of recurrent points on diagonal lines
  (length ≥ 2), indicates predictability
- **LAM%** — Laminarity: fraction of recurrent points on vertical lines
  (length ≥ 2), indicates trapping
- **L_max** — Longest diagonal line, related to maximum prediction horizon
- **TT** — Trapping Time: mean length of vertical lines

### Regime Classification
Automatic regime detection from RQA stats:
- **Periodic**: DET > 80% and RR > 20%
- **Laminar**: DET > 50% and LAM > 50%
- **Chaotic**: DET < 30% and RR < 10%
- **Trapped**: LAM > 60%
- **Transient**: none of the above

### Visual Display
- **Sidebar panel**: 32×32 downsampled recurrence matrix rendered with
  Unicode half-block characters (▀) for 2 vertical pixels per character row
  → 16 character rows × 32 character columns
- **Color scheme**: Dark purple/magenta for recurrent states (distance ≤ ε),
  gray-to-warm gradient for non-recurrent states
- **Stats section**: 4 rows showing RR%, DET%, LAM%, TT, L_max, max
  distance, and auto-detected regime label

### Integration points (13 locations in life.c)
1. Global variables and forward declarations (after gd_mode globals)
2. Overlay table: N_SPLIT_OVERLAYS 26→27, entry at index 26
3. split_set_overlay: disable list + case 26
4. split_detect_current: rp_mode → 26
5. rp_record() + rp_compute() implementations (after cm_compute)
6. demo_reset_overlays: rp_mode = 0
7. split_ensure_computed: case 26
8. Split-screen save/restore: split_saved_rp
9. Status bar indicator: ▦REC:N/64
10. Sidebar panel with recurrence matrix + RQA stats
11. Key binding: 'o' toggle
12. Main loop: record every 2 generations when running

## Key design decisions
- Used `o` key instead of `R` (task spec) since `R`/`r` was already bound
  to grid randomize
- 64-step buffer matches the spec; display downsampled to 32×32 for
  terminal-friendly sidebar rendering
- Half-block rendering packs 64 visual rows into 16 character rows
- Normalized metrics before distance computation to prevent magnitude-biased
  distances (population ~1000s vs entropy ~0-8)
- Purple/magenta color theme distinguishes from existing overlays
