// ╒════════════════════ AssetWindow.hpp ═╕
// │ Syngine Studio                       │
// │ Created 2026-09-03                   │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once

#include <Syngine/Syngine.h>
#include "bgfx/bgfx.h"

#include <imgui/imgui_internal.h>
#include <imgui/imgui.h>
#include <miniscl.hpp>

#include <memory>
#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

namespace SynEditor {

#define ASSET_SOURCE_TO_STRING(x) ((x) == AssetSource::GAME ? "GAME" : "ENGINE")
#define ASSET_TYPE_TO_STRING(x)                                                \
    ((x) == AssetType::TEXTURE     ? "TEXTURE"                                 \
     : (x) == AssetType::MODEL     ? "MODEL"                                   \
     : (x) == AssetType::SOUND     ? "SOUND"                                   \
     : (x) == AssetType::SCRIPT    ? "SCRIPT"                                  \
     : (x) == AssetType::SHADER    ? "SHADER"                                  \
     : (x) == AssetType::ANIMATION ? "ANIMATION"                               \
     : (x) == AssetType::PREFAB    ? "PREFAB"                                  \
     : (x) == AssetType::SCENE     ? "SCENE"                                   \
                                   : "UNKNOWN")

enum class AssetSource { GAME, ENGINE };

// Since assets are on both disk and the engine VFS, we just keep track of both
// paths here
struct AssetPath {
    std::string diskPath; // Relative to the project
    scl::string bundlePath;
    std::string absoluteDiskPath; // Real on-disk path, used for delete/reveal
    scl::string pathInBundle;
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

    static void _DestroyThumbnails(AssetDirectory* directory);

  public:
    static std::unique_ptr<AssetDirectory> g_rootDirectory;
    static int                             g_numShownAssets;
    static uint32_t                        g_shownAssetsTotalSizeDisk;
    static uint32_t                        g_shownAssetsTotalSizeVFS;
    static std::vector<FavoriteEntry>      g_favorites;

    // the currently selected (non-directory) asset, empty if none
    inline static AssetInfo* m_selectedAsset = nullptr;

    static bool ContainsInsensitive(const std::string& text,
                                    const char*        searchText);

    static void BuildFileTree(scl::path projectPath, std::string projectName);

    // Re-scans disk/VFS from scratch, e.g. after an asset was deleted
    static void RebuildTree();

    // Must be called before bgfx shuts down, since g_rootDirectory is static
    // and otherwise outlives bgfx teardown.
    static void Shutdown();

    static std::string
    GetDirectoryAbsolutePath(const AssetDirectory* directory);

    static void RequestDelete(const std::string& absolutePath,
                              bool               isDirectory,
                              const std::string& label);

    // Queues the confirmation modal to open; the actual deletion only happens
    // once the user confirms it.
    static void RequestDeleteConfirmation(const std::string& absolutePath,
                                          bool               isDirectory,
                                          const std::string& label);

    static void DrawDeleteConfirmationPopup();

    static void ProcessPendingDeletion();

    static void ShowInFileBrowser(const std::string& absolutePath);

    static void CreateNewAsset(const char*           assetKind,
                               const AssetDirectory* targetDirectory,
                               const std::string&    assetName);

    // Shared by both a tile's context menu and the content browser's
    // background context menu.
    static void DrawNewAssetMenuItems(const AssetDirectory* targetDirectory);

    // Right-click anywhere over empty space in the content browser
    static void DrawContentBackgroundContextMenu();

    // Draws a little asset tile, designed to be tileable
    static void DrawAsset(const AssetInfo&      asset,
                          const AssetDirectory* targetDir,
                          bool                  sameLine);

    static void DrawAssetTree(const char* searchText = nullptr);

    static void DrawAssetDirectory(const AssetDirectory* directory,
                                   const char*           searchText);

    static void DrawAssetSearchResults(const AssetDirectory* directory,
                                       const char*           searchText,
                                       int&                  assetIndex);

    // SetCurrentDirectory is a macro set by some library so can't use that
    static void SetCurrentShownDirectory(const AssetDirectory* directory);

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

    static const AssetDirectory* SearchForDirectory(
        const std::string&    name,
        const AssetDirectory* startDirectory = g_rootDirectory.get());

    static const AssetDirectory* FindDirectoryByRelativePath(
        const std::string&    relativePath,
        const AssetDirectory* startDirectory = g_rootDirectory.get());

    static const AssetInfo* FindAssetByDiskPath(
        const std::string&    diskPath,
        const AssetDirectory* startDirectory = g_rootDirectory.get());

    inline static bool IsFavorited(const std::string& relativePath) {
        return std::any_of(g_favorites.begin(),
                           g_favorites.end(),
                           [&](const FavoriteEntry& favorite) {
                               return favorite.relativePath == relativePath;
                           });
    }

    static void ToggleFavorite(const std::string& relativePath,
                               const std::string& displayName,
                               bool               isDirectory);

    static void NavigateToFavorite(const FavoriteEntry& favorite);

    inline static AssetInfo* GetSelectedAsset() { return m_selectedAsset; }

    inline static std::string GetSelectedAssetDisplayName() {
        if (!m_selectedAsset) {
            return "";
        }
        return m_selectedAsset->displayName;
    }

    static bgfx::TextureHandle TryGenerateThumbnail(scl::path   bundle,
                                                    scl::string pathInBundle);
};

} // namespace SynEditor
