#include "stdafx.h"
#pragma hdrstop

#include "EditorTextures11.h"

CEditorTextures11 EditorTextures11;

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

static const u32 DDS_MAGIC    = 0x20534444u;
static const u32 DDPF_FOURCC  = 0x00000004u;
static const u32 DDPF_RGB     = 0x00000040u;
static const u32 DDPF_ALPHA   = 0x00000002u;

struct FormatInfo {
    DXGI_FORMAT fmt;
    bool        is_bc;
    u32         block_bytes;
};

static bool GetFormatInfo(const DDS_PIXELFORMAT& pf, FormatInfo& out)
{
    out = {};
    if (pf.dwFlags & DDPF_FOURCC) {
        switch (pf.dwFourCC) {
        case 0x31545844u: out = { DXGI_FORMAT_BC1_UNORM, true,  8 }; return true;
        case 0x33545844u: out = { DXGI_FORMAT_BC2_UNORM, true, 16 }; return true;
        case 0x35545844u: out = { DXGI_FORMAT_BC3_UNORM, true, 16 }; return true;
        default: return false;
        }
    }
    if ((pf.dwFlags & DDPF_RGB) && pf.dwRGBBitCount == 32) {
        if (pf.dwRBitMask == 0x00FF0000u) {
            bool hasAlpha = (pf.dwFlags & DDPF_ALPHA) || (pf.dwABitMask != 0);
            out = { hasAlpha ? DXGI_FORMAT_B8G8R8A8_UNORM : DXGI_FORMAT_B8G8R8X8_UNORM, false, 4 };
            return true;
        }
        if (pf.dwRBitMask == 0x000000FFu) {
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
    return (size_t)w * h * fi.block_bytes;
}

}

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

ID3D11ShaderResourceView* CEditorTextures11::SeqFrame(const SeqAnim& a) const
{
    const u32 n = (u32)a.frames.size();
    if (!n) return m_default_srv;
    const u32 f  = a.mspf ? (m_time / a.mspf) : 0;
    const u32 id = f % n;
    return a.frames[id] ? a.frames[id] : m_default_srv;
}

bool CEditorTextures11::TryLoadSeq(ID3D11Device* dev, const char* name, const shared_str& key)
{
    string_path seq_path;
    FS.update_path(seq_path, "$game_textures$", name);
    xr_strcat(seq_path, ".seq");
    if (!FS.exist(seq_path)) return false;

    IReader* fs = FS.r_open(seq_path);
    if (!fs) return false;

    SeqAnim anim;
    string256 buffer;
    fs->r_string(buffer, sizeof(buffer));
    if (0 == stricmp(buffer, "cycled")) { anim.cycles = true; fs->r_string(buffer, sizeof(buffer)); }
    int fps = atoi(buffer); if (fps < 1) fps = 1;
    anim.mspf = 1000 / fps;

    while (!fs->eof())
    {
        fs->r_string(buffer, sizeof(buffer));
        _Trim(buffer);
        if (!buffer[0]) continue;
        {
            char* dot = strrchr(buffer, '.');
            char* sl1 = strrchr(buffer, '\\');
            char* sl2 = strrchr(buffer, '/');
            char* sl  = sl1 > sl2 ? sl1 : sl2;
            if (dot && dot > sl) *dot = 0;
        }
        string_path fp;
        FS.update_path(fp, "$game_textures$", buffer);
        xr_strcat(fp, ".dds");
        anim.frames.push_back(FS.exist(fp) ? LoadDDS(dev, fp) : nullptr);
    }
    FS.r_close(fs);

    if (anim.frames.empty()) return false;
    m_seq.emplace(key, std::move(anim));
    return true;
}

ID3D11ShaderResourceView* CEditorTextures11::Get(ID3D11Device* dev, const char* name)
{
    if (!name || !name[0] || !dev) return m_default_srv;

    shared_str key(name);

    auto sit = m_seq.find(key);
    if (sit != m_seq.end())
        return SeqFrame(sit->second);

    auto it = m_cache.find(key);
    if (it != m_cache.end())
        return it->second ? it->second : m_default_srv;

    if (TryLoadSeq(dev, name, key))
        return SeqFrame(m_seq[key]);

    string_path tex_path;
    FS.update_path(tex_path, "$game_textures$", name);
    xr_strcat(tex_path, ".dds");

    ID3D11ShaderResourceView* srv = nullptr;
    if (FS.exist(tex_path))
        srv = LoadDDS(dev, tex_path);

    m_cache.emplace(key, srv);

    if (!srv)
        ELog.Msg(mtError, "DX11 texture missing: '%s'", name);

    return srv ? srv : m_default_srv;
}

void CEditorTextures11::Flush()
{
    for (auto& [k, srv] : m_cache)
        if (srv) srv->Release();
    m_cache.clear();
    for (auto& [k, a] : m_seq)
        for (auto* srv : a.frames)
            if (srv) srv->Release();
    m_seq.clear();
    ++m_generation;
}

bool CEditorTextures11::CreateDefault(ID3D11Device* dev)
{
    if (m_default_srv) return true;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = td.Height        = 1;
    td.MipLevels = td.ArraySize = 1;
    td.Format                   = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count         = 1;
    td.BindFlags                = D3D11_BIND_SHADER_RESOURCE;
    td.Usage                    = D3D11_USAGE_IMMUTABLE;
    u32 white                   = 0xFFFFFFFF;
    D3D11_SUBRESOURCE_DATA sd   = { &white, 4, 0 };
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &m_default_srv);
    tex->Release();
    return SUCCEEDED(hr);
}

void CEditorTextures11::ReleaseDefault()
{
    if (m_default_srv) { m_default_srv->Release(); m_default_srv = nullptr; }
}
