// Multi-surface instanced pixel shader for SM5.0.
// Dynamic texture array indexing is not supported in SM5.0 (X3512).
// Instead, declare 16 separate slots and dispatch via switch — the compiler
// handles this correctly and the GPU serializes divergent surf_id within a warp.
Texture2D g_t0  : register(t0);
Texture2D g_t1  : register(t1);
Texture2D g_t2  : register(t2);
Texture2D g_t3  : register(t3);
Texture2D g_t4  : register(t4);
Texture2D g_t5  : register(t5);
Texture2D g_t6  : register(t6);
Texture2D g_t7  : register(t7);
Texture2D g_t8  : register(t8);
Texture2D g_t9  : register(t9);
Texture2D g_t10 : register(t10);
Texture2D g_t11 : register(t11);
Texture2D g_t12 : register(t12);
Texture2D g_t13 : register(t13);
Texture2D g_t14 : register(t14);
Texture2D g_t15 : register(t15);
SamplerState LinearSamp : register(s0);

struct PSIn {
    float4 pos                   : SV_POSITION;
    float3 wn                    : NORMAL;
    float2 uv                    : TEXCOORD0;
    nointerpolation uint surf_id : TEXCOORD1;
    float4 col                   : COLOR;
};

float4 SampleTex(uint id, float2 uv) {
    switch (id) {
        case  0: return g_t0 .Sample(LinearSamp, uv);
        case  1: return g_t1 .Sample(LinearSamp, uv);
        case  2: return g_t2 .Sample(LinearSamp, uv);
        case  3: return g_t3 .Sample(LinearSamp, uv);
        case  4: return g_t4 .Sample(LinearSamp, uv);
        case  5: return g_t5 .Sample(LinearSamp, uv);
        case  6: return g_t6 .Sample(LinearSamp, uv);
        case  7: return g_t7 .Sample(LinearSamp, uv);
        case  8: return g_t8 .Sample(LinearSamp, uv);
        case  9: return g_t9 .Sample(LinearSamp, uv);
        case 10: return g_t10.Sample(LinearSamp, uv);
        case 11: return g_t11.Sample(LinearSamp, uv);
        case 12: return g_t12.Sample(LinearSamp, uv);
        case 13: return g_t13.Sample(LinearSamp, uv);
        case 14: return g_t14.Sample(LinearSamp, uv);
        case 15: return g_t15.Sample(LinearSamp, uv);
        default: return float4(1, 0, 1, 1); // magenta = surf_id out of range
    }
}

float4 main(PSIn i) : SV_Target {
    float4 col = SampleTex(i.surf_id, i.uv);
    clip(col.a - 0.5);
    col.rgb = lerp(col.rgb, i.col.rgb, i.col.a);
    return col;
}
