// ╒════════════════════ AssetWindow.inl ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-03                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "bgfx/bgfx.h"
#include "imgui/backends/imgui_impl_bgfx.hpp"
#include <Syngine/Syngine.h>

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <memory>
#include <miniscl.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace SynEditor {

enum class AssetSource { GAME, ENGINE };

// Since assets are on both disk and the engine VFS, we just keep track of both
// paths here
struct AssetPath {
    std::string diskPath;
    scl::string bundlePath;
};

enum class AssetType { TEXTURE, MODEL, SOUND, SCRIPT, SHADER, UNKNOWN };

struct AssetInfo {
    uint64_t    id;
    AssetType   type;
    AssetSource source;
    AssetPath   path;
    uint32_t    sizeDisk = 0;
    uint32_t    sizeVFS  = 0;

    std::string         displayName;
    bgfx::TextureHandle thumbnail   = BGFX_INVALID_HANDLE;
    bool                isDirectory = false;
};

struct AssetDirectory {
    std::string name;

    std::vector<std::unique_ptr<AssetDirectory>> children;
    std::vector<AssetInfo>                       assets;
    AssetDirectory*                              parent = nullptr;
};

class AssetWindow {
    static int                                 ASSET_TILE_SIZE;
    static int                                 TILE_FIT_WIDTH;
    static AssetDirectory*                     m_currentDirectory;
    inline static std::vector<AssetDirectory*> m_directoryHistory;
    inline static size_t                       m_historyIndex = 0;

  public:
    static std::unique_ptr<AssetDirectory> m_rootDirectory;
    static int                             numShownAssets;
    static uint32_t                        shownAssetsTotalSizeDisk;
    static uint32_t                        shownAssetsTotalSizeVFS;

    inline static bool ContainsInsensitive(const std::string& text,
                                           const char*        searchText) {
        if (!searchText || !searchText[0]) {
            return true;
        }
        const std::string query(searchText);
        if (query.size() > text.size()) {
            return false;
        }
        return std::search(
                   text.begin(),
                   text.end(),
                   query.begin(),
                   query.end(),
                   [](char left, char right) {
                       return std::tolower(static_cast<unsigned char>(left)) ==
                              std::tolower(static_cast<unsigned char>(right));
                   }) != text.end();
    }

