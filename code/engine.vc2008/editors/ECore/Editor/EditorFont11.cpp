#include "stdafx.h"
#pragma hdrstop

#include "EditorFont11.h"
#include "HW11.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

//==================================================================
// Embedded HLSL
//==================================================================
// VS: pixel coords → NDC via b2 cbuffer; BGRA color unpacked as float4
static const char* s_font_vs = R"HLSL(
cbuffer cbFontVP : register(b2) { float2 vpSize; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos.x / vpSize.x * 2.0 - 1.0,
                   1.0 - i.pos.y / vpSize.y * 2.0, 0.0, 1.0);
    o.uv  = i.uv;
    o.col = i.col;
    return o;
}
)HLSL";

// PS: R8 atlas channel → alpha, tinted by vertex color
static const char* s_font_ps = R"HLSL(
Texture2D<float> Atlas : register(t1);
SamplerState     SS    : register(s1);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 main(PSIn i) : SV_Target {
    float a = Atlas.Sample(SS, i.uv);
    return float4(i.col.rgb, i.col.a * a);
}
)HLSL";

//==================================================================
// Implementation
//==================================================================

CEditorFont11 EditorFont11;

static ID3DBlob* FontCompile(const char* src, const char* entry, const char* profile)
{
    ID3DBlob* code = nullptr; ID3DBlob* errs = nullptr;
    UINT f = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
    f |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    f |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    HRESULT hr = D3DCompile(src, strlen(src), profile/*name*/, nullptr, nullptr, entry, profile, f, 0, &code, &errs);
    if (FAILED(hr)) {
        if (errs) { ELog.DlgMsg(mtError, "DX11 font shader '%s' error:\n%s", profile, (char*)errs->GetBufferPointer()); errs->Release(); }
        return nullptr;
    }
    if (errs) errs->Release();
    return code;
}

bool CEditorFont11::BuildAtlas(ID3D11Device* dev, const char* face, int height)
{
    HDC hRef = CreateCompatibleDC(nullptr);
    if (!hRef) return false;

    LOGFONTA lf = {};
    lf.lfHeight   = -(int)height;
    lf.lfWeight   = FW_NORMAL;
    lf.lfCharSet  = ANSI_CHARSET;
    lf.lfQuality  = NONANTIALIASED_QUALITY;
    strncpy_s(lf.lfFaceName, face, LF_FACESIZE - 1);
    HFONT hFont = CreateFontIndirectA(&lf);
    if (!hFont) { DeleteDC(hRef); return false; }
    HFONT hOld = (HFONT)SelectObject(hRef, hFont);

    TEXTMETRICA tm = {};
    GetTextMetricsA(hRef, &tm);
    m_cellH = tm.tmHeight + 1;  // +1px bottom padding
    m_cellW = 0;
    for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
        SIZE sz = {}; char ch = (char)c;
        GetTextExtentPoint32A(hRef, &ch, 1, &sz);
        if (sz.cx > m_cellW) m_cellW = sz.cx;
    }
    m_cellW += 2;  // 1px left+right padding
    SelectObject(hRef, hOld);
    DeleteDC(hRef);

    m_atlasW = m_cellW * COLS;
    m_atlasH = m_cellH * ROWS;
    m_lineH  = (float)m_cellH;

    // GDI rendering
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(bih); bih.biWidth = m_atlasW; bih.biHeight = -m_atlasH;
    bih.biPlanes = 1; bih.biBitCount = 32; bih.biCompression = BI_RGB;
    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(nullptr, (BITMAPINFO*)&bih, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp) { DeleteObject(hFont); return false; }

    HDC hDC = CreateCompatibleDC(nullptr);
    SelectObject(hDC, hBmp);
    SelectObject(hDC, hFont);
    RECT rc = { 0, 0, m_atlasW, m_atlasH };
    FillRect(hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(255, 255, 255));

    for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
        int i = c - FIRST_CHAR;
        int cx = (i % COLS) * m_cellW + 1;
        int cy = (i / COLS) * m_cellH;
        char ch = (char)c;
        TextOutA(hDC, cx, cy, &ch, 1);
    }
    GdiFlush();

    // Extract R channel → R8
    xr_vector<u8> r8((size_t)m_atlasW * m_atlasH);
    u32* src32 = (u32*)pBits;
    for (int i = 0; i < m_atlasW * m_atlasH; ++i)
        r8[i] = (u8)(src32[i] & 0xFF); // BGRX→ B channel (white text → 0xFF)

    DeleteDC(hDC);
    DeleteObject(hBmp);
    DeleteObject(hFont);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)m_atlasW; td.Height = (UINT)m_atlasH;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA sd = { r8.data(), (UINT)m_atlasW, 0 };
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &m_atlasSRV);
    tex->Release();
    return SUCCEEDED(hr);
}

