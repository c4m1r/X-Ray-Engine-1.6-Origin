#include "stdafx.h"
#include "cl_log_window.h"
#include "cl_log_window_ids.h"
#include "../../xrcore/log.h"
#include <time.h>
#include <mmsystem.h>

static const wchar_t kClgMenuOptions[] = L"Опции";
static const wchar_t kClgMenuInstructions[] = L"Инструкции";
static const wchar_t kClgMenuPauseInactive[] =
	L"Пауза компиляции: не активна";
static const wchar_t kClgMenuPauseActive[] =
	L"Пауза компиляции: активна";

static xrCriticalSection csLog
#ifdef PROFILE_CRITICAL_SECTIONS
	(MUTEX_PROFILE_ID(csLog))
#endif
	;

volatile bool bClose = false;

static char status[1024] = "";
static char phase[1024] = "";
static float progress = 0.0f;
static u32 phase_start_time = 0;
static BOOL bStatusChange = FALSE;
static u32 phase_total_time = 0;

static HWND hwLog = 0;
static HWND hwProgress = 0;
static HWND hwInfo = 0;
static HWND hwStage = 0;
static HWND hwTime = 0;
static HWND hwPText = 0;
static HWND hwPhaseTime = 0;

HWND logWindow = NULL;
volatile HANDLE mainThread = NULL;
volatile char* args = NULL;

static HMENU hMenu = NULL;
static bool s_clLogMenuWeOwn = false;
static bool isMainThread = true;

static cl_log_window_config g_cfg = { "", false, "Compiler", nullptr };

/* POPUP "Опции" is the first top-level item; submenu handle is not the bar HMENU. */
static HMENU cl_log_options_submenu(HMENU hBar) { return hBar ? GetSubMenu(hBar, 0) : NULL; }

/* Rebuild bar from UTF-16 literals so text does not depend on .rc + rc.exe code page (submenus still broke for some). */
static void cl_log_install_menu_unicode(HWND hw)
{
	HMENU hOld = GetMenu(hw);
	HMENU hBar = CreateMenu();
	HMENU hOpt = CreatePopupMenu();
	if (!hBar || !hOpt)
	{
		if (hBar) DestroyMenu(hBar);
		if (hOpt) DestroyMenu(hOpt);
		if (hOld)
		{
			hMenu = hOld;
			HMENU hO = cl_log_options_submenu(hMenu);
			if (hO)
			{
				ModifyMenuW(hO, 0, MF_BYPOSITION | MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseInactive);
			}
		}
		return;
	}
	{
		const BOOL a = AppendMenuW(hOpt, MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseInactive);
		const BOOL b = a && AppendMenuW(hBar, MF_STRING | MF_POPUP, (UINT_PTR)hOpt, kClgMenuOptions);
		const BOOL c = b && AppendMenuW(hBar, MF_STRING, CLG_ID_INSTRUCTION, kClgMenuInstructions);
		if (!c)
		{
			if (b) DestroyMenu(hBar);  /* hOpt is owned by hBar */
			else
			{
				if (a) DestroyMenu(hOpt);
				DestroyMenu(hBar);
			}
			if (hOld)
			{
				hMenu = hOld;
				HMENU hO = cl_log_options_submenu(hMenu);
				if (hO)
				{
					ModifyMenuW(hO, 0, MF_BYPOSITION | MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseInactive);
				}
			}
			return;
		}
	}
	/* SetMenu returns BOOL; previous bar is the hOld from GetMenu above. */
	if (!SetMenu(hw, hBar))
	{
		DestroyMenu(hBar);
		if (hOld)
		{
			hMenu = hOld;
			HMENU hO = cl_log_options_submenu(hMenu);
			if (hO)
				ModifyMenuW(hO, 0, MF_BYPOSITION | MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseInactive);
		}
		return;
	}
	if (hOld) DestroyMenu(hOld);
	hMenu = hBar;
	s_clLogMenuWeOwn = true;
}

/* Title bar: UTF-16 only (avoids ACP/ANSI mojibake with non-ASCII). */
static void cl_log_apply_window_caption(HWND hw)
{
	if (!hw) return;
	if (g_cfg.window_title_w)
		SetWindowTextW(hw, g_cfg.window_title_w);
	else
		SetWindowTextW(hw, L"XRay");
}

void cl_log_window_set_title_w(const wchar_t* title_w)
{
	if (logWindow) SetWindowTextW(logWindow, title_w ? title_w : L"");
}

static int s_max_log_line_px = 0;

void cl_log_window_set_config(const cl_log_window_config& cfg) { g_cfg = cfg; }

void cl_log_window_register_worker_thread()
{
	if (mainThread)
		return;
	HANDLE h = NULL;
	if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &h, 0, FALSE, DUPLICATE_SAME_ACCESS))
		mainThread = h;
}

