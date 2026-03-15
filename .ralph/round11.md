# Round 11: Save/Load State Persistence

## Goal
Add file-based save and load so that configurations — grid state, zones, emitters, absorbers, ruleset, and symmetry settings — survive between sessions. Pure exploitation at 30% explore.

## What was built

### Binary Save Format
- Magic header "LIFE" + version byte for forward compatibility
- Serializes: birth/survival masks, symmetry, wrap mode, heatmap mode, zone_enabled
- Serializes: generation counter, population, emitter/absorber counts
- Full grid data: cell ages (80KB), ghost trails (80KB), zone map (80KB)
- Variable-length emitter array (4×int32 each) and absorber array (3×int32 each)
- Little-endian encoding throughout for portability

### Auto-Numbered Save Slots
- `Ctrl-S` saves to `save_001.life`, `save_002.life`, etc.
- Scans for first unused slot (up to 999) to prevent overwrites
- `Ctrl-O` loads the highest-numbered (most recent) save
- Files saved in current working directory

### Status Flash Feedback
- Green flash message on status bar for 2 seconds after save/load
- Shows filename: "Saved save_003.life" or "Loaded save_003.life"
- Error messages for missing saves, bad files, unknown versions

### State Restoration
- On load: resets timeline, population history, and replay mode
- Finds matching ruleset preset or keeps custom/mutant masks
- Full screen clear to avoid visual artifacts

## Architecture decisions
- Placed save/load code after all dependency declarations (rule system, history, emitters) to avoid forward-declaration complexity
- Used `(void)!fread()` pattern to suppress unused-result warnings while keeping the code clean
- Binary format chosen over text for speed (240KB+ of grid data per save)
- Version byte in header enables future format extensions without breaking old saves
- Flash message system uses `clock_gettime(CLOCK_MONOTONIC)` for reliable 2-second display, reusing the same clock source as the main loop

## Explore/exploit balance
At 30% exploration, this is pure exploitation. Save/load is the highest-leverage missing feature — it makes every prior feature (zones, emitters, absorbers, kaleidoscope, custom rules) permanently valuable by allowing configurations to survive across sessions. No new simulation mechanics, just infrastructure that multiplies the value of existing features.

## Lines changed
~190 lines added to life.c (now ~2060 lines total)
