#include "stdafx.h"
#pragma hdrstop

#include "EditorModelRender11.h"
#include "HW11.h"
#include "EditorTextures11.h"

#include "../xrRender/FBasicVisual.h"
#include "../xrRender/FHierrarhyVisual.h"
#include "../xrRender/FVisual.h"
#include "../xrRender/SkeletonX.h"
#include "../xrRender/SkeletonXVertRender.h"

void RenderModelED11(dxRender_Visual* V, const Fmatrix& world)
{
    if (!V || !HW11.pDevice) return;

    if (FHierrarhyVisual* H = dynamic_cast<FHierrarhyVisual*>(V)) {
        for (u32 i = 0; i < H->children.size(); ++i)
            RenderModelED11(H->children[i], world);
        return;
    }

    CSkeletonX* sk = dynamic_cast<CSkeletonX*>(V);
    Fvisual*    fv = dynamic_cast<Fvisual*>(V);
    if (sk && fv && fv->vCount && !fv->e_indices.empty()) {
        static xr_vector<vertRender> dst;
        dst.resize(fv->vCount);
        if (sk->Skin_Editor(dst.data(), fv->vCount)) {
            ID3D11ShaderResourceView* srv = fv->e_texture.size()
                ? EditorTextures11.Get(HW11.pDevice, fv->e_texture.c_str())
                : nullptr;
            HW11.DrawIndexedSolid(dst.data(), fv->vCount,
                                  fv->e_indices.data(), (u32)fv->e_indices.size(),
                                  (const float*)&world, srv, 0.f, 0.f, 0.f, 0.f);
        }
    }
}
