# Round 15: Wormhole Portals — Non-Local Spatial Coupling

## Goal
Break the fundamental locality assumption of cellular automata by adding paired wormhole portals that create tunnels between distant grid regions. Cells near a portal entrance see additional neighbors from the paired exit (and vice versa), enabling long-range spatial coupling.

## What was built
- **Portal data structures**: Up to 8 portal pairs, each with entrance (A) and exit (B) coordinates and a coupling radius (2-6 cells)
- **Non-local neighbor counting**: `portal_neighbor_count()` adds extra neighbors from the paired endpoint during `grid_step()`, using positional mapping (same offset from center)
- **Animated portal visualization**: Swirling ring markers with angle-based brightness variation that animates with generation count. Cyan/teal for entrances, magenta/pink for exits
- **Portal placement mode** (`W` key): Click to place entrance, click again for exit. Right-click to cancel or remove. Scroll wheel adjusts radius. Mutually exclusive with emit/zone modes
- **Full rendering integration**: Portals render correctly at all 3 zoom levels (1x, 2x half-block, 4x quarter-block) and appear on the minimap
- **Save/load**: Portals appended to `.life` binary format with backward compatibility (old files without portal data load fine)
- **Status bar**: Shows portal count, radius, and placement state. Help bar includes `[W]worm`
- **Cleanup integration**: Double-clear (`c` twice) removes portals along with zones/emitters

## Key design decisions
- **Additive neighbor model**: Portal neighbors are *added* to the normal 8-neighbor count rather than replacing them. This means cells near portals can have >8 effective neighbors, creating interesting overpopulation dynamics at standard Conway rules. The B/S bitmask still works (just caps at bit 8 for count >8)
- **Bidirectional coupling**: Both endpoints are symmetric — entrance sees exit's neighborhood and vice versa
- **Positional mapping**: A cell at offset (dx,dy) from entrance center sees neighbors at the same (dx,dy) offset from exit center, preserving spatial structure through the wormhole
- **Ring visualization**: Only the perimeter ring is drawn (r-1 to r), keeping the interior clear for actual cell activity

## Lines added
~268 lines (2484 → 2752)

## Interactions with existing systems
- **Zones**: Portals can connect regions with different rulesets, creating cross-rule neighbor influence
- **Emitters**: Glider streams can be redirected through wormholes
- **Wrapping**: Portal neighbor sampling respects toroidal wrap mode
- **Signal tracer**: Trail accumulation shows wormhole traffic patterns
- **Frequency analysis**: Oscillators near portals show altered periods due to non-local coupling
- **Symmetry**: Portal placement is not symmetric (intentional — portals break symmetry by design)
