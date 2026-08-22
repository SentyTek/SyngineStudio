# root/CMakeLists.txt
cmake_minimum_required(VERSION 3.31)
project("SyngineStudio")
set(EXECUTABLE_NAME ${PROJECT_NAME})
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set_property(GLOBAL PROPERTY USE_FOLDERS ON "Enable folder grouping in IDEs")

# Set the current year for copyright notices
set(CURRENT_YEAR 2026)

# Set the install prefix based on platform
if(UNIX AND NOT APPLE)
    set(CMAKE_INSTALL_PREFIX "$ENV{HOME}/.local" CACHE INTERNAL "")
else()
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")
endif()

if(APPLE)
    # set the minimum deployment version for the asset compiler
    set(MINIMUM_MACOS_VERSION 26.0)
    # set the bundle identifier for xcode
    set(BUNDLE_IDENTIFIER "com.example.SyngineStudio")
endif()

# Enable making a Windows executable if on Windows
if(MSVC)
    set(CMAKE_WIN32_EXECUTABLE ON)

    # Use static runtime on MSVC. Several libraries we use require this.
    # Also set CMAKE_MSVC_RUNTIME_LIBRARY. Sub-projects might still query this variable.
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# set the output directory for built objects.
# This makes sure that the dynamic library goes into the build directory automatically.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

# Add engine first
add_subdirectory(engine)

# Add the game
add_subdirectory(game)

set_target_properties(${EXECUTABLE_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/bin")
