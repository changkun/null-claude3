# Round 55 — Geodesic Distance Field

**Explore/Exploit: ~40% explore / 60% exploit**

## Goal
Add a geodesic distance field overlay that computes BFS shortest-path
distances through the live cell network from a user-selected seed point,
revealing connectivity topology, bottlenecks, bridges, and isolated
archipelagos in real-time.

## What was built
- **Toggle**: `"` (double-quote) key
- **Interaction**: Click-to-seed (like cone_mode / probe_mode pattern)
  - Mode 1: awaiting click — cursor shows "GEO:?"
  - Mode 2: active — shows distance field, updates every 2 generations
- **BFS computation**: O(W×H) flood-fill through 8-connected live cells
  - Seeds on dead cells auto-snap to nearest live cell within radius 10
  - Tracks: max distance (diameter), reachable count, isolated count,
    mean distance
- **Color scheme**: 5-stop gradient normalized to max distance
  - White (seed) → cyan (near) → green (mid) → yellow (far) → red (very far) → magenta
  - Live but unreachable cells shown in dim gray (isolated components)
  - Dead cells remain black
- **Sidebar panel** (7 rows):
  - Seed coordinates and network diameter
  - Reachable cells count and isolated (unreachable live) count
  - Mean geodesic distance
  - Mini histogram of distance distribution (8-bin sparkline bar)
  - Color legend
- **Status bar**: ◈GEO:? (awaiting) or ◈GEO:dN (showing diameter)
- **Integration**: Split-screen overlay #25, save/restore, auto-refresh,
  stale flag system

## Key insight
The geodesic distance field fills a gap in the overlay suite — none of the
25 prior overlays measured **network reachability** through the living
structure.  While topology (β₀/β₁) counts components and holes, and
percolation identifies clusters, geodesic distance adds *metric geometry*:
how far apart are two points within the same connected component?  This
reveals bottlenecks (narrow bridges where distance jumps), measures the
effective "size" of structures beyond simple cell count, and shows whether
a cluster is compact (low diameter) or elongated (high diameter).

## Lines added
~280 lines (variable declarations, compute function, color function,
cell rendering, sidebar panel, key handler, mouse handler, status bar,
split-screen integration, auto-refresh).
