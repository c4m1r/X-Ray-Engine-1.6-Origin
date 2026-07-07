#include "stdafx.h"
#pragma hdrstop

#include "HW11.h"
#include "EditorShaders11.h"
#include "EditorTextures11.h"
#include "ResourceManager11.h"
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

CHW11 HW11;


static void UpdateBuffer(ID3D11DeviceContext* ctx, ID3D11Buffer* buf,
                          const void* data, u32 size)
{
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, data, size);
        ctx->Unmap(buf, 0);
    }
}

CHW11::CHW11()  = default;
CHW11::~CHW11() = default;

bool CHW11::CreateDevice(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    BackBufferW = std::max<u32>(rc.right  - rc.left, 1u);
    BackBufferH = std::max<u32>(rc.bottom - rc.top,  1u);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount                          = 1;
    scd.BufferDesc.Width                     = BackBufferW;
    scd.BufferDesc.Height                    = BackBufferH;
    scd.BufferDesc.Format                    = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator     = 60;
    scd.BufferDesc.RefreshRate.Denominator   = 1;
    scd.BufferUsage                          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                         = hwnd;
    scd.SampleDesc.Count                     = 1;
    scd.Windowed                             = TRUE;
    scd.SwapEffect                           = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };

    UINT flags = 0;
#ifdef DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        featureLevels, _countof(featureLevels),
        D3D11_SDK_VERSION, &scd,
        &pSwapChain, &pDevice,
        &FeatureLevel, &pContext);

    if (FAILED(hr)) {
        ELog.DlgMsg(mtError, "HW11: D3D11CreateDeviceAndSwapChain failed (0x%X)", hr);
        return false;
    }

    Msg("* DX11 editor device created. Feature level: 0x%X", (u32)FeatureLevel);

    if (!CreateBackBuffer())        return false;
    if (!CreatePrimBuf())           return false;
    if (!CreateSpriteBuf())         return false;
    if (!CreateLODResources(pDevice)) return false;

    States.rs_dirty = States.ds_dirty = States.bs_dirty = true;
    return true;
}

bool CHW11::CreateBackBuffer()
{
    ID3D11Texture2D* pBackBuf = nullptr;
    HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuf);
    if (FAILED(hr)) return false;
    hr = pDevice->CreateRenderTargetView(pBackBuf, nullptr, &pRTV);
    pBackBuf->Release();
    if (FAILED(hr)) return false;

    D3D11_TEXTURE2D_DESC dtd = {};
    dtd.Width             = BackBufferW;
    dtd.Height            = BackBufferH;
    dtd.MipLevels         = 1;
    dtd.ArraySize         = 1;
    dtd.Format            = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dtd.SampleDesc.Count  = 1;
    dtd.BindFlags         = D3D11_BIND_DEPTH_STENCIL;
    hr = pDevice->CreateTexture2D(&dtd, nullptr, &pDepthTex);
    if (FAILED(hr)) return false;
    hr = pDevice->CreateDepthStencilView(pDepthTex, nullptr, &pDSV);
    if (FAILED(hr)) return false;

    pContext->OMSetRenderTargets(1, &pRTV, pDSV);

    D3D11_VIEWPORT vp = {};
    vp.Width    = (float)BackBufferW;
    vp.Height   = (float)BackBufferH;
    vp.MaxDepth = 1.0f;
    pContext->RSSetViewports(1, &vp);

    return true;
}

void CHW11::ReleaseBackBuffer()
{
    pContext->OMSetRenderTargets(0, nullptr, nullptr);
    if (pRTV)      { pRTV->Release();      pRTV      = nullptr; }
    if (pDSV)      { pDSV->Release();      pDSV      = nullptr; }
    if (pDepthTex) { pDepthTex->Release(); pDepthTex = nullptr; }
}

void CHW11::DestroyDevice()
{
    DestroyCullResources();
    ReleaseBackBuffer();
    States.release_states();
    if (inst_buf)     { inst_buf->Release();       inst_buf     = nullptr; inst_buf_cap = 0; }
    if (lod_quad_vb)  { lod_quad_vb->Release();    lod_quad_vb  = nullptr; }
    if (lod_inst_buf) { lod_inst_buf->Release();   lod_inst_buf = nullptr; lod_inst_cap = 0; }
    if (prim_vb)      { prim_vb->Release();        prim_vb      = nullptr; }
    if (mesh_vb)      { mesh_vb->Release();        mesh_vb      = nullptr; mesh_vb_cap = 0; }
    if (mesh_ib)      { mesh_ib->Release();        mesh_ib      = nullptr; mesh_ib_cap = 0; }
    if (part_vb)      { part_vb->Release();        part_vb      = nullptr; part_vb_cap = 0; }
    if (part_ib)      { part_ib->Release();        part_ib      = nullptr; part_ib_quads = 0; }
    if (basetex_vb)   { basetex_vb->Release();     basetex_vb   = nullptr; basetex_vb_cap = 0; }
    ReleaseGrassGeom();
    if (sprite_vb)    { sprite_vb->Release();      sprite_vb    = nullptr; }
    if (pSwapChain)   { pSwapChain->Release();    pSwapChain   = nullptr; }
    if (pContext)     { pContext->Release();       pContext     = nullptr; }
    if (pDevice)      { pDevice->Release();        pDevice      = nullptr; }
}


