#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct VSIn  { float3 pos:POSITION; };
struct VSOut { float4 pos:SV_POSITION; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(mul(float4(i.pos,1), World), ViewProj);
    return o;
}
