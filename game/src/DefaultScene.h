// ╒═════════════════════ DefaultScene.h ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-21                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once
#include "Syngine/GameObjects/Components/MeshComponent.h"
#include "Syngine/GameObjects/Components/RigidbodyComponent.h"
#include "Syngine/GameObjects/Components/TransformComponent.h"
#include "Syngine/GameObjects/GameObject.h"
#include "Syngine/Scene/GameObjectRegistry.h"
#include <Syngine/Syngine.h>

using namespace Syngine;
namespace sm = Syngine::Math;

class DefaultScene {
  private:
    Syngine::GameObject* sun = nullptr;

  public:
    inline void Load() {
        const sm::Vec3 initialSunDirection =
            sm::Vec3(45.0f, 45.0f, 0.0f).toRads();
        const sm::Vec3 initialSunColor(1.0f, 0.956f, 0.839f);
        sun = &GameObjectRegistry::CreateGameObject("sun");
        sun->AddComponent<Syngine::DirectionalLightComponent>(
            initialSunDirection, initialSunColor, 1.0f);

        auto mug = &GameObjectRegistry::CreateGameObject("mug");
        auto t   = mug->AddComponent<TransformComponent>();
        t->SetPosition(Vector3(0.f, 0.f, 5.f));
        mug->AddComponent<MeshComponent>("meshes/meshes.spk", "mug.glb", false);

        GameObject& cube = GameObjectRegistry::CreateGameObject("cube");
        cube.AddComponent<TransformComponent>();
        cube.AddComponent<MeshComponent>(
            "meshes/meshes.spk", "cube.glb", false);
        sm::Vector3 boxExtents(1.0f); // Half extents of the box
        Syngine::RigidbodyParameters params = { .shape    = PhysicsShapes::BOX,
                                                .mass     = 0.0f,
                                                .friction = 0.7f,
                                                .restitution     = 0.02f,
                                                .shapeParameters = boxExtents,
                                                .motionType =
                                                    JPH::EMotionType::Dynamic,
                                                .layer = Layers::MOVING };
        cube.AddComponent<RigidbodyComponent>(params);
    };

    inline void SunDirLeft() {
        auto* sunComp = sun->GetComponent<Syngine::DirectionalLightComponent>();
        if (!sunComp) {
            Syngine::Logger::Error("Sun component not found on sun GameObject");
            return;
        }
        Math::Vector3 currentDir = sunComp->GetDirectionVector();

        float rotationSpeed = -60.0f; // degrees per second
        float angleRad      = bx::toRad(rotationSpeed * Core::deltaTime);

        // Rotate around the world's X axis. This simulates the sun
        // rising/setting.
        Math::Mat4 mtx;
        mtx.rotateX(angleRad);
        Math::Vec3 dirVec = currentDir;
        dirVec            = (dirVec * mtx).normalized().xyz();
        sunComp->SetDirectionVector(dirVec);
    }

    inline void SunDirRight() {
        auto* sunComp = sun->GetComponent<Syngine::DirectionalLightComponent>();
        if (!sunComp) {
            Syngine::Logger::Error("Sun component not found on sun GameObject");
            return;
        }
        Math::Vector3 currentDir = sunComp->GetDirectionVector();

        float rotationSpeed = 60.0f; // degrees per second
        float angleRad      = bx::toRad(rotationSpeed * Core::deltaTime);

        // Rotate around the world's X axis. This simulates the sun
        // rising/setting.
        Math::Mat4 mtx;
        mtx.rotateX(angleRad);
        Math::Vec3 dirVec = currentDir;
        dirVec            = (dirVec * mtx).normalized().xyz();
        sunComp->SetDirectionVector(dirVec);
    }
};
