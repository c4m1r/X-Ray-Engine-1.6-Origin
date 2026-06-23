#include "stdafx.h"
#pragma hdrstop

#include "EditorParticleRender11.h"
#include "HW11.h"
#include "EditorTextures11.h"

#include "../xrRender/fvf.h"
#include "../xrRender/ParticleEffect.h"
#include "../xrRender/ParticleGroup.h"
#include "../../xrParticles/psystem.h"   // PAPI::ParticleManager()

#include "../../editors/ECore/Editor/device.h"   // EDevice (camera vectors) — editor module, not xrRender

using namespace PS;

// ---- CPU billboard quad builders (FVF::LIT, world-space) ------------------------------------
// Reimplemented here so all DX11 editor render code stays in xrRenderED11 (xrRender is untouched).
// Vertex order is Z-style (DL,UL,DR,UR) — matches HW11.DrawParticles' quad index pattern.

static void FillSprite(FVF::LIT*& pv, const Fvector& T, const Fvector& R, const Fvector& pos,
                       const Fvector2& lt, const Fvector2& rb, float r1, float r2, u32 clr, float angle)
{
    float sa = _sin(angle), ca = _cos(angle);
    Fvector Vr, Vt;
    Vr.x = T.x*r1*sa + R.x*r1*ca;  Vr.y = T.y*r1*sa + R.y*r1*ca;  Vr.z = T.z*r1*sa + R.z*r1*ca;
    Vt.x = T.x*r2*ca - R.x*r2*sa;  Vt.y = T.y*r2*ca - R.y*r2*sa;  Vt.z = T.z*r2*ca - R.z*r2*sa;
    Fvector a, b, c, d;
    a.sub(Vt, Vr);  b.add(Vt, Vr);  c.invert(a);  d.invert(b);
    pv->set(d.x+pos.x, d.y+pos.y, d.z+pos.z, clr, lt.x, rb.y);  pv++;
    pv->set(a.x+pos.x, a.y+pos.y, a.z+pos.z, clr, lt.x, lt.y);  pv++;
    pv->set(c.x+pos.x, c.y+pos.y, c.z+pos.z, clr, rb.x, rb.y);  pv++;
    pv->set(b.x+pos.x, b.y+pos.y, b.z+pos.z, clr, rb.x, lt.y);  pv++;
}

// path-aligned variant: builds the "right" vector from the particle direction and the camera
static void FillSpriteDir(FVF::LIT*& pv, const Fvector& pos, const Fvector& dir,
                          const Fvector2& lt, const Fvector2& rb, float r1, float r2, u32 clr, float angle)
{
    Fvector R;  R.crossproduct(dir, EDevice.vCameraDirection).normalize_safe();
    FillSprite(pv, dir, R, pos, lt, rb, r1, r2, clr, angle);
}

