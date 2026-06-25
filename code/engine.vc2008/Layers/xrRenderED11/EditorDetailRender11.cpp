#include "stdafx.h"
#pragma hdrstop

#include "EditorDetailRender11.h"
#include "HW11.h"
#include "EditorTextures11.h"

#include "../xrRender/DetailManager.h"
#include "../xrRender/DetailModel.h"
#include "../../editors/ECore/Editor/EDetailModel.h"   // EDetail::GetTextureName (editor)

// One detail vertex (matches IRender_DetailModel::fvfVertexOut == FVF::LIT == il_particle)
typedef IRender_DetailModel::fvfVertexOut DetVert;

// Flush limit: keep vertex offsets within u16 index range.
static const u32 DET_MAX_VERTS = 60000;

void RenderDetailED11(CDetailManager* dm)
{
    if (!dm) return;

    // NOTE: the visible-slot cache (MT_SYNC/MT_CALC) is built by the caller in LevelEditor,
    // because CDetailManager's out-of-line methods (MT_CALC) live in LevelEditor, not xrECoreB.
    // Here we only read the already-built m_visibles and draw.

    static xr_vector<DetVert> verts;
    static xr_vector<u16>     idx;

    for (u32 O = 0; O < dm->objects.size(); ++O)
    {
        CDetail* Obj = dm->objects[O];
        if (!Obj || !Obj->number_vertices || !Obj->number_indices) continue;

        // per-object diffuse texture (editor EDetail)
        EDetail* ed = dynamic_cast<EDetail*>(Obj);
        LPCSTR tex  = ed ? ed->GetTextureName() : nullptr;
        ID3D11ShaderResourceView* srv = (tex && tex[0])
            ? EditorTextures11.Get(HW11.pDevice, tex) : nullptr;

        verts.clear(); idx.clear();
        u32 iOffset = 0;

        // gather instances from all 3 LOD/wave visible lists
        for (int lod = 0; lod < 3; ++lod)
        {
            if (O >= dm->m_visibles[lod].size()) continue;
            xr_vector<CDetailManager::SlotItemVec*>& vis = dm->m_visibles[lod][O];

            for (u32 s = 0; s < vis.size(); ++s)
            {
                CDetailManager::SlotItemVec* sv = vis[s];
                if (!sv) continue;
                for (u32 k = 0; k < sv->size(); ++k)
                {
                    CDetailManager::SlotItem* it = (*sv)[k];
                    if (!it) continue;

                    // flush batch if the next instance would overflow u16 indices
                    if (iOffset + Obj->number_vertices > DET_MAX_VERTS)
                    {
                        if (!verts.empty())
                            HW11.DrawDetails(verts.data(), (u32)verts.size(), idx.data(), (u32)idx.size(), srv);
                        verts.clear(); idx.clear(); iOffset = 0;
                    }

                    // instance transform = rotation·scale (translation untouched), as in soft_Render
                    const float    scale = it->scale_calculated;
                    const Fmatrix& M     = it->mRotY;
                    Fmatrix mX;
                    mX._11=M._11*scale; mX._12=M._12*scale; mX._13=M._13*scale; mX._14=M._14;
                    mX._21=M._21*scale; mX._22=M._22*scale; mX._23=M._23*scale; mX._24=M._24;
                    mX._31=M._31*scale; mX._32=M._32*scale; mX._33=M._33*scale; mX._34=M._34;
                    mX._41=M._41;       mX._42=M._42;       mX._43=M._43;       mX._44=1.f;

                    size_t vb = verts.size(); verts.resize(vb + Obj->number_vertices);
                    size_t ib = idx.size();   idx.resize  (ib + Obj->number_indices);

                    // transform vertices (world-space), white color
                    for (u32 v = 0; v < Obj->number_vertices; ++v)
                    {
                        DetVert& d = verts[vb + v];
                        mX.transform_tiny(d.P, Obj->vertices[v].P);
                        d.C = 0xffffffff;
                        d.u = Obj->vertices[v].u;
                        d.v = Obj->vertices[v].v;
                    }
                    // indices offset into the accumulated buffer
                    for (u32 n = 0; n < Obj->number_indices; ++n)
                        idx[ib + n] = u16(Obj->indices[n] + iOffset);

                    iOffset += Obj->number_vertices;
                }
            }
        }

        if (!verts.empty())
            HW11.DrawDetails(verts.data(), (u32)verts.size(), idx.data(), (u32)idx.size(), srv);
    }
}
