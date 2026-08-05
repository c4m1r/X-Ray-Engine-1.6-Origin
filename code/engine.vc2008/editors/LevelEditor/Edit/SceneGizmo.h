#ifndef SceneGizmoH
#define SceneGizmoH


enum EGizmoMode  { gmMove = 0, gmRotate, gmScale, gmUniversal };
enum EGizmoSpace { gsWorld = 0, gsLocal };

enum EGizmoElem  {
    geNone = 0,
    geMoveX, geMoveY, geMoveZ,
    geRotX,  geRotY,  geRotZ,
    geScaleX, geScaleY, geScaleZ,
    gePlaneXY, gePlaneYZ, gePlaneZX,
    geScreen,
    geUniform
};

class CSceneGizmo
{
public:
    EGizmoMode   m_mode    = gmUniversal;
    EGizmoSpace  m_space   = gsWorld;
    bool         m_center_pivot = false;

    EGizmoElem   m_hover   = geNone;
    EGizmoElem   m_active  = geNone;

    bool         m_visible = false;
    Fmatrix      m_xform;
    float        m_screen_scale = 1.f;

    CSceneGizmo() { m_xform.identity(); }

    bool  Update();

    void  Render();

    EGizmoElem HitTest(const Fvector& start, const Fvector& dir);

    bool  BeginDrag(const Fvector& start, const Fvector& dir);
    void  Drag     (const Fvector& start, const Fvector& dir);
    void  EndDrag  ();
    bool  IsDragging() const { return m_active != geNone; }

private:
    void  GetAxes(Fvector& ax, Fvector& ay, Fvector& az) const;
    float AxisLen() const { return m_screen_scale; }

    Fvector m_drag_origin;
    Fvector m_drag_axis;
    float   m_drag_t0   = 0.f;
    float   m_rot_last  = 0.f;
    Fvector m_drag_point;
    Fvector m_plane_u, m_plane_v;
    Fvector m_move_reminder;
    float   m_rot_reminder = 0.f;

    bool  IsMoveElem (EGizmoElem e) const { return e>=geMoveX  && e<=geMoveZ; }
    bool  IsRotElem  (EGizmoElem e) const { return e>=geRotX   && e<=geRotZ;  }
    bool  IsScaleElem(EGizmoElem e) const { return e>=geScaleX && e<=geScaleZ;}
    bool  IsPlaneElem(EGizmoElem e) const { return e>=gePlaneXY && e<=gePlaneZX;}

    void  AllowedGroups(bool& mv, bool& rot, bool& scl) const;
};

extern CSceneGizmo Gizmo;

#endif
