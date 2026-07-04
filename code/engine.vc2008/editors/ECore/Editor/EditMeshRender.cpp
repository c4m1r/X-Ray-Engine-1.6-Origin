//----------------------------------------------------
// file: StaticMesh.cpp
//----------------------------------------------------

#include "stdafx.h"
#pragma hdrstop

// range fix
//#include "EditMeshVLight.h"
#include "EditMesh.h"
#include "EditObject.h"
#include "EditorPreferences.h"   // EPrefs->render_backface (double-sided toggle)
#include "ui_main.h"
#include "d3dutils.h"
#include "render.h"
#include "device.h"
#include "EditorShaders11.h"
#include "ResourceManager11.h"   // Resources11.cb_PerFrame / UploadPerObject
//----------------------------------------------------
#define F_LIM (10000)
#define V_LIM (F_LIM*3)

#include <unordered_map>
//----------------------------------------------------
// Vertex dedup key for building indexed DX11 geometry (bitwise compare of EditorVertex11).
namespace {
    struct EVtxKey {
        EditorVertex11 v;
        bool operator==(const EVtxKey& o) const { return 0 == memcmp(&v, &o.v, sizeof(v)); }
    };
    struct EVtxHash {
        size_t operator()(const EVtxKey& k) const {
            const u8* p = (const u8*)&k.v;
            size_t h = 1469598103934665603ULL;            // FNV-1a
            for (size_t i = 0; i < sizeof(k.v); ++i) { h ^= p[i]; h *= 1099511628211ULL; }
            return h;
        }
    };

    //------------------------------------------------------------------
    // Vertex-cache optimization (Tom Forsyth, "linear-speed vertex cache
    // optimisation"). Reorders triangle indices so neighbouring triangles
    // reuse vertices still in the GPU post-transform cache → far fewer VS
    // invocations. Geometry is unchanged; only the index order is permuted.
    //------------------------------------------------------------------
    static const int   VCO_CACHE = 32;
    static float Forsyth_Score(int activeTris, int cachePos)
    {
        if (activeTris <= 0) return -1.f;                 // vertex has no remaining triangles
        float score = 0.f;
        if (cachePos >= 0) {
            if (cachePos < 3) {
                score = 0.75f;                            // last-triangle bonus
            } else {
                const float scaler = 1.f / (VCO_CACHE - 3);
                score = 1.f - (cachePos - 3) * scaler;
                score = powf(score, 1.5f);                // cache-decay power
            }
        }
        score += 2.0f * powf((float)activeTris, -0.5f);   // valence boost
        return score;
    }

