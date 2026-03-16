# Round 62: Spatial Coherence Overlay (Kuramoto Synchronization)

## Goal
Add a spatial coherence overlay that reveals synchronized oscillation domains across the grid using the Kuramoto order parameter.

## What was built
A new grid-level analysis overlay (toggle `` ` ``) that maps phase synchronization across the automaton:

- **Phase extraction**: Each cell's on/off flip history from the timeline buffer (32 frames) is used to estimate an instantaneous oscillation phase θ ∈ [0, 2π)
- **Kuramoto order parameter**: Local R = |1/N Σ e^{iθ}| computed over a radius-3 neighbourhood for each cell
- **Grid heatmap**: Dark blue = desynchronized (low R), cyan = partial sync, white = perfect lockstep
- **Domain counting**: Flood-fill BFS identifies coherent domains (contiguous regions with R > 0.6)
- **Sidebar panel**: Shows global R, sync fraction, domain count, sparkline history, depth/radius info
- **Color legend**: 5-stop gradient from deep blue through cyan to white

## Technical details
- `COH_DEPTH=32` timeline frames for phase estimation via flip counting
- `COH_R=3` neighbourhood radius for local Kuramoto computation
- Phase computed from flip-time fraction: θ = 2π × (time_since_last_flip / estimated_period)
- Period estimated as 2×depth/flips — adapts to each cell's oscillation frequency
- Static cells (no flips) assigned phase 0
- Auto-refreshes every 4 generations for smooth visualization without excessive cost
- Domain detection via 8-connected BFS on cells with R > 0.6
- Sparkline tracks global R evolution over 64 samples

## Integration points
- Split overlay table entry 28 ("Coherence")
- Key: `` ` `` (backtick, outside split mode)
- Stale flag reset on grid changes
- Demo scene support via '`' overlay char
- Panel stacking: positioned between ergodicity and percolation panels

## Why this matters
Round 61 added frequency analysis (DFT of global metrics) but couldn't show *where* synchronization lives spatially. This overlay fills that gap — it reveals emergent coordination domains where distant cells oscillate in phase without direct contact, a hallmark of complex spatiotemporal dynamics in cellular automata.
