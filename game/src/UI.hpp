// ╒═════════════════════════════ UI.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-24                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include "Syngine/Core/Logger.h"
#include "bgfx/bgfx.h"
#define SYNGINE_STUDIO_VERSION_STRING "0.0.1.dev"

#include <Syngine/Core/Core.h>

#include <lib/imgui/imgui.h>
#include "InspectorWidgets.inl"
#include "AssetWindow.hpp"

namespace SynEditor {

enum class Tool { Select, Move, Rotate, Scale };
class UI {
    ImGuiWindowFlags m_wFlags =
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
    static bool m_layoutBuilt;

    bgfx::TextureHandle  m_logoTexture    = BGFX_INVALID_HANDLE;
    Syngine::GameObject* m_selectedObject = nullptr;
    AssetInfo*           m_selectedAsset  = nullptr;

    bool _DrawHierarchyNode(Syngine::GameObject* object,
                            const char*          searchText);
    bool _HierarchyNodeMatches(Syngine::GameObject* object,
                               const char*          searchText);
    bool _ContainsInsensitive(const std::string& text, const char* searchText);
    void _RegisterInspectorWidgets();
    Syngine::GameObject& _AddGameObject(int type, int shape);

    struct LogMessage {
        std::string message;
        std::string level;
        std::string timestamp;
    };
    static std::vector<LogMessage> m_logMessages;

    static void _LogMsgCb(const std::string& message,
                          Syngine::LogLevel  level,
                          const std::string& timestamp);

    friend class Syngine::Logger;

    struct SceneSettings {
        Tool  tool       = Tool::Select;
        float snapAmount = 0.1f;
        bool  gridSnap   = false;
    } m_sceneSettings;

    inline bool SelectableWithIcon(const char* label,
                                   const char* fileName     = nullptr,
                                   bool        drawCheckbox = false,
                                   bool        checked      = false) {
        ImVec2 pos  = ImGui::GetCursorScreenPos();
        ImVec2 size = ImVec2(250.0f, 24.0f);

        std::string lower = fileName ? fileName : label;
        for (auto& c : lower) c = std::tolower(c);
        // replace spaces with underscores
        for (auto& c : lower) {
            if (c == ' ') c = '_';
        }

        ImTextureRef icon = ImTextureRef(
            IconManager::GetIconImGui(fileName ? fileName : lower));

        bool clicked =
            ImGui::Selectable(("##" + lower).c_str(), false, 0, size);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddImage(
            icon, ImVec2(pos.x + 4, pos.y + 4), ImVec2(pos.x + 20, pos.y + 20));

        draw->AddText(ImVec2(pos.x + 24, pos.y + 4),
                      ImGui::GetColorU32(ImGuiCol_Text),
                      label);

        if (drawCheckbox) {
            ImVec2 checkboxPos = ImVec2(pos.x + size.x - 24, pos.y + 4);
            ImGui::GetWindowDrawList()->AddRectFilled(
                checkboxPos,
                ImVec2(checkboxPos.x + 16, checkboxPos.y + 16),
                checked ? IM_COL32(0, 255, 0, 255) : IM_COL32(255, 0, 0, 255));
        }

        return clicked;
    };

  public:
    std::string g_projectName;
    scl::path   g_projectDirectory;

    void Setup(std::string projectName, scl::path projectDirectory);
    void Shutdown();
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
