from __future__ import annotations

import argparse
import hashlib
import json
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Any


HEADER_SIZE = 16
PRG_BANK_SIZE = 16 * 1024
CHR_BANK_SIZE = 8 * 1024
ROOM_WIDTH = 16
ROOM_HEIGHT = 15
ROOM_TILE_COUNT = ROOM_WIDTH * ROOM_HEIGHT


NES_PALETTE = [
    (84, 84, 84),
    (0, 30, 116),
    (8, 16, 144),
    (48, 0, 136),
    (68, 0, 100),
    (92, 0, 48),
    (84, 4, 0),
    (60, 24, 0),
    (32, 42, 0),
    (8, 58, 0),
    (0, 64, 0),
    (0, 60, 0),
    (0, 50, 60),
    (0, 0, 0),
    (0, 0, 0),
    (0, 0, 0),
]


@dataclass(slots=True)
class RomInfo:
    path: str
    size_bytes: int
    sha256: str
    ines_version: str
    mapper: int
    submapper: int
    mirroring: str
    has_battery: bool
    has_trainer: bool
    four_screen: bool
    prg_rom_banks_16kb: int
    chr_rom_banks_8kb: int
    prg_rom_size_bytes: int
    chr_rom_size_bytes: int
    prg_ram_size_bytes: int
    chr_ram_size_bytes: int
    reset_vector: int
    nmi_vector: int
    irq_vector: int


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect an iNES ROM and optionally dump banks.")
    parser.add_argument("rom", type=Path, help="Path to the .nes ROM file")
    parser.add_argument("--dump-dir", type=Path, help="Directory to write extracted PRG/CHR banks")
    parser.add_argument("--json", action="store_true", help="Print machine-readable JSON")
    return parser.parse_args()


def parse_ines(rom_bytes: bytes, source_path: Path) -> tuple[RomInfo, bytes, bytes]:
    if len(rom_bytes) < HEADER_SIZE:
        raise ValueError("ROM is too small to contain an iNES header")

    header = rom_bytes[:HEADER_SIZE]
    if header[:4] != b"NES\x1A":
        raise ValueError("File does not start with a valid iNES signature")

    flags6 = header[6]
    flags7 = header[7]
    is_nes2 = (flags7 & 0x0C) == 0x08

    if is_nes2:
        mapper = ((header[8] & 0x0F) << 8) | (flags7 & 0xF0) | (flags6 >> 4)
        submapper = header[8] >> 4
        prg_banks = header[4] | ((header[9] & 0x0F) << 8)
        chr_banks = header[5] | ((header[9] & 0xF0) << 4)
        prg_ram_shift = header[10] & 0x0F
        chr_ram_shift = header[11] & 0x0F
        prg_ram_size = 0 if prg_ram_shift == 0 else 64 << prg_ram_shift
        chr_ram_size = 0 if chr_ram_shift == 0 else 64 << chr_ram_shift
        ines_version = "NES 2.0"
    else:
        mapper = (flags7 & 0xF0) | (flags6 >> 4)
        submapper = 0
        prg_banks = header[4]
        chr_banks = header[5]
        prg_ram_units = header[8] if header[8] else 1
        prg_ram_size = prg_ram_units * 8 * 1024
        chr_ram_size = 8 * 1024 if chr_banks == 0 else 0
        ines_version = "iNES 1.0"

    trainer_size = 512 if flags6 & 0x04 else 0
    prg_rom_size = prg_banks * PRG_BANK_SIZE
    chr_rom_size = chr_banks * CHR_BANK_SIZE
    prg_start = HEADER_SIZE + trainer_size
    prg_end = prg_start + prg_rom_size
    chr_end = prg_end + chr_rom_size

    if chr_end > len(rom_bytes):
        raise ValueError("ROM is truncated relative to the header-declared sizes")

    prg_rom = rom_bytes[prg_start:prg_end]
    chr_rom = rom_bytes[prg_end:chr_end]

    if len(prg_rom) < 6:
        raise ValueError("PRG ROM is too small to contain vectors")

    nmi_vector = int.from_bytes(prg_rom[-6:-4], "little")
    reset_vector = int.from_bytes(prg_rom[-4:-2], "little")
    irq_vector = int.from_bytes(prg_rom[-2:], "little")

    rom_info = RomInfo(
        path=str(source_path),
        size_bytes=len(rom_bytes),
        sha256=hashlib.sha256(rom_bytes).hexdigest(),
        ines_version=ines_version,
        mapper=mapper,
        submapper=submapper,
        mirroring="vertical" if flags6 & 0x01 else "horizontal",
        has_battery=bool(flags6 & 0x02),
        has_trainer=bool(flags6 & 0x04),
        four_screen=bool(flags6 & 0x08),
        prg_rom_banks_16kb=prg_banks,
        chr_rom_banks_8kb=chr_banks,
        prg_rom_size_bytes=prg_rom_size,
        chr_rom_size_bytes=chr_rom_size,
        prg_ram_size_bytes=prg_ram_size,
        chr_ram_size_bytes=chr_ram_size,
        reset_vector=reset_vector,
        nmi_vector=nmi_vector,
        irq_vector=irq_vector,
    )
    return rom_info, prg_rom, chr_rom


