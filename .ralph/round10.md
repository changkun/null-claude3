# Round 10: Time-Travel Replay with Timeline Scrubber

## Goal
Add a history recording system that captures grid snapshots every generation, allowing rewind, fast-forward, and scrubbing through simulation history. Enables branching from any historical point.

## What was built

### Timeline Ring Buffer
- Heap-allocated ring buffer storing up to 256 grid snapshots (~40MB)
- Each frame stores complete grid state (cell ages), ghost trails, generation number, and population count
- Automatic recording on every `grid_step()` call
- Proper cleanup on grid clear, randomize, and pattern load

### Replay Controls
- `<` / `,` — Rewind (step backward through history, enters replay mode)
- `>` / `.` — Fast-forward (step forward through history)
- `s` — Step forward one frame in replay mode (consistent with normal step behavior)
- `SPACE` — Resume live simulation from current historical point (branching)
- `t` — Toggle timeline bar visibility

### Timeline Bar Visualization
- Horizontal progress bar with playhead marker (●) showing position in history
- Bright/dim bar segments near the playhead for visual focus
- Frame counter showing position (e.g., "Gen 150 (42/256)")
- Yellow color scheme in replay mode, dim gray in live mode
- Shown inline on status bar after sparkline

### Branching
- Pressing SPACE in replay mode restores the historical state and truncates future frames
- Simulation continues from that point, creating a new branch of history
- All prior history is preserved

### Status Bar
- New "⏪ REPLAY" indicator replaces "▶ RUN" / "⏸ PAUSE" when in replay mode
- Help line updated with `[<>]time [t]tbar` shortcuts

## Architecture decisions
- Used `malloc` for the 256-frame buffer to avoid bloating the binary's BSS segment
- Ring buffer design (not linked list) for O(1) access by age offset
- Timeline automatically clears on destructive operations (clear, randomize, pattern load)
- Replay mode blocks the simulation loop (`running && !replay_mode`)
- `grid_step()` pushes to timeline, so history builds automatically during live simulation

## Explore/exploit balance
At 55% exploration, chose to deepen engagement with *existing* features rather than add another spatial dimension. Time-travel is a force multiplier — every prior feature (heatmaps, zones, emitters, kaleidoscope) becomes more powerful when you can rewind and replay to observe emergent phenomena.

## Lines changed
~130 lines added to life.c (now ~1800 lines total)
