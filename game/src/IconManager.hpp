// ╒════════════════════ IconManager.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-15                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

// This file manages all the icons for Studio, providing easy access and
// organization for the editor's UI elements.
// This does NOT manage the thumbs generated for texture assets.

#pragma once

#include <Syngine/Syngine.h>
#include <imgui/imgui.h>
#include <unordered_map>

#include "bgfx/bgfx.h"

#include "../lib/miniscl.hpp"

namespace SynEditor {

class IconManager {
    static std::unordered_map<std::string, bgfx::TextureHandle> m_icons;

  public:
    static void LoadIcons();
    static void UnloadIcons();

    static bgfx::TextureHandle GetIcon(const std::string& iconName);
    static ImTextureID         GetIconImGui(const std::string& iconName);
};

} // namespace SynEditor
