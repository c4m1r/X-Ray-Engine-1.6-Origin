#include "stdafx.h"
#pragma hdrstop

#include "Scene.h"
#include "SceneObject.h"
#include "bottombar.h"
#include "d3dutils.h"
#include "SpawnPoint.h"
#include "SpatialIndex.h"
#include "../../ECore/Editor/device.h"
#include "../../ECore/Editor/HW11.h"
#include "../../ECore/Editor/EditorShaders11.h"
#include "../../ECore/Editor/EditorTextures11.h"
#include "../../ECore/Editor/EditMesh.h"

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

	// Instance batches: unselected CSceneObjects grouped by mesh reference.
	// Within a group consecutive RenderRoot calls share shader/VB/format state
	// (RCache deduplicates D3D state changes), so only world-transform + DrawPrimitive
	// is issued per instance after the first in each group.
	// Selected objects and non-CSceneObjects use the legacy distance-sorted path.
	typedef xr_vector<CSceneObject*> CSOBatch;
	std::unordered_map<CEditableObject*, CSOBatch> inst_batches;
	inst_batches.reserve(512);

	// DX11 only: selected CSceneObjects rendered as single-instance draws with highlight tint
	xr_vector<CSceneObject*> dx11_selected;

	{
		// Use the lesser of m_fRenderRadius and the camera far plane so that
		// the spatial query never exceeds what the camera can actually see.
		const float render_radius = (m_fRenderRadius < EPrefs->view_fp)
		                            ? m_fRenderRadius : EPrefs->view_fp;
		xr_vector<CCustomObject*> candidates;
		candidates.reserve(4096);
		m_pSpatialIndex->Query(EDevice.vCameraPosition, render_radius * render_radius, candidates);

		u32 rendered_obj_count = 0;
		for (CCustomObject* obj : candidates) {
			if (!obj->Visible() || !obj->IsRender()) continue;
			++rendered_obj_count;

			if (obj->ClassID == OBJCLASS_SCENEOBJECT) {
				CSceneObject* so = static_cast<CSceneObject*>(obj);
				CEditableObject* ref = so->GetReference();
				if (ref) {
					if (obj->Selected()) {
						if (g_bEditorDX11) {
							dx11_selected.push_back(so);
						} else {
							float distSQ = EDevice.vCameraPosition.distance_to_sqr(obj->FPosition);
							mapRenderObjects.insertInAnyWay(distSQ, obj);
						}
					} else {
						inst_batches[ref].push_back(so);
					}
					continue;
				}
			}
			// No-reference or non-SceneObject — distance-sorted (DX9 only; DX11 skips these)
			if (!g_bEditorDX11) {
				float distSQ = EDevice.vCameraPosition.distance_to_sqr(obj->FPosition);
				mapRenderObjects.insertInAnyWay(distSQ, obj);
			}
		}
		EDevice.Statistic->dwRenderedObjects = rendered_obj_count;
	}

	// Helper: draw all surfaces of one mesh with per-surface textures.
	// The instance buffer must already be uploaded before calling.
	auto DrawMeshSurfaces = [&](CEditableMesh* mesh, u32 inst_count) {
        const RBMap* rbs = mesh->GetRenderBuffers();
        if (!rbs) return;
        for (auto& kv2 : *rbs) {
            CSurface* surf = kv2.first;
            ID3D11ShaderResourceView* srv =
                EditorTextures11.Get(HW11.pDevice, surf->_Texture());
            EditorShaders11.SetTexture(HW11.pContext, srv);
            mesh->RenderInstanced11(HW11.pContext, HW11.inst_buf, inst_count, surf);
        }
	};

	// DX11 hardware-instanced render of a batch of same-mesh objects.
	// Uploads one world matrix per CSceneObject to the instance buffer,
	// then calls DrawInstanced per surface (one texture bind per surface).
	auto RenderInstBatchesDX11 = [&]() {
        EditorShaders11.BindInstanced(HW11.pContext);

        xr_vector<EditorInstanceData> inst_data;
        for (auto& kv : inst_batches) {
            CEditableObject* ref = kv.first;
            const CSOBatch& batch = kv.second;

            inst_data.resize(batch.size());
            for (u32 i = 0; i < (u32)batch.size(); ++i) {
                const Fmatrix& world = batch[i]->_Transform();
                memcpy(inst_data[i].world, &world, sizeof(float)*16);
                inst_data[i].color[0] = inst_data[i].color[1] = inst_data[i].color[2] = 0.f;
                inst_data[i].color[3] = 0.f;
            }

            if (!HW11.UploadInstances(inst_data.data(), (u32)inst_data.size()))
                continue;

            for (CEditableMesh* mesh : ref->Meshes())
                DrawMeshSurfaces(mesh, (u32)inst_data.size());
        }

        // Render selected objects with orange highlight tint (single instance each)
        if (!dx11_selected.empty()) {
            EditorInstanceData single_inst;
            for (CSceneObject* so : dx11_selected) {
                CEditableObject* ref = so->GetReference();
                if (!ref) continue;
                const Fmatrix& world = so->_Transform();
                memcpy(single_inst.world, &world, sizeof(float)*16);
                single_inst.color[0] = 1.f;
                single_inst.color[1] = 0.5f;
                single_inst.color[2] = 0.f;
                single_inst.color[3] = 0.5f;
                if (!HW11.UploadInstances(&single_inst, 1)) continue;
                for (CEditableMesh* mesh : ref->Meshes())
                    DrawMeshSurfaces(mesh, 1);
            }
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

 

