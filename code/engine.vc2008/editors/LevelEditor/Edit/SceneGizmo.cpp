#include "stdafx.h"
#pragma hdrstop

#include "SceneGizmo.h"
#include "Scene.h"
#include "CustomObject.h"
#include "UI_LevelTools.h"
#include "../../ECore/Editor/ui_main.h"
#include "../../ECore/Editor/device.h"
#include "../../ECore/Editor/d3dutils.h"
#include "../../Layers/xrRenderED11/HW11.h"

CSceneGizmo Gizmo;

//----------------------------------------------------------------------------
// Axis colors (ABGR for FVF::L). Highlighted when hovered.
static const u32 CLR_X      = 0xFF2020FF; // red
static const u32 CLR_Y      = 0xFF20FF20; // green
static const u32 CLR_Z      = 0xFFFF8020; // blue
static const u32 CLR_HOVER  = 0xFF00FFFF; // yellow (hovered element)
static const u32 CLR_SCREEN = 0xFFC0C0C0; // light grey (screen/uniform)

//----------------------------------------------------------------------------
void CSceneGizmo::GetAxes(Fvector& ax, Fvector& ay, Fvector& az) const
{
    ax.set(m_xform.i);
    ay.set(m_xform.j);
    az.set(m_xform.k);
}

//----------------------------------------------------------------------------
// Two unit vectors perpendicular to 'd' (for building arrow heads / rings).
static void Perp2(const Fvector& d, Fvector& p1, Fvector& p2)
{
    Fvector up;
    if (_abs(d.y) < 0.9f) up.set(0.f, 1.f, 0.f); else up.set(1.f, 0.f, 0.f);
    p1.crossproduct(d, up); p1.normalize_safe();
    p2.crossproduct(d, p1); p2.normalize_safe();
}

// ---- Solid geometry (filled triangles, position-only; colored per draw call) ----
// Built into an Fvector triangle list and drawn via DU_impl::DrawPrimitiveL, which
// routes to both DX9 and DX11 — so both renderers get the same volumetric handles.
static void PushTri(xr_vector<Fvector>& v, const Fvector& a, const Fvector& b, const Fvector& c)
{
    v.push_back(a); v.push_back(b); v.push_back(c);
}

// Thick shaft: a camera-facing quad of width 'w' from o to tip.
static void PushShaft(xr_vector<Fvector>& v, const Fvector& o, const Fvector& tip, float w)
{
    Fvector axis; axis.sub(tip, o); axis.normalize_safe();
    Fvector view; view.sub(o, EDevice.vCameraPosition); view.normalize_safe();
    Fvector perp; perp.crossproduct(axis, view); perp.normalize_safe(); perp.mul(w * 0.5f);
    Fvector a, b, c, d;
    a.sub(o, perp); b.add(o, perp); c.sub(tip, perp); d.add(tip, perp);
    PushTri(v, a, b, c);
    PushTri(v, c, b, d);
}

// Filled cone (arrow head): base ring at 'base' along 'dir', tip at base+dir*height.
static void PushCone(xr_vector<Fvector>& v, const Fvector& base, const Fvector& dir,
                     float radius, float height)
{
    Fvector tip; tip.mad(base, dir, height);
    Fvector p1, p2; Perp2(dir, p1, p2);
    const int N = 10;
    Fvector prev;
    for (int i = 0; i <= N; ++i) {
        float ang = i * (PI_MUL_2 / N);
        Fvector rim; rim.set(base);
        rim.mad(rim, p1, radius * _cos(ang));
        rim.mad(rim, p2, radius * _sin(ang));
        if (i > 0) {
            PushTri(v, prev, rim, tip);    // side
            PushTri(v, prev, base, rim);   // base cap
        }
        prev = rim;
    }
}

// Full solid move-arrow along one axis.
static void PushAxisArrow(xr_vector<Fvector>& v, const Fvector& o, const Fvector& dir, float len)
{
    const float head_h = len * 0.22f;
    const float head_r = len * 0.05f;
    const float shaft_w = len * 0.016f;
    Fvector shaft_end; shaft_end.mad(o, dir, len - head_h);
    PushShaft(v, o, shaft_end, shaft_w);
    PushCone (v, shaft_end, dir, head_r, head_h);
}

