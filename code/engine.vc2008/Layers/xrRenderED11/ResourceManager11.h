#pragma once
#include "EditorTypes11.h"
#include "EditorShaders11.h"
#include "EditorTextures11.h"
#include "EditorShaderRegistry11.h"
#include "EditorFont11.h"

// Central owner/cache of the DX11 editor GPU resources — the DX11 counterpart of the D3D9
// CResourceManager. Pragmatic subset: the fixed-function editor render has no blenders / passes /
// shader scripts (.s) / RT pool / constant tables, so those are intentionally absent. It owns the
// GPU-resource subsystems and drives their device lifecycle from one place (OnDeviceCreate /
// OnDeviceDestroy), mirroring CResourceManager::OnDeviceCreate/OnDeviceDestroy.
//
// Migration is phased:
//   Phase 1: owns shaders + textures.
//   Phase 2 (current): also owns the shader registry (name→pipeline) and the editor font.
//   Later phases fold in states, constant buffers and the reusable geometry buffers now in CHW11,
//   and the Draw* helpers move to the render entry points using this manager.
class ECORE_API CResourceManager11
{
public:
    // Create device-dependent resources (shaders compile here; textures/font load on demand).
    bool OnDeviceCreate(ID3D11Device* dev);
    // Release every device-dependent resource.
    void OnDeviceDestroy();

    // Accessors to the real global subsystems (each defined in its own .cpp — a robust cross-BPL
    // export, unlike an exported reference). The manager orchestrates their lifecycle; it does not
    // own them. Existing EditorShaders11. / EditorTextures11. call sites keep working unchanged.
    CEditorShaders11&        Shaders()        { return EditorShaders11;        }
    CEditorTextures11&       Textures()       { return EditorTextures11;       }
    CEditorShaderRegistry11& ShaderRegistry() { return EditorShaderRegistry11; }
    CEditorFont11&           Font()           { return EditorFont11;           }

    // Owned by the manager (ECore-internal usage only): per-frame (b0) / per-object (b1) constant
    // buffers. Created in OnDeviceCreate; filled + bound by the Upload* helpers (CHW11 context).
    ID3D11Buffer* cb_PerFrame  = nullptr;
    ID3D11Buffer* cb_PerObject = nullptr;
    // Upload view/proj/viewproj + camera pos into b0 and bind it to VS+PS.
    void UploadPerFrame(const float* view4x4, const float* proj4x4, const float* cam_pos3);
    // Upload world matrix + tint (rgb, selection-blend a) into b1 and bind it to VS+PS.
    void UploadPerObject(const float* world4x4, float sel_r, float sel_g, float sel_b, float sel_a);

private:
    bool CreateConstantBuffers(ID3D11Device* dev);
};

extern ECORE_API CResourceManager11 Resources11;
