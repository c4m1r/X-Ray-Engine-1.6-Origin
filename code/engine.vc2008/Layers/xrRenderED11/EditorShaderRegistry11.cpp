#include "stdafx.h"
#pragma hdrstop

#include "EditorShaderRegistry11.h"

CEditorShaderRegistry11 EditorShaderRegistry11;

void CEditorShaderRegistry11::Add(const char* name, ED11ShaderPipeline pipeline)
{
    m_db[name] = pipeline;
}

ED11ShaderPipeline* CEditorShaderRegistry11::Find(const char* name)
{
    auto it = m_db.find(name);
    return (it != m_db.end()) ? &it->second : nullptr;
}

void CEditorShaderRegistry11::Clear()
{
    m_db.clear();
}
