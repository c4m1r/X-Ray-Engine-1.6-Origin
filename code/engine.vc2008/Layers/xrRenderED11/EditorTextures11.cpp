#include "stdafx.h"
#pragma hdrstop

#include "EditorTextures11.h"
#include "HW11.h"

CEditorTextures11 EditorTextures11;

namespace {

static inline void rgb565_to_888(u32 c, u8& r, u8& g, u8& b)
{
    r = u8((((c >> 11) & 31) * 255 + 15) / 31);
    g = u8((((c >> 5)  & 63) * 255 + 31) / 63);
    b = u8((( c        & 31) * 255 + 15) / 31);
}

static void decode_bc_colors(const u8* blk, bool bc1, u8 rgb[16][3])
{
    u32 c0 = blk[0] | (blk[1] << 8);
    u32 c1 = blk[2] | (blk[3] << 8);
    u32 idx = blk[4] | (blk[5] << 8) | (blk[6] << 16) | (blk[7] << 24);
    u8 p[4][3];
    rgb565_to_888(c0, p[0][0], p[0][1], p[0][2]);
    rgb565_to_888(c1, p[1][0], p[1][1], p[1][2]);
    if (!bc1 || c0 > c1) {
        for (int k = 0; k < 3; ++k) {
            p[2][k] = u8((2 * p[0][k] + p[1][k]) / 3);
            p[3][k] = u8((p[0][k] + 2 * p[1][k]) / 3);
        }
    } else {
        for (int k = 0; k < 3; ++k) {
            p[2][k] = u8((p[0][k] + p[1][k]) / 2);
            p[3][k] = 0;
        }
    }
    for (int i = 0; i < 16; ++i) {
        int ci = (idx >> (i * 2)) & 3;
        rgb[i][0] = p[ci][0]; rgb[i][1] = p[ci][1]; rgb[i][2] = p[ci][2];
    }
}

static void decode_bc3_alpha(const u8* blk, u8 a[16])
{
    u8 a0 = blk[0], a1 = blk[1];
    u8 al[8]; al[0] = a0; al[1] = a1;
    if (a0 > a1) { for (int i = 1; i < 7; ++i) al[i + 1] = u8(((7 - i) * a0 + i * a1) / 7); }
    else { for (int i = 1; i < 5; ++i) al[i + 1] = u8(((5 - i) * a0 + i * a1) / 5); al[6] = 0; al[7] = 255; }
    u64 bits = 0; for (int i = 0; i < 6; ++i) bits |= u64(blk[2 + i]) << (8 * i);
    for (int i = 0; i < 16; ++i) a[i] = al[(bits >> (i * 3)) & 7];
}

static bool decode_base_to_rgba(const struct FormatInfo& fi, DXGI_FORMAT fmt,
                                const u8* src, u32 w, u32 h, xr_vector<u8>& out);

static ID3D11ShaderResourceView* create_with_mips(ID3D11Device* dev, const u8* rgba, u32 w, u32 h)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h; td.MipLevels = 0; td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM; td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex))) return nullptr;
    HW11.pContext->UpdateSubresource(tex, 0, nullptr, rgba, w * 4, 0);
    ID3D11ShaderResourceView* srv = nullptr;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &srv);
    if (SUCCEEDED(hr)) HW11.pContext->GenerateMips(srv);
    tex->Release();
    return SUCCEEDED(hr) ? srv : nullptr;
}


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

static bool decode_base_to_rgba(const FormatInfo& fi, DXGI_FORMAT fmt,
                                const u8* src, u32 w, u32 h, xr_vector<u8>& out)
{
    out.resize((size_t)w * h * 4);
    u8* dst = out.data();

    if (fi.is_bc) {
        const bool bc1 = (fi.block_bytes == 8);
        const u32  bw  = (w + 3) / 4;
        const u32  bh  = (h + 3) / 4;
        for (u32 by = 0; by < bh; ++by) {
            for (u32 bx = 0; bx < bw; ++bx) {
                const u8* blk = src + (size_t)(by * bw + bx) * fi.block_bytes;
                const u8* cblk = bc1 ? blk : blk + 8;
                u8 rgb[16][3]; u8 al[16];
                decode_bc_colors(cblk, bc1, rgb);
                if (bc1) {
                    for (int i = 0; i < 16; ++i) al[i] = 255;
                } else if (fi.block_bytes == 16 && fmt == DXGI_FORMAT_BC2_UNORM) {
                    u64 a = 0; for (int i = 0; i < 8; ++i) a |= u64(blk[i]) << (8 * i);
                    for (int i = 0; i < 16; ++i) al[i] = u8(((a >> (i * 4)) & 0xF) * 255 / 15);
                } else {
                    decode_bc3_alpha(blk, al);
                }
                for (int py = 0; py < 4; ++py) {
                    u32 y = by * 4 + py; if (y >= h) break;
                    for (int px = 0; px < 4; ++px) {
                        u32 x = bx * 4 + px; if (x >= w) break;
                        int i = py * 4 + px;
                        u8* d = dst + ((size_t)y * w + x) * 4;
                        d[0] = rgb[i][0]; d[1] = rgb[i][1]; d[2] = rgb[i][2]; d[3] = al[i];
                    }
                }
            }
        }
        return true;
    }

    const bool bgra = (fmt == DXGI_FORMAT_B8G8R8A8_UNORM || fmt == DXGI_FORMAT_B8G8R8X8_UNORM);
    const bool xset = (fmt == DXGI_FORMAT_B8G8R8X8_UNORM);
    for (size_t i = 0; i < (size_t)w * h; ++i) {
        const u8* s = src + i * 4;
        u8* d = dst + i * 4;
        if (bgra) { d[0] = s[2]; d[1] = s[1]; d[2] = s[0]; d[3] = xset ? 255 : s[3]; }
        else      { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; }
    }
    return true;
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

    if (mips <= 1 && (w > 2 || h > 2)) {
        xr_vector<u8> rgba;
        if (decode_base_to_rgba(fi, fi.fmt, data.data(), w, h, rgba)) {
            ID3D11ShaderResourceView* srv = create_with_mips(dev, rgba.data(), w, h);
            if (srv) return srv;
        }
    }

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
