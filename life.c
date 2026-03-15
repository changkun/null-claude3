/*
 * Life-like Cellular Automaton Explorer — terminal edition (C / ANSI)
 *
 * Controls:
 *   SPACE / p   Play / Pause
 *   s           Step one generation (while paused)
 *   r           Randomize the grid
 *   c           Clear the grid
 *   1-5         Load preset pattern
 *                 1=Glider  2=Pulsar  3=Gosper Gun  4=R-pentomino  5=Acorn
 *   +/-         Speed up / slow down
 *   d           Toggle draw mode (left-click=place, right-click=erase)
 *   g           Toggle population sparkline graph
 *   w           Toggle toroidal wrapping
 *   h           Toggle heatmap mode (age coloring + ghost trails)
 *   [ / ]       Cycle through rule presets (B/S notation)
 *   m           Mutate — randomly flip one birth/survival bit
 *   k           Cycle symmetry: none → 2-fold → 4-fold → 8-fold (kaleidoscope)
 *   z / x       Zoom in / out (3 levels: 1x, 2x half-block, 4x quarter)
 *   n           Toggle minimap overlay (shows full grid + viewport rect when zoomed)
 *   Arrow keys  Pan viewport across the full 400×200 grid
 *   0           Re-center viewport on grid center
 *   q / ESC     Quit
 *
 *   Mouse:      Left-click to place cells, right-click to erase
 *               Click+drag to paint/erase continuously
 *               Scroll wheel to zoom in/out
 *
 * Build:  gcc -O2 -o life life.c
 * Run:    ./life
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

/* ── Grid ──────────────────────────────────────────────────────────────────── */

#define MAX_W 400
#define MAX_H 200

/* grid stores cell age: 0=dead, 1+=generations alive (capped at 255) */
static unsigned char grid[MAX_H][MAX_W];
static unsigned char next_grid[MAX_H][MAX_W];

/* ghost trails: 0=none, 1..GHOST_FRAMES=fading (GHOST_FRAMES=just died) */
#define GHOST_FRAMES 5
static unsigned char ghost[MAX_H][MAX_W];

static int W = MAX_W, H = MAX_H; /* simulation always uses full grid */
static int generation;
static int population;
static int wrap_mode = 0; /* toroidal wrapping */
static int heatmap_mode = 1; /* age heatmap + ghost trails (on by default) */
static int symmetry = 0; /* 0=none, 1=2-fold, 2=4-fold, 3=8-fold */
static int show_minimap = 1; /* minimap overlay when zoomed */

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
    } else { /* zoom == 4 */
        /* quarter: 2 cells per char wide, 2 cells per row tall */
        view_w = term_cols * 2;
        view_h = usable_rows * 2;
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
    int step = (zoom == 1) ? 4 : (zoom == 2) ? 8 : 16;
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
static int hist_pos = 0;
static int hist_count = 0;
static int show_graph = 1;

static void hist_push(int pop) {
    pop_history[hist_pos] = pop;
    hist_pos = (hist_pos + 1) % HIST_LEN;
    if (hist_count < HIST_LEN) hist_count++;
}

static int hist_get(int age) {
    /* age=0 is most recent */
    int idx = (hist_pos - 1 - age + HIST_LEN * 2) % HIST_LEN;
    return pop_history[idx];
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
    generation = 0;
    population = 0;
    hist_count = 0;
    hist_pos = 0;
}

static void grid_randomize(double density) {
    grid_clear();
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            if ((double)rand() / RAND_MAX < density) {
                grid[y][x] = 1;
                population++;
            }
}

static inline int wrap_coord(int v, int max) {
    if (v < 0) return v + max;
    if (v >= max) return v - max;
    return v;
}

static void grid_step(void) {
    population = 0;
    memset(next_grid, 0, sizeof(next_grid));

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0) continue;
                    int nx = x + dx, ny = y + dy;
                    if (wrap_mode) {
                        nx = wrap_coord(nx, W);
                        ny = wrap_coord(ny, H);
                        n += (grid[ny][nx] > 0);
                    } else {
                        if (nx >= 0 && nx < W && ny >= 0 && ny < H)
                            n += (grid[ny][nx] > 0);
                    }
                }
            int alive = grid[y][x] > 0;
            if ((alive && (survival_mask & (1 << n))) ||
                (!alive && (birth_mask & (1 << n)))) {
                /* alive: increment age, cap at 255 */
                int age = grid[y][x];
                next_grid[y][x] = (age < 255) ? age + 1 : 255;
                population++;
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
    generation++;
    hist_push(population);
}