    static void OptimizeVertexCache(xr_vector<u32>& indices, u32 vertexCount)
    {
        const u32 numIndices = (u32)indices.size();
        if (numIndices < 6 || vertexCount == 0) return;
        const u32 numTris = numIndices / 3;

        // Build vertex → triangle adjacency (CSR layout).
        xr_vector<u32> triCount(vertexCount, 0);
        for (u32 i = 0; i < numIndices; ++i) triCount[indices[i]]++;
        xr_vector<u32> triOffset(vertexCount + 1, 0);
        for (u32 v = 0; v < vertexCount; ++v) triOffset[v + 1] = triOffset[v] + triCount[v];
        xr_vector<u32> vtxTris(numIndices);
        xr_vector<u32> fill(vertexCount, 0);
        for (u32 t = 0; t < numTris; ++t)
            for (int k = 0; k < 3; ++k) { u32 v = indices[t*3+k]; vtxTris[triOffset[v] + fill[v]++] = t; }
        // 'remaining[v]' shrinks as triangles are emitted; triOffset[v]..+remaining[v] holds live tris.
        xr_vector<u32> remaining = triCount;

        xr_vector<int>   active(vertexCount);
        xr_vector<int>   cachePos(vertexCount, -1);
        xr_vector<float> vScore(vertexCount);
        for (u32 v = 0; v < vertexCount; ++v) { active[v] = (int)triCount[v]; vScore[v] = Forsyth_Score(active[v], -1); }

        xr_vector<float> tScore(numTris);
        xr_vector<char>  tDone(numTris, 0);
        for (u32 t = 0; t < numTris; ++t)
            tScore[t] = vScore[indices[t*3]] + vScore[indices[t*3+1]] + vScore[indices[t*3+2]];

        xr_vector<u32> out; out.reserve(numIndices);
        int  cache[VCO_CACHE + 3];   // LRU of vertex indices, front = most recent
        int  newCache[VCO_CACHE + 3];
        int  cacheCnt = 0;

        int best = -1;
        u32 done = 0;
        while (done < numTris) {
            if (best < 0) {                               // fallback: linear scan for max score
                float bs = -1.f;
                for (u32 t = 0; t < numTris; ++t) if (!tDone[t] && tScore[t] > bs) { bs = tScore[t]; best = (int)t; }
                if (best < 0) break;
            }
            tDone[best] = 1; ++done;
            const u32 tv[3] = { indices[best*3], indices[best*3+1], indices[best*3+2] };
            out.push_back(tv[0]); out.push_back(tv[1]); out.push_back(tv[2]);

            // Drop this triangle from its vertices' live lists, decrement valence.
            for (int k = 0; k < 3; ++k) {
                u32 vv = tv[k];
                u32 b = triOffset[vv], n = remaining[vv];
                for (u32 j = 0; j < n; ++j) if (vtxTris[b+j] == (u32)best) { vtxTris[b+j] = vtxTris[b+n-1]; remaining[vv]--; break; }
                active[vv]--;
            }

            // Rebuild LRU: 3 new vertices to front, then prior cache entries (dedup).
            int nc = 0;
            newCache[nc++] = (int)tv[0];
            newCache[nc++] = (int)tv[1];
            newCache[nc++] = (int)tv[2];
            for (int i = 0; i < cacheCnt && nc < VCO_CACHE + 3; ++i) {
                int cv = cache[i];
                if (cv != (int)tv[0] && cv != (int)tv[1] && cv != (int)tv[2]) newCache[nc++] = cv;
            }
            cacheCnt = nc;
            for (int i = 0; i < nc; ++i) cache[i] = newCache[i];

            // Refresh cache positions / vertex scores; collect affected triangles.
            best = -1;
            float bs = -1.f;
            for (int i = 0; i < cacheCnt; ++i) {
                int v = cache[i];
                cachePos[v] = (i < VCO_CACHE) ? i : -1;   // entries past cache size are evicted
                vScore[v]   = Forsyth_Score(active[v], cachePos[v]);
            }
            for (int i = 0; i < cacheCnt; ++i) {
                int v = cache[i];
                u32 b = triOffset[v], n = remaining[v];
                for (u32 j = 0; j < n; ++j) {
                    u32 t = vtxTris[b+j];
                    if (tDone[t]) continue;
                    tScore[t] = vScore[indices[t*3]] + vScore[indices[t*3+1]] + vScore[indices[t*3+2]];
                    if (tScore[t] > bs) { bs = tScore[t]; best = (int)t; }
                }
            }
            // Trim cache to scoring window so evicted entries stop being considered.
            if (cacheCnt > VCO_CACHE) cacheCnt = VCO_CACHE;
        }

        indices.swap(out);
    }
}
//----------------------------------------------------
void CEditableMesh::GenerateRenderBuffers()
{
//    CTimer T;
//    T.Start();
/*
    CMemoryWriter 	F;
    m_Parent->PrepareOGF(F,false,this);
	IReader R		(F.pointer(), F.size());
	m_Visual 		= ::Render->Models->Create(GetName(),&R);
//    Log				("Time: ",T.GetElapsed_sec());
//	string_path fn;
//	strconcat		(fn,"_alexmx_\\",GetName(),".ogf");
//	FS.update_path	(fn,_import_,fn);
//	F.save_to		(fn);
	return;
*/
    if (m_RenderBuffers) return;
    m_RenderBuffers		= xr_new<RBMap>();

    GenerateVNormals	(0);

    VERIFY				(m_VertexNormals);

    for (SurfFacesPairIt sp_it=m_SurfFaces.begin(); sp_it!=m_SurfFaces.end(); sp_it++){
		IntVec& face_lst = sp_it->second;
        CSurface* _S = sp_it->first;

        // DX11: EditorVertex11 requires pos+normal+uv (32 bytes). Skip surfaces with other layouts.
        if (g_bEditorDX11) {
            const u32 req = D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1;
            if ((_S->_FVF() & req) != req || D3DXGetFVFVertexSize(_S->_FVF()) != (u32)sizeof(EditorVertex11))
                continue;
        }

        int num_verts=face_lst.size()*3;
        RBVector rb_vec;
		int v_cnt=num_verts;
        int start_face=0;
        int num_face;
        VERIFY3	(v_cnt,"Empty surface arrive.",_S->_Name());
        do{
	        rb_vec.push_back	(st_RenderBuffer(0,(v_cnt<V_LIM)?v_cnt:V_LIM));
            st_RenderBuffer& rb	= rb_vec.back();
            if (_S->m_Flags.is(CSurface::sf2Sided)) 	rb.dwNumVertex *= 2;
            num_face			= (v_cnt<V_LIM)?v_cnt/3:F_LIM;

            int buf_size		= D3DXGetFVFVertexSize(_S->_FVF())*rb.dwNumVertex;
            R_ASSERT2			(buf_size,"Empty buffer size or bad FVF.");
			u8*	bytes			= 0;

            if (g_bEditorDX11) {
                // DX11: build vertices on CPU, then deduplicate into an indexed buffer
                // so the GPU vertex shader runs once per unique vertex, not per triangle corner.
                xr_vector<u8> cpu_buf(buf_size);
                bytes = cpu_buf.data();
                FillRenderBuffer(face_lst, start_face, num_face, _S, bytes);

                const EditorVertex11* src = (const EditorVertex11*)cpu_buf.data();
                const u32 vcount = rb.dwNumVertex;

                std::unordered_map<EVtxKey, u32, EVtxHash> vmap;
                vmap.reserve(vcount);
                xr_vector<EditorVertex11> uniq;  uniq.reserve(vcount);
                xr_vector<u32>            indices(vcount);
                for (u32 i = 0; i < vcount; ++i) {
                    EVtxKey k; k.v = src[i];
                    auto it = vmap.find(k);
                    if (it != vmap.end()) {
                        indices[i] = it->second;
                    } else {
                        u32 ni = (u32)uniq.size();
                        uniq.push_back(src[i]);
                        vmap.emplace(k, ni);
                        indices[i] = ni;
                    }
                }

                // Vertex buffer (unique vertices)
                D3D11_BUFFER_DESC vbd = {};
                vbd.ByteWidth = (UINT)(uniq.size() * sizeof(EditorVertex11));
                vbd.Usage     = D3D11_USAGE_IMMUTABLE;
                vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
                D3D11_SUBRESOURCE_DATA vsd = { uniq.data(), 0, 0 };
                R_ASSERT2(SUCCEEDED(HW11.pDevice->CreateBuffer(&vbd, &vsd, &rb.pVB11)), "DX11: failed to create mesh VB");
                rb.dwVB11VertexCount = (u32)uniq.size();

                // Reorder indices for the GPU post-transform vertex cache (Forsyth)
                // → fewer VS invocations on the same geometry.
                OptimizeVertexCache(indices, (u32)uniq.size());

                // Index buffer (32-bit — meshes can exceed 65k corners before dedup)
                D3D11_BUFFER_DESC ibd = {};
                ibd.ByteWidth = (UINT)(indices.size() * sizeof(u32));
                ibd.Usage     = D3D11_USAGE_IMMUTABLE;
                ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
                D3D11_SUBRESOURCE_DATA isd = { indices.data(), 0, 0 };
                R_ASSERT2(SUCCEEDED(HW11.pDevice->CreateBuffer(&ibd, &isd, &rb.pIB11)), "DX11: failed to create mesh IB");
                rb.dwIB11IndexCount = (u32)indices.size();
            } else {
                IDirect3DVertexBuffer9*	pVB=0;
                R_CHK(HW.pDevice->CreateVertexBuffer(buf_size, D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED, &pVB, 0));
                rb.pGeom.create(_S->_FVF(), pVB, 0);
                R_CHK(pVB->Lock(0, 0, (LPVOID*)&bytes, 0));
                FillRenderBuffer(face_lst, start_face, num_face, _S, bytes);
                pVB->Unlock();
            }

            v_cnt				-= V_LIM;
            start_face			+= (_S->m_Flags.is(CSurface::sf2Sided))?rb.dwNumVertex/6:rb.dwNumVertex/3;
        }while(v_cnt>0);
        if (num_verts>0) m_RenderBuffers->insert(mk_pair(_S, std::move(rb_vec)));
    }
    UnloadVNormals();
}
//----------------------------------------------------

