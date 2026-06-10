#include "stdafx.h"
#pragma hdrstop

#include "Scene.h"
#include "SceneObject.h"
#include "bottombar.h"
#include "d3dutils.h"
#include "SpawnPoint.h"
#include "SpatialIndex.h"

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
		RCache.set_xform_world	(Fidentity);
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

			if (obj->ClassID == OBJCLASS_SCENEOBJECT && !obj->Selected()) {
				CSceneObject* so = static_cast<CSceneObject*>(obj);
				CEditableObject* ref = so->GetReference();
				if (ref) {
					inst_batches[ref].push_back(so);
					continue;
				}
			}
			// Selected, no-reference, or non-SceneObject — keep distance-sorted
			float distSQ = EDevice.vCameraPosition.distance_to_sqr(obj->FPosition);
			mapRenderObjects.insertInAnyWay(distSQ, obj);
		}
		EDevice.Statistic->dwRenderedObjects = rendered_obj_count;
	}

	// Render instance batches at given priority/strictB2F.
	// Groups are in reference-mesh order → consecutive same-ref calls hit RCache.
	auto RenderInstBatches = [&](int priority, bool strictB2F) {
		for (auto& kv : inst_batches) {
			for (CSceneObject* so : kv.second) {
				so->RenderRoot(priority, strictB2F);
			}
		}
	};

// priority #0
	// normal
	RenderInstBatches				(0, false);
	mapRenderObjects.traverseLR		(object_Normal_0);
	RENDER_SCENE_TOOLS				(scene_tools, 0,false);
	// alpha
	RenderInstBatches				(0, true);
	mapRenderObjects.traverseRL		(object_StrictB2F_0);
	RENDER_SCENE_TOOLS				(scene_tools, 0,true);

// priority #1
	// normal
	RenderInstBatches				(1, false);
	mapRenderObjects.traverseLR		(object_Normal_1);
	RENDER_SCENE_TOOLS				(scene_tools, 1,false);
	// alpha
	RenderInstBatches				(1, true);
	mapRenderObjects.traverseRL		(object_StrictB2F_1);
	RENDER_SCENE_TOOLS				(scene_tools, 1,true);
// priority #2
	// normal
	RenderInstBatches				(2, false);
	mapRenderObjects.traverseLR		(object_Normal_2);
	RENDER_SCENE_TOOLS				(scene_tools, 2,false);
	// alpha
	RenderInstBatches				(2, true);
	mapRenderObjects.traverseRL		(object_StrictB2F_2);
	RENDER_SCENE_TOOLS				(scene_tools, 2,true);
// priority #3
	// normal
	RenderInstBatches				(3, false);
	mapRenderObjects.traverseLR		(object_Normal_3);
	RENDER_SCENE_TOOLS				(scene_tools, 3,false);
	// alpha
	RenderInstBatches				(3, true);
	mapRenderObjects.traverseRL		(object_StrictB2F_3);
	RENDER_SCENE_TOOLS				(scene_tools, 3,true);

	//render snap
	RenderSnapList			();

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

 

