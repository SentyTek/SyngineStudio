# CMakeLists.txt for the game (root/game/CMakeLists.txt)

set(GAME_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(GAME_ASSET_DIR "${CMAKE_SOURCE_DIR}/assets")

include(${SYNGINE_SOURCE_DIR}/cmake/GameConfig.cmake)

# Invoke the engine to build the game
SyngineGame(${EXECUTABLE_NAME}
    SOURCES
        src/main.cpp
        src/SceneControls.cpp
        src/UI.cpp
        src/Background.cpp
        src/Process.cpp
)

add_subdirectory(lib/tpl)
target_link_libraries(${EXECUTABLE_NAME} PRIVATE tiny-process-library)
