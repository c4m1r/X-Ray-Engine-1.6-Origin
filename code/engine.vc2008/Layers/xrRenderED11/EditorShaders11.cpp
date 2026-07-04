#include "stdafx.h"
#pragma hdrstop

#include "EditorShaders11.h"
#include "EditorShaderRegistry11.h"
#include "EditorD3DCompileSupport.h"
#include "HW11.h"            // HW11.cs_cull / CreateCullResources (frustum-cull compute shader)

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

CEditorShaders11 EditorShaders11;   // real global; lifecycle driven via CResourceManager11 (Resources11)

//==================================================================
// Compilation helpers
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

// Load HLSL source from gamedata/shaders/ed11/<name> and compile it.
static ID3DBlob* LoadAndCompileFile(const char* hlsl_name, const char* entry, const char* profile)
{
    string_path path;
    FS.update_path(path, "$game_shaders$", "ed11\\");
    xr_strcat(path, hlsl_name);

    if (!FS.exist(path)) {
        ELog.DlgMsg(mtError, "DX11 shader not found: %s", path);
        return nullptr;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        ELog.DlgMsg(mtError, "DX11 shader open failed: %s", path);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    size_t sz = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    xr_vector<char> src(sz + 1);
    fread(src.data(), 1, sz, fp);
    fclose(fp);
    src[sz] = 0;

    return CEditorShaders11::CompileShader(src.data(), entry, profile, hlsl_name);
}

bool CEditorShaders11::Create(ID3D11Device* dev)
{
    HRESULT hr;

    // ----- VS: solid -----
    ID3DBlob* bVsSolid = LoadAndCompileFile("vs_solid.hlsl", "main", "vs_5_0");
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
    ID3DBlob* bPsSolid = LoadAndCompileFile("ps_solid.hlsl", "main", "ps_5_0");
    if (!bPsSolid) return false;
    hr = dev->CreatePixelShader(bPsSolid->GetBufferPointer(),
                                 bPsSolid->GetBufferSize(), nullptr, &ps_solid);
    bPsSolid->Release();
    if (FAILED(hr)) return false;

    // ----- PS: wireframe -----
    ID3DBlob* bPsWire = LoadAndCompileFile("ps_wireframe.hlsl", "main", "ps_5_0");
    if (!bPsWire) return false;
    hr = dev->CreatePixelShader(bPsWire->GetBufferPointer(),
                                 bPsWire->GetBufferSize(), nullptr, &ps_wireframe);
    bPsWire->Release();
    if (FAILED(hr)) return false;
    vs_wireframe = vs_solid; // same VS — just different PS and rasterizer fill mode
    vs_wireframe->AddRef();

    // ----- VS: colored -----
    ID3DBlob* bVsCol = LoadAndCompileFile("vs_colored.hlsl", "main", "vs_5_0");
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
    ID3DBlob* bPsCol = LoadAndCompileFile("ps_colored.hlsl", "main", "ps_5_0");
    if (!bPsCol) return false;
    hr = dev->CreatePixelShader(bPsCol->GetBufferPointer(),
                                 bPsCol->GetBufferSize(), nullptr, &ps_colored);
    bPsCol->Release();
    if (FAILED(hr)) return false;

    // ----- VS: prim (3D world-space per-vertex color) -----
    ID3DBlob* bVsPrim = LoadAndCompileFile("vs_prim.hlsl", "main", "vs_5_0");
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
    ID3DBlob* bVsPrim2d = LoadAndCompileFile("vs_prim2d.hlsl", "main", "vs_5_0");
    if (!bVsPrim2d) return false;
    hr = dev->CreateVertexShader(bVsPrim2d->GetBufferPointer(), bVsPrim2d->GetBufferSize(), nullptr, &vs_prim2d);
    bVsPrim2d->Release();
    if (FAILED(hr)) return false;

    // ----- PS: prim (per-vertex color passthrough) -----
    ID3DBlob* bPsPrim = LoadAndCompileFile("ps_prim.hlsl", "main", "ps_5_0");
    if (!bPsPrim) return false;
    hr = dev->CreatePixelShader(bPsPrim->GetBufferPointer(), bPsPrim->GetBufferSize(), nullptr, &ps_prim);
    bPsPrim->Release();
    if (FAILED(hr)) return false;

    // ----- VS: particle (3D world-space pos + color + uv) -----
    ID3DBlob* bVsPart = LoadAndCompileFile("vs_particle.hlsl", "main", "vs_5_0");
    if (!bVsPart) return false;
    hr = dev->CreateVertexShader(bVsPart->GetBufferPointer(), bVsPart->GetBufferSize(), nullptr, &vs_particle);
    if (FAILED(hr)) { bVsPart->Release(); return false; }

    // ----- Input layout: particle (pos float3 + color B8G8R8A8 + uv float2 = FVF::LIT, stride 24) -----
    D3D11_INPUT_ELEMENT_DESC il_part_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,  0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,   0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,     0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_part_desc, _countof(il_part_desc),
                                 bVsPart->GetBufferPointer(), bVsPart->GetBufferSize(), &il_particle);
    bVsPart->Release();
    if (FAILED(hr)) return false;

    // ----- PS: particle (texture * vertex color) -----
    ID3DBlob* bPsPart = LoadAndCompileFile("ps_particle.hlsl", "main", "ps_5_0");
    if (!bPsPart) return false;
    hr = dev->CreatePixelShader(bPsPart->GetBufferPointer(), bPsPart->GetBufferSize(), nullptr, &ps_particle);
    bPsPart->Release();
    if (FAILED(hr)) return false;

    // ----- PS: detail (texture * vertex color + alpha-test) — grass -----
    ID3DBlob* bPsDet = LoadAndCompileFile("ps_detail.hlsl", "main", "ps_5_0");
    if (!bPsDet) return false;
    hr = dev->CreatePixelShader(bPsDet->GetBufferPointer(), bPsDet->GetBufferSize(), nullptr, &ps_detail);
    bPsDet->Release();
    if (FAILED(hr)) return false;

    // ----- VS: basetex (3D world-space pos + uv) — base-texture overlay -----
    ID3DBlob* bVsBT = LoadAndCompileFile("vs_basetex.hlsl", "main", "vs_5_0");
    if (!bVsBT) return false;
    hr = dev->CreateVertexShader(bVsBT->GetBufferPointer(), bVsBT->GetBufferSize(), nullptr, &vs_basetex);
    if (FAILED(hr)) { bVsBT->Release(); return false; }

    // ----- Input layout: basetex (pos float3 + uv float2 = FVF::V, stride 20) -----
    D3D11_INPUT_ELEMENT_DESC il_bt_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_bt_desc, _countof(il_bt_desc),
                                 bVsBT->GetBufferPointer(), bVsBT->GetBufferSize(), &il_basetex);
    bVsBT->Release();
    if (FAILED(hr)) return false;

    // ----- PS: basetex (texture rgb + ObjectColor.a) -----
    ID3DBlob* bPsBT = LoadAndCompileFile("ps_basetex.hlsl", "main", "ps_5_0");
    if (!bPsBT) return false;
    hr = dev->CreatePixelShader(bPsBT->GetBufferPointer(), bPsBT->GetBufferSize(), nullptr, &ps_basetex);
    bPsBT->Release();
    if (FAILED(hr)) return false;

    // ----- VS: grass instanced (slot0 pos+uv, slot1 per-instance world matrix) -----
    ID3DBlob* bVsGI = LoadAndCompileFile("vs_grass_inst.hlsl", "main", "vs_5_0");
    if (!bVsGI) return false;
    hr = dev->CreateVertexShader(bVsGI->GetBufferPointer(), bVsGI->GetBufferSize(), nullptr, &vs_grass_inst);
    if (FAILED(hr)) { bVsGI->Release(); return false; }

    // ----- Input layout: grass instanced -----
    D3D11_INPUT_ELEMENT_DESC il_gi_desc[] = {
        // slot 0: per-vertex FVF::V (pos+uv, stride 20)
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
        // slot 1: per-instance world matrix (4 rows, stride 64)
        {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    hr = dev->CreateInputLayout(il_gi_desc, _countof(il_gi_desc),
                                 bVsGI->GetBufferPointer(), bVsGI->GetBufferSize(), &il_grass_inst);
    bVsGI->Release();
    if (FAILED(hr)) return false;

    // ----- VS: sprite2d (NDC pos + UV + color) -----
    ID3DBlob* bVsSpr = LoadAndCompileFile("vs_sprite2d.hlsl", "main", "vs_5_0");
    if (!bVsSpr) return false;
    hr = dev->CreateVertexShader(bVsSpr->GetBufferPointer(), bVsSpr->GetBufferSize(), nullptr, &vs_sprite2d);
    if (FAILED(hr)) { bVsSpr->Release(); return false; }

    // ----- Input layout: sprite2d (float2 pos + float2 uv + B8G8R8A8 color = SpriteVert2D, stride 20) -----
    D3D11_INPUT_ELEMENT_DESC il_spr_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,   0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };
    hr = dev->CreateInputLayout(il_spr_desc, _countof(il_spr_desc),
                                 bVsSpr->GetBufferPointer(), bVsSpr->GetBufferSize(), &il_sprite2d);
    bVsSpr->Release();
    if (FAILED(hr)) return false;

    // ----- PS: sprite2d (texture * vertex color) -----
    ID3DBlob* bPsSpr = LoadAndCompileFile("ps_sprite2d.hlsl", "main", "ps_5_0");
    if (!bPsSpr) return false;
    hr = dev->CreatePixelShader(bPsSpr->GetBufferPointer(), bPsSpr->GetBufferSize(), nullptr, &ps_sprite2d);
    bPsSpr->Release();
    if (FAILED(hr)) return false;

    // ----- VS: instanced -----
    ID3DBlob* bVsInst = LoadAndCompileFile("vs_instanced.hlsl", "main", "vs_5_0");
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
    ID3DBlob* bPsInst = LoadAndCompileFile("ps_instanced.hlsl", "main", "ps_5_0");
    if (!bPsInst) return false;
    hr = dev->CreatePixelShader(bPsInst->GetBufferPointer(),
                                 bPsInst->GetBufferSize(), nullptr, &ps_instanced);
    bPsInst->Release();
    if (FAILED(hr)) return false;

    // ----- PS: instanced transparent (нет clip — для стёкол) -----
    ID3DBlob* bPsInstT = LoadAndCompileFile("ps_inst_transparent.hlsl", "main", "ps_5_0");
    if (!bPsInstT) return false;
    hr = dev->CreatePixelShader(bPsInstT->GetBufferPointer(),
                                 bPsInstT->GetBufferSize(), nullptr, &ps_inst_transparent);
    bPsInstT->Release();
    if (FAILED(hr)) return false;

    // ----- VS: LOD billboard -----
    ID3DBlob* bVsLod = LoadAndCompileFile("vs_lod.hlsl", "main", "vs_5_0");
    if (!bVsLod) return false;
    hr = dev->CreateVertexShader(bVsLod->GetBufferPointer(),
                                  bVsLod->GetBufferSize(), nullptr, &vs_lod);
    if (FAILED(hr)) { bVsLod->Release(); return false; }

    // ----- Input layout: LOD (per-vertex quad corner; per-instance center/radius + params) -----
    D3D11_INPUT_ELEMENT_DESC il_lod_desc[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA,   0},
        {"ICENTER",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        {"IPARAM",   0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1},
    };
    hr = dev->CreateInputLayout(il_lod_desc, _countof(il_lod_desc),
                                 bVsLod->GetBufferPointer(),
                                 bVsLod->GetBufferSize(), &il_lod);
    bVsLod->Release();
    if (FAILED(hr)) return false;

    // ----- PS: LOD billboard -----
    ID3DBlob* bPsLod = LoadAndCompileFile("ps_lod.hlsl", "main", "ps_5_0");
    if (!bPsLod) return false;
    hr = dev->CreatePixelShader(bPsLod->GetBufferPointer(),
                                 bPsLod->GetBufferSize(), nullptr, &ps_lod);
    bPsLod->Release();
    if (FAILED(hr)) return false;

    // ----- CS: frustum culling -----
    ID3DBlob* bCsCull = LoadAndCompileFile("cs_frustum_cull.hlsl", "main", "cs_5_0");
    if (!bCsCull) return false;
    hr = dev->CreateComputeShader(bCsCull->GetBufferPointer(),
                                   bCsCull->GetBufferSize(), nullptr, &HW11.cs_cull);
    bCsCull->Release();
    if (FAILED(hr)) return false;

    if (!HW11.CreateCullResources(dev)) return false;

    // ----- Sampler -----
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hr = dev->CreateSamplerState(&sd, &ss_linear);
    if (FAILED(hr)) return false;

    // point + clamp: crisp magnification of small atlases (AI node arrows) — bilinear/WRAP
    // blurs the 16×16 atlas cells and bleeds neighbours at cell borders.
    D3D11_SAMPLER_DESC sp = {};
    sp.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sp.AddressU = sp.AddressV = sp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sp.MaxLOD         = D3D11_FLOAT32_MAX;
    sp.ComparisonFunc = D3D11_COMPARISON_NEVER;
    hr = dev->CreateSamplerState(&sp, &ss_point);
    if (FAILED(hr)) return false;

    // ----- Blend state: alpha blend (src_alpha / inv_src_alpha) — для стёкол -----
    D3D11_BLEND_DESC bld = {};
    bld.RenderTarget[0].BlendEnable            = TRUE;
    bld.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    bld.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bld.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bld.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bld.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = dev->CreateBlendState(&bld, &bs_alpha);
    if (FAILED(hr)) return false;

    // ----- Blend state: additive (src_alpha / one) — для glow/particle спрайтов -----
    D3D11_BLEND_DESC bld_add = {};
    bld_add.RenderTarget[0].BlendEnable            = TRUE;
    bld_add.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    bld_add.RenderTarget[0].DestBlend             = D3D11_BLEND_ONE;
    bld_add.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bld_add.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bld_add.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
    bld_add.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bld_add.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = dev->CreateBlendState(&bld_add, &bs_additive);
    if (FAILED(hr)) return false;

    // Регистрируем текущие шейдеры в реестре по именам
    EditorShaderRegistry11.Add("editor\\solid",      { ps_solid,            nullptr  });
    EditorShaderRegistry11.Add("editor\\wireframe",  { ps_wireframe,        nullptr  });
    EditorShaderRegistry11.Add("editor\\colored",    { ps_colored,          nullptr  });
    EditorShaderRegistry11.Add("editor\\prim",       { ps_prim,             nullptr  });
    EditorShaderRegistry11.Add("editor\\instanced",  { ps_instanced,        nullptr  });
    EditorShaderRegistry11.Add("editor\\transparent",{ ps_inst_transparent, bs_alpha });

    Msg("* DX11 editor shaders compiled OK (from gamedata/shaders/ed11/)");
    return true;
}

