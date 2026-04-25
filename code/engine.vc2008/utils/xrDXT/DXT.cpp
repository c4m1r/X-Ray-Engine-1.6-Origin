#include "stdafx.h"
#include <memory>
#include <vector>
#include <nvtt/nvtt.h>
#include "Layers/xrRender/ETextureParams.h"

// NVTT 3: pass enableCuda to nvtt::Context (true = try CUDA+GPU, false = CPU only).

static void DxtNVTT3_ShowError(LPCSTR why)
{
    if (why && *why)	MessageBox(0, why, "DXT compress error", MB_ICONERROR | MB_OK);
}

static void DxtNVTT3_FillCompressionOptions(nvtt::CompressionOptions& c, STextureParams* fmt)
{
    c.setQuality(nvtt::Quality_Highest);
    c.setQuantization(fmt->flags.is(STextureParams::flDitherColor), false, fmt->flags.is(STextureParams::flBinaryAlpha));
    switch (fmt->fmt)
    {
    case STextureParams::tfDXT1:  c.setFormat(nvtt::Format_BC1); break;
    case STextureParams::tfADXT1: c.setFormat(nvtt::Format_BC1a); break;
    case STextureParams::tfDXT3:  c.setFormat(nvtt::Format_BC2); break;
    case STextureParams::tfDXT5:  c.setFormat(nvtt::Format_BC3); break;
    case STextureParams::tfRGB:   c.setFormat(nvtt::Format_RGB); break;
    case STextureParams::tfRGBA:  c.setFormat(nvtt::Format_RGBA); break;
    }
}

static nvtt::MipmapFilter DxtNVTT3_MipmapFilterValue(STextureParams* fmt)
{
    switch (fmt->mip_filter)
    {
    case STextureParams::kMIPFilterBox:      return nvtt::MipmapFilter_Box;
    case STextureParams::kMIPFilterTriangle: return nvtt::MipmapFilter_Triangle;
    case STextureParams::kMIPFilterKaiser:   return nvtt::MipmapFilter_Kaiser;
    default: return nvtt::MipmapFilter_Box;
    }
}

static void DxtNVTT3_BuildNextMipmap(
    nvtt::Surface& image,
    nvtt::MipmapFilter f,
    nvtt::TimingContext* tc)
{
    if (f == nvtt::MipmapFilter_Kaiser)
    {
        float p[2] = {1.0f, 4.0f};
        image.buildNextMipmap(nvtt::MipmapFilter_Kaiser, 3, p, 1, tc);
    }
    else
        image.buildNextMipmap(f, 1, tc);
}

u32* Build32MipLevel(u32& _w, u32& _h, u32& _p, u32* pdwPixelSrc, STextureParams* fmt, float blend)
{
    R_ASSERT(pdwPixelSrc);
    R_ASSERT(_w % 2 == 0);
    R_ASSERT(_h % 2 == 0);
    R_ASSERT(_p % 4 == 0);
    u32 dwDestPitch = (_w / 2) * 4;
    u32* pNewData = xr_alloc<u32>((_h / 2) * dwDestPitch);
    u8* pDest = (u8*)pNewData;
    u8* pSrc = (u8*)pdwPixelSrc;
    float mixed_a = (float)u8(fmt->fade_color >> 24);
    float mixed_r = (float)u8(fmt->fade_color >> 16);
    float mixed_g = (float)u8(fmt->fade_color >> 8);
    float mixed_b = (float)u8(fmt->fade_color >> 0);
    float inv_blend = 1.f - blend;
    for (u32 y = 0; y < _h; y += 2)
    {
        u8* pScanline = pSrc + y * _p;
        for (u32 x = 0; x < _w; x += 2)
        {
            u8* p1 = pScanline + x * 4;
            u8* p2 = p1 + 4;
            if (1 == _w)
                p2 = p1;
            u8* p3 = p1 + _p;
            if (1 == _h)
                p3 = p1;
            u8* p4 = p2 + _p;
            if (1 == _h)
                p4 = p2;
            float c_r = float(u32(p1[0]) + u32(p2[0]) + u32(p3[0]) + u32(p4[0])) / 4.f;
            float c_g = float(u32(p1[1]) + u32(p2[1]) + u32(p3[1]) + u32(p4[1])) / 4.f;
            float c_b = float(u32(p1[2]) + u32(p2[2]) + u32(p3[2]) + u32(p4[2])) / 4.f;
            float c_a = float(u32(p1[3]) + u32(p2[3]) + u32(p3[3]) + u32(p4[3])) / 4.f;
            if (fmt->flags.is(STextureParams::flFadeToColor))
            {
                c_r = c_r * inv_blend + mixed_r * blend;
                c_g = c_g * inv_blend + mixed_g * blend;
                c_b = c_b * inv_blend + mixed_b * blend;
            }
            if (fmt->flags.is(STextureParams::flFadeToAlpha))
            {
                c_a = c_a * inv_blend + mixed_a * blend;
            }
            float A = c_a + c_a / 8.f;
            int _r = int(c_r);
            clamp(_r, 0, 255);
            *pDest++ = u8(_r);
            int _g = int(c_g);
            clamp(_g, 0, 255);
            *pDest++ = u8(_g);
            int _b = int(c_b);
            clamp(_b, 0, 255);
            *pDest++ = u8(_b);
            int _a = int(A);
            clamp(_a, 0, 255);
            *pDest++ = u8(_a);
        }
    }
    _w /= 2;
    _h /= 2;
    _p = _w * 4;
    return pNewData;
}

