#ifndef Hwarnings_h
#define Hwarnings_h

/*!
	Definitions of macros that allow targeted disabling of compiler warnings
	in a platform-independent way.

	Much of this is inspired by similar QT macros from qcompilerdetection.h

	Macros that can be used include:

	1) Platform-independent macros
		WARN_PUSH(msvc, gcc, clang, intel)	: Enable a warning; parameters indicate the platform-specific warning id
		WARN_POP							: Pop the latest warning

	2) Platform-specific macros, when you want to target a subset of all platforms
		WARN_PUSH_INTEL(id)	: Enable warning number "id" only when using the Intel compiler
		WARN_PUSH_MSVC(id)	: Enable warning number "id" only when using the Visual Studio compiler
		WARN_PUSH_CLANG(id)	: Enable warning name "id" only when using the clang compiler
		WARN_PUSH_GCC(id)	: Enable warning name "id" only when using the gcc compiler
	
	Normal usage is to bracket offending code:
		WARN_PUSH_INTEL(594)
		static tbb::enumerable_thread_specific<int> _evalDepth(0);
		WARN_POP_INTEL
	
	Use this for including third-party files containing warnings that cannot be fixed:
		WARN_PUSH(4000,-Wtautological-pointer-compare,-Wtautological-compare)
		#include <OGS/offending_file.h>
		WARN_POP

	You can also handle multiple warnings from the same location:
		WARN_PUSH_INTEL(594)
		WARN_OFF_INTEL(1125)
		#include <OSG/offending_file.h>
		WARN_POP_INTEL
	
	Don't do this, though, or your warning will be off for the the entire file, not just the bracketed area:
		WARN_OFF_INTEL(1125)
		#include <OSG/offending_file.h>
	
	When you only have warnings in a subset of the compilers use the following codes as placeholders:
		MSVC:  4000
		gcc:   "-Wunknown-variable"
		clang: "-Wunknown-variable"
		Intel: 4000

    #######################################################################

	THIS IS *NOT* INTENDED TO BE USED AS A SUBSTITUTE FOR ACTUALLY FIXING
	THE WARNINGS IN YOUR CODE! It is for silencing warnings arising from
	third-party code that we don't control (e.g. OGS, tbb, Qt).

    #######################################################################
*/
#define DO_PRAGMA(text) _Pragma(#text)

#if defined(_MSC_VER)

//----------------------------------------------------------------------
// The Intel compiler on Windows is different from it on other platforms
#	if defined(__INTEL_COMPILER)

#		define WARN_PUSH_INTEL(id)							__pragma(warning(push));				\
															__pragma(warning(disable: id))
#		define WARN_PUSH_MSVC(id)
#		define WARN_PUSH_CLANG(id)
#		define WARN_PUSH_GCC(id)
#		define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)	__pragma(warning(push));				\
															__pragma(warning(disable: intel_id))
//---
#		define WARN_OFF_INTEL(id)							__pragma(warning(disable: id))
#		define WARN_OFF_MSVC(id)
#		define WARN_OFF_CLANG(id)
#		define WARN_OFF_GCC(id)
#		define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)	__pragma(warning(disable: intel_id))
//---
#		define WARN_POP										__pragma(warning(pop))
#		define WARN_POP_INTEL								__pragma(warning(pop))
#		define WARN_POP_MSVC
#		define WARN_POP_CLANG
#		define WARN_POP_GCC

//----------------------------------------------------------------------
#	else

#		define WARN_PUSH_INTEL(id)
#		define WARN_PUSH_MSVC(id)							__pragma(warning(push));				\
															__pragma(warning(disable: id))
#		define WARN_PUSH_CLANG(id)
#		define WARN_PUSH_GCC(id)
#		define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)	__pragma(warning(push));				\
															__pragma(warning(disable: msvc_id))
//---
#		define WARN_OFF_INTEL(id)
#		define WARN_OFF_MSVC(id)							__pragma(warning(disable: id))
#		define WARN_OFF_CLANG(id)
#		define WARN_OFF_GCC(id)
#		define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)	__pragma(warning(disable: msvc_id))
//---
#		define WARN_POP										__pragma(warning(pop))
#		define WARN_POP_INTEL
#		define WARN_POP_MSVC								__pragma(warning(pop))
#		define WARN_POP_CLANG
#		define WARN_POP_GCC

#	endif

//----------------------------------------------------------------------
#elif defined(__INTEL_COMPILER)

