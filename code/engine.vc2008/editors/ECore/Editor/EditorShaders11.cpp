#include "stdafx.h"
#pragma hdrstop

#include "EditorShaders11.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
// DirectXMath is header-only — no .lib needed

CEditorShaders11 EditorShaders11;

//==================================================================
// Embedded HLSL sources
//==================================================================

// Common per-frame / per-object constant buffer declarations (included in all shaders)
static const char* s_cbDecls = R"HLSL(
cbuffer cbPerFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float4   CamPos;
};
cbuffer cbPerObject : register(b1)
{
    float4x4 World;
    float4   ObjectColor; // xyz=tint, w=selection blend
};
)HLSL";

//------------------------------------------------------------------
// SOLID (textured) shaders
//------------------------------------------------------------------
static const char* s_vs_solid = R"HLSL(
#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct VSIn  { float3 pos:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
VSOut main(VSIn i) {
    VSOut o;
    float4 wp = mul(float4(i.pos,1), World);
    o.wp  = wp.xyz;
    o.pos = mul(wp, ViewProj);
    o.wn  = normalize(mul(i.n, (float3x3)World));
    o.uv  = i.uv;
    return o;
}
)HLSL";

static const char* s_ps_solid = R"HLSL(
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
float4 main(PSIn i) : SV_Target {
    float4 tex  = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(tex.a - 0.5);
    float3 L    = normalize(float3(0.5, 1.0, 0.5));
    float  diff = saturate(dot(normalize(i.wn), L)) * 0.7 + 0.3;
    float4 col  = tex * diff;
    col.rgb = lerp(col.rgb, ObjectColor.rgb, ObjectColor.a);
    return col;
}
)HLSL";

//------------------------------------------------------------------
// WIREFRAME shaders (same VS as solid, flat-color PS)
//------------------------------------------------------------------
static const char* s_ps_wireframe = R"HLSL(
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
float4 main(PSIn i) : SV_Target {
    return float4(ObjectColor.rgb, 1.0);
}
)HLSL";

//------------------------------------------------------------------
// COLORED (gizmos / helpers) — position + color in ObjectColor
//------------------------------------------------------------------
static const char* s_vs_colored = R"HLSL(
#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct VSIn  { float3 pos:POSITION; };
struct VSOut { float4 pos:SV_POSITION; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(mul(float4(i.pos,1), World), ViewProj);
    return o;
}
)HLSL";

static const char* s_ps_colored = R"HLSL(
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
float4 main(float4 pos:SV_POSITION) : SV_Target {
    return float4(ObjectColor.rgb, 1.0);
}
)HLSL";

//------------------------------------------------------------------
// PRIM — per-vertex color, FVF::L layout (float3 pos + B8G8R8A8 color)
// vs_prim: 3D world-space, multiplied by ViewProj only (primitives are already in world coords)
// vs_prim2d: 2D NDC passthrough — pos.xy are already in [-1,1]
//------------------------------------------------------------------
static const char* s_vs_prim = R"HLSL(
#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1), ViewProj);
    o.col = i.col;
    return o;
}
)HLSL";

static const char* s_vs_prim2d = R"HLSL(
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos.xy, 0, 1);
    o.col = i.col;
    return o;
}
)HLSL";

static const char* s_ps_prim = R"HLSL(
struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };
float4 main(PSIn i) : SV_Target { return i.col; }
)HLSL";

//------------------------------------------------------------------
// INSTANCED solid — pos+normal+uv vertex, per-instance world+color
//------------------------------------------------------------------
static const char* s_vs_instanced = R"HLSL(
#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn {
    float3 pos    : POSITION;
    float3 n      : NORMAL;
    float2 uv     : TEXCOORD0;
    // per-instance (slot 1)
    float4 iw0    : WORLDMATRIX0;
    float4 iw1    : WORLDMATRIX1;
    float4 iw2    : WORLDMATRIX2;
    float4 iw3    : WORLDMATRIX3;
    float4 icolor : ICOLOR;
};
struct VSOut { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    float4x4 W = float4x4(i.iw0, i.iw1, i.iw2, i.iw3);
    float4 wp  = mul(float4(i.pos,1), W);
    o.pos = mul(wp, ViewProj);
    o.wn  = normalize(mul(i.n, (float3x3)W));
    o.uv  = i.uv;
    o.col = i.icolor;
    return o;
}
)HLSL";

// Instanced PS reuses ps_solid logic but uses per-instance color from VS
static const char* s_ps_instanced = R"HLSL(
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 main(PSIn i) : SV_Target {
    float4 tex  = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(tex.a - 0.5);
    float3 L    = normalize(float3(0.5, 1.0, 0.5));
    float  diff = saturate(dot(normalize(i.wn), L)) * 0.7 + 0.3;
    float4 col  = tex * diff;
    col.rgb = lerp(col.rgb, i.col.rgb, i.col.a);
    return col;
}
)HLSL";

