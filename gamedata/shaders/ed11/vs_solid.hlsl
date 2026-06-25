#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct VSIn  { float3 pos:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; };
struct VSOut { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
VSOut main(VSIn i) {
    VSOut o;
    float4 wp = mul(float4(i.pos,1), World);
    o.wp  = wp.xyz;
    o.pos = mul(wp, ViewProj);
    o.wn  = normalize(mul(i.n, (float3x3)World));
    o.uv  = i.uv;
    return o;
}