// Solid axis-aligned box (scale handle) centered at c with half-extent 'h'.
static void PushBox(xr_vector<Fvector>& v, const Fvector& c, float h)
{
    Fvector mn, mx;
    mn.set(c.x - h, c.y - h, c.z - h);
    mx.set(c.x + h, c.y + h, c.z + h);
    Fvector p[8] = {
        {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},
        {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z}
    };
    const int q[6][4] = {
        {0,1,2,3}, {5,4,7,6}, {1,5,6,2}, {4,0,3,7}, {3,2,6,7}, {4,5,1,0}
    };
    for (int i = 0; i < 6; ++i) {
        PushTri(v, p[q[i][0]], p[q[i][1]], p[q[i][2]]);
        PushTri(v, p[q[i][0]], p[q[i][2]], p[q[i][3]]);
    }
}

// Draw one single-colored solid element (triangle list), double-sided.
static void DrawSolid(xr_vector<Fvector>& v, u32 clr)
{
    if (v.empty()) return;
    DU_impl.DrawPrimitiveL(D3DPT_TRIANGLELIST, (u32)(v.size() / 3),
                           v.data(), (int)v.size(), clr, FALSE /*no cull*/, FALSE);
}

// Distance from point 'p' to ray (start + t*dir, t>=0).
static float RayPointDist(const Fvector& start, const Fvector& dir, const Fvector& p)
{
    Fvector w; w.sub(p, start);
    float t = w.dotproduct(dir);
    if (t < 0.f) t = 0.f;
    Fvector pc; pc.mad(start, dir, t);
    return pc.distance_to(p);
}

// Rotate ring: circle of radius R in the plane perpendicular to 'axis', centered at o.
static void DrawRing(const Fvector& o, const Fvector& axis, float R, u32 clr)
{
    Fvector p1, p2; Perp2(axis, p1, p2);
    const int N = 48;
    Fvector prev;
    for (int i = 0; i <= N; ++i) {
        float a = i * (PI_MUL_2 / N);
        Fvector pt; pt.set(o);
        pt.mad(pt, p1, R * _cos(a));
        pt.mad(pt, p2, R * _sin(a));
        if (i > 0) DU_impl.DrawLine(prev, pt, clr);
        prev = pt;
    }
}

//----------------------------------------------------------------------------
// Per-tool gizmo policy (requested per level-design). Default = full gizmo.
void CSceneGizmo::AllowedGroups(bool& mv, bool& rot, bool& scl) const
{
    mv = rot = scl = true;
    const int cls = LTools ? LTools->CurrentClassID() : OBJCLASS_DUMMY;
    switch (cls)
    {
    case OBJCLASS_SECTOR:                       // sectors: no gizmo at all
        mv = rot = scl = false;             break;
    case OBJCLASS_LIGHT:                        // move-only tools
    case OBJCLASS_SOUND_SRC:
    case OBJCLASS_SPAWNPOINT:
    case OBJCLASS_WAY:
    case OBJCLASS_PORTAL:
        rot = scl = false;                  break;
    case OBJCLASS_PS:                           // static particles: no scale
        scl = false;                        break;
    default:                                    break;   // everything else: full
    }
}

