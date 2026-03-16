# Round 44: Entropy Production Rate

## Goal
Add an entropy production rate overlay (`=` key) that computes dS/dt — the temporal derivative of local Shannon entropy — making the thermodynamic arrow of time visible in real time.

## Motivation
All existing overlays capture spatial snapshots: entropy at this moment, correlation at this distance, complexity of this neighborhood. None track how thermodynamic quantities *change* over time. The entropy production rate is a fundamental quantity in non-equilibrium thermodynamics that reveals whether a region is self-organizing (entropy decreasing) or dissolving (entropy increasing). This is the first temporal thermodynamic overlay in the analysis suite.

## Implementation

### Data Structures
- Per-cell entropy history with EMA smoothing for flicker-free temporal derivatives
- Global dS/dt tracking with min/max extremes
- Sparkline history buffer for temporal trends

### Algorithm
1. **Local Shannon entropy** computed per cell from neighborhood state distribution
2. **Temporal derivative** dS/dt via difference between current and smoothed historical entropy
3. **EMA smoothing** prevents flicker while preserving genuine thermodynamic trends
4. **Phase classification** based on global dS/dt: crystallizing, ordering, equilibrium, heating, dissipating

### Color Scheme
- **Blue** — entropy decreasing (self-organization emerging)
- **Gray** — thermodynamic equilibrium (dS/dt ≈ 0)
- **Red** — entropy increasing (structures dissolving)

### UI Elements
- Stats panel: global dS/dt, ordering/disordering fractions, thermodynamic phase label, min/max extremes, color-coded sparkline history
- Integrated into cell probe inspector (`?` key)
- Toggle: `=` key

### Integration Points
- Probe inspector: per-cell dS/dt value
- Demo scene support
- Status bar indicator

## Lines Added
+366 lines (11,277 → 11,639)

## Classification
~90% exploration (first temporal thermodynamic observable — new analytical dimension)
