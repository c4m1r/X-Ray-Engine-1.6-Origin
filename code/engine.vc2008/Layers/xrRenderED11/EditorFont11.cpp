#include "stdafx.h"
#pragma hdrstop

#include "EditorFont11.h"
#include "HW11.h"
#include "EditorTextures11.h"
#include "EditorD3DCompileSupport.h"

#pragma comment(lib, "d3dcompiler.lib")

//==================================================================
// Implementation
//==================================================================

CEditorFont11 EditorFont11;   // real global; lifecycle driven via CResourceManager11 (Resources11)

static ID3DBlob* FontCompile(const char* hlsl_name, const char* entry, const char* profile)
{
    string_path path;
    FS.update_path(path, "$game_shaders$", "ed11\\");
    xr_strcat(path, hlsl_name);

    if (!FS.exist(path)) {
        ELog.DlgMsg(mtError, "DX11 font shader not found: %s", path);
        return nullptr;
    }

    FILE* fp = fopen(path, "rb");
    if (!fp) {
        ELog.DlgMsg(mtError, "DX11 font shader open failed: %s", path);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    size_t sz = (size_t)ftell(fp);
    fseek(fp, 0, SEEK_SET);
    xr_vector<char> src(sz + 1);
    fread(src.data(), 1, sz, fp);
    fclose(fp);
    src[sz] = 0;

    ID3DBlob* code = nullptr; ID3DBlob* errs = nullptr;
    UINT f = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef DEBUG
    f |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    f |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
    HRESULT hr = D3DCompile(src.data(), sz, hlsl_name, nullptr, nullptr, entry, profile, f, 0, &code, &errs);
    if (FAILED(hr)) {
        if (errs) { ELog.DlgMsg(mtError, "DX11 font shader '%s' error:\n%s", hlsl_name, (char*)errs->GetBufferPointer()); errs->Release(); }
        return nullptr;
    }
    if (errs) errs->Release();
    return code;
}

bool CEditorFont11::BuildAtlas(ID3D11Device* dev, const char* face, int height)
{
    HDC hRef = CreateCompatibleDC(nullptr);
    if (!hRef) return false;

    LOGFONTA lf = {};
    lf.lfHeight   = -(int)height;
    lf.lfWeight   = FW_NORMAL;
    lf.lfCharSet  = ANSI_CHARSET;
    lf.lfQuality  = NONANTIALIASED_QUALITY;
    strncpy_s(lf.lfFaceName, face, LF_FACESIZE - 1);
    HFONT hFont = CreateFontIndirectA(&lf);
    if (!hFont) { DeleteDC(hRef); return false; }
    HFONT hOld = (HFONT)SelectObject(hRef, hFont);

    TEXTMETRICA tm = {};
    GetTextMetricsA(hRef, &tm);
    m_cellH = tm.tmHeight + 1;  // +1px bottom padding
    m_cellW = 0;
    for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
        SIZE sz = {}; char ch = (char)c;
        GetTextExtentPoint32A(hRef, &ch, 1, &sz);
        if (sz.cx > m_cellW) m_cellW = sz.cx;
    }
    m_cellW += 2;  // 1px left+right padding
    SelectObject(hRef, hOld);
    DeleteDC(hRef);

    m_atlasW = m_cellW * COLS;
    m_atlasH = m_cellH * ROWS;
    m_lineH  = (float)m_cellH;

    // GDI rendering
    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(bih); bih.biWidth = m_atlasW; bih.biHeight = -m_atlasH;
    bih.biPlanes = 1; bih.biBitCount = 32; bih.biCompression = BI_RGB;
    void* pBits = nullptr;
    HBITMAP hBmp = CreateDIBSection(nullptr, (BITMAPINFO*)&bih, DIB_RGB_COLORS, &pBits, nullptr, 0);
    if (!hBmp) { DeleteObject(hFont); return false; }

    HDC hDC = CreateCompatibleDC(nullptr);
    SelectObject(hDC, hBmp);
    SelectObject(hDC, hFont);
    RECT rc = { 0, 0, m_atlasW, m_atlasH };
    FillRect(hDC, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    SetBkMode(hDC, TRANSPARENT);
    SetTextColor(hDC, RGB(255, 255, 255));

    for (int c = FIRST_CHAR; c <= LAST_CHAR; ++c) {
        int i = c - FIRST_CHAR;
        int cx = (i % COLS) * m_cellW + 1;
        int cy = (i / COLS) * m_cellH;
        char ch = (char)c;
        TextOutA(hDC, cx, cy, &ch, 1);
    }
    GdiFlush();

    // Extract R channel → R8
    xr_vector<u8> r8((size_t)m_atlasW * m_atlasH);
    u32* src32 = (u32*)pBits;
    for (int i = 0; i < m_atlasW * m_atlasH; ++i)
        r8[i] = (u8)(src32[i] & 0xFF); // BGRX→ B channel (white text → 0xFF)

    DeleteDC(hDC);
    DeleteObject(hBmp);
    DeleteObject(hFont);

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = (UINT)m_atlasW; td.Height = (UINT)m_atlasH;
    td.MipLevels = td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA sd = { r8.data(), (UINT)m_atlasW, 0 };
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, &sd, &tex))) return false;
    HRESULT hr = dev->CreateShaderResourceView(tex, nullptr, &m_atlasSRV);
    tex->Release();
    return SUCCEEDED(hr);
}

