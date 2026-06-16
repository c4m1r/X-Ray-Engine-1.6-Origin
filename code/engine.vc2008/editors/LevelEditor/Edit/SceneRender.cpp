#include "stdafx.h"
#pragma hdrstop

#include "Scene.h"
#include "SceneObject.h"
#include "bottombar.h"
#include "d3dutils.h"
#include "SpawnPoint.h"
#include "SpatialIndex.h"
#include "../../ECore/Editor/device.h"
#include "../../ECore/Editor/ui_main.h"
#include "../../Layers/xrRenderED11/HW11.h"
#include "../../Layers/xrRenderED11/EditorShaders11.h"
#include "../../Layers/xrRenderED11/EditorTextures11.h"
#include "../../ECore/Editor/EditMesh.h"
#include <chrono>
#define PROF_MS(t) (std::chrono::duration_cast<std::chrono::microseconds>(t).count() / 1000.0)

//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
/*#define RENDER_OBJECT(P,B)\
{\
    try{\
        (N->val)->RenderRoot(P,B);\
    }catch(...){\
		ELog.DlgMsg(mtError, "Please notify AlexMX!!! Critical error has occured in render routine!!! [Type B] - Tools: '%s' Object: '%s'",(N->val)->ParentTool->ClassName(),(N->val)->Name);\
    }\
}*/

void RENDER_OBJECT(EScene::mapObject_Node* N, int P, bool B)
{
	//try
	//{
		/*if(strcmp((N->val)->Name, "full_zone\\part_1\\zapravka\\zapr") == 0)
		{
			DEBUG_MESSAGE((N->val)->Name)
        }*/
		(N->val)->RenderRoot(P,B);
	//}
	//catch(...)
	//{
	//	ELog.DlgMsg(mtError, "Please notify AlexMX!!! Critical error has occured in render routine!!! [Type B] - Tools: '%s' Object: '%s'",(N->val)->ParentTool->ClassName(),(N->val)->Name);
    //}
}
    
void __fastcall object_Normal_0(EScene::mapObject_Node *N)	 {RENDER_OBJECT(N, 0,false); }
void __fastcall object_Normal_1(EScene::mapObject_Node *N)	 {RENDER_OBJECT(N, 1,false); }
void __fastcall object_Normal_2(EScene::mapObject_Node *N)	 {RENDER_OBJECT(N, 2,false); }
void __fastcall object_Normal_3(EScene::mapObject_Node *N)	 {RENDER_OBJECT(N, 3,false); }
//------------------------------------------------------------------------------
void __fastcall object_StrictB2F_0(EScene::mapObject_Node *N){RENDER_OBJECT(N, 0,true);}
void __fastcall object_StrictB2F_1(EScene::mapObject_Node *N){RENDER_OBJECT(N, 1,true);}
void __fastcall object_StrictB2F_2(EScene::mapObject_Node *N){RENDER_OBJECT(N, 2,true);}
void __fastcall object_StrictB2F_3(EScene::mapObject_Node *N){RENDER_OBJECT(N, 3,true);}
//------------------------------------------------------------------------------


void EScene::RenderSky(const Fmatrix& camera)
{
	if( !valid() )	return;

//	draw sky
/*
//.
	if (m_SkyDome&&fraBottomBar->miDrawSky->Checked){
        st_Environment& E = m_LevelOp.m_Envs[m_LevelOp.m_CurEnv];
        m_SkyDome->PPosition = camera.c;
        m_SkyDome->UpdateTransform(true);
		EDevice.SetRS(D3DRS_TEXTUREFACTOR, E.m_SkyColor.get());
    	m_SkyDome->RenderSingle();
	    EDevice.SetRS(D3DRS_TEXTUREFACTOR,	0xffffffff);
    }
*/
}
//------------------------------------------------------------------------------

struct tools_rp_pred
{
    IC bool operator()(ESceneToolBase* x, ESceneToolBase* y) const
    {	return x->RenderPriority()<y->RenderPriority();	}
};

#define DEFINE_MSET_PRED(T,N,I,P)	typedef xr_multiset< T, P > N;		typedef N::iterator I;

DEFINE_MSET_PRED(ESceneToolBase*,SceneMToolsSet,SceneMToolsIt,tools_rp_pred);
DEFINE_MSET_PRED(ESceneCustomOTool*,SceneOToolsSet,SceneOToolsIt,tools_rp_pred);

