// ╒═══════════════════════ Background.h ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-07                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include <string>

#include "Process.hpp"
#include <miniscl.hpp>

namespace SynEditor {

struct BuildEnvironment {
    scl::path   visualStudio;
    scl::path   vcvars;
    std::string arch;
};

class BackgroundActivities {
  public:
    static Process g_cmakeProcess;
    static bool    g_showCmakePopup;

    static BuildEnvironment g_buildEnvironment;

    static void SetupBuildEnvironment();

    static void Update();
    static void RenderCmake();

    static void RunCmake(const std::string& sourceDir);
};

} // namespace SynEditor
