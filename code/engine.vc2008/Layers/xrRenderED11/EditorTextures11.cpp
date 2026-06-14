#include "stdafx.h"
#pragma hdrstop

#include "EditorTextures11.h"
#include "HW11.h"

CEditorTextures11 EditorTextures11;

//==================================================================
// Minimal DDS parser — handles DXT1/3/5 and uncompressed 32-bit
//==================================================================
namespace {

#pragma pack(push, 1)
struct DDS_PIXELFORMAT {
    u32 dwSize, dwFlags, dwFourCC;
    u32 dwRGBBitCount;
    u32 dwRBitMask, dwGBitMask, dwBBitMask, dwABitMask;
};
struct DDS_HEADER {
    u32 dwSize, dwFlags, dwHeight, dwWidth;
    u32 dwPitchOrLinearSize, dwDepth, dwMipMapCount;
    u32 dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    u32 dwCaps, dwCaps2, dwCaps3, dwCaps4;
    u32 dwReserved2;
};
#pragma pack(pop)

static const u32 DDS_MAGIC    = 0x20534444u; // "DDS "
static const u32 DDPF_FOURCC  = 0x00000004u;
static const u32 DDPF_RGB     = 0x00000040u;
static const u32 DDPF_ALPHA   = 0x00000002u;

struct FormatInfo {
    DXGI_FORMAT fmt;
    bool        is_bc;
    u32         block_bytes; // BC: bytes per 4×4 block; uncompressed: bytes per pixel
};

static bool GetFormatInfo(const DDS_PIXELFORMAT& pf, FormatInfo& out)
{
    out = {};
    if (pf.dwFlags & DDPF_FOURCC) {
        switch (pf.dwFourCC) {
        case 0x31545844u: /* DXT1 */ out = { DXGI_FORMAT_BC1_UNORM, true,  8 }; return true;
        case 0x33545844u: /* DXT3 */ out = { DXGI_FORMAT_BC2_UNORM, true, 16 }; return true;
        case 0x35545844u: /* DXT5 */ out = { DXGI_FORMAT_BC3_UNORM, true, 16 }; return true;
        default: return false;
        }
    }
    if ((pf.dwFlags & DDPF_RGB) && pf.dwRGBBitCount == 32) {
        if (pf.dwRBitMask == 0x00FF0000u) {
            // ARGB or XRGB
            bool hasAlpha = (pf.dwFlags & DDPF_ALPHA) || (pf.dwABitMask != 0);
            out = { hasAlpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM, false, 4 };
            return true;
        }
        if (pf.dwRBitMask == 0x000000FFu) {
            // ABGR / RGBA
            out = { DXGI_FORMAT_R8G8B8A8_UNORM, false, 4 };
            return true;
        }
    }
    return false;
}

static size_t MipBytes(const FormatInfo& fi, u32 w, u32 h)
{
    if (fi.is_bc) {
        u32 bw = (w + 3) / 4; if (bw < 1) bw = 1;
        u32 bh = (h + 3) / 4; if (bh < 1) bh = 1;
        return (size_t)bw * bh * fi.block_bytes;
    }
    return (size_t)w * h * fi.block_bytes; // block_bytes == bytes_per_pixel
}

} // namespace

//==================================================================
ID3D11ShaderResourceView* CEditorTextures11::LoadDDS(ID3D11Device* dev, const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) return nullptr;

    u32 magic = 0;
    fread(&magic, 4, 1, fp);
    if (magic != DDS_MAGIC) { fclose(fp); return nullptr; }

    DDS_HEADER hdr = {};
    fread(&hdr, sizeof(hdr), 1, fp);
    if (hdr.dwSize != 124) { fclose(fp); return nullptr; }

    FormatInfo fi;
    if (!GetFormatInfo(hdr.ddspf, fi)) { fclose(fp); return nullptr; }

    u32 w    = hdr.dwWidth  ? hdr.dwWidth  : 1;
    u32 h    = hdr.dwHeight ? hdr.dwHeight : 1;
    u32 mips = hdr.dwMipMapCount ? hdr.dwMipMapCount : 1;

    // Calculate total data size across all mips
    size_t total = 0;
    { u32 mw = w, mh = h;
      for (u32 m = 0; m < mips; ++m) {
          total += MipBytes(fi, mw, mh);
          mw = mw > 1 ? mw >> 1 : 1;
          mh = mh > 1 ? mh >> 1 : 1;
      } }

    xr_vector<u8> data(total);
    size_t read = fread(data.data(), 1, total, fp);
    fclose(fp);
    if (read != total) return nullptr;

    // Build per-mip subresource descriptors
    xr_vector<D3D11_SUBRESOURCE_DATA> srd(mips);
    const u8* ptr = data.data();
    { u32 mw = w, mh = h;
      for (u32 m = 0; m < mips; ++m) {
          if (fi.is_bc) {
              u32 bw = (mw + 3) / 4; if (bw < 1) bw = 1;
              u32 bh = (mh + 3) / 4; if (bh < 1) bh = 1;
              srd[m].SysMemPitch      = bw * fi.block_bytes;
              srd[m].SysMemSlicePitch = (UINT)(srd[m].SysMemPitch * bh);
          } else {
              srd[m].SysMemPitch      = mw * fi.block_bytes;
              srd[m].SysMemSlicePitch = (UINT)(srd[m].SysMemPitch * mh);
          }
          srd[m].pSysMem = ptr;
          ptr += MipBytes(fi, mw, mh);
          mw = mw > 1 ? mw >> 1 : 1;
          mh = mh > 1 ? mh >> 1 : 1;
      } }

    D3D11_TEXTURE2D_DESC td = {};
    td.Width            = w;
    td.Height           = h;
    td.MipLevels        = mips;
    td.ArraySize        = 1;
    td.Format           = fi.fmt;
    td.SampleDesc.Count = 1;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;
    td.Usage            = D3D11_USAGE_IMMUTABLE;

    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, srd.data(), &tex)))
        return nullptr;

    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
    tex->Release();
    return SUCCEEDED(hr) ? srv : nullptr;
}

//==================================================================
ID3D11ShaderResourceView* CEditorTextures11::Get(ID3D11Device* dev, const char* name)
{
    if (!name || !name[0] || !dev) return HW11.pDefaultSRV;

    shared_str key(name);
    auto it = m_cache.find(key);  // xr_map<shared_str,...>: uses shared_str::operator<
    if (it != m_cache.end())
        return it->second ? it->second : HW11.pDefaultSRV;

    // Build file path via FS
    string_path tex_path;
    FS.update_path(tex_path, "$game_textures$", name);
    xr_strcat(tex_path, ".dds");

    ID3D11ShaderResourceView* srv = nullptr;
    if (FS.exist(tex_path))
        srv = LoadDDS(dev, tex_path);

    // Cache the result (nullptr means "tried and failed")
    m_cache.emplace(key, srv);

    if (!srv)
        Msg("~ DX11 tex: missing '%s'", name);

    return srv ? srv : HW11.pDefaultSRV;
}

//==================================================================
void CEditorTextures11::Flush()
{
    for (auto& [k, srv] : m_cache)
        if (srv) srv->Release();
    m_cache.clear();
}
