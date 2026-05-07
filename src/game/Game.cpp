#include "game/Game.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace {
constexpr float kLogicalWidth = 256.0F;
constexpr float kLogicalHeight = 240.0F;
constexpr float kScale = 3.0F;
constexpr int kHudRows = 8;
constexpr float kPlayerCollisionW = 8.0F;
constexpr float kPlayerCollisionH = 8.0F;

enum class EdgeKind {
    North,
    South,
    West,
    East,
};

std::filesystem::path chooseRoomPath()
{
    const std::filesystem::path roomsDirectory = std::filesystem::path("extracted") / "rooms";
    if (!std::filesystem::exists(roomsDirectory)) {
        return roomsDirectory / "prototype_overworld.room";
    }

    for (const auto& entry : std::filesystem::directory_iterator(roomsDirectory)) {
        if (!entry.is_regular_file() || entry.path().extension() != ".room") {
            continue;
        }

        const std::filesystem::path chrPath = entry.path().parent_path() / (entry.path().stem().string() + ".chr.bin");
        if (std::filesystem::exists(chrPath)) {
            return entry.path();
        }
    }

    return roomsDirectory / "prototype_overworld.room";
}

std::filesystem::path chooseRoomPath(const std::filesystem::path& preferredRoomPath)
{
    if (!preferredRoomPath.empty() && std::filesystem::exists(preferredRoomPath)) {
        return preferredRoomPath;
    }
    return chooseRoomPath();
}

SDL_Color lookupTileColor(const RoomAsset& room, std::uint8_t paletteIndex, std::uint8_t colorIndex)
{
    const std::size_t base = static_cast<std::size_t>((paletteIndex % 8) * 4);
    return room.palette[base + (colorIndex % 4)];
}

void renderPatternTile(
    SDL_Renderer* renderer,
    const RoomAsset& room,
    int tileIndex,
    std::uint8_t paletteIndex,
    float x,
    float y,
    float tileSize,
    bool flipHorizontal = false,
    bool flipVertical = false,
    bool transparentZero = false)
{
    const std::size_t start = static_cast<std::size_t>(tileIndex) * 16;
    if (start + 16 > room.chrData.size()) {
        const SDL_Color fallback = lookupTileColor(room, paletteIndex, static_cast<std::uint8_t>(tileIndex & 0x03));
        SDL_SetRenderDrawColor(renderer, fallback.r, fallback.g, fallback.b, 255);
        SDL_FRect tileRect {x, y, tileSize, tileSize};
        SDL_RenderFillRect(renderer, &tileRect);
        return;
    }

    const float pixelSize = tileSize / 8.0F;
    for (int row = 0; row < 8; ++row) {
        const std::uint8_t plane0 = room.chrData[start + row];
        const std::uint8_t plane1 = room.chrData[start + row + 8];
        for (int column = 0; column < 8; ++column) {
            const int shift = 7 - column;
            const std::uint8_t colorIndex = static_cast<std::uint8_t>(((plane0 >> shift) & 0x01) | (((plane1 >> shift) & 0x01) << 1));
            if (transparentZero && colorIndex == 0) {
                continue;
            }

            const int drawColumn = flipHorizontal ? (7 - column) : column;
            const int drawRow = flipVertical ? (7 - row) : row;
            const SDL_Color color = lookupTileColor(room, paletteIndex, colorIndex);
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
            SDL_FRect pixelRect {x + drawColumn * pixelSize, y + drawRow * pixelSize, pixelSize, pixelSize};
            SDL_RenderFillRect(renderer, &pixelRect);
        }
    }
}

bool overlapsRange(float aStart, float aEnd, float bStart, float bEnd)
{
    return aStart < bEnd && aEnd > bStart;
}

void drawGlyph(SDL_Renderer* renderer, char glyph, float x, float y, float scale, SDL_Color color)
{
    const char* rows[5] = {"000", "000", "000", "000", "000"};
    switch (glyph) {
    case 'A':
        rows[0] = "111"; rows[1] = "101"; rows[2] = "111"; rows[3] = "101"; rows[4] = "101";
        break;
    case 'B':
        rows[0] = "110"; rows[1] = "101"; rows[2] = "110"; rows[3] = "101"; rows[4] = "110";
        break;
    case 'D':
        rows[0] = "110"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "110";
        break;
    case 'E':
        rows[0] = "111"; rows[1] = "100"; rows[2] = "110"; rows[3] = "100"; rows[4] = "111";
        break;
    case 'F':
        rows[0] = "111"; rows[1] = "100"; rows[2] = "110"; rows[3] = "100"; rows[4] = "100";
        break;
    case 'G':
        rows[0] = "111"; rows[1] = "100"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111";
        break;
    case 'N':
        rows[0] = "101"; rows[1] = "111"; rows[2] = "111"; rows[3] = "111"; rows[4] = "101";
        break;
    case 'O':
        rows[0] = "111"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111";
        break;
    case 'U':
        rows[0] = "101"; rows[1] = "101"; rows[2] = "101"; rows[3] = "101"; rows[4] = "111";
        break;
    default:
        break;
    }

    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (int row = 0; row < 5; ++row) {
        for (int column = 0; column < 3; ++column) {
            if (rows[row][column] != '1') {
                continue;
            }
            SDL_FRect pixel {
                x + static_cast<float>(column) * scale,
                y + static_cast<float>(row) * scale,
                scale,
                scale,
            };
            SDL_RenderFillRect(renderer, &pixel);
        }
    }
}

void drawText(SDL_Renderer* renderer, const char* text, float x, float y, float scale, SDL_Color color)
{
    float cursorX = x;
    for (const char* it = text; *it != '\0'; ++it) {
        if (*it == ' ') {
            cursorX += 4.0F * scale;
            continue;
        }
        drawGlyph(renderer, static_cast<char>(std::toupper(static_cast<unsigned char>(*it))), cursorX, y, scale, color);
        cursorX += 4.0F * scale;
    }
}

