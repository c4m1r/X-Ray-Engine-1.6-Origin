#pragma pack_matrix(row_major)
cbuffer cbPerFrame   : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbInstOffset : register(b2) { uint g_inst_start; uint g_use_cull; uint2 _ip; };

Buffer<uint> g_vis : register(t16);

struct VSIn {
    float3 pos    : POSITION;
    float3 n      : NORMAL;
    float2 uv     : TEXCOORD0;
    float4 iw0    : WORLDMATRIX0;
    float4 iw1    : WORLDMATRIX1;
    float4 iw2    : WORLDMATRIX2;
    float4 iw3    : WORLDMATRIX3;
    float4 icolor : ICOLOR;
};
struct VSOut { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; float3 rdir:TEXCOORD1; };

VSOut main(VSIn i, uint iid : SV_InstanceID) {
    VSOut o;
    if (g_use_cull && g_vis[g_inst_start + iid] == 0u) {
        o.pos  = float4(2.f, 2.f, 2.f, 1.f);
        o.wn   = float3(0, 1, 0);
        o.uv   = float2(0, 0);
        o.col  = float4(0, 0, 0, 0);
        o.rdir = float3(0, 0, 0);
        return o;
    }
    float4x4 W = float4x4(i.iw0, i.iw1, i.iw2, i.iw3);
    float4 wp  = mul(float4(i.pos,1), W);
    o.pos = mul(wp, ViewProj);
    o.wn  = normalize(mul(i.n, (float3x3)W));
    o.uv  = i.uv;
    o.col = i.icolor;
    o.rdir = reflect(normalize(wp.xyz - CamPos.xyz), o.wn);
    return o;
}