#	define WARN_PUSH_INTEL(id)							DO_PRAGMA(warning push);					\
														DO_PRAGMA(warning (disable:id))
#	define WARN_PUSH_MSVC(id)
#	define WARN_PUSH_CLANG(id)
#	define WARN_PUSH_GCC(id)
#	define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(warning push);					\
														DO_PRAGMA(warning (disable:intel_id))
//---
#	define WARN_OFF_INTEL(id)							DO_PRAGMA(warning (disable:id))
#	define WARN_OFF_MSVC(id)
#	define WARN_OFF_CLANG(id)
#	define WARN_OFF_GCC(id)
#	define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(warning (disable:intel_id))
//---
#	define WARN_POP										DO_PRAGMA(warning pop)
#	define WARN_POP_INTEL								DO_PRAGMA(warning pop)
#	define WARN_POP_MSVC
#	define WARN_POP_CLANG
#	define WARN_POP_GCC

//----------------------------------------------------------------------
#elif defined(__clang__)

//	XCode 7 warnings are the only ones we want to support
#	if __clang_major__ > 6
#		define WARN_PUSH_INTEL(id)
#		define WARN_PUSH_MSVC(id)
#		define WARN_PUSH_CLANG(id)							DO_PRAGMA(clang diagnostic push);			\
															DO_PRAGMA(clang diagnostic ignored id)
#		define WARN_PUSH_GCC(id)
#		define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(clang diagnostic push);			\
															DO_PRAGMA(clang diagnostic ignored clang_id)
//---
#		define WARN_OFF_INTEL(id)
#		define WARN_OFF_MSVC(id)
#		define WARN_OFF_CLANG(id)							DO_PRAGMA(clang diagnostic ignored id)
#		define WARN_OFF_GCC(id)
#		define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(clang diagnostic ignored clang_id)
//---
#		define WARN_POP										DO_PRAGMA(clang diagnostic pop)
#		define WARN_POP_INTEL
#		define WARN_POP_MSVC
#		define WARN_POP_CLANG								DO_PRAGMA(clang diagnostic pop)
#		define WARN_POP_GCC
#	else
#		define WARN_PUSH_INTEL(id)
#		define WARN_PUSH_MSVC(id)
#		define WARN_PUSH_CLANG(id)
#		define WARN_PUSH_GCC(id)
#		define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)
//---
#		define WARN_OFF_INTEL(id)
#		define WARN_OFF_MSVC(id)
#		define WARN_OFF_CLANG(id)
#		define WARN_OFF_GCC(id)
#		define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)
//---
#		define WARN_POP
#		define WARN_POP_INTEL
#		define WARN_POP_MSVC
#		define WARN_POP_CLANG
#		define WARN_POP_GCC
#	endif

//----------------------------------------------------------------------
#elif defined(__GNUC__)

#	define WARN_PUSH_INTEL(id)
#	define WARN_PUSH_MSVC(id)
#	define WARN_PUSH_CLANG(id)
#	define WARN_PUSH_GCC(id)							DO_PRAGMA(GCC diagnostic push);				\
														DO_PRAGMA(GCC diagnostic ignored id)
#	define WARN_PUSH(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(GCC diagnostic push);				\
														DO_PRAGMA(GCC diagnostic ignored gcc_id)
//---
#	define WARN_OFF_INTEL(id)
#	define WARN_OFF_MSVC(id)
#	define WARN_OFF_CLANG(id)
#	define WARN_OFF_GCC(id)								DO_PRAGMA(GCC diagnostic ignored id)
#	define WARN_OFF(msvc_id,gcc_id,clang_id,intel_id)	DO_PRAGMA(GCC diagnostic ignored gcc_id)
//---
#	define WARN_POP										DO_PRAGMA(GCC diagnostic pop)
#	define WARN_POP_INTEL
#	define WARN_POP_MSVC
#	define WARN_POP_CLANG
#	define WARN_POP_GCC									DO_PRAGMA(GCC diagnostic pop)

//----------------------------------------------------------------------
#else
#error	"Trying to turn warnings off on an unsupported platform"
#endif

// ==================================================================
// Copyright 2016 Autodesk, Inc.  All rights reserved.
// 
// This computer source code  and related  instructions and comments are
// the unpublished confidential and proprietary information of Autodesk,
// Inc. and are  protected  under applicable  copyright and trade secret
// law. They may not  be disclosed to, copied or used by any third party
// without the prior written consent of Autodesk, Inc.
// ==================================================================

#endif	// Hwarnings_h
