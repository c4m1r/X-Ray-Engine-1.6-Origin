Texture2D    DiffuseTex : register(t0);
Texture2D    EnvTex     : register(t1);
SamplerState LinearSamp : register(s0);
SamplerState EnvSamp    : register(s1);
cbuffer cbSurfParams : register(b2) { float4 SurfParams; };
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; float3 rdir:TEXCOORD1; };
float4 main(PSIn i) : SV_Target {
    float4 col = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(col.a - SurfParams.x);
    if (SurfParams.y > 0.5) {
        float3 e = EnvTex.Sample(EnvSamp, i.rdir.xy).rgb;
        col.rgb = lerp(e, col.rgb, col.a);
    }
    col.rgb = lerp(col.rgb, i.col.rgb, i.col.a);
    return col;
}
