#pragma once

#include <windows.h>
#include <string>

struct cl_log_window_config
{
	// Shown in menu "Инструкции" (MessageBox).
	const char* help_text;
	// If true, FlushLog() after syncing new lines to the listbox (xrDO_Light, xrAI).
	bool flush_log_after_lines;
	// First log banner line (tool name).
	const char* app_title;
	// Title bar (UTF-16). If nullptr, cl_log uses L"XRay" at dialog init.
	const wchar_t* window_title_w;
};

void cl_log_window_set_config(const cl_log_window_config& cfg);
void cl_log_window_set_title_w(const wchar_t* title_w);

// Call once from the worker thread that should respond to Pause (after log window exists).
void cl_log_window_register_worker_thread();

void logThread(void* dummy);

void clMsg(const char* format, ...);
void clLog(LPCSTR msg);
void Status(const char* format, ...);
void Progress(float F);
void Phase(const char* phase_name);
std::string make_time(u32 sec);

extern HWND logWindow;
extern volatile HANDLE mainThread;
extern volatile char* args;
extern volatile bool bClose;
