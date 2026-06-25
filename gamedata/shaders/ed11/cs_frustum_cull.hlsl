// Compute shader: parallel AABB vs frustum test, one thread per instance.
// Writes 1 (visible) or 0 (culled) into g_vis[i].
// VS reads g_vis[g_inst_start + SV_InstanceID]; if 0 → degenerate vertex → rasterizer discards.
//
// X-Ray CFrustum uses OUTWARD-POINTING plane normals:
//   classify(v) = dot(n, v) + d < 0  → inside (visible)
//   classify(v) = dot(n, v) + d > 0  → outside (invisible)
//
// Conservative AABB cull: compute the n-vertex (corner with MINIMUM classify).
// If min-classify > 0 → every corner is outside → AABB is entirely outside → cull.

struct AabbEntry { float3 mn; float3 mx; };

cbuffer cbCull : register(b1)
{
    float4 planes[6];   // world-space frustum planes (outward normals): classify > 0 = outside
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
        // n-vertex: AABB corner with MINIMUM classify (most inside / least outside).
        // For outward normal: n.x > 0 → minimum x-contribution comes from mn.x; n.x < 0 → from mx.x.
        float3 nv;
        nv.x = (n.x >= 0.f) ? e.mn.x : e.mx.x;
        nv.y = (n.y >= 0.f) ? e.mn.y : e.mx.y;
        nv.z = (n.z >= 0.f) ? e.mn.z : e.mx.z;
        // If even the most-inside corner is outside (classify > 0) → cull.
        if (dot(n, nv) + planes[p].w > 0.f)
        {
            vis = 0u;
            break;
        }
    }

    g_vis[i] = vis;
}
