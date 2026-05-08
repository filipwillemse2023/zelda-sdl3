# Emulator Dump Workflow

This project now supports importing a scene captured from an emulator into the native `.room` format used by the SDL3 prototype.

## Expected Inputs

- A raw nametable dump of 960 or 1024 bytes.
- A raw palette RAM dump of at least 16 bytes.
- An optional raw attribute table dump of 64 bytes.
- An optional raw CHR RAM dump for actual tile graphics.
- An optional raw OAM dump of 256 bytes for sprite placement and tile indices.

## Example Conversion

```powershell
python tools/import_mesen_scene.py --nametable dumps/first_room_nametable.bin --palette dumps/first_room_palette.bin --attribute dumps/first_room_attribute.bin --chr dumps/first_room_chr.bin --oam dumps/first_room_oam.bin --oam-mode none --sprite-height 8 --out extracted/rooms/first_room_from_mesen.room --name "First Room"
```

Recommended defaults for reproducible room captures:

- Use `--oam-mode none` to avoid importing transient enemy/player snapshots from a paused frame.
- Use `--oam-mode snapshot` only when you explicitly want a full static sprite snapshot from that exact frame.
- Use `--oam-mode player-only` to import only the player character near the screen center.
- Keep `--sprite-height` aligned with emulator state (8 or 16) when snapshot mode is used.

## Mesen-Oriented Workflow

1. Run the ROM in Mesen 2.
2. Pause on the scene you want to import.
3. Export nametable memory for the active scene.
4. Export palette RAM.
5. Export the attribute table if the nametable dump does not already include it.
6. Export CHR RAM if you want the SDL3 prototype to draw real tile patterns.
7. Export OAM RAM if you want optional sprite snapshots.
8. Run conversion in stable mode:

```powershell
python tools/import_mesen_scene.py --nametable dumps/first_room_nametable.bin --palette dumps/first_room_palette.bin --attribute dumps/first_room_attribute.bin --chr dumps/first_room_chr.bin --oam dumps/first_room_oam.bin --oam-mode none --sprite-height 16 --bg-pattern-base 4096 --sprite-pattern-base 0 --out extracted/rooms/first_room_full_16.room --name "First Room Full (16)"
```

9. Run the SDL3 build against the generated room:

```powershell
.\build\Debug\zelda_sdl3.exe --room extracted\rooms\first_room_full_16.room
```

## Current Limits

- OAM import supports both `--sprite-height 8` and `--sprite-height 16` modes.
- OAM behavior is configurable with `--oam-mode snapshot|player-only|none`.
- Player-only mode detects center-screen sprites as likely player character candidates.
- The importer currently models a top-left 16x15 room crop of the active nametable.
- Attribute-derived palettes are expanded to per-tile palette IDs for the top-left 16x15 room crop.

## Runtime Room Selection

You can run the SDL3 app against a specific imported room file:

```powershell
.\build\Debug\zelda_sdl3.exe --room extracted\rooms\first_room_from_mesen.room
```