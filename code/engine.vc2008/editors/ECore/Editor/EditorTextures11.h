#pragma once
#include "EditorTypes11.h"

// On-demand DDS texture loader and SRV cache for the DX11 editor renderer.
// Textures are loaded from $game_textures$ on the first Get() call and kept
// alive until Flush().  The cache key is the name as stored in CSurface
// (no extension, directory separator may be backslash or forward-slash).
class ECORE_API CEditorTextures11
{
public:
    // Return a cached (or freshly loaded) SRV for the given texture name.
    // Returns HW11.pDefaultSRV (1×1 white) if the file is missing or broken.
    ID3D11ShaderResourceView* Get(ID3D11Device* dev, const char* name);

    // Release every cached SRV.  Call before destroying the DX11 device.
    void Flush();

private:
    xr_map<shared_str, ID3D11ShaderResourceView*> m_cache;

    ID3D11ShaderResourceView* LoadDDS(ID3D11Device* dev, const char* path);
};

extern ECORE_API CEditorTextures11 EditorTextures11;
