#pragma once
#include "EditorTypes11.h"

class ECORE_API CEditorTextures11
{
public:
    ID3D11ShaderResourceView* Get(ID3D11Device* dev, const char* name);

    ID3D11ShaderResourceView* LoadDDS(ID3D11Device* dev, const char* path);

    void Flush();

    ID3D11ShaderResourceView* Default() const { return m_default_srv; }
    bool CreateDefault(ID3D11Device* dev);
    void ReleaseDefault();

    u32  Generation() const { return m_generation; }

    void SetTime(u32 ms) { m_time = ms; }

private:
    struct SeqAnim { xr_vector<ID3D11ShaderResourceView*> frames; u32 mspf = 0; bool cycles = false; };

    bool  TryLoadSeq(ID3D11Device* dev, const char* name, const shared_str& key);
    ID3D11ShaderResourceView* SeqFrame(const SeqAnim& a) const;

    xr_map<shared_str, ID3D11ShaderResourceView*> m_cache;
    xr_map<shared_str, SeqAnim>                   m_seq;
    ID3D11ShaderResourceView* m_default_srv = nullptr;
    u32 m_generation = 1;
    u32 m_time       = 0;
};

extern ECORE_API CEditorTextures11 EditorTextures11;
