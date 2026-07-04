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

    // Animated ("$game_textures$\<name>.seq") textures: push the current editor time (ms) once
    // per frame so Get() returns the correct animation frame. Mirrors the DX9 CTexture::apply_seq
    // path (frame = time / (1000/fps)). Callers must fetch the SRV every frame for animation.
    void SetTime(u32 ms) { m_time = ms; }

private:
    // A .seq sequence: N frame SRVs cycled at 1000/fps ms per frame.
    struct SeqAnim { xr_vector<ID3D11ShaderResourceView*> frames; u32 mspf = 0; bool cycles = false; };

    // Try to load "<name>.seq" as an animation; returns true and fills m_seq[key] on success.
    bool  TryLoadSeq(ID3D11Device* dev, const char* name, const shared_str& key);
    // Current frame SRV of a loaded sequence (by m_time).
    ID3D11ShaderResourceView* SeqFrame(const SeqAnim& a) const;

    xr_map<shared_str, ID3D11ShaderResourceView*> m_cache;
    xr_map<shared_str, SeqAnim>                   m_seq;   // animated textures (by name)
    u32 m_generation = 1; // 0 is reserved for "never fetched" in CSurface
    u32 m_time       = 0; // editor time (ms) for animation frame selection
};

extern ECORE_API CEditorTextures11 EditorTextures11;
