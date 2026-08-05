#pragma once

#include "EditorTypes11.h"

class ECORE_API CEditorFont11
{
public:
    CEditorFont11() = default;
    ~CEditorFont11() = default;

    bool Create(ID3D11Device* dev, const char* face = "Courier New", int height = 10);
    bool CreateFromGameSection(ID3D11Device* dev, const char* section_name);
    void Destroy();

    void SetColor(u32 argb);
    void OutSet(float x, float y);
    void OutNext(const char* fmt, ...);
    void OutSkip(float factor = 1.f);

    void Flush(ID3D11DeviceContext* ctx, float vp_w, float vp_h);

private:
    struct GlyphInfo { float u0, v0, u1, v1, w; };
    bool      m_game_font_mode = false;
    GlyphInfo m_glyphs[256]    = {};
    float     m_tex_w          = 0;
    float     m_tex_h          = 0;
    float     m_glyph_h        = 0;
    ID3D11PixelShader* m_ps_rgba = nullptr;

    static const int FIRST_CHAR = 32;
    static const int LAST_CHAR  = 126;
    static const int NUM_CHARS  = LAST_CHAR - FIRST_CHAR + 1;
    static const int COLS       = 16;
    static const int ROWS       = (NUM_CHARS + COLS - 1) / COLS;

    ID3D11Texture2D*            m_atlas    = nullptr;
    ID3D11ShaderResourceView*   m_atlasSRV = nullptr;
    int   m_cellW  = 0;
    int   m_cellH  = 0;
    int   m_atlasW = 0;
    int   m_atlasH = 0;

    static const u32 MAX_CHARS_PER_FLUSH = 4096;
    ID3D11Buffer*               m_vb      = nullptr;
    ID3D11Buffer*               m_cbVP    = nullptr;

    ID3D11VertexShader*         m_vs      = nullptr;
    ID3D11PixelShader*          m_ps      = nullptr;
    ID3D11InputLayout*          m_il      = nullptr;
    ID3D11SamplerState*         m_ss      = nullptr;
    ID3D11BlendState*           m_bs      = nullptr;
    ID3D11DepthStencilState*    m_dss     = nullptr;

    float m_curX  = 0.f;
    float m_curY  = 0.f;
    float m_lineH = 0.f;
    u32   m_color = 0xFFFFFFFF;

    struct Vtx2D {
        float x, y, u, v;
        u32   color;
    };
    xr_vector<Vtx2D> m_buf;

    bool BuildAtlas(ID3D11Device* dev, const char* face, int height);
    bool BuildPipeline(ID3D11Device* dev);
    void PushChar(char c, float& x, float y);
};

extern ECORE_API CEditorFont11 EditorFont11;
