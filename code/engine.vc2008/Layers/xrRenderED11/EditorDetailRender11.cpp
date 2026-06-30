#include "stdafx.h"
#pragma hdrstop

#include <algorithm>
#include <utility>

#include "EditorDetailRender11.h"
#include "HW11.h"
#include "EditorTextures11.h"

#include "../xrRender/DetailManager.h"
#include "../xrRender/DetailModel.h"
#include "../../editors/ECore/Editor/EDetailModel.h"   // EDetail::GetTextureName (editor)
#include "../../editors/ECore/Editor/device.h"         // EDevice (camera for distance cull)
#include "../../xrCDB/frustum.h"                        // CFrustum (view culling)

// Grass draw distance (meters). Beyond this instances are skipped — cuts overdraw at grazing
// angles / low camera, where far grass stacks into heavy overdraw. Tunable live via the
// Detail Objects inspector ("Draw distance"). Exported so LevelEditor can bind it.
ECORE_API float g_detail_draw_dist = 40.f;

// DX11 editor detail (grass) render — HARDWARE INSTANCING (few draw calls; DX11 hates many small
// draws). Per slot: frustum/distance cull once, then gather per-instance world matrices near→far
// and issue ONE DrawIndexedInstanced per model. Model geometry is cached in HW11 by the CDetail*.
void RenderDetailED11(CDetailManager* dm, CFrustum* frustum)
{
    if (!dm) return;

    static xr_vector<Fmatrix> inst;   // all instances, all models (reused across frames)
    struct ModelRange { CDetail* obj; u32 start; u32 count; ID3D11ShaderResourceView* srv; };
    static xr_vector<ModelRange> ranges;
    inst.clear();
    ranges.clear();

    const Fvector cam     = EDevice.m_Camera.GetPosition();
    const float   distSqr = g_detail_draw_dist * g_detail_draw_dist;

    static xr_vector<std::pair<float, CDetailManager::SlotItemVec*>> slots; // {slot dist², items}

    for (u32 O = 0; O < dm->objects.size(); ++O)
    {
        CDetail* Obj = dm->objects[O];
        if (!Obj || !Obj->number_vertices || !Obj->number_indices) continue;
        if (O >= dm->m_visibles[0].size()) continue;

        slots.clear();
        float slotR = Obj->bv_sphere.R; if (slotR < 1.f) slotR = 1.f; slotR += 3.f;

        xr_vector<CDetailManager::SlotItemVec*>& vis = dm->m_visibles[0][O];
        for (u32 s = 0; s < vis.size(); ++s)
        {
            CDetailManager::SlotItemVec* sv = vis[s];
            if (!sv || sv->empty()) continue;
            CDetailManager::SlotItem* a = (*sv)[0];
            if (!a) continue;

            Fvector spos; spos.set(a->mRotY._41, a->mRotY._42, a->mRotY._43);
            const float dsq = cam.distance_to_sqr(spos);
            if (dsq > distSqr) continue;
            if (frustum && !frustum->testSphere_dirty(spos, slotR)) continue;
            slots.push_back(std::make_pair(dsq, sv));
        }
        if (slots.empty()) continue;

        // near → far: nearer slots write depth first, so the GPU's early-Z can reject some of the
        // occluded blades before shading (cheap; the slot count is small after culling).
        std::sort(slots.begin(), slots.end(),
                  [](const std::pair<float,CDetailManager::SlotItemVec*>& a,
                     const std::pair<float,CDetailManager::SlotItemVec*>& b)
                  { return a.first < b.first; });

        const u32 start = (u32)inst.size();
        for (u32 si = 0; si < slots.size(); ++si)
        {
            CDetailManager::SlotItemVec* sv = slots[si].second;
            for (u32 k = 0; k < sv->size(); ++k)
            {
                CDetailManager::SlotItem* sit = (*sv)[k];
                if (!sit) continue;
                const float    scale = sit->scale_calculated;
                const Fmatrix& M     = sit->mRotY;
                Fmatrix mX;
                mX._11=M._11*scale; mX._12=M._12*scale; mX._13=M._13*scale; mX._14=M._14;
                mX._21=M._21*scale; mX._22=M._22*scale; mX._23=M._23*scale; mX._24=M._24;
                mX._31=M._31*scale; mX._32=M._32*scale; mX._33=M._33*scale; mX._34=M._34;
                mX._41=M._41;       mX._42=M._42;       mX._43=M._43;       mX._44=1.f;
                inst.push_back(mX);
            }
        }
        const u32 count = (u32)inst.size() - start;

        EDetail* ed = dynamic_cast<EDetail*>(Obj);
        LPCSTR tex  = ed ? ed->GetTextureName() : nullptr;
        ID3D11ShaderResourceView* srv = (tex && tex[0])
            ? EditorTextures11.Get(HW11.pDevice, tex) : nullptr;
        ranges.push_back({ Obj, start, count, srv });
    }

    // CRITICAL: empty the visible-slot cache for the next frame. MT_CALC (run from MT_SYNC) APPENDS
    // to m_visibles every frame; the DX9 soft_Render path drains it via _vis.clear(), but this DX11
    // path must do it explicitly. Without this, m_visibles grows without bound — the per-frame
    // draw count keeps climbing and FPS decays from ~30 to ~10 over a few seconds. (All instance
    // data we need is already copied into 'inst'/'ranges', so clearing here is safe.)
    for (u32 lod = 0; lod < 3; ++lod)
        for (u32 O = 0; O < dm->m_visibles[lod].size(); ++O)
            dm->m_visibles[lod][O].clear();

    if (inst.empty()) return;

    HW11.UploadGrassInstances((const float*)inst.data(), (u32)inst.size());
    for (u32 r = 0; r < ranges.size(); ++r)
    {
        const ModelRange& mr = ranges[r];
        HW11.DrawGrassModel(mr.obj, mr.obj->vertices, mr.obj->number_vertices,
                            mr.obj->indices, mr.obj->number_indices,
                            mr.start, mr.count, mr.srv);
    }
}
