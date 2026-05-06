# Zelda NES to SDL3 Reverse-Engineering Plan

## Goal

Rebuild *The Legend of Zelda* as a native SDL3 C++ game that uses extracted data from the NES ROM rather than emulating the original 6502 code at runtime.

## What This ROM Tells Us

- Mapper 1 (MMC1), which means bank-switched PRG ROM.
- 128 KB of PRG ROM.
- No CHR ROM, so the game uses CHR RAM and uploads pattern data dynamically.
- Battery-backed save RAM, which means the save schema matters if compatibility is a goal.

## Recommended Strategy

1. Reverse-engineer the ROM into documented subsystems and extracted data.
2. Build a native SDL3 runtime that consumes those extracted assets.
3. Reimplement game logic system by system, validating against emulator traces.

## Tooling

- Mesen 2 for debugging, tracing, event viewer, nametable inspection, and CHR RAM inspection.
- The extractor in [tools/nes_rom_inspector.py](../tools/nes_rom_inspector.py) for repeatable ROM metadata and bank dumps.
- A spreadsheet or markdown tables for RAM addresses, bank maps, room formats, and entity formats.

## Phase 1: Cartridge and Memory Map

1. Build a PRG bank map.
2. Identify the fixed bank and likely reset path.
3. Document all MMC1 writes and note what each bank switch controls.
4. Create a RAM map with at least player state, room state, inventory, and save-related addresses.

## Phase 2: Scene and Data Extraction

1. Locate overworld room data.
2. Locate dungeon room data and per-room metadata.
3. Locate enemy spawn tables.
4. Locate item tables, damage tables, and projectile tables.
5. Locate text encoding and text blocks.
6. Identify palette tables and runtime CHR upload sources.

### Deliverables

- One documented format for overworld screens.
- One documented format for dungeon rooms.
- One documented format for object spawns.
- One documented text decoder.

## Phase 3: Graphics Reconstruction

Because this ROM uses CHR RAM, graphics likely exist as compressed, packed, or copied data in PRG ROM and are uploaded per scene.

1. Capture CHR RAM contents for title screen, one overworld screen, one dungeon room, and one combat-heavy room.
2. Dump nametables and palettes for those same scenes.
3. Compare the live dumps against PRG banks to locate source data and decompression/copy routines.
4. Decide whether the native port should preserve the original tile/palette pipeline or convert the game into atlas-based rendering during extraction.

## Phase 4: SDL3 Runtime Milestones

1. Window, renderer, fixed timestep, input, and save path.
2. Tilemap renderer for a 16x15 NES playfield.
3. Sprite/entity renderer.
4. HUD renderer.
5. Audio layer placeholder, then native music and SFX playback.

## Phase 5: Vertical Slice

Ship this slice before broadening scope:

1. Title screen.
2. One overworld screen rendered from extracted data.
3. Link movement and collision.
4. One sword attack state.
5. One enemy with damage and knockback.
6. One screen transition.
7. Save/load of health and position in a native debug format.

## Phase 6: Validation

1. Record emulator footage for deterministic test cases.
2. Match player speed, collision boundaries, damage frames, and room transitions.
3. Add replay-driven tests for movement and combat once the native runtime becomes stable.

## Immediate Next Investigation Targets

1. Confirm the reset flow and bank layout in Mesen.
2. Identify the room transition routine.
3. Capture CHR RAM and nametable state for the first overworld room.
4. Find the save RAM structure for hearts, inventory, and room flags.

## Suggested Repo Evolution

- `src/engine`: SDL3 platform code.
- `src/game`: native gameplay runtime.
- `tools`: ROM analysis and extraction scripts.
- `docs`: reverse-engineering notes and format documentation.
- `assets_extracted`: generated native assets, excluded from source control if large.