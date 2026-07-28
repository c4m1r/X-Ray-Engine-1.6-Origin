cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
cbuffer cbSurfParams : register(b2) { float4 SurfParams; };
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
float4 main(PSIn i) : SV_Target {
    float4 col = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(col.a - SurfParams.x);
    return float4(col.rgb, col.a * ObjectColor.a);
}
