#ifndef __dxgi_jpeg_compat_h__
#define __dxgi_jpeg_compat_h__

#include <windows.h>

#ifndef DXGI_JPEG_DC_HUFFMAN_TABLE
typedef struct DXGI_JPEG_DC_HUFFMAN_TABLE
{
    BYTE CodeCounts[12];
    BYTE CodeValues[12];
} DXGI_JPEG_DC_HUFFMAN_TABLE;
#endif

#ifndef DXGI_JPEG_AC_HUFFMAN_TABLE
typedef struct DXGI_JPEG_AC_HUFFMAN_TABLE
{
    BYTE CodeCounts[16];
    BYTE CodeValues[162];
} DXGI_JPEG_AC_HUFFMAN_TABLE;
#endif

#ifndef DXGI_JPEG_QUANTIZATION_TABLE
typedef struct DXGI_JPEG_QUANTIZATION_TABLE
{
    BYTE Elements[64];
} DXGI_JPEG_QUANTIZATION_TABLE;
#endif

#endif // __dxgi_jpeg_compat_h__
