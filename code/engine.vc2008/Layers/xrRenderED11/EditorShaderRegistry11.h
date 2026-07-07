#pragma once
#include "EditorTypes11.h"


struct ED11ShaderPipeline
{
    ID3D11PixelShader*  ps = nullptr;
    ID3D11BlendState*   bs = nullptr;
};

class ECORE_API CEditorShaderRegistry11
{
public:
    void                Add(const char* name, ED11ShaderPipeline pipeline);
    ED11ShaderPipeline* Find(const char* name);
    void                Clear();

private:
    xr_map<shared_str, ED11ShaderPipeline> m_db;
};

extern ECORE_API CEditorShaderRegistry11 EditorShaderRegistry11;
