#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn  { float3 pos:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float3 rdir:TEXCOORD1; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos  = mul(float4(i.pos, 1.0), ViewProj);
    o.uv   = i.uv;
    o.rdir = reflect(normalize(i.pos - CamPos.xyz), normalize(i.n));
    return o;
}
