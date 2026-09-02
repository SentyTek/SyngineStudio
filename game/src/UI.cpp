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
#include "Syngine/GameObjects/Components/CameraComponent.h"
#include "Syngine/GameObjects/Components/MeshComponent.h"
#include "Syngine/GameObjects/Components/RigidbodyComponent.h"
#include "Syngine/GameObjects/Components/TransformComponent.h"
#include "Syngine/Math/Vector3.hpp"
#include "Syngine/Scene/GameObjectRegistry.h"
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

bool                        UI::m_layoutBuilt    = false;
bool                        UI::ConfigFileExists = false;
std::vector<UI::LogMessage> UI::m_logMessages;

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

// type 0 = empty, 2 = with mesh, 3 = with mesh & rigidbody
// shape 0 = cube, 1 = sphere
Syngine::GameObject& UI::_AddGameObject(int type, int shape) {
    Syngine::GameObject& gameObject =
        Syngine::GameObjectRegistry::CreateGameObject("GameObject");
    gameObject.AddComponent<Syngine::TransformComponent>();
    if (type > 1) {
        if (shape == 0) {
            gameObject.AddComponent<Syngine::MeshComponent>(
                "meshes/meshes.spk", "cube.glb", false);
        } else if (shape == 1) {
            gameObject.AddComponent<Syngine::MeshComponent>(
                "meshes/meshes.spk", "sphere.glb", false);
        }
    }
    if (type > 2) {
        Syngine::RigidbodyParameters params;
        if (shape == 0) {
            params.shape = Syngine::PhysicsShapes::BOX;
        } else if (shape == 1) {
            params.shape = Syngine::PhysicsShapes::SPHERE;
        }
        gameObject.AddComponent<Syngine::RigidbodyComponent>(params);
    }
    m_selectedObject = &gameObject;
    return gameObject;
}

