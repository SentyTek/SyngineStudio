// ╒═════════════════════════════ UI.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-24                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "UI.hpp"
#include "imgui/backends/imgui_impl_bgfx.hpp"
#include "imgui/imgui_internal.h"

#include <lib/imgui/imgui.h>

namespace SynEditor {

bool UI::m_layoutBuilt    = false;
bool UI::ConfigFileExists = false;

void UI::Draw(int frameNum) {
    DrawMainMenuBar();
    DrawMainDockspace();

    DrawScene();
    DrawHierarchy();
    DrawInspector();
    DrawAssets();
    DrawConsole();
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
        dockSpace, ImGuiDir_Down, 0.25f, &bottom, &dockSpace);

    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Assets", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
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
            }
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) {
                // Save Scene
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit")) {
                // Exit application
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
            }
            if (ImGui::MenuItem("Inspector")) {
                // Toggle Inspector window
            }
            if (ImGui::MenuItem("Assets")) {
                // Toggle Assets window
            }
            if (ImGui::MenuItem("Console")) {
                // Toggle Console window
            }
            if (ImGui::MenuItem("Scene")) {
                // Toggle Scene window
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                // Show about dialog
            }
            ImGui::EndMenu();
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
    ImGui::Text("Hierarchy content goes here.");
    ImGui::End();
}

void UI::DrawInspector() {
    ImGui::Begin("Inspector", nullptr, m_wFlags);
    ImGui::Text("Inspector content goes here.");
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
