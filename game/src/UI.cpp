// ╒═════════════════════════════ UI.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-24                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "UI.hpp"
#include <Syngine/Syngine.h>

#include <SDL3/SDL.h>
#include "Syngine/GameObjects/Component.h"
#include "bgfx/bgfx.h"

#include "imgui/backends/imgui_impl_bgfx.hpp"
#include "imgui/imgui_internal.h"

#include <lib/imgui/imgui.h>

#include <string>
#include <unordered_map>

#define FCBT_TO_PTR(x) reinterpret_cast<void*>(static_cast<std::uintptr_t>(x))
#define PTR_TO_FCBT(x)                                                         \
    static_cast<UI::FileCallbackType>(reinterpret_cast<std::uintptr_t>(x))
namespace SynEditor {

bool UI::m_layoutBuilt    = false;
bool UI::ConfigFileExists = false;

void SDLCALL FileDialogCallback(void*              userdata,
                                const char* const* filelist,
                                int                filter) {
    if (filelist == nullptr) {
        Syngine::Logger::LogF(Syngine::LogLevel::ERR,
                              false,
                              "Open scene dialog was canceled or failed: %s",
                              SDL_GetError());
        return;
    }

    if (!filelist[0]) {
        Syngine::Logger::Info("File dialog was cancelled", true);
        return;
    }

    UI::FileCallbackType type = PTR_TO_FCBT(userdata);

    Syngine::Logger::Info("File dialog callback type: " +
                              std::to_string(static_cast<int>(type)),
                          true);

    for (const char* const* file = filelist; *file != nullptr; ++file) {
        Syngine::Logger::LogF(
            Syngine::LogLevel::INFO, false, "Selected scene: %s", *file);
        // Load the selected scene here.
    }
}

void UI::_DrawHierarchyNode(Syngine::GameObject* object) {
    if (!object) return;

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_OpenOnArrow;

    if (m_selectedObject == object) {
        nodeFlags |= ImGuiTreeNodeFlags_Selected;
    }
    if (object->GetChildren().empty()) {
        nodeFlags |= ImGuiTreeNodeFlags_Leaf;
    }

    bool open = ImGui::TreeNodeEx(std::to_string(object->GetID()).c_str(),
                                  nodeFlags,
                                  "%s",
                                  object->name.c_str());

    if (ImGui::IsItemFocused()) {
        m_selectedObject = object;
    }

    if (open) {
        for (Syngine::GameObject* child : object->GetChildren()) {
            _DrawHierarchyNode(child);
        }
        ImGui::TreePop();
    }
}

void UI::_RegisterInspectorWidgets() {
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_TRANSFORM,
        InspectorWidgets::DrawTransformWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_LIGHT_DIRECTIONAL,
        InspectorWidgets::DrawDirectionalLightWidget);
}

void UI::Draw(int frameNum) {
    if (!bgfx::isValid(m_logoTexture)) {
        m_logoTexture = Syngine::UI::Debug::ImGui_ImplBgfx::LoadTex(
            "imgs/imgs.spk", "builtin/Syngine_Logo_Banner_Rounded.png");
    }

    if (frameNum == 1) {
        _RegisterInspectorWidgets();
    }

    DrawMainMenuBar();
    DrawMainDockspace();

    DrawScene();
    DrawHierarchy();
    DrawInspector();
    DrawConsole();
    DrawAssets();
}

void UI::BuildDefaultLayout(ImGuiID dockSpace) {
    ImGui::DockBuilderRemoveNode(dockSpace);
    ImGui::DockBuilderAddNode(dockSpace, ImGuiDockNodeFlags_DockSpace);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2         workSize = viewport->WorkSize;
    ImVec2         workPos  = viewport->WorkPos;

    ImGui::DockBuilderSetNodeSize(dockSpace, workSize);
    ImGui::DockBuilderSetNodePos(dockSpace, workPos);

    ImGuiID left;
    ImGuiID right;
    ImGuiID top;
    ImGuiID bottom;

    ImGui::DockBuilderSplitNode(
        dockSpace, ImGuiDir_Left, 0.2f, &left, &dockSpace);
    ImGui::DockBuilderSplitNode(
        dockSpace, ImGuiDir_Right, 0.25f, &right, &dockSpace);
    ImGui::DockBuilderSplitNode(
        dockSpace, ImGuiDir_Down, 0.35f, &bottom, &dockSpace);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Scene", dockSpace);

    ImGui::DockBuilderFinish(dockSpace);
}

void UI::DrawMainDockspace() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoBackground;

    ImGui::Begin("EditorDockspace", nullptr, flags);

    ImGuiID dockSpace = ImGui::GetID("MainDockspace");

    if (!m_layoutBuilt && !ConfigFileExists) {
        BuildDefaultLayout(dockSpace);
        m_layoutBuilt = true;
    }

    ImGui::DockSpace(dockSpace, ImVec2(0, 0), 0);

    ImGui::End();
}