void UI::_DrawHierarchyNode(Syngine::GameObject* object) {
    if (!object) return;

    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnDoubleClick |
                                   ImGuiTreeNodeFlags_SpanAvailWidth |
                                   ImGuiTreeNodeFlags_OpenOnArrow |
                                   ImGuiTreeNodeFlags_DefaultOpen;

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

    if (ImGui::IsItemClicked()) {
        m_selectedObject = object;
    }

    bool shouldOpenRenamePopup = false;
    if (ImGui::BeginPopupContextItem("GameObjectHierarchyContextMenu")) {
        if (ImGui::MenuItem("Add empty child")) {
            object->AddChild(&_AddGameObject(0, -1));
        }

        if (ImGui::MenuItem("Rename")) {
            shouldOpenRenamePopup = true;
        }

        if (ImGui::MenuItem("Delete")) {
            Syngine::GameObjectRegistry::RemoveGameObject(object);
            if (m_selectedObject == object) {
                m_selectedObject = nullptr;
            }
        }
        ImGui::EndPopup();
    }

    if (m_selectedObject == object && ImGui::IsKeyPressed(ImGuiKey_F2) &&
        ImGui::IsItemFocused()) {
        shouldOpenRenamePopup = true;
    }

    if (shouldOpenRenamePopup) {
        ImGui::OpenPopup("RenameGameObjectPopup");
    }

    if (ImGui::BeginPopup("RenameGameObjectPopup")) {
        static char newName[128] = "";
        if (ImGui::IsWindowAppearing()) {
            ImGui::SetKeyboardFocusHere();
        }
        ImGui::InputTextWithHint(
            "New Name", object->name.c_str(), newName, IM_ARRAYSIZE(newName));
        if (ImGui::Button("Rename") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            if (strlen(newName) > 0) {
                object->name = newName;
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // Drag source
    if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload(
            "SYNGINE_GAMEOBJECT", &object, sizeof(Syngine::GameObject*));
        ImGui::Text(
            "%s",
            object->name.c_str()); // Display the name of the dragged object
        ImGui::EndDragDropSource();
    }

    // Drop Target
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("SYNGINE_GAMEOBJECT")) {
            Syngine::GameObject* droppedObject =
                *(Syngine::GameObject**)payload->Data;
            if (droppedObject && droppedObject != object) {
                if (droppedObject->CanBeParentedTo(object)) {
                    object->AddChild(droppedObject);
                }
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Children
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
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_MESH,
        InspectorWidgets::DrawMeshComponentWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_RIGIDBODY,
        InspectorWidgets::DrawRigidbodyComponentWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_ZONE,
        InspectorWidgets::DrawZoneComponentWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_CAMERA,
        InspectorWidgets::DrawCameraComponentWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_BILLBOARD,
        InspectorWidgets::DrawBillboardComponentWidget);
    InspectorRegistry::RegisterInspectorFunction(
        Syngine::DefaultComponents::SYN_COMPONENT_PLAYER,
        InspectorWidgets::DrawPlayerComponentWidget);
}

void UI::Draw(int frameNum) {
    if (!bgfx::isValid(m_logoTexture)) {
        m_logoTexture = Syngine::UI::Debug::ImGui_ImplBgfx::LoadTex(
            "imgs/imgs.spk", "builtin/Syngine_Logo_Banner_Rounded.png");
    }

    if (frameNum == 1) {
        _RegisterInspectorWidgets();
        Syngine::Logger::RegisterCallback(_LogMsgCb);
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
    ImGui::DockBuilderDockWindow("Game", dockSpace);

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

        if (ImGui::BeginMenu("Add")) {
            if (ImGui::MenuItem("Empty")) {
                _AddGameObject(0, -1);
            }
            if (ImGui::MenuItem("Cube")) {
                _AddGameObject(3, 0);
            }
            if (ImGui::MenuItem("Sphere")) {
                _AddGameObject(3, 1);
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

    if (ImGui::Button("+")) {
        ImGui::OpenPopup("AddGameObjectPopup");
    }
    if (ImGui::BeginPopup("AddGameObjectPopup")) {
        if (ImGui::MenuItem("Empty")) {
            _AddGameObject(0, -1);
        }
        if (ImGui::MenuItem("Cube")) {
            _AddGameObject(3, 0);
        }
        if (ImGui::MenuItem("Sphere")) {
            _AddGameObject(3, 1);
        }
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    static char searchBuffer[128] = "";
    ImGui::InputTextWithHint(
        "Search", "Search2", searchBuffer, sizeof(searchBuffer));

    // Figure out root objects
    const std::unordered_map<int, Syngine::GameObject>& allObjects =
        Syngine::GameObjectRegistry::GetAllGameObjects();
    std::vector<const Syngine::GameObject*> rootObjects;
    for (auto& [id, object] : allObjects) {
        if (!object.GetParent()) {
            rootObjects.push_back(&object);
        }
    }

    ImGui::BeginChild("HierarchyTree", ImVec2(0, 0));

    for (const Syngine::GameObject* rootObject : rootObjects) {
        _DrawHierarchyNode(const_cast<Syngine::GameObject*>(rootObject));
    }

    // If dropped in empty space become a root object (no parent)
    float  treeBottom = ImGui::GetCursorScreenPos().y;
    ImVec2 min        = ImVec2(ImGui::GetWindowPos().x, treeBottom);
    ImVec2 max = ImVec2(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x,
                        ImGui::GetWindowPos().y + ImGui::GetWindowSize().y);
    if (ImGui::BeginDragDropTargetCustom(ImRect(min, max),
                                         ImGui::GetID("HierarchyTree"))) {
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("SYNGINE_GAMEOBJECT")) {
            auto object = *static_cast<Syngine::GameObject**>(payload->Data);

            if (object) object->SetParent(nullptr);
        }

        ImGui::EndDragDropTarget();
    }

    if (ImGui::BeginPopupContextWindow(nullptr,
                                       ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Add Empty GameObject")) {
            _AddGameObject(0, -1);
        }
        if (ImGui::MenuItem("Add Cube")) {
            _AddGameObject(3, 0);
        }
        if (ImGui::MenuItem("Add Sphere")) {
            _AddGameObject(3, 1);
        }
        ImGui::EndPopup();
    }

    ImGui::EndChild();

    ImGui::End();
}

void UI::DrawInspector() {
    ImGui::Begin("Inspector", nullptr, m_wFlags);

    if (m_selectedObject) {
        const char* buf = m_selectedObject->name.c_str();
        if (ImGui::InputText("Name", (char*)buf, 128)) {
            m_selectedObject->name = std::string(buf);
        }

        bool active = m_selectedObject->IsActive();
        if (ImGui::Checkbox("Enabled", &active)) {
            m_selectedObject->SetActive(active);
        }

        if (ImGui::CollapsingHeader("Tags")) {
            auto tags = m_selectedObject->GetTags();
            for (size_t i = 0; i < tags.size(); ++i) {
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::InputText("##tag", &tags[i][0], 128)) {
                    m_selectedObject->SetTags(tags);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    m_selectedObject->RemoveTag(tags[i]);
                    tags.erase(tags.begin() + i);
                    --i;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Tag")) {
                m_selectedObject->AddTag("NewTag");
            }
        }
        ImGui::Separator();

        SynEditor::InspectorRegistry::Draw(*m_selectedObject);

        ImGui::Separator();
        if (ImGui::Button("Add Component")) {
            ImGui::OpenPopup("AddComponentPopup");
        }
        if (ImGui::BeginPopup("AddComponentPopup")) {
            if (ImGui::Selectable("Transform")) {
                if (m_selectedObject) {
                    m_selectedObject
                        ->AddComponent<Syngine::TransformComponent>();
                }
            }
            if (ImGui::Selectable("Mesh")) {
                if (m_selectedObject) {
                    m_selectedObject->AddComponent<Syngine::MeshComponent>();
                }
            }
            if (ImGui::Selectable("Rigidbody")) {
                if (m_selectedObject) {
                    Syngine::RigidbodyParameters p{};
                    m_selectedObject->AddComponent<Syngine::RigidbodyComponent>(
                        p);
                }
            }
            if (ImGui::Selectable("Billboard")) {
                if (m_selectedObject) {
                    m_selectedObject->AddComponent<Syngine::BillboardComponent>(
                        "", "");
                }
            }
            if (ImGui::Selectable("Camera")) {
                if (m_selectedObject) {
                    m_selectedObject->AddComponent<Syngine::CameraComponent>();
                    m_selectedObject->GetComponent<Syngine::CameraComponent>()
                        ->syncToTransform = true;
                }
            }
            if (ImGui::Selectable("Player")) {
                if (m_selectedObject) {
                    m_selectedObject
                        ->AddComponent<Syngine::TransformComponent>();
                    m_selectedObject->AddComponent<Syngine::CameraComponent>();
                    m_selectedObject->AddComponent<Syngine::PlayerComponent>(
                        m_selectedObject
                            ->GetComponent<Syngine::CameraComponent>());
                }
            }
            if (ImGui::Selectable("Zone")) {
                if (m_selectedObject) {
                    m_selectedObject->AddComponent<Syngine::ZoneComponent>(
                        Syngine::ZoneShape::BOX,
                        m_selectedObject
                            ->GetComponent<Syngine::TransformComponent>()
                            ->GetWorldPosition(),
                        Syngine::Math::Vector3(1.0f, 1.0f, 1.0f));
                }
            }
            if (ImGui::Selectable("Directional Light")) {
                if (m_selectedObject) {
                    m_selectedObject
                        ->AddComponent<Syngine::DirectionalLightComponent>(
                            Syngine::Vector3(0.f, -1.0f, 0.f),
                            Syngine::Vector3(1.0f),
                            1.0f);
                }
            }
            ImGui::EndPopup();
        }
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

    if (ImGui::Button("Clear")) {
        m_logMessages.clear();
    }

    static bool autoScroll = true;
    static bool showInfo   = true;
    static bool showWarn   = true;
    static bool showError  = true;
    ImGui::SameLine();
    ImGui::Checkbox("Auto Scroll", &autoScroll);
    ImGui::SameLine();
    ImGui::Checkbox("Info", &showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warn", &showWarn);
    ImGui::SameLine();
    ImGui::Checkbox("Error", &showError);
    ImGui::Separator();

    ImGui::BeginChild("ConsoleOutput",
                      ImVec2(0, 0),
                      ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);

    int i = 0;
    for (const auto& logMessage : m_logMessages) {
        bool shouldDisplay = false;
        if (logMessage.level == "INFO" && showInfo) {
            shouldDisplay = true;
        } else if (logMessage.level == "WARN" && showWarn) {
            shouldDisplay = true;
        } else if (logMessage.level == "ERROR" && showError) {
            shouldDisplay = true;
        }

        if (shouldDisplay) {
            ImGui::PushID(i++);
            ImVec4 color;
            if (logMessage.level == "INFO") {
                color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            } else if (logMessage.level == "WARN") {
                color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f);
            } else if (logMessage.level == "ERROR") {
                color = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
            } else {
                color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            }

            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::Text("[%s] [%s] %s",
                        logMessage.timestamp.c_str(),
                        logMessage.level.c_str(),
                        logMessage.message.c_str());
            ImGui::PopStyleColor();

            if (ImGui::BeginPopupContextItem("LogContextMenu")) {
                if (ImGui::MenuItem("Copy")) {
                    ImGui::SetClipboardText(logMessage.message.c_str());
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }
    }

    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
    ImGui::End();
}

void UI::_LogMsgCb(const std::string& message,
                   Syngine::LogLevel  level,
                   const std::string& timestamp) {
    m_logMessages.push_back(

        { message, SYN_LOGLEVEL_TO_STRING(level), timestamp });
}

} // namespace SynEditor
