Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
float4 main(PSIn i) : SV_Target {
    return DiffuseTex.Sample(LinearSamp, i.uv) * i.col;
}
