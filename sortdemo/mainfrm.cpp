// mainfrm.cpp : implementation of the CMainFrame class
//
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been
// included.  You may also use portions of the sample code in your
// own programs, as desired.

#include "stdafx.h"
#include "sortdemo.h"
#include "mainview.h"
#include "sortproc.h"

#include "mainfrm.h"

#ifdef _DEBUG
#ifdef THIS_FILE
#undef THIS_FILE
#endif // THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


extern "C" const int wxID_MAINFRAME=(wxID_HIGHEST + 1001);
extern "C" const int wxID_STATUSBAR=(wxID_HIGHEST + 1002);

extern "C" const int wxEVT_PAINTLINE=(wxEVT_USER_FIRST + 1002);
extern "C" const int wxEVT_SORT_COMPLETE=(wxEVT_USER_FIRST + 1003);

extern "C" const int IDM_SORT_NEWDATA = XRCID("IDM_SORT_NEWDATA");
extern "C" const int IDM_SORT_RESTORE = XRCID("IDM_SORT_RESTORE");
extern "C" const int IDM_SORT_INSERTION = XRCID("IDM_SORT_INSERTION");
extern "C" const int IDM_SORT_EXCHANGE = XRCID("IDM_SORT_EXCHANGE");
extern "C" const int IDM_SORT_BUBBLE = XRCID("IDM_SORT_BUBBLE");
extern "C" const int IDM_SORT_SHELL = XRCID("IDM_SORT_SHELL");
extern "C" const int IDM_SORT_HEAP = XRCID("IDM_SORT_HEAP");
extern "C" const int IDM_SORT_HEAP2 = XRCID("IDM_SORT_HEAP2");
extern "C" const int IDM_SORT_QUICK = XRCID("IDM_SORT_QUICK");
extern "C" const int IDM_SORT_THREAD_QUICK = XRCID("IDM_SORT_THREAD_QUICK");
extern "C" const int IDM_SORT_KILL = XRCID("IDM_SORT_KILL");
extern "C" const int IDM_SORT_EXIT = XRCID("IDM_SORT_EXIT");
extern "C" const int ID_VIEW_TOOLBAR = XRCID("ID_VIEW_TOOLBAR");
extern "C" const int ID_VIEW_STATUS_BAR = XRCID("ID_VIEW_STATUS_BAR");


// There is a bug in the GTK implementation of 'wxUpdateUIEvent'
// that prevents the toolbar buttons from being updated properly.
// call this macro AFTER the 'wxUpdateUIEvent::Enable()' call,
// passing the reference to the 'wxUpdateUIEvent' as 'X'

#ifdef WIN32
#define CMDUI_MANAGE_TOOLBAR(X)
#else
#define CMDUI_MANAGE_TOOLBAR(X) \
 { wxToolBar *pTB = GetToolBar(); \
   if(pTB) pTB->EnableTool(X.GetId(), X.GetEnabled()); }
#endif // WIN32


/////////////////////////////////////////////////////////////////////////////
// CMainFrame