bool CSceneGizmo::Update()
{
    m_visible = false;
    // No gizmo (no scene/tools or nothing selected) → clear hover/active state so it can't
    // linger. After Undo (Unload+Load) or a delete the selection goes empty; a stale m_hover
    // would make MouseStart keep grabbing the gizmo (blocking object selection, cursor stuck as
    // the grab cursor), and a stale m_active would divert MouseMove into Gizmo::Drag (freezing
    // the cursor / selection). Resetting here lets the gizmo self-heal on the next frame.
    if (!Scene || !LTools) { m_hover = geNone; m_active = geNone; return false; }

    // Pivot = center of the currently SELECTED objects (bbox centers averaged).
    ObjectList lst;
    int n = Scene->GetQueryObjects(lst, LTools->CurrentClassID(), 1, -1, -1);
    if (n == 0) { m_hover = geNone; m_active = geNone; return false; }

    // Tool with no allowed manipulation groups (e.g. sectors) → don't show the gizmo.
    { bool a_mv, a_rot, a_scl; AllowedGroups(a_mv, a_rot, a_scl);
      if (!a_mv && !a_rot && !a_scl) { m_hover = geNone; m_active = geNone; return false; } }

    Fvector center = {0.f, 0.f, 0.f};
    for (ObjectIt it = lst.begin(); it != lst.end(); ++it)
    {
        Fbox bb; bb.invalidate();
        Fvector c;
        if ((*it)->GetBox(bb)) bb.getcenter(c);
        else                   c.set((*it)->FPosition);
        center.add(c);
    }
    center.div((float)n);

    m_xform.identity();
    m_xform.c.set(center);

    // Local vs World orientation — driven by the existing "Coordinate System: Parent"
    // toolbar toggle (etfCSParent). Parent/world → axis-aligned; otherwise → local basis
    // of the first selected object.
    m_space = (Tools && Tools->GetSettings(etfCSParent)) ? gsWorld : gsLocal;
    if (m_space == gsLocal && !lst.empty())
    {
        Fmatrix t = lst.front()->_Transform();
        m_xform.i.set(t.i); m_xform.i.normalize_safe();
        m_xform.j.set(t.j); m_xform.j.normalize_safe();
        m_xform.k.set(t.k); m_xform.k.normalize_safe();
    }

    // Screen-constant size: scale the gizmo with distance so it looks the same on screen.
    float dist = EDevice.vCameraPosition.distance_to(center);
    if (dist < 0.1f) dist = 0.1f;
    m_screen_scale = dist * 0.2f;

    m_visible = true;

    // Update hovered element from the current cursor ray (unless dragging an element).
    if (m_active == geNone && UI)
        HitTest(UI->m_CurrentRStart, UI->m_CurrentRDir);

    return true;
}

//----------------------------------------------------------------------------
void CSceneGizmo::Render()
{
    if (!m_visible) return;
    bool a_mv, a_rot, a_scl; AllowedGroups(a_mv, a_rot, a_scl);

    // Draw over the scene: disable depth test so the gizmo is never occluded.
    if (g_bEditorDX11) {
        HW11.States.depth_enable = false;
        HW11.States.ds_dirty     = true;
        HW11.FlushStates();
    } else {
        EDevice.SetRS(D3DRS_ZENABLE, FALSE);
    }

    Fvector ax, ay, az;
    GetAxes(ax, ay, az);
    const Fvector o = m_xform.c;
    const float   L = AxisLen();

    // Move-arrow colors (highlight when hovered).
    const u32 mx = (m_hover == geMoveX) ? CLR_HOVER : CLR_X;
    const u32 my = (m_hover == geMoveY) ? CLR_HOVER : CLR_Y;
    const u32 mz = (m_hover == geMoveZ) ? CLR_HOVER : CLR_Z;
    // Rotate-ring colors.
    const u32 rx = (m_hover == geRotX) ? CLR_HOVER : CLR_X;
    const u32 ry = (m_hover == geRotY) ? CLR_HOVER : CLR_Y;
    const u32 rz = (m_hover == geRotZ) ? CLR_HOVER : CLR_Z;
    // Scale-handle colors.
    const u32 sx = (m_hover == geScaleX) ? CLR_HOVER : CLR_X;
    const u32 sy = (m_hover == geScaleY) ? CLR_HOVER : CLR_Y;
    const u32 sz = (m_hover == geScaleZ) ? CLR_HOVER : CLR_Z;
    const float ringR  = L * 0.9f;
    const float scaleD = L * 1.25f;      // scale handle distance from center
    const float boxH   = L * 0.045f;     // scale handle half-size

    // Solid handles — same geometry for DX9 and DX11. Force double-sided so the
    // billboard shafts never cull away (DX9 via DrawPrimitiveL's bCull=FALSE; DX11
    // via an explicit CULL_NONE state, since the DX11 path ignores per-call cull).
    D3D11_CULL_MODE saved_cull = D3D11_CULL_BACK;
    if (g_bEditorDX11) {
        saved_cull = HW11.States.cull_mode;
        HW11.States.cull_mode = D3D11_CULL_NONE;
        HW11.States.rs_dirty  = true;
        HW11.FlushStates();
    }
    {
        static xr_vector<Fvector> vv;
        vv.reserve(256);
        Fvector sxp, syp, szp;
        sxp.mad(o, ax, scaleD); syp.mad(o, ay, scaleD); szp.mad(o, az, scaleD);
        const u32 cu = (m_hover == geUniform) ? CLR_HOVER : CLR_SCREEN;

        if (a_mv) {
            vv.clear(); PushAxisArrow(vv, o, ax, L); DrawSolid(vv, mx);
            vv.clear(); PushAxisArrow(vv, o, ay, L); DrawSolid(vv, my);
            vv.clear(); PushAxisArrow(vv, o, az, L); DrawSolid(vv, mz);
        }
        if (a_scl) {
            vv.clear(); PushBox(vv, sxp, boxH);      DrawSolid(vv, sx);
            vv.clear(); PushBox(vv, syp, boxH);      DrawSolid(vv, sy);
            vv.clear(); PushBox(vv, szp, boxH);      DrawSolid(vv, sz);
            vv.clear(); PushBox(vv, o, L * 0.06f);   DrawSolid(vv, cu);   // uniform-scale cube
        }
    }
    if (g_bEditorDX11) {
        HW11.States.cull_mode = saved_cull;
        HW11.States.rs_dirty  = true;
        HW11.FlushStates();
    }

    // Rotate rings (lines, both renderers).
    if (a_rot) {
        DrawRing(o, ax, ringR, rx);
        DrawRing(o, ay, ringR, ry);
        DrawRing(o, az, ringR, rz);
    }

    // XZ move plane (always world-horizontal, normal = world Y).
    if (a_mv) {
        const Fvector wx = {1.f,0.f,0.f}, wz = {0.f,0.f,1.f};
        const float pd = L * 0.30f, ps = L * 0.32f;
        Fvector q0, q1, q2, q3;
        q0.mad(o,  wx, pd); q0.mad(q0, wz, pd);
        q1.mad(q0, wx, ps);
        q2.mad(q1, wz, ps);
        q3.mad(q0, wz, ps);
        const u32 cp = (m_hover == gePlaneZX) ? CLR_HOVER : CLR_SCREEN;
        DU_impl.DrawLine(q0, q1, cp);
        DU_impl.DrawLine(q1, q2, cp);
        DU_impl.DrawLine(q2, q3, cp);
        DU_impl.DrawLine(q3, q0, cp);
    }

    // restore depth test
    if (g_bEditorDX11) {
        HW11.States.depth_enable = true;
        HW11.States.ds_dirty     = true;
        HW11.FlushStates();
    } else {
        EDevice.SetRS(D3DRS_ZENABLE, TRUE);
    }
}