bool CEditorFont11::BuildPipeline(ID3D11Device* dev)
{
    // VS
    ID3DBlob* bVS = FontCompile("vs_font.hlsl", "main", "vs_5_0");
    if (!bVS) return false;
    HRESULT hr = dev->CreateVertexShader(bVS->GetBufferPointer(), bVS->GetBufferSize(), nullptr, &m_vs);
    if (SUCCEEDED(hr)) {
        // pos(2f) + uv(2f) + color(B8G8R8A8_UNORM → float4 in shader with correct RGBA order)
        D3D11_INPUT_ELEMENT_DESC ild[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_B8G8R8A8_UNORM,  0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        hr = dev->CreateInputLayout(ild, _countof(ild), bVS->GetBufferPointer(), bVS->GetBufferSize(), &m_il);
    }
    bVS->Release();
    if (FAILED(hr)) return false;

    // PS
    ID3DBlob* bPS = FontCompile("ps_font.hlsl", "main", "ps_5_0");
    if (!bPS) return false;
    hr = dev->CreatePixelShader(bPS->GetBufferPointer(), bPS->GetBufferSize(), nullptr, &m_ps);
    bPS->Release();
    if (FAILED(hr)) return false;

    // viewport CB (slot b2)
    D3D11_BUFFER_DESC cbd = {};
    cbd.ByteWidth = 16; cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&cbd, nullptr, &m_cbVP))) return false;

    // dynamic VB
    D3D11_BUFFER_DESC vbd = {};
    vbd.ByteWidth = MAX_CHARS_PER_FLUSH * 6 * sizeof(Vtx2D);
    vbd.Usage = D3D11_USAGE_DYNAMIC; vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&vbd, nullptr, &m_vb))) return false;

    // sampler: nearest-neighbor, clamp
    D3D11_SAMPLER_DESC ss = {};
    ss.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    ss.AddressU = ss.AddressV = ss.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    ss.MaxLOD = D3D11_FLOAT32_MAX; ss.ComparisonFunc = D3D11_COMPARISON_NEVER;
    if (FAILED(dev->CreateSamplerState(&ss, &m_ss))) return false;

    // blend: src-alpha
    D3D11_BLEND_DESC bd = {};
    auto& rt = bd.RenderTarget[0];
    rt.BlendEnable = TRUE;
    rt.SrcBlend = D3D11_BLEND_SRC_ALPHA; rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA; rt.BlendOp = D3D11_BLEND_OP_ADD;
    rt.SrcBlendAlpha = D3D11_BLEND_ONE;  rt.DestBlendAlpha = D3D11_BLEND_ZERO;     rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
    rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    if (FAILED(dev->CreateBlendState(&bd, &m_bs))) return false;

    // depth-stencil: all off
    D3D11_DEPTH_STENCIL_DESC dsd = {};
    dsd.DepthEnable = FALSE; dsd.StencilEnable = FALSE;
    if (FAILED(dev->CreateDepthStencilState(&dsd, &m_dss))) return false;

    return true;
}