    inline static void BuildFileTree(scl::path   projectPath,
                                     std::string projectName) {
        // Building the tree is in four steps:
        // 1. Search the engine's directory for assets.
        // 2. Search the project's directory for assets.
        // 3. Search the virtual file system for assets. Ideally, there will be
        // 100% matches between the build VFS and the earlier discovered assets.
        // 4. Build the hierarchical tree structure based on the found assets.

        // 1. Search the engine's directory for assets.
        scl::path engineAssetsPath(projectPath / "engine/default");
        if (!engineAssetsPath.exists()) {
            Syngine::Logger::Fatal("Engine assets directory does not exist. "
                                   "Double check your Syngine installation.");
        }

        auto engineAssets = scl::path::glob(engineAssetsPath / "**/*");

        // 2. Search the project's directory for assets.
        scl::path projectAssetsPath(projectPath / "assets");
        if (!projectAssetsPath.exists()) {
            Syngine::Logger::Fatal("Project assets directory does not exist. "
                                   "Double check your project setup.");
        }

        auto projectAssets = scl::path::glob(projectAssetsPath / "**/*");

        // 3. Check the build VFS
#ifdef BX_PLATFORM_BSD
        // Macos special app bundles think their so special
        scl::path   appName(projectName + ".app");
        scl::string pattern = projectPath / "build/Debug/bin/" / appName /
                              "Contents/Resources/rom/**/*.spk";
        auto buildBundles = scl::path::glob(pattern);
#else
        auto buildBundles =
            scl::path::glob(projectPath / "build/bin/Debug/rom");
#endif

        // name, bundle, compressed size, original size
        std::vector<std::tuple<scl::string, scl::path, uint32_t, uint32_t>>
                            allFilesInBundles;
        scl::pack::Packager packager;
        for (const auto& bundle : buildBundles) {
            if (!packager.open(bundle)) {
                continue;
            }
            const auto& files = packager.index();
            for (const auto& filename : files) {
                allFilesInBundles.push_back({ filename.second.filepath(),
                                              bundle,
                                              filename.second.compressed(),
                                              filename.second.original() });
            }
            packager.close();
        }

        // 3.5. Ensure every non-generated VFS file exists on disk. If it does,
        // create an AssetInfo entry for it, being sure to strip the full path
        // and just include the project path (e.g "assets/textures/texture.png")
        bool                   allMatch = true;
        std::vector<AssetInfo> allAssets;
        for (const auto& [vfsFile, bundle, compressedSize, originalSize] :
             allFilesInBundles) {
            const std::string vfsFileStr     = vfsFile.cstr();
            const size_t      fileNameOffset = vfsFileStr.find_last_of("/\\");
            const std::string fileName       = vfsFileStr.substr(
                fileNameOffset == std::string::npos ? 0 : fileNameOffset + 1);
            // Only exists in vfs and not an actual asset, or is a meta/system
            // file
            if (fileName == "meta.xml" || fileName == ".DS_Store") {
                continue;
            }

            const bool isPackagedShader =
                (vfsFileStr.size() >= 9 &&
                 vfsFileStr.compare(vfsFileStr.size() - 9, 9, ".vert.bin") ==
                     0) ||
                (vfsFileStr.size() >= 9 &&
                 vfsFileStr.compare(vfsFileStr.size() - 9, 9, ".frag.bin") ==
                     0);
            const scl::string diskSearchName =
                isPackagedShader ? vfsFileStr.substr(0, vfsFileStr.size() - 4)
                                 : vfsFileStr;
            const auto findOnDisk = [&diskSearchName,
                                     isPackagedShader](const auto& diskFiles) {
                return std::find_if(
                    diskFiles.begin(),
                    diskFiles.end(),
                    [&diskSearchName, isPackagedShader](const auto& diskFile) {
                        if (!diskFile.isfile()) {
                            return false;
                        }

                        // If it is a packaged shader, we need to match the base
                        // name without the ".bin" suffix.
                        const std::string diskPath = diskFile.cstr();
                        const size_t      diskFileNameOffset =
                            diskPath.find_last_of("/\\");
                        const std::string diskFileName = diskPath.substr(
                            diskFileNameOffset == std::string::npos
                                ? 0
                                : diskFileNameOffset + 1);
                        // Skip macOS metadata files like ".DS_Store".
                        if (diskFileName == ".DS_Store") {
                            return false;
                        }
                        const std::string searchName = diskSearchName.cstr();
                        const std::string suffix =
                            isPackagedShader ? searchName + "." : searchName;
                        const size_t suffixOffset = diskPath.rfind(suffix);
                        return suffixOffset != std::string::npos &&
                               (suffixOffset == 0 ||
                                diskPath[suffixOffset - 1] == '/' ||
                                diskPath[suffixOffset - 1] == '\\') &&
                               (isPackagedShader
                                    ? suffixOffset + suffix.size() <
                                          diskPath.size()
                                    : suffixOffset + suffix.size() ==
                                          diskPath.size());
                    });
            };

            const auto engineFile  = findOnDisk(engineAssets);
            const auto projectFile = findOnDisk(projectAssets);
            if (engineFile == engineAssets.end() &&
                projectFile == projectAssets.end()) {
                allMatch = false;
                Syngine::Logger::ToConsole("File %s not found on disk.",
                                           vfsFile.cstr());
                continue;
            }

            {
                AssetType   type = vfsFile.endswith(".glb") ? AssetType::MODEL
                                   : vfsFile.endswith(".bin") ? AssetType::SHADER
                                   : vfsFile.endswith(".png")
                                       ? AssetType::TEXTURE
                                       : AssetType::UNKNOWN;
                std::string stem =
                    vfsFileStr.substr(vfsFileStr.find_last_of("/\\") + 1);
                scl::path diskFile = projectFile != projectAssets.end()
                                         ? *projectFile
                                         : *engineFile;
                // In diskFile elliminate anything before the projectName
                diskFile = diskFile.substr(diskFile.ffi(projectName));

                allAssets.push_back(
                    { .id          = 0,
                      .type        = type,
                      .source      = projectFile != projectAssets.end()
                                         ? AssetSource::GAME
                                         : AssetSource::ENGINE,
                      .path        = { .diskPath   = diskFile.cstr(),
                                       .bundlePath = bundle },
                      .sizeDisk    = compressedSize,
                      .sizeVFS     = originalSize,
                      .displayName = stem });
            }
        }

        // 4. Build the tree from the vfs
        for (const auto& asset : allAssets) {
            // Build the tree from the asset path
            scl::path                    path(asset.path.diskPath);
            const std::vector<scl::path> parts = path.split();

            AssetDirectory* current = m_rootDirectory.get();

            // Walk every directory component except the filename
            for (size_t index = 0; index + 1 < parts.size(); ++index) {
                const auto&       part      = parts[index];
                const std::string directory = part.cstr();

                AssetDirectory* child = nullptr;

                for (auto& existing : current->children) {
                    if (existing->name == directory) {
                        child = existing.get();
                        break;
                    }
                }

                if (!child) {
                    auto newDirectory = std::make_unique<AssetDirectory>(
                        AssetDirectory{ .name     = directory,
                                        .children = {},
                                        .assets   = {},
                                        .parent   = current });
                    child = newDirectory.get();
                    current->children.push_back(std::move(newDirectory));
                }

                current = child;
            }

            current->assets.push_back(std::move(asset));
        }

        m_currentDirectory =
            m_rootDirectory->children.front()->children.front().get();
        m_directoryHistory = { m_currentDirectory };
        m_historyIndex     = 0;
    }

