// mainview.h : header file
//

/////////////////////////////////////////////////////////////////////////////
// CMainView view

class CMainDoc : public wxDocument
{
DECLARE_DYNAMIC_CLASS(CMainDoc)
public:
  // nothing to see here, really
};

class CMainView : public wxView
{
DECLARE_DYNAMIC_CLASS(CMainView)
public:
	CMainView();           // protected constructor used by dynamic creation
//	DECLARE_DYNCREATE(CMainView)

// Attributes
public:

// Operations

//   void RePaintLine(UINT uiIndex, CDC *pDC);
   void RePaintLine(UINT uiIndex, wxDC *pDC);

public:

//// Overrides
//	// ClassWizard generated virtual function overrides
//	//{{AFX_VIRTUAL(CMainView)
//	protected:
//	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
//	virtual void PostNcDestroy();
//	//}}AFX_VIRTUAL

  virtual void OnPaint(wxPaintEvent &event);
  virtual void OnDraw(wxDC *pDC);
//  virtual void PostNcDestroy();

// Implementation
protected:
	virtual ~CMainView();
//#ifdef _DEBUG
//	virtual void AssertValid() const;
//	virtual void Dump(CDumpContext& dc) const;
//#endif

	// Generated message map functions
protected:
	//{{AFX_MSG(CMainView)
		// NOTE - the ClassWizard will add and remove member functions here.
	//}}AFX_MSG
//	DECLARE_MESSAGE_MAP()
  DECLARE_EVENT_TABLE()

   friend class CMainFrame;  // allows access to protected members
   friend class CSortdemoApp;
};

/////////////////////////////////////////////////////////////////////////////

extern int pData[256];

// color helpers

struct MY_RGB_INFO
{
  unsigned char ucData[sizeof(long)];  // the data

  // members

  MY_RGB_INFO() { ucData[0] = ucData[1] = ucData[2] = ucData[3] = 0; }
  MY_RGB_INFO(unsigned char nRed, unsigned char nGreen, unsigned char nBlue)
  {
    ucData[0] = nRed;
    ucData[1] = nGreen;
    ucData[2] = nBlue;
    ucData[3] = 0;
  }
  MY_RGB_INFO(const wxColour &clr)
  {
    ucData[0] = clr.Red();
    ucData[1] = clr.Green();
    ucData[2] = clr.Blue();
    ucData[3] = 0;
  }

  operator wxColour () const
  {
    return(wxColour(ucData[0],ucData[1],ucData[2]));
  }

  MY_RGB_INFO & operator= (const wxColour &clr)
  {
    ucData[0] = clr.Red();
    ucData[1] = clr.Green();
    ucData[2] = clr.Blue();
    ucData[3] = 0;
    return(*this);
  }
};

inline unsigned char RED(MY_RGB_INFO &rgb) { return(rgb.ucData[0]); }
inline unsigned char GREEN(MY_RGB_INFO &rgb) { return(rgb.ucData[1]); }
inline unsigned char BLUE(MY_RGB_INFO &rgb) { return(rgb.ucData[2]); }

inline MY_RGB_INFO RGB(unsigned char nRed, unsigned char nGreen, unsigned char nBlue)
{
  return(MY_RGB_INFO(nRed, nGreen, nBlue));
}
  
extern MY_RGB_INFO pColor[N_DIMENSIONS(pData)];  // same dim as 'pData'

