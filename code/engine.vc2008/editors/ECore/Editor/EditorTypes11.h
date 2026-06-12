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
