//---------------------------------------------------------------------------
#ifndef LevelPreferencesH
#define LevelPreferencesH

#include "../ECore/Editor/EditorPreferences.h"

class CLevelPreferences: public CCustomPreferences
{
	typedef CCustomPreferences			inherited;
	void __stdcall	OnEnabledChange		(PropValue* prop);
	void __stdcall	OnReadonlyChange	(PropValue* prop);
protected:
    virtual void 	Load				(CInifile*);
    virtual void 	Save				(CInifile*);
public:
					CLevelPreferences	();
	// Tools: LODs Builder (stored in level.ini, section editor_prefs)
	BOOL			bLODsBuilderUseGPU;
    virtual void	FillProp          	(PropItemVec& items);
};
//---------------------------------------------------------------------------
#endif
