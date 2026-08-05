#pragma pack_matrix(row_major)
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn {
    float3 pos    : POSITION;
    float3 n      : NORMAL;
    float2 uv     : TEXCOORD0;
    uint   surf_id: TEXCOORD1;
    float4 iw0    : WORLDMATRIX0;
    float4 iw1    : WORLDMATRIX1;
    float4 iw2    : WORLDMATRIX2;
    float4 iw3    : WORLDMATRIX3;
    float4 icolor : ICOLOR;
};
struct VSOut {
    float4 pos                      : SV_POSITION;
    float3 wn                       : NORMAL;
    float2 uv                       : TEXCOORD0;
    nointerpolation uint surf_id    : TEXCOORD1;
    float4 col                      : COLOR;
};
VSOut main(VSIn i) {
    VSOut o;
    float4x4 W = float4x4(i.iw0, i.iw1, i.iw2, i.iw3);
    float4 wp  = mul(float4(i.pos, 1), W);
    o.pos      = mul(wp, ViewProj);
    o.wn       = normalize(mul(i.n, (float3x3)W));
    o.uv       = i.uv;
    o.surf_id  = i.surf_id;
    o.col      = i.icolor;
    return o;
}
