// mainfrm.h : interface of the CMainFrame class
//
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been
// included.  You may also use portions of the sample code in your
// own programs, as desired.
/////////////////////////////////////////////////////////////////////////////

// events must be 'const int', might as well do the same with ID's
extern "C" const int wxID_MAINFRAME;
extern "C" const int wxID_STATUSBAR;

extern "C" const int wxEVT_PAINTLINE;
extern "C" const int wxEVT_SORT_COMPLETE;

extern "C" const int IDM_SORT_NEWDATA;
extern "C" const int IDM_SORT_RESTORE;
extern "C" const int IDM_SORT_INSERTION;
extern "C" const int IDM_SORT_EXCHANGE;
extern "C" const int IDM_SORT_BUBBLE;
extern "C" const int IDM_SORT_SHELL;
extern "C" const int IDM_SORT_HEAP;
extern "C" const int IDM_SORT_HEAP2;
extern "C" const int IDM_SORT_QUICK;
extern "C" const int IDM_SORT_THREAD_QUICK;
extern "C" const int IDM_SORT_KILL;
extern "C" const int IDM_SORT_EXIT;
extern "C" const int ID_VIEW_TOOLBAR;
extern "C" const int ID_VIEW_STATUS_BAR;



class CMainDoc;
class CMainView;

class CMainFrame : public wxDocParentFrame
{
// Attributes
protected:
//	CSplitterWnd m_wndSplitter;
public:

// Operations
public:

// Overrides
	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CMainFrame)
	//}}AFX_VIRTUAL

// Implementation
public:
   CMainFrame(wxDocManager* manager, wxFrame *parent, wxWindowID id, const wxString& title,
              const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
              long style = wxDEFAULT_FRAME_STYLE, const wxString& name = __T("frame"));

   virtual ~CMainFrame();

   void CreateSortThread(UINT uiParms);
   void EnableSortMenuItems(BOOL bEnableFlag);

   wxDocument *GetDocument();
   wxView *GetView();

   void RePaintLine(UINT uiIndex);
   void OnPaintLine(wxCommandEvent &evt);

   void DoOnCreate(wxDocTemplate *); // was 'OnCreate()' message handler, explicitly called now after I create it

   void DoAddPendingEvent(const wxEvent& event) { AddPendingEvent(event); }

protected:  // control bar embedded members
   CMainDoc     *m_pDoc;
   CMainView    *m_pView;
   BOOL         m_bEnable;

   wxDocManager *m_pDocManager;  // a copy of the doc manager pointer, from constructor


// Generated message map functions
protected:
  void OnPaint(wxPaintEvent& event);
  void OnSortComplete(wxCommandEvent &evt);

  void OnSortInsertion(wxCommandEvent &evt);
  void OnSortExchange(wxCommandEvent &evt);
  void OnSortBubble(wxCommandEvent &evt);
  void OnSortShell(wxCommandEvent &evt);
  void OnSortHeap(wxCommandEvent &evt);
  void OnSortHeap2(wxCommandEvent &evt);
  void OnSortQuick(wxCommandEvent &evt);
  void OnSortThreadQuick(wxCommandEvent &evt);

  void OnUpdateSortBubble(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortHeap(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortHeap2(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortInsertion(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortExchange(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortQuick(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortThreadQuick(wxUpdateUIEvent &cmdUI);
  void OnUpdateSortShell(wxUpdateUIEvent &cmdUI);

  void OnSortNewdata(wxCommandEvent &evt);
  void OnUpdateSortNewdata(wxUpdateUIEvent &cmdUI);
  void OnSortRestore(wxCommandEvent &evt);
  void OnUpdateSortRestore(wxUpdateUIEvent &cmdUI);

  void OnSortKill(wxCommandEvent &evt);
  void OnUpdateSortKill(wxUpdateUIEvent &cmdUI);

  void OnSortExit(wxCommandEvent &evt);

  void OnViewToolbar(wxCommandEvent &evt);
  void OnUpdateViewToolbar(wxUpdateUIEvent &cmdUI);

  void OnViewStatusBar(wxCommandEvent &evt);
  void OnUpdateViewStatusBar(wxUpdateUIEvent &cmdUI);

  DECLARE_EVENT_TABLE()


  friend class CSortdemoApp;
  friend void FlashBar(int iBar);
  friend void FlashBar2(int iBar1, int iBar2);
  friend void DrawBar(int iBar);
  friend void SwapBars(int iBar1, int iBar2);

};

enum
{
  ID_SORT_EXCHANGE = 1,
  ID_SORT_SHELL,
  ID_SORT_BUBBLE,
  ID_SORT_INSERTION,
  ID_SORT_HEAP,
  ID_SORT_QUICK,
  ID_SORT_THREAD_QUICK,
  ID_SORT_EXIT,
  ID_SORT_RESTORE,
  ID_SORT_KILL,
  ID_SORT_HEAP2
};


/////////////////////////////////////////////////////////////////////////////

