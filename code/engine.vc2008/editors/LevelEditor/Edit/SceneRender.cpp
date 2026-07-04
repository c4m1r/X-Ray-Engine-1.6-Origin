#include "stdafx.h"
#pragma hdrstop

#include "Scene.h"
#include "SceneObject.h"
#include "SceneGizmo.h"
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

void RENDER_SCENE_TOOLS(const SceneMToolsSet& scene_tools, int P, bool B)
{
	auto s_it 	= scene_tools.begin();
	auto s_end	= scene_tools.end();
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

    // extract and sort scene tools (object_tools removed — it was built every frame but never read)
	SceneMToolsSet scene_tools;
	{
		SceneToolsMapPairIt t_it 	= m_SceneTools.begin();
		SceneToolsMapPairIt t_end 	= m_SceneTools.end();
		for (; t_it!=t_end; t_it++) {
			if (t_it->second){
				// before render
				t_it->second->BeforeRender();
				scene_tools.insert	(t_it->second);
			}
		}
	}

	// Rebuild spatial index if dirty (scene was modified or objects moved)
	bool spatial_rebuilt = false;
	if (m_bSpatialIndexDirty) {
		RebuildSpatialIndex();
		spatial_rebuilt = true;
	}

	// Instance batches: CSceneObjects grouped by mesh reference.
	typedef xr_vector<CSceneObject*> CSOBatch;
	static std::unordered_map<CEditableObject*, CSOBatch> s_inst_batches;
	static xr_vector<CCustomObject*> s_candidates;

	const float render_radius = (m_fRenderRadius < EPrefs->view_fp) ? m_fRenderRadius : EPrefs->view_fp;

	// DX11 candidate cache: reuse the batch list while the view does not change.
	// The query uses the SAME render_radius as the original per-frame path, so when
	// it does run the result is identical to before — no missing objects. We only
	// skip the work when the camera (position AND direction) and the scene are
	// unchanged, which is the common "editing a static view" case.
	static bool    s_cand_valid         = false;
	static Fvector s_cand_cam           = {0,0,0};
	static Fvector s_cand_dir           = {0,0,0};
	static u32     s_cand_gen           = 0xFFFFFFFFu; // last m_uObjChangeGen we built at
	static u32     s_rendered_obj_count = 0;

	// Invalidate on: first run, spatial index rebuild, any object transform/select
	// change (m_uObjChangeGen bumps in CSceneObject::OnUpdateTransform/Select), or
	// ANY camera movement/rotation. Direction is checked too because an arcball
	// camera orbits (the frustum sweeps to a different region on rotation).
	bool rebuild_candidates = true;
	if (g_bEditorDX11) {
		const bool view_same =
			EDevice.vCameraPosition.similar(s_cand_cam, EPS_L) &&
			EDevice.vCameraDirection.similar(s_cand_dir, EPS_L);
		rebuild_candidates = !s_cand_valid || spatial_rebuilt
		                  || (m_uObjChangeGen != s_cand_gen)
		                  || !view_same;
	}

	if (rebuild_candidates) {
		s_inst_batches.clear();
		s_inst_batches.reserve(512);
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
		s_rendered_obj_count = rendered_obj_count;
		s_cand_valid = true;
		s_cand_cam   = EDevice.vCameraPosition;
		s_cand_dir   = EDevice.vCameraDirection;
		s_cand_gen   = m_uObjChangeGen;
	}
	EDevice.Statistic->dwRenderedObjects = s_rendered_obj_count;

	auto& inst_batches = s_inst_batches;

	// Helper: draw all surfaces of one mesh with per-surface textures.
	// start_inst = first instance index in the shared instance buffer (StartInstanceLocation).
	// Two-pass: opaque first, then transparent (glass/window) with alpha blending.
	// Per-surface transparency, cached on the surface (computed once, not via strstr every frame).
	auto SurfTransparent = [](CSurface* surf) -> bool {
        if (surf->m_transparent11 < 0) {
            const char* name = surf->_ShaderName();
            bool t = name && (strstr(name, "glass")       != nullptr
                           || strstr(name, "transparent") != nullptr
                           || strstr(name, "window")      != nullptr);
            surf->m_transparent11 = t ? 1 : 0;
        }
        return surf->m_transparent11 != 0;
	};

	// Per-surface texture SRV. Fetched every frame so animated (.seq) fx textures return their
	// current frame; static textures resolve via a cheap name→SRV cache inside Get(). (The old
	// per-surface generation cache froze animated textures on their first frame.)
	auto SurfSRV = [&](CSurface* surf) -> ID3D11ShaderResourceView* {
        surf->m_srv11 = EditorTextures11.Get(HW11.pDevice, surf->_Texture());
        return surf->m_srv11;
	};

	auto DrawMeshSurfaces = [&](CEditableMesh* mesh, u32 inst_count, u32 start_inst) {
        const RBMap* rbs = mesh->GetRenderBuffers();
        if (!rbs) return;

        // Pass 1: opaque
        for (auto& kv2 : *rbs) {
            CSurface* surf = kv2.first;
            if (SurfTransparent(surf)) continue;
            EditorShaders11.SetTexture(HW11.pContext, SurfSRV(surf));
            mesh->RenderInstanced11(HW11.pContext, HW11.inst_buf, inst_count, surf, start_inst);
        }

        // Pass 2: transparent — alpha blend, no clip
        bool in_transparent = false;
        for (auto& kv2 : *rbs) {
            CSurface* surf = kv2.first;
            if (!SurfTransparent(surf)) continue;
            if (!in_transparent) {
                in_transparent = true;
                const float bf[4] = { 1,1,1,1 };
                HW11.pContext->OMSetBlendState(EditorShaders11.bs_alpha, bf, 0xFFFFFFFF);
                HW11.pContext->PSSetShader(EditorShaders11.ps_inst_transparent, nullptr, 0);
            }
            EditorShaders11.SetTexture(HW11.pContext, SurfSRV(surf));
            mesh->RenderInstanced11(HW11.pContext, HW11.inst_buf, inst_count, surf, start_inst);
        }
        if (in_transparent) {
            HW11.pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
            HW11.pContext->PSSetShader(EditorShaders11.ps_instanced, nullptr, 0);
        }
	};

	// LOD billboard texture SRV for a model — "lod\lod_<libname with '\'→'_'>".
	// Cached in EditorTextures11 by name.
	auto LodSRV = [&](CEditableObject* ref) -> ID3D11ShaderResourceView* {
        string_path nm;
        xr_strcpy(nm, sizeof(nm), "lod\\lod_");
        const char* lib = ref->GetName();
        char* d = nm + xr_strlen(nm);
        for (const char* s = lib; *s; ++s) *d++ = (*s == '\\') ? '_' : *s;
        *d = 0;
        return EditorTextures11.Get(HW11.pDevice, nm);
	};

	// DX11 hardware-instanced render of all batches in one GPU upload.
	// Phase 1: pack all visible instance data into one CPU array.
	// Phase 2: single UploadInstances (one Map/Unmap).
	// Phase 3: DrawInstanced per batch using StartInstanceLocation offset.
	auto RenderInstBatchesDX11 = [&]() {
        struct BatchDraw { CEditableObject* ref; u32 start, count; };
        static xr_vector<EditorInstanceData> s_all_inst;
        static xr_vector<GpuAabb>            s_all_aabbs;
        static xr_vector<BatchDraw>          s_draws;
        static xr_vector<CSceneObject*>      s_all_objs; // parallel to s_all_inst (for blink color refresh)
        static xr_vector<LodInstanceData>    s_lod_inst; // far vegetation drawn as billboards
        static xr_vector<BatchDraw>          s_lod_draws;

        // ---- Level 1 (expensive): rebuild instance/aabb/draw arrays only when the
        //      candidate set changed (scene edited or camera left the margin). ----
        if (rebuild_candidates) {
            s_all_inst.clear();
            s_all_aabbs.clear();
            s_draws.clear();
            s_all_objs.clear();
            s_lod_inst.clear();
            s_lod_draws.clear();

            const Fvector cam = EDevice.vCameraPosition;
            // Distance² beyond which a vegetation instance is drawn as a billboard LOD
            // instead of its full mesh (configurable via BottomBar → LOD Distance).
            const float lod_th2 = m_fLODRadius * m_fLODRadius;

            for (auto& kv : inst_batches) {
                CEditableObject* ref = kv.first;
                const CSOBatch& batch = kv.second;
                if (batch.empty()) continue;
                const bool ref_has_lod = ref->m_objectFlags.is(CEditableObject::eoUsingLOD);

                u32 mesh_start = (u32)s_all_inst.size();
                u32 lod_start  = (u32)s_lod_inst.size();
                for (u32 i = 0; i < (u32)batch.size(); ++i) {
                    CSceneObject* so = batch[i];
                    Fbox bb = so->GetBBox();
                    float cx = (bb.min.x+bb.max.x)*.5f;
                    float cy = (bb.min.y+bb.max.y)*.5f;
                    float cz = (bb.min.z+bb.max.z)*.5f;
                    float dx=cx-cam.x, dy=cy-cam.y, dz=cz-cam.z;
                    float dist2 = dx*dx+dy*dy+dz*dz;

                    // Far + has LOD + not selected → billboard.
                    if (ref_has_lod && dist2 > lod_th2 && !so->Selected()) {
                        LodInstanceData li;
                        li.center[0]=cx; li.center[1]=cy; li.center[2]=cz;
                        li.radius     = _max((bb.max.x-bb.min.x)*.5f, (bb.max.z-bb.min.z)*.5f);
                        li.halfHeight = (bb.max.y-bb.min.y)*.5f;
                        const Fmatrix& m = so->_Transform();
                        li.rotY = atan2f(m.k.x, m.k.z); // instance Y rotation from forward axis
                        li._pad[0]=li._pad[1]=0.f;
                        s_lod_inst.push_back(li);
                        continue;
                    }

                    // Near → full mesh instance.
                    EditorInstanceData id;
                    memcpy(id.world, &so->_Transform(), sizeof(float)*16);
                    id.color[0] = id.color[1] = id.color[2] = id.color[3] = 0.f; // filled in Level 2
                    s_all_inst.push_back(id);
                    s_all_objs.push_back(so);

                    GpuAabb aabb;
                    aabb.mn[0]=bb.min.x; aabb.mn[1]=bb.min.y; aabb.mn[2]=bb.min.z;
                    aabb.mx[0]=bb.max.x; aabb.mx[1]=bb.max.y; aabb.mx[2]=bb.max.z;
                    s_all_aabbs.push_back(aabb);
                }
                u32 mesh_count = (u32)s_all_inst.size() - mesh_start;
                if (mesh_count) s_draws.push_back({ref, mesh_start, mesh_count});
                u32 lod_count = (u32)s_lod_inst.size() - lod_start;
                if (lod_count) s_lod_draws.push_back({ref, lod_start, lod_count});
            }

            // AABBs change only on rebuild → upload once here.
            HW11.UploadCullAabbs(s_all_aabbs.data(), (u32)s_all_aabbs.size());
            if (!s_lod_inst.empty())
                HW11.UploadLODInstances(s_lod_inst.data(), (u32)s_lod_inst.size());
        }

        if (s_all_inst.empty() && s_lod_inst.empty()) return;

        // ---- Level 2 (mesh, cheap every frame): blink colors + conditional re-upload. ----
        if (!s_all_inst.empty()) {
            bool any_blink      = false;
            bool colors_changed = rebuild_candidates;
            for (u32 i = 0; i < (u32)s_all_objs.size(); ++i) {
                float r, g, b, a;
                int ba = s_all_objs[i]->BlinkAlpha();
                if (ba > 0) { r = g = b = 1.f; a = ba / 64.f; any_blink = true; }
                else        { r = g = b = a = 0.f; }
                EditorInstanceData& id = s_all_inst[i];
                if (id.color[0] != r || id.color[3] != a) {
                    id.color[0] = r; id.color[1] = g; id.color[2] = b; id.color[3] = a;
                    colors_changed = true;
                }
            }
            if (any_blink) UI->RedrawScene();
            if (colors_changed)
                HW11.UploadInstances(s_all_inst.data(), (u32)s_all_inst.size());
        }

        // ---- Frustum cull dispatch for mesh instances (planes depend on camera). ----
        float planes[6][4] = {};
        int pc = std::min(RImplementation.ViewBase.p_count, 6);
        for (int p = 0; p < pc; ++p) {
            planes[p][0] = RImplementation.ViewBase.planes[p].n.x;
            planes[p][1] = RImplementation.ViewBase.planes[p].n.y;
            planes[p][2] = RImplementation.ViewBase.planes[p].n.z;
            planes[p][3] = RImplementation.ViewBase.planes[p].d;
        }
        bool cull_ok = !s_all_aabbs.empty() && HW11.DispatchFrustumCull((u32)s_all_aabbs.size(), planes, pc);

        // ---- Draw near (full mesh, instanced) ----
        if (!s_draws.empty()) {
            EditorShaders11.BindInstanced(HW11.pContext);
            const UINT inst_stride = sizeof(EditorInstanceData);
            const UINT inst_offset = 0;
            HW11.pContext->IASetVertexBuffers(1, 1, &HW11.inst_buf, &inst_stride, &inst_offset);
            HW11.pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            for (auto& d : s_draws) {
                HW11.SetInstOffset(d.start, cull_ok);
                for (CEditableMesh* mesh : d.ref->Meshes())
                    DrawMeshSurfaces(mesh, d.count, d.start);
            }
            if (cull_ok) HW11.EndCull();
        }

        // ---- Draw far (LOD billboards, instanced quad) ----
        if (!s_lod_draws.empty() && HW11.lod_quad_vb && HW11.lod_inst_buf) {
            EditorShaders11.BindLOD(HW11.pContext);
            ID3D11Buffer* vbs[2] = { HW11.lod_quad_vb, HW11.lod_inst_buf };
            UINT strides[2] = { sizeof(float)*2, sizeof(LodInstanceData) };
            UINT offsets[2] = { 0, 0 };
            HW11.pContext->IASetVertexBuffers(0, 2, vbs, strides, offsets);
            HW11.pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            HW11.pContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);

            // Billboards are camera-facing quads — disable backface culling so they
            // are never rejected by winding order. FlushStates() applies the dirty RS.
            D3D11_CULL_MODE saved_cull = HW11.States.cull_mode;
            HW11.States.cull_mode = D3D11_CULL_NONE;
            HW11.States.rs_dirty  = true;
            HW11.FlushStates();

            for (auto& d : s_lod_draws) {
                ID3D11ShaderResourceView* srv = LodSRV(d.ref);
                HW11.pContext->PSSetShaderResources(0, 1, &srv);
                HW11.pContext->DrawInstanced(6, d.count, 0, d.start);
            }

            // restore rasterizer cull state
            HW11.States.cull_mode = saved_cull;
            HW11.States.rs_dirty  = true;
            HW11.FlushStates();
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

	//render snap list (red highlight). Works in both APIs now: CEditableMesh::RenderSelection
	//has a DX11 path (flat translucent overlay via RenderSectorColor11).
	RenderSnapList();

	// Transform gizmo — drawn on top of the selection (works in DX9 and DX11).
	Gizmo.Update();
	Gizmo.Render();

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

 