SDL_FRect makeCollisionRect(const SDL_FRect& spriteRect)
{
    const float offsetX = std::max(0.0F, (spriteRect.w - kPlayerCollisionW) * 0.5F);
    const float offsetY = std::max(0.0F, spriteRect.h - kPlayerCollisionH);
    return SDL_FRect {
        spriteRect.x + offsetX,
        spriteRect.y + offsetY,
        std::min(kPlayerCollisionW, spriteRect.w),
        std::min(kPlayerCollisionH, spriteRect.h),
    };
}

void renderRoomTileRows(SDL_Renderer* renderer, const RoomAsset& room, int rowStart, int rowEndExclusive, float offsetX, float offsetY)
{
    const int startY = std::max(0, rowStart);
    const int endY = std::min(room.height, rowEndExclusive);
    for (int y = startY; y < endY; ++y) {
        for (int x = 0; x < room.width; ++x) {
            const std::size_t index = static_cast<std::size_t>(y * room.width + x);
            if (index >= room.tiles.size() || index >= room.tilePalettes.size()) {
                continue;
            }
            const std::uint8_t tile = room.tiles[index];
            const int tileWithBase = static_cast<int>(tile) + (room.bgPatternBase / 16);
            const std::uint8_t paletteIndex = room.tilePalettes[index];
            const float drawX = offsetX + static_cast<float>(x * room.tileSize);
            const float drawY = offsetY + static_cast<float>(y * room.tileSize);
            renderPatternTile(renderer, room, tileWithBase, paletteIndex, drawX, drawY, static_cast<float>(room.tileSize));
        }
    }
}

void renderPlayerSpriteForRoom(
    SDL_Renderer* renderer,
    const RoomAsset& room,
    float x,
    float y,
    std::uint8_t topLeft,
    std::uint8_t topRight,
    std::uint8_t bottomLeft,
    std::uint8_t bottomRight,
    int widthTiles,
    int heightTiles,
    std::uint8_t palette,
    bool flipH,
    bool flipV)
{
    if (room.chrData.empty()) {
        return;
    }

    const int baseTile = room.spritePatternBase / 16;
    const int spriteTiles[2][2] = {
        {topLeft, topRight},
        {bottomLeft, bottomRight},
    };

    for (int tileY = 0; tileY < heightTiles; ++tileY) {
        for (int tileX = 0; tileX < widthTiles; ++tileX) {
            const int tileWithBase = spriteTiles[tileY][tileX] + baseTile;
            renderPatternTile(
                renderer,
                room,
                tileWithBase,
                static_cast<std::uint8_t>(palette + 4),
                x + static_cast<float>(tileX * 8),
                y + static_cast<float>(tileY * 8),
                8.0F,
                flipH,
                flipV,
                true);
        }
    }
}

std::vector<SDL_FRect> buildEdgeOpenings(const RoomAsset& room, EdgeKind edge)
{
    std::vector<SDL_FRect> openings;
    const int tileSize = room.tileSize;

    if (edge == EdgeKind::North || edge == EdgeKind::South) {
        auto rowHasOpening = [&](int row) {
            for (int tileX = 0; tileX < room.width; ++tileX) {
                if (!room.isBlockedTile(tileX, row)) {
                    return true;
                }
            }
            return false;
        };

        int edgeY = (edge == EdgeKind::North) ? std::clamp(kHudRows, 0, room.height - 1) : (room.height - 1);
        if (!rowHasOpening(edgeY)) {
            if (edge == EdgeKind::North) {
                for (int probeY = edgeY + 1; probeY < room.height; ++probeY) {
                    if (rowHasOpening(probeY)) {
                        edgeY = probeY;
                        break;
                    }
                }
            } else {
                const int minProbeY = std::clamp(kHudRows, 0, room.height - 1);
                for (int probeY = edgeY - 1; probeY >= minProbeY; --probeY) {
                    if (rowHasOpening(probeY)) {
                        edgeY = probeY;
                        break;
                    }
                }
            }
        }

        int runStart = -1;
        for (int tileX = 0; tileX < room.width; ++tileX) {
            const bool open = !room.isBlockedTile(tileX, edgeY);
            if (open && runStart < 0) {
                runStart = tileX;
            }
            if ((!open || tileX == room.width - 1) && runStart >= 0) {
                const int runEndExclusive = (open && tileX == room.width - 1) ? tileX + 1 : tileX;
                const int pixelY = edgeY * tileSize;
                const float drawHeight = (edge == EdgeKind::North) ? std::max(1.0F, static_cast<float>(tileSize) * 0.5F) : static_cast<float>(tileSize);
                openings.push_back(SDL_FRect {
                    static_cast<float>(runStart * tileSize),
                    static_cast<float>(pixelY),
                    static_cast<float>((runEndExclusive - runStart) * tileSize),
                    drawHeight,
                });
                runStart = -1;
            }
        }
        return openings;
    }

    const int edgeX = edge == EdgeKind::West ? 0 : room.width - 1;
    int runStart = -1;
    const int startTileY = std::clamp(kHudRows, 0, room.height - 1);
    for (int tileY = startTileY; tileY < room.height; ++tileY) {
        const bool open = !room.isBlockedTile(edgeX, tileY);
        if (open && runStart < 0) {
            runStart = tileY;
        }
        if ((!open || tileY == room.height - 1) && runStart >= 0) {
            const int runEndExclusive = (open && tileY == room.height - 1) ? tileY + 1 : tileY;
            const int pixelX = (edge == EdgeKind::West) ? 0 : (room.width - 1) * tileSize;
            openings.push_back(SDL_FRect {
                static_cast<float>(pixelX),
                static_cast<float>(runStart * tileSize),
                static_cast<float>(tileSize),
                static_cast<float>((runEndExclusive - runStart) * tileSize),
            });
            runStart = -1;
        }
    }
    return openings;
}

