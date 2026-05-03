#include "stdafx.h"
#include "cl_log.h"
#include "../compiler_log_window/cl_log_window.h"
#include "../xrLC_Light/xrLC_Light.h"

void logCallback(LPCSTR) {}

class client_log_impl : public i_lc_log
{
	virtual void clMsg(LPCSTR msg) { ::clMsg(msg); }
	virtual void clLog(LPCSTR) {}
	virtual void Status(LPCSTR msg) { ::Status(msg); }
	virtual void Progress(const float F) { ::Progress(F); }
	virtual void Phase(LPCSTR phase_name) { ::Phase(phase_name); }

public:
	client_log_impl() { lc_log = this; }
} client_log_impl;
