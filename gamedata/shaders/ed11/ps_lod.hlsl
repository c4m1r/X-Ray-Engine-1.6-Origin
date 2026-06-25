// LOD billboard pixel shader (DX11 editor).
// Samples the LOD atlas and alpha-tests (vegetation billboards have cut-out alpha).
Texture2D    tex : register(t0);
SamplerState smp : register(s0);

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main(VSOut i) : SV_Target
{
    float4 c = tex.Sample(smp, i.uv);
    clip(c.a - 0.33f);   // discard transparent texels of the cut-out billboard
    return c;
}