/*#define RENDER_SCENE_TOOLS(P,B)\
	{\
		SceneMToolsIt s_it 	= scene_tools.begin();\
		SceneMToolsIt s_end	= scene_tools.end();\
        for (; s_it!=s_end; s_it++){\
            EDevice.SetShader		(B?EDevice.m_SelectionShader:EDevice.m_WireShader);\
            RCache.set_xform_world	(Fidentity);\
            try{\
            	(*s_it)->OnRenderRoot(P,B);\
            }catch(...){\
		        ELog.DlgMsg(mtError, "Please notify AlexMX!!! Critical error has occured in render routine!!! [Type B] - Tools: '%s'",(*s_it)->ClassName());\
            }\
        }\
	}*/

void RENDER_SCENE_TOOLS(SceneMToolsSet scene_tools, int P, bool B)
{
	SceneMToolsIt s_it 	= scene_tools.begin();
	SceneMToolsIt s_end	= scene_tools.end();
	for (; s_it!=s_end; s_it++)
	{
		EDevice.SetShader		(B?EDevice.m_SelectionShader:EDevice.m_WireShader);
		if (!g_bEditorDX11) RCache.set_xform_world(Fidentity);
		//try
		//{
			(*s_it)->OnRenderRoot(P,B);
		//}
		//catch(...)
		//{
		//	ELog.DlgMsg(mtError, "Please notify AlexMX!!! Critical error has occured in render routine!!! [Type B] - Tools: '%s'",(*s_it)->ClassName());\
		//}
	}
}

// #include "../../include/stack_trace.h"