IC u32 GetPowerOf2Plus1(u32 v)
{
    u32 cnt = 0;
    while (v)
    {
        v >>= 1;
        cnt++;
    }
    return cnt;
}

void FillRect(u8* data, u8* new_data, u32 offs, u32 pitch, u32 h, u32 full_pitch)
{
    for (u32 i = 0; i < h; i++)
    {
        CopyMemory(data + (full_pitch * i + offs), new_data + i * pitch, pitch);
    }
}

static void DxtNVTT3_SurfaceSetBGRA8(
    nvtt::Surface& s,
    u32 w,
    u32 h,
    u32 pitch,
    u8* raw,
    nvtt::TimingContext* tc)
{
    R_ASSERT(0 == pitch % 4);
    R_ASSERT(0 < pitch);
    R_ASSERT(0 < w && 0 < h);
    if (pitch == w * 4)
    {
        s.setImage(nvtt::InputFormat_BGRA_8UB, (int)w, (int)h, 1, raw, tc);
        return;
    }
    u32		sz	= w * h * 4;
    u8*		tmp	= xr_alloc<u8>(sz);
    for (u32 y = 0; y < h; y++)
        CopyMemory(tmp + y * w * 4, raw + y * pitch, w * 4);
    s.setImage(nvtt::InputFormat_BGRA_8UB, (int)w, (int)h, 1, tmp, tc);
    xr_free(tmp);
}

