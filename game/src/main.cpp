// ╒═══════════════════════════ main.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-21                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include <Syngine/Syngine.h>

#include "DefaultScene.h"
#include "Editor.h"
#include "Syngine/Core/Input.h"

#include <string>

using namespace Syngine;

int AppMain(int argc, char* argv[]) {
    std::string           gameName = "Syngine Studio";
    Syngine::EngineConfig config   = { .gameName     = gameName,
                                       .windowWidth  = 1600,
                                       .windowHeight = 900,
                                       .usePhysics   = true };

    Syngine::RendererConfig rConfig = { .useShadows      = true,
                                        .shadowDist      = 500,
                                        .vsync           = true,
                                        .usePseudoCamera = false };

    Syngine::Logger::Info("Starting " + gameName, true);

    // Create game
    Syngine::Core engine(config);
    engine.Initialize(rConfig);

    Editor::MakeCamera();
    DefaultScene defaultScene;
    defaultScene.Load();

    Editor::MovementBindings movementBindings = {
        .forwards = InputAction("editor.movement.forwards",
                                "Move Forward",
                                "editor",
                                KeyBinding(Syngine::Scancode::W)),

        .backwards = InputAction("editor.movement.backwards",
                                 "Move Back",
                                 "editor",
                                 KeyBinding(Syngine::Scancode::S)),

        .left = InputAction("editor.movement.leftwards",
                            "Move Left",
                            "editor",
                            KeyBinding(Syngine::Scancode::A)),

        .right = InputAction("editor.movement.rightwards",
                             "Move Right",
                             "editor",
                             KeyBinding(Syngine::Scancode::D)),

        .up = InputAction("editor.movement.upwards",
                          "Move Up",
                          "editor",
                          KeyBinding(Syngine::Scancode::E)),

        .down = InputAction("editor.movement.downwards",
                            "Move Down",
                            "editor",
                            KeyBinding(Syngine::Scancode::Q)),

        .slow = InputAction("editor.movement.slow",
                            "Slow Movement",
                            "editor",
                            KeyBinding(Syngine::Scancode::LEFT_CONTROL)),

        // i want my control sprinting back
        .fast = InputAction("editor.movement.fast",
                            "Fast Movement",
                            "editor",
                            KeyBinding(Syngine::Scancode::LEFT_SHIFT))
    };

    InputAction::RegisterAction(
        "editor.sun.right",
        "Move Sun Right",
        "editor",
        KeyBinding(Syngine::Scancode::RIGHT),
        { .onPressed = [&defaultScene]() { defaultScene.SunDirRight(); } });
    InputAction::RegisterAction(
        "editor.sun.left",
        "Move Sun Left",
        "editor",
        KeyBinding(Syngine::Scancode::LEFT),
        { .onPressed = [&defaultScene]() { defaultScene.SunDirLeft(); } });
    InputAction::RegisterAction("editor.rightMouseButtonHandling",
                                "Editor Mouse",
                                "editor",
                                KeyBinding(MouseButton::RIGHT),
                                { .onPressed  = Editor::HandleRMouseButtonDown,
                                  .onReleased = Editor::HandleRMouseButtonUp });

    InputAction::RegisterMouseMoveEvent(Editor::HandleMouseMovement);
    InputAction::RegisterScrollEvent(Editor::HandleMouseScroll);

    Renderer::SetActiveCamera(Editor::editorCamera);

    Logger::Info("Starting event loop", true);
    while (engine.IsRunning()) {
        Profiler::Reset();
        {
            SYN_PROFILE_SCOPE("MainLoop")
            engine.HandleEvents();
            engine.Update();
            Editor::UpdateCamera(movementBindings);
            engine.Render();
        }
    }

    // Cleanup
    Syngine::GameObjectRegistry::Clear();
    ShaderManager::UnloadAllShaders();
    return 0;
}
