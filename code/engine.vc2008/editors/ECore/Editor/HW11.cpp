#include "stdafx.h"
#pragma hdrstop

#include "HW11.h"
#include "EditorShaders11.h"
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using namespace DirectX;

CHW11 HW11;

//------------------------------------------------------------------
// helpers
//------------------------------------------------------------------
static HRESULT CreateBufferHelper(ID3D11Device* dev, u32 bind, u32 size,
                                   const void* init_data, ID3D11Buffer** out)
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = (size + 15) & ~15u; // align to 16
    bd.BindFlags      = bind;
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    if (init_data) {
        D3D11_SUBRESOURCE_DATA sd = {};
        sd.pSysMem = init_data;
        return dev->CreateBuffer(&bd, &sd, out);
    }
    return dev->CreateBuffer(&bd, nullptr, out);
}

static void UpdateBuffer(ID3D11DeviceContext* ctx, ID3D11Buffer* buf,
                          const void* data, u32 size)
{
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, data, size);
        ctx->Unmap(buf, 0);
    }
}

//------------------------------------------------------------------
CHW11::CHW11()  = default;
CHW11::~CHW11() = default;

bool CHW11::CreateDevice(HWND hwnd)
{
    // Get client rect for initial swap chain dimensions
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
    if (!CreateConstantBuffers())   return false;
    if (!CreateDefaultTexture())    return false;
    if (!CreatePrimBuf())           return false;

    States.rs_dirty = States.ds_dirty = States.bs_dirty = true;
    return true;
}

bool CHW11::CreateBackBuffer()
{
    // RTV
    ID3D11Texture2D* pBackBuf = nullptr;
    HRESULT hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuf);
    if (FAILED(hr)) return false;
    hr = pDevice->CreateRenderTargetView(pBackBuf, nullptr, &pRTV);
    pBackBuf->Release();
    if (FAILED(hr)) return false;

    // Depth-stencil
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

bool CHW11::CreateConstantBuffers()
{
    HRESULT hr;
    hr = CreateBufferHelper(pDevice, D3D11_BIND_CONSTANT_BUFFER,
                             sizeof(CEditorCB_PerFrame), nullptr, &cb_PerFrame);
    if (FAILED(hr)) return false;
    hr = CreateBufferHelper(pDevice, D3D11_BIND_CONSTANT_BUFFER,
                             sizeof(CEditorCB_PerObject), nullptr, &cb_PerObject);
    return SUCCEEDED(hr);
}

void CHW11::DestroyDevice()
{
    ReleaseBackBuffer();
    if (cb_PerFrame)  { cb_PerFrame->Release();   cb_PerFrame  = nullptr; }
    if (cb_PerObject) { cb_PerObject->Release();  cb_PerObject = nullptr; }
    if (inst_buf)     { inst_buf->Release();       inst_buf     = nullptr; inst_buf_cap = 0; }
    if (prim_vb)      { prim_vb->Release();        prim_vb      = nullptr; }
    if (pDefaultSRV)  { pDefaultSRV->Release();   pDefaultSRV  = nullptr; }
    if (pSwapChain)   { pSwapChain->Release();    pSwapChain   = nullptr; }
    if (pContext)     { pContext->Release();       pContext     = nullptr; }
    if (pDevice)      { pDevice->Release();        pDevice      = nullptr; }
}

bool CHW11::CreateDefaultTexture()
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height    = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format               = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count     = 1;
    td.BindFlags            = D3D11_BIND_SHADER_RESOURCE;
    td.Usage                = D3D11_USAGE_IMMUTABLE;
    u32 white               = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA sd = { &white, 4, 0 };
    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = pDevice->CreateTexture2D(&td, &sd, &tex);
    if (FAILED(hr)) return false;
    hr = pDevice->CreateShaderResourceView(tex, nullptr, &pDefaultSRV);
    tex->Release();
    return SUCCEEDED(hr);
}