    // Draws a little asset tile, designed to be tileable
    inline static void DrawAsset(const AssetInfo&      asset,
                                 const AssetDirectory* targetDir,
                                 bool                  sameLine) {
        const ImVec2      size(ASSET_TILE_SIZE, ASSET_TILE_SIZE);
        const ImGuiStyle& style = ImGui::GetStyle();

        if (sameLine) {
            ImGui::SameLine();
        }
        ImGui::BeginChild(asset.displayName.c_str(),
                          size,
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);

        if (ImGui::IsWindowHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            SetCurrentDirectory(targetDir);
        }

        const float labelHeight = ImGui::GetTextLineHeight();
        const float labelWidth  = size.x - style.WindowPadding.x * 2.0f;

        if (bgfx::isValid(asset.thumbnail)) {
            ImGui::Image(
                Syngine::UI::Debug::SImGui::ToImGui(asset.thumbnail),
                ImVec2(ASSET_TILE_SIZE * 0.6f, ASSET_TILE_SIZE * 0.6f));
        } else {
            ImGui::Text("No Thumbnail");
        }

        ImGui::SetCursorPos(
            ImVec2(style.WindowPadding.x,
                   size.y - style.WindowPadding.y - labelHeight));
        const ImVec2 labelPosition = ImGui::GetCursorScreenPos();
        ImGui::RenderTextEllipsis(
            ImGui::GetWindowDrawList(),
            labelPosition,
            ImVec2(labelPosition.x + labelWidth, labelPosition.y + labelHeight),
            labelPosition.x + labelWidth,
            asset.displayName.c_str(),
            nullptr,
            nullptr);
        ImGui::Dummy(
            ImVec2(labelWidth,
                   labelHeight)); // Needed for SetCursorPos to work correctly
        ImGui::EndChild();
    }

    inline static void DrawAssetTree(const char* searchText = nullptr) {
        ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        TILE_FIT_WIDTH = static_cast<int>(contentRegion.x) / ASSET_TILE_SIZE;
        numShownAssets = 0;
        shownAssetsTotalSizeDisk = 0;
        shownAssetsTotalSizeVFS  = 0;

        const AssetDirectory* displayRoot = searchText && searchText[0]
                                                ? m_rootDirectory.get()
                                                : m_currentDirectory;
        if (displayRoot) {
            DrawAssetDirectory(displayRoot, searchText);
        }
    }

    inline static void DrawAssetDirectory(const AssetDirectory* directory,
                                          const char*           searchText) {
        const int columns    = std::max(1, TILE_FIT_WIDTH);
        int       assetIndex = 0;

        if (searchText && searchText[0]) {
            DrawAssetSearchResults(directory, searchText, assetIndex);
            return;
        }

        for (const auto& child : directory->children) {
            AssetInfo folder{ .path        = { .diskPath = child->name },
                              .displayName = child->name,
                              .isDirectory = true };
            DrawAsset(folder, child.get(), assetIndex++ % columns != 0);
        }

        for (const auto& asset : directory->assets) {
            if (!ContainsInsensitive(asset.displayName, searchText)) {
                continue;
            }
            DrawAsset(asset, nullptr, assetIndex++ % columns != 0);
            numShownAssets++;
            shownAssetsTotalSizeDisk += asset.sizeDisk;
            shownAssetsTotalSizeVFS += asset.sizeVFS;
        }
    }

    inline static void DrawAssetSearchResults(const AssetDirectory* directory,
                                              const char*           searchText,
                                              int& assetIndex) {
        const int columns = std::max(1, TILE_FIT_WIDTH);
        for (const auto& child : directory->children) {
            if (ContainsInsensitive(child->name, searchText)) {
                AssetInfo folder{ .path        = { .diskPath = child->name },
                                  .displayName = child->name,
                                  .isDirectory = true };
                DrawAsset(folder, child.get(), assetIndex++ % columns != 0);
            }
            DrawAssetSearchResults(child.get(), searchText, assetIndex);
        }

        for (const auto& asset : directory->assets) {
            if (!ContainsInsensitive(asset.displayName, searchText)) {
                continue;
            }
            DrawAsset(asset, nullptr, assetIndex++ % columns != 0);
            numShownAssets++;
            shownAssetsTotalSizeDisk += asset.sizeDisk;
            shownAssetsTotalSizeVFS += asset.sizeVFS;
        }
    }

    inline static void SetCurrentDirectory(const AssetDirectory* directory) {
        if (!directory || directory == m_currentDirectory) {
            return;
        }
        if (m_historyIndex + 1 < m_directoryHistory.size()) {
            m_directoryHistory.resize(m_historyIndex + 1);
        }
        m_directoryHistory.push_back(const_cast<AssetDirectory*>(directory));
        m_historyIndex     = m_directoryHistory.size() - 1;
        m_currentDirectory = const_cast<AssetDirectory*>(directory);
    }

    inline static bool CanNavigateBack() { return m_historyIndex > 0; }

    inline static bool CanNavigateForward() {
        return m_historyIndex + 1 < m_directoryHistory.size();
    }

    inline static void NavigateBack() {
        if (CanNavigateBack()) {
            m_currentDirectory = m_directoryHistory[--m_historyIndex];
        }
    }

    inline static void NavigateForward() {
        if (CanNavigateForward()) {
            m_currentDirectory = m_directoryHistory[++m_historyIndex];
        }
    }

    inline static const AssetDirectory* GetCurrentDirectory() {
        return m_currentDirectory;
    }
};

} // namespace SynEditor