SDL_FRect normalizeDoorRect(const RoomDoorTrigger& door)
{
    return SDL_FRect {
        static_cast<float>(door.x),
        static_cast<float>(door.y),
        std::max(1.0F, static_cast<float>(door.w)),
        std::max(1.0F, static_cast<float>(door.h)),
    };
}
}

void Game::initialize(const std::filesystem::path& preferredRoomPath)
{
    const std::filesystem::path roomPath = chooseRoomPath(preferredRoomPath);
    loadRoom(roomPath);
}

void Game::handleEvent(const SDL_Event& event)
{
    if (event.type != SDL_EVENT_KEY_DOWN && event.type != SDL_EVENT_KEY_UP) {
        return;
    }

    const bool pressed = event.type == SDL_EVENT_KEY_DOWN;
    switch (event.key.key) {
    case SDLK_W:
    case SDLK_UP:
        moveUp_ = pressed;
        break;
    case SDLK_S:
    case SDLK_DOWN:
        moveDown_ = pressed;
        break;
    case SDLK_A:
    case SDLK_LEFT:
        moveLeft_ = pressed;
        break;
    case SDLK_D:
    case SDLK_RIGHT:
        moveRight_ = pressed;
        break;
    case SDLK_F3:
        if (pressed && !event.key.repeat) {
            showDebugOverlays_ = !showDebugOverlays_;
        }
        break;
    default:
        break;
    }
}

void Game::update(double deltaSeconds)
{
    if (screenTransitionActive_) {
        screenTransitionPixels_ += static_cast<float>(deltaSeconds) * 220.0F;
        if (screenTransitionPixels_ >= screenTransitionDistance_) {
            screenTransitionActive_ = false;
            screenTransitionPixels_ = 0.0F;
        }
        return;
    }

    transitionCooldown_ = std::max(0.0F, transitionCooldown_ - static_cast<float>(deltaSeconds));
    linkHideTimer_ = std::max(0.0F, linkHideTimer_ - static_cast<float>(deltaSeconds));

    const float distance = static_cast<float>(deltaSeconds) * speed_;
    float dx = 0.0F;
    float dy = 0.0F;

    if (moveUp_) {
        dy -= distance;
    }
    if (moveDown_) {
        dy += distance;
    }
    if (moveLeft_) {
        dx -= distance;
    }
    if (moveRight_) {
        dx += distance;
    }

    moveWithCollision(dx, dy);

    if (!tryDoorTransition() && !tryRoomTransition()) {
        const SDL_FRect collisionRect = makeCollisionRect(link_);
        const float minX = -std::max(0.0F, (link_.w - kPlayerCollisionW) * 0.5F);
        const float minY = -std::max(0.0F, link_.h - kPlayerCollisionH);
        const float maxX = roomPixelWidth() - (collisionRect.x - link_.x + collisionRect.w);
        const float maxY = roomPixelHeight() - (collisionRect.y - link_.y + collisionRect.h);
        link_.x = std::clamp(link_.x, minX, maxX);
        link_.y = std::clamp(link_.y, minY, maxY);
    }
}

