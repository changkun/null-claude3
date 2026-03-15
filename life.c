/*
 * Conway's Game of Life — terminal edition (C / ANSI)
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
 *   q / ESC     Quit
 *
 *   Mouse:      Left-click to place cells, right-click to erase
 *               Click+drag to paint/erase continuously
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

static int W, H;
static int generation;
static int population;
static int wrap_mode = 0; /* toroidal wrapping */
static int heatmap_mode = 1; /* age heatmap + ghost trails (on by default) */

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
            if (n == 3 || (n == 2 && grid[y][x])) {
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
                return -2;
            }
            /* Other CSI sequences — ignore */
            while (c3 >= 0 && c3 < 0x40) c3 = read_byte(); /* consume params */
            return -1;
        }
        return 27;
    }
    return c;
}

/* ── Color palette (legacy flat color, cycles each generation) ─────────────── */

static const char *alive_colors[] = {
    "\033[42m",   /* green */
    "\033[102m",  /* bright green */
    "\033[46m",   /* cyan */
    "\033[106m",  /* bright cyan */
    "\033[43m",   /* yellow */
    "\033[103m",  /* bright yellow */
};
#define N_COLORS 6

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

/* ── Rendering ─────────────────────────────────────────────────────────────── */

/* Larger buffer for true-color escape sequences */
static char render_buf[MAX_H * MAX_W * 40 + 16384];

static void render(int running, int speed_ms, int draw_mode) {
    char *p = render_buf;

    /* cursor home */
    p += sprintf(p, "\033[H");

    /* status bar line 1 */
    const char *state = running
        ? "\033[92m\u25B6 RUN\033[0m"
        : "\033[93m\u23F8 PAUSE\033[0m";
    const char *wrap_str = wrap_mode
        ? " \033[95m\u221E\033[0m"  /* infinity symbol for toroidal */
        : "";
    const char *draw_str = draw_mode
        ? " \033[96m\u270E DRAW\033[0m"
        : "";
    const char *heat_str = heatmap_mode
        ? " \033[91m\u2588HEAT\033[0m"
        : "";
    p += sprintf(p, " %s%s%s%s  Gen \033[96m%d\033[0m  Pop \033[96m%d\033[0m  "
                     "\033[90m%dms\033[0m",
                 state, wrap_str, draw_str, heat_str, generation, population, speed_ms);

    /* sparkline right after stats */
    if (show_graph && hist_count > 1) {
        p += sprintf(p, "  ");
        render_sparkline(&p, 40);
    }

    p += sprintf(p, "\033[K\n");

    /* status bar line 2: compact help */
    p += sprintf(p, " \033[90m[SPC]play [s]step [r]rand [c]clr "
                     "[1-5]pre [d]draw [g]graph [w]wrap [h]heat [+/-]spd [q]quit\033[0m\033[K\n");

    if (heatmap_mode) {
        /* ── Heatmap rendering with true-color ── */
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (grid[y][x]) {
                    RGB c = age_to_rgb(grid[y][x]);
                    p += sprintf(p, "\033[48;2;%d;%d;%dm  \033[0m", c.r, c.g, c.b);
                } else if (ghost[y][x]) {
                    RGB c = ghost_to_rgb(ghost[y][x]);
                    /* Ghost: foreground dot on black background */
                    p += sprintf(p, "\033[38;2;%d;%d;%dm\xC2\xB7 \033[0m", c.r, c.g, c.b);
                } else {
                    *p++ = ' '; *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
    } else {
        /* ── Legacy flat-color rendering ── */
        const char *color = alive_colors[generation % N_COLORS];
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                if (grid[y][x]) {
                    const char *c = color;
                    while (*c) *p++ = *c++;
                    *p++ = ' '; *p++ = ' ';
                    *p++ = '\033'; *p++ = '['; *p++ = '0'; *p++ = 'm';
                } else {
                    *p++ = ' '; *p++ = ' ';
                }
            }
            *p++ = '\033'; *p++ = '['; *p++ = 'K'; *p++ = '\n';
        }
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
    int cols, rows;
    get_term_size(&cols, &rows);
    int nw = cols / 2;
    int nh = rows - 3; /* 2 status lines + margin */
    if (nw > MAX_W) nw = MAX_W;
    if (nh > MAX_H) nh = MAX_H;
    if (nw < 10) nw = 10;
    if (nh < 5) nh = 5;

    unsigned char tmp[MAX_H][MAX_W];
    unsigned char tmp_ghost[MAX_H][MAX_W];
    memcpy(tmp, grid, sizeof(grid));
    memcpy(tmp_ghost, ghost, sizeof(ghost));
    memset(grid, 0, sizeof(grid));
    memset(ghost, 0, sizeof(ghost));
    population = 0;
    int mw = nw < W ? nw : W;
    int mh = nh < H ? nh : H;
    for (int y = 0; y < mh; y++)
        for (int x = 0; x < mw; x++) {
            if (tmp[y][x]) { grid[y][x] = tmp[y][x]; population++; }
            ghost[y][x] = tmp_ghost[y][x];
        }

    W = nw;
    H = nh;
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

    int cols, rows;
    get_term_size(&cols, &rows);
    W = cols / 2;
    H = rows - 3;
    if (W > MAX_W) W = MAX_W;
    if (H > MAX_H) H = MAX_H;

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
        else if (key == -2 && draw_mode) {
            /* Mouse event */
            MouseEvent *m = &last_mouse;
            /* Convert terminal coords to grid coords (1-based, 2 chars per cell) */
            int gx = (m->x - 1) / 2;
            int gy = m->y - 3; /* 2 status lines + 1-based */

            if (m->type == 1) { /* press */
                int btn = m->button & 0x03;
                if (btn == 0) { mouse_held = 1; grid_set(gx, gy); }
                else if (btn == 2) { mouse_held = 2; grid_unset(gx, gy); }
            } else if (m->type == 3) { /* drag */
                if (mouse_held == 1) grid_set(gx, gy);
                else if (mouse_held == 2) grid_unset(gx, gy);
            } else if (m->type == 2) { /* release */
                mouse_held = 0;
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
