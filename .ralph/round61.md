# Round 61: Temporal Power Spectrum Overlay

## Goal
Add a temporal power spectrum overlay that reveals dominant oscillation frequencies across all 16 metrics via DFT of each metric's time series from a 128-frame ring buffer.

## What was built
A new analysis overlay (toggle `6`) computing and displaying the power spectrum of all 16 metric time series:

- **128-frame ring buffer**: `ps_buf[128][16]` stores metric snapshots every 2 generations
- **Direct DFT**: 32 frequency bins per metric (bins 1..32 of 128-point DFT), with Hann windowing and DC removal
- **16x32 heatmap**: Half-block rendering (8 char rows for 16 metrics), frequency on X-axis, metric on Y-axis
- **Color scheme**: Dark blue (low power) -> cyan -> yellow -> red (high power), peak frequency highlighted in white
- **Sidebar stats**: Top 4 most powerful metrics with dominant period (T) and spectral entropy (H)
- **Spectral character indicator**: Overall mean entropy classified as PERIODIC (<0.4), MIXED (0.4-0.7), or CHAOTIC (>0.7)
- **Status bar**: Blue FFT indicator showing frame count

## Technical details
- `PS_WINDOW=128`, `PS_N=16`, `PS_NFREQ=32` frequency bins
- Goertzel-style direct DFT — no external FFT library needed
- Hann window applied before DFT to reduce spectral leakage
- Spectral entropy normalized to [0,1] via log2(PS_NFREQ)
- Panel height: 18 rows (border + info + freq label + 8 heatmap rows + separator + 4 dominant + character + border)
- All downstream panels (recurrence plot, 3D attractor, particle tracker) updated for stacking
- Split-screen support added (overlay index 28)

## Design decisions
- **Key `6`**: Only used in ecosystem_mode for species brush toggle; reused for power spectrum when not in ecosystem_mode
- **32 frequency bins**: Half the Nyquist limit of 64 — provides sufficient resolution while keeping heatmap compact
- **Hann window**: Standard choice for reducing spectral leakage in windowed DFT; better than rectangular for non-periodic signals
- **Half-block rendering**: Matches project convention (recurrence plot, kymograph) — doubles vertical resolution
- **Top 4 dominant metrics**: Keeps sidebar concise while highlighting the most spectrally active channels

## Explore/exploit balance
~20% explore (new frequency-domain computation, DFT implementation) / ~80% exploit (reuses metric ring buffer pattern from CM/RP/AD, rendering conventions, panel stacking)