void CEditableMesh::UnloadRenderBuffers()
{
	if (m_RenderBuffers){
        for (RBMapPairIt rbmp_it=m_RenderBuffers->begin(); rbmp_it!=m_RenderBuffers->end(); rbmp_it++){
            for(RBVecIt rb_it=rbmp_it->second.begin(); rb_it!=rbmp_it->second.end(); rb_it++)
                if (rb_it->pGeom){
                    _RELEASE		(rb_it->pGeom->vb);
                    _RELEASE		(rb_it->pGeom->ib);
                    rb_it->pGeom.destroy();
                }
        }
        xr_delete					(m_RenderBuffers);
    }
}
//----------------------------------------------------

#ifdef _EDITOR
auto CEditableMesh::GetRenderBuffers() -> const RBMap*
{
    if (!m_RenderBuffers)
        GenerateRenderBuffers();
    return m_RenderBuffers;
}
#endif
//----------------------------------------------------

void CEditableMesh::RenderInstanced11(ID3D11DeviceContext* ctx,
                                      ID3D11Buffer* inst_buf,
                                      u32 inst_count,
                                      CSurface* filter_surf,
                                      u32 start_inst)
{
    (void)inst_buf; // shared instance buffer (slot 1) is now bound once by the caller

    // Lazy init: generate DX11 vertex buffers on first use
    if (!m_RenderBuffers)
        GenerateRenderBuffers();
    if (!m_RenderBuffers) return;

    const UINT vert_stride = sizeof(EditorVertex11);
    const UINT vert_offset = 0;

    // Caller (RenderInstBatchesDX11) binds the shared instance buffer (slot 1) and
    // primitive topology once per frame, so here we only swap the per-surface vertex
    // buffer (slot 0). filter_surf is looked up directly — no O(N) scan of the map.
    auto draw_rb = [&](RBVector& rb_vec) {
        for (st_RenderBuffer& rb : rb_vec) {
            if (!rb.pVB11 || !rb.dwVB11VertexCount) continue;
            ctx->IASetVertexBuffers(0, 1, &rb.pVB11, &vert_stride, &vert_offset);
            if (rb.pIB11 && rb.dwIB11IndexCount) {
                ctx->IASetIndexBuffer(rb.pIB11, DXGI_FORMAT_R32_UINT, 0);
                ctx->DrawIndexedInstanced(rb.dwIB11IndexCount, inst_count, 0, 0, start_inst);
            } else {
                ctx->DrawInstanced(rb.dwVB11VertexCount, inst_count, 0, start_inst);
            }
        }
    };

    if (filter_surf) {
        RBMapPairIt rbmp = m_RenderBuffers->find(filter_surf);
        if (rbmp != m_RenderBuffers->end())
            draw_rb(rbmp->second);
    } else {
        for (RBMapPairIt rbmp = m_RenderBuffers->begin(); rbmp != m_RenderBuffers->end(); ++rbmp)
            draw_rb(rbmp->second);
    }
}
//----------------------------------------------------