void Game::render(SDL_Renderer* renderer) const
{
    SDL_SetRenderScale(renderer, kScale, kScale);

    if (screenTransitionActive_) {
        const float distance = std::max(1.0F, screenTransitionDistance_);
        const float offset = std::min(screenTransitionPixels_, distance);
        const int hudPixelHeight = kHudRows * transitionFromRoom_.tileSize;
        const float playfieldHeight = static_cast<float>(transitionFromRoom_.height * transitionFromRoom_.tileSize - hudPixelHeight);

        const SDL_Color background = transitionFromRoom_.palette[0];
        SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, 255);
        SDL_FRect playfield {0.0F, 0.0F, roomPixelWidth(), roomPixelHeight()};
        SDL_RenderFillRect(renderer, &playfield);

        float fromOffsetX = 0.0F;
        float toOffsetX = 0.0F;
        float fromOffsetY = 0.0F;
        float toOffsetY = 0.0F;
        if (screenTransitionEdge_ == TransitionEdge::North || screenTransitionEdge_ == TransitionEdge::South) {
            fromOffsetY = (screenTransitionEdge_ == TransitionEdge::North) ? offset : -offset;
            toOffsetY = (screenTransitionEdge_ == TransitionEdge::North) ? -(playfieldHeight - offset) : (playfieldHeight - offset);
        } else {
            fromOffsetX = (screenTransitionEdge_ == TransitionEdge::West) ? offset : -offset;
            toOffsetX = (screenTransitionEdge_ == TransitionEdge::West) ? -(roomPixelWidth() - offset) : (roomPixelWidth() - offset);
        }

        const SDL_Rect clipRect {
            0,
            static_cast<int>(std::round(static_cast<float>(hudPixelHeight))),
            static_cast<int>(std::round(roomPixelWidth())),
            static_cast<int>(std::round(roomPixelHeight() - static_cast<float>(hudPixelHeight))),
        };
        SDL_SetRenderClipRect(renderer, &clipRect);
        renderRoomTileRows(renderer, transitionFromRoom_, kHudRows, transitionFromRoom_.height, fromOffsetX, fromOffsetY);
        renderRoomTileRows(renderer, transitionToRoom_, kHudRows, transitionToRoom_.height, toOffsetX, toOffsetY);

        // Draw a single interpolated Link sprite during scrolling to avoid a double image.
        const float transitionT = std::clamp(offset / distance, 0.0F, 1.0F);
        const float fromLinkX = transitionFromLink_.x + fromOffsetX;
        const float fromLinkY = transitionFromLink_.y + fromOffsetY;
        const float toLinkX = transitionToLink_.x + toOffsetX;
        const float toLinkY = transitionToLink_.y + toOffsetY;
        const float drawLinkX = fromLinkX + (toLinkX - fromLinkX) * transitionT;
        const float drawLinkY = fromLinkY + (toLinkY - fromLinkY) * transitionT;
        const RoomAsset& linkRoom = transitionToRoom_.chrData.empty() ? transitionFromRoom_ : transitionToRoom_;
        renderPlayerSpriteForRoom(
            renderer,
            linkRoom,
            drawLinkX,
            drawLinkY,
            playerTileTopLeft_,
            playerTileTopRight_,
            playerTileBottomLeft_,
            playerTileBottomRight_,
            playerSpriteWidthTiles_,
            playerSpriteHeightTiles_,
            playerPalette_,
            playerFlipH_,
            playerFlipV_);
        SDL_SetRenderClipRect(renderer, nullptr);

        renderRoomTileRows(renderer, transitionFromRoom_, 0, kHudRows, 0.0F, 0.0F);

        SDL_SetRenderScale(renderer, 1.0F, 1.0F);
        return;
    }

    if (roomLoaded_) {
        const SDL_Color background = room_.palette[0];
        SDL_SetRenderDrawColor(renderer, background.r, background.g, background.b, 255);
        SDL_FRect playfield {0.0F, 0.0F, roomPixelWidth(), roomPixelHeight()};
        SDL_RenderFillRect(renderer, &playfield);

        for (int y = 0; y < room_.height; ++y) {
            for (int x = 0; x < room_.width; ++x) {
                const std::uint8_t tile = room_.tiles[static_cast<std::size_t>(y * room_.width + x)];
                const int tileWithBase = static_cast<int>(tile) + (room_.bgPatternBase / 16);
                const std::uint8_t paletteIndex = room_.tilePalettes[static_cast<std::size_t>(y * room_.width + x)];
                const SDL_FRect tileRect {static_cast<float>(x * room_.tileSize), static_cast<float>(y * room_.tileSize), static_cast<float>(room_.tileSize), static_cast<float>(room_.tileSize)};
                renderPatternTile(renderer, room_, tileWithBase, paletteIndex, tileRect.x, tileRect.y, tileRect.w);
            }
        }

        for (std::size_t entityIndex = 0; entityIndex < room_.entities.size(); ++entityIndex) {
            if (std::find(hiddenEntityIndices_.begin(), hiddenEntityIndices_.end(), entityIndex) != hiddenEntityIndices_.end()) {
                continue;
            }

            const RoomEntity& entity = room_.entities[entityIndex];
            const float entityX = static_cast<float>(entity.x);
            const float entityY = static_cast<float>(entity.y);
            if (!room_.chrData.empty()) {
                const int spriteTileWithBase = static_cast<int>(entity.tile) + (room_.spritePatternBase / 16);
                renderPatternTile(renderer, room_, spriteTileWithBase, static_cast<std::uint8_t>(entity.palette + 4), entityX, entityY, 8.0F, entity.flipHorizontal, entity.flipVertical, true);
            } else {
                SDL_SetRenderDrawColor(renderer, 196, 68, 48, 255);
                SDL_FRect entityRect {entityX, entityY, 8.0F, 8.0F};
                SDL_RenderFillRect(renderer, &entityRect);
            }
        }

        if (showDebugOverlays_) {
            for (const RoomDoorTrigger& door : room_.doorTriggers) {
                const SDL_FRect doorRect = normalizeDoorRect(door);
                SDL_SetRenderDrawColor(renderer, 0, 220, 255, 80);
                SDL_RenderFillRect(renderer, &doorRect);
                SDL_SetRenderDrawColor(renderer, 0, 220, 255, 220);
                SDL_RenderRect(renderer, &doorRect);
            }

            if (!room_.northRoom.empty()) {
                for (const SDL_FRect& rect : buildEdgeOpenings(room_, EdgeKind::North)) {
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 70);
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 200);
                    SDL_RenderRect(renderer, &rect);
                }
            }
            if (!room_.southRoom.empty()) {
                for (const SDL_FRect& rect : buildEdgeOpenings(room_, EdgeKind::South)) {
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 70);
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 200);
                    SDL_RenderRect(renderer, &rect);
                }
            }
            if (!room_.westRoom.empty()) {
                for (const SDL_FRect& rect : buildEdgeOpenings(room_, EdgeKind::West)) {
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 70);
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 200);
                    SDL_RenderRect(renderer, &rect);
                }
            }
            if (!room_.eastRoom.empty()) {
                for (const SDL_FRect& rect : buildEdgeOpenings(room_, EdgeKind::East)) {
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 70);
                    SDL_RenderFillRect(renderer, &rect);
                    SDL_SetRenderDrawColor(renderer, 64, 255, 96, 200);
                    SDL_RenderRect(renderer, &rect);
                }
            }
        }
    } else {
        renderFallback(renderer);
    }

    if (roomLoaded_ && playerUsesSprite_ && !room_.chrData.empty() && linkHideTimer_ <= 0.0F) {
        const int baseTile = room_.spritePatternBase / 16;
        const int spriteTiles[2][2] = {
            {playerTileTopLeft_, playerTileTopRight_},
            {playerTileBottomLeft_, playerTileBottomRight_},
        };

        for (int tileY = 0; tileY < playerSpriteHeightTiles_; ++tileY) {
            for (int tileX = 0; tileX < playerSpriteWidthTiles_; ++tileX) {
                const int tileWithBase = spriteTiles[tileY][tileX] + baseTile;
                renderPatternTile(
                    renderer,
                    room_,
                    tileWithBase,
                    static_cast<std::uint8_t>(playerPalette_ + 4),
                    link_.x + static_cast<float>(tileX * 8),
                    link_.y + static_cast<float>(tileY * 8),
                    8.0F,
                    playerFlipH_,
                    playerFlipV_,
                    true);
            }
        }
    } else if (linkHideTimer_ <= 0.0F) {
        SDL_SetRenderDrawColor(renderer, 224, 208, 112, 255);
        SDL_RenderFillRect(renderer, &link_);

        SDL_SetRenderDrawColor(renderer, 32, 20, 12, 255);
        SDL_FRect swordHint {link_.x + 4.0F, link_.y - 4.0F, 8.0F, 4.0F};
        SDL_RenderFillRect(renderer, &swordHint);
    }

    const SDL_Color labelColor = showDebugOverlays_ ? SDL_Color {64, 255, 96, 255} : SDL_Color {220, 220, 220, 255};
    drawText(renderer, showDebugOverlays_ ? "DEBUG ON" : "DEBUG OFF", 4.0F, 4.0F, 1.5F, labelColor);

    SDL_SetRenderScale(renderer, 1.0F, 1.0F);
}

