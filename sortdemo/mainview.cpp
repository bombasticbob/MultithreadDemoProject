// mainview.cpp : implementation file
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
#include "mainfrm.h"
#include "mainview.h"

#ifdef _DEBUG
#ifdef THIS_FILE
#undef THIS_FILE
#endif // THIS_FILE
static char BASED_CODE THIS_FILE[] = __FILE__;
#endif


// STATIC DATA GOES HERE


int pData[N_DATA_DIMENSIONS]={0};
MY_RGB_INFO pColor[N_DATA_DIMENSIONS];


IMPLEMENT_DYNAMIC_CLASS(CMainDoc, wxDocument)

/////////////////////////////////////////////////////////////////////////////
// CMainView

IMPLEMENT_DYNAMIC_CLASS(CMainView, wxView)

CMainView::CMainView()
{
  memset(pData, 0, sizeof(pData));
  memset(pColor, 0, sizeof(pColor));
}

CMainView::~CMainView()
{
}


//BEGIN_MESSAGE_MAP(CMainView, CView)
//	//{{AFX_MSG_MAP(CMainView)
//		// NOTE - the ClassWizard will add and remove mapping macros here.
//	//}}AFX_MSG_MAP
//END_MESSAGE_MAP()

BEGIN_EVENT_TABLE(CMainView, wxView)
END_EVENT_TABLE()


/////////////////////////////////////////////////////////////////////////////
// CMainView drawing

void CMainView::OnPaint(wxPaintEvent& event)
{
  wxPaintDC dc(GetFrame());

  OnDraw(&dc);

#if 0
  // Find Out where the window is scrolled to
  int vbX,vbY;                     // Top left corner of client
  GetViewStart(&vbX,&vbY);

//  int vX,vY,vW,vH;                 // Dimensions of client area in pixels
  wxRegionIterator upd(GetUpdateRegion()); // get the update rect list

  while (upd)
  {
//    vX = upd.GetX();
//    vY = upd.GetY();
//    vW = upd.GetW();
//    vH = upd.GetH();

    wxRect rect;
    upd.GetRect(&rect);

    dc.

    upd ++ ;
  }
#endif // 0
}

void CMainView::OnDraw(wxDC* pDC)
{
UINT ui1;
wxRect rct;
wxPen cpenBlack(MY_RGB_INFO(0,0,0), 1, wxSOLID);

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   rct = GetFrame()->GetClientRect();

//   pDC->BeginDrawing(); this doesn't do anything anyway and is now deprecated

   wxPen cpenOld = pDC->GetPen();
   pDC->SetPen(cpenBlack);
   pDC->DrawRectangle(rct.x, rct.y, rct.width, rct.height);

   for(ui1=0; ui1<N_DIMENSIONS(pData); ui1++)
   {
      RePaintLine(ui1, pDC);
   }

   pDC->SetPen(cpenOld);

//   pDC->EndDrawing(); this doesn't do anything anyway and is now deprecated

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();
}

void CMainView::RePaintLine(UINT uiIndex, wxDC *pDC)
{
wxPen cpenWhite(MY_RGB_INFO(255,255,255), 1, wxSOLID);
wxPen cpen(pColor[uiIndex], 1, wxSOLID);
wxDC *pDC2;
wxRect rct;

   rct = GetFrame()->GetClientRect();

   if(!pDC)
     pDC2 = new wxClientDC(GetFrame());
   else
     pDC2 = pDC;

//   pDC2->BeginDrawing(); this doesn't do anything anyway and is now deprecated

   wxPen cpenOld = pDC2->GetPen();
   pDC2->SetPen(cpen);

   pDC2->DrawLine(rct.x + 2, rct.y + 2 + uiIndex,
                  rct.x + 2 + pData[uiIndex], rct.y + 2 + uiIndex);

   pDC2->SetPen(cpenWhite);
   pDC2->DrawLine(rct.x + 2 + pData[uiIndex], rct.y + 2 + uiIndex,
                  rct.x + rct.width - 2,rct.y + 2 + uiIndex);

   pDC2->SetPen(cpenOld);

//   pDC2->EndDrawing(); this doesn't do anything anyway and is now deprecated

   if(pDC != pDC2)
     delete pDC2;
}


///////////////////////////////////////////////////////////////////////////////
//// CMainView diagnostics
//
//#ifdef _DEBUG
//void CMainView::AssertValid() const
//{
//	CView::AssertValid();
//}
//
//void CMainView::Dump(CDumpContext& dc) const
//{
//	CView::Dump(dc);
//}
//#endif //_DEBUG

///////////////////////////////////////////////////////////////////////////////
//// CMainView message handlers
//
//void CMainView::PostNcDestroy()
//{
//	// TODO: Add your specialized code here and/or call the base class
//
////	CView::PostNcDestroy();  // do *NOT* call this!  it deletes 'this'
//                            // and is only valid if I'm allocated
//                            // from the heap, which I am not.
//}