BEGIN_EVENT_TABLE(CMainFrame, wxDocParentFrame)

  EVT_COMMAND(wxID_MAINFRAME,wxEVT_PAINTLINE,CMainFrame::OnPaintLine)
  EVT_COMMAND(wxID_MAINFRAME,wxEVT_SORT_COMPLETE,CMainFrame::OnSortComplete)

  EVT_PAINT(CMainFrame::OnPaint)

  EVT_MENU(IDM_SORT_NEWDATA, CMainFrame::OnSortNewdata)
  EVT_UPDATE_UI(IDM_SORT_NEWDATA, CMainFrame::OnUpdateSortNewdata)
  EVT_MENU(IDM_SORT_RESTORE, CMainFrame::OnSortRestore)
  EVT_UPDATE_UI(IDM_SORT_RESTORE, CMainFrame::OnUpdateSortRestore)

  EVT_MENU(IDM_SORT_INSERTION, CMainFrame::OnSortInsertion)
  EVT_UPDATE_UI(IDM_SORT_INSERTION, CMainFrame::OnUpdateSortInsertion)
  EVT_MENU(IDM_SORT_EXCHANGE, CMainFrame::OnSortExchange)
  EVT_UPDATE_UI(IDM_SORT_EXCHANGE, CMainFrame::OnUpdateSortExchange)
  EVT_MENU(IDM_SORT_BUBBLE, CMainFrame::OnSortBubble)
  EVT_UPDATE_UI(IDM_SORT_BUBBLE, CMainFrame::OnUpdateSortBubble)
  EVT_MENU(IDM_SORT_SHELL, CMainFrame::OnSortShell)
  EVT_UPDATE_UI(IDM_SORT_SHELL, CMainFrame::OnUpdateSortShell)
  EVT_MENU(IDM_SORT_HEAP, CMainFrame::OnSortHeap)
  EVT_UPDATE_UI(IDM_SORT_HEAP, CMainFrame::OnUpdateSortHeap)
  EVT_MENU(IDM_SORT_HEAP2, CMainFrame::OnSortHeap2)
  EVT_UPDATE_UI(IDM_SORT_HEAP2, CMainFrame::OnUpdateSortHeap2)
  EVT_MENU(IDM_SORT_QUICK, CMainFrame::OnSortQuick)
  EVT_UPDATE_UI(IDM_SORT_QUICK, CMainFrame::OnUpdateSortQuick)
  EVT_MENU(IDM_SORT_THREAD_QUICK, CMainFrame::OnSortThreadQuick)
  EVT_UPDATE_UI(IDM_SORT_THREAD_QUICK, CMainFrame::OnUpdateSortThreadQuick)

  EVT_MENU(IDM_SORT_KILL, CMainFrame::OnSortKill)
  EVT_UPDATE_UI(IDM_SORT_KILL, CMainFrame::OnUpdateSortKill)

  EVT_MENU(IDM_SORT_EXIT, CMainFrame::OnSortExit)

  EVT_MENU(ID_VIEW_TOOLBAR, CMainFrame::OnViewToolbar)
  EVT_UPDATE_UI(ID_VIEW_TOOLBAR, CMainFrame::OnUpdateViewToolbar)

  EVT_MENU(ID_VIEW_STATUS_BAR, CMainFrame::OnViewStatusBar)
  EVT_UPDATE_UI(ID_VIEW_STATUS_BAR, CMainFrame::OnUpdateViewStatusBar)

END_EVENT_TABLE()


/////////////////////////////////////////////////////////////////////////////
// arrays of IDs used to initialize control bars


/////////////////////////////////////////////////////////////////////////////
// CMainFrame construction/destruction

CMainFrame::CMainFrame(wxDocManager* manager, wxFrame *parent, wxWindowID id,
                       const wxString& title, const wxPoint& pos /*= wxDefaultPosition*/,
                       const wxSize& size /*= wxDefaultSize*/, long style /*= wxDEFAULT_FRAME_STYLE*/,
                       const wxString& name /*= "frame"*/)
: wxDocParentFrame(manager, parent, (id == -1 ? wxID_MAINFRAME : id),
                   title, pos, size, style, name)
{
   m_bEnable = TRUE;  // important - enables sort menu & button items
   m_pDocManager = manager;
   m_pDoc = NULL;
   m_pView = NULL;

   SetIcon(wxXmlResource::Get()->LoadIcon(__T("ICON_Main")));

   SetMenuBar(wxXmlResource::Get()->LoadMenuBar(__T("MENUBAR_Main")));
   SetToolBar(wxXmlResource::Get()->LoadToolBar(this, __T("TOOLBAR_Main")));
   CreateStatusBar(1,0,wxID_STATUSBAR);
}

CMainFrame::~CMainFrame()
{
//  delete m_pView;
//  delete m_pDoc;
}

void CMainFrame::DoOnCreate(wxDocTemplate *pTempl)
{
  m_pDoc = (CMainDoc *)pTempl->CreateDocument(__T(""));
  m_pView = (CMainView *)pTempl->CreateView(m_pDoc);


  m_pView->SetFrame(this);

  return;
}

wxDocument *CMainFrame::GetDocument()
{
  wxView *pV = GetView();

  if(pV)
    return(pV->GetDocument());

  return(NULL);
}

wxView *CMainFrame::GetView()
{
  // figure out which view has the current focus

  if(!m_pDocManager)
    return(NULL);

  wxView *pRval = m_pDocManager->GetCurrentView();

  if(pRval && pRval->GetFrame() == this)
    return(pRval);

  // go through list of views, find the one that matches me

  wxNode *pN = m_pDocManager->GetDocuments().GetFirst();

  while(pN)
  {
    wxDocument *pDoc = (wxDocument *)pN->GetData();

    if(pDoc)
    {
      wxNode *pN2 = pDoc->GetViews().GetFirst();

      while(pN2)
      {
        wxView *pV = (wxView *)pN2->GetData();

        if(pV->GetFrame() == this)
          return(pV);

        pN2 = pN2->GetNext();
      }
    }

    pN = pN->GetNext();
  }

  return(NULL);  // none found
}

