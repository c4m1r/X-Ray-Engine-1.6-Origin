#pragma once
// Replaces <d3dcompiler.h> for the DX11 editor renderer.
//
// Problem: xrD3DDefs.h (included via stdafx.h in DX9/editor mode) already defines:
//   typedef ID3DXBuffer   ID3DBlob;
//   typedef D3DXMACRO     D3D_SHADER_MACRO;
//   typedef ID3DXInclude  ID3DInclude;
// The SDK's D3Dcompiler.h re-declares these as DX10 interface aliases, causing
// redefinition errors. We bypass that by declaring D3DCompile directly.

// D3DCOMPILE_* flags (Windows 8+ SDK values, matching d3d10shader.h constants).
#ifndef D3DCOMPILE_DEBUG
#  define D3DCOMPILE_DEBUG                    (1 << 0)
#  define D3DCOMPILE_SKIP_VALIDATION          (1 << 1)
#  define D3DCOMPILE_SKIP_OPTIMIZATION        (1 << 2)
#  define D3DCOMPILE_ENABLE_STRICTNESS        (1 << 11)
#  define D3DCOMPILE_OPTIMIZATION_LEVEL0      (1 << 14)
#  define D3DCOMPILE_OPTIMIZATION_LEVEL1      0
#  define D3DCOMPILE_OPTIMIZATION_LEVEL2      ((1 << 14) | (1 << 15))
#  define D3DCOMPILE_OPTIMIZATION_LEVEL3      (1 << 15)
#endif

// Forward-declare D3DCompile using types already resolved by xrD3DDefs.h:
//   ID3DBlob**          ← ID3DXBuffer** (GetBufferPointer / GetBufferSize)
//   D3D_SHADER_MACRO*   ← D3DXMACRO*
//   ID3DInclude*        ← ID3DXInclude*
#ifdef __cplusplus
extern "C" {
#endif

HRESULT WINAPI D3DCompile(
    LPCVOID                  pSrcData,
    SIZE_T                   SrcDataSize,
    LPCSTR                   pSourceName,
    const D3D_SHADER_MACRO*  pDefines,
    ID3DInclude*             pInclude,
    LPCSTR                   pEntrypoint,
    LPCSTR                   pTarget,
    UINT                     Flags1,
    UINT                     Flags2,
    ID3DBlob**               ppCode,
    ID3DBlob**               ppErrorMsgs
);

#ifdef __cplusplus
}
#endif