bool CEditorFont11::Create(ID3D11Device* dev, const char* face, int height)
{
    if (!BuildAtlas(dev, face, height))   return false;
    if (!BuildPipeline(dev))             { Destroy(); return false; }
    Msg("* DX11 editor font '%s' %dpx, atlas %dx%d", face, height, m_atlasW, m_atlasH);
    return true;
}

bool CEditorFont11::CreateFromGameSection(ID3D11Device* dev, const char* section_name)
{
    if (!pSettings || !pSettings->section_exist(section_name)) {
        Msg("~ DX11 font: pSettings null or section '%s' missing — using GDI fallback", section_name);
        return Create(dev);
    }

    LPCSTR tex_name = pSettings->r_string(section_name, "texture");

    // Strip extension
    string_path tex_base;
    xr_strcpy(tex_base, sizeof(tex_base), tex_name);
    if (strext(tex_base)) *strext(tex_base) = 0;

    // Find files
    string_path fn_dds, fn_ini;
    if (!FS.exist(fn_dds, "$game_textures$", tex_base, ".dds") ||
        !FS.exist(fn_ini, "$game_textures$", tex_base, ".ini")) {
        Msg("~ DX11 font: texture/ini not found for '%s' (tex='%s') — using GDI fallback", section_name, tex_name);
        return Create(dev);
    }

    // Load texture
    Msg("* DX11 font: loading '%s' (tex='%s')", section_name, tex_name);
    m_atlasSRV = EditorTextures11.LoadDDS(dev, fn_dds);
    if (!m_atlasSRV) {
        Msg("~ DX11 font: DDS load failed for '%s' — using GDI fallback", fn_dds);
        return Create(dev);
    }

    // Query texture size and format
    DXGI_FORMAT tex_fmt = DXGI_FORMAT_UNKNOWN;
    {
        ID3D11Resource* res = nullptr;
        m_atlasSRV->GetResource(&res);
        if (res) {
            ID3D11Texture2D* t2d = nullptr;
            res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&t2d);
            if (t2d) {
                D3D11_TEXTURE2D_DESC d = {};
                t2d->GetDesc(&d);
                m_tex_w  = (float)d.Width;
                m_tex_h  = (float)d.Height;
                tex_fmt  = d.Format;
                t2d->Release();
            }
            res->Release();
        }
    }
    if (m_tex_w <= 0 || m_tex_h <= 0) { Destroy(); return Create(dev); }
    Msg("* DX11 font: tex %dx%d fmt=%d", (int)m_tex_w, (int)m_tex_h, (int)tex_fmt);

    CInifile* ini = CInifile::Create(fn_ini);
    if (!ini) { Destroy(); return Create(dev); }

    // native_h_px — высота символа в пикселях текстуры после парсинга
    float native_h_px = 0;
    char key[8];

    // Game font INI coords are normalized UVs (0-1) if value <= 1.0,
    // or texture pixels if value > 1.0. Detect per section.
    if (ini->section_exist("symbol_coords")) {
        float h  = ini->r_float("symbol_coords", "height");
        bool  px = h > 1.0f;  // pixel coords if height > 1
        float uv_h = px ? h / m_tex_h : h;
        native_h_px = uv_h * m_tex_h;

        for (int i = 0; i < 256; i++) {
            xr_sprintf(key, sizeof(key), "%03d", i);
            if (!ini->line_exist("symbol_coords", key)) continue;
            Fvector v = ini->r_fvector3("symbol_coords", key);
            float u0 = px ? v.x / m_tex_w : v.x;
            float v0 = px ? v.y / m_tex_h : v.y;
            float u1 = px ? v.z / m_tex_w : v.z;
            // w = native pixel width of this glyph
            m_glyphs[i] = { u0, v0, u1, v0 + uv_h, (u1 - u0) * m_tex_w };
        }
    } else if (ini->section_exist("char widths")) {
        float h   = ini->r_float("char widths", "height");
        bool  px  = h > 1.0f;
        float uv_h = px ? h / m_tex_h : h;
        native_h_px = uv_h * m_tex_h;
        const int cpl = 16;

        for (int i = 0; i < 256; i++) {
            xr_sprintf(key, sizeof(key), "%d", i);
            if (!ini->line_exist("char widths", key)) continue;
            float w    = ini->r_float("char widths", key);
            float step = px ? h / m_tex_w : h;  // column step in UV (cells are square)
            float uv_w = px ? w / m_tex_w : w;
            float tx   = (i % cpl) * step;
            float ty   = (i / cpl) * uv_h;
            m_glyphs[i] = { tx, ty, tx + uv_w, ty + uv_h, uv_w * m_tex_w };
        }
    } else if (ini->section_exist("font_size")) {
        float h     = ini->r_float("font_size", "height");
        float width = ini->r_float("font_size", "width");
        int   cpl   = ini->r_s32  ("font_size", "cpl");
        bool  px    = h > 1.0f;
        float uv_h  = px ? h / m_tex_h : h;
        float uv_w  = px ? width / m_tex_w : width;
        native_h_px = uv_h * m_tex_h;

        for (int i = 0; i < 256; i++) {
            float tx = (i % cpl) * uv_w;
            float ty = (i / cpl) * uv_h;
            m_glyphs[i] = { tx, ty, tx + uv_w, ty + uv_h, uv_w * m_tex_w };
        }
    } else {
        Msg("~ DX11 font: no recognised section in '%s' — using GDI fallback", fn_ini);
        CInifile::Destroy(ini);
        Destroy();
        return Create(dev);
    }
    CInifile::Destroy(ini);

    if (native_h_px <= 0) {
        Msg("~ DX11 font: glyph height is zero in '%s' — using GDI fallback", fn_ini);
        Destroy(); return Create(dev);
    }

    // Compute display scale matching CGameFont::SetHeightI / SetHeight:
    //   is_di=true  → SetHeightI: fCurrentHeight = sz * RDEVICE.dwHeight  (sz is screen fraction)
    //   is_di=false → SetHeight:  fCurrentHeight = sz                     (sz is direct pixels)
    float display_h = native_h_px;
    if (pSettings->line_exist(section_name, "size")) {
        float sz = pSettings->r_float(section_name, "size");
        bool is_di = strstr(tex_name, "ui_font_hud_01") ||
                     strstr(tex_name, "ui_font_hud_02") ||
                     strstr(tex_name, "ui_font_console_02");
        float dh = is_di ? sz * (float)HW11.BackBufferH : sz;
        if (dh > 0) display_h = dh;
    }

    float scale = display_h / native_h_px;
    for (int i = 0; i < 256; i++)
        m_glyphs[i].w *= scale;
    m_glyph_h = display_h;
    m_lineH   = m_glyph_h + 1.f;
    m_cellH   = (int)m_glyph_h;
    m_cellW   = (int)(m_glyphs[(unsigned char)'A'].w > 0
                    ? m_glyphs[(unsigned char)'A'].w
                    : m_glyphs[(unsigned char)'0'].w);
    m_atlasW  = (int)m_tex_w;
    m_atlasH  = (int)m_tex_h;
    m_game_font_mode = true;

    if (!BuildPipeline(dev)) { Destroy(); return false; }

    ID3DBlob* bPS = FontCompile("ps_font_rgba.hlsl", "main", "ps_5_0");
    if (bPS) {
        dev->CreatePixelShader(bPS->GetBufferPointer(), bPS->GetBufferSize(), nullptr, &m_ps_rgba);
        bPS->Release();
    }

    Msg("* DX11 game font '%s' native=%.0fpx display=%.1fpx scale=%.3f from '%s'",
        tex_name, native_h_px, display_h, scale, section_name);
    return true;
}

