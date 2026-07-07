#pragma once

#include "EditorTypes11.h"

#include <d3d9.h>

struct CEditorDX11States
{
    D3D11_FILL_MODE     fill_mode   = D3D11_FILL_SOLID;
    D3D11_CULL_MODE     cull_mode   = D3D11_CULL_BACK;
    bool                front_ccw   = false;
    bool                depth_clip  = true;
    bool                scissor     = false;
    int                 depth_bias  = 0;

    bool                depth_enable    = true;
    bool                depth_write     = true;
    D3D11_COMPARISON_FUNC depth_func    = D3D11_COMPARISON_LESS_EQUAL;
    bool                stencil_enable  = false;

    bool                alpha_blend       = false;
    bool                alpha_test        = false;
    D3D11_BLEND         src_blend       = D3D11_BLEND_SRC_ALPHA;
    D3D11_BLEND         dst_blend       = D3D11_BLEND_INV_SRC_ALPHA;

    bool                rs_dirty    = true;
    bool                ds_dirty    = true;
    bool                bs_dirty    = true;

    xr_map<u64, ID3D11RasterizerState*>   rs_cache;
    xr_map<u64, ID3D11DepthStencilState*> ds_cache;
    xr_map<u64, ID3D11BlendState*>        bs_cache;
    void release_states();

    void apply_rs(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void apply_ds(ID3D11Device* dev, ID3D11DeviceContext* ctx);
    void apply_bs(ID3D11Device* dev, ID3D11DeviceContext* ctx);
};

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
    float   color[4];
};
#pragma pack(pop)

class ECORE_API CHW11
{
public:
    CHW11();
    ~CHW11();

    bool CreateDevice(HWND hwnd);
    void DestroyDevice();

    void ResizeSwapChain(u32 w, u32 h);

    bool BeginFrame(u32 clear_color_abgr);
    void EndFrame();

    void SetRenderState(D3DRENDERSTATETYPE type, u32 value);
    void SetSamplerState(u32 sampler, D3DSAMPLERSTATETYPE type, u32 value);

    void FlushStates();


    ID3D11Device*           pDevice     = nullptr;
    ID3D11DeviceContext*    pContext    = nullptr;
    IDXGISwapChain*         pSwapChain  = nullptr;
    ID3D11RenderTargetView* pRTV        = nullptr;
    ID3D11DepthStencilView* pDSV        = nullptr;
    ID3D11Texture2D*        pDepthTex   = nullptr;

    D3D_FEATURE_LEVEL       FeatureLevel;

    u32 BackBufferW = 0;
    u32 BackBufferH = 0;

    CEditorDX11States   States;

    ID3D11Buffer*   inst_buf        = nullptr;
    u32             inst_buf_cap    = 0;

    ID3D11Buffer*   lod_quad_vb     = nullptr;
    ID3D11Buffer*   lod_inst_buf    = nullptr;
    u32             lod_inst_cap    = 0;
    bool CreateLODResources(ID3D11Device* dev);
    bool UploadLODInstances(const struct LodInstanceData* data, u32 count);


    ID3D11Buffer*   mesh_vb         = nullptr;
    u32             mesh_vb_cap     = 0;
    ID3D11Buffer*   mesh_ib         = nullptr;
    u32             mesh_ib_cap     = 0;
    void DrawIndexedSolid(const void* verts, u32 vCount, const u16* idx, u32 idxCount,
                          const float* world4x4, ID3D11ShaderResourceView* srv,
                          float tr, float tg, float tb, float ta);

    ID3D11Buffer*   part_vb         = nullptr;
    u32             part_vb_cap     = 0;
    ID3D11Buffer*   part_ib         = nullptr;
    u32             part_ib_quads   = 0;
    void DrawParticles(const void* verts, u32 vCount, ID3D11ShaderResourceView* srv, int blendMode);
    void DrawParticles(const void* verts, u32 vCount, ID3D11ShaderResourceView* srv, int blendMode, bool pointSample);

    ID3D11Buffer*   basetex_vb      = nullptr;
    u32             basetex_vb_cap  = 0;
    void DrawBaseTex(const void* verts, u32 vCount, const char* texName, bool blended);

    void DrawMeshTex(const void* verts, u32 vCount, const char* texName, bool cull_back);

    void DrawWallmark(const void* verts, u32 vCount, const char* texName, int blendMode);

    ID3D11Texture2D*        ss_rt       = nullptr;
    ID3D11RenderTargetView* ss_rtv      = nullptr;
    ID3D11Texture2D*        ss_depth    = nullptr;
    ID3D11DepthStencilView* ss_dsv      = nullptr;
    ID3D11RenderTargetView* ss_prev_rtv = nullptr;
    ID3D11DepthStencilView* ss_prev_dsv = nullptr;
    D3D11_VIEWPORT          ss_prev_vp  = {};
    bool ScreenshotBegin(u32 width, u32 height, u32 clear_color_abgr);
    bool ScreenshotEnd  (xr_vector<u32>& pixels, u32 width, u32 height);

    struct GrassGeom { ID3D11Buffer* vb = nullptr; ID3D11Buffer* ib = nullptr; u32 vcount = 0; u32 icount = 0; };
    xr_map<const void*, GrassGeom> grass_geom;
    ID3D11Buffer*   grass_inst_vb   = nullptr;
    u32             grass_inst_cap  = 0;
    void ReleaseGrassGeom();

    void UploadGrassInstances(const float* instMat, u32 instCount);
    void DrawGrassModel(const void* key, const void* mverts, u32 mvCount,
                        const u16* midx, u32 miCount,
                        u32 startInstance, u32 instCount, ID3D11ShaderResourceView* srv);

    ID3D11Buffer*   prim_vb         = nullptr;
    static const u32 PRIM_VB_CAP   = 65536;

    ID3D11Buffer*   sprite_vb       = nullptr;
    static const u32 SPRITE_VB_CAP = 4096;

    void DU_DrawPrim  (const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo);
    void DU_DrawPrim2D(const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo);
    void DU_DrawSprite2D(const SpriteVert2D* verts, u32 count,
                         D3D11_PRIMITIVE_TOPOLOGY topo, ID3D11ShaderResourceView* srv);

    bool UploadInstances(const EditorInstanceData* data, u32 count);

    static const u32 MAX_CULL_INSTS = 16384;

    ID3D11ComputeShader*       cs_cull       = nullptr;
    ID3D11Buffer*              cull_aabb_buf = nullptr;
    ID3D11ShaderResourceView*  cull_aabb_srv = nullptr;
    ID3D11Buffer*              cull_vis_buf  = nullptr;
    ID3D11UnorderedAccessView* cull_vis_uav  = nullptr;
    ID3D11ShaderResourceView*  cull_vis_srv  = nullptr;
    ID3D11Buffer*              cull_planes_cb = nullptr;
    ID3D11Buffer*              cull_offset_cb = nullptr;

    bool CreateCullResources(ID3D11Device* dev);
    void DestroyCullResources();
    bool UploadCullAabbs(const GpuAabb* aabbs, u32 count);
    bool DispatchFrustumCull(u32 count, const float planes[][4], int plane_count);
    void SetInstOffset(u32 start_inst, bool use_cull);
    void EndCull();

private:
    bool CreateBackBuffer();
    void ReleaseBackBuffer();
    bool CreatePrimBuf();
    bool CreateSpriteBuf();
};

extern ECORE_API CHW11   HW11;
