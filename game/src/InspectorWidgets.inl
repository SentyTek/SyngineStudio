// ╒═══════════════ InspectorWidgets.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-28                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include "Syngine/Math/Vector3.hpp"
#include <Syngine/Syngine.h>
#include <cfloat>
#include <imgui/imgui.h>

namespace SynEditor {

using InspectorFunction = void (*)(Syngine::GameObject*);

class InspectorRegistry {
    inline static std::unordered_map<Syngine::ComponentTypeID,
                                     InspectorFunction>
        m_componentInspectors;

  public:
    inline static void
    RegisterInspectorFunction(Syngine::ComponentTypeID typeID,
                              InspectorFunction        func) {
        m_componentInspectors[typeID] = func;
    }

    inline static void Draw(Syngine::GameObject& object) {
        for (auto& [typeID, func] : m_componentInspectors) {
            if (object.HasComponent(typeID)) {
                func(&object);
            }
        }
    }
};

class InspectorWidgets {
    // Cached Euler angles (degrees) for whichever object's rotation is being
    // edited. Re-deriving Euler angles from the quaternion every frame causes
    // the widget to fight the user (sign flips / gimbal lock near +-90deg),
    // so we only resync from the quaternion when the selected object changes.
    inline static Syngine::GameObject* s_rotCacheObject = nullptr;
    inline static Syngine::Vector3     s_rotCacheDegs;

    inline static bool InspectorCategory(const char* name,
                                         const char* doclink) {
        ImGui::PushID(name);

        ImVec2 start  = ImGui::GetCursorScreenPos();
        float  width  = ImGui::GetContentRegionAvail().x;
        float  height = ImGui::GetFrameHeight();

        ImGui::InvisibleButton("Header", ImVec2(width - 30.0f, height));

        bool hovered = ImGui::IsItemHovered();
        bool clicked = ImGui::IsItemClicked();

        bool open = ImGui::GetStateStorage()->GetBool(ImGui::GetID(name), true);

        if (clicked) {
            open = !open;
            ImGui::GetStateStorage()->SetBool(ImGui::GetID(name), open);
        }

        // Background
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(start,
                            ImVec2(start.x + width, start.y + height),
                            ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered
                                                       : ImGuiCol_Header));

        draw->AddText(ImVec2(start.x + 4, start.y + 2),
                      ImGui::GetColorU32(ImGuiCol_Text),
                      name);

        ImGui::PopID();

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 24.0f);

        if (ImGui::SmallButton("?")) {
            SDL_OpenURL(doclink);
        }
        ImGui::SetItemTooltip("Open Documentation");

        return open;
    }

  public:
    inline static void DrawTransformWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto tcomp = object->GetComponent<Syngine::TransformComponent>();
        if (!tcomp) return;

        Syngine::Vector3 pos   = tcomp->GetPosition();
        Syngine::Vector3 scale = tcomp->GetScale();

        if (s_rotCacheObject != object) {
            s_rotCacheObject = object;
            s_rotCacheDegs   = tcomp->GetRotationEuler().toDegs();
        }

        bool open =
            InspectorCategory("Transform",
                              "https://github.com/SentyTek/Syngine/blob/main/"
                              "docs/api/transformcomponent_doc.md");

        if (open) {
            ImGui::Indent();

            if (ImGui::DragFloat3(
                    "Position", pos.data(), 0.1f, -FLT_MAX, FLT_MAX, "%.6f")) {
                tcomp->SetPosition(pos);
            }
            ImGui::SetItemTooltip("The position of the object.");

            if (ImGui::DragFloat3("Rotation",
                                  s_rotCacheDegs.data(),
                                  1.0f,
                                  0.f,
                                  360.f,
                                  "%.3f",
                                  ImGuiSliderFlags_WrapAround)) {
                tcomp->SetRotationEuler(s_rotCacheDegs.toRads());
            }
            ImGui::SetItemTooltip(
                "The rotation of the object in Euler angles.");

            if (ImGui::DragFloat3(
                    "Scale", scale.data(), 0.1f, -FLT_MAX, FLT_MAX, "%.3f")) {
                tcomp->SetScale(scale);
            }
            ImGui::SetItemTooltip("The scale of the object.");

            ImGui::Unindent();
        }
    };

    inline static void DrawDirectionalLightWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto dlight =
            object->GetComponent<Syngine::DirectionalLightComponent>();
        if (!dlight) return;

        Syngine::Vec3 direction = dlight->GetDirection().toDegs();
        Syngine::Vec3 color     = dlight->GetColor();
        float         intensity = dlight->GetIntensity();
        bool          enabled   = dlight->IsEnabled();

        bool open =
            InspectorCategory("Directional Light",
                              "https://github.com/SentyTek/Syngine/blob/main/"
                              "docs/api/directionallightcomponent_doc.md");

        if (open) {
            ImGui::Indent();

            if (ImGui::DragFloat3("Direction",
                                  direction.data(),
                                  1.0f,
                                  -FLT_MAX,
                                  FLT_MAX,
                                  "%.6f")) {
                dlight->SetDirection(direction.toRads());
            }
            ImGui::SetItemTooltip("The direction of the directional light.");

            if (ImGui::ColorEdit3("Color", color.data())) {
                dlight->SetColor(color);
            }
            ImGui::SetItemTooltip("The color of the directional light.");

            if (ImGui::DragFloat(
                    "Intensity", &intensity, 0.1f, 0.0f, FLT_MAX, "%.3f")) {
                dlight->SetIntensity(intensity);
            }
            ImGui::SetItemTooltip("The intensity of the directional light.");

            if (ImGui::Checkbox("Enabled", &enabled)) {
                dlight->SetEnabled(enabled);
            }
            ImGui::SetItemTooltip("Enable or disable the directional light.");

            ImGui::Unindent();
        }
    };
};

} // namespace SynEditor
