cbuffer cbPerObject: register(b1) { float4x4 World; float4 ObjectColor; };
float4 main(float4 pos:SV_POSITION) : SV_Target {
    return float4(ObjectColor.rgb, ObjectColor.a);
}