static void log_list_refresh_h_extent_after_add(const char* lastLine, int lineLen)
{
	if (!hwLog || !lastLine)
		return;
	HDC hdc = GetDC(hwLog);
	HFONT hf = (HFONT)SendMessage(hwLog, WM_GETFONT, 0, 0);
	HFONT oldf = NULL;
	if (hf)
		oldf = (HFONT)SelectObject(hdc, hf);
	SIZE sz = {};
	GetTextExtentPoint32A(hdc, lastLine, lineLen, &sz);
	if (sz.cx > s_max_log_line_px)
		s_max_log_line_px = sz.cx;
	if (oldf)
		SelectObject(hdc, oldf);
	ReleaseDC(hwLog, hdc);
	RECT rc = {};
	GetClientRect(hwLog, &rc);
	int cw = rc.right - rc.left;
	SendMessage(hwLog, LB_SETHORIZONTALEXTENT, (WPARAM)((s_max_log_line_px > cw) ? s_max_log_line_px : 0), 0);
}

static void PressButtonPause()
{
	if (!hMenu || !logWindow)
		return;
	/* W API + UTF-16 literals: with _MBCS/UTF-8 .cpp, ModifyMenuA mangles non-ASCII */
	HMENU hOpt = cl_log_options_submenu(hMenu);
	if (!hOpt)
		return;
	if (isMainThread)
	{
		ModifyMenuW(hOpt, 0, MF_BYPOSITION | MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseActive);
		CheckMenuItem(hOpt, 0, MF_BYPOSITION | MF_CHECKED);
	}
	else
	{
		ModifyMenuW(hOpt, 0, MF_BYPOSITION | MF_STRING, CLG_ID_MAINMENU_PAUSE, kClgMenuPauseInactive);
		CheckMenuItem(hOpt, 0, MF_BYPOSITION | MF_UNCHECKED);
	}
	DrawMenuBar(logWindow);

	if (!mainThread)
		return;
	if (isMainThread)
		SuspendThread(mainThread);
	else
		ResumeThread(mainThread);
	isMainThread = !isMainThread;
}

