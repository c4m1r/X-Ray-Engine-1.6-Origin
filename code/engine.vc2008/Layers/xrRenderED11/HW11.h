#pragma once
// Editor-specific DX11 hardware device.
// Lives alongside the DX9 CHW; selected at runtime based on EditorPreferences::render_api.
// Does NOT depend on USE_DX11 define — always compiles as DX11.

#include "EditorTypes11.h"

// Need D3DRENDERSTATETYPE and D3DSAMPLERSTATETYPE for the SetRenderState/SetSamplerState API.
// d3d9.h has its own include guards so including it here is safe even if stdafx.h included it first.
#include <d3d9.h>

//------------------------------------------------------------------
// Render-state shadow for DX11 pipeline state mapping
//------------------------------------------------------------------
struct CEditorDX11States
{
    // rasterizer
    D3D11_FILL_MODE     fill_mode   = D3D11_FILL_SOLID;
    D3D11_CULL_MODE     cull_mode   = D3D11_CULL_BACK;
    bool                front_ccw   = false;
    bool                depth_clip  = true;
    bool                scissor     = false;

    // depth-stencil
    bool                depth_enable    = true;
    bool                depth_write     = true;
    D3D11_COMPARISON_FUNC depth_func    = D3D11_COMPARISON_LESS_EQUAL;
    bool                stencil_enable  = false;

    // blend (single RT)
    bool                alpha_blend     = false;
    bool                alpha_test      = false;
    // configurable color blend factors (used when alpha_blend); default = classic src-alpha blend
    D3D11_BLEND         src_blend       = D3D11_BLEND_SRC_ALPHA;
    D3D11_BLEND         dst_blend       = D3D11_BLEND_INV_SRC_ALPHA;

    // dirty flags
    bool                rs_dirty    = true;
    bool                ds_dirty    = true;
    bool                bs_dirty    = true;

