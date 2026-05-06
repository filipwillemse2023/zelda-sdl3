#pragma once

#include <SDL3/SDL.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

struct RoomEntity {
    int x = 0;
    int y = 0;
    std::string tag;
    std::uint8_t tile = 0;
    std::uint8_t palette = 0;
    bool flipHorizontal = false;
    bool flipVertical = false;
};

struct RoomDoorTrigger {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    std::filesystem::path targetRoom;
    int spawnX = -1;
    int spawnY = -1;
};

struct RoomAsset {
    bool loadFromFile(const std::filesystem::path& path);
    bool isBlockedTile(int tileX, int tileY) const;

    std::string name;
    std::filesystem::path sourcePath;
    std::filesystem::path chrPath;
    int width = 16;
    int height = 15;
    int tileSize = 16;
    int playerStartX = 96;
    int playerStartY = 96;
    int spriteHeight = 8;
    int bgPatternBase = 0;
    int spritePatternBase = 0;
    std::array<SDL_Color, 32> palette {};
    std::vector<std::uint8_t> tiles;
    std::vector<std::uint8_t> tilePalettes;
    std::vector<std::uint8_t> collision;
    std::vector<std::uint8_t> chrData;
    std::vector<RoomEntity> entities;
    std::vector<RoomDoorTrigger> doorTriggers;
    std::filesystem::path northRoom;
    std::filesystem::path southRoom;
    std::filesystem::path westRoom;
    std::filesystem::path eastRoom;
    int entryNorthX = -1;
    int entryNorthY = -1;
    int entrySouthX = -1;
    int entrySouthY = -1;
    int entryWestX = -1;
    int entryWestY = -1;
    int entryEastX = -1;
    int entryEastY = -1;
};