def dump_banks(dump_dir: Path, prg_rom: bytes, chr_rom: bytes) -> None:
    dump_dir.mkdir(parents=True, exist_ok=True)
    (dump_dir / "prg").mkdir(exist_ok=True)
    (dump_dir / "chr").mkdir(exist_ok=True)

    for index in range(0, len(prg_rom), PRG_BANK_SIZE):
        bank_number = index // PRG_BANK_SIZE
        bank_path = dump_dir / "prg" / f"bank_{bank_number:02d}.bin"
        bank_path.write_bytes(prg_rom[index:index + PRG_BANK_SIZE])

    if chr_rom:
        for index in range(0, len(chr_rom), CHR_BANK_SIZE):
            bank_number = index // CHR_BANK_SIZE
            bank_path = dump_dir / "chr" / f"bank_{bank_number:02d}.bin"
            bank_path.write_bytes(chr_rom[index:index + CHR_BANK_SIZE])


def score_room_window(window: bytes) -> float:
    unique_values = len(set(window))
    ff_count = window.count(0xFF)
    zero_count = window.count(0x00)
    return unique_values - (ff_count * 0.18) - (zero_count * 0.08)


def find_room_candidates(prg_rom: bytes, limit: int = 6) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    for offset in range(0, len(prg_rom) - ROOM_TILE_COUNT, 0x10):
        window = prg_rom[offset:offset + ROOM_TILE_COUNT]
        score = score_room_window(window)
        candidates.append(
            {
                "bank": offset // PRG_BANK_SIZE,
                "offset": offset,
                "offset_in_bank": offset % PRG_BANK_SIZE,
                "score": round(score, 3),
                "unique_values": len(set(window)),
                "ff_count": window.count(0xFF),
                "zero_count": window.count(0x00),
                "tile_preview": list(window[:16]),
            }
        )

    candidates.sort(key=lambda item: item["score"], reverse=True)
    return candidates[:limit]


def decode_text_preview(raw_bytes: bytes) -> str:
    preview = []
    for value in raw_bytes[:24]:
        if 0 <= value <= 25:
            preview.append(chr(ord("A") + value))
        elif 26 <= value <= 35:
            preview.append(chr(ord("0") + value - 26))
        elif value in {0x24, 0x25}:
            preview.append(" ")
        else:
            preview.append(".")
    return "".join(preview)


def find_text_candidates(prg_rom: bytes, limit: int = 20) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    offset = 0
    while offset < len(prg_rom) - 8:
        if prg_rom[offset] in {0x00, 0xFF}:
            offset += 1
            continue

        end = offset
        while end < len(prg_rom) and prg_rom[end] not in {0x00, 0xFF} and end - offset < 48:
            end += 1

        run = prg_rom[offset:end]
        if 8 <= len(run) <= 40 and sum(1 for byte in run if byte <= 0x3F) / len(run) > 0.85:
            candidates.append(
                {
                    "bank": offset // PRG_BANK_SIZE,
                    "offset": offset,
                    "offset_in_bank": offset % PRG_BANK_SIZE,
                    "length": len(run),
                    "hex": run.hex(),
                    "preview_guess": decode_text_preview(run),
                }
            )
        offset = end + 1

    candidates.sort(key=lambda item: item["length"], reverse=True)
    return candidates[:limit]


