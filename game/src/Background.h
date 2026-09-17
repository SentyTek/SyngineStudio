// ╒═══════════════════════ Background.h ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include <string>

#include "Process.hpp"
#include <miniscl.hpp>

#include <SDL3/SDL.h>

namespace SynEditor {

class BackgroundActivities {
    static SDL_Window*   g_startupWindow;
    static SDL_Renderer* g_startupRenderer;
    static SDL_Texture*  g_startupTexture;

  public:
    static Process     g_cmakeProcess;
    static bool        g_showCmakePopup;
    static std::string g_loadingText;

    static void Update();
    static void RenderCmake();

    static void RunCmake(const std::string& sourceDir);

    static void ShowStartupScreen();
    static void RenderStartupScreen();
    static void HideStartupScreen();
};

} // namespace SynEditor
