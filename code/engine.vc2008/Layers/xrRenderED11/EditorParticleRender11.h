#pragma once
// DX11 editor render of engine particle visuals (PS::CParticleEffect / CParticleGroup).
// CPU-builds billboard quads (Fill_Editor) and draws them via HW11.DrawParticles.
// Lives in xrRenderED11 so all DX11/GPU code stays out of the shared xrRender layer.

class dxRender_Visual;

// Called intra-module from CRender::model_RenderParticle (ECore); LevelEditor reaches it
// through the exported CRender method, mirroring RenderModelED11.
void RenderParticleED11(dxRender_Visual* V);