def find_entity_candidates(prg_rom: bytes, limit: int = 20) -> list[dict[str, Any]]:
    candidates: list[dict[str, Any]] = []
    for offset in range(0, len(prg_rom) - 24, 3):
        block = prg_rom[offset:offset + 24]
        triples = [tuple(block[index:index + 3]) for index in range(0, len(block), 3)]
        plausible = all(x <= 0xF0 and y <= 0xF0 and kind <= 0x7F for x, y, kind in triples)
        diversity = len({kind for _, _, kind in triples})
        if plausible and diversity >= 3:
            candidates.append(
                {
                    "bank": offset // PRG_BANK_SIZE,
                    "offset": offset,
                    "offset_in_bank": offset % PRG_BANK_SIZE,
                    "triples": [list(entry) for entry in triples[:6]],
                    "score": diversity,
                }
            )

    unique_keys = set()
    filtered: list[dict[str, Any]] = []
    for candidate in sorted(candidates, key=lambda item: item["score"], reverse=True):
        key = (candidate["bank"], candidate["offset"] // 0x10)
        if key in unique_keys:
            continue
        unique_keys.add(key)
        filtered.append(candidate)
        if len(filtered) == limit:
            break
    return filtered


def select_entity_candidate(room_candidate: dict[str, Any], entity_candidates: list[dict[str, Any]]) -> dict[str, Any] | None:
    same_bank = [candidate for candidate in entity_candidates if candidate["bank"] == room_candidate["bank"]]
    if not same_bank:
        return entity_candidates[0] if entity_candidates else None
    return min(same_bank, key=lambda candidate: abs(candidate["offset"] - room_candidate["offset"]))


def expand_palette_groups(window: bytes) -> list[list[tuple[int, int, int]]]:
    groups: list[list[tuple[int, int, int]]] = []
    for palette_index in range(4):
        group = []
        for color_index in range(4):
            sample = window[(palette_index * 13 + color_index * 7) % len(window)]
            group.append(NES_PALETTE[(sample >> 4) & 0x0F])
        groups.append(group)
    return groups


def expand_tile_palettes(window: bytes) -> list[int]:
    return [((value >> 4) & 0x03) for value in window]


def build_room_entities(entity_candidate: dict[str, Any] | None) -> list[str]:
    if not entity_candidate:
        return ["64,64,candidate_spawn,0,0,0,0"]

    entities = []
    for x, y, kind in entity_candidate["triples"]:
        tile = kind
        palette = (kind >> 5) & 0x03
        entities.append(f"{x},{y},enemy_{kind:02X},{tile},{palette},0,0")
    return entities


def build_bank_report(prg_rom: bytes, room_candidates: list[dict[str, Any]], entity_candidates: list[dict[str, Any]], text_candidates: list[dict[str, Any]]) -> dict[str, Any]:
    bank = 6
    bank_start = bank * PRG_BANK_SIZE
    bank_end = bank_start + PRG_BANK_SIZE
    return {
        "bank": bank,
        "size": PRG_BANK_SIZE,
        "leading_bytes": list(prg_rom[bank_start:bank_start + 32]),
        "room_candidates": [candidate for candidate in room_candidates if candidate["bank"] == bank],
        "entity_candidates": [candidate for candidate in entity_candidates if candidate["bank"] == bank][:6],
        "text_candidates": [candidate for candidate in text_candidates if bank_start <= candidate["offset"] < bank_end][:10],
        "notes": [
            "Bank 6 currently dominates the strongest room-window heuristics for this ROM image.",
            "This is still correlation, not proof of the original room table structure.",
        ],
    }


def make_room_palette(window: bytes) -> list[tuple[int, int, int]]:
    buckets = sorted(set(byte >> 4 for byte in window))[:4]
    while len(buckets) < 4:
        buckets.append(len(buckets))
    return [NES_PALETTE[bucket % len(NES_PALETTE)] for bucket in buckets[:4]]


def write_room_file(path: Path, candidate: dict[str, Any], prg_rom: bytes, entity_candidate: dict[str, Any] | None) -> None:
    offset = candidate["offset"]
    window = prg_rom[offset:offset + ROOM_TILE_COUNT]
    palette_groups = expand_palette_groups(window)
    tile_rows = []
    tile_palette_rows = []
    tile_palettes = expand_tile_palettes(window)
    for row in range(ROOM_HEIGHT):
        row_bytes = window[row * ROOM_WIDTH:(row + 1) * ROOM_WIDTH]
        row_values = [str(byte) for byte in row_bytes]
        tile_rows.append(" ".join(row_values))
        row_palettes = tile_palettes[row * ROOM_WIDTH:(row + 1) * ROOM_WIDTH]
        tile_palette_rows.append(" ".join(str(value) for value in row_palettes))

    player_x = ROOM_WIDTH * 8 - 8
    player_y = ROOM_HEIGHT * 8 - 8
    entities = build_room_entities(entity_candidate)
    content = [
        "# Heuristic room candidate generated from PRG ROM bytes",
        f"name=PRG bank {candidate['bank']:02d} candidate at 0x{candidate['offset_in_bank']:04X}",
        f"width={ROOM_WIDTH}",
        f"height={ROOM_HEIGHT}",
        "tile_size=16",
        f"player_start={player_x},{player_y}",
        "palette=" + ";".join("|".join(f"{r},{g},{b}" for r, g, b in group) for group in palette_groups),
        "tiles:",
        *tile_rows,
        "tile_palettes:",
        *tile_palette_rows,
        "entities:",
        *entities,
        "",
    ]
    path.write_text("\n".join(content), encoding="utf-8")


def write_analysis(dump_dir: Path, prg_rom: bytes) -> None:
    analysis_dir = dump_dir / "analysis"
    rooms_dir = dump_dir / "rooms"
    analysis_dir.mkdir(exist_ok=True)
    rooms_dir.mkdir(exist_ok=True)

    room_candidates = find_room_candidates(prg_rom)
    text_candidates = find_text_candidates(prg_rom)
    entity_candidates = find_entity_candidates(prg_rom)

    analysis = {
        "room_candidates": room_candidates,
        "text_candidates": text_candidates,
        "entity_candidates": entity_candidates,
        "notes": [
            "These candidates are heuristic and intended to accelerate reverse-engineering, not to claim the original game data format is decoded.",
            "The prototype room file is generated from the strongest 16x15 byte window found in PRG ROM.",
        ],
    }

    (analysis_dir / "summary.json").write_text(json.dumps(analysis, indent=2), encoding="utf-8")
    (analysis_dir / "room_candidates.json").write_text(json.dumps(room_candidates, indent=2), encoding="utf-8")
    (analysis_dir / "text_candidates.json").write_text(json.dumps(text_candidates, indent=2), encoding="utf-8")
    (analysis_dir / "entity_candidates.json").write_text(json.dumps(entity_candidates, indent=2), encoding="utf-8")
    (analysis_dir / "bank_06_report.json").write_text(json.dumps(build_bank_report(prg_rom, room_candidates, entity_candidates, text_candidates), indent=2), encoding="utf-8")

    if room_candidates:
        linked_entities = select_entity_candidate(room_candidates[0], entity_candidates)
        write_room_file(rooms_dir / "prototype_overworld.room", room_candidates[0], prg_rom, linked_entities)


def print_human_readable(rom_info: RomInfo) -> None:
    print(f"ROM: {rom_info.path}")
    print(f"Size: {rom_info.size_bytes} bytes")
    print(f"SHA-256: {rom_info.sha256}")
    print(f"Format: {rom_info.ines_version}")
    print(f"Mapper: {rom_info.mapper} (submapper {rom_info.submapper})")
    print(f"Mirroring: {rom_info.mirroring}")
    print(f"Battery-backed RAM: {rom_info.has_battery}")
    print(f"Trainer present: {rom_info.has_trainer}")
    print(f"Four-screen mirroring: {rom_info.four_screen}")
    print(f"PRG ROM: {rom_info.prg_rom_banks_16kb} x 16 KB = {rom_info.prg_rom_size_bytes} bytes")
    print(f"CHR ROM: {rom_info.chr_rom_banks_8kb} x 8 KB = {rom_info.chr_rom_size_bytes} bytes")
    print(f"PRG RAM: {rom_info.prg_ram_size_bytes} bytes")
    print(f"CHR RAM: {rom_info.chr_ram_size_bytes} bytes")
    print(f"NMI vector: 0x{rom_info.nmi_vector:04X}")
    print(f"RESET vector: 0x{rom_info.reset_vector:04X}")
    print(f"IRQ/BRK vector: 0x{rom_info.irq_vector:04X}")


def main() -> int:
    args = parse_args()
    rom_bytes = args.rom.read_bytes()
    rom_info, prg_rom, chr_rom = parse_ines(rom_bytes, args.rom)

    if args.dump_dir:
        dump_banks(args.dump_dir, prg_rom, chr_rom)
        write_analysis(args.dump_dir, prg_rom)

    if args.json:
        print(json.dumps(asdict(rom_info), indent=2))
    else:
        print_human_readable(rom_info)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())