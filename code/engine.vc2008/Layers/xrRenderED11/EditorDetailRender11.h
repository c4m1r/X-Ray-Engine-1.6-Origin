#pragma once
// DX11 editor render of detail objects (grass): CPU-builds per-instance geometry from the
// detail manager's visible-slot cache and draws via HW11.DrawDetails (alpha-test cutout).
// Lives in xrRenderED11 so all DX11/GPU code stays out of the shared xrRender layer.

class CDetailManager;

// Called intra-module from CRender::model_RenderDetail (ECore); LevelEditor reaches it
// through the exported CRender method, mirroring RenderModelED11 / RenderParticleED11.
void RenderDetailED11(CDetailManager* dm);