bool CHW11::UploadInstances(const EditorInstanceData* data, u32 count)
{
    if (!count) return false;

    if (count > inst_buf_cap) {
        u32 new_cap = std::max<u32>(count, inst_buf_cap + inst_buf_cap / 2 + 256);
        if (inst_buf) { inst_buf->Release(); inst_buf = nullptr; }

        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth          = new_cap * sizeof(EditorInstanceData);
        bd.Usage              = D3D11_USAGE_DYNAMIC;
        bd.BindFlags          = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE;
        HRESULT hr = pDevice->CreateBuffer(&bd, nullptr, &inst_buf);
        if (FAILED(hr)) return false;
        inst_buf_cap = new_cap;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    HRESULT hr = pContext->Map(inst_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
    if (FAILED(hr)) return false;
    memcpy(ms.pData, data, count * sizeof(EditorInstanceData));
    pContext->Unmap(inst_buf, 0);
    return true;
}

bool CHW11::CreateLODResources(ID3D11Device* dev)
{
    const float quad[6][2] = {
        {-1.f, 1.f}, { 1.f, 1.f}, {-1.f,-1.f},
        {-1.f,-1.f}, { 1.f, 1.f}, { 1.f,-1.f},
    };
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(quad);
    bd.Usage     = D3D11_USAGE_IMMUTABLE;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd = { quad, 0, 0 };
    return SUCCEEDED(dev->CreateBuffer(&bd, &sd, &lod_quad_vb));
}

bool CHW11::UploadLODInstances(const LodInstanceData* data, u32 count)
{
    if (!count) return false;
    if (count > lod_inst_cap) {
        u32 new_cap = std::max<u32>(count, lod_inst_cap + lod_inst_cap / 2 + 256);
        if (lod_inst_buf) { lod_inst_buf->Release(); lod_inst_buf = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = new_cap * sizeof(LodInstanceData);
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &lod_inst_buf))) return false;
        lod_inst_cap = new_cap;
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(lod_inst_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return false;
    memcpy(ms.pData, data, count * sizeof(LodInstanceData));
    pContext->Unmap(lod_inst_buf, 0);
    return true;
}

void CHW11::ResizeSwapChain(u32 w, u32 h)
{
    if (w == BackBufferW && h == BackBufferH) return;
    BackBufferW = std::max<u32>(w, 1u);
    BackBufferH = std::max<u32>(h, 1u);

    ReleaseBackBuffer();
    pSwapChain->ResizeBuffers(1, BackBufferW, BackBufferH,
                               DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    CreateBackBuffer();
}

bool CHW11::BeginFrame(u32 clear_color_abgr)
{
    float r = ((clear_color_abgr >> 16) & 0xFF) / 255.f;
    float g = ((clear_color_abgr >>  8) & 0xFF) / 255.f;
    float b = ((clear_color_abgr >>  0) & 0xFF) / 255.f;
    float a = ((clear_color_abgr >> 24) & 0xFF) / 255.f;
    float clearColor[4] = {r, g, b, a};
    pContext->ClearRenderTargetView(pRTV, clearColor);
    pContext->ClearDepthStencilView(pDSV,
        D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    return true;
}

void CHW11::EndFrame()
{
    pSwapChain->Present(0, 0);
}

void CHW11::SetRenderState(D3DRENDERSTATETYPE type, u32 value)
{
    switch (type)
    {
    case D3DRS_FILLMODE:
        {
            D3D11_FILL_MODE fm = (value == D3DFILL_WIREFRAME) ? D3D11_FILL_WIREFRAME :
                                  (value == D3DFILL_POINT)     ? D3D11_FILL_WIREFRAME :
                                                                  D3D11_FILL_SOLID;
            if (States.fill_mode != fm) { States.fill_mode = fm; States.rs_dirty = true; }
        }
        break;
    case D3DRS_CULLMODE:
        {
            D3D11_CULL_MODE cm = (value == D3DCULL_NONE) ? D3D11_CULL_NONE :
                                  (value == D3DCULL_CW)   ? D3D11_CULL_FRONT :
                                                             D3D11_CULL_BACK;
            if (States.cull_mode != cm) { States.cull_mode = cm; States.rs_dirty = true; }
        }
        break;
    case D3DRS_ZENABLE:
        if (States.depth_enable != (value != 0)) {
            States.depth_enable = (value != 0); States.ds_dirty = true;
        }
        break;
    case D3DRS_ZWRITEENABLE:
        if (States.depth_write != (value != 0)) {
            States.depth_write = (value != 0); States.ds_dirty = true;
        }
        break;
    case D3DRS_ALPHABLENDENABLE:
        if (States.alpha_blend != (value != 0)) {
            States.alpha_blend = (value != 0); States.bs_dirty = true;
        }
        break;
    case D3DRS_STENCILENABLE:
        if (States.stencil_enable != (value != 0)) {
            States.stencil_enable = (value != 0); States.ds_dirty = true;
        }
        break;
    default: break;
    }
}

void CHW11::SetSamplerState(u32, D3DSAMPLERSTATETYPE, u32)
{
}

void CEditorDX11States::release_states()
{
    for (auto& it : rs_cache) if (it.second) it.second->Release();
    for (auto& it : ds_cache) if (it.second) it.second->Release();
    for (auto& it : bs_cache) if (it.second) it.second->Release();
    rs_cache.clear(); ds_cache.clear(); bs_cache.clear();
}

void CEditorDX11States::apply_rs(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    const u64 key = (u64)fill_mode | ((u64)cull_mode<<3) | ((u64)front_ccw<<6)
                  | ((u64)depth_clip<<7) | ((u64)scissor<<8) | ((u64)(u32)depth_bias<<32);
    ID3D11RasterizerState* rs = nullptr;
    auto it = rs_cache.find(key);
    if (it != rs_cache.end()) rs = it->second;
    else {
        D3D11_RASTERIZER_DESC rd = {};
        rd.FillMode              = fill_mode;
        rd.CullMode              = cull_mode;
        rd.FrontCounterClockwise = front_ccw;
        rd.DepthClipEnable       = depth_clip;
        rd.ScissorEnable         = scissor;
        rd.MultisampleEnable     = FALSE;
        rd.AntialiasedLineEnable = FALSE;
        rd.DepthBias             = depth_bias;
        rd.SlopeScaledDepthBias  = depth_bias ? -1.5f : 0.f;
        rd.DepthBiasClamp        = 0.f;
        dev->CreateRasterizerState(&rd, &rs);
        rs_cache[key] = rs;
    }
    ctx->RSSetState(rs);
    rs_dirty = false;
}

void CEditorDX11States::apply_ds(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    const u64 key = (u64)depth_enable | ((u64)depth_write<<1) | ((u64)depth_func<<2) | ((u64)stencil_enable<<8);
    ID3D11DepthStencilState* dss = nullptr;
    auto it = ds_cache.find(key);
    if (it != ds_cache.end()) dss = it->second;
    else {
        D3D11_DEPTH_STENCIL_DESC dsd = {};
        dsd.DepthEnable    = depth_enable ? TRUE : FALSE;
        dsd.DepthWriteMask = depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
        dsd.DepthFunc      = depth_func;
        dsd.StencilEnable  = stencil_enable ? TRUE : FALSE;
        dsd.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
        dsd.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
        dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        dsd.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
        dsd.BackFace = dsd.FrontFace;
        dev->CreateDepthStencilState(&dsd, &dss);
        ds_cache[key] = dss;
    }
    ctx->OMSetDepthStencilState(dss, 0);
    ds_dirty = false;
}

void CEditorDX11States::apply_bs(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    const u64 key = (u64)alpha_blend | ((u64)src_blend<<1) | ((u64)dst_blend<<8);
    ID3D11BlendState* bs = nullptr;
    auto it = bs_cache.find(key);
    if (it != bs_cache.end()) bs = it->second;
    else {
        D3D11_BLEND_DESC bd = {};
        if (alpha_blend) {
            bd.RenderTarget[0].BlendEnable    = TRUE;
            bd.RenderTarget[0].SrcBlend       = src_blend;
            bd.RenderTarget[0].DestBlend      = dst_blend;
            bd.RenderTarget[0].BlendOp        = D3D11_BLEND_OP_ADD;
            bd.RenderTarget[0].SrcBlendAlpha  = D3D11_BLEND_ONE;
            bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
            bd.RenderTarget[0].BlendOpAlpha   = D3D11_BLEND_OP_ADD;
        }
        bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        dev->CreateBlendState(&bd, &bs);
        bs_cache[key] = bs;
    }
    float bf[4] = {};
    ctx->OMSetBlendState(bs, bf, 0xFFFFFFFF);
    bs_dirty = false;
}

void CHW11::FlushStates()
{
    if (States.rs_dirty) States.apply_rs(pDevice, pContext);
    if (States.ds_dirty) States.apply_ds(pDevice, pContext);
    if (States.bs_dirty) States.apply_bs(pDevice, pContext);
}


bool CHW11::CreatePrimBuf()
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = PRIM_VB_CAP * 16;
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = pDevice->CreateBuffer(&bd, nullptr, &prim_vb);
    if (FAILED(hr)) {
        ELog.DlgMsg(mtError, "HW11: failed to create primitive vertex buffer (0x%X)", hr);
        return false;
    }
    return true;
}

bool CHW11::CreateSpriteBuf()
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = SPRITE_VB_CAP * sizeof(SpriteVert2D);
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = pDevice->CreateBuffer(&bd, nullptr, &sprite_vb);
    if (FAILED(hr)) {
        ELog.DlgMsg(mtError, "HW11: failed to create sprite vertex buffer (0x%X)", hr);
        return false;
    }
    return true;
}

void CHW11::DU_DrawSprite2D(const SpriteVert2D* verts, u32 count,
                              D3D11_PRIMITIVE_TOPOLOGY topo, ID3D11ShaderResourceView* srv)
{
    if (!count || !verts || !sprite_vb) return;
    u32 to_draw = std::min<u32>(count, SPRITE_VB_CAP);

    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(sprite_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    memcpy(ms.pData, verts, to_draw * sizeof(SpriteVert2D));
    pContext->Unmap(sprite_vb, 0);

    EditorShaders11.BindSprite2D(pContext, srv ? srv : Resources11.Textures().Default());

    UINT stride = sizeof(SpriteVert2D), offset = 0;
    pContext->IASetVertexBuffers(0, 1, &sprite_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(topo);

    bool saved_depth = States.depth_enable;
    States.depth_enable = false; States.ds_dirty = true;
    if (States.ds_dirty) States.apply_ds(pDevice, pContext);

    float bf[4] = {};
    pContext->OMSetBlendState(EditorShaders11.bs_additive, bf, 0xFFFFFFFF);

    pContext->Draw(to_draw, 0);

    States.depth_enable = saved_depth; States.ds_dirty = true;
    States.bs_dirty = true;
}

void CHW11::DU_DrawPrim(const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo)
{
    if (!count || !verts || !prim_vb) return;
    u32 to_draw = std::min<u32>(count, PRIM_VB_CAP);

    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(prim_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    memcpy(ms.pData, verts, to_draw * 16);
    pContext->Unmap(prim_vb, 0);

    EditorShaders11.BindPrim3D(pContext);
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    UINT stride = 16, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &prim_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(topo);

    FlushStates();
    pContext->Draw(to_draw, 0);
}

void CHW11::DU_DrawPrim2D(const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo)
{
    if (!count || !verts || !prim_vb) return;
    u32 to_draw = std::min<u32>(count, PRIM_VB_CAP);

    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(prim_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    memcpy(ms.pData, verts, to_draw * 16);
    pContext->Unmap(prim_vb, 0);

    EditorShaders11.BindPrim2D(pContext);

    UINT stride = 16, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &prim_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(topo);

    bool saved_depth = States.depth_enable;
    States.depth_enable = false;
    States.ds_dirty = true;
    FlushStates();
    pContext->Draw(to_draw, 0);

    States.depth_enable = saved_depth;
    States.ds_dirty = true;
}

void CHW11::DrawIndexedSolid(const void* verts, u32 vCount, const u16* idx, u32 idxCount,
                            const float* world4x4, ID3D11ShaderResourceView* srv,
                            float tr, float tg, float tb, float ta)
{
    if (!pDevice || !pContext || !verts || !idx || !vCount || !idxCount) return;

    const u32 vstride = (u32)sizeof(EditorVertex11);

    if (mesh_vb_cap < vCount) {
        if (mesh_vb) { mesh_vb->Release(); mesh_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = vstride * vCount;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &mesh_vb))) return;
        mesh_vb_cap = vCount;
    }
    if (mesh_ib_cap < idxCount) {
        if (mesh_ib) { mesh_ib->Release(); mesh_ib = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = (u32)sizeof(u16) * idxCount;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &mesh_ib))) return;
        mesh_ib_cap = idxCount;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(mesh_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, verts, vstride * vCount);
        pContext->Unmap(mesh_vb, 0);
    }
    if (SUCCEEDED(pContext->Map(mesh_ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, idx, sizeof(u16) * idxCount);
        pContext->Unmap(mesh_ib, 0);
    }

    EditorShaders11.BindSolid(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);
    Resources11.UploadPerObject(world4x4, tr, tg, tb, ta);

    UINT stride = vstride, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &mesh_vb, &stride, &offset);
    pContext->IASetIndexBuffer(mesh_ib, DXGI_FORMAT_R16_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D11_CULL_MODE saved_cull = States.cull_mode;
    States.cull_mode = D3D11_CULL_NONE; States.rs_dirty = true;
    FlushStates();
    pContext->DrawIndexed(idxCount, 0, 0);
    States.cull_mode = saved_cull; States.rs_dirty = true;
}

void CHW11::DrawParticles(const void* verts, u32 vCount, ID3D11ShaderResourceView* srv, int blendMode)
{
    DrawParticles(verts, vCount, srv, blendMode, false);
}

void CHW11::DrawParticles(const void* verts, u32 vCount, ID3D11ShaderResourceView* srv, int blendMode, bool pointSample)
{
    if (!pDevice || !pContext || !verts || vCount < 4) return;

    const u32 vstride = 24;
    const u32 quads   = vCount / 4;
    const u32 idxCount = quads * 6;

    if (part_vb_cap < vCount) {
        if (part_vb) { part_vb->Release(); part_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = vstride * vCount;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &part_vb))) return;
        part_vb_cap = vCount;
    }

    if (part_ib_quads < quads) {
        if (part_ib) { part_ib->Release(); part_ib = nullptr; }
        xr_vector<u32> idx; idx.resize(quads * 6);
        for (u32 q = 0; q < quads; ++q) {
            u32 b = q * 4;
            u32* o = &idx[q * 6];
            o[0] = b + 0; o[1] = b + 1; o[2] = b + 2;
            o[3] = b + 1; o[4] = b + 3; o[5] = b + 2;
        }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (u32)(idx.size() * sizeof(u32));
        bd.Usage     = D3D11_USAGE_IMMUTABLE;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA sd = {}; sd.pSysMem = idx.data();
        if (FAILED(pDevice->CreateBuffer(&bd, &sd, &part_ib))) return;
        part_ib_quads = quads;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(part_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, verts, vstride * vCount);
        pContext->Unmap(part_vb, 0);
    }

    EditorShaders11.BindParticle(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    if (pointSample) EditorShaders11.SetPointSampler(pContext);
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    UINT stride = vstride, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &part_vb, &stride, &offset);
    pContext->IASetIndexBuffer(part_ib, DXGI_FORMAT_R32_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const bool            saved_blend = States.alpha_blend;
    const bool            saved_zw    = States.depth_write;
    const D3D11_CULL_MODE saved_cull  = States.cull_mode;
    const D3D11_BLEND     saved_src   = States.src_blend;
    const D3D11_BLEND     saved_dst   = States.dst_blend;

    bool blend_on = true;
    switch (blendMode) {
        case 0: blend_on = false;                                                                           break;
        case 1: States.src_blend = D3D11_BLEND_SRC_ALPHA;  States.dst_blend = D3D11_BLEND_INV_SRC_ALPHA;    break;
        case 2: States.src_blend = D3D11_BLEND_ONE;        States.dst_blend = D3D11_BLEND_ONE;              break;
        case 3: States.src_blend = D3D11_BLEND_DEST_COLOR; States.dst_blend = D3D11_BLEND_ZERO;             break;
        case 4: States.src_blend = D3D11_BLEND_DEST_COLOR; States.dst_blend = D3D11_BLEND_SRC_COLOR;        break;
        case 5: States.src_blend = D3D11_BLEND_SRC_ALPHA;  States.dst_blend = D3D11_BLEND_ONE;              break;
        default:States.src_blend = D3D11_BLEND_SRC_ALPHA;  States.dst_blend = D3D11_BLEND_ONE;              break;
    }
    States.alpha_blend = blend_on;             States.bs_dirty = true;
    States.depth_write = (blendMode == 0);     States.ds_dirty = true;
    States.cull_mode   = D3D11_CULL_NONE;      States.rs_dirty = true;
    FlushStates();

    pContext->DrawIndexed(idxCount, 0, 0);

    States.alpha_blend = saved_blend; States.src_blend = saved_src; States.dst_blend = saved_dst; States.bs_dirty = true;
    States.depth_write = saved_zw;    States.ds_dirty = true;
    States.cull_mode   = saved_cull;  States.rs_dirty = true;
    FlushStates();
}

void CHW11::DrawBaseTex(const void* verts, u32 vCount, const char* texName, bool blended)
{
    if (!pDevice || !pContext || !verts || !vCount) return;

    const u32 vstride = 20;

    if (basetex_vb_cap < vCount) {
        if (basetex_vb) { basetex_vb->Release(); basetex_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = vstride * vCount; bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &basetex_vb))) return;
        basetex_vb_cap = vCount;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(basetex_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, verts, vstride * vCount); pContext->Unmap(basetex_vb, 0);
    }

    ID3D11ShaderResourceView* srv = EditorTextures11.Get(pDevice, texName);

    EditorShaders11.BindBaseTex(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    Resources11.UploadPerObject(ident, 1.f, 1.f, 1.f, blended ? 0.5f : 1.0f);

    UINT stride = vstride, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &basetex_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const bool            saved_blend = States.alpha_blend;
    const bool            saved_zw    = States.depth_write;
    const D3D11_CULL_MODE saved_cull  = States.cull_mode;
    const D3D11_BLEND     saved_src   = States.src_blend;
    const D3D11_BLEND     saved_dst   = States.dst_blend;
    const int             saved_bias  = States.depth_bias;
    States.alpha_blend = blended;          States.bs_dirty = true;
    if (blended) { States.src_blend = D3D11_BLEND_SRC_ALPHA; States.dst_blend = D3D11_BLEND_INV_SRC_ALPHA; }
    States.depth_write = false;            States.ds_dirty = true;
    States.cull_mode   = D3D11_CULL_NONE;  States.rs_dirty = true;
    States.depth_bias  = -8000;            States.rs_dirty = true;
    FlushStates();

    pContext->Draw(vCount, 0);

    States.alpha_blend = saved_blend; States.src_blend = saved_src; States.dst_blend = saved_dst; States.bs_dirty = true;
    States.depth_write = saved_zw;    States.ds_dirty = true;
    States.cull_mode   = saved_cull;  States.rs_dirty = true;
    States.depth_bias  = saved_bias;  States.rs_dirty = true;
    FlushStates();
}

void CHW11::DrawMeshTex(const void* verts, u32 vCount, const char* texName, bool cull_back)
{
    if (!pDevice || !pContext || !verts || vCount < 3) return;

    const u32 vstride = 20;

    if (basetex_vb_cap < vCount) {
        if (basetex_vb) { basetex_vb->Release(); basetex_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = vstride * vCount; bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &basetex_vb))) return;
        basetex_vb_cap = vCount;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(basetex_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, verts, vstride * vCount); pContext->Unmap(basetex_vb, 0);
    }

    ID3D11ShaderResourceView* srv = EditorTextures11.Get(pDevice, texName);

    EditorShaders11.BindBaseTex(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    Resources11.UploadPerObject(ident, 1.f, 1.f, 1.f, 1.0f);

    UINT stride = vstride, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &basetex_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const bool            saved_blend = States.alpha_blend;
    const bool            saved_zw    = States.depth_write;
    const D3D11_CULL_MODE saved_cull  = States.cull_mode;
    States.alpha_blend = false;            States.bs_dirty = true;
    States.depth_write = true;             States.ds_dirty = true;
    States.cull_mode   = cull_back ? D3D11_CULL_BACK : D3D11_CULL_NONE;  States.rs_dirty = true;
    FlushStates();

    pContext->Draw(vCount, 0);

    States.alpha_blend = saved_blend; States.bs_dirty = true;
    States.depth_write = saved_zw;    States.ds_dirty = true;
    States.cull_mode   = saved_cull;  States.rs_dirty = true;
    FlushStates();
}