// DX11: flat translucent fill of the whole mesh in a single solid color (sector overlay).
// Reuses the existing DX11 vertex/index buffers; draws with the colored shader
// (outputs ObjectColor.rgb with ObjectColor.a), alpha-blended, no depth-write, double-sided.
void CEditableMesh::RenderSectorColor11(const Fmatrix& world, float r, float g, float b, float a)
{
    if (!m_RenderBuffers) GenerateRenderBuffers();
    if (!m_RenderBuffers) return;

    ID3D11DeviceContext* ctx = HW11.pContext;
    if (!ctx) return;

    EditorShaders11.BindColored(ctx);
    ctx->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);
    Resources11.UploadPerObject((const float*)&world, r, g, b, a);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // translucent overlay state (saved/restored)
    const bool            saved_blend = HW11.States.alpha_blend;
    const bool            saved_zw    = HW11.States.depth_write;
    const D3D11_CULL_MODE saved_cull  = HW11.States.cull_mode;
    HW11.States.alpha_blend = true;             HW11.States.bs_dirty = true;
    HW11.States.depth_write = false;            HW11.States.ds_dirty = true;
    HW11.States.cull_mode   = D3D11_CULL_NONE;  HW11.States.rs_dirty = true;
    HW11.FlushStates();

    const UINT stride = sizeof(EditorVertex11);
    const UINT offset = 0;
    for (RBMapPairIt p_it = m_RenderBuffers->begin(); p_it != m_RenderBuffers->end(); ++p_it) {
        for (st_RenderBuffer& rb : p_it->second) {
            if (!rb.pVB11 || !rb.dwVB11VertexCount) continue;
            ctx->IASetVertexBuffers(0, 1, &rb.pVB11, &stride, &offset);
            if (rb.pIB11 && rb.dwIB11IndexCount) {
                ctx->IASetIndexBuffer(rb.pIB11, DXGI_FORMAT_R32_UINT, 0);
                ctx->DrawIndexed(rb.dwIB11IndexCount, 0, 0);
            } else {
                ctx->Draw(rb.dwVB11VertexCount, 0);
            }
        }
    }

    HW11.States.alpha_blend = saved_blend; HW11.States.bs_dirty = true;
    HW11.States.depth_write = saved_zw;    HW11.States.ds_dirty = true;
    HW11.States.cull_mode   = saved_cull;  HW11.States.rs_dirty = true;
    HW11.FlushStates();
}
//----------------------------------------------------

