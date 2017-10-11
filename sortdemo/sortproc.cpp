// sort procs for 'sortdemo'
//
// Copyright (c) 1995 by R. E. Frazier - all rights reserved
//
// This program and source provided for example purposes.  You may
// redistribute it so long as no modifications are made to any of
// the source files, and the above copyright notice has been 
// included.  You may also use portions of the sample code in your
// own programs, as desired.
//
// Portions of this file were adapted from 'SORTDEMO.C', a sample 
// application from Microsoft 'C' 7.0, and modified specifically
// to fit within this context.  The 'quick sort' algorithm is not
// the same as the original, but is a much faster algorithm
// containing a 'median of 3' optimization for the pivot point.
// The other algorithms have been retained more or less "as-is".
//

#include "stdafx.h"
#include "sortdemo.h"
#include "mainview.h"
#include "mainfrm.h"

#include "sortproc.h"

// global variables

int iCompares=0, iSwaps=0;
int iTimeDelay=2;  // 0.002 seconds, approximately


void QuickSort2( int iLow, int iHigh );
void PercolateDown( int iMaxLevel );
void PercolateUp( int iMaxLevel );


void FlashBar(int iBar)
{
MY_RGB_INFO clrOld;

   clrOld = pColor[iBar];
   pColor[iBar]=RGB(255,255,255);

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar);

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();

   MySleep(iTimeDelay);  // this gets me 1 delay for a 'compare'

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   pColor[iBar] = clrOld;
   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar);

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();
}

void FlashBar2(int iBar1, int iBar2)
{
MY_RGB_INFO clrOld1, clrOld2;

   clrOld1 = pColor[iBar1];
   clrOld2 = pColor[iBar2];
   pColor[iBar2] = pColor[iBar1] = RGB(255,255,255);

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar1);
   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar2);

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();

   MySleep(iTimeDelay);  // this gets me 1 delay for a 'compare'

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   pColor[iBar1] = clrOld1;
   pColor[iBar2] = clrOld2;

   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar1);
   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar2);

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();
}

void DrawBar(int iBar)
{
//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar);

//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();

   MySleep(iTimeDelay);  // this gets me 2 delays for a 'swap'
}

void SwapBars(int iBar1, int iBar2)
{
int iTemp;
MY_RGB_INFO clrTemp;

   iSwaps++;

   iTemp = pData[iBar1];
   clrTemp = pColor[iBar1];

   pData[iBar1] = pData[iBar2];
   pColor[iBar1] = pColor[iBar2];

   pData[iBar2] = iTemp;
   pColor[iBar2] = clrTemp;

//   WaitForSingleObject(theApp.m_hSortMutex, INFINITE);
//   theApp.m_SortMutex.Lock();

   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar1);
   ((CMainFrame *)theApp.m_pMainWnd)->RePaintLine(iBar2);
   
//   ReleaseMutex(theApp.m_hSortMutex);
//   theApp.m_SortMutex.Unlock();

   MySleep(2 * iTimeDelay);  // this gets me 2 delays for a 'swap'
}




/* InsertionSort - InsertionSort compares the length of each element
 * with the lengths of all the preceding elements. When the appropriate
 * place for the new element is found, the element is inserted and
 * all the other elements are moved down one place.
 */
void InsertionSort()
{
MY_RGB_INFO clrTemp;
int iTemp, iRow, iRowTmp, iLength;

    /* Start at the top. */
    for( iRow = 0; iRow < N_DIMENSIONS(pData); iRow++ )
    {
      if(wxMyThread::SafeTestDestroy())
         return;

       iTemp = pData[iRow];
       clrTemp = pColor[iRow];
       iLength = iTemp;

        /* As long as the length of the temporary element is greater than
         * the length of the original, keep shifting the elements down.
         */
        for( iRowTmp = iRow; iRowTmp; iRowTmp-- )
        {
          if(wxMyThread::SafeTestDestroy())
            return;

            iCompares++;
            FlashBar( iRowTmp - 1 );        // once per compare (for accuracy)

            if( pData[iRowTmp - 1] > iLength )
            {
                ++iSwaps;
                pData[iRowTmp] = pData[iRowTmp - 1];
                pColor[iRowTmp] = pColor[iRowTmp - 1];
                DrawBar( iRowTmp );         // Print the new bar

            }
            else                            // Otherwise, exit
                break;
        }

        /* Insert the original bar in the temporary position. */
        pData[iRowTmp] = iTemp;
        pColor[iRowTmp] = clrTemp; 
        DrawBar( iRowTmp );
    }
}


