//---------------------------------------------------------------------------
#include "stdafx.h"
#pragma hdrstop

#include <cstdint>

#include "ui_toolscustom.h"
#include "ui_main.h"
#include "ResourceManager.h"
#include "EditorPreferences.h"                  // EPrefs (scene_clear_color)
#include "../../Layers/xrRenderED11/HW11.h"     // HW11 off-screen screenshot (DX11)

bool CEditorRenderDevice::MakeScreenshot(U32Vec& pixels, u32 width, u32 height)
{
	if (!b_is_Ready) return false;
    if (g_bEditorDX11) {
        // DX11: render the scene to an off-screen RT via HW11, then read pixels back.
        // Mirrors the DX9 path below (begin -> render -> read -> restore); the D3D11 specifics
        // live in HW11 (xrRenderED11). Output is BGRA == D3DFMT_A8R8G8B8, same as the DX9 path.
        const u32 clr = EPrefs ? EPrefs->scene_clear_color : 0x00555555u;
        UI->PrepareRedraw();
        EDevice.Begin();                                            // BeginFrame: bind+clear backbuffer
        if (!HW11.ScreenshotBegin(width, height, clr)) { EDevice.End(); return false; }
        Tools->Render();                                            // render the scene into the off-screen RT
        const bool ok = HW11.ScreenshotEnd(pixels, width, height);  // read pixels + restore backbuffer binding
        EDevice.End();                                              // seqRender/font/present (backbuffer)
        return ok;
    }

    // free managed resource
    Resources->Evict();

    IDirect3DSurface9* 	poldZB=0;
    IDirect3DSurface9* 	pZB=0;
    IDirect3DSurface9* 	pRT=0;
    IDirect3DSurface9* 	poldRT=0;
    D3DVIEWPORT9		oldViewport;
    SetRS(D3DRS_COLORWRITEENABLE,D3DCOLORWRITEENABLE_ALPHA|D3DCOLORWRITEENABLE_BLUE|D3DCOLORWRITEENABLE_GREEN|D3DCOLORWRITEENABLE_RED);
    CHK_DX(HW.pDevice->GetRenderTarget(0,&poldRT));
    CHK_DX(HW.pDevice->GetDepthStencilSurface(&poldZB));
    CHK_DX(HW.pDevice->GetViewport(&oldViewport));

	CHK_DX(HW.pDevice->CreateRenderTarget(width,height,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&pRT,0));
	CHK_DX(HW.pDevice->CreateDepthStencilSurface(width,height,HW.Caps.bStencil?D3DFMT_D24S8:D3DFMT_D24X8,D3DMULTISAMPLE_NONE,0,FALSE,&pZB,0));
	CHK_DX(HW.pDevice->SetRenderTarget(0,pRT));
	CHK_DX(HW.pDevice->SetDepthStencilSurface(pZB));

	UI->PrepareRedraw	();
    EDevice.Begin		();
    Tools->Render		();
    EDevice.End			();

	// Create temp-surface
	IDirect3DSurface9*	pFB;
	R_CHK(HW.pDevice->CreateOffscreenPlainSurface(
		width,height,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&pFB,NULL));
	R_CHK(HW.pDevice->GetRenderTargetData(pRT, pFB));

	D3DLOCKED_RECT	D;
	R_CHK(pFB->LockRect(&D,0,D3DLOCK_NOSYSLOCK));
	pixels.resize(width*height);
	// Image processing
	u32* pPixel	= (u32*)D.pBits;

	//U32It it 		= pixels.begin();
	for (int h=height-1, it_index = 0; h>=0; h--,it_index+=width){
		LPDWORD dt 	= reinterpret_cast<LPDWORD>(reinterpret_cast<uintptr_t>(pPixel) + static_cast<uintptr_t>(D.Pitch) * static_cast<uintptr_t>(h));
		CopyMemory	(&pixels.at(it_index),
		dt,
		sizeof(u32)*width);
    }

    R_CHK(pFB->UnlockRect());

	CHK_DX(HW.pDevice->SetDepthStencilSurface(poldZB));
    CHK_DX(HW.pDevice->SetRenderTarget(0,poldRT));
    CHK_DX(HW.pDevice->SetViewport(&oldViewport));

    _RELEASE(pZB);
    _RELEASE(poldZB);
    _RELEASE(pFB);
    _RELEASE(pRT);
    _RELEASE(poldRT);

    return true;
}