void CEditableMesh::FillRenderBuffer(IntVec& face_lst, int start_face, int num_face, const CSurface* surf, LPBYTE& src_data)
{
	LPBYTE data 		= src_data;
    u32 dwFVF 			= surf->_FVF();
	u32 dwTexCnt 		= ((dwFVF&D3DFVF_TEXCOUNT_MASK)>>D3DFVF_TEXCOUNT_SHIFT);
    for (int fl_i=start_face; fl_i<start_face+num_face; fl_i++){
        u32 f_index 	= face_lst[fl_i];
        VERIFY			(f_index<m_FaceCount);
    	st_Face& face 	= m_Faces[f_index];
        for (int k=0; k<3; k++){
            st_FaceVert& fv = face.pv[k];
            u32 norm_id = f_index*3+k;//fv.pindex;
	        VERIFY2(norm_id<m_FaceCount*3,"Normal index out of range.");
            VERIFY2(fv.pindex<(int)m_VertCount,"Point index out of range.");
            Fvector& PN = m_VertexNormals[norm_id];
			Fvector& V 	= m_Vertices[fv.pindex];
            int sz;
            if (dwFVF&D3DFVF_XYZ){
                sz=sizeof(Fvector);
                VERIFY2(fv.pindex<int(m_VertCount),"- Face index out of range.");
                CopyMemory(data,&V,sz);
                data+=sz;
            }
            if (dwFVF&D3DFVF_NORMAL){
                sz=sizeof(Fvector);
                CopyMemory(data,&PN,sz);
                data+=sz;
            }
            sz			= sizeof(Fvector2);
            int offs 	= 0;
            for (int t=0; t<(int)dwTexCnt; t++){
                VERIFY2((t+offs)<(int)m_VMRefs[fv.vmref].count,"- VMap layer index out of range");
            	st_VMapPt& vm_pt 	= m_VMRefs[fv.vmref].pts[t+offs];
                if (m_VMaps[vm_pt.vmap_index]->type!=vmtUV){
                	offs++;
                    t--;
                    continue;
                }
                VERIFY2	(vm_pt.vmap_index<int(m_VMaps.size()),"- VMap index out of range");
				st_VMap* vmap		= m_VMaps[vm_pt.vmap_index];
                VERIFY2	(vm_pt.index<vmap->size(),"- VMap point index out of range");
                CopyMemory(data,&vmap->getUV(vm_pt.index),sz); data+=sz;
//                Msg("%3.2f, %3.2f",vmap->getUV(vm_pt.index).x,vmap->getUV(vm_pt.index).y);
            }
        }
        if (surf->m_Flags.is(CSurface::sf2Sided)){
            for (int k=2; k>=0; k--){
                st_FaceVert& fv = face.pv[k];
	            Fvector& PN = m_VertexNormals[f_index*3+k];
                int sz;
                if (dwFVF&D3DFVF_XYZ){
                    sz=sizeof(Fvector);
	                VERIFY2(fv.pindex<int(m_VertCount),"- Face index out of range.");
                    CopyMemory(data,&m_Vertices[fv.pindex],sz);
                    data+=sz;
                }
                if (dwFVF&D3DFVF_NORMAL){
                    sz=sizeof(Fvector);
                    Fvector N; N.invert(PN);
                    CopyMemory(data,&N,sz);
                    data+=sz;
                }
                sz=sizeof(Fvector2);
				int offs = 0;
                for (int t=0; t<(int)dwTexCnt; t++){
	                VERIFY2((t+offs)<(int)m_VMRefs[fv.vmref].count,"- VMap layer index out of range");
                    st_VMapPt& vm_pt 	= m_VMRefs[fv.vmref].pts[t];
                    if (m_VMaps[vm_pt.vmap_index]->type!=vmtUV){
                        offs++;
                        t--;
                        continue;
                    }
	                VERIFY2(vm_pt.vmap_index<int(m_VMaps.size()),"- VMap index out of range");
                    st_VMap* vmap		= m_VMaps[vm_pt.vmap_index];
    	            VERIFY2(vm_pt.index<vmap->size(),"- VMap point index out of range");
                    CopyMemory(data,&vmap->getUV(vm_pt.index),sz); data+=sz;

//	                Msg("%3.2f, %3.2f",vmap->getUV(vm_pt.index).x,vmap->getUV(vm_pt.index).y);
                }
            }
        }
    }
}
//----------------------------------------------------
void CEditableMesh::Render(const Fmatrix& parent, CSurface* S)
{
	if (0==m_RenderBuffers) GenerateRenderBuffers();
	// visibility test
	if (!m_Flags.is(flVisible)) return;
	// frustum test
    Fbox bb; bb.set(m_Box);
	bb.xform(parent);
	if (!::Render->occ_visible(bb)) return;

    if (g_bEditorDX11) {
        // DX11: CPU-build this surface's faces into world-space FVF::V (pos+uv) and draw with the
        // base texture via the editor DX11 layer. The DX9 streaming DP path below is unavailable;
        // LevelEditor draws statics via hardware instancing (RenderInstanced11), but ActorEditor
        // renders a single object directly, so build + draw it here.
        SurfFacesPairIt sp_it = m_SurfFaces.find(S);
        if (sp_it == m_SurfFaces.end()) return;
        IntVec& face_lst = sp_it->second;
        static xr_vector<FVF::V> vb;
        vb.clear();
        const bool two_sided = S->m_Flags.is(CSurface::sf2Sided);
        vb.reserve(face_lst.size() * 3 * (two_sided ? 2 : 1));
        for (IntIt i_it = face_lst.begin(); i_it != face_lst.end(); ++i_it)
        {
            auto& F = m_Faces[*i_it];
            FVF::V tri[3];
            for (int k = 0; k < 3; k++)
            {
                auto& fv = F.pv[k];
                parent.transform_tiny(tri[k].p, m_Vertices[fv.pindex]);
                tri[k].t.set(0.f, 0.f);
                auto& vref = m_VMRefs[fv.vmref];            // first UV-type vmap layer of this corner
                for (u32 t = 0; t < vref.count; t++)
                {
                    auto& vm_pt = vref.pts[t];
                    auto  vmap  = m_VMaps[vm_pt.vmap_index];
                    if (vmap->type == vmtUV) { tri[k].t.set(vmap->getUV(vm_pt.index)); break; }
                }
            }
            vb.push_back(tri[0]); vb.push_back(tri[1]); vb.push_back(tri[2]);
            if (two_sided) { vb.push_back(tri[2]); vb.push_back(tri[1]); vb.push_back(tri[0]); }
        }
        if (!vb.empty())
            HW11.DrawMeshTex(vb.data(), (u32)vb.size(), S->_Texture(), !(EPrefs && EPrefs->render_backface));
        return;
    }
	// render
	RBMapPairIt rb_pair = m_RenderBuffers->find(S);
	if (rb_pair!=m_RenderBuffers->end())
	{
		RBVector& rb_vec = rb_pair->second;
		for (RBVecIt rb_it=rb_vec.begin(); rb_it!=rb_vec.end(); rb_it++)
		{
			EDevice.DP(D3DPT_TRIANGLELIST,rb_it->pGeom,0,rb_it->dwNumVertex/3);
        }
	}
}
//----------------------------------------------------
#define MAX_VERT_COUNT 0xFFFF
static Fvector RB[MAX_VERT_COUNT];
static int RB_cnt=0;

