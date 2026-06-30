#pragma once
// DX11 editor render of detail objects (grass): gathers per-instance world matrices from the
// detail manager's visible-slot cache and draws via hardware instancing (HW11.DrawGrassModel,
// alpha-test cutout). Lives in xrRenderED11 so all DX11/GPU code stays out of shared xrRender.
//
// IMPORTANT: this function DRAINS dm->m_visibles every frame (the cache is appended to by MT_CALC
// each frame; the DX9 soft_Render path drains it via _vis.clear() — this path must do the same,
// otherwise the cache grows without bound and FPS decays over time).

class CDetailManager;
class CFrustum;

// Called intra-module from CRender::model_RenderDetail (ECore); LevelEditor reaches it
// through the exported CRender method, mirroring RenderModelED11 / RenderParticleED11.
// 'frustum' (optional) culls grass outside the view — big win since the cache is 360°.
void RenderDetailED11(CDetailManager* dm, CFrustum* frustum);