void UI::DrawMainMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene")) {
                // New Scene
            }
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                // Open Scene
                const SDL_DialogFileFilter filters[] = { { "Scene Files",
                                                           "scene" } };
                SDL_ShowOpenFileDialog(
                    FileDialogCallback,
                    FCBT_TO_PTR(UI::FileCallbackType::OpenScene),
                    Syngine::Window::_GetSDLWindow(),
                    filters,
                    SDL_arraysize(filters),
                    nullptr,
                    false);
            }
            if (ImGui::MenuItem("Save Scene...", "Ctrl+S")) {
                // Save Scene
                const SDL_DialogFileFilter filters[] = { { "Scene Files",
                                                           "scene" } };
                SDL_ShowSaveFileDialog(
                    FileDialogCallback,
                    FCBT_TO_PTR(UI::FileCallbackType::SaveScene),
                    Syngine::Window::_GetSDLWindow(),
                    filters,
                    SDL_arraysize(filters),
                    nullptr);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Exit application
                Syngine::Core::Quit();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
            if (ImGui::MenuItem("Undo", "Ctrl+Z")) {
                // Undo action
            }
            if (ImGui::MenuItem("Redo", "Ctrl+Y")) {
                // Redo action
            }
            ImGui::Separator();
            ImGui::MenuItem("Preferences");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Hierarchy")) {
                // Toggle Hierarchy window
                ImGui::SetWindowFocus("Hierarchy");
            }
            if (ImGui::MenuItem("Inspector")) {
                // Toggle Inspector window
                ImGui::SetWindowFocus("Inspector");
            }
            if (ImGui::MenuItem("Assets")) {
                // Toggle Assets window
                ImGui::SetWindowFocus("Assets");
            }
            if (ImGui::MenuItem("Console")) {
                // Toggle Console window
                ImGui::SetWindowFocus("Console");
            }
            if (ImGui::MenuItem("Scene")) {
                // Toggle Scene window
                ImGui::SetWindowFocus("Scene");
            }
            ImGui::EndMenu();
        }

        bool openAbout = false;
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                openAbout = true;
            }
            ImGui::EndMenu();
        }

        if (openAbout) {
            ImGui::OpenPopup("About");
        }

        if (ImGui::BeginPopup("About")) {
            ImGui::Text("Syngine Studio\nVersion: %s",
                        SYNGINE_STUDIO_VERSION_STRING);
            ImGui::Separator();
            ImGui::Text("Licensed under the MIT License");
            ImGui::TextLinkOpenURL("GitHub",
                                   "https://github.com/SentyTek/"
                                   "SyngineStudio");
            ImGui::Separator();
            ImGui::Image(Syngine::UI::Debug::SImGui::ToImGui(m_logoTexture),
                         ImVec2(600, 200));
            ImGui::Text("© 2025-2026 SentyTek Software. All rights reserved.");

            if (ImGui::Button("Close")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::EndMainMenuBar();
    }
}

void UI::DrawScene() {
    static bool viewPortActive  = false;
    static bool viewPortHovered = false;
    ImGui::Begin("Scene", nullptr, m_wFlags);

    ImVec2 avail = ImGui::GetContentRegionAvail();

    float aspect = Syngine::Renderer::width / (float)Syngine::Renderer::height;

    float width  = avail.x;
    float height = width / aspect;

    if (height > avail.y) {
        height = avail.y;
        width  = height * aspect;
    }

    ImGui::Image(Syngine::UI::Debug::SImGui::ToImGui(
                     Syngine::Renderer::GetSceneTexture()),
                 ImVec2(width, height));

    viewPortHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        viewPortActive = true;
    } else if (!viewPortHovered) {
        viewPortActive = false;
    }

    ImGui::End();

    Syngine::Core::viewportHovered = viewPortHovered;
    Syngine::Core::viewportActive  = viewPortActive;
}

void UI::DrawHierarchy() {
    ImGui::Begin("Hierarchy", nullptr, m_wFlags);

    // Figure out root objects
    const std::unordered_map<int, Syngine::GameObject>& allObjects =
        Syngine::GameObjectRegistry::GetAllGameObjects();
    std::vector<const Syngine::GameObject*> rootObjects;
    for (auto& [id, object] : allObjects) {
        if (!object.GetParent()) {
            rootObjects.push_back(&object);
        }
    }

    for (const Syngine::GameObject* rootObject : rootObjects) {
        _DrawHierarchyNode(const_cast<Syngine::GameObject*>(rootObject));
    }

    ImGui::End();
}

void UI::DrawInspector() {
    ImGui::Begin("Inspector", nullptr, m_wFlags);

    if (m_selectedObject) {
        SynEditor::InspectorRegistry::Draw(*m_selectedObject);
    } else {
        ImGui::Text("No object selected.");
    }

    ImGui::End();
}

void UI::DrawAssets() {
    ImGui::Begin("Assets", nullptr, m_wFlags);
    ImGui::Text("Assets content goes here.");
    ImGui::End();
}

void UI::DrawConsole() {
    ImGui::Begin("Console", nullptr, m_wFlags);
    ImGui::Text("Console content goes here.");
    ImGui::End();
}

} // namespace SynEditor