void CHW11::DrawWallmark(const void* verts, u32 vCount, const char* texName, int blendMode)
{
    if (!pDevice || !pContext || !verts || vCount < 3) return;

    const u32 vstride = 24;

    if (part_vb_cap < vCount) {
        if (part_vb) { part_vb->Release(); part_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = vstride * vCount;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &part_vb))) return;
        part_vb_cap = vCount;
    }

    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(part_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, verts, vstride * vCount);
        pContext->Unmap(part_vb, 0);
    }

    ID3D11ShaderResourceView* srv = EditorTextures11.Get(pDevice, texName);

    EditorShaders11.BindParticle(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    UINT stride = vstride, offset = 0;
    pContext->IASetVertexBuffers(0, 1, &part_vb, &stride, &offset);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const bool            saved_blend = States.alpha_blend;
    const bool            saved_zw    = States.depth_write;
    const D3D11_CULL_MODE saved_cull  = States.cull_mode;
    const D3D11_BLEND     saved_src   = States.src_blend;
    const D3D11_BLEND     saved_dst   = States.dst_blend;
    const int             saved_bias  = States.depth_bias;

    switch (blendMode) {
        case 4:  States.src_blend = D3D11_BLEND_DEST_COLOR; States.dst_blend = D3D11_BLEND_SRC_COLOR;     break;
        case 1:
        default: States.src_blend = D3D11_BLEND_SRC_ALPHA;  States.dst_blend = D3D11_BLEND_INV_SRC_ALPHA; break;
    }
    States.alpha_blend = true;             States.bs_dirty = true;
    States.depth_write = false;            States.ds_dirty = true;
    States.cull_mode   = D3D11_CULL_NONE;  States.rs_dirty = true;
    States.depth_bias  = -8000;            States.rs_dirty = true;
    FlushStates();

    pContext->Draw(vCount, 0);

    States.alpha_blend = saved_blend; States.src_blend = saved_src; States.dst_blend = saved_dst; States.bs_dirty = true;
    States.depth_write = saved_zw;    States.ds_dirty = true;
    States.cull_mode   = saved_cull;  States.rs_dirty = true;
    States.depth_bias  = saved_bias;  States.rs_dirty = true;
    FlushStates();
}