//==================================================================
// Compilation
//==================================================================
ID3DBlob* CEditorShaders11::CompileShader(const char* src, const char* entry,
                                           const char* profile, const char* debug_name)
{
    ID3DBlob* code = nullptr;
    ID3DBlob* errs = nullptr;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    HRESULT hr = D3DCompile(src, strlen(src), debug_name,
                             nullptr, nullptr, entry, profile,
                             flags, 0, &code, &errs);
    if (FAILED(hr)) {
        if (errs) {
            ELog.DlgMsg(mtError, "Shader compile error [%s]:\n%s",
                        debug_name, (const char*)errs->GetBufferPointer());
            errs->Release();
        }
        return nullptr;
    }
    if (errs) errs->Release();
    return code;
}

bool CEditorShaders11::Create(ID3D11Device* dev)
{
    HRESULT hr;

    // ----- VS: solid -----
    ID3DBlob* bVsSolid = CompileShader(s_vs_solid, "main", "vs_5_0", "vs_solid");
    if (!bVsSolid) return false;
    hr = dev->CreateVertexShader(bVsSolid->GetBufferPointer(),
                                  bVsSolid->GetBufferSize(), nullptr, &vs_solid);
    if (FAILED(hr)) { bVsSolid->Release(); return false; }

    // ----- Input layout: solid (pos+normal+uv) -----
    D3D11_INPUT_ELEMENT_DESC il_solid_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_solid_desc, _countof(il_solid_desc),
                                 bVsSolid->GetBufferPointer(),
                                 bVsSolid->GetBufferSize(), &il_solid);
    bVsSolid->Release();
    if (FAILED(hr)) return false;

    // ----- PS: solid -----
    ID3DBlob* bPsSolid = CompileShader(s_ps_solid, "main", "ps_5_0", "ps_solid");
    if (!bPsSolid) return false;
    hr = dev->CreatePixelShader(bPsSolid->GetBufferPointer(),
                                 bPsSolid->GetBufferSize(), nullptr, &ps_solid);
    bPsSolid->Release();
    if (FAILED(hr)) return false;

    // ----- PS: wireframe -----
    ID3DBlob* bPsWire = CompileShader(s_ps_wireframe, "main", "ps_5_0", "ps_wireframe");
    if (!bPsWire) return false;
    hr = dev->CreatePixelShader(bPsWire->GetBufferPointer(),
                                 bPsWire->GetBufferSize(), nullptr, &ps_wireframe);
    bPsWire->Release();
    if (FAILED(hr)) return false;
    vs_wireframe = vs_solid; // same VS — just different PS and rasterizer fill mode
    vs_wireframe->AddRef();

    // ----- VS: colored -----
    ID3DBlob* bVsCol = CompileShader(s_vs_colored, "main", "vs_5_0", "vs_colored");
    if (!bVsCol) return false;
    hr = dev->CreateVertexShader(bVsCol->GetBufferPointer(),
                                  bVsCol->GetBufferSize(), nullptr, &vs_colored);
    if (FAILED(hr)) { bVsCol->Release(); return false; }

    // ----- Input layout: colored (pos only) -----
    D3D11_INPUT_ELEMENT_DESC il_col_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_col_desc, _countof(il_col_desc),
                                 bVsCol->GetBufferPointer(),
                                 bVsCol->GetBufferSize(), &il_colored);
    bVsCol->Release();
    if (FAILED(hr)) return false;

    // ----- PS: colored -----
    ID3DBlob* bPsCol = CompileShader(s_ps_colored, "main", "ps_5_0", "ps_colored");
    if (!bPsCol) return false;
    hr = dev->CreatePixelShader(bPsCol->GetBufferPointer(),
                                 bPsCol->GetBufferSize(), nullptr, &ps_colored);
    bPsCol->Release();
    if (FAILED(hr)) return false;

    // ----- VS: prim (3D world-space per-vertex color) -----
    ID3DBlob* bVsPrim = CompileShader(s_vs_prim, "main", "vs_5_0", "vs_prim");
    if (!bVsPrim) return false;
    hr = dev->CreateVertexShader(bVsPrim->GetBufferPointer(), bVsPrim->GetBufferSize(), nullptr, &vs_prim);
    if (FAILED(hr)) { bVsPrim->Release(); return false; }

    // ----- Input layout: prim (pos float3 + color B8G8R8A8 — matches FVF::L stride 16) -----
    D3D11_INPUT_ELEMENT_DESC il_prim_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,  0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_prim_desc, _countof(il_prim_desc),
                                 bVsPrim->GetBufferPointer(), bVsPrim->GetBufferSize(), &il_prim);
    bVsPrim->Release();
    if (FAILED(hr)) return false;

    // ----- VS: prim2d (NDC passthrough) -----
    ID3DBlob* bVsPrim2d = CompileShader(s_vs_prim2d, "main", "vs_5_0", "vs_prim2d");
    if (!bVsPrim2d) return false;
    hr = dev->CreateVertexShader(bVsPrim2d->GetBufferPointer(), bVsPrim2d->GetBufferSize(), nullptr, &vs_prim2d);
    bVsPrim2d->Release();
    if (FAILED(hr)) return false;

    // ----- PS: prim (per-vertex color passthrough) -----
    ID3DBlob* bPsPrim = CompileShader(s_ps_prim, "main", "ps_5_0", "ps_prim");
    if (!bPsPrim) return false;
    hr = dev->CreatePixelShader(bPsPrim->GetBufferPointer(), bPsPrim->GetBufferSize(), nullptr, &ps_prim);
    bPsPrim->Release();
    if (FAILED(hr)) return false;

    // ----- VS: instanced -----
    ID3DBlob* bVsInst = CompileShader(s_vs_instanced, "main", "vs_5_0", "vs_instanced");
    if (!bVsInst) return false;
    hr = dev->CreateVertexShader(bVsInst->GetBufferPointer(),
                                  bVsInst->GetBufferSize(), nullptr, &vs_instanced);
    if (FAILED(hr)) { bVsInst->Release(); return false; }

    // ----- Input layout: instanced (per-vertex pos+normal+uv; per-instance world+color) -----
    D3D11_INPUT_ELEMENT_DESC il_inst_desc[] = {
        // per-vertex (slot 0)
        {"POSITION",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
        {"NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
        {"TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA,   0},
        // per-instance (slot 1) — 4 float4 rows of the world matrix
        {"WORLDMATRIX", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLDMATRIX", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLDMATRIX", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"WORLDMATRIX", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"ICOLOR",      0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 64, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    hr = dev->CreateInputLayout(il_inst_desc, _countof(il_inst_desc),
                                 bVsInst->GetBufferPointer(),
                                 bVsInst->GetBufferSize(), &il_instanced);
    bVsInst->Release();
    if (FAILED(hr)) return false;

    // ----- PS: instanced (texture + per-instance color tint from VS) -----
    ID3DBlob* bPsInst = CompileShader(s_ps_instanced, "main", "ps_5_0", "ps_instanced");
    if (!bPsInst) return false;
    hr = dev->CreatePixelShader(bPsInst->GetBufferPointer(),
                                 bPsInst->GetBufferSize(), nullptr, &ps_instanced);
    bPsInst->Release();
    if (FAILED(hr)) return false;

    // ----- Sampler -----
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hr = dev->CreateSamplerState(&sd, &ss_linear);
    if (FAILED(hr)) return false;

    Msg("* DX11 editor shaders compiled OK");
    return true;
}

void CEditorShaders11::Destroy()
{
    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    rel(il_solid); rel(il_instanced); rel(il_colored); rel(il_prim);
    rel(vs_solid); rel(vs_wireframe); rel(vs_colored); rel(vs_instanced); rel(vs_prim); rel(vs_prim2d);
    rel(ps_solid); rel(ps_wireframe); rel(ps_colored); rel(ps_prim); rel(ps_instanced);
    rel(ss_linear);
}

void CEditorShaders11::BindSolid(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_solid);
    ctx->VSSetShader(vs_solid, nullptr, 0);
    ctx->PSSetShader(ps_solid, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::BindWireframe(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_solid);
    ctx->VSSetShader(vs_wireframe, nullptr, 0);
    ctx->PSSetShader(ps_wireframe, nullptr, 0);
}

void CEditorShaders11::BindColored(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_colored);
    ctx->VSSetShader(vs_colored, nullptr, 0);
    ctx->PSSetShader(ps_colored, nullptr, 0);
}

void CEditorShaders11::BindInstanced(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_instanced);
    ctx->VSSetShader(vs_instanced, nullptr, 0);
    ctx->PSSetShader(ps_instanced, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::BindPrim3D(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_prim);
    ctx->VSSetShader(vs_prim, nullptr, 0);
    ctx->PSSetShader(ps_prim, nullptr, 0);
}

void CEditorShaders11::BindPrim2D(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_prim);
    ctx->VSSetShader(vs_prim2d, nullptr, 0);
    ctx->PSSetShader(ps_prim, nullptr, 0);
}

void CEditorShaders11::SetTexture(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv)
{
    ctx->PSSetShaderResources(0, 1, &srv);
}

void CEditorShaders11::SetDefaultSampler(ID3D11DeviceContext* ctx)
{
    ctx->PSSetSamplers(0, 1, &ss_linear);
}
