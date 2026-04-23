#include "stdafx.h"
#pragma hdrstop
#include "../ECore/Editor/SplashScreen.h"
#include "../ECore/Editor/LogForm.h"
#include "../ECore/Editor/EditMesh.h"
#include "main.h"
#include "scene.h"
#include "UI_LevelMain.h"
#include "UI_LevelTools.h"

#include "../Include/stack_trace.h"

//---------------------------------------------------------------------------
USEFORM("FrameGroup.cpp", fraGroup);
USEFORM("FrameLight.cpp", fraLight);
USEFORM("FrameObject.cpp", fraObject);
USEFORM("FramePortal.cpp", fraPortal);
USEFORM("FramePS.cpp", fraPS);
USEFORM("FrameAIMap.cpp", fraAIMap);
USEFORM("FrameDetObj.cpp", fraDetailObject);
USEFORM("FrameFogVol.cpp", fraFogVol);
USEFORM("FrameSector.cpp", fraSector);
USEFORM("LEClipEditor.cpp", ClipMaker);
USEFORM("LeftBar.cpp", fraLeftBar); /* TFrame: File Type */
USEFORM("FrameShape.cpp", fraShape);
USEFORM("FrameSpawn.cpp", fraSpawn);
USEFORM("FrameWayPoint.cpp", fraWayPoint);
USEFORM("FrmDBXpacker.cpp", DB_packer);
USEFORM("RightForm.cpp", frmRight);
USEFORM("TopBar.cpp", fraTopBar); /* TFrame: File Type */
USEFORM("main.cpp", frmMain);
USEFORM("ObjectList.cpp", frmObjectList);
USEFORM("previewimage.cpp", frmPreviewImage);
USEFORM("PropertiesEObject.cpp", frmPropertiesEObject);
USEFORM("EditLibrary.cpp", frmEditLibrary);
USEFORM("EditLightAnim.cpp", frmEditLightAnim);
USEFORM("Edit\AppendObjectInfoForm.cpp", frmAppendObjectInfo);
USEFORM("BottomBar.cpp", fraBottomBar); /* TFrame: File Type */
USEFORM("DOOneColor.cpp", frmOneColor);
USEFORM("DOShuffle.cpp", frmDOShuffle);
//---------------------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int)
{
		if (!Application->Handle){
            Application->CreateHandle	();
            Application->Icon->Handle 	= LoadIcon(MainInstance, "MAINICON");
			Application->Title 			= "Loading...";
        }

		SplashScreen::Show		("Editor_Level_Splash.jpg", "Level Editor");
		SplashScreen::SetStatus	("Core initializing...");

		Core._initialize		("level",ELogCallback);
        CEditableMesh::SetDraftMeshMode(TRUE);

		SplashScreen::SetStatus	("Initializing application...");
		Application->Initialize	();

		SplashScreen::SetStatus	("Creating tools...");
		Tools					= xr_new<CLevelTool>();
		UI						= xr_new<CLevelMain>();
        UI->RegisterCommands	();

		SplashScreen::SetStatus	("Creating scene...");
		Scene					= xr_new<EScene>();
		Application->Title 		= UI->EditorDesc();

		SplashScreen::SetStatus	("Creating log...");
        TfrmLog::CreateLog		();

		SplashScreen::SetStatus	("Creating main window...");
		Application->CreateForm(__classid(TfrmMain), &frmMain);
		Application->CreateForm(__classid(TfrmRight), &frmRight);
		frmMain->SetHInst		(hInst);

		SplashScreen::Hide		();
		Application->Run		();

        TfrmLog::DestroyLog		();

		UI->ClearCommands		();
        xr_delete				(Scene);
        xr_delete				(Tools);
        xr_delete				(UI);

    	Core._destroy			();

    return 0;
}
//---------------------------------------------------------------------------
