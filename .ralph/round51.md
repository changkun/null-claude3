# Round 51: Hamiltonian Energy Landscape Overlay

## Goal
Add a statistical mechanics overlay that treats the automaton as an Ising spin system, mapping alive/dead cells to +1/-1 spins and computing per-cell Hamiltonian energy H = -s·Σs_neighbors. Visualize the result as a continuous energy landscape revealing why certain structures are stable from a physics perspective.

## What was built

### Hamiltonian Energy Landscape (Toggle: `;`)
- **Ising spin mapping**: Alive cells → spin +1, dead cells → spin -1
- **Per-cell energy**: H = -s·Σs_neighbors computed over the 8-cell Moore neighborhood
- **Energy landscape coloring**:
  - Deep blue = energy wells (stable, aligned structures like still lifes)
  - Cyan = shallow wells (partially stable configurations)
  - Yellow/orange = mild frustration (boundary regions)
  - Bright red = strongly frustrated (domain walls, unstable configurations)

### Sidebar Stats Panel
- **Total energy H**: Sum of all per-cell Hamiltonian values
- **Magnetization m**: Net spin alignment (fraction of +1 vs -1)
- **Susceptibility χ**: Variance-based measure that peaks at phase transitions
- **Domain wall count/density**: Number and density of frustrated neighbor pairs
- **Energy sparkline**: Rolling history of total energy over time
- **Phase classification**: Ordered / Critical / Disordered / Paramagnetic based on magnetization and susceptibility

### Key insight
This overlay explains *why* certain structures are stable from a statistical mechanics perspective. Still lifes sit in deep energy minima (all neighbors aligned), while chaotic regions are frustrated high-energy states that the system "wants" to relax away from. Domain walls — boundaries between ordered regions — show up as bright red lines of maximum frustration.

### Integration
- Overlay index 22, accessible via `;` key
- Full split-screen support as a dual-comparison overlay
- Status bar indicator showing active state
- Help bar updated with key hint
- Clean compilation with existing flags

## Technical notes
- Pure Ising model analogy — no actual spin dynamics, just energy evaluation of the CA state
- The coupling constant J is implicitly +1 (ferromagnetic), so aligned neighbors lower energy
- Susceptibility computed via χ = (⟨m²⟩ - ⟨m⟩²) / N, tracking variance over a rolling window
- Domain walls detected by counting neighbor pairs with opposite spin signs

## Lines added
~385 lines (overlay rendering, energy computation, stats panel, key handling, status integration)
