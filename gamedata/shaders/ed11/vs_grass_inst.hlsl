#pragma pack_matrix(row_major)
// Hardware-instanced detail objects (grass). Per-vertex: pos+uv (FVF::V).
// Per-instance: a 4x4 world matrix (rotation*scale + translation), 4 rows in TEXCOORD1..4.
cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
struct VSIn {
    float3 pos : POSITION;     // slot 0 (per-vertex)
    float2 uv  : TEXCOORD0;    // slot 0
    float4 r0  : TEXCOORD1;    // slot 1 (per-instance) world row 0
    float4 r1  : TEXCOORD2;    // world row 1
    float4 r2  : TEXCOORD3;    // world row 2
    float4 r3  : TEXCOORD4;    // world row 3
};
struct VSOut { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
VSOut main(VSIn i) {
    float4x4 world = float4x4(i.r0, i.r1, i.r2, i.r3);
    float4 wp = mul(float4(i.pos, 1.0), world);   // model -> world (row-vector convention)
    VSOut o;
    o.pos = mul(wp, ViewProj);
    o.col = float4(1,1,1,1);                       // grass is unlit white * texture
    o.uv  = i.uv;
    return o;
}
