from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path


ROOM_WIDTH = 32
ROOM_HEIGHT = 30
EXPECTED_TILE_BYTES = ROOM_WIDTH * ROOM_HEIGHT
ATTRIBUTE_WIDTH = 8
ATTRIBUTE_HEIGHT = 8


NES_PALETTE = [
    (124, 124, 124), (0, 0, 252), (0, 0, 188), (68, 40, 188),
    (148, 0, 132), (168, 0, 32), (168, 16, 0), (136, 20, 0),
    (80, 48, 0), (0, 120, 0), (0, 104, 0), (0, 88, 0),
    (0, 64, 88), (0, 0, 0), (0, 0, 0), (0, 0, 0),
    (188, 188, 188), (0, 120, 248), (0, 88, 248), (104, 68, 252),
    (216, 0, 204), (228, 0, 88), (248, 56, 0), (228, 92, 16),
    (172, 124, 0), (0, 184, 0), (0, 168, 0), (0, 168, 68),
    (0, 136, 136), (0, 0, 0), (0, 0, 0), (0, 0, 0),
    (248, 248, 248), (60, 188, 252), (104, 136, 252), (152, 120, 248),
    (248, 120, 248), (248, 88, 152), (248, 120, 88), (252, 160, 68),
    (248, 184, 0), (184, 248, 24), (88, 216, 84), (88, 248, 152),
    (0, 232, 216), (120, 120, 120), (0, 0, 0), (0, 0, 0),
    (252, 252, 252), (164, 228, 252), (184, 184, 248), (216, 184, 248),
    (248, 184, 248), (248, 164, 192), (240, 208, 176), (252, 224, 168),
    (248, 216, 120), (216, 248, 120), (184, 248, 184), (184, 248, 216),
    (0, 252, 252), (248, 216, 248), (0, 0, 0), (0, 0, 0),
]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert emulator scene dumps into the native .room format.")
    parser.add_argument("--nametable", type=Path, required=True, help="Raw nametable dump, 960 or 1024 bytes")
    parser.add_argument("--palette", type=Path, required=True, help="Raw palette RAM dump, 32 bytes")
    parser.add_argument("--attribute", type=Path, help="Raw attribute table dump, 64 bytes")
    parser.add_argument("--chr", type=Path, help="Raw CHR RAM dump to copy alongside the room asset")
    parser.add_argument("--oam", type=Path, help="Raw OAM dump, 256 bytes")
    parser.add_argument(
        "--oam-mode",
        choices=["snapshot", "none"],
        default="snapshot",
        help="How to interpret OAM data: snapshot imports visible sprites, none ignores OAM",
    )
    parser.add_argument(
        "--oam-max-entities",
        type=int,
        default=24,
        help="Maximum entities written from OAM when --oam-mode snapshot is active",
    )
    parser.add_argument("--sprite-height", type=int, choices=[8, 16], default=8, help="Sprite height mode used when decoding OAM")
    parser.add_argument("--bg-pattern-base", type=int, choices=[0, 4096], default=0, help="Background CHR pattern table base ($0000 or $1000)")
    parser.add_argument("--sprite-pattern-base", type=int, choices=[0, 4096], default=0, help="Sprite CHR pattern table base ($0000 or $1000, 8x8 mode)")
    parser.add_argument("--metadata", type=Path, help="Optional metadata JSON from nes_scene_dump.lua")
    parser.add_argument("--out", type=Path, required=True, help="Output .room path")
    parser.add_argument("--name", default="Imported Mesen Scene", help="Scene name")
    return parser.parse_args()


