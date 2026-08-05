#pragma once
#include "EditorTypes11.h"
#include "EditorShaders11.h"
#include "EditorTextures11.h"
#include "EditorShaderRegistry11.h"
#include "EditorBlenders11.h"
#include "EditorFont11.h"

class ECORE_API CResourceManager11
{
public:
    bool OnDeviceCreate(ID3D11Device* dev);
    void OnDeviceDestroy();

    CEditorShaders11&        Shaders()        { return EditorShaders11;        }
    CEditorTextures11&       Textures()       { return EditorTextures11;       }
    CEditorShaderRegistry11& ShaderRegistry() { return EditorShaderRegistry11; }
    CEditorBlenders11&       Blenders()       { return EditorBlenders11;       }
    CEditorFont11&           Font()           { return EditorFont11;           }

    ID3D11Buffer* cb_PerFrame   = nullptr;
    ID3D11Buffer* cb_PerObject  = nullptr;
    ID3D11Buffer* cb_SurfParams = nullptr;
    void UploadPerFrame(const float* view4x4, const float* proj4x4, const float* cam_pos3);
    void UploadPerObject(const float* world4x4, float sel_r, float sel_g, float sel_b, float sel_a);
    void UploadSurfParams(float aref, float env = 0.f);
    float m_cur_aref = -1.f;
    float m_cur_env  = -1.f;

private:
    bool CreateConstantBuffers(ID3D11Device* dev);
};

extern ECORE_API CResourceManager11 Resources11;
