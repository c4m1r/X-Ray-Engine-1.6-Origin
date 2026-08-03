//---------------------------------------------------------------------------
#ifndef EditorWindowsH
#define EditorWindowsH

class TUI;

namespace EditorWindows
{
    void Enforce(TUI* ui);
    ECORE_API void HookModal(void* application);
};

#endif
