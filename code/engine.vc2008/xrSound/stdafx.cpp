// stdafx.cpp : source file that includes just the standard includes
// xrSound.pch will be the pre-compiled header
// stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"

// TODO: reference any additional headers you need in STDAFX.H
// and not in this file

#ifdef __BORLANDC__
#pragma comment(lib, "libogg_static_b.lib")
#pragma comment(lib, "libvorbis_static_b.lib")
#pragma comment(lib, "libvorbisfile_static_b.lib")
#pragma comment(lib,	"xrCoreB.lib"		)
#	pragma comment(lib,	"ETools.lib"		)
#	pragma comment(lib,	"OpenAL.lib"		)
#	pragma comment(lib,	"dsound.lib" 		)
#else
//#	pragma comment(lib,	"eax.lib"			)
#	pragma comment(lib,	"xrCore.lib"		)
#	pragma comment(lib,	"xrCDB.lib"			)
#	pragma comment(lib,	"dsound.lib" 		)
#	pragma comment(lib,	"xrapi.lib" 		)
#endif

