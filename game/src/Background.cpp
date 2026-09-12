// ╒═════════════════════ Background.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include <Syngine/Syngine.h>

#include "Background.h"
#include "Process.hpp"
#include "AssetWindow.inl"

#include <imgui/imgui.h>

#ifdef _WIN32
#include <windows.h>
#endif

namespace SynEditor {

Process          BackgroundActivities::g_cmakeProcess;
bool             BackgroundActivities::g_showCmakePopup = false;
BuildEnvironment BackgroundActivities::g_buildEnvironment;

void BackgroundActivities::SetupBuildEnvironment() {
#ifdef _WIN32
    // First find the Visual Studio installation and vcvars path
    // Fortunately, vswhere exists
    char programFilesX86[MAX_PATH] = {};
    GetEnvironmentVariableA("ProgramFiles(x86)", programFilesX86, MAX_PATH);
    std::string installerDir =
        std::string(programFilesX86) + "\\Microsoft Visual Studio\\Installer";

    Process vswhereProcess;
    vswhereProcess.Start(
        installerDir + "\\vswhere.exe -latest -products * -requires "
                       "Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
                       "-property installationPath",
        installerDir);
    vswhereProcess.Wait();
    if (vswhereProcess.GetExitCode() != 0) {
        Syngine::Logger::ToConsole(
            "Failed to find Visual Studio installation via vswhere.");
    }
    std::string vswhereOutput = vswhereProcess.Output();
    if (vswhereOutput.empty()) {
        Syngine::Logger::ToConsole(
            "Visual Studio installation path not found in vswhere output.");
    }

    // strip any trailing newline characters from the vswhere output
    while (!vswhereOutput.empty() &&
           (vswhereOutput.back() == '\n' || vswhereOutput.back() == '\r')) {
        vswhereOutput.pop_back();
    }
    std::string vcvarsPath =
        vswhereOutput + "\\VC\\Auxiliary\\Build\\vcvars64.bat";

    BuildEnvironment buildEnvironment{ .visualStudio = scl::path(vswhereOutput),
                                       .vcvars       = scl::path(vcvarsPath),
                                       .arch         = "x64" };
    g_buildEnvironment = buildEnvironment;
#else
    // For non-Windows platforms, set up a default build environment
    BuildEnvironment buildEnvironment{ .visualStudio = scl::path(""),
                                       .vcvars       = scl::path(""),
                                       .arch         = "x64" };
    g_buildEnvironment = buildEnvironment;
#endif
}

void BackgroundActivities::Update() {}

void BackgroundActivities::RenderCmake() {
    if (g_showCmakePopup) {
        if (ImGui::BeginPopupModal(
                "CMake Output", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::BeginChild("Output", ImVec2(500, 300));
            ImGui::TextUnformatted(g_cmakeProcess.Output().c_str());
            ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (ImGui::Button("Cancel")) {
                g_cmakeProcess.Kill();
                g_showCmakePopup = false;
                ImGui::CloseCurrentPopup();
                AssetWindow::RebuildTree();
            }
            ImGui::SameLine();
            if (ImGui::Button("Close")) {
                g_showCmakePopup = false;
                ImGui::CloseCurrentPopup();
                AssetWindow::RebuildTree();
            }
            ImGui::EndPopup();
        }
    }
}

void BackgroundActivities::RunCmake(const std::string& sourceDir) {
    g_showCmakePopup = true;
    std::string command;
#if BX_PLATFORM_WINDOWS
    command = std::string("cmd.exe /c call \"") +
              g_buildEnvironment.vcvars.cstr() +
              "\" && cmake -S . -B build -G Ninja && cmake --build "
              "build --target BuildAssets";
#elif BX_PLATFORM_OSX
    command = "cmake -S . -B build -G Ninja && cmake --build build --target "
              "BuildAssets";
#elif BX_PLATFORM_LINUX
    command = "cmake -S . -B build -G Ninja && cmake --build build --target "
              "BuildAssets";
#endif
    g_cmakeProcess.Start(command, sourceDir);
}

} // namespace SynEditor
