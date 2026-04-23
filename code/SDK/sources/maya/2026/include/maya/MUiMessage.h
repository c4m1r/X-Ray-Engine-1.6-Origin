#pragma once
//-
// ===========================================================================
// Copyright 2018 Autodesk, Inc. All rights reserved.
//
// Use of this software is subject to the terms of the Autodesk license
// agreement provided at the time of installation or download, or which
// otherwise accompanies this software in either electronic or hard copy form.
// ===========================================================================
//+
//
// CLASS:    MUiMessage
//
// ****************************************************************************

#include <maya/MMessage.h>

OPENMAYA_MAJOR_NAMESPACE_OPEN

// ****************************************************************************
// CLASS DECLARATION (MUiMessage)

//! \ingroup OpenMayaUI
//! \brief UI messages
/*!

  This class is used to register callbacks to track the deletion of UI
  objects.

  The first parameter passed to the add callback method is the name of
  the UI that will trigger the callback.

  The method returns an id which is used to remove the callback.

  To remove a callback use MMessage::removeCallback.

  All callbacks that are registered by a plug-in must be removed by
  that plug-in when it is unloaded.  Failure to do so will result in a
  fatal error.
*/
class OPENMAYAUI_EXPORT MUiMessage : public MMessage
{
public:

	static MCallbackId	addUiDeletedCallback(
		const MString& uiName,
		MMessage::MBasicFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	addCameraChangedCallback(
		const MString& panelName,
		MMessage::MStringNode func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewDestroyMsgCallback(
		const MString& panelName,
		MMessage::MStringFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewPreRenderMsgCallback(
		const MString& panelName,
		MMessage::MStringFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewPostRenderMsgCallback(
		const MString& panelName,
		MMessage::MStringFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewPreMultipleDrawPassMsgCallback(
		const MString& panelName,
		MUiMessage::MStringIndexFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewPostMultipleDrawPassMsgCallback(
		const MString& panelName,
		MUiMessage::MStringIndexFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewRendererChangedCallback(
		const MString& panelName,
		MMessage::MThreeStringFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

	static MCallbackId	add3dViewRenderOverrideChangedCallback(
		const MString& panelName,
		MMessage::MThreeStringFunction func,
		void * clientData = NULL,
		MStatus * ReturnStatus = NULL );

BEGIN_NO_SCRIPT_SUPPORT:
	// This method does not get SWIG-ed properly for the OpenMayaNet version of
	// the API.  For an unknown reason, the generated code has an undefined
	// "bridge" identifier, so will not SWIG this.
	#if !defined(SWIG)
OPENMAYA_AVAILABLE(2026)
	//! \brief Add a callback to be notified when the view selected state or
	//! view selected objects for any 3d view changes.
	//! NO SCRIPT SUPPORT
	static MCallbackId	add3dViewSelectedChangedCallback(
		MUiMessage::MStringBoolFunction func,
		void * clientData = nullptr,
		MStatus * ReturnStatus = nullptr );
	#endif
END_NO_SCRIPT_SUPPORT:

	static const char* className();
};
OPENMAYA_NAMESPACE_CLOSE
