#pragma once
// Shared DX11 editor data types — no dependencies on other editor headers.

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>

// Standard editor mesh vertex (matches D3DFVF_XYZ | D3DFVF_NORMAL | D3DFVF_TEX1)
struct EditorVertex11
{
    float pos[3];
    float normal[3];
    float uv[2];
};

// Per-instance data for hardware instancing (slot 1 vertex buffer)
struct EditorInstanceData
{
    float world[16];   // row-major 4x4 world matrix
    float color[4];    // xyz=selection tint, w=blend factor
};

// World-space AABB for GPU frustum culling (uploaded once per frame, read by CS)
struct GpuAabb
{
    float mn[3];   // min corner
    float mx[3];   // max corner
};

// Per-instance data for LOD billboards (matches il_lod: ICENTER float4 + IPARAM float4)
struct LodInstanceData
{
    float center[3];    // world bbox center
    float radius;       // half-width (max of X/Z extent)
    float halfHeight;   // half of Y extent
    float rotY;         // instance Y rotation (radians)
    float _pad[2];
};

// 2D textured sprite vertex: NDC xy + UV + BGRA color — stride 20 bytes
struct SpriteVert2D
{
    float x, y;   // NDC position [-1, 1]
    float u, v;   // texture coordinates [0, 1]
    u32   color;  // BGRA packed (D3DCOLOR / FVF::L::color format)
};
