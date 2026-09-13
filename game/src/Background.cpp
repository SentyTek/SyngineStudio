// ╒═════════════════════ Background.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include <Syngine/Syngine.h>

#include "Background.h"
#include "Process.hpp"
#include "AssetWindow.inl"
#include "SDL3/SDL_render.h"

#include <imgui/imgui.h>
#include <stb_image.h>

#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace SynEditor {

Process          BackgroundActivities::g_cmakeProcess;
bool             BackgroundActivities::g_showCmakePopup = false;
std::string      BackgroundActivities::g_loadingText    = "Starting";
BuildEnvironment BackgroundActivities::g_buildEnvironment;

SDL_Window*   BackgroundActivities::g_startupWindow   = nullptr;
SDL_Renderer* BackgroundActivities::g_startupRenderer = nullptr;
SDL_Texture*  BackgroundActivities::g_startupTexture  = nullptr;

void BackgroundActivities::SetupBuildEnvironment() {
#ifdef _WIN32
    // First find the Visual Studio installation and vcvars path
    // Fortunately, vswhere exists
    char programFilesX86[MAX_PATH] = {};
    GetEnvironmentVariableA("ProgramFiles(x86)", programFilesX86, MAX_PATH);
    std::string installerDir =
        std::string(programFilesX86) + "\\Microsoft Visual Studio\\Installer";

    Process vswhereProcess;
    vswhereProcess.Start(
        installerDir + "\\vswhere.exe -latest -products * -requires "
                       "Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                       "-property installationPath",
        installerDir);
    vswhereProcess.Wait();
    if (vswhereProcess.GetExitCode() != 0) {
        Syngine::Logger::ToConsole(
            "Failed to find Visual Studio installation via vswhere.");
    }
    std::string vswhereOutput = vswhereProcess.Output();
    if (vswhereOutput.empty()) {
        Syngine::Logger::ToConsole(
            "Visual Studio installation path not found in vswhere output.");
    }

    // strip any trailing newline characters from the vswhere output
    while (!vswhereOutput.empty() &&
           (vswhereOutput.back() == '\n' || vswhereOutput.back() == '\r')) {
        vswhereOutput.pop_back();
    }
    std::string vcvarsPath =
        vswhereOutput + "\\VC\\Auxiliary\\Build\\vcvars64.bat";

    BuildEnvironment buildEnvironment{ .visualStudio = scl::path(vswhereOutput),
                                       .vcvars       = scl::path(vcvarsPath),
                                       .arch         = "x64" };
    g_buildEnvironment = buildEnvironment;
#else
    // For non-Windows platforms, set up a default build environment
    BuildEnvironment buildEnvironment{ .visualStudio = scl::path(""),
                                       .vcvars       = scl::path(""),
                                       .arch         = "x64" };
    g_buildEnvironment = buildEnvironment;
#endif
}

void BackgroundActivities::Update() {}

