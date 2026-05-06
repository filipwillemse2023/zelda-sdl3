#pragma once

#include "game/Game.hpp"

#include <SDL3/SDL.h>

#include <filesystem>

class Application {
public:
    int run(int argc, char** argv);

private:
    void initialize(const std::filesystem::path& preferredRoomPath);
    void shutdown();
    void processEvents(bool& running);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    Game game_;
};