// ╒══════════════════ TransformGizmo.h ═╕
// │ SyngineStudio                        │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#pragma once
#include <Syngine/Syngine.h>

namespace SynEditor {

/// @brief Move/Rotate/Scale gizmo. Hit-testing and dragging are analytic (ray
/// math); meshes are only used for display via Syngine::GizmoOverlay.
class TransformGizmo {
  public:
    enum class Mode { Move, Rotate, Scale };
    enum class Part { None, X, Y, Z, XY, XZ, YZ, Uniform };

    struct Input {
        Syngine::Math::Ray ray;                //* Ray under the mouse cursor
        bool               rayValid   = false; //* Mouse is over the viewport
        bool               pressed    = false; //* LMB went down this frame
        bool               down       = false; //* LMB is held
        bool               cancel     = false; //* Escape pressed
        bool               snap       = false;
        float              snapAmount = 1.0f;
    };

    /// @brief Update hover/drag state and edit the object.
    /// @return true if the gizmo consumed the mouse (hovering or dragging)
    bool Update(Syngine::GameObject*          object,
                Mode                          mode,
                const Syngine::Math::Vector3& cameraPos,
                const Input&                  input);

    /// @brief Queue the gizmo meshes for rendering
    void Submit(Syngine::GameObject*          object,
                Mode                          mode,
                const Syngine::Math::Vector3& cameraPos) const;

    /// @brief Abort the active drag, restoring the object's start values
    void Cancel(Syngine::GameObject* object);

    bool IsDragging() const { return m_dragging; }

    /// @brief Names of every mesh the gizmo can draw, for preloading
    static std::vector<std::string> GetAllGizmoMeshNames();

  private:
    Part m_hover    = Part::None;
    Part m_active   = Part::None;
    bool m_dragging = false;
    Mode m_dragMode = Mode::Move;

    Syngine::Math::Vector3    m_startPos;
    Syngine::Math::Quaternion m_startRot;
    Syngine::Math::Vector3    m_startScale;
    Syngine::Math::Vector3    m_startPoint; // Mode-specific start reference
    float                     m_startValue = 1.0f;
    Syngine::Math::Vector3    m_dragAxis;
    Syngine::Math::Vector3    m_dragNormal;

    void Restore(Syngine::GameObject* object) const;
};

} // namespace SynEditor