bool CHW11::ScreenshotBegin(u32 width, u32 height, u32 clear_color_abgr)
{
    if (!pDevice || !pContext || !width || !height) return false;

    if (ss_rtv)   { ss_rtv->Release();   ss_rtv = nullptr; }
    if (ss_rt)    { ss_rt->Release();    ss_rt = nullptr; }
    if (ss_dsv)   { ss_dsv->Release();   ss_dsv = nullptr; }
    if (ss_depth) { ss_depth->Release(); ss_depth = nullptr; }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width; td.Height = height; td.MipLevels = 1; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (FAILED(pDevice->CreateTexture2D(&td, nullptr, &ss_rt)))            return false;
    if (FAILED(pDevice->CreateRenderTargetView(ss_rt, nullptr, &ss_rtv)))  return false;

    D3D11_TEXTURE2D_DESC dd = {};
    dd.Width = width; dd.Height = height; dd.MipLevels = 1; dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dd.SampleDesc.Count = 1;
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (FAILED(pDevice->CreateTexture2D(&dd, nullptr, &ss_depth)))         return false;
    if (FAILED(pDevice->CreateDepthStencilView(ss_depth, nullptr, &ss_dsv))) return false;

    ss_prev_rtv = nullptr; ss_prev_dsv = nullptr;
    pContext->OMGetRenderTargets(1, &ss_prev_rtv, &ss_prev_dsv);
    UINT nvp = 1; ss_prev_vp = {}; pContext->RSGetViewports(&nvp, &ss_prev_vp);

    pContext->OMSetRenderTargets(1, &ss_rtv, ss_dsv);
    D3D11_VIEWPORT vp = {}; vp.Width = (float)width; vp.Height = (float)height; vp.MinDepth = 0.f; vp.MaxDepth = 1.f;
    pContext->RSSetViewports(1, &vp);

    const float clr[4] = {
        ((clear_color_abgr >> 16) & 0xFF) / 255.f,
        ((clear_color_abgr >>  8) & 0xFF) / 255.f,
        ((clear_color_abgr >>  0) & 0xFF) / 255.f,
        ((clear_color_abgr >> 24) & 0xFF) / 255.f };
    pContext->ClearRenderTargetView(ss_rtv, clr);
    pContext->ClearDepthStencilView(ss_dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
    return true;
}

bool CHW11::ScreenshotEnd(xr_vector<u32>& pixels, u32 width, u32 height)
{
    bool ok = false;
    if (pDevice && pContext && ss_rt && width && height)
    {
        D3D11_TEXTURE2D_DESC sd = {};
        sd.Width = width; sd.Height = height; sd.MipLevels = 1; sd.ArraySize = 1;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.Usage = D3D11_USAGE_STAGING;
        sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* staging = nullptr;
        if (SUCCEEDED(pDevice->CreateTexture2D(&sd, nullptr, &staging)))
        {
            pContext->CopyResource(staging, ss_rt);
            D3D11_MAPPED_SUBRESOURCE ms;
            if (SUCCEEDED(pContext->Map(staging, 0, D3D11_MAP_READ, 0, &ms)))
            {
                pixels.resize(width * height);
                const u8* base = (const u8*)ms.pData;
                for (u32 y = 0; y < height; ++y)
                {
                    const u32* row = (const u32*)(base + (size_t)ms.RowPitch * (height - 1 - y));
                    CopyMemory(&pixels[(size_t)y * width], row, sizeof(u32) * width);
                }
                pContext->Unmap(staging, 0);
                ok = true;
            }
            staging->Release();
        }
    }

    if (pContext)
    {
        ID3D11RenderTargetView* rtv = ss_prev_rtv ? ss_prev_rtv : pRTV;
        ID3D11DepthStencilView* dsv = ss_prev_dsv ? ss_prev_dsv : pDSV;
        pContext->OMSetRenderTargets(1, &rtv, dsv);
        if (ss_prev_vp.Width > 0.f) pContext->RSSetViewports(1, &ss_prev_vp);
    }
    if (ss_prev_rtv) { ss_prev_rtv->Release(); ss_prev_rtv = nullptr; }
    if (ss_prev_dsv) { ss_prev_dsv->Release(); ss_prev_dsv = nullptr; }

    if (ss_rtv)   { ss_rtv->Release();   ss_rtv = nullptr; }
    if (ss_rt)    { ss_rt->Release();    ss_rt = nullptr; }
    if (ss_dsv)   { ss_dsv->Release();   ss_dsv = nullptr; }
    if (ss_depth) { ss_depth->Release(); ss_depth = nullptr; }
    return ok;
}

void CHW11::ReleaseGrassGeom()
{
    for (auto& it : grass_geom) {
        if (it.second.vb) it.second.vb->Release();
        if (it.second.ib) it.second.ib->Release();
    }
    grass_geom.clear();
    if (grass_inst_vb) { grass_inst_vb->Release(); grass_inst_vb = nullptr; grass_inst_cap = 0; }
}

void CHW11::UploadGrassInstances(const float* instMat, u32 instCount)
{
    if (!pDevice || !pContext || !instMat || !instCount) return;
    if (grass_inst_cap < instCount) {
        if (grass_inst_vb) { grass_inst_vb->Release(); grass_inst_vb = nullptr; }
        D3D11_BUFFER_DESC bd = {}; bd.ByteWidth = 64 * instCount; bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(pDevice->CreateBuffer(&bd, nullptr, &grass_inst_vb))) return;
        grass_inst_cap = instCount;
    }
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(pContext->Map(grass_inst_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, instMat, 64 * instCount); pContext->Unmap(grass_inst_vb, 0);
    }
}

