Texture2D<float4> Atlas : register(t1);
SamplerState      SS    : register(s1);
struct PSIn { float4 pos:SV_POSITION; float2 uv:TEXCOORD0; float4 col:COLOR; };
float4 main(PSIn i) : SV_Target {
    float4 tex = Atlas.Sample(SS, i.uv);
    float lum = tex.r * 0.299 + tex.g * 0.587 + tex.b * 0.114;
    float a = (tex.a < 0.5) ? tex.a : lum;
    return float4(i.col.rgb, i.col.a * a);
}
