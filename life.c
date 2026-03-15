/*
 * Life-like Cellular Automaton Explorer — terminal edition (C / ANSI)
 *
 * Controls:
 *   SPACE / p   Play / Pause (in replay mode: resume live from current point)
 *   P           Screenshot — save viewport as PPM image (frame_NNNN.ppm)
 *   s           Step one generation (while paused)
 *   r           Randomize the grid
 *   c           Clear the grid
 *   1-5         Load preset pattern
 *                 1=Glider  2=Pulsar  3=Gosper Gun  4=R-pentomino  5=Acorn
 *   +/-         Speed up / slow down
 *   d           Toggle draw mode (left-click=place, right-click=erase)
 *   g           Toggle population sparkline graph
 *   y           Toggle population dynamics dashboard overlay
 *   w           Cycle topology: flat → torus → Klein bottle → Möbius strip → projective plane
 *   h           Toggle heatmap mode (age coloring + ghost trails)
 *   [ / ]       Cycle through rule presets (or zone brush in zone mode)
 *   m           Mutate — randomly flip one birth/survival bit
 *   b           Toggle rule editor overlay (click B/S bits or preset names)
 *   k           Cycle symmetry: none → 2-fold → 4-fold → 8-fold (kaleidoscope)
 *   j           Toggle zone-paint mode (paint regions with different rulesets)
 *   e           Toggle emitter/absorber mode (left=emitter, right=absorber, middle=remove)
 *                 [/] cycle emit pattern, +/- adjust rate/radius in this mode
 *   z / x       Zoom in / out (3 levels: 1x, 2x half-block, 4x quarter)
 *   n           Toggle minimap overlay (shows full grid + viewport rect when zoomed)
 *   < / ,       Rewind through history (enter replay mode)
 *   > / .       Fast-forward through history
 *   t           Toggle timeline bar display
 *   T           Cycle signal tracer: off → accumulate → frozen → clear+off
 *   f           Toggle frequency analysis overlay (period detection heatmap)
 *   i           Toggle entropy heatmap overlay (local Shannon entropy)
 *   W           Toggle wormhole portal placement (left=entrance, right=exit, middle=remove)
 *                 Paired portals create non-local neighbor coupling
 *   v           Toggle pattern census overlay (counts known structures)
 *   G           Genetic rule explorer — evolve interesting rulesets
 *                 Press G again to breed next generation, 1-5 to load rule, q to close
 *   S           Toggle stamp mode (place classic patterns from library)
 *                 [/] cycle pattern, scroll wheel rotates 0°/90°/180°/270°
 *                 Left-click places pattern, right-click cancels stamp mode
 *   a           Toggle dual-species ecosystem mode (Red vs Blue)
 *   6           Toggle active brush species (A/B) in ecosystem mode
 *   { / }       Adjust cross-species interaction coefficient (-1.0 to +1.0)
 *   P           Capture screenshot as PPM image (auto-numbered frame_NNN.ppm)
 *   Ctrl-P      Dump full timeline buffer as numbered image sequence
 *   Ctrl-S      Save state to numbered .life file
 *   Ctrl-O      Load most recent .life save (or Ctrl-O N for slot N)
 *   X           Toggle temperature mode — stochastic noise field
 *                 Left-click=paint hot, right-click=paint cold, scroll=brush size
 *                 +/- adjust brush temperature, {/} adjust global temperature
 *                 [/] adjust brush radius
 *   u           Toggle 2D Fourier spectrum analyzer (spatial frequency analysis)
 *   F           Toggle box-counting fractal dimension analyzer
 *   C           Toggle Wolfram class detector (auto-classify I/II/III/IV)
 *   O           Toggle information flow field (transfer entropy causal vectors)
 *   A           Toggle phase-space attractor (Takens delay embedding portrait)
 *   9           Toggle causal light cone (click cell to trace backward/forward cones)
 *                 +/- adjust forward cone depth, click to retarget
 *   !           Toggle prediction surprise field (per-cell surprisal heatmap)
 *   @           Toggle mutual information network (inter-region coupling map)
 *   #           Toggle composite complexity index (edge-of-chaos heatmap)
 *   $           Toggle topological feature map (connected components + holes)
 *                 Colors each component uniquely, highlights enclosed holes
 *                 Sidebar shows β₀ (components), β₁ (holes), sparklines
 *   %           Toggle renormalization group flow (multi-scale structure)
 *                 Majority-rule block decimation at 2x, 4x, 8x scales
 *                 Cyan=fine, yellow=meso, magenta=coarse, white=scale-invariant
 *   ^           Toggle Kolmogorov complexity estimator (algorithmic complexity)
 *                 LZ77-style compression of local neighborhoods
 *                 Blue=compressible, gold=structured, red=incompressible
 *   Ctrl-E      Export current grid as RLE file (auto-numbered export_NNN.rle)
 *   Arrow keys  Pan viewport across the full 400×200 grid
 *   0           Re-center viewport on grid center
 *   q / ESC     Quit
 *
 *   Mouse:      Left-click to place cells, right-click to erase
 *               Click+drag to paint/erase continuously
 *               Scroll wheel to zoom in/out
 *
 * Build:  gcc -O2 -o life life.c
 * Run:    ./life              (random start)
 *         ./life pattern.rle  (load RLE file)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <signal.h>
#include <time.h>
#include <math.h>

/* ── Grid ──────────────────────────────────────────────────────────────────── */

#define MAX_W 400
#define MAX_H 200

/* grid stores cell age: 0=dead, 1+=generations alive (capped at 255) */
static unsigned char grid[MAX_H][MAX_W];
static unsigned char next_grid[MAX_H][MAX_W];

/* zone grid: stores ruleset index (0..N_RULESETS-1) per cell */
static unsigned char zone[MAX_H][MAX_W]; /* default 0 = first ruleset */

/* ghost trails: 0=none, 1..GHOST_FRAMES=fading (GHOST_FRAMES=just died) */
#define GHOST_FRAMES 5
static unsigned char ghost[MAX_H][MAX_W];

/* signal tracer: accumulates cell presence over time (0-255) */
static unsigned char tracer[MAX_H][MAX_W];
static int tracer_mode = 0; /* 0=off, 1=accumulating, 2=frozen (visible but not growing) */

/* frequency analysis: detected oscillation period per cell */
static unsigned char freq_grid[MAX_H][MAX_W]; /* 0=dead, 1=still life, 2-32=period, 255=chaotic */
static int freq_mode = 0; /* 0=off, 1=on (analysis overlay active) */
static int freq_stale = 1; /* 1=needs recomputation */

/* ── Entropy Heatmap ─────────────────────────────────────────────────────── */
/* Shannon entropy computed over each cell's Moore neighborhood (3x3 → 9 cells).
   Measures local spatial information content: low entropy = uniform (all alive
   or all dead), high entropy = mixed (edge of structures, phase boundaries). */
static float entropy_grid[MAX_H][MAX_W]; /* 0.0 = uniform, 1.0 = max entropy */
static int entropy_mode = 0;    /* 0=off, 1=on */
static int entropy_stale = 1;   /* 1=needs recomputation */
static float entropy_global = 0.0f;    /* mean entropy across grid */
static float entropy_max_local = 0.0f; /* max local entropy found */

/* ── Temperature Field ──────────────────────────────────────────────────── */
/* Stochastic temperature parameter: T=0 is deterministic, T>0 flips birth/death
   decisions with probability T.  Spatial gradients create coexisting ordered
   (cold/blue) and chaotic (hot/red) regions with visible phase boundaries. */
static float temp_grid[MAX_H][MAX_W];  /* per-cell temperature 0.0–1.0 */
static int   temp_mode = 0;            /* 0=off, 1=on (painting/overlay active) */
static float temp_global = 0.0f;       /* global uniform temperature offset */
static float temp_brush = 0.5f;        /* brush temperature for painting */
static int   temp_brush_radius = 3;    /* brush radius for spatial painting */

/* ── Lyapunov Sensitivity Map ──────────────────────────────────────────── */
/* Perturbation sensitivity: run a shadow grid with single-cell bit-flips
   and measure how quickly the perturbation diverges from the real grid.
   Cells colored by local sensitivity: blue (stable) → yellow (critical) → red (chaotic). */
static float lyapunov_grid[MAX_H][MAX_W];  /* accumulated sensitivity per cell 0.0–1.0 */
static int   lyapunov_mode = 0;            /* 0=off, 1=on */
static int   lyapunov_stale = 1;           /* 1=needs recomputation */
static float lyapunov_global = 0.0f;       /* mean sensitivity across grid */
static float lyapunov_max_local = 0.0f;    /* max local sensitivity found */

#define LYAP_HORIZON  16   /* shadow generations per measurement cycle */
#define LYAP_PERTURB  80   /* 1-in-N cells get bit-flipped in shadow */
#define LYAP_DECAY    0.82f /* sliding window EMA decay factor */

/* ── 2D Fourier Spectrum Analyzer ──────────────────────────────────────── */
/* Spatial frequency analysis via 2D discrete FFT (Cooley-Tukey radix-2).
   Reveals dominant wavelengths, periodicity, and structural regularity.
   Power spectrum mapped to grid, plus radial profile and spectral entropy. */
#define FFT_W 512      /* padded width  (next power of 2 above MAX_W=400) */
#define FFT_H 256      /* padded height (next power of 2 above MAX_H=200) */
#define FFT_RADIAL_BINS 128  /* bins for radial power profile */

static float fourier_grid[MAX_H][MAX_W];  /* per-cell log-power (fftshifted, normalized 0–1) */
static int   fourier_mode = 0;            /* 0=off, 1=on */
static int   fourier_stale = 1;           /* 1=needs recomputation */
static float fourier_spectral_entropy = 0.0f;   /* how spread energy is across frequencies */
static float fourier_dominant_wl = 0.0f;         /* dominant wavelength in cells */
static float fourier_peak_power = 0.0f;          /* peak radial power (for display) */
static float fourier_mean_power = 0.0f;          /* mean spectral power */
static float fourier_radial[FFT_RADIAL_BINS];    /* azimuthally-averaged power vs frequency */
static float fourier_radial_max = 0.0f;          /* max radial bin value */

/* FFT workspace — static to avoid stack overflow */
static float fft_re[FFT_H][FFT_W];
static float fft_im[FFT_H][FFT_W];

/* ── Box-Counting Fractal Dimension ───────────────────────────────────────── */
/* Estimate fractal dimension D_box via box-counting: cover the grid with boxes
   of size ε = 2,4,8,16,32,64,128 and count N(ε) = occupied boxes at each scale.
   D_box = -slope of log(N) vs log(ε) regression.  Per-cell overlay shows which
   scale contributes most structural detail (finest occupied scale). */
#define FRACTAL_N_SCALES 7  /* ε = 2,4,8,16,32,64,128 */

static float fractal_grid[MAX_H][MAX_W];  /* per-cell finest scale (normalized 0–1) */
static int   fractal_mode = 0;            /* 0=off, 1=on */
static int   fractal_stale = 1;           /* 1=needs recomputation */
static float fractal_dbox = 0.0f;         /* estimated fractal dimension */
static float fractal_r2 = 0.0f;           /* R² goodness of fit */
static int   fractal_n_boxes[FRACTAL_N_SCALES];  /* N(ε) per scale */
static float fractal_log_eps[FRACTAL_N_SCALES];  /* log(ε) per scale */
static float fractal_log_n[FRACTAL_N_SCALES];    /* log(N(ε)) per scale */
static int   fractal_total_alive = 0;     /* total alive cells */

/* ── Wolfram Class Detector ─────────────────────────────────────────────── */
/* Automatic classification into Wolfram Classes I–IV by fusing signals from
   entropy, Lyapunov sensitivity, fractal dimension, and population dynamics.
   Class I:   Fixed (death / homogeneous)
   Class II:  Periodic (oscillators, still lifes)
   Class III: Chaotic (pseudo-random, space-filling)
   Class IV:  Complex (edge of chaos: gliders, long transients, localized structures) */
#define WOLFRAM_HIST 32   /* population samples for temporal analysis */

static int   wolfram_mode = 0;            /* 0=off, 1=on */
static int   wolfram_stale = 1;           /* 1=needs recomputation */
static int   wolfram_class = 0;           /* 0=unknown, 1-4=Wolfram class */
static float wolfram_confidence = 0.0f;   /* 0.0–1.0 confidence in classification */
static float wolfram_scores[4];           /* raw score per class (I,II,III,IV) */

/* Feature vector used for classification */
static float wf_entropy = 0.0f;     /* global mean entropy */
static float wf_lyapunov = 0.0f;    /* global mean Lyapunov exponent */
static float wf_fractal = 0.0f;     /* fractal dimension D_box */
static float wf_pop_var = 0.0f;     /* population variance (normalized) */
static float wf_pop_trend = 0.0f;   /* population trend (growth/decay rate) */
static float wf_density = 0.0f;     /* live cell density */
static float wf_pop_acf = 0.0f;     /* population autocorrelation (periodicity) */

/* ── Information Flow Field ────────────────────────────────────────────────── */
/* Transfer entropy-based directional causal influence vectors.
   Computes pairwise transfer entropy between each cell and its 4 cardinal
   neighbors over a sliding temporal window.  Derives a net information flow
   vector (direction + magnitude) per cell, revealing glider highways,
   information barriers, and computational channels. */
#define FLOW_WINDOW 16   /* temporal frames for transfer entropy */
#define FLOW_BLOCK  4    /* spatial block size for coarse-graining */

static float flow_vx[MAX_H][MAX_W];   /* x-component of flow vector */
static float flow_vy[MAX_H][MAX_W];   /* y-component of flow vector */
static float flow_mag[MAX_H][MAX_W];  /* magnitude of flow vector */
static int   flow_mode = 0;           /* 0=off, 1=on */
static int   flow_stale = 1;          /* 1=needs recomputation */
static float flow_global_mag = 0.0f;  /* mean flow magnitude */
static float flow_max_mag = 0.0f;     /* max flow magnitude */
static float flow_vorticity = 0.0f;   /* mean curl (rotational tendency) */
static int   flow_n_sources = 0;      /* cells with strong outward divergence */
static int   flow_n_sinks = 0;        /* cells with strong inward convergence */
/* Direction histogram: 8 bins for N,NE,E,SE,S,SW,W,NW */
static int   flow_dir_hist[8];
static int   flow_dir_hist_max = 1;

/* ── Phase-Space Attractor (Takens delay embedding) ──────────────────────── */
static int   attractor_mode = 0;        /* 0=off, 1=on */
static int   attractor_stale = 1;       /* 1=needs recomputation */

/* Attractor scatter plot: 2D delay-embedded phase portrait rendered into a
   grid of bins.  Axes are population(t) vs population(t+τ).  Each bin
   accumulates hit counts; brighter = more trajectory visits. */
#define ATTR_BINS 80                    /* resolution of scatter plot (bins per axis) */
#define ATTR_MAX_PTS 4096               /* max trajectory points to keep */
static int   attr_canvas[ATTR_BINS][ATTR_BINS]; /* hit-count canvas */
static int   attr_canvas_max = 1;       /* max bin count (for normalization) */
static int   attr_n_pts = 0;            /* number of points in trajectory */
static float attr_pts_x[ATTR_MAX_PTS];  /* normalized x coords (pop(t))   */
static float attr_pts_y[ATTR_MAX_PTS];  /* normalized y coords (pop(t+τ)) */
static int   attr_delay = 1;            /* embedding delay τ */
static int   attr_traj_len = 0;         /* usable trajectory length */
static float attr_corr_dim = 0.0f;      /* correlation dimension D₂ */
static float attr_pop_min = 0.0f;       /* population range for axis labels */
static float attr_pop_max = 1.0f;
static int   attr_embed_dim = 2;        /* embedding dimension (always 2 for display) */

/* ── Causal Light Cone ───────────────────────────────────────────────────── */
/* Relativistic-style causal structure visualization.  Select a cell and trace
   its backward light cone (which cells causally influenced it) through the
   timeline buffer, and its forward light cone (which cells it can influence)
   based on the ruleset's Moore neighborhood propagation speed (1 cell/step).
   Backward cone: actual causal ancestors highlighted inside the theoretical
   diamond.  Forward cone: theoretical maximum influence boundary. */
#define CONE_MAX_DEPTH 128  /* max frames to trace backward (half of TL_MAX) */

static int   cone_mode = 0;          /* 0=off, 1=on (awaiting click), 2=on (computed) */
static int   cone_sel_x = -1;        /* selected cell x */
static int   cone_sel_y = -1;        /* selected cell y */
static int   cone_depth = 0;         /* actual backward depth traced */
static int   cone_fwd_depth = 32;    /* forward cone depth to show */

/* Backward cone: per-cell minimum causal distance (0 = selected cell, 1..N = ancestor depth)
   0 means not in cone.  We use a separate grid to store depth+1 (0=not in cone). */
static unsigned char cone_back[MAX_H][MAX_W];  /* backward cone depth+1 (0=not in cone) */
static unsigned char cone_fwd[MAX_H][MAX_W];   /* forward cone depth+1 (0=not in cone) */

/* Stats */
static int   cone_back_cells = 0;    /* cells in backward cone */
static int   cone_fwd_cells = 0;     /* cells in forward cone */
static int   cone_theoretical = 0;   /* theoretical max cells in cone */
static float cone_fill_ratio = 0.0f; /* actual/theoretical ratio */
static int   cone_back_alive = 0;    /* backward cone cells currently alive */

/* ── Prediction Surprise Field ─────────────────────────────────────────── */
/* Per-cell surprisal: accumulate transition statistics (neighborhood config →
   outcome) over a sliding window, then color each cell by -log₂ P(state|nbrs).
   Predictable regions (still lifes, known oscillators) render cool/dark;
   unpredictable boundaries and chaotic zones glow hot. */
#define SURP_WINDOW 32      /* frames of transition history to track */
#define SURP_HASH_SIZE 8192 /* hash table size for transition counts (power of 2) */
#define SURP_HASH_MASK (SURP_HASH_SIZE - 1)

/* Hash table entry: maps (neighborhood_config, outcome) → count */
typedef struct {
    unsigned int key;   /* packed neighborhood config (9 bits for Moore nbrs + 1 bit center state) */
    int outcome;        /* 0 or 1: what the center cell became */
    int count;          /* transition count */
    int total;          /* total times this neighborhood config was seen */
} SurpEntry;

static float surp_grid[MAX_H][MAX_W];   /* per-cell surprisal (bits) */
static int   surp_mode = 0;             /* 0=off, 1=on */
static int   surp_stale = 1;            /* 1=needs recomputation */
static float surp_global = 0.0f;        /* mean surprisal across grid */
static float surp_max_local = 0.0f;     /* max local surprisal */
static float surp_min_local = 99.0f;    /* min non-zero surprisal */
static int   surp_predictable = 0;      /* cells with surprisal < 0.1 bits */
static int   surp_surprising = 0;       /* cells with surprisal > 0.8 bits */

/* Ring buffer of recent grids for transition statistics */
static unsigned char surp_history[SURP_WINDOW][MAX_H][MAX_W];
static int surp_hist_head = 0;          /* next write position */
static int surp_hist_len = 0;           /* filled entries */

/* Hash table for transition frequency counting */
static int surp_trans_count[SURP_HASH_SIZE];  /* count of (config → alive) */
static int surp_trans_total[SURP_HASH_SIZE];  /* total count of config */

/* ── Mutual Information Network ────────────────────────────────────────────── */
/* Inter-region coupling map: partition grid into coarse blocks, compute MI
   between block population time series over the timeline history, and render
   the strongest couplings as colored lines — revealing long-range
   synchronization, oscillator entrainment, and glider communication channels. */
#define MI_BLK_W  20   /* block width in cells */
#define MI_BLK_H  20   /* block height in cells */
#define MI_GW     (MAX_W / MI_BLK_W)  /* 20 block columns */
#define MI_GH     (MAX_H / MI_BLK_H)  /* 10 block rows */
#define MI_NBLK   (MI_GW * MI_GH)     /* 200 total blocks */
#define MI_BINS   8     /* discretization bins for population time series */
#define MI_TOP_N  40    /* max number of top couplings to display */

typedef struct {
    int a, b;      /* block indices */
    float mi;      /* mutual information (bits) */
} MICoupling;

static int   mi_mode = 0;            /* 0=off, 1=on */
static int   mi_stale = 1;           /* 1=needs recomputation */
static float mi_overlay[MAX_H][MAX_W]; /* per-cell MI value for line drawing */
static MICoupling mi_top[MI_TOP_N];  /* top-N strongest couplings */
static int   mi_n_top = 0;           /* number of stored couplings */
static float mi_mean_val = 0.0f;     /* mean MI across all pairs */
static float mi_max_val = 0.0f;      /* max MI value */
static float mi_net_density = 0.0f;  /* fraction of pairs above threshold */
static float mi_clustering = 0.0f;   /* clustering coefficient */
static int   mi_n_frames_used = 0;   /* frames used in last computation */

/* ── Composite Complexity Index ──────────────────────────────────────────── */
/* Per-cell edge-of-chaos score fusing entropy, Lyapunov sensitivity,
   prediction surprise, and frequency analysis into one complexity measure.
   Highlights Wolfram Class IV regions at cell-level spatial resolution.
   Low = simple/dead, mid-green = periodic, gold = edge-of-chaos, red = chaotic. */
static float cplx_grid[MAX_H][MAX_W];  /* composite complexity 0.0–1.0 */
static int   cplx_mode = 0;            /* 0=off, 1=on */
static int   cplx_stale = 1;           /* 1=needs recomputation */
static float cplx_global = 0.0f;       /* mean complexity across grid */
static float cplx_max_local = 0.0f;    /* max local complexity */
static int   cplx_n_simple = 0;        /* cells with score < 0.2 */
static int   cplx_n_complex = 0;       /* cells in edge-of-chaos band 0.4–0.7 */
static int   cplx_n_chaotic = 0;       /* cells with score > 0.8 */
static float cplx_edge_frac = 0.0f;    /* fraction of alive cells in edge band */

/* ── Topological Feature Map ──────────────────────────────────────────────── */
/* Connected component labeling and hole detection via flood fill.
   β₀ = number of distinct connected live-cell components (4-connected).
   β₁ = number of enclosed dead-cell regions (holes/voids bounded by live cells).
   Each component gets a unique hue; holes are highlighted in magenta/violet.
   Sidebar sparkline tracks β₀ and β₁ over time. */
#define TOPO_MAX_COMPONENTS 1024   /* max distinct components to label */
#define TOPO_HIST_LEN 64           /* sparkline history length */

static unsigned short topo_label[MAX_H][MAX_W];  /* component label per cell (0=dead/unlabeled) */
static unsigned char  topo_hole[MAX_H][MAX_W];    /* 1=cell is inside a hole */
static int   topo_mode = 0;              /* 0=off, 1=on */
static int   topo_stale = 1;             /* 1=needs recomputation */
static int   topo_beta0 = 0;             /* β₀: connected components */
static int   topo_beta1 = 0;             /* β₁: enclosed holes */
static int   topo_largest_comp = 0;      /* size of largest component */
static int   topo_n_holes_cells = 0;     /* total dead cells inside holes */
static float topo_mean_comp_size = 0.0f; /* mean component size */

/* Per-component hue (assigned via golden ratio for max visual separation) */
static unsigned short topo_comp_hue[TOPO_MAX_COMPONENTS]; /* hue 0-359 */
static int   topo_comp_size[TOPO_MAX_COMPONENTS];         /* cells per component */

/* Sparkline history */
static int   topo_hist_b0[TOPO_HIST_LEN];  /* β₀ history */
static int   topo_hist_b1[TOPO_HIST_LEN];  /* β₁ history */
static int   topo_hist_idx = 0;             /* write index */
static int   topo_hist_count = 0;           /* entries filled */

/* Flood-fill queue (shared workspace) */
#define TOPO_QUEUE_SIZE (MAX_W * MAX_H)
static int topo_qx[TOPO_QUEUE_SIZE];
static int topo_qy[TOPO_QUEUE_SIZE];

/* ── Renormalization Group Flow ──────────────────────────────────────────── */
/* Real-space RG via majority-rule block decimation at scales 2x, 4x, 8x.
   At each scale, non-overlapping blocks of s×s cells are reduced to a single
   "coarse" cell (alive if majority of block cells are alive).  The density
   at each scale reveals which spatial frequencies carry structure:
     - Fine-dominated (cyan): structure only at small scales (dust, noise)
     - Meso-dominated (yellow): medium-scale organization (clusters)
     - Coarse-dominated (magenta): large-scale coherent structure
     - Scale-invariant (white): similar density across all scales (critical/fractal)
   Criticality score = 1 - max deviation from mean across scales. */
#define RG_N_SCALES 3   /* scales: 2x, 4x, 8x block sizes */
#define RG_HIST_LEN 64  /* sparkline history length */

static float rg_grid[MAX_H][MAX_W];     /* per-cell dominant scale (0=fine, 0.5=meso, 1.0=coarse) */
static float rg_invariance[MAX_H][MAX_W]; /* per-cell scale invariance (0=dominated, 1=flat) */
static int   rg_mode = 0;               /* 0=off, 1=on */
static int   rg_stale = 1;              /* 1=needs recomputation */
static float rg_density[RG_N_SCALES];   /* global density at each RG scale */
static float rg_criticality = 0.0f;     /* global criticality score (flatness) */
static float rg_global_fine = 0.0f;     /* fraction of cells that are fine-dominated */
static float rg_global_meso = 0.0f;     /* fraction of cells that are meso-dominated */
static float rg_global_coarse = 0.0f;   /* fraction of cells that are coarse-dominated */
static float rg_global_invariant = 0.0f;/* fraction of cells that are scale-invariant */
static float rg_mean_invariance = 0.0f; /* mean invariance across grid */

/* Coarse-grained grids at each scale */
#define RG_MAX_CW (MAX_W / 2)  /* max coarse width at scale 2 */
#define RG_MAX_CH (MAX_H / 2)  /* max coarse height at scale 2 */
static unsigned char rg_coarse[RG_N_SCALES][RG_MAX_CH][RG_MAX_CW];

/* Sparkline history */
static float rg_hist_crit[RG_HIST_LEN]; /* criticality history */
static float rg_hist_d[RG_N_SCALES][RG_HIST_LEN]; /* density per scale history */
static int   rg_hist_idx = 0;
static int   rg_hist_count = 0;

/* ── Kolmogorov Complexity Estimator ──────────────────────────────────────── */
/* Estimate per-cell algorithmic complexity via LZ77-style compression of
   local neighborhoods.  For each cell, serialize a 16×16 block into a
   bitstring and compress using a sliding-window dictionary search.
   Complexity = compressed_bits / raw_bits (compression ratio).
   Low ratio = highly compressible (algorithmically simple: uniform, periodic);
   High ratio = incompressible (algorithmically complex: random-looking).
   This is fundamentally different from Shannon entropy: entropy measures
   statistical disorder, Kolmogorov complexity measures descriptional richness.
   A perfect checkerboard has max entropy but near-zero K-complexity. */
#define KC_BLOCK 16         /* neighborhood block size (16×16 = 256 bits) */
#define KC_WINDOW 64        /* LZ77 sliding window size (bits to search back) */
#define KC_MIN_MATCH 3      /* minimum match length for LZ77 */
#define KC_HIST_LEN 64      /* sparkline history length */

static float kc_grid[MAX_H][MAX_W];     /* per-cell complexity ratio 0.0–1.0 */
static int   kc_mode = 0;               /* 0=off, 1=on */
static int   kc_stale = 1;              /* 1=needs recomputation */
static float kc_global_mean = 0.0f;     /* mean complexity across alive cells */
static float kc_global_max = 0.0f;      /* max complexity found */
static float kc_global_min = 1.0f;      /* min complexity found */
static int   kc_n_simple = 0;           /* cells with ratio < 0.3 */
static int   kc_n_structured = 0;       /* cells with ratio 0.3–0.7 */
static int   kc_n_complex = 0;          /* cells with ratio > 0.7 */

/* Sparkline history */
static float kc_hist_mean[KC_HIST_LEN];
static float kc_hist_max[KC_HIST_LEN];
static int   kc_hist_idx = 0;
static int   kc_hist_count = 0;

/* ── Pattern Census ───────────────────────────────────────────────────────── */
static int census_mode = 0;    /* 0=off, 1=on */
static int census_stale = 1;   /* 1=needs recomputation */

/* Census pattern templates: bitmask-based matching for small patterns.
   Each pattern is stored as a WxH bitmask grid (max 6x6).
   The grid must match EXACTLY — live cells where mask=1, dead where mask=0
   within the bounding box, PLUS a 1-cell dead border around it. */
#define CENSUS_MAX_PAT 14
#define CENSUS_PAT_MAX_W 6
#define CENSUS_PAT_MAX_H 6

typedef struct {
    const char *name;
    int w, h;
    unsigned char bits[CENSUS_PAT_MAX_H][CENSUS_PAT_MAX_W]; /* 1=alive, 0=dead */
} CensusPattern;

static const CensusPattern census_patterns[CENSUS_MAX_PAT] = {
    /* ── Still lifes ── */
    { "Block",    2, 2, {{1,1},{1,1}} },
    { "Beehive",  4, 3, {{0,1,1,0},{1,0,0,1},{0,1,1,0}} },
    { "Loaf",     4, 4, {{0,1,1,0},{1,0,0,1},{0,1,0,1},{0,0,1,0}} },
    { "Boat",     3, 3, {{1,1,0},{1,0,1},{0,1,0}} },
    { "Tub",      3, 3, {{0,1,0},{1,0,1},{0,1,0}} },
    { "Pond",     4, 4, {{0,1,1,0},{1,0,0,1},{1,0,0,1},{0,1,1,0}} },
    { "Ship",     3, 3, {{1,1,0},{1,0,1},{0,1,1}} },
    { "Barge",    4, 4, {{0,1,0,0},{1,0,1,0},{0,1,0,1},{0,0,1,0}} },
    /* ── Oscillators (phase 1 only — we match either phase) ── */
    { "Blinker",  3, 1, {{1,1,1}} },
    { "Blinker",  1, 3, {{1},{1},{1}} },  /* vertical phase */
    { "Toad",     4, 2, {{0,1,1,1},{1,1,1,0}} },
    { "Toad",     2, 4, {{1,0},{1,1},{1,1},{0,1}} },  /* vertical phase */
    { "Beacon",   4, 4, {{1,1,0,0},{1,0,0,0},{0,0,0,1},{0,0,1,1}} },
    { "Clock",    4, 4, {{0,0,1,0},{1,0,1,0},{0,1,0,1},{0,1,0,0}} },
};

/* Census results */
static int census_counts[CENSUS_MAX_PAT]; /* raw counts per pattern template */

/* Deduplicated counts for display (merge both phases of same oscillator) */
#define CENSUS_DISPLAY_MAX 16
static struct { const char *name; int count; int type; /* 0=still,1=osc,2=ship */ } census_display[CENSUS_DISPLAY_MAX];
static int census_n_display = 0;
static int census_total = 0; /* total identified structures */
static int census_unmatched = 0; /* live cells not part of any recognized pattern */

/* Temporary grid to mark already-claimed cells during census scan */
static unsigned char census_claimed[MAX_H][MAX_W];

/* ── Spaceship / Glider Detection ────────────────────────────────────────── */
/* Detect moving structures by matching a known template at (x,y) in frame N,
   then confirming it shifted by the expected displacement in frame N+1.
   Each spaceship "class" has multiple phases and a displacement per full cycle. */

#define SHIP_MAX_W 7
#define SHIP_MAX_H 5
#define SHIP_MAX_PHASES 4
#define SHIP_MAX_TYPES 4

typedef struct {
    int w, h;
    unsigned char bits[SHIP_MAX_H][SHIP_MAX_W];
} ShipPhase;

typedef struct {
    const char *name;
    int n_phases;           /* number of phases in one full cycle */
    int dx, dy;             /* displacement per full cycle (phase 0 → phase 0) */
    ShipPhase phases[SHIP_MAX_PHASES];
} ShipType;

/* Glider: 4 phases, moves (1,1) per full cycle (SE direction as canonical) */
static const ShipType ship_types[SHIP_MAX_TYPES] = {
    { "Glider", 4, 1, 1, {
        { 3, 3, {{0,1,0},{0,0,1},{1,1,1}} },           /* phase 0 */
        { 3, 3, {{1,0,1},{0,1,1},{0,1,0}} },           /* phase 1 (after 1 gen) */
        { 3, 3, {{0,0,1},{1,0,1},{0,1,1}} },           /* phase 2 */
        { 3, 3, {{1,0,0},{0,1,1},{1,1,0}} },           /* phase 3 — next gen is phase 0 at (+1,+1) */
    }},
    /* LWSS: 4 phases, moves (2,0) per full cycle (E direction canonical) */
    { "LWSS", 4, 2, 0, {
        { 5, 4, {{0,1,0,0,1},{1,0,0,0,0},{1,0,0,0,1},{1,1,1,1,0}} },
        { 5, 4, {{1,1,0,0,0},{0,0,0,0,0},{1,0,0,1,0},{0,1,1,1,1}} },
        { 5, 4, {{0,0,1,0,0},{1,0,0,0,0},{0,0,0,0,1},{0,1,1,1,1}} },
        { 5, 4, {{1,0,0,1,0},{0,0,0,0,1},{0,0,0,0,1},{0,1,1,1,1}} },
    }},
    /* MWSS: 4 phases, moves (2,0) per cycle */
    { "MWSS", 4, 2, 0, {
        { 6, 5, {{0,0,1,0,0,0},{1,0,0,0,1,0},{0,0,0,0,0,1},{1,0,0,0,0,1},{0,1,1,1,1,1}} },
        { 6, 5, {{0,0,0,0,0,0},{0,1,1,0,0,0},{1,0,0,0,0,0},{1,0,0,0,1,0},{0,1,1,1,1,1}} },
        { 6, 5, {{0,0,0,1,0,0},{0,0,0,0,0,0},{1,0,0,0,0,0},{0,0,0,0,0,1},{0,1,1,1,1,1}} },
        { 6, 5, {{0,0,0,0,0,0},{0,1,0,0,1,0},{0,0,0,0,0,1},{0,0,0,0,0,1},{0,1,1,1,1,1}} },
    }},
    /* HWSS: 4 phases, moves (2,0) per cycle */
    { "HWSS", 4, 2, 0, {
        { 7, 5, {{0,0,1,1,0,0,0},{1,0,0,0,0,1,0},{0,0,0,0,0,0,1},{1,0,0,0,0,0,1},{0,1,1,1,1,1,1}} },
        { 7, 5, {{0,0,0,0,0,0,0},{0,1,1,1,0,0,0},{1,0,0,0,0,0,0},{1,0,0,0,0,1,0},{0,1,1,1,1,1,1}} },
        { 7, 5, {{0,0,0,1,1,0,0},{0,0,0,0,0,0,0},{1,0,0,0,0,0,0},{0,0,0,0,0,0,1},{0,1,1,1,1,1,1}} },
        { 7, 5, {{0,0,0,0,0,0,0},{0,1,0,0,0,1,0},{0,0,0,0,0,0,1},{0,0,0,0,0,0,1},{0,1,1,1,1,1,1}} },
    }},
};

/* Detected spaceships for overlay rendering */
#define SHIP_DETECT_MAX 256
typedef struct {
    int x, y;       /* position of matched phase in current frame */
    int w, h;       /* bounding box of the matched phase */
    int dir;        /* direction index: 0=↑ 1=↗ 2=→ 3=↘ 4=↓ 5=↙ 6=← 7=↖ */
    int type_idx;   /* index into ship_types */
} ShipDetection;

static ShipDetection ship_detections[SHIP_DETECT_MAX];
static int ship_n_detections = 0;
static int ship_counts[SHIP_MAX_TYPES]; /* per-type counts for display */

/* Map (dx,dy) displacement to direction index and arrow character */
static int ship_dir_from_delta(int dx, int dy) {
    /* Normalize signs */
    int sx = (dx > 0) ? 1 : (dx < 0) ? -1 : 0;
    int sy = (dy > 0) ? 1 : (dy < 0) ? -1 : 0;
    /* Map to 8-direction index */
    if (sx ==  0 && sy == -1) return 0; /* ↑ */
    if (sx ==  1 && sy == -1) return 1; /* ↗ */
    if (sx ==  1 && sy ==  0) return 2; /* → */
    if (sx ==  1 && sy ==  1) return 3; /* ↘ */
    if (sx ==  0 && sy ==  1) return 4; /* ↓ */
    if (sx == -1 && sy ==  1) return 5; /* ↙ */
    if (sx == -1 && sy ==  0) return 6; /* ← */
    if (sx == -1 && sy == -1) return 7; /* ↖ */
    return 2; /* default → */
}
static const char *ship_arrows[] = {"↑","↗","→","↘","↓","↙","←","↖"};

/* Overlay grid: which cells are part of a detected spaceship (for cyan coloring) */
static unsigned char census_ship[MAX_H][MAX_W]; /* 0=no, 1=spaceship cell */

/* Forward declaration */
static void census_ship_scan(void);

/* Forward declaration — defined after grid_step where H/W/topology are available */
static void census_scan(void);

/* Forward declaration for surprise field */
static void surp_record_frame(void);

/* ── Genetic Rule Explorer ────────────────────────────────────────────────── */
#define GENPOP_SIZE 20       /* population of candidate rules per generation */
#define GENBEST_SIZE 5       /* top N shown in overlay / used as parents */
#define GENEVAL_STEPS 200    /* simulation steps per evaluation */
#define GENEVAL_W 80         /* mini-grid width for evaluation */
#define GENEVAL_H 60         /* mini-grid height for evaluation */

typedef struct {
    unsigned short birth;
    unsigned short survival;
    float score;             /* fitness score (higher = more interesting) */
    char label[24];          /* B.../S... string */
} GeneCandidate;

static int gene_mode = 0;        /* 0=off, 1=running search, 2=showing results */
static int gene_gen = 0;         /* evolution generation counter */
static GeneCandidate gene_best[GENBEST_SIZE]; /* top candidates to display */
static int gene_selected = -1;   /* highlighted candidate (-1=none) */
static int gene_search_done = 0; /* flag: search iteration complete */

/* Forward declarations */
static void gene_search_step(void);
static void gene_format_rule(char *buf, int buflen, unsigned short b, unsigned short s);

static int W = MAX_W, H = MAX_H; /* simulation always uses full grid */
static int generation;
static int population;
/* Topology: 0=flat(finite), 1=torus, 2=Klein bottle, 3=Möbius strip, 4=projective plane */
enum { TOPO_FLAT=0, TOPO_TORUS, TOPO_KLEIN, TOPO_MOBIUS, TOPO_PROJECTIVE, TOPO_COUNT };
static int topology = 0;
static const char *topo_names[] = {"flat","torus","Klein","Möbius","proj"};
static const char *topo_symbols[] = {"","∞","𝕂","𝕄","ℙ"};
static int heatmap_mode = 1; /* age heatmap + ghost trails (on by default) */
static int symmetry = 0; /* 0=none, 1=2-fold, 2=4-fold, 3=8-fold */
static int show_minimap = 1; /* minimap overlay when zoomed */
static int zone_mode = 0; /* 0=off, 1=zone-paint mode */
static int zone_brush = 0; /* which ruleset to paint in zone mode */
static int zone_enabled = 0; /* whether per-cell zones are active */
static int rule_editor = 0; /* 0=off, 1=rule editor overlay visible */

/* ── Dual-Species Ecosystem ─────────────────────────────────────────────── */
static unsigned char species[MAX_H][MAX_W]; /* 0=none, 1=species A, 2=species B */
static unsigned char next_species[MAX_H][MAX_W]; /* double-buffer for species */
static int ecosystem_mode = 0;     /* 0=off, 1=dual-species active */
static int brush_species = 1;      /* 1=A, 2=B — which species the draw brush places */
static float interaction = 0.0f;   /* -1.0 to +1.0: cross-species neighbor weight */
static unsigned short species_a_birth = 0x008;    /* B3 (Conway default) */
static unsigned short species_a_survival = 0x00C; /* S23 */
static unsigned short species_b_birth = 0x048;    /* B36 (HighLife default) */
static unsigned short species_b_survival = 0x00C; /* S23 */

/* Rule editor overlay geometry (computed at render time) */
static int re_col0, re_row0; /* top-left corner (1-based terminal coords) */
#define RE_WIDTH 40
#define RE_HEIGHT 6

/* Forward declarations for emitter/absorber use */
static void grid_set(int x, int y);
static void grid_unset(int x, int y);

/* ── Emitters & Absorbers (sources/sinks) ─────────────────────────────────── */

#define MAX_EMITTERS 32
#define MAX_ABSORBERS 32

typedef struct {
    int x, y;       /* grid position */
    int rate;        /* emit every N generations (1=every step, 5=every 5th) */
    int pattern;     /* 0=dot, 1=cross, 2=random 3x3, 3=glider */
} Emitter;

typedef struct {
    int x, y;       /* grid position */
    int radius;     /* kill radius (1..6) */
} Absorber;

static Emitter emitters[MAX_EMITTERS];
static int n_emitters = 0;
static Absorber absorbers[MAX_ABSORBERS];
static int n_absorbers = 0;
static int emit_mode = 0;      /* 0=off, 1=emitter/absorber placement mode */
static int emit_pattern = 0;   /* current pattern for new emitters */
static int emit_rate = 3;      /* current rate for new emitters */
static int absorb_radius = 3;  /* current radius for new absorbers */

/* Emit patterns into the grid at (cx,cy) */
static void emitter_fire(Emitter *e) {
    int cx = e->x, cy = e->y;
    switch (e->pattern) {
        case 0: /* dot */
            grid_set(cx, cy);
            break;
        case 1: /* cross */
            grid_set(cx, cy);
            grid_set(cx-1, cy); grid_set(cx+1, cy);
            grid_set(cx, cy-1); grid_set(cx, cy+1);
            break;
        case 2: /* random 3x3 */
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++)
                    if (rand() % 2)
                        grid_set(cx + dx, cy + dy);
            break;
        case 3: /* glider (direction varies by position) */
            {
                int d = (cx + cy) % 4;
                static const int gx[4][5] = {{0,1,2,2,1},{0,0,0,1,-1},{0,-1,-2,-2,-1},{0,0,0,-1,1}};
                static const int gy[4][5] = {{0,0,0,-1,-2},{0,1,2,2,1},{0,0,0,1,2},{0,-1,-2,-2,-1}};
                for (int i = 0; i < 5; i++)
                    grid_set(cx + gx[d][i], cy + gy[d][i]);
            }
            break;
    }
}

/* Apply absorber: kill all cells within radius */
static void absorber_apply(Absorber *a) {
    int r = a->radius;
    int r2 = r * r;
    for (int dy = -r; dy <= r; dy++)
        for (int dx = -r; dx <= r; dx++)
            if (dx*dx + dy*dy <= r2)
                grid_unset(a->x + dx, a->y + dy);
}

/* Apply all emitters and absorbers (called after grid_step) */
static void apply_sources_sinks(void) {
    for (int i = 0; i < n_emitters; i++) {
        if (generation % emitters[i].rate == 0)
            emitter_fire(&emitters[i]);
    }
    for (int i = 0; i < n_absorbers; i++) {
        absorber_apply(&absorbers[i]);
    }
}

/* Add emitter (with symmetry) */
static void add_emitter_at(int x, int y) {
    if (n_emitters >= MAX_EMITTERS) return;
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    /* Don't place duplicate at same position */
    for (int i = 0; i < n_emitters; i++)
        if (emitters[i].x == x && emitters[i].y == y) return;
    emitters[n_emitters++] = (Emitter){ x, y, emit_rate, emit_pattern };
}

static void add_absorber_at(int x, int y) {
    if (n_absorbers >= MAX_ABSORBERS) return;
    if (x < 0 || x >= W || y < 0 || y >= H) return;
    for (int i = 0; i < n_absorbers; i++)
        if (absorbers[i].x == x && absorbers[i].y == y) return;
    absorbers[n_absorbers++] = (Absorber){ x, y, absorb_radius };
}

/* Remove any emitter or absorber near (x,y) within radius 3 */
static void remove_source_sink_near(int x, int y) {
    for (int i = 0; i < n_emitters; i++) {
        int dx = emitters[i].x - x, dy = emitters[i].y - y;
        if (dx*dx + dy*dy <= 9) {
            emitters[i] = emitters[--n_emitters];
            i--;
        }
    }
    for (int i = 0; i < n_absorbers; i++) {
        int dx = absorbers[i].x - x, dy = absorbers[i].y - y;
        if (dx*dx + dy*dy <= 9) {
            absorbers[i] = absorbers[--n_absorbers];
            i--;
        }
    }
}

/* Place emitter with symmetry */
static void place_emitter_sym(int gx, int gy) {
    int cx = W / 2, cy = H / 2;
    int dx = gx - cx, dy = gy - cy;
    add_emitter_at(gx, gy);
    if (symmetry >= 1) add_emitter_at(cx - dx, gy);
    if (symmetry >= 2) { add_emitter_at(gx, cy - dy); add_emitter_at(cx - dx, cy - dy); }
    if (symmetry >= 3) {
        add_emitter_at(cx+dy, cy+dx); add_emitter_at(cx-dy, cy+dx);
        add_emitter_at(cx+dy, cy-dx); add_emitter_at(cx-dy, cy-dx);
    }
}

static void place_absorber_sym(int gx, int gy) {
    int cx = W / 2, cy = H / 2;
    int dx = gx - cx, dy = gy - cy;
    add_absorber_at(gx, gy);
    if (symmetry >= 1) add_absorber_at(cx - dx, gy);
    if (symmetry >= 2) { add_absorber_at(gx, cy - dy); add_absorber_at(cx - dx, cy - dy); }
    if (symmetry >= 3) {
        add_absorber_at(cx+dy, cy+dx); add_absorber_at(cx-dy, cy+dx);
        add_absorber_at(cx+dy, cy-dx); add_absorber_at(cx-dy, cy-dx);
    }
}

static const char *emit_pattern_names[] = { "dot", "cross", "rand3", "glider" };
#define N_EMIT_PATTERNS 4

/* Forward declaration for topology coordinate mapping */
static inline int topo_map(int *nx, int *ny);

/* ── Wormhole Portals (non-local spatial coupling) ─────────────────────────── */

#define MAX_PORTALS 8   /* max portal pairs */

typedef struct {
    int ax, ay;     /* entrance position */
    int bx, by;     /* exit position */
    int radius;     /* coupling radius (2..5) */
    int active;     /* 1=both endpoints placed, 0=incomplete */
} Portal;

static Portal portals[MAX_PORTALS];
static int n_portals = 0;
static int portal_mode = 0;       /* 0=off, 1=portal placement mode */
static int portal_placing = 0;    /* 0=ready, 1=placing exit (entrance set in portal_wip) */
static int portal_wip_x, portal_wip_y; /* work-in-progress entrance coords */
static int portal_radius = 3;     /* radius for new portals */

/* Remove portal near (x,y) */
static void remove_portal_near(int x, int y) {
    for (int i = 0; i < n_portals; i++) {
        int dxa = x - portals[i].ax, dya = y - portals[i].ay;
        int dxb = x - portals[i].bx, dyb = y - portals[i].by;
        if (dxa*dxa + dya*dya <= 25 || dxb*dxb + dyb*dyb <= 25) {
            portals[i] = portals[--n_portals];
            i--;
        }
    }
}

/* Get the count of extra "portal neighbors" for cell (x,y).
 * Cells near a portal entrance also see cells near the paired exit, and vice versa. */
static int portal_neighbor_count(int x, int y) {
    int extra = 0;
    for (int i = 0; i < n_portals; i++) {
        if (!portals[i].active) continue;
        int r = portals[i].radius;
        int r2 = r * r;
        /* Check if near entrance → sample around exit */
        int dxa = x - portals[i].ax, dya = y - portals[i].ay;
        if (dxa*dxa + dya*dya <= r2) {
            /* Map relative position: offset from entrance center → same offset at exit */
            int ox = portals[i].bx + dxa;
            int oy = portals[i].by + dya;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = ox + dx, ny = oy + dy;
                    if (!topo_map(&nx, &ny)) continue;
                    extra += (grid[ny][nx] > 0);
                }
        }
        /* Check if near exit → sample around entrance */
        int dxb = x - portals[i].bx, dyb = y - portals[i].by;
        if (dxb*dxb + dyb*dyb <= r2) {
            int ox = portals[i].ax + dxb;
            int oy = portals[i].ay + dyb;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = ox + dx, ny = oy + dy;
                    if (!topo_map(&nx, &ny)) continue;
                    extra += (grid[ny][nx] > 0);
                }
        }
    }
    return extra;
}

/* ── Timeline (time-travel history) ────────────────────────────────────────── */

#define TL_MAX 256  /* max history frames (~40MB total) */

typedef struct {
    unsigned char grid[MAX_H][MAX_W];
    unsigned char ghost[MAX_H][MAX_W];
    unsigned char species[MAX_H][MAX_W];
    int generation;
    int population;
} TimelineFrame;

static TimelineFrame *timeline = NULL; /* heap-allocated ring buffer */
static int tl_head = 0;     /* next write position */
static int tl_len = 0;      /* number of stored frames */
static int replay_mode = 0; /* 0=live, 1=replaying history */
static int replay_pos = 0;  /* offset from newest: 0=newest, tl_len-1=oldest */
static int show_timeline = 1; /* show timeline bar */

static void timeline_init(void) {
    timeline = (TimelineFrame *)malloc(sizeof(TimelineFrame) * TL_MAX);
    if (!timeline) {
        fprintf(stderr, "Failed to allocate timeline buffer\n");
        exit(1);
    }
}

/* Save current grid state to timeline */
static void timeline_push(void) {
    if (!timeline) return;
    TimelineFrame *f = &timeline[tl_head];
    memcpy(f->grid, grid, sizeof(grid));
    memcpy(f->ghost, ghost, sizeof(ghost));
    memcpy(f->species, species, sizeof(species));
    f->generation = generation;
    f->population = population;
    tl_head = (tl_head + 1) % TL_MAX;
    if (tl_len < TL_MAX) tl_len++;
}

/* Get a historical frame: age=0 is newest, age=tl_len-1 is oldest */
static TimelineFrame *timeline_get(int age) {
    if (!timeline || age < 0 || age >= tl_len) return NULL;
    int idx = (tl_head - 1 - age + TL_MAX * 2) % TL_MAX;
    return &timeline[idx];
}

/* Load a historical frame into the live grid */
static void timeline_restore(int age) {
    TimelineFrame *f = timeline_get(age);
    if (!f) return;
    memcpy(grid, f->grid, sizeof(grid));
    memcpy(ghost, f->ghost, sizeof(ghost));
    memcpy(species, f->species, sizeof(species));
    generation = f->generation;
    population = f->population;
}

/* Truncate history newer than replay_pos (for branching) */
static void timeline_truncate_at(int age) {
    /* Keep only frames from age onward (discard newer ones) */
    if (age <= 0) return;
    /* Move head back by 'age' positions and reduce length */
    tl_head = (tl_head - age + TL_MAX * 2) % TL_MAX;
    tl_len -= age;
    if (tl_len < 0) tl_len = 0;
}

/* Clear timeline (on grid_clear/randomize) */
static void timeline_clear(void) {
    tl_head = 0;
    tl_len = 0;
    replay_mode = 0;
    replay_pos = 0;
}

/* ── Viewport (zoom + pan) ─────────────────────────────────────────────────── */

static int view_x = 0, view_y = 0;     /* top-left corner in grid coords */
static int view_w, view_h;             /* viewport size in grid cells */
static int term_cols, term_rows;        /* terminal dimensions */
static int zoom = 1; /* 1=normal(2ch/cell), 2=half-block(1ch/cell,2rows/char), 4=quarter */

static void viewport_update(void) {
    /* Compute how many grid cells fit in the terminal at current zoom */
    int usable_rows = term_rows - 3; /* 2 status lines + margin */
    if (usable_rows < 5) usable_rows = 5;

    if (zoom == 1) {
        view_w = term_cols / 2;
        view_h = usable_rows;
    } else if (zoom == 2) {
        /* half-block: 1 char per cell wide, 2 cells per row tall */
        view_w = term_cols;
        view_h = usable_rows * 2;
    } else if (zoom == 4) {
        /* quarter: 2 cells per char wide, 2 cells per row tall */
        view_w = term_cols * 2;
        view_h = usable_rows * 2;
    } else { /* zoom == 8 */
        /* braille: 2 cells per char wide, 4 cells per row tall */
        view_w = term_cols * 2;
        view_h = usable_rows * 4;
    }

    if (view_w > MAX_W) view_w = MAX_W;
    if (view_h > MAX_H) view_h = MAX_H;

    /* Clamp viewport position */
    if (view_x + view_w > W) view_x = W - view_w;
    if (view_y + view_h > H) view_y = H - view_h;
    if (view_x < 0) view_x = 0;
    if (view_y < 0) view_y = 0;
}

static void viewport_center(void) {
    view_x = (W - view_w) / 2;
    view_y = (H - view_h) / 2;
    if (view_x < 0) view_x = 0;
    if (view_y < 0) view_y = 0;
}

static void viewport_pan(int dx, int dy) {
    int step = (zoom == 1) ? 4 : (zoom == 2) ? 8 : (zoom == 4) ? 16 : 32;
    view_x += dx * step;
    view_y += dy * step;
    if (view_x + view_w > W) view_x = W - view_w;
    if (view_y + view_h > H) view_y = H - view_h;
    if (view_x < 0) view_x = 0;
    if (view_y < 0) view_y = 0;
}

/* ── Population history (for sparkline) ────────────────────────────────────── */

#define HIST_LEN 120
static int pop_history[HIST_LEN];
static int pop_history_a[HIST_LEN]; /* species A population history */
static int pop_history_b[HIST_LEN]; /* species B population history */
static int hist_pos = 0;
static int hist_count = 0;
static int show_graph = 1;
static int dashboard_mode = 0; /* 0=off, 1=population dynamics dashboard */

static void hist_push(int pop) {
    pop_history[hist_pos] = pop;
    /* Count per-species populations */
    if (ecosystem_mode) {
        int ca = 0, cb = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (grid[y][x]) {
                    if (species[y][x] == 1) ca++;
                    else if (species[y][x] == 2) cb++;
                }
        pop_history_a[hist_pos] = ca;
        pop_history_b[hist_pos] = cb;
    } else {
        pop_history_a[hist_pos] = 0;
        pop_history_b[hist_pos] = 0;
    }
    hist_pos = (hist_pos + 1) % HIST_LEN;
    if (hist_count < HIST_LEN) hist_count++;
}

static int hist_get(int age) {
    /* age=0 is most recent */
    int idx = (hist_pos - 1 - age + HIST_LEN * 2) % HIST_LEN;
    return pop_history[idx];
}

static int hist_get_a(int age) {
    int idx = (hist_pos - 1 - age + HIST_LEN * 2) % HIST_LEN;
    return pop_history_a[idx];
}

static int hist_get_b(int age) {
    int idx = (hist_pos - 1 - age + HIST_LEN * 2) % HIST_LEN;
    return pop_history_b[idx];
}

/* ── Rule system (B/S notation via bitmasks) ───────────────────────────────── */

/*
 * birth_mask:    bit N set → dead cell with N neighbors is born
 * survival_mask: bit N set → live cell with N neighbors survives
 * Conway's Life = B3/S23 → birth=0x008, survival=0x00C
 */
static unsigned short birth_mask    = 0x008; /* B3 */
static unsigned short survival_mask = 0x00C; /* S23 */

typedef struct {
    const char *name;
    unsigned short birth;
    unsigned short survival;
} Ruleset;

static const Ruleset rulesets[] = {
    { "Conway",     0x008, 0x00C },  /* B3/S23     — the classic */
    { "HighLife",   0x048, 0x00C },  /* B36/S23    — replicators */
    { "Day&Night",  0x1C8, 0x1D8 },  /* B3678/S34678 — symmetric */
    { "Seeds",      0x004, 0x000 },  /* B2/S       — explosive */
    { "Diamoeba",   0x1E8, 0x1E0 },  /* B35678/S5678 — amoeba blobs */
    { "Morley",     0x148, 0x034 },  /* B368/S245  — stable+chaotic */
    { "2x2",        0x048, 0x026 },  /* B36/S125   — blocks split */
    { "Maze",       0x008, 0x03E },  /* B3/S12345  — maze-like growth */
    { "Coral",      0x008, 0x1F0 },  /* B3/S45678  — slow coral growth */
    { "Anneal",     0x1D0, 0x1E8 },  /* B4678/S35678 — annealing */
};
#define N_RULESETS (int)(sizeof(rulesets) / sizeof(rulesets[0]))
static int current_ruleset = 0;

/* Format current rule as B.../S... string */
static void rule_to_string(char *buf, int buflen) {
    char *p = buf;
    char *end = buf + buflen - 1;
    *p++ = 'B';
    for (int i = 0; i <= 8 && p < end; i++)
        if (birth_mask & (1 << i)) *p++ = '0' + i;
    *p++ = '/';
    *p++ = 'S';
    for (int i = 0; i <= 8 && p < end; i++)
        if (survival_mask & (1 << i)) *p++ = '0' + i;
    *p = '\0';
}

/* Check if current masks match a named preset */
static int find_matching_ruleset(void) {
    for (int i = 0; i < N_RULESETS; i++)
        if (rulesets[i].birth == birth_mask && rulesets[i].survival == survival_mask)
            return i;
    return -1; /* custom / mutated */
}

/* ── Grid operations ───────────────────────────────────────────────────────── */

static void grid_clear(void) {
    memset(grid, 0, sizeof(grid));
    memset(ghost, 0, sizeof(ghost));
    memset(tracer, 0, sizeof(tracer));
    memset(species, 0, sizeof(species));
    /* zones and temperature are preserved across clear — use 'c' twice to reset */
    generation = 0;
    population = 0;
    hist_count = 0;
    hist_pos = 0;
    timeline_clear();
}

static void zones_clear(void) {
    memset(zone, 0, sizeof(zone));
    zone_enabled = 0;
}

static void grid_randomize(double density) {
    grid_clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if ((double)rand() / RAND_MAX < density) {
                grid[y][x] = 1;
                if (ecosystem_mode)
                    species[y][x] = (unsigned char)((rand() % 2) + 1);
                population++;
            }
}

/* Map coordinates through the current topology.
 * Returns 1 if the coordinate is valid (possibly transformed), 0 if out-of-bounds.
 *
 * Topologies and their edge identifications:
 *   Flat:       no wrapping — edges are boundaries
 *   Torus:      left↔right, top↔bottom (standard wrapping)
 *   Klein:      left↔right normal, top↔bottom WITH horizontal flip (x → W-1-x)
 *   Möbius:     left↔right WITH vertical flip (y → H-1-y), top↔bottom are boundaries
 *   Projective: left↔right WITH vertical flip, top↔bottom WITH horizontal flip
 */
static inline int topo_map(int *nx, int *ny) {
    int x = *nx, y = *ny;
    if (topology == TOPO_FLAT) {
        return (x >= 0 && x < W && y >= 0 && y < H);
    }
    if (topology == TOPO_TORUS) {
        if (x < 0) x += W; else if (x >= W) x -= W;
        if (y < 0) y += H; else if (y >= H) y -= H;
    } else if (topology == TOPO_KLEIN) {
        /* Left/right: normal wrap */
        if (x < 0) x += W; else if (x >= W) x -= W;
        /* Top/bottom: wrap with horizontal flip */
        if (y < 0) { y += H; x = W - 1 - x; }
        else if (y >= H) { y -= H; x = W - 1 - x; }
    } else if (topology == TOPO_MOBIUS) {
        /* Left/right: wrap with vertical flip */
        if (x < 0) { x += W; y = H - 1 - y; }
        else if (x >= W) { x -= W; y = H - 1 - y; }
        /* Top/bottom: boundaries (Möbius strip has open edges) */
        if (y < 0 || y >= H) return 0;
    } else if (topology == TOPO_PROJECTIVE) {
        /* Left/right: wrap with vertical flip */
        if (x < 0) { x += W; y = H - 1 - y; }
        else if (x >= W) { x -= W; y = H - 1 - y; }
        /* Top/bottom: wrap with horizontal flip */
        if (y < 0) { y += H; x = W - 1 - x; }
        else if (y >= H) { y -= H; x = W - 1 - x; }
    }
    *nx = x; *ny = y;
    return 1;
}

static void grid_step(void) {
    population = 0;
    memset(next_grid, 0, sizeof(next_grid));
    if (ecosystem_mode) memset(next_species, 0, sizeof(next_species));

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int n = 0;
            int n_a = 0, n_b = 0; /* per-species neighbor counts (ecosystem mode) */
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (!topo_map(&nx, &ny)) continue;
                    if (grid[ny][nx] > 0) {
                        n++;
                        if (ecosystem_mode) {
                            if (species[ny][nx] == 1) n_a++;
                            else n_b++;
                        }
                    }
                }
            /* Add portal neighbor contributions (non-local coupling) */
            if (n_portals > 0)
                n += portal_neighbor_count(x, y);

            if (ecosystem_mode) {
                int alive = grid[y][x] > 0;
                int my_sp = alive ? species[y][x] : 0;
                /* Effective temperature for this cell */
                float T_eco = temp_global + temp_grid[y][x];
                if (T_eco > 1.0f) T_eco = 1.0f;
                int t_flip = (T_eco > 0.0f && (float)rand() / (float)RAND_MAX < T_eco);

                if (alive) {
                    /* Survival: use this species' rules with interaction-weighted neighbors */
                    int same = (my_sp == 1) ? n_a : n_b;
                    int cross = (my_sp == 1) ? n_b : n_a;
                    float eff_f = same + interaction * cross;
                    int eff = (int)(eff_f >= 0 ? eff_f + 0.5f : eff_f - 0.5f);
                    if (eff < 0) eff = 0;
                    if (eff > 15) eff = 15;
                    unsigned short s_mask = (my_sp == 1) ? species_a_survival : species_b_survival;
                    int survives = (s_mask & (1 << eff)) != 0;
                    if (t_flip) survives = !survives;
                    if (survives) {
                        int age = grid[y][x];
                        next_grid[y][x] = (age < 255) ? age + 1 : 255;
                        next_species[y][x] = (unsigned char)my_sp;
                        population++;
                    }
                } else {
                    /* Birth: check both species' rules; dominant neighbor count wins ties */
                    float fa = n_a + interaction * n_b;
                    float fb = n_b + interaction * n_a;
                    int eff_a = (int)(fa >= 0 ? fa + 0.5f : fa - 0.5f);
                    int eff_b = (int)(fb >= 0 ? fb + 0.5f : fb - 0.5f);
                    if (eff_a < 0) eff_a = 0;
                    if (eff_a > 15) eff_a = 15;
                    if (eff_b < 0) eff_b = 0;
                    if (eff_b > 15) eff_b = 15;
                    int born_a = (species_a_birth & (1 << eff_a)) != 0;
                    int born_b = (species_b_birth & (1 << eff_b)) != 0;
                    /* Temperature can spontaneously ignite or suppress birth */
                    if (t_flip) { born_a = !born_a; born_b = !born_b; }
                    if (born_a && born_b) {
                        /* Both species want to birth — majority wins, random tiebreak */
                        next_species[y][x] = (n_a > n_b) ? 1 : (n_b > n_a) ? 2 : (unsigned char)((rand() % 2) + 1);
                        next_grid[y][x] = 1;
                        population++;
                    } else if (born_a) {
                        next_species[y][x] = 1;
                        next_grid[y][x] = 1;
                        population++;
                    } else if (born_b) {
                        next_species[y][x] = 2;
                        next_grid[y][x] = 1;
                        population++;
                    }
                }
            } else {
                int alive = grid[y][x] > 0;
                /* Use per-cell zone rules if zones are active, else global */
                unsigned short b_mask = birth_mask;
                unsigned short s_mask = survival_mask;
                if (zone_enabled) {
                    int zi = zone[y][x];
                    if (zi < N_RULESETS) {
                        b_mask = rulesets[zi].birth;
                        s_mask = rulesets[zi].survival;
                    }
                }
                int outcome = (alive && (s_mask & (1 << n))) ||
                              (!alive && (b_mask & (1 << n)));
                /* Stochastic temperature: flip outcome with probability T */
                {
                    float T = temp_global + temp_grid[y][x];
                    if (T > 1.0f) T = 1.0f;
                    if (T > 0.0f && (float)rand() / (float)RAND_MAX < T)
                        outcome = !outcome;
                }
                if (outcome) {
                    /* alive: increment age, cap at 255 */
                    int age = grid[y][x];
                    next_grid[y][x] = (age < 255) ? age + 1 : 255;
                    population++;
                }
            }
        }
    }

    /* Update ghost trails */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid[y][x] && !next_grid[y][x]) {
                /* cell just died — start ghost trail */
                ghost[y][x] = GHOST_FRAMES;
            } else if (next_grid[y][x]) {
                /* cell is alive — no ghost */
                ghost[y][x] = 0;
            } else if (ghost[y][x] > 0) {
                /* existing ghost — decay */
                ghost[y][x]--;
            }
        }
    }

    memcpy(grid, next_grid, sizeof(grid));
    if (ecosystem_mode) memcpy(species, next_species, sizeof(species));
    generation++;

    /* Accumulate signal tracer */
    if (tracer_mode == 1) {
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                if (grid[y][x] && tracer[y][x] < 255)
                    tracer[y][x]++;
    }

    /* Apply emitters and absorbers after step */
    if (n_emitters > 0 || n_absorbers > 0)
        apply_sources_sinks();

    hist_push(population);
    timeline_push();
    freq_stale = 1;
    census_stale = 1;
    entropy_stale = 1;
    lyapunov_stale = 1;
    fourier_stale = 1;
    fractal_stale = 1;
    wolfram_stale = 1;
    flow_stale = 1;
    attractor_stale = 1;
    surp_stale = 1;
    mi_stale = 1;
    cplx_stale = 1;
    topo_stale = 1;
    rg_stale = 1;
    kc_stale = 1;

    /* Always record frames for surprise field (cheap memcpy) */
    surp_record_frame();
}

/* ── Census scan implementation ───────────────────────────────────────────── */
static void census_scan(void) {
    memset(census_counts, 0, sizeof(census_counts));
    memset(census_claimed, 0, sizeof(census_claimed));

    /* For each pattern, scan all possible positions */
    for (int pi = 0; pi < CENSUS_MAX_PAT; pi++) {
        const CensusPattern *pat = &census_patterns[pi];
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int match = 1;

                /* Skip if any pattern cell already claimed */
                for (int py = 0; py < pat->h && match; py++)
                    for (int px = 0; px < pat->w && match; px++)
                        if (pat->bits[py][px] && census_claimed[(y+py) % H][(x+px) % W])
                            match = 0;
                if (!match) continue;

                /* Check pattern interior: exact alive/dead match */
                for (int py = 0; py < pat->h && match; py++) {
                    for (int px = 0; px < pat->w && match; px++) {
                        int gy = (y + py) % H;
                        int gx = (x + px) % W;
                        int alive = (grid[gy][gx] > 0) ? 1 : 0;
                        if (alive != pat->bits[py][px])
                            match = 0;
                    }
                }
                if (!match) continue;

                /* Check 1-cell dead border (top/bottom rows) */
                for (int px = -1; px <= pat->w && match; px++) {
                    int gx2 = (x + px + W) % W;
                    int gy_top = (y - 1 + H) % H;
                    int gy_bot = (y + pat->h) % H;
                    if (topology != TOPO_FLAT || y > 0)
                        if (grid[gy_top][gx2]) match = 0;
                    if (topology != TOPO_FLAT || y + pat->h < H)
                        if (grid[gy_bot][gx2]) match = 0;
                }
                /* Check left/right border columns */
                for (int py = 0; py < pat->h && match; py++) {
                    int gy2 = (y + py) % H;
                    int gx_left = (x - 1 + W) % W;
                    int gx_right = (x + pat->w) % W;
                    if (topology != TOPO_FLAT || x > 0)
                        if (grid[gy2][gx_left]) match = 0;
                    if (topology != TOPO_FLAT || x + pat->w < W)
                        if (grid[gy2][gx_right]) match = 0;
                }
                if (!match) continue;

                /* Pattern matched — claim cells and count */
                census_counts[pi]++;
                for (int py = 0; py < pat->h; py++)
                    for (int px = 0; px < pat->w; px++)
                        if (pat->bits[py][px])
                            census_claimed[(y+py) % H][(x+px) % W] = 1;
            }
        }
    }

    /* Build display: merge patterns with same name (e.g. both blinker phases) */
    census_n_display = 0;
    census_total = 0;
    for (int pi = 0; pi < CENSUS_MAX_PAT; pi++) {
        if (census_counts[pi] == 0) continue;
        int found = -1;
        for (int d = 0; d < census_n_display; d++) {
            if (strcmp(census_display[d].name, census_patterns[pi].name) == 0) {
                found = d; break;
            }
        }
        if (found >= 0) {
            census_display[found].count += census_counts[pi];
        } else if (census_n_display < CENSUS_DISPLAY_MAX) {
            census_display[census_n_display].name = census_patterns[pi].name;
            census_display[census_n_display].count = census_counts[pi];
            /* Determine type: still life (0) or oscillator (1) */
            int is_osc = (strcmp(census_patterns[pi].name, "Blinker") == 0 ||
                          strcmp(census_patterns[pi].name, "Toad") == 0 ||
                          strcmp(census_patterns[pi].name, "Beacon") == 0 ||
                          strcmp(census_patterns[pi].name, "Clock") == 0);
            census_display[census_n_display].type = is_osc ? 1 : 0;
            census_n_display++;
        }
        census_total += census_counts[pi];
    }

    /* Count unclaimed live cells */
    census_unmatched = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (grid[y][x] && !census_claimed[y][x])
                census_unmatched++;

    census_stale = 0;

    /* Run spaceship detection after static pattern census */
    census_ship_scan();
}

/* ── Spaceship detection: compare consecutive timeline frames ─────────────── */
static int ship_match_at(const unsigned char frame[MAX_H][MAX_W],
                         const ShipPhase *ph, int x, int y) {
    /* Check pattern interior: exact alive/dead match */
    for (int py = 0; py < ph->h; py++) {
        for (int px = 0; px < ph->w; px++) {
            int gy = (y + py) % H;
            int gx = (x + px) % W;
            if (gy < 0) gy += H;
            if (gx < 0) gx += W;
            int alive = (frame[gy][gx] > 0) ? 1 : 0;
            if (alive != ph->bits[py][px])
                return 0;
        }
    }
    /* Check 1-cell dead border (top/bottom rows) */
    for (int px = -1; px <= ph->w; px++) {
        int gx2 = (x + px + W) % W;
        int gy_top = (y - 1 + H) % H;
        int gy_bot = (y + ph->h) % H;
        if (topology != TOPO_FLAT || y > 0)
            if (frame[gy_top][gx2]) return 0;
        if (topology != TOPO_FLAT || y + ph->h < H)
            if (frame[gy_bot][gx2]) return 0;
    }
    /* Check left/right border columns */
    for (int py = 0; py < ph->h; py++) {
        int gy2 = (y + py) % H;
        int gx_left = (x - 1 + W) % W;
        int gx_right = (x + ph->w) % W;
        if (topology != TOPO_FLAT || x > 0)
            if (frame[gy2][gx_left]) return 0;
        if (topology != TOPO_FLAT || x + ph->w < W)
            if (frame[gy2][gx_right]) return 0;
    }
    return 1;
}

static void census_ship_scan(void) {
    memset(census_ship, 0, sizeof(census_ship));
    memset(ship_counts, 0, sizeof(ship_counts));
    ship_n_detections = 0;

    /* Need at least 2 frames to detect motion */
    if (tl_len < 2) return;

    /* Frame 0 = current (newest), Frame 1 = previous generation */
    TimelineFrame *f_cur = timeline_get(0);
    TimelineFrame *f_prev = timeline_get(1);
    if (!f_cur || !f_prev) return;

    /* For each spaceship type, try all 4 rotations/reflections of displacement
       to detect all 4 (or 8) travel directions */
    for (int ti = 0; ti < SHIP_MAX_TYPES; ti++) {
        const ShipType *st = &ship_types[ti];
        int n_ph = st->n_phases;

        /* Try matching each phase in the previous frame, then confirm the
           next phase (with displacement) in the current frame.
           For a glider with 4 phases and displacement (1,1) per full cycle:
           - Match phase P at (x,y) in f_prev
           - Expect phase (P+1)%4 in f_cur
           - Per-step displacement = (dx/n_phases, dy/n_phases) — but for gliders
             the displacement happens non-uniformly across phases.
           Instead: just try all 8 reflections of each phase pattern and confirm
           by checking whether the NEXT phase (reflected) appears shifted by 1 step. */

        /* For simplicity and correctness: try matching phase 0 in f_prev,
           then check phase 1 at the same position in f_cur. The displacement
           between consecutive phases is effectively (0,0) for the bounding box
           because the pattern slides within or just outside its bounding box.
           The TRUE approach: match ANY phase in prev frame, then match next phase
           in cur frame allowing small positional shifts (-1..+1 in each axis). */

        for (int phase = 0; phase < n_ph; phase++) {
            const ShipPhase *cur_phase_pat = &st->phases[(phase + 1) % n_ph];
            const ShipPhase *prev_phase_pat = &st->phases[phase];

            for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                    /* Skip if cells already claimed by static census or another ship */
                    int already = 0;
                    for (int py = 0; py < prev_phase_pat->h && !already; py++)
                        for (int px = 0; px < prev_phase_pat->w && !already; px++)
                            if (prev_phase_pat->bits[py][px] &&
                                census_claimed[(y+py) % H][(x+px) % W])
                                already = 1;
                    if (already) continue;

                    /* Match this phase in the previous frame */
                    if (!ship_match_at(f_prev->grid, prev_phase_pat, x, y))
                        continue;

                    /* Try to find next phase in current frame at nearby offsets */
                    int found = 0;
                    int best_ox = 0, best_oy = 0;
                    for (int oy = -2; oy <= 2 && !found; oy++) {
                        for (int ox = -2; ox <= 2 && !found; ox++) {
                            int nx = (x + ox + W) % W;
                            int ny = (y + oy + H) % H;
                            if (ship_match_at(f_cur->grid, cur_phase_pat, nx, ny)) {
                                found = 1;
                                best_ox = ox;
                                best_oy = oy;
                            }
                        }
                    }

                    if (!found) continue;

                    /* Compute effective displacement direction.
                       For full-cycle displacement, scale by n_phases.
                       But single-step displacement is what we actually observed. */
                    int eff_dx = best_ox;
                    int eff_dy = best_oy;
                    /* If displacement is (0,0) this phase, use the canonical direction */
                    if (eff_dx == 0 && eff_dy == 0) {
                        eff_dx = st->dx;
                        eff_dy = st->dy;
                    }

                    int dir = ship_dir_from_delta(eff_dx, eff_dy);

                    /* Record detection — mark cells in CURRENT frame */
                    int cx = (x + best_ox + W) % W;
                    int cy = (y + best_oy + H) % H;
                    if (ship_n_detections < SHIP_DETECT_MAX) {
                        ship_detections[ship_n_detections].x = cx;
                        ship_detections[ship_n_detections].y = cy;
                        ship_detections[ship_n_detections].w = cur_phase_pat->w;
                        ship_detections[ship_n_detections].h = cur_phase_pat->h;
                        ship_detections[ship_n_detections].dir = dir;
                        ship_detections[ship_n_detections].type_idx = ti;
                        ship_n_detections++;
                    }
                    ship_counts[ti]++;

                    /* Claim cells in current frame */
                    for (int py = 0; py < cur_phase_pat->h; py++)
                        for (int px = 0; px < cur_phase_pat->w; px++)
                            if (cur_phase_pat->bits[py][px]) {
                                int gy = (cy + py) % H;
                                int gx = (cx + px) % W;
                                census_ship[gy][gx] = 1;
                                census_claimed[gy][gx] = 1;
                            }
                }
            }
        }
    }

    /* Add spaceship counts to census_display */
    for (int ti = 0; ti < SHIP_MAX_TYPES; ti++) {
        if (ship_counts[ti] == 0) continue;
        if (census_n_display < CENSUS_DISPLAY_MAX) {
            census_display[census_n_display].name = ship_types[ti].name;
            census_display[census_n_display].count = ship_counts[ti];
            census_display[census_n_display].type = 2; /* spaceship */
            census_n_display++;
        }
        census_total += ship_counts[ti];
    }

    /* Recount unmatched (some cells may now be claimed by spaceships) */
    census_unmatched = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (grid[y][x] && !census_claimed[y][x])
                census_unmatched++;
}

/* ── Genetic Rule Explorer — evaluation & evolution ───────────────────────── */

static void gene_format_rule(char *buf, int buflen, unsigned short b, unsigned short s) {
    char *p = buf;
    char *end = buf + buflen - 1;
    *p++ = 'B';
    for (int i = 0; i <= 8 && p < end; i++)
        if (b & (1 << i)) *p++ = '0' + i;
    *p++ = '/';
    *p++ = 'S';
    for (int i = 0; i <= 8 && p < end; i++)
        if (s & (1 << i)) *p++ = '0' + i;
    *p = '\0';
}

/* Evaluate a rule's "interestingness" via mini-simulation */
static float gene_evaluate(unsigned short b, unsigned short s) {
    static unsigned char mg[GENEVAL_H][GENEVAL_W];
    static unsigned char mg2[GENEVAL_H][GENEVAL_W];

    /* Seed the mini-grid with R-pentomino + random 10% fill in center region */
    memset(mg, 0, sizeof(mg));
    int cx = GENEVAL_W / 2, cy = GENEVAL_H / 2;
    /* R-pentomino */
    mg[cy-1][cx] = 1; mg[cy-1][cx+1] = 1;
    mg[cy][cx-1] = 1; mg[cy][cx] = 1;
    mg[cy+1][cx] = 1;
    /* Random scatter in center 20x20 */
    for (int dy = -10; dy < 10; dy++)
        for (int dx = -10; dx < 10; dx++)
            if (rand() % 10 == 0)
                mg[cy+dy][cx+dx] = 1;

    int pop_history[GENEVAL_STEPS];
    memset(pop_history, 0, sizeof(pop_history));
    int pop = 0;
    for (int y = 0; y < GENEVAL_H; y++)
        for (int x = 0; x < GENEVAL_W; x++)
            if (mg[y][x]) pop++;
    pop_history[0] = pop;

    /* Run simulation */
    for (int step = 1; step < GENEVAL_STEPS; step++) {
        memset(mg2, 0, sizeof(mg2));
        pop = 0;
        for (int y = 0; y < GENEVAL_H; y++) {
            for (int x = 0; x < GENEVAL_W; x++) {
                int n = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int ny = y + dy, nx = x + dx;
                        if (ny < 0 || ny >= GENEVAL_H || nx < 0 || nx >= GENEVAL_W) continue;
                        if (mg[ny][nx]) n++;
                    }
                int alive = mg[y][x];
                if ((alive && (s & (1 << n))) || (!alive && (b & (1 << n)))) {
                    mg2[y][x] = 1;
                    pop++;
                }
            }
        }
        memcpy(mg, mg2, sizeof(mg));
        pop_history[step] = pop;

        /* Early exit: extinction or explosion */
        if (pop == 0) break;
        if (pop > (GENEVAL_W * GENEVAL_H * 3) / 4) break;
    }

    /* ── Scoring ── */
    float score = 0.0f;

    /* Count valid steps first */
    int valid_steps = 1; /* at least step 0 */
    for (int i = 1; i < GENEVAL_STEPS; i++) {
        if (pop_history[i] == 0 && i > 0) break;
        valid_steps = i + 1;
    }

    /* 1. Population stability: penalize extinction and explosion */
    int final_pop = pop_history[valid_steps - 1];
    int total_cells = GENEVAL_W * GENEVAL_H;
    if (final_pop == 0) return 0.0f;  /* boring: everything dies */
    if (final_pop > total_cells * 3 / 4) return 0.0f;  /* boring: fills up */

    /* Prefer populations in 5-50% range */
    float density = (float)final_pop / total_cells;
    if (density >= 0.05f && density <= 0.50f)
        score += 20.0f;
    else
        score += 5.0f;

    /* 2. Population variance — measure dynamism */
    float mean_pop = 0;
    for (int i = 0; i < valid_steps; i++)
        mean_pop += pop_history[i];
    mean_pop /= valid_steps;
    float variance = 0;
    for (int i = 0; i < valid_steps; i++) {
        float d = pop_history[i] - mean_pop;
        variance += d * d;
    }
    variance /= valid_steps;
    float cv = (mean_pop > 0) ? sqrtf(variance) / mean_pop : 0; /* coefficient of variation */
    /* Sweet spot: some variation (0.05-0.5) is interesting, too stable or too wild is less so */
    if (cv >= 0.02f && cv <= 0.5f)
        score += 30.0f * (1.0f - fabsf(cv - 0.15f) / 0.5f);
    else if (cv > 0.5f)
        score += 5.0f;

    /* 3. Oscillation detection: compare late-phase snapshots */
    if (valid_steps >= 50) {
        int matches = 0;
        for (int period = 1; period <= 12; period++) {
            int pops_match = 0;
            for (int i = valid_steps - 20; i < valid_steps; i++) {
                if (i - period >= 0 && pop_history[i] == pop_history[i - period])
                    pops_match++;
            }
            if (pops_match >= 15) matches++; /* likely periodic */
        }
        score += matches * 5.0f; /* bonus for periodicity */
    }

    /* 4. Longevity bonus: survived longer = more interesting */
    score += (float)valid_steps / GENEVAL_STEPS * 15.0f;

    /* 5. Structural complexity: count distinct population values in last 50 steps */
    if (valid_steps > 50) {
        int distinct = 0;
        int seen[256]; memset(seen, 0, sizeof(seen));
        for (int i = valid_steps - 50; i < valid_steps; i++) {
            int bucket = pop_history[i] % 256;
            if (!seen[bucket]) { seen[bucket] = 1; distinct++; }
        }
        score += (distinct > 20 ? 20 : distinct) * 0.5f;
    }

    return score;
}

/* Mutate a rule: flip 1-3 random bits */
static void gene_mutate(unsigned short *b, unsigned short *s, int strength) {
    int flips = 1 + (rand() % strength);
    for (int i = 0; i < flips; i++) {
        int which = rand() % 18;
        if (which < 9)
            *b ^= (unsigned short)(1 << which);
        else
            *s ^= (unsigned short)(1 << (which - 9));
    }
}

/* Crossover: combine two parents' B/S masks */
static void gene_crossover(unsigned short b1, unsigned short s1,
                           unsigned short b2, unsigned short s2,
                           unsigned short *bo, unsigned short *so) {
    /* Uniform crossover: each bit randomly from parent 1 or 2 */
    *bo = 0; *so = 0;
    for (int i = 0; i <= 8; i++) {
        if (rand() % 2)
            *bo |= (b1 & (unsigned short)(1 << i));
        else
            *bo |= (b2 & (unsigned short)(1 << i));
        if (rand() % 2)
            *so |= (s1 & (unsigned short)(1 << i));
        else
            *so |= (s2 & (unsigned short)(1 << i));
    }
}

/* Run one evolution generation */
static void gene_search_step(void) {
    GeneCandidate pool[GENPOP_SIZE];

    if (gene_gen == 0) {
        /* First generation: seed from current rule + random mutations + random rules */
        for (int i = 0; i < GENPOP_SIZE; i++) {
            if (i == 0) {
                /* Current rule as-is */
                pool[i].birth = birth_mask;
                pool[i].survival = survival_mask;
            } else if (i < GENPOP_SIZE / 2) {
                /* Mutations of current rule */
                pool[i].birth = birth_mask;
                pool[i].survival = survival_mask;
                gene_mutate(&pool[i].birth, &pool[i].survival, 1 + (i / 4));
            } else if (i < GENPOP_SIZE * 3 / 4) {
                /* Mutations of known interesting presets */
                int pi = rand() % N_RULESETS;
                pool[i].birth = rulesets[pi].birth;
                pool[i].survival = rulesets[pi].survival;
                gene_mutate(&pool[i].birth, &pool[i].survival, 2);
            } else {
                /* Fully random */
                pool[i].birth = (unsigned short)(rand() & 0x1FF);    /* bits 0-8 */
                pool[i].survival = (unsigned short)(rand() & 0x1FF);
            }
        }
    } else {
        /* Breed from previous best */
        for (int i = 0; i < GENPOP_SIZE; i++) {
            if (i < GENBEST_SIZE) {
                /* Keep the elites with minor mutation */
                pool[i].birth = gene_best[i].birth;
                pool[i].survival = gene_best[i].survival;
                if (i > 0) gene_mutate(&pool[i].birth, &pool[i].survival, 1);
            } else if (i < GENPOP_SIZE * 3 / 4) {
                /* Crossover of two random elites + mutation */
                int p1 = rand() % GENBEST_SIZE;
                int p2 = rand() % GENBEST_SIZE;
                gene_crossover(gene_best[p1].birth, gene_best[p1].survival,
                               gene_best[p2].birth, gene_best[p2].survival,
                               &pool[i].birth, &pool[i].survival);
                gene_mutate(&pool[i].birth, &pool[i].survival, 1);
            } else {
                /* Fresh random to maintain diversity */
                pool[i].birth = (unsigned short)(rand() & 0x1FF);
                pool[i].survival = (unsigned short)(rand() & 0x1FF);
            }
        }
    }

    /* Evaluate all candidates */
    for (int i = 0; i < GENPOP_SIZE; i++) {
        pool[i].score = gene_evaluate(pool[i].birth, pool[i].survival);
        gene_format_rule(pool[i].label, sizeof(pool[i].label), pool[i].birth, pool[i].survival);
    }

    /* Selection sort top GENBEST_SIZE */
    for (int i = 0; i < GENBEST_SIZE; i++) {
        int best = i;
        for (int j = i + 1; j < GENPOP_SIZE; j++)
            if (pool[j].score > pool[best].score) best = j;
        if (best != i) {
            GeneCandidate tmp = pool[i];
            pool[i] = pool[best];
            pool[best] = tmp;
        }
        gene_best[i] = pool[i];
    }

    gene_gen++;
    gene_search_done = 1;
}

static void grid_set(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H && !grid[y][x]) {
        grid[y][x] = 1;
        ghost[y][x] = 0;
        if (ecosystem_mode) species[y][x] = (unsigned char)brush_species;
        population++;
    }
}

static void grid_unset(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H && grid[y][x]) {
        grid[y][x] = 0;
        ghost[y][x] = GHOST_FRAMES;
        species[y][x] = 0;
        population--;
    }
}

/* ── Symmetric drawing ─────────────────────────────────────────────────────── */

static void sym_apply(int gx, int gy, void (*fn)(int, int)) {
    int cx = W / 2, cy = H / 2;
    int dx = gx - cx, dy = gy - cy;

    fn(gx, gy); /* always draw the original point */

    if (symmetry >= 1) {
        /* 2-fold: vertical mirror */
        fn(cx - dx, gy);
    }
    if (symmetry >= 2) {
        /* 4-fold: + horizontal mirror of both */
        fn(gx, cy - dy);
        fn(cx - dx, cy - dy);
    }
    if (symmetry >= 3) {
        /* 8-fold: + diagonal reflections (swap dx,dy) */
        fn(cx + dy, cy + dx);
        fn(cx - dy, cy + dx);
        fn(cx + dy, cy - dx);
        fn(cx - dy, cy - dx);
    }
}

/* ── Patterns ──────────────────────────────────────────────────────────────── */

typedef struct { int dx, dy; } Offset;

static void place_pattern(const Offset *pat, int n) {
    grid_clear();
    int cx = W / 2, cy = H / 2;
    for (int i = 0; i < n; i++)
        grid_set(cx + pat[i].dx, cy + pat[i].dy);
}

/* 1: Glider */
static const Offset pat_glider[] = {
    {0,0},{1,0},{2,0},{2,-1},{1,-2}
};

/* 2: Pulsar (quarter + reflect) */
static Offset pat_pulsar[48];
static int pat_pulsar_n;

static void build_pulsar(void) {
    static const Offset q[] = {
        {2,1},{3,1},{4,1},
        {1,2},{1,3},{1,4},
        {6,2},{6,3},{6,4},
        {2,6},{3,6},{4,6},
    };
    pat_pulsar_n = 0;
    for (int i = 0; i < 12; i++) {
        int signs[4][2] = {{1,1},{-1,1},{1,-1},{-1,-1}};
        for (int s = 0; s < 4; s++) {
            int dx = q[i].dx * signs[s][0];
            int dy = q[i].dy * signs[s][1];
            /* deduplicate */
            int dup = 0;
            for (int j = 0; j < pat_pulsar_n; j++)
                if (pat_pulsar[j].dx == dx && pat_pulsar[j].dy == dy) { dup = 1; break; }
            if (!dup)
                pat_pulsar[pat_pulsar_n++] = (Offset){dx, dy};
        }
    }
}

/* 3: Gosper Glider Gun */
static const Offset pat_gun[] = {
    {0,4},{0,5},{1,4},{1,5},
    {10,4},{10,5},{10,6},{11,3},{11,7},{12,2},{12,8},{13,2},{13,8},
    {14,5},{15,3},{15,7},{16,4},{16,5},{16,6},{17,5},
    {20,2},{20,3},{20,4},{21,2},{21,3},{21,4},{22,1},{22,5},
    {24,0},{24,1},{24,5},{24,6},
    {34,2},{34,3},{35,2},{35,3},
};

/* 4: R-pentomino */
static const Offset pat_rpent[] = {
    {0,0},{1,0},{-1,0},{0,-1},{1,1}
};

/* 5: Acorn */
static const Offset pat_acorn[] = {
    {0,0},{1,0},{-3,0},{1,1},{-1,-1},{1,-1},{2,-1}
};

static void load_pattern(int id) {
    switch (id) {
        case 1: place_pattern(pat_glider, 5); break;
        case 2: place_pattern(pat_pulsar, pat_pulsar_n); break;
        case 3: place_pattern(pat_gun, 36); break;
        case 4: place_pattern(pat_rpent, 5); break;
        case 5: place_pattern(pat_acorn, 7); break;
    }
}

/* ── Stamp Tool (pattern library with rotation) ────────────────────────────── */

#define MAX_STAMP_CELLS 128
#define N_STAMPS 20

typedef struct {
    const char *name;
    int n;                              /* number of cells */
    Offset cells[MAX_STAMP_CELLS];      /* relative coords */
} StampPattern;

/* Pre-rotated versions are computed at init */
typedef struct {
    int n;
    Offset cells[MAX_STAMP_CELLS];
    int w, h;                           /* bounding box */
} RotatedStamp;

static RotatedStamp stamp_rotations[N_STAMPS][4]; /* [pattern][rotation] */
static int stamp_mode = 0;       /* 0=off, 1=stamp placement mode */
static int stamp_sel = 0;        /* selected pattern index */
static int stamp_rot = 0;        /* rotation: 0=0°, 1=90°, 2=180°, 3=270° */

/* Raw pattern definitions (coordinates relative to origin) */
static const StampPattern stamp_library[N_STAMPS] = {
    /* ── Still lifes ── */
    { "Block", 4, {{0,0},{1,0},{0,1},{1,1}} },
    { "Beehive", 6, {{1,0},{2,0},{0,1},{3,1},{1,2},{2,2}} },
    { "Loaf", 7, {{1,0},{2,0},{0,1},{3,1},{1,2},{3,2},{2,3}} },
    { "Boat", 5, {{0,0},{1,0},{0,1},{2,1},{1,2}} },
    { "Tub", 4, {{1,0},{0,1},{2,1},{1,2}} },

    /* ── Oscillators ── */
    { "Blinker", 3, {{0,0},{1,0},{2,0}} },
    { "Toad", 6, {{1,0},{2,0},{3,0},{0,1},{1,1},{2,1}} },
    { "Beacon", 6, {{0,0},{1,0},{0,1},{3,2},{2,3},{3,3}} },
    { "Pentadecathlon", 10, {{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},{7,0},
                              {8,0},{9,0}} },
    { "Clock", 6, {{2,0},{0,1},{2,1},{1,2},{3,2},{1,3}} },

    /* ── Spaceships ── */
    { "Glider", 5, {{1,0},{2,1},{0,2},{1,2},{2,2}} },
    { "LWSS", 9, {{1,0},{4,0},{0,1},{0,2},{4,2},{0,3},{1,3},{2,3},{3,3}} },
    { "MWSS", 11, {{2,0},{0,1},{5,1},{0,2},{0,3},{5,3},{0,4},{1,4},{2,4},{3,4},{4,4}} },
    { "HWSS", 13, {{2,0},{3,0},{0,1},{6,1},{0,2},{0,3},{6,3},{0,4},{1,4},{2,4},{3,4},{4,4},{5,4}} },

    /* ── Methuselahs ── */
    { "R-pentomino", 5, {{1,0},{2,0},{0,1},{1,1},{1,2}} },
    { "Diehard", 7, {{6,0},{0,1},{1,1},{1,2},{5,2},{6,2},{7,2}} },
    { "Acorn", 7, {{1,0},{3,1},{0,2},{1,2},{4,2},{5,2},{6,2}} },
    { "Pi-heptomino", 7, {{0,0},{1,0},{2,0},{0,1},{2,1},{0,2},{2,2}} },

    /* ── Guns ── */
    { "Gosper Gun", 36, {
        {0,4},{0,5},{1,4},{1,5},
        {10,4},{10,5},{10,6},{11,3},{11,7},{12,2},{12,8},{13,2},{13,8},
        {14,5},{15,3},{15,7},{16,4},{16,5},{16,6},{17,5},
        {20,2},{20,3},{20,4},{21,2},{21,3},{21,4},{22,1},{22,5},
        {24,0},{24,1},{24,5},{24,6},
        {34,2},{34,3},{35,2},{35,3},
    } },

    /* ── Other ── */
    { "Pulsar", 48, {
        {2,0},{3,0},{4,0},{-2,0},{-3,0},{-4,0},
        {2,5},{3,5},{4,5},{-2,5},{-3,5},{-4,5},
        {2,-5},{3,-5},{4,-5},{-2,-5},{-3,-5},{-4,-5},
        {0,2},{0,3},{0,4},{0,-2},{0,-3},{0,-4},
        {5,2},{5,3},{5,4},{5,-2},{5,-3},{5,-4},
        {-5,2},{-5,3},{-5,4},{-5,-2},{-5,-3},{-5,-4},
        {2,1},{-2,1},{2,-1},{-2,-1},
        {1,2},{-1,2},{1,-2},{-1,-2},
        {4,1},{-4,1},{4,-1},{-4,-1},
        {1,4},{-1,4},{1,-4},{-1,-4},
    } },
};

/* Build rotated versions of all stamps (called once at init) */
static void stamp_init(void) {
    for (int s = 0; s < N_STAMPS; s++) {
        const StampPattern *sp = &stamp_library[s];
        /* Find center of bounding box */
        int minx = 999, miny = 999, maxx = -999, maxy = -999;
        for (int i = 0; i < sp->n && i < MAX_STAMP_CELLS; i++) {
            if (sp->cells[i].dx < minx) minx = sp->cells[i].dx;
            if (sp->cells[i].dy < miny) miny = sp->cells[i].dy;
            if (sp->cells[i].dx > maxx) maxx = sp->cells[i].dx;
            if (sp->cells[i].dy > maxy) maxy = sp->cells[i].dy;
        }
        float cx = (minx + maxx) / 2.0f;
        float cy = (miny + maxy) / 2.0f;

        for (int r = 0; r < 4; r++) {
            RotatedStamp *rs = &stamp_rotations[s][r];
            rs->n = sp->n < MAX_STAMP_CELLS ? sp->n : MAX_STAMP_CELLS;
            int rminx = 999, rminy = 999, rmaxx = -999, rmaxy = -999;
            for (int i = 0; i < rs->n; i++) {
                float dx = sp->cells[i].dx - cx;
                float dy = sp->cells[i].dy - cy;
                float rdx, rdy;
                switch (r) {
                    case 0: rdx = dx; rdy = dy; break;
                    case 1: rdx = -dy; rdy = dx; break;  /* 90° CW */
                    case 2: rdx = -dx; rdy = -dy; break;  /* 180° */
                    default: rdx = dy; rdy = -dx; break;  /* 270° CW */
                }
                int ix = (int)(rdx + (rdx >= 0 ? 0.5f : -0.5f));
                int iy = (int)(rdy + (rdy >= 0 ? 0.5f : -0.5f));
                rs->cells[i] = (Offset){ix, iy};
                if (ix < rminx) rminx = ix;
                if (iy < rminy) rminy = iy;
                if (ix > rmaxx) rmaxx = ix;
                if (iy > rmaxy) rmaxy = iy;
            }
            rs->w = rmaxx - rminx + 1;
            rs->h = rmaxy - rminy + 1;
        }
    }
}

/* Place stamp at grid position (gx, gy) — center of pattern goes there */
static void stamp_place(int gx, int gy) {
    const RotatedStamp *rs = &stamp_rotations[stamp_sel][stamp_rot];
    for (int i = 0; i < rs->n; i++) {
        int px = gx + rs->cells[i].dx;
        int py = gy + rs->cells[i].dy;
        if (symmetry > 0) {
            sym_apply(px, py, grid_set);
        } else {
            grid_set(px, py);
        }
    }
}

/* ── Terminal helpers ──────────────────────────────────────────────────────── */

static struct termios orig_termios;
static int term_raw = 0;

static void mouse_disable(void) {
    printf("\033[?1000l\033[?1002l\033[?1006l");
    fflush(stdout);
}

static void mouse_enable(void) {
    /* 1000=button events, 1002=button+motion (drag), 1006=SGR extended coords */
    printf("\033[?1000h\033[?1002h\033[?1006h");
    fflush(stdout);
}

static void term_restore(void) {
    if (term_raw) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        term_raw = 0;
    }
    mouse_disable();
    printf("\033[?25h");   /* show cursor */
    printf("\033[0m");     /* reset colors */
    printf("\033[2J\033[H"); /* clear */
    fflush(stdout);
}

static void term_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(term_restore);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    term_raw = 1;
}

static void get_term_size(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0) {
        *cols = ws.ws_col;
        *rows = ws.ws_row;
    } else {
        *cols = 80;
        *rows = 24;
    }
}

/* ── Input handling (keyboard + SGR mouse) ─────────────────────────────────── */

/* Read a single byte, non-blocking. Returns 0 if nothing. */
static int read_byte(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) return c;
    return -1;
}

/* Parse SGR mouse: ESC [ < Cb ; Cx ; Cy M/m */
typedef struct {
    int type;   /* 0=none, 1=press, 2=release, 3=drag */
    int button; /* 0=left, 2=right, 1=middle, 64=scroll-up, 65=scroll-down */
    int x, y;   /* 1-based terminal coordinates */
} MouseEvent;

static MouseEvent parse_sgr_mouse(void) {
    MouseEvent ev = {0, 0, 0, 0};
    /* We've already consumed ESC [ < — now read "Cb;Cx;CyM" or "Cb;Cx;Cym" */
    char buf[64];
    int len = 0;
    for (;;) {
        int c = read_byte();
        if (c < 0) break;
        if (len < 63) buf[len++] = (char)c;
        if (c == 'M' || c == 'm') break;
    }
    buf[len] = '\0';

    int cb = 0, cx = 0, cy = 0;
    char trail = 'M';
    if (sscanf(buf, "%d;%d;%d%c", &cb, &cx, &cy, &trail) >= 3) {
        ev.button = cb & 0x43; /* button bits */
        ev.x = cx;
        ev.y = cy;
        if (trail == 'm') {
            ev.type = 2; /* release */
        } else if (cb & 32) {
            ev.type = 3; /* drag (motion with button held) */
        } else {
            ev.type = 1; /* press */
        }
    }
    return ev;
}

/* High-level key/mouse reader. Returns key char, or 0 if nothing, or -2 for mouse. */
static MouseEvent last_mouse;

/* Special return codes for read_input */
#define KEY_MOUSE  (-2)
#define KEY_UP     (-3)
#define KEY_DOWN   (-4)
#define KEY_RIGHT  (-5)
#define KEY_LEFT   (-6)
#define KEY_IGNORE (-1)

static int read_input(void) {
    int c = read_byte();
    if (c < 0) return 0;
    if (c == 27) { /* ESC sequence */
        int c2 = read_byte();
        if (c2 < 0) return 27; /* bare ESC */
        if (c2 == '[') {
            int c3 = read_byte();
            if (c3 == '<') {
                /* SGR mouse */
                last_mouse = parse_sgr_mouse();
                return KEY_MOUSE;
            }
            /* Arrow keys: CSI A/B/C/D */
            if (c3 == 'A') return KEY_UP;
            if (c3 == 'B') return KEY_DOWN;
            if (c3 == 'C') return KEY_RIGHT;
            if (c3 == 'D') return KEY_LEFT;
            /* Other CSI sequences — consume and ignore */
            while (c3 >= 0 && c3 < 0x40) c3 = read_byte();
            return KEY_IGNORE;
        }
        return 27;
    }
    return c;
}

/* ── Thermal heatmap: age → RGB ────────────────────────────────────────────── */

typedef struct { unsigned char r, g, b; } RGB;

/* Thermal gradient stops:
 *   age  1: blue      (30,  60,  255)
 *   age  5: cyan      (0,   220, 255)
 *   age 12: green     (0,   255, 80)
 *   age 25: yellow    (255, 255, 0)
 *   age 50: red       (255, 40,  0)
 *   age 80+: white    (255, 255, 255)
 */
static const int    heat_ages[] = { 1,   5,   12,  25,  50,  80 };
static const RGB heat_colors[] = {
    { 30,  60, 255},   /* blue */
    {  0, 220, 255},   /* cyan */
    {  0, 255,  80},   /* green */
    {255, 255,   0},   /* yellow */
    {255,  40,   0},   /* red */
    {255, 255, 255},   /* white-hot */
};
#define N_HEAT_STOPS 6

static RGB age_to_rgb(int age) {
    if (age <= heat_ages[0])
        return heat_colors[0];
    if (age >= heat_ages[N_HEAT_STOPS - 1])
        return heat_colors[N_HEAT_STOPS - 1];

    for (int i = 0; i < N_HEAT_STOPS - 1; i++) {
        if (age <= heat_ages[i + 1]) {
            int a0 = heat_ages[i], a1 = heat_ages[i + 1];
            float t = (float)(age - a0) / (float)(a1 - a0);
            RGB c0 = heat_colors[i], c1 = heat_colors[i + 1];
            RGB out;
            out.r = (unsigned char)(c0.r + t * (c1.r - c0.r));
            out.g = (unsigned char)(c0.g + t * (c1.g - c0.g));
            out.b = (unsigned char)(c0.b + t * (c1.b - c0.b));
            return out;
        }
    }
    return heat_colors[N_HEAT_STOPS - 1];
}

/* Species A color: cool blue-cyan gradient */
static const int    spa_ages[] = { 1,   5,   12,  25,  50,  80 };
static const RGB spa_colors[] = {
    { 20,  40, 200},   /* deep blue */
    {  0, 100, 255},   /* blue */
    {  0, 180, 255},   /* cyan-blue */
    {100, 220, 255},   /* light cyan */
    {180, 240, 255},   /* pale cyan */
    {230, 250, 255},   /* near-white blue */
};

static RGB species_a_to_rgb(int age) {
    if (age <= spa_ages[0]) return spa_colors[0];
    if (age >= spa_ages[N_HEAT_STOPS - 1]) return spa_colors[N_HEAT_STOPS - 1];
    for (int i = 0; i < N_HEAT_STOPS - 1; i++) {
        if (age <= spa_ages[i + 1]) {
            int a0 = spa_ages[i], a1 = spa_ages[i + 1];
            float t = (float)(age - a0) / (float)(a1 - a0);
            RGB c0 = spa_colors[i], c1 = spa_colors[i + 1];
            RGB out;
            out.r = (unsigned char)(c0.r + t * (c1.r - c0.r));
            out.g = (unsigned char)(c0.g + t * (c1.g - c0.g));
            out.b = (unsigned char)(c0.b + t * (c1.b - c0.b));
            return out;
        }
    }
    return spa_colors[N_HEAT_STOPS - 1];
}

/* Species B color: warm red-orange gradient */
static const int    spb_ages[] = { 1,   5,   12,  25,  50,  80 };
static const RGB spb_colors[] = {
    {200,  20,  20},   /* deep red */
    {255,  60,   0},   /* red-orange */
    {255, 120,   0},   /* orange */
    {255, 180,  40},   /* amber */
    {255, 220, 100},   /* gold */
    {255, 245, 220},   /* near-white warm */
};

static RGB species_b_to_rgb(int age) {
    if (age <= spb_ages[0]) return spb_colors[0];
    if (age >= spb_ages[N_HEAT_STOPS - 1]) return spb_colors[N_HEAT_STOPS - 1];
    for (int i = 0; i < N_HEAT_STOPS - 1; i++) {
        if (age <= spb_ages[i + 1]) {
            int a0 = spb_ages[i], a1 = spb_ages[i + 1];
            float t = (float)(age - a0) / (float)(a1 - a0);
            RGB c0 = spb_colors[i], c1 = spb_colors[i + 1];
            RGB out;
            out.r = (unsigned char)(c0.r + t * (c1.r - c0.r));
            out.g = (unsigned char)(c0.g + t * (c1.g - c0.g));
            out.b = (unsigned char)(c0.b + t * (c1.b - c0.b));
            return out;
        }
    }
    return spb_colors[N_HEAT_STOPS - 1];
}

/* Ghost trail colors: fading gray/blue */
static RGB ghost_to_rgb(int g) {
    /* g ranges from GHOST_FRAMES (just died, brightest) to 1 (about to vanish) */
    int brightness = 30 + (g * 50) / GHOST_FRAMES; /* 30..80 */
    int blue_tint  = 40 + (g * 60) / GHOST_FRAMES; /* 40..100 */
    return (RGB){ brightness, brightness, blue_tint };
}

/* Signal tracer: purple→magenta→pink gradient (distinct from thermal heatmap) */
static RGB tracer_to_rgb(int t) {
    /* t ranges from 1 (barely visited) to 255 (heavily visited) */
    /* Gradient stops: dark indigo → violet → magenta → hot pink */
    if (t <= 8) {
        return (RGB){ (unsigned char)(15 + t*2), (unsigned char)(5 + t), (unsigned char)(30 + t*3) };
    } else if (t <= 40) {
        float f = (float)(t - 8) / 32.0f;
        return (RGB){ (unsigned char)(31 + f*89), (unsigned char)(13 + f*17), (unsigned char)(54 + f*106) };
    } else if (t <= 120) {
        float f = (float)(t - 40) / 80.0f;
        return (RGB){ (unsigned char)(120 + f*100), (unsigned char)(30 + f*20), (unsigned char)(160 + f*40) };
    } else {
        float f = (float)(t - 120) / 135.0f;
        if (f > 1.0f) f = 1.0f;
        return (RGB){ (unsigned char)(220 + f*35), (unsigned char)(50 + f*50), (unsigned char)(200 + f*55) };
    }
}

/* ── Frequency analysis: period detection via autocorrelation ───────────────── */

/* Color palette for oscillation periods:
 *   dead (0):      off/black
 *   still life (1): cool ice blue
 *   period 2:       emerald green
 *   period 3:       golden yellow
 *   period 4-5:     warm orange
 *   period 6-12:    coral/salmon
 *   period 13-32:   deep magenta
 *   chaotic (255):  hot red
 */
static RGB freq_to_rgb(int period) {
    if (period == 0) return (RGB){0, 0, 0};
    if (period == 1) return (RGB){80, 180, 240};    /* ice blue — still life */
    if (period == 2) return (RGB){40, 220, 80};      /* emerald — blinkers */
    if (period == 3) return (RGB){240, 220, 40};     /* gold — pulsars */
    if (period <= 5) {
        float t = (float)(period - 4) / 1.0f;
        return (RGB){(unsigned char)(240 + t*15), (unsigned char)(160 - t*40), (unsigned char)(30 + t*10)};
    }
    if (period <= 12) {
        float t = (float)(period - 6) / 6.0f;
        return (RGB){(unsigned char)(240 - t*40), (unsigned char)(100 - t*40), (unsigned char)(80 + t*40)};
    }
    if (period <= 32) {
        float t = (float)(period - 13) / 19.0f;
        return (RGB){(unsigned char)(180 + t*40), (unsigned char)(40 + t*20), (unsigned char)(160 + t*40)};
    }
    /* chaotic (255) */
    return (RGB){255, 30, 20};
}

/* Analyze timeline history to detect oscillation period for each cell.
 * Uses autocorrelation on the last N frames of on/off state. */
static void freq_analyze(void) {
    int n = tl_len;
    if (n < 4) {
        memset(freq_grid, 0, sizeof(freq_grid));
        return;
    }
    /* Use up to 64 most recent frames */
    int depth = n < 64 ? n : 64;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            /* Extract on/off history: bit vector (1=alive, 0=dead) */
            /* age 0 = newest frame */
            unsigned char state[64];
            int alive_count = 0;
            for (int i = 0; i < depth; i++) {
                TimelineFrame *f = timeline_get(i);
                if (f) {
                    state[i] = (f->grid[y][x] > 0) ? 1 : 0;
                    alive_count += state[i];
                } else {
                    state[i] = 0;
                }
            }

            /* If never alive, mark as dead */
            if (alive_count == 0) {
                freq_grid[y][x] = 0;
                continue;
            }
            /* If always alive (same state throughout), still life */
            if (alive_count == depth) {
                freq_grid[y][x] = 1;
                continue;
            }

            /* Autocorrelation: test periods 1..32 */
            int best_period = 0;
            int best_match = 0;
            int max_period = depth / 2;
            if (max_period > 32) max_period = 32;

            for (int p = 1; p <= max_period; p++) {
                int matches = 0;
                int total = depth - p;
                for (int i = 0; i < total; i++) {
                    if (state[i] == state[i + p])
                        matches++;
                }
                /* Need >85% match to count as periodic */
                if (matches * 100 > total * 85 && matches > best_match) {
                    best_match = matches;
                    best_period = p;
                }
            }

            if (best_period > 0) {
                /* Verify: period 1 means constant state — check if truly constant */
                if (best_period == 1) {
                    /* Check if cell is actually constant (still life or permanent dead) */
                    int changes = 0;
                    for (int i = 1; i < depth; i++)
                        if (state[i] != state[i-1]) changes++;
                    if (changes == 0)
                        freq_grid[y][x] = alive_count > 0 ? 1 : 0;
                    else
                        freq_grid[y][x] = 255; /* looks period-1 but has changes = noise */
                } else {
                    freq_grid[y][x] = (unsigned char)best_period;
                }
            } else {
                /* No clear period found — chaotic */
                freq_grid[y][x] = alive_count > 0 ? 255 : 0;
            }
        }
    }
    freq_stale = 0;
}

/* ── Entropy Heatmap computation ──────────────────────────────────────────── */

/* Compute Shannon entropy for each cell's 3x3 Moore neighborhood.
   H = -p*log2(p) - (1-p)*log2(1-p) where p = fraction of alive neighbors.
   Result is 0.0 (all same) to 1.0 (half alive, half dead). */
static void entropy_compute(void) {
    double sum = 0.0;
    float mx = 0.0f;
    int counted = 0;
    static const double log2_inv = 1.4426950408889634; /* 1/ln(2) */

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int alive = 0, total = 0;
            for (int dy = -1; dy <= 1; dy++) {
                for (int dx = -1; dx <= 1; dx++) {
                    int nx = x + dx, ny = y + dy;
                    if (!topo_map(&nx, &ny)) continue;
                    total++;
                    if (grid[ny][nx] > 0) alive++;
                }
            }
            if (total == 0) {
                entropy_grid[y][x] = 0.0f;
                continue;
            }
            double p = (double)alive / (double)total;
            double h;
            if (p <= 0.0 || p >= 1.0)
                h = 0.0;
            else
                h = -(p * log(p) + (1.0 - p) * log(1.0 - p)) * log2_inv;
            entropy_grid[y][x] = (float)h;
            sum += h;
            if ((float)h > mx) mx = (float)h;
            counted++;
        }
    }
    entropy_global = counted > 0 ? (float)(sum / counted) : 0.0f;
    entropy_max_local = mx;
    entropy_stale = 0;
}

/* Map entropy value (0.0-1.0) to RGB:
   green (low/ordered) → white (mid/boundary) → magenta (high/chaotic) */
static RGB entropy_to_rgb(float h) {
    if (h <= 0.0f) return (RGB){10, 30, 10};       /* near-black green */
    if (h >= 1.0f) return (RGB){255, 40, 220};      /* hot magenta */
    if (h < 0.5f) {
        /* Green → white (0.0 → 0.5) */
        float t = h * 2.0f;
        return (RGB){
            (unsigned char)(10 + t * 245),
            (unsigned char)(80 + t * 175),
            (unsigned char)(10 + t * 245)
        };
    } else {
        /* White → magenta (0.5 → 1.0) */
        float t = (h - 0.5f) * 2.0f;
        return (RGB){
            (unsigned char)(255),
            (unsigned char)(255 - t * 215),
            (unsigned char)(255 - t * 35)
        };
    }
}

/* Temperature field: blue (cold/ordered) → white (mid) → red (hot/chaotic) */
static RGB temp_to_rgb(float t) {
    if (t <= 0.0f) return (RGB){15, 20, 80};        /* deep blue (cold) */
    if (t >= 1.0f) return (RGB){255, 40, 20};        /* hot red */
    if (t < 0.5f) {
        /* Blue → white (0.0 → 0.5) */
        float f = t * 2.0f;
        return (RGB){
            (unsigned char)(15 + f * 240),
            (unsigned char)(20 + f * 235),
            (unsigned char)(80 + f * 175)
        };
    } else {
        /* White → red (0.5 → 1.0) */
        float f = (t - 0.5f) * 2.0f;
        return (RGB){
            (unsigned char)(255),
            (unsigned char)(255 - f * 215),
            (unsigned char)(255 - f * 235)
        };
    }
}

/* ── Lyapunov Sensitivity computation ──────────────────────────────────────── */

/* Lightweight grid stepper: deterministic rules only (no ghosts, tracers,
   emitters, ecosystem, or temperature noise).  Used for shadow/reference
   forward simulation in Lyapunov measurement. */
static void grid_step_bare(unsigned char src[MAX_H][MAX_W],
                           unsigned char dst[MAX_H][MAX_W]) {
    memset(dst, 0, MAX_H * MAX_W);
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx2 = x + dx, ny2 = y + dy;
                    if (!topo_map(&nx2, &ny2)) continue;
                    if (src[ny2][nx2] > 0) n++;
                }
            unsigned short b_mask = birth_mask;
            unsigned short s_mask = survival_mask;
            if (zone_enabled) {
                int zi = zone[y][x];
                if (zi < N_RULESETS) {
                    b_mask = rulesets[zi].birth;
                    s_mask = rulesets[zi].survival;
                }
            }
            int outcome = (src[y][x] > 0 && (s_mask & (1 << n))) ||
                          (src[y][x] == 0 && (b_mask & (1 << n)));
            if (outcome) {
                int age = src[y][x];
                dst[y][x] = (unsigned char)((age < 255) ? age + 1 : 255);
            }
        }
    }
}

/* Run shadow perturbation experiment and accumulate sensitivity.
   1. Copy grid → reference and shadow
   2. Inject random single-cell bit-flips into shadow
   3. Step both forward LYAP_HORIZON generations (deterministic)
   4. Measure local Hamming distance per cell (5×5 neighborhood)
   5. Blend into lyapunov_grid via exponential moving average */
static void lyapunov_compute(void) {
    static unsigned char ref[MAX_H][MAX_W];
    static unsigned char ref_next[MAX_H][MAX_W];
    static unsigned char shd[MAX_H][MAX_W];
    static unsigned char shd_next[MAX_H][MAX_W];

    /* Snapshot current grid into both reference and shadow */
    memcpy(ref, grid, sizeof(grid));
    memcpy(shd, grid, sizeof(grid));

    /* Inject perturbations: flip ~1/LYAP_PERTURB of cells in shadow */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (rand() % LYAP_PERTURB == 0)
                shd[y][x] = shd[y][x] ? 0 : 1;

    /* Run both grids forward LYAP_HORIZON steps */
    for (int s = 0; s < LYAP_HORIZON; s++) {
        grid_step_bare(ref, ref_next);
        grid_step_bare(shd, shd_next);
        memcpy(ref, ref_next, MAX_H * MAX_W);
        memcpy(shd, shd_next, MAX_H * MAX_W);
    }

    /* Accumulate local Hamming distance into lyapunov_grid (EMA) */
    double sum = 0.0;
    float mx = 0.0f;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int diff = 0, total = 0;
            for (int dy = -2; dy <= 2; dy++)
                for (int dx = -2; dx <= 2; dx++) {
                    int nx2 = x + dx, ny2 = y + dy;
                    if (!topo_map(&nx2, &ny2)) continue;
                    total++;
                    if ((ref[ny2][nx2] > 0) != (shd[ny2][nx2] > 0))
                        diff++;
                }
            float sensitivity = total > 0 ? (float)diff / (float)total : 0.0f;
            lyapunov_grid[y][x] = lyapunov_grid[y][x] * LYAP_DECAY
                                + sensitivity * (1.0f - LYAP_DECAY);
            sum += lyapunov_grid[y][x];
            if (lyapunov_grid[y][x] > mx) mx = lyapunov_grid[y][x];
        }
    }
    lyapunov_global = (float)(sum / (double)(W * H));
    lyapunov_max_local = mx;
    lyapunov_stale = 0;
}

/* Map sensitivity value (0.0–1.0) to RGB:
   blue (stable) → yellow (critical) → red (chaotic/sensitive) */
static RGB lyapunov_to_rgb(float s) {
    if (s <= 0.0f) return (RGB){10, 12, 60};        /* deep blue (stable) */
    if (s >= 1.0f) return (RGB){255, 30, 15};        /* hot red (chaotic) */
    if (s < 0.5f) {
        /* Blue → yellow (0.0 → 0.5) */
        float t = s * 2.0f;
        return (RGB){
            (unsigned char)(10 + t * 245),
            (unsigned char)(12 + t * 228),
            (unsigned char)(60 - t * 50)
        };
    } else {
        /* Yellow → red (0.5 → 1.0) */
        float t = (s - 0.5f) * 2.0f;
        return (RGB){
            255,
            (unsigned char)(240 - t * 210),
            (unsigned char)(10)
        };
    }
}

/* ── 2D Fourier Spectrum Analyzer ─────────────────────────────────────────── */

/* In-place Cooley-Tukey radix-2 FFT on interleaved float arrays.
   n must be a power of 2. */
static void fft1d(float *re, float *im, int n) {
    /* Bit-reversal permutation */
    int j = 0;
    for (int i = 0; i < n - 1; i++) {
        if (i < j) {
            float tr = re[i]; re[i] = re[j]; re[j] = tr;
            float ti = im[i]; im[i] = im[j]; im[j] = ti;
        }
        int m = n >> 1;
        while (m >= 1 && j >= m) { j -= m; m >>= 1; }
        j += m;
    }
    /* Butterfly stages */
    for (int step = 2; step <= n; step <<= 1) {
        int half = step >> 1;
        float angle = -6.283185307179586f / (float)step;
        float wr = cosf(angle), wi = sinf(angle);
        for (int k = 0; k < n; k += step) {
            float cr = 1.0f, ci = 0.0f;
            for (int m = 0; m < half; m++) {
                float tr = cr * re[k+m+half] - ci * im[k+m+half];
                float ti = cr * im[k+m+half] + ci * re[k+m+half];
                re[k+m+half] = re[k+m] - tr;
                im[k+m+half] = im[k+m] - ti;
                re[k+m] += tr;
                im[k+m] += ti;
                float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
}

/* Map Fourier power (0.0–1.0) to RGB:
   dark indigo (low) → blue → cyan → green → yellow → white (high) */
static RGB fourier_to_rgb(float p) {
    if (p <= 0.0f) return (RGB){4, 2, 20};          /* near-black indigo */
    if (p >= 1.0f) return (RGB){255, 255, 240};      /* hot white-yellow */
    if (p < 0.2f) {
        /* Dark indigo → blue */
        float t = p * 5.0f;
        return (RGB){
            (unsigned char)(4 + t * 16),
            (unsigned char)(2 + t * 40),
            (unsigned char)(20 + t * 180)
        };
    } else if (p < 0.4f) {
        /* Blue → cyan */
        float t = (p - 0.2f) * 5.0f;
        return (RGB){
            (unsigned char)(20 - t * 10),
            (unsigned char)(42 + t * 198),
            (unsigned char)(200 + t * 55)
        };
    } else if (p < 0.6f) {
        /* Cyan → green */
        float t = (p - 0.4f) * 5.0f;
        return (RGB){
            (unsigned char)(10 + t * 80),
            (unsigned char)(240 - t * 20),
            (unsigned char)(255 - t * 200)
        };
    } else if (p < 0.8f) {
        /* Green → yellow */
        float t = (p - 0.6f) * 5.0f;
        return (RGB){
            (unsigned char)(90 + t * 165),
            (unsigned char)(220 + t * 35),
            (unsigned char)(55 - t * 30)
        };
    } else {
        /* Yellow → white */
        float t = (p - 0.8f) * 5.0f;
        return (RGB){
            255,
            255,
            (unsigned char)(25 + t * 215)
        };
    }
}

static void fourier_compute(void) {
    /* Copy grid into FFT workspace (zero-padded) */
    memset(fft_re, 0, sizeof(fft_re));
    memset(fft_im, 0, sizeof(fft_im));
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            fft_re[y][x] = grid[y][x] ? 1.0f : 0.0f;

    /* 2D FFT: rows then columns */
    for (int y = 0; y < FFT_H; y++)
        fft1d(fft_re[y], fft_im[y], FFT_W);

    static float col_re[FFT_H], col_im[FFT_H];
    for (int x = 0; x < FFT_W; x++) {
        for (int y = 0; y < FFT_H; y++) {
            col_re[y] = fft_re[y][x];
            col_im[y] = fft_im[y][x];
        }
        fft1d(col_re, col_im, FFT_H);
        for (int y = 0; y < FFT_H; y++) {
            fft_re[y][x] = col_re[y];
            fft_im[y][x] = col_im[y];
        }
    }

    /* Compute log-power spectrum with fftshift, map to grid */
    /* First pass: find max log-power for normalization */
    float max_log_power = -1e30f;
    int hw = FFT_W / 2, hh = FFT_H / 2;
    for (int y = 0; y < FFT_H; y++)
        for (int x = 0; x < FFT_W; x++) {
            float re = fft_re[y][x], im = fft_im[y][x];
            float power = re * re + im * im;
            float lp = logf(1.0f + power);
            /* Store temporarily in fft_im (we no longer need it) */
            fft_im[y][x] = lp;
            if (lp > max_log_power) max_log_power = lp;
        }

    /* Radial power profile: bin by distance from DC */
    memset(fourier_radial, 0, sizeof(fourier_radial));
    int radial_counts[FFT_RADIAL_BINS];
    memset(radial_counts, 0, sizeof(radial_counts));
    float total_power = 0.0f;

    /* Second pass: normalize and map fftshifted spectrum to grid + radial bins */
    float inv_max = (max_log_power > 0.001f) ? 1.0f / max_log_power : 1.0f;
    for (int y = 0; y < FFT_H; y++) {
        /* fftshift: remap so DC is at center */
        int sy = (y + hh) % FFT_H;
        for (int x = 0; x < FFT_W; x++) {
            int sx = (x + hw) % FFT_W;
            float norm = fft_im[sy][sx] * inv_max;

            /* Map to grid coordinates (center spectrum on grid) */
            int gx = x - (FFT_W - W) / 2;
            int gy = y - (FFT_H - H) / 2;
            if (gx >= 0 && gx < W && gy >= 0 && gy < H) {
                fourier_grid[gy][gx] = norm;
            }

            /* Radial binning (distance from center in frequency space) */
            float fx = (float)(x - hw);
            float fy = (float)(y - hh);
            float dist = sqrtf(fx * fx + fy * fy);
            float max_dist = sqrtf((float)(hw * hw + hh * hh));
            int bin = (int)(dist / max_dist * (FFT_RADIAL_BINS - 1));
            if (bin >= 0 && bin < FFT_RADIAL_BINS) {
                fourier_radial[bin] += fft_im[sy][sx];
                radial_counts[bin]++;
                total_power += fft_im[sy][sx];
            }
        }
    }

    /* Average radial bins */
    fourier_radial_max = 0.0f;
    int peak_bin = 0;
    for (int i = 0; i < FFT_RADIAL_BINS; i++) {
        if (radial_counts[i] > 0)
            fourier_radial[i] /= radial_counts[i];
        if (i > 0 && fourier_radial[i] > fourier_radial_max) {
            fourier_radial_max = fourier_radial[i];
            peak_bin = i;
        }
    }

    /* Dominant wavelength: wavelength = N / k, where k is peak frequency bin.
       Map bin back to spatial frequency, then to wavelength in cells. */
    if (peak_bin > 0) {
        float max_dist = sqrtf((float)(hw * hw + hh * hh));
        float peak_freq = (float)peak_bin / (FFT_RADIAL_BINS - 1) * max_dist;
        /* Average grid dimension for wavelength conversion */
        float avg_N = (float)(W + H) / 2.0f;
        fourier_dominant_wl = avg_N / (peak_freq > 0.5f ? peak_freq : 0.5f);
        if (fourier_dominant_wl > avg_N) fourier_dominant_wl = avg_N;
    } else {
        fourier_dominant_wl = 0.0f;
    }

    fourier_peak_power = fourier_radial_max;

    /* Spectral entropy: H = -sum(p_i * log2(p_i)) over normalized radial profile.
       Measures how evenly energy is distributed across frequencies.
       Low = peaked/crystalline, High = broadband/noisy. */
    float spec_ent = 0.0f;
    float radial_sum = 0.0f;
    for (int i = 1; i < FFT_RADIAL_BINS; i++)  /* skip DC bin */
        radial_sum += fourier_radial[i];
    if (radial_sum > 0.001f) {
        float inv_sum = 1.0f / radial_sum;
        for (int i = 1; i < FFT_RADIAL_BINS; i++) {
            float p = fourier_radial[i] * inv_sum;
            if (p > 1e-10f)
                spec_ent -= p * log2f(p);
        }
    }
    /* Normalize to 0–1 range: max possible = log2(FFT_RADIAL_BINS-1) */
    float max_ent = log2f((float)(FFT_RADIAL_BINS - 1));
    fourier_spectral_entropy = (max_ent > 0.0f) ? spec_ent / max_ent : 0.0f;

    /* Mean power (excluding DC) */
    int total_cells = FFT_W * FFT_H;
    fourier_mean_power = (total_cells > 0) ? total_power / total_cells : 0.0f;

    fourier_stale = 0;
}

/* ── Box-Counting Fractal Dimension computation ───────────────────────────── */

/* Map fractal scale value (0.0–1.0) to RGB:
   deep purple (coarse) → teal (mid) → gold (fine-scale detail) → white (peak) */
static RGB fractal_to_rgb(float v) {
    if (v <= 0.0f) return (RGB){8, 4, 16};
    if (v >= 1.0f) return (RGB){255, 250, 220};
    if (v < 0.33f) {
        float t = v / 0.33f;
        return (RGB){(unsigned char)(8 + t * 42),
                     (unsigned char)(4 + t * 100),
                     (unsigned char)(16 + t * 104)};  /* purple → teal */
    }
    if (v < 0.66f) {
        float t = (v - 0.33f) / 0.33f;
        return (RGB){(unsigned char)(50 + t * 180),
                     (unsigned char)(104 + t * 96),
                     (unsigned char)(120 - t * 80)};  /* teal → gold */
    }
    float t = (v - 0.66f) / 0.34f;
    return (RGB){(unsigned char)(230 + t * 25),
                 (unsigned char)(200 + t * 50),
                 (unsigned char)(40 + t * 180)};       /* gold → white */
}

static void fractal_compute(void) {
    static const int box_sizes[FRACTAL_N_SCALES] = {2, 4, 8, 16, 32, 64, 128};

    memset(fractal_grid, 0, sizeof(fractal_grid));
    fractal_total_alive = 0;

    /* Count total alive cells */
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (grid[y][x]) fractal_total_alive++;

    if (fractal_total_alive == 0) {
        fractal_dbox = 0.0f;
        fractal_r2 = 0.0f;
        memset(fractal_n_boxes, 0, sizeof(fractal_n_boxes));
        fractal_stale = 0;
        return;
    }

    /* For each scale, count occupied boxes and record per-cell finest occupied scale */
    for (int s = 0; s < FRACTAL_N_SCALES; s++) {
        int eps = box_sizes[s];
        int nboxes = 0;
        int bx_count = (W + eps - 1) / eps;
        int by_count = (H + eps - 1) / eps;

        for (int by = 0; by < by_count; by++) {
            for (int bx = 0; bx < bx_count; bx++) {
                int occupied = 0;
                int y0 = by * eps, y1 = y0 + eps;
                int x0 = bx * eps, x1 = x0 + eps;
                if (y1 > H) y1 = H;
                if (x1 > W) x1 = W;
                for (int y = y0; y < y1 && !occupied; y++)
                    for (int x = x0; x < x1 && !occupied; x++)
                        if (grid[y][x]) occupied = 1;
                if (occupied) {
                    nboxes++;
                    /* For the finest scale (s=0 is finest), mark cells */
                    if (s == 0) {
                        /* At the finest scale, all alive cells in this box
                           get a baseline detail value */
                        for (int y = y0; y < y1; y++)
                            for (int x = x0; x < x1; x++)
                                if (grid[y][x])
                                    fractal_grid[y][x] = 1.0f;
                    }
                }
            }
        }
        fractal_n_boxes[s] = nboxes;
        fractal_log_eps[s] = logf((float)eps);
        fractal_log_n[s] = (nboxes > 0) ? logf((float)nboxes) : 0.0f;
    }

    /* Per-cell overlay: show "structural detail" — cells that exist at fine
       scales near the structure boundary get higher values.  We use scale isolation:
       at each scale, mark cells near boundary boxes (adjacent box occupied vs not). */
    /* Simple approach: density ratio at each scale → cells in sparse regions
       (low local density at coarse scale) are structurally interesting */
    for (int s = 0; s < FRACTAL_N_SCALES; s++) {
        int eps = box_sizes[s];
        int bx_count = (W + eps - 1) / eps;
        int by_count = (H + eps - 1) / eps;

        for (int by = 0; by < by_count; by++) {
            for (int bx = 0; bx < bx_count; bx++) {
                int y0 = by * eps, y1 = y0 + eps;
                int x0 = bx * eps, x1 = x0 + eps;
                if (y1 > H) y1 = H;
                if (x1 > W) x1 = W;

                /* Count alive in this box */
                int alive = 0, total = (y1 - y0) * (x1 - x0);
                for (int y = y0; y < y1; y++)
                    for (int x = x0; x < x1; x++)
                        if (grid[y][x]) alive++;

                if (alive > 0 && total > 0) {
                    /* Sparseness at this scale: lower fill ratio = more fractal detail */
                    float fill = (float)alive / (float)total;
                    float detail = (1.0f - fill) * (1.0f - (float)s / (float)FRACTAL_N_SCALES);
                    /* Boost cells in partially filled coarse boxes */
                    for (int y = y0; y < y1; y++)
                        for (int x = x0; x < x1; x++)
                            if (grid[y][x] && detail > fractal_grid[y][x])
                                fractal_grid[y][x] = detail;
                }
            }
        }
    }

    /* Normalize fractal_grid to 0–1 */
    float fmax = 0.001f;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (fractal_grid[y][x] > fmax) fmax = fractal_grid[y][x];
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            fractal_grid[y][x] /= fmax;

    /* Linear regression: log(N) = a + b * log(ε),  D_box = -b */
    float sum_x = 0, sum_y = 0, sum_xx = 0, sum_xy = 0;
    int n_valid = 0;
    for (int s = 0; s < FRACTAL_N_SCALES; s++) {
        if (fractal_n_boxes[s] > 0) {
            sum_x += fractal_log_eps[s];
            sum_y += fractal_log_n[s];
            sum_xx += fractal_log_eps[s] * fractal_log_eps[s];
            sum_xy += fractal_log_eps[s] * fractal_log_n[s];
            n_valid++;
        }
    }
    if (n_valid >= 2) {
        float denom = n_valid * sum_xx - sum_x * sum_x;
        if (denom > 0.0001f) {
            float slope = (n_valid * sum_xy - sum_x * sum_y) / denom;
            float intercept = (sum_y - slope * sum_x) / n_valid;
            fractal_dbox = -slope;

            /* R² computation */
            float mean_y = sum_y / n_valid;
            float ss_tot = 0, ss_res = 0;
            for (int s = 0; s < FRACTAL_N_SCALES; s++) {
                if (fractal_n_boxes[s] > 0) {
                    float pred = intercept + slope * fractal_log_eps[s];
                    ss_res += (fractal_log_n[s] - pred) * (fractal_log_n[s] - pred);
                    ss_tot += (fractal_log_n[s] - mean_y) * (fractal_log_n[s] - mean_y);
                }
            }
            fractal_r2 = (ss_tot > 0.0001f) ? (1.0f - ss_res / ss_tot) : 1.0f;
        } else {
            fractal_dbox = 0.0f;
            fractal_r2 = 0.0f;
        }
    } else {
        fractal_dbox = 0.0f;
        fractal_r2 = 0.0f;
    }

    fractal_stale = 0;
}

/* ── Wolfram Class Detector ────────────────────────────────────────────────── */
/* Compute feature vector from existing analyzer outputs + population history,
   then classify into Wolfram Class I–IV using weighted scoring. */
static void wolfram_classify(void) {
    /* Step 1: Compute entropy features (inline if entropy_mode is off) */
    {
        float sum = 0.0f;
        int count = 0;
        for (int y = 0; y < H; y += 2)
            for (int x = 0; x < W; x += 2) {
                /* Sample Moore neighborhood entropy */
                int alive = 0, total = 0;
                for (int dy = -1; dy <= 1; dy++)
                    for (int dx = -1; dx <= 1; dx++) {
                        int ny = y + dy, nx = x + dx;
                        if (ny >= 0 && ny < H && nx >= 0 && nx < W) {
                            total++;
                            if (grid[ny][nx]) alive++;
                        }
                    }
                if (total > 0) {
                    float p = (float)alive / total;
                    float h = 0.0f;
                    if (p > 0.001f && p < 0.999f)
                        h = -(p * logf(p) + (1.0f - p) * logf(1.0f - p)) / logf(2.0f);
                    sum += h;
                    count++;
                }
            }
        wf_entropy = count > 0 ? sum / count : 0.0f;
    }

    /* Step 2: Lyapunov — lightweight perturbation test (4 random probes) */
    {
        float sensitivity = 0.0f;
        int probes = 4;
        unsigned char shadow[60][80];  /* small test window */
        int sw = 80, sh = 60;
        int ox = (W - sw) / 2, oy = (H - sh) / 2;
        if (ox < 0) ox = 0;
        if (oy < 0) oy = 0;

        for (int p = 0; p < probes; p++) {
            /* Copy small region to shadow */
            for (int y = 0; y < sh && (oy + y) < H; y++)
                for (int x = 0; x < sw && (ox + x) < W; x++)
                    shadow[y][x] = grid[oy + y][ox + x] ? 1 : 0;

            /* Flip one cell */
            int fy = (rand() % (sh - 2)) + 1;
            int fx = (rand() % (sw - 2)) + 1;
            shadow[fy][fx] = shadow[fy][fx] ? 0 : 1;

            /* Evolve both for 8 steps, count differences */
            unsigned char sh2[60][80];
            memcpy(sh2, shadow, sizeof(shadow));

            for (int step = 0; step < 8; step++) {
                unsigned char next_sh[60][80];
                memset(next_sh, 0, sizeof(next_sh));
                for (int y = 1; y < sh - 1; y++)
                    for (int x = 1; x < sw - 1; x++) {
                        int nn = 0;
                        for (int dy = -1; dy <= 1; dy++)
                            for (int dx = -1; dx <= 1; dx++)
                                if (dy || dx) nn += sh2[y+dy][x+dx] ? 1 : 0;
                        /* Use current rule */
                        if (sh2[y][x])
                            next_sh[y][x] = (survival_mask & (1 << nn)) ? 1 : 0;
                        else
                            next_sh[y][x] = (birth_mask & (1 << nn)) ? 1 : 0;
                    }
                memcpy(sh2, next_sh, sizeof(sh2));
            }

            /* Compare shadow to actual grid after 8 steps (approximate) */
            int diffs = 0;
            for (int y = 1; y < sh - 1; y++)
                for (int x = 1; x < sw - 1; x++) {
                    int actual = (oy + y < H && ox + x < W) ? (grid[oy+y][ox+x] ? 1 : 0) : 0;
                    if (sh2[y][x] != actual) diffs++;
                }
            sensitivity += (float)diffs / ((sh - 2) * (sw - 2));
        }
        wf_lyapunov = sensitivity / probes;
    }

    /* Step 3: Quick fractal dimension (3 scales only: 4, 16, 64) */
    {
        static const int scales[] = {4, 16, 64};
        float log_eps[3], log_n[3];
        int valid = 0;
        for (int s = 0; s < 3; s++) {
            int eps = scales[s];
            int nboxes = 0;
            for (int by = 0; by < H; by += eps)
                for (int bx = 0; bx < W; bx += eps) {
                    int found = 0;
                    for (int y = by; y < by + eps && y < H && !found; y++)
                        for (int x = bx; x < bx + eps && x < W && !found; x++)
                            if (grid[y][x]) found = 1;
                    nboxes += found;
                }
            if (nboxes > 0) {
                log_eps[valid] = logf((float)eps);
                log_n[valid] = logf((float)nboxes);
                valid++;
            }
        }
        if (valid >= 2) {
            float sx = 0, sy = 0, sxx = 0, sxy = 0;
            for (int i = 0; i < valid; i++) {
                sx += log_eps[i]; sy += log_n[i];
                sxx += log_eps[i] * log_eps[i];
                sxy += log_eps[i] * log_n[i];
            }
            float denom = valid * sxx - sx * sx;
            wf_fractal = denom > 0.001f ? -(valid * sxy - sx * sy) / denom : 0.0f;
        } else {
            wf_fractal = 0.0f;
        }
    }

    /* Step 4: Population dynamics features from history ring buffer */
    {
        int n = hist_count;
        if (n < 2) {
            wf_pop_var = 0.0f;
            wf_pop_trend = 0.0f;
            wf_pop_acf = 0.0f;
        } else {
            int window = n < WOLFRAM_HIST ? n : WOLFRAM_HIST;
            float sum = 0, sum2 = 0;
            for (int i = 0; i < window; i++) {
                float v = (float)hist_get(i);
                sum += v;
                sum2 += v * v;
            }
            float mean = sum / window;
            float var = sum2 / window - mean * mean;
            if (var < 0) var = 0;

            /* Normalize variance by mean² to get coefficient of variation² */
            wf_pop_var = (mean > 1.0f) ? var / (mean * mean) : 0.0f;

            /* Trend: linear regression slope over recent window */
            float sx = 0, sxy = 0, sxx = 0, sy = 0;
            for (int i = 0; i < window; i++) {
                float x = (float)i;
                float y = (float)hist_get(i);
                sx += x; sy += y;
                sxx += x * x;
                sxy += x * y;
            }
            float denom = window * sxx - sx * sx;
            float slope = (denom > 0.001f) ? (window * sxy - sx * sy) / denom : 0.0f;
            /* Normalize by mean population */
            wf_pop_trend = (mean > 1.0f) ? slope / mean : 0.0f;

            /* Autocorrelation at lag 1 for periodicity detection */
            float acf = 0.0f;
            if (window >= 3 && var > 0.01f) {
                float cov = 0;
                for (int i = 0; i < window - 1; i++)
                    cov += (hist_get(i) - mean) * (hist_get(i+1) - mean);
                acf = cov / ((window - 1) * var);
                if (acf < -1.0f) acf = -1.0f;
                if (acf > 1.0f) acf = 1.0f;
            }
            wf_pop_acf = acf;
        }
    }

    /* Step 5: Density */
    wf_density = (W * H > 0) ? (float)population / (W * H) : 0.0f;

    /* Step 6: Classification via weighted scoring */
    /* Each class gets a score based on how well the feature vector matches its profile:
       Class I   (Fixed):    very low entropy, very low Lyapunov, low density, pop→0
       Class II  (Periodic): low-moderate entropy, low Lyapunov, high ACF, stable pop
       Class III (Chaotic):  high entropy, high Lyapunov, high D_box, high variance
       Class IV  (Complex):  moderate entropy, moderate Lyapunov, moderate D_box, moderate var */
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0;

    /* --- Class I: Death / Fixed point --- */
    /* Low density is the strongest signal */
    if (wf_density < 0.001f) {
        s1 += 5.0f;  /* near-empty grid: almost certainly Class I */
    } else {
        s1 += (1.0f - wf_density) * 0.5f;
    }
    /* Low entropy strongly favors Class I */
    s1 += (wf_entropy < 0.1f) ? 2.0f : (wf_entropy < 0.2f) ? 1.0f : 0.0f;
    /* Low Lyapunov (stable) */
    s1 += (wf_lyapunov < 0.05f) ? 1.5f : (wf_lyapunov < 0.1f) ? 0.5f : 0.0f;
    /* Population declining */
    s1 += (wf_pop_trend < -0.01f) ? 1.0f : 0.0f;
    /* Very low variance */
    s1 += (wf_pop_var < 0.001f) ? 1.0f : 0.0f;

    /* --- Class II: Periodic / Static structures --- */
    /* Moderate density with low variance = settled into still lifes / oscillators */
    s2 += (wf_density > 0.01f && wf_density < 0.4f) ? 1.0f : 0.0f;
    /* Low entropy but not zero */
    s2 += (wf_entropy > 0.05f && wf_entropy < 0.5f) ? 1.5f : 0.0f;
    /* Low Lyapunov = perturbation-resistant */
    s2 += (wf_lyapunov < 0.15f) ? 1.5f : (wf_lyapunov < 0.25f) ? 0.5f : 0.0f;
    /* High autocorrelation = periodic */
    s2 += (wf_pop_acf > 0.5f) ? 1.5f : (wf_pop_acf > 0.0f) ? 0.5f : 0.0f;
    /* Low population variance = stable */
    s2 += (wf_pop_var < 0.01f) ? 1.5f : (wf_pop_var < 0.05f) ? 0.5f : 0.0f;
    /* Low fractal dimension (simple structures) */
    s2 += (wf_fractal < 1.3f) ? 0.5f : 0.0f;

    /* --- Class III: Chaotic --- */
    /* High entropy = disorder */
    s3 += (wf_entropy > 0.6f) ? 2.0f : (wf_entropy > 0.4f) ? 1.0f : 0.0f;
    /* High Lyapunov = sensitive to perturbation */
    s3 += (wf_lyapunov > 0.3f) ? 2.0f : (wf_lyapunov > 0.2f) ? 1.0f : 0.0f;
    /* High fractal dimension (space-filling) */
    s3 += (wf_fractal > 1.7f) ? 1.5f : (wf_fractal > 1.5f) ? 0.5f : 0.0f;
    /* High population variance */
    s3 += (wf_pop_var > 0.05f) ? 1.0f : 0.0f;
    /* High density */
    s3 += (wf_density > 0.3f) ? 1.0f : (wf_density > 0.15f) ? 0.5f : 0.0f;
    /* Low autocorrelation (non-periodic) */
    s3 += (wf_pop_acf < 0.2f) ? 0.5f : 0.0f;

    /* --- Class IV: Complex / Edge of chaos --- */
    /* Moderate entropy (neither ordered nor fully disordered) */
    if (wf_entropy > 0.2f && wf_entropy < 0.6f) s4 += 2.0f;
    else if (wf_entropy > 0.15f && wf_entropy < 0.7f) s4 += 0.5f;
    /* Moderate Lyapunov */
    if (wf_lyapunov > 0.1f && wf_lyapunov < 0.35f) s4 += 2.0f;
    else if (wf_lyapunov > 0.05f && wf_lyapunov < 0.4f) s4 += 0.5f;
    /* Moderate fractal dimension (complex but not space-filling) */
    if (wf_fractal > 1.2f && wf_fractal < 1.7f) s4 += 1.5f;
    /* Moderate density */
    if (wf_density > 0.02f && wf_density < 0.25f) s4 += 1.0f;
    /* Some variance but not extreme */
    if (wf_pop_var > 0.005f && wf_pop_var < 0.05f) s4 += 1.0f;
    /* Near-zero or slightly positive ACF (intermittent dynamics) */
    if (wf_pop_acf > -0.3f && wf_pop_acf < 0.5f) s4 += 0.5f;

    wolfram_scores[0] = s1;
    wolfram_scores[1] = s2;
    wolfram_scores[2] = s3;
    wolfram_scores[3] = s4;

    /* Find winner */
    int best = 0;
    float best_score = s1;
    if (s2 > best_score) { best = 1; best_score = s2; }
    if (s3 > best_score) { best = 2; best_score = s3; }
    if (s4 > best_score) { best = 3; best_score = s4; }
    wolfram_class = best + 1;

    /* Confidence: margin between best and second-best, normalized */
    float total = s1 + s2 + s3 + s4;
    if (total > 0.01f) {
        wolfram_confidence = best_score / total;
    } else {
        wolfram_confidence = 0.0f;
        wolfram_class = 0;
    }

    wolfram_stale = 0;
}

/* ── Information Flow Field computation ───────────────────────────────────── */

/* Compute transfer entropy from cell (sx,sy) to cell (tx,ty) over temporal window.
   TE(X→Y) = Σ p(y_{t+1}, y_t, x_t) * log2( p(y_{t+1}|y_t,x_t) / p(y_{t+1}|y_t) )
   Using binary states: count joint occurrences in the temporal buffer. */
static float transfer_entropy_pair(int sx, int sy, int tx, int ty, int depth) {
    if (!timeline || depth < 4) return 0.0f;

    /* Count joint state occurrences: (y_{t+1}, y_t, x_t) — 2×2×2 = 8 bins */
    int counts[2][2][2]; /* [y_next][y_cur][x_cur] */
    memset(counts, 0, sizeof(counts));
    int total = 0;

    for (int i = 0; i < depth - 1; i++) {
        TimelineFrame *f_cur = timeline_get(i + 1); /* older */
        TimelineFrame *f_nxt = timeline_get(i);      /* newer */
        if (!f_cur || !f_nxt) continue;

        int x_cur = (f_cur->grid[sy][sx] > 0) ? 1 : 0;
        int y_cur = (f_cur->grid[ty][tx] > 0) ? 1 : 0;
        int y_nxt = (f_nxt->grid[ty][tx] > 0) ? 1 : 0;

        counts[y_nxt][y_cur][x_cur]++;
        total++;
    }

    if (total < 3) return 0.0f;

    /* Compute TE = Σ p(y',y,x) * log2( p(y'|y,x) / p(y'|y) ) */
    double te = 0.0;
    double inv_total = 1.0 / (double)total;

    for (int yn = 0; yn < 2; yn++) {
        for (int yc = 0; yc < 2; yc++) {
            /* p(y'|y) marginal */
            int n_yc = 0;
            int n_yn_yc = 0;
            for (int xc = 0; xc < 2; xc++) {
                n_yc += counts[0][yc][xc] + counts[1][yc][xc];
                n_yn_yc += counts[yn][yc][xc];
            }
            if (n_yc == 0 || n_yn_yc == 0) continue;
            double p_yn_given_yc = (double)n_yn_yc / (double)n_yc;

            for (int xc = 0; xc < 2; xc++) {
                int n_joint = counts[yn][yc][xc];
                if (n_joint == 0) continue;

                /* p(y'|y,x) conditional */
                int n_yc_xc = counts[0][yc][xc] + counts[1][yc][xc];
                if (n_yc_xc == 0) continue;
                double p_yn_given_yc_xc = (double)n_joint / (double)n_yc_xc;

                /* p(y',y,x) */
                double p_joint = (double)n_joint * inv_total;

                if (p_yn_given_yc > 0.0 && p_yn_given_yc_xc > 0.0) {
                    te += p_joint * log(p_yn_given_yc_xc / p_yn_given_yc);
                }
            }
        }
    }

    /* Convert to bits (log2) */
    te *= 1.4426950408889634; /* 1/ln(2) */
    if (te < 0.0) te = 0.0;  /* numerical noise */
    return (float)te;
}

static void flow_compute(void) {
    int n = tl_len;
    int depth = n < FLOW_WINDOW ? n : FLOW_WINDOW;

    if (depth < 4) {
        memset(flow_vx, 0, sizeof(flow_vx));
        memset(flow_vy, 0, sizeof(flow_vy));
        memset(flow_mag, 0, sizeof(flow_mag));
        flow_global_mag = 0.0f;
        flow_max_mag = 0.0f;
        flow_vorticity = 0.0f;
        flow_n_sources = 0;
        flow_n_sinks = 0;
        memset(flow_dir_hist, 0, sizeof(flow_dir_hist));
        flow_dir_hist_max = 1;
        flow_stale = 0;
        return;
    }

    /* Compute per-block transfer entropy in 4 cardinal directions.
       Use coarse blocks (FLOW_BLOCK×FLOW_BLOCK) for performance. */
    int bw = (W + FLOW_BLOCK - 1) / FLOW_BLOCK;
    int bh = (H + FLOW_BLOCK - 1) / FLOW_BLOCK;

    /* Temporary block-level flow vectors */
    float bvx[50][100]; /* max 200/4=50, 400/4=100 */
    float bvy[50][100];
    memset(bvx, 0, sizeof(bvx));
    memset(bvy, 0, sizeof(bvy));

    double sum_mag = 0.0;
    float max_m = 0.0f;
    double sum_curl = 0.0;
    int n_blocks = 0;
    int sources = 0, sinks = 0;
    memset(flow_dir_hist, 0, sizeof(flow_dir_hist));

    for (int by = 0; by < bh && by < 50; by++) {
        for (int bx = 0; bx < bw && bx < 100; bx++) {
            /* Representative cell: center of block */
            int cx = bx * FLOW_BLOCK + FLOW_BLOCK / 2;
            int cy = by * FLOW_BLOCK + FLOW_BLOCK / 2;
            if (cx >= W) cx = W - 1;
            if (cy >= H) cy = H - 1;

            /* Compute TE from each cardinal neighbor TO this cell
               (how much does neighbor predict this cell's future?) */
            float te_from_east = 0.0f, te_from_west = 0.0f;
            float te_from_south = 0.0f, te_from_north = 0.0f;
            float te_to_east = 0.0f, te_to_west = 0.0f;
            float te_to_south = 0.0f, te_to_north = 0.0f;

            int nx, ny;

            /* East neighbor */
            nx = cx + FLOW_BLOCK; ny = cy;
            if (nx < W) {
                te_from_east = transfer_entropy_pair(nx, ny, cx, cy, depth);
                te_to_east = transfer_entropy_pair(cx, cy, nx, ny, depth);
            }
            /* West neighbor */
            nx = cx - FLOW_BLOCK; ny = cy;
            if (nx >= 0) {
                te_from_west = transfer_entropy_pair(nx, ny, cx, cy, depth);
                te_to_west = transfer_entropy_pair(cx, cy, nx, ny, depth);
            }
            /* South neighbor */
            nx = cx; ny = cy + FLOW_BLOCK;
            if (ny < H) {
                te_from_south = transfer_entropy_pair(nx, ny, cx, cy, depth);
                te_to_south = transfer_entropy_pair(cx, cy, nx, ny, depth);
            }
            /* North neighbor */
            nx = cx; ny = cy - FLOW_BLOCK;
            if (ny >= 0) {
                te_from_north = transfer_entropy_pair(nx, ny, cx, cy, depth);
                te_to_north = transfer_entropy_pair(cx, cy, nx, ny, depth);
            }

            /* Net flow: direction of information OUTFLOW from this cell
               Positive x = information flows rightward (we influence east more than east influences us) */
            float vx_val = (te_to_east - te_from_east) - (te_to_west - te_from_west);
            float vy_val = (te_to_south - te_from_south) - (te_to_north - te_from_north);

            bvx[by][bx] = vx_val;
            bvy[by][bx] = vy_val;

            float m = sqrtf(vx_val * vx_val + vy_val * vy_val);
            if (m > max_m) max_m = m;
            sum_mag += m;
            n_blocks++;

            /* Divergence (source/sink detection) */
            float div = (te_to_east + te_to_west + te_to_south + te_to_north)
                      - (te_from_east + te_from_west + te_from_south + te_from_north);
            if (div > 0.05f) sources++;
            else if (div < -0.05f) sinks++;

            /* Direction histogram */
            if (m > 0.005f) {
                float angle = atan2f(-vy_val, vx_val); /* -vy because screen y is inverted */
                if (angle < 0) angle += 6.2831853f;
                int bin = (int)(angle * 8.0f / 6.2831853f) % 8;
                flow_dir_hist[bin]++;
            }
        }
    }

    /* Compute vorticity (curl of flow field) */
    int curl_count = 0;
    for (int by = 1; by < bh - 1 && by < 49; by++) {
        for (int bx = 1; bx < bw - 1 && bx < 99; bx++) {
            /* curl = dvx/dy - dvy/dx (finite differences) */
            float dvx_dy = (bvx[by+1][bx] - bvx[by-1][bx]) * 0.5f;
            float dvy_dx = (bvy[by][bx+1] - bvy[by][bx-1]) * 0.5f;
            sum_curl += fabs(dvx_dy - dvy_dx);
            curl_count++;
        }
    }

    /* Spread block values to per-cell grids */
    for (int y = 0; y < H; y++) {
        int by = y / FLOW_BLOCK;
        if (by >= 50) by = 49;
        for (int x = 0; x < W; x++) {
            int bx = x / FLOW_BLOCK;
            if (bx >= 100) bx = 99;
            flow_vx[y][x] = bvx[by][bx];
            flow_vy[y][x] = bvy[by][bx];
            float m = sqrtf(bvx[by][bx] * bvx[by][bx] + bvy[by][bx] * bvy[by][bx]);
            flow_mag[y][x] = m;
        }
    }

    flow_global_mag = n_blocks > 0 ? (float)(sum_mag / n_blocks) : 0.0f;
    flow_max_mag = max_m;
    flow_vorticity = curl_count > 0 ? (float)(sum_curl / curl_count) : 0.0f;
    flow_n_sources = sources;
    flow_n_sinks = sinks;

    /* Compute direction histogram max */
    flow_dir_hist_max = 1;
    for (int i = 0; i < 8; i++)
        if (flow_dir_hist[i] > flow_dir_hist_max)
            flow_dir_hist_max = flow_dir_hist[i];

    flow_stale = 0;
}

/* ── Phase-Space Attractor computation ────────────────────────────────────── */

static void attractor_compute(void) {
    /* Need enough population history for delay embedding */
    if (hist_count < 4) {
        attractor_stale = 0;
        return;
    }

    int n = hist_count;

    /* ── Step 1: Auto-tune delay τ via first zero-crossing of autocorrelation ── */
    /* This is the standard heuristic from nonlinear dynamics:
       choose τ where the autocorrelation first drops to zero (or 1/e). */
    {
        /* Compute mean */
        double mean = 0.0;
        for (int i = 0; i < n; i++) {
            int idx = (hist_pos - n + i + HIST_LEN) % HIST_LEN;
            mean += pop_history[idx];
        }
        mean /= n;

        /* Compute variance */
        double var = 0.0;
        for (int i = 0; i < n; i++) {
            int idx = (hist_pos - n + i + HIST_LEN) % HIST_LEN;
            double d = pop_history[idx] - mean;
            var += d * d;
        }
        var /= n;

        /* Find first zero-crossing of ACF or first drop below 1/e */
        int best_tau = 1;
        if (var > 0.001) {
            double prev_acf = 1.0;
            for (int tau = 1; tau < n / 2 && tau <= 30; tau++) {
                double acf = 0.0;
                int cnt = 0;
                for (int i = 0; i < n - tau; i++) {
                    int idx0 = (hist_pos - n + i + HIST_LEN) % HIST_LEN;
                    int idx1 = (hist_pos - n + i + tau + HIST_LEN) % HIST_LEN;
                    acf += (pop_history[idx0] - mean) * (pop_history[idx1] - mean);
                    cnt++;
                }
                acf = cnt > 0 ? acf / (cnt * var) : 0.0;

                if (acf <= 0.0 || acf <= 1.0 / M_E) {
                    best_tau = tau;
                    break;
                }
                if (acf < prev_acf * 0.5 && tau >= 2) {
                    best_tau = tau;
                    break;
                }
                prev_acf = acf;
                best_tau = tau;
            }
        }
        if (best_tau < 1) best_tau = 1;
        attr_delay = best_tau;
    }

    /* ── Step 2: Build delay-embedded trajectory ── */
    int tau = attr_delay;
    int traj_len = n - tau;
    if (traj_len < 2) {
        attractor_stale = 0;
        return;
    }
    if (traj_len > ATTR_MAX_PTS) traj_len = ATTR_MAX_PTS;
    attr_traj_len = traj_len;

    /* Find population range for normalization */
    float pmin = 1e9f, pmax = -1e9f;
    for (int i = 0; i < n; i++) {
        int idx = (hist_pos - n + i + HIST_LEN) % HIST_LEN;
        float v = (float)pop_history[idx];
        if (v < pmin) pmin = v;
        if (v > pmax) pmax = v;
    }
    if (pmax <= pmin) pmax = pmin + 1.0f;
    attr_pop_min = pmin;
    attr_pop_max = pmax;
    float range = pmax - pmin;

    /* Build normalized (x, y) = (pop(t), pop(t+τ)) */
    for (int i = 0; i < traj_len; i++) {
        int idx_t   = (hist_pos - n + i + HIST_LEN) % HIST_LEN;
        int idx_ttau = (hist_pos - n + i + tau + HIST_LEN) % HIST_LEN;
        attr_pts_x[i] = (pop_history[idx_t]   - pmin) / range;
        attr_pts_y[i] = (pop_history[idx_ttau] - pmin) / range;
    }
    attr_n_pts = traj_len;

    /* ── Step 3: Rasterize into scatter plot canvas ── */
    memset(attr_canvas, 0, sizeof(attr_canvas));
    attr_canvas_max = 1;

    for (int i = 0; i < traj_len; i++) {
        int bx = (int)(attr_pts_x[i] * (ATTR_BINS - 1));
        int by = (int)((1.0f - attr_pts_y[i]) * (ATTR_BINS - 1)); /* invert Y for screen */
        if (bx < 0) bx = 0; if (bx >= ATTR_BINS) bx = ATTR_BINS - 1;
        if (by < 0) by = 0; if (by >= ATTR_BINS) by = ATTR_BINS - 1;
        attr_canvas[by][bx]++;
        if (attr_canvas[by][bx] > attr_canvas_max)
            attr_canvas_max = attr_canvas[by][bx];
    }

    /* ── Step 4: Correlation dimension D₂ estimate ── */
    /* Grassberger-Procaccia algorithm: D₂ = lim_{r→0} log C(r) / log r
       where C(r) = fraction of point pairs within distance r.
       We estimate the slope from two radii. */
    {
        int max_pairs = traj_len < 200 ? traj_len : 200; /* subsample for speed */
        int step = traj_len / max_pairs;
        if (step < 1) step = 1;

        float r_small = 0.05f, r_large = 0.20f;
        int count_small = 0, count_large = 0;
        int total_pairs = 0;

        for (int i = 0; i < traj_len; i += step) {
            for (int j = i + step; j < traj_len; j += step) {
                float dx = attr_pts_x[i] - attr_pts_x[j];
                float dy = attr_pts_y[i] - attr_pts_y[j];
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < r_small) count_small++;
                if (dist < r_large) count_large++;
                total_pairs++;
            }
        }

        if (total_pairs > 0 && count_small > 0 && count_large > count_small) {
            float c_small = (float)count_small / total_pairs;
            float c_large = (float)count_large / total_pairs;
            float log_ratio_c = logf(c_large / c_small);
            float log_ratio_r = logf(r_large / r_small);
            attr_corr_dim = log_ratio_r > 0.001f ? log_ratio_c / log_ratio_r : 0.0f;
            if (attr_corr_dim < 0.0f) attr_corr_dim = 0.0f;
            if (attr_corr_dim > 3.0f) attr_corr_dim = 3.0f;
        } else {
            attr_corr_dim = count_large > 0 ? 0.1f : 0.0f;
        }
    }

    attractor_stale = 0;
}

/* ── Causal Light Cone computation ──────────────────────────────────────── */
/* Trace backward through timeline to find actual causal ancestors of the
   selected cell.  A cell at time t-1 is a causal ancestor if:
   (a) it is within the Moore neighborhood of a cell already in the cone at time t, AND
   (b) it was alive at time t-1 (contributed to the neighbor count).
   Forward cone: theoretical maximum influence boundary (diamond shape). */
static void cone_compute(void) {
    memset(cone_back, 0, sizeof(cone_back));
    memset(cone_fwd, 0, sizeof(cone_fwd));
    cone_back_cells = 0;
    cone_fwd_cells = 0;
    cone_theoretical = 0;
    cone_fill_ratio = 0.0f;
    cone_back_alive = 0;

    if (cone_sel_x < 0 || cone_sel_x >= W || cone_sel_y < 0 || cone_sel_y >= H)
        return;
    if (!timeline || tl_len < 2) return;

    /* ── Backward cone: trace actual causal ancestors ── */
    /* Use two layers: current frontier and next frontier */
    static unsigned char frontier[MAX_H][MAX_W];
    static unsigned char next_frontier[MAX_H][MAX_W];
    memset(frontier, 0, sizeof(frontier));

    /* Seed: the selected cell in the current frame */
    frontier[cone_sel_y][cone_sel_x] = 1;
    cone_back[cone_sel_y][cone_sel_x] = 1; /* depth 0 + 1 */

    int depth = 0;
    int max_depth = tl_len - 1;
    if (max_depth > CONE_MAX_DEPTH) max_depth = CONE_MAX_DEPTH;

    for (int d = 1; d <= max_depth; d++) {
        TimelineFrame *prev_frame = timeline_get(d);
        if (!prev_frame) break;

        memset(next_frontier, 0, sizeof(next_frontier));
        int found_any = 0;

        /* For each cell in the current frontier, mark its Moore neighborhood
           in the previous frame as causal ancestors IF they were alive */
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (!frontier[y][x]) continue;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx < 0 || nx >= W || ny < 0 || ny >= H) continue;
                        /* Was this cell alive in the previous frame? */
                        if (prev_frame->grid[ny][nx] > 0 && cone_back[ny][nx] == 0) {
                            cone_back[ny][nx] = (unsigned char)(d + 1 > 255 ? 255 : d + 1);
                            next_frontier[ny][nx] = 1;
                            found_any = 1;
                            cone_back_cells++;
                        }
                    }
                }
            }
        }

        if (!found_any) break;
        memcpy(frontier, next_frontier, sizeof(frontier));
        depth = d;
    }
    cone_depth = depth;

    /* Count backward cone cells that are currently alive */
    cone_back_alive = 0;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (cone_back[y][x] > 0 && grid[y][x] > 0)
                cone_back_alive++;

    /* ── Forward cone: theoretical maximum influence (diamond) ── */
    int fwd = cone_fwd_depth;
    cone_fwd_cells = 0;
    cone_theoretical = 0;

    for (int dy = -fwd; dy <= fwd; dy++) {
        for (int dx = -fwd; dx <= fwd; dx++) {
            int dist = (dy < 0 ? -dy : dy) + (dx < 0 ? -dx : dx);
            /* Chebyshev distance for Moore neighborhood: max(|dx|,|dy|) */
            int cheb = (dy < 0 ? -dy : dy);
            if ((dx < 0 ? -dx : dx) > cheb) cheb = (dx < 0 ? -dx : dx);
            if (cheb > fwd) continue;
            if (cheb == 0) continue; /* skip the selected cell itself */

            int fx = cone_sel_x + dx;
            int fy = cone_sel_y + dy;
            if (fx < 0 || fx >= W || fy < 0 || fy >= H) continue;

            cone_fwd[fy][fx] = (unsigned char)(cheb > 255 ? 255 : cheb);
            cone_fwd_cells++;
        }
    }

    /* Theoretical max: (2*depth+1)^2 - 1 for Moore neighborhood Chebyshev diamond */
    if (depth > 0) {
        int side = 2 * depth + 1;
        cone_theoretical = side * side - 1;
        cone_fill_ratio = cone_theoretical > 0 ? (float)cone_back_cells / cone_theoretical : 0.0f;
    }

    cone_mode = 2; /* mark as computed */
}

/* ── Prediction Surprise Field computation ────────────────────────────────── */
/* Record current grid into surprise history ring buffer */
static void surp_record_frame(void) {
    memcpy(surp_history[surp_hist_head], grid, sizeof(grid));
    surp_hist_head = (surp_hist_head + 1) % SURP_WINDOW;
    if (surp_hist_len < SURP_WINDOW) surp_hist_len++;
}

/* Compute neighborhood configuration hash for cell (x,y) in a grid snapshot */
static unsigned int surp_nbr_hash(const unsigned char g[MAX_H][MAX_W], int x, int y) {
    unsigned int h = 0;
    int bit = 0;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= W || ny < 0 || ny >= H) {
                /* out of bounds = dead */
            } else if (g[ny][nx] > 0) {
                h |= (1u << bit);
            }
            bit++;
        }
    }
    return h; /* 9 bits: bit0..8 = Moore neighborhood including center */
}

static void surp_compute(void) {
    if (surp_hist_len < 3) {
        /* Not enough history yet */
        memset(surp_grid, 0, sizeof(surp_grid));
        surp_global = 0.0f;
        surp_max_local = 0.0f;
        surp_stale = 0;
        return;
    }

    /* Clear hash table */
    memset(surp_trans_count, 0, sizeof(surp_trans_count));
    memset(surp_trans_total, 0, sizeof(surp_trans_total));

    /* Build transition statistics from history:
       For each consecutive pair of frames, record (neighborhood_config → outcome) */
    int n_pairs = surp_hist_len - 1;
    /* Sample spatially: use 4x4 blocks for efficiency, then interpolate */
    for (int fi = 0; fi < n_pairs; fi++) {
        int idx_curr = (surp_hist_head - surp_hist_len + fi + SURP_WINDOW) % SURP_WINDOW;
        int idx_next = (idx_curr + 1) % SURP_WINDOW;

        /* Sample every 2nd cell for performance */
        for (int y = 1; y < H - 1; y += 2) {
            for (int x = 1; x < W - 1; x += 2) {
                unsigned int cfg = surp_nbr_hash(surp_history[idx_curr], x, y);
                int outcome = surp_history[idx_next][y][x] > 0 ? 1 : 0;

                /* Hash: use cfg directly (9 bits = 512 possible configs) */
                unsigned int slot = cfg & SURP_HASH_MASK;
                surp_trans_total[slot]++;
                if (outcome) surp_trans_count[slot]++;
            }
        }
    }

    /* Compute per-cell surprisal using current grid's neighborhood config */
    static const float log2_inv = 1.4426950408889634f;
    double sum = 0.0;
    float mx = 0.0f, mn = 99.0f;
    int counted = 0;
    int n_pred = 0, n_surp = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            unsigned int cfg = surp_nbr_hash(grid, x, y);
            unsigned int slot = cfg & SURP_HASH_MASK;
            int total = surp_trans_total[slot];

            if (total < 2) {
                /* Unseen configuration — maximum uncertainty (1 bit) */
                surp_grid[y][x] = 1.0f;
                sum += 1.0;
                if (1.0f > mx) mx = 1.0f;
                counted++;
                n_surp++;
                continue;
            }

            /* P(alive | config) */
            float p_alive = (float)surp_trans_count[slot] / (float)total;
            int cell_alive = grid[y][x] > 0 ? 1 : 0;

            /* P(actual outcome) */
            float p_outcome = cell_alive ? p_alive : (1.0f - p_alive);

            /* Clamp to avoid log(0) */
            if (p_outcome < 0.001f) p_outcome = 0.001f;
            if (p_outcome > 0.999f) p_outcome = 0.999f;

            /* Surprisal = -log2(p_outcome) */
            float s = -logf(p_outcome) * log2_inv;
            if (s > 10.0f) s = 10.0f; /* cap at 10 bits */

            surp_grid[y][x] = s;
            sum += s;
            if (s > mx) mx = s;
            if (s < mn && s > 0.001f) mn = s;
            if (s < 0.1f) n_pred++;
            if (s > 0.8f) n_surp++;
            counted++;
        }
    }

    surp_global = counted > 0 ? (float)(sum / counted) : 0.0f;
    surp_max_local = mx;
    surp_min_local = mn < 99.0f ? mn : 0.0f;
    surp_predictable = n_pred;
    surp_surprising = n_surp;
    surp_stale = 0;
}

/* Map surprisal (0..~10 bits) to RGB:
   dark blue (predictable, ~0) → cyan (low surprise) → yellow (moderate) → red/white (high surprise) */
static RGB surp_to_rgb(float s) {
    /* Normalize: most surprisal values are 0-2 bits, scale accordingly */
    float t = s / 2.0f; /* map 0-2 bits to 0-1 */
    if (t > 1.0f) t = 1.0f;

    if (t < 0.01f) return (RGB){5, 8, 20};           /* near-zero: dark */
    if (t < 0.25f) {
        /* Dark blue → cyan */
        float u = t / 0.25f;
        return (RGB){
            (unsigned char)(10 + 10 * u),
            (unsigned char)(20 + 180 * u),
            (unsigned char)(80 + 175 * u)
        };
    } else if (t < 0.5f) {
        /* Cyan → yellow */
        float u = (t - 0.25f) / 0.25f;
        return (RGB){
            (unsigned char)(20 + 235 * u),
            (unsigned char)(200 + 55 * u),
            (unsigned char)(255 - 215 * u)
        };
    } else if (t < 0.75f) {
        /* Yellow → red */
        float u = (t - 0.5f) / 0.25f;
        return (RGB){
            (unsigned char)(255),
            (unsigned char)(255 - 200 * u),
            (unsigned char)(40 - 30 * u)
        };
    } else {
        /* Red → hot white */
        float u = (t - 0.75f) / 0.25f;
        return (RGB){
            (unsigned char)(255),
            (unsigned char)(55 + 200 * u),
            (unsigned char)(10 + 245 * u)
        };
    }
}

/* ── Composite Complexity Index computation ──────────────────────────────── */
/* Fuses entropy, Lyapunov, surprise, and frequency into a per-cell complexity
   score.  Each component is lazily computed if needed, then combined via a
   weighted geometric-mean-like formula that peaks at the edge of chaos. */

static void cplx_compute(void) {
    /* Ensure component analyzers are fresh */
    if (entropy_stale) {
        /* Inline entropy computation to populate entropy_grid */
        /* We call the existing compute path by temporarily toggling mode */
        int save_ent = entropy_mode;
        entropy_mode = 1;
        /* Compute entropy grid manually (mirrors compute_entropy) */
        entropy_mode = save_ent;
        /* Just recompute entropy_grid directly */
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                int alive = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = x + dx, ny = y + dy;
                        if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                            if (grid[ny][nx] > 0) alive++;
                        }
                    }
                }
                float p = alive / 9.0f;
                if (p < 0.001f || p > 0.999f) {
                    entropy_grid[y][x] = 0.0f;
                } else {
                    entropy_grid[y][x] = -(p * logf(p) + (1.0f - p) * logf(1.0f - p)) / logf(2.0f);
                }
            }
        }
        entropy_stale = 0;
    }

    /* Compute per-cell complexity from available signals */
    double sum = 0.0;
    float mx = 0.0f;
    int n_simple = 0, n_complex = 0, n_chaotic = 0;
    int alive_total = 0, alive_edge = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            /* Component 1: Spatial entropy (0-1, peaks at mixed boundaries) */
            float e = entropy_grid[y][x];

            /* Component 2: Lyapunov sensitivity (0-1, high = chaotic) */
            float l = lyapunov_grid[y][x] * 3.0f;
            if (l > 1.0f) l = 1.0f;

            /* Component 3: Prediction surprise (0-1, normalized from bits) */
            float s = surp_grid[y][x] / 2.0f;
            if (s > 1.0f) s = 1.0f;

            /* Component 4: Frequency richness — transform period into complexity:
               still=0, periodic=moderate, chaotic=high */
            float f = 0.0f;
            int period = freq_grid[y][x];
            if (period == 255) f = 0.9f;          /* chaotic */
            else if (period >= 13) f = 0.7f;       /* long period */
            else if (period >= 4) f = 0.5f;        /* moderate period */
            else if (period >= 2) f = 0.3f;        /* simple oscillation */
            else if (period == 1) f = 0.05f;       /* still life */
            /* period == 0 → dead → f = 0.0 */

            /* Combine: edge-of-chaos peaks when entropy is moderate, Lyapunov
               is moderate, and surprise is moderate.  Pure chaos scores high
               on all axes; pure order scores low.  We use a formula that
               rewards balanced moderate values over extreme ones.

               complexity = w_e*e + w_l*l + w_s*s + w_f*f
               with a penalty for being too uniform (all high = chaos, not complexity) */
            float raw = 0.30f * e + 0.25f * l + 0.25f * s + 0.20f * f;

            /* Edge-of-chaos boost: maximum complexity is when signals are
               moderate (0.3-0.7), not extreme.  Apply a concave transform. */
            float c = 4.0f * raw * (1.0f - raw); /* peaks at raw=0.5, giving c=1.0 */
            /* Slight mix to preserve some chaotic signal */
            c = 0.7f * c + 0.3f * raw;
            if (c > 1.0f) c = 1.0f;
            if (c < 0.0f) c = 0.0f;

            cplx_grid[y][x] = c;
            sum += c;
            if (c > mx) mx = c;

            if (c < 0.2f) n_simple++;
            else if (c >= 0.4f && c <= 0.7f) n_complex++;
            else if (c > 0.8f) n_chaotic++;

            if (grid[y][x] > 0) {
                alive_total++;
                if (c >= 0.4f && c <= 0.7f) alive_edge++;
            }
        }
    }

    int total = W * H;
    cplx_global = total > 0 ? (float)(sum / total) : 0.0f;
    cplx_max_local = mx;
    cplx_n_simple = n_simple;
    cplx_n_complex = n_complex;
    cplx_n_chaotic = n_chaotic;
    cplx_edge_frac = alive_total > 0 ? (float)alive_edge / alive_total : 0.0f;
    cplx_stale = 0;
}

/* Map complexity 0–1 to RGB:
   deep blue (simple/dead) → green (periodic) → gold (edge-of-chaos) → red (chaotic) */
static RGB cplx_to_rgb(float c) {
    if (c < 0.01f) return (RGB){4, 6, 18};           /* near-zero: dark */
    if (c < 0.25f) {
        /* Deep blue → teal-green (simple → periodic) */
        float u = c / 0.25f;
        return (RGB){
            (unsigned char)(8 + 5 * u),
            (unsigned char)(15 + 120 * u),
            (unsigned char)(60 + 60 * u)
        };
    } else if (c < 0.5f) {
        /* Teal-green → bright gold (periodic → edge-of-chaos) */
        float u = (c - 0.25f) / 0.25f;
        return (RGB){
            (unsigned char)(13 + 222 * u),
            (unsigned char)(135 + 90 * u),
            (unsigned char)(120 - 90 * u)
        };
    } else if (c < 0.7f) {
        /* Gold → warm amber (peak edge-of-chaos band — the sweet spot) */
        float u = (c - 0.5f) / 0.2f;
        return (RGB){
            (unsigned char)(235 + 20 * u),
            (unsigned char)(225 - 60 * u),
            (unsigned char)(30 + 20 * u)
        };
    } else {
        /* Amber → hot red (chaotic) */
        float u = (c - 0.7f) / 0.3f;
        if (u > 1.0f) u = 1.0f;
        return (RGB){
            (unsigned char)(255),
            (unsigned char)(165 - 130 * u),
            (unsigned char)(50 - 40 * u)
        };
    }
}

/* ── Topological Feature Map computation ──────────────────────────────────── */
/* Flood-fill connected component labeling (4-connected for live cells),
   then flood-fill dead cells to find bounded holes (β₁). */

static void topo_compute(void) {
    memset(topo_label, 0, sizeof(unsigned short) * H * MAX_W);
    memset(topo_hole, 0, sizeof(unsigned char) * H * MAX_W);
    memset(topo_comp_size, 0, sizeof(topo_comp_size));

    int n_comp = 0;

    /* Phase 1: Label connected components of live cells (4-connected BFS) */
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid[y][x] > 0 && topo_label[y][x] == 0) {
                if (n_comp >= TOPO_MAX_COMPONENTS) goto done_comp;
                n_comp++;
                unsigned short lbl = (unsigned short)n_comp;
                /* BFS flood fill */
                int qh = 0, qt = 0;
                topo_qx[qt] = x; topo_qy[qt] = y; qt++;
                topo_label[y][x] = lbl;
                int sz = 0;
                while (qh < qt) {
                    int cx = topo_qx[qh], cy = topo_qy[qh]; qh++;
                    sz++;
                    /* 4-connected neighbors */
                    const int dx4[4] = {1, -1, 0, 0};
                    const int dy4[4] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; d++) {
                        int nx = cx + dx4[d], ny = cy + dy4[d];
                        if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
                            grid[ny][nx] > 0 && topo_label[ny][nx] == 0) {
                            topo_label[ny][nx] = lbl;
                            if (qt < TOPO_QUEUE_SIZE) {
                                topo_qx[qt] = nx; topo_qy[qt] = ny; qt++;
                            }
                        }
                    }
                }
                topo_comp_size[n_comp - 1] = sz;
                /* Assign hue via golden ratio for maximal visual separation */
                topo_comp_hue[n_comp - 1] = (unsigned short)((n_comp * 137) % 360);
            }
        }
    }
done_comp:
    topo_beta0 = n_comp;

    /* Phase 2: Find enclosed holes — flood-fill dead cells.
       Any dead-cell region that does NOT touch the grid boundary is a hole. */
    /* Use topo_label high bits: mark visited dead cells with a temp value */
    static unsigned char dead_visited[MAX_H][MAX_W];
    memset(dead_visited, 0, sizeof(unsigned char) * H * MAX_W);

    int n_holes = 0;
    int total_hole_cells = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid[y][x] == 0 && dead_visited[y][x] == 0) {
                /* BFS flood fill of dead region */
                int qh = 0, qt = 0;
                topo_qx[qt] = x; topo_qy[qt] = y; qt++;
                dead_visited[y][x] = 1;
                int touches_boundary = 0;
                int region_size = 0;

                while (qh < qt) {
                    int cx = topo_qx[qh], cy = topo_qy[qh]; qh++;
                    region_size++;
                    if (cx == 0 || cx == W - 1 || cy == 0 || cy == H - 1)
                        touches_boundary = 1;
                    const int dx4[4] = {1, -1, 0, 0};
                    const int dy4[4] = {0, 0, 1, -1};
                    for (int d = 0; d < 4; d++) {
                        int nx = cx + dx4[d], ny = cy + dy4[d];
                        if (nx >= 0 && nx < W && ny >= 0 && ny < H &&
                            grid[ny][nx] == 0 && dead_visited[ny][nx] == 0) {
                            dead_visited[ny][nx] = 1;
                            if (qt < TOPO_QUEUE_SIZE) {
                                topo_qx[qt] = nx; topo_qy[qt] = ny; qt++;
                            }
                        }
                    }
                }

                if (!touches_boundary && region_size > 0 && region_size < W * H / 2) {
                    /* This is a hole — mark all cells from the BFS queue */
                    n_holes++;
                    total_hole_cells += region_size;
                    for (int i = 0; i < qh; i++) {
                        topo_hole[topo_qy[i]][topo_qx[i]] = 1;
                    }
                }
            }
        }
    }

    topo_beta1 = n_holes;
    topo_n_holes_cells = total_hole_cells;

    /* Compute stats */
    int largest = 0;
    long total_size = 0;
    for (int i = 0; i < n_comp; i++) {
        if (topo_comp_size[i] > largest) largest = topo_comp_size[i];
        total_size += topo_comp_size[i];
    }
    topo_largest_comp = largest;
    topo_mean_comp_size = n_comp > 0 ? (float)total_size / n_comp : 0.0f;

    /* Record history */
    topo_hist_b0[topo_hist_idx] = topo_beta0;
    topo_hist_b1[topo_hist_idx] = topo_beta1;
    topo_hist_idx = (topo_hist_idx + 1) % TOPO_HIST_LEN;
    if (topo_hist_count < TOPO_HIST_LEN) topo_hist_count++;

    topo_stale = 0;
}

/* Convert component label to RGB — each component gets its own hue */
static RGB topo_comp_to_rgb(int comp_idx) {
    if (comp_idx < 0 || comp_idx >= TOPO_MAX_COMPONENTS) return (RGB){80, 80, 80};
    int hue = topo_comp_hue[comp_idx];
    /* HSV to RGB with S=0.8, V=0.9 */
    float h = hue / 60.0f;
    float s = 0.8f, v = 0.9f;
    int hi = (int)h % 6;
    float f = h - (int)h;
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));
    float r, g, b;
    switch (hi) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }
    return (RGB){(unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255)};
}

/* Hole cells get a magenta/violet glow */
static RGB topo_hole_rgb(void) {
    return (RGB){160, 50, 200};
}

/* ── Renormalization Group Flow computation ────────────────────────────────── */
/* Majority-rule block decimation: for block size s, a coarse cell is alive
   if more than half the s×s block cells are alive. */
static void rg_compute(void) {
    int scales[RG_N_SCALES] = {2, 4, 8};  /* block sizes */

    /* Phase 1: Build coarse-grained grids at each scale */
    for (int si = 0; si < RG_N_SCALES; si++) {
        int s = scales[si];
        int cw = W / s;
        int ch = H / s;
        int threshold = (s * s) / 2;  /* majority threshold */
        int total_alive = 0;
        int total_cells = cw * ch;

        for (int cy = 0; cy < ch; cy++) {
            for (int cx = 0; cx < cw; cx++) {
                /* Count alive cells in this block */
                int count = 0;
                int bx = cx * s, by = cy * s;
                for (int dy = 0; dy < s && (by + dy) < H; dy++) {
                    for (int dx = 0; dx < s && (bx + dx) < W; dx++) {
                        if (grid[by + dy][bx + dx] > 0) count++;
                    }
                }
                int alive = (count > threshold) ? 1 : 0;
                rg_coarse[si][cy][cx] = (unsigned char)alive;
                total_alive += alive;
            }
        }
        rg_density[si] = total_cells > 0 ? (float)total_alive / total_cells : 0.0f;
    }

    /* Phase 2: Per-cell analysis — determine dominant scale and invariance */
    int n_fine = 0, n_meso = 0, n_coarse = 0, n_invariant = 0;
    float total_inv = 0.0f;
    int n_alive = 0;

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid[y][x] == 0) {
                rg_grid[y][x] = 0.0f;
                rg_invariance[y][x] = 0.0f;
                continue;
            }
            n_alive++;

            /* Get this cell's state at each coarse scale */
            float local_density[RG_N_SCALES];
            for (int si = 0; si < RG_N_SCALES; si++) {
                int s = scales[si];
                int cx = x / s, cy = y / s;
                int cw = W / s, ch = H / s;
                if (cx >= cw) cx = cw - 1;
                if (cy >= ch) cy = ch - 1;

                /* Use a local neighborhood of coarse cells for smoother result */
                int alive_count = 0, total_count = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        int nx = cx + dx, ny = cy + dy;
                        if (nx >= 0 && nx < cw && ny >= 0 && ny < ch) {
                            alive_count += rg_coarse[si][ny][nx];
                            total_count++;
                        }
                    }
                }
                local_density[si] = total_count > 0 ? (float)alive_count / total_count : 0.0f;
            }

            /* Determine which scale dominates: where density drops most */
            /* Compute "structure contribution" at each scale:
               how much density the scale retains compared to raw */
            float raw_density = 1.0f; /* this cell is alive */
            float drop[RG_N_SCALES];
            float max_drop = 0.0f;
            int max_drop_idx = 0;

            for (int si = 0; si < RG_N_SCALES; si++) {
                float prev = (si == 0) ? raw_density : local_density[si - 1];
                drop[si] = prev - local_density[si];
                if (drop[si] < 0) drop[si] = 0;
                if (drop[si] > max_drop) {
                    max_drop = drop[si];
                    max_drop_idx = si;
                }
            }

            /* Map dominant scale to 0..1: 0=fine(2x), 0.5=meso(4x), 1.0=coarse(8x) */
            rg_grid[y][x] = (float)max_drop_idx / (RG_N_SCALES - 1);

            /* Scale invariance: 1 - normalized variance of densities across scales */
            float mean_d = 0.0f;
            for (int si = 0; si < RG_N_SCALES; si++) mean_d += local_density[si];
            mean_d /= RG_N_SCALES;

            float var_d = 0.0f;
            for (int si = 0; si < RG_N_SCALES; si++) {
                float diff = local_density[si] - mean_d;
                var_d += diff * diff;
            }
            var_d /= RG_N_SCALES;
            /* Invariance: high when variance is low (all scales similar) */
            float inv = 1.0f - sqrtf(var_d) * 3.0f;  /* scale factor for visual range */
            if (inv < 0.0f) inv = 0.0f;
            if (inv > 1.0f) inv = 1.0f;
            rg_invariance[y][x] = inv;
            total_inv += inv;

            /* Classify */
            if (inv > 0.6f) {
                n_invariant++;
            } else if (max_drop_idx == 0) {
                n_fine++;
            } else if (max_drop_idx == 1) {
                n_meso++;
            } else {
                n_coarse++;
            }
        }
    }

    /* Global stats */
    float n_total = (float)(n_alive > 0 ? n_alive : 1);
    rg_global_fine = (float)n_fine / n_total;
    rg_global_meso = (float)n_meso / n_total;
    rg_global_coarse = (float)n_coarse / n_total;
    rg_global_invariant = (float)n_invariant / n_total;
    rg_mean_invariance = n_alive > 0 ? total_inv / n_alive : 0.0f;

    /* Criticality: how flat the density spectrum is across scales */
    float d_mean = 0.0f;
    for (int si = 0; si < RG_N_SCALES; si++) d_mean += rg_density[si];
    d_mean /= RG_N_SCALES;
    float d_var = 0.0f;
    for (int si = 0; si < RG_N_SCALES; si++) {
        float diff = rg_density[si] - d_mean;
        d_var += diff * diff;
    }
    d_var /= RG_N_SCALES;
    rg_criticality = 1.0f - sqrtf(d_var) * 4.0f;
    if (rg_criticality < 0.0f) rg_criticality = 0.0f;
    if (rg_criticality > 1.0f) rg_criticality = 1.0f;

    /* Record history */
    rg_hist_crit[rg_hist_idx] = rg_criticality;
    for (int si = 0; si < RG_N_SCALES; si++)
        rg_hist_d[si][rg_hist_idx] = rg_density[si];
    rg_hist_idx = (rg_hist_idx + 1) % RG_HIST_LEN;
    if (rg_hist_count < RG_HIST_LEN) rg_hist_count++;

    rg_stale = 0;
}

/* RG flow color: blend dominant scale (hue) with invariance (brightness) */
static RGB rg_to_rgb(float dominant_scale, float invariance) {
    /* Scale-invariant cells → white/silver; dominated cells → colored by scale */
    if (invariance > 0.6f) {
        /* Scale-invariant: white with slight warm tint */
        float t = (invariance - 0.6f) / 0.4f;
        unsigned char v = (unsigned char)(180 + 75 * t);
        return (RGB){v, (unsigned char)(v - 10), (unsigned char)(v - 20)};
    }

    /* Colored by dominant scale: cyan(fine) → yellow(meso) → magenta(coarse) */
    if (dominant_scale < 0.25f) {
        /* Fine: cyan */
        float t = dominant_scale / 0.25f;
        return (RGB){(unsigned char)(30 + 40 * t),
                     (unsigned char)(180 + 40 * t),
                     (unsigned char)(220 - 30 * t)};
    } else if (dominant_scale < 0.75f) {
        /* Meso: yellow-gold */
        float t = (dominant_scale - 0.25f) / 0.5f;
        return (RGB){(unsigned char)(200 + 55 * t),
                     (unsigned char)(200 - 40 * t),
                     (unsigned char)(40 + 80 * t)};
    } else {
        /* Coarse: magenta-violet */
        float t = (dominant_scale - 0.75f) / 0.25f;
        return (RGB){(unsigned char)(220 - 40 * t),
                     (unsigned char)(60 + 30 * t),
                     (unsigned char)(200 + 55 * t)};
    }
}

/* ── Kolmogorov Complexity Estimator computation ─────────────────────────── */

/* LZ77-style compression of a bitstring: returns compressed size as fraction
   of original size (0.0 = perfectly compressible, 1.0 = incompressible).
   Uses a sliding window to find longest matches in previously seen data. */
static float kc_lz_compress(const unsigned char *bits, int len) {
    if (len <= 0) return 0.0f;

    int pos = 0;          /* current position in bitstring */
    int output_bits = 0;  /* total bits in compressed output */

    /* Encoding: for each position, either emit a literal (1 + 1 bit)
       or a back-reference (1 + log2(offset) + log2(length) bits).
       This is a simplified LZ77 model for estimation purposes. */
    while (pos < len) {
        int best_len = 0;
        int search_start = pos - KC_WINDOW;
        if (search_start < 0) search_start = 0;

        /* Search for longest match in sliding window */
        for (int s = search_start; s < pos; s++) {
            int match_len = 0;
            while (pos + match_len < len && match_len < 255 &&
                   bits[s + match_len] == bits[pos + match_len]) {
                match_len++;
                /* Allow overlapping: s + match_len can go past pos */
            }
            if (match_len > best_len) {
                best_len = match_len;
            }
        }

        if (best_len >= KC_MIN_MATCH) {
            /* Back-reference: flag(1) + offset cost (~5 bits) + length cost (~4 bits) */
            output_bits += 10;
            pos += best_len;
        } else {
            /* Literal: flag(1) + raw bit(1) */
            output_bits += 2;
            pos++;
        }
    }

    float ratio = (float)output_bits / (float)(len > 0 ? len : 1);
    if (ratio > 1.0f) ratio = 1.0f;
    return ratio;
}

static void kc_compute(void) {
    int half = KC_BLOCK / 2;  /* 8: half-block for centering */
    float total_kc = 0.0f;
    float max_kc = 0.0f;
    float min_kc = 1.0f;
    int n_alive = 0;
    int n_simple = 0, n_struct = 0, n_complex = 0;

    /* Temporary bitstring buffer for one block */
    unsigned char block_bits[KC_BLOCK * KC_BLOCK];

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (grid[y][x] == 0) {
                kc_grid[y][x] = 0.0f;
                continue;
            }

            /* Serialize KC_BLOCK × KC_BLOCK neighborhood centered on (x,y) */
            int n_bits = 0;
            for (int dy = -half; dy < half; dy++) {
                for (int dx = -half; dx < half; dx++) {
                    int nx = x + dx, ny = y + dy;
                    unsigned char val = 0;
                    if (nx >= 0 && nx < W && ny >= 0 && ny < H) {
                        val = grid[ny][nx] > 0 ? 1 : 0;
                    }
                    block_bits[n_bits++] = val;
                }
            }

            /* Compress and compute ratio */
            float ratio = kc_lz_compress(block_bits, n_bits);
            kc_grid[y][x] = ratio;
            total_kc += ratio;
            n_alive++;

            if (ratio > max_kc) max_kc = ratio;
            if (ratio < min_kc) min_kc = ratio;

            if (ratio < 0.3f) n_simple++;
            else if (ratio <= 0.7f) n_struct++;
            else n_complex++;
        }
    }

    kc_global_mean = n_alive > 0 ? total_kc / n_alive : 0.0f;
    kc_global_max = max_kc;
    kc_global_min = n_alive > 0 ? min_kc : 0.0f;
    kc_n_simple = n_simple;
    kc_n_structured = n_struct;
    kc_n_complex = n_complex;

    /* Record sparkline history */
    kc_hist_mean[kc_hist_idx] = kc_global_mean;
    kc_hist_max[kc_hist_idx] = kc_global_max;
    kc_hist_idx = (kc_hist_idx + 1) % KC_HIST_LEN;
    if (kc_hist_count < KC_HIST_LEN) kc_hist_count++;

    kc_stale = 0;
}

/* Kolmogorov complexity color: blue (simple) → gold (structured) → red/white (complex) */
static RGB kc_to_rgb(float ratio) {
    if (ratio < 0.3f) {
        /* Simple: deep blue → cyan */
        float t = ratio / 0.3f;
        return (RGB){(unsigned char)(20 + 30 * t),
                     (unsigned char)(40 + 120 * t),
                     (unsigned char)(180 + 40 * t)};
    } else if (ratio < 0.6f) {
        /* Structured: cyan → gold */
        float t = (ratio - 0.3f) / 0.3f;
        return (RGB){(unsigned char)(50 + 190 * t),
                     (unsigned char)(160 + 50 * t),
                     (unsigned char)(220 - 190 * t)};
    } else if (ratio < 0.85f) {
        /* Complex: gold → red */
        float t = (ratio - 0.6f) / 0.25f;
        return (RGB){(unsigned char)(240),
                     (unsigned char)(210 - 160 * t),
                     (unsigned char)(30 + 20 * t)};
    } else {
        /* Incompressible: red → white-hot */
        float t = (ratio - 0.85f) / 0.15f;
        return (RGB){(unsigned char)(240 + 15 * t),
                     (unsigned char)(50 + 200 * t),
                     (unsigned char)(50 + 200 * t)};
    }
}

/* ── Mutual Information Network computation ──────────────────────────────── */

/* Draw a Bresenham line on mi_overlay, keeping max MI at each cell */
static void mi_draw_line(int x0, int y0, int x1, int y1, float val) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < W && y0 >= 0 && y0 < H) {
            if (val > mi_overlay[y0][x0])
                mi_overlay[y0][x0] = val;
        }
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* MI color gradient: dark blue → cyan → green → yellow → magenta → white */
static RGB mi_to_rgb(float val) {
    float t = mi_max_val > 0.001f ? val / mi_max_val : 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t < 0.2f) {
        float s = t / 0.2f;
        return (RGB){ (unsigned char)(15 + 15 * s),
                      (unsigned char)(30 + 170 * s),
                      (unsigned char)(120 + 135 * s) };
    } else if (t < 0.4f) {
        float s = (t - 0.2f) / 0.2f;
        return (RGB){ (unsigned char)(30 + 30 * s),
                      (unsigned char)(200 + 55 * s),
                      (unsigned char)(255 - 120 * s) };
    } else if (t < 0.6f) {
        float s = (t - 0.4f) / 0.2f;
        return (RGB){ (unsigned char)(60 + 195 * s),
                      (unsigned char)(255 - 30 * s),
                      (unsigned char)(135 - 115 * s) };
    } else if (t < 0.8f) {
        float s = (t - 0.6f) / 0.2f;
        return (RGB){ (unsigned char)(255),
                      (unsigned char)(225 - 125 * s),
                      (unsigned char)(20 + 130 * s) };
    } else {
        float s = (t - 0.8f) / 0.2f;
        return (RGB){ (unsigned char)(255),
                      (unsigned char)(100 + 155 * s),
                      (unsigned char)(150 + 105 * s) };
    }
}

static void mi_compute(void) {
    int n = tl_len;
    if (n < 16) {
        memset(mi_overlay, 0, sizeof(mi_overlay));
        mi_n_top = 0;
        mi_mean_val = 0; mi_max_val = 0;
        mi_net_density = 0; mi_clustering = 0;
        mi_n_frames_used = n;
        mi_stale = 0;
        return;
    }

    int depth = n < 256 ? n : 256;
    mi_n_frames_used = depth;

    /* Step 1: Extract block populations from timeline */
    static int bpop[MI_NBLK][256]; /* 200 × 256 × 4 = 200KB */
    memset(bpop, 0, sizeof(bpop));

    for (int fi = 0; fi < depth; fi++) {
        TimelineFrame *f = timeline_get(fi);
        if (!f) continue;
        for (int y = 0; y < H; y++) {
            int by = y / MI_BLK_H;
            for (int x = 0; x < W; x++) {
                if (f->grid[y][x] > 0) {
                    int bx = x / MI_BLK_W;
                    bpop[by * MI_GW + bx][fi]++;
                }
            }
        }
    }

    /* Step 2: Compute min/max per block for binning */
    int bmin[MI_NBLK], bmax[MI_NBLK];
    for (int bi = 0; bi < MI_NBLK; bi++) {
        bmin[bi] = bpop[bi][0]; bmax[bi] = bpop[bi][0];
        for (int fi = 1; fi < depth; fi++) {
            if (bpop[bi][fi] < bmin[bi]) bmin[bi] = bpop[bi][fi];
            if (bpop[bi][fi] > bmax[bi]) bmax[bi] = bpop[bi][fi];
        }
    }

    /* Step 3: Quantize to bins */
    static unsigned char bbin[MI_NBLK][256]; /* 200 × 256 = 50KB */
    for (int bi = 0; bi < MI_NBLK; bi++) {
        int range = bmax[bi] - bmin[bi];
        if (range == 0) {
            memset(bbin[bi], 0, depth);
        } else {
            for (int fi = 0; fi < depth; fi++) {
                int b = (bpop[bi][fi] - bmin[bi]) * (MI_BINS - 1) / range;
                if (b >= MI_BINS) b = MI_BINS - 1;
                bbin[bi][fi] = (unsigned char)b;
            }
        }
    }

    /* Step 4: Precompute per-block marginal entropy and marginal histogram */
    int marg_hist[MI_NBLK][MI_BINS];
    memset(marg_hist, 0, sizeof(marg_hist));
    for (int bi = 0; bi < MI_NBLK; bi++)
        for (int fi = 0; fi < depth; fi++)
            marg_hist[bi][(int)bbin[bi][fi]]++;

    /* Step 5: Compute MI for all pairs, keep top-N */
    mi_n_top = 0;
    double sum_mi = 0.0;
    int n_pairs = 0;
    int n_above = 0;
    float mi_threshold = 0.1f;

    for (int ai = 0; ai < MI_NBLK; ai++) {
        if (bmax[ai] == bmin[ai]) continue; /* skip constant blocks */
        for (int bi = ai + 1; bi < MI_NBLK; bi++) {
            if (bmax[bi] == bmin[bi]) continue;
            n_pairs++;

            /* Build joint histogram */
            int joint[MI_BINS][MI_BINS];
            memset(joint, 0, sizeof(joint));
            for (int fi = 0; fi < depth; fi++)
                joint[bbin[ai][fi]][bbin[bi][fi]]++;

            /* MI = Σ p(x,y) log2(p(x,y) / (p(x)p(y))) */
            double mi = 0.0;
            double inv_n = 1.0 / (double)depth;
            for (int i = 0; i < MI_BINS; i++) {
                if (marg_hist[ai][i] == 0) continue;
                double pa = (double)marg_hist[ai][i] * inv_n;
                for (int j = 0; j < MI_BINS; j++) {
                    if (joint[i][j] == 0 || marg_hist[bi][j] == 0) continue;
                    double pab = (double)joint[i][j] * inv_n;
                    double pb = (double)marg_hist[bi][j] * inv_n;
                    mi += pab * log(pab / (pa * pb));
                }
            }
            mi *= 1.4426950408889634; /* convert to bits (1/ln2) */
            if (mi < 0.0) mi = 0.0;

            float mif = (float)mi;
            sum_mi += mi;
            if (mif > mi_threshold) n_above++;

            /* Insert into top-N sorted list (descending by MI) */
            if (mi_n_top < MI_TOP_N || mif > mi_top[mi_n_top - 1].mi) {
                int pos = mi_n_top < MI_TOP_N ? mi_n_top : MI_TOP_N - 1;
                while (pos > 0 && mi_top[pos - 1].mi < mif) {
                    if (pos < MI_TOP_N)
                        mi_top[pos] = mi_top[pos - 1];
                    pos--;
                }
                mi_top[pos].a = ai;
                mi_top[pos].b = bi;
                mi_top[pos].mi = mif;
                if (mi_n_top < MI_TOP_N) mi_n_top++;
            }
        }
    }

    mi_mean_val = n_pairs > 0 ? (float)(sum_mi / n_pairs) : 0.0f;
    mi_max_val = mi_n_top > 0 ? mi_top[0].mi : 0.0f;
    mi_net_density = n_pairs > 0 ? (float)n_above / n_pairs : 0.0f;

    /* Step 6: Compute clustering coefficient */
    static unsigned char adj[MI_NBLK][MI_NBLK]; /* 200×200 = 40KB */
    memset(adj, 0, sizeof(adj));
    int degree[MI_NBLK];
    memset(degree, 0, sizeof(degree));

    for (int i = 0; i < mi_n_top; i++) {
        adj[mi_top[i].a][mi_top[i].b] = 1;
        adj[mi_top[i].b][mi_top[i].a] = 1;
        degree[mi_top[i].a]++;
        degree[mi_top[i].b]++;
    }

    double cc_sum = 0.0;
    int cc_count = 0;
    for (int ni = 0; ni < MI_NBLK; ni++) {
        if (degree[ni] < 2) continue;
        int nbrs[MI_TOP_N * 2];
        int nn = 0;
        for (int nj = 0; nj < MI_NBLK && nn < MI_TOP_N * 2; nj++)
            if (adj[ni][nj]) nbrs[nn++] = nj;
        int triangles = 0;
        for (int a = 0; a < nn; a++)
            for (int b = a + 1; b < nn; b++)
                if (adj[nbrs[a]][nbrs[b]]) triangles++;
        int possible = nn * (nn - 1) / 2;
        if (possible > 0) {
            cc_sum += (double)triangles / possible;
            cc_count++;
        }
    }
    mi_clustering = cc_count > 0 ? (float)(cc_sum / cc_count) : 0.0f;

    /* Step 7: Draw coupling lines on overlay grid */
    memset(mi_overlay, 0, sizeof(mi_overlay));

    for (int i = 0; i < mi_n_top; i++) {
        int ai = mi_top[i].a;
        int bi = mi_top[i].b;
        int ax = (ai % MI_GW) * MI_BLK_W + MI_BLK_W / 2;
        int ay = (ai / MI_GW) * MI_BLK_H + MI_BLK_H / 2;
        int bx = (bi % MI_GW) * MI_BLK_W + MI_BLK_W / 2;
        int by = (bi / MI_GW) * MI_BLK_H + MI_BLK_H / 2;
        mi_draw_line(ax, ay, bx, by, mi_top[i].mi);
    }

    mi_stale = 0;
}

/* Map flow magnitude + direction to RGB color and Unicode arrow glyph */
static RGB flow_to_rgb(float mag, float vx, float vy) {
    /* Normalize magnitude for color */
    float norm = flow_max_mag > 0.001f ? mag / flow_max_mag : 0.0f;
    if (norm > 1.0f) norm = 1.0f;

    /* Color by direction: hue encodes angle, brightness encodes magnitude
       Use HSV-style mapping where angle → hue, magnitude → value */
    float angle = atan2f(-vy, vx); /* screen coords: -vy for upward */
    if (angle < 0) angle += 6.2831853f;
    float hue = angle / 6.2831853f; /* 0..1 */

    /* 6-sector HSV to RGB */
    float h6 = hue * 6.0f;
    int hi = (int)h6 % 6;
    float f = h6 - (int)h6;
    float v = 0.3f + 0.7f * norm; /* brightness: 0.3 (dim) to 1.0 (bright) */
    float s = 0.7f + 0.3f * norm; /* saturation increases with magnitude */
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (hi) {
        case 0: r = v; g = t; b = p; break;
        case 1: r = q; g = v; b = p; break;
        case 2: r = p; g = v; b = t; break;
        case 3: r = p; g = q; b = v; break;
        case 4: r = t; g = p; b = v; break;
        default: r = v; g = p; b = q; break;
    }

    return (RGB){
        (unsigned char)(r * 255),
        (unsigned char)(g * 255),
        (unsigned char)(b * 255)
    };
}

/* Get Unicode arrow character for flow direction */
static const char *flow_arrow(float vx, float vy, float mag) {
    if (mag < 0.001f) return "\xc2\xb7"; /* middle dot for no flow */
    float angle = atan2f(-vy, vx);
    if (angle < 0) angle += 6.2831853f;
    int octant = ((int)(angle * 8.0f / 6.2831853f + 0.5f)) % 8;
    static const char *arrows[] = {
        "\xe2\x86\x92", /* → E   */
        "\xe2\x86\x97", /* ↗ NE  */
        "\xe2\x86\x91", /* ↑ N   */
        "\xe2\x86\x96", /* ↖ NW  */
        "\xe2\x86\x90", /* ← W   */
        "\xe2\x86\x99", /* ↙ SW  */
        "\xe2\x86\x93", /* ↓ S   */
        "\xe2\x86\x98", /* ↘ SE  */
    };
    return arrows[octant];
}

/* ── Save / Load (.life files) ─────────────────────────────────────────────── */

/*
 * File format (binary):
 *   4 bytes: magic "LIFE"
 *   1 byte:  version (1)
 *   2 bytes: birth_mask (LE)
 *   2 bytes: survival_mask (LE)
 *   1 byte:  symmetry, topology, heatmap_mode, zone_enabled
 *   4 bytes: generation, population (LE)
 *   1 byte:  n_emitters, n_absorbers
 *   grids: grid, ghost, zone (MAX_H*MAX_W each)
 *   emitters (4×int32 each), absorbers (3×int32 each)
 */

#define SAVE_MAGIC "LIFE"
#define SAVE_VERSION 1

static char flash_msg[128] = "";
static long long flash_until = 0;

static void flash_set(const char *msg) {
    snprintf(flash_msg, sizeof(flash_msg), "%s", msg);
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    flash_until = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000 + 2000;
}

static int flash_active(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    long long now = (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    return now < flash_until;
}

/* Forward declaration for cell_color (defined later after rendering helpers) */
static int cell_color(int x, int y, RGB *out);

/* ── Frame capture (PPM screenshot) ────────────────────────────────────────── */

static int frame_counter = 0;

static int next_frame_number(void) {
    char path[64];
    for (int n = 1; n <= 9999; n++) {
        snprintf(path, sizeof(path), "frame_%04d.ppm", n);
        if (access(path, F_OK) != 0) return n;
    }
    return 9999;
}

/* Render the current viewport to a PPM file.  Returns the filename via flash. */
static void capture_frame(void) {
    int num = next_frame_number();
    char path[64];
    snprintf(path, sizeof(path), "frame_%04d.ppm", num);

    FILE *f = fopen(path, "wb");
    if (!f) { flash_set("Capture failed!"); return; }

    /* Image dimensions = viewport in pixels (1 cell = 1 pixel) */
    int img_w = view_w;
    int img_h = view_h;
    if (img_w < 1) img_w = 1;
    if (img_h < 1) img_h = 1;

    fprintf(f, "P6\n%d %d\n255\n", img_w, img_h);

    for (int row = 0; row < img_h; row++) {
        for (int col = 0; col < img_w; col++) {
            int gx = view_x + col;
            int gy = view_y + row;
            RGB c = {0, 0, 0};
            cell_color(gx, gy, &c);
            fputc(c.r, f);
            fputc(c.g, f);
            fputc(c.b, f);
        }
    }

    fclose(f);
    char msg[128];
    snprintf(msg, sizeof(msg), "Saved %s (%dx%d)", path, img_w, img_h);
    flash_set(msg);
}

/* Dump the entire timeline buffer as a numbered image sequence */
static void capture_timeline(void) {
    if (!timeline || tl_len == 0) {
        flash_set("No timeline frames to capture");
        return;
    }

    /* Save current grid state so we can restore it after */
    unsigned char save_grid[MAX_H][MAX_W];
    unsigned char save_ghost[MAX_H][MAX_W];
    unsigned char save_species[MAX_H][MAX_W];
    int save_gen = generation;
    int save_pop = population;
    memcpy(save_grid, grid, sizeof(grid));
    memcpy(save_ghost, ghost, sizeof(ghost));
    memcpy(save_species, species, sizeof(species));

    int base = next_frame_number();
    int img_w = view_w;
    int img_h = view_h;
    if (img_w < 1) img_w = 1;
    if (img_h < 1) img_h = 1;

    int saved = 0;
    /* Iterate from oldest to newest */
    for (int age = tl_len - 1; age >= 0; age--) {
        timeline_restore(age);

        char path[64];
        snprintf(path, sizeof(path), "frame_%04d.ppm", base + saved);

        FILE *f = fopen(path, "wb");
        if (!f) continue;

        fprintf(f, "P6\n%d %d\n255\n", img_w, img_h);

        for (int row = 0; row < img_h; row++) {
            for (int col = 0; col < img_w; col++) {
                int gx = view_x + col;
                int gy = view_y + row;
                RGB c = {0, 0, 0};
                cell_color(gx, gy, &c);
                fputc(c.r, f);
                fputc(c.g, f);
                fputc(c.b, f);
            }
        }

        fclose(f);
        saved++;
    }

    /* Restore original grid state */
    memcpy(grid, save_grid, sizeof(grid));
    memcpy(ghost, save_ghost, sizeof(ghost));
    memcpy(species, save_species, sizeof(species));
    generation = save_gen;
    population = save_pop;

    char msg[128];
    snprintf(msg, sizeof(msg), "Saved %d frames (%dx%d) frame_%04d..%04d.ppm",
             saved, img_w, img_h, base, base + saved - 1);
    flash_set(msg);
}

static int next_save_slot(void) {
    char path[64];
    for (int slot = 1; slot <= 999; slot++) {
        snprintf(path, sizeof(path), "save_%03d.life", slot);
        if (access(path, F_OK) != 0) return slot;
    }
    return 999;
}

static int latest_save_slot(void) {
    char path[64];
    for (int slot = 999; slot >= 1; slot--) {
        snprintf(path, sizeof(path), "save_%03d.life", slot);
        if (access(path, F_OK) == 0) return slot;
    }
    return 0;
}

static void write_u16(FILE *f, unsigned short v) {
    fputc(v & 0xFF, f);
    fputc((v >> 8) & 0xFF, f);
}

static void write_i32(FILE *f, int v) {
    unsigned int u = (unsigned int)v;
    fputc(u & 0xFF, f); fputc((u >> 8) & 0xFF, f);
    fputc((u >> 16) & 0xFF, f); fputc((u >> 24) & 0xFF, f);
}

static unsigned short read_u16(FILE *f) {
    int lo = fgetc(f), hi = fgetc(f);
    return (unsigned short)((hi << 8) | lo);
}

static int read_i32(FILE *f) {
    int b0 = fgetc(f), b1 = fgetc(f), b2 = fgetc(f), b3 = fgetc(f);
    return (int)((unsigned int)b0 | ((unsigned int)b1 << 8) |
                 ((unsigned int)b2 << 16) | ((unsigned int)b3 << 24));
}

/* ── RLE pattern import / export ────────────────────────────────────────── */

static int next_export_slot(void) {
    char path[64];
    for (int slot = 1; slot <= 999; slot++) {
        snprintf(path, sizeof(path), "export_%03d.rle", slot);
        if (access(path, F_OK) != 0) return slot;
    }
    return 999;
}

/* Load an RLE file into the grid, centering the pattern.
 * Returns 1 on success, 0 on failure. */
static int load_rle(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    int pat_w = 0, pat_h = 0;
    int got_header = 0;
    char line[4096];

    /* Parse header: comments (#), then x = N, y = M[, rule = ...] */
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#') continue; /* skip comments */

        /* Look for header line: x = N, y = M ... */
        if (!got_header) {
            char *xp = strstr(line, "x");
            if (!xp) xp = strstr(line, "X");
            if (xp) {
                /* Parse x = N */
                char *eq = strchr(xp, '=');
                if (eq) pat_w = atoi(eq + 1);

                /* Parse y = M */
                char *yp = strstr(eq ? eq : xp, "y");
                if (!yp) yp = strstr(eq ? eq : xp, "Y");
                if (yp) {
                    eq = strchr(yp, '=');
                    if (eq) pat_h = atoi(eq + 1);
                }

                /* Parse optional rule = B.../S... */
                char *rp = strstr(line, "rule");
                if (!rp) rp = strstr(line, "Rule");
                if (!rp) rp = strstr(line, "RULE");
                if (rp) {
                    eq = strchr(rp, '=');
                    if (eq) {
                        /* Skip whitespace */
                        char *rs = eq + 1;
                        while (*rs == ' ' || *rs == '\t') rs++;
                        /* Parse B.../S... or b.../s... */
                        unsigned short b = 0, s = 0;
                        char *p = rs;
                        if (*p == 'B' || *p == 'b') {
                            p++;
                            while (*p >= '0' && *p <= '8') {
                                b |= (1 << (*p - '0'));
                                p++;
                            }
                            if (*p == '/') p++;
                            if (*p == 'S' || *p == 's') {
                                p++;
                                while (*p >= '0' && *p <= '8') {
                                    s |= (1 << (*p - '0'));
                                    p++;
                                }
                            }
                            birth_mask = b;
                            survival_mask = s;
                            current_ruleset = find_matching_ruleset();
                            if (current_ruleset < 0) current_ruleset = 0;
                        }
                    }
                }
                got_header = 1;
            }
            continue;
        }
        break; /* first non-comment, non-header line = start of RLE data */
    }

    if (!got_header || pat_w <= 0 || pat_h <= 0) {
        fclose(f);
        return 0;
    }

    /* Center pattern on grid */
    int ox = (W - pat_w) / 2;
    int oy = (H - pat_h) / 2;
    if (ox < 0) ox = 0;
    if (oy < 0) oy = 0;

    /* Clear grid before loading */
    grid_clear();

    /* Parse RLE data: digits are run counts, b=dead, o=alive, $=end row, !=end */
    int cx = 0, cy = 0;
    int run = 0;
    int done = 0;

    /* We already have 'line' with the first data line; process it then read more */
    for (;;) {
        char *p = line;
        while (*p && !done) {
            if (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t') {
                p++;
                continue;
            }
            if (*p >= '0' && *p <= '9') {
                run = run * 10 + (*p - '0');
                p++;
                continue;
            }
            if (run == 0) run = 1;

            if (*p == 'b') {
                cx += run;
            } else if (*p == 'o') {
                for (int i = 0; i < run; i++) {
                    int gx = ox + cx;
                    int gy = oy + cy;
                    if (gx >= 0 && gx < W && gy >= 0 && gy < H) {
                        grid[gy][gx] = 1;
                        population++;
                    }
                    cx++;
                }
            } else if (*p == '$') {
                cy += run;
                cx = 0;
            } else if (*p == '!') {
                done = 1;
            }
            run = 0;
            p++;
        }
        if (done) break;
        if (!fgets(line, sizeof(line), f)) break;
    }

    fclose(f);
    generation = 0;
    return 1;
}

/* Export current grid as RLE file. */
static void export_rle(void) {
    /* Find bounding box of live cells */
    int x0 = W, y0 = H, x1 = -1, y1 = -1;
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if (grid[y][x]) {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }

    if (x1 < 0) {
        flash_set("Nothing to export (grid empty)");
        return;
    }

    int pat_w = x1 - x0 + 1;
    int pat_h = y1 - y0 + 1;

    int slot = next_export_slot();
    char path[64];
    snprintf(path, sizeof(path), "export_%03d.rle", slot);

    FILE *f = fopen(path, "w");
    if (!f) { flash_set("Export failed!"); return; }

    /* Write header comment */
    fprintf(f, "#C Exported from Life Explorer, generation %d\n", generation);

    /* Write header line with rule */
    char rule[32];
    rule_to_string(rule, sizeof(rule));
    fprintf(f, "x = %d, y = %d, rule = %s\n", pat_w, pat_h, rule);

    /* Encode RLE data */
    int col = 0; /* column counter for line wrapping at 70 chars */
    for (int y = y0; y <= y1; y++) {
        int x = x0;
        while (x <= x1) {
            /* Count run of same state */
            int alive = grid[y][x] ? 1 : 0;
            int run = 0;
            while (x + run <= x1 && (grid[y][x + run] ? 1 : 0) == alive)
                run++;

            /* Skip trailing dead cells on this row */
            if (!alive && x + run > x1) break;

            /* Write run */
            char tag = alive ? 'o' : 'b';
            char tmp[16];
            int n;
            if (run > 1)
                n = snprintf(tmp, sizeof(tmp), "%d%c", run, tag);
            else
                n = snprintf(tmp, sizeof(tmp), "%c", tag);

            /* Line wrap at ~70 chars */
            if (col + n > 70) {
                fputc('\n', f);
                col = 0;
            }
            fputs(tmp, f);
            col += n;

            x += run;
        }
        /* End of row: $ (or ! for last row) */
        if (y < y1) {
            /* Count consecutive blank rows */
            int blank_rows = 0;
            while (y + 1 + blank_rows <= y1) {
                int all_dead = 1;
                for (int bx = x0; bx <= x1; bx++)
                    if (grid[y + 1 + blank_rows][bx]) { all_dead = 0; break; }
                if (!all_dead) break;
                blank_rows++;
            }
            char tmp[16];
            int total = 1 + blank_rows;
            int n;
            if (total > 1)
                n = snprintf(tmp, sizeof(tmp), "%d$", total);
            else
                n = snprintf(tmp, sizeof(tmp), "$");
            if (col + n > 70) { fputc('\n', f); col = 0; }
            fputs(tmp, f);
            col += n;
            y += blank_rows;
        }
    }
    fprintf(f, "!\n");
    fclose(f);

    char msg[128];
    snprintf(msg, sizeof(msg), "Exported %s (%dx%d)", path, pat_w, pat_h);
    flash_set(msg);
}

static void save_state(void) {
    int slot = next_save_slot();
    char path[64];
    snprintf(path, sizeof(path), "save_%03d.life", slot);

    FILE *f = fopen(path, "wb");
    if (!f) { flash_set("Save failed!"); return; }

    fwrite(SAVE_MAGIC, 1, 4, f);
    fputc(SAVE_VERSION, f);
    write_u16(f, birth_mask);
    write_u16(f, survival_mask);
    fputc(symmetry, f);
    fputc(topology, f);
    fputc(heatmap_mode, f);
    fputc(zone_enabled, f);
    write_i32(f, generation);
    write_i32(f, population);
    fputc((unsigned char)n_emitters, f);
    fputc((unsigned char)n_absorbers, f);

    fwrite(grid, 1, sizeof(grid), f);
    fwrite(ghost, 1, sizeof(ghost), f);
    fwrite(zone, 1, sizeof(zone), f);

    for (int i = 0; i < n_emitters; i++) {
        write_i32(f, emitters[i].x); write_i32(f, emitters[i].y);
        write_i32(f, emitters[i].rate); write_i32(f, emitters[i].pattern);
    }
    for (int i = 0; i < n_absorbers; i++) {
        write_i32(f, absorbers[i].x); write_i32(f, absorbers[i].y);
        write_i32(f, absorbers[i].radius);
    }
    /* Portals (appended for backward compat — old files just won't have them) */
    fputc((unsigned char)n_portals, f);
    for (int i = 0; i < n_portals; i++) {
        write_i32(f, portals[i].ax); write_i32(f, portals[i].ay);
        write_i32(f, portals[i].bx); write_i32(f, portals[i].by);
        write_i32(f, portals[i].radius);
        fputc(portals[i].active, f);
    }

    /* Ecosystem data (appended for backward compat) */
    fputc((unsigned char)ecosystem_mode, f);
    if (ecosystem_mode) {
        fwrite(species, 1, sizeof(species), f);
        write_u16(f, species_a_birth);
        write_u16(f, species_a_survival);
        write_u16(f, species_b_birth);
        write_u16(f, species_b_survival);
        write_i32(f, (int)(interaction * 100.0f));
    }

    fclose(f);
    char msg[80];
    snprintf(msg, sizeof(msg), "Saved %s", path);
    flash_set(msg);
}

static void load_state(int slot) {
    if (slot <= 0) slot = latest_save_slot();
    if (slot <= 0) { flash_set("No saves found"); return; }

    char path[64];
    snprintf(path, sizeof(path), "save_%03d.life", slot);
    FILE *f = fopen(path, "rb");
    if (!f) { flash_set("Load failed!"); return; }

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, SAVE_MAGIC, 4) != 0) {
        fclose(f); flash_set("Bad save file"); return;
    }
    if (fgetc(f) != SAVE_VERSION) {
        fclose(f); flash_set("Unknown save version"); return;
    }

    birth_mask = read_u16(f);
    survival_mask = read_u16(f);
    symmetry = fgetc(f);
    topology = fgetc(f);
    if (topology >= TOPO_COUNT) topology = TOPO_FLAT; /* compat: clamp invalid values */
    heatmap_mode = fgetc(f);
    zone_enabled = fgetc(f);
    generation = read_i32(f);
    population = read_i32(f);
    n_emitters = fgetc(f);
    n_absorbers = fgetc(f);
    if (n_emitters > MAX_EMITTERS) n_emitters = MAX_EMITTERS;
    if (n_absorbers > MAX_ABSORBERS) n_absorbers = MAX_ABSORBERS;

    (void)!fread(grid, 1, sizeof(grid), f);
    (void)!fread(ghost, 1, sizeof(ghost), f);
    (void)!fread(zone, 1, sizeof(zone), f);

    for (int i = 0; i < n_emitters; i++) {
        emitters[i].x = read_i32(f); emitters[i].y = read_i32(f);
        emitters[i].rate = read_i32(f); emitters[i].pattern = read_i32(f);
    }
    for (int i = 0; i < n_absorbers; i++) {
        absorbers[i].x = read_i32(f); absorbers[i].y = read_i32(f);
        absorbers[i].radius = read_i32(f);
    }

    /* Load portals if present (backward compat: may be at EOF) */
    int np = fgetc(f);
    if (np != EOF && np > 0 && np <= MAX_PORTALS) {
        n_portals = np;
        for (int i = 0; i < n_portals; i++) {
            portals[i].ax = read_i32(f); portals[i].ay = read_i32(f);
            portals[i].bx = read_i32(f); portals[i].by = read_i32(f);
            portals[i].radius = read_i32(f);
            portals[i].active = fgetc(f);
        }
    } else {
        n_portals = 0;
    }
    portal_placing = 0;

    /* Load ecosystem data if present (backward compat: may be at EOF) */
    int eco = fgetc(f);
    if (eco != EOF && eco > 0) {
        ecosystem_mode = 1;
        (void)!fread(species, 1, sizeof(species), f);
        species_a_birth = read_u16(f);
        species_a_survival = read_u16(f);
        species_b_birth = read_u16(f);
        species_b_survival = read_u16(f);
        interaction = read_i32(f) / 100.0f;
    } else {
        ecosystem_mode = 0;
        memset(species, 0, sizeof(species));
    }

    fclose(f);

    current_ruleset = find_matching_ruleset();
    if (current_ruleset < 0) current_ruleset = 0;
    timeline_clear();
    timeline_push();
    hist_count = 0;
    hist_pos = 0;
    hist_push(population);
    replay_mode = 0;

    char msg[80];
    snprintf(msg, sizeof(msg), "Loaded %s", path);
    flash_set(msg);
}

/* ── Zone colors & painting ────────────────────────────────────────────────── */

/* Distinct hue per zone (used for subtle background tinting) */
static const RGB zone_colors[] = {
    {  60,  80, 180 },  /* 0: Conway — blue */
    { 160,  60, 180 },  /* 1: HighLife — purple */
    {  60, 160, 160 },  /* 2: Day&Night — teal */
    { 200, 120,  30 },  /* 3: Seeds — orange */
    { 140, 100,  60 },  /* 4: Diamoeba — brown */
    { 180,  50,  80 },  /* 5: Morley — rose */
    { 100, 160,  50 },  /* 6: 2x2 — olive */
    {  50, 140, 100 },  /* 7: Maze — sea green */
    { 180, 140,  50 },  /* 8: Coral — gold */
    { 100,  80, 160 },  /* 9: Anneal — lavender */
};

/* Paint a zone at (x,y) with current zone_brush */
static void zone_paint(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H) {
        zone[y][x] = (unsigned char)zone_brush;
        zone_enabled = 1;
    }
}

/* Paint a 3x3 zone brush for easier coverage */
static void zone_paint_brush(int x, int y) {
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            zone_paint(x + dx, y + dy);
}

/* Preset zone layouts for quick demos */
static void zone_preset(int id) {
    zone_enabled = 1;
    if (id == 1) {
        /* Vertical split: left=Conway, right=HighLife */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                zone[y][x] = (x < W/2) ? 0 : 1;
    } else if (id == 2) {
        /* 4 quadrants: Conway/Seeds/Maze/Day&Night */
        int rules[4] = {0, 3, 7, 2};
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                zone[y][x] = rules[(y < H/2 ? 0 : 2) + (x < W/2 ? 0 : 1)];
    } else if (id == 3) {
        /* Concentric rings: different rules radiating from center */
        int cx = W/2, cy = H/2;
        int ring_rules[] = {0, 8, 7, 1, 2, 3, 4, 5, 6, 9};
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                int dx = x - cx, dy = y - cy;
                int dist = (int)(sqrt((double)(dx*dx + dy*dy)));
                int ring = dist / 20;
                if (ring >= 10) ring = 9;
                zone[y][x] = ring_rules[ring];
            }
    } else if (id == 4) {
        /* Horizontal stripes */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                zone[y][x] = (y / 25) % N_RULESETS;
    } else if (id == 5) {
        /* Checkerboard */
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
                zone[y][x] = ((x / 25) + (y / 25)) % N_RULESETS;
    }
}

/* Symmetric zone painting (uses sym_apply logic but paints zones) */
static void zone_paint_sym(int gx, int gy) {
    int cx = W / 2, cy = H / 2;
    int dx = gx - cx, dy = gy - cy;

    zone_paint_brush(gx, gy);

    if (symmetry >= 1)
        zone_paint_brush(cx - dx, gy);
    if (symmetry >= 2) {
        zone_paint_brush(gx, cy - dy);
        zone_paint_brush(cx - dx, cy - dy);
    }
    if (symmetry >= 3) {
        zone_paint_brush(cx + dy, cy + dx);
        zone_paint_brush(cx - dy, cy + dx);
        zone_paint_brush(cx + dy, cy - dx);
        zone_paint_brush(cx - dy, cy - dx);
    }
}

/* ── Temperature painting ──────────────────────────────────────────────────── */
static void temp_paint(int cx, int cy, float val, int radius) {
    for (int dy = -radius; dy <= radius; dy++)
        for (int dx = -radius; dx <= radius; dx++) {
            if (dx*dx + dy*dy > radius*radius) continue;
            int x = cx + dx, y = cy + dy;
            if (x < 0 || x >= W || y < 0 || y >= H) continue;
            /* Gaussian-like falloff from center */
            float dist = sqrtf((float)(dx*dx + dy*dy)) / (float)radius;
            float strength = 1.0f - dist;
            if (strength < 0.0f) strength = 0.0f;
            /* Blend toward brush value */
            temp_grid[y][x] = temp_grid[y][x] * (1.0f - strength) + val * strength;
            if (temp_grid[y][x] < 0.0f) temp_grid[y][x] = 0.0f;
            if (temp_grid[y][x] > 1.0f) temp_grid[y][x] = 1.0f;
        }
}

static void temp_paint_sym(int gx, int gy) {
    int cx = W / 2, cy = H / 2;
    int dx = gx - cx, dy = gy - cy;

    temp_paint(gx, gy, temp_brush, temp_brush_radius);

    if (symmetry >= 1)
        temp_paint(cx - dx, gy, temp_brush, temp_brush_radius);
    if (symmetry >= 2) {
        temp_paint(gx, cy - dy, temp_brush, temp_brush_radius);
        temp_paint(cx - dx, cy - dy, temp_brush, temp_brush_radius);
    }
    if (symmetry >= 3) {
        temp_paint(cx + dy, cy + dx, temp_brush, temp_brush_radius);
        temp_paint(cx - dy, cy + dx, temp_brush, temp_brush_radius);
        temp_paint(cx + dy, cy - dx, temp_brush, temp_brush_radius);
        temp_paint(cx - dy, cy - dx, temp_brush, temp_brush_radius);
    }
}

/* ── Sparkline rendering ───────────────────────────────────────────────────── */

/* Unicode block elements for sparkline: 8 levels */
static const char *spark_chars[] = {
    "\u2581", "\u2582", "\u2583", "\u2584",
    "\u2585", "\u2586", "\u2587", "\u2588"
};

static void render_sparkline(char **pp, int width) {
    char *p = *pp;
    if (hist_count < 2) {
        *pp = p;
        return;
    }

    int n = hist_count < width ? hist_count : width;

    /* Find min/max in visible range */
    int mn = hist_get(0), mx = hist_get(0);
    for (int i = 0; i < n; i++) {
        int v = hist_get(i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }

    /* Render right-to-left (newest on right) */
    p += sprintf(p, "\033[90m");
    for (int i = n - 1; i >= 0; i--) {
        int v = hist_get(i);
        int level;
        if (mx == mn)
            level = 3;
        else
            level = (v - mn) * 7 / (mx - mn);
        if (level < 0) level = 0;
        if (level > 7) level = 7;

        /* Color gradient: low=red, mid=yellow, high=green */
        if (level <= 2)
            p += sprintf(p, "\033[91m");
        else if (level <= 4)
            p += sprintf(p, "\033[93m");
        else
            p += sprintf(p, "\033[92m");

        const char *ch = spark_chars[level];
        while (*ch) *p++ = *ch++;
    }
    p += sprintf(p, "\033[0m");
    *pp = p;
}

/* Forward declaration */
static int portal_marker(int x, int y, RGB *out);

/* ── Minimap overlay ───────────────────────────────────────────────────────── */

/* Quarter-block chars for minimap (same encoding as zoom 4x) */
static const char *mm_quad[16] = {
    " ",
    "\xe2\x96\x98", "\xe2\x96\x9d", "\xe2\x96\x80",
    "\xe2\x96\x96", "\xe2\x96\x8c", "\xe2\x96\x9e", "\xe2\x96\x9b",
    "\xe2\x96\x97", "\xe2\x96\x9a", "\xe2\x96\x90", "\xe2\x96\x9c",
    "\xe2\x96\x84", "\xe2\x96\x99", "\xe2\x96\x9f", "\xe2\x96\x88",
};

static void render_minimap(char **pp) {
    if (zoom == 1 || !show_minimap) return;

    char *p = *pp;
    int usable_rows = term_rows - 3;
    if (usable_rows < 10) return;

    /* Minimap size in terminal chars — adaptive to terminal size */
    int mw = term_cols / 4;
    int mh = usable_rows / 3;
    if (mw > 50) mw = 50;
    if (mh > 25) mh = 25;
    if (mw < 10 || mh < 5) return;

    /* Subpixel resolution (2x2 per char) */
    int spw = mw * 2; /* subpixels across (for 400 cols) */
    int sph = mh * 2; /* subpixels down   (for 200 rows) */

    /* Cells per subpixel (floating point for accurate mapping) */
    float sx_scale = (float)MAX_W / spw;
    float sy_scale = (float)MAX_H / sph;

    /* Viewport rectangle in subpixel coords */
    int vp_l = (int)(view_x / sx_scale);
    int vp_r = (int)((view_x + view_w) / sx_scale);
    int vp_t = (int)(view_y / sy_scale);
    int vp_b = (int)((view_y + view_h) / sy_scale);
    if (vp_r >= spw) vp_r = spw - 1;
    if (vp_b >= sph) vp_b = sph - 1;

    /* Position: bottom-right of usable grid area (1-based terminal coords) */
    int box_w = mw + 2; /* +2 for border chars */
    int box_h = mh + 2;
    int col0 = term_cols - box_w + 1;
    int row0 = 3 + usable_rows - box_h; /* row 3 = first grid row */
    if (row0 < 3) return;

    /* ── Top border ── */
    p += sprintf(p, "\033[%d;%dH\033[48;2;20;20;30;90m\xe2\x94\x8c", row0, col0);
    for (int i = 0; i < mw; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
    *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
    p += sprintf(p, "\033[0m");

    /* ── Content rows ── */
    for (int row = 0; row < mh; row++) {
        p += sprintf(p, "\033[%d;%dH\033[48;2;20;20;30;90m\xe2\x94\x82\033[0m",
                     row0 + 1 + row, col0);

        int sy0 = row * 2;
        int sy1 = sy0 + 1;

        for (int col = 0; col < mw; col++) {
            int sx0 = col * 2;
            int sx1 = sx0 + 1;

            /* 4 subpixels: TL(sx0,sy0), TR(sx1,sy0), BL(sx0,sy1), BR(sx1,sy1) */
            int alive[4] = {0, 0, 0, 0};
            int border[4] = {0, 0, 0, 0};
            int sp_q[4] = {0, 0, 0, 0}; /* species of first alive cell in quadrant */
            int spxs[4][2] = {{sx0,sy0},{sx1,sy0},{sx0,sy1},{sx1,sy1}};

            for (int q = 0; q < 4; q++) {
                int sx = spxs[q][0], sy = spxs[q][1];

                /* Viewport border check */
                if ((sx >= vp_l && sx <= vp_r && (sy == vp_t || sy == vp_b)) ||
                    (sy >= vp_t && sy <= vp_b && (sx == vp_l || sx == vp_r)))
                    border[q] = 1;

                /* Sample grid: check if any cell alive in this subpixel's region */
                int gx0 = (int)(sx * sx_scale);
                int gy0 = (int)(sy * sy_scale);
                int gx1 = (int)((sx + 1) * sx_scale);
                int gy1 = (int)((sy + 1) * sy_scale);
                if (gx1 > MAX_W) gx1 = MAX_W;
                if (gy1 > MAX_H) gy1 = MAX_H;

                for (int gy = gy0; gy < gy1 && !alive[q]; gy++)
                    for (int gx = gx0; gx < gx1 && !alive[q]; gx++) {
                        if (grid[gy][gx]) {
                            alive[q] = 1;
                            if (ecosystem_mode) sp_q[q] = species[gy][gx];
                        } else if (tracer_mode > 0 && tracer[gy][gx] > 10)
                            alive[q] = 1;
                        /* Show portal regions on minimap */
                        if (!alive[q] && n_portals > 0) {
                            RGB dummy;
                            if (portal_marker(gx, gy, &dummy))
                                alive[q] = 1;
                        }
                    }
            }

            int bits = 0;
            int any_border = 0;
            for (int q = 0; q < 4; q++) {
                if (alive[q] || border[q]) bits |= (1 << q);
                if (border[q]) any_border = 1;
            }

            /* Color: yellow for viewport rect, species-colored or dim green for cells */
            if (any_border)
                p += sprintf(p, "\033[93;48;2;20;20;30m");
            else if (bits && ecosystem_mode) {
                /* Determine dominant species in this minimap cell */
                int sa = 0, sb = 0;
                for (int q = 0; q < 4; q++) {
                    if (alive[q]) { if (sp_q[q] == 1) sa++; else if (sp_q[q] == 2) sb++; }
                }
                if (sa > 0 && sb > 0)
                    p += sprintf(p, "\033[38;2;140;80;200;48;2;20;20;30m"); /* mixed: purple */
                else if (sa > 0)
                    p += sprintf(p, "\033[38;2;60;120;255;48;2;20;20;30m"); /* species A: blue */
                else if (sb > 0)
                    p += sprintf(p, "\033[38;2;255;80;40;48;2;20;20;30m");  /* species B: red */
                else
                    p += sprintf(p, "\033[38;2;0;140;0;48;2;20;20;30m");
            } else if (bits)
                p += sprintf(p, "\033[38;2;0;140;0;48;2;20;20;30m");
            else
                p += sprintf(p, "\033[48;2;20;20;30m");

            const char *ch = mm_quad[bits];
            while (*ch) *p++ = *ch++;
            p += sprintf(p, "\033[0m");
        }

        p += sprintf(p, "\033[48;2;20;20;30;90m\xe2\x94\x82\033[0m");
    }

    /* ── Bottom border ── */
    p += sprintf(p, "\033[%d;%dH\033[48;2;20;20;30;90m\xe2\x94\x94",
                 row0 + 1 + mh, col0);
    for (int i = 0; i < mw; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
    *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
    p += sprintf(p, "\033[0m");

    *pp = p;
}

/* ── Timeline bar rendering ─────────────────────────────────────────────────── */

static void render_timeline_bar(char **pp, int width) {
    char *p = *pp;
    if (tl_len < 2 || width < 10) { *pp = p; return; }

    /* Draw a horizontal bar showing position in history */
    /* Format: ◀━━━━━●━━━━━▶ or ◀━━━━━━━━━━▶ (live) */
    int bar_w = width - 2; /* minus ◀ and ▶ */
    if (bar_w > 60) bar_w = 60;
    if (bar_w < 5) bar_w = 5;

    /* Position: replay_pos maps to bar position */
    /* replay_pos=0 (newest) → rightmost, replay_pos=tl_len-1 → leftmost */
    int cursor = bar_w - 1 - (tl_len > 1 ? replay_pos * (bar_w - 1) / (tl_len - 1) : 0);
    if (cursor < 0) cursor = 0;
    if (cursor >= bar_w) cursor = bar_w - 1;

    if (replay_mode) {
        p += sprintf(p, "\033[93m"); /* yellow for replay */
    } else {
        p += sprintf(p, "\033[90m"); /* dim for live */
    }

    /* Left arrow */
    const char *la = "\xe2\x97\x80"; /* ◀ */
    while (*la) *p++ = *la++;

    for (int i = 0; i < bar_w; i++) {
        if (replay_mode && i == cursor) {
            /* Playhead marker */
            p += sprintf(p, "\033[97m\xe2\x97\x8f\033[%sm", replay_mode ? "93" : "90"); /* ● */
        } else {
            /* Bar segment — brighter near the cursor in replay mode */
            if (replay_mode) {
                int dist = abs(i - cursor);
                if (dist < 3)
                    p += sprintf(p, "\033[93m\xe2\x94\x81"); /* ━ bright */
                else
                    p += sprintf(p, "\033[90m\xe2\x94\x80"); /* ─ dim */
            } else {
                p += sprintf(p, "\xe2\x94\x80"); /* ─ */
            }
        }
    }

    /* Right arrow */
    const char *ra = "\xe2\x96\xb6"; /* ▶ */
    while (*ra) *p++ = *ra++;

    /* Frame info */
    if (replay_mode) {
        TimelineFrame *f = timeline_get(replay_pos);
        if (f) {
            p += sprintf(p, " \033[93mGen %d\033[90m (%d/%d)\033[0m",
                         f->generation, tl_len - replay_pos, tl_len);
        }
    } else {
        p += sprintf(p, " \033[90m%d frames\033[0m", tl_len);
    }
    p += sprintf(p, "\033[0m");
    *pp = p;
}

/* ── Rule editor overlay ───────────────────────────────────────────────────── */

/* Check if a mouse click hits a B/S bit in the rule editor overlay.
 * Returns 1 if handled (bit toggled), 0 otherwise. */
static int rule_editor_click(int mx, int my) {
    if (!rule_editor) return 0;

    /* Birth row is at re_row0+1, Survive row at re_row0+2 (1-based terminal coords) */
    int birth_row = re_row0 + 1;
    int surv_row = re_row0 + 2;

    if (my != birth_row && my != surv_row) return 0;

    /* Layout: │ + " Birth:   " = 11 visible cols before first digit.
     * Each digit takes 2 cols (digit char + space). */
    int rel_x = mx - re_col0;
    if (rel_x < 11 || rel_x >= 11 + 9 * 2) return 0;

    int digit = (rel_x - 11) / 2;
    if (digit < 0 || digit > 8) return 0;

    if (my == birth_row) {
        birth_mask ^= (1 << digit);
    } else {
        survival_mask ^= (1 << digit);
    }
    return 1;
}

/* Check if click hits a preset name in the rule editor.
 * Presets are listed on rows re_row0+4 and re_row0+5 */
static int rule_editor_preset_click(int mx, int my) {
    if (!rule_editor) return 0;

    /* Row re_row0+4: first 5 presets, row re_row0+5: next 5 */
    int row1 = re_row0 + 4;
    int row2 = re_row0 + 5;
    if (my != row1 && my != row2) return 0;

    /* Layout: │ + space = 2 cols before presets. Each preset name is %-7s = 7 chars */
    int rel_x = mx - re_col0 - 2;
    if (rel_x < 0 || rel_x >= 5 * 7) return 0;

    int base = (my == row1) ? 0 : 5;
    int preset = base + rel_x / 7;
    if (preset < 0 || preset >= N_RULESETS) return 0;

    birth_mask = rulesets[preset].birth;
    survival_mask = rulesets[preset].survival;
    current_ruleset = preset;
    return 1;
}

static void render_rule_editor(char **pp) {
    if (!rule_editor) return;
    char *p = *pp;

    /* Position: centered, starting at row 4 (below 2-line status bar + 1 grid row) */
    re_col0 = (term_cols - RE_WIDTH) / 2;
    if (re_col0 < 1) re_col0 = 1;
    re_row0 = 4;

    /* Style strings */
    const char *bdr = "\033[38;2;120;140;200;48;2;15;15;25m";
    const char *bg  = "\033[48;2;15;15;25m";
    const char *dim = "\033[90;48;2;15;15;25m";
    const char *rst = "\033[0m";
    int right_col = re_col0 + RE_WIDTH - 1; /* column for right │ */

    /* Helper macro: fill background to right border, then draw │ */
    #define RE_CLOSE_ROW(row) do { \
        p += sprintf(p, "%s", bg); \
        for (int _c = 0; _c < 12; _c++) *p++ = ' '; /* generous padding */ \
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", (row), right_col, bdr, rst); \
    } while(0)

    /* ── Top border ── */
    p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Rule Editor ", re_row0, re_col0, bdr);
    for (int i = 0; i < RE_WIDTH - 16; i++) {
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80';
    }
    *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
    p += sprintf(p, "%s", rst);

    /* ── Birth row (row+1) ── */
    p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", re_row0 + 1, re_col0, bdr, bg);
    p += sprintf(p, " \033[97;48;2;15;15;25mBirth:   ");
    for (int i = 0; i <= 8; i++) {
        if (birth_mask & (1 << i))
            p += sprintf(p, "\033[97;48;2;60;120;60m%d%s ", i, bg);
        else
            p += sprintf(p, "%s%d%s ", dim, i, bg);
    }
    RE_CLOSE_ROW(re_row0 + 1);

    /* ── Survive row (row+2) ── */
    p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", re_row0 + 2, re_col0, bdr, bg);
    p += sprintf(p, " \033[97;48;2;15;15;25mSurvive: ");
    for (int i = 0; i <= 8; i++) {
        if (survival_mask & (1 << i))
            p += sprintf(p, "\033[97;48;2;60;60;120m%d%s ", i, bg);
        else
            p += sprintf(p, "%s%d%s ", dim, i, bg);
    }
    RE_CLOSE_ROW(re_row0 + 2);

    /* ── Rule display row (row+3) ── */
    char rule_str[32];
    rule_to_string(rule_str, sizeof(rule_str));
    int rs = find_matching_ruleset();

    p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", re_row0 + 3, re_col0, bdr, bg);
    if (rs >= 0)
        p += sprintf(p, " \033[95;48;2;15;15;25m%s \033[93;48;2;15;15;25m%s",
                     rule_str, rulesets[rs].name);
    else
        p += sprintf(p, " \033[95;48;2;15;15;25m%s \033[33;48;2;15;15;25m(mutant)",
                     rule_str);
    RE_CLOSE_ROW(re_row0 + 3);

    /* ── Preset rows (row+4, row+5) ── */
    for (int row = 0; row < 2; row++) {
        int base = row * 5;
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                     re_row0 + 4 + row, re_col0, bdr, bg);
        for (int i = base; i < base + 5 && i < N_RULESETS; i++) {
            int is_cur = (rulesets[i].birth == birth_mask &&
                          rulesets[i].survival == survival_mask);
            if (is_cur)
                p += sprintf(p, "\033[97;48;2;50;50;80m%-7s%s", rulesets[i].name, bg);
            else {
                RGB zc = zone_colors[i];
                p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;15;15;25m%-7s%s",
                             zc.r, zc.g, zc.b, rulesets[i].name, bg);
            }
        }
        RE_CLOSE_ROW(re_row0 + 4 + row);
    }

    /* ── Bottom border ── */
    p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", re_row0 + 6, re_col0, bdr);
    for (int i = 0; i < RE_WIDTH - 2; i++) {
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80';
    }
    *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
    p += sprintf(p, "%s", rst);

    #undef RE_CLOSE_ROW
    *pp = p;
}

/* ── Rendering ─────────────────────────────────────────────────────────────── */

/* Larger buffer for true-color escape sequences */
static char render_buf[MAX_H * MAX_W * 40 + 16384];

/* Check if (x,y) is on a portal ring. Returns 8=entrance, 9=exit, 0=no */
static int portal_marker(int x, int y, RGB *out) {
    for (int i = 0; i < n_portals; i++) {
        int r = portals[i].radius;
        int r2_outer = r * r;
        int r2_inner = (r - 1) * (r - 1);
        /* Animated swirl: phase offset based on generation + angle */
        int phase = (generation / 2 + i * 37) % 24;

        /* Entrance ring (cyan/teal) */
        int dxa = x - portals[i].ax, dya = y - portals[i].ay;
        int d2a = dxa*dxa + dya*dya;
        if (d2a >= r2_inner && d2a <= r2_outer) {
            /* Swirl effect: brightness varies by angle */
            int angle = (dxa + dya + phase) & 7;
            int bright = 120 + angle * 18;
            if (bright > 255) bright = 255;
            *out = (RGB){ 0, (unsigned char)(bright * 3/4), (unsigned char)bright };
            return portals[i].active ? 8 : 10; /* 10 = incomplete entrance */
        }
        /* Center dot for entrance */
        if (d2a <= 1) {
            *out = (RGB){ 40, 220, 255 };
            return 8;
        }

        if (!portals[i].active) continue;

        /* Exit ring (magenta/pink) */
        int dxb = x - portals[i].bx, dyb = y - portals[i].by;
        int d2b = dxb*dxb + dyb*dyb;
        if (d2b >= r2_inner && d2b <= r2_outer) {
            int angle = (dxb + dyb + phase) & 7;
            int bright = 120 + angle * 18;
            if (bright > 255) bright = 255;
            *out = (RGB){ (unsigned char)bright, 0, (unsigned char)(bright * 3/4) };
            return 9;
        }
        /* Center dot for exit */
        if (d2b <= 1) {
            *out = (RGB){ 255, 40, 200 };
            return 9;
        }
    }
    /* Also show WIP entrance if placing */
    if (portal_mode && portal_placing) {
        int dx = x - portal_wip_x, dy = y - portal_wip_y;
        int d2 = dx*dx + dy*dy;
        int r = portal_radius;
        if (d2 >= (r-1)*(r-1) && d2 <= r*r) {
            int angle = (dx + dy + (generation/2)) & 7;
            int bright = 100 + angle * 20;
            if (bright > 255) bright = 255;
            *out = (RGB){ 0, (unsigned char)(bright * 3/4), (unsigned char)bright };
            return 10;
        }
        if (d2 <= 1) {
            *out = (RGB){ 40, 220, 255 };
            return 10;
        }
    }
    return 0;
}

/* Check if (x,y) is near an emitter or absorber. Returns: 4=emitter, 5=absorber, 0=neither */
static int source_sink_marker(int x, int y, RGB *out) {
    /* Emitters: bright pulsing cyan/white marker */
    for (int i = 0; i < n_emitters; i++) {
        int dx = x - emitters[i].x, dy = y - emitters[i].y;
        if (dx*dx + dy*dy <= 4) { /* radius ~2 visual marker */
            /* Pulse based on generation and rate */
            int phase = generation % (emitters[i].rate * 2);
            int bright = (phase < emitters[i].rate) ? 255 : 180;
            *out = (RGB){ (unsigned char)(bright * 3/4), (unsigned char)bright, (unsigned char)bright };
            return 4;
        }
    }
    /* Absorbers: dark purple/red vortex marker */
    for (int i = 0; i < n_absorbers; i++) {
        int dx = x - absorbers[i].x, dy = y - absorbers[i].y;
        int d2 = dx*dx + dy*dy;
        int r = absorbers[i].radius;
        if (d2 <= 4) { /* center marker */
            *out = (RGB){ 200, 30, 60 };
            return 5;
        }
        /* Show kill radius ring when in emit mode */
        if (emit_mode && d2 >= (r-1)*(r-1) && d2 <= r*r) {
            *out = (RGB){ 80, 15, 30 };
            return 5;
        }
    }
    return 0;
}

/* Get cell color for rendering (returns 1 if alive, fills rgb; 2 if ghost; 3 if zone bg; 4=emitter; 5=absorber) */
static int cell_color(int x, int y, RGB *out) {
    if (x < 0 || x >= W || y < 0 || y >= H) return 0;

    /* Portal markers */
    int pm = portal_marker(x, y, out);
    if (pm) return pm;

    /* Source/sink markers take priority for center pixels */
    int ss = source_sink_marker(x, y, out);
    if (ss) return ss;

    /* Frequency analysis overlay: color by oscillation period */
    if (freq_mode) {
        int period = freq_grid[y][x];
        if (period > 0) {
            *out = freq_to_rgb(period);
            /* Brighten currently-alive cells */
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 225 ? out->r + 30 : 255);
                out->g = (unsigned char)(out->g < 225 ? out->g + 30 : 255);
                out->b = (unsigned char)(out->b < 225 ? out->b + 30 : 255);
            } else {
                /* Dim dead cells in oscillation — show the pattern shape */
                out->r = out->r * 2 / 5;
                out->g = out->g * 2 / 5;
                out->b = out->b * 2 / 5;
            }
            return grid[y][x] ? 1 : 7; /* 7 = freq ghost (dead but periodic) */
        }
        return 0; /* truly dead — no period */
    }

    /* Entropy heatmap overlay: color by local Shannon entropy */
    if (entropy_mode) {
        float h = entropy_grid[y][x];
        if (h > 0.001f || grid[y][x]) {
            *out = entropy_to_rgb(h);
            /* Brighten alive cells for structure visibility */
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 8; /* 8 = entropy ghost */
        }
        return 0;
    }

    /* Lyapunov sensitivity overlay: perturbation divergence map */
    if (lyapunov_mode) {
        float raw = lyapunov_grid[y][x];
        /* Scale: multiply by 3 so moderate sensitivity fills the color range */
        float s = raw * 3.0f;
        if (s > 1.0f) s = 1.0f;
        if (s > 0.005f || grid[y][x]) {
            *out = lyapunov_to_rgb(s);
            if (grid[y][x]) {
                /* Brighten alive cells for structure visibility */
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 11; /* 11 = lyapunov ghost */
        }
        return 0;
    }

    /* Fourier spectrum overlay: 2D power spectrum mapped to grid space */
    if (fourier_mode) {
        float fp = fourier_grid[y][x];
        if (fp > 0.005f || grid[y][x]) {
            *out = fourier_to_rgb(fp);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 12; /* 12 = fourier ghost */
        }
        return 0;
    }

    /* Fractal dimension overlay: box-counting detail per cell */
    if (fractal_mode) {
        float fv = fractal_grid[y][x];
        if (fv > 0.005f || grid[y][x]) {
            *out = fractal_to_rgb(fv);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 13; /* 13 = fractal ghost */
        }
        return 0;
    }

    /* Information flow field overlay: directional causal influence */
    if (flow_mode) {
        float m = flow_mag[y][x];
        if (m > 0.001f || grid[y][x]) {
            *out = flow_to_rgb(m, flow_vx[y][x], flow_vy[y][x]);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 14; /* 14 = flow ghost */
        }
        return 0;
    }

    /* Phase-space attractor overlay: scatter plot of delay-embedded trajectory */
    if (attractor_mode && attr_n_pts > 2) {
        /* Map grid cell (x,y) to attractor canvas bin.
           The scatter plot occupies a centered square region of the viewport. */
        int vw = (view_w > 0) ? view_w : W;
        int vh = (view_h > 0) ? view_h : H;
        int plot_size = (vw < vh ? vw : vh) * 3 / 4; /* 75% of smaller dimension */
        int ox = (vw - plot_size) / 2 + view_x;       /* grid coords of plot origin */
        int oy = (vh - plot_size) / 2 + view_y;

        int rx = x - ox;
        int ry = y - oy;
        if (rx >= 0 && rx < plot_size && ry >= 0 && ry < plot_size) {
            int bx = rx * ATTR_BINS / plot_size;
            int by = ry * ATTR_BINS / plot_size;
            if (bx >= 0 && bx < ATTR_BINS && by >= 0 && by < ATTR_BINS) {
                int hits = attr_canvas[by][bx];
                if (hits > 0) {
                    /* Color by density: dark blue → cyan → yellow → white */
                    float t = (float)hits / attr_canvas_max;
                    /* Apply log scaling for better dynamic range */
                    t = logf(1.0f + t * 9.0f) / logf(10.0f);
                    if (t > 1.0f) t = 1.0f;

                    if (t < 0.33f) {
                        float s = t / 0.33f;
                        out->r = (unsigned char)(20 + 10 * s);
                        out->g = (unsigned char)(40 + 160 * s);
                        out->b = (unsigned char)(120 + 135 * s);
                    } else if (t < 0.66f) {
                        float s = (t - 0.33f) / 0.33f;
                        out->r = (unsigned char)(30 + 225 * s);
                        out->g = (unsigned char)(200 + 55 * s);
                        out->b = (unsigned char)(255 - 155 * s);
                    } else {
                        float s = (t - 0.66f) / 0.34f;
                        out->r = (unsigned char)(255);
                        out->g = (unsigned char)(255);
                        out->b = (unsigned char)(100 + 155 * s);
                    }
                    return 1;
                }
                /* Inside plot area but empty — dark background with faint grid */
                if ((bx % (ATTR_BINS / 4) == 0) || (by % (ATTR_BINS / 4) == 0)) {
                    out->r = 20; out->g = 25; out->b = 35;
                } else {
                    out->r = 6; out->g = 8; out->b = 14;
                }
                return 15; /* 15 = attractor background */
            }
        }
        /* Outside plot area — show normal cells dimmed */
        if (grid[y][x]) {
            out->r = 40; out->g = 45; out->b = 55;
            return 1;
        }
        return 0;
    }

    /* Causal light cone overlay: backward (blue/violet) + forward (orange/red) */
    if (cone_mode == 2) {
        int bd = cone_back[y][x]; /* backward depth+1 (0=not in cone) */
        int fd = cone_fwd[y][x];  /* forward Chebyshev distance (0=not in cone) */

        /* Selected cell: bright white marker */
        if (x == cone_sel_x && y == cone_sel_y) {
            out->r = 255; out->g = 255; out->b = 255;
            return 1;
        }

        if (bd > 0) {
            /* Backward cone: cool gradient violet → blue → cyan by depth */
            float t = (float)(bd - 1) / (cone_depth > 1 ? (float)cone_depth : 1.0f);
            if (t > 1.0f) t = 1.0f;
            /* Near (t~0): bright cyan/white, Far (t~1): deep violet */
            if (t < 0.5f) {
                float s = t / 0.5f;
                out->r = (unsigned char)(180 - 120 * s);
                out->g = (unsigned char)(240 - 140 * s);
                out->b = (unsigned char)(255);
            } else {
                float s = (t - 0.5f) / 0.5f;
                out->r = (unsigned char)(60 + 80 * s);
                out->g = (unsigned char)(100 - 60 * s);
                out->b = (unsigned char)(255 - 80 * s);
            }
            /* Brighten alive cells */
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 16; /* 16 = cone ghost */
        }

        if (fd > 0) {
            /* Forward cone: warm gradient orange → red by distance */
            float t = (float)fd / (cone_fwd_depth > 1 ? (float)cone_fwd_depth : 1.0f);
            if (t > 1.0f) t = 1.0f;
            /* Near (t~0): bright orange, Far (t~1): dim red */
            out->r = (unsigned char)(255 - 80 * t);
            out->g = (unsigned char)(160 - 130 * t);
            out->b = (unsigned char)(40 - 30 * t);
            /* Dim the forward cone more — it's theoretical, not actual */
            out->r = out->r * 2 / 5;
            out->g = out->g * 2 / 5;
            out->b = out->b * 2 / 5;
            /* Brighten alive cells */
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 200 ? out->r + 55 : 255);
                out->g = (unsigned char)(out->g < 200 ? out->g + 55 : 255);
                out->b = (unsigned char)(out->b < 200 ? out->b + 55 : 255);
            }
            return grid[y][x] ? 1 : 16;
        }

        /* Outside both cones — show normal cells very dimmed */
        if (grid[y][x]) {
            out->r = 25; out->g = 30; out->b = 35;
            return 1;
        }
        return 0;
    }

    /* Mutual information network overlay: inter-region coupling lines */
    if (mi_mode) {
        float mv = mi_overlay[y][x];
        if (mv > 0.001f) {
            *out = mi_to_rgb(mv);
            /* Brighten where live cells coincide with lines */
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return 1; /* line pixel */
        }
        /* Block center markers */
        if ((x % MI_BLK_W == MI_BLK_W / 2) && (y % MI_BLK_H == MI_BLK_H / 2)) {
            out->r = 140; out->g = 140; out->b = 180;
            return 1;
        }
        /* Block grid lines (very faint) */
        if ((x % MI_BLK_W == 0) || (y % MI_BLK_H == 0)) {
            if (grid[y][x]) {
                out->r = 40; out->g = 55; out->b = 65;
                return 1;
            }
            out->r = 10; out->g = 13; out->b = 18;
            return 18; /* MI grid ghost */
        }
        /* Normal cells dimmed */
        if (grid[y][x]) {
            out->r = 25; out->g = 45; out->b = 25;
            return 1;
        }
        return 0;
    }

    /* Topological feature map overlay: colored components + hole highlighting */
    if (topo_mode) {
        if (topo_hole[y][x]) {
            *out = topo_hole_rgb();
            return 20; /* hole ghost cell */
        }
        if (grid[y][x] && topo_label[y][x] > 0) {
            *out = topo_comp_to_rgb(topo_label[y][x] - 1);
            return 1;
        }
        /* Ghost: show faint component color for recently-dead cells with ghost trails */
        if (ghost[y][x] > 0) {
            out->r = 30; out->g = 20; out->b = 40;
            return 20;
        }
        return 0;
    }

    /* Kolmogorov complexity estimator overlay: algorithmic complexity */
    if (kc_mode) {
        float k = kc_grid[y][x];
        if (k > 0.005f || grid[y][x]) {
            *out = kc_to_rgb(k);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 22; /* 22 = kc ghost */
        }
        return 0;
    }

    /* Renormalization group flow overlay: multi-scale structure */
    if (rg_mode) {
        if (grid[y][x]) {
            *out = rg_to_rgb(rg_grid[y][x], rg_invariance[y][x]);
            return 1;
        }
        /* Show faint color for dead cells near structure */
        float inv = rg_invariance[y][x];
        float dom = rg_grid[y][x];
        if (inv > 0.01f || dom > 0.01f) {
            RGB c = rg_to_rgb(dom, inv);
            out->r = c.r / 4;
            out->g = c.g / 4;
            out->b = c.b / 4;
            return 21; /* 21 = rg ghost */
        }
        return 0;
    }

    /* Composite complexity index overlay: edge-of-chaos heatmap */
    if (cplx_mode) {
        float c = cplx_grid[y][x];
        if (c > 0.005f || grid[y][x]) {
            *out = cplx_to_rgb(c);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 19; /* 19 = complexity ghost */
        }
        return 0;
    }

    /* Prediction surprise field overlay: surprisal heatmap */
    if (surp_mode) {
        float s = surp_grid[y][x];
        if (s > 0.005f || grid[y][x]) {
            *out = surp_to_rgb(s);
            if (grid[y][x]) {
                out->r = (unsigned char)(out->r < 220 ? out->r + 35 : 255);
                out->g = (unsigned char)(out->g < 220 ? out->g + 35 : 255);
                out->b = (unsigned char)(out->b < 220 ? out->b + 35 : 255);
            }
            return grid[y][x] ? 1 : 17; /* 17 = surprise ghost */
        }
        return 0;
    }

    /* Temperature field overlay: show thermal landscape as blue→red wash */
    if (temp_mode) {
        float t = temp_global + temp_grid[y][x];
        if (t > 1.0f) t = 1.0f;
        if (t > 0.001f || grid[y][x]) {
            RGB trgb = temp_to_rgb(t);
            if (grid[y][x]) {
                /* Blend: 50% cell base color + 50% temperature wash for alive cells */
                RGB base;
                if (ecosystem_mode && species[y][x] > 0) {
                    base = (species[y][x] == 1) ? species_a_to_rgb(grid[y][x])
                                                 : species_b_to_rgb(grid[y][x]);
                } else if (heatmap_mode) {
                    base = age_to_rgb(grid[y][x]);
                } else {
                    base = (RGB){0, 200, 0};
                }
                out->r = (unsigned char)(base.r / 2 + trgb.r / 2);
                out->g = (unsigned char)(base.g / 2 + trgb.g / 2);
                out->b = (unsigned char)(base.b / 2 + trgb.b / 2);
            } else {
                /* Dead cells: dim temperature wash */
                out->r = trgb.r / 3;
                out->g = trgb.g / 3;
                out->b = trgb.b / 3;
            }
            return grid[y][x] ? 1 : 9; /* 9 = temperature ghost */
        }
        return 0;
    }

    /* Census spaceship overlay: cyan for detected moving structures */
    if (census_mode && grid[y][x] && census_ship[y][x]) {
        int age = grid[y][x];
        /* Bright cyan, modulated slightly by cell age for visual depth */
        int base = 160 + (age > 40 ? 40 : age) * 2;
        if (base > 255) base = 255;
        out->r = 0;
        out->g = (unsigned char)(base * 4 / 5);
        out->b = (unsigned char)base;
        return 1;
    }

    if (grid[y][x]) {
        if (ecosystem_mode && species[y][x] > 0) {
            if (species[y][x] == 1)
                *out = species_a_to_rgb(grid[y][x]);
            else
                *out = species_b_to_rgb(grid[y][x]);
        } else if (heatmap_mode)
            *out = age_to_rgb(grid[y][x]);
        else {
            /* flat green for non-heatmap */
            *out = (RGB){0, 200, 0};
        }
        /* Blend zone tint when zones are active */
        if (zone_enabled && zone_mode) {
            RGB zc = zone_colors[zone[y][x] % N_RULESETS];
            /* Subtle tint: 80% cell color + 20% zone color */
            out->r = (unsigned char)(out->r * 4/5 + zc.r * 1/5);
            out->g = (unsigned char)(out->g * 4/5 + zc.g * 1/5);
            out->b = (unsigned char)(out->b * 4/5 + zc.b * 1/5);
        }
        return 1;
    }
    if (heatmap_mode && ghost[y][x]) {
        *out = ghost_to_rgb(ghost[y][x]);
        return 2; /* ghost */
    }
    /* Signal tracer underlay: show trails for dead cells */
    if (tracer_mode > 0 && tracer[y][x] > 0) {
        *out = tracer_to_rgb(tracer[y][x]);
        return 6; /* tracer trail */
    }
    /* Show zone background tint when in zone mode */
    if (zone_enabled && zone_mode) {
        RGB zc = zone_colors[zone[y][x] % N_RULESETS];
        /* Very dim tint for empty cells */
        out->r = zc.r / 6;
        out->g = zc.g / 6;
        out->b = zc.b / 6;
        return 3; /* zone background */
    }
    return 0;
}

static void render(int running, int speed_ms, int draw_mode) {
    char *p = render_buf;

    /* cursor home */
    p += sprintf(p, "\033[H");

    /* status bar line 1 */
    const char *state = replay_mode
        ? "\033[93m\u23EA REPLAY\033[0m"
        : running
        ? "\033[92m\u25B6 RUN\033[0m"
        : "\033[93m\u23F8 PAUSE\033[0m";
    char topo_str[64] = "";
    if (topology > TOPO_FLAT) {
        snprintf(topo_str, sizeof(topo_str),
                 " \033[95m%s%s\033[0m", topo_symbols[topology], topo_names[topology]);
    }
    const char *draw_str = draw_mode
        ? " \033[96m\u270E DRAW\033[0m"
        : "";
    const char *heat_str = heatmap_mode
        ? " \033[91m\u2588HEAT\033[0m"
        : "";
    char sym_str[32] = "";
    if (symmetry > 0) {
        static const int folds[] = {0, 2, 4, 8};
        snprintf(sym_str, sizeof(sym_str),
                 " \033[93m\u2735SYM:%d\033[0m", folds[symmetry]);
    }

    /* Tracer indicator */
    char tracer_str[48] = "";
    if (tracer_mode == 1)
        snprintf(tracer_str, sizeof(tracer_str),
                 " \033[38;2;220;80;200m\u2593TRACE\033[0m");
    else if (tracer_mode == 2)
        snprintf(tracer_str, sizeof(tracer_str),
                 " \033[38;2;120;30;160m\u2593TRACE:FRZ\033[0m");

    /* Frequency analysis indicator */
    char freq_str[64] = "";
    if (freq_mode)
        snprintf(freq_str, sizeof(freq_str),
                 " \033[38;2;80;180;240m\u2261FREQ\033[0m");

    /* Entropy indicator */
    char entropy_str[96] = "";
    if (entropy_mode)
        snprintf(entropy_str, sizeof(entropy_str),
                 " \033[38;2;180;255;180m\u2234ENT:%.3f\033[0m", entropy_global);

    /* Lyapunov sensitivity indicator */
    char lyapunov_str[96] = "";
    if (lyapunov_mode)
        snprintf(lyapunov_str, sizeof(lyapunov_str),
                 " \033[38;2;220;200;60m\xce\xbbLYAP:%.4f\033[0m", lyapunov_global);

    /* Fourier spectrum indicator */
    char fourier_str[96] = "";
    if (fourier_mode)
        snprintf(fourier_str, sizeof(fourier_str),
                 " \033[38;2;80;200;255m\xe2\x89\x88" "FFT:\xce\xbb%.1f\033[0m",
                 fourier_dominant_wl);

    /* Fractal dimension indicator */
    char fractal_str[96] = "";
    if (fractal_mode)
        snprintf(fractal_str, sizeof(fractal_str),
                 " \033[38;2;230;200;40mD\xe2\x82\x93=%.3f\033[0m", fractal_dbox);

    /* Wolfram class indicator */
    char wolfram_str[96] = "";
    if (wolfram_mode) {
        static const char *class_names[] = {"?", "I", "II", "III", "IV"};
        static const char *class_colors[] = {"90", "38;2;80;80;120", "38;2;80;200;120", "38;2;255;80;60", "38;2;255;200;60"};
        int wc = (wolfram_class >= 1 && wolfram_class <= 4) ? wolfram_class : 0;
        snprintf(wolfram_str, sizeof(wolfram_str),
                 " \033[%sm\xe2\x97\x86" "W:%s(%.0f%%)\033[0m",
                 class_colors[wc], class_names[wc], wolfram_confidence * 100.0f);
    }

    /* Information flow field indicator */
    char flow_str[96] = "";
    if (flow_mode)
        snprintf(flow_str, sizeof(flow_str),
                 " \033[38;2;60;220;200m\xe2\x87\x89" "FLOW:%.4f\033[0m", flow_global_mag);

    /* Census indicator */
    char census_str[64] = "";
    if (census_mode)
        snprintf(census_str, sizeof(census_str),
                 " \033[38;2;100;220;160m\u2630CENSUS\033[0m");

    /* Light cone indicator */
    char cone_str[96] = "";
    if (cone_mode == 1)
        snprintf(cone_str, sizeof(cone_str),
                 " \033[38;2;200;140;60m\xe2\x97\x87" "CONE:?\033[0m");
    else if (cone_mode == 2)
        snprintf(cone_str, sizeof(cone_str),
                 " \033[38;2;200;140;60m\xe2\x97\x87" "CONE:(%d,%d)\033[0m",
                 cone_sel_x, cone_sel_y);

    /* Surprise field indicator */
    char surp_str[96] = "";
    if (surp_mode)
        snprintf(surp_str, sizeof(surp_str),
                 " \033[38;2;255;200;60m\xe2\x9a\xa1SURP:%.3f\033[0m", surp_global);

    /* MI network indicator */
    char mi_str[96] = "";
    if (mi_mode)
        snprintf(mi_str, sizeof(mi_str),
                 " \033[38;2;100;200;255m\xe2\x97\x88MI:%.3f\033[0m", mi_max_val);

    /* Complexity index indicator */
    char cplx_str[96] = "";
    if (cplx_mode)
        snprintf(cplx_str, sizeof(cplx_str),
                 " \033[38;2;235;210;30m\xe2\x9c\xa6" "CPLX:%.3f\033[0m", cplx_global);

    /* Topological feature map indicator */
    char topo_str2[96] = "";
    if (topo_mode)
        snprintf(topo_str2, sizeof(topo_str2),
                 " \033[38;2;180;100;240m\xe2\x97\x86TOPO:\xce\xb2\xe2\x82\x80=%d \xce\xb2\xe2\x82\x81=%d\033[0m",
                 topo_beta0, topo_beta1);

    /* Renormalization group flow indicator */
    char rg_str[96] = "";
    if (rg_mode)
        snprintf(rg_str, sizeof(rg_str),
                 " \033[38;2;100;220;200m\xe2\x97\x86RG:C=%.2f\033[0m", rg_criticality);

    /* Kolmogorov complexity indicator */
    char kc_str[96] = "";
    if (kc_mode)
        snprintf(kc_str, sizeof(kc_str),
                 " \033[38;2;240;180;40m\xe2\x97\x86KC:%.2f\033[0m", kc_global_mean);

    /* Genetic explorer indicator */
    char gene_str[64] = "";
    if (gene_mode == 1)
        snprintf(gene_str, sizeof(gene_str),
                 " \033[38;2;255;160;40m\u2042EVOLVE...\033[0m");
    else if (gene_mode == 2)
        snprintf(gene_str, sizeof(gene_str),
                 " \033[38;2;255;160;40m\u2042EVOLVE G%d\033[0m", gene_gen);

    /* Zoom indicator */
    char zoom_str[32] = "";
    if (zoom > 1)
        snprintf(zoom_str, sizeof(zoom_str),
                 " \033[94m\u2302%dx\033[0m", zoom);

    /* Minimap indicator */
    const char *map_str = (zoom > 1 && show_minimap)
        ? " \033[94m\u25A3MAP\033[0m" : "";

    /* Emit mode indicator */
    char emit_str[128] = "";
    if (emit_mode) {
        snprintf(emit_str, sizeof(emit_str),
                 " \033[38;2;180;255;255m\xe2\x97\x89" "EMIT:%s r%d\033[0m"
                 " \033[38;2;200;30;60m\xe2\x97\x8b" "SINK:r%d\033[0m"
                 " \033[90m(%dE/%dA)\033[0m",
                 emit_pattern_names[emit_pattern], emit_rate,
                 absorb_radius, n_emitters, n_absorbers);
    } else if (n_emitters > 0 || n_absorbers > 0) {
        snprintf(emit_str, sizeof(emit_str),
                 " \033[90m\xe2\x97\x89" "%dE/%dA\033[0m", n_emitters, n_absorbers);
    }

    /* Portal mode indicator */
    char portal_str[128] = "";
    if (portal_mode) {
        if (portal_placing)
            snprintf(portal_str, sizeof(portal_str),
                     " \033[38;2;40;220;255m\xE2\x97\x8EPORTAL:exit? r%d\033[0m"
                     " \033[90m(%d/%d)\033[0m",
                     portal_radius, n_portals, MAX_PORTALS);
        else
            snprintf(portal_str, sizeof(portal_str),
                     " \033[38;2;40;220;255m\xE2\x97\x8EPORTAL r%d\033[0m"
                     " \033[90m(%d/%d)\033[0m",
                     portal_radius, n_portals, MAX_PORTALS);
    } else if (n_portals > 0) {
        snprintf(portal_str, sizeof(portal_str),
                 " \033[90m\xE2\x97\x8E%dP\033[0m", n_portals);
    }

    /* Ecosystem mode indicator */
    char eco_str[128] = "";
    if (ecosystem_mode) {
        const char *sp_name = (brush_species == 1) ? "A" : "B";
        const char *sp_clr = (brush_species == 1) ? "38;2;60;120;255" : "38;2;255;80;40";
        char int_sign = interaction >= 0 ? '+' : '-';
        float int_abs = interaction >= 0 ? interaction : -interaction;
        snprintf(eco_str, sizeof(eco_str),
                 " \033[%sm\xe2\x97\x89" "ECO:%s %c%.1f\033[0m",
                 sp_clr, sp_name, int_sign, int_abs);
    }

    /* Temperature mode indicator */
    char temp_str[128] = "";
    if (temp_mode) {
        float eff_max = temp_global;
        float t_sum = 0.0f;
        int t_nonzero = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float t = temp_grid[y][x];
                if (t > 0.001f) { t_sum += t; t_nonzero++; }
                if (t + temp_global > eff_max) eff_max = t + temp_global;
            }
        float t_mean = t_nonzero > 0 ? t_sum / t_nonzero : 0.0f;
        snprintf(temp_str, sizeof(temp_str),
                 " \033[38;2;255;120;60m\xe2\x96\x93" "TEMP:G%.2f B%.2f\033[0m",
                 temp_global, temp_brush);
    } else if (temp_global > 0.001f) {
        snprintf(temp_str, sizeof(temp_str),
                 " \033[90m\xe2\x96\x93" "T:%.2f\033[0m", temp_global);
    }

    /* Zone mode indicator */
    char zone_str[80] = "";
    if (zone_mode) {
        RGB zc = zone_colors[zone_brush % N_RULESETS];
        snprintf(zone_str, sizeof(zone_str),
                 " \033[38;2;%d;%d;%dm\u25A8ZONE:%s\033[0m",
                 zc.r, zc.g, zc.b, rulesets[zone_brush].name);
    } else if (zone_enabled) {
        snprintf(zone_str, sizeof(zone_str),
                 " \033[90m\u25A8ZONES\033[0m");
    }

    /* Stamp mode indicator */
    char stamp_str[128] = "";
    if (stamp_mode) {
        static const char *rot_labels[] = {"0\xC2\xB0","90\xC2\xB0","180\xC2\xB0","270\xC2\xB0"};
        snprintf(stamp_str, sizeof(stamp_str),
                 " \033[38;2;255;200;60m\xe2\x96\xa3" "STAMP:%s %s\033[0m",
                 stamp_library[stamp_sel].name, rot_labels[stamp_rot]);
    }

    /* Rule string */
    char rule_str[32];
    rule_to_string(rule_str, sizeof(rule_str));
    int rs = find_matching_ruleset();
    char rule_display[80];
    if (zone_enabled && !zone_mode) {
        snprintf(rule_display, sizeof(rule_display),
                 "\033[90mmulti-zone\033[0m");
    } else if (rs >= 0)
        snprintf(rule_display, sizeof(rule_display),
                 "\033[95m%s\033[90m(%s)\033[0m", rule_str, rulesets[rs].name);
    else
        snprintf(rule_display, sizeof(rule_display),
                 "\033[95m%s\033[33m(mutant)\033[0m", rule_str);

    p += sprintf(p, " %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s  %s  Gen \033[96m%d\033[0m  Pop \033[96m%d\033[0m  "
                     "\033[90m%dms\033[0m",
                 state, topo_str, draw_str, heat_str, tracer_str, freq_str, entropy_str, lyapunov_str, fourier_str, fractal_str, wolfram_str, flow_str, census_str, cone_str, surp_str, mi_str, cplx_str, topo_str2, rg_str, kc_str, gene_str, temp_str, sym_str, zoom_str, map_str, zone_str,
                 portal_str, emit_str, eco_str, stamp_str, rule_display, generation, population, speed_ms);

    /* Flash message (save/load feedback) */
    if (flash_active()) {
        p += sprintf(p, "  \033[97;42m %s \033[0m", flash_msg);
    }

    /* sparkline right after stats */
    if (show_graph && hist_count > 1) {
        p += sprintf(p, "  ");
        render_sparkline(&p, 40);
    }

    /* Timeline bar inline */
    if (show_timeline && tl_len > 1) {
        p += sprintf(p, "  ");
        render_timeline_bar(&p, 50);
    }

    p += sprintf(p, "\033[K\n");

    /* status bar line 2: compact help */
    p += sprintf(p, " \033[90m[SPC]play [s]step [r]rand [c]clr "
                     "[1-5]pre [d]draw [k]sym [g]graph [y]dash [w]topo [h]heat [T]trace [f]freq [i]ent [L]lyap [u]fft "
                     "[X]temp [C]class [O]flow [9]cone [!]surp [@]mi [#]cplx [$]topo [/]rule [m]mut [b]edit [G]evolve [j]zone [e]emit [W]worm [a]eco [6]sp {/}int "
                     "[S]stamp [v]census [z/x]zoom [n]map [<>]time [t]tbar [P]snap C-p:seq "
                     "C-s:save C-o:load C-e:rle [q]quit\033[0m\033[K\n");

    int usable_rows = term_rows - 3;
    if (usable_rows < 5) usable_rows = 5;

    if (zoom == 1) {
        /* ── Normal zoom: 2 chars per cell ── */
        for (int row = 0; row < usable_rows && row < view_h; row++) {
            int gy = view_y + row;
            for (int col = 0; col < view_w; col++) {
                int gx = view_x + col;
                RGB c;
                int t = cell_color(gx, gy, &c);
                if (t == 1) {
                    /* Show arrow glyphs at block centers in flow mode */
                    if (flow_mode && (gx % FLOW_BLOCK == FLOW_BLOCK/2) && (gy % FLOW_BLOCK == FLOW_BLOCK/2)) {
                        float m = flow_mag[gy][gx];
                        const char *arrow = flow_arrow(flow_vx[gy][gx], flow_vy[gy][gx], m);
                        p += sprintf(p, "\033[48;2;%d;%d;%dm\033[38;2;255;255;255m%s \033[0m",
                                     c.r, c.g, c.b, arrow);
                    } else {
                        p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                    }
                } else if (t == 2) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xC2\xB7 \033[0m", c.r, c.g, c.b);
                } else if (t == 3) {
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else if (t == 4) {
                    /* Emitter: bright marker */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm\033[97m\xe2\x97\x89 \033[0m", c.r/3, c.g/3, c.b/3);
                } else if (t == 5) {
                    /* Absorber: dark vortex marker */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm\033[97m\xe2\x97\x8b \033[0m", c.r/3, c.g/3, c.b/3);
                } else if (t == 6) {
                    /* Tracer trail: colored background */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else if (t == 7) {
                    /* Freq analysis ghost: dim colored background */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else if (t == 8) {
                    /* Portal entrance: swirling cyan ring */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm\033[97m\xE2\x97\x8E \033[0m", c.r/3, c.g/3, c.b/3);
                } else if (t == 9) {
                    /* Portal exit: swirling magenta ring */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm\033[97m\xE2\x97\x8E \033[0m", c.r/3, c.g/3, c.b/3);
                } else if (t == 10) {
                    /* WIP portal entrance (placing) */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm\033[97m\xE2\x97\x8C \033[0m", c.r/3, c.g/3, c.b/3);
                } else if (t == 14) {
                    /* Flow field ghost: show arrow glyph at block centers, colored bg elsewhere */
                    int in_center = (gx % FLOW_BLOCK == FLOW_BLOCK/2) && (gy % FLOW_BLOCK == FLOW_BLOCK/2);
                    if (in_center) {
                        float m = flow_mag[gy][gx];
                        const char *arrow = flow_arrow(flow_vx[gy][gx], flow_vy[gy][gx], m);
                        p += sprintf(p, "\033[48;2;%d;%d;%dm\033[38;2;255;255;255m%s \033[0m",
                                     c.r/2, c.g/2, c.b/2, arrow);
                    } else {
                        p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                    }
                } else if (t == 18) {
                    /* MI network grid lines: dim colored background */
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else {
                    *p++ = ' '; *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    } else if (zoom == 2) {
        /* ── Half-block zoom: 1 char per cell, 2 rows per terminal row ── */
        /* Use ▀ (upper half block) with fg=top cell, bg=bottom cell */
        int vis_w = view_w < term_cols ? view_w : term_cols;
        for (int row = 0; row < usable_rows; row++) {
            int gy_top = view_y + row * 2;
            int gy_bot = gy_top + 1;
            for (int col = 0; col < vis_w; col++) {
                int gx = view_x + col;
                RGB ct, cb;
                int tt = cell_color(gx, gy_top, &ct);
                int tb = cell_color(gx, gy_bot, &cb);

                /* Treat emitter(4), absorber(5), tracer(6), freq(7), portal(8,9,10) as solid colored cells */
                int tt_solid = (tt == 1 || tt >= 4);
                int tb_solid = (tb == 1 || tb >= 4);
                if (tt_solid && tb_solid) {
                    p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b, cb.r, cb.g, cb.b);
                } else if (tt_solid && tb == 3) {
                    p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b, cb.r, cb.g, cb.b);
                } else if (tb_solid && tt == 3) {
                    p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x84\033[0m",
                                 cb.r, cb.g, cb.b, ct.r, ct.g, ct.b);
                } else if (tt_solid) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b);
                } else if (tb_solid) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84\033[0m",
                                 cb.r, cb.g, cb.b);
                } else if (tt == 2 || tb == 2) {
                    /* ghost in either half — dim dot */
                    RGB gc = tt == 2 ? ct : cb;
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xC2\xB7\033[0m",
                                 gc.r, gc.g, gc.b);
                } else if (tt == 3 && tb == 3) {
                    /* both zone bg */
                    p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b, cb.r, cb.g, cb.b);
                } else if (tt == 3) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b);
                } else if (tb == 3) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84\033[0m",
                                 cb.r, cb.g, cb.b);
                } else {
                    *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    } else if (zoom == 4) {
        /* ── Quarter zoom: 2 cells per char wide, 2 cells per row tall ── */
        /* Use quadrant block chars: ▘▝▀▖▌▞▛▗▚▐▜▄▙▟█ */
        /* Encoding: bit0=TL, bit1=TR, bit2=BL, bit3=BR */
        static const char *quad_chars[16] = {
            " ",                    /* 0000 */
            "\xe2\x96\x98",        /* 0001 ▘ TL */
            "\xe2\x96\x9d",        /* 0010 ▝ TR */
            "\xe2\x96\x80",        /* 0011 ▀ top */
            "\xe2\x96\x96",        /* 0100 ▖ BL */
            "\xe2\x96\x8c",        /* 0101 ▌ left */
            "\xe2\x96\x9e",        /* 0110 ▞ diag */
            "\xe2\x96\x9b",        /* 0111 ▛ TL+TR+BL */
            "\xe2\x96\x97",        /* 1000 ▗ BR */
            "\xe2\x96\x9a",        /* 1001 ▚ anti-diag */
            "\xe2\x96\x90",        /* 1010 ▐ right */
            "\xe2\x96\x9c",        /* 1011 ▜ TL+TR+BR */
            "\xe2\x96\x84",        /* 1100 ▄ bottom */
            "\xe2\x96\x99",        /* 1101 ▙ TL+BL+BR */
            "\xe2\x96\x9f",        /* 1110 ▟ TR+BL+BR */
            "\xe2\x96\x88",        /* 1111 █ full */
        };
        int vis_w = (view_w + 1) / 2;
        if (vis_w > term_cols) vis_w = term_cols;
        for (int row = 0; row < usable_rows; row++) {
            int gy0 = view_y + row * 2;
            int gy1 = gy0 + 1;
            for (int col = 0; col < vis_w; col++) {
                int gx0 = view_x + col * 2;
                int gx1 = gx0 + 1;

                /* Check which quadrants are alive or zone-tinted */
                RGB dummy;
                int tl_t = cell_color(gx0, gy0, &dummy);
                int tr_t = cell_color(gx1, gy0, &dummy);
                int bl_t = cell_color(gx0, gy1, &dummy);
                int br_t = cell_color(gx1, gy1, &dummy);
                int tl = (tl_t == 1 || tl_t >= 4);
                int tr = (tr_t == 1 || tr_t >= 4);
                int bl = (bl_t == 1 || bl_t >= 4);
                int br = (br_t == 1 || br_t >= 4);
                int bits = tl | (tr << 1) | (bl << 2) | (br << 3);

                /* In zone mode, also show zone bg for empty cells */
                int zone_bits = 0;
                if (zone_enabled && zone_mode && bits != 15) {
                    if (!tl && tl_t == 3) zone_bits |= 1;
                    if (!tr && tr_t == 3) zone_bits |= 2;
                    if (!bl && bl_t == 3) zone_bits |= 4;
                    if (!br && br_t == 3) zone_bits |= 8;
                }

                if (bits && heatmap_mode) {
                    /* Average color of alive cells */
                    int rr = 0, gg = 0, bb = 0, cnt = 0;
                    RGB c;
                    if (tl && cell_color(gx0, gy0, &c)) { rr += c.r; gg += c.g; bb += c.b; cnt++; }
                    if (tr && cell_color(gx1, gy0, &c)) { rr += c.r; gg += c.g; bb += c.b; cnt++; }
                    if (bl && cell_color(gx0, gy1, &c)) { rr += c.r; gg += c.g; bb += c.b; cnt++; }
                    if (br && cell_color(gx1, gy1, &c)) { rr += c.r; gg += c.g; bb += c.b; cnt++; }
                    if (cnt) {
                        p += sprintf(p, "\033[38;2;%d;%d;%dm", rr/cnt, gg/cnt, bb/cnt);
                    } else {
                        p += sprintf(p, "\033[37m");
                    }
                } else if (zone_bits && !bits) {
                    /* Pure zone background — use average zone color */
                    int rr = 0, gg = 0, bb = 0, cnt = 0;
                    int coords[4][2] = {{gx0,gy0},{gx1,gy0},{gx0,gy1},{gx1,gy1}};
                    for (int q = 0; q < 4; q++) {
                        if (zone_bits & (1 << q)) {
                            RGB zc;
                            cell_color(coords[q][0], coords[q][1], &zc);
                            rr += zc.r; gg += zc.g; bb += zc.b; cnt++;
                        }
                    }
                    if (cnt) p += sprintf(p, "\033[38;2;%d;%d;%dm", rr/cnt, gg/cnt, bb/cnt);
                    bits = zone_bits; /* use zone_bits for char selection */
                } else if (bits) {
                    p += sprintf(p, "\033[92m"); /* green */
                }

                const char *ch = quad_chars[bits];
                while (*ch) *p++ = *ch++;

                if (bits) {
                    p += sprintf(p, "\033[0m");
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    } else { /* zoom == 8 */
        /* ── Braille ultra-density: 2 cells wide × 4 cells tall per char ── */
        /* Unicode Braille U+2800-U+28FF encodes 8 dots in a 2×4 grid:
         * dot1(0,0)=0x01 dot4(1,0)=0x08
         * dot2(0,1)=0x02 dot5(1,1)=0x10
         * dot3(0,2)=0x04 dot6(1,2)=0x20
         * dot7(0,3)=0x40 dot8(1,3)=0x80 */
        int vis_w = (view_w + 1) / 2;
        if (vis_w > term_cols) vis_w = term_cols;
        for (int row = 0; row < usable_rows; row++) {
            int gy0 = view_y + row * 4;
            for (int col = 0; col < vis_w; col++) {
                int gx0 = view_x + col * 2;
                int gx1 = gx0 + 1;

                /* Sample 8 cells in the 2×4 block */
                int coords_x[8] = {gx0, gx0, gx0, gx1, gx1, gx1, gx0, gx1};
                int coords_y[8] = {gy0, gy0+1, gy0+2, gy0, gy0+1, gy0+2, gy0+3, gy0+3};
                int braille_bits[8] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80};

                unsigned char dots = 0;
                int rr = 0, gg = 0, bb = 0, cnt = 0;
                for (int d = 0; d < 8; d++) {
                    int cx = coords_x[d], cy = coords_y[d];
                    if (cx >= W || cy >= H) continue;
                    RGB c;
                    int t = cell_color(cx, cy, &c);
                    int solid = (t == 1 || t >= 4);
                    int ghost_dot = (t == 2);
                    int zone_bg = (t == 3);
                    if (solid) {
                        dots |= braille_bits[d];
                        rr += c.r; gg += c.g; bb += c.b; cnt++;
                    } else if (ghost_dot) {
                        dots |= braille_bits[d];
                        rr += c.r; gg += c.g; bb += c.b; cnt++;
                    } else if (zone_enabled && zone_mode && zone_bg) {
                        dots |= braille_bits[d];
                        rr += c.r; gg += c.g; bb += c.b; cnt++;
                    }
                }

                if (dots) {
                    if (cnt) {
                        p += sprintf(p, "\033[38;2;%d;%d;%dm", rr/cnt, gg/cnt, bb/cnt);
                    } else {
                        p += sprintf(p, "\033[92m");
                    }
                    /* Encode braille character: U+2800 + dots */
                    /* UTF-8: E2, A0+(dots>>6), 80+(dots&0x3F) */
                    *p++ = '\xe2';
                    *p++ = (char)(0xa0 + (dots >> 6));
                    *p++ = (char)(0x80 + (dots & 0x3f));
                    p += sprintf(p, "\033[0m");
                } else {
                    *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    }

    /* Minimap overlay (only when zoomed) */
    render_minimap(&p);

    /* Frequency analysis legend overlay */
    if (freq_mode) {
        int usable = term_rows - 3;
        int leg_h = 9;
        int leg_w = 22;
        int leg_col = 2;
        int leg_row = 3 + usable - leg_h;
        if (leg_row >= 3 && leg_col + leg_w < term_cols) {
            const char *bdr = "\033[38;2;80;100;140;48;2;10;10;20m";
            const char *bg = "\033[48;2;10;10;20m";
            const char *rst = "\033[0m";
            /* Top border */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Frequency ", leg_row, leg_col, bdr);
            for (int i = 0; i < leg_w - 13; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
            *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
            p += sprintf(p, "%s", rst);
            /* Legend entries */
            struct { const char *label; int period; } entries[] = {
                {"Still life", 1}, {"Period 2", 2}, {"Period 3", 3},
                {"Period 4-5", 4}, {"Period 6-12", 8}, {"Period 13+", 20}, {"Chaotic", 255}
            };
            for (int i = 0; i < 7; i++) {
                RGB ec = freq_to_rgb(entries[i].period);
                p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", leg_row + 1 + i, leg_col, bdr, bg);
                p += sprintf(p, " \033[48;2;%d;%d;%dm  %s \033[38;2;%d;%d;%d;48;2;10;10;20m%-11s",
                             ec.r, ec.g, ec.b, bg, ec.r, ec.g, ec.b, entries[i].label);
                p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", leg_row + 1 + i, leg_col + leg_w + 1, bdr, rst);
            }
            /* Bottom border */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", leg_row + 8, leg_col, bdr);
            for (int i = 0; i < leg_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
            *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
            p += sprintf(p, "%s", rst);
        }
    }

    /* Rule editor overlay */
    render_rule_editor(&p);

    /* Stamp preview overlay (bottom-right corner) */
    if (stamp_mode) {
        const RotatedStamp *rs = &stamp_rotations[stamp_sel][stamp_rot];
        /* Find bounding box */
        int sminx = 999, sminy = 999, smaxx = -999, smaxy = -999;
        for (int i = 0; i < rs->n; i++) {
            if (rs->cells[i].dx < sminx) sminx = rs->cells[i].dx;
            if (rs->cells[i].dy < sminy) sminy = rs->cells[i].dy;
            if (rs->cells[i].dx > smaxx) smaxx = rs->cells[i].dx;
            if (rs->cells[i].dy > smaxy) smaxy = rs->cells[i].dy;
        }
        int pw = smaxx - sminx + 1;
        int ph = smaxy - sminy + 1;
        /* Clamp preview size */
        if (pw > 40) pw = 40;
        if (ph > 16) ph = 16;

        int box_w = (pw > 12 ? pw : 12) + 4; /* min width for title */
        int box_h = ph + 3;  /* title + cells + bottom border */
        int box_col = term_cols - box_w - 1;
        int box_row = term_rows - box_h;
        if (box_col < 1) box_col = 1;
        if (box_row < 3) box_row = 3;

        const char *bdr = "\033[38;2;255;200;60;48;2;15;15;25m";
        const char *bg = "\033[48;2;15;15;25m";
        const char *rst = "\033[0m";
        static const char *rot_labels[] = {"0\xC2\xB0","90\xC2\xB0","180\xC2\xB0","270\xC2\xB0"};

        /* Top border with name */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 %s %s ",
                     box_row, box_col, bdr, stamp_library[stamp_sel].name, rot_labels[stamp_rot]);
        int title_len = (int)strlen(stamp_library[stamp_sel].name) + (int)strlen(rot_labels[stamp_rot]) + 4;
        for (int i = title_len; i < box_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", rst);

        /* Pattern cells */
        for (int row = 0; row < ph; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s", box_row + 1 + row, box_col, bdr, bg);
            for (int col = 0; col < box_w - 1; col++) {
                int cx = sminx + col;
                int cy = sminy + row;
                int found = 0;
                if (col < pw) {
                    for (int i = 0; i < rs->n; i++) {
                        if (rs->cells[i].dx == cx && rs->cells[i].dy == cy) {
                            found = 1; break;
                        }
                    }
                }
                if (found)
                    p += sprintf(p, "\033[38;2;255;200;60;48;2;15;15;25m\xe2\x96\x88");
                else
                    *p++ = ' ';
            }
            p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst);
        }

        /* Bottom border with hint */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94\xe2\x94\x80 [/]sel \xe2\x86\x95rot ",
                     box_row + 1 + ph, box_col, bdr);
        int hint_len = 12;
        for (int i = hint_len; i < box_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", rst);
    }

    /* Census overlay (top-left, below status bars) */
    if (census_mode) {
        int usable = term_rows - 3;
        int cen_h = census_n_display + 3; /* top border + entries + unmatched + bottom border */
        int cen_w = 24;
        int cen_col = 2;
        int cen_row = 3; /* just below status bars */
        if (cen_row + cen_h > 3 + usable) cen_h = 3 + usable - cen_row;

        const char *bdr = "\033[38;2;100;220;160;48;2;10;18;12m";
        const char *bg = "\033[48;2;10;18;12m";
        const char *rst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Census (%d) ",
                     cen_row, cen_col, bdr, census_total);
        int title_len = 13;
        { int t = census_total; if (t >= 10) title_len++; if (t >= 100) title_len++; if (t >= 1000) title_len++; }
        for (int i = title_len; i < cen_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", rst);

        /* Pattern entries */
        int row_i = 1;
        for (int d = 0; d < census_n_display && row_i < cen_h - 1; d++, row_i++) {
            /* Color by type: green=still life, amber=oscillator, cyan=spaceship */
            const char *clr;
            if (census_display[d].type == 2)
                clr = "\033[38;2;80;220;255m"; /* cyan for spaceships */
            else if (census_display[d].type == 1)
                clr = "\033[38;2;255;200;80m"; /* amber for oscillators */
            else
                clr = "\033[38;2;80;255;160m"; /* green for still lifes */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s %s%-10s\033[38;2;200;200;200m %3d%s",
                         cen_row + row_i, cen_col, bdr, bg,
                         clr, census_display[d].name, census_display[d].count, bg);
            /* Pad to width */
            int entry_len = 16; /* " name(10) count(4)" */
            for (int i = entry_len; i < cen_w - 1; i++) *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst);
        }

        /* Unmatched cells row */
        if (row_i < cen_h - 1) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s \033[38;2;120;120;120m\xe2\x80\xa6other   %4d%s",
                         cen_row + row_i, cen_col, bdr, bg, census_unmatched, bg);
            int entry_len = 16;
            for (int i = entry_len; i < cen_w - 1; i++) *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst);
            row_i++;
        }

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", cen_row + row_i, cen_col, bdr);
        for (int i = 0; i < cen_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", rst);

        /* Spaceship direction arrows overlay — place a Unicode arrow at the
           center of each detected spaceship's bounding box */
        if (zoom == 0 && ship_n_detections > 0) {
            for (int si = 0; si < ship_n_detections; si++) {
                ShipDetection *sd = &ship_detections[si];
                /* Center of the bounding box in grid coords */
                int cx = sd->x + sd->w / 2;
                int cy = sd->y + sd->h / 2;
                /* Convert grid coords to screen coords (accounting for viewport) */
                int sx = cx - view_x;
                int sy = cy - view_y;
                if (sx < 0 || sy < 0) continue;
                /* Screen position: each cell is 2 chars wide, rows start at line 3 */
                int scol = 1 + sx * 2;
                int srow = 3 + sy;
                if (srow < 3 || srow > term_rows - 1) continue;
                if (scol < 1 || scol > term_cols - 2) continue;
                /* Render cyan arrow */
                p += sprintf(p, "\033[%d;%dH\033[38;2;80;220;255m%s\033[0m",
                             srow, scol, ship_arrows[sd->dir]);
            }
        }
    }

    /* Genetic Rule Explorer overlay (center of screen) */
    if (gene_mode == 2 && gene_search_done) {
        int ge_w = 36;
        int ge_h = GENBEST_SIZE + 4; /* border + header + entries + footer + border */
        int ge_col = (term_cols - ge_w) / 2;
        int ge_row = (term_rows - ge_h) / 2;
        if (ge_col < 1) ge_col = 1;
        if (ge_row < 3) ge_row = 3;

        const char *bdr = "\033[38;2;255;160;40;48;2;20;14;4m";
        const char *bg = "\033[48;2;20;14;4m";
        const char *rst2 = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Evolve G%d ",
                     ge_row, ge_col, bdr, gene_gen);
        int title_len = 12;
        { int t = gene_gen; while (t >= 10) { title_len++; t /= 10; } }
        for (int i = title_len; i < ge_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", rst2);

        /* Header */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s \033[38;2;200;200;200m#  Rule             Score%s",
                     ge_row + 1, ge_col, bdr, bg, bg);
        for (int i = 26; i < ge_w - 1; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst2);

        /* Candidate entries */
        for (int i = 0; i < GENBEST_SIZE; i++) {
            int row = ge_row + 2 + i;
            int is_sel = (gene_selected == i);
            const char *hi = is_sel ? "\033[48;2;60;40;10m" : bg;
            const char *nclr = is_sel ? "\033[38;2;255;255;200m" : "\033[38;2;255;200;80m";
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s %s%d  %-18s %5.1f%s",
                         row, ge_col, bdr, hi,
                         nclr, i + 1, gene_best[i].label, (double)gene_best[i].score, hi);
            /* pad */
            int entry_len = 28;
            { float sc = gene_best[i].score; if (sc >= 10) entry_len++; if (sc >= 100) entry_len++; }
            for (int j = entry_len; j < ge_w - 1; j++) *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst2);
        }

        /* Footer with instructions */
        int frow = ge_row + 2 + GENBEST_SIZE;
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s \033[38;2;140;140;100m1-5:load G:evolve q:close%s",
                     frow, ge_col, bdr, bg, bg);
        for (int i = 26; i < ge_w - 1; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst2);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", frow + 1, ge_col, bdr);
        for (int i = 0; i < ge_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", rst2);
    }

    /* ── Entropy Heatmap overlay panel ────────────────────────────────────── */
    if (entropy_mode) {
        int ent_w = 44;
        int ent_col = term_cols - ent_w - 2;
        int ent_row = 3;
        if (ent_col < 1) ent_col = 1;

        const char *ebdr = "\033[38;2;120;255;160;48;2;8;16;8m";
        const char *ebg  = "\033[48;2;8;16;8m";
        const char *erst  = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Entropy Map ",
                     ent_row, ent_col, ebdr);
        for (int i = 14; i < ent_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", erst);

        /* Stats row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     ent_row + 1, ent_col, ebdr, ebg);
        p += sprintf(p, " \033[38;2;120;255;160mH\xcc\x84=\033[38;2;255;255;255m%.4f"
                     "  \033[38;2;120;255;160mH\xe2\x82\x98\xe2\x82\x90\xe2\x82\x93=\033[38;2;255;255;255m%.4f",
                     entropy_global, entropy_max_local);
        /* Pad */
        p += sprintf(p, "%s", ebg);
        for (int i = 0; i < 8; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", ebdr, erst);

        /* Distribution histogram: bin entropy into 10 buckets, show as bar */
        int bins[10] = {0};
        int total_cells = 0;
        for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++) {
                float h = entropy_grid[y][x];
                int b = (int)(h * 9.99f);
                if (b < 0) b = 0;
                if (b > 9) b = 9;
                bins[b]++;
                total_cells++;
            }
        int bmax = 1;
        for (int i = 0; i < 10; i++)
            if (bins[i] > bmax) bmax = bins[i];

        /* Label row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     ent_row + 2, ent_col, ebdr, ebg);
        p += sprintf(p, " \033[38;2;80;120;80m0.0");
        for (int i = 0; i < 24; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;80;120;80mEntropy");
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "1.0 ");
        p += sprintf(p, "%s\xe2\x94\x82%s", ebdr, erst);

        /* 4 rows of histogram bars */
        for (int row = 0; row < 4; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         ent_row + 3 + row, ent_col, ebdr, ebg);
            int level = 3 - row; /* top=3, bottom=0 */
            for (int b = 0; b < 10; b++) {
                int bar_h = bins[b] * 4 / bmax;
                /* Each bin gets 4 chars wide */
                for (int c = 0; c < 4; c++) {
                    if (bar_h > level) {
                        /* Color by bin position: green → white → magenta */
                        RGB bc = entropy_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88",
                                     bc.r, bc.g, bc.b);
                    } else if (bar_h == level) {
                        RGB bc = entropy_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84",
                                     bc.r/2, bc.g/2, bc.b/2);
                    } else {
                        p += sprintf(p, "\033[38;2;15;25;15m\xc2\xb7");
                    }
                }
            }
            /* Pad remaining */
            *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", ebdr, erst);
        }

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", ent_row + 7, ent_col, ebdr);
        for (int i = 0; i < ent_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", erst);
    }

    /* ── Temperature Field overlay panel ──────────────────────────────────── */
    if (temp_mode) {
        int tp_w = 44;
        int tp_col = term_cols - tp_w - 2;
        int tp_row = entropy_mode ? 12 : 3; /* below entropy panel if both active */
        if (tp_col < 1) tp_col = 1;

        const char *tbdr = "\033[38;2;255;140;60;48;2;16;8;4m";
        const char *tbg  = "\033[48;2;16;8;4m";
        const char *trst  = "\033[0m";

        /* Compute stats */
        float t_sum = 0.0f, t_max = 0.0f;
        int t_nonzero = 0;
        for (int ty = 0; ty < H; ty++)
            for (int tx = 0; tx < W; tx++) {
                float t = temp_grid[ty][tx];
                if (t > 0.001f) { t_sum += t; t_nonzero++; }
                if (t > t_max) t_max = t;
            }
        float t_mean = t_nonzero > 0 ? (t_sum / t_nonzero) : 0.0f;

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Temperature ",
                     tp_row, tp_col, tbdr);
        for (int i = 14; i < tp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", trst);

        /* Stats row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 1, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;255;180;100mGlobal=\033[38;2;255;255;255m%.3f"
                     "  \033[38;2;255;180;100mBrush=\033[38;2;255;255;255m%.2f"
                     "  \033[38;2;255;180;100mr=\033[38;2;255;255;255m%d",
                     temp_global, temp_brush, temp_brush_radius);
        /* Pad */
        p += sprintf(p, "%s", tbg);
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Stats row 2 */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 2, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;255;180;100mT\xcc\x84=\033[38;2;255;255;255m%.4f"
                     "  \033[38;2;255;180;100mT\xe2\x82\x98\xe2\x82\x90\xe2\x82\x93=\033[38;2;255;255;255m%.4f"
                     "  \033[38;2;255;180;100mhot=\033[38;2;255;255;255m%d",
                     t_mean, t_max, t_nonzero);
        p += sprintf(p, "%s", tbg);
        for (int i = 0; i < 6; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Distribution histogram: bin temperature into 10 buckets */
        int tbins[10] = {0};
        for (int ty = 0; ty < H; ty++)
            for (int tx = 0; tx < W; tx++) {
                float t = temp_grid[ty][tx] + temp_global;
                if (t > 1.0f) t = 1.0f;
                int b = (int)(t * 9.99f);
                if (b < 0) b = 0;
                if (b > 9) b = 9;
                tbins[b]++;
            }
        int tbmax = 1;
        for (int i = 0; i < 10; i++)
            if (tbins[i] > tbmax) tbmax = tbins[i];

        /* Label row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 3, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;60;80;140m0.0");
        for (int i = 0; i < 22; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;120;90;80mTemperature");
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "1.0 ");
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* 4 rows of histogram bars */
        for (int row = 0; row < 4; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         tp_row + 4 + row, tp_col, tbdr, tbg);
            int level = 3 - row;
            for (int b = 0; b < 10; b++) {
                int bar_h = tbins[b] * 4 / tbmax;
                for (int c = 0; c < 4; c++) {
                    if (bar_h > level) {
                        RGB bc = temp_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88",
                                     bc.r, bc.g, bc.b);
                    } else if (bar_h == level) {
                        RGB bc = temp_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84",
                                     bc.r/2, bc.g/2, bc.b/2);
                    } else {
                        p += sprintf(p, "\033[38;2;20;12;8m\xc2\xb7");
                    }
                }
            }
            *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);
        }

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", tp_row + 8, tp_col, tbdr);
        for (int i = 0; i < tp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", trst);
    }

    /* ── Lyapunov Sensitivity overlay panel ───────────────────────────────── */
    if (lyapunov_mode) {
        int lp_w = 44;
        int lp_col = term_cols - lp_w - 2;
        /* Stack below entropy and/or temperature panels if active */
        int lp_row = 3;
        if (entropy_mode) lp_row += 8;   /* entropy panel is 8 rows */
        if (temp_mode)    lp_row += 9;   /* temp panel is 9 rows */
        if (lp_col < 1) lp_col = 1;

        const char *lbdr = "\033[38;2;220;200;60;48;2;16;14;4m";
        const char *lbg  = "\033[48;2;16;14;4m";
        const char *lrst  = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Lyapunov \xce\xbb ",
                     lp_row, lp_col, lbdr);
        for (int i = 14; i < lp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", lrst);

        /* Stats row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     lp_row + 1, lp_col, lbdr, lbg);
        p += sprintf(p, " \033[38;2;220;200;60m\xce\xbb\xcc\x84=\033[38;2;255;255;255m%.5f"
                     "  \033[38;2;220;200;60m\xce\xbb\xe2\x82\x98\xe2\x82\x90\xe2\x82\x93=\033[38;2;255;255;255m%.5f",
                     lyapunov_global, lyapunov_max_local);
        p += sprintf(p, "%s", lbg);
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", lbdr, lrst);

        /* Label row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     lp_row + 2, lp_col, lbdr, lbg);
        p += sprintf(p, " \033[38;2;40;50;120mStable");
        for (int i = 0; i < 18; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;120;100;40mSensitivity");
        for (int i = 0; i < 3; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;180;40;20mChaotic ");
        p += sprintf(p, "%s\xe2\x94\x82%s", lbdr, lrst);

        /* Distribution histogram: bin sensitivity into 10 buckets */
        int lbins[10] = {0};
        for (int ly = 0; ly < H; ly++)
            for (int lx = 0; lx < W; lx++) {
                float sv = lyapunov_grid[ly][lx] * 3.0f;
                if (sv > 1.0f) sv = 1.0f;
                int b = (int)(sv * 9.99f);
                if (b < 0) b = 0;
                if (b > 9) b = 9;
                lbins[b]++;
            }
        int lbmax = 1;
        for (int i = 0; i < 10; i++)
            if (lbins[i] > lbmax) lbmax = lbins[i];

        /* 4 rows of histogram bars */
        for (int row = 0; row < 4; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         lp_row + 3 + row, lp_col, lbdr, lbg);
            int level = 3 - row;
            for (int b = 0; b < 10; b++) {
                int bar_h = lbins[b] * 4 / lbmax;
                for (int c = 0; c < 4; c++) {
                    if (bar_h > level) {
                        RGB bc = lyapunov_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88",
                                     bc.r, bc.g, bc.b);
                    } else if (bar_h == level) {
                        RGB bc = lyapunov_to_rgb((float)b / 9.0f);
                        p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84",
                                     bc.r/2, bc.g/2, bc.b/2);
                    } else {
                        p += sprintf(p, "\033[38;2;20;18;8m\xc2\xb7");
                    }
                }
            }
            *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", lbdr, lrst);
        }

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", lp_row + 7, lp_col, lbdr);
        for (int i = 0; i < lp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", lrst);
    }

    /* ── Fourier Spectrum Analyzer overlay panel ─────────────────────────── */
    if (fourier_mode) {
        int fp_w = 44;
        int fp_col = term_cols - fp_w - 2;
        /* Stack below other active panels */
        int fp_row = 3;
        if (entropy_mode) fp_row += 8;
        if (temp_mode)    fp_row += 9;
        if (lyapunov_mode) fp_row += 8;
        if (fp_col < 1) fp_col = 1;

        const char *fbdr = "\033[38;2;80;200;255;48;2;4;8;24m";
        const char *fbg  = "\033[48;2;4;8;24m";
        const char *frst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Fourier \xe2\x89\x88 ",
                     fp_row, fp_col, fbdr);
        for (int i = 13; i < fp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", frst);

        /* Row 1: Spectral entropy + dominant wavelength */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fp_row + 1, fp_col, fbdr, fbg);
        p += sprintf(p, " \033[38;2;80;200;255mH\xe2\x82\x9b=\033[38;2;255;255;255m%.3f"
                     "  \033[38;2;80;200;255m\xce\xbb=\033[38;2;255;255;255m%.1f cells",
                     fourier_spectral_entropy, fourier_dominant_wl);
        /* Pad to width */
        int used1 = 7 + 5 + 2 + 4 + 6; /* approximate visible chars */
        for (int i = used1; i < fp_w - 1; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);

        /* Row 2: Labels for radial profile */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fp_row + 2, fp_col, fbdr, fbg);
        p += sprintf(p, " \033[38;2;40;80;140mDC");
        for (int i = 0; i < 20; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;60;140;180mRadial Power");
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;40;80;140mNyq ");
        p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);

        /* Rows 3-6: Radial power profile as 4-row histogram (40 bins displayed) */
        int rbins[40];
        memset(rbins, 0, sizeof(rbins));
        float rmax = 0.001f;
        for (int i = 1; i < FFT_RADIAL_BINS; i++) {
            int b = (i - 1) * 40 / (FFT_RADIAL_BINS - 1);
            if (b >= 40) b = 39;
            rbins[b] += (int)(fourier_radial[i] * 1000.0f);
        }
        for (int i = 0; i < 40; i++)
            if (rbins[i] > rmax) rmax = (float)rbins[i];

        for (int row = 0; row < 4; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         fp_row + 3 + row, fp_col, fbdr, fbg);
            int level = 3 - row;
            for (int b = 0; b < 40; b++) {
                int bar_h = (int)((float)rbins[b] * 4.0f / rmax);
                /* Color based on frequency (low=blue, high=cyan/green) */
                float freq_t = (float)b / 39.0f;
                RGB bc = fourier_to_rgb(0.3f + freq_t * 0.5f);
                if (bar_h > level) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88",
                                 bc.r, bc.g, bc.b);
                } else if (bar_h == level) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84",
                                 bc.r/2, bc.g/2, bc.b/2);
                } else {
                    p += sprintf(p, "\033[38;2;12;16;40m\xc2\xb7");
                }
            }
            *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);
        }

        /* Row 7: Power spectrum mini-view label */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fp_row + 7, fp_col, fbdr, fbg);
        p += sprintf(p, " \033[38;2;60;140;180m2D Spectrum (log)");
        for (int i = 18; i < fp_w - 1; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);

        /* Rows 8-15: 8-row × 40-col mini 2D power spectrum using block chars */
        /* Map the center region of the spectrum to a small display */
        for (int row = 0; row < 8; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         fp_row + 8 + row, fp_col, fbdr, fbg);
            for (int col = 0; col < 40; col++) {
                /* Map mini display (col,row) to grid coordinates */
                /* Show center portion of spectrum */
                int gy_top = H/2 - 8 + row * 2;
                int gy_bot = gy_top + 1;
                int gx = W/2 - 20 + col;
                float v_top = 0.0f, v_bot = 0.0f;
                if (gy_top >= 0 && gy_top < H && gx >= 0 && gx < W)
                    v_top = fourier_grid[gy_top][gx];
                if (gy_bot >= 0 && gy_bot < H && gx >= 0 && gx < W)
                    v_bot = fourier_grid[gy_bot][gx];
                /* Use half-block: top pixel = fg, bottom pixel = bg */
                RGB ct = fourier_to_rgb(v_top);
                RGB cb = fourier_to_rgb(v_bot);
                p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x80",
                             ct.r, ct.g, ct.b, cb.r, cb.g, cb.b);
            }
            p += sprintf(p, "%s ", fbg);
            p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);
        }

        /* Row 16: Spectral entropy bar */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fp_row + 16, fp_col, fbdr, fbg);
        p += sprintf(p, " \033[38;2;80;200;255mEntropy: ");
        /* Render entropy bar (30 chars wide) */
        int ent_fill = (int)(fourier_spectral_entropy * 30.0f);
        if (ent_fill > 30) ent_fill = 30;
        for (int i = 0; i < 30; i++) {
            if (i < ent_fill) {
                float t = (float)i / 29.0f;
                RGB ec = fourier_to_rgb(0.2f + t * 0.6f);
                p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88", ec.r, ec.g, ec.b);
            } else {
                p += sprintf(p, "\033[38;2;12;16;40m\xe2\x96\x91");
            }
        }
        p += sprintf(p, "  ");
        p += sprintf(p, "%s\xe2\x94\x82%s", fbdr, frst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", fp_row + 17, fp_col, fbdr);
        for (int i = 0; i < fp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", frst);
    }

    /* ── Box-Counting Fractal Dimension overlay panel ────────────────────── */
    if (fractal_mode) {
        int fd_w = 44;
        int fd_col = term_cols - fd_w - 2;
        /* Stack below other active panels */
        int fd_row = 3;
        if (entropy_mode) fd_row += 8;
        if (temp_mode)    fd_row += 9;
        if (lyapunov_mode) fd_row += 8;
        if (fourier_mode) fd_row += 18;
        if (fd_col < 1) fd_col = 1;

        const char *fdbdr = "\033[38;2;230;200;40;48;2;8;8;4m";
        const char *fdbg  = "\033[48;2;8;8;4m";
        const char *fdrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Fractal D\xe2\x82\x93 ",
                     fd_row, fd_col, fdbdr);
        for (int i = 14; i < fd_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", fdrst);

        /* Row 1: D_box value + R² + alive count */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fd_row + 1, fd_col, fdbdr, fdbg);
        p += sprintf(p, " \033[38;2;230;200;40mD\xe2\x82\x93=\033[38;2;255;255;200m%.4f"
                     "  \033[38;2;230;200;40mR\xc2\xb2=\033[38;2;255;255;200m%.3f"
                     "  \033[38;2;120;120;80mn=\033[38;2;180;180;140m%d",
                     fractal_dbox, fractal_r2, fractal_total_alive);
        /* Pad */
        {
            /* Approximate chars used: D₀=X.XXXX  R²=X.XXX  n=XXXXX = ~35 */
            int used = 35;
            for (int i = used; i < fd_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);

        /* Row 2: Interpretation */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fd_row + 2, fd_col, fdbdr, fdbg);
        const char *interp;
        if (fractal_dbox < 0.5f)       interp = "sparse / scattered";
        else if (fractal_dbox < 1.0f)  interp = "line-like structures";
        else if (fractal_dbox < 1.3f)  interp = "thin fractal / filaments";
        else if (fractal_dbox < 1.6f)  interp = "moderate fractal complexity";
        else if (fractal_dbox < 1.8f)  interp = "rich fractal structure";
        else                           interp = "space-filling / dense";
        p += sprintf(p, " \033[38;2;180;160;60m%s", interp);
        {
            int slen = (int)strlen(interp);
            for (int i = slen + 1; i < fd_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);

        /* Row 3: Log-log plot header */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fd_row + 3, fd_col, fdbdr, fdbg);
        p += sprintf(p, " \033[38;2;140;130;50mlog(\xce\xb5)");
        for (int i = 0; i < 10; i++) *p++ = ' ';
        p += sprintf(p, "\033[38;2;140;130;50mlog(N) vs log(\xce\xb5)");
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);

        /* Rows 4-7: Log-log scatter plot (4 rows high, 40 cols wide) */
        /* Find range of log values for plotting */
        float lx_min = 1e9f, lx_max = -1e9f, ly_min = 1e9f, ly_max = -1e9f;
        for (int s = 0; s < FRACTAL_N_SCALES; s++) {
            if (fractal_n_boxes[s] > 0) {
                if (fractal_log_eps[s] < lx_min) lx_min = fractal_log_eps[s];
                if (fractal_log_eps[s] > lx_max) lx_max = fractal_log_eps[s];
                if (fractal_log_n[s] < ly_min) ly_min = fractal_log_n[s];
                if (fractal_log_n[s] > ly_max) ly_max = fractal_log_n[s];
            }
        }
        if (lx_max <= lx_min) { lx_min = 0; lx_max = 5; }
        if (ly_max <= ly_min) { ly_min = 0; ly_max = 12; }
        /* Add small margin */
        float lx_range = lx_max - lx_min;
        float ly_range = ly_max - ly_min;
        if (lx_range < 0.1f) lx_range = 0.1f;
        if (ly_range < 0.1f) ly_range = 0.1f;

        for (int row = 0; row < 4; row++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                         fd_row + 4 + row, fd_col, fdbdr, fdbg);
            /* Rasterize: 40 × 4 plot area */
            char plot_chars[40];
            memset(plot_chars, 0, sizeof(plot_chars));

            /* Plot regression line */
            for (int px = 0; px < 40; px++) {
                float lx = lx_min + (float)px / 39.0f * lx_range;
                /* predicted: log(N) = log(N_intercept) - D_box * log(eps) */
                /* Use regression: y = sum_y/n + slope * (x - sum_x/n) */
                float pred_ly = ly_max - fractal_dbox * (lx - lx_min);
                /* Actually use proper regression */
                int n_valid = 0;
                float sx = 0, sy2 = 0;
                for (int s = 0; s < FRACTAL_N_SCALES; s++)
                    if (fractal_n_boxes[s] > 0) { sx += fractal_log_eps[s]; sy2 += fractal_log_n[s]; n_valid++; }
                float mean_lx = n_valid > 0 ? sx / n_valid : 0;
                float mean_ly = n_valid > 0 ? sy2 / n_valid : 0;
                float pred = mean_ly + (-fractal_dbox) * (lx - mean_lx);
                int py = (int)((pred - ly_min) / ly_range * 3.0f);
                py = 3 - py; /* invert: row 0 is top */
                if (py == row && px >= 0 && px < 40)
                    plot_chars[px] = 1; /* regression line */
            }

            /* Plot data points */
            for (int s = 0; s < FRACTAL_N_SCALES; s++) {
                if (fractal_n_boxes[s] > 0) {
                    int px = (int)((fractal_log_eps[s] - lx_min) / lx_range * 39.0f);
                    int py = (int)((fractal_log_n[s] - ly_min) / ly_range * 3.0f);
                    py = 3 - py;
                    if (px >= 0 && px < 40 && py == row)
                        plot_chars[px] = 2; /* data point */
                }
            }

            for (int px = 0; px < 40; px++) {
                if (plot_chars[px] == 2) {
                    /* Data point: bright gold */
                    p += sprintf(p, "\033[38;2;255;220;60m\xe2\x97\x8f");
                } else if (plot_chars[px] == 1) {
                    /* Regression line: dim teal */
                    p += sprintf(p, "\033[38;2;60;120;100m\xe2\x94\x80");
                } else {
                    /* Empty: dot grid */
                    p += sprintf(p, "\033[38;2;30;28;16m\xc2\xb7");
                }
            }
            *p++ = ' ';
            p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);
        }

        /* Row 8: Scale legend (ε values) */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fd_row + 8, fd_col, fdbdr, fdbg);
        p += sprintf(p, " \033[38;2;120;110;50m\xce\xb5: ");
        {
            static const int box_sizes[FRACTAL_N_SCALES] = {2, 4, 8, 16, 32, 64, 128};
            for (int s = 0; s < FRACTAL_N_SCALES; s++) {
                p += sprintf(p, "%d", box_sizes[s]);
                if (s < FRACTAL_N_SCALES - 1) *p++ = ' ';
            }
        }
        /* Pad remaining */
        {
            int used = 4 + 3 + 2 + 2 + 3 + 3 + 3 + 4 + 6; /* approximate "ε: 2 4 8 16 32 64 128" */
            for (int i = used; i < fd_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);

        /* Row 9: N(ε) values as horizontal bar chart summary */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fd_row + 9, fd_col, fdbdr, fdbg);
        p += sprintf(p, " \033[38;2;120;110;50mN: ");
        {
            for (int s = 0; s < FRACTAL_N_SCALES; s++) {
                int nb = fractal_n_boxes[s];
                if (nb > 9999)
                    p += sprintf(p, "\033[38;2;200;180;80m%dk", nb / 1000);
                else
                    p += sprintf(p, "\033[38;2;200;180;80m%d", nb);
                if (s < FRACTAL_N_SCALES - 1) *p++ = ' ';
            }
        }
        {
            int used = 38; /* approximate */
            for (int i = used; i < fd_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", fdbdr, fdrst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", fd_row + 10, fd_col, fdbdr);
        for (int i = 0; i < fd_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", fdrst);
    }

    /* ── Wolfram Class Detector overlay panel ──────────────────────────────── */
    if (wolfram_mode) {
        int wf_w = 44;
        int wf_col = term_cols - wf_w - 2;
        /* Stack below other active panels */
        int wf_row = 3;
        if (entropy_mode) wf_row += 8;
        if (temp_mode)    wf_row += 9;
        if (lyapunov_mode) wf_row += 8;
        if (fourier_mode) wf_row += 18;
        if (fractal_mode) wf_row += 11;
        if (wf_col < 1) wf_col = 1;

        /* Color scheme based on detected class */
        static const char *wf_bdrs[] = {
            "\033[38;2;100;100;100;48;2;10;10;10m",   /* unknown: gray */
            "\033[38;2;80;80;160;48;2;6;6;16m",        /* I: slate blue */
            "\033[38;2;60;200;100;48;2;4;16;8m",       /* II: green */
            "\033[38;2;255;80;60;48;2;16;6;4m",        /* III: red */
            "\033[38;2;255;200;60;48;2;16;14;4m",      /* IV: gold */
        };
        static const char *wf_bgs[] = {
            "\033[48;2;10;10;10m",
            "\033[48;2;6;6;16m",
            "\033[48;2;4;16;8m",
            "\033[48;2;16;6;4m",
            "\033[48;2;16;14;4m",
        };
        static const char *class_names[] = {"???", "I: Fixed", "II: Periodic", "III: Chaotic", "IV: Complex"};
        static const char *class_desc[] = {
            "Insufficient data",
            "Converges to uniform/empty state",
            "Stable oscillators & still lifes",
            "Pseudo-random, sensitive to IC",
            "Edge of chaos: emergent structures"
        };

        int wc = (wolfram_class >= 1 && wolfram_class <= 4) ? wolfram_class : 0;
        const char *wbdr = wf_bdrs[wc];
        const char *wbg  = wf_bgs[wc];
        const char *wrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Wolfram Class ",
                     wf_row, wf_col, wbdr);
        for (int i = 16; i < wf_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", wrst);

        /* Row 1: Classification result */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 1, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[1;38;2;255;255;255m%-22s \033[0;38;2;180;180;140m%.0f%% conf",
                     class_names[wc], wolfram_confidence * 100.0f);
        p += sprintf(p, "%s", wbg);
        for (int i = 0; i < 4; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 2: Description */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 2, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[38;2;140;140;120m%-41s", class_desc[wc]);
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 3: Separator */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82", wf_row + 3, wf_col, wbdr);
        p += sprintf(p, "%s", wbg);
        for (int i = 0; i < wf_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 4: Feature vector header */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 4, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[38;2;120;120;100mFeatures:                              ");
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 5: Entropy + Lyapunov */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 5, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[38;2;180;255;180mH\xcc\x84=\033[38;2;255;255;255m%.4f"
                     "  \033[38;2;220;200;60m\xce\xbb=\033[38;2;255;255;255m%.4f"
                     "  \033[38;2;230;200;40mD\xe2\x82\x93=\033[38;2;255;255;255m%.3f",
                     wf_entropy, wf_lyapunov, wf_fractal);
        p += sprintf(p, "%s", wbg);
        for (int i = 0; i < 2; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 6: Population dynamics */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 6, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[38;2;120;180;255m\xcf\x83\xc2\xb2=\033[38;2;255;255;255m%.5f"
                     "  \033[38;2;120;180;255mACF=\033[38;2;255;255;255m%.3f"
                     "  \033[38;2;120;180;255m\xcf\x81=\033[38;2;255;255;255m%.4f",
                     wf_pop_var, wf_pop_acf, wf_density);
        p += sprintf(p, "%s", wbg);
        for (int i = 0; i < 2; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 7: Separator */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82", wf_row + 7, wf_col, wbdr);
        p += sprintf(p, "%s", wbg);
        for (int i = 0; i < wf_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Row 8: Score bars header */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     wf_row + 8, wf_col, wbdr, wbg);
        p += sprintf(p, " \033[38;2;120;120;100mClass Scores:                          ");
        p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);

        /* Rows 9-12: Score bars for each class */
        {
            static const char *bar_labels[] = {"  I Fixed  ", " II Peri.  ", "III Chaos  ", " IV Cmplx  "};
            static const int bar_colors[][3] = {
                {80, 80, 160},   /* slate blue */
                {60, 200, 100},  /* green */
                {255, 80, 60},   /* red */
                {255, 200, 60},  /* gold */
            };
            float max_score = 0.01f;
            for (int i = 0; i < 4; i++)
                if (wolfram_scores[i] > max_score) max_score = wolfram_scores[i];

            for (int c = 0; c < 4; c++) {
                p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                             wf_row + 9 + c, wf_col, wbdr, wbg);
                /* Label */
                int is_winner = (c + 1 == wolfram_class);
                if (is_winner)
                    p += sprintf(p, "\033[1;38;2;255;255;255m");
                else
                    p += sprintf(p, "\033[38;2;120;120;100m");
                p += sprintf(p, "%s", bar_labels[c]);

                /* Bar: 28 chars wide */
                int bar_len = (int)(wolfram_scores[c] / max_score * 28.0f);
                if (bar_len > 28) bar_len = 28;
                for (int i = 0; i < 28; i++) {
                    if (i < bar_len) {
                        int r = bar_colors[c][0], g = bar_colors[c][1], b = bar_colors[c][2];
                        if (is_winner) {
                            p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88", r, g, b);
                        } else {
                            p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x93", r/2, g/2, b/2);
                        }
                    } else {
                        p += sprintf(p, "\033[38;2;30;30;25m\xc2\xb7");
                    }
                }
                /* Score value */
                p += sprintf(p, "\033[38;2;140;140;120m%3.1f", wolfram_scores[c]);
                p += sprintf(p, "%s\xe2\x94\x82%s", wbdr, wrst);
            }
        }

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", wf_row + 13, wf_col, wbdr);
        for (int i = 0; i < wf_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", wrst);
    }

    /* ── Information Flow Field overlay panel ─────────────────────────────── */
    if (flow_mode) {
        int fl_w = 44;
        int fl_col = term_cols - fl_w - 2;
        /* Stack below other active panels */
        int fl_row = 3;
        if (entropy_mode) fl_row += 8;
        if (temp_mode)    fl_row += 9;
        if (lyapunov_mode) fl_row += 8;
        if (fourier_mode) fl_row += 18;
        if (fractal_mode) fl_row += 11;
        if (wolfram_mode) fl_row += 14;
        if (fl_col < 1) fl_col = 1;

        const char *flbdr = "\033[38;2;60;220;200;48;2;4;16;14m";
        const char *flbg  = "\033[48;2;4;16;14m";
        const char *flrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Info Flow \xe2\x87\x89 ",
                     fl_row, fl_col, flbdr);
        for (int i = 14; i < fl_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", flrst);

        /* Row 1: Global mean flow magnitude */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 1, fl_col, flbdr, flbg);
        p += sprintf(p, " \033[38;2;60;220;200mMean mag: \033[1;38;2;120;255;240m%.6f",
                     flow_global_mag);
        {
            int used = 26;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Row 2: Max magnitude + vorticity */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 2, fl_col, flbdr, flbg);
        p += sprintf(p, " \033[38;2;60;220;200mMax: \033[38;2;255;220;80m%.5f"
                     " \033[38;2;60;220;200mCurl: \033[38;2;200;120;255m%.5f",
                     flow_max_mag, flow_vorticity);
        {
            int used = 36;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Row 3: Sources and sinks */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 3, fl_col, flbdr, flbg);
        p += sprintf(p, " \033[38;2;255;160;60mSources:%d"
                     " \033[38;2;60;160;255mSinks:%d"
                     " \033[38;2;100;100;80mWindow:%d",
                     flow_n_sources, flow_n_sinks,
                     tl_len < FLOW_WINDOW ? tl_len : FLOW_WINDOW);
        {
            int used = 34;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Row 4: Direction histogram label */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 4, fl_col, flbdr, flbg);
        p += sprintf(p, " \033[38;2;60;220;200mDirection: ");
        {
            /* Compact direction rose: show 8 directions with bar heights */
            static const int dir_colors[][3] = {
                {255,80,80}, {255,180,60}, {255,255,60}, {80,255,80},
                {60,200,255}, {80,80,255}, {180,60,255}, {255,60,180}
            };
            for (int d = 0; d < 8; d++) {
                int h = flow_dir_hist[d] * 4 / flow_dir_hist_max;
                if (h > 4) h = 4;
                p += sprintf(p, "\033[38;2;%d;%d;%dm",
                             dir_colors[d][0], dir_colors[d][1], dir_colors[d][2]);
                if (h >= 3) p += sprintf(p, "\xe2\x96\x88"); /* █ */
                else if (h >= 2) p += sprintf(p, "\xe2\x96\x93"); /* ▓ */
                else if (h >= 1) p += sprintf(p, "\xe2\x96\x91"); /* ░ */
                else p += sprintf(p, "\xc2\xb7"); /* · */
            }
        }
        {
            int used = 30;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Row 5: Direction labels */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 5, fl_col, flbdr, flbg);
        p += sprintf(p, " \033[38;2;100;100;80m          E NEN NWW SWS SE");
        {
            int used = 37;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Row 6: Visual compass rose */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     fl_row + 6, fl_col, flbdr, flbg);
        {
            /* Show dominant direction as large arrow */
            int best_dir = 0;
            for (int d = 1; d < 8; d++)
                if (flow_dir_hist[d] > flow_dir_hist[best_dir]) best_dir = d;
            static const char *big_arrows[] = {
                "\xe2\x87\x92", "\xe2\x87\x97", "\xe2\x87\x91", "\xe2\x87\x96",
                "\xe2\x87\x90", "\xe2\x87\x99", "\xe2\x87\x93", "\xe2\x87\x98"
            };
            static const char *dir_names[] = {"East","NE","North","NW","West","SW","South","SE"};
            static const int dir_c[][3] = {
                {255,80,80}, {255,180,60}, {255,255,60}, {80,255,80},
                {60,200,255}, {80,80,255}, {180,60,255}, {255,60,180}
            };
            p += sprintf(p, " \033[1;38;2;%d;%d;%dm Dominant: %s %s ",
                         dir_c[best_dir][0], dir_c[best_dir][1], dir_c[best_dir][2],
                         big_arrows[best_dir], dir_names[best_dir]);
        }
        {
            int used = 30;
            for (int i = used; i < fl_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", flbdr, flrst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", fl_row + 7, fl_col, flbdr);
        for (int i = 0; i < fl_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", flrst);
    }

    /* ── Phase-Space Attractor overlay panel ────────────────────────────────── */
    if (attractor_mode) {
        int at_w = 44;
        int at_col = term_cols - at_w - 2;
        /* Stack below other active panels */
        int at_row = 3;
        if (entropy_mode)  at_row += 8;
        if (temp_mode)     at_row += 9;
        if (lyapunov_mode) at_row += 8;
        if (fourier_mode)  at_row += 18;
        if (fractal_mode)  at_row += 11;
        if (wolfram_mode)  at_row += 14;
        if (flow_mode)     at_row += 9;
        if (at_col < 1) at_col = 1;

        const char *abdr = "\033[38;2;100;180;255;48;2;6;10;20m";
        const char *abg  = "\033[48;2;6;10;20m";
        const char *arst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Attractor \xcf\x86 ",
                     at_row, at_col, abdr);
        for (int i = 15; i < at_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", arst);

        /* Row 1: Embedding parameters */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     at_row + 1, at_col, abdr, abg);
        p += sprintf(p, " \033[38;2;100;180;255mEmbed: \033[1;38;2;160;220;255md=%d  "
                     "\xcf\x84=%d  \033[0;38;2;100;180;255mpts=%d",
                     attr_embed_dim, attr_delay, attr_traj_len);
        {
            int used = 28 + (attr_delay >= 10 ? 1 : 0) + (attr_traj_len >= 100 ? 1 : 0)
                     + (attr_traj_len >= 10 ? 1 : 0);
            for (int i = used; i < at_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", abdr, arst);

        /* Row 2: Correlation dimension */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     at_row + 2, at_col, abdr, abg);
        {
            /* Classify the attractor from D₂ */
            const char *atype;
            int tr, tg, tb;
            if (attr_corr_dim < 0.15f) {
                atype = "Fixed point"; tr = 80; tg = 200; tb = 80;
            } else if (attr_corr_dim < 0.6f) {
                atype = "Limit cycle"; tr = 80; tg = 220; tb = 255;
            } else if (attr_corr_dim < 1.3f) {
                atype = "Torus/Quasi"; tr = 255; tg = 220; tb = 80;
            } else {
                atype = "Strange"; tr = 255; tg = 80; tb = 100;
            }
            p += sprintf(p, " \033[38;2;100;180;255mD\xe2\x82\x82: "
                         "\033[1;38;2;%d;%d;%dm%.3f \033[0;38;2;%d;%d;%dm%s",
                         tr, tg, tb, attr_corr_dim, tr, tg, tb, atype);
        }
        {
            int used = 30;
            for (int i = used; i < at_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", abdr, arst);

        /* Row 3: Population range */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     at_row + 3, at_col, abdr, abg);
        p += sprintf(p, " \033[38;2;100;180;255mPop range: "
                     "\033[38;2;180;180;160m%.0f \xe2\x80\x93 %.0f",
                     attr_pop_min, attr_pop_max);
        {
            int used = 28 + (attr_pop_max >= 1000 ? 1 : 0) + (attr_pop_max >= 10000 ? 1 : 0);
            for (int i = used; i < at_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", abdr, arst);

        /* Row 4: Canvas density (fraction of bins with hits) */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     at_row + 4, at_col, abdr, abg);
        {
            int filled = 0;
            for (int by = 0; by < ATTR_BINS; by++)
                for (int bx = 0; bx < ATTR_BINS; bx++)
                    if (attr_canvas[by][bx] > 0) filled++;
            float fill_pct = 100.0f * filled / (ATTR_BINS * ATTR_BINS);
            p += sprintf(p, " \033[38;2;100;180;255mCanvas fill: "
                         "\033[38;2;200;200;180m%.1f%% (%d/%d)",
                         fill_pct, filled, ATTR_BINS * ATTR_BINS);
        }
        {
            int used = 34;
            for (int i = used; i < at_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", abdr, arst);

        /* Row 5: Axes label */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     at_row + 5, at_col, abdr, abg);
        p += sprintf(p, " \033[38;2;80;120;160mX: pop(t)  Y: pop(t+\xcf\x84)");
        {
            int used = 28;
            for (int i = used; i < at_w - 1; i++) *p++ = ' ';
        }
        p += sprintf(p, "%s\xe2\x94\x82%s", abdr, arst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", at_row + 6, at_col, abdr);
        for (int i = 0; i < at_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", arst);
    }

    /* ── Causal Light Cone overlay panel ──────────────────────────────────── */
    if (cone_mode >= 1) {
        int cn_w = 44;
        int cn_col = term_cols - cn_w - 2;
        /* Stack below other active panels */
        int cn_row = 3;
        if (entropy_mode)   cn_row += 8;
        if (temp_mode)      cn_row += 9;
        if (lyapunov_mode)  cn_row += 8;
        if (fourier_mode)   cn_row += 18;
        if (fractal_mode)   cn_row += 11;
        if (wolfram_mode)   cn_row += 14;
        if (flow_mode)      cn_row += 9;
        if (attractor_mode) cn_row += 8;
        if (cn_col < 1) cn_col = 1;

        const char *cbdr = "\033[38;2;200;140;60;48;2;10;8;4m";
        const char *cbg  = "\033[48;2;10;8;4m";
        const char *crst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Light Cone \xe2\x97\x87 ",
                     cn_row, cn_col, cbdr);
        for (int i = 17; i < cn_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", crst);

        if (cone_mode == 1) {
            /* Waiting for click */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 1, cn_col, cbdr, cbg);
            p += sprintf(p, " \033[38;2;200;180;120mClick a cell to trace...");
            { int used = 27; for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Bottom border */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", cn_row + 2, cn_col, cbdr);
            for (int i = 0; i < cn_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
            *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
            p += sprintf(p, "%s", crst);
        } else {
            /* Row 1: Selected cell */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 1, cn_col, cbdr, cbg);
            p += sprintf(p, " \033[38;2;200;180;120mCell: \033[1;38;2;255;255;255m(%d,%d)",
                         cone_sel_x, cone_sel_y);
            { int used = 18 + (cone_sel_x >= 100 ? 1 : 0) + (cone_sel_x >= 10 ? 1 : 0)
                           + (cone_sel_y >= 100 ? 1 : 0) + (cone_sel_y >= 10 ? 1 : 0);
              for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Row 2: Backward cone stats */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 2, cn_col, cbdr, cbg);
            p += sprintf(p, " \033[38;2;120;180;255m\xe2\x97\x80 Back: "
                         "\033[38;2;180;220;255m%d cells  \033[38;2;120;180;255mdepth=%d",
                         cone_back_cells, cone_depth);
            { int used = 32 + (cone_back_cells >= 100 ? 1 : 0) + (cone_back_cells >= 1000 ? 1 : 0)
                           + (cone_back_cells >= 10000 ? 1 : 0)
                           + (cone_depth >= 10 ? 1 : 0) + (cone_depth >= 100 ? 1 : 0);
              for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Row 3: Forward cone stats */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 3, cn_col, cbdr, cbg);
            p += sprintf(p, " \033[38;2;255;140;40m\xe2\x97\xb6 Fwd:  "
                         "\033[38;2;255;180;100m%d cells  \033[38;2;255;140;40mdepth=%d",
                         cone_fwd_cells, cone_fwd_depth);
            { int used = 32 + (cone_fwd_cells >= 100 ? 1 : 0) + (cone_fwd_cells >= 1000 ? 1 : 0)
                           + (cone_fwd_cells >= 10000 ? 1 : 0)
                           + (cone_fwd_depth >= 10 ? 1 : 0) + (cone_fwd_depth >= 100 ? 1 : 0);
              for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Row 4: Fill ratio */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 4, cn_col, cbdr, cbg);
            {
                const char *fill_clr;
                if (cone_fill_ratio < 0.1f) fill_clr = "38;2;80;200;80";       /* sparse */
                else if (cone_fill_ratio < 0.4f) fill_clr = "38;2;200;200;80"; /* moderate */
                else fill_clr = "38;2;255;80;60";                               /* dense */
                p += sprintf(p, " \033[38;2;200;180;120mFill: "
                             "\033[%sm%.1f%%  \033[38;2;150;150;130malive=%d",
                             fill_clr, cone_fill_ratio * 100.0f, cone_back_alive);
            }
            { int used = 30 + (cone_back_alive >= 100 ? 1 : 0) + (cone_back_alive >= 1000 ? 1 : 0);
              for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Row 5: Hint */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         cn_row + 5, cn_col, cbdr, cbg);
            p += sprintf(p, " \033[38;2;120;110;90mClick to retarget  [+/-]depth");
            { int used = 32; for (int i = used; i < cn_w - 1; i++) *p++ = ' '; }
            p += sprintf(p, "%s\xe2\x94\x82%s", cbdr, crst);

            /* Bottom border */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", cn_row + 6, cn_col, cbdr);
            for (int i = 0; i < cn_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
            *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
            p += sprintf(p, "%s", crst);
        }
    }

    /* ── Prediction Surprise overlay panel ─────────────────────────────────── */
    if (surp_mode) {
        int sp_w = 44;
        int sp_col = term_cols - sp_w - 2;
        /* Stack below other active panels */
        int sp_row = 3;
        if (entropy_mode)   sp_row += 8;
        if (temp_mode)      sp_row += 9;
        if (lyapunov_mode)  sp_row += 8;
        if (fourier_mode)   sp_row += 18;
        if (fractal_mode)   sp_row += 11;
        if (wolfram_mode)   sp_row += 14;
        if (flow_mode)      sp_row += 9;
        if (attractor_mode) sp_row += 8;
        if (cone_mode >= 1) sp_row += 8;
        if (sp_col < 1) sp_col = 1;

        const char *sbdr = "\033[38;2;255;200;60;48;2;14;10;4m";
        const char *sbg  = "\033[48;2;14;10;4m";
        const char *srst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Surprise \xe2\x9a\xa1 ",
                     sp_row, sp_col, sbdr);
        for (int i = 15; i < sp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", srst);

        /* Row 1: Mean surprisal */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     sp_row + 1, sp_col, sbdr, sbg);
        p += sprintf(p, " \033[38;2;200;180;120mMean: \033[1;38;2;255;220;100m%.4f bits",
                     surp_global);
        { int used = 24; for (int i = used; i < sp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", sbdr, srst);

        /* Row 2: Max surprisal */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     sp_row + 2, sp_col, sbdr, sbg);
        p += sprintf(p, " \033[38;2;200;180;120mMax:  \033[38;2;255;100;60m%.4f bits",
                     surp_max_local);
        { int used = 24; for (int i = used; i < sp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", sbdr, srst);

        /* Row 3: Predictable vs surprising cell counts */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     sp_row + 3, sp_col, sbdr, sbg);
        p += sprintf(p, " \033[38;2;80;180;255mPredictable: \033[38;2;120;220;255m%d",
                     surp_predictable);
        { int used = 18 + (surp_predictable >= 10 ? 1 : 0) + (surp_predictable >= 100 ? 1 : 0)
                       + (surp_predictable >= 1000 ? 1 : 0) + (surp_predictable >= 10000 ? 1 : 0);
          for (int i = used; i < sp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", sbdr, srst);

        /* Row 4: Surprising cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     sp_row + 4, sp_col, sbdr, sbg);
        p += sprintf(p, " \033[38;2;255;140;40mSurprising:  \033[38;2;255;200;80m%d",
                     surp_surprising);
        { int used = 18 + (surp_surprising >= 10 ? 1 : 0) + (surp_surprising >= 100 ? 1 : 0)
                       + (surp_surprising >= 1000 ? 1 : 0) + (surp_surprising >= 10000 ? 1 : 0);
          for (int i = used; i < sp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", sbdr, srst);

        /* Row 5: History depth */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     sp_row + 5, sp_col, sbdr, sbg);
        p += sprintf(p, " \033[38;2;120;110;90mHistory: %d/%d frames  [!]toggle",
                     surp_hist_len, SURP_WINDOW);
        { int used = 38; for (int i = used; i < sp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", sbdr, srst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", sp_row + 6, sp_col, sbdr);
        for (int i = 0; i < sp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", srst);
    }

    /* ── Mutual Information Network overlay panel ────────────────────────────── */
    if (mi_mode) {
        int mi_w = 44;
        int mi_col = term_cols - mi_w - 2;
        /* Stack below other active panels */
        int mi_row = 3;
        if (entropy_mode)   mi_row += 8;
        if (temp_mode)      mi_row += 9;
        if (lyapunov_mode)  mi_row += 8;
        if (fourier_mode)   mi_row += 18;
        if (fractal_mode)   mi_row += 11;
        if (wolfram_mode)   mi_row += 14;
        if (flow_mode)      mi_row += 9;
        if (attractor_mode) mi_row += 8;
        if (cone_mode >= 1) mi_row += 8;
        if (surp_mode)      mi_row += 8;
        if (mi_col < 1) mi_col = 1;

        const char *mbdr = "\033[38;2;100;200;255;48;2;8;14;22m";
        const char *mbg  = "\033[48;2;8;14;22m";
        const char *mrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 MI Network \xe2\x97\x88 ",
                     mi_row, mi_col, mbdr);
        for (int i = 17; i < mi_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", mrst);

        /* Row 1: Max MI */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     mi_row + 1, mi_col, mbdr, mbg);
        p += sprintf(p, " \033[38;2;180;220;255mMax MI:  \033[1;38;2;100;200;255m%.4f bits",
                     mi_max_val);
        { int used = 26; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);

        /* Row 2: Mean MI */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     mi_row + 2, mi_col, mbdr, mbg);
        p += sprintf(p, " \033[38;2;180;220;255mMean MI: \033[38;2;140;190;240m%.4f bits",
                     mi_mean_val);
        { int used = 26; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);

        /* Row 3: Network density */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     mi_row + 3, mi_col, mbdr, mbg);
        p += sprintf(p, " \033[38;2;180;220;255mDensity: \033[38;2;140;255;180m%.3f",
                     mi_net_density);
        { int used = 18; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);

        /* Row 4: Clustering coefficient */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     mi_row + 4, mi_col, mbdr, mbg);
        p += sprintf(p, " \033[38;2;180;220;255mCluster: \033[38;2;255;200;100m%.3f",
                     mi_clustering);
        { int used = 18; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);

        /* Rows 5-9: Top 5 couplings */
        int show_n = mi_n_top < 5 ? mi_n_top : 5;
        for (int ci = 0; ci < 5; ci++) {
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         mi_row + 5 + ci, mi_col, mbdr, mbg);
            if (ci < show_n) {
                int ai = mi_top[ci].a;
                int bi = mi_top[ci].b;
                int ax = ai % MI_GW, ay = ai / MI_GW;
                int bx = bi % MI_GW, by = bi / MI_GW;
                /* Color coupling by rank: brightest for strongest */
                int bright = 255 - ci * 35;
                p += sprintf(p, " \033[38;2;%d;%d;%dm(%d,%d)\xe2\x86\x94(%d,%d) %.3f",
                             bright, bright - 30 > 0 ? bright - 30 : 0, bright/2,
                             ax, ay, bx, by, mi_top[ci].mi);
                { int used = 24; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
            } else {
                for (int i = 0; i < mi_w - 1; i++) *p++ = ' ';
            }
            p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);
        }

        /* Row 10: History depth + toggle key */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     mi_row + 10, mi_col, mbdr, mbg);
        p += sprintf(p, " \033[38;2;100;100;80mFrames: %d  Blocks: %dx%d  [@]toggle",
                     mi_n_frames_used, MI_GW, MI_GH);
        { int used = 39; for (int i = used; i < mi_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", mbdr, mrst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", mi_row + 11, mi_col, mbdr);
        for (int i = 0; i < mi_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", mrst);
    }

    /* ── Composite Complexity Index overlay panel ────────────────────────────── */
    if (cplx_mode) {
        int cx_w = 44;
        int cx_col = term_cols - cx_w - 2;
        /* Stack below other active panels */
        int cx_row = 3;
        if (entropy_mode)   cx_row += 8;
        if (temp_mode)      cx_row += 9;
        if (lyapunov_mode)  cx_row += 8;
        if (fourier_mode)   cx_row += 18;
        if (fractal_mode)   cx_row += 11;
        if (wolfram_mode)   cx_row += 14;
        if (flow_mode)      cx_row += 9;
        if (attractor_mode) cx_row += 8;
        if (cone_mode >= 1) cx_row += 8;
        if (surp_mode)      cx_row += 8;
        if (mi_mode)        cx_row += 12;
        if (cx_col < 1) cx_col = 1;

        const char *xbdr = "\033[38;2;235;210;30;48;2;16;12;4m";
        const char *xbg  = "\033[48;2;16;12;4m";
        const char *xrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Complexity \xe2\x9c\xa6 ",
                     cx_row, cx_col, xbdr);
        for (int i = 17; i < cx_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", xrst);

        /* Row 1: Mean complexity */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 1, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;200;190;120mMean: \033[1;38;2;235;210;30m%.4f",
                     cplx_global);
        { int used = 18; for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 2: Max complexity */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 2, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;200;190;120mMax:  \033[38;2;255;100;40m%.4f",
                     cplx_max_local);
        { int used = 18; for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 3: Simple cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 3, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;60;120;180mSimple (<0.2): \033[38;2;80;160;220m%d",
                     cplx_n_simple);
        { int used = 20 + (cplx_n_simple >= 10 ? 1 : 0) + (cplx_n_simple >= 100 ? 1 : 0)
                       + (cplx_n_simple >= 1000 ? 1 : 0) + (cplx_n_simple >= 10000 ? 1 : 0);
          for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 4: Edge-of-chaos cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 4, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;235;210;30mEdge (0.4-0.7): \033[1;38;2;255;230;60m%d",
                     cplx_n_complex);
        { int used = 22 + (cplx_n_complex >= 10 ? 1 : 0) + (cplx_n_complex >= 100 ? 1 : 0)
                       + (cplx_n_complex >= 1000 ? 1 : 0) + (cplx_n_complex >= 10000 ? 1 : 0);
          for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 5: Chaotic cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 5, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;255;80;40mChaotic (>0.8): \033[38;2;255;120;60m%d",
                     cplx_n_chaotic);
        { int used = 21 + (cplx_n_chaotic >= 10 ? 1 : 0) + (cplx_n_chaotic >= 100 ? 1 : 0)
                       + (cplx_n_chaotic >= 1000 ? 1 : 0) + (cplx_n_chaotic >= 10000 ? 1 : 0);
          for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 6: Edge fraction of alive cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 6, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;200;190;120mAlive@edge: \033[38;2;255;230;100m%.1f%%",
                     cplx_edge_frac * 100.0f);
        { int used = 22; for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 7: Color legend bar */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s ",
                     cx_row + 7, cx_col, xbdr, xbg);
        /* Mini gradient: 30 colored blocks */
        for (int gi = 0; gi < 30; gi++) {
            float gv = (float)gi / 29.0f;
            RGB gc = cplx_to_rgb(gv);
            p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x88", gc.r, gc.g, gc.b);
        }
        p += sprintf(p, "%s", xbg);
        { for (int i = 31; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 8: Legend labels */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 8, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;100;100;80mSimple  Periodic  Edge  Chaotic");
        { int used = 37; for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Row 9: Toggle hint */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     cx_row + 9, cx_col, xbdr, xbg);
        p += sprintf(p, " \033[38;2;100;100;80m[#]toggle  Fuses: entropy+lyap+surp+freq");
        { int used = 43; for (int i = used; i < cx_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", xbdr, xrst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", cx_row + 10, cx_col, xbdr);
        for (int i = 0; i < cx_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", xrst);
    }

    /* ── Topological Feature Map overlay panel ─────────────────────────────── */
    if (topo_mode) {
        int tp_w = 44;
        int tp_col = term_cols - tp_w - 2;
        /* Stack below other active panels */
        int tp_row = 3;
        if (entropy_mode)   tp_row += 8;
        if (temp_mode)      tp_row += 9;
        if (lyapunov_mode)  tp_row += 8;
        if (fourier_mode)   tp_row += 18;
        if (fractal_mode)   tp_row += 11;
        if (wolfram_mode)   tp_row += 14;
        if (flow_mode)      tp_row += 9;
        if (attractor_mode) tp_row += 8;
        if (cone_mode >= 1) tp_row += 8;
        if (surp_mode)      tp_row += 8;
        if (mi_mode)        tp_row += 12;
        if (cplx_mode)      tp_row += 11;
        if (tp_col < 1) tp_col = 1;

        const char *tbdr = "\033[38;2;180;100;240;48;2;12;6;20m";
        const char *tbg  = "\033[48;2;12;6;20m";
        const char *trst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Topology \xe2\x97\x86 ",
                     tp_row, tp_col, tbdr);
        for (int i = 16; i < tp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", trst);

        /* Row 1: β₀ (components) */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 1, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;180;160;220m\xce\xb2\xe2\x82\x80 Components: \033[1;38;2;220;180;255m%d",
                     topo_beta0);
        { int used = 22 + (topo_beta0 >= 10 ? 1 : 0) + (topo_beta0 >= 100 ? 1 : 0)
                       + (topo_beta0 >= 1000 ? 1 : 0);
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 2: β₁ (holes) */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 2, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;180;160;220m\xce\xb2\xe2\x82\x81 Holes:      \033[1;38;2;160;50;200m%d",
                     topo_beta1);
        { int used = 22 + (topo_beta1 >= 10 ? 1 : 0) + (topo_beta1 >= 100 ? 1 : 0)
                       + (topo_beta1 >= 1000 ? 1 : 0);
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 3: Largest component */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 3, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;140;130;180mLargest: \033[38;2;200;200;255m%d cells",
                     topo_largest_comp);
        { int used = 18 + (topo_largest_comp >= 10 ? 1 : 0) + (topo_largest_comp >= 100 ? 1 : 0)
                       + (topo_largest_comp >= 1000 ? 1 : 0) + (topo_largest_comp >= 10000 ? 1 : 0);
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 4: Mean component size */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 4, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;140;130;180mMean sz: \033[38;2;200;200;255m%.1f",
                     topo_mean_comp_size);
        { int used = 16; for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 5: Hole cells */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 5, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;140;130;180mHole cells: \033[38;2;160;50;200m%d",
                     topo_n_holes_cells);
        { int used = 17 + (topo_n_holes_cells >= 10 ? 1 : 0) + (topo_n_holes_cells >= 100 ? 1 : 0)
                       + (topo_n_holes_cells >= 1000 ? 1 : 0) + (topo_n_holes_cells >= 10000 ? 1 : 0);
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 6: β₀ sparkline */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 6, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;100;100;160m\xce\xb2\xe2\x82\x80 ");
        if (topo_hist_count > 1) {
            int n = topo_hist_count < 30 ? topo_hist_count : 30;
            int mx = 1;
            for (int i = 0; i < n; i++) {
                int idx = (topo_hist_idx - n + i + TOPO_HIST_LEN) % TOPO_HIST_LEN;
                if (topo_hist_b0[idx] > mx) mx = topo_hist_b0[idx];
            }
            const char *spark_chars[] = {"\xe2\x96\x81","\xe2\x96\x82","\xe2\x96\x83",
                                         "\xe2\x96\x84","\xe2\x96\x85","\xe2\x96\x86",
                                         "\xe2\x96\x87","\xe2\x96\x88"};
            for (int i = 0; i < n; i++) {
                int idx = (topo_hist_idx - n + i + TOPO_HIST_LEN) % TOPO_HIST_LEN;
                int lvl = mx > 0 ? (topo_hist_b0[idx] * 7) / mx : 0;
                if (lvl > 7) lvl = 7;
                p += sprintf(p, "\033[38;2;180;140;255m%s", spark_chars[lvl]);
            }
        }
        p += sprintf(p, "%s", tbg);
        { int used = 5 + (topo_hist_count < 30 ? topo_hist_count : 30) * 1;
          /* Each sparkline char is 3 bytes but 1 column */
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 7: β₁ sparkline */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 7, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;100;100;160m\xce\xb2\xe2\x82\x81 ");
        if (topo_hist_count > 1) {
            int n = topo_hist_count < 30 ? topo_hist_count : 30;
            int mx = 1;
            for (int i = 0; i < n; i++) {
                int idx = (topo_hist_idx - n + i + TOPO_HIST_LEN) % TOPO_HIST_LEN;
                if (topo_hist_b1[idx] > mx) mx = topo_hist_b1[idx];
            }
            const char *spark_chars[] = {"\xe2\x96\x81","\xe2\x96\x82","\xe2\x96\x83",
                                         "\xe2\x96\x84","\xe2\x96\x85","\xe2\x96\x86",
                                         "\xe2\x96\x87","\xe2\x96\x88"};
            for (int i = 0; i < n; i++) {
                int idx = (topo_hist_idx - n + i + TOPO_HIST_LEN) % TOPO_HIST_LEN;
                int lvl = mx > 0 ? (topo_hist_b1[idx] * 7) / mx : 0;
                if (lvl > 7) lvl = 7;
                p += sprintf(p, "\033[38;2;160;50;200m%s", spark_chars[lvl]);
            }
        }
        p += sprintf(p, "%s", tbg);
        { int used = 5 + (topo_hist_count < 30 ? topo_hist_count : 30) * 1;
          for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Row 8: Toggle hint */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     tp_row + 8, tp_col, tbdr, tbg);
        p += sprintf(p, " \033[38;2;100;100;80m[$]toggle  4-connected flood fill");
        { int used = 37; for (int i = used; i < tp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", tbdr, trst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", tp_row + 9, tp_col, tbdr);
        for (int i = 0; i < tp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", trst);
    }

    /* ── Renormalization Group Flow overlay panel ────────────────────────────── */
    if (rg_mode) {
        int rp_w = 44;
        int rp_col = term_cols - rp_w - 2;
        /* Stack below other active panels */
        int rp_row = 3;
        if (entropy_mode)   rp_row += 8;
        if (temp_mode)      rp_row += 9;
        if (lyapunov_mode)  rp_row += 8;
        if (fourier_mode)   rp_row += 18;
        if (fractal_mode)   rp_row += 11;
        if (wolfram_mode)   rp_row += 14;
        if (flow_mode)      rp_row += 9;
        if (attractor_mode) rp_row += 8;
        if (cone_mode >= 1) rp_row += 8;
        if (surp_mode)      rp_row += 8;
        if (mi_mode)        rp_row += 12;
        if (cplx_mode)      rp_row += 11;
        if (topo_mode)      rp_row += 11;
        if (rp_col < 1) rp_col = 1;

        const char *rbdr = "\033[38;2;100;220;200;48;2;8;18;16m";
        const char *rbg  = "\033[48;2;8;18;16m";
        const char *rrst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 RG Flow \xe2\x97\x86 ",
                     rp_row, rp_col, rbdr);
        for (int i = 15; i < rp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", rrst);

        /* Row 1: Criticality score */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 1, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;100;220;200mCriticality: \033[1;38;2;");
        /* Color criticality: low=red, mid=yellow, high=cyan */
        if (rg_criticality < 0.4f) p += sprintf(p, "220;80;80m");
        else if (rg_criticality < 0.7f) p += sprintf(p, "220;200;80m");
        else p += sprintf(p, "80;255;220m");
        p += sprintf(p, "%.3f", rg_criticality);
        { int used = 21; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 2: Scale density bars */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 2, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;80;200;220m2x:%.2f \033[38;2;220;200;60m4x:%.2f \033[38;2;200;80;220m8x:%.2f",
                     rg_density[0], rg_density[1], rg_density[2]);
        { int used = 29; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 3: Scale spectrum bar (visual) */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 3, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;100;180;160mSpectrum: ");
        {
            int bar_w = rp_w - 14;
            const char *block = "\xe2\x96\x88"; /* █ */
            for (int i = 0; i < bar_w; i++) {
                float t = (float)i / (bar_w - 1);
                int si;
                if (t < 0.33f) si = 0;
                else if (t < 0.67f) si = 1;
                else si = 2;
                int lvl = (int)(rg_density[si] * 8);
                if (lvl > 8) lvl = 8;
                if (lvl > 0) {
                    if (si == 0) p += sprintf(p, "\033[38;2;80;200;220m");
                    else if (si == 1) p += sprintf(p, "\033[38;2;220;200;60m");
                    else p += sprintf(p, "\033[38;2;200;80;220m");
                    p += sprintf(p, "%s", block);
                } else {
                    p += sprintf(p, "\033[38;2;30;40;35m\xe2\x96\x91");
                }
            }
        }
        p += sprintf(p, " ");
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 4: Classification fractions */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 4, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;80;200;220mFine:%.0f%% \033[38;2;220;200;60mMeso:%.0f%%",
                     rg_global_fine * 100, rg_global_meso * 100);
        { int used = 21; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 5: Coarse and scale-invariant fractions */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 5, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;200;80;220mCoarse:%.0f%% \033[38;2;240;240;220mInvariant:%.0f%%",
                     rg_global_coarse * 100, rg_global_invariant * 100);
        { int used = 28; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 6: Mean invariance */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 6, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;140;180;170mMean invariance: \033[38;2;200;240;230m%.3f",
                     rg_mean_invariance);
        { int used = 25; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 7: Criticality sparkline */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 7, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;80;140;130mC ");
        if (rg_hist_count > 1) {
            int n = rg_hist_count < 30 ? rg_hist_count : 30;
            const char *spark_chars[] = {"\xe2\x96\x81","\xe2\x96\x82","\xe2\x96\x83",
                                         "\xe2\x96\x84","\xe2\x96\x85","\xe2\x96\x86",
                                         "\xe2\x96\x87","\xe2\x96\x88"};
            for (int i = 0; i < n; i++) {
                int idx = (rg_hist_idx - n + i + RG_HIST_LEN) % RG_HIST_LEN;
                int lvl = (int)(rg_hist_crit[idx] * 7.0f);
                if (lvl < 0) lvl = 0;
                if (lvl > 7) lvl = 7;
                /* Color: red→yellow→cyan */
                if (rg_hist_crit[idx] < 0.4f)
                    p += sprintf(p, "\033[38;2;220;80;80m%s", spark_chars[lvl]);
                else if (rg_hist_crit[idx] < 0.7f)
                    p += sprintf(p, "\033[38;2;220;200;80m%s", spark_chars[lvl]);
                else
                    p += sprintf(p, "\033[38;2;80;255;220m%s", spark_chars[lvl]);
            }
        }
        p += sprintf(p, "%s", rbg);
        { int used = 4 + (rg_hist_count < 30 ? rg_hist_count : 30);
          for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Row 8: Toggle hint */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     rp_row + 8, rp_col, rbdr, rbg);
        p += sprintf(p, " \033[38;2;80;100;90m[%%]toggle  majority-rule RG");
        { int used = 33; for (int i = used; i < rp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", rbdr, rrst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", rp_row + 9, rp_col, rbdr);
        for (int i = 0; i < rp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", rrst);
    }

    /* ── Kolmogorov Complexity Estimator overlay panel ────────────────────── */
    if (kc_mode) {
        int kp_w = 44;
        int kp_col = term_cols - kp_w - 2;
        /* Stack below other active panels */
        int kp_row = 3;
        if (entropy_mode)   kp_row += 8;
        if (temp_mode)      kp_row += 9;
        if (lyapunov_mode)  kp_row += 8;
        if (fourier_mode)   kp_row += 18;
        if (fractal_mode)   kp_row += 11;
        if (wolfram_mode)   kp_row += 14;
        if (flow_mode)      kp_row += 9;
        if (attractor_mode) kp_row += 8;
        if (cone_mode >= 1) kp_row += 8;
        if (surp_mode)      kp_row += 8;
        if (mi_mode)        kp_row += 12;
        if (cplx_mode)      kp_row += 11;
        if (topo_mode)      kp_row += 11;
        if (rg_mode)        kp_row += 11;
        if (kp_col < 1) kp_col = 1;

        const char *kbdr = "\033[38;2;240;180;40;48;2;12;10;4m";
        const char *kbg  = "\033[48;2;12;10;4m";
        const char *krst = "\033[0m";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 K-Complexity \xe2\x97\x86 ",
                     kp_row, kp_col, kbdr);
        for (int i = 19; i < kp_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", krst);

        /* Row 1: Mean complexity */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 1, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;240;180;40mMean K: \033[1;38;2;");
        if (kc_global_mean < 0.3f) p += sprintf(p, "60;160;220m");
        else if (kc_global_mean < 0.7f) p += sprintf(p, "240;200;40m");
        else p += sprintf(p, "240;80;60m");
        p += sprintf(p, "%.3f", kc_global_mean);
        p += sprintf(p, "\033[0;38;2;140;120;60m  max:");
        p += sprintf(p, "\033[38;2;240;120;80m%.3f", kc_global_max);
        { int used = 30; for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 2: Distribution bar */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 2, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;180;150;80mDistribution: ");
        {
            int total = kc_n_simple + kc_n_structured + kc_n_complex;
            int bar_w = kp_w - 17;
            if (total > 0) {
                int b_simp = (kc_n_simple * bar_w) / total;
                int b_stru = (kc_n_structured * bar_w) / total;
                int b_comp = bar_w - b_simp - b_stru;
                if (b_comp < 0) b_comp = 0;
                for (int i = 0; i < b_simp; i++)
                    p += sprintf(p, "\033[38;2;60;160;220m\xe2\x96\x88");
                for (int i = 0; i < b_stru; i++)
                    p += sprintf(p, "\033[38;2;240;200;40m\xe2\x96\x88");
                for (int i = 0; i < b_comp; i++)
                    p += sprintf(p, "\033[38;2;240;80;60m\xe2\x96\x88");
            } else {
                for (int i = 0; i < bar_w; i++)
                    p += sprintf(p, "\033[38;2;30;25;10m\xe2\x96\x91");
            }
        }
        p += sprintf(p, " ");
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 3: Class counts */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 3, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;60;160;220mSimple:%d \033[38;2;240;200;40mStruct:%d \033[38;2;240;80;60mComplex:%d",
                     kc_n_simple, kc_n_structured, kc_n_complex);
        { int used = 34; for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 4: Compressibility interpretation */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 4, kp_col, kbdr, kbg);
        {
            int total = kc_n_simple + kc_n_structured + kc_n_complex;
            float simp_pct = total > 0 ? (float)kc_n_simple * 100.0f / total : 0;
            float comp_pct = total > 0 ? (float)kc_n_complex * 100.0f / total : 0;
            p += sprintf(p, " \033[38;2;160;140;80mCompressible:%.0f%% Random:%.0f%%",
                         simp_pct, comp_pct);
        }
        { int used = 30; for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 5: Min complexity */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 5, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;140;120;60mMin K: \033[38;2;60;160;220m%.3f",
                     kc_global_min);
        p += sprintf(p, " \033[38;2;100;80;40m(most compressible)");
        { int used = 34; for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 6: Sparkline — mean complexity over time */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 6, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;140;120;60mK ");
        if (kc_hist_count > 1) {
            int n = kc_hist_count < 30 ? kc_hist_count : 30;
            const char *spark_chars[] = {"\xe2\x96\x81","\xe2\x96\x82","\xe2\x96\x83",
                                         "\xe2\x96\x84","\xe2\x96\x85","\xe2\x96\x86",
                                         "\xe2\x96\x87","\xe2\x96\x88"};
            for (int i = 0; i < n; i++) {
                int idx = (kc_hist_idx - n + i + KC_HIST_LEN) % KC_HIST_LEN;
                int lvl = (int)(kc_hist_mean[idx] * 7.0f);
                if (lvl < 0) lvl = 0;
                if (lvl > 7) lvl = 7;
                if (kc_hist_mean[idx] < 0.3f)
                    p += sprintf(p, "\033[38;2;60;160;220m%s", spark_chars[lvl]);
                else if (kc_hist_mean[idx] < 0.7f)
                    p += sprintf(p, "\033[38;2;240;200;40m%s", spark_chars[lvl]);
                else
                    p += sprintf(p, "\033[38;2;240;80;60m%s", spark_chars[lvl]);
            }
        }
        p += sprintf(p, "%s", kbg);
        { int used = 4 + (kc_hist_count < 30 ? kc_hist_count : 30);
          for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Row 7: Toggle hint */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     kp_row + 7, kp_col, kbdr, kbg);
        p += sprintf(p, " \033[38;2;100;80;40m[^]toggle  LZ77 compression ratio");
        { int used = 37; for (int i = used; i < kp_w - 1; i++) *p++ = ' '; }
        p += sprintf(p, "%s\xe2\x94\x82%s", kbdr, krst);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94", kp_row + 8, kp_col, kbdr);
        for (int i = 0; i < kp_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", krst);
    }

    /* ── Population Dynamics Dashboard overlay ─────────────────────────────── */
    if (dashboard_mode && hist_count > 1) {
        int db_w = 62;      /* panel width */
        int graph_h = 16;   /* graph rows */
        int db_h = graph_h + 6; /* border + title + stats + graph + legend + border */
        int db_col = (term_cols - db_w) / 2;
        int db_row = (term_rows - db_h) / 2;
        if (db_col < 1) db_col = 1;
        if (db_row < 3) db_row = 3;

        const char *bdr = "\033[38;2;80;180;255;48;2;8;12;20m";
        const char *bg  = "\033[48;2;8;12;20m";
        const char *rst3 = "\033[0m";

        /* Compute stats over history */
        int n = hist_count;
        int mn = hist_get(0), mx = hist_get(0);
        long long sum = 0;
        for (int i = 0; i < n; i++) {
            int v = hist_get(i);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
            sum += v;
        }
        int avg = (int)(sum / n);
        int delta = (n >= 2) ? hist_get(0) - hist_get(1) : 0;

        /* Growth rate over last 10 generations */
        int rate_window = (n > 10) ? 10 : n - 1;
        int growth = (rate_window > 0) ? hist_get(0) - hist_get(rate_window) : 0;
        float growth_per_gen = (rate_window > 0) ? (float)growth / rate_window : 0.0f;

        /* Trend indicator */
        const char *trend;
        if (growth_per_gen > 2.0f)       trend = "\033[38;2;80;255;80m\xe2\x96\xb2 growing";
        else if (growth_per_gen > 0.2f)  trend = "\033[38;2;80;255;80m\xe2\x86\x97 rising";
        else if (growth_per_gen > -0.2f) trend = "\033[38;2;200;200;200m\xe2\x86\x92 stable";
        else if (growth_per_gen > -2.0f) trend = "\033[38;2;255;200;80m\xe2\x86\x98 falling";
        else                             trend = "\033[38;2;255;80;80m\xe2\x96\xbc dying";

        /* Top border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x8c\xe2\x94\x80 Population Dynamics ",
                     db_row, db_col, bdr);
        for (int i = 22; i < db_w - 1; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x90';
        p += sprintf(p, "%s", rst3);

        /* Stats row 1 */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     db_row + 1, db_col, bdr, bg);
        p += sprintf(p, " \033[38;2;80;200;255mPop:\033[38;2;255;255;255m%-6d"
                     " \033[38;2;120;120;180m\xce\x94:\033[38;2;%s%+d"
                     " \033[38;2;120;120;180mMin:\033[38;2;200;200;200m%-5d"
                     " \033[38;2;120;120;180mMax:\033[38;2;200;200;200m%-5d"
                     " \033[38;2;120;120;180mAvg:\033[38;2;200;200;200m%-5d",
                     population,
                     (delta >= 0) ? "80;255;80m" : "255;80;80m", delta,
                     mn, mx, avg);
        /* Pad to width */
        p += sprintf(p, "%s", bg);
        for (int i = 52; i < db_w - 1; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst3);

        /* Stats row 2: trend + species info */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     db_row + 2, db_col, bdr, bg);
        p += sprintf(p, " \033[38;2;120;120;180mTrend: %s", trend);
        if (ecosystem_mode && hist_count > 0) {
            int pa = hist_get_a(0), pb = hist_get_b(0);
            p += sprintf(p, "%s  \033[38;2;60;120;255m\xe2\x97\x89" "A:%d"
                         "  \033[38;2;255;80;40m\xe2\x97\x89" "B:%d",
                         bg, pa, pb);
        }
        p += sprintf(p, "%s", bg);
        /* Pad - approximate since we don't know exact cursor col */
        for (int i = 0; i < 8; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst3);

        /* Separator */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x9c", db_row + 3, db_col, bdr);
        for (int i = 0; i < db_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\xa4';
        p += sprintf(p, "%s", rst3);

        /* Graph area: plot population as block chars, newest on right */
        int graph_w = db_w - 2; /* inside borders */
        int gn = (hist_count < graph_w) ? hist_count : graph_w;

        /* For each row of the graph (top=max, bottom=min) */
        int range = mx - mn;
        if (range == 0) range = 1;

        /* Build column heights (0 to graph_h-1) for total, species A, species B */
        int col_total[120], col_a[120], col_b[120];
        for (int c = 0; c < gn; c++) {
            int age = gn - 1 - c; /* age: leftmost col = oldest */
            col_total[c] = (hist_get(age) - mn) * (graph_h - 1) / range;
            if (ecosystem_mode) {
                /* Scale species to same range as total */
                col_a[c] = (hist_get_a(age) - mn) * (graph_h - 1) / range;
                col_b[c] = (hist_get_b(age) - mn) * (graph_h - 1) / range;
                if (col_a[c] < 0) col_a[c] = 0;
                if (col_b[c] < 0) col_b[c] = 0;
            }
        }

        for (int row = 0; row < graph_h; row++) {
            int y_val = graph_h - 1 - row; /* value level for this row (0=bottom) */
            p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                         db_row + 4 + row, db_col, bdr, bg);

            /* Y-axis label on leftmost 5 chars */
            if (row == 0)
                p += sprintf(p, "\033[38;2;100;100;140m%5d", mx);
            else if (row == graph_h - 1)
                p += sprintf(p, "\033[38;2;100;100;140m%5d", mn);
            else if (row == graph_h / 2)
                p += sprintf(p, "\033[38;2;100;100;140m%5d", (mx + mn) / 2);
            else
                p += sprintf(p, "     ");

            int data_w = graph_w - 5; /* remaining width for data points */
            int dn = (gn < data_w) ? gn : data_w;
            int offset = data_w - dn; /* left padding */

            for (int c = 0; c < offset; c++)
                p += sprintf(p, "\033[38;2;20;25;35m\xc2\xb7");

            for (int c = 0; c < dn; c++) {
                int ci = c + (gn - dn); /* index into col arrays */
                if (ecosystem_mode) {
                    /* Show species lines: A=blue, B=red, overlap=white */
                    int is_a = (col_a[ci] == y_val);
                    int is_b = (col_b[ci] == y_val);
                    int is_t = (col_total[ci] == y_val);
                    if (is_a && is_b)
                        p += sprintf(p, "\033[38;2;255;255;255m\xe2\x96\x88");
                    else if (is_a)
                        p += sprintf(p, "\033[38;2;60;120;255m\xe2\x96\x88");
                    else if (is_b)
                        p += sprintf(p, "\033[38;2;255;80;40m\xe2\x96\x88");
                    else if (is_t)
                        p += sprintf(p, "\033[38;2;80;200;255m\xe2\x96\x88");
                    else if (col_total[ci] > y_val)
                        p += sprintf(p, "\033[38;2;15;20;30m\xe2\x96\x91");
                    else
                        p += sprintf(p, "\033[38;2;12;16;24m ");
                } else {
                    /* Single population line with fill */
                    if (col_total[ci] == y_val) {
                        /* Line point — color by value level */
                        int lvl = y_val * 7 / (graph_h - 1);
                        if (lvl <= 2)
                            p += sprintf(p, "\033[38;2;255;80;60m\xe2\x96\x88");
                        else if (lvl <= 4)
                            p += sprintf(p, "\033[38;2;255;200;60m\xe2\x96\x88");
                        else
                            p += sprintf(p, "\033[38;2;80;255;120m\xe2\x96\x88");
                    } else if (col_total[ci] > y_val) {
                        /* Fill below line */
                        p += sprintf(p, "\033[38;2;15;20;30m\xe2\x96\x91");
                    } else {
                        p += sprintf(p, "\033[38;2;12;16;24m ");
                    }
                }
            }
            p += sprintf(p, "%s%s\xe2\x94\x82%s", bg, bdr, rst3);
        }

        /* Legend row */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x82%s",
                     db_row + 4 + graph_h, db_col, bdr, bg);
        if (ecosystem_mode)
            p += sprintf(p, " \033[38;2;80;200;255m\xe2\x94\x80total"
                         " \033[38;2;60;120;255m\xe2\x94\x80" "A"
                         " \033[38;2;255;80;40m\xe2\x94\x80" "B"
                         " \033[38;2;100;100;140m  %d gens  [y]close",
                         hist_count);
        else
            p += sprintf(p, " \033[38;2;80;200;255m\xe2\x94\x80population"
                         " \033[38;2;100;100;140m  %d gens  %.1f/gen  [y]close",
                         hist_count, (double)growth_per_gen);
        p += sprintf(p, "%s", bg);
        for (int i = 0; i < 6; i++) *p++ = ' ';
        p += sprintf(p, "%s\xe2\x94\x82%s", bdr, rst3);

        /* Bottom border */
        p += sprintf(p, "\033[%d;%dH%s\xe2\x94\x94",
                     db_row + 5 + graph_h, db_col, bdr);
        for (int i = 0; i < db_w; i++) { *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x80'; }
        *p++ = '\xe2'; *p++ = '\x94'; *p++ = '\x98';
        p += sprintf(p, "%s", rst3);
    }

    *p = '\0';

    (void)!write(STDOUT_FILENO, render_buf, p - render_buf);
}

/* ── Resize handling ───────────────────────────────────────────────────────── */

static volatile sig_atomic_t resize_flag = 0;

static void handle_winch(int sig) {
    (void)sig;
    resize_flag = 1;
}

static void apply_resize(void) {
    get_term_size(&term_cols, &term_rows);
    viewport_update();
    printf("\033[2J");
    fflush(stdout);
}

/* ── Main ──────────────────────────────────────────────────────────────────── */

static long long now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

int main(int argc, char **argv) {
    srand(time(NULL));
    build_pulsar();
    stamp_init();
    timeline_init();

    W = MAX_W;
    H = MAX_H;
    get_term_size(&term_cols, &term_rows);
    viewport_update();
    viewport_center();

    if (argc > 1 && argv[1][0] != '-') {
        /* Load RLE file from command line */
        if (!load_rle(argv[1])) {
            fprintf(stderr, "Failed to load RLE file: %s\n", argv[1]);
            return 1;
        }
    } else {
        grid_randomize(0.25);
    }
    hist_push(population);
    timeline_push(); /* save initial state */

    signal(SIGWINCH, handle_winch);

    term_raw_mode();
    mouse_enable();
    printf("\033[?25l");  /* hide cursor */
    printf("\033[2J\033[H");
    fflush(stdout);

    int running = 1;
    int speed_ms = 100;
    int draw_mode = 1;  /* mouse drawing on by default */
    long long last_step = now_ms();
    int mouse_held = 0; /* 0=none, 1=left(place), 2=right(erase) */

    for (;;) {
        if (resize_flag) {
            resize_flag = 0;
            apply_resize();
        }

        int key = read_input();
        if (gene_mode == 2 && (key == 'q' || key == 'Q' || key == 27)) {
            /* Close genetic explorer overlay without quitting */
            gene_mode = 0;
            gene_search_done = 0;
            printf("\033[2J"); fflush(stdout);
        }
        else if (gene_mode == 2 && key >= '1' && key <= '5') {
            /* Load selected candidate rule from gene overlay */
            int idx = key - '1';
            if (idx < GENBEST_SIZE && gene_best[idx].score > 0) {
                birth_mask = gene_best[idx].birth;
                survival_mask = gene_best[idx].survival;
                current_ruleset = find_matching_ruleset();
                if (current_ruleset < 0) current_ruleset = 0;
                char msg[64];
                snprintf(msg, sizeof(msg), "Loaded %s", gene_best[idx].label);
                flash_set(msg);
                gene_mode = 0;
                gene_search_done = 0;
                printf("\033[2J"); fflush(stdout);
            }
        }
        else if (gene_mode == 2 && key == 'G') {
            /* Evolve another generation while overlay is showing */
            gene_search_step();
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'q' || key == 'Q' || key == 27 || key == 3)
            break;
        else if (key == ' ' || key == 'p') {
            if (replay_mode) {
                /* Resume live from this historical point */
                timeline_restore(replay_pos);
                timeline_truncate_at(replay_pos);
                replay_mode = 0;
                replay_pos = 0;
                running = 1;
            } else {
                running = !running;
            }
        }
        else if (key == 'P') {
            capture_frame();
        }
        else if (key == 16) { /* Ctrl-P */
            capture_timeline();
        }
        else if (key == 's') {
            if (replay_mode) {
                /* Step forward in replay */
                if (replay_pos > 0) {
                    replay_pos--;
                    timeline_restore(replay_pos);
                }
            } else if (!running) {
                grid_step();
            }
        }
        else if (key == '<' || key == ',') {
            /* Rewind: enter/deepen replay */
            if (tl_len > 1) {
                if (!replay_mode) {
                    replay_mode = 1;
                    replay_pos = 0;
                    running = 0;
                }
                /* Step backward */
                if (replay_pos < tl_len - 1) {
                    replay_pos++;
                    timeline_restore(replay_pos);
                }
            }
        }
        else if (key == '>' || key == '.') {
            if (replay_mode) {
                if (replay_pos > 0) {
                    replay_pos--;
                    timeline_restore(replay_pos);
                } else {
                    /* Reached present — exit replay */
                    replay_mode = 0;
                }
            }
        }
        else if (key == 't')
            show_timeline = !show_timeline;
        else if (key == 'T') {
            /* Cycle tracer: off(0) → accumulate(1) → frozen(2) → off+clear(0) */
            tracer_mode = (tracer_mode + 1) % 3;
            if (tracer_mode == 0)
                memset(tracer, 0, sizeof(tracer));
        }
        else if (key == 19) /* Ctrl-S: save */
            save_state();
        else if (key == 15) { /* Ctrl-O: load */
            load_state(0); /* load most recent */
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 5) /* Ctrl-E: export RLE */
            export_rle();
        else if (key == 'r' || key == 'R') {
            grid_randomize(0.25);
            timeline_push(); /* save initial state */
            running = 1;
        }
        else if (key == 'c' || key == 'C') {
            if (population == 0 && generation == 0) {
                /* Second clear: also reset zones, temperature, emitters/absorbers, and portals */
                zones_clear();
                memset(temp_grid, 0, sizeof(temp_grid));
                temp_global = 0.0f;
                n_emitters = 0;
                n_absorbers = 0;
                n_portals = 0;
                portal_placing = 0;
            }
            grid_clear();
            running = 0;
        }
        else if (key >= '1' && key <= '5') {
            if (zone_mode) {
                zone_preset(key - '0');
                printf("\033[2J"); fflush(stdout);
            } else {
                load_pattern(key - '0');
                running = 1;
            }
        }
        else if (key == '+' || key == '=') {
            if (cone_mode >= 1) {
                cone_fwd_depth = cone_fwd_depth < 128 ? cone_fwd_depth + 4 : 128;
                if (cone_mode == 2) cone_compute();
            } else if (temp_mode) {
                temp_brush += 0.05f;
                if (temp_brush > 1.0f) temp_brush = 1.0f;
            } else if (emit_mode) {
                emit_rate = emit_rate > 1 ? emit_rate - 1 : 1;
            } else {
                speed_ms = speed_ms > 20 ? speed_ms - 20 : 20;
            }
        }
        else if (key == '-' || key == '_') {
            if (cone_mode >= 1) {
                cone_fwd_depth = cone_fwd_depth > 4 ? cone_fwd_depth - 4 : 4;
                if (cone_mode == 2) cone_compute();
            } else if (temp_mode) {
                temp_brush -= 0.05f;
                if (temp_brush < 0.0f) temp_brush = 0.0f;
            } else if (emit_mode) {
                emit_rate = emit_rate < 20 ? emit_rate + 1 : 20;
            } else {
                speed_ms = speed_ms < 1000 ? speed_ms + 20 : 1000;
            }
        }
        else if (key == 'd' || key == 'D')
            draw_mode = !draw_mode;
        else if (key == 'g' || key == 'G')
            show_graph = !show_graph;
        else if (key == 'y' || key == 'Y')
            dashboard_mode = !dashboard_mode;
        else if (key == 'w')
            topology = (topology + 1) % TOPO_COUNT;
        else if (key == 'W') {
            portal_mode = !portal_mode;
            if (portal_mode) {
                portal_placing = 0;
                emit_mode = 0;
                zone_mode = 0;
                stamp_mode = 0;
                draw_mode = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'h' || key == 'H')
            heatmap_mode = !heatmap_mode;
        else if (key == 'f' || key == 'F') {
            freq_mode = !freq_mode;
            if (freq_mode) {
                freq_analyze(); /* compute on toggle-on */
            }
        }
        else if (key == 'i' || key == 'I') {
            entropy_mode = !entropy_mode;
            if (entropy_mode) {
                entropy_compute(); /* compute on toggle-on */
            }
        }
        else if (key == 'L' || key == 'l') {
            lyapunov_mode = !lyapunov_mode;
            if (lyapunov_mode) {
                lyapunov_compute(); /* compute on toggle-on */
            }
        }
        else if (key == 'u' || key == 'U') {
            fourier_mode = !fourier_mode;
            if (fourier_mode) {
                fourier_compute(); /* compute on toggle-on */
            }
        }
        else if (key == 'F') {
            fractal_mode = !fractal_mode;
            if (fractal_mode) {
                fractal_compute(); /* compute on toggle-on */
            }
        }
        else if (key == 'C') {
            wolfram_mode = !wolfram_mode;
            if (wolfram_mode) {
                wolfram_classify(); /* compute on toggle-on */
            }
        }
        else if (key == 'O') {
            flow_mode = !flow_mode;
            if (flow_mode) {
                flow_compute(); /* compute on toggle-on */
            }
        }
        else if (key == 'A') {
            attractor_mode = !attractor_mode;
            if (attractor_mode) {
                attractor_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '9') {
            if (cone_mode > 0) {
                cone_mode = 0; /* toggle off */
                memset(cone_back, 0, sizeof(cone_back));
                memset(cone_fwd, 0, sizeof(cone_fwd));
            } else {
                cone_mode = 1; /* waiting for click */
                cone_sel_x = -1;
                cone_sel_y = -1;
            }
        }
        else if (key == '!') {
            surp_mode = !surp_mode;
            if (surp_mode) {
                surp_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '@') {
            mi_mode = !mi_mode;
            if (mi_mode) {
                mi_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '#') {
            cplx_mode = !cplx_mode;
            if (cplx_mode) {
                cplx_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '$') {
            topo_mode = !topo_mode;
            if (topo_mode) {
                topo_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '%') {
            rg_mode = !rg_mode;
            if (rg_mode) {
                rg_compute(); /* compute on toggle-on */
            }
        }
        else if (key == '^') {
            kc_mode = !kc_mode;
            if (kc_mode) {
                kc_compute(); /* compute on toggle-on */
            }
        }
        else if (key == ']') {
            if (temp_mode) {
                temp_brush_radius = temp_brush_radius < 20 ? temp_brush_radius + 1 : 20;
            } else if (stamp_mode) {
                stamp_sel = (stamp_sel + 1) % N_STAMPS;
            } else if (emit_mode) {
                emit_pattern = (emit_pattern + 1) % N_EMIT_PATTERNS;
            } else if (zone_mode) {
                zone_brush = (zone_brush + 1) % N_RULESETS;
            } else {
                current_ruleset = (current_ruleset + 1) % N_RULESETS;
                birth_mask = rulesets[current_ruleset].birth;
                survival_mask = rulesets[current_ruleset].survival;
            }
        }
        else if (key == '[') {
            if (temp_mode) {
                temp_brush_radius = temp_brush_radius > 1 ? temp_brush_radius - 1 : 1;
            } else if (stamp_mode) {
                stamp_sel = (stamp_sel - 1 + N_STAMPS) % N_STAMPS;
            } else if (emit_mode) {
                emit_pattern = (emit_pattern - 1 + N_EMIT_PATTERNS) % N_EMIT_PATTERNS;
            } else if (zone_mode) {
                zone_brush = (zone_brush - 1 + N_RULESETS) % N_RULESETS;
            } else {
                current_ruleset = (current_ruleset - 1 + N_RULESETS) % N_RULESETS;
                birth_mask = rulesets[current_ruleset].birth;
                survival_mask = rulesets[current_ruleset].survival;
            }
        }
        else if (key == 'n' || key == 'N')
            show_minimap = !show_minimap;
        else if (key == 'j' || key == 'J') {
            zone_mode = !zone_mode;
            emit_mode = 0; stamp_mode = 0;
            portal_mode = 0; portal_placing = 0;
            if (zone_mode) {
                draw_mode = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'e' || key == 'E') {
            emit_mode = !emit_mode;
            zone_mode = 0; stamp_mode = 0;
            portal_mode = 0; portal_placing = 0;
            if (emit_mode) {
                draw_mode = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'b' || key == 'B') {
            rule_editor = !rule_editor;
        }
        else if (key == 'G') {
            if (gene_mode == 0) {
                /* Start genetic search */
                gene_mode = 1;
                gene_gen = 0;
                gene_search_done = 0;
                gene_selected = -1;
                gene_search_step();
                gene_mode = 2; /* show results */
                printf("\033[2J"); fflush(stdout);
            }
            /* gene_mode==2 'G' is handled above in the priority block */
        }
        else if (key == 'v' || key == 'V') {
            census_mode = !census_mode;
            if (census_mode) {
                census_stale = 1;
                census_scan();
            }
        }
        else if (key == 'k' || key == 'K')
            symmetry = (symmetry + 1) % 4;
        else if (key == 'S') {
            stamp_mode = !stamp_mode;
            if (stamp_mode) {
                emit_mode = 0;
                zone_mode = 0;
                portal_mode = 0; portal_placing = 0;
                draw_mode = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'a') {
            ecosystem_mode = !ecosystem_mode;
            if (ecosystem_mode) {
                /* Assign existing live cells to species A by default */
                for (int y = 0; y < H; y++)
                    for (int x = 0; x < W; x++)
                        if (grid[y][x] && species[y][x] == 0)
                            species[y][x] = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == 'X') {
            temp_mode = !temp_mode;
            if (temp_mode) {
                emit_mode = 0; stamp_mode = 0;
                portal_mode = 0; portal_placing = 0;
                zone_mode = 0;
                draw_mode = 1;
            }
            printf("\033[2J"); fflush(stdout);
        }
        else if (key == '6') {
            if (ecosystem_mode)
                brush_species = (brush_species == 1) ? 2 : 1;
        }
        else if (key == '{') {
            if (temp_mode) {
                temp_global -= 0.01f;
                if (temp_global < 0.0f) temp_global = 0.0f;
            } else {
                interaction -= 0.1f;
                if (interaction < -1.0f) interaction = -1.0f;
            }
        }
        else if (key == '}') {
            if (temp_mode) {
                temp_global += 0.01f;
                if (temp_global > 1.0f) temp_global = 1.0f;
            } else {
                interaction += 0.1f;
                if (interaction > 1.0f) interaction = 1.0f;
            }
        }
        else if (key == 'm' || key == 'M') {
            /* Mutate: randomly flip one bit in birth or survival mask (bits 0-8) */
            int which = rand() % 18; /* 0-8: birth bits, 9-17: survival bits */
            if (which < 9)
                birth_mask ^= (1 << which);
            else
                survival_mask ^= (1 << (which - 9));
        }
        else if (key == 'z' || key == 'Z') {
            /* Zoom in */
            if (zoom > 1) {
                /* Center the new view on the midpoint of old view */
                int cx = view_x + view_w / 2;
                int cy = view_y + view_h / 2;
                zoom /= 2;
                viewport_update();
                view_x = cx - view_w / 2;
                view_y = cy - view_h / 2;
                viewport_update(); /* re-clamp */
                printf("\033[2J"); fflush(stdout);
            }
        }
        else if (key == 'x' || key == 'X') {
            /* Zoom out */
            if (zoom < 8) {
                int cx = view_x + view_w / 2;
                int cy = view_y + view_h / 2;
                zoom *= 2;
                viewport_update();
                view_x = cx - view_w / 2;
                view_y = cy - view_h / 2;
                viewport_update();
                printf("\033[2J"); fflush(stdout);
            }
        }
        else if (key == '0') {
            viewport_center();
        }
        else if (key == KEY_UP) viewport_pan(0, -1);
        else if (key == KEY_DOWN) viewport_pan(0, 1);
        else if (key == KEY_LEFT) viewport_pan(-1, 0);
        else if (key == KEY_RIGHT) viewport_pan(1, 0);
        else if (key == KEY_MOUSE) {
            MouseEvent *m = &last_mouse;
            int btn = m->button & 0x43;

            /* Rule editor click handling (intercept before grid) */
            if (rule_editor && m->type == 1 && (btn & 0x03) == 0) {
                if (rule_editor_click(m->x, m->y) || rule_editor_preset_click(m->x, m->y))
                    goto mouse_done;
            }

            /* Scroll wheel: stamp rotation / portal radius / emit radius / zoom */
            if (stamp_mode && m->type == 1 && (btn == 64 || btn == 65)) {
                if (btn == 64)
                    stamp_rot = (stamp_rot + 1) % 4;
                else
                    stamp_rot = (stamp_rot + 3) % 4;
            } else if (portal_mode && m->type == 1 && (btn == 64 || btn == 65)) {
                if (btn == 64)
                    portal_radius = portal_radius < 6 ? portal_radius + 1 : 6;
                else
                    portal_radius = portal_radius > 2 ? portal_radius - 1 : 2;
            } else if (temp_mode && m->type == 1 && (btn == 64 || btn == 65)) {
                if (btn == 64)
                    temp_brush_radius = temp_brush_radius < 20 ? temp_brush_radius + 1 : 20;
                else
                    temp_brush_radius = temp_brush_radius > 1 ? temp_brush_radius - 1 : 1;
            } else if (emit_mode && m->type == 1 && (btn == 64 || btn == 65)) {
                if (btn == 64)
                    absorb_radius = absorb_radius < 10 ? absorb_radius + 1 : 10;
                else
                    absorb_radius = absorb_radius > 1 ? absorb_radius - 1 : 1;
            } else if (m->type == 1 && btn == 64) {
                if (zoom > 1) {
                    int cx = view_x + view_w / 2;
                    int cy = view_y + view_h / 2;
                    zoom /= 2;
                    viewport_update();
                    view_x = cx - view_w / 2;
                    view_y = cy - view_h / 2;
                    viewport_update();
                    printf("\033[2J"); fflush(stdout);
                }
            } else if (m->type == 1 && btn == 65) {
                if (zoom < 8) {
                    int cx = view_x + view_w / 2;
                    int cy = view_y + view_h / 2;
                    zoom *= 2;
                    viewport_update();
                    view_x = cx - view_w / 2;
                    view_y = cy - view_h / 2;
                    viewport_update();
                    printf("\033[2J"); fflush(stdout);
                }
            } else if (draw_mode) {
                /* Convert terminal coords to grid coords through viewport */
                int gx, gy;
                if (zoom == 1) {
                    gx = view_x + (m->x - 1) / 2;
                    gy = view_y + (m->y - 3);
                } else if (zoom == 2) {
                    gx = view_x + (m->x - 1);
                    gy = view_y + (m->y - 3) * 2;
                } else if (zoom == 4) {
                    gx = view_x + (m->x - 1) * 2;
                    gy = view_y + (m->y - 3) * 2;
                } else { /* zoom == 8 */
                    gx = view_x + (m->x - 1) * 2;
                    gy = view_y + (m->y - 3) * 4;
                }

                if (cone_mode >= 1 && m->type == 1 && (btn & 0x03) == 0) {
                    /* Light cone: left-click selects target cell */
                    if (gx >= 0 && gx < W && gy >= 0 && gy < H) {
                        cone_sel_x = gx;
                        cone_sel_y = gy;
                        cone_compute();
                        char msg[64];
                        snprintf(msg, sizeof(msg), "Light cone: (%d,%d) depth=%d",
                                 gx, gy, cone_depth);
                        flash_set(msg);
                    }
                } else if (stamp_mode) {
                    /* Stamp placement: left-click places, right-click exits stamp mode */
                    if (m->type == 1) {
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) {
                            stamp_place(gx, gy);
                            char msg[64];
                            static const char *rot_names[] = {"0\xC2\xB0","90\xC2\xB0","180\xC2\xB0","270\xC2\xB0"};
                            snprintf(msg, sizeof(msg), "Placed %s (%s)",
                                     stamp_library[stamp_sel].name, rot_names[stamp_rot]);
                            flash_set(msg);
                        } else if (mbtn == 2) {
                            stamp_mode = 0;
                            printf("\033[2J"); fflush(stdout);
                        }
                    }
                    if (m->type == 2) mouse_held = 0;
                } else if (portal_mode) {
                    /* Portal placement: left=place entrance/exit, middle=remove */
                    if (m->type == 1) {
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) {
                            if (!portal_placing) {
                                /* First click: set entrance */
                                portal_wip_x = gx;
                                portal_wip_y = gy;
                                portal_placing = 1;
                            } else {
                                /* Second click: set exit, create portal pair */
                                if (n_portals < MAX_PORTALS) {
                                    portals[n_portals] = (Portal){
                                        portal_wip_x, portal_wip_y,
                                        gx, gy,
                                        portal_radius, 1
                                    };
                                    n_portals++;
                                    char msg[64];
                                    snprintf(msg, sizeof(msg), "Portal %d created (r=%d)", n_portals, portal_radius);
                                    flash_set(msg);
                                }
                                portal_placing = 0;
                            }
                        } else if (mbtn == 2) {
                            /* Right click: cancel placement or remove nearby */
                            if (portal_placing) {
                                portal_placing = 0;
                            } else {
                                remove_portal_near(gx, gy);
                            }
                        } else if (mbtn == 1) {
                            remove_portal_near(gx, gy);
                        }
                    }
                    if (m->type == 2) mouse_held = 0;
                } else if (emit_mode) {
                    /* Emitter/absorber placement: left=emitter, right=absorber, middle=remove */
                    if (m->type == 1) {
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) {
                            place_emitter_sym(gx, gy);
                        } else if (mbtn == 2) {
                            place_absorber_sym(gx, gy);
                        } else if (mbtn == 1) {
                            /* Middle click: remove nearby */
                            remove_source_sink_near(gx, gy);
                        }
                    }
                    /* No drag support for emit — one click per placement */
                    if (m->type == 2) mouse_held = 0;
                } else if (temp_mode) {
                    /* Temperature painting: left=paint hot, right=paint cold (T=0) */
                    if (m->type == 1) {
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) { mouse_held = 1; temp_paint_sym(gx, gy); }
                        else if (mbtn == 2) {
                            mouse_held = 2;
                            float save = temp_brush;
                            temp_brush = 0.0f;
                            temp_paint_sym(gx, gy);
                            temp_brush = save;
                        }
                    } else if (m->type == 3) {
                        if (mouse_held == 1) temp_paint_sym(gx, gy);
                        else if (mouse_held == 2) {
                            float save = temp_brush;
                            temp_brush = 0.0f;
                            temp_paint_sym(gx, gy);
                            temp_brush = save;
                        }
                    } else if (m->type == 2) {
                        mouse_held = 0;
                    }
                } else if (zone_mode) {
                    /* Zone painting mode: left=paint zone, right=reset to zone 0 */
                    if (m->type == 1) {
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) { mouse_held = 1; zone_paint_sym(gx, gy); }
                        else if (mbtn == 2) {
                            mouse_held = 2;
                            int save = zone_brush;
                            zone_brush = 0;
                            zone_paint_sym(gx, gy);
                            zone_brush = save;
                        }
                    } else if (m->type == 3) {
                        if (mouse_held == 1) zone_paint_sym(gx, gy);
                        else if (mouse_held == 2) {
                            int save = zone_brush;
                            zone_brush = 0;
                            zone_paint_sym(gx, gy);
                            zone_brush = save;
                        }
                    } else if (m->type == 2) {
                        mouse_held = 0;
                    }
                } else {
                    if (m->type == 1) { /* press */
                        int mbtn = btn & 0x03;
                        if (mbtn == 0) { mouse_held = 1; sym_apply(gx, gy, grid_set); }
                        else if (mbtn == 2) { mouse_held = 2; sym_apply(gx, gy, grid_unset); }
                    } else if (m->type == 3) { /* drag */
                        if (mouse_held == 1) sym_apply(gx, gy, grid_set);
                        else if (mouse_held == 2) sym_apply(gx, gy, grid_unset);
                    } else if (m->type == 2) { /* release */
                        mouse_held = 0;
                    }
                }
            }
            mouse_done: ;
        }

        long long now = now_ms();
        if (running && !replay_mode && now - last_step >= speed_ms) {
            grid_step();
            last_step = now;
        }

        /* Auto-refresh frequency analysis every 8 generations when active */
        if (freq_mode && freq_stale && (generation % 8 == 0 || !running)) {
            freq_analyze();
        }

        /* Auto-refresh entropy heatmap every 4 generations when active */
        if (entropy_mode && entropy_stale && (generation % 4 == 0 || !running)) {
            entropy_compute();
        }

        /* Auto-refresh census every 16 generations when active (heavier scan) */
        if (census_mode && census_stale && (generation % 16 == 0 || !running)) {
            census_scan();
        }

        /* Auto-refresh Lyapunov sensitivity every 16 generations (heavy: 2× shadow simulation) */
        if (lyapunov_mode && lyapunov_stale && (generation % 16 == 0 || !running)) {
            lyapunov_compute();
        }

        /* Auto-refresh Fourier spectrum every 8 generations (2D FFT on padded grid) */
        if (fourier_mode && fourier_stale && (generation % 8 == 0 || !running)) {
            fourier_compute();
        }

        /* Auto-refresh fractal dimension every 8 generations (box-counting over 7 scales) */
        if (fractal_mode && fractal_stale && (generation % 8 == 0 || !running)) {
            fractal_compute();
        }

        /* Auto-refresh Wolfram classifier every 16 generations (fuses entropy+Lyapunov+fractal+pop) */
        if (wolfram_mode && wolfram_stale && (generation % 16 == 0 || !running)) {
            wolfram_classify();
        }

        /* Auto-refresh information flow field every 16 generations (transfer entropy over temporal window) */
        if (flow_mode && flow_stale && (generation % 16 == 0 || !running)) {
            flow_compute();
        }

        /* Auto-refresh phase-space attractor every 8 generations (uses population history) */
        if (attractor_mode && attractor_stale && (generation % 8 == 0 || !running)) {
            attractor_compute();
        }

        /* Auto-refresh causal light cone every 8 generations while running */
        if (cone_mode == 2 && cone_sel_x >= 0 && (generation % 8 == 0)) {
            cone_compute();
        }

        /* Auto-refresh surprise field every 4 generations */
        if (surp_mode && surp_stale && (generation % 4 == 0 || !running)) {
            surp_compute();
        }

        /* Auto-refresh MI network every 8 generations */
        if (mi_mode && mi_stale && (generation % 8 == 0 || !running)) {
            mi_compute();
        }

        /* Auto-refresh complexity index every 4 generations */
        if (cplx_mode && cplx_stale && (generation % 4 == 0 || !running)) {
            cplx_compute();
        }

        /* Auto-refresh topological feature map every 4 generations */
        if (topo_mode && topo_stale && (generation % 4 == 0 || !running)) {
            topo_compute();
        }

        /* Auto-refresh RG flow every 4 generations */
        if (rg_mode && rg_stale && (generation % 4 == 0 || !running)) {
            rg_compute();
        }

        /* Auto-refresh Kolmogorov complexity every 4 generations */
        if (kc_mode && kc_stale && (generation % 4 == 0 || !running)) {
            kc_compute();
        }

        render(running, speed_ms, draw_mode);
        usleep(16000); /* ~60 fps cap */
    }

    return 0;
}
