# Round 40: Kolmogorov Complexity Estimator

## Goal
Add a per-cell algorithmic complexity overlay that estimates Kolmogorov complexity via LZ77-style compression of local neighborhoods.

## Motivation
All five existing information-theoretic overlays (entropy, Lyapunov, surprise, MI, composite) measure *statistical* properties of the automaton. Kolmogorov complexity measures something fundamentally different: *algorithmic incompressibility* — how long the shortest program to produce a pattern is. This is the core concept of algorithmic information theory, the one major branch of complexity science the project hadn't touched.

Key insight: Shannon entropy and Kolmogorov complexity can diverge dramatically. A checkerboard has maximum entropy (every 3×3 neighborhood is mixed) but near-zero K-complexity (describable in ~10 bits: "alternate 0/1"). Conversely, a glider gun's output stream has moderate entropy but high algorithmic complexity.

## Implementation
- **Key:** `^` (Shift+6)
- **Algorithm:** For each alive cell, serialize a 16×16 neighborhood centered on it into a 256-bit bitstring. Compress using a simplified LZ77 sliding-window compressor (window=64, min match=3). The compression ratio (compressed_bits / raw_bits) estimates algorithmic complexity.
- **Color scheme:** Deep blue (compressible/simple) → cyan → gold (structured) → red → white-hot (incompressible/complex)
- **Stats panel:** Mean/max/min K, distribution bar (simple|structured|complex), class counts with percentages, sparkline history
- **Auto-refresh:** Every 4 generations

## Architecture
Follows established overlay pattern:
- `kc_grid[MAX_H][MAX_W]` — per-cell complexity ratio
- `kc_compute()` — LZ77 compression + statistics
- `kc_to_rgb()` — color mapping
- `kc_lz_compress()` — core LZ77 sliding-window compressor
- Stats panel stacks below RG panel
- Status bar indicator: ◆KC:0.XX

## Lines added: +351 (9,910 → 10,259)

## Exploration vs Exploitation
92% exploration — this opens an entirely new analytical dimension (algorithmic information theory) orthogonal to all existing overlays.
