# CMakeLists.txt for the game (root/game/CMakeLists.txt)

set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GAME_ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")

include(${SYNGINE_SOURCE_DIR}/cmake/GameConfig.cmake)

# Invoke the engine to build the game
SyngineGame(${EXECUTABLE_NAME}
    SOURCES
        src/main.cpp
        src/Editor.cpp
        src/UI.cpp
)
