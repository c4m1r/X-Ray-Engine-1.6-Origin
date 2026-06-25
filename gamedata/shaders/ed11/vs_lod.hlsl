// LOD billboard vertex shader (DX11 editor).
// Renders a far-away vegetation instance as a single camera-facing quad (4 verts)
// instead of its full mesh. The LOD texture is an 8-frame horizontal atlas (512x64,
// LOD_SAMPLE_COUNT=8); the frame is chosen by the horizontal view angle relative to
// the instance's Y rotation, matching the editor's RenderLOD/CalculateLODTC layout.
#pragma pack_matrix(row_major)

cbuffer cbPerFrame : register(b0)
{
    float4x4 View;
    float4x4 Proj;
    float4x4 ViewProj;
    float4   CamPos;
};

struct VSIn
{
    float2 corner : POSITION;   // per-vertex quad corner in [-1..1]
    float4 ctr_r  : ICENTER;    // per-instance: center.xyz, radius (half-width)
    float4 prm    : IPARAM;     // per-instance: x=halfHeight, y=rotY(rad), zw unused
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOut main(VSIn i)
{
    float3 center = i.ctr_r.xyz;
    float  r      = i.ctr_r.w;
    float  hh     = i.prm.x;
    float  rotY   = i.prm.y;

    // Vertical billboard: face the camera by rotating around the world Y axis.
    float3 toCam = CamPos.xyz - center;
    toCam.y = 0.f;
    float  len = length(toCam);
    toCam = (len > 1e-4f) ? (toCam / len) : float3(0, 0, 1);
    float3 right = float3(toCam.z, 0.f, -toCam.x); // horizontal vector perpendicular to view

    float3 wp = center
              + right        * (i.corner.x * r)
              + float3(0,1,0) * (i.corner.y * hh);

    VSOut o;
    o.pos = mul(float4(wp, 1.f), ViewProj);

    // Choose atlas frame from horizontal angle, accounting for instance rotation.
    const float TWO_PI = 6.2831853f;
    float ang = atan2(toCam.x, toCam.z) - rotY;
    ang = ang - floor(ang / TWO_PI) * TWO_PI;     // wrap to [0, 2pi)
    int   frame = ((int)(ang / (TWO_PI / 8.f) + 0.5f)) & 7;

    float du = 1.f / 8.f;
    float2 uv;
    uv.x = frame * du + (i.corner.x * 0.5f + 0.5f) * du;
    uv.y = 1.f - (i.corner.y * 0.5f + 0.5f);       // corner.y=+1 -> top (v=0)
    o.uv = uv;
    return o;
}