void Game::renderFallback(SDL_Renderer* renderer) const
{
    SDL_SetRenderDrawColor(renderer, 18, 84, 92, 255);
    SDL_FRect playfield {0.0F, 0.0F, kLogicalWidth, kLogicalHeight};
    SDL_RenderFillRect(renderer, &playfield);

    SDL_SetRenderDrawColor(renderer, 16, 32, 24, 255);
    for (int y = 0; y < 15; ++y) {
        for (int x = 0; x < 16; ++x) {
            if ((x + y) % 2 == 0) {
                SDL_FRect tile {x * 16.0F, y * 16.0F, 16.0F, 16.0F};
                SDL_RenderRect(renderer, &tile);
            }
        }
    }
}

SDL_Point Game::logicalSize() const
{
    return SDL_Point {
        static_cast<int>(roomPixelWidth()),
        static_cast<int>(roomPixelHeight()),
    };
}

std::string Game::currentRoomLabel() const
{
    if (!roomLoaded_) {
        return "(fallback)";
    }
    if (!room_.name.empty()) {
        return room_.name;
    }
    if (!room_.sourcePath.empty()) {
        return room_.sourcePath.filename().string();
    }
    return "(unknown room)";
}

bool Game::loadRoom(const std::filesystem::path& roomPath, TransitionEdge enteredFrom, float preservedAxis)
{
    RoomAsset nextRoom;
    if (!nextRoom.loadFromFile(roomPath)) {
        roomLoaded_ = false;
        return false;
    }

    room_ = std::move(nextRoom);
    roomLoaded_ = true;

    hiddenEntityIndices_.clear();
    if (!playerSpriteLocked_) {
        playerUsesSprite_ = false;
        playerTileTopLeft_ = 0;
        playerTileTopRight_ = 0;
        playerTileBottomLeft_ = 0;
        playerTileBottomRight_ = 0;
        playerSpriteWidthTiles_ = 1;
        playerSpriteHeightTiles_ = 1;
        playerPalette_ = 0;
        playerFlipH_ = false;
        playerFlipV_ = false;
    } else {
        playerUsesSprite_ = true;
    }

    if (!playerSpriteLocked_ && !room_.entities.empty()) {
        std::size_t nearestIndex = 0;
        int bestDistance = std::numeric_limits<int>::max();
        for (std::size_t index = 0; index < room_.entities.size(); ++index) {
            const RoomEntity& entity = room_.entities[index];
            const int dx = entity.x - room_.playerStartX;
            const int dy = entity.y - room_.playerStartY;
            const int distance = dx * dx + dy * dy;
            if (distance < bestDistance) {
                bestDistance = distance;
                nearestIndex = index;
            }
        }

        const RoomEntity& nearest = room_.entities[nearestIndex];
        playerTileTopLeft_ = nearest.tile;
        playerTileTopRight_ = nearest.tile;
        playerTileBottomLeft_ = nearest.tile;
        playerTileBottomRight_ = nearest.tile;
        playerPalette_ = nearest.palette;
        playerFlipH_ = nearest.flipHorizontal;
        playerFlipV_ = nearest.flipVertical;
        playerUsesSprite_ = true;

        int minX = nearest.x;
        int minY = nearest.y;
        for (const RoomEntity& entity : room_.entities) {
            if (entity.palette != nearest.palette || entity.flipHorizontal != nearest.flipHorizontal || entity.flipVertical != nearest.flipVertical) {
                continue;
            }
            const int dx = entity.x - nearest.x;
            const int dy = entity.y - nearest.y;
            if (dx < -8 || dx > 8 || dy < -8 || dy > 8) {
                continue;
            }
            if ((dx % 8) != 0 || (dy % 8) != 0) {
                continue;
            }
            minX = std::min(minX, entity.x);
            minY = std::min(minY, entity.y);
        }

        std::map<std::pair<int, int>, std::size_t> indexByLocalOffset;
        for (std::size_t index = 0; index < room_.entities.size(); ++index) {
            const RoomEntity& entity = room_.entities[index];
            if (entity.palette != nearest.palette || entity.flipHorizontal != nearest.flipHorizontal || entity.flipVertical != nearest.flipVertical) {
                continue;
            }
            const int localX = entity.x - minX;
            const int localY = entity.y - minY;
            if ((localX == 0 || localX == 8) && (localY == 0 || localY == 8)) {
                indexByLocalOffset[std::make_pair(localX, localY)] = index;
            }
        }

        const auto addHidden = [this](std::size_t index) {
            if (std::find(hiddenEntityIndices_.begin(), hiddenEntityIndices_.end(), index) == hiddenEntityIndices_.end()) {
                hiddenEntityIndices_.push_back(index);
            }
        };

        const auto useTileIfPresent = [&](int localX, int localY, std::uint8_t& tileOut) {
            const auto it = indexByLocalOffset.find(std::make_pair(localX, localY));
            if (it == indexByLocalOffset.end()) {
                return false;
            }
            tileOut = room_.entities[it->second].tile;
            addHidden(it->second);
            return true;
        };

        const bool hasTopLeft = useTileIfPresent(0, 0, playerTileTopLeft_);
        const bool hasTopRight = useTileIfPresent(8, 0, playerTileTopRight_);
        const bool hasBottomLeft = useTileIfPresent(0, 8, playerTileBottomLeft_);
        const bool hasBottomRight = useTileIfPresent(8, 8, playerTileBottomRight_);

        if (!hasTopLeft) {
            playerTileTopLeft_ = nearest.tile;
            addHidden(nearestIndex);
        }
        if (!hasTopRight) {
            playerTileTopRight_ = playerTileTopLeft_;
        }
        if (!hasBottomLeft) {
            playerTileBottomLeft_ = playerTileTopLeft_;
        }
        if (!hasBottomRight) {
            playerTileBottomRight_ = playerTileBottomLeft_;
        }

        playerSpriteWidthTiles_ = hasTopRight ? 2 : 1;
        playerSpriteHeightTiles_ = (room_.spriteHeight > 8 && hasBottomLeft) ? 2 : 1;
        playerSpriteLocked_ = true;
    }

    if (playerSpriteLocked_) {
        const auto isPlayerTile = [this](std::uint8_t tile) {
            return tile == playerTileTopLeft_
                || tile == playerTileTopRight_
                || tile == playerTileBottomLeft_
                || tile == playerTileBottomRight_;
        };

        for (std::size_t index = 0; index < room_.entities.size(); ++index) {
            const RoomEntity& entity = room_.entities[index];
            if (entity.palette == playerPalette_ && isPlayerTile(entity.tile)) {
                if (std::find(hiddenEntityIndices_.begin(), hiddenEntityIndices_.end(), index) == hiddenEntityIndices_.end()) {
                    hiddenEntityIndices_.push_back(index);
                }
            }
        }
    }

    link_.w = static_cast<float>(playerSpriteWidthTiles_ * 8);
    link_.h = static_cast<float>(playerSpriteHeightTiles_ * 8);

    const float maxX = std::max(0.0F, roomPixelWidth() - link_.w);
    const float maxY = std::max(0.0F, roomPixelHeight() - link_.h);

    if (enteredFrom == TransitionEdge::None) {
        link_.x = std::clamp(static_cast<float>(room_.playerStartX), 0.0F, maxX);
        link_.y = std::clamp(static_cast<float>(room_.playerStartY), 0.0F, maxY);
        return true;
    }

    switch (enteredFrom) {
    case TransitionEdge::North:
        if (room_.entryNorthX >= 0 && room_.entryNorthY >= 0) {
            link_.x = std::clamp(static_cast<float>(room_.entryNorthX), 0.0F, maxX);
            link_.y = std::clamp(static_cast<float>(room_.entryNorthY), 0.0F, maxY);
        } else {
            link_.x = std::clamp(preservedAxis, 0.0F, maxX);
            // Keep Link below the HUD/top transition threshold to avoid instant bounce-back.
            const float minPlayfieldY = static_cast<float>(std::clamp(kHudRows, 0, room_.height - 1) * room_.tileSize);
            link_.y = std::clamp(minPlayfieldY, 0.0F, maxY);
        }
        break;
    case TransitionEdge::South:
        if (room_.entrySouthX >= 0 && room_.entrySouthY >= 0) {
            link_.x = std::clamp(static_cast<float>(room_.entrySouthX), 0.0F, maxX);
            link_.y = std::clamp(static_cast<float>(room_.entrySouthY), 0.0F, maxY);
        } else {
            link_.x = std::clamp(preservedAxis, 0.0F, maxX);
            link_.y = maxY;
        }
        break;
    case TransitionEdge::West:
        if (room_.entryWestX >= 0 && room_.entryWestY >= 0) {
            link_.x = std::clamp(static_cast<float>(room_.entryWestX), 0.0F, maxX);
            link_.y = std::clamp(static_cast<float>(room_.entryWestY), 0.0F, maxY);
        } else {
            link_.x = 0.0F;
            link_.y = std::clamp(preservedAxis, 0.0F, maxY);
        }
        break;
    case TransitionEdge::East:
        if (room_.entryEastX >= 0 && room_.entryEastY >= 0) {
            link_.x = std::clamp(static_cast<float>(room_.entryEastX), 0.0F, maxX);
            link_.y = std::clamp(static_cast<float>(room_.entryEastY), 0.0F, maxY);
        } else {
            link_.x = maxX;
            link_.y = std::clamp(preservedAxis, 0.0F, maxY);
        }
        break;
    case TransitionEdge::None:
        break;
    }
    return true;
}

