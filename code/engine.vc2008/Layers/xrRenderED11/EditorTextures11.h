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

    // Load a DDS file by absolute path; returns nullptr on failure (not cached).
    ID3D11ShaderResourceView* LoadDDS(ID3D11Device* dev, const char* path);

    // Release every cached SRV.  Call before destroying the DX11 device.
    void Flush();

    // Monotonic generation; bumped on every Flush().  CSurface caches an SRV
    // pointer together with the generation it was fetched at, and re-fetches
    // when the generation no longer matches (i.e. textures were reloaded).
    u32  Generation() const { return m_generation; }

private:
    xr_map<shared_str, ID3D11ShaderResourceView*> m_cache;
    u32 m_generation = 1; // 0 is reserved for "never fetched" in CSurface
};

extern ECORE_API CEditorTextures11 EditorTextures11;