//----------------------------------------------------------------------------
// Shortest distance between ray (rp + t*rd, t>=0) and segment [s0,s1].
static float RaySegDist(const Fvector& rp, const Fvector& rd,
                        const Fvector& s0, const Fvector& s1)
{
    Fvector u; u.sub(s1, s0);            // segment direction
    Fvector w; w.sub(rp, s0);            // ray-origin relative to segment start
    float a = rd.dotproduct(rd);
    float b = rd.dotproduct(u);
    float c = u.dotproduct(u);
    float d = rd.dotproduct(w);
    float e = u.dotproduct(w);
    float D = a*c - b*b;
    float sc, tc;
    if (D < EPS_S) { sc = 0.f; tc = (c > EPS_S) ? (e / c) : 0.f; }   // near-parallel
    else           { sc = (b*e - c*d) / D; tc = (a*e - b*d) / D; }
    if (sc < 0.f) sc = 0.f;              // ray: t >= 0
    clamp(tc, 0.f, 1.f);                 // segment: [0,1]
    Fvector pc; pc.mad(rp, rd, sc);
    Fvector qc; qc.mad(s0, u, tc);
    return pc.distance_to(qc);
}

EGizmoElem CSceneGizmo::HitTest(const Fvector& start, const Fvector& dir)
{
    m_hover = geNone;
    if (!m_visible) return geNone;
    bool a_mv, a_rot, a_scl; AllowedGroups(a_mv, a_rot, a_scl);

    Fvector ax, ay, az;
    GetAxes(ax, ay, az);
    const Fvector o = m_xform.c;
    const float   L = AxisLen();

    const float pick_r = L * 0.12f;      // pick tolerance (world; screen-constant via L)
    const float ringR  = L * 0.9f;
    const float scaleD = L * 1.25f;
    float best = pick_r;

    // Central cube — uniform scale (highest priority: it's at the pivot).
    if (a_scl && RayPointDist(start, dir, o) < L * 0.14f) {
        m_hover = geUniform;
        return m_hover;
    }

    // Scale handles (cubes at the axis ends) — checked first (outermost, distinct).
    if (a_scl) {
        Fvector raxis[3] = { ax, ay, az };
        EGizmoElem se[3] = { geScaleX, geScaleY, geScaleZ };
        for (int i = 0; i < 3; ++i) {
            Fvector hp; hp.mad(o, raxis[i], scaleD);
            float dst = RayPointDist(start, dir, hp);
            if (dst < best) { best = dst; m_hover = se[i]; }
        }
    }

    // XZ move plane: ray vs the world-horizontal plane, hit point inside the square.
    if (a_mv && m_hover == geNone) {
        const Fvector wx = {1.f,0.f,0.f}, wy = {0.f,1.f,0.f}, wz = {0.f,0.f,1.f};
        const float pd = L * 0.30f, ps = L * 0.32f;
        Fplane pl; pl.build(o, wy);
        float d;
        if (pl.intersectRayDist(start, dir, d) && d >= 0.f) {
            Fvector P; P.mad(start, dir, d);
            Fvector rel; rel.sub(P, o);
            float u = wx.dotproduct(rel), v = wz.dotproduct(rel);
            if (u >= pd && u <= pd + ps && v >= pd && v <= pd + ps)
                m_hover = gePlaneZX;
        }
    }

    // Move arrows (center). Only if nothing else was hit.
    if (a_mv && m_hover == geNone) {
        struct { Fvector dir; EGizmoElem e; } axes[3] = {
            { ax, geMoveX }, { ay, geMoveY }, { az, geMoveZ }
        };
        for (int i = 0; i < 3; ++i) {
            Fvector tip; tip.mad(o, axes[i].dir, L);
            float dst = RaySegDist(start, dir, o, tip);
            if (dst < best) { best = dst; m_hover = axes[i].e; }
        }
    }

    // Rotate rings: intersect the ray with the ring plane, check |hit-center| ~ R.
    if (a_rot && m_hover == geNone) {
        EGizmoElem rots[3] = { geRotX, geRotY, geRotZ };
        Fvector raxis[3]   = { ax, ay, az };
        float best_ring = pick_r;
        for (int i = 0; i < 3; ++i) {
            Fplane pl; pl.build(o, raxis[i]);
            float d;
            if (!pl.intersectRayDist(start, dir, d) || d < 0.f) continue;
            Fvector P; P.mad(start, dir, d);
            float rr = o.distance_to(P);
            float diff = _abs(rr - ringR);
            if (diff < best_ring) { best_ring = diff; m_hover = rots[i]; }
        }
    }
    return m_hover;
}