bool Game::collidesAt(float x, float y) const
{
    if (!roomLoaded_) {
        return false;
    }

    SDL_FRect candidate {x, y, link_.w, link_.h};
    const SDL_FRect collisionRect = makeCollisionRect(candidate);
    const float left = collisionRect.x;
    const float right = collisionRect.x + collisionRect.w - 0.01F;
    const float top = collisionRect.y;
    const float bottom = collisionRect.y + collisionRect.h - 0.01F;

    const int leftTile = static_cast<int>(std::floor(left / static_cast<float>(room_.tileSize)));
    const int rightTile = static_cast<int>(std::floor(right / static_cast<float>(room_.tileSize)));
    const int topTile = static_cast<int>(std::floor(top / static_cast<float>(room_.tileSize)));
    const int bottomTile = static_cast<int>(std::floor(bottom / static_cast<float>(room_.tileSize)));

    return room_.isBlockedTile(leftTile, topTile)
        || room_.isBlockedTile(rightTile, topTile)
        || room_.isBlockedTile(leftTile, bottomTile)
        || room_.isBlockedTile(rightTile, bottomTile);
}

void Game::moveWithCollision(float dx, float dy)
{
    if (!roomLoaded_) {
        link_.x += dx;
        link_.y += dy;
        return;
    }

    const auto moveAxis = [this](float distance, bool horizontal) {
        float remaining = distance;
        const float step = (distance > 0.0F) ? 1.0F : -1.0F;

        while (std::fabs(remaining) > 0.001F) {
            const float delta = (std::fabs(remaining) >= 1.0F) ? step : remaining;
            const float targetX = horizontal ? (link_.x + delta) : link_.x;
            const float targetY = horizontal ? link_.y : (link_.y + delta);

            SDL_FRect targetSprite {targetX, targetY, link_.w, link_.h};
            const SDL_FRect targetCollision = makeCollisionRect(targetSprite);
            if (targetCollision.x < 0.0F || targetCollision.y < 0.0F || targetCollision.x + targetCollision.w > roomPixelWidth() || targetCollision.y + targetCollision.h > roomPixelHeight()) {
                link_.x = targetX;
                link_.y = targetY;
                break;
            }

            if (collidesAt(targetX, targetY)) {
                break;
            }

            link_.x = targetX;
            link_.y = targetY;
            remaining -= delta;
        }
    };

    moveAxis(dx, true);
    moveAxis(dy, false);
}

