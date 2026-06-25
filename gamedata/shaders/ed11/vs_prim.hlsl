#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = mul(float4(i.pos, 1), ViewProj);
    o.col = i.col;
    return o;
}
