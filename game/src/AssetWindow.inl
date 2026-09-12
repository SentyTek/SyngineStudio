// ╒════════════════════ AssetWindow.inl ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-03                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include <Syngine/Syngine.h>
#include "imgui/backends/imgui_impl_bgfx.hpp"

#include <imgui/imgui_internal.h>
#include <imgui/imgui.h>
#include <miniscl.hpp>

#include <memory>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace SynEditor {

enum class AssetSource { GAME, ENGINE };

// Since assets are on both disk and the engine VFS, we just keep track of both
// paths here
struct AssetPath {
    std::string diskPath;
    scl::string bundlePath;
    std::string absoluteDiskPath; // Real on-disk path, used for delete/reveal
};

enum class AssetType {
    TEXTURE,
    MODEL,
    SOUND,
    SCRIPT,
    SHADER,
    ANIMATION,
    PREFAB,
    SCENE,
    UNKNOWN
};

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
    std::string relativePath; // Path relative to the project root, used to
                              // resolve absolute disk paths

    std::vector<std::unique_ptr<AssetDirectory>> children;
    std::vector<AssetInfo>                       assets;
    AssetDirectory*                              parent = nullptr;
};

// Favorites are stored by relative path rather than pointer since the tree is
// rebuilt wholesale after any disk change, invalidating existing pointers.
struct FavoriteEntry {
    std::string relativePath;
    std::string displayName;
    bool        isDirectory = false;
};

class AssetWindow {
    static int                                 ASSET_TILE_SIZE;
    static int                                 TILE_FIT_WIDTH;
    static AssetDirectory*                     m_currentDirectory;
    inline static std::vector<AssetDirectory*> m_directoryHistory;
    inline static size_t                       m_historyIndex = 0;

    // Remembered so RebuildTree() can re-scan without needing new arguments
    inline static scl::path   m_projectRoot;
    inline static std::string m_projectName;

    // Deletion is deferred until after the tree traversal for the frame
    // finishes, since the tile that requested it may be destroyed by a rebuild
    inline static bool        m_hasPendingDeletion       = false;
    inline static bool        m_pendingDeleteIsDirectory = false;
    inline static std::string m_pendingDeletePath;
    inline static std::string m_pendingDeleteLabel;

    // Set by a tile's context menu, consumed once by DrawAssetTree to open
    // the confirmation modal outside of the context menu's own popup scope
    inline static bool        m_openDeleteConfirmPopup   = false;
    inline static bool        m_confirmDeleteIsDirectory = false;
    inline static std::string m_confirmDeletePath;
    inline static std::string m_confirmDeleteLabel;

    // diskPath of the currently selected (non-directory) asset, empty if none
    inline static std::string m_selectedAssetPath;

  public:
    static std::unique_ptr<AssetDirectory> g_rootDirectory;
    static int                             g_numShownAssets;
    static uint32_t                        g_shownAssetsTotalSizeDisk;
    static uint32_t                        g_shownAssetsTotalSizeVFS;
    static std::vector<FavoriteEntry>      g_favorites;

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
        m_projectRoot = projectPath;
        m_projectName = projectName;

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
#if BX_PLATFORM_OSX
        // Macos special app bundles think they're so special
        scl::path   appName(projectName + ".app");
        scl::string pattern      = projectPath / "build/Debug/bin/" / appName /
                                   "Contents/Resources/rom/**/*.spk";
        auto        buildBundles = scl::path::glob(pattern);
#else
        scl::string pattern      = projectPath / "build/Debug/bin/rom/**/*.spk";
        auto        buildBundles = scl::path::glob(pattern);
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
                AssetType type = vfsFile.endswith(".glb")   ? AssetType::MODEL
                                 : vfsFile.endswith(".bin") ? AssetType::SHADER
                                 : vfsFile.endswith(".png")
                                     ? AssetType::TEXTURE
                                     : AssetType::UNKNOWN;
                std::string stem =
                    vfsFileStr.substr(vfsFileStr.find_last_of("/\\") + 1);
                scl::path         diskFile = projectFile != projectAssets.end()
                                                 ? *projectFile
                                                 : *engineFile;
                const std::string absoluteDiskPath = diskFile.cstr();
                // In diskFile elliminate anything before the projectName
                diskFile = diskFile.substr(diskFile.ffi(projectName));