void CEditableMesh::RenderList(const Fmatrix& parent, u32 color, bool bEdge, IntVec& fl)
{
//	if (!m_Visible) return;
//	if (!m_LoadState.is(LS_RBUFFERS)) CreateRenderBuffers();

	if (fl.size()==0) return;
	RCache.set_xform_world(parent);
	EDevice.RenderNearer(0.0006);
	RB_cnt = 0;
    if (bEdge){
    	EDevice.SetShader(EDevice.m_WireShader);
	    EDevice.SetRS(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
    }else
    	EDevice.SetShader(EDevice.m_SelectionShader);
    for (IntIt dw_it=fl.begin(); dw_it!=fl.end(); ++dw_it)
    {
        st_Face& face 		= m_Faces[*dw_it];
        for (int k=0; k<3; ++k)
        	RB[RB_cnt++].set(m_Vertices[face.pv[k].pindex]);

		if (RB_cnt==MAX_VERT_COUNT)
        {
        	DU_impl.DrawPrimitiveL(D3DPT_TRIANGLELIST,RB_cnt/3,RB,RB_cnt,color,true,false);
			RB_cnt = 0;
        }
    }

	if (RB_cnt)
    	DU_impl.DrawPrimitiveL(D3DPT_TRIANGLELIST,RB_cnt/3,RB,RB_cnt,color,true,false);

    if (bEdge)
    	EDevice.SetRS(D3DRS_FILLMODE,EDevice.dwFillMode);

	EDevice.ResetNearer();
}
//----------------------------------------------------

void CEditableMesh::RenderSelection(const Fmatrix& parent, CSurface* s, u32 color)
{
    if (0==m_RenderBuffers) GenerateRenderBuffers();
//	if (!m_Visible) return;
    Fbox bb; bb.set(m_Box);
    bb.xform(parent);
	if (!::Render->occ_visible(bb)) return;

    // DX11: flat translucent overlay of the whole mesh in the highlight colour (snap list,
    // selection, sector fill). The DX9 path below uses RCache/D3DRS_TEXTUREFACTOR, unavailable
    // in DX11. color is D3DCOLOR (ARGB). Per-surface filter (s) is ignored — callers that need
    // the highlight pass s==0 (whole mesh).
    if (g_bEditorDX11) {
        const float a = ((color>>24)&0xff)/255.f, r = ((color>>16)&0xff)/255.f,
                    g = ((color>> 8)&0xff)/255.f, b = ( color     &0xff)/255.f;
        RenderSectorColor11(parent, r, g, b, a);
        return;
    }
    // render
	RCache.set_xform_world(parent);
    if (s){
        SurfFacesPairIt sp_it = m_SurfFaces.find(s);
        if (sp_it!=m_SurfFaces.end()) RenderList(parent,color,false,sp_it->second);
    }else{
	    EDevice.SetRS(D3DRS_TEXTUREFACTOR,	color);
        for (RBMapPairIt p_it=m_RenderBuffers->begin(); p_it!=m_RenderBuffers->end(); p_it++){
            RBVector& rb_vec = p_it->second;
            for (RBVecIt rb_it=rb_vec.begin(); rb_it!=rb_vec.end(); rb_it++)
                EDevice.DP(D3DPT_TRIANGLELIST,rb_it->pGeom,0,rb_it->dwNumVertex/3);
        }
	    EDevice.SetRS(D3DRS_TEXTUREFACTOR,	0xffffffff);
    }
}
//----------------------------------------------------

void CEditableMesh::RenderEdge(const Fmatrix& parent, CSurface* s, u32 color)
{
    if (0==m_RenderBuffers) GenerateRenderBuffers();
//	if (!m_Visible) return;
	RCache.set_xform_world(parent);
	EDevice.SetShader(EDevice.m_WireShader);
	EDevice.RenderNearer(0.001);

    // render
    EDevice.SetRS(D3DRS_FILLMODE,D3DFILL_WIREFRAME);
    if (s){
        SurfFacesPairIt sp_it = m_SurfFaces.find(s);
        if (sp_it!=m_SurfFaces.end()) RenderList(parent,color,true,sp_it->second);
    }else{
	    EDevice.SetRS(D3DRS_TEXTUREFACTOR,	color);
        for (RBMapPairIt p_it=m_RenderBuffers->begin(); p_it!=m_RenderBuffers->end(); p_it++){
            RBVector& rb_vec = p_it->second;
            for (RBVecIt rb_it=rb_vec.begin(); rb_it!=rb_vec.end(); rb_it++)
                EDevice.DP(D3DPT_TRIANGLELIST,rb_it->pGeom,0,rb_it->dwNumVertex/3);
        }
	    EDevice.SetRS(D3DRS_TEXTUREFACTOR,	0xffffffff);
    }
    EDevice.SetRS(D3DRS_FILLMODE,EDevice.dwFillMode);
    EDevice.ResetNearer();
}
//----------------------------------------------------

#define SKEL_MAX_FACE_COUNT 10000
struct svertRender
{
	Fvector		P;
	Fvector		N;
	Fvector2 	uv;
};
void CEditableMesh::RenderSkeleton(const Fmatrix& parent, CSurface* S)
{
    if (false==IsGeneratedSVertices(RENDER_SKELETON_LINKS))
    	GenerateSVertices(RENDER_SKELETON_LINKS);

	R_ASSERT2(m_SVertices,"SVertices empty!");
	SurfFacesPairIt sp_it 	= m_SurfFaces.find(S); R_ASSERT(sp_it!=m_SurfFaces.end());
    IntVec& face_lst 		= sp_it->second;

    if (g_bEditorDX11) {
        // DX11: CPU-skin this surface's faces (same bone blend as the DX9 path below) into
        // world-space FVF::V (pos+uv) and draw via the editor DX11 layer with the surface
        // texture. The DX9 streaming VB (RCache.Vertex) is unavailable under DX11. Used by
        // ActorEditor's "editor style" (CEditableObject::RenderSkeletonSingle).
        static xr_vector<FVF::V> vb;
        vb.clear();
        const bool two_sided = S->m_Flags.is(CSurface::sf2Sided);
        vb.reserve(face_lst.size() * (two_sided ? 6 : 3));
        for (IntIt i_it=face_lst.begin(); i_it!=face_lst.end(); i_it++)
        {
            FVF::V tri[3];
            for (int k=0; k<3; k++)
            {
                st_SVert& SV = m_SVertices[*i_it*3+k];
                float total  = SV.bones[0].w;
                Fvector Pl;
                m_Parent->m_Bones[SV.bones[0].id]->_RenderTransform().transform_tiny(Pl, SV.offs);
                for (u8 cnt=1; cnt<(u8)SV.bones.size(); cnt++)
                {
                    total += SV.bones[cnt].w;
                    Fvector P;
                    m_Parent->m_Bones[SV.bones[cnt].id]->_RenderTransform().transform_tiny(P, SV.offs);
                    Pl.lerp(Pl, P, SV.bones[cnt].w/total);
                }
                parent.transform_tiny(tri[k].p, Pl);   // object-local → world (DrawMeshTex is world-space)
                tri[k].t.set(SV.uv);
            }
            vb.push_back(tri[0]); vb.push_back(tri[1]); vb.push_back(tri[2]);
            if (two_sided) { vb.push_back(tri[2]); vb.push_back(tri[1]); vb.push_back(tri[0]); }
        }
        if (!vb.empty())
            HW11.DrawMeshTex(vb.data(), (u32)vb.size(), S->_Texture(), !(EPrefs && EPrefs->render_backface));
        return;
    }

	_VertexStream*	Stream	= &RCache.Vertex;
	u32				vBase;

	svertRender*	pv		= (svertRender*)Stream->Lock(SKEL_MAX_FACE_COUNT*3,m_Parent->vs_SkeletonGeom->vb_stride,vBase);
	Fvector			P0,N0,P1,N1;
    
    int f_cnt=0;
    for (IntIt i_it=face_lst.begin(); i_it!=face_lst.end(); i_it++)
    {
        for (int k=0; k<3; k++,pv++)
        {
        	st_SVert& SV 			= m_SVertices[*i_it*3+k];
            pv->uv.set				(SV.uv);
            float total				= SV.bones[0].w;

            const Fmatrix& M		= m_Parent->m_Bones[SV.bones[0].id]->_RenderTransform();
            M.transform_tiny		(pv->P,SV.offs);
            M.transform_dir 		(pv->N,SV.norm);

            Fvector P,N;

            for (u8 cnt=1; cnt<(u8)SV.bones.size(); cnt++)
            {
                total			    += SV.bones[cnt].w;
                const Fmatrix& M     = m_Parent->m_Bones[SV.bones[cnt].id]->_RenderTransform();
                M.transform_tiny    (P,SV.offs);
                M.transform_dir     (N,SV.norm);
                pv->P.lerp		    (pv->P,P,SV.bones[cnt].w/total);
                pv->N.lerp		    (pv->N,N,SV.bones[cnt].w/total);
            }
        }
        f_cnt++;
        if (S->m_Flags.is(CSurface::sf2Sided))
        {
        	pv->P.set((pv-1)->P);	pv->N.invert((pv-1)->N);	pv->uv.set((pv-1)->uv); pv++;
        	pv->P.set((pv-3)->P);	pv->N.invert((pv-3)->N);	pv->uv.set((pv-3)->uv); pv++;
        	pv->P.set((pv-5)->P);	pv->N.invert((pv-5)->N);	pv->uv.set((pv-5)->uv); pv++;
	        f_cnt++;
        }
        if (f_cnt>=SKEL_MAX_FACE_COUNT-1)
        {
            Stream->Unlock		(f_cnt*3,m_Parent->vs_SkeletonGeom->vb_stride);
            EDevice.DP			(D3DPT_TRIANGLELIST,m_Parent->vs_SkeletonGeom,vBase,f_cnt);
			pv					= (svertRender*)Stream->Lock(SKEL_MAX_FACE_COUNT*3,m_Parent->vs_SkeletonGeom->vb_stride,vBase);
            f_cnt				= 0;
        }
    }
	Stream->Unlock				(f_cnt*3,m_Parent->vs_SkeletonGeom->vb_stride);
	if (f_cnt)
    	EDevice.DP		(D3DPT_TRIANGLELIST,m_Parent->vs_SkeletonGeom,vBase,f_cnt);    
}
//----------------------------------------------------


