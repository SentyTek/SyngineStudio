// ╒════════════════════ IconManager.cpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-15                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "IconManager.hpp"

#include "../lib/miniscl.hpp"
#include "bgfx/bgfx.h"
#include "imgui/backends/imgui_impl_bgfx.hpp"

#define SYNSTUDIO_ICON_PATH "imgs/iconset.spk"

namespace SynEditor {

std::unordered_map<std::string, bgfx::TextureHandle> IconManager::m_icons;

void IconManager::LoadIcons() {
    // oh my god there's so many icons
    const std::vector<std::string> icons = { "component/transform",
                                             "component/mesh",
                                             "component/camera",
                                             "component/directional_light",
                                             "component/point_light",
                                             "component/spot_light",
                                             "component/audio_source",
                                             "component/audio_input",
                                             "component/audio_listener",
                                             "component/particle_system",
                                             "component/terrain",
                                             "component/billboard",
                                             "component/rigidbody",
                                             "component/zone",
                                             "component/audio_effect_area",
                                             "component/player_controller",

                                             "window/assets",
                                             "window/scene",
                                             "window/inspector",
                                             "window/console",
                                             "window/hierarchy",

                                             "assets/folder",
                                             "assets/shader",
                                             "assets/gameobject",

                                             "buttons/local_lock",
                                             "buttons/local_unlock",
                                             "buttons/grid_lock",
                                             "buttons/grid_unlock",
                                             "buttons/debug/toggle",
                                             "buttons/debug/wireframes",
                                             "buttons/debug/gizmos",
                                             "buttons/debug/shadows",
                                             "buttons/debug/bounding_boxes",
                                             "buttons/reload_meshes",
                                             "buttons/reload_shaders",
                                             "buttons/reload_lua",
                                             "buttons/tools/select",
                                             "buttons/tools/translate",
                                             "buttons/tools/rotate",
                                             "buttons/tools/scale" };

    for (const auto& icon : icons) {
        bgfx::TextureHandle handle =
            Syngine::UI::Debug::ImGui_ImplBgfx::LoadTex(SYNSTUDIO_ICON_PATH,
                                                        icon + ".png");
        if (bgfx::isValid(handle)) {
            m_icons[icon] = handle;
        } else {
            Syngine::Logger::Warn("Failed to load icon: " + icon, true);
        }
    }
}

void IconManager::UnloadIcons() {
    for (auto& [key, handle] : m_icons) {
        if (bgfx::isValid(handle)) {
            bgfx::destroy(handle);
        }
    }
    m_icons.clear();
}

bgfx::TextureHandle IconManager::GetIcon(const std::string& iconName) {
    auto it = m_icons.find(iconName);
    if (it != m_icons.end()) {
        return it->second;
    }
    return BGFX_INVALID_HANDLE;
}

ImTextureID IconManager::GetIconImGui(const std::string& iconName) {
    return Syngine::UI::Debug::ImGui_ImplBgfx::ToImGui(GetIcon(iconName));
}

} // namespace SynEditor
