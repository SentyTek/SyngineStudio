// ╒═══════════════════════════ main.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-08-21                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include <Syngine/Syngine.h>

#include "DefaultScene.h"
#include "Process.hpp"
#include "SceneControls.h"
#include "UI.hpp"
#include "Background.h"

#include <string>

using namespace Syngine;

int AppMain(int argc, char* argv[]) {
    bool skipCmake = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--skip-cmake") {
            skipCmake = true;
            break;
        }
    }

    std::string           gameName = "Syngine Studio";
    Syngine::EngineConfig config   = { .gameName       = gameName,
                                       .windowWidth    = 1600,
                                       .windowHeight   = 900,
                                       .showWindowAuto = false,
                                       .usePhysics     = true };

    Syngine::RendererConfig rConfig = { .useShadows      = true,
                                        .shadowDist      = 500,
                                        .vsync           = true,
                                        .usePseudoCamera = false };

    Syngine::Logger::Info("Starting " + gameName, true);

    // Create game
    Syngine::Core engine(config);
    engine.Initialize(rConfig);

    SynEditor::BackgroundActivities::ShowStartupScreen();

    SynEditor::BackgroundActivities::g_loadingText = "Loading default scene";
    SynEditor::BackgroundActivities::RenderStartupScreen();
    Scene::MakeCamera();
    DefaultScene defaultScene;
    defaultScene.Load();

    SynEditor::UI editorUi;

    SynEditor::BackgroundActivities::g_loadingText =
        "Setting up build environment";
    SynEditor::BackgroundActivities::RenderStartupScreen();
    SynEditor::BackgroundActivities::SetupBuildEnvironment();
    scl::path ProjectDirectory;
#if BX_PLATFORM_OSX
    ProjectDirectory = scl::path::execdir()
                           .parentpath()
                           .parentpath()
                           .parentpath()
                           .parentpath()
                           .parentpath()
                           .parentpath();
#else
    ProjectDirectory =
        scl::path::execdir().parentpath().parentpath().parentpath();
#endif
    Syngine::Logger::LogF(Syngine::LogLevel::INFO,
                          true,
                          "project dir: %s",
                          scl::path::execdir().cstr());

    if (!skipCmake) {
        SynEditor::BackgroundActivities::g_loadingText = "Running CMake";
        SynEditor::BackgroundActivities::RenderStartupScreen();
        SynEditor::BackgroundActivities::RunCmake(ProjectDirectory.cstr());
        SynEditor::BackgroundActivities::g_cmakeProcess.Wait();
    }

    SynEditor::BackgroundActivities::g_loadingText = "Importing assets";
    SynEditor::BackgroundActivities::RenderStartupScreen();
    editorUi.Setup("SyngineStudio", ProjectDirectory);
    // space illegal in project name

    SynEditor::BackgroundActivities::g_loadingText = "Configuring editor UI";
    SynEditor::BackgroundActivities::RenderStartupScreen();
    scl::path imguiini("imgui.ini");
    editorUi.ConfigFileExists = imguiini.exists();

    Scene::MovementBindings movementBindings = {
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
                                { .onPressed  = Scene::HandleRMouseButtonDown,
                                  .onReleased = Scene::HandleRMouseButtonUp });

    InputAction::RegisterMouseMoveEvent(Scene::HandleMouseMovement);
    InputAction::RegisterScrollEvent(Scene::HandleMouseScroll);

    Renderer::SetActiveCamera(Scene::editorCamera);

    engine.AddFrameCallback(
        [&editorUi](int frameNum) { editorUi.Draw(frameNum); });

    SynEditor::BackgroundActivities::HideStartupScreen();
    Syngine::Window::SetWindowVisible(true);
    Logger::Info("Starting event loop", true);
    while (engine.IsRunning()) {
        Profiler::Reset();
        {
            SYN_PROFILE_SCOPE("MainLoop")
            engine.HandleEvents();
            engine.Update();
            Scene::UpdateCamera(movementBindings);
            engine.Render();
        }
    }

    // Cleanup
    Syngine::GameObjectRegistry::Clear();
    ShaderManager::UnloadAllShaders();
    return 0;
}
