#include "engine/Application.hpp"

#include <SDL3/SDL.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {
constexpr int kDefaultLogicalWidth = 256;
constexpr int kDefaultLogicalHeight = 240;
constexpr int kWindowScale = 3;

std::filesystem::path parsePreferredRoomPath(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if ((argument == "--room" || argument == "-r") && index + 1 < argc) {
            return std::filesystem::path(argv[index + 1]);
        }
    }
    return {};
}
}

int Application::run(int argc, char** argv)
{
    const std::filesystem::path preferredRoomPath = parsePreferredRoomPath(argc, argv);
    initialize(preferredRoomPath);

    bool running = true;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    const double frequency = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running) {
        processEvents(running);

        const Uint64 currentCounter = SDL_GetPerformanceCounter();
        const double deltaSeconds = static_cast<double>(currentCounter - previousCounter) / frequency;
        previousCounter = currentCounter;

        game_.update(deltaSeconds);

        const std::string title = "Zelda SDL3 Port Prototype - " + game_.currentRoomLabel();
        SDL_SetWindowTitle(window_, title.c_str());

        SDL_SetRenderDrawColor(renderer_, 8, 11, 18, 255);
        SDL_RenderClear(renderer_);

        game_.render(renderer_);

        SDL_RenderPresent(renderer_);
    }

    shutdown();
    return 0;
}

void Application::initialize(const std::filesystem::path& preferredRoomPath)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        throw std::runtime_error(SDL_GetError());
    }

    window_ = SDL_CreateWindow(
        "Zelda SDL3 Port Prototype",
        kDefaultLogicalWidth * kWindowScale,
        kDefaultLogicalHeight * kWindowScale,
        SDL_WINDOW_RESIZABLE);
    if (!window_) {
        throw std::runtime_error(SDL_GetError());
    }

    renderer_ = SDL_CreateRenderer(window_, nullptr);
    if (!renderer_) {
        throw std::runtime_error(SDL_GetError());
    }

    SDL_SetRenderVSync(renderer_, 1);

    game_.initialize(preferredRoomPath);

    const SDL_Point logicalSize = game_.logicalSize();
    const int windowWidth = std::max(logicalSize.x, 1) * kWindowScale;
    const int windowHeight = std::max(logicalSize.y, 1) * kWindowScale;
    SDL_SetWindowSize(window_, windowWidth, windowHeight);
    SDL_SetWindowMinimumSize(window_, windowWidth, windowHeight);
}

void Application::shutdown()
{
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }

    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }

    SDL_Quit();
}

void Application::processEvents(bool& running)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            running = false;
            continue;
        }

        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
            running = false;
            continue;
        }

        game_.handleEvent(event);
    }
}