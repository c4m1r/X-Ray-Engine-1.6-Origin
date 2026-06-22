#pragma once
// DX11 editor rendering of engine render-visuals (e.g. spawn/actor skinned models).
// This belongs to the DX11 editor layer; the shared xrRender layer stays free of any
// GPU-API code and only provides render-agnostic data + CPU skinning (CSkeletonX::Skin_Editor).

class dxRender_Visual;

// Render a visual (recursing its hierarchy) at 'world' through the HW11 editor pipeline.
void RenderModelED11(dxRender_Visual* V, const Fmatrix& world);
