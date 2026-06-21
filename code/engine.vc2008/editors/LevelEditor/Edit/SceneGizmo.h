//----------------------------------------------------------------------------
// SceneGizmo.h — interactive 3D transform gizmo for the Level Editor.
//
// Blender-style single gizmo: move arrows + rotate rings + scale handles drawn
// at the selection pivot. The active axis is chosen by clicking the gizmo in the
// viewport (not by toolbar buttons). Works in both DX9 and DX11 (renders via
// DU_impl primitives). Orientation is World or Local, switchable from the TopBar.
//----------------------------------------------------------------------------
#ifndef SceneGizmoH
#define SceneGizmoH

// Fmatrix / Fvector / u32 come from the precompiled header (xrCore math),
// like the other editor headers in this folder.

enum EGizmoMode  { gmMove = 0, gmRotate, gmScale, gmUniversal };
enum EGizmoSpace { gsWorld = 0, gsLocal };

// Which sub-part of the gizmo is hovered/grabbed.
enum EGizmoElem  {
    geNone = 0,
    geMoveX, geMoveY, geMoveZ,        // move arrows
    geRotX,  geRotY,  geRotZ,         // rotate rings (around the axis)
    geScaleX, geScaleY, geScaleZ,     // scale handles (along the axis)
    gePlaneXY, gePlaneYZ, gePlaneZX,  // two-axis move planes
    geScreen,                         // screen-space (free move / view-aligned rotate)
    geUniform                         // uniform scale (center cube)
};

class CSceneGizmo
{
public:
    EGizmoMode   m_mode    = gmUniversal;
    EGizmoSpace  m_space   = gsWorld;
    bool         m_center_pivot = false; // Maya-style "Center Pivot":
                                         //  true  = rotate/scale each object around its own
                                         //          geometric center (world bbox center);
                                         //  false = around its world position (FPosition).

    EGizmoElem   m_hover   = geNone;   // element under cursor (highlight)
    EGizmoElem   m_active  = geNone;   // element grabbed during drag

    bool         m_visible = false;    // true when a manipulable selection exists
    Fmatrix      m_xform;              // pivot position + orientation (world/local basis)
    float        m_screen_scale = 1.f; // world size that keeps the gizmo screen-constant

    CSceneGizmo() { m_xform.identity(); }

    // Recompute pivot/orientation/visibility from the current selection.
    // Returns m_visible.
    bool  Update();

    // Draw the gizmo (called from EScene::Render after objects/tools).
    void  Render();

    // Cursor ray hover test → updates m_hover. start/dir in world space.
    // Returns the hovered element (geNone if the ray misses the gizmo).
    EGizmoElem HitTest(const Fvector& start, const Fvector& dir);

    // Manipulation. BeginDrag grabs the currently hovered element; returns true if
    // a drag started (caller should then capture the mouse). Drag applies movement
    // to the selected objects; EndDrag releases and records undo.
    bool  BeginDrag(const Fvector& start, const Fvector& dir);
    void  Drag     (const Fvector& start, const Fvector& dir);
    void  EndDrag  ();
    bool  IsDragging() const { return m_active != geNone; }

private:
    // axis directions in the current space (columns of m_xform basis)
    void  GetAxes(Fvector& ax, Fvector& ay, Fvector& az) const;
    // world length of one gizmo axis (screen-constant)
    float AxisLen() const { return m_screen_scale; }

    // drag state (axis is frozen in world space for the whole drag)
    Fvector m_drag_origin;   // pivot at drag start
    Fvector m_drag_axis;     // grabbed axis direction
    float   m_drag_t0   = 0.f; // move: last projected parameter along the axis
    float   m_rot_last  = 0.f; // rotate: last angle around the ring (radians)
    Fvector m_drag_point;      // plane move: last hit point on the drag plane
    Fvector m_plane_u, m_plane_v; // plane move: the two in-plane axes
    Fvector m_move_reminder;   // grid-snap accumulator for translation (etfMSnap)
    float   m_rot_reminder = 0.f; // angle-snap accumulator for rotation (etfASnap)

    bool  IsMoveElem (EGizmoElem e) const { return e>=geMoveX  && e<=geMoveZ; }
    bool  IsRotElem  (EGizmoElem e) const { return e>=geRotX   && e<=geRotZ;  }
    bool  IsScaleElem(EGizmoElem e) const { return e>=geScaleX && e<=geScaleZ;}
    bool  IsPlaneElem(EGizmoElem e) const { return e>=gePlaneXY && e<=gePlaneZX;}
};

extern CSceneGizmo Gizmo;

#endif