void EScene::Render( const Fmatrix& camera )
{
	if( !valid() )	return;
    using clk = std::chrono::high_resolution_clock;
    auto T0 = clk::now();

//	if( locked() )	return;

    // extract and sort object tools
    SceneOToolsSet object_tools;
	SceneMToolsSet scene_tools;
	{
		SceneToolsMapPairIt t_it 	= m_SceneTools.begin();
		SceneToolsMapPairIt t_end 	= m_SceneTools.end();
		for (; t_it!=t_end; t_it++) {
			//DEBUG_MESSAGE(t_it->first)
//			if(t_it->first == 13)
//				continue;
			if (t_it->second){
				// before render
				t_it->second->BeforeRender();
				// sort tools
				ESceneCustomOTool* mt = dynamic_cast<ESceneCustomOTool*>(t_it->second);
				if (mt)
				{
					object_tools.insert(mt);
				}
				scene_tools.insert	(t_it->second);
			}
		}
	}

	// Rebuild spatial index if dirty (scene was modified or objects moved)
	if (m_bSpatialIndexDirty)
		RebuildSpatialIndex();

	// Instance batches: CSceneObjects grouped by mesh reference.
	// Static cache — rebuilt only when camera moves, objects change, or scene is edited.
	typedef xr_vector<CSceneObject*> CSOBatch;
	static std::unordered_map<CEditableObject*, CSOBatch> s_inst_batches;

	// Rebuild candidates every frame — no camera-static caching.
	s_inst_batches.clear();
	s_inst_batches.reserve(512);

	const float render_radius = (m_fRenderRadius < EPrefs->view_fp) ? m_fRenderRadius : EPrefs->view_fp;
	static xr_vector<CCustomObject*> s_candidates;
	s_candidates.clear();
	s_candidates.reserve(4096);
	m_pSpatialIndex->Query(EDevice.vCameraPosition, render_radius * render_radius, s_candidates);

	u32 rendered_obj_count = 0;
	for (CCustomObject* obj : s_candidates) {
		if (!obj->Visible() || !obj->IsRender()) continue;
		++rendered_obj_count;

		if (obj->ClassID == OBJCLASS_SCENEOBJECT) {
			CSceneObject* so = static_cast<CSceneObject*>(obj);
			CEditableObject* ref = so->GetReference();
			if (ref) {
				if (g_bEditorDX11) {
					Fbox bb = so->GetBBox();
					if (!RImplementation.occ_visible(bb)) continue;
					s_inst_batches[ref].push_back(so);
				} else {
					if (obj->Selected()) {
						float distSQ = EDevice.vCameraPosition.distance_to_sqr(obj->FPosition);
						mapRenderObjects.insertInAnyWay(distSQ, obj);
					} else {
						s_inst_batches[ref].push_back(so);
					}
				}
				continue;
			}
		}
		if (!g_bEditorDX11) {
			float distSQ = EDevice.vCameraPosition.distance_to_sqr(obj->FPosition);
			mapRenderObjects.insertInAnyWay(distSQ, obj);
		}
	}
	EDevice.Statistic->dwRenderedObjects = rendered_obj_count;

	auto& inst_batches = s_inst_batches;
    auto T1 = clk::now(); // after spatial query + inst_batches build (or cache hit)

	// Helper: draw all surfaces of one mesh with per-surface textures.
	// start_inst = first instance index in the shared instance buffer (StartInstanceLocation).
	// Two-pass: opaque first, then transparent (glass/window) with alpha blending.
	auto DrawMeshSurfaces = [&](CEditableMesh* mesh, u32 inst_count, u32 start_inst) {
        const RBMap* rbs = mesh->GetRenderBuffers();
        if (!rbs) return;

        auto isTransparent = [](const char* name) -> bool {
            if (!name) return false;
            return strstr(name, "glass")       != nullptr
                || strstr(name, "transparent") != nullptr
                || strstr(name, "window")      != nullptr;
        };

        // Pass 1: opaque
        for (auto& kv2 : *rbs) {
            CSurface* surf = kv2.first;
            if (isTransparent(surf->_ShaderName())) continue;
            EditorShaders11.SetTexture(HW11.pContext, EditorTextures11.Get(HW11.pDevice, surf->_Texture()));
            mesh->RenderInstanced11(HW11.pContext, HW11.inst_buf, inst_count, surf, start_inst);
        }

        // Pass 2: transparent — alpha blend, no clip
        bool in_transparent = false;
        for (auto& kv2 : *rbs) {
            CSurface* surf = kv2.first;
            if (!isTransparent(surf->_ShaderName())) continue;
            if (!in_transparent) {
                in_transparent = true;
                const float bf[4] = { 1,1,1,1 };
                HW11.pContext->OMSetBlendState(EditorShaders11.bs_alpha, bf, 0xFFFFFFFF);
                HW11.pContext->PSSetShader(EditorShaders11.ps_inst_transparent, nullptr, 0);
            }
            EditorShaders11.SetTexture(HW11.pContext, EditorTextures11.Get(HW11.pDevice, surf->_Texture()));
            mesh->RenderInstanced11(HW11.pContext, HW11.inst_buf, inst_count, surf, start_inst);
        }
        if (in_transparent) {
            HW11.pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            HW11.pContext->PSSetShader(EditorShaders11.ps_instanced, nullptr, 0);
        }
	};

	// DX11 hardware-instanced render of all batches in one GPU upload.
	// Phase 1: pack all visible instance data into one CPU array.
	// Phase 2: single UploadInstances (one Map/Unmap).
	// Phase 3: DrawInstanced per batch using StartInstanceLocation offset.
	auto RenderInstBatchesDX11 = [&]() {
        struct BatchDraw { CEditableObject* ref; u32 start, count; };
        static xr_vector<EditorInstanceData> s_all_inst;
        static xr_vector<BatchDraw>          s_draws;

        auto Tp0 = clk::now();

        s_all_inst.clear();
        s_draws.clear();
        bool any_blink = false;

        for (auto& kv : inst_batches) {
            CEditableObject* ref = kv.first;
            const CSOBatch& batch = kv.second;
            if (batch.empty()) continue;

            u32 start = (u32)s_all_inst.size();
            for (u32 i = 0; i < (u32)batch.size(); ++i) {
                EditorInstanceData id;
                memcpy(id.world, &batch[i]->_Transform(), sizeof(float)*16);
                int ba = batch[i]->BlinkAlpha();
                if (ba > 0) {
                    id.color[0] = id.color[1] = id.color[2] = 1.f;
                    id.color[3] = ba / 64.f;
                    any_blink = true;
                } else {
                    id.color[0] = id.color[1] = id.color[2] = id.color[3] = 0.f;
                }
                s_all_inst.push_back(id);
            }
            u32 count = (u32)s_all_inst.size() - start;
            if (count) s_draws.push_back({ref, start, count});
        }

        if (any_blink) UI->RedrawScene();

        if (s_all_inst.empty()) return;
        if (!HW11.UploadInstances(s_all_inst.data(), (u32)s_all_inst.size())) return;

        auto Tp1 = clk::now();

        EditorShaders11.BindInstanced(HW11.pContext);
        for (auto& d : s_draws) {
            for (CEditableMesh* mesh : d.ref->Meshes()) {
                DrawMeshSurfaces(mesh, d.count, d.start);
            }
        }

        auto Tp2 = clk::now();

        static int s_idc = 0;
        if (++s_idc >= 60) {
            s_idc = 0;
            ELog.Msg(mtInformation, "instDraw: pack+upload=%.2fms  draw=%.2fms  inst=%u",
                PROF_MS(Tp1-Tp0), PROF_MS(Tp2-Tp1), (u32)s_all_inst.size());
        }
	};

	// DX9 path: groups are in reference-mesh order → consecutive same-ref calls hit RCache.
	auto RenderInstBatches = [&](int priority, bool strictB2F) {
        if (g_bEditorDX11) return; // DX11 uses RenderInstBatchesDX11 below
		for (auto& kv : inst_batches) {
			for (CSceneObject* so : kv.second) {
				so->RenderRoot(priority, strictB2F);
			}
		}
	};

    if (g_bEditorDX11) {
        // DX11: all CSceneObjects (batched unselected + selected with highlight) in one pass.
        RenderInstBatchesDX11();
        auto T2 = clk::now(); // after instanced draw

        // Selection boxes and pivot axes for selected SceneObjects.
        // ESceneCustomOTool::OnRender skips OBJCLASS_SCENEOBJECT in DX11 to avoid
        // 64k redundant virtual calls. Call Render(1,false) only on selected objects —
        // that single priority+strictB2F combination draws selection box and pivot axes.
        for (auto& kv : inst_batches) {
            for (CSceneObject* so : kv.second) {
                if (so->Selected())
                    so->Render(1, false);
            }
        }

        // Tool-specific gizmos: lights, sound sources, waypoints, shapes, glows, AI map, etc.
        // ESceneCustomOTool::OnRender skips OBJCLASS_SCENEOBJECT, so only non-mesh tools run here.
        // ESceneAIMapTool::OnRender has its own DX11 node rendering.
        for (int P = 0; P <= 3; P++) {
            RENDER_SCENE_TOOLS(scene_tools, P, false);
            RENDER_SCENE_TOOLS(scene_tools, P, true);
        }
        auto T3 = clk::now(); // after RENDER_SCENE_TOOLS

        // Log timing every 60 frames
        static int s_prof_frame = 0;
        if (++s_prof_frame >= 60) {
            s_prof_frame = 0;
            ELog.Msg(mtInformation,
                "Render timing: query+batch=%.1fms  instDraw=%.1fms  tools=%.1fms  total=%.1fms  batches=%u  inst=%u",
                PROF_MS(T1-T0), PROF_MS(T2-T1), PROF_MS(T3-T2), PROF_MS(T3-T0),
                (u32)inst_batches.size(), EDevice.Statistic->dwRenderedObjects);
        }
    } else {
// priority #0
        RenderInstBatches				(0, false);
        mapRenderObjects.traverseLR		(object_Normal_0);
        RENDER_SCENE_TOOLS				(scene_tools, 0,false);
        RenderInstBatches				(0, true);
        mapRenderObjects.traverseRL		(object_StrictB2F_0);
        RENDER_SCENE_TOOLS				(scene_tools, 0,true);
// priority #1
        RenderInstBatches				(1, false);
        mapRenderObjects.traverseLR		(object_Normal_1);
        RENDER_SCENE_TOOLS				(scene_tools, 1,false);
        RenderInstBatches				(1, true);
        mapRenderObjects.traverseRL		(object_StrictB2F_1);
        RENDER_SCENE_TOOLS				(scene_tools, 1,true);
// priority #2
        RenderInstBatches				(2, false);
        mapRenderObjects.traverseLR		(object_Normal_2);
        RENDER_SCENE_TOOLS				(scene_tools, 2,false);
        RenderInstBatches				(2, true);
        mapRenderObjects.traverseRL		(object_StrictB2F_2);
        RENDER_SCENE_TOOLS				(scene_tools, 2,true);
// priority #3
        RenderInstBatches				(3, false);
        mapRenderObjects.traverseLR		(object_Normal_3);
        RENDER_SCENE_TOOLS				(scene_tools, 3,false);
        RenderInstBatches				(3, true);
        mapRenderObjects.traverseRL		(object_StrictB2F_3);
        RENDER_SCENE_TOOLS				(scene_tools, 3,true);
    }

	//render snap (DX9 only — uses DX9 RCache directly)
	if (!g_bEditorDX11)
		RenderSnapList();

	//clear
	mapRenderObjects.clear			();


    SceneMToolsIt s_it 	= scene_tools.begin();
	SceneMToolsIt s_end	= scene_tools.end();
	for (; s_it!=s_end; s_it++)
	{
		(*s_it)->AfterRender();
	}
}
//------------------------------------------------------------------------------

 

