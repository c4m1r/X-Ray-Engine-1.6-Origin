#include "stdafx.h"
#pragma hdrstop

#include "ResourceManager11.h"
#include "HW11.h"
#include <DirectXMath.h>

CResourceManager11 Resources11;

bool CResourceManager11::OnDeviceCreate(ID3D11Device* dev)
{
    if (!CreateConstantBuffers(dev)) return false;
    EditorTextures11.CreateDefault(dev);
    return EditorShaders11.Create(dev);
}

void CResourceManager11::OnDeviceDestroy()
{
    EditorFont11.Destroy();
    EditorTextures11.ReleaseDefault();
    EditorTextures11.Flush();
    EditorShaders11.Destroy();
    EditorShaderRegistry11.Clear();
    if (cb_PerObject) { cb_PerObject->Release(); cb_PerObject = nullptr; }
    if (cb_PerFrame)  { cb_PerFrame->Release();  cb_PerFrame  = nullptr; }
}

bool CResourceManager11::CreateConstantBuffers(ID3D11Device* dev)
{
    auto make = [&](u32 size, ID3D11Buffer** out) -> bool {
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth      = (size + 15) & ~15u;
        bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        bd.Usage          = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        return SUCCEEDED(dev->CreateBuffer(&bd, nullptr, out));
    };
    return make(sizeof(CEditorCB_PerFrame),  &cb_PerFrame)
        && make(sizeof(CEditorCB_PerObject), &cb_PerObject);
}

void CResourceManager11::UploadPerFrame(const float* view4x4, const float* proj4x4, const float* cam_pos3)
{
    CEditorCB_PerFrame cb;
    memcpy(cb.view, view4x4, 64);
    memcpy(cb.proj, proj4x4, 64);
    DirectX::XMMATRIX V = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)view4x4);
    DirectX::XMMATRIX P = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)proj4x4);
    DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)cb.viewproj, DirectX::XMMatrixMultiply(V, P));
    cb.cam_pos[0] = cam_pos3[0]; cb.cam_pos[1] = cam_pos3[1]; cb.cam_pos[2] = cam_pos3[2]; cb.cam_pos[3] = 1.f;

    ID3D11DeviceContext* ctx = HW11.pContext;
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(cb_PerFrame, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) { memcpy(ms.pData, &cb, sizeof(cb)); ctx->Unmap(cb_PerFrame, 0); }
    ctx->VSSetConstantBuffers(0, 1, &cb_PerFrame);
    ctx->PSSetConstantBuffers(0, 1, &cb_PerFrame);
}

void CResourceManager11::UploadPerObject(const float* world4x4, float sel_r, float sel_g, float sel_b, float sel_a)
{
    CEditorCB_PerObject cb;
    memcpy(cb.world, world4x4, 64);
    cb.color[0] = sel_r; cb.color[1] = sel_g; cb.color[2] = sel_b; cb.color[3] = sel_a;

    ID3D11DeviceContext* ctx = HW11.pContext;
    D3D11_MAPPED_SUBRESOURCE ms;
    if (SUCCEEDED(ctx->Map(cb_PerObject, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) { memcpy(ms.pData, &cb, sizeof(cb)); ctx->Unmap(cb_PerObject, 0); }
    ctx->VSSetConstantBuffers(1, 1, &cb_PerObject);
    ctx->PSSetConstantBuffers(1, 1, &cb_PerObject);
}
