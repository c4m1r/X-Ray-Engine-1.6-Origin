// Detail objects (grass): diffuse texture * vertex color with alpha-test cutout.
// Uses vs_particle (pos+color+uv, world-space) — same FVF::LIT layout.
Texture2D    DiffuseTex : register(t0);
SamplerState LinearSamp : register(s0);
struct PSIn { float4 pos:SV_POSITION; float4 col:COLOR0; float2 uv:TEXCOORD0; };
float4 main(PSIn i) : SV_Target {
    float4 c = DiffuseTex.Sample(LinearSamp, i.uv);
    clip(c.a - 0.5);          // grass cutout
    return c * i.col;
}