bool CHW11::UploadInstances(const EditorInstanceData* data, u32 count)
{
    if (!count) return false;

    if (count > inst_buf_cap) {
        // grow by 1.5× to amortise allocations
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

//------------------------------------------------------------------
// SetRenderState: map D3D9 states to DX11 pipeline state fields
//------------------------------------------------------------------
void CHW11::SetRenderState(D3DRENDERSTATETYPE type, u32 value)
{
    switch (type)
    {
    case D3DRS_FILLMODE:
        {
            D3D11_FILL_MODE fm = (value == D3DFILL_WIREFRAME) ? D3D11_FILL_WIREFRAME :
                                  (value == D3DFILL_POINT)     ? D3D11_FILL_WIREFRAME : // no point in DX11, use wire
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
    // many DX9 states have no DX11 equivalent — intentionally ignore
    default: break;
    }
}

void CHW11::SetSamplerState(u32 /*sampler*/, D3DSAMPLERSTATETYPE /*type*/, u32 /*value*/)
{
    // Sampler states in DX11 are bound via SamplerState objects created per shader.
    // The editor shaders use pre-built sampler states — no runtime changes needed here.
}

//------------------------------------------------------------------
// FlushStates — apply dirty pipeline states
//------------------------------------------------------------------
void CEditorDX11States::apply_rs(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode              = fill_mode;
    rd.CullMode              = cull_mode;
    rd.FrontCounterClockwise = front_ccw;
    rd.DepthClipEnable       = depth_clip;
    rd.ScissorEnable         = scissor;
    rd.MultisampleEnable     = FALSE;
    rd.AntialiasedLineEnable = FALSE;
    ID3D11RasterizerState* rs = nullptr;
    dev->CreateRasterizerState(&rd, &rs);
    ctx->RSSetState(rs);
    if (rs) rs->Release();
    rs_dirty = false;
}

void CEditorDX11States::apply_ds(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable    = depth_enable ? TRUE : FALSE;
    dsd.DepthWriteMask = depth_write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc      = depth_func;
    dsd.StencilEnable  = stencil_enable ? TRUE : FALSE;
    // default stencil ops — editor doesn't use complex stencil logic
    dsd.FrontFace.StencilPassOp      = D3D11_STENCIL_OP_KEEP;
    dsd.FrontFace.StencilFailOp      = D3D11_STENCIL_OP_KEEP;
    dsd.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    dsd.FrontFace.StencilFunc        = D3D11_COMPARISON_ALWAYS;
    dsd.BackFace = dsd.FrontFace;
    ID3D11DepthStencilState* dss = nullptr;
    dev->CreateDepthStencilState(&dsd, &dss);
    ctx->OMSetDepthStencilState(dss, 0);
    if (dss) dss->Release();
    ds_dirty = false;
}

void CEditorDX11States::apply_bs(ID3D11Device* dev, ID3D11DeviceContext* ctx)
{
    D3D11_BLEND_DESC bd = {};
    if (alpha_blend) {
        bd.RenderTarget[0].BlendEnable           = TRUE;
        bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    }
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11BlendState* bs = nullptr;
    dev->CreateBlendState(&bd, &bs);
    float bf[4] = {};
    ctx->OMSetBlendState(bs, bf, 0xFFFFFFFF);
    if (bs) bs->Release();
    bs_dirty = false;
}

void CHW11::FlushStates()
{
    if (States.rs_dirty) States.apply_rs(pDevice, pContext);
    if (States.ds_dirty) States.apply_ds(pDevice, pContext);
    if (States.bs_dirty) States.apply_bs(pDevice, pContext);
}

//------------------------------------------------------------------
// Constant buffer upload
//------------------------------------------------------------------
void CHW11::UploadPerFrame(const float* view4x4, const float* proj4x4, const float* cam_pos3)
{
    CEditorCB_PerFrame cb;
    memcpy(cb.view,    view4x4, 64);
    memcpy(cb.proj,    proj4x4, 64);

    XMMATRIX V = XMLoadFloat4x4((const XMFLOAT4X4*)view4x4);
    XMMATRIX P = XMLoadFloat4x4((const XMFLOAT4X4*)proj4x4);
    XMMATRIX VP = XMMatrixMultiply(V, P);
    XMStoreFloat4x4((XMFLOAT4X4*)cb.viewproj, VP);

    cb.cam_pos[0] = cam_pos3[0];
    cb.cam_pos[1] = cam_pos3[1];
    cb.cam_pos[2] = cam_pos3[2];
    cb.cam_pos[3] = 1.f;

    UpdateBuffer(pContext, cb_PerFrame, &cb, sizeof(cb));
    pContext->VSSetConstantBuffers(0, 1, &cb_PerFrame);
    pContext->PSSetConstantBuffers(0, 1, &cb_PerFrame);
}

bool CHW11::CreatePrimBuf()
{
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = PRIM_VB_CAP * 16; // sizeof(FVF::L) = 12 (pos) + 4 (color) = 16
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

void CHW11::DU_DrawPrim(const void* verts, u32 count, D3D11_PRIMITIVE_TOPOLOGY topo)
{
    if (!count || !verts || !prim_vb) return;
    u32 to_draw = std::min<u32>(count, PRIM_VB_CAP);

    D3D11_MAPPED_SUBRESOURCE ms;
    if (FAILED(pContext->Map(prim_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) return;
    memcpy(ms.pData, verts, to_draw * 16);
    pContext->Unmap(prim_vb, 0);

    EditorShaders11.BindPrim3D(pContext);
    pContext->VSSetConstantBuffers(0, 1, &cb_PerFrame);

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

    // disable depth test for 2D screen-space overlays
    bool saved_depth = States.depth_enable;
    States.depth_enable = false;
    States.ds_dirty = true;
    FlushStates();
    pContext->Draw(to_draw, 0);

    // restore depth state
    States.depth_enable = saved_depth;
    States.ds_dirty = true;
}

void CHW11::UploadPerObject(const float* world4x4,
                             float sel_r, float sel_g, float sel_b, float sel_a)
{
    CEditorCB_PerObject cb;
    memcpy(cb.world, world4x4, 64);
    cb.color[0] = sel_r;
    cb.color[1] = sel_g;
    cb.color[2] = sel_b;
    cb.color[3] = sel_a;
    UpdateBuffer(pContext, cb_PerObject, &cb, sizeof(cb));
    pContext->VSSetConstantBuffers(1, 1, &cb_PerObject);
    pContext->PSSetConstantBuffers(1, 1, &cb_PerObject);
}
