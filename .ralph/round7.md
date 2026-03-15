---
round: 7
goal: "Minimap overlay — full-grid thumbnail with viewport rectangle when zoomed"
status: complete
exploration_pct: 0
---

# Round 7: Minimap Overlay

Added a navigational minimap that appears when zoomed in, showing the entire 400×200 grid as a compact thumbnail with a highlighted rectangle marking the current viewport position.

## What was built

**Minimap rendering** (bottom-right corner overlay):
- Quarter-block characters (same 2×2 subpixel technique as zoom 4x) compress the full grid into a thumbnail
- Dark blue-gray background (`rgb(20,20,30)`) with box-drawing border separates it from grid content
- ANSI cursor positioning overlays the minimap on top of the rendered grid

**Viewport rectangle:**
- Bright yellow border shows exactly which portion of the grid is currently visible
- Updates in real-time as you pan with arrow keys

**Adaptive sizing:**
- Scales to 1/4 terminal width and 1/3 usable height
- Capped at 50×25 characters maximum
- Hides automatically if terminal is too small (below 10×5 chars)

**Controls:**
- Auto-shows at zoom 2x and 4x, hidden at 1x (full grid already visible)
- Toggle with `n` key
- Status bar shows `▣MAP` indicator when active

## Why this matters

At zoom 2x and 4x, the viewport shows only a fraction of the 80,000-cell grid — navigating is blind without spatial context. The minimap closes this gap: you can see where you are relative to the whole simulation, track distant activity (glider streams, spreading methuselahs), and pan with confidence. This makes the zoom/pan system from round 6 actually usable for exploration.

## Technical notes

- Grid sampling maps each subpixel to a region of cells and checks for any alive cell (OR reduction, not averaging)
- Viewport rectangle is computed in subpixel coordinates for accurate positioning at all terminal sizes
- Rendering appended after main grid draw, using absolute cursor positioning to overlay without disrupting the grid buffer