def validate_input_paths(args: argparse.Namespace) -> None:
    required_paths = {
        "--nametable": args.nametable,
        "--palette": args.palette,
    }

    optional_paths = {
        "--attribute": args.attribute,
        "--chr": args.chr,
        "--oam": args.oam,
        "--metadata": args.metadata,
    }

    missing = []
    for option, path in required_paths.items():
        if not path.exists():
            missing.append((option, path))

    for option, path in optional_paths.items():
        if path is not None and not path.exists():
            missing.append((option, path))

    if not missing:
        return

    dump_dir = args.nametable.parent
    available = []
    if dump_dir.exists() and dump_dir.is_dir():
        available = sorted(entry.name for entry in dump_dir.iterdir() if entry.is_file())

    message_lines = ["Missing input file(s):"]
    for option, path in missing:
        message_lines.append(f"  {option}: {path}")

    if available:
        message_lines.append("")
        message_lines.append(f"Available files in {dump_dir}:")
        for name in available:
            message_lines.append(f"  - {name}")

    raise FileNotFoundError("\n".join(message_lines))


def infer_metadata_path(nametable_path: Path) -> Path | None:
    stem = nametable_path.stem
    suffix = "_nametable"
    if not stem.endswith(suffix):
        return None

    prefix = stem[: -len(suffix)]
    candidate = nametable_path.with_name(f"{prefix}_metadata.json")
    if candidate.exists():
        return candidate
    return None


def extract_tiles(nametable_bytes: bytes) -> list[int]:
    if len(nametable_bytes) not in {960, 1024}:
        raise ValueError("Nametable dump must be 960 or 1024 bytes")
    tiles: list[int] = []
    for row in range(ROOM_HEIGHT):
        row_start = row * 32
        tiles.extend(nametable_bytes[row_start:row_start + ROOM_WIDTH])
    return tiles


def extract_palette(palette_bytes: bytes) -> list[list[tuple[int, int, int]]]:
    if len(palette_bytes) < 32:
        raise ValueError("Palette dump must contain 32 bytes")

    groups: list[list[tuple[int, int, int]]] = []
    for group in range(8):
        colors = []
        for color in palette_bytes[group * 4:(group + 1) * 4]:
            colors.append(NES_PALETTE[color & 0x3F])
        groups.append(colors)
    return groups


def extract_entities_from_oam(oam_bytes: bytes, sprite_height: int, max_entities: int) -> list[str]:
    if len(oam_bytes) < 256:
        raise ValueError("OAM dump must contain 256 bytes")

    entities: list[str] = []
    for index in range(64):
        base = index * 4
        y = oam_bytes[base]
        tile = oam_bytes[base + 1]
        attr = oam_bytes[base + 2]
        x = oam_bytes[base + 3]

        if y >= 0xEF:
            continue

        screen_y = y + 1
        if x >= ROOM_WIDTH * 16 or screen_y >= ROOM_HEIGHT * 16:
            continue

        palette_index = attr & 0x03
        flip_h = 1 if attr & 0x40 else 0
        flip_v = 1 if attr & 0x80 else 0
        if sprite_height == 8:
            entities.append(f"{x},{screen_y},sprite_{index:02d},{tile},{palette_index},{flip_h},{flip_v}")
        else:
            base_tile = tile & 0xFE
            top_tile = base_tile + (1 if flip_v else 0)
            bottom_tile = base_tile + (0 if flip_v else 1)
            entities.append(f"{x},{screen_y},sprite_{index:02d}_top,{top_tile},{palette_index},{flip_h},{flip_v}")
            entities.append(f"{x},{screen_y + 8},sprite_{index:02d}_bottom,{bottom_tile},{palette_index},{flip_h},{flip_v}")

        if len(entities) >= max_entities:
            break

    return entities