                allAssets.push_back(
                    { .id          = 0,
                      .type        = type,
                      .source      = projectFile != projectAssets.end()
                                         ? AssetSource::GAME
                                         : AssetSource::ENGINE,
                      .path        = { .diskPath         = diskFile.cstr(),
                                       .bundlePath       = bundle,
                                       .absoluteDiskPath = absoluteDiskPath },
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

            AssetDirectory* current = g_rootDirectory.get();

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
                    auto newDirectory =
                        std::make_unique<AssetDirectory>(AssetDirectory{
                            .name = directory,
                            .relativePath =
                                current->relativePath.empty()
                                    ? directory
                                    : current->relativePath + "/" + directory,
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

        // Find project dir and set it as the current directory
        AssetDirectory* projectDir =
            const_cast<AssetDirectory*>(SearchForDirectory("assets"));
        m_currentDirectory = projectDir ? projectDir : g_rootDirectory.get();

        m_directoryHistory = { m_currentDirectory };
        m_historyIndex     = 0;
    }

    // Re-scans disk/VFS from scratch, e.g. after an asset was deleted
    inline static void RebuildTree() {
        g_rootDirectory = std::make_unique<AssetDirectory>(
            AssetDirectory{ .name     = "Assets",
                            .children = {},
                            .assets   = {},
                            .parent   = nullptr });
        BuildFileTree(m_projectRoot, m_projectName);
    }

    inline static std::string
    GetDirectoryAbsolutePath(const AssetDirectory* directory) {
        if (!directory || directory->relativePath.empty()) {
            return m_projectRoot.parentpath().cstr();
        }
        return (m_projectRoot.parentpath() / directory->relativePath.c_str())
            .cstr();
    }

    inline static void RequestDelete(const std::string& absolutePath,
                                     bool               isDirectory,
                                     const std::string& label) {
        if (absolutePath.empty()) {
            return;
        }
        m_hasPendingDeletion       = true;
        m_pendingDeleteIsDirectory = isDirectory;
        m_pendingDeletePath        = absolutePath;
        m_pendingDeleteLabel       = label;
    }

    // Queues the confirmation modal to open; the actual deletion only happens
    // once the user confirms it.
    inline static void
    RequestDeleteConfirmation(const std::string& absolutePath,
                              bool               isDirectory,
                              const std::string& label) {
        if (absolutePath.empty()) {
            return;
        }
        m_confirmDeleteIsDirectory = isDirectory;
        m_confirmDeletePath        = absolutePath;
        m_confirmDeleteLabel       = label;
        m_openDeleteConfirmPopup   = true;
    }

    inline static void DrawDeleteConfirmationPopup() {
        if (m_openDeleteConfirmPopup) {
            ImGui::OpenPopup("Delete Asset?");
            m_openDeleteConfirmPopup = false;
        }

        if (ImGui::BeginPopupModal(
                "Delete Asset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Are you sure you want to delete \"%s\"?\nThis cannot "
                        "be undone.",
                        m_confirmDeleteLabel.c_str());
            ImGui::Separator();

            if (ImGui::Button("Delete", ImVec2(120, 0))) {
                RequestDelete(m_confirmDeletePath,
                              m_confirmDeleteIsDirectory,
                              m_confirmDeleteLabel);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    inline static void ProcessPendingDeletion() {
        std::error_code errorCode;
        if (m_pendingDeleteIsDirectory) {
            std::filesystem::remove_all(m_pendingDeletePath, errorCode);
        } else {
            std::filesystem::remove(m_pendingDeletePath, errorCode);
        }

        if (errorCode) {
            Syngine::Logger::LogF(Syngine::LogLevel::ERR,
                                  true,
                                  "Failed to delete %s: %s",
                                  m_pendingDeletePath.c_str(),
                                  errorCode.message().c_str());
        } else {
            Syngine::Logger::Info("Deleted " + m_pendingDeleteLabel, true);
            m_selectedAssetPath.clear();
            RebuildTree();
        }

        m_hasPendingDeletion = false;
        m_pendingDeletePath.clear();
        m_pendingDeleteLabel.clear();
    }

    inline static void ShowInFileBrowser(const std::string& absolutePath) {
        if (absolutePath.empty()) {
            return;
        }
        // Escape embedded quotes since the path is interpolated into a shell
        // command below.
        std::string escaped = absolutePath;
        size_t      pos     = 0;
        while ((pos = escaped.find('"', pos)) != std::string::npos) {
            escaped.insert(pos, "\\");
            pos += 2;
        }

        scl::path dir = scl::path(escaped.c_str());
        if (dir.isfile()) {
            dir = dir.parentpath();
        }

#if defined(_WIN32)
        const std::string file = "file:///" + std::string(dir.cstr());
        SDL_OpenURL(file.c_str());
#elif BX_PLATFORM_OSX
        const std::string command =
            "open -R \"" + std::string(dir.cstr()) + "\"";
        std::system(command.c_str());
#else
        const std::string command =
            "xdg-open \"" + std::string(dir.cstr()) + "\"";
        std::system(command.c_str());
#endif
    }

    // Placeholder: real asset creation isn't implemented yet.
    inline static void CreateNewAsset(const char*           assetKind,
                                      const AssetDirectory* targetDirectory,
                                      const std::string&    assetName) {
        Syngine::Logger::Info("TODO: create new " + std::string(assetKind) +
                                  " in " +
                                  GetDirectoryAbsolutePath(targetDirectory),
                              true);
        if (assetKind == std::string("Folder")) {
            scl::path dirPath(
                (GetDirectoryAbsolutePath(targetDirectory) + "/" + assetName));
        }
        RebuildTree();
    }

    // Shared by both a tile's context menu and the content browser's
    // background context menu.
    inline static void
    DrawNewAssetMenuItems(const AssetDirectory* targetDirectory) {
        if (ImGui::MenuItem("Folder")) {
            CreateNewAsset("Folder", targetDirectory, "New Folder");
        }
        if (ImGui::MenuItem("Shader")) {
            CreateNewAsset("Shader", targetDirectory, "New Shader");
        }
        if (ImGui::MenuItem("Scene")) {
            CreateNewAsset("Scene", targetDirectory, "New Scene");
        }
        if (ImGui::MenuItem("Prefab")) {
            CreateNewAsset("Prefab", targetDirectory, "New Prefab");
        }
    }

    // Right-click anywhere over empty space in the content browser
    inline static void DrawContentBackgroundContextMenu() {
        if (ImGui::BeginPopupContextWindow(
                "AssetsContentContextMenu",
                ImGuiPopupFlags_MouseButtonRight |
                    ImGuiPopupFlags_NoOpenOverExistingPopup)) {
            if (ImGui::MenuItem("Show in File Browser")) {
                ShowInFileBrowser(GetDirectoryAbsolutePath(m_currentDirectory));
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("New Asset")) {
                DrawNewAssetMenuItems(m_currentDirectory);
                ImGui::EndMenu();
            }
            ImGui::EndPopup();
        }
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

        // Hover/press state must be known before BeginChild so the tile's
        // background color can react to it.
        const ImVec2 tileMin = ImGui::GetCursorScreenPos();
        const ImVec2 tileMax(tileMin.x + size.x, tileMin.y + size.y);
        const bool   tileHovered = ImGui::IsMouseHoveringRect(tileMin, tileMax);
        const bool   tilePressed =
            tileHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool isSelected = !asset.isDirectory &&
                                !m_selectedAssetPath.empty() &&
                                asset.path.diskPath == m_selectedAssetPath;

        ImVec4 tileColor = isSelected ? style.Colors[ImGuiCol_HeaderActive]
                                      : style.Colors[ImGuiCol_ChildBg];
        if (tilePressed) {
            tileColor = style.Colors[ImGuiCol_ButtonActive];
        } else if (tileHovered) {
            tileColor = style.Colors[ImGuiCol_ButtonHovered];
        }
        ImGui::PushStyleColor(ImGuiCol_ChildBg, tileColor);

        ImGui::BeginChild(asset.displayName.c_str(),
                          size,
                          ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar |
                              ImGuiWindowFlags_NoScrollWithMouse);

        if (ImGui::IsWindowHovered() &&
            ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
            SetCurrentShownDirectory(targetDir);
        }

        if (!asset.isDirectory && ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            m_selectedAssetPath = asset.path.diskPath;
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
        ImGui::PopStyleColor();

        const std::string contextMenuId =
            "AssetContextMenu##" + asset.displayName;
        if (ImGui::BeginPopupContextItem(contextMenuId.c_str())) {
            if (asset.isDirectory) {
                if (ImGui::MenuItem(IsFavorited(targetDir->relativePath)
                                        ? "Remove from Favorites"
                                        : "Add to Favorites")) {
                    ToggleFavorite(
                        targetDir->relativePath, targetDir->name, true);
                }
                if (ImGui::MenuItem("Delete")) {
                    RequestDeleteConfirmation(
                        GetDirectoryAbsolutePath(targetDir),
                        true,
                        targetDir->name);
                }
                if (ImGui::MenuItem("Show in File Browser")) {
                    ShowInFileBrowser(GetDirectoryAbsolutePath(targetDir));
                }
            } else {
                if (ImGui::MenuItem(IsFavorited(asset.path.diskPath)
                                        ? "Remove from Favorites"
                                        : "Add to Favorites")) {
                    ToggleFavorite(
                        asset.path.diskPath, asset.displayName, false);
                }
                if (ImGui::MenuItem("Delete")) {
                    RequestDeleteConfirmation(
                        asset.path.absoluteDiskPath, false, asset.displayName);
                }
                if (ImGui::MenuItem("Show in File Browser")) {
                    ShowInFileBrowser(asset.path.absoluteDiskPath);
                }
            }

            ImGui::Separator();
            if (ImGui::BeginMenu("New Asset")) {
                const AssetDirectory* newAssetTarget =
                    asset.isDirectory ? targetDir : m_currentDirectory;
                DrawNewAssetMenuItems(newAssetTarget);
                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }
    }

    inline static void DrawAssetTree(const char* searchText = nullptr) {
        ImVec2 contentRegion = ImGui::GetContentRegionAvail();
        TILE_FIT_WIDTH   = static_cast<int>(contentRegion.x) / ASSET_TILE_SIZE;
        g_numShownAssets = 0;
        g_shownAssetsTotalSizeDisk = 0;
        g_shownAssetsTotalSizeVFS  = 0;

        const AssetDirectory* displayRoot = searchText && searchText[0]
                                                ? g_rootDirectory.get()
                                                : m_currentDirectory;
        if (displayRoot) {
            DrawAssetDirectory(displayRoot, searchText);
        }

        DrawContentBackgroundContextMenu();

        // Deferred so a tile's rebuild doesn't invalidate data still being
        // read by the traversal above.
        if (m_hasPendingDeletion) {
            ProcessPendingDeletion();
        }

        DrawDeleteConfirmationPopup();
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
            g_numShownAssets++;
            g_shownAssetsTotalSizeDisk += asset.sizeDisk;
            g_shownAssetsTotalSizeVFS += asset.sizeVFS;
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
            g_numShownAssets++;
            g_shownAssetsTotalSizeDisk += asset.sizeDisk;
            g_shownAssetsTotalSizeVFS += asset.sizeVFS;
        }
    }

    // SetCurrentDirectory is a macro set by some library so can't use that
    inline static void
    SetCurrentShownDirectory(const AssetDirectory* directory) {
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

    inline static const AssetDirectory* GetCurrentShownDirectory() {
        return m_currentDirectory;
    }

    inline static const AssetDirectory* SearchForDirectory(
        const std::string&    name,
        const AssetDirectory* startDirectory = g_rootDirectory.get()) {
        for (const auto& child : startDirectory->children) {
            if (child->name == name) {
                return child.get();
            }
        }
        // If not found in the immediate children, search recursively
        for (const auto& child : startDirectory->children) {
            const AssetDirectory* found = SearchForDirectory(name, child.get());
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    inline static const AssetDirectory* FindDirectoryByRelativePath(
        const std::string&    relativePath,
        const AssetDirectory* startDirectory = g_rootDirectory.get()) {
        if (startDirectory->relativePath == relativePath) {
            return startDirectory;
        }
        for (const auto& child : startDirectory->children) {
            const AssetDirectory* found =
                FindDirectoryByRelativePath(relativePath, child.get());
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    inline static const AssetInfo* FindAssetByDiskPath(
        const std::string&    diskPath,
        const AssetDirectory* startDirectory = g_rootDirectory.get()) {
        for (const auto& asset : startDirectory->assets) {
            if (asset.path.diskPath == diskPath) {
                return &asset;
            }
        }
        for (const auto& child : startDirectory->children) {
            const AssetInfo* found = FindAssetByDiskPath(diskPath, child.get());
            if (found) {
                return found;
            }
        }
        return nullptr;
    }

    inline static bool IsFavorited(const std::string& relativePath) {
        return std::any_of(g_favorites.begin(),
                           g_favorites.end(),
                           [&](const FavoriteEntry& favorite) {
                               return favorite.relativePath == relativePath;
                           });
    }

    inline static void ToggleFavorite(const std::string& relativePath,
                                      const std::string& displayName,
                                      bool               isDirectory) {
        const auto existing =
            std::find_if(g_favorites.begin(),
                         g_favorites.end(),
                         [&](const FavoriteEntry& favorite) {
                             return favorite.relativePath == relativePath;
                         });
        if (existing != g_favorites.end()) {
            g_favorites.erase(existing);
        } else {
            g_favorites.push_back({ .relativePath = relativePath,
                                    .displayName  = displayName,
                                    .isDirectory  = isDirectory });
        }
    }

    inline static void NavigateToFavorite(const FavoriteEntry& favorite) {
        if (favorite.isDirectory) {
            const AssetDirectory* directory =
                FindDirectoryByRelativePath(favorite.relativePath);
            SetCurrentShownDirectory(directory);
            return;
        }

        // Files are shown inside their parent directory; navigate there and
        // mark the file itself as selected.
        const size_t      separator = favorite.relativePath.find_last_of("/\\");
        const std::string parentRelativePath =
            separator == std::string::npos
                ? ""
                : favorite.relativePath.substr(0, separator);
        const AssetDirectory* directory =
            parentRelativePath.empty()
                ? g_rootDirectory.get()
                : FindDirectoryByRelativePath(parentRelativePath);
        SetCurrentShownDirectory(directory);
        m_selectedAssetPath = favorite.relativePath;
    }

    inline static const std::string& GetSelectedAssetPath() {
        return m_selectedAssetPath;
    }

    inline static std::string GetSelectedAssetDisplayName() {
        if (m_selectedAssetPath.empty()) {
            return "";
        }
        const AssetInfo* asset = FindAssetByDiskPath(m_selectedAssetPath);
        return asset ? asset->displayName : "";
    }
};

} // namespace SynEditor
