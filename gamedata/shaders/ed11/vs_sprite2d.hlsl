// 2D textured sprite vertex shader.
// Input: NDC position (float2) + UV (float2) + per-vertex BGRA color (float4 via B8G8R8A8_UNORM).
// No constant buffer needed — position is already in NDC.

struct VSIn {
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR;
};

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR;
};

VSOut main(VSIn i)
{
    VSOut o;
    o.pos = float4(i.pos.x, i.pos.y, 0.0f, 1.0f);
    o.uv  = i.uv;
    o.col = i.col;
    return o;
}
