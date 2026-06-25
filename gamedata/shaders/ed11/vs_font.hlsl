cbuffer cbFontVP : register(b2) { float2 vpSize; float2 _pad; };
struct VSIn  { float2 pos:POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos.x / vpSize.x * 2.0 - 1.0,
                   1.0 - i.pos.y / vpSize.y * 2.0, 0.0, 1.0);
    o.uv  = i.uv;
    o.col = i.col;
    return o;
}
