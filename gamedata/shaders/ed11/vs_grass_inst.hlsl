#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn {
    float3 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 r0  : TEXCOORD1;
    float4 r1  : TEXCOORD2;
    float4 r2  : TEXCOORD3;
    float4 r3  : TEXCOORD4;
};
struct VSOut { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
VSOut main(VSIn i) {
    float4x4 world = float4x4(i.r0, i.r1, i.r2, i.r3);
    float4 wp = mul(float4(i.pos, 1.0), world);
    VSOut o;
    o.pos = mul(wp, ViewProj);
    o.col = float4(1,1,1,1);
    o.uv  = i.uv;
    return o;
}
