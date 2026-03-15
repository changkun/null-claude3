# Round 24: RLE Pattern File Import/Export

## Goal
Connect the simulator to the broader Game of Life ecosystem by supporting RLE (Run Length Encoded) files — the universal interchange format used by Golly, LifeWiki, and every major Life tool. This makes the 23 rounds of existing features (census detection, spaceship tracking, topology modes, frequency analysis) immediately usable on thousands of community patterns.

## What Was Built

### RLE Import (`load_rle()`, command-line loading)
- Parses standard RLE format: `#` comment lines, `x = N, y = M, rule = B.../S...` header, run-length encoded cell data (`b`=dead, `o`=alive, `$`=end row, `!`=end pattern)
- Extracts and applies the rule from the RLE header (sets `birth_mask`/`survival_mask`)
- Centers the pattern on the 400×200 grid; clips if pattern exceeds grid bounds
- Handles whitespace/newlines in run data and missing `!` terminator
- Clears grid and resets generation counter before loading
- Usage: `./life pattern.rle`

### RLE Export (`export_rle()`, Ctrl-E)
- Scans grid for bounding box of live cells
- Encodes as standard RLE with proper `x/y/rule` header and `#C` comment
- Auto-numbered output files: `export_001.rle`, `export_002.rle`, etc.
- Line-wraps RLE data at 70 columns per spec
- Optimizes consecutive blank rows (encoded as `N$`)
- Flash message confirms filename and dimensions

### Integration
- `main()` signature changed from `main(void)` to `main(int argc, char **argv)` for argument parsing
- Help bar updated with `C-e:rle`
- Header comment block updated with Ctrl-E documentation

## Key Design Decisions
- Rule extraction from RLE headers means loading a Highlife or Seeds pattern automatically switches the simulator to the correct ruleset
- Pattern centering rather than top-left placement gives better initial view
- Grid clearing on load prevents confusion from leftover state
- Export includes generation number in comment for provenance

## Files Modified
- `life.c`: Added `load_rle()`, `export_rle()`, `next_export_slot()`, `rule_to_string()`, command-line argument handling, help bar and header updates
