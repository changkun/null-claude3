---
round: 5
goal: "Kaleidoscope drawing — symmetric mouse painting with 2/4/8-fold reflection"
status: complete
exploration_pct: 40
---

# Round 5: Kaleidoscope Drawing

Added symmetric mouse painting that reflects strokes across the grid center, turning the automaton into a generative art tool.

## What was built

**Symmetry modes** (`k` key cycles through):
- **None** — default, single-point drawing
- **2-fold** — vertical mirror (left↔right)
- **4-fold** — quad mirror (all four quadrants)
- **8-fold** — full kaleidoscope (quad + diagonal reflections)

**Implementation details:**
- `sym_apply()` function takes a grid coordinate and a callback (`grid_set` or `grid_unset`), then applies the appropriate reflections around the grid center
- Works with both click and drag painting
- Status bar shows `✵SYM:N` indicator when active
- ~25 lines of new code, zero architectural changes

## Why this matters

Symmetric seeds interact dramatically with different rulesets:
- 8-fold symmetry + Day & Night → organic snowflake growth
- 4-fold symmetry + Diamoeba → pulsating amoeba blobs
- 2-fold symmetry + Conway → symmetric glider factories
- Any symmetry + mutation → evolving generative art

The user shifts from observer to artist — draw a few strokes in 8-fold mode, press play, and watch symmetric structures emerge and evolve.

## Future directions

- Pattern import/export (RLE format) for sharing discoveries
- Multi-state automata (Brian's Brain, Wireworld)
- Recording/GIF export of interesting evolutions
- Undo/redo for drawing strokes
