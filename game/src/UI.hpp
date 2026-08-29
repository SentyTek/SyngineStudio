// ╒═════════════════════════════ UI.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-24                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include "bgfx/bgfx.h"
#define SYNGINE_STUDIO_VERSION_STRING "0.0.1.dev"

#include <Syngine/Core/Core.h>

#include <lib/imgui/imgui.h>

namespace SynEditor {

class UI {
    ImGuiWindowFlags m_wFlags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    static bool m_layoutBuilt;

    bgfx::TextureHandle m_logoTexture = BGFX_INVALID_HANDLE;

  public:
    void Draw(int frameNum);

    void BuildDefaultLayout(ImGuiID dockSpace);

    void DrawMainDockspace();
    void DrawMainMenuBar();

    void DrawScene();
    void DrawHierarchy();
    void DrawInspector();
    void DrawAssets();
    void DrawConsole();

    static bool ConfigFileExists;

    enum class FileCallbackType { OpenScene, SaveScene };
};

} // namespace SynEditor
