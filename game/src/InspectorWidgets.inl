// ╒═══════════════ InspectorWidgets.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-28                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include "Syngine/GameObjects/Component.h"
#include "Syngine/GameObjects/Components/ZoneComponent.h"
#include "Syngine/Graphics/Rendering/Renderer.h"
#include <Syngine/Syngine.h>
#include <cfloat>
#include <imgui/imgui.h>
#include <stdbool.h>

#define JOLTMOTION_TO_STRING(motion)                                           \
    ((motion) == JPH::EMotionType::Static      ? "Static"                      \
     : (motion) == JPH::EMotionType::Kinematic ? "Kinematic"                   \
     : (motion) == JPH::EMotionType::Dynamic   ? "Dynamic"                     \
                                               : "Unknown")

#define JOLTOBJECTLAYER_TO_STRING(layer)                                       \
    ((layer) == Syngine::Layers::NON_MOVING ? "Non Moving"                     \
     : (layer) == Syngine::Layers::MOVING   ? "Moving"                         \
                                            : "Unknown")

#define PHYSICSSHAPE_TO_STRING(shape)                                          \
    ((shape) == Syngine::PhysicsShapes::SPHERE            ? "Sphere"           \
     : (shape) == Syngine::PhysicsShapes::BOX             ? "Box"              \
     : (shape) == Syngine::PhysicsShapes::CAPSULE         ? "Capsule"          \
     : (shape) == Syngine::PhysicsShapes::CAPSULE_TAPERED ? "Tapered Capsule"  \
     : (shape) == Syngine::PhysicsShapes::CYLINDER        ? "Cylinder"         \
     : (shape) == Syngine::PhysicsShapes::CYLINDER_TAPERED                     \
         ? "Tapered Cylinder"                                                  \
     : (shape) == Syngine::PhysicsShapes::CONE        ? "Cone"                 \
     : (shape) == Syngine::PhysicsShapes::CONVEX_HULL ? "Convex Hull"          \
     : (shape) == Syngine::PhysicsShapes::PLANE       ? "Plane"                \
     : (shape) == Syngine::PhysicsShapes::MESH        ? "Mesh"                 \
     : (shape) == Syngine::PhysicsShapes::COMPOUND    ? "Compound"             \
                                                      : "Unknown")

#define BILLBOARD_MODE_TO_STRING(mode)                                         \
    ((mode) == Syngine::BillboardMode::FIXED            ? "Fixed"              \
     : (mode) == Syngine::BillboardMode::CAMERA_ALIGNED ? "Camera"             \
     : (mode) == Syngine::BillboardMode::AXIS_Y_ALIGNED ? "Y-Axis"             \
                                                        : "Unknown")

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
                ImGui::Separator();
            }
        }
    }
};

class InspectorWidgets {
    // Cached Euler angles (degrees) for whichever object's rotation is being
    // edited. Re-deriving Euler angles from the quaternion every frame causes
    // the widget to fight the user (sign flips / gimbal lock near +-90deg),
    // so we only resync from the quaternion when the selected object changes.
    inline static Syngine::GameObject* s_rotCacheObject   = nullptr;
    inline static bool                 s_objectLocalState = false;
    inline static Syngine::Vector3     s_rotCacheDegs;

    // helper function to create the custom collapsing header
    template <typename F, typename G>
    inline static bool InspectorCategory(const char* name,
                                         const char* doclink,
                                         F           resetFunc,
                                         G           removeFunc,
                                         bool*       enabled) {
        ImGui::PushID(name);

        ImVec2 start  = ImGui::GetCursorScreenPos();
        float  width  = ImGui::GetContentRegionAvail().x;
        float  height = ImGui::GetFrameHeight();

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 20.0f);
        ImGui::InvisibleButton("Header", ImVec2(width - 80.0f, height));

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

