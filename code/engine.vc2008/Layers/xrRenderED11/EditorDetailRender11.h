#pragma once

class CDetailManager;
class CFrustum;

void RenderDetailED11(CDetailManager* dm, CFrustum* frustum);
ECORE_API bool DetailCacheValidED11(CDetailManager* dm);

extern ECORE_API u32 g_detail_visible_gen;