void CEditorFont11::Destroy()
{
    auto rel = [](auto*& p){ if (p){ p->Release(); p = nullptr; } };
    rel(m_atlasSRV); rel(m_cbVP); rel(m_vb);
    rel(m_vs); rel(m_ps); rel(m_ps_rgba); rel(m_il);
    rel(m_ss); rel(m_bs); rel(m_dss);
    m_game_font_mode = false;
    m_buf.clear();
}

void CEditorFont11::SetColor(u32 argb)      { m_color = argb; }
void CEditorFont11::OutSet(float x, float y){ m_curX = x; m_curY = y; }
void CEditorFont11::OutSkip(float factor)   { m_curY += m_lineH * factor; }

void CEditorFont11::PushChar(char ch, float& x, float y)
{
    u32 c = m_color;

    if (m_game_font_mode) {
        const GlyphInfo& g = m_glyphs[(unsigned char)ch];
        if (g.w <= 0) { x += m_cellW * 0.5f; return; }
        float x1 = x + g.w, y1 = y + m_glyph_h;
        m_buf.push_back({x,  y,  g.u0, g.v0, c});
        m_buf.push_back({x1, y,  g.u1, g.v0, c});
        m_buf.push_back({x,  y1, g.u0, g.v1, c});
        m_buf.push_back({x1, y,  g.u1, g.v0, c});
        m_buf.push_back({x1, y1, g.u1, g.v1, c});
        m_buf.push_back({x,  y1, g.u0, g.v1, c});
        x += g.w;
        return;
    }

    if ((unsigned char)ch < FIRST_CHAR || (unsigned char)ch > LAST_CHAR) { x += m_cellW; return; }
    int idx = (unsigned char)ch - FIRST_CHAR;
    int col = idx % COLS; int row = idx / COLS;
    float u0 = (float)(col * m_cellW)            / m_atlasW;
    float v0 = (float)(row * m_cellH)            / m_atlasH;
    float u1 = (float)(col * m_cellW + m_cellW)  / m_atlasW;
    float v1 = (float)(row * m_cellH + m_cellH)  / m_atlasH;
    float x1 = x + m_cellW, y1 = y + m_cellH;

    m_buf.push_back({x,  y,  u0, v0, c});
    m_buf.push_back({x1, y,  u1, v0, c});
    m_buf.push_back({x,  y1, u0, v1, c});
    m_buf.push_back({x1, y,  u1, v0, c});
    m_buf.push_back({x1, y1, u1, v1, c});
    m_buf.push_back({x,  y1, u0, v1, c});
    x += m_cellW;
}