void CEditorShaders11::Destroy()
{
    EditorShaderRegistry11.Clear();

    HW11.DestroyCullResources();
    auto rel = [](auto*& p) { if (p) { p->Release(); p = nullptr; } };
    rel(il_solid); rel(il_instanced); rel(il_colored); rel(il_prim); rel(il_sprite2d); rel(il_lod); rel(il_particle); rel(il_basetex); rel(il_grass_inst);
    rel(vs_solid); rel(vs_wireframe); rel(vs_colored); rel(vs_instanced); rel(vs_prim); rel(vs_prim2d); rel(vs_sprite2d); rel(vs_lod); rel(vs_particle); rel(vs_basetex); rel(vs_grass_inst);
    rel(ps_solid); rel(ps_wireframe); rel(ps_colored); rel(ps_prim); rel(ps_instanced);
    rel(ps_inst_transparent); rel(ps_sprite2d); rel(ps_lod); rel(ps_particle); rel(ps_detail); rel(ps_basetex);
    rel(bs_alpha); rel(bs_additive);
    rel(ss_linear); rel(ss_point);
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

void CEditorShaders11::BindLOD(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_lod);
    ctx->VSSetShader(vs_lod, nullptr, 0);
    ctx->PSSetShader(ps_lod, nullptr, 0);
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

void CEditorShaders11::BindSprite2D(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv)
{
    ctx->IASetInputLayout(il_sprite2d);
    ctx->VSSetShader(vs_sprite2d, nullptr, 0);
    ctx->PSSetShader(ps_sprite2d, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &ss_linear);
    ctx->PSSetShaderResources(0, 1, &srv);
}

void CEditorShaders11::BindParticle(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_particle);
    ctx->VSSetShader(vs_particle, nullptr, 0);
    ctx->PSSetShader(ps_particle, nullptr, 0);
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::BindBaseTex(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_basetex);         // pos+uv (FVF::V)
    ctx->VSSetShader(vs_basetex, nullptr, 0);
    ctx->PSSetShader(ps_basetex, nullptr, 0);  // tex.rgb + ObjectColor.a
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::BindGrassInstanced(ID3D11DeviceContext* ctx)
{
    ctx->IASetInputLayout(il_grass_inst);      // slot0 pos+uv, slot1 per-instance world
    ctx->VSSetShader(vs_grass_inst, nullptr, 0);
    ctx->PSSetShader(ps_detail, nullptr, 0);   // tex*color + alpha-test (reused)
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::SetTexture(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv)
{
    ctx->PSSetShaderResources(0, 1, &srv);
}

void CEditorShaders11::SetDefaultSampler(ID3D11DeviceContext* ctx)
{
    ctx->PSSetSamplers(0, 1, &ss_linear);
}

void CEditorShaders11::SetPointSampler(ID3D11DeviceContext* ctx)
{
    ctx->PSSetSamplers(0, 1, &ss_point);
}