static void grid_set(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H && !grid[y][x]) {
        grid[y][x] = 1;
        ghost[y][x] = 0;
        population++;
    }
}

static void grid_unset(int x, int y) {
    if (x >= 0 && x < W && y >= 0 && y < H && grid[y][x]) {
        grid[y][x] = 0;
        ghost[y][x] = GHOST_FRAMES;
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

/* Ghost trail colors: fading gray/blue */
static RGB ghost_to_rgb(int g) {
    /* g ranges from GHOST_FRAMES (just died, brightest) to 1 (about to vanish) */
    int brightness = 30 + (g * 50) / GHOST_FRAMES; /* 30..80 */
    int blue_tint  = 40 + (g * 60) / GHOST_FRAMES; /* 40..100 */
    return (RGB){ brightness, brightness, blue_tint };
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
                    for (int gx = gx0; gx < gx1 && !alive[q]; gx++)
                        if (grid[gy][gx]) alive[q] = 1;
            }

            int bits = 0;
            int any_border = 0;
            for (int q = 0; q < 4; q++) {
                if (alive[q] || border[q]) bits |= (1 << q);
                if (border[q]) any_border = 1;
            }

            /* Color: yellow for viewport rect, dim green for cells, dark bg */
            if (any_border)
                p += sprintf(p, "\033[93;48;2;20;20;30m");
            else if (bits)
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

/* ── Rendering ─────────────────────────────────────────────────────────────── */

/* Larger buffer for true-color escape sequences */
static char render_buf[MAX_H * MAX_W * 40 + 16384];

/* Get cell color for rendering (returns 1 if alive, fills rgb; 2 if ghost) */
static int cell_color(int x, int y, RGB *out) {
    if (x < 0 || x >= W || y < 0 || y >= H) return 0;
    if (grid[y][x]) {
        if (heatmap_mode)
            *out = age_to_rgb(grid[y][x]);
        else {
            /* flat green for non-heatmap */
            *out = (RGB){0, 200, 0};
        }
        return 1;
    }
    if (heatmap_mode && ghost[y][x]) {
        *out = ghost_to_rgb(ghost[y][x]);
        return 2; /* ghost */
    }
    return 0;
}

static void render(int running, int speed_ms, int draw_mode) {
    char *p = render_buf;

    /* cursor home */
    p += sprintf(p, "\033[H");

    /* status bar line 1 */
    const char *state = running
        ? "\033[92m\u25B6 RUN\033[0m"
        : "\033[93m\u23F8 PAUSE\033[0m";
    const char *wrap_str = wrap_mode
        ? " \033[95m\u221E\033[0m"
        : "";
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

    /* Zoom indicator */
    char zoom_str[32] = "";
    if (zoom > 1)
        snprintf(zoom_str, sizeof(zoom_str),
                 " \033[94m\u2302%dx\033[0m", zoom);

    /* Minimap indicator */
    const char *map_str = (zoom > 1 && show_minimap)
        ? " \033[94m\u25A3MAP\033[0m" : "";

    /* Rule string */
    char rule_str[32];
    rule_to_string(rule_str, sizeof(rule_str));
    int rs = find_matching_ruleset();
    char rule_display[80];
    if (rs >= 0)
        snprintf(rule_display, sizeof(rule_display),
                 "\033[95m%s\033[90m(%s)\033[0m", rule_str, rulesets[rs].name);
    else
        snprintf(rule_display, sizeof(rule_display),
                 "\033[95m%s\033[33m(mutant)\033[0m", rule_str);

    p += sprintf(p, " %s%s%s%s%s%s%s  %s  Gen \033[96m%d\033[0m  Pop \033[96m%d\033[0m  "
                     "\033[90m%dms\033[0m",
                 state, wrap_str, draw_str, heat_str, sym_str, zoom_str, map_str,
                 rule_display, generation, population, speed_ms);

    /* sparkline right after stats */
    if (show_graph && hist_count > 1) {
        p += sprintf(p, "  ");
        render_sparkline(&p, 40);
    }

    p += sprintf(p, "\033[K\n");

    /* status bar line 2: compact help */
    p += sprintf(p, " \033[90m[SPC]play [s]step [r]rand [c]clr "
                     "[1-5]pre [d]draw [k]sym [g]graph [w]wrap [h]heat "
                     "[/]rule [m]mut [z/x]zoom [n]map [\u2190\u2191\u2192\u2193]pan [0]center [q]quit\033[0m\033[K\n");

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
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else if (t == 2) {
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xC2\xB7 \033[0m", c.r, c.g, c.b);
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

                if (tt == 1 && tb == 1) {
                    /* both alive: fg=top, bg=bottom, print ▀ */
                    p += sprintf(p, "\033[38;2;%d;%d;%d;48;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b, cb.r, cb.g, cb.b);
                } else if (tt == 1) {
                    /* top alive only */
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x80\033[0m",
                                 ct.r, ct.g, ct.b);
                } else if (tb == 1) {
                    /* bottom alive only: use ▄ (lower half) */
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xe2\x96\x84\033[0m",
                                 cb.r, cb.g, cb.b);
                } else if (tt == 2 || tb == 2) {
                    /* ghost in either half — dim dot */
                    RGB gc = tt == 2 ? ct : cb;
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xC2\xB7\033[0m",
                                 gc.r, gc.g, gc.b);
                } else {
                    *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    } else { /* zoom == 4 */
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

                /* Check which quadrants are alive */
                RGB dummy;
                int tl = (cell_color(gx0, gy0, &dummy) == 1);
                int tr = (cell_color(gx1, gy0, &dummy) == 1);
                int bl = (cell_color(gx0, gy1, &dummy) == 1);
                int br = (cell_color(gx1, gy1, &dummy) == 1);
                int bits = tl | (tr << 1) | (bl << 2) | (br << 3);

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
    }

    /* Minimap overlay (only when zoomed) */
    render_minimap(&p);

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

int main(void) {
    srand(time(NULL));
    build_pulsar();

    W = MAX_W;
    H = MAX_H;
    get_term_size(&term_cols, &term_rows);
    viewport_update();
    viewport_center();

    grid_randomize(0.25);
    hist_push(population);

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
        if (key == 'q' || key == 'Q' || key == 27 || key == 3)
            break;
        else if (key == ' ' || key == 'p' || key == 'P')
            running = !running;
        else if (key == 's' && !running)
            grid_step();
        else if (key == 'r' || key == 'R') {
            grid_randomize(0.25);
            running = 1;
        }
        else if (key == 'c' || key == 'C') {
            grid_clear();
            running = 0;
        }
        else if (key >= '1' && key <= '5') {
            load_pattern(key - '0');
            running = 1;
        }
        else if (key == '+' || key == '=')
            speed_ms = speed_ms > 20 ? speed_ms - 20 : 20;
        else if (key == '-' || key == '_')
            speed_ms = speed_ms < 1000 ? speed_ms + 20 : 1000;
        else if (key == 'd' || key == 'D')
            draw_mode = !draw_mode;
        else if (key == 'g' || key == 'G')
            show_graph = !show_graph;
        else if (key == 'w' || key == 'W')
            wrap_mode = !wrap_mode;
        else if (key == 'h' || key == 'H')
            heatmap_mode = !heatmap_mode;
        else if (key == ']') {
            current_ruleset = (current_ruleset + 1) % N_RULESETS;
            birth_mask = rulesets[current_ruleset].birth;
            survival_mask = rulesets[current_ruleset].survival;
        }
        else if (key == '[') {
            current_ruleset = (current_ruleset - 1 + N_RULESETS) % N_RULESETS;
            birth_mask = rulesets[current_ruleset].birth;
            survival_mask = rulesets[current_ruleset].survival;
        }
        else if (key == 'n' || key == 'N')
            show_minimap = !show_minimap;
        else if (key == 'k' || key == 'K')
            symmetry = (symmetry + 1) % 4;
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
            if (zoom < 4) {
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

            /* Scroll wheel zoom: button 64=scroll-up(zoom in), 65=scroll-down(zoom out) */
            if (m->type == 1 && btn == 64) {
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
                if (zoom < 4) {
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
                } else { /* zoom == 4 */
                    gx = view_x + (m->x - 1) * 2;
                    gy = view_y + (m->y - 3) * 2;
                }

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

        long long now = now_ms();
        if (running && now - last_step >= speed_ms) {
            grid_step();
            last_step = now;
        }

        render(running, speed_ms, draw_mode);
        usleep(16000); /* ~60 fps cap */
    }

    return 0;
}
