
struct AabbEntry { float3 mn; float3 mx; };

cbuffer cbCull : register(b1)
{
    float4 planes[6];
    uint   count;
    uint3  _pad;
};

StructuredBuffer<AabbEntry> g_aabbs : register(t0);
RWBuffer<uint>              g_vis   : register(u0);

[numthreads(64, 1, 1)]
void main(uint3 tid : SV_DispatchThreadID)
{
    uint i = tid.x;
    if (i >= count) return;

    AabbEntry e = g_aabbs[i];
    uint vis = 1u;

    for (int p = 0; p < 6; p++)
    {
        float3 n = planes[p].xyz;
        float3 nv;
        nv.x = (n.x >= 0.f) ? e.mn.x : e.mx.x;
        nv.y = (n.y >= 0.f) ? e.mn.y : e.mx.y;
        nv.z = (n.z >= 0.f) ? e.mn.z : e.mx.z;
        if (dot(n, nv) + planes[p].w > 0.f)
        {
            vis = 0u;
            break;
        }
    }

    g_vis[i] = vis;
}
