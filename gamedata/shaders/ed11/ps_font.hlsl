Texture2D<float> Atlas : register(t1);
SamplerState     SS    : register(s1);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 main(PSIn i) : SV_Target {
    float a = Atlas.Sample(SS, i.uv);
    return float4(i.col.rgb, i.col.a * a);
}
