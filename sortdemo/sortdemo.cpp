// sortdemo.cpp : Defines the class behaviors for the application.
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been
// included.  You may also use portions of the sample code in your
// own programs, as desired.

// Original conversion from MFC to wxWidgets, January 2005


#include "stdafx.h"
#include "sortdemo.h"
#include "mainview.h"
#include "mainfrm.h"


#ifdef _DEBUG
#ifdef THIS_FILE
#undef THIS_FILE
#endif // THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif

extern void InitXmlResource();  // defined in SortdemoResource.cpp



const TCHAR szAppName[] = __T("Sort Demo");


/////////////////////////////////////////////////////////////////////////////
// CSortdemoApp

BEGIN_EVENT_TABLE(CSortdemoApp, wxApp)
	EVT_MENU(XRCID(("ID_APP_ABOUT")), CSortdemoApp::OnAppAbout)
END_EVENT_TABLE()


/////////////////////////////////////////////////////////////////////////////
// CSortdemoApp construction

CSortdemoApp::CSortdemoApp()
{
  m_pMainWnd = NULL;
  m_pSortThread = NULL;
  m_pDocManager = NULL;
}

CSortdemoApp::~CSortdemoApp()
{
//  if(m_pMainWnd)
//    delete m_pMainWnd;

  if(m_pDocManager)
    delete m_pDocManager;
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CSortdemoApp object

//IMPLEMENT_APP(CSortdemoApp);

CSortdemoApp theApp;

//IMPLEMENT_APP_NO_MAIN(CSortdemoApp);
//IMPLEMENT_WX_THEME_SUPPORT;

int main(int argc, char *argv[])
{
    wxApp::SetInstance(&theApp);

    wxEntryStart( argc, argv );
    wxTheApp->CallOnInit();
    wxTheApp->OnRun();

    return 0;
}


/////////////////////////////////////////////////////////////////////////////
// CSortdemoApp initialization

BOOL CSortdemoApp::OnInit()
{
wxString csCaption;

  wxInitAllImageHandlers();

  wxXmlResource::Get()->InitAllHandlers();
  InitXmlResource();

  wxThread::SetConcurrency(255);  // always

  // must allocate this now
  m_pDocManager = new wxDocManager(wxDEFAULT_DOCMAN_FLAGS, TRUE);

  //// Create a doc template - doc manager created in constructor
  wxDocTemplate *pTempl =  new wxDocTemplate(m_pDocManager, _T("SortDemo"), _T(""), _T(""), _T(""),
                                             _T("SortDemo Doc"), _T("SortDemo View"),
                                             CLASSINFO(CMainDoc), CLASSINFO(CMainView));

  m_pDocManager->SetMaxDocsOpen(1);  // single instance

  m_pMainWnd = (wxWindow *)
    new CMainFrame(m_pDocManager, (wxFrame *) NULL, -1, wxString(_T("Sort Demo")),
                   wxPoint(0, 0), wxSize(500, 400), wxDEFAULT_FRAME_STYLE);

  if(!m_pMainWnd)
    return(FALSE);

  ((CMainFrame *)m_pMainWnd)->DoOnCreate(pTempl);  // make sure this happens

  m_pMainWnd->Centre(wxBOTH);
  m_pMainWnd->Show(1);
  m_pMainWnd->Raise();

  SetTopWindow(m_pMainWnd);

  DoSortNewdata();

  if(argc > 1)
  {
    // TODO: add command line processing here
  }

  return(TRUE);  // load OK!
}

int CSortdemoApp::OnExit()
{
  if(m_pSortThread)
  {
    m_SortMutex.Lock();

    if(m_pSortThread)  // must check it again
    {
      if(m_pSortThread->IsAlive())
        m_pSortThread->Kill();

      m_pSortThread = NULL;
    }

    m_SortMutex.Unlock();
  }

  if(!m_pMainWnd)
    return(1);

//  delete m_pMainWnd;
//  m_pMainWnd = NULL;

  return(0);
}


/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About

class CAboutDlg : public wxDialog
{
public:
  CAboutDlg();

  static const TCHAR m_IDD[];

  wxWindow *GetDlgItem(long lID)  { return(FindWindowById(lID, this)); }
  void SetDlgItemText(long lID, const TCHAR *szString)
  {
    wxWindow *pItem = GetDlgItem(lID);
    if(pItem)
    {
      if(pItem->IsKindOf(CLASSINFO(wxTextCtrl)))
        ((wxTextCtrl *)pItem)->SetValue(szString);
      else if(pItem->IsKindOf(CLASSINFO(wxStaticText)))
        ((wxStaticText *)pItem)->SetLabel(szString);
      else if(pItem->IsKindOf(CLASSINFO(wxButton)))
        ((wxButton *)pItem)->SetLabel(szString);
    }
  }
  wxString GetDlgItemText(long lID)
  {
    wxWindow *pItem = GetDlgItem(lID);
    if(pItem)
      return(pItem->GetLabel());

    return(wxString(__T("")));
  }


// Implementation
protected:
  virtual void Update(BOOL bUpdateFlag);  // instead of DDX/DDV
  virtual void OnOK(wxCommandEvent &evt);
  virtual void OnInitDialog(wxInitDialogEvent &evt);  // this should be a standard

  DECLARE_EVENT_TABLE()
};

const TCHAR CAboutDlg::m_IDD[]=__T("IDD_ABOUTBOX");

CAboutDlg::CAboutDlg() // : CDialog(CAboutDlg::IDD)
{
  wxXmlResource::Get()->LoadDialog(this, theApp.m_pMainWnd, m_IDD);

	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

static const long lAboutBoxID1 = XRCID(("IDD_ABOUTBOX_S2"));

void CAboutDlg::Update(BOOL bUpdateFlag)
{
  // an example implementation of 'Update()'

  if(bUpdateFlag) // controls to data
  {
    TransferDataFromWindow();
  }
  else            // data to controls
  {
    TransferDataToWindow();

    SetDlgItemText(lAboutBoxID1, __T("Copyright (c) 2005-2017 by R. E. Frazier"));
  }
}

void CAboutDlg::OnInitDialog(wxInitDialogEvent &evt)
{
  // TODO:  put things here that I do on dialog box initialization

  Update(0);
  Centre();

//  return(1);  // must call this at the end (default processing)
}

void CAboutDlg::OnOK(wxCommandEvent &evt)
{
  EndModal(1);  // must do this (a standard implementation of OnOK)
//  return(TRUE);
}


BEGIN_EVENT_TABLE(CAboutDlg, wxDialog)
  EVT_INIT_DIALOG(CAboutDlg::OnInitDialog)
  EVT_BUTTON(wxID_OK, CAboutDlg::OnOK)
END_EVENT_TABLE()

// App command to run the dialog

void CSortdemoApp::OnAppAbout(wxCommandEvent &evt)
{
  CAboutDlg aboutDlg;
  aboutDlg.ShowModal();
}



/////////////////////////////////////////////////////////////////////////////
// CSortdemoApp commands

int irand(int iMax)
{
  return((int)((double)rand() * iMax / RAND_MAX));
}

MY_RGB_INFO GetLineColor(UINT uiIndex)
{
double d5PIover3= 5 * 3.1415926538 / 3;
double dRoot3 = sqrt(3.0);
double dX, dY, dSinX, dCosX, dR, dG, dB;

   // NOTE:  5 * PI / 3 corresponds to VIOLET to give me a rainbow

   dX = (d5PIover3 * pData[uiIndex]) / N_DIMENSIONS(pData);

   dSinX = sin(dX);
   dCosX = cos(dX);

   if(dSinX == 0 && dCosX >= 0)
   {
      dR = 1;
      dG = 0;
      dB = 0;
   }
   else if(dSinX > 0 && dCosX == -.5)
   {
      dR = 0;
      dG = 1;
      dB = 0;
   }
   else if(dSinX < 0 && dCosX == -.5)
   {
      dR = 0;
      dG = 0;
      dB = 1;
   }
   else if(dSinX > 0 && dCosX > -.5) // RG
   {
      // vector sum of R and G as follows:
      // R sin(r) + G sin(g) = X sin(x)
      // R cos(r) + G cos(g) = X cos(x)
      //
      // ASSUME 'X' = 1
      //
      // R * 0 + G * sqrt(3)/2 = dSinX
      // R * 1 + G * -.5 = dCosX

      dB = 0;
      dG = dSinX * 2 / dRoot3;
      dR = dCosX + dG * .5;
   }
   else if(dSinX < 0 && dCosX > -.5)  // RB
   {
      // vector sum of R and G as follows:
      // R sin(r) + B sin(b) = X sin(x)
      // R cos(r) + B cos(b) = X cos(x)
      //
      // ASSUME 'X' = 1
      //
      // R * 0 + B * -sqrt(3)/2 = dSinX
      // R * 1 + B * -.5 = dCosX

      dG = 0;
      dB = - dSinX * 2 / dRoot3;
      dR = dCosX + dB * .5;
   }
   else                                // GB
   {
      // vector sum of R and G as follows:
      // G sin(g) + B sin(b) = X sin(x)
      // G cos(g) + B cos(b) = X cos(x)
      //
      // ASSUME 'X' = 1
      //
      // G * sqrt(3)/2 + B * -sqrt(3)/2 = dSinX
      // G * -.5 + B * -.5 = dCosX
      //
      // G = dSinX * 2 / sqrt(3) + B
      //
      // G + B = -2dCosX
      // B = -(2 * dCosX + G)
      //
      // B = -(2 * dCosX + dSinX * 2 / sqrt(3) + B)
      // 2B = -2 * dCosX - dSinX * 2 / sqrt(3)
      // B = -dCosX - dSinX / sqrt(3)

      dR = 0;
      dB = -dCosX - dSinX / dRoot3;
      dG = dSinX * 2 / dRoot3 + dB;
   }

   dY = 255 / sqrt(dB * dB + dG * dG + dR * dR);

   return(MY_RGB_INFO((int)(dY * dR),(int)(dY * dG),(int)(dY * dB)));
}


static int pData0[N_DIMENSIONS(pData)];
static MY_RGB_INFO pColor0[N_DIMENSIONS(pData)];

void CSortdemoApp::DoSortNewdata()
{
UINT ui1;

   srand(MyGetTickCount());

   for(ui1=0; ui1<N_DIMENSIONS(pData); ui1++)
   {
     pData[ui1] = irand(N_DIMENSIONS(pData));
     pData0[ui1] = pData[ui1];

     pColor[ui1] = GetLineColor(ui1);
     pColor0[ui1] = pColor[ui1];
   }

   ((CMainFrame *)m_pMainWnd)->Refresh(TRUE, NULL);
}

void CSortdemoApp::DoSortRestore()
{
UINT ui1;

   for(ui1=0; ui1<N_DIMENSIONS(pData); ui1++)
   {
     pData[ui1] = pData0[ui1];
     pColor[ui1] = pColor0[ui1];
   }

   ((CMainFrame *)m_pMainWnd)->Refresh(TRUE, NULL);
}


// COMMON UTILITIES

unsigned long MyGetTickCount()
{
  struct timeb tb;
  ftime(&tb);

  return((unsigned long)tb.time * 1000L + (unsigned long)tb.millitm);
}

class CMyInternalThreadClass : public wxMyThread
{
DECLARE_DYNAMIC_CLASS(CMyInternalThreadClass)
public:
  CMyInternalThreadClass() { m_pParam = NULL; m_pThreadProc = NULL; };
  ~CMyInternalThreadClass() { };

  virtual wxThread::ExitCode Entry();
  unsigned long (*m_pThreadProc)(void *);
  void *m_pParam;
};

IMPLEMENT_ABSTRACT_CLASS(wxMyThread, wxObject);
IMPLEMENT_DYNAMIC_CLASS(CMyInternalThreadClass,wxMyThread);

wxThread::ExitCode CMyInternalThreadClass::Entry()
{
  if(!m_pThreadProc)
    return((wxThread::ExitCode)NULL);


  return((wxThread::ExitCode)m_pThreadProc(m_pParam));
}

wxMyThread *wxBeginThread(wxClassInfo *pThreadClass,
                          int nPriority /*= WXTHREAD_DEFAULT_PRIORITY*/,
                          unsigned int nStackSize /*= 0*/,
                          unsigned long dwCreateFlags /* = 0*/,
                          void *pSecurity /*= NULL*/)
{
  if(!pThreadClass || !pThreadClass->IsKindOf(CLASSINFO(wxMyThread)))
    return(NULL);

  wxMyThread *pThread = (wxMyThread *)pThreadClass->CreateObject();
  if(pThread)
  {
    if(pThread->Create(nStackSize) != wxTHREAD_NO_ERROR)
    {
      delete pThread;
      return(NULL);
    }

    pThread->SetPriority(nPriority);

    if(!(dwCreateFlags & WXBEGINTHREAD_SUSPENDED))
      pThread->Run();
  }

  return(pThread);
}

wxMyThread *wxBeginThread(unsigned long (*pThreadProc)(void *), void *pParam,
                          int nPriority /*= WXTHREAD_DEFAULT_PRIORITY*/,
                          unsigned int nStackSize /*= 0*/,
                          unsigned long dwCreateFlags /* = 0*/,
                          void *pSecurity /*= NULL*/)
{
  CMyInternalThreadClass *pThread = (CMyInternalThreadClass *)
    wxBeginThread(CLASSINFO(CMyInternalThreadClass), nPriority, nStackSize,
                  dwCreateFlags | WXBEGINTHREAD_SUSPENDED, pSecurity);

  if(pThread)
  {
    pThread->m_pThreadProc = pThreadProc;
    pThread->m_pParam = pParam;

    if(!(dwCreateFlags & WXBEGINTHREAD_SUSPENDED))
      pThread->Run();
  }

  return(pThread);
}


#ifdef WIN32
// one of the Win32 API calls that I can't do without...
extern "C" void __stdcall Sleep(unsigned long dwMilliseconds);
#endif // WIN32

extern "C" void MySleep(unsigned long lMilliSeconds)
{
  wxWakeUpIdle();  // force idle processing to 'wake up'

#ifdef WIN32
  Sleep(lMilliSeconds);
#else // WIN32
  wxThread *pT = wxThread::This();
  if(pT)
  {
    unsigned long lStartTick = MyGetTickCount();
    do
    {
      wxWakeUpIdle();  // do this again (just because)
      pT->Sleep(5);    // a fairly efficient way of doing it
    } while((MyGetTickCount() - lStartTick) < lMilliSeconds);
  }
  else
    usleep(lMilliSeconds * 1000L);
#endif // WIN32

  wxWakeUpIdle();  // do this again (just because)
}

