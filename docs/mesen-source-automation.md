# Mesen 2 Source Automation Flow

This workspace includes a one-shot Lua dumper script that runs in Mesen 2 and writes the exact files expected by the importer.

## Files

- Script: `tools/mesen/nes_scene_dump.lua`
- Output folder: `C:/ProjectsPersonal/Zelda/dumps` (must already exist)
- Output prefix: `first_room_`

## Dump Steps in Mesen 2

1. Run Zelda and pause on the scene you want to capture.
2. Ensure script I/O is enabled in Mesen: `Debugger Settings -> Script Window -> Allow access to I/O and OS functions`.
3. Open the script window in Mesen 2 and load `tools/mesen/nes_scene_dump.lua`.
4. Resume emulation for at least one frame.
5. Confirm these files exist in `dumps`:
   - `first_room_nametable.bin`
   - `first_room_attribute.bin`
   - `first_room_palette.bin`
   - `first_room_palette32.bin`
   - `first_room_oam.bin`
   - `first_room_chr.bin`
   - `first_room_metadata.json`

## Convert Dumps to Native Room

Run this in the Zelda workspace:

```powershell
python tools/import_mesen_scene.py --nametable dumps/first_room_nametable.bin --palette dumps/first_room_palette.bin --attribute dumps/first_room_attribute.bin --chr dumps/first_room_chr.bin --oam dumps/first_room_oam.bin --sprite-height 8 --out extracted/rooms/first_room_from_mesen.room --name "First Room"
```

If metadata reports sprite height 16, switch to `--sprite-height 16`.

## One-Command VS Code Task

Use task `Import Mesen Scene + Run` to build, import, and launch in one step.

## Run Native Scene

```powershell
.\build\Debug\zelda_sdl3.exe --room extracted\rooms\first_room_from_mesen.room
```