#include "game/RoomAsset.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {
enum class Section {
    None,
    Tiles,
    TilePalettes,
    Collision,
    Doors,
    DoorTiles,
    Entities,
};

std::string trim(const std::string& value)
{
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }

    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::vector<std::string> split(const std::string& value, char delimiter)
{
    std::vector<std::string> parts;
    std::stringstream stream(value);
    std::string item;
    while (std::getline(stream, item, delimiter)) {
        parts.push_back(trim(item));
    }
    return parts;
}

bool parseBoolean(const std::string& value)
{
    return value == "1" || value == "true" || value == "True" || value == "TRUE";
}

std::array<SDL_Color, 32> makeDefaultPalette()
{
    return {
        SDL_Color {18, 84, 92, 255},
        SDL_Color {16, 32, 24, 255},
        SDL_Color {224, 208, 112, 255},
        SDL_Color {88, 144, 80, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {24, 44, 92, 255},
        SDL_Color {120, 160, 196, 255},
        SDL_Color {216, 228, 240, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {72, 52, 24, 255},
        SDL_Color {164, 120, 52, 255},
        SDL_Color {236, 216, 136, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {48, 32, 80, 255},
        SDL_Color {140, 92, 164, 255},
        SDL_Color {224, 192, 232, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {32, 20, 12, 255},
        SDL_Color {160, 84, 52, 255},
        SDL_Color {240, 176, 112, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {24, 32, 52, 255},
        SDL_Color {96, 124, 192, 255},
        SDL_Color {196, 212, 252, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {60, 16, 40, 255},
        SDL_Color {176, 64, 120, 255},
        SDL_Color {248, 164, 208, 255},
        SDL_Color {18, 84, 92, 255},
        SDL_Color {40, 40, 40, 255},
        SDL_Color {164, 164, 164, 255},
        SDL_Color {248, 248, 248, 255},
    };
}

void parsePalette(std::array<SDL_Color, 32>& palette, const std::string& value)
{
    const auto groups = split(value, ';');
    if (groups.size() == 1) {
        const auto colors = split(value, '|');
        const std::size_t limit = std::min(colors.size(), static_cast<std::size_t>(4));
        for (std::size_t index = 0; index < limit; ++index) {
            const auto channels = split(colors[index], ',');
            if (channels.size() == 3) {
                const SDL_Color color {
                    static_cast<std::uint8_t>(std::stoi(channels[0])),
                    static_cast<std::uint8_t>(std::stoi(channels[1])),
                    static_cast<std::uint8_t>(std::stoi(channels[2])),
                    255,
                };
                for (std::size_t group = 0; group < 8; ++group) {
                    palette[group * 4 + index] = color;
                }
            }
        }
        return;
    }

    for (std::size_t groupIndex = 0; groupIndex < std::min(groups.size(), static_cast<std::size_t>(8)); ++groupIndex) {
        const auto colors = split(groups[groupIndex], '|');
        for (std::size_t colorIndex = 0; colorIndex < std::min(colors.size(), static_cast<std::size_t>(4)); ++colorIndex) {
            const auto channels = split(colors[colorIndex], ',');
            if (channels.size() == 3) {
                palette[groupIndex * 4 + colorIndex] = SDL_Color {
                    static_cast<std::uint8_t>(std::stoi(channels[0])),
                    static_cast<std::uint8_t>(std::stoi(channels[1])),
                    static_cast<std::uint8_t>(std::stoi(channels[2])),
                    255,
                };
            }
        }
    }
}
}

bool RoomAsset::loadFromFile(const std::filesystem::path& path)
{
    std::ifstream input(path);
    if (!input) {
        return false;
    }

    name.clear();
    sourcePath = path;
    chrPath.clear();
    width = 16;
    height = 15;
    tileSize = 16;
    playerStartX = 96;
    playerStartY = 96;
    spriteHeight = 8;
    bgPatternBase = 0;
    spritePatternBase = 0;
    palette = makeDefaultPalette();
    tiles.clear();
    tilePalettes.clear();
    collision.clear();
    chrData.clear();
    entities.clear();
    doorTriggers.clear();
    northRoom.clear();
    southRoom.clear();
    westRoom.clear();
    eastRoom.clear();
    entryNorthX = -1;
    entryNorthY = -1;
    entrySouthX = -1;
    entrySouthY = -1;
    entryWestX = -1;
    entryWestY = -1;
    entryEastX = -1;
    entryEastY = -1;

    std::unordered_set<int> solidTileIds;

    Section section = Section::None;
    std::string line;
    while (std::getline(input, line)) {
        const std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        if (trimmed == "tiles:") {
            section = Section::Tiles;
            continue;
        }

        if (trimmed == "tile_palettes:") {
            section = Section::TilePalettes;
            continue;
        }

        if (trimmed == "collision:") {
            section = Section::Collision;
            continue;
        }

        if (trimmed == "entities:") {
            section = Section::Entities;
            continue;
        }

        if (trimmed == "doors:") {
            section = Section::Doors;
            continue;
        }

        if (trimmed == "door_tiles:") {
            section = Section::DoorTiles;
            continue;
        }

        if (section == Section::Tiles || section == Section::TilePalettes || section == Section::Collision) {
            const auto entries = split(trimmed, ' ');
            for (const std::string& entry : entries) {
                if (!entry.empty()) {
                    if (section == Section::Tiles) {
                        tiles.push_back(static_cast<std::uint8_t>(std::stoi(entry)));
                    } else if (section == Section::TilePalettes) {
                        tilePalettes.push_back(static_cast<std::uint8_t>(std::stoi(entry)));
                    } else {
                        collision.push_back(static_cast<std::uint8_t>(parseBoolean(entry) ? 1 : 0));
                    }
                }
            }
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos) {
            if (section == Section::Entities) {
                const auto entityParts = split(trimmed, ',');
                if (entityParts.size() >= 3) {
                    RoomEntity entity;
                    entity.x = std::stoi(entityParts[0]);
                    entity.y = std::stoi(entityParts[1]);
                    entity.tag = entityParts[2];
                    if (entityParts.size() >= 7) {
                        entity.tile = static_cast<std::uint8_t>(std::stoi(entityParts[3]));
                        entity.palette = static_cast<std::uint8_t>(std::stoi(entityParts[4]));
                        entity.flipHorizontal = parseBoolean(entityParts[5]);
                        entity.flipVertical = parseBoolean(entityParts[6]);
                    }
                    entities.push_back(entity);
                }
            } else if (section == Section::Doors) {
                const auto doorParts = split(trimmed, ',');
                if (doorParts.size() >= 5) {
                    RoomDoorTrigger door;
                    door.x = std::stoi(doorParts[0]);
                    door.y = std::stoi(doorParts[1]);
                    door.w = std::stoi(doorParts[2]);
                    door.h = std::stoi(doorParts[3]);
                    door.targetRoom = doorParts[4];
                    if (doorParts.size() >= 7) {
                        door.spawnX = std::stoi(doorParts[5]);
                        door.spawnY = std::stoi(doorParts[6]);
                    }
                    doorTriggers.push_back(door);
                }
            } else if (section == Section::DoorTiles) {
                const auto doorParts = split(trimmed, ',');
                if (doorParts.size() >= 5) {
                    const int tileX = std::stoi(doorParts[0]);
                    const int tileY = std::stoi(doorParts[1]);

                    RoomDoorTrigger door;
                    door.x = tileX * tileSize;
                    door.y = tileY * tileSize;
                    door.w = tileSize;
                    door.h = tileSize;
                    door.targetRoom = doorParts[2];
                    door.spawnX = std::stoi(doorParts[3]);
                    door.spawnY = std::stoi(doorParts[4]);
                    doorTriggers.push_back(door);
                }
            }
            continue;
        }

        const std::string key = trimmed.substr(0, separator);
        const std::string value = trim(trimmed.substr(separator + 1));

        if (key == "name") {
            name = value;
        } else if (key == "width") {
            width = std::stoi(value);
        } else if (key == "height") {
            height = std::stoi(value);
        } else if (key == "tile_size") {
            tileSize = std::stoi(value);
        } else if (key == "player_start") {
            const auto parts = split(value, ',');
            if (parts.size() == 2) {
                playerStartX = std::stoi(parts[0]);
                playerStartY = std::stoi(parts[1]);
            }
        } else if (key == "sprite_height") {
            spriteHeight = std::stoi(value);
        } else if (key == "bg_pattern_base") {
            bgPatternBase = std::stoi(value);
        } else if (key == "sprite_pattern_base") {
            spritePatternBase = std::stoi(value);
        } else if (key == "palette") {
            parsePalette(palette, value);
        } else if (key == "chr_path") {
            chrPath = value;
        } else if (key == "solid_tiles") {
            const auto parts = split(value, ',');
            for (const std::string& part : parts) {
                if (!part.empty()) {
                    solidTileIds.insert(std::stoi(part));
                }
            }
        } else if (key == "north_room") {
            northRoom = value;
        } else if (key == "south_room") {
            southRoom = value;
        } else if (key == "west_room") {
            westRoom = value;
        } else if (key == "east_room") {
            eastRoom = value;
        } else if (key == "entry_north") {
            const auto parts = split(value, ',');
            if (parts.size() == 2) {
                entryNorthX = std::stoi(parts[0]);
                entryNorthY = std::stoi(parts[1]);
            }
        } else if (key == "entry_south") {
            const auto parts = split(value, ',');
            if (parts.size() == 2) {
                entrySouthX = std::stoi(parts[0]);
                entrySouthY = std::stoi(parts[1]);
            }
        } else if (key == "entry_west") {
            const auto parts = split(value, ',');
            if (parts.size() == 2) {
                entryWestX = std::stoi(parts[0]);
                entryWestY = std::stoi(parts[1]);
            }
        } else if (key == "entry_east") {
            const auto parts = split(value, ',');
            if (parts.size() == 2) {
                entryEastX = std::stoi(parts[0]);
                entryEastY = std::stoi(parts[1]);
            }
        }
    }

    if (tilePalettes.empty()) {
        tilePalettes.assign(static_cast<std::size_t>(width * height), 0);
    }

    if (collision.empty()) {
        collision.assign(static_cast<std::size_t>(width * height), 0);
        if (!solidTileIds.empty()) {
            for (std::size_t index = 0; index < tiles.size() && index < collision.size(); ++index) {
                collision[index] = solidTileIds.contains(static_cast<int>(tiles[index])) ? 1 : 0;
            }
        }
    }

    if (!chrPath.empty()) {
        std::ifstream chrInput(path.parent_path() / chrPath, std::ios::binary);
        if (chrInput) {
            chrData.assign(std::istreambuf_iterator<char>(chrInput), std::istreambuf_iterator<char>());
        }
    }

    return tiles.size() == static_cast<std::size_t>(width * height)
        && tilePalettes.size() == static_cast<std::size_t>(width * height)
        && collision.size() == static_cast<std::size_t>(width * height);
}

bool RoomAsset::isBlockedTile(int tileX, int tileY) const
{
    if (tileX < 0 || tileY < 0 || tileX >= width || tileY >= height) {
        return true;
    }

    const std::size_t index = static_cast<std::size_t>(tileY * width + tileX);
    if (index >= collision.size()) {
        return false;
    }
    return collision[index] != 0;
}