void CHW11::DrawGrassModel(const void* key, const void* mverts, u32 mvCount,
                           const u16* midx, u32 miCount,
                           u32 startInstance, u32 instCount, ID3D11ShaderResourceView* srv)
{
    if (!pDevice || !pContext || !mverts || !midx || !mvCount || !miCount || !instCount || !grass_inst_vb) return;

    GrassGeom g;
    auto it = grass_geom.find(key);
    if (it != grass_geom.end() && it->second.vcount == mvCount && it->second.icount == miCount) {
        g = it->second;
    } else {
        if (it != grass_geom.end()) {
            if (it->second.vb) it->second.vb->Release();
            if (it->second.ib) it->second.ib->Release();
        }
        g = GrassGeom{};
        D3D11_BUFFER_DESC vbd = {}; vbd.ByteWidth = 20 * mvCount; vbd.Usage = D3D11_USAGE_IMMUTABLE; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vsd = {}; vsd.pSysMem = mverts;
        if (FAILED(pDevice->CreateBuffer(&vbd, &vsd, &g.vb))) return;
        D3D11_BUFFER_DESC ibd = {}; ibd.ByteWidth = (u32)sizeof(u16) * miCount; ibd.Usage = D3D11_USAGE_IMMUTABLE; ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA isd = {}; isd.pSysMem = midx;
        if (FAILED(pDevice->CreateBuffer(&ibd, &isd, &g.ib))) { g.vb->Release(); return; }
        g.vcount = mvCount; g.icount = miCount;
        grass_geom[key] = g;
    }

    EditorShaders11.BindGrassInstanced(pContext);
    EditorShaders11.SetTexture(pContext, srv ? srv : Resources11.Textures().Default());
    pContext->VSSetConstantBuffers(0, 1, &Resources11.cb_PerFrame);

    ID3D11Buffer* vbs[2] = { g.vb, grass_inst_vb };
    UINT strides[2] = { 20, 64 }, offsets[2] = { 0, 0 };
    pContext->IASetVertexBuffers(0, 2, vbs, strides, offsets);
    pContext->IASetIndexBuffer(g.ib, DXGI_FORMAT_R16_UINT, 0);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    const bool            saved_blend = States.alpha_blend;
    const bool            saved_zw    = States.depth_write;
    const D3D11_CULL_MODE saved_cull  = States.cull_mode;
    States.alpha_blend = false;            States.bs_dirty = true;
    States.depth_write = true;             States.ds_dirty = true;
    States.cull_mode   = D3D11_CULL_NONE;  States.rs_dirty = true;
    FlushStates();

    pContext->DrawIndexedInstanced(g.icount, instCount, 0, 0, startInstance);

    States.alpha_blend = saved_blend; States.bs_dirty = true;
    States.depth_write = saved_zw;    States.ds_dirty = true;
    States.cull_mode   = saved_cull;  States.rs_dirty = true;
    FlushStates();
}


