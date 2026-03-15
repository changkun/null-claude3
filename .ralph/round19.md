# Round 19: Live Pattern Census Overlay

## Feature
Real-time pattern recognition engine that scans the grid and identifies/counts known still lifes and oscillators, displayed in a toggleable overlay panel.

## Key Binding
- `v` — Toggle census overlay on/off

## How It Works
- **14 pattern templates** defined as exact bitmask grids (8 still lifes + 6 oscillator phases)
- Still lifes: Block, Beehive, Loaf, Boat, Tub, Pond, Ship, Barge
- Oscillators: Blinker (2 phases), Toad (2 phases), Beacon, Clock
- **Exact matching**: pattern interior must match alive/dead precisely, plus a 1-cell dead border ensures isolation
- **Claim-based deduplication**: matched cells are "claimed" so no cell is counted in multiple patterns
- **Phase merging**: both phases of an oscillator (e.g. horizontal + vertical blinker) merge into one count
- **Unmatched counter**: shows how many live cells don't belong to any recognized pattern ("…other")

## Overlay Panel
- Dark green-bordered box in top-left corner below status bars
- Still lifes shown in green, oscillators in amber
- Each row: pattern name + count
- Bottom row: unclaimed live cells count
- Header shows total recognized structures

## Performance
- Scans every 16 generations (not every frame) to keep it fast on 400×200 grid
- Immediate scan on toggle-on
- ~O(14 × W × H × max_pattern_size) per scan — fast for small patterns

## Integration
- Status bar shows ☰CENSUS indicator when active
- Help bar includes [v]census
- Census invalidated on every grid_step()

## Lines Added
~160 lines (pattern definitions, scan function, overlay renderer, key binding, status indicator)

## Exploitation Score: High
- Reuses existing overlay rendering patterns (frequency legend, stamp preview)
- Leverages grid data and wrap_mode directly
- Complements frequency analysis (period detection) with structural identification
- Enriches screenshots/screencasts with analytical data
