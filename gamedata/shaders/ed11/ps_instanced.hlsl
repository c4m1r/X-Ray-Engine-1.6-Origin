Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 main(PSIn i) : SV_Target {
    float4 col = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(col.a - 0.5);
    col.rgb = lerp(col.rgb, i.col.rgb, i.col.a);
    return col;
}