bool Game::tryRoomTransition()
{
    if (!roomLoaded_ || transitionCooldown_ > 0.0F) {
        return false;
    }

    const SDL_FRect linkCollision = makeCollisionRect(link_);
    const float linkLeft = linkCollision.x;
    const float linkRight = linkCollision.x + linkCollision.w;
    const float linkTop = linkCollision.y;
    const float linkBottom = linkCollision.y + linkCollision.h;
    const std::filesystem::path baseDir = room_.sourcePath.parent_path();

    const auto overlapsAnyHorizontal = [&](const std::vector<SDL_FRect>& openings) {
        for (const SDL_FRect& opening : openings) {
            if (overlapsRange(linkLeft, linkRight, opening.x, opening.x + opening.w)) {
                return true;
            }
        }
        return false;
    };

    const auto overlapsAnyVertical = [&](const std::vector<SDL_FRect>& openings) {
        for (const SDL_FRect& opening : openings) {
            if (overlapsRange(linkTop, linkBottom, opening.y, opening.y + opening.h)) {
                return true;
            }
        }
        return false;
    };

    if (linkLeft <= -1.0F && !room_.westRoom.empty()) {
        const RoomAsset oldRoom = room_;
        const SDL_FRect oldLink = link_;
        if (!overlapsAnyVertical(buildEdgeOpenings(room_, EdgeKind::West))) {
            return false;
        }

        const bool transitioned = loadRoom(baseDir / room_.westRoom, TransitionEdge::East, link_.y);
        if (transitioned) {
            transitionCooldown_ = 0.15F;
            screenTransitionActive_ = true;
            screenTransitionEdge_ = TransitionEdge::West;
            screenTransitionPixels_ = 0.0F;
            screenTransitionDistance_ = roomPixelWidth();
            transitionFromRoom_ = oldRoom;
            transitionToRoom_ = room_;
            transitionFromLink_ = oldLink;
            transitionToLink_ = link_;

            // West scroll should show Link traversing from left screen edge to right screen edge.
            transitionFromLink_.x = 0.0F;
            transitionFromLink_.y = oldLink.y;
            transitionToLink_.x = std::max(0.0F, roomPixelWidth() - transitionToLink_.w);
            transitionToLink_.y = oldLink.y;

            // Keep final in-room position aligned with what the transition shows.
            link_.x = transitionToLink_.x;
            link_.y = transitionToLink_.y;
        }
        return transitioned;
    }

    if (linkRight >= roomPixelWidth() + 1.0F && !room_.eastRoom.empty()) {
        const RoomAsset oldRoom = room_;
        const SDL_FRect oldLink = link_;
        if (!overlapsAnyVertical(buildEdgeOpenings(room_, EdgeKind::East))) {
            return false;
        }

        const bool transitioned = loadRoom(baseDir / room_.eastRoom, TransitionEdge::West, link_.y);
        if (transitioned) {
            transitionCooldown_ = 0.15F;
            screenTransitionActive_ = true;
            screenTransitionEdge_ = TransitionEdge::East;
            screenTransitionPixels_ = 0.0F;
            screenTransitionDistance_ = roomPixelWidth();
            transitionFromRoom_ = oldRoom;
            transitionToRoom_ = room_;
            transitionFromLink_ = oldLink;
            transitionToLink_ = link_;

            // East scroll should show Link traversing from right screen edge to left screen edge.
            transitionFromLink_.x = std::max(0.0F, roomPixelWidth() - transitionFromLink_.w);
            transitionFromLink_.y = oldLink.y;
            transitionToLink_.x = 0.0F;
            transitionToLink_.y = oldLink.y;

            // Keep final in-room position aligned with what the transition shows.
            link_.x = transitionToLink_.x;
            link_.y = transitionToLink_.y;
        }
        return transitioned;
    }

    const float northTransitionY = static_cast<float>(std::clamp(kHudRows, 0, room_.height - 1) * room_.tileSize);
    if (linkTop <= northTransitionY - 1.0F && !room_.northRoom.empty()) {
        const RoomAsset oldRoom = room_;
        const SDL_FRect oldLink = link_;
        if (!overlapsAnyHorizontal(buildEdgeOpenings(room_, EdgeKind::North))) {
            return false;
        }

        const bool transitioned = loadRoom(baseDir / room_.northRoom, TransitionEdge::South, link_.x);
        if (transitioned) {
            transitionCooldown_ = 0.15F;
            screenTransitionActive_ = true;
            screenTransitionEdge_ = TransitionEdge::North;
            screenTransitionPixels_ = 0.0F;
            screenTransitionDistance_ = std::max(1.0F, roomPixelHeight() - static_cast<float>(kHudRows * room_.tileSize));
            transitionFromRoom_ = oldRoom;
            transitionToRoom_ = room_;
            transitionFromLink_ = oldLink;
            transitionToLink_ = link_;
        }
        return transitioned;
    }

    if (linkBottom >= roomPixelHeight() + 1.0F && !room_.southRoom.empty()) {
        const RoomAsset oldRoom = room_;
        const SDL_FRect oldLink = link_;
        if (!overlapsAnyHorizontal(buildEdgeOpenings(room_, EdgeKind::South))) {
            return false;
        }

        const bool transitioned = loadRoom(baseDir / room_.southRoom, TransitionEdge::North, link_.x);
        if (transitioned) {
            transitionCooldown_ = 0.15F;
            screenTransitionActive_ = true;
            screenTransitionEdge_ = TransitionEdge::South;
            screenTransitionPixels_ = 0.0F;
            screenTransitionDistance_ = std::max(1.0F, roomPixelHeight() - static_cast<float>(kHudRows * room_.tileSize));
            transitionFromRoom_ = oldRoom;
            transitionToRoom_ = room_;
            transitionFromLink_ = oldLink;
            transitionToLink_ = link_;
        }
        return transitioned;
    }

    return false;
}

