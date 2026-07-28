#pragma once

class IBlender;

enum ED11BlendMode : u8
{
    ED11_BLEND_NONE      = 0,
    ED11_BLEND_ALPHA     = 1,
    ED11_BLEND_ADD       = 2,
    ED11_BLEND_MUL       = 3,
    ED11_BLEND_MUL2X     = 4,
    ED11_BLEND_ALPHA_ADD = 5,
};

struct ED11BlendInfo
{
    bool blend    = false;
    bool atest    = false;
    u32  aref     = 0;
    int  priority = 1;
    bool strict   = false;
    u8   mode     = ED11_BLEND_NONE;
    bool zwrite   = true;
};

class ECORE_API CEditorBlenders11
{
public:
    void OnDeviceCreate();
    void OnDeviceDestroy();

    bool                 IsTransparent(LPCSTR shader_name);
    int                  SurfPriority(LPCSTR shader_name);
    bool                 SurfStrictB2F(LPCSTR shader_name);
    const ED11BlendInfo* Find(LPCSTR shader_name);

private:
    ED11BlendInfo Classify(IBlender* B);

    xr_map<shared_str, ED11BlendInfo> m_info;
};

extern ECORE_API CEditorBlenders11 EditorBlenders11;
