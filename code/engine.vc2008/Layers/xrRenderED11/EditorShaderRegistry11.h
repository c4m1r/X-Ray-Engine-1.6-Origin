#pragma once
#include "EditorTypes11.h"

// Реестр D3D11-шейдеров редактора.
// Заменяет механизм .s-файлов из gamedata для DX11-режима:
// шейдеры регистрируются по имени (как в оригинальной системе X-Ray)
// и возвращаются по запросу без обращения к файловой системе.
//
// Текущие записи: editor\solid, editor\wireframe, editor\colored, editor\prim.
// Будущие записи: particles\blend, particles\add, particles\alpha_add, ...

struct ED11ShaderPipeline
{
    ID3D11PixelShader*  ps = nullptr; // nullptr — шейдер не реализован
    ID3D11BlendState*   bs = nullptr; // nullptr — непрозрачный (opaque)
};

class ECORE_API CEditorShaderRegistry11
{
public:
    void                Add(const char* name, ED11ShaderPipeline pipeline);
    ED11ShaderPipeline* Find(const char* name); // nullptr если не зарегистрирован
    void                Clear();

private:
    xr_map<shared_str, ED11ShaderPipeline> m_db;
};

extern ECORE_API CEditorShaderRegistry11 EditorShaderRegistry11;
