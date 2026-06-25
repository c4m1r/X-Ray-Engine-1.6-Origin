struct PSIn { float4 pos : SV_POSITION; float4 col : COLOR; };
float4 main(PSIn i) : SV_Target { return i.col; }
