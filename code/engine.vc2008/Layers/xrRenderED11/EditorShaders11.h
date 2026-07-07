#pragma once

#include "EditorTypes11.h"
#include "HW11.h"

class ECORE_API CEditorShaders11
{
public:
    CEditorShaders11()  = default;
    ~CEditorShaders11() = default;

    bool Create(ID3D11Device* dev);
    void Destroy();

    void BindSolid(ID3D11DeviceContext* ctx);
    void BindWireframe(ID3D11DeviceContext* ctx);
    void BindColored(ID3D11DeviceContext* ctx);
    void BindInstanced(ID3D11DeviceContext* ctx);
    void BindLOD(ID3D11DeviceContext* ctx);
    void BindPrim3D(ID3D11DeviceContext* ctx);
    void BindPrim2D(ID3D11DeviceContext* ctx);
    void BindSprite2D(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv);
    void BindParticle(ID3D11DeviceContext* ctx);
    void BindBaseTex(ID3D11DeviceContext* ctx);
    void BindGrassInstanced(ID3D11DeviceContext* ctx);

    void SetTexture(ID3D11DeviceContext* ctx, ID3D11ShaderResourceView* srv);
    void SetDefaultSampler(ID3D11DeviceContext* ctx);
    void SetPointSampler(ID3D11DeviceContext* ctx);

    ID3D11VertexShader* vs_solid              = nullptr;
    ID3D11VertexShader* vs_wireframe          = nullptr;
    ID3D11VertexShader* vs_colored            = nullptr;
    ID3D11VertexShader* vs_instanced          = nullptr;
    ID3D11VertexShader* vs_prim               = nullptr;
    ID3D11VertexShader* vs_prim2d             = nullptr;
    ID3D11VertexShader* vs_sprite2d           = nullptr;
    ID3D11VertexShader* vs_lod                = nullptr;
    ID3D11VertexShader* vs_particle           = nullptr;
    ID3D11VertexShader* vs_basetex            = nullptr;
    ID3D11VertexShader* vs_grass_inst         = nullptr;

    ID3D11PixelShader*  ps_solid              = nullptr;
    ID3D11PixelShader*  ps_wireframe          = nullptr;
    ID3D11PixelShader*  ps_colored            = nullptr;
    ID3D11PixelShader*  ps_prim               = nullptr;
    ID3D11PixelShader*  ps_instanced          = nullptr;
    ID3D11PixelShader*  ps_inst_transparent   = nullptr;
    ID3D11PixelShader*  ps_sprite2d           = nullptr;
    ID3D11PixelShader*  ps_lod                = nullptr;
    ID3D11PixelShader*  ps_particle           = nullptr;
    ID3D11PixelShader*  ps_detail             = nullptr;
    ID3D11PixelShader*  ps_basetex            = nullptr;

    ID3D11InputLayout*  il_solid              = nullptr;
    ID3D11InputLayout*  il_instanced          = nullptr;
    ID3D11InputLayout*  il_colored            = nullptr;
    ID3D11InputLayout*  il_prim               = nullptr;
    ID3D11InputLayout*  il_sprite2d           = nullptr;
    ID3D11InputLayout*  il_lod                = nullptr;
    ID3D11InputLayout*  il_particle           = nullptr;
    ID3D11InputLayout*  il_basetex            = nullptr;
    ID3D11InputLayout*  il_grass_inst         = nullptr;

    ID3D11BlendState*   bs_alpha              = nullptr;
    ID3D11BlendState*   bs_additive           = nullptr;

    ID3D11SamplerState* ss_linear             = nullptr;
    ID3D11SamplerState* ss_point              = nullptr;

    static ID3DBlob* CompileShader(const char* src, const char* entry,
                                    const char* profile, const char* debug_name);
};

extern ECORE_API CEditorShaders11 EditorShaders11;
