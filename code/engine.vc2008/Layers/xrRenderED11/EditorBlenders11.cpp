#include "stdafx.h"
#pragma hdrstop

#include "EditorBlenders11.h"

#include "../xrRender/blenders/Blender.h"
#include "../xrRender/blenders/Blender_CLSID.h"
#include "../xrRenderPC_R1/blender_default_aref.h"
#include "../xrRenderPC_R1/blender_vertex_aref.h"
#include "../xrRenderPC_R1/blender_model.h"
#include "../xrRender/Blender_Model_EbB.h"
#include "../xrRender/Blender_tree.h"
#include "../xrRender/Blender_detail_still.h"
#include "../xrRender/Blender_Screen_SET.h"
#include "../xrRender/Blender_Particle.h"

CEditorBlenders11 EditorBlenders11;

void CEditorBlenders11::OnDeviceCreate()
{
    OnDeviceDestroy();

    string_path sh;
    FS.update_path(sh, "$game_data$", "shaders.xr");
    if (!FS.exist(sh))
        return;

    IReader* F = FS.r_open(0, sh);
    if (!F)
        return;

    IReader* fs = F->open_chunk(2);
    if (fs) {
        IReader* chunk    = NULL;
        int      chunk_id = 0;
        while ((chunk = fs->open_chunk(chunk_id)) != NULL) {
            CBlender_DESC desc;
            chunk->r(&desc, sizeof(desc));
            IBlender* B = IBlender::Create(desc.CLS);
            if (B) {
                chunk->seek(0);
                B->Load(*chunk, desc.version);
                m_info[shared_str(desc.cName)] = Classify(B);
                IBlender::Destroy(B);
            }
            chunk->close();
            chunk_id += 1;
        }
        fs->close();
    }
    FS.r_close(F);
}

void CEditorBlenders11::OnDeviceDestroy()
{
    m_info.clear();
}

ED11BlendInfo CEditorBlenders11::Classify(IBlender* B)
{
    ED11BlendInfo bi;
    switch (B->getDescription().CLS)
    {
    case B_DEFAULT_AREF:
        bi.blend = !!((CBlender_default_aref*)B)->oBlend.value;
        bi.atest = true;
        bi.aref  = u32(((CBlender_default_aref*)B)->oAREF.value);
        bi.mode  = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        break;
    case B_VERT_AREF:
        bi.blend = !!((CBlender_Vertex_aref*)B)->oBlend.value;
        bi.atest = true;
        bi.aref  = u32(((CBlender_Vertex_aref*)B)->oAREF.value);
        bi.mode  = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        break;
    case B_MODEL:
        bi.blend  = !!((CBlender_Model*)B)->oBlend.value;
        bi.atest  = bi.blend;
        bi.aref   = bi.blend ? u32(((CBlender_Model*)B)->oAREF.value) : 0;
        bi.mode   = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        bi.zwrite = !(bi.blend && bi.aref < 200);
        break;
    case B_MODEL_EbB:
        bi.blend  = !!((CBlender_Model_EbB*)B)->oBlend.value;
        bi.mode   = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        bi.zwrite = !bi.blend;
        break;
    case B_TREE:
        bi.blend = !!((CBlender_Tree*)B)->oBlend.value;
        bi.atest = true;
        bi.aref  = 200;
        bi.mode  = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        break;
    case B_DETAIL:
        bi.blend = !!((CBlender_Detail_Still*)B)->oBlend.value;
        bi.atest = true;
        bi.aref  = 200;
        bi.mode  = bi.blend ? ED11_BLEND_ALPHA : ED11_BLEND_NONE;
        break;
    case B_SCREEN_SET:
        {
            CBlender_Screen_SET* S = (CBlender_Screen_SET*)B;
            u32 mode = S->getBlendMode();
            u32 aref = u32(S->getAREF());
            switch (mode)
            {
            case 1: bi.blend = true;  bi.atest = true;  bi.aref = aref; bi.mode = ED11_BLEND_ALPHA;     break;
            case 2: bi.blend = true;                                    bi.mode = ED11_BLEND_ADD;       break;
            case 3: bi.blend = true;                                    bi.mode = ED11_BLEND_MUL;       break;
            case 4:
            case 6: bi.blend = true;                                    bi.mode = ED11_BLEND_MUL2X;     break;
            case 5: bi.blend = true;  bi.atest = true;  bi.aref = aref; bi.mode = ED11_BLEND_ALPHA_ADD; break;
            case 7:                   bi.atest = true;  bi.aref = 0;                                    break;
            case 8:
            case 9: bi.blend = true;  bi.atest = true;  bi.aref = aref; bi.mode = ED11_BLEND_ALPHA;     break;
            }
            bi.zwrite = !!S->getZWrite();
        }
        break;
    case B_PARTICLE:
        {
            u32 mode = ((CBlender_Particle*)B)->getBlendMode();
            if (mode != 0)
            {
                bi.blend  = true;
                bi.atest  = true;
                bi.aref   = 0;
                bi.zwrite = false;
                switch (mode)
                {
                case 1: bi.mode = ED11_BLEND_ALPHA;     break;
                case 2: bi.mode = ED11_BLEND_ADD;       break;
                case 3: bi.mode = ED11_BLEND_MUL;       break;
                case 4: bi.mode = ED11_BLEND_MUL2X;     break;
                case 5: bi.mode = ED11_BLEND_ALPHA_ADD; break;
                }
            }
        }
        break;
    case B_EDITOR_SEL:
        bi.blend  = true;
        bi.mode   = ED11_BLEND_ALPHA;
        bi.zwrite = false;
        break;
    }

    bi.priority = B->getPriority();
    bi.strict   = !!B->getStrictSorting();
    if (bi.strict && (1 != (bi.priority / 2)))
        bi.strict = false;

    if (bi.blend && bi.strict)
        bi.zwrite = false;

    return bi;
}

int CEditorBlenders11::SurfPriority(LPCSTR shader_name)
{
    const ED11BlendInfo* bi = Find(shader_name);
    return bi ? bi->priority : 1;
}

bool CEditorBlenders11::SurfStrictB2F(LPCSTR shader_name)
{
    const ED11BlendInfo* bi = Find(shader_name);
    return bi ? bi->strict : false;
}

const ED11BlendInfo* CEditorBlenders11::Find(LPCSTR shader_name)
{
    if (!shader_name || !shader_name[0])
        return nullptr;
    auto it = m_info.find(shared_str(shader_name));
    return (it != m_info.end()) ? &it->second : nullptr;
}

bool CEditorBlenders11::IsTransparent(LPCSTR shader_name)
{
    const ED11BlendInfo* bi = Find(shader_name);
    return bi ? bi->blend : false;
}
