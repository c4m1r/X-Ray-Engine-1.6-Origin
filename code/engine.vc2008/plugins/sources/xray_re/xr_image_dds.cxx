#define NOMINMAX
#include <climits>
#include <algorithm>
#include <cmath>
#include <vector>
#include <nvtt/nvtt.h>
#include "xr_image.h"
#include "xr_file_system.h"

typedef uint8_t u8;

using namespace xray_re;

static u8 f2u8(float f)
{
    int		v	= (int)std::lround(f * 255.f);
    if (v < 0)		v	= 0;
    if (v > 255)	v	= 255;
    return		(u8)v;
}

bool xr_image::load_dds(const std::string& path)
{
    nvtt::Surface	image;
    image.load		(path.c_str());
    const int		W	= image.width();
    const int		H	= image.height();
    if (W <= 0 || H <= 0)		return false;
    m_width	= (unsigned)W;
    m_height	= (unsigned)H;
    m_data	= new rgba32[m_width * m_height];
    float*		c0	= image.channel(0);
    float*		c1	= image.channel(1);
    float*		c2	= image.channel(2);
    float*		c3	= image.channel(3);
    const size_t	n	= (size_t)W * (size_t)H;
    for (size_t i = 0; i < n; i++)
    {
        const u8   r	= f2u8(c0[i]);
        const u8   g	= f2u8(c1[i]);
        const u8   b	= f2u8(c2[i]);
        const u8   a	= c3 ? f2u8(c3[i]) : 255;
        m_data[i]	= (rgba32(a) << 24) | (rgba32(b) << 16) | (rgba32(g) << 8) | rgba32(r);
    }
    return		true;
}

bool xr_image::load_dds(const char* path, const char* name)
{
    xr_file_system&	fs	= xr_file_system::instance();
    std::string		full_path;
    if (!fs.resolve_path(path, name, full_path))	return false;
    return		load_dds(full_path);
}

bool xr_image::save_dds(const char* path, const std::string& name, const irect* rect) const
{
    xr_memory_writer*	w	= new xr_memory_writer();
    bool		status	= save_dds(*w, rect) && w->save_to(path, name);
    delete		w;
    return		status;
}

struct dds_writer: public nvtt::OutputHandler
{
    dds_writer(xr_writer& _w): w(_w) {}
    void		beginImage	(int, int, int, int, int, int) override	{}
    bool		writeData	(const void* data, int size) override
    {
        if (size < 0)		return true;
        w.w_raw		(data, (size_t)size);
        return		true;
    }
    void		endImage	() override	{}
    xr_writer&		w;
};

bool xr_image::save_dds(xr_writer& w, const irect* rect) const
{
    int		width, height;
    std::vector<u8> temp_bgra;
    u8*		pix	= 0;
    if (rect) {
        width	= int((rect->x2 - rect->x1 + 1) & INT_MAX);
        height	= int((rect->y2 - rect->y1 + 1) & INT_MAX);
        temp_bgra.resize((size_t)width * (size_t)height * 4);
        pix	= temp_bgra.data();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                rgba32	d	= m_data[(y + (int)rect->y1) * (int)m_width + (x + (int)rect->x1)];
                const u8   r	= (u8)(d & 0xff);
                const u8   g	= (u8)((d >> 8) & 0xff);
                const u8   b	= (u8)((d >> 16) & 0xff);
                const u8   a	= (u8)((d >> 24) & 0xff);
                u8*		o	= pix + (y * width + x) * 4;
                o[0]	= b;
                o[1]	= g;
                o[2]	= r;
                o[3]	= a;
            }
        }
    } else {
        width	= (int)(m_width & INT_MAX);
        height	= (int)(m_height & INT_MAX);
        temp_bgra.resize((size_t)width * (size_t)height * 4);
        pix	= temp_bgra.data();
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                rgba32	d	= m_data[y * (int)m_width + x];
                const u8   r	= (u8)(d & 0xff);
                const u8   g	= (u8)((d >> 8) & 0xff);
                const u8   b	= (u8)((d >> 16) & 0xff);
                const u8   a	= (u8)((d >> 24) & 0xff);
                u8*		o	= pix + (y * width + x) * 4;
                o[0]	= b;
                o[1]	= g;
                o[2]	= r;
                o[3]	= a;
            }
        }
    }

    nvtt::Context		context	(true);
    nvtt::TimingContext*	tc	= context.getTimingContext();
    nvtt::Surface		surf;
    surf.setImage(nvtt::InputFormat_BGRA_8UB, width, height, 1, pix, tc);
    surf.setWrapMode	(nvtt::WrapMode_Clamp);

    nvtt::CompressionOptions	comp;
    comp.setFormat	(nvtt::Format_BC3);
    comp.setQuality	(nvtt::Quality_Highest);

    nvtt::OutputOptions		out;
    dds_writer			h	(w);
    out.setOutputHandler	(&h);

    if (context.isCudaAccelerationEnabled())	surf.ToGPU	(tc);
    if (!context.outputHeader	(surf, 1, comp, out))		return false;
    if (!context.compress	(surf, 0, 0, comp, out))		return false;
    return		true;
}
