//----------------------------------------------------
// file: EParticlesObject.cpp
//----------------------------------------------------
#include "stdafx.h"
#pragma hdrstop

#include "ESceneAIMapTools.h"
#include "../../ECORE/EDITOR/D3dUtils.h"
#include "SceneObject.h"
#include "bottombar.h"
#include "ui_leveltools.h"
#include "../../Layers/xrRenderED11/HW11.h"             // DX11 textured node quads
#include "../../Layers/xrRenderED11/EditorTextures11.h" // ed_ai_arrows_01 atlas SRV

typedef Fvector2 t_node_tc[4];
static const float dtc = 0.25f;
static t_node_tc node_tc[16]=
{
	{{0.f+0*dtc,0.25f+0*dtc},	{0.25f+0*dtc,0.25f+0*dtc},	{0.25f+0*dtc,0.f+0*dtc},	{0.f+0*dtc,0.f+0*dtc}},
	{{0.f+1*dtc,0.25f+0*dtc},	{0.25f+1*dtc,0.25f+0*dtc},	{0.25f+1*dtc,0.f+0*dtc},	{0.f+1*dtc,0.f+0*dtc}},
	{{0.f+2*dtc,0.25f+0*dtc},	{0.25f+2*dtc,0.25f+0*dtc},	{0.25f+2*dtc,0.f+0*dtc},	{0.f+2*dtc,0.f+0*dtc}},
	{{0.f+3*dtc,0.25f+0*dtc},	{0.25f+3*dtc,0.25f+0*dtc},	{0.25f+3*dtc,0.f+0*dtc},	{0.f+3*dtc,0.f+0*dtc}},

	{{0.f+0*dtc,0.25f+1*dtc},	{0.25f+0*dtc,0.25f+1*dtc},	{0.25f+0*dtc,0.f+1*dtc},	{0.f+0*dtc,0.f+1*dtc}},
	{{0.f+1*dtc,0.25f+1*dtc},	{0.25f+1*dtc,0.25f+1*dtc},	{0.25f+1*dtc,0.f+1*dtc},	{0.f+1*dtc,0.f+1*dtc}},
	{{0.f+2*dtc,0.25f+1*dtc},	{0.25f+2*dtc,0.25f+1*dtc},	{0.25f+2*dtc,0.f+1*dtc},	{0.f+2*dtc,0.f+1*dtc}},
	{{0.f+3*dtc,0.25f+1*dtc},	{0.25f+3*dtc,0.25f+1*dtc},	{0.25f+3*dtc,0.f+1*dtc},	{0.f+3*dtc,0.f+1*dtc}},

	{{0.f+0*dtc,0.25f+2*dtc},	{0.25f+0*dtc,0.25f+2*dtc},	{0.25f+0*dtc,0.f+2*dtc},	{0.f+0*dtc,0.f+2*dtc}},
	{{0.f+1*dtc,0.25f+2*dtc},	{0.25f+1*dtc,0.25f+2*dtc},	{0.25f+1*dtc,0.f+2*dtc},	{0.f+1*dtc,0.f+2*dtc}},
	{{0.f+2*dtc,0.25f+2*dtc},	{0.25f+2*dtc,0.25f+2*dtc},	{0.25f+2*dtc,0.f+2*dtc},	{0.f+2*dtc,0.f+2*dtc}},
	{{0.f+3*dtc,0.25f+2*dtc},	{0.25f+3*dtc,0.25f+2*dtc},	{0.25f+3*dtc,0.f+2*dtc},	{0.f+3*dtc,0.f+2*dtc}},

	{{0.f+0*dtc,0.25f+3*dtc},	{0.25f+0*dtc,0.25f+3*dtc},	{0.25f+0*dtc,0.f+3*dtc},	{0.f+0*dtc,0.f+3*dtc}},
	{{0.f+1*dtc,0.25f+3*dtc},	{0.25f+1*dtc,0.25f+3*dtc},	{0.25f+1*dtc,0.f+3*dtc},	{0.f+1*dtc,0.f+3*dtc}},
	{{0.f+2*dtc,0.25f+3*dtc},	{0.25f+2*dtc,0.25f+3*dtc},	{0.25f+2*dtc,0.f+3*dtc},	{0.f+2*dtc,0.f+3*dtc}},
	{{0.f+3*dtc,0.25f+3*dtc},	{0.25f+3*dtc,0.25f+3*dtc},	{0.25f+3*dtc,0.f+3*dtc},	{0.f+3*dtc,0.f+3*dtc}},
};

void ESceneAIMapTool::OnDeviceCreate()
{
    if (g_bEditorDX11) return;
	m_Shader.create("editor\\ai_node","ed\\ed_ai_arrows_01");
	// AI map quads are submitted via DrawPrimitive, so no index buffer is needed here.
    m_RGeom.create(FVF::F_LIT,RCache.Vertex.Buffer(),0);
}

void ESceneAIMapTool::OnDeviceDestroy()
{
	m_Shader.destroy();
	m_RGeom.destroy();
}

BOOL ai_map_shown = TRUE;

