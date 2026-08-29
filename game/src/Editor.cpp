// ╒═════════════════════════ Editor.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-21                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "Editor.h"
#include <Syngine/Math/Math.hpp>

Syngine::CameraComponent* Editor::editorCamera = nullptr;
Editor::EditorState       Editor::editorState;

void Editor::_MoveCameraInDirection(const Syngine::Math::Vector3& direction,
                                    float                         speed) {
    Syngine::Math::Vector3 pos = editorCamera->GetPosition();
    Syngine::Math::Vector3 moveVec =
        Syngine::Math::Vector3(direction) * (speed * Syngine::Core::deltaTime);
    pos = pos + moveVec;

    editorCamera->SetPosition(pos);
}

void Editor::UpdateCamera(MovementBindings& movementBindings) {
    float realSpeed = editorState.cameraSpeed;
    float deltaTime = Syngine::Core::deltaTime;

    if (movementBindings.fast.isPressed()) realSpeed *= 2.0f;
    if (movementBindings.slow.isPressed()) realSpeed *= 0.5f;

    float                  yaw, pitch;
    Syngine::Math::Vector2 angles = editorCamera->GetAngles();
    yaw                           = angles.x();
    pitch                         = angles.y();

    if (movementBindings.forwards.isPressed()) {
        Syngine::Math::Vector3 forward(
            cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw));
        _MoveCameraInDirection(forward, realSpeed);
    }
    if (movementBindings.backwards.isPressed()) {
        Syngine::Math::Vector3 backward(
            -cosf(pitch) * sinf(yaw), -sinf(pitch), -cosf(pitch) * cosf(yaw));
        _MoveCameraInDirection(backward, realSpeed);
    }
    if (movementBindings.left.isPressed()) {
        Syngine::Math::Vector3 left(
            sinf(yaw - bx::kPiHalf), 0.0f, cosf(yaw - bx::kPiHalf));
        _MoveCameraInDirection(left, realSpeed);
    }
    if (movementBindings.right.isPressed()) {
        Syngine::Math::Vector3 right(
            -sinf(yaw - bx::kPiHalf), 0.0f, -cosf(yaw - bx::kPiHalf));
        _MoveCameraInDirection(right, realSpeed);
    }
    if (movementBindings.down.isPressed()) {
        Syngine::Math::Vector3 down(0.0f, -1.0f, 0.0f);
        _MoveCameraInDirection(down, realSpeed);
    }
    if (movementBindings.up.isPressed()) {
        Syngine::Math::Vector3 up(0.0f, 1.0f, 0.0f);
        _MoveCameraInDirection(up, realSpeed);
    }
}

void Editor::SetSimulate(bool simulate) { editorState.simulate = simulate; }

void Editor::HandleMouseMovement(float x, float y) {
    if (!editorState.simulate && editorState.rmbHeld) {
        // Update camera angles based on mouse movement
        float deltaX = x * editorState.DEFAULT_SENSITIVITY;
        float deltaY = y * editorState.DEFAULT_SENSITIVITY;

        // Adjust camera angles
        Syngine::Math::Vector2 angles = editorCamera->GetAngles();
        float                  yaw    = angles.x();
        float                  pitch  = angles.y();
        yaw += deltaX;
        pitch -= deltaY;

        // Clamp pitch to avoid flipping
        if (pitch > editorState.DEFAULT_MAX_PITCH) {
            pitch = editorState.DEFAULT_MAX_PITCH;
        } else if (pitch < -editorState.DEFAULT_MAX_PITCH) {
            pitch = -editorState.DEFAULT_MAX_PITCH;
        }

        editorCamera->SetAngles(yaw, pitch);
        Syngine::Window::SetMousePosition(editorState.mousePos);
    }
}

void Editor::HandleMouseScroll(float x, float y) {
    if (!editorState.simulate) {
        if (y > 0) {
            editorState.cameraSpeed +=
                editorState.DEFAULT_EDITOR_SPEED_INCREMENT;
            if (editorState.cameraSpeed >
                editorState.DEFAULT_MAX_EDITOR_SPEED) {
                editorState.cameraSpeed = editorState.DEFAULT_MAX_EDITOR_SPEED;
            }
        } else if (y < 0) {
            editorState.cameraSpeed -=
                editorState.DEFAULT_EDITOR_SPEED_INCREMENT;
            if (editorState.cameraSpeed < 0.0f) {
                editorState.cameraSpeed = 0.0f;
            }
        }
    }
}

void Editor::HandleRMouseButtonDown() {
    if (!editorState.simulate) {
        editorState.rmbHeld  = true;
        editorState.mousePos = Syngine::Window::GetMousePosition();
        Syngine::Window::SetMouseCursorVisible(false);
        Syngine::Core::mouseCaptureOverride = true;
    }
};

void Editor::HandleRMouseButtonUp() {
    if (!editorState.simulate) {
        editorState.rmbHeld = false;
        if (Syngine::Core::viewportHovered) {
            Syngine::Window::SetMousePosition(editorState.mousePos);
        }

        Syngine::Window::SetMouseCursorVisible(true);
        Syngine::Core::mouseCaptureOverride = false;
    }
};

void Editor::MakeCamera() {
    auto* cam = new Syngine::CameraComponent(nullptr);
    cam->SetFarPlane(2000);
    editorCamera = cam;
}