void CEditorFont11::OutNext(const char* fmt, ...)
{
    if (!m_vs || m_buf.size() + 512 * 6 > MAX_CHARS_PER_FLUSH * 6) return;
    char line[1024];
    va_list va; va_start(va, fmt);
    vsnprintf(line, sizeof(line) - 1, fmt, va);
    va_end(va);
    float x = m_curX;
    for (const char* p = line; *p; ++p) PushChar(*p, x, m_curY);
    m_curY += m_lineH;
}

void CEditorFont11::Flush(ID3D11DeviceContext* ctx, float vp_w, float vp_h)
{
    if (m_buf.empty() || !m_vs || !m_vb) return;
    static bool s_logged = false;
    if (!s_logged) {
        s_logged = true;
        Msg("* DX11 font flush: mode=%s verts=%d ps_rgba=%p",
            m_game_font_mode ? "GAME" : "GDI", (int)m_buf.size(), m_ps_rgba);
    }

    u32 nv = (u32)m_buf.size();
    if (nv > MAX_CHARS_PER_FLUSH * 6) nv = MAX_CHARS_PER_FLUSH * 6;

    // upload vertices
    D3D11_MAPPED_SUBRESOURCE ms = {};
    if (FAILED(ctx->Map(m_vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) { m_buf.clear(); return; }
    memcpy(ms.pData, m_buf.data(), nv * sizeof(Vtx2D));
    ctx->Unmap(m_vb, 0);

    // upload viewport CB
    if (m_cbVP) {
        struct { float w, h, _0, _1; } vpd = { vp_w, vp_h, 0, 0 };
        if (SUCCEEDED(ctx->Map(m_cbVP, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            memcpy(ms.pData, &vpd, sizeof(vpd));
            ctx->Unmap(m_cbVP, 0);
        }
        ctx->VSSetConstantBuffers(2, 1, &m_cbVP);
    }

    UINT stride = sizeof(Vtx2D), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &m_vb, &stride, &offset);
    ctx->IASetInputLayout(m_il);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(m_vs, nullptr, 0);
    ctx->PSSetShader(m_game_font_mode && m_ps_rgba ? m_ps_rgba : m_ps, nullptr, 0);
    ctx->PSSetShaderResources(1, 1, &m_atlasSRV);
    ctx->PSSetSamplers(1, 1, &m_ss);

    float bf[4] = {};
    ctx->OMSetBlendState(m_bs, bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(m_dss, 0);

    ctx->Draw(nv, 0);

    ctx->OMSetDepthStencilState(nullptr, 0);
    ctx->OMSetBlendState(nullptr, bf, 0xFFFFFFFF);

    m_buf.clear();
}