static const u32 block_size = 0x2000;
void ESceneAIMapTool::OnRender(int priority, bool strictB2F)
{
	if (m_Flags.is(flHideNodes) || !ai_map_shown) return;
    if (1==priority){
        if (false==strictB2F){
            RCache.set_xform_world(Fidentity);
			if (OBJCLASS_AIMAP==LTools->CurrentClassID()){
	            u32 clr = 0xffffc000;
	            EDevice.SetShader	(EDevice.m_WireShader);
    	        DU_impl.DrawSelectionBoxB	(m_AIBBox,&clr);
            }
            if (Valid() && g_bEditorDX11) {
                // Textured nodes (matches DX9): the ed_ai_arrows_01 atlas (4×4 cells) shows each
                // node's connectivity; node_tc[k] picks the cell for the neighbour bitmask k. Drawn
                // as alpha-blended FVF::LIT quads modulated by the per-node colour (sel/hl/normal).
                const Fvector DUP = {0,1,0};
                const float st  = (m_Params.fPatchSize*0.9f)*0.5f;
                const float tt  = 0.01f;
                ID3D11ShaderResourceView* node_srv =
                    EditorTextures11.Get(HW11.pDevice, "ed\\ed_ai_arrows_01");
                Irect rect;
                HashRect(EDevice.m_Camera.GetPosition(), m_VisRadius, rect);
                xr_vector<FVF::LIT> verts;          // 4 verts/node (DL,UL,DR,UR for DrawParticles)
                verts.reserve(block_size);
                for (int x=rect.x1; x<=rect.x2; x++) {
                    for (int z=rect.y1; z<=rect.y2; z++) {
                        AINodeVec* nodes = HashMap(x,z);
                        if (!nodes) continue;
                        for (AINodeIt it=nodes->begin(); it!=nodes->end(); it++) {
                            SAINode& N = **it;
                            Fvector v; v.set(N.Pos.x-st, N.Pos.y, N.Pos.z-st);
                            float p_denom = N.Plane.n.dotproduct(DUP);
                            float b = (_abs(p_denom)<EPS_S)?m_Params.fPatchSize:_abs(N.Plane.classify(v)/p_denom);
                            if (!Render->ViewBase.testSphere_dirty(N.Pos, _max(b,st))) continue;
                            u32 clr;
                            if (N.flags.is(SAINode::flSelected))        clr = 0xffffffff;
                            else if (N.flags.is(SAINode::flHLSelected)) clr = 0xff909090;
                            else                                         clr = 0xff606060;
                            // atlas cell from the 4-neighbour bitmask (n1..n4 == n[0..3])
                            int k = 0;
                            if (N.n[0]) k |= 1; if (N.n[1]) k |= 2; if (N.n[2]) k |= 4; if (N.n[3]) k |= 8;
                            // Half-texel inset toward the cell centre so bilinear filtering stays
                            // inside this 16×16 atlas cell — without it the footprint crosses the
                            // cell border and smears in the neighbours (the "soapy" look). 64px atlas.
                            const float ins = 1.0f/64.f;
                            const float ccx = (k&3)*0.25f + 0.125f, ccz = (k>>2)*0.25f + 0.125f;
                            auto iuv = [&](int i){ Fvector2 t = node_tc[k][i];
                                t.x += (t.x<ccx?ins:-ins); t.y += (t.y<ccz?ins:-ins); return t; };
                            Fvector c1,c2,c3,c4;
                            v.set(N.Pos.x-st, N.Pos.y, N.Pos.z-st); N.Plane.intersectRayPoint(v,DUP,c1); c1.mad(c1,N.Plane.n,tt); // minX,minZ
                            v.set(N.Pos.x+st, N.Pos.y, N.Pos.z-st); N.Plane.intersectRayPoint(v,DUP,c2); c2.mad(c2,N.Plane.n,tt); // maxX,minZ
                            v.set(N.Pos.x+st, N.Pos.y, N.Pos.z+st); N.Plane.intersectRayPoint(v,DUP,c3); c3.mad(c3,N.Plane.n,tt); // maxX,maxZ
                            v.set(N.Pos.x-st, N.Pos.y, N.Pos.z+st); N.Plane.intersectRayPoint(v,DUP,c4); c4.mad(c4,N.Plane.n,tt); // minX,maxZ
                            // DrawParticles quad order = DL,UL,DR,UR → c1,c4,c2,c3 with matching UVs
                            FVF::LIT vv;
                            vv.p=c1; vv.color=clr; vv.t.set(iuv(0)); verts.push_back(vv); // DL
                            vv.p=c4; vv.color=clr; vv.t.set(iuv(3)); verts.push_back(vv); // UL
                            vv.p=c2; vv.color=clr; vv.t.set(iuv(1)); verts.push_back(vv); // DR
                            vv.p=c3; vv.color=clr; vv.t.set(iuv(2)); verts.push_back(vv); // UR
                            if (verts.size() >= (size_t)(block_size-4)) {
                                HW11.DrawParticles(verts.data(), (u32)verts.size(), node_srv, 1); // bilinear
                                verts.clear();
                            }
                        }
                    }
                }
                if (!verts.empty())
                    HW11.DrawParticles(verts.data(), (u32)verts.size(), node_srv, 1); // bilinear
            } else if (Valid() && !g_bEditorDX11){
                // render nodes (DX9 only — streaming VB not available in DX11)
                EDevice.SetShader	(m_Shader);
                EDevice.SetRS		(D3DRS_CULLMODE,		D3DCULL_NONE);
                Irect rect;
                HashRect			(EDevice.m_Camera.GetPosition(),m_VisRadius,rect);

                u32 vBase;
                _VertexStream* Stream= &RCache.Vertex;
                FVF::LIT* pv		= (FVF::LIT*)Stream->Lock(block_size,m_RGeom->vb_stride,vBase);
                u32	cnt				= 0;
//				EDevice.Statistic.TEST0.Begin();
//				EDevice.Statistic.TEST2.Begin();
                for (int x=rect.x1; x<=rect.x2; x++){
                    for (int z=rect.y1; z<=rect.y2; z++){
                        AINodeVec* nodes	= HashMap(x,z);
                        if (nodes){
                            const Fvector	DUP={0,1,0};
                            const float st 	= (m_Params.fPatchSize*0.9f)*0.5f;
                            for (AINodeIt it=nodes->begin(); it!=nodes->end(); it++){
                                SAINode& N 	= **it;

								Fvector v;	v.set(N.Pos.x-st,N.Pos.y,N.Pos.z-st);
                                float p_denom 	= N.Plane.n.dotproduct(DUP);
                                float b			= (_abs(p_denom)<EPS_S)?m_Params.fPatchSize:_abs(N.Plane.classify(v) / p_denom);

                                if (Render->ViewBase.testSphere_dirty(N.Pos,_max(b,st))){
                                    u32 clr;
                                    if (N.flags.is(SAINode::flSelected))clr = 0xffffffff;
                                    else 								clr = N.flags.is(SAINode::flHLSelected)?0xff909090:0xff606060;
                                    int k = 0;
                                    if (N.n1) k |= 1<<0;
                                    if (N.n2) k |= 1<<1;
                                    if (N.n3) k |= 1<<2;
                                    if (N.n4) k |= 1<<3;
                                    Fvector		v;
                                    FVF::LIT	v1,v2,v3,v4;
                                    float tt	= 0.01f;
                                    v.set(N.Pos.x-st,N.Pos.y,N.Pos.z-st);	N.Plane.intersectRayPoint(v,DUP,v1.p);	v1.p.mad(v1.p,N.Plane.n,tt); v1.t.set(node_tc[k][0]); v1.color=clr;	// minX,minZ
                                    v.set(N.Pos.x+st,N.Pos.y,N.Pos.z-st);	N.Plane.intersectRayPoint(v,DUP,v2.p);	v2.p.mad(v2.p,N.Plane.n,tt); v2.t.set(node_tc[k][1]); v2.color=clr;	// maxX,minZ
                                    v.set(N.Pos.x+st,N.Pos.y,N.Pos.z+st);	N.Plane.intersectRayPoint(v,DUP,v3.p);	v3.p.mad(v3.p,N.Plane.n,tt); v3.t.set(node_tc[k][2]); v3.color=clr;	// maxX,maxZ
                                    v.set(N.Pos.x-st,N.Pos.y,N.Pos.z+st);	N.Plane.intersectRayPoint(v,DUP,v4.p);	v4.p.mad(v4.p,N.Plane.n,tt); v4.t.set(node_tc[k][3]); v4.color=clr;	// minX,maxZ
                                    pv->set(v3); pv++;
                                    pv->set(v2); pv++;
                                    pv->set(v1); pv++;
                                    pv->set(v1); pv++;
                                    pv->set(v4); pv++;
                                    pv->set(v3); pv++;
                                    cnt+=6;
                                    if (cnt>=block_size-6){
                                        Stream->Unlock	(cnt,m_RGeom->vb_stride);
                                        EDevice.DP		(D3DPT_TRIANGLELIST,m_RGeom,vBase,cnt/3);
                                        pv 				= (FVF::LIT*)Stream->Lock(block_size,m_RGeom->vb_stride,vBase);
                                        cnt				= 0;
                                    }
                                }
                            }
                        }
                    }
                }
//                EDevice.Statistic.TEST2.End();
//                EDevice.Statistic.TEST0.End();
				Stream->Unlock		(cnt,m_RGeom->vb_stride);
                if (cnt) EDevice.DP	(D3DPT_TRIANGLELIST,m_RGeom,vBase,cnt/3);
                EDevice.SetRS		(D3DRS_CULLMODE,		D3DCULL_CCW);
            }
        }else{
/*            // render snap
            if (m_Flags.is(flDrawSnapObjects))
                for(ObjectIt _F=m_SnapObjects.begin();_F!=m_SnapObjects.end();_F++)
                    if((*_F)->Visible()) ((CSceneObject*)(*_F))->RenderSelection(0x4046B646);
*/        }
    }
}

//----------------------------------------------------


 