/* BubbleSort - BubbleSort cycles through the elements, comparing
 * adjacent elements and swapping pairs that are out of order. It
 * continues to do this until no out-of-order pairs are found.
 */
void BubbleSort()
{
int iRow, iSwitch, iLimit = N_DIMENSIONS(pData);

    iCompares = iSwaps = 0;

    /* Move the longest bar down to the bottom until all are in order. */
    do
    {
       if(wxMyThread::SafeTestDestroy())
         return;

        iSwitch = 0;
        for( iRow = 0; iRow < iLimit; iRow++ )
        {
            if(wxMyThread::SafeTestDestroy())
              return;

            /* If two adjacent elements are out of order, swap their values
             * and redraw those two bars.
             */
            iCompares++;
            FlashBar( iRow ); // provides proper delay for compares

            if( pData[iRow] > pData[iRow + 1] )
            {
                SwapBars( iRow, iRow + 1 );
                iSwitch = iRow;
            }
        }

        /* Sort on next pass only to where the last switch was made. */
        iLimit = iSwitch;
    } while( iSwitch );
}


/* HeapSort - HeapSort (also called TreeSort) works by calling
 * PercolateUp and PercolateDown. PercolateUp organizes the elements
 * into a "heap" or "tree," which has the properties shown below:
 *
 *                             element[1]
 *                           /            \
 *                element[2]                element[3]
 *               /          \              /          \
 *         element[4]     element[5]   element[6]    element[7]
 *         /        \     /        \   /        \    /        \
 *        ...      ...   ...      ... ...      ...  ...      ...
 *
 *
 * Each "parent node" is greater than each of its "child nodes"; for
 * example, element[1] is greater than element[2] or element[3];
 * element[4] is greater than element[5] or element[6], and so forth.
 * Therefore, once the first loop in HeapSort is finished, the
 * largest element is in element[1].
 *
 * The second loop rebuilds the heap (with PercolateDown), but starts
 * at the top and works down, moving the largest elements to the bottom.
 * This has the effect of moving the smallest elements to the top and
 * sorting the heap.
 */

void HeapSort()
{
int i;

    iCompares = iSwaps = 0;

    for( i = 1; i < N_DIMENSIONS(pData); i++ )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        PercolateUp( i );
    }

    for( i = N_DIMENSIONS(pData) - 1; i > 0; i-- )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        SwapBars( 0, i );
        PercolateDown( i - 1 );
    }
}


/* PercolateUp - Converts elements into a "heap" with the largest
 * element at the top (see the diagram above).
 */
void PercolateUp( int iMaxLevel )
{
    int i = iMaxLevel, iParent;

    /* Move the value in pData[iMaxLevel] up the heap until it has
     * reached its proper node (that is, until it is greater than either
     * of its child nodes, or until it has reached 1, the top of the heap).
     */
    while( i )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        iParent = i / 2;    // Get the subscript for the parent node

        iCompares++;
        FlashBar2( i, iParent );

        if( pData[i] > pData[iParent] )
        {
            /* The value at the current node is bigger than the value at
             * its parent node, so swap these two array elements.
             */
            SwapBars( iParent, i );
            i = iParent;
        }
        else
            /* Otherwise, the element has reached its proper place in the
             * heap, so exit this procedure.
             */
            break;
    }
}


/* PercolateDown - Converts elements to a "heap" with the largest elements
 * at the bottom. When this is done to a reversed heap (largest elements
 * at top), it has the effect of sorting the elements.
 */
void PercolateDown( int iMaxLevel )
{
    int iChild, i = 0;

    /* Move the value in pData[0] down the heap until it has reached
     * its proper node (that is, until it is less than its parent node
     * or until it has reached iMaxLevel, the bottom of the current heap).
     */
    while( TRUE )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        /* Get the subscript for the child node. */
        iChild = 2 * i;

        /* Reached the bottom of the heap, so exit this procedure. */
        if( iChild > iMaxLevel )
            break;

        /* If there are two child nodes, find out which one is bigger. */
        if( iChild + 1 <= iMaxLevel )
        {
            iCompares++;
            FlashBar2( iChild + 1, iChild );

            if( pData[iChild + 1] > pData[iChild] )
                iChild++;
        }

        iCompares++;
        FlashBar2( i, iChild );

        if( pData[i] < pData[iChild] )
        {
            /* Move the value down since it is still not bigger than
             * either one of its children.
             */
            SwapBars( i, iChild );
            i = iChild;
        }
        else
            /* Otherwise, pData has been restored to a heap from 1 to
             * iMaxLevel, so exit.
             */
            break;
    }
}


