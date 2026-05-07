#pragma once

#include "game/RoomAsset.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <vector>

class Game {
public:
    void initialize(const std::filesystem::path& preferredRoomPath = {});
    void handleEvent(const SDL_Event& event);
    void update(double deltaSeconds);
    void render(SDL_Renderer* renderer) const;
    SDL_Point logicalSize() const;
    std::string currentRoomLabel() const;

private:
    enum class TransitionEdge {
        None,
        North,
        South,
        West,
        East,
    };

    bool loadRoom(const std::filesystem::path& roomPath, TransitionEdge enteredFrom = TransitionEdge::None, float preservedAxis = 0.0F);
    bool collidesAt(float x, float y) const;
    void moveWithCollision(float dx, float dy);
    bool tryDoorTransition();
    bool tryRoomTransition();
    float roomPixelWidth() const;
    float roomPixelHeight() const;
    void renderFallback(SDL_Renderer* renderer) const;

    RoomAsset room_;
    bool roomLoaded_ = false;
    SDL_FRect link_ {96.0F, 96.0F, 16.0F, 16.0F};
    float speed_ = 88.0F;
    bool moveUp_ = false;
    bool moveDown_ = false;
    bool moveLeft_ = false;
    bool moveRight_ = false;
    float transitionCooldown_ = 0.0F;
    bool doorTriggerReleased_ = true;

    bool playerUsesSprite_ = false;
    std::uint8_t playerTileTopLeft_ = 0;
    std::uint8_t playerTileTopRight_ = 0;
    std::uint8_t playerTileBottomLeft_ = 0;
    std::uint8_t playerTileBottomRight_ = 0;
    int playerSpriteWidthTiles_ = 1;
    int playerSpriteHeightTiles_ = 1;
    std::uint8_t playerPalette_ = 0;
    bool playerFlipH_ = false;
    bool playerFlipV_ = false;
    bool playerSpriteLocked_ = false;
    std::vector<std::size_t> hiddenEntityIndices_;
    bool showDebugOverlays_ = false;

    float linkHideTimer_ = 0.0F;

    bool screenTransitionActive_ = false;
    TransitionEdge screenTransitionEdge_ = TransitionEdge::None;
    float screenTransitionPixels_ = 0.0F;
    float screenTransitionDistance_ = 0.0F;
    RoomAsset transitionFromRoom_;
    RoomAsset transitionToRoom_;
    SDL_FRect transitionFromLink_ {};
    SDL_FRect transitionToLink_ {};
};