//----------------------------------------------------------------------------
// Project the cursor ray onto the manipulation axis (line origin+t*axis):
// intersect the ray with the plane that contains the axis and faces the camera,
// then take the projection of the hit point onto the axis. Returns t (world units).
static bool ProjectRayOnAxis(const Fvector& start, const Fvector& dir,
                             const Fvector& origin, const Fvector& axis, float& t)
{
    // plane normal: contains 'axis', most perpendicular to the view ray
    Fvector n; n.crossproduct(axis, dir);          // perp to axis and view
    Fvector pn; pn.crossproduct(n, axis);          // plane normal (plane holds axis)
    if (pn.square_magnitude() < EPS_S) return false;
    pn.normalize();

    Fplane pl; pl.build(origin, pn);
    float d;
    if (!pl.intersectRayDist(start, dir, d)) return false;

    Fvector P; P.mad(start, dir, d);
    Fvector rel; rel.sub(P, origin);
    t = axis.dotproduct(rel);
    return true;
}

// Angle of the cursor ray's hit point on the ring plane, around the ring axis.
static bool RingAngle(const Fvector& start, const Fvector& dir,
                      const Fvector& origin, const Fvector& axis, float& ang)
{
    Fplane pl; pl.build(origin, axis);
    float d;
    if (!pl.intersectRayDist(start, dir, d)) return false;
    Fvector P; P.mad(start, dir, d);
    Fvector rel; rel.sub(P, origin);
    Fvector p1, p2; Perp2(axis, p1, p2);
    ang = atan2f(rel.dotproduct(p2), rel.dotproduct(p1));
    return true;
}