bool CHW11::CreateCullResources(ID3D11Device* dev)
{
    HRESULT hr;

    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth           = MAX_CULL_INSTS * (u32)sizeof(GpuAabb);
        bd.Usage               = D3D11_USAGE_DYNAMIC;
        bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
        bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
        bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.StructureByteStride = (u32)sizeof(GpuAabb);
        hr = dev->CreateBuffer(&bd, nullptr, &cull_aabb_buf);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format               = DXGI_FORMAT_UNKNOWN;
        sd.ViewDimension        = D3D11_SRV_DIMENSION_BUFFER;
        sd.Buffer.FirstElement  = 0;
        sd.Buffer.NumElements   = MAX_CULL_INSTS;
        hr = dev->CreateShaderResourceView(cull_aabb_buf, &sd, &cull_aabb_srv);
        if (FAILED(hr)) return false;
    }

    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth  = MAX_CULL_INSTS * (u32)sizeof(u32);
        bd.Usage      = D3D11_USAGE_DEFAULT;
        bd.BindFlags  = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        hr = dev->CreateBuffer(&bd, nullptr, &cull_vis_buf);
        if (FAILED(hr)) return false;

        D3D11_UNORDERED_ACCESS_VIEW_DESC ud = {};
        ud.Format              = DXGI_FORMAT_R32_UINT;
        ud.ViewDimension       = D3D11_UAV_DIMENSION_BUFFER;
        ud.Buffer.FirstElement = 0;
        ud.Buffer.NumElements  = MAX_CULL_INSTS;
        hr = dev->CreateUnorderedAccessView(cull_vis_buf, &ud, &cull_vis_uav);
        if (FAILED(hr)) return false;

        D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
        sd.Format               = DXGI_FORMAT_R32_UINT;
        sd.ViewDimension        = D3D11_SRV_DIMENSION_BUFFER;
        sd.Buffer.FirstElement  = 0;
        sd.Buffer.NumElements   = MAX_CULL_INSTS;
        hr = dev->CreateShaderResourceView(cull_vis_buf, &sd, &cull_vis_srv);
        if (FAILED(hr)) return false;
    }

    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = 112;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = dev->CreateBuffer(&bd, nullptr, &cull_planes_cb);
        if (FAILED(hr)) return false;
    }

    {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = 16;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        hr = dev->CreateBuffer(&bd, nullptr, &cull_offset_cb);
        if (FAILED(hr)) return false;
    }

    return true;
}

