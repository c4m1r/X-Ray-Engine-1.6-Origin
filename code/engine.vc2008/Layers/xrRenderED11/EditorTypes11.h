#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d11.h>

struct EditorVertex11
{
    float pos[3];
    float normal[3];
    float uv[2];
};

struct EditorInstanceData
{
    float world[16];
    float color[4];
};

struct GpuAabb
{
    float mn[3];
    float mx[3];
};

struct LodInstanceData
{
    float center[3];
    float radius;
    float halfHeight;
    float rotY;
    float _pad[2];
};

struct SpriteVert2D
{
    float x, y;
    float u, v;
    u32   color;
};