// 'HeapSort2()' is similar to 'HeapSort()', but uses a slightly improved
// algorithm.


void SiftUp(int iLow, int iHigh);

void HeapSort2()
{
int iCount, iHigh=N_DIMENSIONS(pData);

   iCompares = iSwaps = 0;

   for( iCount=(iHigh / 2); iCount>=1; iCount--)
   {
      if(wxMyThread::SafeTestDestroy())
        return;

      SiftUp(iCount, iHigh);    // note: 'SiftUp' uses 1-based indices
   }

   for(iCount=iHigh; iCount > 1; iCount--)
   {
      if(wxMyThread::SafeTestDestroy())
        return;

      SwapBars(0, iCount - 1);  // swap element 0 with 'last' item
      SiftUp(1, iCount - 1);    // re-create heap through 'last minus 1'
                                // note: 'SiftUp' uses 1-based indices 
   }
}

void SiftUp(int iLow, int iHigh)
{
int i1, iCount=iLow, half_way, double_count;

   if(iLow==iHigh) return;
   half_way = iHigh / 2;   // divide by 2 - done once for efficiency

   while(iCount <= half_way)
   {
      if(wxMyThread::SafeTestDestroy())
        return;

      double_count = iCount * 2;   // multiply by 2 - done once for efficiency

      if( double_count==iHigh )
      {
         iCompares++;
         FlashBar2(iCount - 1, double_count - 1);
         if(pData[iCount - 1] < pData[double_count - 1])
         {
	         SwapBars(iCount - 1, double_count - 1);     
         }

	      iCount = iHigh;
      }
      else
      {
         iCompares++;
         FlashBar2(double_count - 1, double_count);
         i1 = pData[double_count - 1] - pData[double_count];

         if(i1 >= 0)
         {
            iCompares++;
            FlashBar2(iCount - 1, double_count - 1);
            if(pData[iCount - 1] < pData[double_count - 1])
            {
               SwapBars(iCount - 1, double_count - 1);
  	           iCount = double_count;
               continue;
            }
         }
         else // if(i1 < 0)
         {
            iCompares++;
            FlashBar2(iCount - 1, double_count);
            if(pData[iCount - 1] < pData[double_count])
            {
               SwapBars(iCount - 1, double_count);
               iCount = double_count+1;
               continue;
            }
         }

         iCount = iHigh;
      }
   }
}



/* ExchangeSort - The ExchangeSort compares each element--starting with
 * the first--with every following element. If any of the following
 * elements is smaller than the current element, it is exchanged with
 * the current element and the process is repeated for the next element.
 */
void ExchangeSort()
{
int iRowCur, iRowMin, iRowNext;

    iCompares = iSwaps = 0;

    for( iRowCur = 0; iRowCur < N_DIMENSIONS(pData); iRowCur++ )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        iRowMin = iRowCur;
        for( iRowNext = iRowCur; iRowNext < N_DIMENSIONS(pData); iRowNext++ )
        {
            if(wxMyThread::SafeTestDestroy())
              return;

            iCompares++;
            FlashBar( iRowNext );

            if( pData[iRowNext] < pData[iRowMin] )
            {
                iRowMin = iRowNext;
            }
        }

        /* If a row is shorter than the current row, swap those two
         * array elements.
         */
        if( iRowMin > iRowCur )
        {
            SwapBars( iRowCur, iRowMin );
        }
    }
}


/* ShellSort - ShellSort is similar to the BubbleSort. However, it
 * begins by comparing elements that are far apart (separated by the
 * value of the iOffset variable, which is initially half the distance
 * between the first and last element), then comparing elements that
 * are closer together. When iOffset is one, the last iteration of
 * is merely a bubble sort.
 */