static INT_PTR CALLBACK logDlgProc(HWND hw, UINT msg, WPARAM wp, LPARAM lp)
{
	(void)lp;
	switch (msg)
	{
	case WM_INITDIALOG:
		cl_log_install_menu_unicode(hw);
		cl_log_apply_window_caption(hw);
		RegisterHotKey(hw, CLG_HOTKEY_PAUSE, 0, VK_PAUSE);
		break;

	case WM_DESTROY:
		UnregisterHotKey(hw, CLG_HOTKEY_PAUSE);
		if (s_clLogMenuWeOwn && hMenu)
		{
			DestroyMenu(hMenu);
			s_clLogMenuWeOwn = false;
			hMenu = NULL;
		}
		break;

	case WM_CLOSE:
		ExitProcess(0);
		break;

	case WM_HOTKEY:
		if (wp == CLG_HOTKEY_PAUSE)
			PressButtonPause();
		break;

	case WM_COMMAND:
		switch (LOWORD(wp))
		{
		case CLG_ID_MAINMENU_PAUSE:
			PressButtonPause();
			break;
		case CLG_ID_INSTRUCTION:
			MessageBox(logWindow, g_cfg.help_text ? g_cfg.help_text : "", "Command line options",
					   MB_OK | MB_ICONINFORMATION);
			break;
		case IDCANCEL:
			ExitProcess(0);
			break;
		default:
			return FALSE;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void _process_messages(void)
{
	MSG msg;
	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

std::string make_time(u32 sec)
{
	char buf[64];
	xr_sprintf(buf, "%2.0d:%2.0d:%2.0d", sec / 3600, (sec % 3600) / 60, sec % 60);
	int len = int(xr_strlen(buf));
	for (int i = 0; i < len; i++)
		if (buf[i] == ' ')
			buf[i] = '0';
	return std::string(buf);
}

void __cdecl Status(const char* format, ...)
{
	csLog.Enter();
	va_list mark;
	va_start(mark, format);
	vsprintf(status, format, mark);
	bStatusChange = TRUE;
	Msg("    | %s", status);
	csLog.Leave();
}

void Progress(const float F) { progress = F; }

void Phase(const char* phase_name)
{
	while (!(hwPhaseTime && hwStage))
		Sleep(1);

	csLog.Enter();
	char tbuf[512];
	phase_total_time = timeGetTime() - phase_start_time;
	xr_sprintf(tbuf, "%s : %s", make_time(phase_total_time / 1000).c_str(), phase);
	SendMessage(hwPhaseTime, LB_DELETESTRING, SendMessage(hwPhaseTime, LB_GETCOUNT, 0, 0) - 1, 0);
	SendMessage(hwPhaseTime, LB_ADDSTRING, 0, (LPARAM)tbuf);

	phase_start_time = timeGetTime();
	xr_strcpy(phase, phase_name);
	SetWindowText(hwStage, phase_name);
	xr_sprintf(tbuf, "--:--:-- * %s", phase);
	SendMessage(hwPhaseTime, LB_ADDSTRING, 0, (LPARAM)tbuf);
	SendMessage(hwPhaseTime, LB_SETTOPINDEX, SendMessage(hwPhaseTime, LB_GETCOUNT, 0, 0) - 1, 0);
	Progress(0);

	Msg("\n* New phase started: %s", phase_name);
	csLog.Leave();
}

void clLog(LPCSTR msg)
{
	csLog.Enter();
	Log(msg);
	csLog.Leave();
}

void logThread(void* dummy)
{
	(void)dummy;
	SetProcessPriorityBoost(GetCurrentProcess(), TRUE);

	logWindow = CreateDialog(HINSTANCE(GetModuleHandle(0)), MAKEINTRESOURCE(CLG_IDD_LOG), 0, logDlgProc);
	if (!logWindow)
		R_CHK(GetLastError());

	SetWindowPos(logWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	hwLog = GetDlgItem(logWindow, CLG_IDC_LOG);
	hwProgress = GetDlgItem(logWindow, CLG_IDC_PROGRESS);
	hwInfo = GetDlgItem(logWindow, CLG_IDC_INFO);
	hwStage = GetDlgItem(logWindow, CLG_IDC_STAGE);
	hwTime = GetDlgItem(logWindow, CLG_IDC_TIMING);
	hwPText = GetDlgItem(logWindow, CLG_IDC_P_TEXT);
	hwPhaseTime = GetDlgItem(logWindow, CLG_IDC_PHASE_TIME);

	SendMessage(hwProgress, PBM_SETRANGE, 0, MAKELPARAM(0, 1000));
	SendMessage(hwProgress, PBM_SETPOS, 0, 0);

	{
		const char* title = g_cfg.app_title && g_cfg.app_title[0] ? g_cfg.app_title : "Compiler";
		Msg("\"%s\"\nCompilation date: %s\n", title, __DATE__);
		char tmpbuf[128];
		Msg("Startup time: %s", _strtime(tmpbuf));
	}

	BOOL bHighPriority = FALSE;
	string256 u_name;
	unsigned long u_size = sizeof(u_name) - 1;
	GetUserName(u_name, &u_size);
	_strlwr(u_name);
	if ((0 == xr_strcmp(u_name, "oles")) || (0 == xr_strcmp(u_name, "alexmx")))
		bHighPriority = TRUE;

	u32 LogSize = 0;
	float PrSave = 0;
	while (TRUE)
	{
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

		while (!csLog.TryEnter())
		{
			_process_messages();
			Sleep(1);
		}
		if (progress > 1.f)
			progress = 1.f;
		else if (progress < 0)
			progress = 0;

		BOOL bWasChanges = FALSE;
		char tbuf[256];
		csLog.Enter();
		if (LogSize != LogFile->size())
		{
			bWasChanges = TRUE;
			for (; LogSize < LogFile->size(); LogSize++)
			{
				const char* S = *(*LogFile)[LogSize];
				if (0 == S)
					S = "";
				SendMessage(hwLog, LB_ADDSTRING, 0, (LPARAM)S);
				int len = (int)xr_strlen(S);
				log_list_refresh_h_extent_after_add(S, len);
			}
			SendMessage(hwLog, LB_SETTOPINDEX, LogSize - 1, 0);
			if (g_cfg.flush_log_after_lines)
				FlushLog();
		}
		csLog.Leave();
		if (_abs(PrSave - progress) > EPS_L)
		{
			bWasChanges = TRUE;
			PrSave = progress;
			SendMessage(hwProgress, PBM_SETPOS, u32(progress * 1000.f), 0);

			if (progress > 0.005f)
			{
				u32 dwCurrentTime = timeGetTime();
				u32 dwTimeDiff = dwCurrentTime - phase_start_time;
				u32 secElapsed = dwTimeDiff / 1000;
				u32 secRemain = u32(float(secElapsed) / progress) - secElapsed;
				xr_sprintf(tbuf,
						   "Elapsed: %s\n"
						   "Remain:  %s",
						   make_time(secElapsed).c_str(),
						   make_time(secRemain).c_str());
				SetWindowText(hwTime, tbuf);
			}
			else
				SetWindowText(hwTime, "");

			xr_sprintf(tbuf, "%3.2f%%", progress * 100.f);
			SetWindowText(hwPText, tbuf);
		}

		if (bStatusChange)
		{
			bWasChanges = TRUE;
			bStatusChange = FALSE;
			SetWindowText(hwInfo, status);
		}
		if (bWasChanges)
		{
			UpdateWindow(logWindow);
			bWasChanges = FALSE;
		}
		csLog.Leave();

		_process_messages();
		if (bClose)
			break;
		Sleep(200);
	}

	DestroyWindow(logWindow);
}

void __cdecl clMsg(const char* format, ...)
{
	va_list mark;
	char buf[4 * 256];
	va_start(mark, format);
	vsprintf(buf, format, mark);

	csLog.Enter();
	string1024 _out_;
	strconcat(sizeof(_out_), _out_, "    |    | ", buf);
	clLog(_out_);
	csLog.Leave();
}
