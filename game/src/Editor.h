// ╒═══════════════════════════ Editor.h ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-21                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once
#include <Syngine/Syngine.h>

class Editor {
  public:
    static Syngine::CameraComponent* editorCamera;

    static void HandleMouseMovement(float x, float y);
    static void HandleMouseScroll(float x, float y);
    static void HandleRMouseButtonUp();
    static void HandleRMouseButtonDown();

    static void SetSimulate(bool simulate);

    struct MovementBindings {
        Syngine::InputAction forwards;
        Syngine::InputAction backwards;
        Syngine::InputAction left;
        Syngine::InputAction right;
        Syngine::InputAction up;
        Syngine::InputAction down;
        Syngine::InputAction slow;
        Syngine::InputAction fast;
    };

    static void UpdateCamera(MovementBindings& movementBindings);
    static void MakeCamera();

  private:
    static struct EditorState {
        static constexpr float DEFAULT_SENSITIVITY = 0.002f;
        static constexpr float DEFAULT_MAX_PITCH   = 3.14f / 2 - 0.01f;
        static constexpr float DEFAULT_EDITOR_SPEED_INCREMENT = 0.5f;
        static constexpr float DEFAULT_MAX_EDITOR_SPEED       = 100.0f;

        float                  cameraSpeed = 3.0f; // Default camera speed
        Syngine::Math::Vector2 mousePos;           // Current mouse state (x, y)

        bool rmbHeld    = false;
        bool mouseState = false;
        bool simulate   = false;
    } editorState;

    static void _MoveCameraInDirection(const Syngine::Math::Vector3& direction,
                                       float                         speed);
};
