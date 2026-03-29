#include "stdafx.h"
#pragma hdrstop

// external dependencies

//#pragma comment(lib,"vfw32")
#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"freeimage.lib")
#pragma comment(lib,"dxt.lib")
#pragma comment(lib,"MagicFM.lib")
#pragma comment(lib,"ETools.lib")
#pragma comment(lib,"xrCoreB.lib")
#pragma comment(lib,"xrEPropsB.lib")
#pragma comment(lib,"xrECoreB.lib")
#pragma comment(lib,"xrSoundB.lib")
#pragma comment(lib, "libogg_static_b.lib")
#pragma comment(lib,"libtheora_static_b.lib")

#ifdef _WIN64
#ifdef __DEBUG
#pragma comment(lib, "d3dx9d.lib")
#else
#pragma comment(lib, "d3dx9.lib")
#endif
#endif


