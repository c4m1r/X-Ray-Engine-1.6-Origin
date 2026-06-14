#pragma once
// DX11 shader set for editor rendering.
// All shaders compiled at device creation from embedded HLSL source.

#include "EditorTypes11.h"
#include "HW11.h"
// d3dcompiler.h is included only in EditorShaders11.cpp — conflicts with xrD3dDefs.h aliases in PCH.

//------------------------------------------------------------------
class ECORE_API CEditorShaders11
{
public:
    CEditorShaders11()  = default;
    ~CEditorShaders11() = default;

    bool Create(ID3D11Device* dev);
    void Destroy();

    // Bind the solid (textured) shader set
    void BindSolid(ID3D11DeviceContext* ctx);
    // Bind the wireframe shader set
    void BindWireframe(ID3D11DeviceContext* ctx);
    // Bind the solid colored (no texture) shader set — gizmos/helpers
    void BindColored(ID3D11DeviceContext* ctx);
    // Bind instanced solid shader (takes instance buffer at slot 1)
    void BindInstanced(ID3D11DeviceContext* ctx);
    // Bind per-vertex color primitive shader — 3D world-space (no World matrix, uses ViewProj)
    void BindPrim3D(ID3D11DeviceContext* ctx);
    // Bind per-vertex color primitive shader — 2D NDC passthrough (for screen-space overlays)
    void BindPrim2D(ID3D11DeviceContext* ctx);
    // Bind 2D textured sprite shader and set srv into slot 0
    void BindSprite2D(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv);

    // Set active texture (SRV slot 0)
    void SetTexture(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv);
    // Set default linear sampler (already set after BindSolid, but can re-set)
    void SetDefaultSampler(ID3D11DeviceContext* ctx);

    // Input layouts
    ID3D11InputLayout*  il_solid        = nullptr;  // pos+normal+uv
    ID3D11InputLayout*  il_instanced    = nullptr;  // pos+normal+uv + per-instance world+color
    ID3D11InputLayout*  il_colored      = nullptr;  // pos only (for helpers)
    ID3D11InputLayout*  il_prim         = nullptr;  // pos(float3) + color(B8G8R8A8) = FVF::L layout
    ID3D11InputLayout*  il_sprite2d     = nullptr;  // float2 pos + float2 uv + B8G8R8A8 color = SpriteVert2D

    // Vertex shaders
    ID3D11VertexShader* vs_solid        = nullptr;
    ID3D11VertexShader* vs_wireframe    = nullptr;
    ID3D11VertexShader* vs_colored      = nullptr;
    ID3D11VertexShader* vs_instanced    = nullptr;
    ID3D11VertexShader* vs_prim         = nullptr;  // 3D: mul(float4(pos,1), ViewProj)
    ID3D11VertexShader* vs_prim2d       = nullptr;  // 2D: float4(pos.xy, 0, 1)
    ID3D11VertexShader* vs_sprite2d     = nullptr;  // 2D NDC + UV passthrough

    // Pixel shaders
    ID3D11PixelShader*  ps_solid             = nullptr;  // texture * diffuse
    ID3D11PixelShader*  ps_wireframe         = nullptr;  // flat color
    ID3D11PixelShader*  ps_colored           = nullptr;  // vertex/constant color
    ID3D11PixelShader*  ps_prim              = nullptr;  // per-vertex color passthrough
    ID3D11PixelShader*  ps_instanced         = nullptr;  // texture + per-instance color tint
    ID3D11PixelShader*  ps_inst_transparent  = nullptr;  // instanced без clip() — для стёкол/прозрачных
    ID3D11PixelShader*  ps_sprite2d          = nullptr;  // texture * vertex color, 2D

    // Blend states
    ID3D11BlendState*   bs_alpha        = nullptr;  // src-alpha / inv-src-alpha
    ID3D11BlendState*   bs_additive     = nullptr;  // src-alpha / one — for glow/particle sprites

    // Shared sampler
    ID3D11SamplerState* ss_linear       = nullptr;

    static ID3DBlob* CompileShader(const char* src, const char* entry,
                                    const char* profile, const char* debug_name);
};

extern ECORE_API CEditorShaders11 EditorShaders11;
