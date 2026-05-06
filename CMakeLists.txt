cmake_minimum_required(VERSION 3.24)

project(zelda_sdl3_port VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(ZELDA_FETCH_SDL3 "Fetch SDL3 if it is not installed locally" ON)

find_package(SDL3 QUIET CONFIG)

if(NOT SDL3_FOUND AND ZELDA_FETCH_SDL3)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "Build SDL shared library" FORCE)
    set(SDL_STATIC ON CACHE BOOL "Build SDL static library" FORCE)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.10
    )
    FetchContent_MakeAvailable(SDL3)
endif()

if(NOT SDL3_FOUND AND NOT TARGET SDL3::SDL3)
    message(FATAL_ERROR "SDL3 was not found. Install SDL3 or configure with ZELDA_FETCH_SDL3=ON.")
endif()

add_executable(zelda_sdl3
    src/main.cpp
    src/engine/Application.cpp
    src/game/RoomAsset.cpp
    src/game/Game.cpp
)

target_include_directories(zelda_sdl3
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(zelda_sdl3
    PRIVATE
        SDL_MAIN_USE_CALLBACKS=0
)

if(TARGET SDL3::SDL3-static)
    target_link_libraries(zelda_sdl3 PRIVATE SDL3::SDL3-static)
else()
    target_link_libraries(zelda_sdl3 PRIVATE SDL3::SDL3)
endif()

if(MSVC)
    target_compile_options(zelda_sdl3 PRIVATE /W4 /permissive-)
else()
    target_compile_options(zelda_sdl3 PRIVATE -Wall -Wextra -Wpedantic)
endif()