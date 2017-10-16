// sortdemo.h : main header file for the SORTDEMO application
//
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been
// included.  You may also use portions of the sample code in your
// own programs, as desired.

#ifndef BOOL
#define BOOL bool
#endif // BOOL

#ifndef afx_msg
#define afx_msg
#endif // afx_msg

#ifndef UINT
#define UINT unsigned int
#endif // UINT

#ifndef LPCSTR
#define LPCSTR const char *
#endif // LPCSTR

#ifndef LPSTR
#define LPSTR char *
#endif // LPSTR

#if wxUSE_WCHAR_T /* defined as non-zero value when using wide char */
#define TCHAR wchar_t
#define __T(X) (L##X)
#else // single-byte characters, aka ASCII
#define TCHAR char
#define __T(X) (X)
#endif // wxUSE_WCHAR_T


/////////////////////////////////////////////////////////////////////////////
// CSortdemoApp:
// See sortdemo.cpp for the implementation of this class
//

class CSortdemoApp : public wxApp
{
public:
   CSortdemoApp();
   virtual ~CSortdemoApp();

   // DOC/VIEW helper stuff
   wxDocManager *m_pDocManager;  // yep, like MFC



   wxWindow *m_pMainWnd;  // must add this for 'wxWidgets' version

   wxThread *m_pSortThread; // sort thread object to terminate
                            // on application closure

   wxMutex m_SortMutex;  // use this instead

   virtual bool OnInit();
   virtual int OnExit();

// Implementation

  void OnAppAbout(wxCommandEvent &evt);
  void DoSortNewdata();
  void DoSortRestore();

  DECLARE_EVENT_TABLE()
};

extern const wxChar szAppName[];

unsigned long MyGetTickCount();

#define N_DATA_DIMENSIONS 256

enum WXBEGINTHREAD
{
  WXBEGINTHREAD_DEFAULT = 0,
  WXBEGINTHREAD_SUSPENDED = 1  // for now this is the only flag
};

// this next class allows anyone to call the 'TestDestroy()' function
// by casting a 'wxThread *' to 'wxWhoreThread *'.
class wxWhoreThread : public wxThread
{
public:
  bool TestDestroy() { return(wxThread::TestDestroy()); }
};

class wxMyThread : public wxThread, public wxObject
{
DECLARE_ABSTRACT_CLASS(wxMyThread)
protected:  // you must create this class dynamically, and derive from it
  wxMyThread() : wxThread() { }
  ~wxMyThread() { }

  friend wxMyThread *wxBeginThread(unsigned long (*)(void *), void *, int, unsigned int, unsigned lnog, void *);
  friend wxMyThread *wxBeginThread(wxClassInfo *, int, unsigned int, unsigned long, void *);

public:

  // use 'SafeTestDestroy()' within any loop

  static BOOL SafeTestDestroy()
  {
    // use of 'wxWhoreThread' is necessary, because gcc will 'gripe'
    // otherwise.  This sounds more like a BUG, to me...

    wxWhoreThread *pThread = (wxWhoreThread *)wxThread::This();

    if(pThread)
      return(pThread->TestDestroy());

    return(FALSE);
  }
};

wxMyThread *wxBeginThread(unsigned long (*)(void *), void *,
                          int nPriority = WXTHREAD_DEFAULT_PRIORITY,
                          unsigned int nStackSize = 0,
                          unsigned long dwCreateFlags = 0,
                          void *pSecurity = NULL);
wxMyThread *wxBeginThread(wxClassInfo *pThreadClass,
                          int nPriority = WXTHREAD_DEFAULT_PRIORITY,
                          unsigned int nStackSize = 0,
                          unsigned long dwCreateFlags = 0,
                          void *pSecurity = NULL);


//#define theApp (wxGetApp())  /* for MFC compatibility */
//DECLARE_APP(CSortdemoApp);
extern CSortdemoApp theApp;

extern "C" void MySleep(unsigned long lMilliSeconds);


/////////////////////////////////////////////////////////////////////////////

