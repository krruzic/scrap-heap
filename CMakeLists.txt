cmake_minimum_required(VERSION 3.20)
project(scrapheap VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch SDL3 and SDL3_ttf
include(FetchContent)

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.4
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    SDL3_ttf
    GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
    GIT_TAG release-3.2.0
    GIT_SHALLOW TRUE
)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL3TTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDL3TTF_SAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(SDL3 SDL3_ttf)

# Source files
set(SOURCES
    src/main.cpp
    src/game_context.cpp
    src/input.cpp
    src/renderer.cpp
    src/components.cpp
    src/bot.cpp
    src/physics.cpp
    src/combat.cpp
    src/stage.cpp
    src/powerup.cpp
    src/menu.cpp
    src/game_setup.cpp
    src/battle.cpp
    src/data.cpp
    src/utils.cpp
)

# Header files
set(HEADERS
    include/game_context.h
    include/input.h
    include/renderer.h
    include/components.h
    include/bot.h
    include/physics.h
    include/combat.h
    include/stage.h
    include/powerup.h
    include/menu.h
    include/game_setup.h
    include/battle.h
    include/data.h
    include/utils.h
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3-static
    SDL3_ttf::SDL3_ttf-static
)

# Copy assets to build directory
file(GLOB ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
foreach(ASSET ${ASSET_FILES})
    get_filename_component(ASSET_NAME ${ASSET} NAME)
    configure_file(${ASSET} ${CMAKE_CURRENT_BINARY_DIR}/assets/${ASSET_NAME} COPYONLY)
endforeach()

# Platform-specific settings
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE winmm imm32 version setupapi)
elseif(UNIX AND NOT APPLE)
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
endif()

# Install target
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY assets/ DESTINATION share/${PROJECT_NAME}/assets)
