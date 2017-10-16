// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been
// included.  You may also use portions of the sample code in your
// own programs, as desired.

//#include <afxwin.h>         // MFC core and standard components
//#include <afxext.h>         // MFC extensions

#define _ALL_SOURCE /* for interix, mostly */

#include <wx/defs.h>
#include <wx/app.h>
#include <wx/frame.h>
#include <wx/docview.h>
#include <wx/dc.h>
#include <wx/colour.h>
#include <wx/image.h>
#include <wx/statusbr.h>
#include <wx/toolbar.h>
#include <wx/thread.h>
#include <wx/event.h>
#include <wx/msgdlg.h>
#include <wx/thread.h>
#include <wx/list.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <wx/dcclient.h>

#include <wx/xrc/xmlres.h> // XML Resources

#include <sys/timeb.h>


#if !wxUSE_DOC_VIEW_ARCHITECTURE
#error You must set wxUSE_DOC_VIEW_ARCHITECTURE to 1 in setup.h!
#endif


#ifndef BASED_CODE
#define BASED_CODE
#endif // BASED_CODE


#ifndef RC_INVOKED

#include "stdlib.h"
#include "math.h"

#ifndef WIN32
#include <unistd.h>
#endif // WIN32


// common macros
#define N_DIMENSIONS(X) (sizeof(X)/sizeof(*(X)))

#endif // RC_INVOKED


