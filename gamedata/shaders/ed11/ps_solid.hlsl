cbuffer cbPerFrame : register(b0) { float4x4 View; float4x4 Proj; float4x4 ViewProj; float4 CamPos; };
cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
float4 main(PSIn i) : SV_Target {
    float4 col = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(col.a - 0.5);
    col.rgb = lerp(col.rgb, ObjectColor.rgb, ObjectColor.a);
    return col;
}
