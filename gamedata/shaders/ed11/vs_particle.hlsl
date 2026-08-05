#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn  { float3 pos:POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1.0), ViewProj);
    o.col = i.col;
    o.uv  = i.uv;
    return o;
}
