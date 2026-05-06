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
python tools/import_mesen_scene.py --nametable dumps/first_room_nametable.bin --palette dumps/first_room_palette.bin --attribute dumps/first_room_attribute.bin --chr dumps/first_room_chr.bin --oam dumps/first_room_oam.bin --sprite-height 8 --out extracted/rooms/first_room_from_mesen.room --name "First Room"
```

## Mesen-Oriented Workflow

1. Run the ROM in Mesen 2.
2. Pause on the scene you want to import.
3. Export nametable memory for the active scene.
4. Export palette RAM.
5. Export the attribute table if the nametable dump does not already include it.
6. Export CHR RAM if you want the SDL3 prototype to draw real tile patterns.
7. Export OAM RAM if you want imported sprite entities.
8. Run the conversion command above.
9. Point the SDL3 runtime at the generated `.room` file.

## Current Limits

- OAM import supports both `--sprite-height 8` and `--sprite-height 16` modes.
- The importer currently models a top-left 16x15 room crop of the active nametable.
- Attribute-derived palettes are expanded to per-tile palette IDs for the top-left 16x15 room crop.

## Runtime Room Selection

You can run the SDL3 app against a specific imported room file:

```powershell
.\build\Debug\zelda_sdl3.exe --room extracted\rooms\first_room_from_mesen.room
```