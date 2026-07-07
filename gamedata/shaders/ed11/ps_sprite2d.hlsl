
Texture2D    t0 : register(t0);
SamplerState s0 : register(s0);

struct PSIn {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR;
};

float4 main(PSIn i) : SV_Target
{
    return t0.Sample(s0, i.uv) * i.col;
}