bool Game::tryDoorTransition()
{
    if (!roomLoaded_ || transitionCooldown_ > 0.0F) {
        return false;
    }

    const float linkLeft = link_.x;
    const float linkTop = link_.y;
    const float linkRight = link_.x + link_.w;
    const float linkBottom = link_.y + link_.h;
    const std::filesystem::path baseDir = room_.sourcePath.parent_path();

    bool intersectsAnyDoor = false;

    for (const RoomDoorTrigger& door : room_.doorTriggers) {
        const SDL_FRect doorRect = normalizeDoorRect(door);
        const float doorLeft = doorRect.x;
        const float doorTop = doorRect.y;
        const float doorRight = doorRect.x + doorRect.w;
        const float doorBottom = doorRect.y + doorRect.h;

        const bool intersects = linkLeft < doorRight && linkRight > doorLeft && linkTop < doorBottom && linkBottom > doorTop;
        intersectsAnyDoor = intersectsAnyDoor || intersects;
        if (!intersects || door.targetRoom.empty()) {
            continue;
        }

        if (!doorTriggerReleased_) {
            continue;
        }

        const float roomW = roomPixelWidth();
        const float roomH = roomPixelHeight();
        const float edgeThreshold = static_cast<float>(room_.tileSize) * 0.5F;
        const bool nearNorth = doorTop <= edgeThreshold;
        const bool nearSouth = doorBottom >= roomH - edgeThreshold;
        const bool nearWest = doorLeft <= edgeThreshold;
        const bool nearEast = doorRight >= roomW - edgeThreshold;

        bool hasActivationIntent = moveUp_ || moveDown_ || moveLeft_ || moveRight_;
        if (nearSouth) {
            hasActivationIntent = moveDown_;
        } else if (nearNorth) {
            hasActivationIntent = moveUp_;
        } else if (nearWest) {
            hasActivationIntent = moveLeft_;
        } else if (nearEast) {
            hasActivationIntent = moveRight_;
        }
        if (!hasActivationIntent) {
            continue;
        }

        const RoomDoorTrigger triggeredDoor = door;

        if (!loadRoom(baseDir / triggeredDoor.targetRoom)) {
            return false;
        }

        int spawnX = triggeredDoor.spawnX;
        int spawnY = triggeredDoor.spawnY;

        // Enforce exact cave door placements using target room mapping.
        if (triggeredDoor.targetRoom == "door_room_full_16.room") {
            spawnX = 120;
            spawnY = 222;
        } else if (triggeredDoor.targetRoom == "first_room_full_16.room") {
            spawnX = 64;
            spawnY = 96;
        }

        const float maxX = std::max(0.0F, roomPixelWidth() - link_.w);
        const float maxY = std::max(0.0F, roomPixelHeight() - link_.h);
        if (spawnX >= 0 && spawnY >= 0) {
            link_.x = std::clamp(static_cast<float>(spawnX), 0.0F, maxX);
            link_.y = std::clamp(static_cast<float>(spawnY), 0.0F, maxY);
        }

        transitionCooldown_ = 0.2F;
        linkHideTimer_ = 0.0F;
        doorTriggerReleased_ = false;
        return true;
    }

    doorTriggerReleased_ = !intersectsAnyDoor;

    return false;
}

float Game::roomPixelWidth() const
{
    if (roomLoaded_) {
        return static_cast<float>(room_.width * room_.tileSize);
    }
    return kLogicalWidth;
}

float Game::roomPixelHeight() const
{
    if (roomLoaded_) {
        return static_cast<float>(room_.height * room_.tileSize);
    }
    return kLogicalHeight;
}