    void apply_rs(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void apply_ds(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void apply_bs(ID3D11Device* dev, ID3D11DeviceContext* ctx);
};

//------------------------------------------------------------------
// Per-frame constant buffers
//------------------------------------------------------------------
#pragma pack(push, 16)
struct CEditorCB_PerFrame
{
    float   view[16];
    float   proj[16];
    float   viewproj[16];
    float   cam_pos[4];
};

struct CEditorCB_PerObject
{
    float   world[16];
    float   color[4];     // {r,g,b,selection_blend}
};
#pragma pack(pop)

//------------------------------------------------------------------
class ECORE_API CHW11
{
public:
    CHW11();
    ~CHW11();

    bool CreateDevice(HWND hwnd);
    void DestroyDevice();

    void ResizeSwapChain(u32 w, u32 h);

    // returns false → device lost (not applicable in DX11, always true after init)
    bool BeginFrame(u32 clear_color_abgr);
    void EndFrame();

    // D3D9-style state wrappers (internally mapped to DX11 pipeline states)
    void SetRenderState(D3DRENDERSTATETYPE type, u32 value);
    void SetSamplerState(u32 sampler, D3DSAMPLERSTATETYPE type, u32 value);

    // flush accumulated render/depth/blend states to pipeline
    void FlushStates();

    // upload per-frame data (call once per frame after view/proj are known)
    void UploadPerFrame(const float* view4x4, const float* proj4x4, const float* cam_pos3);
    // upload per-object world matrix and selection tint
    void UploadPerObject(const float* world4x4, float sel_r, float sel_g, float sel_b, float sel_a);

    ID3D11Device*           pDevice     = nullptr;
    ID3D11DeviceContext*    pContext    = nullptr;
    IDXGISwapChain*         pSwapChain  = nullptr;
    ID3D11RenderTargetView* pRTV        = nullptr;
    ID3D11DepthStencilView* pDSV        = nullptr;
    ID3D11Texture2D*        pDepthTex   = nullptr;

    D3D_FEATURE_LEVEL       FeatureLevel;

    u32 BackBufferW = 0;
    u32 BackBufferH = 0;

    // constant buffers
    ID3D11Buffer*   cb_PerFrame  = nullptr;
    ID3D11Buffer*   cb_PerObject = nullptr;

    CEditorDX11States   States;

    // Per-frame instance buffer for hardware instancing (world matrix + color per object)
    ID3D11Buffer*   inst_buf        = nullptr;
    u32             inst_buf_cap    = 0;      // current capacity in instances

    // LOD billboard resources: shared quad (6 verts) + per-frame instance buffer.
    ID3D11Buffer*   lod_quad_vb     = nullptr; // immutable: 6 corner float2 (2 triangles)
    ID3D11Buffer*   lod_inst_buf    = nullptr; // dynamic: LodInstanceData[]
    u32             lod_inst_cap    = 0;
    bool CreateLODResources(ID3D11Device* dev);
    bool UploadLODInstances(const struct LodInstanceData* data, u32 count);

    // 1×1 white placeholder texture — bound when no real texture is available.
    ID3D11ShaderResourceView*  pDefaultSRV = nullptr;

    // Editor: reusable dynamic buffers to draw an indexed solid mesh — used to render
    // engine visuals (e.g. CPU-skinned spawn/actor models) in the DX11 editor.
    ID3D11Buffer*   mesh_vb         = nullptr;
    u32             mesh_vb_cap     = 0;   // capacity in EditorVertex11
    ID3D11Buffer*   mesh_ib         = nullptr;
    u32             mesh_ib_cap     = 0;   // capacity in u16 indices
    // Draw an indexed triangle list (EditorVertex11 = pos+normal+uv, world-space verts
    // expressed in model space) with the solid shader and a row-major world matrix.
    // tint = flat ObjectColor (rgb,a-blend) applied by ps_solid.
    void DrawIndexedSolid(const void* verts, u32 vCount, const u16* idx, u32 idxCount,
                          const float* world4x4, ID3D11ShaderResourceView* srv,
                          float tr, float tg, float tb, float ta);

    // Editor: reusable dynamic buffers to draw particle billboard quads (FVF::LIT,
    // world-space verts built on CPU). IB holds a fixed quad pattern, grown on demand.
    ID3D11Buffer*   part_vb         = nullptr;
    u32             part_vb_cap     = 0;   // capacity in FVF::LIT vertices
    ID3D11Buffer*   part_ib         = nullptr;
    u32             part_ib_quads   = 0;   // quad capacity (each quad = 4 verts / 6 indices)
    // Draw particle quads: verts = FVF::LIT (pos+color+uv), vCount must be a multiple of 4.
    // blendMode matches CBlender_Particle: 0=SET 1=BLEND 2=ADD 3=MUL 4=MUL_2X 5=ALPHA-ADD.
    void DrawParticles(const void* verts, u32 vCount, ID3D11ShaderResourceView* srv, int blendMode);

    // Dynamic vertex buffer for immediate-mode primitive drawing (FVF::L = float3 pos + u32 color)
    ID3D11Buffer*   prim_vb         = nullptr;
    static const u32 PRIM_VB_CAP   = 65536; // vertices

    // Dynamic vertex buffer for textured 2D sprites (SpriteVert2D = float2 pos + float2 uv + u32 color)
    ID3D11Buffer*   sprite_vb       = nullptr;
    static const u32 SPRITE_VB_CAP = 4096;  // vertices

    // Draw FVF::L vertices in 3D world-space (transformed by ViewProj, no World matrix)
    void DU_DrawPrim  (const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo);
    // Draw FVF::L vertices in 2D: pos.xy must be in NDC [-1,1]
    void DU_DrawPrim2D(const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo);
    // Draw SpriteVert2D vertices: NDC pos + UV, textured with srv; alpha-blended, no depth test
    void DU_DrawSprite2D(const SpriteVert2D* verts, u32 count,
                         D3D11_PRIMITIVE_TOPOLOGY topo, ID3D11ShaderResourceView* srv);

    // Upload instance data for this frame; resizes buffer if needed.
    // Returns false if upload fails.
    bool UploadInstances(const EditorInstanceData* data, u32 count);

    // GPU frustum culling via Compute Shader.
    // CS tests each AABB against 6 frustum planes and writes a visibility uint per instance.
    // VS reads visibility flag: if 0, outputs a degenerate vertex → rasterizer discards triangle.
    static const u32 MAX_CULL_INSTS = 16384;

    ID3D11ComputeShader*       cs_cull       = nullptr;  // compiled from cs_frustum_cull.hlsl
    ID3D11Buffer*              cull_aabb_buf = nullptr;  // DYNAMIC structured buffer: GpuAabb[MAX_CULL_INSTS]
    ID3D11ShaderResourceView*  cull_aabb_srv = nullptr;  // SRV for CS (t0)
    ID3D11Buffer*              cull_vis_buf  = nullptr;  // DEFAULT RWBuffer<uint>[MAX_CULL_INSTS]
    ID3D11UnorderedAccessView* cull_vis_uav  = nullptr;  // UAV for CS (u0)
    ID3D11ShaderResourceView*  cull_vis_srv  = nullptr;  // SRV for VS (t16)
    ID3D11Buffer*              cull_planes_cb = nullptr; // cbuffer b1: planes[6] + count
    ID3D11Buffer*              cull_offset_cb = nullptr; // cbuffer b2: inst_start + use_cull per draw

    bool CreateCullResources(ID3D11Device* dev);
    void DestroyCullResources();
    bool UploadCullAabbs(const GpuAabb* aabbs, u32 count);
    // Returns true if culling was dispatched and VS cull SRV is bound.
    bool DispatchFrustumCull(u32 count, const float planes[][4], int plane_count);
    // Updates per-draw cbuffer b2 (inst_start + use_cull) and binds it to VS.
    void SetInstOffset(u32 start_inst, bool use_cull);
    // Unbinds visibility SRV from VS t16 after draw loop.
    void EndCull();

private:
    bool CreateBackBuffer();
    void ReleaseBackBuffer();
    bool CreateConstantBuffers();
    bool CreateDefaultTexture();
    bool CreatePrimBuf();
    bool CreateSpriteBuf();
};

extern ECORE_API CHW11   HW11;