// Per-object transform pivot (Maya-style "Center Pivot" toggle):
//   center_pivot == true  -> object's own geometric center (world bbox center),
//                            so it rotates/scales in place regardless of how far
//                            its origin sits in world space;
//   center_pivot == false -> object's world position (FPosition / origin).
static void ObjPivot(CCustomObject* o, bool center_pivot, Fvector& pivot)
{
    if (center_pivot) {
        Fbox bb;
        if (o->GetBox(bb)) { bb.getcenter(pivot); return; }
    }
    pivot.set(o->PPosition);
}

// Apply a scale delta to the selection around each object's pivot.
static void ApplyScaleTo(ObjectList& lst, Fvector amount, bool center_pivot)
{
    for (ObjectIt it = lst.begin(); it != lst.end(); ++it) {
        if (center_pivot) {
            // keep the object's geometric center fixed while it grows/shrinks
            Fvector pivot;  ObjPivot(*it, true, pivot);
            Fvector rel;    rel.sub((*it)->PPosition, pivot);
            rel.x += rel.x * amount.x;
            rel.y += rel.y * amount.y;
            rel.z += rel.z * amount.z;
            Fvector np;     np.add(pivot, rel);
            (*it)->PPosition = np;
        }
        (*it)->Scale(amount);
    }
}

bool CSceneGizmo::BeginDrag(const Fvector& start, const Fvector& dir)
{
    if (m_hover == geNone || !m_visible) return false;
    m_active = m_hover;

    m_move_reminder.set(0.f, 0.f, 0.f);
    m_rot_reminder = 0.f;

    Fvector ax, ay, az; GetAxes(ax, ay, az);
    m_drag_origin.set(m_xform.c);

    if (IsMoveElem(m_active)) {
        m_drag_axis.set(m_active == geMoveX ? ax : m_active == geMoveY ? ay : az);
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, m_drag_t0))
            m_drag_t0 = 0.f;
        return true;
    }
    if (IsRotElem(m_active)) {
        m_drag_axis.set(m_active == geRotX ? ax : m_active == geRotY ? ay : az);
        if (!RingAngle(start, dir, m_drag_origin, m_drag_axis, m_rot_last))
            m_rot_last = 0.f;
        return true;
    }
    if (IsScaleElem(m_active)) {
        m_drag_axis.set(m_active == geScaleX ? ax : m_active == geScaleY ? ay : az);
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, m_drag_t0))
            m_drag_t0 = 0.f;
        return true;
    }
    if (m_active == geUniform) {
        // uniform scale driven by horizontal cursor motion (screen right axis)
        m_drag_axis.set(EDevice.vCameraRight); m_drag_axis.normalize_safe();
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, m_drag_t0))
            m_drag_t0 = 0.f;
        return true;
    }
    if (m_active == gePlaneZX) {
        // move on the world-horizontal XZ plane (normal = world Y)
        m_plane_u.set(1.f, 0.f, 0.f);
        m_plane_v.set(0.f, 0.f, 1.f);
        Fvector wy = {0.f, 1.f, 0.f};
        Fplane pl; pl.build(m_drag_origin, wy);
        float d;
        if (pl.intersectRayDist(start, dir, d)) m_drag_point.mad(start, dir, d);
        else m_drag_point.set(m_drag_origin);
        return true;
    }
    m_active = geNone;
    return false;
}

