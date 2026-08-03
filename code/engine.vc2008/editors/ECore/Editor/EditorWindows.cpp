//---------------------------------------------------------------------------
#include "stdafx.h"
#pragma hdrstop

#include "EditorWindows.h"
#include "ui_main.h"

namespace {

const char P_IDX[]  = "XRwmIdx";
const char P_ORIG[] = "XRwmOrig";

struct DockData
{
    bool used;
    bool docked;
    int  slot;
    int  stripH;
    RECT saved;
};

DockData s_data[128];
bool     s_slot[64]  = { false };
HWND     s_main      = 0;
RECT     s_lastMain  = { 0, 0, 0, 0 };

WNDPROC OrigOf(HWND h) { return (WNDPROC)GetPropA(h, P_ORIG); }

DockData* DataOf(HWND h)
{
    int idx = (int)(INT_PTR)GetPropA(h, P_IDX);
    if (idx <= 0 || idx > 128) return nullptr;
    DockData* d = &s_data[idx - 1];
    return d->used ? d : nullptr;
}

int StripW()
{
    int w = GetSystemMetrics(SM_CXMINIMIZED);
    return w > 0 ? w : 160;
}

void SlotPos(int slot, int stripH, int& x, int& y, int& w)
{
    RECT mr; GetWindowRect(s_main, &mr);
    w = StripW();
    int perRow = (mr.right - mr.left) / w;
    if (perRow < 1) perRow = 1;
    x = mr.left + (slot % perRow) * w;
    y = mr.bottom - stripH * (slot / perRow + 1);
}

int  AllocSlot() { for (int s = 0; s < 64; s++) if (!s_slot[s]) { s_slot[s] = true; return s; } return 0; }
void FreeSlot(int s) { if (s >= 0 && s < 64) s_slot[s] = false; }

void Dock(HWND h, DockData* d)
{
    GetWindowRect(h, &d->saved);
    RECT cr; GetClientRect(h, &cr);
    d->stripH = (d->saved.bottom - d->saved.top) - (cr.bottom - cr.top);
    if (d->stripH < 8)
        d->stripH = GetSystemMetrics(SM_CYCAPTION) + 2 * GetSystemMetrics(SM_CYSIZEFRAME);
    d->slot   = AllocSlot();
    d->docked = true;
    int x, y, w; SlotPos(d->slot, d->stripH, x, y, w);
    SetWindowPos(h, 0, x, y, w, d->stripH, SWP_NOZORDER | SWP_NOACTIVATE);
}

void Undock(HWND h, DockData* d)
{
    d->docked = false;
    FreeSlot(d->slot);
    SetWindowPos(h, 0, d->saved.left, d->saved.top,
                 d->saved.right - d->saved.left, d->saved.bottom - d->saved.top,
                 SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT CALLBACK DockProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    WNDPROC orig = OrigOf(h);
    if (!orig) return DefWindowProcA(h, msg, wp, lp);
    DockData* d = DataOf(h);

    if (d)
    {
        if (msg == WM_SYSCOMMAND)
        {
            UINT cmd = wp & 0xFFF0;
            if (cmd == SC_MINIMIZE) { if (d->docked) Undock(h, d); else Dock(h, d); return 0; }
            if ((cmd == SC_RESTORE || cmd == SC_MAXIMIZE) && d->docked) { Undock(h, d); return 0; }
        }
        else if (msg == WM_GETMINMAXINFO && d->docked)
        {
            LRESULT r = CallWindowProcA(orig, h, msg, wp, lp);
            MINMAXINFO* mmi = (MINMAXINFO*)lp;
            int sh = d->stripH > 8 ? d->stripH : 40;
            if (mmi->ptMinTrackSize.y > sh) mmi->ptMinTrackSize.y = sh;
            if (mmi->ptMinTrackSize.x > 60) mmi->ptMinTrackSize.x = 60;
            return r;
        }
        else if (msg == WM_WINDOWPOSCHANGING && d->docked)
        {
            LRESULT r = CallWindowProcA(orig, h, msg, wp, lp);
            WINDOWPOS* wpz = (WINDOWPOS*)lp;
            if (!(wpz->flags & SWP_NOSIZE)) { wpz->cx = StripW(); wpz->cy = d->stripH; }
            return r;
        }
        else if (msg == WM_NCLBUTTONDBLCLK && d->docked)
        {
            Undock(h, d);
            return 0;
        }
    }

    if (msg == WM_NCDESTROY)
    {
        if (d) { FreeSlot(d->slot); d->used = false; }
        RemovePropA(h, P_IDX);
        RemovePropA(h, P_ORIG);
        SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)orig);
        return CallWindowProcA(orig, h, msg, wp, lp);
    }

    return CallWindowProcA(orig, h, msg, wp, lp);
}

int AllocData()
{
    for (int i = 0; i < 128; i++)
        if (!s_data[i].used) { ZeroMemory(&s_data[i], sizeof(DockData)); s_data[i].used = true; return i; }
    return -1;
}

void Subclass(HWND h)
{
    if (OrigOf(h)) return;
    int idx = AllocData();
    if (idx < 0) return;
    WNDPROC orig = (WNDPROC)SetWindowLongPtrA(h, GWLP_WNDPROC, (LONG_PTR)DockProc);
    SetPropA(h, P_ORIG, (HANDLE)orig);
    SetPropA(h, P_IDX,  (HANDLE)(INT_PTR)(idx + 1));
}

bool IsEditorForm(HWND h, HWND mainH)
{
    if (h == mainH)                    return false;
    if (GetAncestor(h, GA_ROOT) != h)  return false;
    char cls[64] = { 0 };
    GetClassNameA(h, cls, sizeof(cls));
    if (cls[0] != 'T')                 return false;
    if (0 == strcmp(cls, "TApplication")) return false;
    if (0 == strcmp(cls, "THintWindow"))  return false;
    return true;
}

BOOL CALLBACK EnforceProc(HWND h, LPARAM lp)
{
    HWND mainH = (HWND)lp;
    if (!IsEditorForm(h, mainH)) return TRUE;
    if (!IsIconic(mainH) && IsIconic(h))
        ShowWindow(h, SW_RESTORE);
    if (GetWindow(h, GW_OWNER) == 0)
        SetWindowLongPtrA(h, GWLP_HWNDPARENT, (LONG_PTR)mainH);
    if (GetWindowLongA(h, GWL_STYLE) & WS_CAPTION)
        Subclass(h);
    return TRUE;
}

BOOL CALLBACK RepositionProc(HWND h, LPARAM)
{
    DockData* d = DataOf(h);
    if (d && d->docked)
    {
        int x, y, w; SlotPos(d->slot, d->stripH, x, y, w);
        SetWindowPos(h, 0, x, y, w, d->stripH, SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return TRUE;
}

}

void EditorWindows::Enforce(TUI* ui)
{
    if (!ui || !ui->m_bReady) return;
    HWND mainH = GetAncestor((HWND)ui->GetHWND(), GA_ROOT);
    if (!mainH) return;
    s_main = mainH;
    EnumThreadWindows(GetCurrentThreadId(), EnforceProc, (LPARAM)mainH);

    RECT mr; GetWindowRect(mainH, &mr);
    if (mr.left != s_lastMain.left || mr.top != s_lastMain.top ||
        mr.right != s_lastMain.right || mr.bottom != s_lastMain.bottom)
    {
        EnumThreadWindows(GetCurrentThreadId(), RepositionProc, 0);
        s_lastMain = mr;
    }
}