// Build all billboard quads for one effect into dst (mirrors the editor CParticleEffect::Render loop).
static void BuildEffectQuads(CParticleEffect* pe, xr_vector<FVF::LIT>& dst)
{
    CPEDef* def = pe->GetDefinition();
    if (!def || !def->m_Flags.is(CPEDef::dfSprite)) return;

    PAPI::Particle* particles;
    u32             p_cnt;
    PAPI::ParticleManager()->GetParticles(pe->GetHandleEffect(), particles, p_cnt);
    if (0 == p_cnt) return;

    const bool bXFORM = pe->m_RT_Flags.is(CParticleEffect::flRT_XFORM);
    const Fmatrix& X  = pe->m_XFORM;

    size_t base = dst.size();
    dst.resize(base + p_cnt*4);
    FVF::LIT* pv = dst.data() + base;

    for (u32 i = 0; i < p_cnt; i++) {
        PAPI::Particle& m = particles[i];

        Fvector2 lt, rb;  lt.set(0.f, 0.f);  rb.set(1.f, 1.f);
        if (def->m_Flags.is(CPEDef::dfFramed)) def->m_Frame.CalculateTC(iFloor(float(m.frame)/255.f), lt, rb);

        float r_x = m.size.x*0.5f;
        float r_y = m.size.y*0.5f;
        if (def->m_Flags.is(CPEDef::dfVelocityScale)) {
            float speed = m.vel.magnitude();
            r_x += speed*def->m_VelocityScale.x;
            r_y += speed*def->m_VelocityScale.y;
        }

        if (def->m_Flags.is(CPEDef::dfAlignToPath)) {
            float speed = m.vel.magnitude();
            if ((speed < EPS_S) && def->m_Flags.is(CPEDef::dfWorldAlign)) {
                Fmatrix M;  M.setXYZ(def->m_APDefaultRotation);
                if (bXFORM) { Fvector p; X.transform_tiny(p, m.pos); M.mulA_43(X); FillSprite(pv, M.k, M.i, p, lt, rb, r_x, r_y, m.color, m.rot.x); }
                else        { FillSprite(pv, M.k, M.i, m.pos, lt, rb, r_x, r_y, m.color, m.rot.x); }
            } else if ((speed >= EPS_S) && def->m_Flags.is(CPEDef::dfFaceAlign)) {
                Fmatrix M;  M.identity();
                M.k.div(m.vel, speed);
                M.j.set(0,1,0);  if (_abs(M.j.dotproduct(M.k)) > .99f) M.j.set(0,0,1);
                M.i.crossproduct(M.j, M.k);  M.i.normalize();
                M.j.crossproduct(M.k, M.i);  M.j.normalize();
                if (bXFORM) { Fvector p; X.transform_tiny(p, m.pos); M.mulA_43(X); FillSprite(pv, M.j, M.i, p, lt, rb, r_x, r_y, m.color, m.rot.x); }
                else        { FillSprite(pv, M.j, M.i, m.pos, lt, rb, r_x, r_y, m.color, m.rot.x); }
            } else {
                Fvector dir;
                if (speed >= EPS_S) dir.div(m.vel, speed);
                else                dir.setHP(-def->m_APDefaultRotation.y, -def->m_APDefaultRotation.x);
                if (bXFORM) { Fvector p, dd; X.transform_tiny(p, m.pos); X.transform_dir(dd, dir); FillSpriteDir(pv, p, dd, lt, rb, r_x, r_y, m.color, m.rot.x); }
                else        { FillSpriteDir(pv, m.pos, dir, lt, rb, r_x, r_y, m.color, m.rot.x); }
            }
        } else {
            if (bXFORM) { Fvector p; X.transform_tiny(p, m.pos); FillSprite(pv, EDevice.vCameraTop, EDevice.vCameraRight, p, lt, rb, r_x, r_y, m.color, m.rot.x); }
            else        { FillSprite(pv, EDevice.vCameraTop, EDevice.vCameraRight, m.pos, lt, rb, r_x, r_y, m.color, m.rot.x); }
        }
    }
}

// ---- entry point --------------------------------------------------------------------------------
void RenderParticleED11(dxRender_Visual* V)
{
    if (!V) return;

    // Group: recurse into child effects (each has its own texture)
    if (CParticleGroup* grp = dynamic_cast<CParticleGroup*>(V)) {
        for (CParticleGroup::SItemVecIt it = grp->items.begin(); it != grp->items.end(); ++it)
            RenderParticleED11(it->_effect);
        return;
    }

    // Single effect
    if (CParticleEffect* pe = dynamic_cast<CParticleEffect*>(V)) {
        static xr_vector<FVF::LIT> verts;   // reused across calls to avoid reallocs
        verts.clear();
        BuildEffectQuads(pe, verts);
        if (verts.empty()) return;

        CPEDef* def = pe->GetDefinition();
        ID3D11ShaderResourceView* srv = (def && def->m_TextureName.size())
            ? EditorTextures11.Get(HW11.pDevice, def->m_TextureName.c_str())
            : nullptr;

        // Blend mode lives in the particle .s shader (CBlender_Particle::oBlend, private).
        // Reading it would require an accessor in xrRender, so a forgiving default is used here:
        // 5 = ALPHA-ADD (src_alpha,one). Most editor FX preview acceptably with it.
        const int blend = 5;
        HW11.DrawParticles(verts.data(), (u32)verts.size(), srv, blend);
    }
}
