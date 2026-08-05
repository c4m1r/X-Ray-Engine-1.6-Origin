struct VSIn  { float3 pos : POSITION; float4 col : COLOR; };
struct VSOut { float4 pos : SV_POSITION; float4 col : COLOR; };
VSOut main(VSIn i) {
    VSOut o;
    o.pos = float4(i.pos.xy, 0, 1);
    o.col = i.col;
    return o;
}