void CMainFrame::RePaintLine(UINT uiIndex)
{
  // post a message to re-paint the line.  This should
  // make everthing work ok.  Since one request to paint
  // happens after each line is swapped, so long as no
  // events are lost, this will always work.

  // TODO:  if this becomes a problem, pending events could
  //        be consolidated into a single event using a queue
  //        such that a single received command event empties
  //        the queue and processes all pending requests

  wxCommandEvent evt(wxEVT_PAINTLINE, wxID_MAINFRAME);
  evt.SetInt(uiIndex);

  AddPendingEvent(evt);
  wxWakeUpIdle();
}

void CMainFrame::OnPaintLine(wxCommandEvent &evt)
{
  if(m_pView)
    m_pView->RePaintLine(evt.GetInt(), NULL);
}



/////////////////////////////////////////////////////////////////////////////
// CMainFrame message handlers

void CMainFrame::OnPaint(wxPaintEvent& event)
{
  wxView *pV = GetView();
  if(pV && pV->IsKindOf(CLASSINFO(CMainView)))
    ((CMainView *)pV)->OnPaint(event);
  else
  {
    wxPaintDC dc(this);
    // do nothing, this cleans it up right
  }
}

void CMainFrame::OnSortExit(wxCommandEvent &evt)
{
//  wxCloseEvent evtClose(0,wxID_MAINFRAME);
//  AddPendingEvent(evtClose);

  Close(false);
}


unsigned long dwStartTime;
wxChar szSortInfo[4096];

void SortInfo(const wxChar *szSortName)
{
   wxSprintf(szSortInfo, __T("Sort: %s\n%d swaps, %d compares,\n%d milliseconds"),
             szSortName, iSwaps, iCompares,
             (MyGetTickCount() - dwStartTime));

   // called outside of main thread - on sort thread exit, a dialog
   // box will display this information.
}

void SortInfoT(const wxChar *szSortName)
{
   wxSprintf(szSortInfo, __T("Sort: %s\n%d swaps, %d compares,\n%d sched, %d milliseconds"),
             szSortName, iSwaps, iCompares, iSchedWork,
             (MyGetTickCount() - dwStartTime));

   // called outside of main thread - on sort thread exit, a dialog
   // box will display this information.
}

void CMainFrame::EnableSortMenuItems(BOOL bEnable)
{
   m_bEnable = bEnable;
   wxWakeUpIdle();
}

unsigned long SortThread(void * lpvParms)
{
   dwStartTime = MyGetTickCount();
   *szSortInfo = 0;

   switch((UINT)(unsigned long)lpvParms)
   {
      case ID_SORT_HEAP:
         HeapSort();
         SortInfo(__T("Heap Sort (1)"));
         break;

      case ID_SORT_HEAP2:
         HeapSort2();
         SortInfo(__T("Heap Sort (2)"));
         break;

      case ID_SORT_QUICK:
         QuickSort();
         SortInfo(__T("Quick Sort"));
         break;

      case ID_SORT_THREAD_QUICK:
         ThreadQuickSort();
         SortInfoT(__T("Thread Quick Sort"));
         break;

      case ID_SORT_BUBBLE:
         BubbleSort();
         SortInfo(__T("Bubble Sort"));
         break;

      case ID_SORT_SHELL:
         ShellSort();
         SortInfo(__T("Shell Sort"));
         break;

      case ID_SORT_INSERTION:
         InsertionSort();
         SortInfo(__T("Insertion Sort"));
         break;

      case ID_SORT_EXCHANGE:
         ExchangeSort();
         SortInfo(__T("Exchange Sort"));
         break;
   }

   theApp.m_SortMutex.Lock();

   theApp.m_pSortThread = NULL;  // make sure

   if(theApp.m_pMainWnd)
   {
     wxCommandEvent evt(wxEVT_SORT_COMPLETE, wxID_MAINFRAME);
     ((CMainFrame *)theApp.m_pMainWnd)->DoAddPendingEvent(evt);
   }

   theApp.m_SortMutex.Unlock();

   return(0);
}