void CSceneGizmo::Drag(const Fvector& start, const Fvector& dir)
{
    if (m_active == geNone) return;

    ObjectList lst;
    if (!Scene->GetQueryObjects(lst, LTools->CurrentClassID(), 1, -1, -1)) return;

    if (IsMoveElem(m_active)) {
        float t;
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, t)) return;
        float dt = t - m_drag_t0;
        if (_abs(dt) < EPS_S) return;
        m_drag_t0 = t;
        Fvector amount; amount.mul(m_drag_axis, dt);
        if (Tools && Tools->GetSettings(etfMSnap)) {
            CHECK_SNAP(m_move_reminder.x, amount.x, Tools->m_MoveSnap);
            CHECK_SNAP(m_move_reminder.y, amount.y, Tools->m_MoveSnap);
            CHECK_SNAP(m_move_reminder.z, amount.z, Tools->m_MoveSnap);
            if (amount.square_magnitude() < EPS_S) return;
        }
        for (ObjectIt it = lst.begin(); it != lst.end(); ++it)
            (*it)->Move(amount);
        return;
    }

    if (IsRotElem(m_active)) {
        float ang;
        if (!RingAngle(start, dir, m_drag_origin, m_drag_axis, ang)) return;
        float da = ang - m_rot_last;
        // wrap to [-pi,pi]
        while (da >  PI) da -= PI_MUL_2;
        while (da < -PI) da += PI_MUL_2;
        if (_abs(da) < EPS_S) return;
        m_rot_last = ang;
        if (Tools && Tools->GetSettings(etfASnap)) {
            CHECK_SNAP(m_rot_reminder, da, Tools->m_RotateSnapAngle);
            if (_abs(da) < EPS_S) return;
        }
        // Rotate each object about its pivot via proper matrix composition (mirrors
        // CGroupObject::RotateParent). The plain RotateParent only adds axis*angle to
        // the Euler vector; when the mesh sits far from its origin that approximation
        // explodes and the object swings around the wrong point. Composing the world
        // delta into FTransformRP and re-extracting position+rotation is exact.
        Fmatrix R; R.rotation(m_drag_axis, da);
        for (ObjectIt it = lst.begin(); it != lst.end(); ++it) {
            Fvector pivot;  ObjPivot(*it, m_center_pivot, pivot);
            Fmatrix Gold;   Gold.identity(); Gold.c.set(pivot);   // pivot frame (before)
            Fmatrix Gnew;   Gnew.set(R);     Gnew.c.set(pivot);   // pivot frame rotated by R
            Fmatrix Ginv;   Ginv.invert(Gold);
            Fmatrix O, On;
            O.mul (Ginv, (*it)->FTransformRP);                    // object in pivot space
            On.mul(Gnew, O);                                      // re-apply rotated pivot frame
            Fvector xyz;    On.getXYZ(xyz);
            (*it)->NumSetRotation(xyz);
            (*it)->NumSetPosition(On.c);
        }
        return;
    }

    if (IsScaleElem(m_active)) {
        float t;
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, t)) return;
        float dt = t - m_drag_t0;
        if (_abs(dt) < EPS_S) return;
        m_drag_t0 = t;
        // scale delta along the corresponding local axis component
        const float k = 1.0f;            // sensitivity (world units → scale units)
        Fvector amount = {0.f, 0.f, 0.f};
        if      (m_active == geScaleX) amount.x = dt * k;
        else if (m_active == geScaleY) amount.y = dt * k;
        else                           amount.z = dt * k;
        ApplyScaleTo(lst, amount, m_center_pivot);
        return;
    }

    if (m_active == geUniform) {
        float t;
        if (!ProjectRayOnAxis(start, dir, m_drag_origin, m_drag_axis, t)) return;
        float dt = t - m_drag_t0;
        if (_abs(dt) < EPS_S) return;
        m_drag_t0 = t;
        Fvector amount; amount.set(dt, dt, dt);   // uniform on all axes
        ApplyScaleTo(lst, amount, m_center_pivot);
        return;
    }

    if (IsPlaneElem(m_active)) {
        Fvector n; n.crossproduct(m_plane_u, m_plane_v); n.normalize_safe();
        Fplane pl; pl.build(m_drag_origin, n);
        float d;
        if (!pl.intersectRayDist(start, dir, d)) return;
        Fvector P; P.mad(start, dir, d);
        Fvector delta; delta.sub(P, m_drag_point);
        if (delta.square_magnitude() < EPS_S) return;
        m_drag_point.set(P);
        if (Tools && Tools->GetSettings(etfMSnap)) {
            CHECK_SNAP(m_move_reminder.x, delta.x, Tools->m_MoveSnap);
            CHECK_SNAP(m_move_reminder.y, delta.y, Tools->m_MoveSnap);
            CHECK_SNAP(m_move_reminder.z, delta.z, Tools->m_MoveSnap);
            if (delta.square_magnitude() < EPS_S) return;
        }
        for (ObjectIt it = lst.begin(); it != lst.end(); ++it)
            (*it)->Move(delta);
        return;
    }
}

void CSceneGizmo::EndDrag()
{
    if (m_active == geNone) return;
    m_active = geNone;
    if (Scene) Scene->UndoSave();
}