        draw->AddText(ImVec2(start.x + 25, start.y + 2),
                      ImGui::GetColorU32(ImGuiCol_Text),
                      name);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 40.0f);
        if (ImGui::SmallButton("?")) {
            SDL_OpenURL(doclink);
        }
        ImGui::SetItemTooltip("Open Documentation");

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 25.0f);
        if (ImGui::SmallButton("R")) {
            resetFunc();
        }
        ImGui::SetItemTooltip("Reset %s", name);

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10.0f);
        if (ImGui::SmallButton("X")) {
            removeFunc();
        }
        ImGui::SetItemTooltip("Remove %s", name);

        // Enabled checkbox for the component
        ImGui::SameLine(10.0f);
        ImGui::Checkbox("", enabled);

        ImGui::PopID();

        return open;
    }

  public:
    inline static void DrawTransformWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto tcomp = object->GetComponent<Syngine::TransformComponent>();
        if (!tcomp) return;

        ImGui::PushID("TransformComponent");
        bool useLocal = ImGui::GetStateStorage()->GetBool(
            ImGui::GetID("Use Local Transform"), true);

        if (s_rotCacheObject != object || s_objectLocalState != useLocal) {
            s_rotCacheObject   = object;
            s_rotCacheDegs     = tcomp->GetRotationEuler().toDegs();
            s_objectLocalState = useLocal;
        }

        auto resetFunc = [&tcomp]() {
            tcomp->SetRotationEuler(Syngine::Vector3());
            tcomp->SetPosition(Syngine::Vector3());
            tcomp->SetScale(Syngine::Vector3(1.0f));
            s_rotCacheObject = nullptr; // force rotation cache to update
        };

        bool enabled = tcomp->IsEnabled();
        bool open    = InspectorCategory(
            "Transform",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/transformcomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_TRANSFORM);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_TRANSFORM)) {
            ImGui::PopID();
            return;
        }
        tcomp->SetEnabled(enabled);

        ImGui::Indent();

        if (ImGui::Checkbox("Use Local Transform", &useLocal)) {
            ImGui::GetStateStorage()->SetBool(
                ImGui::GetID("Use Local Transform"), useLocal);
        }

        Syngine::Vector3 pos   = tcomp->GetPosition();
        Syngine::Vector3 scale = tcomp->GetScale();
        if (!useLocal) {
            pos            = tcomp->GetWorldPosition();
            scale          = tcomp->GetWorldScale();
            s_rotCacheDegs = tcomp->GetWorldRotationEuler().toDegs();
        }

        if (ImGui::DragFloat3(
                "Position", pos.data(), 0.1f, -FLT_MAX, FLT_MAX, "%.6f")) {
            if (useLocal) {
                tcomp->SetPosition(pos);
            } else {
                tcomp->SetWorldPosition(pos);
            }
        }
        ImGui::SetItemTooltip("The position of the object.");

        if (ImGui::DragFloat3("Rotation",
                              s_rotCacheDegs.data(),
                              1.0f,
                              0.f,
                              360.f,
                              "%.3f",
                              ImGuiSliderFlags_WrapAround)) {
            if (useLocal) {
                tcomp->SetRotationEuler(s_rotCacheDegs.toRads());
            } else {
                tcomp->SetWorldRotationEuler(s_rotCacheDegs.toRads());
            }
        }
        ImGui::SetItemTooltip("The rotation of the object in Euler angles.");

        if (ImGui::DragFloat3(
                "Scale", scale.data(), 0.1f, -FLT_MAX, FLT_MAX, "%.3f")) {
            if (useLocal) {
                tcomp->SetScale(scale);
            } else {
                tcomp->SetWorldScale(scale);
            }
        }
        ImGui::SetItemTooltip("The scale of the object.");

        ImGui::Unindent();
        ImGui::PopID();
    };

    inline static void DrawDirectionalLightWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto dlight =
            object->GetComponent<Syngine::DirectionalLightComponent>();
        if (!dlight) return;

        Syngine::Vec3 direction = dlight->GetDirection().toDegs();
        Syngine::Vec3 color     = dlight->GetColor();
        float         intensity = dlight->GetIntensity();

        auto resetFunc = [&dlight]() {
            dlight->SetDirection(Syngine::Vec3(0.0f, -1.0f, 0.0f));
            dlight->SetColor(Syngine::Vec3(1.0f, 1.0f, 1.0f));
            dlight->SetIntensity(1.0f);
            dlight->SetEnabled(true);
        };

        ImGui::PushID("DirectionalLightComponent");
        bool enabled = dlight->IsEnabled();
        bool open    = InspectorCategory(
            "Directional Light",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/directionallightcomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(
                    Syngine::SYN_COMPONENT_LIGHT_DIRECTIONAL);
                return;
            },
            &enabled);
        if (!open ||
            !object->HasComponent(Syngine::SYN_COMPONENT_LIGHT_DIRECTIONAL)) {
            ImGui::PopID();
            return;
        }
        dlight->SetEnabled(enabled);

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

        ImGui::Unindent();
        ImGui::PopID();
    };

    inline static void DrawMeshComponentWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto meshComp = object->GetComponent<Syngine::MeshComponent>();
        if (!meshComp) return;

        auto resetFunc = [&meshComp]() { meshComp->SetLoadTextures(true); };

        ImGui::PushID("MeshComponent");
        bool enabled = meshComp->IsEnabled();
        bool open    = InspectorCategory(
            "Mesh",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/meshcomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_MESH);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_MESH)) {
            ImGui::PopID();
            return;
        }
        meshComp->SetEnabled(enabled);

        ImGui::Indent();
        std::string bundlePath   = meshComp->GetBundlePath();
        std::string modelPath    = meshComp->GetModelPath();
        bool        loadTextures = meshComp->GetLoadTextures();

        bool receiveShadows = meshComp->receiveShadows;
        bool castShadows    = meshComp->castShadows;

        ImGui::Text("Bundle Path: %s", bundlePath.c_str());
        ImGui::Text("Model Path: %s", modelPath.c_str());
        if (ImGui::Checkbox("Load Textures", &loadTextures)) {
            meshComp->SetLoadTextures(loadTextures);
        }

        if (ImGui::Checkbox("Receive Shadows", &receiveShadows)) {
            meshComp->receiveShadows = receiveShadows;
        }
        if (ImGui::Checkbox("Cast Shadows", &castShadows)) {
            meshComp->castShadows = castShadows;
        }

        ImGui::Unindent();
        ImGui::PopID();
    };

    inline static void
    DrawRigidbodyComponentWidget(Syngine::GameObject* object) {
        if (!object) return;
        auto rigidBodyComp =
            object->GetComponent<Syngine::RigidbodyComponent>();
        if (!rigidBodyComp) return;

        auto resetFunc = [&rigidBodyComp]() {
            rigidBodyComp->SetCurrentParameters({});
        };

        ImGui::PushID("RigidbodyComponent");
        bool enabled = rigidBodyComp->IsEnabled();
        bool open    = InspectorCategory(
            "Rigidbody",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/rigidbodycomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_RIGIDBODY);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_RIGIDBODY)) {
            ImGui::PopID();
            return;
        }
        rigidBodyComp->SetEnabled(enabled);

        ImGui::Indent();

        Syngine::RigidbodyParameters params =
            rigidBodyComp->GetCurrentParameters();

        JPH::EMotionType motionType  = params.motionType;
        JPH::ObjectLayer objectLayer = params.layer;

        if (ImGui::BeginCombo("Motion Type",
                              JOLTMOTION_TO_STRING(motionType))) {
            if (ImGui::Selectable("Static",
                                  motionType == JPH::EMotionType::Static)) {
                motionType        = JPH::EMotionType::Static;
                params.motionType = motionType;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Kinematic",
                                  motionType == JPH::EMotionType::Kinematic)) {
                motionType        = JPH::EMotionType::Kinematic;
                params.motionType = motionType;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Dynamic",
                                  motionType == JPH::EMotionType::Dynamic)) {
                motionType        = JPH::EMotionType::Dynamic;
                params.motionType = motionType;
                rigidBodyComp->SetCurrentParameters(params);
            }
            ImGui::EndCombo();
        }
        if (ImGui::BeginCombo("Object Layer",
                              JOLTOBJECTLAYER_TO_STRING(objectLayer))) {
            if (ImGui::Selectable("Non Moving",
                                  objectLayer == Syngine::Layers::NON_MOVING)) {
                objectLayer  = Syngine::Layers::NON_MOVING;
                params.layer = objectLayer;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Moving",
                                  objectLayer == Syngine::Layers::MOVING)) {
                objectLayer  = Syngine::Layers::MOVING;
                params.layer = objectLayer;
                rigidBodyComp->SetCurrentParameters(params);
            }
            ImGui::EndCombo();
        }
        ImGui::Separator();

        float friction    = rigidBodyComp->GetFriction();
        float restitution = rigidBodyComp->GetRestitution();

        if (ImGui::SliderFloat("Friction", &friction, 0.0f, 1.0f)) {
            rigidBodyComp->SetFriction(friction);
        }
        ImGui::SetItemTooltip("The friction coefficient of the rigidbody.");

        if (ImGui::SliderFloat("Bounce", &restitution, 0.0f, 1.0f)) {
            rigidBodyComp->SetRestitution(restitution);
        }
        ImGui::SetItemTooltip("The bounciness (restitution) of the rigidbody.");
        ImGui::Separator();

        ImGui::Text("Collision Shape");
        ImGui::Indent();

        Syngine::PhysicsShapes shape = params.shape;
        if (ImGui::BeginCombo("Shape", PHYSICSSHAPE_TO_STRING(shape))) {
            if (ImGui::Selectable("Box",
                                  shape == Syngine::PhysicsShapes::BOX)) {
                params.shape = Syngine::PhysicsShapes::BOX;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Sphere",
                                  shape == Syngine::PhysicsShapes::SPHERE)) {
                params.shape = Syngine::PhysicsShapes::SPHERE;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Cylinder",
                                  shape == Syngine::PhysicsShapes::CYLINDER)) {
                params.shape = Syngine::PhysicsShapes::CYLINDER;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Capsule",
                                  shape == Syngine::PhysicsShapes::CAPSULE)) {
                params.shape = Syngine::PhysicsShapes::CAPSULE;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Mesh",
                                  shape == Syngine::PhysicsShapes::MESH)) {
                params.shape = Syngine::PhysicsShapes::MESH;
                rigidBodyComp->SetCurrentParameters(params);
            }
            if (ImGui::Selectable("Compound",
                                  shape == Syngine::PhysicsShapes::COMPOUND)) {
                params.shape = Syngine::PhysicsShapes::COMPOUND;
                rigidBodyComp->SetCurrentParameters(params);
            }
            ImGui::EndCombo();
        }

        // The shape params depend on the selected collision shape.
        auto drawShapeWidget = [&](Syngine::PhysicsShapes shape, size_t index) {
            if (index > 100) // 100 is used by the main shape, so compound
                             // parts can only go up to 99
                throw std::out_of_range("Compound part index exceeds the "
                                        "maximum allowed value of 99.");
            switch (shape) {
            case Syngine::PhysicsShapes::BOX: {
                float dataA = params.shapeParameters.x();
                float dataB = params.shapeParameters.y();
                float dataC = params.shapeParameters.z();
                if (index < 100) {
                    dataA = params.compoundParts[index].shapeParameters.x();
                    dataB = params.compoundParts[index].shapeParameters.y();
                    dataC = params.compoundParts[index].shapeParameters.z();
                }
                if (ImGui::DragFloat("X", &dataA, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, dataC };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        dataC };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                if (ImGui::DragFloat("Y", &dataB, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, dataC };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        dataC };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                if (ImGui::DragFloat("Z", &dataC, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, dataC };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        dataC };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                break;
            }
            case Syngine::PhysicsShapes::SPHERE: {
                float* data = params.shapeParameters.data();
                if (index < 100) {
                    data[0] = params.compoundParts[index].shapeParameters.x();
                }
                if (ImGui::DragFloat("Radius", data, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { data[0] };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = {
                            data[0]
                        };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                break;
            }
            case Syngine::PhysicsShapes::CYLINDER: {
                float dataA = params.shapeParameters.x();
                float dataB = params.shapeParameters.y();
                if (index < 100) {
                    dataA = params.compoundParts[index].shapeParameters.x();
                    dataB = params.compoundParts[index].shapeParameters.y();
                }
                if (ImGui::DragFloat("Radius", &dataA, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, 0.0f };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        0.0f };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                if (ImGui::DragFloat("Height", &dataB, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, 0.0f };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        0.0f };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                break;
            }
            case Syngine::PhysicsShapes::CAPSULE: {
                float dataA = params.shapeParameters.x();
                float dataB = params.shapeParameters.y();
                if (index < 100) {
                    dataA = params.compoundParts[index].shapeParameters.x();
                    dataB = params.compoundParts[index].shapeParameters.y();
                }
                if (ImGui::DragFloat("Radius", &dataA, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, 0.0f };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        0.0f };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                if (ImGui::DragFloat("Height", &dataB, 0.1f, 0.01f, 1000.0f)) {
                    params.shapeParameters = { dataA, dataB, 0.0f };
                    if (index < 100) {
                        params.compoundParts[index].shapeParameters = { dataA,
                                                                        dataB,
                                                                        0.0f };
                    }
                    rigidBodyComp->SetCurrentParameters(params);
                }
                break;
            }
            default: break;
            }
        };

        ImGui::Text("Shape Parameter(s):");
        switch (shape) {
        case Syngine::PhysicsShapes::BOX:
        case Syngine::PhysicsShapes::SPHERE:
        case Syngine::PhysicsShapes::CAPSULE:
        case Syngine::PhysicsShapes::CYLINDER:
            drawShapeWidget(shape, 100);
            break;
        case Syngine::PhysicsShapes::MESH:
            // Display mesh-specific parameters here.
            break;
        case Syngine::PhysicsShapes::COMPOUND: {
            // Compound shapes are difficult.
            ImGui::Text("Parts");
            ImGui::SameLine();
            bool modified = false;
            if (ImGui::Button("Add Part")) {
                params.compoundParts.push_back({});
                rigidBodyComp->SetCurrentParameters(params);
                modified = true;
            }

            for (size_t i = 0; i < params.compoundParts.size(); ++i) {
                ImGui::Separator();
                ImGui::PushID(std::to_string(i).c_str());
                if (ImGui::CollapsingHeader(
                        ("Part " + std::to_string(i)).c_str())) {
                    if (ImGui::Button("Remove Part")) {
                        params.compoundParts.erase(
                            params.compoundParts.begin() + i);
                        rigidBodyComp->SetCurrentParameters(params);
                        if (i > 0) {
                            --i; // Adjust the index after removal to avoid
                                 // skipping the next element.
                        } else {
                            ImGui::PopID();
                            break; // No more elements to adjust, exit the
                                   // loop.
                        }
                        modified = true;
                    }
                    auto& part = params.compoundParts[i];

                    if (ImGui::BeginCombo("Shape",
                                          PHYSICSSHAPE_TO_STRING(part.shape))) {
                        if (ImGui::Selectable(
                                "Box",
                                part.shape == Syngine::PhysicsShapes::BOX)) {
                            part.shape = Syngine::PhysicsShapes::BOX;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        if (ImGui::Selectable(
                                "Sphere",
                                part.shape == Syngine::PhysicsShapes::SPHERE)) {
                            part.shape = Syngine::PhysicsShapes::SPHERE;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        if (ImGui::Selectable(
                                "Cylinder",
                                part.shape ==
                                    Syngine::PhysicsShapes::CYLINDER)) {
                            part.shape = Syngine::PhysicsShapes::CYLINDER;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        if (ImGui::Selectable(
                                "Capsule",
                                part.shape ==
                                    Syngine::PhysicsShapes::CAPSULE)) {
                            part.shape = Syngine::PhysicsShapes::CAPSULE;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        if (ImGui::Selectable(
                                "Mesh",
                                part.shape == Syngine::PhysicsShapes::MESH)) {
                            part.shape = Syngine::PhysicsShapes::MESH;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        if (ImGui::Selectable(
                                "Compound",
                                part.shape ==
                                    Syngine::PhysicsShapes::COMPOUND)) {
                            part.shape = Syngine::PhysicsShapes::COMPOUND;
                            params.compoundParts[i] = part;
                            rigidBodyComp->SetCurrentParameters(params);
                        }
                        ImGui::EndCombo();
                    }
                    drawShapeWidget(part.shape, i);

                    float* pos = part.position.data();
                    float* rot =
                        Syngine::Vector3(part.rotation.toEulerAngles()).data();
                    if (ImGui::DragFloat3(
                            "Position", pos, 0.1f, -1000.0f, 1000.0f)) {
                        part.position           = { pos[0], pos[1], pos[2] };
                        params.compoundParts[i] = part;
                        modified                = true;
                    }
                    if (ImGui::DragFloat3(
                            "Rotation", rot, 0.1f, -360.0f, 360.0f)) {
                        part.rotation = Syngine::Quaternion(
                            Syngine::Vector3(rot[0], rot[1], rot[2]));
                        params.compoundParts[i] = part;
                        modified                = true;
                    }
                }
                ImGui::PopID();
            }

            if (modified) {
                rigidBodyComp->SetCurrentParameters(params);
            }
            break;
        }
        default: break;
        }

        ImGui::Unindent();

        ImGui::Unindent();
        ImGui::PopID();
    };

    inline static void DrawZoneComponentWidget(Syngine::GameObject* object) {
        auto zoneComp = object->GetComponent<Syngine::ZoneComponent>();
        if (!zoneComp) return;

        auto resetFunc = [&zoneComp, object]() {
            if (auto t = object->GetComponent<Syngine::TransformComponent>()) {
                zoneComp->SetPosition(t->GetPosition());
            } else {
                zoneComp->SetPosition(Syngine::Vector3(0.0f));
            }
            zoneComp->SetSize(Syngine::Math::Vector3(1.0f));
        };

        ImGui::PushID("ZoneComponent");
        bool enabled = zoneComp->IsEnabled();
        bool open    = InspectorCategory(
            "Zone",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/zonecomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_ZONE);
                return;
            },
            &enabled);
        if (!object->HasComponent(Syngine::SYN_COMPONENT_ZONE)) {
            ImGui::PopID();
            return;
        }
        zoneComp->SetEnabled(enabled);

        if (!open) {
            ImGui::PopID();
            return;
        };

        ImGui::Indent();

        float*             pos     = zoneComp->GetPosition().data();
        float*             size    = zoneComp->GetSize().data();
        bool               oneShot = zoneComp->IsOneShot();
        Syngine::ZoneShape shape   = zoneComp->GetShape();

        const char* shapeItems[] = { "Box", "Sphere" };
        int         shapeIndex   = static_cast<int>(shape);
        if (ImGui::Combo(
                "Shape", &shapeIndex, shapeItems, IM_ARRAYSIZE(shapeItems))) {
            shape = static_cast<Syngine::ZoneShape>(shapeIndex);
            zoneComp->SetShape(shape);
        }
        if (ImGui::DragFloat3("Position", pos, 0.1f, -1000.0f, 1000.0f)) {
            zoneComp->SetPosition({ pos[0], pos[1], pos[2] });
        }

        switch (shape) {
        case Syngine::ZoneShape::BOX:
            if (ImGui::DragFloat3("Size", size, 0.1f, 0.0f, 1000.0f)) {
                zoneComp->SetSize({ size[0], size[1], size[2] });
            }
            break;
        case Syngine::ZoneShape::SPHERE:
            if (ImGui::DragFloat("Radius", &size[0], 0.1f, 0.0f, 1000.0f)) {
                zoneComp->SetSize({ size[0], 0.0f, 0.0f });
            }
            break;
        default: break;
        }

        if (ImGui::Checkbox("One Shot", &oneShot)) {
            zoneComp->SetOneShot(oneShot);
        }

        if (ImGui::CollapsingHeader("Tags")) {
            auto tags = zoneComp->GetTags();
            for (size_t i = 0; i < tags.size(); ++i) {
                ImGui::PushID(
                    static_cast<int>(i)); // Push a unique ID for each tag
                if (ImGui::InputText("##tag", &tags[i][0], 128)) {
                    zoneComp->SetTags(tags);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove")) {
                    zoneComp->RemoveTag(tags[i]);
                    tags.erase(tags.begin() + i);
                    --i;
                }
                ImGui::PopID();
            }
            if (ImGui::Button("Add Tag")) {
                zoneComp->AddTag("NewTag");
            }
        }

        ImGui::Unindent();
        ImGui::PopID();
    }

    inline static void DrawCameraComponentWidget(Syngine::GameObject* object) {
        auto cameraComp = object->GetComponent<Syngine::CameraComponent>();
        if (!cameraComp) return;

        auto resetFunc = [cameraComp]() {
            if (cameraComp) {
                cameraComp->SetPosition(Syngine::Vector3(0.0f));
                cameraComp->SetAngles(Syngine::Vector2(0.0f));
                cameraComp->SetFOV(70.0f);
            }
        };

        ImGui::PushID("CameraComponent");
        bool enabled = cameraComp->IsEnabled();
        bool open    = InspectorCategory(
            "Camera",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/cameracomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_CAMERA);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_CAMERA)) {
            ImGui::PopID();
            return;
        }

        ImGui::Indent();

        float* pos             = cameraComp->GetPosition().data();
        float* angles          = cameraComp->GetAngles().data();
        float  fov             = cameraComp->GetFOV();
        float  nearClip        = cameraComp->GetNearPlane();
        float  farClip         = cameraComp->GetFarPlane();
        bool   syncToTransform = cameraComp->syncToTransform;
        bool   isMainCamera    = cameraComp->isMainCamera;

        if (ImGui::DragFloat3("Position", pos, 0.1f, -1000.0f, 1000.0f)) {
            cameraComp->SetPosition({ pos[0], pos[1], pos[2] });
        }
        if (ImGui::DragFloat2("Angles", angles, 0.1f, -180.0f, 180.0f)) {
            cameraComp->SetAngles({ angles[0], angles[1] });
        }
        if (ImGui::DragFloat("FOV", &fov, 0.1f, 1.0f, 180.0f)) {
            cameraComp->SetFOV(fov);
        }
        if (ImGui::DragFloat("Near Clip", &nearClip, 0.1f, 0.01f, 1000.0f)) {
            cameraComp->SetNearPlane(nearClip);
        }
        if (ImGui::DragFloat("Far Clip", &farClip, 0.1f, 0.01f, 10000.0f)) {
            cameraComp->SetFarPlane(farClip);
        }
        if (ImGui::Checkbox("Sync to Transform", &syncToTransform)) {
            cameraComp->syncToTransform = syncToTransform;
        }
        if (ImGui::Checkbox("Main Camera", &isMainCamera)) {
            cameraComp->isMainCamera = isMainCamera;
            // check for other cameras that might be set as main and update them
            // accordingly
            if (isMainCamera) {
                for (auto& obj :
                     Syngine::GameObjectRegistry::GetGameObjectsWithComponent(
                         Syngine::SYN_COMPONENT_CAMERA)) {
                    auto comp = obj->GetComponent<Syngine::CameraComponent>();
                    if (comp != cameraComp && comp->isMainCamera) {
                        comp->isMainCamera = false;
                    }
                }
            }
        }

        ImGui::Unindent();
        ImGui::PopID();
    };

    inline static void
    DrawBillboardComponentWidget(Syngine::GameObject* object) {
        auto billboardComp =
            object->GetComponent<Syngine::BillboardComponent>();
        if (!billboardComp) return;

        auto resetFunc = [billboardComp]() {
            if (billboardComp) {
                // Reset logic for the billboard component goes here
            }
        };

        ImGui::PushID("BillboardComponent");
        bool enabled = billboardComp->IsEnabled();
        bool open    = InspectorCategory(
            "Billboard",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/billboardcomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_BILLBOARD);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_BILLBOARD)) {
            ImGui::PopID();
            return;
        }

        ImGui::Indent();

        Syngine::BillboardMode mode        = billboardComp->GetMode();
        float*                 rot         = billboardComp->GetRot().data();
        std::string            bundlePath  = billboardComp->GetBundlePath();
        std::string            texturePath = billboardComp->GetTexturePath();

        bool receiveShadows = billboardComp->receiveShadows;
        bool receiveSun     = billboardComp->receiveSunLight;

        ImGui::Text("Bundle Path: %s", bundlePath.c_str());
        ImGui::Text("Texture Path: %s", texturePath.c_str());

        if (ImGui::BeginCombo("Mode", BILLBOARD_MODE_TO_STRING(mode))) {
            if (ImGui::Selectable("Fixed",
                                  mode == Syngine::BillboardMode::FIXED)) {
                billboardComp->SetMode(Syngine::BillboardMode::FIXED);
            }
            if (ImGui::Selectable(
                    "Camera", mode == Syngine::BillboardMode::CAMERA_ALIGNED)) {
                billboardComp->SetMode(Syngine::BillboardMode::CAMERA_ALIGNED);
            }
            if (ImGui::Selectable(
                    "Y-Axis", mode == Syngine::BillboardMode::AXIS_Y_ALIGNED)) {
                billboardComp->SetMode(Syngine::BillboardMode::AXIS_Y_ALIGNED);
            }
            ImGui::EndCombo();
        }
        if (ImGui::DragFloat3("Rotation", rot, 0.1f, -180.0f, 180.0f)) {
            billboardComp->SetRot({ rot[0], rot[1], rot[2] });
        }

        ImGui::Checkbox("Receive Shadows", &receiveShadows);
        ImGui::Checkbox("Receive Sun Light", &receiveSun);
        billboardComp->receiveShadows  = receiveShadows;
        billboardComp->receiveSunLight = receiveSun;

        ImGui::Unindent();
        ImGui::PopID();
    }

    inline static void DrawPlayerComponentWidget(Syngine::GameObject* object) {
        auto playerComp = object->GetComponent<Syngine::PlayerComponent>();
        if (!playerComp) return;

        auto resetFunc = [playerComp]() { playerComp->Reset(); };

        ImGui::PushID("PlayerComponent");
        bool enabled = playerComp->IsEnabled();
        bool open    = InspectorCategory(
            "Player Controller",
            "https://github.com/SentyTek/Syngine/blob/main/"
            "docs/api/playercomponent_doc.md",
            resetFunc,
            [&object]() {
                object->RemoveComponent(Syngine::SYN_COMPONENT_PLAYER);
                return;
            },
            &enabled);
        if (!open || !object->HasComponent(Syngine::SYN_COMPONENT_PLAYER)) {
            ImGui::PopID();
            return;
        }
        playerComp->SetEnabled(enabled);

        ImGui::Indent();

        float maxPitchAngle  = playerComp->maxPitchAngle;
        float sprintMult     = playerComp->sprintMult;
        float crouchSpeed    = playerComp->crouchSpeed;
        float moveSpeed      = playerComp->moveSpeed;
        float standHeight    = playerComp->standHeight;
        float crouchHeight   = playerComp->crouchHeight;
        float playerRadius   = playerComp->playerRadius;
        float mouseSens      = playerComp->mouseSens;
        float normalFov      = playerComp->normalFov;
        float sprintFov      = playerComp->sprintFov;
        float crouchFov      = playerComp->crouchFov;
        float slideFov       = playerComp->slideFov;
        float slideDecay     = playerComp->slideDecay;
        float slideSpeedMult = playerComp->slideSpeedMult;

        bool enableSprinting = playerComp->enableSprinting;
        bool enableCrouching = playerComp->enableCrouching;
        bool enableSliding   = playerComp->enableSliding;
        bool enableJumping   = playerComp->enableJumping;
        bool enableMovement  = playerComp->enableMovement;

        bool haveAnyChanged = false;

        /* clang-format off */
        if (ImGui::Checkbox("Enable Sprinting", &enableSprinting)) haveAnyChanged = true;
        if (ImGui::Checkbox("Enable Crouching", &enableCrouching)) haveAnyChanged = true;
        if (ImGui::Checkbox("Enable Sliding", &enableSliding)) haveAnyChanged = true;
        if (ImGui::Checkbox("Enable Jumping", &enableJumping)) haveAnyChanged = true;
        if (ImGui::Checkbox("Enable Movement", &enableMovement)) haveAnyChanged = true;
        playerComp->enableSprinting = enableSprinting;
        playerComp->enableCrouching = enableCrouching;
        playerComp->enableSliding   = enableSliding;
        playerComp->enableJumping   = enableJumping;
        playerComp->enableMovement  = enableMovement;

        if (ImGui::SliderFloat("Max Pitch Angle", &maxPitchAngle, 0.f, 89.f)) haveAnyChanged = true;
        ImGui::SetItemTooltip("The max pitch the camera can go to.");
        if (ImGui::SliderFloat("Sprint Speed Multiplier", &sprintMult, 0.f, 5.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Crouch Speed", &crouchSpeed, 0.f, 5.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Move Speed", &moveSpeed, 0.f, 50.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Stand Height", &standHeight, playerRadius * 2.f + 0.01f, 10.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Crouch Height", &crouchHeight, playerRadius * 2.f + 0.01f, 10.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Player Radius", &playerRadius, 0.01f, 9.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Mouse Sensitivity", &mouseSens, 0.f, 5.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Normal FOV", &normalFov, 0.f, 180.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Sprint FOV", &sprintFov, 0.f, 180.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Crouch FOV", &crouchFov, 0.f, 180.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Slide FOV", &slideFov, 0.f, 180.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Slide Decay", &slideDecay, 0.f, 5.f)) haveAnyChanged = true;
        if (ImGui::SliderFloat("Slide Speed Multiplier", &slideSpeedMult, 0.f, 5.f)) haveAnyChanged = true;
        standHeight  = Syngine::Math::Clampf(standHeight, playerRadius * 2.f + 0.01f, 10.f);
        crouchHeight = Syngine::Math::Clampf(crouchHeight, playerRadius * 2.f + 0.01f, 10.f);
        playerRadius = Syngine::Math::Clampf(playerRadius, 0.01f, 9.f);
        playerComp->sprintMult     = sprintMult;
        playerComp->crouchSpeed    = crouchSpeed;
        playerComp->moveSpeed      = moveSpeed;
        playerComp->standHeight    = standHeight;
        playerComp->crouchHeight   = crouchHeight;
        playerComp->playerRadius   = playerRadius;
        playerComp->mouseSens      = mouseSens;
        playerComp->normalFov      = normalFov;
        playerComp->sprintFov      = sprintFov;
        playerComp->crouchFov      = crouchFov;
        playerComp->slideFov       = slideFov;
        playerComp->slideDecay     = slideDecay;
        playerComp->slideSpeedMult = slideSpeedMult;

        /* clang-format on */
        if (haveAnyChanged) {
            playerComp->RebuildCharacter();
        }

        ImGui::Unindent();
        ImGui::PopID();
    }
};

} // namespace SynEditor
