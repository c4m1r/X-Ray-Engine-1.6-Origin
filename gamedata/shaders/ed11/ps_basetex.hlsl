cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; };
float4 main(PSIn i) : SV_Target {
    float3 c = DiffuseTex.Sample(LinearSamp, i.uv).rgb;
    return float4(c, ObjectColor.a);
}