void CMainFrame::CreateSortThread(UINT uiParms)
{
   // temporarily disable menu (and buttons)
   EnableSortMenuItems(0);

   theApp.m_pSortThread = wxBeginThread(SortThread, (void *)(intptr_t)uiParms);

   if(!theApp.m_pSortThread)
   {
      wxMessageBox(__T("?Unable to spawn thread for sort"), szAppName, wxOK | wxICON_HAND);
      EnableSortMenuItems(1);
   }
}

void CMainFrame::OnSortComplete(wxCommandEvent &evt)
{
   EnableSortMenuItems(1);
   theApp.m_pSortThread = NULL;  // make sure

   wxMessageBox(szSortInfo, __T("* SORT RESULTS *"), wxOK | wxICON_INFORMATION);
}

void CMainFrame::OnSortKill(wxCommandEvent &evt)
{
   Enable(0);  // disables the window

   theApp.m_SortMutex.Lock();

   if(theApp.m_pSortThread)
   {
     wxThread *pT = theApp.m_pSortThread;  // make a copy of it
     pT->Pause();                   // pause it first

     theApp.m_pSortThread = NULL;   // assign my copy of the object to NULL

     theApp.m_SortMutex.Unlock();   // now  unlock the mutex
     pT->Delete();                  // and end it, gracefully
                                    // 'Delete()' won't return until it's gone
   }
   else
     theApp.m_SortMutex.Unlock();


   Enable(1);  // re-enable the window (I am done)
}

void CMainFrame::OnUpdateSortKill(wxUpdateUIEvent &cmdUI)
{
  cmdUI.Enable(theApp.m_pSortThread != NULL);

  CMDUI_MANAGE_TOOLBAR(cmdUI);
}


// SORT MENU FUNCTIONS

void CMainFrame::OnSortHeap(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_HEAP);
}

void CMainFrame::OnSortHeap2(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_HEAP2);
}

void CMainFrame::OnSortInsertion(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_INSERTION);
}

void CMainFrame::OnSortQuick(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_QUICK);
}

void CMainFrame::OnSortThreadQuick(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_THREAD_QUICK);
}

void CMainFrame::OnSortShell(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_SHELL);
}

void CMainFrame::OnSortExchange(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_EXCHANGE);
}

void CMainFrame::OnSortBubble(wxCommandEvent &evt)
{
   if(!theApp.m_pSortThread)
     CreateSortThread(ID_SORT_BUBBLE);
}



void CMainFrame::OnUpdateSortBubble(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortExchange(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortHeap(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortHeap2(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortInsertion(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortQuick(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortThreadQuick(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnUpdateSortShell(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnSortNewdata(wxCommandEvent &evt)
{
  if(m_bEnable && !theApp.m_pSortThread)
    theApp.DoSortNewdata();
}

void CMainFrame::OnUpdateSortNewdata(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnSortRestore(wxCommandEvent &evt)
{
  if(m_bEnable && !theApp.m_pSortThread)
    theApp.DoSortRestore();
}

void CMainFrame::OnUpdateSortRestore(wxUpdateUIEvent &cmdUI)
{
   cmdUI.Enable(m_bEnable);

   CMDUI_MANAGE_TOOLBAR(cmdUI);
}

void CMainFrame::OnViewToolbar(wxCommandEvent &evt)
{
  wxToolBar *pTB = GetToolBar();
  if(pTB != NULL)
  {
    SetToolBar(NULL);
    pTB->Destroy();
  }
  else
    SetToolBar(wxXmlResource::Get()->LoadToolBar(this, __T("TOOLBAR_Main")));

  Refresh();
}

void CMainFrame::OnUpdateViewToolbar(wxUpdateUIEvent &cmdUI)
{
  cmdUI.Check(GetToolBar() != NULL);
}

void CMainFrame::OnViewStatusBar(wxCommandEvent &evt)
{
  wxStatusBar *pSB = GetStatusBar();
  if(pSB != NULL)
  {
    SetStatusBar(NULL);
    pSB->Destroy();
  }
  else
    CreateStatusBar(1,0,wxID_STATUSBAR);

  Refresh();
}

void CMainFrame::OnUpdateViewStatusBar(wxUpdateUIEvent &cmdUI)
{
  cmdUI.Check(GetStatusBar() != NULL);
}