bool CEditorFont11::BuildPipeline(ID3D11Device* dev)
{
    // VS
    ID3DBlob* bVS = FontCompile(s_font_vs, "main", "vs_5_0");
    if (!bVS) return false;
    HRESULT hr = dev->CreateVertexShader(bVS->GetBufferPointer(), bVS->GetBufferSize(), nullptr, &m_vs);
    if (SUCCEEDED(hr)) {
        // pos(2f) + uv(2f) + color(B8G8R8A8_UNORM → float4 in shader with correct RGBA order)
        D3D11_INPUT_ELEMENT_DESC ild[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = dev->CreateInputLayout(ild, _countof(ild), bVS->GetBufferPointer(), bVS->GetBufferSize(), &m_il);
    }
    bVS->Release();
    if (FAILED(hr)) return false;

    // PS
    ID3DBlob* bPS = FontCompile(s_font_ps, "main", "ps_5_0");
    if (!bPS) return false;
    hr = dev->CreatePixelShader(bPS->GetBufferPointer(), bPS->GetBufferSize(), nullptr, &m_ps);
    bPS->Release();
    if (FAILED(hr)) return false;

    // viewport CB (slot b2)
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 16; cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&cbd, nullptr, &m_cbVP))) return false;

    // dynamic VB
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = MAX_CHARS_PER_FLUSH * 6 * sizeof(Vtx2D);
    vbd.Usage = D3D11_USAGE_DYNAMIC; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&vbd, nullptr, &m_vb))) return false;

    // sampler: nearest-neighbor, clamp
    D3D11_SAMPLER_DESC ss = {};
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ss.MaxLOD = D3D11_FLOAT32_MAX; ss.ComparisonFunc = D3D11_COMPARISON_NEVER;
    if (FAILED(dev->CreateSamplerState(&ss, &m_ss))) return false;

    // blend: src-alpha
    D3D11_BLEND_DESC bd = {};
    auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;  rt.DestBlendAlpha = D3D11_BLEND_ZERO;     rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bd, &m_bs))) return false;

    // depth-stencil: all off
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE; dsd.StencilEnable = FALSE;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &m_dss))) return false;

    return true;
}

bool CEditorFont11::Create(ID3D11Device* dev, const char* face, int height)
{
    if (!BuildAtlas(dev, face, height))   return false;
    if (!BuildPipeline(dev))             { Destroy(); return false; }
    Msg("* DX11 editor font '%s' %dpx, atlas %dx%d", face, height, m_atlasW, m_atlasH);
    return true;
}

void CEditorFont11::Destroy()
{
    auto rel = [](auto*& p){ if (p){ p->Release(); p = nullptr; } };
    rel(m_atlasSRV); rel(m_cbVP); rel(m_vb);
    rel(m_vs); rel(m_ps); rel(m_il);
    rel(m_ss); rel(m_bs); rel(m_dss);
    m_buf.clear();
}

void CEditorFont11::SetColor(u32 argb)      { m_color = argb; }
void CEditorFont11::OutSet(float x, float y){ m_curX = x; m_curY = y; }
void CEditorFont11::OutSkip(float factor)   { m_curY += m_lineH * factor; }

void CEditorFont11::PushChar(char ch, float& x, float y)
{
    if ((unsigned char)ch < FIRST_CHAR || (unsigned char)ch > LAST_CHAR) { x += m_cellW; return; }
    int idx = (unsigned char)ch - FIRST_CHAR;
    int col = idx % COLS; int row = idx / COLS;
    float u0 = (float)(col * m_cellW)            / m_atlasW;
    float v0 = (float)(row * m_cellH)            / m_atlasH;
    float u1 = (float)(col * m_cellW + m_cellW)  / m_atlasW;
    float v1 = (float)(row * m_cellH + m_cellH)  / m_atlasH;
    float x1 = x + m_cellW, y1 = y + m_cellH;

    // m_color is D3DCOLOR ARGB = 0xAARRGGBB
    // DXGI_FORMAT_B8G8R8A8_UNORM reads memory as B,G,R,A → shader gets (R,G,B,A) correctly
    // D3DCOLOR bytes in memory (little-endian): byte0=B, byte1=G, byte2=R, byte3=A → matches B8G8R8A8
    u32 c = m_color;

    m_buf.push_back({x,  y,  u0, v0, c});
    m_buf.push_back({x1, y,  u1, v0, c});
    m_buf.push_back({x,  y1, u0, v1, c});
    m_buf.push_back({x1, y,  u1, v0, c});
    m_buf.push_back({x1, y1, u1, v1, c});
    m_buf.push_back({x,  y1, u0, v1, c});
    x += m_cellW;
}

void CEditorFont11::OutNext(const char* fmt, ...)
{
    if (!m_vs || m_buf.size() + 512 * 6 > MAX_CHARS_PER_FLUSH * 6) return;
    char line[1024];
    va_list va; va_start(va, fmt);
    vsnprintf(line, sizeof(line) - 1, fmt, va);
    va_end(va);
    float x = m_curX;
    for (const char* p = line; *p; ++p) PushChar(*p, x, m_curY);
    m_curY += m_lineH;
}

void CEditorFont11::Flush(ID3D11DeviceContext* ctx, float vp_w, float vp_h)
{
    if (m_buf.empty() || !m_vs || !m_vb) return;

    u32 nv = (u32)m_buf.size();
    if (nv > MAX_CHARS_PER_FLUSH * 6) nv = MAX_CHARS_PER_FLUSH * 6;

    // upload vertices
    D3D11_MAPPED_SUBRESOURCE ms = {};
    if (FAILED(ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) { m_buf.clear(); return; }
    memcpy(ms.pData, m_buf.data(), nv * sizeof(Vtx2D));
    ctx->Unmap(m_vb, 0);

    // upload viewport CB
    if (m_cbVP) {
        struct { float w, h, _0, _1; } vpd = { vp_w, vp_h, 0, 0 };
        if (SUCCEEDED(ctx->Map(m_cbVP, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &vpd, sizeof(vpd));
            ctx->Unmap(m_cbVP, 0);
        }
        ctx->VSSetConstantBuffers(2, 1, &m_cbVP);
    }

    UINT stride = sizeof(Vtx2D), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    ctx->IASetInputLayout(m_il);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_ps, nullptr, 0);
    ctx->PSSetShaderResources(1, 1, &m_atlasSRV);
    ctx->PSSetSamplers(1, 1, &m_ss);

    float bf[4] = {};
    ctx->OMSetBlendState(m_bs, bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_dss, 0);

    ctx->Draw(nv, 0);

    ctx->OMSetDepthStencilState(nullptr, 0);
    ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

    m_buf.clear();
}