def expand_attributes(attribute_bytes: bytes) -> list[int]:
    if len(attribute_bytes) < 64:
        raise ValueError("Attribute data must contain at least 64 bytes")

    tile_palettes = [0] * EXPECTED_TILE_BYTES
    for tile_y in range(ROOM_HEIGHT):
        for tile_x in range(ROOM_WIDTH):
            attribute_x = tile_x // 4
            attribute_y = tile_y // 4
            attribute_index = attribute_y * ATTRIBUTE_WIDTH + attribute_x
            attribute = attribute_bytes[attribute_index]
            quadrant = ((tile_y % 4) // 2) * 2 + ((tile_x % 4) // 2)
            palette_index = (attribute >> (quadrant * 2)) & 0x03
            tile_palettes[tile_y * ROOM_WIDTH + tile_x] = palette_index
    return tile_palettes


def write_room(
    out_path: Path,
    name: str,
    tiles: list[int],
    tile_palettes: list[int],
    palette: list[list[tuple[int, int, int]]],
    chr_path: str | None,
    entities: list[str],
    sprite_height: int,
    bg_pattern_base: int,
    sprite_pattern_base: int,
) -> None:
    rows = []
    for index in range(0, len(tiles), ROOM_WIDTH):
        rows.append(" ".join(str(value) for value in tiles[index:index + ROOM_WIDTH]))

    palette_rows = []
    for index in range(0, len(tile_palettes), ROOM_WIDTH):
        palette_rows.append(" ".join(str(value) for value in tile_palettes[index:index + ROOM_WIDTH]))

    palette_groups = []
    for group in palette:
        palette_groups.append("|".join(f"{r},{g},{b}" for r, g, b in group))

    content = [
        "# Imported from emulator nametable/palette dumps",
        f"name={name}",
        f"width={ROOM_WIDTH}",
        f"height={ROOM_HEIGHT}",
        "tile_size=8",
        "player_start=120,112",
        f"sprite_height={sprite_height}",
        f"bg_pattern_base={bg_pattern_base}",
        f"sprite_pattern_base={sprite_pattern_base}",
        "palette=" + ";".join(palette_groups),
        *( [f"chr_path={chr_path}"] if chr_path else [] ),
        "tiles:",
        *rows,
        "tile_palettes:",
        *palette_rows,
        "entities:",
        *entities,
    ]
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(content), encoding="utf-8")


def main() -> int:
    args = parse_args()
    validate_input_paths(args)

    metadata_path = args.metadata if args.metadata else infer_metadata_path(args.nametable)
    metadata = {}
    if metadata_path:
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))

    sprite_height = int(metadata.get("sprite_height", args.sprite_height))
    bg_pattern_base = int(metadata.get("bg_pattern_base", args.bg_pattern_base))
    sprite_pattern_base = int(metadata.get("sprite_pattern_base", args.sprite_pattern_base))

    nametable_bytes = args.nametable.read_bytes()
    palette_bytes = args.palette.read_bytes()
    attribute_bytes = args.attribute.read_bytes() if args.attribute else nametable_bytes[960:1024]
    tiles = extract_tiles(nametable_bytes)
    palette = extract_palette(palette_bytes)
    tile_palettes = expand_attributes(attribute_bytes)
    if args.oam_mode == "none":
        entities = []
    elif args.oam:
        max_entities = max(args.oam_max_entities, 0)
        entities = extract_entities_from_oam(args.oam.read_bytes(), sprite_height, max_entities)
    else:
        entities = []

    chr_relative_path = None
    if args.chr:
        chr_relative_path = f"{args.out.stem}.chr.bin"
        shutil.copyfile(args.chr, args.out.parent / chr_relative_path)

    write_room(
        args.out,
        args.name,
        tiles,
        tile_palettes,
        palette,
        chr_relative_path,
        entities,
        sprite_height,
        bg_pattern_base,
        sprite_pattern_base,
    )

    output_metadata = {
        "name": args.name,
        "nametable": str(args.nametable),
        "palette": str(args.palette),
        "attribute": str(args.attribute) if args.attribute else "embedded in nametable dump",
        "chr": str(args.chr) if args.chr else None,
        "oam": str(args.oam) if args.oam else None,
        "oam_mode": args.oam_mode,
        "oam_max_entities": args.oam_max_entities,
        "metadata": str(metadata_path) if metadata_path else None,
        "sprite_height": sprite_height,
        "bg_pattern_base": bg_pattern_base,
        "sprite_pattern_base": sprite_pattern_base,
        "entities_written": len(entities),
        "out": str(args.out),
    }
    print(json.dumps(output_metadata, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())