void BackgroundActivities::RenderCmake() {
    if (g_showCmakePopup) {
        if (ImGui::BeginPopupModal(
                "CMake Output", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::BeginChild("Output", ImVec2(500, 300));
            ImGui::TextUnformatted(g_cmakeProcess.Output().c_str());
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (ImGui::Button("Cancel")) {
                g_cmakeProcess.Kill();
                g_showCmakePopup = false;
                ImGui::CloseCurrentPopup();
                AssetWindow::RebuildTree();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                g_showCmakePopup = false;
                ImGui::CloseCurrentPopup();
                AssetWindow::RebuildTree();
            }
            ImGui::EndPopup();
        }
    }
}

void BackgroundActivities::RunCmake(const std::string& sourceDir) {
    g_showCmakePopup = true;
    std::string command;
#if BX_PLATFORM_WINDOWS
    command = std::string("cmd.exe /c call \"") +
              g_buildEnvironment.vcvars.cstr() +
              "\" && cmake -S . -B build -G Ninja && cmake --build "
              "build --target BuildAssets";
#elif BX_PLATFORM_OSX
    command = "cmake -S . -B build -G Ninja && cmake --build build --target "
              "BuildAssets";
#elif BX_PLATFORM_LINUX
    command = "cmake -S . -B build -G Ninja && cmake --build build --target "
              "BuildAssets";
#endif
    g_cmakeProcess.Start(command, sourceDir);
}

void BackgroundActivities::ShowStartupScreen() {
    if (g_startupWindow) {
        return;
    }
    // Shows the "Syngine Studio" startup screen
    // Creates a borderless SDL window since this shows before the main window
    // is initialized
    int         windowWidth   = 800;
    int         windowHeight  = 600;
    SDL_Window* startupWindow = SDL_CreateWindow(
        "Syngine Studio", windowWidth, windowHeight, SDL_WINDOW_BORDERLESS);
    if (!startupWindow) {
        return;
    }
    SDL_HideWindow(startupWindow);
    SDL_SetWindowPosition(
        startupWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

    SDL_Renderer* renderer = SDL_CreateRenderer(startupWindow, nullptr);
    if (!renderer) {
        SDL_DestroyWindow(startupWindow);
        Syngine::Logger::LogF(Syngine::LogLevel::ERR,
                              false,
                              "Failed to create SDL renderer: %s",
                              SDL_GetError());
        return;
    }

    g_startupWindow   = startupWindow;
    g_startupRenderer = renderer;
    g_startupTexture  = nullptr;

    if (g_startupRenderer) {
        int            imgWidth = 0, imgHeight = 0, channels = 0;
        unsigned char* pixels = nullptr;

        // Loads from bundle using stream read
        auto stream = Syngine::Serializer::_ReadFromBundle(
            "imgs/imgs.spk", "builtin/StartupLogo.png");
        if (stream.size() > 0) {
            std::vector<stbi_uc> buffer(stream.size());
            stream.read(buffer.data(), buffer.size());
            pixels = stbi_load_from_memory(buffer.data(),
                                           (int)buffer.size(),
                                           &imgWidth,
                                           &imgHeight,
                                           &channels,
                                           4);
        }

        if (!pixels) {
            Syngine::Logger::Error("Failed to load startup image");
        } else {
            SDL_Surface* surface = SDL_CreateSurfaceFrom(imgWidth,
                                                         imgHeight,
                                                         SDL_PIXELFORMAT_RGBA32,
                                                         pixels,
                                                         imgWidth * 4);
            if (surface) {
                g_startupTexture =
                    SDL_CreateTextureFromSurface(g_startupRenderer, surface);
                SDL_DestroySurface(surface);
            }
            stbi_image_free(pixels);
        }
    }

    SDL_ShowWindow(startupWindow);
    SDL_RaiseWindow(startupWindow);

    Syngine::Logger::Info("Created startup window and renderer");

    for (int i = 0; i < 5; ++i) {
        RenderStartupScreen();
        SDL_Delay(10);
    }
}

void BackgroundActivities::RenderStartupScreen() {
    if (!g_startupWindow || !g_startupRenderer) {
        return;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
    }

    int windowWidth  = 800;
    int windowHeight = 600;
    SDL_GetWindowSize(g_startupWindow, &windowWidth, &windowHeight);

    // Clear background
    SDL_SetRenderDrawColor(g_startupRenderer, 20, 20, 26, 255);
    SDL_RenderClear(g_startupRenderer);

    // Render image taking up the full window
    if (g_startupTexture) {
        SDL_FRect dstRect = {
            0.0f, 0.0f, (float)windowWidth, (float)windowHeight
        };
        SDL_RenderTexture(
            g_startupRenderer, g_startupTexture, nullptr, &dstRect);
    }

    // Render text "Syngine Studio" on top
    SDL_SetRenderDrawColor(g_startupRenderer, 255, 255, 255, 255);
    const char* titleText = "Syngine Studio";
    float       textScale = 5.0f;
    SDL_SetRenderScale(g_startupRenderer, textScale, textScale);

    float textLen = (float)std::strlen(titleText) * 8.0f;
    float textX   = 2.0f; // Const
    float textY   = ((float)windowHeight / textScale) - 25.0f;

    SDL_RenderDebugText(g_startupRenderer, textX, textY, titleText);

    SDL_SetRenderScale(g_startupRenderer, 1.0f, 1.0f);

    SDL_RenderDebugText(g_startupRenderer,
                        textX + 10.0f,
                        textY * textScale + 50.0f,
                        g_loadingText.c_str());

    SDL_RenderPresent(g_startupRenderer);
}

void BackgroundActivities::HideStartupScreen() {
    if (g_startupTexture) {
        SDL_DestroyTexture(g_startupTexture);
        g_startupTexture = nullptr;
    }
    if (g_startupRenderer) {
        SDL_DestroyRenderer(g_startupRenderer);
        g_startupRenderer = nullptr;
    }
    if (g_startupWindow) {
        SDL_HideWindow(g_startupWindow);
        SDL_DestroyWindow(g_startupWindow);
        g_startupWindow = nullptr;
    }
}

} // namespace SynEditor
