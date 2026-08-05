#pragma once

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