void CHW11::DestroyCullResources()
{
    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    rel(cs_cull);
    rel(cull_aabb_srv); rel(cull_aabb_buf);
    rel(cull_vis_uav); rel(cull_vis_srv); rel(cull_vis_buf);
    rel(cull_planes_cb); rel(cull_offset_cb);
}

bool CHW11::UploadCullAabbs(const GpuAabb* aabbs, u32 count)
{
    if (!cull_aabb_buf || !count) return false;
    u32 upload = (count < MAX_CULL_INSTS) ? count : MAX_CULL_INSTS;
    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(cull_aabb_buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return false;
    memcpy(ms.pData, aabbs, upload * sizeof(GpuAabb));
    pContext->Unmap(cull_aabb_buf, 0);
    return true;
}

bool CHW11::DispatchFrustumCull(u32 count, const float planes[][4], int plane_count)
{
    if (!cs_cull || !cull_aabb_srv || !cull_vis_uav || !cull_planes_cb || !count) return false;
    if (count > MAX_CULL_INSTS) count = MAX_CULL_INSTS;

    struct CullCB { float planes[6][4]; u32 count; u32 _pad[3]; };
    CullCB cb = {};
    int n = (plane_count < 6) ? plane_count : 6;
    for (int p = 0; p < n; p++) {
        cb.planes[p][0] = planes[p][0];
        cb.planes[p][1] = planes[p][1];
        cb.planes[p][2] = planes[p][2];
        cb.planes[p][3] = planes[p][3];
    }
    cb.count = count;
    UpdateBuffer(pContext, cull_planes_cb, &cb, sizeof(cb));

    pContext->CSSetShader(cs_cull, nullptr, 0);
    pContext->CSSetShaderResources(0, 1, &cull_aabb_srv);
    pContext->CSSetConstantBuffers(1, 1, &cull_planes_cb);
    pContext->CSSetUnorderedAccessViews(0, 1, &cull_vis_uav, nullptr);
    u32 groups = (count + 63u) / 64u;
    pContext->Dispatch(groups, 1, 1);

    ID3D11UnorderedAccessView* null_uav = nullptr;
    pContext->CSSetUnorderedAccessViews(0, 1, &null_uav, nullptr);
    ID3D11ShaderResourceView* null_srv = nullptr;
    pContext->CSSetShaderResources(0, 1, &null_srv);
    pContext->CSSetShader(nullptr, nullptr, 0);

    pContext->VSSetShaderResources(16, 1, &cull_vis_srv);
    pContext->VSSetConstantBuffers(2, 1, &cull_offset_cb);
    return true;
}

void CHW11::SetInstOffset(u32 start_inst, bool use_cull)
{
    if (!cull_offset_cb) return;
    struct { u32 inst_start; u32 use_cull; u32 _pad[2]; } data = { start_inst, use_cull ? 1u : 0u };
    UpdateBuffer(pContext, cull_offset_cb, &data, sizeof(data));
}

void CHW11::EndCull()
{
    ID3D11ShaderResourceView* null_srv = nullptr;
    pContext->VSSetShaderResources(16, 1, &null_srv);
}