int DXTCompressImage(LPCSTR out_name, u8* raw_data, u32 w, u32 h, u32 pitch, STextureParams* fmt, u32 /*depth*/, bool isCudaActive)
{
    R_ASSERT(0 != w && 0 != h);
    if (0 == w || 0 == h)	return 0;
    R_ASSERT(0 < pitch);
    R_ASSERT(0 == pitch % 4);

    nvtt::Context		context	(isCudaActive);
    nvtt::TimingContext*	tc	= context.getTimingContext();

    nvtt::CompressionOptions	compOpt;
    DxtNVTT3_FillCompressionOptions(compOpt, fmt);

    nvtt::OutputOptions		outOpt;
    outOpt.setFileName	(out_name);

    const bool			genMip	= fmt->flags.is(STextureParams::flGenerateMipMaps);
    const bool			advMip	= genMip && (fmt->mip_filter == STextureParams::kMIPFilterAdvanced);
    const bool			normalU	= (fmt->type == STextureParams::ttNormalMap);

    // — advanced: custom mip stack (fade) → batch (header, then fill batch, then compress; same order as nvtt_compress)
    if (advMip)
    {
        int				numMipmaps	= (int)GetPowerOf2Plus1(__min(w, h));
        std::vector<std::vector<u8>>	levels;
        std::vector<u32>		lw, lh;
        u32				dwW	= w, dwH = h, dwP = pitch;
        u32*				pLastMip	= xr_alloc<u32>(w * h * 4);
        CopyMemory(pLastMip, raw_data, w * h * 4);
        {
            std::vector<u8> m((size_t)dwW * (size_t)dwH * 4);
            CopyMemory(m.data(), pLastMip, m.size());
            lw.push_back(dwW);
            lh.push_back(dwH);
            levels.push_back(std::move(m));
        }
        float	inv_fade	= clampr(1.f - float(fmt->fade_amount) / 100.f, 0.f, 1.f);
        float	blend		= fmt->flags.is_any(STextureParams::flFadeToColor | STextureParams::flFadeToAlpha) ? inv_fade : 1.f;
        for (int i = 1; i < numMipmaps; i++)
        {
            u32* pNewMip	= Build32MipLevel(dwW, dwH, dwP, pLastMip, fmt, i < fmt->fade_delay ? 0.f : 1.f - blend);
            xr_free(pLastMip);
            pLastMip	= pNewMip;
            pNewMip	= 0;
            std::vector<u8> m((size_t)dwW * (size_t)dwH * 4);
            CopyMemory(m.data(), pLastMip, m.size());
            lw.push_back(dwW);
            lh.push_back(dwH);
            levels.push_back(std::move(m));
        }
        xr_free(pLastMip);

        if (!context.outputHeader(
                nvtt::TextureType_2D,
                (int)lw[0],
                (int)lh[0],
                1,
                (int)levels.size(),
                normalU,
                compOpt,
                outOpt))
        {
            DxtNVTT3_ShowError("NVTT: outputHeader (mip chain) failed.");
            return 0;
        }

        nvtt::BatchList		batch;
        std::vector<std::unique_ptr<nvtt::Surface>>	held;
        for (size_t mi = 0; mi < levels.size(); mi++)
        {
            nvtt::Surface	tmpS;
            tmpS.setImage(
                nvtt::InputFormat_BGRA_8UB,
                (int)lw[mi], (int)lh[mi], 1, levels[mi].data(), tc);
            if (context.isCudaAccelerationEnabled())
                tmpS.ToGPU	(tc);
            nvtt::Surface	q	= tmpS;
            context.quantize	(q, compOpt);
            std::unique_ptr<nvtt::Surface> up = std::make_unique<nvtt::Surface>(q);
            batch.Append	(up.get(), 0, (int)mi, &outOpt);
            held.push_back	(std::move(up));
        }
        if (!context.compress(batch, compOpt))
        {
            DxtNVTT3_ShowError("NVTT: compress (batch) failed.");
            unlink(out_name);
            return 0;
        }
        return 1;
    }

    // — single or generated mips on one surface
    nvtt::Surface		image;
    DxtNVTT3_SurfaceSetBGRA8(image, w, h, pitch, raw_data, tc);
    image.setWrapMode		(nvtt::WrapMode_Clamp);
    image.setNormalMap		(fmt->type == STextureParams::ttNormalMap);

    const nvtt::MipmapFilter mif = DxtNVTT3_MipmapFilterValue(fmt);

    if (!genMip)
    {
        if (!context.outputHeader(image, 1, compOpt, outOpt)) { DxtNVTT3_ShowError("NVTT: outputHeader failed."); return 0; }
        if (context.isCudaAccelerationEnabled())
            image.ToGPU	(tc);
        if (!context.compress(image, 0, 0, compOpt, outOpt))
        {
            DxtNVTT3_ShowError("NVTT: compress failed.");
            unlink(out_name);
            return 0;
        }
        return 1;
    }

    int				numMips	= (int)GetPowerOf2Plus1(__min(w, h));
    if (numMips < 1)		numMips	= 1;
    if (!context.outputHeader(image, numMips, compOpt, outOpt)) { DxtNVTT3_ShowError("NVTT: outputHeader (mips) failed."); return 0; }
    for (int mip = 0; mip < numMips; mip++)
    {
        if (context.isCudaAccelerationEnabled())
            image.ToGPU	(tc);
        if (!context.compress(image, 0, mip, compOpt, outOpt))
        {
            DxtNVTT3_ShowError("NVTT: compress (mip) failed.");
            unlink(out_name);
            return 0;
        }
        if (mip < numMips - 1)
            DxtNVTT3_BuildNextMipmap(image, mif, tc);
    }
    return 1;
}

extern int DXTCompressBump(LPCSTR out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch, STextureParams* fmt, u32 depth, bool isCudaActive);

extern "C" __declspec(dllexport) int __stdcall DXTCompress(LPCSTR out_name, u8* raw_data, u8* normal_map, u32 w, u32 h, u32 pitch, STextureParams* fmt, u32 depth, bool isCudaActive = false)
{
    switch (fmt->type)
    {
    case STextureParams::ttImage:
    case STextureParams::ttCubeMap:
    case STextureParams::ttNormalMap:
    case STextureParams::ttTerrain:
        return DXTCompressImage(out_name, raw_data, w, h, pitch, fmt, depth, isCudaActive);
    case STextureParams::ttBumpMap:
        return DXTCompressBump(out_name, raw_data, normal_map, w, h, pitch, fmt, depth, isCudaActive);
    default: NODEFAULT;
    }
    return -1;
}
