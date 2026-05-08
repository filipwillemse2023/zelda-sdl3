# Zelda SDL3 Port Prototype

This workspace now contains a native SDL3 C++ scaffold, a first-pass NES ROM inspection tool, and a reverse-engineering plan for turning the included NES ROM into a native game.

## Build the SDL3 Prototype

```powershell
cmake -S . -B build
cmake --build build
```

If SDL3 is not installed locally, CMake will attempt to fetch it automatically.

## Run the ROM Inspector

```powershell
python tools/nes_rom_inspector.py "Legend of Zelda, The (USA) (Rev 1).nes"
python tools/nes_rom_inspector.py "Legend of Zelda, The (USA) (Rev 1).nes" --json
python tools/nes_rom_inspector.py "Legend of Zelda, The (USA) (Rev 1).nes" --dump-dir extracted
```

The dump command now also writes:

- `extracted/analysis/*.json` for heuristic room, text, and entity candidates
- `extracted/rooms/prototype_overworld.room` for the SDL3 prototype renderer
- `extracted/analysis/bank_06_report.json` for the bank currently producing the strongest room-window matches

## Import an Emulator Scene

```powershell
python tools/import_mesen_scene.py --nametable dumps/first_room_nametable.bin --palette dumps/first_room_palette.bin --attribute dumps/first_room_attribute.bin --chr dumps/first_room_chr.bin --oam dumps/first_room_oam.bin --oam-mode none --sprite-height 8 --out extracted/rooms/first_room_from_mesen.room --name "First Room"
```

For deterministic room imports that do not include transient paused-frame sprites, keep `--oam-mode none`.
Use `--oam-mode snapshot` only when you want a static sprite snapshot imported from OAM.

Run a specific captured room directly:

```powershell
.\build\Debug\zelda_sdl3.exe --room extracted\rooms\first_room_from_mesen.room
```

See [docs/emulator-dump-workflow.md](docs/emulator-dump-workflow.md) for the expected dump workflow.

If you have Mesen 2 source available in this workspace, use the one-shot Lua dump script in [docs/mesen-source-automation.md](docs/mesen-source-automation.md) to generate `first_room_*` files automatically.

## Next Work

- Expand the extractor so it can decode room, text, and entity formats.
- Replace placeholder rendering in the SDL3 app with extracted room data.
- Add emulator-assisted dumping for CHR RAM and nametable capture.