void ShellSort()
{
int iOffset, iSwitch, iLimit, iRow;

    iCompares = iSwaps = 0;

    /* Set comparison offset to half the number of bars. */
    iOffset = N_DIMENSIONS(pData) / 2;

    while( iOffset )
    {
        if(wxMyThread::SafeTestDestroy())
          return;

        /* Loop until offset gets to zero. */
        iLimit = N_DIMENSIONS(pData) - iOffset;
        do
        {
            if(wxMyThread::SafeTestDestroy())
              return;

            iSwitch = FALSE;     // Assume no switches at this offset.

            /* Compare elements and switch ones out of order. */
            for( iRow = 0; iRow <= iLimit; iRow++ )
            {
                if(wxMyThread::SafeTestDestroy())
                  return;

                iCompares++;
                FlashBar( iRow );

                if( pData[iRow] > pData[iRow + iOffset] )
                {
                    SwapBars( iRow, iRow + iOffset );
                    iSwitch = iRow;
                }
            }

            /* Sort on next pass only to where last switch was made. */
            iLimit = iSwitch - iOffset;
        } while( iSwitch );

        /* No switches at last offset, try one half as big. */
        iOffset = iOffset / 2;
    }
}


/* QuickSort - QuickSort works by picking a random "pivot" element,
 * then moving every element that is bigger to one side of the pivot,
 * and every element that is smaller to the other side. QuickSort is
 * then called recursively with the two subdivisions created by the pivot.
 * Once the number of elements in a subdivision reaches two, the recursive
 * calls end and the array is sorted.
 */
// NOTE:  This quick sort algorithm uses a 'median of 3' optimization,
//        comparing the low and high limits and center values, and
//        using the median of these 3 values as the pivot point.
//        Pivot points remain constant throughout the iteration.
//        Choice of a good pivot point results in better symmetry,
//        better sort efficiency, and less stack overhead.

void QuickSort()
{
   iCompares = iSwaps = 0;

   QuickSort2(0, N_DIMENSIONS(pData) - 1);
}

void QuickSort2( int iLow, int iHigh )
{
int iUp, iDown, iBreak;

    if( iLow < iHigh )                 // do not sort 1 element block
    {
        iBreak = pData[(iLow + iHigh)/2];    // initial pivot point

        if( (iHigh - iLow) > 5 )       // 5 or more elements?
        {
            // do a 'median of 3' optimization when practical
            // this ensures a better pivot point, limiting the 
            // effect of an already sorted or nearly sorted 
            // array on performance.  Pivot points should be
            // as close to the median value as practical for
            // the current range of data points.  Otherwise,
            // a series of bad pivot points could degenerate
            // the 'quicksort' into a 'slowsort', and possibly
            // cause stack overflows on large data sets.

            if(pData[iLow] <= pData[iHigh])
            {
               if(iBreak > pData[iLow])
               {
                  if(iBreak > pData[iHigh])
                  {
                     iBreak = pData[iHigh];
                  }
               }
               else
               {
                  iBreak = pData[iLow];
               }
            }
            else
            {
               if(iBreak > pData[iHigh])
               {
                  if(iBreak > pData[iLow])
                  {
                     iBreak = pData[iLow];
                  }
               }
               else
               {
                  iBreak = pData[iHigh];
               }
            }
        }


        iUp = iLow;                  // initialize indices
        iDown = iHigh;

        do
        {
            if(wxMyThread::SafeTestDestroy())
              return;

            // Move in from both sides towards the pivot point.

            while( iUp < iHigh)
            {
                if(wxMyThread::SafeTestDestroy())
                  return;

                iCompares++;
                FlashBar( iUp );

                if(pData[iUp] < iBreak)
                {
                    iUp++;
                }
                else
                {
                    break;
                }
            }

            while(iDown > iLow)
            {
                if(wxMyThread::SafeTestDestroy())
                  return;

                iCompares++;
                FlashBar( iDown );

                if(pData[iDown] > iBreak)
                {
                    iDown--;
                }
                else
                {
                    break;
                }
            }


            // if low/high boundaries have not crossed, swap current
            // 'boundary' values so that the 'iUp' pointer points
            // to a value less than or equal to the pivot point, 
            // and the 'iDown' value points to a value greater than
            // or equal to the pivot point, and continue.

            if( iUp <= iDown )
            {
                if(iUp != iDown) SwapBars( iUp, iDown );
                iUp++;
                iDown--;
            }

        } while ( iUp <= iDown );


        // the recursive part...

        if(iLow < iDown )  // everything to the left of the pivot point
        {
            QuickSort2( iLow, iDown );
        }
        if(iUp < iHigh)    // everything to the right of the pivot point
        {
            QuickSort2( iUp, iHigh );
        }
    }
}
