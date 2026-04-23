#include "stdafx.h"
#pragma hdrstop

#include "SplashScreen.h"
#include <windows.h>
#include <olectl.h>

namespace SplashScreen
{

static HWND				g_hwnd		= NULL;
static HANDLE			g_thread	= NULL;
static HBITMAP			g_bitmap	= NULL;
static int				g_width		= 480;
static int				g_height	= 270;
static CRITICAL_SECTION	g_cs;
static char				g_status[256]	= "";
static char				g_title[128]	= "Loading...";
static bool				g_initialized	= false;

static const int	CLOSE_BTN_SIZE	= 20;
static const int	CLOSE_BTN_PAD	= 6;
static const UINT	WM_SPLASH_UPDATE = WM_USER + 1;
static const wchar_t	WND_CLASS[]	= L"XRay_SplashScreen";

// ---- path resolution (search relative to EXE) ----

static bool FileExistsA(LPCSTR path)
{
	return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

static bool ResolveSplashPath(LPCSTR filename, char* out, int outSize)
{
	if (FileExistsA(filename))
	{
		strncpy(out, filename, outSize - 1);
		return true;
	}

	char exeDir[MAX_PATH] = {};
	GetModuleFileNameA(NULL, exeDir, MAX_PATH);
	char* p = strrchr(exeDir, '\\');
	if (p) *(p + 1) = 0;

	_snprintf(out, outSize, "%s%s", exeDir, filename);
	if (FileExistsA(out)) return true;

	_snprintf(out, outSize, "%simages\\Splash\\%s", exeDir, filename);
	if (FileExistsA(out)) return true;

	char walkDir[MAX_PATH];
	strncpy(walkDir, exeDir, MAX_PATH - 1);
	for (int i = 0; i < 8; ++i)
	{
		_snprintf(out, outSize, "%scode\\engine.vc2008\\editors\\images\\Splash\\%s",
				  walkDir, filename);
		if (FileExistsA(out)) return true;

		p = walkDir + strlen(walkDir) - 1;
		if (p > walkDir && *p == '\\') --p;
		while (p > walkDir && *p != '\\') --p;
		if (p <= walkDir) break;
		*(p + 1) = 0;
	}

	strncpy(out, filename, outSize - 1);
	return false;
}

// ---- image loading via OLE/IPicture (supports JPEG, BMP, GIF, etc.) ----

static HBITMAP LoadImageFromMemory(const void* data, DWORD size)
{
	if (!data || size == 0)
		return NULL;

	HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
	if (!hMem) return NULL;

	void* pMem = GlobalLock(hMem);
	memcpy(pMem, data, size);
	GlobalUnlock(hMem);

	IStream* pStream = NULL;
	if (FAILED(CreateStreamOnHGlobal(hMem, TRUE, &pStream)))
	{
		GlobalFree(hMem);
		return NULL;
	}

	IPicture* pPic = NULL;
	OleLoadPicture(pStream, (LONG)size, FALSE, IID_IPicture, (void**)&pPic);
	pStream->Release();

	if (!pPic)
		return NULL;

	OLE_HANDLE hOle = 0;
	pPic->get_Handle(&hOle);
	HBITMAP result = NULL;
	if (hOle)
		result = (HBITMAP)CopyImage((HANDLE)(ULONG_PTR)hOle,
									IMAGE_BITMAP, 0, 0, LR_COPYRETURNORG);
	pPic->Release();
	return result;
}

static HBITMAP LoadImageFromFile(LPCSTR path)
{
	if (!path || !path[0])
		return NULL;

	HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ,
							   NULL, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		return NULL;

	DWORD fileSize = GetFileSize(hFile, NULL);
	if (fileSize == 0 || fileSize == INVALID_FILE_SIZE)
	{
		CloseHandle(hFile);
		return NULL;
	}

	BYTE* buf = (BYTE*)malloc(fileSize);
	DWORD bytesRead = 0;
	ReadFile(hFile, buf, fileSize, &bytesRead, NULL);
	CloseHandle(hFile);

	HBITMAP result = LoadImageFromMemory(buf, bytesRead);
	free(buf);
	return result;
}

// ---- hit-test for close button ----

static bool HitCloseButton(int x, int y)
{
	return x >= g_width  - CLOSE_BTN_SIZE - CLOSE_BTN_PAD
		&& x <  g_width  - CLOSE_BTN_PAD
		&& y >= CLOSE_BTN_PAD
		&& y <  CLOSE_BTN_PAD + CLOSE_BTN_SIZE;
}

// ---- window procedure (runs on splash thread) ----

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		if (g_bitmap)
		{
			HDC mem = CreateCompatibleDC(hdc);
			HGDIOBJ old = SelectObject(mem, g_bitmap);
			BitBlt(hdc, 0, 0, g_width, g_height, mem, 0, 0, SRCCOPY);
			SelectObject(mem, old);
			DeleteDC(mem);
		}
		else
		{
			RECT rc = {0, 0, g_width, g_height};
			FillRect(hdc, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
		}

		SetBkMode(hdc, TRANSPARENT);

		// status text
		EnterCriticalSection(&g_cs);
		if (g_status[0])
		{
			RECT rcText = {4, g_height - 20, g_width - 4, g_height - 2};
			HFONT hFont = CreateFontA(-11, 0, 0, 0, FW_NORMAL, 0, 0, 0,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, DEFAULT_PITCH, "Lucida Console");
			HGDIOBJ oldFont = SelectObject(hdc, hFont);
			SetTextColor(hdc, RGB(255, 255, 255));
			DrawTextA(hdc, g_status, -1, &rcText,
					  DT_CENTER | DT_SINGLELINE | DT_VCENTER);
			SelectObject(hdc, oldFont);
			DeleteObject(hFont);
		}
		LeaveCriticalSection(&g_cs);

		// close button "X"
		{
			RECT rcBtn = {
				g_width - CLOSE_BTN_SIZE - CLOSE_BTN_PAD, CLOSE_BTN_PAD,
				g_width - CLOSE_BTN_PAD, CLOSE_BTN_PAD + CLOSE_BTN_SIZE
			};
			HFONT hFont = CreateFontA(-14, 0, 0, 0, FW_BOLD, 0, 0, 0,
				DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY, DEFAULT_PITCH, "Arial");
			HGDIOBJ oldFont = SelectObject(hdc, hFont);
			SetTextColor(hdc, RGB(220, 220, 220));
			DrawTextW(hdc, L"\x2715", 1, &rcBtn,
					  DT_CENTER | DT_SINGLELINE | DT_VCENTER);
			SelectObject(hdc, oldFont);
			DeleteObject(hFont);
		}

		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_LBUTTONDOWN:
	{
		int x = (short)LOWORD(lp);
		int y = (short)HIWORD(lp);
		if (HitCloseButton(x, y))
		{
			TerminateProcess(GetCurrentProcess(), 0);
			return 0;
		}
		ReleaseCapture();
		SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
		return 0;
	}

	case WM_SETCURSOR:
	{
		POINT pt;
		GetCursorPos(&pt);
		ScreenToClient(hwnd, &pt);
		SetCursor(LoadCursor(NULL, HitCloseButton(pt.x, pt.y)
									? IDC_HAND : IDC_ARROW));
		return TRUE;
	}

	case WM_SPLASH_UPDATE:
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}

// ---- splash thread entry point ----

static DWORD WINAPI ThreadProc(LPVOID)
{
	HINSTANCE hInst = GetModuleHandle(NULL);

	WNDCLASSEXW wc	= {};
	wc.cbSize		= sizeof(wc);
	wc.style		= CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc	= WndProc;
	wc.hInstance	= hInst;
	wc.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground= (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszClassName= WND_CLASS;
	RegisterClassExW(&wc);

	int sx = (GetSystemMetrics(SM_CXSCREEN) - g_width)  / 2;
	int sy = (GetSystemMetrics(SM_CYSCREEN) - g_height) / 2;

	wchar_t wTitle[128] = {};
	MultiByteToWideChar(CP_ACP, 0, g_title, -1, wTitle, 127);

	g_hwnd = CreateWindowExW(
		WS_EX_APPWINDOW,
		WND_CLASS, wTitle,
		WS_POPUP | WS_VISIBLE,
		sx, sy, g_width, g_height,
		NULL, NULL, hInst, NULL);

	if (!g_hwnd)
		return 1;

	ShowWindow(g_hwnd, SW_SHOW);
	UpdateWindow(g_hwnd);

	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	g_hwnd = NULL;
	return 0;
}

// ---- public API ----

static void StartSplash(LPCSTR title)
{
	if (title)
		strncpy(g_title, title, sizeof(g_title) - 1);

	if (g_bitmap)
	{
		BITMAP bm;
		if (GetObject(g_bitmap, sizeof(bm), &bm))
		{
			g_width  = bm.bmWidth;
			g_height = bm.bmHeight;
		}
	}

	g_thread = CreateThread(NULL, 0, ThreadProc, NULL, 0, NULL);

	for (int i = 0; i < 500 && !g_hwnd; ++i)
		Sleep(10);
}

void Show(LPCSTR imagePath, LPCSTR title)
{
	if (g_initialized)
		return;
	g_initialized = true;
	InitializeCriticalSection(&g_cs);

	char resolved[MAX_PATH] = {};
	ResolveSplashPath(imagePath, resolved, MAX_PATH);

	CoInitialize(NULL);
	g_bitmap = LoadImageFromFile(resolved);
	CoUninitialize();

	StartSplash(title);
}

void ShowFromMem(const void* data, u32 size, LPCSTR title)
{
	if (g_initialized)
		return;
	g_initialized = true;
	InitializeCriticalSection(&g_cs);

	CoInitialize(NULL);
	g_bitmap = LoadImageFromMemory(data, size);
	CoUninitialize();

	StartSplash(title);
}

void SetStatus(LPCSTR text)
{
	if (!g_hwnd)
		return;
	EnterCriticalSection(&g_cs);
	if (text)
		strncpy(g_status, text, sizeof(g_status) - 1);
	LeaveCriticalSection(&g_cs);
	PostMessage(g_hwnd, WM_SPLASH_UPDATE, 0, 0);
}

void Hide()
{
	if (g_hwnd)
		PostMessage(g_hwnd, WM_CLOSE, 0, 0);

	if (g_thread)
	{
		WaitForSingleObject(g_thread, 5000);
		CloseHandle(g_thread);
		g_thread = NULL;
	}

	if (g_bitmap)
	{
		DeleteObject(g_bitmap);
		g_bitmap = NULL;
	}

	if (g_initialized)
	{
		DeleteCriticalSection(&g_cs);
		g_initialized = false;
	}
}

} // namespace SplashScreen
