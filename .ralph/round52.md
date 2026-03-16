# Round 52 — Topological Persistence Barcode

**Explore/Exploit: 55% explore**

## Goal
Add a persistence barcode overlay that tracks connected components across
generations, recording birth/death times and rendering a barcode diagram
showing feature lifetimes.

## What was built
- **Toggle**: `:` key
- **Core algorithm**: Frame-to-frame component matching via spatial overlap
  (greedy matching using a 64×64 overlap matrix computed by cell scanning)
- **Tracking**: Ring buffer of 256 PersistBar structs storing birth gen,
  death gen, peak size, and display hue for each feature's lifetime
- **Grid coloring**: Per-cell persistence age mapped to a thermal gradient:
  white/cyan (newborn) → green (young) → gold → amber (ancient/persistent)
- **Sidebar panel** (14 rows):
  - Alive/born/died counts
  - Mean lifetime (μlife) and max lifetime
  - Color legend (new → old gradient)
  - β₀ sparkline (active feature count over time)
  - 7-row barcode diagram showing longest-lived features
  - Live features have ▶ arrow, dead features have │ terminator
  - Toggle hint

## Key design decisions
- Piggybacks on existing `topo_compute()` for per-frame component labels —
  no duplicate BFS flood fill needed
- Overlap matrix limited to 64×64 for O(n²) performance bound while still
  catching the vast majority of matches
- Greedy (not Hungarian) matching: simpler, sufficient for frame-to-frame
  continuity where components rarely swap positions
- `pb_initialized` flag + full reset in `grid_clear()` ensures clean state
  when user randomizes or clears

## Integration points
- Split-screen overlay table: index 23
- `split_set_overlay` / `split_detect_current` / `split_ensure_computed`
- `demo_reset_overlays` clears `pb_mode`
- `demo_setup_scene` supports `':'` in overlay strings
- Stale flag set alongside all other overlays in `update()`
- Auto-refresh every 2 generations in main loop

## What it reveals
- Still lifes appear as gold/amber bars stretching indefinitely (▶ tip)
- Gliders and spaceships create short bars (component emerges, translates,
  but overlap matching may lose track across moves → death + rebirth)
- Oscillators create persistent bars if they maintain connected topology
- Chaotic regions produce a blizzard of short bars (high turnover)
- The barcode diagram visually separates structural signal from noise

## Lines
13,953 → 14,457 (+504 lines)

## Key insight
Persistent homology reveals the *temporal stability* of spatial structure
in a way no single-frame analysis can. The barcode is a fingerprint of the
dynamics: ordered systems (Wolfram Class I/II) show few long bars, chaotic
systems (Class III) show many short bars, and edge-of-chaos systems
(Class IV) show a mix — a few long bars amid turbulent turnover. This
complements the Hamiltonian energy landscape (round 51) which explains
*why* structures are stable, while persistence explains *how long*.
