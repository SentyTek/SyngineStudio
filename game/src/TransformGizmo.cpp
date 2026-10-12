// ╒════════════════ TransformGizmo.cpp ═╕
// │ SyngineStudio                        │
// ├──────────────────────────────────────┤
// │ Copyright (c) SentyTek 2025-2026     │
// │ Licensed under the MIT License       │
// ╰──────────────────────────────────────╯

#include "TransformGizmo.h"

#include <algorithm>
#include <cmath>

namespace SynEditor {

namespace sm = Syngine::Math;

namespace {

constexpr const char* MOVE_AXIS[3]   = { "move_x", "move_y", "move_z" };
constexpr const char* MOVE_PLANE[3]  = { "move_xy", "move_xz", "move_yz" };
constexpr const char* ROTATE_RING[3] = { "rotate_x", "rotate_y", "rotate_z" };
constexpr const char* SCALE_AXIS[3]  = { "move_x", "move_y", "move_z" };
constexpr const char* SCALE_UNIFORM  = "scale_uniform";

// Gizmo geometry, in units of the gizmo scale (meshes are modeled at unit size)
constexpr float SCREEN_SIZE    = 0.15f; // gizmo scale per unit camera distance
constexpr float AXIS_LENGTH    = 1.0f;
constexpr float AXIS_HIT_RAD   = 0.1f;
constexpr float PLANE_MIN      = 0.10f;
constexpr float PLANE_MAX      = 0.3f;
constexpr float RING_RADIUS    = 0.6f;
constexpr float RING_HIT_WIDTH = 0.05f;
constexpr float UNIFORM_RAD    = 0.25f;
constexpr float ROTATE_SNAP    = 0.2617994f; // 15 degrees

const sm::Vec4 AXIS_COLOR[3] = { sm::Vec4(0.90f, 0.20f, 0.20f, 1.0f),
                                 sm::Vec4(0.25f, 0.85f, 0.25f, 1.0f),
                                 sm::Vec4(0.25f, 0.45f, 0.95f, 1.0f) };
const sm::Vec4 UNIFORM_COLOR(0.85f, 0.85f, 0.85f, 1.0f);
const sm::Vec4 HOVER_COLOR(1.0f, 0.85f, 0.2f, 1.0f);

sm::Vector3 AxisVec(int i) {
    return sm::Vector3(
        i == 0 ? 1.0f : 0.0f, i == 1 ? 1.0f : 0.0f, i == 2 ? 1.0f : 0.0f);
}

// Axis index (0..2) of a single-axis part, or -1
int PartAxis(TransformGizmo::Part p) {
    switch (p) {
    case TransformGizmo::Part::X: return 0;
    case TransformGizmo::Part::Y: return 1;
    case TransformGizmo::Part::Z: return 2;
    default: return -1;
    }
}

// Plane handles: XY, XZ, YZ -> index of the normal axis (Z, Y, X)
int PlaneNormalAxis(TransformGizmo::Part p) {
    switch (p) {
    case TransformGizmo::Part::XY: return 2;
    case TransformGizmo::Part::XZ: return 1;
    case TransformGizmo::Part::YZ: return 0;
    default: return -1;
    }
}

// Ray vs plane. Returns false if parallel or behind the origin.
bool RayPlane(const sm::Ray&     ray,
              const sm::Vector3& point,
              const sm::Vector3& normal,
              sm::Vector3&       out,
              float&             t) {
    float denom = normal.dot(ray.direction());
    if (std::fabs(denom) < 1e-6f) return false;
    t = normal.dot(point - ray.origin()) / denom;
    if (t < 0.0f) return false;
    out = ray.origin() + ray.direction() * t;
    return true;
}

// Parameter along line (P, a) closest to the ray. a must be normalized.
bool RayLineParam(const sm::Ray&     ray,
                  const sm::Vector3& P,
                  const sm::Vector3& a,
                  float&             lineT,
                  float&             rayS) {
    sm::Vector3 d   = ray.direction();
    sm::Vector3 w0  = P - ray.origin();
    float       b   = a.dot(d);
    float       dd  = a.dot(w0);
    float       e   = d.dot(w0);
    float       den = 1.0f - b * b;
    if (den < 1e-4f) return false;
    lineT = (b * e - dd) / den;
    rayS  = (e - b * dd) / den;
    return true;
}

Syngine::Math::Mat4 MakeModel(const sm::Vector3& x,
                              const sm::Vector3& y,
                              const sm::Vector3& z,
                              const sm::Vector3& pos,
                              float              s) {
    const float m[16] = { x.x() * s, x.y() * s, x.z() * s, 0.0f,
                          y.x() * s, y.y() * s, y.z() * s, 0.0f,
                          z.x() * s, z.y() * s, z.z() * s, 0.0f,
                          pos.x(),   pos.y(),   pos.z(),   1.0f };
    return Syngine::Math::Mat4(m);
}

float SnapTo(float v, float step) {
    return step > 1e-6f ? std::round(v / step) * step : v;
}

struct Frame {
    sm::Vector3 pos;
    sm::Vector3 axes[3];
    float       scale;
};

Frame GetFrame(Syngine::GameObject* object,
               TransformGizmo::Mode mode,
               const sm::Vector3&   cameraPos) {
    auto* tr = object->GetComponent<Syngine::TransformComponent>();
    Frame f;
    f.pos              = tr->GetWorldPosition();
    sm::Quaternion rot = mode == TransformGizmo::Mode::Scale
                             ? tr->GetWorldRotationQuaternion()
                             : sm::Quaternion();
    for (int i = 0; i < 3; i++) f.axes[i] = rot * AxisVec(i);
    f.scale = std::max(f.pos.distance(cameraPos) * SCREEN_SIZE, 0.001f);
    return f;
}

} // namespace

std::vector<std::string> TransformGizmo::GetAllGizmoMeshNames() {
    std::vector<std::string> names;
    for (const auto* group :
         { MOVE_AXIS, MOVE_PLANE, ROTATE_RING, SCALE_AXIS }) {
        for (int i = 0; i < 3; i++) names.emplace_back(group[i]);
    }
    names.emplace_back(SCALE_UNIFORM);
    return names;
}

bool TransformGizmo::Update(Syngine::GameObject* object,
                            Mode                 mode,
                            const sm::Vector3&   cameraPos,
                            const Input&         in) {
    if (!object) {
        m_hover = m_active = Part::None;
        m_dragging         = false;
        return false;
    }
    auto* tr = object->GetComponent<Syngine::TransformComponent>();
    if (!tr) return false;

    // Camera rays are not normalized, but the math below requires it
    const sm::Ray ray(in.ray.origin(), in.ray.direction().normalized());

    // Active drag
    if (m_dragging) {
        if (m_dragMode != mode || in.cancel) {
            Cancel(object);
            return true;
        }
        if (!in.down) {
            m_dragging = false;
            m_active   = Part::None;
            return false;
        }

        const Part p = m_active;
        if (m_dragMode == Mode::Move) {
            sm::Vector3 target = m_startPos;
            if (int ax = PartAxis(p); ax >= 0) {
                float t, s;
                if (RayLineParam(ray, m_startPos, m_dragAxis, t, s)) {
                    float delta = t - m_startValue;
                    if (in.snap) delta = SnapTo(delta, in.snapAmount);
                    target = m_startPos + m_dragAxis * delta;
                }
            } else {
                sm::Vector3 hit;
                float       t;
                if (RayPlane(ray, m_startPos, m_dragNormal, hit, t)) {
                    sm::Vector3 delta = hit - m_startPoint;
                    if (in.snap) {
                        delta = sm::Vector3(SnapTo(delta.x(), in.snapAmount),
                                            SnapTo(delta.y(), in.snapAmount),
                                            SnapTo(delta.z(), in.snapAmount));
                        // Only move along the plane's axes
                        delta = delta - m_dragNormal * delta.dot(m_dragNormal);
                    }
                    target = m_startPos + delta;
                }
            }
            tr->SetWorldPosition(target);
        } else if (m_dragMode == Mode::Rotate) {
            sm::Vector3 hit;
            float       t;
            if (RayPlane(ray, m_startPos, m_dragAxis, hit, t)) {
                sm::Vector3 a = m_startPoint;
                sm::Vector3 b = hit - m_startPos;
                float angle = std::atan2(m_dragAxis.dot(a.cross(b)), a.dot(b));
                if (in.snap) angle = SnapTo(angle, ROTATE_SNAP);
                tr->SetWorldRotationQuat(sm::Quaternion(m_dragAxis, angle) *
                                         m_startRot);
            }
        } else {
            float ratio = 1.0f;
            if (p == Part::Uniform) {
                sm::Vector3 hit;
                float       t;
                if (RayPlane(ray, m_startPos, m_dragNormal, hit, t) &&
                    m_startValue > 1e-4f) {
                    ratio = (hit - m_startPos).length() / m_startValue;
                }
            } else {
                float t, s;
                if (RayLineParam(ray, m_startPos, m_dragAxis, t, s) &&
                    std::fabs(m_startValue) > 1e-4f) {
                    ratio = t / m_startValue;
                }
            }
            ratio          = std::max(ratio, 0.001f);
            sm::Vector3 sc = m_startScale;
            if (p == Part::Uniform) {
                sc = m_startScale * ratio;
            } else if (int ax = PartAxis(p); ax >= 0) {
                const float v = ax == 0   ? m_startScale.x()
                                : ax == 1 ? m_startScale.y()
                                          : m_startScale.z();
                sc.set(ax, v * ratio);
            }
            if (in.snap) {
                sc = sm::Vector3(SnapTo(sc.x(), in.snapAmount),
                                 SnapTo(sc.y(), in.snapAmount),
                                 SnapTo(sc.z(), in.snapAmount));
                sc = sm::Vector3(std::max(sc.x(), 0.001f),
                                 std::max(sc.y(), 0.001f),
                                 std::max(sc.z(), 0.001f));
            }
            tr->SetScale(sc);
        }
        return true;
    }

    // Hover
    m_hover = Part::None;
    if (!in.rayValid) return false;

    const Frame f = GetFrame(object, mode, cameraPos);

    float best = FLT_MAX;

    auto consider = [&](Part p, float depth) {
        if (depth < best) {
            best    = depth;
            m_hover = p;
        }
    };

    const Part axisParts[3]  = { Part::X, Part::Y, Part::Z };
    const Part planeParts[3] = { Part::YZ,
                                 Part::XZ,
                                 Part::XY }; // by normal axis

    auto testAxis = [&](int i) {
        float t, s;
        if (!RayLineParam(ray, f.pos, f.axes[i], t, s) || s < 0.0f) return;
        t             = std::clamp(t, 0.0f, AXIS_LENGTH * f.scale);
        sm::Vector3 q = f.pos + f.axes[i] * t;
        float       d = (q - ray.origin()).cross(ray.direction()).length();
        if (d < AXIS_HIT_RAD * f.scale) consider(axisParts[i], s);
    };

    if (mode == Mode::Move) {
        for (int n = 0; n < 3; n++) {
            sm::Vector3 hit;
            float       t;
            if (!RayPlane(ray, f.pos, f.axes[n], hit, t)) continue;
            sm::Vector3 r = hit - f.pos;
            int         u = (n + 1) % 3, v = (n + 2) % 3;
            float       ru = r.dot(f.axes[u]) / f.scale;
            float       rv = r.dot(f.axes[v]) / f.scale;
            if (ru > PLANE_MIN && ru < PLANE_MAX && rv > PLANE_MIN &&
                rv < PLANE_MAX)
                consider(planeParts[n], t - 0.001f * f.scale);
        }
        for (int i = 0; i < 3; i++) testAxis(i);
    } else if (mode == Mode::Scale) {
        sm::Vector3 toCenter = f.pos - ray.origin();
        float       s        = toCenter.dot(ray.direction());
        float       d = (f.pos - ray.origin()).cross(ray.direction()).length();
        for (int i = 0; i < 3; i++) testAxis(i);
        if (s > 0.0f && d < UNIFORM_RAD * f.scale)
            consider(Part::Uniform, s - 0.001f * f.scale);
    } else {
        for (int i = 0; i < 3; i++) {
            sm::Vector3 hit;
            float       t;
            if (!RayPlane(ray, f.pos, f.axes[i], hit, t)) continue;
            float r = (hit - f.pos).length() / f.scale;
            if (std::fabs(r - RING_RADIUS) < RING_HIT_WIDTH)
                consider(axisParts[i], t);
        }
    }

    if (m_hover == Part::None || !in.pressed) return m_hover != Part::None;

    // Begin drag
    m_dragging   = true;
    m_active     = m_hover;
    m_dragMode   = mode;
    m_startPos   = tr->GetWorldPosition();
    m_startRot   = tr->GetWorldRotationQuaternion();
    m_startScale = tr->GetScale();
    const int ax = PartAxis(m_active);

    if (mode == Mode::Move) {
        if (ax >= 0) {
            m_dragAxis = f.axes[ax];
            float t, s;
            m_startValue =
                RayLineParam(ray, m_startPos, m_dragAxis, t, s) ? t : 0.0f;
        } else {
            m_dragNormal = f.axes[PlaneNormalAxis(m_active)];
            sm::Vector3 hit;
            float       t;
            m_startPoint = RayPlane(ray, m_startPos, m_dragNormal, hit, t)
                               ? hit
                               : m_startPos;
        }
    } else if (mode == Mode::Rotate) {
        m_dragAxis = f.axes[ax];
        sm::Vector3 hit;
        float       t;
        if (RayPlane(ray, m_startPos, m_dragAxis, hit, t)) {
            m_startPoint = hit - m_startPos;
        } else {
            m_dragging = false;
            m_active   = Part::None;
        }
        if (m_dragging && m_startPoint.length() < 1e-5f) {
            m_dragging = false;
            m_active   = Part::None;
        }
    } else {
        if (m_active == Part::Uniform) {
            m_dragNormal = (cameraPos - m_startPos).normalized();
            sm::Vector3 hit;
            float       t;
            m_startValue = RayPlane(ray, m_startPos, m_dragNormal, hit, t)
                               ? (hit - m_startPos).length()
                               : 0.0f;
        } else {
            m_dragAxis = f.axes[ax];
            float t, s;
            m_startValue =
                RayLineParam(ray, m_startPos, m_dragAxis, t, s) ? t : 0.0f;
        }
    }
    return true;
}

void TransformGizmo::Restore(Syngine::GameObject* object) const {
    if (!object) return;
    auto* tr = object->GetComponent<Syngine::TransformComponent>();
    if (!tr) return;
    tr->SetWorldPosition(m_startPos);
    tr->SetWorldRotationQuat(m_startRot);
    tr->SetScale(m_startScale);
}

void TransformGizmo::Cancel(Syngine::GameObject* object) {
    if (m_dragging) Restore(object);
    m_dragging = false;
    m_active   = Part::None;
    m_hover    = Part::None;
}

void TransformGizmo::Submit(Syngine::GameObject* object,
                            Mode                 mode,
                            const sm::Vector3&   cameraPos) const {
    if (!object || !object->GetComponent<Syngine::TransformComponent>()) return;
    if ((cameraPos - object->GetComponent<Syngine::TransformComponent>()
                         ->GetWorldPosition())
            .length() > 90.0f)
        return;
    const Frame f = GetFrame(object, mode, cameraPos);

    auto color = [&](Part p, const sm::Vec4& base) {
        return (p == m_active || (!m_dragging && p == m_hover)) ? HOVER_COLOR
                                                                : base;
    };
    auto push = [&](const char* mesh, const sm::Vec4& c) {
        Syngine::Debug::GizmoOverlay::Item item;
        item.meshName = mesh;
        item.model = MakeModel(f.axes[0], f.axes[1], f.axes[2], f.pos, f.scale);
        item.color = c;
        Syngine::Debug::GizmoOverlay::Submit(item);
    };

    const Part axisParts[3]  = { Part::X, Part::Y, Part::Z };
    const Part planeParts[3] = { Part::YZ, Part::XZ, Part::XY };
    // Plane meshes are named by their axes (xy, xz, yz); order by the normal
    const char* const* planeNames  = MOVE_PLANE;
    const int          planeIdx[3] = { 2, 1, 0 }; // normal axis n -> mesh index

    if (mode == Mode::Move) {
        for (int i = 0; i < 3; i++)
            push(MOVE_AXIS[i], color(axisParts[i], AXIS_COLOR[i]));
        for (int n = 0; n < 3; n++)
            push(planeNames[planeIdx[n]], color(planeParts[n], AXIS_COLOR[n]));
    } else if (mode == Mode::Rotate) {
        for (int i = 0; i < 3; i++)
            push(ROTATE_RING[i], color(axisParts[i], AXIS_COLOR[i]));
    } else {
        for (int i = 0; i < 3; i++)
            push(SCALE_AXIS[i], color(axisParts[i], AXIS_COLOR[i]));
        push(SCALE_UNIFORM, color(Part::Uniform, UNIFORM_COLOR));
    }
}

} // namespace SynEditor
