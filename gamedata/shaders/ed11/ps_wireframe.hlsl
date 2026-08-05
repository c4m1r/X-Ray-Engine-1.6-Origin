cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
struct PSIn { float4 pos:SV_POSITION; float3 wn:NORMAL; float2 uv:TEXCOORD0; float3 wp:TEXCOORD1; };
float4 main(PSIn i) : SV_Target {
    return float4(ObjectColor.rgb, 1.0);
}
