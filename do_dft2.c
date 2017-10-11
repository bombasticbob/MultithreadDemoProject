//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//                  _                _   __  _    ____                      //
//               __| |  ___       __| | / _|| |_ |___ \     ___             //
//              / _` | / _ \     / _` || |_ | __|  __) |   / __|            //
//             | (_| || (_) |   | (_| ||  _|| |_  / __/  _| (__             //
//              \__,_| \___/_____\__,_||_|   \__||_____|(_)\___|            //
//                         |_____|                                          //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////
//                                                                          //
//          Copyright (c) 2013 by S.F.T. Inc. - All rights reserved         //
//      Use, copying, and distribution of this software may be licensed     //
//         using a GPLv2, GPLv3, MIT, or BSD license, as appropriate.       //
//                                                                          //
//     OR - if you prefer - just use/distribute it without ANY license.     //
//   But I'd like some credit for it. A favorable mention is appreciated.   //
//                                                                          //
//////////////////////////////////////////////////////////////////////////////

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>

//**************************************************************************
// build command:  cc -O3 -o do_dft2 do_dft2.c -lm -lpthread
//
// you can add '-D' directives as appropriate (such as '-DUSE_FAST_SINCOS')
//**************************************************************************


// approximate value of pi beyond precision of double
#define _PI_ 3.1415926535897932384626433832795028841971693993751

#define MAX_HARMONIC 4096

//#define USE_FAST_SINCOS  /* define this out to use the fast sin/cos instead of the 'libc' sin/cos */
//#define TEST_FAST_SINCOS /* define this to include the test '-!' for fast sin/cos */

#ifdef __gnu_linux__
#define HAS_SINCOS /* this should work for all GNU-LINUX implementations */
// NOTE:  if this causes build problems, please let me know.
#endif // __gnu_linux__

// NOTE:  FreeBSD's libc does not have 'sincos', nor do any of the ports
//        for gcc, as far as I can tell.  It's a GNU extension.

#ifdef USE_FAST_SINCOS
#define sin fast_sin
#define cos fast_cos
#define sincos fast_sincos

double dDebugY = 0.0;
int wDebugIndex = -1;

void fast_sincos(double dTheta, double *pSin, double *pCos);
double fast_cos(double);
double fast_sin(double);

#ifndef HAS_SINCOS
#define HAS_SINCOS
#endif // HAS_SINCOS

static const float _SIN_VALUES_[];
static const float * const _COS_VALUES_;

#endif // USE_FAST_SINCOS

typedef struct _XY_
{
  double dX;
  double dY;
} XY;

typedef struct _MY_XY_
{
  XY *pData;
  int nItems; // # of items
  int nSize;  //memory block size
} MY_XY;

typedef struct _WORK_UNIT_
{
  double *pdA, *pdB;
  XY *pData;
  int nVal;
  double dC, dX0, dXY;  // for retest and for scaling X(Xnew = X * dXY + dX0, use 0.0 and 1.0 to leave X as - is)
  long lStart, lEnd;
  double dRval;         // NOTE: cannot be 'passed in', initial value will be 0.0
  volatile long lState; // initially zero, non - zero when thread has finished
  pthread_t idThread;   // caller waits on this object; object must be free 'd by caller via pthread_detach
                        // call pthread_join when finished to properly clean up and get err return
} WORK_UNIT;

WORK_UNIT *create_work_unit(double *pdA, double *pdB, double dC, XY * pData, int nVal,
                            double dX0, double dXY, long lStart, long lEnd,
                            void *(*callback) (void *), int iThreadFlag)
{
WORK_UNIT *pRval = (WORK_UNIT *) malloc(sizeof(WORK_UNIT));

  if(!pRval)
  {
    return NULL;
  }

  pRval->pdA = pdA;
  pRval->pdB = pdB;
  pRval->dC = dC;
  pRval->pData = pData;
  pRval->nVal = nVal;
  pRval->dX0 = dX0;
  pRval->dXY = dXY;
  pRval->lStart = lStart;
  pRval->lEnd = lEnd;
  pRval->dRval = 0.0;
  pRval->lState = 0;
  pRval->idThread = 0; // initially

  if(!iThreadFlag) // direct call, useful for first work unit(after spawning others)
  {
    // fprintf(stderr, "TEMPORARY:  call direct\n");
    // fflush(stderr);

    if(callback(pRval))
    {
      free(pRval);
      return NULL;
    }

    return pRval; // so I can get the return info
  }

  // fprintf(stderr, "TEMPORARY:  spawn thread\n");
  // fflush(stderr);

  if(pthread_create(&(pRval->idThread), NULL, callback, (void *)pRval))
  {
    if(pRval->idThread)
    {
      pthread_cancel(pRval->idThread);
      pRval->idThread = 0;
    }

    free(pRval);
    return NULL;
  }

  return pRval;
}

// determining # of cpus (workaround)

#define THREAD_COUNT 16

static unsigned long long MyGetTick(void)
{
static unsigned long long iMyTick = 0;
static unsigned long long lLastTickCount;
static int iFirstTime = 1;
struct timeval tv;
long long lTick;

  gettimeofday(&tv, NULL);
  lTick = (long long)tv.tv_sec * (long long)1000000 + (long long)tv.tv_usec;

  if(iFirstTime)
  {
    lLastTickCount = lTick;
    iMyTick = lTick;
    iFirstTime = 0;
  }
  else
  {
    iMyTick += lTick - lLastTickCount; //TODO:improve this for wraparound?
    lLastTickCount = lTick;
  }

  return (iMyTick);
}

static void *thread_proc(void *pNothing)
{
volatile long *pMe = pNothing ? (volatile long *)pNothing : (volatile long *)&(pMe);
volatile long long l1;
unsigned long long lStart;
int i1;

  usleep(1000);

  if(pNothing)
  {
    while(!*pMe)
    {
      usleep(100);
    }
  }

  lStart = MyGetTick();

  for(i1 = 0; i1 < 1000000; i1++)
  {
    l1 += *pMe;
  }

  if(pNothing)
  {
    *((long *)pNothing) = MyGetTick() - lStart;
    return 0;
  }

  return (void *)(MyGetTick() - lStart);
}

int cpu_count0(void)
{
pthread_t thr[THREAD_COUNT] = {NULL};
static volatile long lResult[THREAD_COUNT] = {0};
long lResult0, lResultTTL;
int i1;

  lResult0 = (long)thread_proc(0);

  usleep(1000);

  for(i1 = 0; i1 < THREAD_COUNT; i1++)
  {
    thr[i1] = NULL; //make sure
    if(pthread_create(&(thr[i1]), NULL, thread_proc, (void *)&(lResult[i1])))
    {
      while(i1 >= 0)
      {
        if(thr[i1])
        {
          pthread_cancel(thr[i1]);
        }

        i1--;
      }

      return -2;
    }
  }

  for(i1 = 0; i1 < THREAD_COUNT; i1++)
  {
    lResult[i1] = 1;
  }

  for(i1 = 0, lResultTTL = 0; i1 < THREAD_COUNT; i1++)
  {
    if(pthread_join(thr[i1], NULL))
    {
      fprintf(stderr, "thread canceled\n");
      pthread_cancel(thr[i1]);
      continue;
    }

    lResultTTL += lResult[i1];
  }

  // printf("%ld %ld\n", lResultTTL, lResult0);

  lResultTTL += lResultTTL / 2; // for round-off, add 75 %
  lResultTTL /= (THREAD_COUNT * lResult0);

  // printf("Total CPU:  %ld\n", lResultTTL);

  return (int)lResultTTL;
}

static int my_int_cmp(const void *p1, const void *p2)
{
int i1 = *((const int *)p1) - *((const int *)p2);

  return i1 > 0 ? 1 : (i1 < 0 ? -1 : 0);
}

int cpu_count(void)
{
int i1, i2[THREAD_COUNT];

  for(i1 = 0; i1 < THREAD_COUNT; i1++)
  {
    i2[i1] = cpu_count0();
  }

  qsort(i2, sizeof(i2[0]), THREAD_COUNT, my_int_cmp);

  return i2[THREAD_COUNT / 2];
}




/////////////////////////////////////////////////////////////////////////////
// FUNCTION: dFourier
//
// on entry 'aVal' is array of XY, 'nVal' is # of entries in aVal,
// nH is # harmonic (sin,cos) coefficients to generate [excluding '0']
// and 'dA' and 'dB' are the sin and cos arrays, and 'dC' is the 'C0' value.
// and 'nWU' is the # of 'work units' (using pthreads)
//
// note:  list must be sorted by X value, no duplicate X values
//
/////////////////////////////////////////////////////////////////////////////

void *dFourier_work(void *pV)
{
WORK_UNIT *pW = (WORK_UNIT *) pV;
int i1, i2, i3;
double dX, dY, dX0, dXY;
XY *aVal;
int nVal;
double dRval, *dA, *dB;


  if(!pV)
  {
    return NULL;
  }

  dRval = 0;

  aVal = pW->pData;
  nVal = pW->nVal;
  dA = pW->pdA;
  dB = pW->pdB;
  dX0 = pW->dX0;
  dXY = pW->dXY;

  for(i1 = pW->lStart, i3 = pW->lEnd; i1 <= i3; i1++)
  {
    for(i2 = 0; i2 < nVal; i2++)
    {
      if(!i1)
      {
        dRval += aVal[i2].dY;

        // printf("temporary:  item %d X=%g\n",i2, (double)(dX0 + aVal[i2].dX * dXY));
      }
      else
      {
        double dS, dC, dXNew = i1 * (dX0 + aVal[i2].dX * dXY);
#ifdef HAS_SINCOS // GNU linux and when I do 'fast sin/cos'
        sincos(dXNew, &dS, &dC); // NOTE:   'sincos' should be slightly faster than individual calls
#else // HAS_SINCOS
        dS = sin(dXNew);
        dC = cos(dXNew);
#endif // HAS_SINCOS
        dA[i1 - 1] += aVal[i2].dY * dC;
        dB[i1 - 1] += aVal[i2].dY * dS;
      }
    }
  }

  pW->dRval = dRval;
  pW->lState = 1;  // to say I 'm done

  return 0;
}

void dFourier(XY * aVal, int nVal, int nH, double *dC, double *dA, double *dB, int nWU, int iAutoScale)
{
int i1, i2, iW;
double dX, dY, dX0, dXY;
WORK_UNIT *aW[THREAD_COUNT] = {0};


  *dC = 0.0;
  // fprintf(stderr, "call to dFourier, %d work units\n", nWU);
  // fflush(stderr);
  // usleep(10000);

  // assume sorted list, calculate X0, XY for x = -PI to PI

  if(!iAutoScale)
  {
    dXY = 1.0;
    dX0 = 0.0;
  }
  else
  {
    dXY = 2.0 * _PI_ / (aVal[nVal - 1].dX + (aVal[nVal - 1].dX - aVal[0].dX) / (nVal - 1));
    dX0 = -dXY * aVal[0].dX - _PI_; // derived from -_PI_ == dX0 + dXY * aVal[0].dX
  }

  if(nH < nWU)
  {
    nWU = nH;
  }
  if(nVal < 1024)
  {
    nWU = 1;
  }

  for(i1 = 0; i1 < nH; i1++)
  {
    dA[i1] = dB[i1] = 0.0; // zero this out
  }

  for(iW = 0, i1 = 0; iW < nWU; iW++)
  {
    i2 = (iW + 1) * nH / nWU; // next i1

    if(i2 <= i1)
    {
      continue; // just in case
    }

    if(i2 >= nH || (iW == (nWU - 1)))
    {
      i2 = nH + 1; // to make sure I capture the last data point in the work unit
    }

    // fprintf(stderr, "temporary:  work unit %d\n", iW);
    // fflush(stderr);
    aW[iW] = create_work_unit(dA, dB, 0.0, aVal, nVal, dX0, dXY, i1, i2 - 1,
                              dFourier_work, iW < (nWU - 1) ? 1 : 0);

    i1 = i2; // "next"
    if(i2 >= nH)
    {
      break; // safety(for now)
    }
  }

        //now we must wait for all of the work units to complete
  for(iW = 0; iW < nWU; iW++)
  {
    if(!aW[iW])
    {
      continue;
    }

    if(aW[iW]->idThread)
    {
      pthread_join(aW[iW]->idThread, NULL); // ignore any error for now
      // pthread_detach(&(aW[iW].idThread);

      *dC += aW[iW]->dRval; // returned C0 value(when applicable) adds into 'dC'
    }
    else // not a thread, keep 'dC' return here also
    {
      *dC += aW[iW]->dRval; // returned C0 value(when applicable) adds into 'dC'
    }

    free(aW[iW]);
    aW[iW] = NULL; // by convention
  }

  // fix up arrays and whatnot

  *dC /= nVal;  // C0 must be half A[0] i.e Y = A[0] / 2 +[sum n = 1 - ?] An *cos(n * X) + Bn * sin(n * X)
  // see http : //en.wikipedia.org / wiki / Fourier_series

  for(i1 = 0; i1 < nH; i1++)
  {
    dA[i1] *= 2.0 / nVal;
    dB[i1] *= 2.0 / nVal;
  }
}




// FUNCTION:xy_comp - sort compare for 'XY' structure

int xy_comp(const void *p1, const void *p2)
{
  int iRval = ((XY *) p1)->dX - ((XY *) p2)->dX;

  if(iRval > 0)
  {
    return 1;
  }

  if(iRval < 0)
  {
    return -1;
  }

  return 0;
}

//FUNCTION:get_xy_data - file input of X and Y values(space delimiter)

MY_XY get_xy_data(FILE * pIn)
{
  char tbuf[512];
  double dX, dY;
  MY_XY xyNULL = {NULL, 0, 0}, xy = {NULL, 0, 0};


  while(fgets(tbuf, sizeof(tbuf), pIn))
  {
    if(!xy.pData ||
        xy.nItems * sizeof(xy.pData[0]) >= xy.nSize)
    {
      if(xy.nItems > 1024)
      {
        xy.nSize = (xy.nItems * 2) * sizeof(xy.pData[0]);
      }
      else
      {
        xy.nSize = 2048 * sizeof(xy.pData[0]);
      }

      if(xy.pData)
      {
        void *p1 = realloc(xy.pData, xy.nSize + 1);

        if(!p1)
        {
          free(xy.pData);
          return xyNULL;
        }

        xy.pData = (XY *) p1;
      }
      else
      {
        xy.pData = (XY *) malloc(xy.nSize);
        if(!xy.pData)
        {
          return xyNULL;
        }
      }
    }

    dX = dY = 0.0;

    sscanf(tbuf, "%lg %lg\n", &dX, &dY);

    // printf("TEMPORARY:  data point %d %g %g   %s\n", xy.nItems, dX, dY, tbuf);

    xy.pData[xy.nItems].dX = dX;
    xy.pData[xy.nItems].dY = dY;
    xy.nItems++;
  }

  // sort data by X

  qsort(xy.pData, xy.nItems, sizeof(xy.pData[0]), xy_comp);

  return xy;
}


void usage(void)
{
  fprintf(stderr,
          "         Copyright (c) 2013 by S.F.T. Inc. - All rights reserved\n"
          "     Use, copying, and distribution of this software may be licensed\n"
          "        using a GPLv2, GPLv3, MIT, or BSD license, as appropriate.\n"
          "\n"
          "    OR - if you prefer - just use/distribute it without ANY license.\n"
          "  But I'd like some credit for it. A favorable mention is appreciated.\n"
          "\n"
          "USAGE:  do_dft -h\n"
          "        do_dft [-a|-s m,n][-t nthrd][input_file [input_file [...]]]\n"
          "where   'input_file' is the name of a file containing rows of sorted X and Y\n"
          "                     values delimited by white-space and terminated with LF\n"
          " and    '-a' indicates 'auto scale X' to 0-2pi\n"
          " and    '-s' specifies a range of 'm to n'\n"
          " and    '-t' indicates how many threads you want to use\n"
          " and    '-h' instructs do_dft to print this information\n"
          "        (if no file or '-h' specified, input is 'stdin')\n");
}


// check_callback - offloads work for multiple threads to check data and calc std dev

void *check_callback(void *pV)
{
  WORK_UNIT *pW = (WORK_UNIT *) pV;
  int i1, i2, i3;
  double dX, dY, dX0, dXY, dC;
  XY *aVal;
  int nHarm;
  double dErr, *pdA, *pdB;


  if(!pV)
  {
    return 0;
  }

   dErr = 0.0;

  aVal = pW->pData;
  nHarm = pW->nVal;
  pdA = pW->pdA;
  pdB = pW->pdB;
  dC = pW->dC;
  dX0 = pW->dX0;
  dXY = pW->dXY;

  for(i1 = pW->lStart; i1 < pW->lEnd; i1++)
  {
    double dCheck = dC;

    for(i2 = 0; i2 < nHarm; i2++)
    {
      dCheck += pdA[i2] * cos((i2 + 1) * (aVal[i1].dX * dXY + dX0))
         + pdB[i2] * sin((i2 + 1) * (aVal[i1].dX * dXY + dX0));
    }

    // printf("  data point %d\t%g\t%g\t%g\n", i1, xy.pData[i1].dX, xy.pData[i1].dY, dCheck);
    dErr += (dCheck - aVal[i1].dY) * (dCheck - aVal[i1].dY);
  }

  pW->dRval = dErr;
  pW->lState = 1;
  return 0;
}


  ////////////////
  //   MAIN
  ///////////////


int main(int argc, char *argv[])
{
double dC, dXY, dX0, *pdA = NULL, *pdB = NULL, dErr;
int i1, i2, iW;
FILE *pIn = stdin;
int nHarm, nThread = 0;
WORK_UNIT *aW[THREAD_COUNT];
double dScale1 = 0.0, dScale2 = 0.0;
int bDoScale = 0;
double dXFactor, dXOffset;
char tbuf[256];



  while(argc > 1)
  {
    const char *p1 = argv[1];
    char *p2;

    if(*p1 != '-')
    {
      break;
    }

    p1++;

#if defined(TEST_FAST_SINCOS) && defined(USE_FAST_SINCOS)

#undef sin /* I need the original versions for this section */
#undef cos /* so I must un-define them, then re-define them */

#define SINCOS_ACCURACY 0.000001

    if(*p1 == '!')
    {
      // this is a special switch that allows self-testing the fast sin/cos functions

      double d1, dS, dC;
      int i2 = 0;
      unsigned long long llTick;

      for(i1=0, d1=-2.0 * _PI_ - 0.1; d1 < 2.0 * _PI_ + 0.1; d1 += 0.00001, i1+=4)
      {
        fast_sincos(d1, &dS, &dC);

        if(fabs(cos(d1) - dC) > SINCOS_ACCURACY)
        {
          fprintf(stderr, "cos(%0.3f) delta = %0.7f  wIndex=%d  y=%0.6f\n", d1,
                  fabs(cos(d1) - dC), wDebugIndex, dDebugY);
        }
        else
        {
          i2++;
        }

        if(fabs(sin(d1) - dS) > SINCOS_ACCURACY)
        {
          fprintf(stderr, "sin(%0.3f) delta = %0.7f  wIndex=%d  y=%0.6f\n", d1,
                  fabs(sin(d1) - dS), wDebugIndex, dDebugY);
        }
        else
        {
          i2++;
        }

        if(fabs(cos(d1) - fast_cos(d1)) > SINCOS_ACCURACY)
        {
          fprintf(stderr, "cos(%0.3f) delta = %0.7f  wIndex=%d  y=%0.6f\n", d1,
                  fabs(cos(d1) - fast_cos(d1)), wDebugIndex, dDebugY);
        }
        else
        {
          i2++;
        }

        if(fabs(sin(d1) - fast_sin(d1)) > SINCOS_ACCURACY)
        {
          fprintf(stderr, "sin(%0.3f) delta = %0.7f  wIndex=%d  y=%0.6f\n", d1,
                  fabs(sin(d1) - fast_sin(d1)), wDebugIndex, dDebugY);
        }
        else
        {
          i2++;
        }
      }

      fprintf(stderr, "Tested %d values, %d in spec\n", i1, i2);

      llTick = MyGetTick();

      for(i1=0, d1=-2.0 * _PI_ - 0.1; d1 < 2.0 * _PI_ + 0.1; d1 += 0.00001, i1+=4)
      {
        fast_sincos(d1, &dS, &dC);
      }

      fprintf(stderr, "fast_sincos took %lld msecs\n", MyGetTick() - llTick);

      llTick = MyGetTick();

      for(i1=0, d1=-2.0 * _PI_ - 0.1; d1 < 2.0 * _PI_ + 0.1; d1 += 0.00001, i1+=4)
      {
        *((volatile double *)&dS) = sin(d1);
        *((volatile double *)&dC) = cos(d1);
      }

      fprintf(stderr, "sin,cos took %lld msecs\n", MyGetTick() - llTick);

      return 0;
    }

#define sin fast_sin
#define cos fast_cos

#endif // USE_FAST_SINCOS, TEST_FAST_SINCOS

    if(!*p1)
    {
      usage();
      return -1;
    }
    while(*p1)
    {
      if(*p1 == 'h')
      {
        usage();
        if(p1[1] || argc > 2)
        {
          return -1;
        }
        else
        {
          return 0;
        }
      }
      else if(*p1 == 'a') // autoscale
      {
        bDoScale = -1;
      }
      else if(*p1 == 's') // set scale
      {
        bDoScale = 1;

        p1++;
        if(!*p1)
        {
          argc--;
          argv++;
          if(argc < 2)
          {
            usage();
            return -1;
          }

          p1 = argv[1];
        }

        p2 = tbuf;

        while(*p1 && *p1 != ',')
        {
          *(p2++) = *(p1++);
        }

        *p2 = 0;
        dScale1 = atof(tbuf);

        if(*p1 != ',' || !p1[1])
        {
          usage();
          return -1;
        }

        p1++;
        p2 = tbuf;

        while(*p1 && *p1 != ',')
        {
          *(p2++) = *(p1++);
        }

        *p2 = 0;
        dScale2 = atof(tbuf);

        break; //the parsing stops here for this term
      }
      else if(*p1 == 't') // # of threads
      {
        p1++;
        if(*p1)
        {
          nThread = atoi(p1);
        }
        else
        {
          argc--;
          argv++;
          if(argc < 2)
          {
            usage();
            return -1;
          }

          nThread = atoi(argv[1]);
        }

        if(nThread <= 0)
        {
          usage();
          return -2;
        }
        break; // the parsing stops here for this term
      }
      // TODO: other options, like cycle count maybe ?
      else
      {
        usage();
        return -2; // unknown option
      }
      p1++;
    }

    argc--; // in anticipation of other options, this is the loop counter for it
    argv++;
  }

  if(!nThread)
  {
    nThread = cpu_count();
    //fprintf(stderr, "TEMPORARY:  %d threads\n");
  }

  while(argc > 1 || pIn == stdin)
  {
MY_XY xy;

    if(argc > 1)
    {
      pIn = fopen(argv[1], "r");

      if(!pIn)
      {
        argv++;
        argc--;

        continue;
      }

      printf("FILE:  %s\n", argv[1]);
      argv++;
      argc--;
    }

    xy = get_xy_data(pIn);

    fclose(pIn);
    pIn = NULL;

    if(!xy.pData || !xy.nItems)
    {
      continue;
    }

    if(bDoScale > 0)
    {
      if(xy.pData[xy.nItems - 1].dX > xy.pData[0].dX)
      {
        //assume data is sorted
        dXFactor = ((dScale2 - dScale1) // the delta scale(normally - pi to pi for autoscale)
                 / (xy.pData[xy.nItems - 1].dX - xy.pData[0].dX)) // the delta X
                 * (double)(xy.nItems - 1)
                 / (double)(xy.nItems);  // last data point represents "not quite 2 * pi"

        dXOffset = dScale1 - xy.pData[0].dX * dXFactor;

        for(i1 = 0; i1 < xy.nItems; i1++)
        {
          xy.pData[i1].dX = xy.pData[i1].dX * dXFactor + dXOffset;
        }
      }

      fprintf(stderr, "TEMPORARY:  scaling factors:  m=%g b=%g\n", dXFactor, dXOffset);
    }
    else if(bDoScale < 0)
    {
      fprintf(stderr, "TEMPORARY:  autoscale -PI to PI\n");
    }


    nHarm = xy.nItems / 2 > MAX_HARMONIC
          ? MAX_HARMONIC : xy.nItems / 2;

    if(xy.nItems < 2 || nHarm < 1)
    {
      continue;
    }

    pdA = (double *)malloc(sizeof(*pdA) * (nHarm + 1) * 2);
    if(!pdA)
    {
      fprintf(stderr, "out of memory for work buffers\n");
      return -3;
    }

    pdB = pdA + nHarm + 1;

    dFourier(xy.pData, xy.nItems, nHarm, &dC, pdA, pdB, nThread, bDoScale < 0 ? 1 : 0);

    printf("harm #\t      magnitude\t    phase (deg)\t  offset (C0)=%g\n", dC);

    for(i1 = 0; i1 < nHarm; i1++)
    {
      printf("  %3d\t"
             // "%7g\t%7g\t"
             "%15.6f\t%15.6f\n",
             i1 + 1,
             //dA[i1],
             //dB[i1],
             sqrt(pdA[i1] * pdA[i1] + pdB[i1] * pdB[i1]),
             atan2(pdB[i1], pdA[i1]) * 180 / _PI_ + 180);
      // NOTE:  atan result for cosine will be - 180, sin - 90
      //        because the analysis is - PI to PI
      //        adding 180 will give you 0, 90
    }

    // figure out relative error (i.e. std deviation) and report it

//  if() TODO - make this optional
//  {
    dXY = 2.0 * _PI_ / (xy.pData[xy.nItems - 1].dX + (xy.pData[xy.nItems - 1].dX - xy.pData[0].dX) / (xy.nItems - 1));
    dX0 = -dXY * xy.pData[0].dX - _PI_; // derived from -_PI_ == dX0 + dXY * aVal[0].dX


    for(i1 = 0, iW = 0; iW < nThread; iW++)
    {
      i2 = (iW + 1) * xy.nItems / nThread;
      if(i2 > xy.nItems)
      {
        i2 = xy.nItems;
      }
      aW[iW] = create_work_unit(pdA, pdB, dC, xy.pData, nHarm, dX0, dXY,
                                i1, i2 - 1, check_callback, iW < (nThread - 1));
      if(!aW[iW])
      {
        fprintf(stderr, "threading error on data check\n");
        return -3;
      }

      i1 = i2; // next group
    }

    for(iW = 0, dErr = 0.0; iW < nThread; iW++)
    {
      if(!aW[iW])
      {
        continue;
      }

      if(aW[iW]->idThread)
      {
        pthread_join(aW[iW]->idThread, NULL); // ignore any error for now
        //pthread_detach(&(aW[iW].idThread);

        dErr += aW[iW]->dRval; // returned C0 value(when applicable) adds into 'dC'
      }
      else // not a thread, keep 'dC' return here also
      {
        dErr += aW[iW]->dRval; // returned C0 value(when applicable) adds into 'dC'
      }

      free(aW[iW]);
      aW[iW] = NULL; // by convention
    }

    printf("relative accuracy:  %g\n", sqrt(dErr / xy.nItems));

//  }

    if(xy.pData)
    {
      free(xy.pData);
    }
    if(pdA)
    {
      free(pdA);
    }
  }

  return 0;
}

#ifdef USE_FAST_SINCOS

// FAST SIN/COS UTILITIES - you can use these as you see fit, by the way (no license restrictions)
// These really are trivial, though, so it's no big deal.  Just has a sin/cos table.
// What makes it faster is a) float precision, b) table lookup, c) shared calcs for 'fast_sincos'
// And the precision tester, if you compile it in, shows that it's a match within 6 digits of precision
// so there should be no problem using it, though your results may differ slightly.

double make_valid_sincos_range(double dTheta)
{
float x0;

  x0 = (float)dTheta;

  if(x0<(float)-(3 * _PI_) || x0>(float)(3 * _PI_))
  {
    x0 -= ( floorf(x0 * (float)(1 / (2 * _PI_))) ) * (float)(2.0 * _PI_);
  }

  /* optimized, hopefully, to minimize multiplications! */

  if(x0<(float)-_PI_)
  {
    x0+= (float)(2 * _PI_);  /* ensure it's between -pi & pi */
  }
  if(x0>(float)_PI_)
  {
    x0-= (float)(2 * _PI_);  /* ensure it's between -pi & pi */
  }

  return(x0);
}

void fast_sincos(double dTheta, double *pSin, double *pCos)
{
unsigned int wIndex;
float x0, y;


  x0 = make_valid_sincos_range(dTheta);             /* x is between -pi and pi */

  y = (float)5000.0 + x0 * (float)(5000.0 / _PI_); /* convert to an 'index' value... */

  wIndex = (unsigned int)floorf(y);
  y -= (float)wIndex;  /* this gives us the index and the 'interpolate' value... */

  dDebugY = y;
  wDebugIndex = wIndex;

  if(y == 0.0) // possible
  {
    *pCos = _COS_VALUES_[wIndex];
    *pSin = _SIN_VALUES_[wIndex];
  }
  else       /* not an 'even' value - interpolate between two entries */
  {
    *pCos = _COS_VALUES_[wIndex] +
            (_COS_VALUES_[wIndex + 1] - _COS_VALUES_[wIndex]) * y;

    *pSin = _SIN_VALUES_[wIndex] +
            (_SIN_VALUES_[wIndex + 1] - _SIN_VALUES_[wIndex]) * y;
  }
}

double fast_cos(double dTheta)
{
unsigned int wIndex;
float x0, y;


  x0 = make_valid_sincos_range(dTheta);             /* x is between -pi and pi */

  y = (float)5000.0 + x0 * (float)(5000.0 / _PI_);
                                       /* convert to an 'index' value... */

  wIndex = (unsigned int)floorf(y);
  y -= (float)wIndex;    /* this gives us the index and the 'interpolate' value... */

  if(!y)
  {
    return(_COS_VALUES_[wIndex]);
  }
  else       /* not an 'even' value - interpolate between two entries */
  {
    return(_COS_VALUES_[wIndex] +
           (_COS_VALUES_[wIndex + 1] - _COS_VALUES_[wIndex]) * y);
  }
}

double fast_sin(double dTheta)
{
unsigned int wIndex;
float x0, y;

  x0 = make_valid_sincos_range(dTheta);            /* x is between -pi and pi */

  y = (float)5000.0 + x0 * (float)(5000.0 / _PI_);
                                       /* convert to an 'index' value... */

  wIndex = (unsigned int)floorf(y);
  y -= (float)wIndex;    /* this gives us the index and the 'interpolate' value... */

  if(!y)
  {
    return(_SIN_VALUES_[wIndex]);
  }
  else       /* not an 'even' value - interpolate between two entries */
  {
    return(_SIN_VALUES_[wIndex] +
           (_SIN_VALUES_[wIndex + 1] - _SIN_VALUES_[wIndex]) * y);
  }
}

static const float _SIN_VALUES_[12501]=
{
   0.00000000,-0.00062831,-0.00125663,-0.00188495,-0.00251327,
   -0.00314158,-0.00376990,-0.00439821,-0.00502652,-0.00565483,
   -0.00628314,-0.00691144,-0.00753975,-0.00816804,-0.00879634,
   -0.00942463,-0.01005292,-0.01068121,-0.01130949,-0.01193776,
   -0.01256603,-0.01319430,-0.01382256,-0.01445082,-0.01507907,
   -0.01570731,-0.01633555,-0.01696378,-0.01759201,-0.01822022,
   -0.01884843,-0.01947664,-0.02010483,-0.02073302,-0.02136120,
   -0.02198937,-0.02261753,-0.02324569,-0.02387383,-0.02450197,
   -0.02513009,-0.02575821,-0.02638631,-0.02701440,-0.02764249,
   -0.02827056,-0.02889862,-0.02952667,-0.03015471,-0.03078274,
   -0.03141075,-0.03203876,-0.03266675,-0.03329472,-0.03392269,
   -0.03455064,-0.03517857,-0.03580650,-0.03643440,-0.03706230,
   -0.03769018,-0.03831804,-0.03894589,-0.03957373,-0.04020154,
   -0.04082935,-0.04145713,-0.04208490,-0.04271266,-0.04334039,
   -0.04396811,-0.04459582,-0.04522350,-0.04585117,-0.04647882,
   -0.04710645,-0.04773406,-0.04836165,-0.04898922,-0.04961678,
   -0.05024431,-0.05087183,-0.05149932,-0.05212680,-0.05275425,
   -0.05338168,-0.05400910,-0.05463649,-0.05526386,-0.05589120,
   -0.05651853,-0.05714583,-0.05777311,-0.05840037,-0.05902760,
   -0.05965482,-0.06028200,-0.06090917,-0.06153631,-0.06216342,
   -0.06279051,-0.06341758,-0.06404462,-0.06467164,-0.06529863,
   -0.06592559,-0.06655253,-0.06717944,-0.06780633,-0.06843319,
   -0.06906002,-0.06968683,-0.07031360,-0.07094035,-0.07156707,
   -0.07219377,-0.07282043,-0.07344707,-0.07407367,-0.07470025,
   -0.07532680,-0.07595332,-0.07657981,-0.07720627,-0.07783269,
   -0.07845909,-0.07908546,-0.07971179,-0.08033809,-0.08096437,
   -0.08159061,-0.08221681,-0.08284299,-0.08346913,-0.08409524,
   -0.08472132,-0.08534736,-0.08597337,-0.08659934,-0.08722528,
   -0.08785119,-0.08847706,-0.08910290,-0.08972870,-0.09035447,
   -0.09098020,-0.09160589,-0.09223155,-0.09285717,-0.09348276,
   -0.09410831,-0.09473382,-0.09535929,-0.09598473,-0.09661013,
   -0.09723549,-0.09786081,-0.09848609,-0.09911134,-0.09973654,
   -0.10036171,-0.10098684,-0.10161192,-0.10223697,-0.10286197,
   -0.10348694,-0.10411186,-0.10473675,-0.10536159,-0.10598639,
   -0.10661115,-0.10723587,-0.10786054,-0.10848517,-0.10910976,
   -0.10973431,-0.11035881,-0.11098327,-0.11160768,-0.11223205,
   -0.11285638,-0.11348066,-0.11410490,-0.11472909,-0.11535324,
   -0.11597734,-0.11660140,-0.11722540,-0.11784937,-0.11847328,
   -0.11909715,-0.11972098,-0.12034475,-0.12096848,-0.12159216,
   -0.12221579,-0.12283938,-0.12346291,-0.12408640,-0.12470984,
   -0.12533323,-0.12595657,-0.12657986,-0.12720310,-0.12782629,
   -0.12844943,-0.12907251,-0.12969555,-0.13031854,-0.13094147,
   -0.13156435,-0.13218718,-0.13280996,-0.13343269,-0.13405536,
   -0.13467798,-0.13530055,-0.13592307,-0.13654553,-0.13716793,
   -0.13779029,-0.13841258,-0.13903483,-0.13965702,-0.14027915,
   -0.14090123,-0.14152325,-0.14214522,-0.14276713,-0.14338898,
   -0.14401078,-0.14463252,-0.14525420,-0.14587583,-0.14649740,
   -0.14711891,-0.14774036,-0.14836175,-0.14898309,-0.14960437,
   -0.15022558,-0.15084674,-0.15146784,-0.15208888,-0.15270986,
   -0.15333078,-0.15395164,-0.15457243,-0.15519317,-0.15581385,
   -0.15643446,-0.15705501,-0.15767550,-0.15829593,-0.15891629,
   -0.15953660,-0.16015684,-0.16077701,-0.16139713,-0.16201717,
   -0.16263716,-0.16325708,-0.16387694,-0.16449673,-0.16511646,
   -0.16573612,-0.16635571,-0.16697524,-0.16759471,-0.16821411,
   -0.16883344,-0.16945270,-0.17007190,-0.17069103,-0.17131010,
   -0.17192910,-0.17254802,-0.17316688,-0.17378568,-0.17440440,
   -0.17502305,-0.17564164,-0.17626016,-0.17687860,-0.17749698,
   -0.17811529,-0.17873352,-0.17935169,-0.17996978,-0.18058781,
   -0.18120576,-0.18182364,-0.18244145,-0.18305919,-0.18367685,
   -0.18429444,-0.18491196,-0.18552941,-0.18614678,-0.18676408,
   -0.18738131,-0.18799846,-0.18861554,-0.18923254,-0.18984947,
   -0.19046633,-0.19108310,-0.19169981,-0.19231644,-0.19293299,
   -0.19354946,-0.19416586,-0.19478218,-0.19539843,-0.19601460,
   -0.19663069,-0.19724670,-0.19786264,-0.19847850,-0.19909427,
   -0.19970998,-0.20032560,-0.20094114,-0.20155660,-0.20217199,
   -0.20278729,-0.20340251,-0.20401766,-0.20463272,-0.20524770,
   -0.20586260,-0.20647742,-0.20709216,-0.20770682,-0.20832139,
   -0.20893589,-0.20955030,-0.21016462,-0.21077887,-0.21139303,
   -0.21200710,-0.21262110,-0.21323501,-0.21384883,-0.21446258,
   -0.21507623,-0.21568980,-0.21630329,-0.21691669,-0.21753001,
   -0.21814324,-0.21875638,-0.21936944,-0.21998241,-0.22059529,
   -0.22120809,-0.22182080,-0.22243342,-0.22304595,-0.22365840,
   -0.22427076,-0.22488302,-0.22549520,-0.22610730,-0.22671930,
   -0.22733121,-0.22794303,-0.22855477,-0.22916641,-0.22977796,
   -0.23038942,-0.23100079,-0.23161207,-0.23222326,-0.23283435,
   -0.23344536,-0.23405627,-0.23466709,-0.23527782,-0.23588845,
   -0.23649899,-0.23710944,-0.23771979,-0.23833005,-0.23894022,
   -0.23955029,-0.24016027,-0.24077015,-0.24137994,-0.24198963,
   -0.24259923,-0.24320873,-0.24381813,-0.24442744,-0.24503665,
   -0.24564577,-0.24625478,-0.24686370,-0.24747253,-0.24808125,
   -0.24868988,-0.24929841,-0.24990684,-0.25051518,-0.25112341,
   -0.25173154,-0.25233958,-0.25294751,-0.25355535,-0.25416309,
   -0.25477072,-0.25537826,-0.25598569,-0.25659302,-0.25720025,
   -0.25780738,-0.25841441,-0.25902134,-0.25962816,-0.26023488,
   -0.26084150,-0.26144802,-0.26205443,-0.26266074,-0.26326694,
   -0.26387304,-0.26447904,-0.26508493,-0.26569072,-0.26629641,
   -0.26690198,-0.26750746,-0.26811282,-0.26871809,-0.26932324,
   -0.26992829,-0.27053323,-0.27113807,-0.27174280,-0.27234742,
   -0.27295193,-0.27355634,-0.27416063,-0.27476482,-0.27536890,
   -0.27597288,-0.27657674,-0.27718050,-0.27778414,-0.27838768,
   -0.27899110,-0.27959442,-0.28019762,-0.28080072,-0.28140370,
   -0.28200657,-0.28260933,-0.28321198,-0.28381452,-0.28441694,
   -0.28501926,-0.28562146,-0.28622355,-0.28682552,-0.28742738,
   -0.28802913,-0.28863077,-0.28923229,-0.28983369,-0.29043498,
   -0.29103616,-0.29163722,-0.29223817,-0.29283900,-0.29343972,
   -0.29404032,-0.29464080,-0.29524117,-0.29584142,-0.29644156,
   -0.29704158,-0.29764148,-0.29824126,-0.29884092,-0.29944047,
   -0.30003990,-0.30063921,-0.30123840,-0.30183748,-0.30243643,
   -0.30303526,-0.30363398,-0.30423257,-0.30483105,-0.30542940,
   -0.30602764,-0.30662575,-0.30722374,-0.30782161,-0.30841936,
   -0.30901699,-0.30961449,-0.31021188,-0.31080914,-0.31140628,
   -0.31200329,-0.31260018,-0.31319695,-0.31379360,-0.31439012,
   -0.31498651,-0.31558279,-0.31617893,-0.31677496,-0.31737086,
   -0.31796663,-0.31856228,-0.31915780,-0.31975319,-0.32034846,
   -0.32094360,-0.32153862,-0.32213351,-0.32272827,-0.32332291,
   -0.32391741,-0.32451179,-0.32510604,-0.32570017,-0.32629416,
   -0.32688802,-0.32748176,-0.32807537,-0.32866884,-0.32926219,
   -0.32985541,-0.33044850,-0.33104145,-0.33163428,-0.33222698,
   -0.33281954,-0.33341197,-0.33400427,-0.33459644,-0.33518848,
   -0.33578038,-0.33637216,-0.33696380,-0.33755530,-0.33814668,
   -0.33873792,-0.33932902,-0.33991999,-0.34051083,-0.34110153,
   -0.34169210,-0.34228254,-0.34287284,-0.34346300,-0.34405303,
   -0.34464292,-0.34523267,-0.34582229,-0.34641178,-0.34700112,
   -0.34759033,-0.34817940,-0.34876834,-0.34935714,-0.34994579,
   -0.35053432,-0.35112270,-0.35171094,-0.35229905,-0.35288701,
   -0.35347484,-0.35406253,-0.35465007,-0.35523748,-0.35582475,
   -0.35641187,-0.35699886,-0.35758570,-0.35817241,-0.35875897,
   -0.35934539,-0.35993167,-0.36051781,-0.36110380,-0.36168965,
   -0.36227536,-0.36286093,-0.36344635,-0.36403163,-0.36461677,
   -0.36520176,-0.36578660,-0.36637131,-0.36695587,-0.36754028,
   -0.36812455,-0.36870867,-0.36929265,-0.36987648,-0.37046017,
   -0.37104371,-0.37162710,-0.37221034,-0.37279344,-0.37337640,
   -0.37395920,-0.37454186,-0.37512437,-0.37570673,-0.37628894,
   -0.37687101,-0.37745292,-0.37803469,-0.37861630,-0.37919777,
   -0.37977909,-0.38036026,-0.38094128,-0.38152214,-0.38210286,
   -0.38268343,-0.38326384,-0.38384411,-0.38442422,-0.38500418,
   -0.38558399,-0.38616364,-0.38674315,-0.38732250,-0.38790170,
   -0.38848074,-0.38905963,-0.38963837,-0.39021696,-0.39079539,
   -0.39137366,-0.39195178,-0.39252975,-0.39310756,-0.39368522,
   -0.39426272,-0.39484006,-0.39541725,-0.39599429,-0.39657116,
   -0.39714789,-0.39772445,-0.39830086,-0.39887711,-0.39945320,
   -0.40002913,-0.40060491,-0.40118053,-0.40175599,-0.40233129,
   -0.40290643,-0.40348141,-0.40405624,-0.40463090,-0.40520541,
   -0.40577975,-0.40635394,-0.40692796,-0.40750183,-0.40807553,
   -0.40864907,-0.40922245,-0.40979567,-0.41036873,-0.41094162,
   -0.41151435,-0.41208692,-0.41265933,-0.41323158,-0.41380366,
   -0.41437558,-0.41494733,-0.41551892,-0.41609035,-0.41666161,
   -0.41723271,-0.41780364,-0.41837441,-0.41894501,-0.41951545,
   -0.42008572,-0.42065583,-0.42122577,-0.42179554,-0.42236515,
   -0.42293459,-0.42350387,-0.42407297,-0.42464191,-0.42521068,
   -0.42577929,-0.42634772,-0.42691599,-0.42748409,-0.42805202,
   -0.42861978,-0.42918737,-0.42975479,-0.43032205,-0.43088913,
   -0.43145604,-0.43202278,-0.43258935,-0.43315576,-0.43372199,
   -0.43428804,-0.43485393,-0.43541965,-0.43598519,-0.43655056,
   -0.43711576,-0.43768079,-0.43824564,-0.43881032,-0.43937483,
   -0.43993916,-0.44050333,-0.44106731,-0.44163112,-0.44219476,
   -0.44275823,-0.44332151,-0.44388463,-0.44444757,-0.44501033,
   -0.44557292,-0.44613533,-0.44669756,-0.44725962,-0.44782151,
   -0.44838321,-0.44894474,-0.44950609,-0.45006726,-0.45062826,
   -0.45118908,-0.45174972,-0.45231018,-0.45287046,-0.45343057,
   -0.45399049,-0.45455024,-0.45510981,-0.45566919,-0.45622840,
   -0.45678743,-0.45734628,-0.45790494,-0.45846343,-0.45902173,
   -0.45957986,-0.46013780,-0.46069556,-0.46125314,-0.46181053,
   -0.46236775,-0.46292478,-0.46348163,-0.46403829,-0.46459477,
   -0.46515107,-0.46570719,-0.46626312,-0.46681887,-0.46737443,
   -0.46792981,-0.46848500,-0.46904001,-0.46959484,-0.47014947,
   -0.47070393,-0.47125819,-0.47181227,-0.47236617,-0.47291988,
   -0.47347340,-0.47402673,-0.47457988,-0.47513284,-0.47568561,
   -0.47623820,-0.47679060,-0.47734280,-0.47789482,-0.47844665,
   -0.47899830,-0.47954975,-0.48010101,-0.48065209,-0.48120297,
   -0.48175367,-0.48230417,-0.48285449,-0.48340461,-0.48395454,
   -0.48450429,-0.48505384,-0.48560319,-0.48615236,-0.48670134,
   -0.48725012,-0.48779871,-0.48834711,-0.48889531,-0.48944333,
   -0.48999115,-0.49053877,-0.49108620,-0.49163344,-0.49218049,
   -0.49272734,-0.49327399,-0.49382045,-0.49436672,-0.49491279,
   -0.49545866,-0.49600434,-0.49654983,-0.49709511,-0.49764021,
   -0.49818510,-0.49872980,-0.49927430,-0.49981860,-0.50036271,
   -0.50090662,-0.50145033,-0.50199385,-0.50253716,-0.50308028,
   -0.50362320,-0.50416592,-0.50470844,-0.50525076,-0.50579288,
   -0.50633480,-0.50687652,-0.50741805,-0.50795937,-0.50850049,
   -0.50904141,-0.50958213,-0.51012265,-0.51066297,-0.51120308,
   -0.51174299,-0.51228271,-0.51282222,-0.51336152,-0.51390063,
   -0.51443953,-0.51497823,-0.51551672,-0.51605501,-0.51659310,
   -0.51713098,-0.51766866,-0.51820614,-0.51874341,-0.51928048,
   -0.51981734,-0.52035399,-0.52089044,-0.52142669,-0.52196273,
   -0.52249856,-0.52303419,-0.52356961,-0.52410482,-0.52463983,
   -0.52517462,-0.52570922,-0.52624360,-0.52677778,-0.52731175,
   -0.52784551,-0.52837906,-0.52891240,-0.52944554,-0.52997846,
   -0.53051118,-0.53104369,-0.53157598,-0.53210807,-0.53263995,
   -0.53317162,-0.53370307,-0.53423432,-0.53476535,-0.53529618,
   -0.53582679,-0.53635719,-0.53688738,-0.53741736,-0.53794712,
   -0.53847668,-0.53900602,-0.53953514,-0.54006406,-0.54059276,
   -0.54112125,-0.54164952,-0.54217758,-0.54270543,-0.54323306,
   -0.54376048,-0.54428768,-0.54481467,-0.54534144,-0.54586800,
   -0.54639434,-0.54692047,-0.54744638,-0.54797207,-0.54849755,
   -0.54902281,-0.54954786,-0.55007269,-0.55059730,-0.55112169,
   -0.55164587,-0.55216982,-0.55269356,-0.55321709,-0.55374039,
   -0.55426347,-0.55478634,-0.55530899,-0.55583141,-0.55635362,
   -0.55687561,-0.55739738,-0.55791893,-0.55844026,-0.55896137,
   -0.55948225,-0.56000292,-0.56052336,-0.56104359,-0.56156359,
   -0.56208337,-0.56260293,-0.56312227,-0.56364138,-0.56416028,
   -0.56467894,-0.56519739,-0.56571561,-0.56623361,-0.56675139,
   -0.56726894,-0.56778627,-0.56830338,-0.56882026,-0.56933691,
   -0.56985334,-0.57036955,-0.57088553,-0.57140129,-0.57191682,
   -0.57243212,-0.57294720,-0.57346205,-0.57397668,-0.57449107,
   -0.57500525,-0.57551919,-0.57603291,-0.57654640,-0.57705966,
   -0.57757270,-0.57808551,-0.57859808,-0.57911043,-0.57962256,
   -0.58013445,-0.58064611,-0.58115755,-0.58166875,-0.58217973,
   -0.58269047,-0.58320099,-0.58371128,-0.58422133,-0.58473116,
   -0.58524075,-0.58575011,-0.58625924,-0.58676814,-0.58727681,
   -0.58778525,-0.58829345,-0.58880142,-0.58930916,-0.58981667,
   -0.59032394,-0.59083099,-0.59133779,-0.59184437,-0.59235071,
   -0.59285682,-0.59336269,-0.59386833,-0.59437373,-0.59487890,
   -0.59538383,-0.59588853,-0.59639300,-0.59689723,-0.59740122,
   -0.59790498,-0.59840850,-0.59891178,-0.59941483,-0.59991764,
   -0.60042022,-0.60092256,-0.60142466,-0.60192652,-0.60242815,
   -0.60292954,-0.60343069,-0.60393160,-0.60443227,-0.60493271,
   -0.60543290,-0.60593286,-0.60643258,-0.60693206,-0.60743129,
   -0.60793029,-0.60842905,-0.60892757,-0.60942585,-0.60992389,
   -0.61042168,-0.61091924,-0.61141655,-0.61191363,-0.61241046,
   -0.61290705,-0.61340340,-0.61389950,-0.61439537,-0.61489099,
   -0.61538637,-0.61588150,-0.61637639,-0.61687104,-0.61736545,
   -0.61785961,-0.61835353,-0.61884720,-0.61934063,-0.61983381,
   -0.62032675,-0.62081945,-0.62131190,-0.62180410,-0.62229606,
   -0.62278778,-0.62327924,-0.62377046,-0.62426144,-0.62475217,
   -0.62524265,-0.62573289,-0.62622288,-0.62671262,-0.62720211,
   -0.62769136,-0.62818035,-0.62866910,-0.62915761,-0.62964586,
   -0.63013387,-0.63062162,-0.63110913,-0.63159639,-0.63208340,
   -0.63257016,-0.63305667,-0.63354293,-0.63402894,-0.63451470,
   -0.63500020,-0.63548546,-0.63597047,-0.63645523,-0.63693973,
   -0.63742398,-0.63790799,-0.63839174,-0.63887523,-0.63935848,
   -0.63984147,-0.64032421,-0.64080670,-0.64128894,-0.64177092,
   -0.64225265,-0.64273412,-0.64321534,-0.64369631,-0.64417702,
   -0.64465748,-0.64513769,-0.64561764,-0.64609733,-0.64657677,
   -0.64705596,-0.64753489,-0.64801356,-0.64849198,-0.64897014,
   -0.64944804,-0.64992569,-0.65040308,-0.65088022,-0.65135710,
   -0.65183372,-0.65231008,-0.65278619,-0.65326204,-0.65373763,
   -0.65421296,-0.65468804,-0.65516285,-0.65563741,-0.65611171,
   -0.65658575,-0.65705953,-0.65753305,-0.65800631,-0.65847932,
   -0.65895206,-0.65942454,-0.65989676,-0.66036872,-0.66084042,
   -0.66131186,-0.66178304,-0.66225396,-0.66272461,-0.66319500,
   -0.66366514,-0.66413501,-0.66460461,-0.66507396,-0.66554304,
   -0.66601186,-0.66648042,-0.66694871,-0.66741674,-0.66788451,
   -0.66835202,-0.66881925,-0.66928623,-0.66975294,-0.67021939,
   -0.67068557,-0.67115149,-0.67161714,-0.67208253,-0.67254765,
   -0.67301251,-0.67347710,-0.67394142,-0.67440548,-0.67486928,
   -0.67533280,-0.67579606,-0.67625906,-0.67672178,-0.67718424,
   -0.67764643,-0.67810836,-0.67857001,-0.67903140,-0.67949252,
   -0.67995337,-0.68041396,-0.68087427,-0.68133432,-0.68179410,
   -0.68225360,-0.68271284,-0.68317181,-0.68363051,-0.68408894,
   -0.68454710,-0.68500499,-0.68546261,-0.68591996,-0.68637703,
   -0.68683384,-0.68729038,-0.68774664,-0.68820263,-0.68865835,
   -0.68911380,-0.68956898,-0.69002388,-0.69047852,-0.69093288,
   -0.69138696,-0.69184078,-0.69229432,-0.69274759,-0.69320058,
   -0.69365330,-0.69410575,-0.69455792,-0.69500982,-0.69546144,
   -0.69591279,-0.69636387,-0.69681467,-0.69726519,-0.69771544,
   -0.69816541,-0.69861511,-0.69906453,-0.69951368,-0.69996255,
   -0.70041115,-0.70085946,-0.70130750,-0.70175527,-0.70220275,
   -0.70264996,-0.70309690,-0.70354355,-0.70398993,-0.70443603,
   -0.70488185,-0.70532739,-0.70577266,-0.70621764,-0.70666235,
   -0.70710678,-0.70755092,-0.70799479,-0.70843838,-0.70888169,
   -0.70932472,-0.70976747,-0.71020994,-0.71065213,-0.71109404,
   -0.71153567,-0.71197702,-0.71241809,-0.71285887,-0.71329937,
   -0.71373960,-0.71417954,-0.71461920,-0.71505857,-0.71549767,
   -0.71593648,-0.71637501,-0.71681325,-0.71725122,-0.71768890,
   -0.71812629,-0.71856341,-0.71900024,-0.71943678,-0.71987304,
   -0.72030902,-0.72074471,-0.72118012,-0.72161525,-0.72205008,
   -0.72248464,-0.72291891,-0.72335289,-0.72378659,-0.72422000,
   -0.72465313,-0.72508596,-0.72551852,-0.72595079,-0.72638277,
   -0.72681446,-0.72724587,-0.72767699,-0.72810782,-0.72853836,
   -0.72896862,-0.72939859,-0.72982827,-0.73025767,-0.73068677,
   -0.73111559,-0.73154412,-0.73197236,-0.73240031,-0.73282797,
   -0.73325534,-0.73368242,-0.73410922,-0.73453572,-0.73496193,
   -0.73538786,-0.73581349,-0.73623883,-0.73666388,-0.73708864,
   -0.73751311,-0.73793729,-0.73836118,-0.73878477,-0.73920808,
   -0.73963109,-0.74005381,-0.74047624,-0.74089837,-0.74132022,
   -0.74174177,-0.74216303,-0.74258399,-0.74300466,-0.74342504,
   -0.74384512,-0.74426492,-0.74468441,-0.74510362,-0.74552253,
   -0.74594114,-0.74635946,-0.74677749,-0.74719522,-0.74761265,
   -0.74802979,-0.74844664,-0.74886319,-0.74927944,-0.74969540,
   -0.75011106,-0.75052643,-0.75094150,-0.75135627,-0.75177075,
   -0.75218493,-0.75259882,-0.75301240,-0.75342569,-0.75383868,
   -0.75425138,-0.75466377,-0.75507587,-0.75548767,-0.75589917,
   -0.75631038,-0.75672128,-0.75713189,-0.75754219,-0.75795220,
   -0.75836191,-0.75877132,-0.75918043,-0.75958924,-0.75999775,
   -0.76040596,-0.76081387,-0.76122148,-0.76162879,-0.76203580,
   -0.76244251,-0.76284891,-0.76325502,-0.76366082,-0.76406633,
   -0.76447153,-0.76487643,-0.76528102,-0.76568532,-0.76608931,
   -0.76649300,-0.76689639,-0.76729947,-0.76770226,-0.76810474,
   -0.76850691,-0.76890878,-0.76931035,-0.76971162,-0.77011258,
   -0.77051324,-0.77091359,-0.77131364,-0.77171338,-0.77211282,
   -0.77251196,-0.77291079,-0.77330931,-0.77370753,-0.77410545,
   -0.77450306,-0.77490036,-0.77529735,-0.77569405,-0.77609043,
   -0.77648651,-0.77688228,-0.77727774,-0.77767290,-0.77806775,
   -0.77846230,-0.77885653,-0.77925046,-0.77964408,-0.78003740,
   -0.78043040,-0.78082310,-0.78121549,-0.78160757,-0.78199934,
   -0.78239081,-0.78278196,-0.78317281,-0.78356334,-0.78395357,
   -0.78434349,-0.78473309,-0.78512239,-0.78551138,-0.78590006,
   -0.78628843,-0.78667648,-0.78706423,-0.78745167,-0.78783879,
   -0.78822561,-0.78861211,-0.78899830,-0.78938418,-0.78976975,
   -0.79015501,-0.79053995,-0.79092458,-0.79130891,-0.79169291,
   -0.79207661,-0.79245999,-0.79284306,-0.79322582,-0.79360826,
   -0.79399039,-0.79437221,-0.79475371,-0.79513490,-0.79551578,
   -0.79589634,-0.79627659,-0.79665652,-0.79703614,-0.79741545,
   -0.79779443,-0.79817311,-0.79855147,-0.79892951,-0.79930724,
   -0.79968465,-0.80006175,-0.80043853,-0.80081500,-0.80119115,
   -0.80156698,-0.80194250,-0.80231770,-0.80269258,-0.80306715,
   -0.80344140,-0.80381533,-0.80418894,-0.80456224,-0.80493522,
   -0.80530788,-0.80568022,-0.80605225,-0.80642396,-0.80679535,
   -0.80716642,-0.80753717,-0.80790760,-0.80827772,-0.80864751,
   -0.80901699,-0.80938615,-0.80975498,-0.81012350,-0.81049170,
   -0.81085958,-0.81122713,-0.81159437,-0.81196129,-0.81232788,
   -0.81269416,-0.81306011,-0.81342575,-0.81379106,-0.81415605,
   -0.81452072,-0.81488507,-0.81524910,-0.81561280,-0.81597618,
   -0.81633925,-0.81670198,-0.81706440,-0.81742649,-0.81778826,
   -0.81814971,-0.81851084,-0.81887164,-0.81923212,-0.81959227,
   -0.81995210,-0.82031161,-0.82067080,-0.82102966,-0.82138819,
   -0.82174640,-0.82210429,-0.82246185,-0.82281909,-0.82317600,
   -0.82353259,-0.82388886,-0.82424479,-0.82460041,-0.82495569,
   -0.82531065,-0.82566529,-0.82601960,-0.82637358,-0.82672724,
   -0.82708057,-0.82743357,-0.82778625,-0.82813860,-0.82849063,
   -0.82884232,-0.82919369,-0.82954473,-0.82989545,-0.83024583,
   -0.83059589,-0.83094563,-0.83129503,-0.83164410,-0.83199285,
   -0.83234127,-0.83268936,-0.83303712,-0.83338455,-0.83373165,
   -0.83407843,-0.83442487,-0.83477099,-0.83511677,-0.83546223,
   -0.83580736,-0.83615215,-0.83649662,-0.83684075,-0.83718456,
   -0.83752803,-0.83787118,-0.83821399,-0.83855648,-0.83889863,
   -0.83924045,-0.83958194,-0.83992309,-0.84026392,-0.84060441,
   -0.84094458,-0.84128441,-0.84162391,-0.84196307,-0.84230191,
   -0.84264041,-0.84297858,-0.84331641,-0.84365391,-0.84399108,
   -0.84432792,-0.84466442,-0.84500059,-0.84533643,-0.84567193,
   -0.84600710,-0.84634194,-0.84667644,-0.84701060,-0.84734443,
   -0.84767793,-0.84801109,-0.84834392,-0.84867641,-0.84900857,
   -0.84934040,-0.84967188,-0.85000303,-0.85033385,-0.85066433,
   -0.85099448,-0.85132429,-0.85165376,-0.85198290,-0.85231170,
   -0.85264016,-0.85296829,-0.85329608,-0.85362353,-0.85395065,
   -0.85427743,-0.85460387,-0.85492997,-0.85525574,-0.85558117,
   -0.85590626,-0.85623102,-0.85655543,-0.85687951,-0.85720325,
   -0.85752665,-0.85784971,-0.85817244,-0.85849482,-0.85881687,
   -0.85913858,-0.85945994,-0.85978097,-0.86010166,-0.86042201,
   -0.86074202,-0.86106169,-0.86138102,-0.86170001,-0.86201866,
   -0.86233697,-0.86265494,-0.86297257,-0.86328986,-0.86360681,
   -0.86392341,-0.86423968,-0.86455560,-0.86487118,-0.86518643,
   -0.86550133,-0.86581588,-0.86613010,-0.86644397,-0.86675751,
   -0.86707070,-0.86738354,-0.86769605,-0.86800821,-0.86832003,
   -0.86863151,-0.86894264,-0.86925344,-0.86956388,-0.86987399,
   -0.87018375,-0.87049317,-0.87080224,-0.87111097,-0.87141936,
   -0.87172740,-0.87203510,-0.87234245,-0.87264946,-0.87295613,
   -0.87326245,-0.87356843,-0.87387406,-0.87417934,-0.87448428,
   -0.87478888,-0.87509313,-0.87539703,-0.87570059,-0.87600381,
   -0.87630667,-0.87660920,-0.87691137,-0.87721320,-0.87751469,
   -0.87781582,-0.87811661,-0.87841706,-0.87871715,-0.87901690,
   -0.87931631,-0.87961536,-0.87991407,-0.88021243,-0.88051044,
   -0.88080811,-0.88110543,-0.88140240,-0.88169902,-0.88199530,
   -0.88229122,-0.88258680,-0.88288203,-0.88317691,-0.88347144,
   -0.88376563,-0.88405946,-0.88435295,-0.88464608,-0.88493887,
   -0.88523131,-0.88552339,-0.88581513,-0.88610652,-0.88639756,
   -0.88668825,-0.88697859,-0.88726858,-0.88755822,-0.88784751,
   -0.88813644,-0.88842503,-0.88871327,-0.88900115,-0.88928869,
   -0.88957587,-0.88986270,-0.89014918,-0.89043531,-0.89072109,
   -0.89100652,-0.89129159,-0.89157632,-0.89186069,-0.89214471,
   -0.89242837,-0.89271169,-0.89299465,-0.89327726,-0.89355952,
   -0.89384142,-0.89412297,-0.89440417,-0.89468501,-0.89496550,
   -0.89524564,-0.89552543,-0.89580486,-0.89608394,-0.89636266,
   -0.89664103,-0.89691905,-0.89719671,-0.89747402,-0.89775097,
   -0.89802757,-0.89830382,-0.89857971,-0.89885524,-0.89913042,
   -0.89940525,-0.89967972,-0.89995383,-0.90022759,-0.90050100,
   -0.90077405,-0.90104674,-0.90131908,-0.90159106,-0.90186268,
   -0.90213395,-0.90240487,-0.90267543,-0.90294563,-0.90321547,
   -0.90348496,-0.90375409,-0.90402287,-0.90429128,-0.90455934,
   -0.90482705,-0.90509439,-0.90536138,-0.90562801,-0.90589429,
   -0.90616021,-0.90642576,-0.90669096,-0.90695581,-0.90722029,
   -0.90748442,-0.90774819,-0.90801160,-0.90827465,-0.90853734,
   -0.90879968,-0.90906165,-0.90932327,-0.90958453,-0.90984543,
   -0.91010597,-0.91036615,-0.91062597,-0.91088543,-0.91114453,
   -0.91140327,-0.91166165,-0.91191968,-0.91217734,-0.91243464,
   -0.91269158,-0.91294816,-0.91320439,-0.91346025,-0.91371575,
   -0.91397089,-0.91422566,-0.91448008,-0.91473414,-0.91498783,
   -0.91524117,-0.91549414,-0.91574675,-0.91599900,-0.91625089,
   -0.91650242,-0.91675358,-0.91700438,-0.91725483,-0.91750490,
   -0.91775462,-0.91800397,-0.91825297,-0.91850160,-0.91874986,
   -0.91899777,-0.91924531,-0.91949249,-0.91973930,-0.91998575,
   -0.92023184,-0.92047757,-0.92072293,-0.92096793,-0.92121256,
   -0.92145684,-0.92170074,-0.92194429,-0.92218747,-0.92243028,
   -0.92267273,-0.92291482,-0.92315655,-0.92339790,-0.92363890,
   -0.92387953,-0.92411979,-0.92435969,-0.92459923,-0.92483840,
   -0.92507720,-0.92531564,-0.92555372,-0.92579142,-0.92602877,
   -0.92626575,-0.92650236,-0.92673860,-0.92697449,-0.92721000,
   -0.92744515,-0.92767993,-0.92791435,-0.92814840,-0.92838208,
   -0.92861540,-0.92884835,-0.92908093,-0.92931315,-0.92954500,
   -0.92977648,-0.93000760,-0.93023835,-0.93046873,-0.93069874,
   -0.93092839,-0.93115767,-0.93138658,-0.93161512,-0.93184330,
   -0.93207111,-0.93229855,-0.93252562,-0.93275232,-0.93297866,
   -0.93320463,-0.93343023,-0.93365546,-0.93388032,-0.93410481,
   -0.93432894,-0.93455269,-0.93477608,-0.93499910,-0.93522175,
   -0.93544403,-0.93566594,-0.93588748,-0.93610865,-0.93632945,
   -0.93654988,-0.93676994,-0.93698964,-0.93720896,-0.93742791,
   -0.93764649,-0.93786471,-0.93808255,-0.93830002,-0.93851712,
   -0.93873385,-0.93895021,-0.93916620,-0.93938182,-0.93959707,
   -0.93981195,-0.94002645,-0.94024059,-0.94045435,-0.94066774,
   -0.94088076,-0.94109341,-0.94130569,-0.94151760,-0.94172913,
   -0.94194030,-0.94215109,-0.94236151,-0.94257155,-0.94278123,
   -0.94299053,-0.94319946,-0.94340802,-0.94361621,-0.94382402,
   -0.94403146,-0.94423853,-0.94444522,-0.94465154,-0.94485749,
   -0.94506307,-0.94526827,-0.94547310,-0.94567756,-0.94588164,
   -0.94608535,-0.94628869,-0.94649165,-0.94669424,-0.94689646,
   -0.94709830,-0.94729977,-0.94750086,-0.94770158,-0.94790193,
   -0.94810190,-0.94830150,-0.94850072,-0.94869957,-0.94889804,
   -0.94909614,-0.94929386,-0.94949121,-0.94968819,-0.94988479,
   -0.95008101,-0.95027686,-0.95047234,-0.95066744,-0.95086216,
   -0.95105651,-0.95125048,-0.95144408,-0.95163730,-0.95183015,
   -0.95202262,-0.95221472,-0.95240644,-0.95259778,-0.95278875,
   -0.95297934,-0.95316955,-0.95335939,-0.95354885,-0.95373794,
   -0.95392665,-0.95411498,-0.95430293,-0.95449051,-0.95467771,
   -0.95486454,-0.95505099,-0.95523706,-0.95542275,-0.95560807,
   -0.95579301,-0.95597757,-0.95616176,-0.95634556,-0.95652899,
   -0.95671205,-0.95689472,-0.95707702,-0.95725894,-0.95744048,
   -0.95762164,-0.95780243,-0.95798283,-0.95816286,-0.95834251,
   -0.95852178,-0.95870068,-0.95887919,-0.95905733,-0.95923509,
   -0.95941247,-0.95958947,-0.95976609,-0.95994233,-0.96011820,
   -0.96029368,-0.96046879,-0.96064351,-0.96081786,-0.96099183,
   -0.96116542,-0.96133863,-0.96151146,-0.96168391,-0.96185598,
   -0.96202767,-0.96219898,-0.96236991,-0.96254046,-0.96271063,
   -0.96288042,-0.96304983,-0.96321886,-0.96338751,-0.96355578,
   -0.96372367,-0.96389118,-0.96405831,-0.96422506,-0.96439143,
   -0.96455741,-0.96472302,-0.96488824,-0.96505309,-0.96521755,
   -0.96538163,-0.96554533,-0.96570865,-0.96587159,-0.96603415,
   -0.96619633,-0.96635812,-0.96651953,-0.96668056,-0.96684121,
   -0.96700148,-0.96716137,-0.96732087,-0.96748000,-0.96763874,
   -0.96779710,-0.96795507,-0.96811267,-0.96826988,-0.96842671,
   -0.96858316,-0.96873922,-0.96889490,-0.96905020,-0.96920512,
   -0.96935966,-0.96951381,-0.96966758,-0.96982097,-0.96997397,
   -0.97012659,-0.97027883,-0.97043068,-0.97058216,-0.97073325,
   -0.97088395,-0.97103427,-0.97118421,-0.97133377,-0.97148294,
   -0.97163173,-0.97178013,-0.97192815,-0.97207579,-0.97222305,
   -0.97236992,-0.97251640,-0.97266250,-0.97280822,-0.97295356,
   -0.97309851,-0.97324307,-0.97338725,-0.97353105,-0.97367446,
   -0.97381749,-0.97396014,-0.97410240,-0.97424427,-0.97438576,
   -0.97452687,-0.97466759,-0.97480793,-0.97494788,-0.97508744,
   -0.97522662,-0.97536542,-0.97550383,-0.97564186,-0.97577950,
   -0.97591676,-0.97605363,-0.97619011,-0.97632621,-0.97646193,
   -0.97659726,-0.97673220,-0.97686676,-0.97700093,-0.97713472,
   -0.97726812,-0.97740113,-0.97753376,-0.97766601,-0.97779786,
   -0.97792933,-0.97806042,-0.97819112,-0.97832143,-0.97845136,
   -0.97858090,-0.97871005,-0.97883882,-0.97896720,-0.97909520,
   -0.97922281,-0.97935003,-0.97947686,-0.97960331,-0.97972937,
   -0.97985505,-0.97998034,-0.98010524,-0.98022975,-0.98035388,
   -0.98047762,-0.98060097,-0.98072394,-0.98084652,-0.98096871,
   -0.98109051,-0.98121193,-0.98133296,-0.98145360,-0.98157386,
   -0.98169372,-0.98181320,-0.98193230,-0.98205100,-0.98216932,
   -0.98228725,-0.98240479,-0.98252194,-0.98263871,-0.98275508,
   -0.98287107,-0.98298667,-0.98310189,-0.98321671,-0.98333115,
   -0.98344520,-0.98355886,-0.98367213,-0.98378502,-0.98389751,
   -0.98400962,-0.98412134,-0.98423267,-0.98434361,-0.98445416,
   -0.98456433,-0.98467411,-0.98478349,-0.98489249,-0.98500110,
   -0.98510932,-0.98521715,-0.98532460,-0.98543165,-0.98553831,
   -0.98564459,-0.98575048,-0.98585597,-0.98596108,-0.98606580,
   -0.98617013,-0.98627407,-0.98637762,-0.98648078,-0.98658356,
   -0.98668594,-0.98678793,-0.98688954,-0.98699075,-0.98709157,
   -0.98719201,-0.98729205,-0.98739171,-0.98749097,-0.98758985,
   -0.98768834,-0.98778643,-0.98788414,-0.98798145,-0.98807838,
   -0.98817491,-0.98827106,-0.98836681,-0.98846218,-0.98855716,
   -0.98865174,-0.98874593,-0.98883974,-0.98893315,-0.98902617,
   -0.98911881,-0.98921105,-0.98930290,-0.98939436,-0.98948543,
   -0.98957611,-0.98966640,-0.98975630,-0.98984581,-0.98993493,
   -0.99002365,-0.99011199,-0.99019993,-0.99028749,-0.99037465,
   -0.99046142,-0.99054780,-0.99063379,-0.99071939,-0.99080460,
   -0.99088941,-0.99097384,-0.99105787,-0.99114151,-0.99122477,
   -0.99130763,-0.99139009,-0.99147217,-0.99155386,-0.99163515,
   -0.99171606,-0.99179657,-0.99187669,-0.99195641,-0.99203575,
   -0.99211470,-0.99219325,-0.99227141,-0.99234918,-0.99242656,
   -0.99250355,-0.99258014,-0.99265634,-0.99273215,-0.99280757,
   -0.99288260,-0.99295723,-0.99303148,-0.99310533,-0.99317879,
   -0.99325185,-0.99332453,-0.99339681,-0.99346870,-0.99354020,
   -0.99361131,-0.99368202,-0.99375234,-0.99382227,-0.99389181,
   -0.99396095,-0.99402970,-0.99409806,-0.99416603,-0.99423360,
   -0.99430079,-0.99436757,-0.99443397,-0.99449998,-0.99456559,
   -0.99463081,-0.99469563,-0.99476007,-0.99482411,-0.99488776,
   -0.99495101,-0.99501387,-0.99507634,-0.99513842,-0.99520011,
   -0.99526140,-0.99532230,-0.99538280,-0.99544291,-0.99550263,
   -0.99556196,-0.99562089,-0.99567943,-0.99573758,-0.99579534,
   -0.99585270,-0.99590966,-0.99596624,-0.99602242,-0.99607821,
   -0.99613360,-0.99618861,-0.99624321,-0.99629743,-0.99635125,
   -0.99640468,-0.99645772,-0.99651036,-0.99656261,-0.99661446,
   -0.99666592,-0.99671699,-0.99676767,-0.99681795,-0.99686783,
   -0.99691733,-0.99696643,-0.99701514,-0.99706345,-0.99711137,
   -0.99715890,-0.99720603,-0.99725277,-0.99729911,-0.99734506,
   -0.99739062,-0.99743578,-0.99748055,-0.99752493,-0.99756891,
   -0.99761250,-0.99765570,-0.99769850,-0.99774090,-0.99778292,
   -0.99782454,-0.99786576,-0.99790659,-0.99794703,-0.99798707,
   -0.99802672,-0.99806598,-0.99810484,-0.99814331,-0.99818138,
   -0.99821906,-0.99825635,-0.99829324,-0.99832973,-0.99836584,
   -0.99840155,-0.99843686,-0.99847178,-0.99850631,-0.99854044,
   -0.99857418,-0.99860752,-0.99864047,-0.99867302,-0.99870519,
   -0.99873695,-0.99876832,-0.99879930,-0.99882989,-0.99886007,
   -0.99888987,-0.99891927,-0.99894828,-0.99897689,-0.99900511,
   -0.99903293,-0.99906036,-0.99908739,-0.99911403,-0.99914028,
   -0.99916613,-0.99919159,-0.99921665,-0.99924132,-0.99926559,
   -0.99928947,-0.99931295,-0.99933604,-0.99935874,-0.99938104,
   -0.99940294,-0.99942445,-0.99944557,-0.99946629,-0.99948662,
   -0.99950656,-0.99952609,-0.99954524,-0.99956399,-0.99958234,
   -0.99960030,-0.99961787,-0.99963504,-0.99965182,-0.99966820,
   -0.99968418,-0.99969978,-0.99971497,-0.99972978,-0.99974419,
   -0.99975820,-0.99977182,-0.99978504,-0.99979787,-0.99981031,
   -0.99982235,-0.99983399,-0.99984524,-0.99985610,-0.99986656,
   -0.99987663,-0.99988630,-0.99989558,-0.99990446,-0.99991295,
   -0.99992104,-0.99992874,-0.99993604,-0.99994295,-0.99994946,
   -0.99995558,-0.99996131,-0.99996664,-0.99997157,-0.99997611,
   -0.99998026,-0.99998401,-0.99998736,-0.99999032,-0.99999289,
   -0.99999506,-0.99999684,-0.99999822,-0.99999921,-0.99999980,

// _COS_VALUES_ starts here, at offset 2500

   -1.00000000,-0.99999980,-0.99999921,-0.99999822,-0.99999684,
   -0.99999506,-0.99999289,-0.99999032,-0.99998736,-0.99998401,
   -0.99998026,-0.99997611,-0.99997157,-0.99996664,-0.99996131,
   -0.99995558,-0.99994946,-0.99994295,-0.99993604,-0.99992874,
   -0.99992104,-0.99991295,-0.99990446,-0.99989558,-0.99988630,
   -0.99987663,-0.99986656,-0.99985610,-0.99984524,-0.99983399,
   -0.99982235,-0.99981031,-0.99979787,-0.99978504,-0.99977182,
   -0.99975820,-0.99974419,-0.99972978,-0.99971497,-0.99969978,
   -0.99968418,-0.99966820,-0.99965182,-0.99963504,-0.99961787,
   -0.99960030,-0.99958234,-0.99956399,-0.99954524,-0.99952609,
   -0.99950656,-0.99948662,-0.99946629,-0.99944557,-0.99942445,
   -0.99940294,-0.99938104,-0.99935874,-0.99933604,-0.99931295,
   -0.99928947,-0.99926559,-0.99924132,-0.99921665,-0.99919159,
   -0.99916613,-0.99914028,-0.99911403,-0.99908739,-0.99906036,
   -0.99903293,-0.99900511,-0.99897689,-0.99894828,-0.99891927,
   -0.99888987,-0.99886007,-0.99882989,-0.99879930,-0.99876832,
   -0.99873695,-0.99870519,-0.99867302,-0.99864047,-0.99860752,
   -0.99857418,-0.99854044,-0.99850631,-0.99847178,-0.99843686,
   -0.99840155,-0.99836584,-0.99832973,-0.99829324,-0.99825635,
   -0.99821906,-0.99818138,-0.99814331,-0.99810484,-0.99806598,
   -0.99802672,-0.99798707,-0.99794703,-0.99790659,-0.99786576,
   -0.99782454,-0.99778292,-0.99774090,-0.99769850,-0.99765570,
   -0.99761250,-0.99756891,-0.99752493,-0.99748055,-0.99743578,
   -0.99739062,-0.99734506,-0.99729911,-0.99725277,-0.99720603,
   -0.99715890,-0.99711137,-0.99706345,-0.99701514,-0.99696643,
   -0.99691733,-0.99686783,-0.99681795,-0.99676767,-0.99671699,
   -0.99666592,-0.99661446,-0.99656261,-0.99651036,-0.99645772,
   -0.99640468,-0.99635125,-0.99629743,-0.99624321,-0.99618861,
   -0.99613360,-0.99607821,-0.99602242,-0.99596624,-0.99590966,
   -0.99585270,-0.99579534,-0.99573758,-0.99567943,-0.99562089,
   -0.99556196,-0.99550263,-0.99544291,-0.99538280,-0.99532230,
   -0.99526140,-0.99520011,-0.99513842,-0.99507634,-0.99501387,
   -0.99495101,-0.99488776,-0.99482411,-0.99476007,-0.99469563,
   -0.99463081,-0.99456559,-0.99449998,-0.99443397,-0.99436757,
   -0.99430079,-0.99423360,-0.99416603,-0.99409806,-0.99402970,
   -0.99396095,-0.99389181,-0.99382227,-0.99375234,-0.99368202,
   -0.99361131,-0.99354020,-0.99346870,-0.99339681,-0.99332453,
   -0.99325185,-0.99317879,-0.99310533,-0.99303148,-0.99295723,
   -0.99288260,-0.99280757,-0.99273215,-0.99265634,-0.99258014,
   -0.99250355,-0.99242656,-0.99234918,-0.99227141,-0.99219325,
   -0.99211470,-0.99203575,-0.99195641,-0.99187669,-0.99179657,
   -0.99171606,-0.99163515,-0.99155386,-0.99147217,-0.99139009,
   -0.99130763,-0.99122477,-0.99114151,-0.99105787,-0.99097384,
   -0.99088941,-0.99080460,-0.99071939,-0.99063379,-0.99054780,
   -0.99046142,-0.99037465,-0.99028749,-0.99019993,-0.99011199,
   -0.99002365,-0.98993493,-0.98984581,-0.98975630,-0.98966640,
   -0.98957611,-0.98948543,-0.98939436,-0.98930290,-0.98921105,
   -0.98911881,-0.98902618,-0.98893315,-0.98883974,-0.98874593,
   -0.98865174,-0.98855716,-0.98846218,-0.98836682,-0.98827106,
   -0.98817491,-0.98807838,-0.98798145,-0.98788414,-0.98778643,
   -0.98768834,-0.98758985,-0.98749097,-0.98739171,-0.98729205,
   -0.98719201,-0.98709157,-0.98699075,-0.98688954,-0.98678793,
   -0.98668594,-0.98658356,-0.98648078,-0.98637762,-0.98627407,
   -0.98617013,-0.98606580,-0.98596108,-0.98585597,-0.98575048,
   -0.98564459,-0.98553831,-0.98543165,-0.98532460,-0.98521715,
   -0.98510932,-0.98500110,-0.98489249,-0.98478349,-0.98467411,
   -0.98456433,-0.98445416,-0.98434361,-0.98423267,-0.98412134,
   -0.98400962,-0.98389751,-0.98378502,-0.98367213,-0.98355886,
   -0.98344520,-0.98333115,-0.98321671,-0.98310189,-0.98298667,
   -0.98287107,-0.98275508,-0.98263871,-0.98252194,-0.98240479,
   -0.98228725,-0.98216932,-0.98205100,-0.98193230,-0.98181320,
   -0.98169372,-0.98157386,-0.98145360,-0.98133296,-0.98121193,
   -0.98109051,-0.98096871,-0.98084652,-0.98072394,-0.98060097,
   -0.98047762,-0.98035388,-0.98022975,-0.98010524,-0.97998034,
   -0.97985505,-0.97972937,-0.97960331,-0.97947686,-0.97935003,
   -0.97922281,-0.97909520,-0.97896720,-0.97883882,-0.97871005,
   -0.97858090,-0.97845136,-0.97832143,-0.97819112,-0.97806042,
   -0.97792933,-0.97779786,-0.97766601,-0.97753376,-0.97740113,
   -0.97726812,-0.97713472,-0.97700093,-0.97686676,-0.97673220,
   -0.97659726,-0.97646193,-0.97632621,-0.97619011,-0.97605363,
   -0.97591676,-0.97577950,-0.97564186,-0.97550383,-0.97536542,
   -0.97522662,-0.97508744,-0.97494788,-0.97480793,-0.97466759,
   -0.97452687,-0.97438576,-0.97424427,-0.97410240,-0.97396014,
   -0.97381749,-0.97367446,-0.97353105,-0.97338725,-0.97324307,
   -0.97309851,-0.97295356,-0.97280822,-0.97266250,-0.97251640,
   -0.97236992,-0.97222305,-0.97207579,-0.97192815,-0.97178013,
   -0.97163173,-0.97148294,-0.97133377,-0.97118421,-0.97103427,
   -0.97088395,-0.97073325,-0.97058216,-0.97043068,-0.97027883,
   -0.97012659,-0.96997397,-0.96982097,-0.96966758,-0.96951381,
   -0.96935966,-0.96920512,-0.96905020,-0.96889490,-0.96873922,
   -0.96858316,-0.96842671,-0.96826988,-0.96811267,-0.96795507,
   -0.96779710,-0.96763874,-0.96748000,-0.96732087,-0.96716137,
   -0.96700148,-0.96684121,-0.96668057,-0.96651953,-0.96635812,
   -0.96619633,-0.96603415,-0.96587159,-0.96570865,-0.96554533,
   -0.96538163,-0.96521755,-0.96505309,-0.96488824,-0.96472302,
   -0.96455741,-0.96439143,-0.96422506,-0.96405831,-0.96389118,
   -0.96372367,-0.96355578,-0.96338751,-0.96321886,-0.96304983,
   -0.96288042,-0.96271063,-0.96254046,-0.96236991,-0.96219898,
   -0.96202767,-0.96185598,-0.96168391,-0.96151146,-0.96133863,
   -0.96116542,-0.96099183,-0.96081786,-0.96064351,-0.96046879,
   -0.96029368,-0.96011820,-0.95994233,-0.95976609,-0.95958947,
   -0.95941247,-0.95923509,-0.95905733,-0.95887919,-0.95870068,
   -0.95852178,-0.95834251,-0.95816286,-0.95798283,-0.95780243,
   -0.95762164,-0.95744048,-0.95725894,-0.95707702,-0.95689472,
   -0.95671205,-0.95652899,-0.95634556,-0.95616176,-0.95597757,
   -0.95579301,-0.95560807,-0.95542275,-0.95523706,-0.95505099,
   -0.95486454,-0.95467771,-0.95449051,-0.95430293,-0.95411498,
   -0.95392665,-0.95373794,-0.95354885,-0.95335939,-0.95316955,
   -0.95297934,-0.95278875,-0.95259778,-0.95240644,-0.95221472,
   -0.95202262,-0.95183015,-0.95163730,-0.95144408,-0.95125048,
   -0.95105651,-0.95086216,-0.95066744,-0.95047234,-0.95027686,
   -0.95008101,-0.94988479,-0.94968819,-0.94949121,-0.94929386,
   -0.94909614,-0.94889804,-0.94869957,-0.94850072,-0.94830150,
   -0.94810190,-0.94790193,-0.94770158,-0.94750086,-0.94729977,
   -0.94709830,-0.94689646,-0.94669424,-0.94649165,-0.94628869,
   -0.94608535,-0.94588164,-0.94567756,-0.94547310,-0.94526827,
   -0.94506307,-0.94485749,-0.94465154,-0.94444522,-0.94423853,
   -0.94403146,-0.94382402,-0.94361621,-0.94340802,-0.94319946,
   -0.94299053,-0.94278123,-0.94257155,-0.94236151,-0.94215109,
   -0.94194030,-0.94172913,-0.94151760,-0.94130569,-0.94109341,
   -0.94088076,-0.94066774,-0.94045435,-0.94024059,-0.94002645,
   -0.93981195,-0.93959707,-0.93938182,-0.93916620,-0.93895021,
   -0.93873385,-0.93851712,-0.93830002,-0.93808255,-0.93786471,
   -0.93764649,-0.93742791,-0.93720896,-0.93698964,-0.93676994,
   -0.93654988,-0.93632945,-0.93610865,-0.93588748,-0.93566594,
   -0.93544403,-0.93522175,-0.93499910,-0.93477608,-0.93455269,
   -0.93432894,-0.93410481,-0.93388032,-0.93365546,-0.93343023,
   -0.93320463,-0.93297866,-0.93275232,-0.93252562,-0.93229855,
   -0.93207111,-0.93184330,-0.93161512,-0.93138658,-0.93115767,
   -0.93092839,-0.93069874,-0.93046873,-0.93023835,-0.93000760,
   -0.92977648,-0.92954500,-0.92931315,-0.92908093,-0.92884835,
   -0.92861540,-0.92838208,-0.92814840,-0.92791435,-0.92767993,
   -0.92744515,-0.92721000,-0.92697449,-0.92673860,-0.92650236,
   -0.92626575,-0.92602877,-0.92579142,-0.92555372,-0.92531564,
   -0.92507720,-0.92483840,-0.92459923,-0.92435969,-0.92411979,
   -0.92387953,-0.92363890,-0.92339790,-0.92315655,-0.92291482,
   -0.92267273,-0.92243028,-0.92218747,-0.92194429,-0.92170074,
   -0.92145684,-0.92121256,-0.92096793,-0.92072293,-0.92047757,
   -0.92023184,-0.91998575,-0.91973930,-0.91949249,-0.91924531,
   -0.91899777,-0.91874986,-0.91850160,-0.91825297,-0.91800397,
   -0.91775462,-0.91750490,-0.91725483,-0.91700438,-0.91675358,
   -0.91650242,-0.91625089,-0.91599900,-0.91574675,-0.91549414,
   -0.91524117,-0.91498783,-0.91473414,-0.91448008,-0.91422566,
   -0.91397089,-0.91371575,-0.91346025,-0.91320439,-0.91294816,
   -0.91269158,-0.91243464,-0.91217734,-0.91191968,-0.91166165,
   -0.91140327,-0.91114453,-0.91088543,-0.91062597,-0.91036615,
   -0.91010597,-0.90984543,-0.90958453,-0.90932327,-0.90906165,
   -0.90879968,-0.90853734,-0.90827465,-0.90801160,-0.90774819,
   -0.90748442,-0.90722029,-0.90695581,-0.90669096,-0.90642576,
   -0.90616021,-0.90589429,-0.90562801,-0.90536138,-0.90509439,
   -0.90482705,-0.90455934,-0.90429128,-0.90402287,-0.90375409,
   -0.90348496,-0.90321547,-0.90294563,-0.90267543,-0.90240487,
   -0.90213395,-0.90186268,-0.90159106,-0.90131908,-0.90104674,
   -0.90077405,-0.90050100,-0.90022759,-0.89995383,-0.89967972,
   -0.89940525,-0.89913042,-0.89885524,-0.89857971,-0.89830382,
   -0.89802757,-0.89775097,-0.89747402,-0.89719671,-0.89691905,
   -0.89664103,-0.89636266,-0.89608394,-0.89580486,-0.89552543,
   -0.89524564,-0.89496550,-0.89468501,-0.89440417,-0.89412297,
   -0.89384142,-0.89355952,-0.89327726,-0.89299465,-0.89271169,
   -0.89242837,-0.89214471,-0.89186069,-0.89157632,-0.89129159,
   -0.89100652,-0.89072109,-0.89043531,-0.89014918,-0.88986270,
   -0.88957587,-0.88928869,-0.88900115,-0.88871327,-0.88842503,
   -0.88813644,-0.88784751,-0.88755822,-0.88726858,-0.88697859,
   -0.88668825,-0.88639756,-0.88610652,-0.88581513,-0.88552339,
   -0.88523131,-0.88493887,-0.88464608,-0.88435295,-0.88405946,
   -0.88376563,-0.88347144,-0.88317691,-0.88288203,-0.88258680,
   -0.88229122,-0.88199530,-0.88169902,-0.88140240,-0.88110543,
   -0.88080811,-0.88051044,-0.88021243,-0.87991407,-0.87961536,
   -0.87931631,-0.87901690,-0.87871715,-0.87841706,-0.87811661,
   -0.87781582,-0.87751469,-0.87721320,-0.87691137,-0.87660920,
   -0.87630668,-0.87600381,-0.87570059,-0.87539703,-0.87509313,
   -0.87478888,-0.87448428,-0.87417934,-0.87387406,-0.87356843,
   -0.87326245,-0.87295613,-0.87264946,-0.87234245,-0.87203510,
   -0.87172740,-0.87141936,-0.87111097,-0.87080224,-0.87049317,
   -0.87018375,-0.86987399,-0.86956388,-0.86925344,-0.86894264,
   -0.86863151,-0.86832003,-0.86800821,-0.86769605,-0.86738354,
   -0.86707070,-0.86675751,-0.86644397,-0.86613010,-0.86581588,
   -0.86550133,-0.86518643,-0.86487118,-0.86455560,-0.86423968,
   -0.86392341,-0.86360681,-0.86328986,-0.86297257,-0.86265494,
   -0.86233697,-0.86201866,-0.86170001,-0.86138102,-0.86106169,
   -0.86074202,-0.86042201,-0.86010166,-0.85978097,-0.85945994,
   -0.85913858,-0.85881687,-0.85849482,-0.85817244,-0.85784971,
   -0.85752665,-0.85720325,-0.85687951,-0.85655543,-0.85623102,
   -0.85590626,-0.85558117,-0.85525574,-0.85492997,-0.85460387,
   -0.85427743,-0.85395065,-0.85362353,-0.85329608,-0.85296829,
   -0.85264016,-0.85231170,-0.85198290,-0.85165376,-0.85132429,
   -0.85099448,-0.85066433,-0.85033385,-0.85000303,-0.84967188,
   -0.84934040,-0.84900857,-0.84867641,-0.84834392,-0.84801109,
   -0.84767793,-0.84734443,-0.84701060,-0.84667644,-0.84634194,
   -0.84600710,-0.84567193,-0.84533643,-0.84500059,-0.84466442,
   -0.84432792,-0.84399108,-0.84365391,-0.84331641,-0.84297858,
   -0.84264041,-0.84230191,-0.84196307,-0.84162391,-0.84128441,
   -0.84094458,-0.84060441,-0.84026392,-0.83992309,-0.83958194,
   -0.83924045,-0.83889863,-0.83855648,-0.83821399,-0.83787118,
   -0.83752804,-0.83718456,-0.83684075,-0.83649662,-0.83615215,
   -0.83580736,-0.83546223,-0.83511677,-0.83477099,-0.83442487,
   -0.83407843,-0.83373165,-0.83338455,-0.83303712,-0.83268936,
   -0.83234127,-0.83199285,-0.83164410,-0.83129503,-0.83094563,
   -0.83059589,-0.83024584,-0.82989545,-0.82954473,-0.82919369,
   -0.82884232,-0.82849063,-0.82813860,-0.82778625,-0.82743357,
   -0.82708057,-0.82672724,-0.82637358,-0.82601960,-0.82566529,
   -0.82531065,-0.82495569,-0.82460041,-0.82424479,-0.82388886,
   -0.82353259,-0.82317600,-0.82281909,-0.82246185,-0.82210429,
   -0.82174640,-0.82138819,-0.82102966,-0.82067080,-0.82031161,
   -0.81995210,-0.81959227,-0.81923212,-0.81887164,-0.81851084,
   -0.81814971,-0.81778826,-0.81742649,-0.81706440,-0.81670198,
   -0.81633925,-0.81597619,-0.81561280,-0.81524910,-0.81488507,
   -0.81452072,-0.81415605,-0.81379106,-0.81342575,-0.81306011,
   -0.81269416,-0.81232788,-0.81196129,-0.81159437,-0.81122713,
   -0.81085958,-0.81049170,-0.81012350,-0.80975498,-0.80938615,
   -0.80901699,-0.80864751,-0.80827772,-0.80790760,-0.80753717,
   -0.80716642,-0.80679535,-0.80642396,-0.80605225,-0.80568022,
   -0.80530788,-0.80493522,-0.80456224,-0.80418894,-0.80381533,
   -0.80344140,-0.80306715,-0.80269258,-0.80231770,-0.80194250,
   -0.80156698,-0.80119115,-0.80081500,-0.80043853,-0.80006175,
   -0.79968465,-0.79930724,-0.79892951,-0.79855147,-0.79817311,
   -0.79779443,-0.79741545,-0.79703614,-0.79665652,-0.79627659,
   -0.79589634,-0.79551578,-0.79513490,-0.79475371,-0.79437221,
   -0.79399039,-0.79360826,-0.79322582,-0.79284306,-0.79245999,
   -0.79207661,-0.79169291,-0.79130891,-0.79092459,-0.79053995,
   -0.79015501,-0.78976975,-0.78938418,-0.78899830,-0.78861211,
   -0.78822561,-0.78783879,-0.78745167,-0.78706423,-0.78667648,
   -0.78628843,-0.78590006,-0.78551138,-0.78512239,-0.78473310,
   -0.78434349,-0.78395357,-0.78356334,-0.78317281,-0.78278196,
   -0.78239081,-0.78199934,-0.78160757,-0.78121549,-0.78082310,
   -0.78043040,-0.78003740,-0.77964408,-0.77925046,-0.77885653,
   -0.77846230,-0.77806775,-0.77767290,-0.77727774,-0.77688228,
   -0.77648651,-0.77609043,-0.77569405,-0.77529735,-0.77490036,
   -0.77450306,-0.77410545,-0.77370753,-0.77330931,-0.77291079,
   -0.77251196,-0.77211282,-0.77171338,-0.77131364,-0.77091359,
   -0.77051324,-0.77011258,-0.76971162,-0.76931035,-0.76890878,
   -0.76850691,-0.76810474,-0.76770226,-0.76729948,-0.76689639,
   -0.76649300,-0.76608931,-0.76568532,-0.76528102,-0.76487643,
   -0.76447153,-0.76406633,-0.76366082,-0.76325502,-0.76284891,
   -0.76244251,-0.76203580,-0.76162879,-0.76122148,-0.76081387,
   -0.76040596,-0.75999775,-0.75958924,-0.75918043,-0.75877132,
   -0.75836191,-0.75795220,-0.75754219,-0.75713189,-0.75672128,
   -0.75631038,-0.75589917,-0.75548767,-0.75507587,-0.75466377,
   -0.75425138,-0.75383868,-0.75342569,-0.75301240,-0.75259882,
   -0.75218493,-0.75177075,-0.75135627,-0.75094150,-0.75052643,
   -0.75011106,-0.74969540,-0.74927944,-0.74886319,-0.74844664,
   -0.74802979,-0.74761265,-0.74719522,-0.74677749,-0.74635946,
   -0.74594114,-0.74552253,-0.74510362,-0.74468441,-0.74426492,
   -0.74384512,-0.74342504,-0.74300466,-0.74258399,-0.74216303,
   -0.74174177,-0.74132022,-0.74089837,-0.74047624,-0.74005381,
   -0.73963109,-0.73920808,-0.73878477,-0.73836118,-0.73793729,
   -0.73751311,-0.73708864,-0.73666388,-0.73623883,-0.73581349,
   -0.73538786,-0.73496193,-0.73453572,-0.73410922,-0.73368242,
   -0.73325534,-0.73282797,-0.73240031,-0.73197236,-0.73154412,
   -0.73111559,-0.73068677,-0.73025767,-0.72982827,-0.72939859,
   -0.72896862,-0.72853837,-0.72810782,-0.72767699,-0.72724587,
   -0.72681446,-0.72638277,-0.72595079,-0.72551852,-0.72508597,
   -0.72465313,-0.72422000,-0.72378659,-0.72335289,-0.72291891,
   -0.72248464,-0.72205008,-0.72161525,-0.72118012,-0.72074471,
   -0.72030902,-0.71987304,-0.71943678,-0.71900024,-0.71856341,
   -0.71812629,-0.71768890,-0.71725122,-0.71681325,-0.71637501,
   -0.71593648,-0.71549767,-0.71505857,-0.71461920,-0.71417954,
   -0.71373960,-0.71329938,-0.71285887,-0.71241809,-0.71197702,
   -0.71153567,-0.71109404,-0.71065213,-0.71020995,-0.70976748,
   -0.70932472,-0.70888169,-0.70843838,-0.70799479,-0.70755093,
   -0.70710678,-0.70666235,-0.70621764,-0.70577266,-0.70532739,
   -0.70488185,-0.70443603,-0.70398993,-0.70354355,-0.70309690,
   -0.70264996,-0.70220276,-0.70175527,-0.70130750,-0.70085946,
   -0.70041115,-0.69996255,-0.69951368,-0.69906453,-0.69861511,
   -0.69816541,-0.69771544,-0.69726519,-0.69681467,-0.69636387,
   -0.69591279,-0.69546144,-0.69500982,-0.69455792,-0.69410575,
   -0.69365330,-0.69320058,-0.69274759,-0.69229432,-0.69184078,
   -0.69138696,-0.69093288,-0.69047852,-0.69002389,-0.68956898,
   -0.68911380,-0.68865835,-0.68820263,-0.68774664,-0.68729038,
   -0.68683384,-0.68637704,-0.68591996,-0.68546261,-0.68500499,
   -0.68454710,-0.68408894,-0.68363051,-0.68317181,-0.68271284,
   -0.68225360,-0.68179410,-0.68133432,-0.68087427,-0.68041396,
   -0.67995337,-0.67949252,-0.67903140,-0.67857001,-0.67810836,
   -0.67764643,-0.67718424,-0.67672178,-0.67625906,-0.67579606,
   -0.67533280,-0.67486928,-0.67440548,-0.67394142,-0.67347710,
   -0.67301251,-0.67254765,-0.67208253,-0.67161714,-0.67115149,
   -0.67068557,-0.67021939,-0.66975294,-0.66928623,-0.66881926,
   -0.66835202,-0.66788451,-0.66741674,-0.66694871,-0.66648042,
   -0.66601186,-0.66554304,-0.66507396,-0.66460461,-0.66413501,
   -0.66366514,-0.66319500,-0.66272461,-0.66225396,-0.66178304,
   -0.66131186,-0.66084042,-0.66036872,-0.65989676,-0.65942454,
   -0.65895206,-0.65847932,-0.65800631,-0.65753305,-0.65705953,
   -0.65658575,-0.65611171,-0.65563741,-0.65516285,-0.65468804,
   -0.65421296,-0.65373763,-0.65326204,-0.65278619,-0.65231008,
   -0.65183372,-0.65135710,-0.65088022,-0.65040308,-0.64992569,
   -0.64944804,-0.64897014,-0.64849198,-0.64801356,-0.64753489,
   -0.64705596,-0.64657677,-0.64609733,-0.64561764,-0.64513769,
   -0.64465748,-0.64417702,-0.64369631,-0.64321534,-0.64273412,
   -0.64225265,-0.64177092,-0.64128894,-0.64080670,-0.64032421,
   -0.63984147,-0.63935848,-0.63887523,-0.63839174,-0.63790799,
   -0.63742398,-0.63693973,-0.63645523,-0.63597047,-0.63548546,
   -0.63500020,-0.63451470,-0.63402894,-0.63354293,-0.63305667,
   -0.63257016,-0.63208340,-0.63159639,-0.63110913,-0.63062162,
   -0.63013387,-0.62964586,-0.62915761,-0.62866911,-0.62818035,
   -0.62769136,-0.62720211,-0.62671262,-0.62622288,-0.62573289,
   -0.62524265,-0.62475217,-0.62426144,-0.62377046,-0.62327924,
   -0.62278778,-0.62229606,-0.62180410,-0.62131190,-0.62081945,
   -0.62032675,-0.61983381,-0.61934063,-0.61884720,-0.61835353,
   -0.61785961,-0.61736545,-0.61687104,-0.61637639,-0.61588150,
   -0.61538637,-0.61489099,-0.61439537,-0.61389950,-0.61340340,
   -0.61290705,-0.61241046,-0.61191363,-0.61141655,-0.61091924,
   -0.61042168,-0.60992389,-0.60942585,-0.60892757,-0.60842905,
   -0.60793029,-0.60743129,-0.60693206,-0.60643258,-0.60593286,
   -0.60543290,-0.60493271,-0.60443227,-0.60393160,-0.60343069,
   -0.60292954,-0.60242815,-0.60192652,-0.60142466,-0.60092256,
   -0.60042022,-0.59991765,-0.59941483,-0.59891178,-0.59840850,
   -0.59790498,-0.59740122,-0.59689723,-0.59639300,-0.59588853,
   -0.59538383,-0.59487890,-0.59437373,-0.59386833,-0.59336269,
   -0.59285682,-0.59235071,-0.59184437,-0.59133779,-0.59083099,
   -0.59032394,-0.58981667,-0.58930916,-0.58880142,-0.58829345,
   -0.58778525,-0.58727681,-0.58676814,-0.58625924,-0.58575011,
   -0.58524075,-0.58473116,-0.58422133,-0.58371128,-0.58320099,
   -0.58269047,-0.58217973,-0.58166875,-0.58115755,-0.58064611,
   -0.58013445,-0.57962256,-0.57911043,-0.57859808,-0.57808551,
   -0.57757270,-0.57705966,-0.57654640,-0.57603291,-0.57551919,
   -0.57500525,-0.57449108,-0.57397668,-0.57346205,-0.57294720,
   -0.57243212,-0.57191682,-0.57140129,-0.57088553,-0.57036955,
   -0.56985334,-0.56933691,-0.56882026,-0.56830338,-0.56778627,
   -0.56726894,-0.56675139,-0.56623362,-0.56571562,-0.56519739,
   -0.56467895,-0.56416028,-0.56364138,-0.56312227,-0.56260293,
   -0.56208337,-0.56156359,-0.56104359,-0.56052337,-0.56000292,
   -0.55948225,-0.55896137,-0.55844026,-0.55791893,-0.55739738,
   -0.55687561,-0.55635362,-0.55583141,-0.55530899,-0.55478634,
   -0.55426347,-0.55374039,-0.55321709,-0.55269356,-0.55216982,
   -0.55164587,-0.55112169,-0.55059730,-0.55007269,-0.54954786,
   -0.54902281,-0.54849755,-0.54797207,-0.54744638,-0.54692047,
   -0.54639434,-0.54586800,-0.54534144,-0.54481467,-0.54428768,
   -0.54376048,-0.54323306,-0.54270543,-0.54217758,-0.54164952,
   -0.54112125,-0.54059276,-0.54006406,-0.53953514,-0.53900602,
   -0.53847668,-0.53794712,-0.53741736,-0.53688738,-0.53635719,
   -0.53582679,-0.53529618,-0.53476535,-0.53423432,-0.53370307,
   -0.53317162,-0.53263995,-0.53210807,-0.53157598,-0.53104369,
   -0.53051118,-0.52997846,-0.52944554,-0.52891240,-0.52837906,
   -0.52784551,-0.52731175,-0.52677778,-0.52624360,-0.52570922,
   -0.52517463,-0.52463983,-0.52410482,-0.52356961,-0.52303419,
   -0.52249856,-0.52196273,-0.52142669,-0.52089044,-0.52035399,
   -0.51981734,-0.51928048,-0.51874341,-0.51820614,-0.51766866,
   -0.51713099,-0.51659310,-0.51605501,-0.51551672,-0.51497823,
   -0.51443953,-0.51390063,-0.51336152,-0.51282222,-0.51228271,
   -0.51174300,-0.51120308,-0.51066297,-0.51012265,-0.50958213,
   -0.50904141,-0.50850049,-0.50795937,-0.50741805,-0.50687652,
   -0.50633480,-0.50579288,-0.50525076,-0.50470844,-0.50416592,
   -0.50362320,-0.50308028,-0.50253716,-0.50199385,-0.50145033,
   -0.50090662,-0.50036271,-0.49981860,-0.49927430,-0.49872980,
   -0.49818510,-0.49764021,-0.49709511,-0.49654983,-0.49600434,
   -0.49545866,-0.49491279,-0.49436672,-0.49382045,-0.49327399,
   -0.49272734,-0.49218049,-0.49163344,-0.49108620,-0.49053877,
   -0.48999115,-0.48944333,-0.48889532,-0.48834711,-0.48779871,
   -0.48725012,-0.48670134,-0.48615236,-0.48560320,-0.48505384,
   -0.48450429,-0.48395454,-0.48340461,-0.48285449,-0.48230417,
   -0.48175367,-0.48120297,-0.48065209,-0.48010102,-0.47954975,
   -0.47899830,-0.47844666,-0.47789482,-0.47734280,-0.47679060,
   -0.47623820,-0.47568561,-0.47513284,-0.47457988,-0.47402673,
   -0.47347340,-0.47291988,-0.47236617,-0.47181228,-0.47125819,
   -0.47070393,-0.47014947,-0.46959484,-0.46904001,-0.46848500,
   -0.46792981,-0.46737443,-0.46681887,-0.46626312,-0.46570719,
   -0.46515107,-0.46459477,-0.46403829,-0.46348163,-0.46292478,
   -0.46236775,-0.46181053,-0.46125314,-0.46069556,-0.46013780,
   -0.45957986,-0.45902173,-0.45846343,-0.45790494,-0.45734628,
   -0.45678743,-0.45622840,-0.45566920,-0.45510981,-0.45455024,
   -0.45399049,-0.45343057,-0.45287046,-0.45231018,-0.45174972,
   -0.45118908,-0.45062826,-0.45006727,-0.44950609,-0.44894474,
   -0.44838321,-0.44782151,-0.44725962,-0.44669756,-0.44613533,
   -0.44557292,-0.44501033,-0.44444757,-0.44388463,-0.44332151,
   -0.44275823,-0.44219476,-0.44163112,-0.44106731,-0.44050333,
   -0.43993916,-0.43937483,-0.43881032,-0.43824564,-0.43768079,
   -0.43711576,-0.43655056,-0.43598519,-0.43541965,-0.43485393,
   -0.43428804,-0.43372199,-0.43315576,-0.43258935,-0.43202278,
   -0.43145604,-0.43088913,-0.43032205,-0.42975479,-0.42918737,
   -0.42861978,-0.42805202,-0.42748409,-0.42691599,-0.42634772,
   -0.42577929,-0.42521068,-0.42464191,-0.42407297,-0.42350387,
   -0.42293459,-0.42236515,-0.42179554,-0.42122577,-0.42065583,
   -0.42008572,-0.41951545,-0.41894501,-0.41837441,-0.41780364,
   -0.41723271,-0.41666161,-0.41609035,-0.41551892,-0.41494733,
   -0.41437558,-0.41380366,-0.41323158,-0.41265933,-0.41208692,
   -0.41151435,-0.41094162,-0.41036873,-0.40979567,-0.40922245,
   -0.40864907,-0.40807553,-0.40750183,-0.40692796,-0.40635394,
   -0.40577975,-0.40520541,-0.40463090,-0.40405624,-0.40348141,
   -0.40290643,-0.40233129,-0.40175599,-0.40118053,-0.40060491,
   -0.40002913,-0.39945320,-0.39887711,-0.39830086,-0.39772445,
   -0.39714789,-0.39657117,-0.39599429,-0.39541725,-0.39484006,
   -0.39426272,-0.39368522,-0.39310756,-0.39252975,-0.39195178,
   -0.39137366,-0.39079539,-0.39021696,-0.38963837,-0.38905963,
   -0.38848074,-0.38790170,-0.38732250,-0.38674315,-0.38616364,
   -0.38558399,-0.38500418,-0.38442422,-0.38384411,-0.38326384,
   -0.38268343,-0.38210286,-0.38152214,-0.38094128,-0.38036026,
   -0.37977909,-0.37919777,-0.37861630,-0.37803469,-0.37745292,
   -0.37687101,-0.37628894,-0.37570673,-0.37512437,-0.37454186,
   -0.37395920,-0.37337640,-0.37279344,-0.37221034,-0.37162710,
   -0.37104371,-0.37046017,-0.36987648,-0.36929265,-0.36870867,
   -0.36812455,-0.36754028,-0.36695587,-0.36637131,-0.36578660,
   -0.36520176,-0.36461677,-0.36403163,-0.36344635,-0.36286093,
   -0.36227536,-0.36168965,-0.36110380,-0.36051781,-0.35993167,
   -0.35934539,-0.35875897,-0.35817241,-0.35758570,-0.35699886,
   -0.35641187,-0.35582475,-0.35523748,-0.35465007,-0.35406253,
   -0.35347484,-0.35288701,-0.35229905,-0.35171094,-0.35112270,
   -0.35053432,-0.34994579,-0.34935714,-0.34876834,-0.34817940,
   -0.34759033,-0.34700112,-0.34641178,-0.34582229,-0.34523267,
   -0.34464292,-0.34405303,-0.34346300,-0.34287284,-0.34228254,
   -0.34169210,-0.34110153,-0.34051083,-0.33991999,-0.33932902,
   -0.33873792,-0.33814668,-0.33755530,-0.33696380,-0.33637216,
   -0.33578038,-0.33518848,-0.33459644,-0.33400427,-0.33341197,
   -0.33281954,-0.33222698,-0.33163428,-0.33104145,-0.33044850,
   -0.32985541,-0.32926219,-0.32866884,-0.32807537,-0.32748176,
   -0.32688802,-0.32629416,-0.32570017,-0.32510604,-0.32451179,
   -0.32391741,-0.32332291,-0.32272827,-0.32213351,-0.32153862,
   -0.32094360,-0.32034846,-0.31975319,-0.31915780,-0.31856228,
   -0.31796663,-0.31737086,-0.31677496,-0.31617894,-0.31558279,
   -0.31498651,-0.31439012,-0.31379360,-0.31319695,-0.31260018,
   -0.31200329,-0.31140628,-0.31080914,-0.31021188,-0.30961449,
   -0.30901699,-0.30841936,-0.30782161,-0.30722374,-0.30662575,
   -0.30602764,-0.30542940,-0.30483105,-0.30423257,-0.30363398,
   -0.30303526,-0.30243643,-0.30183748,-0.30123840,-0.30063921,
   -0.30003990,-0.29944047,-0.29884093,-0.29824126,-0.29764148,
   -0.29704158,-0.29644156,-0.29584142,-0.29524117,-0.29464080,
   -0.29404032,-0.29343972,-0.29283900,-0.29223817,-0.29163722,
   -0.29103616,-0.29043498,-0.28983369,-0.28923229,-0.28863077,
   -0.28802913,-0.28742738,-0.28682552,-0.28622355,-0.28562146,
   -0.28501926,-0.28441694,-0.28381452,-0.28321198,-0.28260933,
   -0.28200657,-0.28140370,-0.28080072,-0.28019762,-0.27959442,
   -0.27899110,-0.27838768,-0.27778414,-0.27718050,-0.27657674,
   -0.27597288,-0.27536891,-0.27476482,-0.27416063,-0.27355634,
   -0.27295193,-0.27234742,-0.27174280,-0.27113807,-0.27053323,
   -0.26992829,-0.26932324,-0.26871809,-0.26811282,-0.26750746,
   -0.26690198,-0.26629641,-0.26569072,-0.26508494,-0.26447904,
   -0.26387305,-0.26326694,-0.26266074,-0.26205443,-0.26144802,
   -0.26084150,-0.26023488,-0.25962816,-0.25902134,-0.25841441,
   -0.25780738,-0.25720025,-0.25659302,-0.25598569,-0.25537826,
   -0.25477072,-0.25416309,-0.25355535,-0.25294751,-0.25233958,
   -0.25173154,-0.25112341,-0.25051518,-0.24990684,-0.24929841,
   -0.24868988,-0.24808125,-0.24747253,-0.24686371,-0.24625478,
   -0.24564577,-0.24503665,-0.24442744,-0.24381813,-0.24320873,
   -0.24259923,-0.24198963,-0.24137994,-0.24077015,-0.24016027,
   -0.23955029,-0.23894022,-0.23833005,-0.23771979,-0.23710944,
   -0.23649899,-0.23588845,-0.23527782,-0.23466709,-0.23405627,
   -0.23344536,-0.23283435,-0.23222326,-0.23161207,-0.23100079,
   -0.23038942,-0.22977796,-0.22916641,-0.22855477,-0.22794303,
   -0.22733121,-0.22671930,-0.22610730,-0.22549521,-0.22488303,
   -0.22427076,-0.22365840,-0.22304595,-0.22243342,-0.22182080,
   -0.22120809,-0.22059529,-0.21998241,-0.21936944,-0.21875638,
   -0.21814324,-0.21753001,-0.21691669,-0.21630329,-0.21568980,
   -0.21507623,-0.21446258,-0.21384883,-0.21323501,-0.21262110,
   -0.21200711,-0.21139303,-0.21077887,-0.21016462,-0.20955030,
   -0.20893589,-0.20832139,-0.20770682,-0.20709216,-0.20647742,
   -0.20586260,-0.20524770,-0.20463272,-0.20401766,-0.20340251,
   -0.20278729,-0.20217199,-0.20155660,-0.20094114,-0.20032560,
   -0.19970998,-0.19909428,-0.19847850,-0.19786264,-0.19724670,
   -0.19663069,-0.19601460,-0.19539843,-0.19478218,-0.19416586,
   -0.19354946,-0.19293299,-0.19231644,-0.19169981,-0.19108311,
   -0.19046633,-0.18984947,-0.18923254,-0.18861554,-0.18799846,
   -0.18738131,-0.18676408,-0.18614678,-0.18552941,-0.18491196,
   -0.18429444,-0.18367685,-0.18305919,-0.18244145,-0.18182364,
   -0.18120576,-0.18058781,-0.17996978,-0.17935169,-0.17873352,
   -0.17811529,-0.17749698,-0.17687860,-0.17626016,-0.17564164,
   -0.17502305,-0.17440440,-0.17378568,-0.17316688,-0.17254802,
   -0.17192910,-0.17131010,-0.17069104,-0.17007190,-0.16945271,
   -0.16883344,-0.16821411,-0.16759471,-0.16697524,-0.16635571,
   -0.16573612,-0.16511646,-0.16449673,-0.16387694,-0.16325708,
   -0.16263716,-0.16201718,-0.16139713,-0.16077701,-0.16015684,
   -0.15953660,-0.15891630,-0.15829593,-0.15767550,-0.15705501,
   -0.15643446,-0.15581385,-0.15519317,-0.15457243,-0.15395164,
   -0.15333078,-0.15270986,-0.15208888,-0.15146784,-0.15084674,
   -0.15022558,-0.14960437,-0.14898309,-0.14836175,-0.14774036,
   -0.14711891,-0.14649740,-0.14587583,-0.14525420,-0.14463252,
   -0.14401078,-0.14338898,-0.14276713,-0.14214522,-0.14152325,
   -0.14090123,-0.14027915,-0.13965702,-0.13903483,-0.13841258,
   -0.13779029,-0.13716793,-0.13654553,-0.13592307,-0.13530055,
   -0.13467798,-0.13405536,-0.13343269,-0.13280996,-0.13218719,
   -0.13156435,-0.13094147,-0.13031854,-0.12969555,-0.12907251,
   -0.12844943,-0.12782629,-0.12720310,-0.12657986,-0.12595657,
   -0.12533323,-0.12470984,-0.12408640,-0.12346292,-0.12283938,
   -0.12221580,-0.12159216,-0.12096848,-0.12034475,-0.11972098,
   -0.11909716,-0.11847329,-0.11784937,-0.11722541,-0.11660140,
   -0.11597734,-0.11535324,-0.11472909,-0.11410490,-0.11348066,
   -0.11285638,-0.11223205,-0.11160768,-0.11098327,-0.11035881,
   -0.10973431,-0.10910976,-0.10848517,-0.10786054,-0.10723587,
   -0.10661115,-0.10598639,-0.10536159,-0.10473675,-0.10411186,
   -0.10348694,-0.10286198,-0.10223697,-0.10161192,-0.10098684,
   -0.10036171,-0.09973654,-0.09911134,-0.09848609,-0.09786081,
   -0.09723549,-0.09661013,-0.09598473,-0.09535929,-0.09473382,
   -0.09410831,-0.09348276,-0.09285717,-0.09223155,-0.09160589,
   -0.09098020,-0.09035447,-0.08972870,-0.08910290,-0.08847706,
   -0.08785119,-0.08722529,-0.08659934,-0.08597337,-0.08534736,
   -0.08472132,-0.08409524,-0.08346913,-0.08284299,-0.08221681,
   -0.08159061,-0.08096437,-0.08033810,-0.07971179,-0.07908546,
   -0.07845909,-0.07783269,-0.07720627,-0.07657981,-0.07595332,
   -0.07532680,-0.07470025,-0.07407367,-0.07344707,-0.07282043,
   -0.07219377,-0.07156707,-0.07094035,-0.07031360,-0.06968683,
   -0.06906002,-0.06843319,-0.06780633,-0.06717944,-0.06655253,
   -0.06592559,-0.06529863,-0.06467164,-0.06404462,-0.06341758,
   -0.06279051,-0.06216342,-0.06153631,-0.06090917,-0.06028200,
   -0.05965482,-0.05902761,-0.05840037,-0.05777311,-0.05714583,
   -0.05651853,-0.05589120,-0.05526386,-0.05463649,-0.05400910,
   -0.05338168,-0.05275425,-0.05212680,-0.05149932,-0.05087183,
   -0.05024431,-0.04961678,-0.04898922,-0.04836165,-0.04773406,
   -0.04710645,-0.04647882,-0.04585117,-0.04522350,-0.04459582,
   -0.04396811,-0.04334039,-0.04271266,-0.04208490,-0.04145713,
   -0.04082935,-0.04020154,-0.03957373,-0.03894589,-0.03831804,
   -0.03769018,-0.03706230,-0.03643440,-0.03580650,-0.03517857,
   -0.03455064,-0.03392269,-0.03329472,-0.03266675,-0.03203876,
   -0.03141075,-0.03078274,-0.03015471,-0.02952667,-0.02889862,
   -0.02827056,-0.02764249,-0.02701441,-0.02638631,-0.02575821,
   -0.02513009,-0.02450197,-0.02387383,-0.02324569,-0.02261753,
   -0.02198937,-0.02136120,-0.02073302,-0.02010483,-0.01947664,
   -0.01884843,-0.01822022,-0.01759201,-0.01696378,-0.01633555,
   -0.01570731,-0.01507907,-0.01445082,-0.01382256,-0.01319430,
   -0.01256603,-0.01193776,-0.01130949,-0.01068121,-0.01005292,
   -0.00942463,-0.00879634,-0.00816805,-0.00753975,-0.00691144,
   -0.00628314,-0.00565483,-0.00502652,-0.00439821,-0.00376990,
   -0.00314158,-0.00251327,-0.00188495,-0.00125663,-0.00062831,
   0.00000000,0.00062832,0.00125664,0.00188496,0.00251328,
   0.00314159,0.00376991,0.00439822,0.00502653,0.00565484,
   0.00628315,0.00691145,0.00753976,0.00816805,0.00879635,
   0.00942464,0.01005293,0.01068122,0.01130950,0.01193777,
   0.01256604,0.01319431,0.01382257,0.01445083,0.01507908,
   0.01570732,0.01633556,0.01696379,0.01759202,0.01822023,
   0.01884844,0.01947665,0.02010484,0.02073303,0.02136121,
   0.02198938,0.02261754,0.02324570,0.02387384,0.02450198,
   0.02513010,0.02575822,0.02638632,0.02701441,0.02764250,
   0.02827057,0.02889863,0.02952668,0.03015472,0.03078275,
   0.03141076,0.03203877,0.03266676,0.03329473,0.03392270,
   0.03455065,0.03517858,0.03580651,0.03643441,0.03706231,
   0.03769019,0.03831805,0.03894590,0.03957374,0.04020155,
   0.04082936,0.04145714,0.04208491,0.04271267,0.04334040,
   0.04396812,0.04459583,0.04522351,0.04585118,0.04647883,
   0.04710646,0.04773407,0.04836166,0.04898923,0.04961679,
   0.05024432,0.05087184,0.05149933,0.05212681,0.05275426,
   0.05338169,0.05400911,0.05463650,0.05526387,0.05589121,
   0.05651854,0.05714584,0.05777312,0.05840038,0.05902761,
   0.05965483,0.06028201,0.06090918,0.06153632,0.06216343,
   0.06279052,0.06341759,0.06404463,0.06467165,0.06529864,
   0.06592560,0.06655254,0.06717945,0.06780634,0.06843320,
   0.06906003,0.06968684,0.07031361,0.07094036,0.07156708,
   0.07219378,0.07282044,0.07344708,0.07407368,0.07470026,
   0.07532681,0.07595333,0.07657982,0.07720628,0.07783270,
   0.07845910,0.07908547,0.07971180,0.08033810,0.08096438,
   0.08159062,0.08221682,0.08284300,0.08346914,0.08409525,
   0.08472133,0.08534737,0.08597338,0.08659935,0.08722529,
   0.08785120,0.08847707,0.08910291,0.08972871,0.09035448,
   0.09098021,0.09160590,0.09223156,0.09285718,0.09348277,
   0.09410832,0.09473383,0.09535930,0.09598474,0.09661014,
   0.09723550,0.09786082,0.09848610,0.09911135,0.09973655,
   0.10036172,0.10098685,0.10161193,0.10223698,0.10286198,
   0.10348695,0.10411187,0.10473676,0.10536160,0.10598640,
   0.10661116,0.10723588,0.10786055,0.10848518,0.10910977,
   0.10973432,0.11035882,0.11098328,0.11160769,0.11223206,
   0.11285639,0.11348067,0.11410491,0.11472910,0.11535325,
   0.11597735,0.11660141,0.11722541,0.11784938,0.11847329,
   0.11909716,0.11972099,0.12034476,0.12096849,0.12159217,
   0.12221580,0.12283939,0.12346292,0.12408641,0.12470985,
   0.12533324,0.12595658,0.12657987,0.12720311,0.12782630,
   0.12844944,0.12907252,0.12969556,0.13031855,0.13094148,
   0.13156436,0.13218719,0.13280997,0.13343270,0.13405537,
   0.13467799,0.13530056,0.13592308,0.13654554,0.13716794,
   0.13779030,0.13841259,0.13903484,0.13965703,0.14027916,
   0.14090124,0.14152326,0.14214523,0.14276714,0.14338899,
   0.14401079,0.14463253,0.14525421,0.14587584,0.14649741,
   0.14711892,0.14774037,0.14836176,0.14898310,0.14960438,
   0.15022559,0.15084675,0.15146785,0.15208889,0.15270987,
   0.15333079,0.15395165,0.15457244,0.15519318,0.15581386,
   0.15643447,0.15705502,0.15767551,0.15829594,0.15891630,
   0.15953661,0.16015685,0.16077702,0.16139714,0.16201718,
   0.16263717,0.16325709,0.16387695,0.16449674,0.16511647,
   0.16573613,0.16635572,0.16697525,0.16759472,0.16821412,
   0.16883345,0.16945272,0.17007191,0.17069104,0.17131011,
   0.17192911,0.17254803,0.17316689,0.17378569,0.17440441,
   0.17502306,0.17564165,0.17626017,0.17687861,0.17749699,
   0.17811530,0.17873353,0.17935170,0.17996979,0.18058782,
   0.18120577,0.18182365,0.18244146,0.18305920,0.18367686,
   0.18429445,0.18491197,0.18552942,0.18614679,0.18676409,
   0.18738132,0.18799847,0.18861555,0.18923255,0.18984948,
   0.19046634,0.19108311,0.19169982,0.19231645,0.19293300,
   0.19354947,0.19416587,0.19478219,0.19539844,0.19601461,
   0.19663070,0.19724671,0.19786265,0.19847851,0.19909428,
   0.19970999,0.20032561,0.20094115,0.20155661,0.20217200,
   0.20278730,0.20340252,0.20401767,0.20463273,0.20524771,
   0.20586261,0.20647743,0.20709217,0.20770683,0.20832140,
   0.20893590,0.20955031,0.21016463,0.21077888,0.21139304,
   0.21200711,0.21262111,0.21323502,0.21384884,0.21446259,
   0.21507624,0.21568981,0.21630330,0.21691670,0.21753002,
   0.21814325,0.21875639,0.21936945,0.21998242,0.22059530,
   0.22120810,0.22182081,0.22243343,0.22304596,0.22365841,
   0.22427077,0.22488303,0.22549522,0.22610731,0.22671931,
   0.22733122,0.22794304,0.22855478,0.22916642,0.22977797,
   0.23038943,0.23100080,0.23161208,0.23222327,0.23283436,
   0.23344537,0.23405628,0.23466710,0.23527783,0.23588846,
   0.23649900,0.23710945,0.23771980,0.23833006,0.23894023,
   0.23955030,0.24016028,0.24077016,0.24137995,0.24198964,
   0.24259924,0.24320874,0.24381814,0.24442745,0.24503666,
   0.24564578,0.24625479,0.24686372,0.24747254,0.24808126,
   0.24868989,0.24929842,0.24990685,0.25051519,0.25112342,
   0.25173155,0.25233959,0.25294752,0.25355536,0.25416310,
   0.25477073,0.25537827,0.25598570,0.25659303,0.25720026,
   0.25780739,0.25841442,0.25902135,0.25962817,0.26023489,
   0.26084151,0.26144803,0.26205444,0.26266075,0.26326695,
   0.26387305,0.26447905,0.26508494,0.26569073,0.26629642,
   0.26690199,0.26750747,0.26811283,0.26871810,0.26932325,
   0.26992830,0.27053324,0.27113808,0.27174281,0.27234743,
   0.27295194,0.27355635,0.27416064,0.27476483,0.27536892,
   0.27597289,0.27657675,0.27718051,0.27778415,0.27838769,
   0.27899111,0.27959443,0.28019763,0.28080073,0.28140371,
   0.28200658,0.28260934,0.28321199,0.28381453,0.28441695,
   0.28501927,0.28562147,0.28622356,0.28682553,0.28742739,
   0.28802914,0.28863078,0.28923230,0.28983370,0.29043499,
   0.29103617,0.29163723,0.29223818,0.29283901,0.29343973,
   0.29404033,0.29464081,0.29524118,0.29584143,0.29644157,
   0.29704159,0.29764149,0.29824127,0.29884093,0.29944048,
   0.30003991,0.30063922,0.30123841,0.30183749,0.30243644,
   0.30303527,0.30363399,0.30423258,0.30483106,0.30542941,
   0.30602765,0.30662576,0.30722375,0.30782162,0.30841937,
   0.30901700,0.30961450,0.31021189,0.31080915,0.31140629,
   0.31200330,0.31260019,0.31319696,0.31379361,0.31439013,
   0.31498652,0.31558280,0.31617894,0.31677497,0.31737087,
   0.31796664,0.31856229,0.31915781,0.31975320,0.32034847,
   0.32094361,0.32153863,0.32213352,0.32272828,0.32332292,
   0.32391742,0.32451180,0.32510605,0.32570018,0.32629417,
   0.32688803,0.32748177,0.32807538,0.32866885,0.32926220,
   0.32985542,0.33044851,0.33104146,0.33163429,0.33222699,
   0.33281955,0.33341198,0.33400428,0.33459645,0.33518849,
   0.33578039,0.33637217,0.33696381,0.33755531,0.33814669,
   0.33873793,0.33932903,0.33992000,0.34051084,0.34110154,
   0.34169211,0.34228255,0.34287285,0.34346301,0.34405304,
   0.34464293,0.34523268,0.34582230,0.34641179,0.34700113,
   0.34759034,0.34817941,0.34876835,0.34935715,0.34994580,
   0.35053433,0.35112271,0.35171095,0.35229906,0.35288702,
   0.35347485,0.35406254,0.35465008,0.35523749,0.35582476,
   0.35641188,0.35699887,0.35758571,0.35817242,0.35875898,
   0.35934540,0.35993168,0.36051782,0.36110381,0.36168966,
   0.36227537,0.36286094,0.36344636,0.36403164,0.36461678,
   0.36520177,0.36578661,0.36637132,0.36695588,0.36754029,
   0.36812456,0.36870868,0.36929266,0.36987649,0.37046018,
   0.37104372,0.37162711,0.37221035,0.37279345,0.37337641,
   0.37395921,0.37454187,0.37512438,0.37570674,0.37628895,
   0.37687102,0.37745293,0.37803470,0.37861631,0.37919778,
   0.37977910,0.38036027,0.38094129,0.38152215,0.38210287,
   0.38268344,0.38326385,0.38384412,0.38442423,0.38500419,
   0.38558400,0.38616365,0.38674316,0.38732251,0.38790171,
   0.38848075,0.38905964,0.38963838,0.39021697,0.39079540,
   0.39137367,0.39195179,0.39252976,0.39310757,0.39368523,
   0.39426273,0.39484007,0.39541726,0.39599430,0.39657117,
   0.39714790,0.39772446,0.39830087,0.39887712,0.39945321,
   0.40002914,0.40060492,0.40118054,0.40175600,0.40233130,
   0.40290644,0.40348142,0.40405625,0.40463091,0.40520542,
   0.40577976,0.40635395,0.40692797,0.40750184,0.40807554,
   0.40864908,0.40922246,0.40979568,0.41036874,0.41094163,
   0.41151436,0.41208693,0.41265934,0.41323159,0.41380367,
   0.41437559,0.41494734,0.41551893,0.41609036,0.41666162,
   0.41723272,0.41780365,0.41837442,0.41894502,0.41951546,
   0.42008573,0.42065584,0.42122578,0.42179555,0.42236516,
   0.42293460,0.42350388,0.42407298,0.42464192,0.42521069,
   0.42577930,0.42634773,0.42691600,0.42748410,0.42805203,
   0.42861979,0.42918738,0.42975480,0.43032206,0.43088914,
   0.43145605,0.43202279,0.43258936,0.43315577,0.43372200,
   0.43428805,0.43485394,0.43541966,0.43598520,0.43655057,
   0.43711577,0.43768080,0.43824565,0.43881033,0.43937484,
   0.43993917,0.44050334,0.44106732,0.44163113,0.44219477,
   0.44275824,0.44332152,0.44388464,0.44444758,0.44501034,
   0.44557293,0.44613534,0.44669757,0.44725963,0.44782152,
   0.44838322,0.44894475,0.44950610,0.45006727,0.45062827,
   0.45118909,0.45174973,0.45231019,0.45287047,0.45343058,
   0.45399050,0.45455025,0.45510982,0.45566920,0.45622841,
   0.45678744,0.45734629,0.45790495,0.45846344,0.45902174,
   0.45957987,0.46013781,0.46069557,0.46125315,0.46181054,
   0.46236776,0.46292479,0.46348164,0.46403830,0.46459478,
   0.46515108,0.46570720,0.46626313,0.46681888,0.46737444,
   0.46792982,0.46848501,0.46904002,0.46959485,0.47014948,
   0.47070394,0.47125820,0.47181228,0.47236618,0.47291989,
   0.47347341,0.47402674,0.47457989,0.47513285,0.47568562,
   0.47623821,0.47679061,0.47734281,0.47789483,0.47844667,
   0.47899831,0.47954976,0.48010102,0.48065210,0.48120298,
   0.48175368,0.48230418,0.48285450,0.48340462,0.48395455,
   0.48450430,0.48505385,0.48560321,0.48615237,0.48670135,
   0.48725013,0.48779872,0.48834712,0.48889532,0.48944334,
   0.48999116,0.49053878,0.49108621,0.49163345,0.49218050,
   0.49272735,0.49327400,0.49382046,0.49436673,0.49491280,
   0.49545867,0.49600435,0.49654984,0.49709512,0.49764022,
   0.49818511,0.49872981,0.49927431,0.49981861,0.50036272,
   0.50090663,0.50145034,0.50199386,0.50253717,0.50308029,
   0.50362321,0.50416593,0.50470845,0.50525077,0.50579289,
   0.50633481,0.50687653,0.50741806,0.50795938,0.50850050,
   0.50904142,0.50958214,0.51012266,0.51066298,0.51120309,
   0.51174301,0.51228272,0.51282223,0.51336153,0.51390064,
   0.51443954,0.51497824,0.51551673,0.51605502,0.51659311,
   0.51713100,0.51766867,0.51820615,0.51874342,0.51928049,
   0.51981735,0.52035400,0.52089045,0.52142670,0.52196274,
   0.52249857,0.52303420,0.52356962,0.52410483,0.52463984,
   0.52517463,0.52570923,0.52624361,0.52677779,0.52731176,
   0.52784552,0.52837907,0.52891241,0.52944555,0.52997847,
   0.53051119,0.53104370,0.53157599,0.53210808,0.53263996,
   0.53317163,0.53370308,0.53423433,0.53476536,0.53529619,
   0.53582680,0.53635720,0.53688739,0.53741737,0.53794713,
   0.53847669,0.53900603,0.53953515,0.54006407,0.54059277,
   0.54112126,0.54164953,0.54217759,0.54270544,0.54323307,
   0.54376049,0.54428769,0.54481468,0.54534145,0.54586801,
   0.54639435,0.54692048,0.54744639,0.54797208,0.54849756,
   0.54902282,0.54954787,0.55007270,0.55059731,0.55112170,
   0.55164588,0.55216983,0.55269357,0.55321710,0.55374040,
   0.55426348,0.55478635,0.55530900,0.55583142,0.55635363,
   0.55687562,0.55739739,0.55791894,0.55844027,0.55896138,
   0.55948226,0.56000293,0.56052337,0.56104360,0.56156360,
   0.56208338,0.56260294,0.56312228,0.56364139,0.56416029,
   0.56467896,0.56519740,0.56571562,0.56623362,0.56675140,
   0.56726895,0.56778628,0.56830339,0.56882027,0.56933692,
   0.56985335,0.57036956,0.57088554,0.57140130,0.57191683,
   0.57243213,0.57294721,0.57346206,0.57397669,0.57449108,
   0.57500526,0.57551920,0.57603292,0.57654641,0.57705967,
   0.57757271,0.57808552,0.57859809,0.57911044,0.57962257,
   0.58013446,0.58064612,0.58115756,0.58166876,0.58217974,
   0.58269048,0.58320100,0.58371129,0.58422134,0.58473117,
   0.58524076,0.58575012,0.58625925,0.58676815,0.58727682,
   0.58778526,0.58829346,0.58880143,0.58930917,0.58981668,
   0.59032395,0.59083100,0.59133780,0.59184438,0.59235072,
   0.59285683,0.59336270,0.59386834,0.59437374,0.59487891,
   0.59538384,0.59588854,0.59639301,0.59689724,0.59740123,
   0.59790499,0.59840851,0.59891179,0.59941484,0.59991766,
   0.60042023,0.60092257,0.60142467,0.60192653,0.60242816,
   0.60292955,0.60343070,0.60393161,0.60443228,0.60493272,
   0.60543291,0.60593287,0.60643259,0.60693207,0.60743130,
   0.60793030,0.60842906,0.60892758,0.60942586,0.60992390,
   0.61042169,0.61091925,0.61141656,0.61191364,0.61241047,
   0.61290706,0.61340341,0.61389951,0.61439538,0.61489100,
   0.61538638,0.61588151,0.61637640,0.61687105,0.61736546,
   0.61785962,0.61835354,0.61884721,0.61934064,0.61983382,
   0.62032676,0.62081946,0.62131191,0.62180411,0.62229607,
   0.62278779,0.62327925,0.62377047,0.62426145,0.62475218,
   0.62524266,0.62573290,0.62622289,0.62671263,0.62720212,
   0.62769137,0.62818036,0.62866911,0.62915762,0.62964587,
   0.63013388,0.63062163,0.63110914,0.63159640,0.63208341,
   0.63257017,0.63305668,0.63354294,0.63402895,0.63451471,
   0.63500021,0.63548547,0.63597048,0.63645524,0.63693974,
   0.63742399,0.63790800,0.63839175,0.63887524,0.63935849,
   0.63984148,0.64032422,0.64080671,0.64128895,0.64177093,
   0.64225266,0.64273413,0.64321535,0.64369632,0.64417703,
   0.64465749,0.64513770,0.64561765,0.64609734,0.64657678,
   0.64705597,0.64753490,0.64801357,0.64849199,0.64897015,
   0.64944805,0.64992570,0.65040309,0.65088023,0.65135711,
   0.65183373,0.65231009,0.65278620,0.65326205,0.65373764,
   0.65421297,0.65468805,0.65516286,0.65563742,0.65611172,
   0.65658576,0.65705954,0.65753306,0.65800632,0.65847933,
   0.65895207,0.65942455,0.65989677,0.66036873,0.66084043,
   0.66131187,0.66178305,0.66225397,0.66272462,0.66319501,
   0.66366515,0.66413502,0.66460462,0.66507397,0.66554305,
   0.66601187,0.66648043,0.66694872,0.66741675,0.66788452,
   0.66835203,0.66881926,0.66928624,0.66975295,0.67021940,
   0.67068558,0.67115150,0.67161715,0.67208254,0.67254766,
   0.67301252,0.67347711,0.67394143,0.67440549,0.67486929,
   0.67533281,0.67579607,0.67625907,0.67672179,0.67718425,
   0.67764644,0.67810837,0.67857002,0.67903141,0.67949253,
   0.67995338,0.68041397,0.68087428,0.68133433,0.68179411,
   0.68225361,0.68271285,0.68317182,0.68363052,0.68408895,
   0.68454711,0.68500500,0.68546262,0.68591997,0.68637704,
   0.68683385,0.68729039,0.68774665,0.68820264,0.68865836,
   0.68911381,0.68956899,0.69002389,0.69047853,0.69093289,
   0.69138697,0.69184079,0.69229433,0.69274760,0.69320059,
   0.69365331,0.69410576,0.69455793,0.69500983,0.69546145,
   0.69591280,0.69636388,0.69681468,0.69726520,0.69771545,
   0.69816542,0.69861512,0.69906454,0.69951369,0.69996256,
   0.70041116,0.70085947,0.70130751,0.70175528,0.70220276,
   0.70264997,0.70309691,0.70354356,0.70398994,0.70443604,
   0.70488186,0.70532740,0.70577267,0.70621765,0.70666236,
   0.70710679,0.70755093,0.70799480,0.70843839,0.70888170,
   0.70932473,0.70976748,0.71020995,0.71065214,0.71109405,
   0.71153568,0.71197703,0.71241810,0.71285888,0.71329939,
   0.71373961,0.71417955,0.71461921,0.71505858,0.71549768,
   0.71593649,0.71637502,0.71681326,0.71725123,0.71768891,
   0.71812630,0.71856342,0.71900025,0.71943679,0.71987305,
   0.72030903,0.72074472,0.72118013,0.72161526,0.72205009,
   0.72248465,0.72291892,0.72335290,0.72378660,0.72422001,
   0.72465314,0.72508598,0.72551853,0.72595080,0.72638278,
   0.72681447,0.72724588,0.72767700,0.72810783,0.72853837,
   0.72896863,0.72939860,0.72982828,0.73025768,0.73068678,
   0.73111560,0.73154413,0.73197237,0.73240032,0.73282798,
   0.73325535,0.73368243,0.73410923,0.73453573,0.73496194,
   0.73538787,0.73581350,0.73623884,0.73666389,0.73708865,
   0.73751312,0.73793730,0.73836119,0.73878478,0.73920809,
   0.73963110,0.74005382,0.74047625,0.74089838,0.74132023,
   0.74174178,0.74216304,0.74258400,0.74300467,0.74342505,
   0.74384513,0.74426493,0.74468442,0.74510363,0.74552254,
   0.74594115,0.74635947,0.74677750,0.74719523,0.74761266,
   0.74802980,0.74844665,0.74886320,0.74927945,0.74969541,
   0.75011107,0.75052644,0.75094151,0.75135628,0.75177076,
   0.75218494,0.75259883,0.75301241,0.75342570,0.75383869,
   0.75425139,0.75466378,0.75507588,0.75548768,0.75589918,
   0.75631039,0.75672129,0.75713190,0.75754220,0.75795221,
   0.75836192,0.75877133,0.75918044,0.75958925,0.75999776,
   0.76040597,0.76081388,0.76122149,0.76162880,0.76203581,
   0.76244252,0.76284892,0.76325503,0.76366083,0.76406634,
   0.76447154,0.76487644,0.76528103,0.76568533,0.76608932,
   0.76649301,0.76689640,0.76729948,0.76770227,0.76810475,
   0.76850692,0.76890879,0.76931036,0.76971163,0.77011259,
   0.77051325,0.77091360,0.77131365,0.77171339,0.77211283,
   0.77251197,0.77291080,0.77330932,0.77370754,0.77410546,
   0.77450307,0.77490037,0.77529736,0.77569406,0.77609044,
   0.77648652,0.77688229,0.77727775,0.77767291,0.77806776,
   0.77846231,0.77885654,0.77925047,0.77964409,0.78003741,
   0.78043041,0.78082311,0.78121550,0.78160758,0.78199935,
   0.78239082,0.78278197,0.78317282,0.78356335,0.78395358,
   0.78434350,0.78473310,0.78512240,0.78551139,0.78590007,
   0.78628844,0.78667649,0.78706424,0.78745168,0.78783880,
   0.78822562,0.78861212,0.78899831,0.78938419,0.78976976,
   0.79015502,0.79053996,0.79092459,0.79130892,0.79169292,
   0.79207662,0.79246000,0.79284307,0.79322583,0.79360827,
   0.79399040,0.79437222,0.79475372,0.79513491,0.79551579,
   0.79589635,0.79627660,0.79665653,0.79703615,0.79741546,
   0.79779444,0.79817312,0.79855148,0.79892952,0.79930725,
   0.79968466,0.80006176,0.80043854,0.80081501,0.80119116,
   0.80156699,0.80194251,0.80231771,0.80269259,0.80306716,
   0.80344141,0.80381534,0.80418895,0.80456225,0.80493523,
   0.80530789,0.80568023,0.80605226,0.80642397,0.80679536,
   0.80716643,0.80753718,0.80790761,0.80827773,0.80864752,
   0.80901700,0.80938616,0.80975499,0.81012351,0.81049171,
   0.81085959,0.81122714,0.81159438,0.81196130,0.81232789,
   0.81269417,0.81306012,0.81342576,0.81379107,0.81415606,
   0.81452073,0.81488508,0.81524911,0.81561281,0.81597619,
   0.81633926,0.81670199,0.81706441,0.81742650,0.81778827,
   0.81814972,0.81851085,0.81887165,0.81923213,0.81959228,
   0.81995211,0.82031162,0.82067081,0.82102967,0.82138820,
   0.82174641,0.82210430,0.82246186,0.82281910,0.82317601,
   0.82353260,0.82388887,0.82424480,0.82460042,0.82495570,
   0.82531066,0.82566530,0.82601961,0.82637359,0.82672725,
   0.82708058,0.82743358,0.82778626,0.82813861,0.82849064,
   0.82884233,0.82919370,0.82954474,0.82989546,0.83024584,
   0.83059590,0.83094564,0.83129504,0.83164411,0.83199286,
   0.83234128,0.83268937,0.83303713,0.83338456,0.83373166,
   0.83407844,0.83442488,0.83477100,0.83511678,0.83546224,
   0.83580737,0.83615216,0.83649663,0.83684076,0.83718457,
   0.83752805,0.83787119,0.83821400,0.83855649,0.83889864,
   0.83924046,0.83958195,0.83992310,0.84026393,0.84060442,
   0.84094459,0.84128442,0.84162392,0.84196308,0.84230192,
   0.84264042,0.84297859,0.84331642,0.84365392,0.84399109,
   0.84432793,0.84466443,0.84500060,0.84533644,0.84567194,
   0.84600711,0.84634195,0.84667645,0.84701061,0.84734444,
   0.84767794,0.84801110,0.84834393,0.84867642,0.84900858,
   0.84934041,0.84967189,0.85000304,0.85033386,0.85066434,
   0.85099449,0.85132430,0.85165377,0.85198291,0.85231171,
   0.85264017,0.85296830,0.85329609,0.85362354,0.85395066,
   0.85427744,0.85460388,0.85492998,0.85525575,0.85558118,
   0.85590627,0.85623103,0.85655544,0.85687952,0.85720326,
   0.85752666,0.85784972,0.85817245,0.85849483,0.85881688,
   0.85913859,0.85945995,0.85978098,0.86010167,0.86042202,
   0.86074203,0.86106170,0.86138103,0.86170002,0.86201867,
   0.86233698,0.86265495,0.86297258,0.86328987,0.86360682,
   0.86392342,0.86423969,0.86455561,0.86487119,0.86518644,
   0.86550134,0.86581589,0.86613011,0.86644398,0.86675752,
   0.86707071,0.86738355,0.86769606,0.86800822,0.86832004,
   0.86863152,0.86894265,0.86925345,0.86956389,0.86987400,
   0.87018376,0.87049318,0.87080225,0.87111098,0.87141937,
   0.87172741,0.87203511,0.87234246,0.87264947,0.87295614,
   0.87326246,0.87356844,0.87387407,0.87417935,0.87448429,
   0.87478889,0.87509314,0.87539704,0.87570060,0.87600382,
   0.87630669,0.87660921,0.87691138,0.87721321,0.87751470,
   0.87781583,0.87811662,0.87841707,0.87871716,0.87901691,
   0.87931632,0.87961537,0.87991408,0.88021244,0.88051045,
   0.88080812,0.88110544,0.88140241,0.88169903,0.88199531,
   0.88229123,0.88258681,0.88288204,0.88317692,0.88347145,
   0.88376564,0.88405947,0.88435296,0.88464609,0.88493888,
   0.88523132,0.88552340,0.88581514,0.88610653,0.88639757,
   0.88668826,0.88697860,0.88726859,0.88755823,0.88784752,
   0.88813645,0.88842504,0.88871328,0.88900116,0.88928870,
   0.88957588,0.88986271,0.89014919,0.89043532,0.89072110,
   0.89100653,0.89129160,0.89157633,0.89186070,0.89214472,
   0.89242838,0.89271170,0.89299466,0.89327727,0.89355953,
   0.89384143,0.89412298,0.89440418,0.89468502,0.89496551,
   0.89524565,0.89552544,0.89580487,0.89608395,0.89636267,
   0.89664104,0.89691906,0.89719672,0.89747403,0.89775098,
   0.89802758,0.89830383,0.89857972,0.89885525,0.89913043,
   0.89940526,0.89967973,0.89995384,0.90022760,0.90050101,
   0.90077406,0.90104675,0.90131909,0.90159107,0.90186269,
   0.90213396,0.90240488,0.90267544,0.90294564,0.90321548,
   0.90348497,0.90375410,0.90402288,0.90429129,0.90455935,
   0.90482706,0.90509440,0.90536139,0.90562802,0.90589430,
   0.90616022,0.90642577,0.90669097,0.90695582,0.90722030,
   0.90748443,0.90774820,0.90801161,0.90827466,0.90853735,
   0.90879969,0.90906166,0.90932328,0.90958454,0.90984544,
   0.91010598,0.91036616,0.91062598,0.91088544,0.91114454,
   0.91140328,0.91166166,0.91191969,0.91217735,0.91243465,
   0.91269159,0.91294817,0.91320440,0.91346026,0.91371576,
   0.91397090,0.91422567,0.91448009,0.91473415,0.91498784,
   0.91524118,0.91549415,0.91574676,0.91599901,0.91625090,
   0.91650243,0.91675359,0.91700439,0.91725484,0.91750491,
   0.91775463,0.91800398,0.91825298,0.91850161,0.91874987,
   0.91899778,0.91924532,0.91949250,0.91973931,0.91998576,
   0.92023185,0.92047758,0.92072294,0.92096794,0.92121257,
   0.92145685,0.92170075,0.92194430,0.92218748,0.92243029,
   0.92267274,0.92291483,0.92315656,0.92339791,0.92363891,
   0.92387954,0.92411980,0.92435970,0.92459924,0.92483841,
   0.92507721,0.92531565,0.92555373,0.92579143,0.92602878,
   0.92626576,0.92650237,0.92673861,0.92697450,0.92721001,
   0.92744516,0.92767994,0.92791436,0.92814841,0.92838209,
   0.92861541,0.92884836,0.92908094,0.92931316,0.92954501,
   0.92977649,0.93000761,0.93023836,0.93046874,0.93069875,
   0.93092840,0.93115768,0.93138659,0.93161513,0.93184331,
   0.93207112,0.93229856,0.93252563,0.93275233,0.93297867,
   0.93320464,0.93343024,0.93365547,0.93388033,0.93410482,
   0.93432895,0.93455270,0.93477609,0.93499911,0.93522176,
   0.93544404,0.93566595,0.93588749,0.93610866,0.93632946,
   0.93654989,0.93676995,0.93698965,0.93720897,0.93742792,
   0.93764650,0.93786472,0.93808256,0.93830003,0.93851713,
   0.93873386,0.93895022,0.93916621,0.93938183,0.93959708,
   0.93981196,0.94002646,0.94024060,0.94045436,0.94066775,
   0.94088077,0.94109342,0.94130570,0.94151761,0.94172914,
   0.94194031,0.94215110,0.94236152,0.94257156,0.94278124,
   0.94299054,0.94319947,0.94340803,0.94361622,0.94382403,
   0.94403147,0.94423854,0.94444523,0.94465155,0.94485750,
   0.94506308,0.94526828,0.94547311,0.94567757,0.94588165,
   0.94608536,0.94628870,0.94649166,0.94669425,0.94689647,
   0.94709831,0.94729978,0.94750087,0.94770159,0.94790194,
   0.94810191,0.94830151,0.94850073,0.94869958,0.94889805,
   0.94909615,0.94929387,0.94949122,0.94968820,0.94988480,
   0.95008102,0.95027687,0.95047235,0.95066745,0.95086217,
   0.95105652,0.95125049,0.95144409,0.95163731,0.95183016,
   0.95202263,0.95221473,0.95240645,0.95259779,0.95278876,
   0.95297935,0.95316956,0.95335940,0.95354886,0.95373795,
   0.95392666,0.95411499,0.95430294,0.95449052,0.95467772,
   0.95486455,0.95505100,0.95523707,0.95542276,0.95560808,
   0.95579302,0.95597758,0.95616177,0.95634557,0.95652900,
   0.95671206,0.95689473,0.95707703,0.95725895,0.95744049,
   0.95762165,0.95780244,0.95798284,0.95816287,0.95834252,
   0.95852179,0.95870069,0.95887920,0.95905734,0.95923510,
   0.95941248,0.95958948,0.95976610,0.95994234,0.96011821,
   0.96029369,0.96046880,0.96064352,0.96081787,0.96099184,
   0.96116543,0.96133864,0.96151147,0.96168392,0.96185599,
   0.96202768,0.96219899,0.96236992,0.96254047,0.96271064,
   0.96288043,0.96304984,0.96321887,0.96338752,0.96355579,
   0.96372368,0.96389119,0.96405832,0.96422507,0.96439144,
   0.96455742,0.96472303,0.96488825,0.96505310,0.96521756,
   0.96538164,0.96554534,0.96570866,0.96587160,0.96603416,
   0.96619634,0.96635813,0.96651954,0.96668057,0.96684122,
   0.96700149,0.96716138,0.96732088,0.96748001,0.96763875,
   0.96779711,0.96795508,0.96811268,0.96826989,0.96842672,
   0.96858317,0.96873923,0.96889491,0.96905021,0.96920513,
   0.96935967,0.96951382,0.96966759,0.96982098,0.96997398,
   0.97012660,0.97027884,0.97043069,0.97058217,0.97073326,
   0.97088396,0.97103428,0.97118422,0.97133378,0.97148295,
   0.97163174,0.97178014,0.97192816,0.97207580,0.97222306,
   0.97236993,0.97251641,0.97266251,0.97280823,0.97295357,
   0.97309852,0.97324308,0.97338726,0.97353106,0.97367447,
   0.97381750,0.97396015,0.97410241,0.97424428,0.97438577,
   0.97452688,0.97466760,0.97480794,0.97494789,0.97508745,
   0.97522663,0.97536543,0.97550384,0.97564187,0.97577951,
   0.97591677,0.97605364,0.97619012,0.97632622,0.97646194,
   0.97659727,0.97673221,0.97686677,0.97700094,0.97713473,
   0.97726813,0.97740114,0.97753377,0.97766602,0.97779787,
   0.97792934,0.97806043,0.97819113,0.97832144,0.97845137,
   0.97858091,0.97871006,0.97883883,0.97896721,0.97909521,
   0.97922282,0.97935004,0.97947687,0.97960332,0.97972938,
   0.97985506,0.97998035,0.98010525,0.98022976,0.98035389,
   0.98047763,0.98060098,0.98072395,0.98084653,0.98096872,
   0.98109052,0.98121194,0.98133297,0.98145361,0.98157387,
   0.98169373,0.98181321,0.98193231,0.98205101,0.98216933,
   0.98228726,0.98240480,0.98252195,0.98263872,0.98275509,
   0.98287108,0.98298668,0.98310190,0.98321672,0.98333116,
   0.98344521,0.98355887,0.98367214,0.98378503,0.98389752,
   0.98400963,0.98412135,0.98423268,0.98434362,0.98445417,
   0.98456434,0.98467412,0.98478350,0.98489250,0.98500111,
   0.98510933,0.98521716,0.98532461,0.98543166,0.98553832,
   0.98564460,0.98575049,0.98585598,0.98596109,0.98606581,
   0.98617014,0.98627408,0.98637763,0.98648079,0.98658357,
   0.98668595,0.98678794,0.98688955,0.98699076,0.98709158,
   0.98719202,0.98729206,0.98739172,0.98749098,0.98758986,
   0.98768835,0.98778644,0.98788415,0.98798146,0.98807839,
   0.98817492,0.98827107,0.98836682,0.98846219,0.98855717,
   0.98865175,0.98874594,0.98883975,0.98893316,0.98902618,
   0.98911882,0.98921106,0.98930291,0.98939437,0.98948544,
   0.98957612,0.98966641,0.98975631,0.98984582,0.98993494,
   0.99002366,0.99011200,0.99019994,0.99028750,0.99037466,
   0.99046143,0.99054781,0.99063380,0.99071940,0.99080461,
   0.99088942,0.99097385,0.99105788,0.99114152,0.99122478,
   0.99130764,0.99139010,0.99147218,0.99155387,0.99163516,
   0.99171607,0.99179658,0.99187670,0.99195642,0.99203576,
   0.99211471,0.99219326,0.99227142,0.99234919,0.99242657,
   0.99250356,0.99258015,0.99265635,0.99273216,0.99280758,
   0.99288261,0.99295724,0.99303149,0.99310534,0.99317880,
   0.99325186,0.99332454,0.99339682,0.99346871,0.99354021,
   0.99361132,0.99368203,0.99375235,0.99382228,0.99389182,
   0.99396096,0.99402971,0.99409807,0.99416604,0.99423361,
   0.99430080,0.99436758,0.99443398,0.99449999,0.99456560,
   0.99463082,0.99469564,0.99476008,0.99482412,0.99488777,
   0.99495102,0.99501388,0.99507635,0.99513843,0.99520012,
   0.99526141,0.99532231,0.99538281,0.99544292,0.99550264,
   0.99556197,0.99562090,0.99567944,0.99573759,0.99579535,
   0.99585271,0.99590967,0.99596625,0.99602243,0.99607822,
   0.99613361,0.99618862,0.99624322,0.99629744,0.99635126,
   0.99640469,0.99645773,0.99651037,0.99656262,0.99661447,
   0.99666593,0.99671700,0.99676768,0.99681796,0.99686784,
   0.99691734,0.99696644,0.99701515,0.99706346,0.99711138,
   0.99715891,0.99720604,0.99725278,0.99729912,0.99734507,
   0.99739063,0.99743579,0.99748056,0.99752494,0.99756892,
   0.99761251,0.99765571,0.99769851,0.99774091,0.99778293,
   0.99782455,0.99786577,0.99790660,0.99794704,0.99798708,
   0.99802673,0.99806599,0.99810485,0.99814332,0.99818139,
   0.99821907,0.99825636,0.99829325,0.99832974,0.99836585,
   0.99840156,0.99843687,0.99847179,0.99850632,0.99854045,
   0.99857419,0.99860753,0.99864048,0.99867303,0.99870520,
   0.99873696,0.99876833,0.99879931,0.99882990,0.99886008,
   0.99888988,0.99891928,0.99894829,0.99897690,0.99900512,
   0.99903294,0.99906037,0.99908740,0.99911404,0.99914029,
   0.99916614,0.99919160,0.99921666,0.99924133,0.99926560,
   0.99928948,0.99931296,0.99933605,0.99935875,0.99938105,
   0.99940295,0.99942446,0.99944558,0.99946630,0.99948663,
   0.99950657,0.99952610,0.99954525,0.99956400,0.99958235,
   0.99960031,0.99961788,0.99963505,0.99965183,0.99966821,
   0.99968419,0.99969979,0.99971498,0.99972979,0.99974420,
   0.99975821,0.99977183,0.99978505,0.99979788,0.99981032,
   0.99982236,0.99983400,0.99984525,0.99985611,0.99986657,
   0.99987664,0.99988631,0.99989559,0.99990447,0.99991296,
   0.99992105,0.99992875,0.99993605,0.99994296,0.99994947,
   0.99995559,0.99996132,0.99996665,0.99997158,0.99997612,
   0.99998027,0.99998402,0.99998737,0.99999033,0.99999290,
   0.99999507,0.99999685,0.99999823,0.99999922,0.99999981,
   1.00000000,0.99999981,0.99999922,0.99999823,0.99999685,
   0.99999507,0.99999290,0.99999033,0.99998737,0.99998402,
   0.99998027,0.99997612,0.99997158,0.99996665,0.99996132,
   0.99995559,0.99994947,0.99994296,0.99993605,0.99992875,
   0.99992105,0.99991296,0.99990447,0.99989559,0.99988631,
   0.99987664,0.99986657,0.99985611,0.99984525,0.99983400,
   0.99982236,0.99981032,0.99979788,0.99978505,0.99977183,
   0.99975821,0.99974420,0.99972979,0.99971498,0.99969979,
   0.99968419,0.99966821,0.99965183,0.99963505,0.99961788,
   0.99960031,0.99958235,0.99956400,0.99954525,0.99952610,
   0.99950657,0.99948663,0.99946630,0.99944558,0.99942446,
   0.99940295,0.99938105,0.99935875,0.99933605,0.99931296,
   0.99928948,0.99926560,0.99924133,0.99921666,0.99919160,
   0.99916614,0.99914029,0.99911404,0.99908740,0.99906037,
   0.99903294,0.99900512,0.99897690,0.99894829,0.99891928,
   0.99888988,0.99886008,0.99882990,0.99879931,0.99876833,
   0.99873696,0.99870520,0.99867303,0.99864048,0.99860753,
   0.99857419,0.99854045,0.99850632,0.99847179,0.99843687,
   0.99840156,0.99836585,0.99832974,0.99829325,0.99825636,
   0.99821907,0.99818139,0.99814332,0.99810485,0.99806599,
   0.99802673,0.99798708,0.99794704,0.99790660,0.99786577,
   0.99782455,0.99778293,0.99774091,0.99769851,0.99765571,
   0.99761251,0.99756892,0.99752494,0.99748056,0.99743579,
   0.99739063,0.99734507,0.99729912,0.99725278,0.99720604,
   0.99715891,0.99711138,0.99706346,0.99701515,0.99696644,
   0.99691734,0.99686784,0.99681796,0.99676768,0.99671700,
   0.99666593,0.99661447,0.99656262,0.99651037,0.99645773,
   0.99640469,0.99635126,0.99629744,0.99624322,0.99618862,
   0.99613361,0.99607822,0.99602243,0.99596625,0.99590967,
   0.99585271,0.99579535,0.99573759,0.99567944,0.99562090,
   0.99556197,0.99550264,0.99544292,0.99538281,0.99532231,
   0.99526141,0.99520012,0.99513843,0.99507635,0.99501388,
   0.99495102,0.99488777,0.99482412,0.99476008,0.99469564,
   0.99463082,0.99456560,0.99449999,0.99443398,0.99436758,
   0.99430080,0.99423361,0.99416604,0.99409807,0.99402971,
   0.99396096,0.99389182,0.99382228,0.99375235,0.99368203,
   0.99361132,0.99354021,0.99346871,0.99339682,0.99332454,
   0.99325186,0.99317880,0.99310534,0.99303149,0.99295724,
   0.99288261,0.99280758,0.99273216,0.99265635,0.99258015,
   0.99250356,0.99242657,0.99234919,0.99227142,0.99219326,
   0.99211471,0.99203576,0.99195642,0.99187670,0.99179658,
   0.99171607,0.99163516,0.99155387,0.99147218,0.99139010,
   0.99130764,0.99122478,0.99114152,0.99105788,0.99097385,
   0.99088942,0.99080461,0.99071940,0.99063380,0.99054781,
   0.99046143,0.99037466,0.99028750,0.99019994,0.99011200,
   0.99002366,0.98993494,0.98984582,0.98975631,0.98966641,
   0.98957612,0.98948544,0.98939437,0.98930291,0.98921106,
   0.98911882,0.98902618,0.98893316,0.98883975,0.98874594,
   0.98865175,0.98855717,0.98846219,0.98836682,0.98827107,
   0.98817492,0.98807839,0.98798146,0.98788415,0.98778644,
   0.98768835,0.98758986,0.98749098,0.98739172,0.98729206,
   0.98719202,0.98709158,0.98699076,0.98688955,0.98678794,
   0.98668595,0.98658357,0.98648079,0.98637763,0.98627408,
   0.98617014,0.98606581,0.98596109,0.98585598,0.98575049,
   0.98564460,0.98553832,0.98543166,0.98532461,0.98521716,
   0.98510933,0.98500111,0.98489250,0.98478350,0.98467412,
   0.98456434,0.98445417,0.98434362,0.98423268,0.98412135,
   0.98400963,0.98389752,0.98378503,0.98367214,0.98355887,
   0.98344521,0.98333116,0.98321672,0.98310190,0.98298668,
   0.98287108,0.98275509,0.98263872,0.98252195,0.98240480,
   0.98228726,0.98216933,0.98205101,0.98193231,0.98181321,
   0.98169373,0.98157387,0.98145361,0.98133297,0.98121194,
   0.98109052,0.98096872,0.98084653,0.98072395,0.98060098,
   0.98047763,0.98035389,0.98022976,0.98010525,0.97998035,
   0.97985506,0.97972938,0.97960332,0.97947687,0.97935004,
   0.97922282,0.97909521,0.97896721,0.97883883,0.97871006,
   0.97858091,0.97845137,0.97832144,0.97819113,0.97806043,
   0.97792934,0.97779787,0.97766602,0.97753377,0.97740114,
   0.97726813,0.97713473,0.97700094,0.97686677,0.97673221,
   0.97659727,0.97646194,0.97632622,0.97619012,0.97605364,
   0.97591677,0.97577951,0.97564187,0.97550384,0.97536543,
   0.97522663,0.97508745,0.97494789,0.97480794,0.97466760,
   0.97452688,0.97438577,0.97424428,0.97410241,0.97396015,
   0.97381750,0.97367447,0.97353106,0.97338726,0.97324308,
   0.97309852,0.97295357,0.97280823,0.97266251,0.97251641,
   0.97236993,0.97222306,0.97207580,0.97192816,0.97178014,
   0.97163174,0.97148295,0.97133378,0.97118422,0.97103428,
   0.97088396,0.97073326,0.97058217,0.97043069,0.97027884,
   0.97012660,0.96997398,0.96982098,0.96966759,0.96951382,
   0.96935967,0.96920513,0.96905021,0.96889491,0.96873923,
   0.96858317,0.96842672,0.96826989,0.96811268,0.96795508,
   0.96779711,0.96763875,0.96748001,0.96732088,0.96716138,
   0.96700149,0.96684122,0.96668058,0.96651954,0.96635813,
   0.96619634,0.96603416,0.96587160,0.96570866,0.96554534,
   0.96538164,0.96521756,0.96505310,0.96488825,0.96472303,
   0.96455742,0.96439144,0.96422507,0.96405832,0.96389119,
   0.96372368,0.96355579,0.96338752,0.96321887,0.96304984,
   0.96288043,0.96271064,0.96254047,0.96236992,0.96219899,
   0.96202768,0.96185599,0.96168392,0.96151147,0.96133864,
   0.96116543,0.96099184,0.96081787,0.96064352,0.96046880,
   0.96029369,0.96011821,0.95994234,0.95976610,0.95958948,
   0.95941248,0.95923510,0.95905734,0.95887920,0.95870069,
   0.95852179,0.95834252,0.95816287,0.95798284,0.95780244,
   0.95762165,0.95744049,0.95725895,0.95707703,0.95689473,
   0.95671206,0.95652900,0.95634557,0.95616177,0.95597758,
   0.95579302,0.95560808,0.95542276,0.95523707,0.95505100,
   0.95486455,0.95467772,0.95449052,0.95430294,0.95411499,
   0.95392666,0.95373795,0.95354886,0.95335940,0.95316956,
   0.95297935,0.95278876,0.95259779,0.95240645,0.95221473,
   0.95202263,0.95183016,0.95163731,0.95144409,0.95125049,
   0.95105652,0.95086217,0.95066745,0.95047235,0.95027687,
   0.95008102,0.94988480,0.94968820,0.94949122,0.94929387,
   0.94909615,0.94889805,0.94869958,0.94850073,0.94830151,
   0.94810191,0.94790194,0.94770159,0.94750087,0.94729978,
   0.94709831,0.94689647,0.94669425,0.94649166,0.94628870,
   0.94608536,0.94588165,0.94567757,0.94547311,0.94526828,
   0.94506308,0.94485750,0.94465155,0.94444523,0.94423854,
   0.94403147,0.94382403,0.94361622,0.94340803,0.94319947,
   0.94299054,0.94278124,0.94257156,0.94236152,0.94215110,
   0.94194031,0.94172914,0.94151761,0.94130570,0.94109342,
   0.94088077,0.94066775,0.94045436,0.94024060,0.94002646,
   0.93981196,0.93959708,0.93938183,0.93916621,0.93895022,
   0.93873386,0.93851713,0.93830003,0.93808256,0.93786472,
   0.93764650,0.93742792,0.93720897,0.93698965,0.93676995,
   0.93654989,0.93632946,0.93610866,0.93588749,0.93566595,
   0.93544404,0.93522176,0.93499911,0.93477609,0.93455270,
   0.93432895,0.93410482,0.93388033,0.93365547,0.93343024,
   0.93320464,0.93297867,0.93275233,0.93252563,0.93229856,
   0.93207112,0.93184331,0.93161513,0.93138659,0.93115768,
   0.93092840,0.93069875,0.93046874,0.93023836,0.93000761,
   0.92977649,0.92954501,0.92931316,0.92908094,0.92884836,
   0.92861541,0.92838209,0.92814841,0.92791436,0.92767994,
   0.92744516,0.92721001,0.92697450,0.92673861,0.92650237,
   0.92626576,0.92602878,0.92579143,0.92555373,0.92531565,
   0.92507721,0.92483841,0.92459924,0.92435970,0.92411980,
   0.92387954,0.92363891,0.92339791,0.92315656,0.92291483,
   0.92267274,0.92243029,0.92218748,0.92194430,0.92170075,
   0.92145685,0.92121257,0.92096794,0.92072294,0.92047758,
   0.92023185,0.91998576,0.91973931,0.91949250,0.91924532,
   0.91899778,0.91874987,0.91850161,0.91825298,0.91800398,
   0.91775463,0.91750491,0.91725484,0.91700439,0.91675359,
   0.91650243,0.91625090,0.91599901,0.91574676,0.91549415,
   0.91524118,0.91498784,0.91473415,0.91448009,0.91422567,
   0.91397090,0.91371576,0.91346026,0.91320440,0.91294817,
   0.91269159,0.91243465,0.91217735,0.91191969,0.91166166,
   0.91140328,0.91114454,0.91088544,0.91062598,0.91036616,
   0.91010598,0.90984544,0.90958454,0.90932328,0.90906166,
   0.90879969,0.90853735,0.90827466,0.90801161,0.90774820,
   0.90748443,0.90722030,0.90695582,0.90669097,0.90642577,
   0.90616022,0.90589430,0.90562802,0.90536139,0.90509440,
   0.90482706,0.90455935,0.90429129,0.90402288,0.90375410,
   0.90348497,0.90321548,0.90294564,0.90267544,0.90240488,
   0.90213396,0.90186269,0.90159107,0.90131909,0.90104675,
   0.90077406,0.90050101,0.90022760,0.89995384,0.89967973,
   0.89940526,0.89913043,0.89885525,0.89857972,0.89830383,
   0.89802758,0.89775098,0.89747403,0.89719672,0.89691906,
   0.89664104,0.89636267,0.89608395,0.89580487,0.89552544,
   0.89524565,0.89496551,0.89468502,0.89440418,0.89412298,
   0.89384143,0.89355953,0.89327727,0.89299466,0.89271170,
   0.89242838,0.89214472,0.89186070,0.89157633,0.89129160,
   0.89100653,0.89072110,0.89043532,0.89014919,0.88986271,
   0.88957588,0.88928870,0.88900116,0.88871328,0.88842504,
   0.88813645,0.88784752,0.88755823,0.88726859,0.88697860,
   0.88668826,0.88639757,0.88610653,0.88581514,0.88552340,
   0.88523132,0.88493888,0.88464609,0.88435296,0.88405947,
   0.88376564,0.88347145,0.88317692,0.88288204,0.88258681,
   0.88229123,0.88199531,0.88169903,0.88140241,0.88110544,
   0.88080812,0.88051045,0.88021244,0.87991408,0.87961537,
   0.87931632,0.87901691,0.87871716,0.87841707,0.87811662,
   0.87781583,0.87751470,0.87721321,0.87691138,0.87660921,
   0.87630669,0.87600382,0.87570060,0.87539704,0.87509314,
   0.87478889,0.87448429,0.87417935,0.87387407,0.87356844,
   0.87326246,0.87295614,0.87264947,0.87234246,0.87203511,
   0.87172741,0.87141937,0.87111098,0.87080225,0.87049318,
   0.87018376,0.86987400,0.86956389,0.86925345,0.86894265,
   0.86863152,0.86832004,0.86800822,0.86769606,0.86738355,
   0.86707071,0.86675752,0.86644398,0.86613011,0.86581589,
   0.86550134,0.86518644,0.86487119,0.86455561,0.86423969,
   0.86392342,0.86360682,0.86328987,0.86297258,0.86265495,
   0.86233698,0.86201867,0.86170002,0.86138103,0.86106170,
   0.86074203,0.86042202,0.86010167,0.85978098,0.85945995,
   0.85913859,0.85881688,0.85849483,0.85817245,0.85784972,
   0.85752666,0.85720326,0.85687952,0.85655544,0.85623103,
   0.85590627,0.85558118,0.85525575,0.85492998,0.85460388,
   0.85427744,0.85395066,0.85362354,0.85329609,0.85296830,
   0.85264017,0.85231171,0.85198291,0.85165377,0.85132430,
   0.85099449,0.85066434,0.85033386,0.85000304,0.84967189,
   0.84934041,0.84900858,0.84867642,0.84834393,0.84801110,
   0.84767794,0.84734444,0.84701061,0.84667645,0.84634195,
   0.84600711,0.84567194,0.84533644,0.84500060,0.84466443,
   0.84432793,0.84399109,0.84365392,0.84331642,0.84297859,
   0.84264042,0.84230192,0.84196308,0.84162392,0.84128442,
   0.84094459,0.84060442,0.84026393,0.83992310,0.83958195,
   0.83924046,0.83889864,0.83855649,0.83821400,0.83787119,
   0.83752805,0.83718457,0.83684076,0.83649663,0.83615216,
   0.83580737,0.83546224,0.83511678,0.83477100,0.83442488,
   0.83407844,0.83373166,0.83338456,0.83303713,0.83268937,
   0.83234128,0.83199286,0.83164411,0.83129504,0.83094564,
   0.83059590,0.83024584,0.82989546,0.82954474,0.82919370,
   0.82884233,0.82849064,0.82813861,0.82778626,0.82743358,
   0.82708058,0.82672725,0.82637359,0.82601961,0.82566530,
   0.82531066,0.82495570,0.82460042,0.82424480,0.82388887,
   0.82353260,0.82317601,0.82281910,0.82246186,0.82210430,
   0.82174641,0.82138820,0.82102967,0.82067081,0.82031162,
   0.81995211,0.81959228,0.81923213,0.81887165,0.81851085,
   0.81814972,0.81778827,0.81742650,0.81706441,0.81670199,
   0.81633926,0.81597619,0.81561281,0.81524911,0.81488508,
   0.81452073,0.81415606,0.81379107,0.81342576,0.81306012,
   0.81269417,0.81232789,0.81196130,0.81159438,0.81122714,
   0.81085959,0.81049171,0.81012351,0.80975499,0.80938616,
   0.80901700,0.80864752,0.80827773,0.80790761,0.80753718,
   0.80716643,0.80679536,0.80642397,0.80605226,0.80568023,
   0.80530789,0.80493523,0.80456225,0.80418895,0.80381534,
   0.80344141,0.80306716,0.80269259,0.80231771,0.80194251,
   0.80156699,0.80119116,0.80081501,0.80043854,0.80006176,
   0.79968466,0.79930725,0.79892952,0.79855148,0.79817312,
   0.79779444,0.79741546,0.79703615,0.79665653,0.79627660,
   0.79589635,0.79551579,0.79513491,0.79475372,0.79437222,
   0.79399040,0.79360827,0.79322583,0.79284307,0.79246000,
   0.79207662,0.79169292,0.79130892,0.79092460,0.79053996,
   0.79015502,0.78976976,0.78938419,0.78899831,0.78861212,
   0.78822562,0.78783880,0.78745168,0.78706424,0.78667649,
   0.78628844,0.78590007,0.78551139,0.78512240,0.78473310,
   0.78434350,0.78395358,0.78356335,0.78317282,0.78278197,
   0.78239082,0.78199935,0.78160758,0.78121550,0.78082311,
   0.78043041,0.78003741,0.77964409,0.77925047,0.77885654,
   0.77846231,0.77806776,0.77767291,0.77727775,0.77688229,
   0.77648652,0.77609044,0.77569406,0.77529736,0.77490037,
   0.77450307,0.77410546,0.77370754,0.77330932,0.77291080,
   0.77251197,0.77211283,0.77171339,0.77131365,0.77091360,
   0.77051325,0.77011259,0.76971163,0.76931036,0.76890879,
   0.76850692,0.76810475,0.76770227,0.76729948,0.76689640,
   0.76649301,0.76608932,0.76568533,0.76528103,0.76487644,
   0.76447154,0.76406634,0.76366083,0.76325503,0.76284892,
   0.76244252,0.76203581,0.76162880,0.76122149,0.76081388,
   0.76040597,0.75999776,0.75958925,0.75918044,0.75877133,
   0.75836192,0.75795221,0.75754220,0.75713190,0.75672129,
   0.75631039,0.75589918,0.75548768,0.75507588,0.75466378,
   0.75425139,0.75383869,0.75342570,0.75301241,0.75259883,
   0.75218494,0.75177076,0.75135628,0.75094151,0.75052644,
   0.75011107,0.74969541,0.74927945,0.74886320,0.74844665,
   0.74802980,0.74761266,0.74719523,0.74677750,0.74635947,
   0.74594115,0.74552254,0.74510363,0.74468442,0.74426493,
   0.74384513,0.74342505,0.74300467,0.74258400,0.74216304,
   0.74174178,0.74132023,0.74089838,0.74047625,0.74005382,
   0.73963110,0.73920809,0.73878478,0.73836119,0.73793730,
   0.73751312,0.73708865,0.73666389,0.73623884,0.73581350,
   0.73538787,0.73496194,0.73453573,0.73410923,0.73368243,
   0.73325535,0.73282798,0.73240032,0.73197237,0.73154413,
   0.73111560,0.73068678,0.73025768,0.72982828,0.72939860,
   0.72896863,0.72853837,0.72810783,0.72767700,0.72724588,
   0.72681447,0.72638278,0.72595080,0.72551853,0.72508598,
   0.72465314,0.72422001,0.72378660,0.72335290,0.72291892,
   0.72248465,0.72205009,0.72161526,0.72118013,0.72074472,
   0.72030903,0.71987305,0.71943679,0.71900025,0.71856342,
   0.71812630,0.71768891,0.71725123,0.71681326,0.71637502,
   0.71593649,0.71549768,0.71505858,0.71461921,0.71417955,
   0.71373961,0.71329939,0.71285888,0.71241810,0.71197703,
   0.71153568,0.71109405,0.71065214,0.71020995,0.70976748,
   0.70932473,0.70888170,0.70843839,0.70799480,0.70755093,
   0.70710679,0.70666236,0.70621765,0.70577267,0.70532740,
   0.70488186,0.70443604,0.70398994,0.70354356,0.70309691,
   0.70264997,0.70220277,0.70175528,0.70130751,0.70085947,
   0.70041116,0.69996256,0.69951369,0.69906454,0.69861512,
   0.69816542,0.69771545,0.69726520,0.69681468,0.69636388,
   0.69591280,0.69546145,0.69500983,0.69455793,0.69410576,
   0.69365331,0.69320059,0.69274760,0.69229433,0.69184079,
   0.69138697,0.69093289,0.69047853,0.69002390,0.68956899,
   0.68911381,0.68865836,0.68820264,0.68774665,0.68729039,
   0.68683385,0.68637704,0.68591997,0.68546262,0.68500500,
   0.68454711,0.68408895,0.68363052,0.68317182,0.68271285,
   0.68225361,0.68179411,0.68133433,0.68087428,0.68041397,
   0.67995338,0.67949253,0.67903141,0.67857002,0.67810837,
   0.67764644,0.67718425,0.67672179,0.67625907,0.67579607,
   0.67533281,0.67486929,0.67440549,0.67394143,0.67347711,
   0.67301252,0.67254766,0.67208254,0.67161715,0.67115150,
   0.67068558,0.67021940,0.66975295,0.66928624,0.66881926,
   0.66835203,0.66788452,0.66741675,0.66694872,0.66648043,
   0.66601187,0.66554305,0.66507397,0.66460462,0.66413502,
   0.66366515,0.66319501,0.66272462,0.66225397,0.66178305,
   0.66131187,0.66084043,0.66036873,0.65989677,0.65942455,
   0.65895207,0.65847933,0.65800632,0.65753306,0.65705954,
   0.65658576,0.65611172,0.65563742,0.65516286,0.65468805,
   0.65421297,0.65373764,0.65326205,0.65278620,0.65231009,
   0.65183373,0.65135711,0.65088023,0.65040309,0.64992570,
   0.64944805,0.64897015,0.64849199,0.64801357,0.64753490,
   0.64705597,0.64657678,0.64609734,0.64561765,0.64513770,
   0.64465749,0.64417703,0.64369632,0.64321535,0.64273413,
   0.64225266,0.64177093,0.64128895,0.64080671,0.64032422,
   0.63984148,0.63935849,0.63887524,0.63839175,0.63790800,
   0.63742399,0.63693974,0.63645524,0.63597048,0.63548547,
   0.63500021,0.63451471,0.63402895,0.63354294,0.63305668,
   0.63257017,0.63208341,0.63159640,0.63110914,0.63062163,
   0.63013388,0.62964587,0.62915762,0.62866911,0.62818036,
   0.62769137,0.62720212,0.62671263,0.62622289,0.62573290,
   0.62524266,0.62475218,0.62426145,0.62377047,0.62327925,
   0.62278779,0.62229607,0.62180411,0.62131191,0.62081946,
   0.62032676,0.61983382,0.61934064,0.61884721,0.61835354,
   0.61785962,0.61736546,0.61687105,0.61637640,0.61588151,
   0.61538638,0.61489100,0.61439538,0.61389951,0.61340341,
   0.61290706,0.61241047,0.61191364,0.61141656,0.61091925,
   0.61042169,0.60992390,0.60942586,0.60892758,0.60842906,
   0.60793030,0.60743130,0.60693207,0.60643259,0.60593287,
   0.60543291,0.60493272,0.60443228,0.60393161,0.60343070,
   0.60292955,0.60242816,0.60192653,0.60142467,0.60092257,
   0.60042023,0.59991766,0.59941484,0.59891179,0.59840851,
   0.59790499,0.59740123,0.59689724,0.59639301,0.59588854,
   0.59538384,0.59487891,0.59437374,0.59386834,0.59336270,
   0.59285683,0.59235072,0.59184438,0.59133780,0.59083100,
   0.59032395,0.58981668,0.58930917,0.58880143,0.58829346,
   0.58778526,0.58727682,0.58676815,0.58625925,0.58575012,
   0.58524076,0.58473117,0.58422134,0.58371129,0.58320100,
   0.58269048,0.58217974,0.58166876,0.58115756,0.58064612,
   0.58013446,0.57962257,0.57911044,0.57859809,0.57808552,
   0.57757271,0.57705967,0.57654641,0.57603292,0.57551920,
   0.57500526,0.57449108,0.57397669,0.57346206,0.57294721,
   0.57243213,0.57191683,0.57140130,0.57088554,0.57036956,
   0.56985335,0.56933692,0.56882027,0.56830339,0.56778628,
   0.56726895,0.56675140,0.56623362,0.56571562,0.56519740,
   0.56467896,0.56416029,0.56364139,0.56312228,0.56260294,
   0.56208338,0.56156360,0.56104360,0.56052338,0.56000293,
   0.55948226,0.55896138,0.55844027,0.55791894,0.55739739,
   0.55687562,0.55635363,0.55583142,0.55530900,0.55478635,
   0.55426348,0.55374040,0.55321710,0.55269357,0.55216983,
   0.55164588,0.55112170,0.55059731,0.55007270,0.54954787,
   0.54902282,0.54849756,0.54797208,0.54744639,0.54692048,
   0.54639435,0.54586801,0.54534145,0.54481468,0.54428769,
   0.54376049,0.54323307,0.54270544,0.54217759,0.54164953,
   0.54112126,0.54059277,0.54006407,0.53953515,0.53900603,
   0.53847669,0.53794713,0.53741737,0.53688739,0.53635720,
   0.53582680,0.53529619,0.53476536,0.53423433,0.53370308,
   0.53317163,0.53263996,0.53210808,0.53157599,0.53104370,
   0.53051119,0.52997847,0.52944555,0.52891241,0.52837907,
   0.52784552,0.52731176,0.52677779,0.52624361,0.52570923,
   0.52517463,0.52463984,0.52410483,0.52356962,0.52303420,
   0.52249857,0.52196274,0.52142670,0.52089045,0.52035400,
   0.51981735,0.51928049,0.51874342,0.51820615,0.51766867,
   0.51713100,0.51659311,0.51605502,0.51551673,0.51497824,
   0.51443954,0.51390064,0.51336153,0.51282223,0.51228272,
   0.51174301,0.51120309,0.51066298,0.51012266,0.50958214,
   0.50904142,0.50850050,0.50795938,0.50741806,0.50687653,
   0.50633481,0.50579289,0.50525077,0.50470845,0.50416593,
   0.50362321,0.50308029,0.50253717,0.50199386,0.50145034,
   0.50090663,0.50036272,0.49981861,0.49927431,0.49872981,
   0.49818511,0.49764022,0.49709512,0.49654984,0.49600435,
   0.49545867,0.49491280,0.49436673,0.49382046,0.49327400,
   0.49272735,0.49218050,0.49163345,0.49108621,0.49053878,
   0.48999116,0.48944334,0.48889533,0.48834712,0.48779872,
   0.48725013,0.48670135,0.48615237,0.48560321,0.48505385,
   0.48450430,0.48395455,0.48340462,0.48285450,0.48230418,
   0.48175368,0.48120298,0.48065210,0.48010103,0.47954976,
   0.47899831,0.47844667,0.47789483,0.47734281,0.47679061,
   0.47623821,0.47568562,0.47513285,0.47457989,0.47402674,
   0.47347341,0.47291989,0.47236618,0.47181229,0.47125820,
   0.47070394,0.47014948,0.46959485,0.46904002,0.46848501,
   0.46792982,0.46737444,0.46681888,0.46626313,0.46570720,
   0.46515108,0.46459478,0.46403830,0.46348164,0.46292479,
   0.46236776,0.46181054,0.46125315,0.46069557,0.46013781,
   0.45957987,0.45902174,0.45846344,0.45790495,0.45734629,
   0.45678744,0.45622841,0.45566920,0.45510982,0.45455025,
   0.45399050,0.45343058,0.45287047,0.45231019,0.45174973,
   0.45118909,0.45062827,0.45006727,0.44950610,0.44894475,
   0.44838322,0.44782152,0.44725963,0.44669757,0.44613534,
   0.44557293,0.44501034,0.44444758,0.44388464,0.44332152,
   0.44275824,0.44219477,0.44163113,0.44106732,0.44050334,
   0.43993917,0.43937484,0.43881033,0.43824565,0.43768080,
   0.43711577,0.43655057,0.43598520,0.43541966,0.43485394,
   0.43428805,0.43372200,0.43315577,0.43258936,0.43202279,
   0.43145605,0.43088914,0.43032206,0.42975480,0.42918738,
   0.42861979,0.42805203,0.42748410,0.42691600,0.42634773,
   0.42577930,0.42521069,0.42464192,0.42407298,0.42350388,
   0.42293460,0.42236516,0.42179555,0.42122578,0.42065584,
   0.42008573,0.41951546,0.41894502,0.41837442,0.41780365,
   0.41723272,0.41666162,0.41609036,0.41551893,0.41494734,
   0.41437559,0.41380367,0.41323159,0.41265934,0.41208693,
   0.41151436,0.41094163,0.41036874,0.40979568,0.40922246,
   0.40864908,0.40807554,0.40750184,0.40692797,0.40635395,
   0.40577976,0.40520542,0.40463091,0.40405625,0.40348142,
   0.40290644,0.40233130,0.40175600,0.40118054,0.40060492,
   0.40002914,0.39945321,0.39887712,0.39830087,0.39772446,
   0.39714790,0.39657117,0.39599430,0.39541726,0.39484007,
   0.39426273,0.39368523,0.39310757,0.39252976,0.39195179,
   0.39137367,0.39079540,0.39021697,0.38963838,0.38905964,
   0.38848075,0.38790171,0.38732251,0.38674316,0.38616365,
   0.38558400,0.38500419,0.38442423,0.38384412,0.38326385,
   0.38268344,0.38210287,0.38152215,0.38094129,0.38036027,
   0.37977910,0.37919778,0.37861631,0.37803470,0.37745293,
   0.37687102,0.37628895,0.37570674,0.37512438,0.37454187,
   0.37395921,0.37337641,0.37279345,0.37221035,0.37162711,
   0.37104372,0.37046018,0.36987649,0.36929266,0.36870868,
   0.36812456,0.36754029,0.36695588,0.36637132,0.36578661,
   0.36520177,0.36461678,0.36403164,0.36344636,0.36286094,
   0.36227537,0.36168966,0.36110381,0.36051782,0.35993168,
   0.35934540,0.35875898,0.35817242,0.35758571,0.35699887,
   0.35641188,0.35582476,0.35523749,0.35465008,0.35406254,
   0.35347485,0.35288702,0.35229906,0.35171095,0.35112271,
   0.35053433,0.34994580,0.34935715,0.34876835,0.34817941,
   0.34759034,0.34700113,0.34641179,0.34582230,0.34523268,
   0.34464293,0.34405304,0.34346301,0.34287285,0.34228255,
   0.34169211,0.34110154,0.34051084,0.33992000,0.33932903,
   0.33873793,0.33814669,0.33755531,0.33696381,0.33637217,
   0.33578039,0.33518849,0.33459645,0.33400428,0.33341198,
   0.33281955,0.33222699,0.33163429,0.33104146,0.33044851,
   0.32985542,0.32926220,0.32866885,0.32807538,0.32748177,
   0.32688803,0.32629417,0.32570018,0.32510605,0.32451180,
   0.32391742,0.32332292,0.32272828,0.32213352,0.32153863,
   0.32094361,0.32034847,0.31975320,0.31915781,0.31856229,
   0.31796664,0.31737087,0.31677497,0.31617894,0.31558280,
   0.31498652,0.31439013,0.31379361,0.31319696,0.31260019,
   0.31200330,0.31140629,0.31080915,0.31021189,0.30961450,
   0.30901700,0.30841937,0.30782162,0.30722375,0.30662576,
   0.30602765,0.30542941,0.30483106,0.30423258,0.30363399,
   0.30303527,0.30243644,0.30183749,0.30123841,0.30063922,
   0.30003991,0.29944048,0.29884094,0.29824127,0.29764149,
   0.29704159,0.29644157,0.29584143,0.29524118,0.29464081,
   0.29404033,0.29343973,0.29283901,0.29223818,0.29163723,
   0.29103617,0.29043499,0.28983370,0.28923230,0.28863078,
   0.28802914,0.28742739,0.28682553,0.28622356,0.28562147,
   0.28501927,0.28441695,0.28381453,0.28321199,0.28260934,
   0.28200658,0.28140371,0.28080073,0.28019763,0.27959443,
   0.27899111,0.27838769,0.27778415,0.27718051,0.27657675,
   0.27597289,0.27536892,0.27476483,0.27416064,0.27355635,
   0.27295194,0.27234743,0.27174281,0.27113808,0.27053324,
   0.26992830,0.26932325,0.26871810,0.26811283,0.26750747,
   0.26690199,0.26629642,0.26569073,0.26508494,0.26447905,
   0.26387305,0.26326695,0.26266075,0.26205444,0.26144803,
   0.26084151,0.26023489,0.25962817,0.25902135,0.25841442,
   0.25780739,0.25720026,0.25659303,0.25598570,0.25537827,
   0.25477073,0.25416310,0.25355536,0.25294752,0.25233959,
   0.25173155,0.25112342,0.25051519,0.24990685,0.24929842,
   0.24868989,0.24808126,0.24747254,0.24686372,0.24625479,
   0.24564578,0.24503666,0.24442745,0.24381814,0.24320874,
   0.24259924,0.24198964,0.24137995,0.24077016,0.24016028,
   0.23955030,0.23894023,0.23833006,0.23771980,0.23710945,
   0.23649900,0.23588846,0.23527783,0.23466710,0.23405628,
   0.23344537,0.23283436,0.23222327,0.23161208,0.23100080,
   0.23038943,0.22977797,0.22916642,0.22855478,0.22794304,
   0.22733122,0.22671931,0.22610731,0.22549522,0.22488303,
   0.22427077,0.22365841,0.22304596,0.22243343,0.22182081,
   0.22120810,0.22059530,0.21998242,0.21936945,0.21875639,
   0.21814325,0.21753002,0.21691670,0.21630330,0.21568981,
   0.21507624,0.21446259,0.21384884,0.21323502,0.21262111,
   0.21200711,0.21139304,0.21077888,0.21016463,0.20955031,
   0.20893590,0.20832140,0.20770683,0.20709217,0.20647743,
   0.20586261,0.20524771,0.20463273,0.20401767,0.20340252,
   0.20278730,0.20217200,0.20155661,0.20094115,0.20032561,
   0.19970999,0.19909428,0.19847851,0.19786265,0.19724671,
   0.19663070,0.19601461,0.19539844,0.19478219,0.19416587,
   0.19354947,0.19293300,0.19231645,0.19169982,0.19108311,
   0.19046634,0.18984948,0.18923255,0.18861555,0.18799847,
   0.18738132,0.18676409,0.18614679,0.18552942,0.18491197,
   0.18429445,0.18367686,0.18305920,0.18244146,0.18182365,
   0.18120577,0.18058782,0.17996979,0.17935170,0.17873353,
   0.17811530,0.17749699,0.17687861,0.17626017,0.17564165,
   0.17502306,0.17440441,0.17378569,0.17316689,0.17254803,
   0.17192911,0.17131011,0.17069104,0.17007191,0.16945272,
   0.16883345,0.16821412,0.16759472,0.16697525,0.16635572,
   0.16573613,0.16511647,0.16449674,0.16387695,0.16325709,
   0.16263717,0.16201718,0.16139714,0.16077702,0.16015685,
   0.15953661,0.15891630,0.15829594,0.15767551,0.15705502,
   0.15643447,0.15581386,0.15519318,0.15457244,0.15395165,
   0.15333079,0.15270987,0.15208889,0.15146785,0.15084675,
   0.15022559,0.14960438,0.14898310,0.14836176,0.14774037,
   0.14711892,0.14649741,0.14587584,0.14525421,0.14463253,
   0.14401079,0.14338899,0.14276714,0.14214523,0.14152326,
   0.14090124,0.14027916,0.13965703,0.13903484,0.13841259,
   0.13779030,0.13716794,0.13654554,0.13592308,0.13530056,
   0.13467799,0.13405537,0.13343270,0.13280997,0.13218720,
   0.13156436,0.13094148,0.13031855,0.12969556,0.12907252,
   0.12844944,0.12782630,0.12720311,0.12657987,0.12595658,
   0.12533324,0.12470985,0.12408641,0.12346292,0.12283939,
   0.12221580,0.12159217,0.12096849,0.12034476,0.11972099,
   0.11909716,0.11847330,0.11784938,0.11722541,0.11660141,
   0.11597735,0.11535325,0.11472910,0.11410491,0.11348067,
   0.11285639,0.11223206,0.11160769,0.11098328,0.11035882,
   0.10973432,0.10910977,0.10848518,0.10786055,0.10723588,
   0.10661116,0.10598640,0.10536160,0.10473676,0.10411187,
   0.10348695,0.10286198,0.10223698,0.10161193,0.10098685,
   0.10036172,0.09973655,0.09911135,0.09848610,0.09786082,
   0.09723550,0.09661014,0.09598474,0.09535930,0.09473383,
   0.09410832,0.09348277,0.09285718,0.09223156,0.09160590,
   0.09098021,0.09035448,0.08972871,0.08910291,0.08847707,
   0.08785120,0.08722529,0.08659935,0.08597338,0.08534737,
   0.08472133,0.08409525,0.08346914,0.08284300,0.08221682,
   0.08159062,0.08096438,0.08033811,0.07971180,0.07908547,
   0.07845910,0.07783270,0.07720628,0.07657982,0.07595333,
   0.07532681,0.07470026,0.07407368,0.07344708,0.07282044,
   0.07219378,0.07156708,0.07094036,0.07031361,0.06968684,
   0.06906003,0.06843320,0.06780634,0.06717945,0.06655254,
   0.06592560,0.06529864,0.06467165,0.06404463,0.06341759,
   0.06279052,0.06216343,0.06153632,0.06090918,0.06028201,
   0.05965483,0.05902762,0.05840038,0.05777312,0.05714584,
   0.05651854,0.05589121,0.05526387,0.05463650,0.05400911,
   0.05338169,0.05275426,0.05212681,0.05149933,0.05087184,
   0.05024432,0.04961679,0.04898923,0.04836166,0.04773407,
   0.04710646,0.04647883,0.04585118,0.04522351,0.04459583,
   0.04396812,0.04334040,0.04271267,0.04208491,0.04145714,
   0.04082936,0.04020155,0.03957374,0.03894590,0.03831805,
   0.03769019,0.03706231,0.03643441,0.03580651,0.03517858,
   0.03455065,0.03392270,0.03329473,0.03266676,0.03203877,
   0.03141076,0.03078275,0.03015472,0.02952668,0.02889863,
   0.02827057,0.02764250,0.02701441,0.02638632,0.02575822,
   0.02513010,0.02450198,0.02387384,0.02324570,0.02261754,
   0.02198938,0.02136121,0.02073303,0.02010484,0.01947665,
   0.01884844,0.01822023,0.01759202,0.01696379,0.01633556,
   0.01570732,0.01507908,0.01445083,0.01382257,0.01319431,
   0.01256604,0.01193777,0.01130950,0.01068122,0.01005293,
   0.00942464,0.00879635,0.00816805,0.00753976,0.00691145,
   0.00628315,0.00565484,0.00502653,0.00439822,0.00376991,
   0.00314159,0.00251328,0.00188496,0.00125664,0.00062832,
   0.00000000,-0.00062831,-0.00125663,-0.00188495,-0.00251327,
   -0.00314158,-0.00376990,-0.00439821,-0.00502652,-0.00565483,
   -0.00628314,-0.00691144,-0.00753975,-0.00816805,-0.00879634,
   -0.00942463,-0.01005292,-0.01068121,-0.01130949,-0.01193776,
   -0.01256603,-0.01319430,-0.01382256,-0.01445082,-0.01507907,
   -0.01570731,-0.01633555,-0.01696378,-0.01759201,-0.01822022,
   -0.01884843,-0.01947664,-0.02010483,-0.02073302,-0.02136120,
   -0.02198937,-0.02261753,-0.02324569,-0.02387383,-0.02450197,
   -0.02513009,-0.02575821,-0.02638631,-0.02701441,-0.02764249,
   -0.02827056,-0.02889862,-0.02952667,-0.03015471,-0.03078274,
   -0.03141075,-0.03203876,-0.03266675,-0.03329472,-0.03392269,
   -0.03455064,-0.03517857,-0.03580650,-0.03643440,-0.03706230,
   -0.03769018,-0.03831804,-0.03894589,-0.03957373,-0.04020154,
   -0.04082935,-0.04145713,-0.04208490,-0.04271266,-0.04334039,
   -0.04396811,-0.04459582,-0.04522350,-0.04585117,-0.04647882,
   -0.04710645,-0.04773406,-0.04836165,-0.04898922,-0.04961678,
   -0.05024431,-0.05087183,-0.05149932,-0.05212680,-0.05275425,
   -0.05338168,-0.05400910,-0.05463649,-0.05526386,-0.05589120,
   -0.05651853,-0.05714583,-0.05777311,-0.05840037,-0.05902761,
   -0.05965482,-0.06028200,-0.06090917,-0.06153631,-0.06216342,
   -0.06279051,-0.06341758,-0.06404462,-0.06467164,-0.06529863,
   -0.06592559,-0.06655253,-0.06717944,-0.06780633,-0.06843319,
   -0.06906002,-0.06968683,-0.07031360,-0.07094035,-0.07156707,
   -0.07219377,-0.07282043,-0.07344707,-0.07407367,-0.07470025,
   -0.07532680,-0.07595332,-0.07657981,-0.07720627,-0.07783269,
   -0.07845909,-0.07908546,-0.07971179,-0.08033810,-0.08096437,
   -0.08159061,-0.08221681,-0.08284299,-0.08346913,-0.08409524,
   -0.08472132,-0.08534736,-0.08597337,-0.08659934,-0.08722529,
   -0.08785119,-0.08847706,-0.08910290,-0.08972870,-0.09035447,
   -0.09098020,-0.09160589,-0.09223155,-0.09285717,-0.09348276,
   -0.09410831,-0.09473382,-0.09535929,-0.09598473,-0.09661013,
   -0.09723549,-0.09786081,-0.09848609,-0.09911134,-0.09973654,
   -0.10036171,-0.10098684,-0.10161192,-0.10223697,-0.10286197,
   -0.10348694,-0.10411186,-0.10473675,-0.10536159,-0.10598639,
   -0.10661115,-0.10723587,-0.10786054,-0.10848517,-0.10910976,
   -0.10973431,-0.11035881,-0.11098327,-0.11160768,-0.11223205,
   -0.11285638,-0.11348066,-0.11410490,-0.11472909,-0.11535324,
   -0.11597734,-0.11660140,-0.11722541,-0.11784937,-0.11847329,
   -0.11909716,-0.11972098,-0.12034475,-0.12096848,-0.12159216,
   -0.12221580,-0.12283938,-0.12346291,-0.12408640,-0.12470984,
   -0.12533323,-0.12595657,-0.12657986,-0.12720310,-0.12782629,
   -0.12844943,-0.12907251,-0.12969555,-0.13031854,-0.13094147,
   -0.13156435,-0.13218719,-0.13280996,-0.13343269,-0.13405536,
   -0.13467798,-0.13530055,-0.13592307,-0.13654553,-0.13716793,
   -0.13779029,-0.13841258,-0.13903483,-0.13965702,-0.14027915,
   -0.14090123,-0.14152325,-0.14214522,-0.14276713,-0.14338898,
   -0.14401078,-0.14463252,-0.14525420,-0.14587583,-0.14649740,
   -0.14711891,-0.14774036,-0.14836175,-0.14898309,-0.14960437,
   -0.15022558,-0.15084674,-0.15146784,-0.15208888,-0.15270986,
   -0.15333078,-0.15395164,-0.15457243,-0.15519317,-0.15581385,
   -0.15643446,-0.15705501,-0.15767550,-0.15829593,-0.15891629,
   -0.15953660,-0.16015684,-0.16077701,-0.16139713,-0.16201718,
   -0.16263716,-0.16325708,-0.16387694,-0.16449673,-0.16511646,
   -0.16573612,-0.16635571,-0.16697524,-0.16759471,-0.16821411,
   -0.16883344,-0.16945271,-0.17007190,-0.17069104,-0.17131010,
   -0.17192910,-0.17254802,-0.17316688,-0.17378568,-0.17440440,
   -0.17502305,-0.17564164,-0.17626016,-0.17687860,-0.17749698,
   -0.17811529,-0.17873352,-0.17935169,-0.17996978,-0.18058781,
   -0.18120576,-0.18182364,-0.18244145,-0.18305919,-0.18367685,
   -0.18429444,-0.18491196,-0.18552941,-0.18614678,-0.18676408,
   -0.18738131,-0.18799846,-0.18861554,-0.18923254,-0.18984947,
   -0.19046633,-0.19108311,-0.19169981,-0.19231644,-0.19293299,
   -0.19354946,-0.19416586,-0.19478218,-0.19539843,-0.19601460,
   -0.19663069,-0.19724670,-0.19786264,-0.19847850,-0.19909428,
   -0.19970998,-0.20032560,-0.20094114,-0.20155660,-0.20217199,
   -0.20278729,-0.20340251,-0.20401766,-0.20463272,-0.20524770,
   -0.20586260,-0.20647742,-0.20709216,-0.20770682,-0.20832139,
   -0.20893589,-0.20955030,-0.21016462,-0.21077887,-0.21139303,
   -0.21200711,-0.21262110,-0.21323501,-0.21384883,-0.21446258,
   -0.21507623,-0.21568980,-0.21630329,-0.21691669,-0.21753001,
   -0.21814324,-0.21875638,-0.21936944,-0.21998241,-0.22059529,
   -0.22120809,-0.22182080,-0.22243342,-0.22304595,-0.22365840,
   -0.22427076,-0.22488303,-0.22549521,-0.22610730,-0.22671930,
   -0.22733121,-0.22794303,-0.22855477,-0.22916641,-0.22977796,
   -0.23038942,-0.23100079,-0.23161207,-0.23222326,-0.23283435,
   -0.23344536,-0.23405627,-0.23466709,-0.23527782,-0.23588845,
   -0.23649899,-0.23710944,-0.23771979,-0.23833005,-0.23894022,
   -0.23955029,-0.24016027,-0.24077015,-0.24137994,-0.24198963,
   -0.24259923,-0.24320873,-0.24381813,-0.24442744,-0.24503665,
   -0.24564577,-0.24625478,-0.24686371,-0.24747253,-0.24808125,
   -0.24868988,-0.24929841,-0.24990684,-0.25051518,-0.25112341,
   -0.25173154,-0.25233958,-0.25294751,-0.25355535,-0.25416309,
   -0.25477072,-0.25537826,-0.25598569,-0.25659302,-0.25720025,
   -0.25780738,-0.25841441,-0.25902134,-0.25962816,-0.26023488,
   -0.26084150,-0.26144802,-0.26205443,-0.26266074,-0.26326694,
   -0.26387305,-0.26447904,-0.26508493,-0.26569072,-0.26629641,
   -0.26690198,-0.26750746,-0.26811282,-0.26871809,-0.26932324,
   -0.26992829,-0.27053323,-0.27113807,-0.27174280,-0.27234742,
   -0.27295193,-0.27355634,-0.27416063,-0.27476482,-0.27536891,
   -0.27597288,-0.27657674,-0.27718050,-0.27778414,-0.27838768,
   -0.27899110,-0.27959442,-0.28019762,-0.28080072,-0.28140370,
   -0.28200657,-0.28260933,-0.28321198,-0.28381452,-0.28441694,
   -0.28501926,-0.28562146,-0.28622355,-0.28682552,-0.28742738,
   -0.28802913,-0.28863077,-0.28923229,-0.28983369,-0.29043498,
   -0.29103616,-0.29163722,-0.29223817,-0.29283900,-0.29343972,
   -0.29404032,-0.29464080,-0.29524117,-0.29584142,-0.29644156,
   -0.29704158,-0.29764148,-0.29824126,-0.29884093,-0.29944047,
   -0.30003990,-0.30063921,-0.30123840,-0.30183748,-0.30243643,
   -0.30303526,-0.30363398,-0.30423257,-0.30483105,-0.30542940,
   -0.30602764,-0.30662575,-0.30722374,-0.30782161,-0.30841936,
   -0.30901699,-0.30961449,-0.31021188,-0.31080914,-0.31140628,
   -0.31200329,-0.31260018,-0.31319695,-0.31379360,-0.31439012,
   -0.31498651,-0.31558279,-0.31617894,-0.31677496,-0.31737086,
   -0.31796663,-0.31856228,-0.31915780,-0.31975319,-0.32034846,
   -0.32094360,-0.32153862,-0.32213351,-0.32272827,-0.32332291,
   -0.32391741,-0.32451179,-0.32510604,-0.32570017,-0.32629416,
   -0.32688802,-0.32748176,-0.32807537,-0.32866884,-0.32926219,
   -0.32985541,-0.33044850,-0.33104145,-0.33163428,-0.33222698,
   -0.33281954,-0.33341197,-0.33400427,-0.33459644,-0.33518848,
   -0.33578038,-0.33637216,-0.33696380,-0.33755530,-0.33814668,
   -0.33873792,-0.33932902,-0.33991999,-0.34051083,-0.34110153,
   -0.34169210,-0.34228254,-0.34287284,-0.34346300,-0.34405303,
   -0.34464292,-0.34523267,-0.34582229,-0.34641178,-0.34700112,
   -0.34759033,-0.34817940,-0.34876834,-0.34935714,-0.34994579,
   -0.35053432,-0.35112270,-0.35171094,-0.35229905,-0.35288701,
   -0.35347484,-0.35406253,-0.35465007,-0.35523748,-0.35582475,
   -0.35641187,-0.35699886,-0.35758570,-0.35817241,-0.35875897,
   -0.35934539,-0.35993167,-0.36051781,-0.36110380,-0.36168965,
   -0.36227536,-0.36286093,-0.36344635,-0.36403163,-0.36461677,
   -0.36520176,-0.36578660,-0.36637131,-0.36695587,-0.36754028,
   -0.36812455,-0.36870867,-0.36929265,-0.36987648,-0.37046017,
   -0.37104371,-0.37162710,-0.37221034,-0.37279344,-0.37337640,
   -0.37395920,-0.37454186,-0.37512437,-0.37570673,-0.37628894,
   -0.37687101,-0.37745292,-0.37803469,-0.37861630,-0.37919777,
   -0.37977909,-0.38036026,-0.38094128,-0.38152214,-0.38210286,
   -0.38268343,-0.38326384,-0.38384411,-0.38442422,-0.38500418,
   -0.38558399,-0.38616364,-0.38674315,-0.38732250,-0.38790170,
   -0.38848074,-0.38905963,-0.38963837,-0.39021696,-0.39079539,
   -0.39137366,-0.39195178,-0.39252975,-0.39310756,-0.39368522,
   -0.39426272,-0.39484006,-0.39541725,-0.39599429,-0.39657117,
   -0.39714789,-0.39772445,-0.39830086,-0.39887711,-0.39945320,
   -0.40002913,-0.40060491,-0.40118053,-0.40175599,-0.40233129,
   -0.40290643,-0.40348141,-0.40405624,-0.40463090,-0.40520541,
   -0.40577975,-0.40635394,-0.40692796,-0.40750183,-0.40807553,
   -0.40864907,-0.40922245,-0.40979567,-0.41036873,-0.41094162,
   -0.41151435,-0.41208692,-0.41265933,-0.41323158,-0.41380366,
   -0.41437558,-0.41494733,-0.41551892,-0.41609035,-0.41666161,
   -0.41723271,-0.41780364,-0.41837441,-0.41894501,-0.41951545,
   -0.42008572,-0.42065583,-0.42122577,-0.42179554,-0.42236515,
   -0.42293459,-0.42350387,-0.42407297,-0.42464191,-0.42521068,
   -0.42577929,-0.42634772,-0.42691599,-0.42748409,-0.42805202,
   -0.42861978,-0.42918737,-0.42975479,-0.43032205,-0.43088913,
   -0.43145604,-0.43202278,-0.43258935,-0.43315576,-0.43372199,
   -0.43428804,-0.43485393,-0.43541965,-0.43598519,-0.43655056,
   -0.43711576,-0.43768079,-0.43824564,-0.43881032,-0.43937483,
   -0.43993916,-0.44050333,-0.44106731,-0.44163112,-0.44219476,
   -0.44275823,-0.44332151,-0.44388463,-0.44444757,-0.44501033,
   -0.44557292,-0.44613533,-0.44669756,-0.44725962,-0.44782151,
   -0.44838321,-0.44894474,-0.44950609,-0.45006726,-0.45062826,
   -0.45118908,-0.45174972,-0.45231018,-0.45287046,-0.45343057,
   -0.45399049,-0.45455024,-0.45510981,-0.45566920,-0.45622840,
   -0.45678743,-0.45734628,-0.45790494,-0.45846343,-0.45902173,
   -0.45957986,-0.46013780,-0.46069556,-0.46125314,-0.46181053,
   -0.46236775,-0.46292478,-0.46348163,-0.46403829,-0.46459477,
   -0.46515107,-0.46570719,-0.46626312,-0.46681887,-0.46737443,
   -0.46792981,-0.46848500,-0.46904001,-0.46959484,-0.47014947,
   -0.47070393,-0.47125819,-0.47181228,-0.47236617,-0.47291988,
   -0.47347340,-0.47402673,-0.47457988,-0.47513284,-0.47568561,
   -0.47623820,-0.47679060,-0.47734280,-0.47789482,-0.47844666,
   -0.47899830,-0.47954975,-0.48010102,-0.48065209,-0.48120297,
   -0.48175367,-0.48230417,-0.48285449,-0.48340461,-0.48395454,
   -0.48450429,-0.48505384,-0.48560320,-0.48615236,-0.48670134,
   -0.48725012,-0.48779871,-0.48834711,-0.48889532,-0.48944333,
   -0.48999115,-0.49053877,-0.49108620,-0.49163344,-0.49218049,
   -0.49272734,-0.49327399,-0.49382045,-0.49436672,-0.49491279,
   -0.49545866,-0.49600434,-0.49654983,-0.49709511,-0.49764021,
   -0.49818510,-0.49872980,-0.49927430,-0.49981860,-0.50036271,
   -0.50090662,-0.50145033,-0.50199385,-0.50253716,-0.50308028,
   -0.50362320,-0.50416592,-0.50470844,-0.50525076,-0.50579288,
   -0.50633480,-0.50687652,-0.50741805,-0.50795937,-0.50850049,
   -0.50904141,-0.50958213,-0.51012265,-0.51066297,-0.51120308,
   -0.51174300,-0.51228271,-0.51282222,-0.51336152,-0.51390063,
   -0.51443953,-0.51497823,-0.51551672,-0.51605501,-0.51659310,
   -0.51713099,-0.51766866,-0.51820614,-0.51874341,-0.51928048,
   -0.51981734,-0.52035399,-0.52089044,-0.52142669,-0.52196273,
   -0.52249856,-0.52303419,-0.52356961,-0.52410482,-0.52463983,
   -0.52517463,-0.52570922,-0.52624360,-0.52677778,-0.52731175,
   -0.52784551,-0.52837906,-0.52891240,-0.52944554,-0.52997846,
   -0.53051118,-0.53104369,-0.53157598,-0.53210807,-0.53263995,
   -0.53317162,-0.53370307,-0.53423432,-0.53476535,-0.53529618,
   -0.53582679,-0.53635719,-0.53688738,-0.53741736,-0.53794712,
   -0.53847668,-0.53900602,-0.53953514,-0.54006406,-0.54059276,
   -0.54112125,-0.54164952,-0.54217758,-0.54270543,-0.54323306,
   -0.54376048,-0.54428768,-0.54481467,-0.54534144,-0.54586800,
   -0.54639434,-0.54692047,-0.54744638,-0.54797207,-0.54849755,
   -0.54902281,-0.54954786,-0.55007269,-0.55059730,-0.55112169,
   -0.55164587,-0.55216982,-0.55269356,-0.55321709,-0.55374039,
   -0.55426347,-0.55478634,-0.55530899,-0.55583141,-0.55635362,
   -0.55687561,-0.55739738,-0.55791893,-0.55844026,-0.55896137,
   -0.55948225,-0.56000292,-0.56052337,-0.56104359,-0.56156359,
   -0.56208337,-0.56260293,-0.56312227,-0.56364138,-0.56416028,
   -0.56467895,-0.56519739,-0.56571561,-0.56623361,-0.56675139,
   -0.56726894,-0.56778627,-0.56830338,-0.56882026,-0.56933691,
   -0.56985334,-0.57036955,-0.57088553,-0.57140129,-0.57191682,
   -0.57243212,-0.57294720,-0.57346205,-0.57397668,-0.57449108,
   -0.57500525,-0.57551919,-0.57603291,-0.57654640,-0.57705966,
   -0.57757270,-0.57808551,-0.57859808,-0.57911043,-0.57962256,
   -0.58013445,-0.58064611,-0.58115755,-0.58166875,-0.58217973,
   -0.58269047,-0.58320099,-0.58371128,-0.58422133,-0.58473116,
   -0.58524075,-0.58575011,-0.58625924,-0.58676814,-0.58727681,
   -0.58778525,-0.58829345,-0.58880142,-0.58930916,-0.58981667,
   -0.59032394,-0.59083099,-0.59133779,-0.59184437,-0.59235071,
   -0.59285682,-0.59336269,-0.59386833,-0.59437373,-0.59487890,
   -0.59538383,-0.59588853,-0.59639300,-0.59689723,-0.59740122,
   -0.59790498,-0.59840850,-0.59891178,-0.59941483,-0.59991765,
   -0.60042022,-0.60092256,-0.60142466,-0.60192652,-0.60242815,
   -0.60292954,-0.60343069,-0.60393160,-0.60443227,-0.60493271,
   -0.60543290,-0.60593286,-0.60643258,-0.60693206,-0.60743129,
   -0.60793029,-0.60842905,-0.60892757,-0.60942585,-0.60992389,
   -0.61042168,-0.61091924,-0.61141655,-0.61191363,-0.61241046,
   -0.61290705,-0.61340340,-0.61389950,-0.61439537,-0.61489099,
   -0.61538637,-0.61588150,-0.61637639,-0.61687104,-0.61736545,
   -0.61785961,-0.61835353,-0.61884720,-0.61934063,-0.61983381,
   -0.62032675,-0.62081945,-0.62131190,-0.62180410,-0.62229606,
   -0.62278778,-0.62327924,-0.62377046,-0.62426144,-0.62475217,
   -0.62524265,-0.62573289,-0.62622288,-0.62671262,-0.62720211,
   -0.62769136,-0.62818035,-0.62866911,-0.62915761,-0.62964586,
   -0.63013387,-0.63062162,-0.63110913,-0.63159639,-0.63208340,
   -0.63257016,-0.63305667,-0.63354293,-0.63402894,-0.63451470,
   -0.63500020,-0.63548546,-0.63597047,-0.63645523,-0.63693973,
   -0.63742398,-0.63790799,-0.63839174,-0.63887523,-0.63935848,
   -0.63984147,-0.64032421,-0.64080670,-0.64128894,-0.64177092,
   -0.64225265,-0.64273412,-0.64321534,-0.64369631,-0.64417702,
   -0.64465748,-0.64513769,-0.64561764,-0.64609733,-0.64657677,
   -0.64705596,-0.64753489,-0.64801356,-0.64849198,-0.64897014,
   -0.64944804,-0.64992569,-0.65040308,-0.65088022,-0.65135710,
   -0.65183372,-0.65231008,-0.65278619,-0.65326204,-0.65373763,
   -0.65421296,-0.65468804,-0.65516285,-0.65563741,-0.65611171,
   -0.65658575,-0.65705953,-0.65753305,-0.65800631,-0.65847932,
   -0.65895206,-0.65942454,-0.65989676,-0.66036872,-0.66084042,
   -0.66131186,-0.66178304,-0.66225396,-0.66272461,-0.66319500,
   -0.66366514,-0.66413501,-0.66460461,-0.66507396,-0.66554304,
   -0.66601186,-0.66648042,-0.66694871,-0.66741674,-0.66788451,
   -0.66835202,-0.66881925,-0.66928623,-0.66975294,-0.67021939,
   -0.67068557,-0.67115149,-0.67161714,-0.67208253,-0.67254765,
   -0.67301251,-0.67347710,-0.67394142,-0.67440548,-0.67486928,
   -0.67533280,-0.67579606,-0.67625906,-0.67672178,-0.67718424,
   -0.67764643,-0.67810836,-0.67857001,-0.67903140,-0.67949252,
   -0.67995337,-0.68041396,-0.68087427,-0.68133432,-0.68179410,
   -0.68225360,-0.68271284,-0.68317181,-0.68363051,-0.68408894,
   -0.68454710,-0.68500499,-0.68546261,-0.68591996,-0.68637704,
   -0.68683384,-0.68729038,-0.68774664,-0.68820263,-0.68865835,
   -0.68911380,-0.68956898,-0.69002389,-0.69047852,-0.69093288,
   -0.69138696,-0.69184078,-0.69229432,-0.69274759,-0.69320058,
   -0.69365330,-0.69410575,-0.69455792,-0.69500982,-0.69546144,
   -0.69591279,-0.69636387,-0.69681467,-0.69726519,-0.69771544,
   -0.69816541,-0.69861511,-0.69906453,-0.69951368,-0.69996255,
   -0.70041115,-0.70085946,-0.70130750,-0.70175527,-0.70220276,
   -0.70264996,-0.70309690,-0.70354355,-0.70398993,-0.70443603,
   -0.70488185,-0.70532739,-0.70577266,-0.70621764,-0.70666235,
   -0.70710678,-0.70755092,-0.70799479,-0.70843838,-0.70888169,
   -0.70932472,-0.70976747,-0.71020995,-0.71065213,-0.71109404,
   -0.71153567,-0.71197702,-0.71241809,-0.71285887,-0.71329938,
   -0.71373960,-0.71417954,-0.71461920,-0.71505857,-0.71549767,
   -0.71593648,-0.71637501,-0.71681325,-0.71725122,-0.71768890,
   -0.71812629,-0.71856341,-0.71900024,-0.71943678,-0.71987304,
   -0.72030902,-0.72074471,-0.72118012,-0.72161525,-0.72205008,
   -0.72248464,-0.72291891,-0.72335289,-0.72378659,-0.72422000,
   -0.72465313,-0.72508597,-0.72551852,-0.72595079,-0.72638277,
   -0.72681446,-0.72724587,-0.72767699,-0.72810782,-0.72853836,
   -0.72896862,-0.72939859,-0.72982827,-0.73025767,-0.73068677,
   -0.73111559,-0.73154412,-0.73197236,-0.73240031,-0.73282797,
   -0.73325534,-0.73368242,-0.73410922,-0.73453572,-0.73496193,
   -0.73538786,-0.73581349,-0.73623883,-0.73666388,-0.73708864,
   -0.73751311,-0.73793729,-0.73836118,-0.73878477,-0.73920808,
   -0.73963109,-0.74005381,-0.74047624,-0.74089837,-0.74132022,
   -0.74174177,-0.74216303,-0.74258399,-0.74300466,-0.74342504,
   -0.74384512,-0.74426492,-0.74468441,-0.74510362,-0.74552253,
   -0.74594114,-0.74635946,-0.74677749,-0.74719522,-0.74761265,
   -0.74802979,-0.74844664,-0.74886319,-0.74927944,-0.74969540,
   -0.75011106,-0.75052643,-0.75094150,-0.75135627,-0.75177075,
   -0.75218493,-0.75259882,-0.75301240,-0.75342569,-0.75383868,
   -0.75425138,-0.75466377,-0.75507587,-0.75548767,-0.75589917,
   -0.75631038,-0.75672128,-0.75713189,-0.75754219,-0.75795220,
   -0.75836191,-0.75877132,-0.75918043,-0.75958924,-0.75999775,
   -0.76040596,-0.76081387,-0.76122148,-0.76162879,-0.76203580,
   -0.76244251,-0.76284891,-0.76325502,-0.76366082,-0.76406633,
   -0.76447153,-0.76487643,-0.76528102,-0.76568532,-0.76608931,
   -0.76649300,-0.76689639,-0.76729947,-0.76770226,-0.76810474,
   -0.76850691,-0.76890878,-0.76931035,-0.76971162,-0.77011258,
   -0.77051324,-0.77091359,-0.77131364,-0.77171338,-0.77211282,
   -0.77251196,-0.77291079,-0.77330931,-0.77370753,-0.77410545,
   -0.77450306,-0.77490036,-0.77529735,-0.77569405,-0.77609043,
   -0.77648651,-0.77688228,-0.77727774,-0.77767290,-0.77806775,
   -0.77846230,-0.77885653,-0.77925046,-0.77964408,-0.78003740,
   -0.78043040,-0.78082310,-0.78121549,-0.78160757,-0.78199934,
   -0.78239081,-0.78278196,-0.78317281,-0.78356334,-0.78395357,
   -0.78434349,-0.78473309,-0.78512239,-0.78551138,-0.78590006,
   -0.78628843,-0.78667648,-0.78706423,-0.78745167,-0.78783879,
   -0.78822561,-0.78861211,-0.78899830,-0.78938418,-0.78976975,
   -0.79015501,-0.79053995,-0.79092459,-0.79130891,-0.79169291,
   -0.79207661,-0.79245999,-0.79284306,-0.79322582,-0.79360826,
   -0.79399039,-0.79437221,-0.79475371,-0.79513490,-0.79551578,
   -0.79589634,-0.79627659,-0.79665652,-0.79703614,-0.79741545,
   -0.79779443,-0.79817311,-0.79855147,-0.79892951,-0.79930724,
   -0.79968465,-0.80006175,-0.80043853,-0.80081500,-0.80119115,
   -0.80156698,-0.80194250,-0.80231770,-0.80269258,-0.80306715,
   -0.80344140,-0.80381533,-0.80418894,-0.80456224,-0.80493522,
   -0.80530788,-0.80568022,-0.80605225,-0.80642396,-0.80679535,
   -0.80716642,-0.80753717,-0.80790760,-0.80827772,-0.80864751,
   -0.80901699,-0.80938615,-0.80975498,-0.81012350,-0.81049170,
   -0.81085958,-0.81122713,-0.81159437,-0.81196129,-0.81232788,
   -0.81269416,-0.81306011,-0.81342575,-0.81379106,-0.81415605,
   -0.81452072,-0.81488507,-0.81524910,-0.81561280,-0.81597619,
   -0.81633925,-0.81670198,-0.81706440,-0.81742649,-0.81778826,
   -0.81814971,-0.81851084,-0.81887164,-0.81923212,-0.81959227,
   -0.81995210,-0.82031161,-0.82067080,-0.82102966,-0.82138819,
   -0.82174640,-0.82210429,-0.82246185,-0.82281909,-0.82317600,
   -0.82353259,-0.82388886,-0.82424479,-0.82460041,-0.82495569,
   -0.82531065,-0.82566529,-0.82601960,-0.82637358,-0.82672724,
   -0.82708057,-0.82743357,-0.82778625,-0.82813860,-0.82849063,
   -0.82884232,-0.82919369,-0.82954473,-0.82989545,-0.83024584,
   -0.83059589,-0.83094563,-0.83129503,-0.83164410,-0.83199285,
   -0.83234127,-0.83268936,-0.83303712,-0.83338455,-0.83373165,
   -0.83407843,-0.83442487,-0.83477099,-0.83511677,-0.83546223,
   -0.83580736,-0.83615215,-0.83649662,-0.83684075,-0.83718456,
   -0.83752804,-0.83787118,-0.83821399,-0.83855648,-0.83889863,
   -0.83924045,-0.83958194,-0.83992309,-0.84026392,-0.84060441,
   -0.84094458,-0.84128441,-0.84162391,-0.84196307,-0.84230191,
   -0.84264041,-0.84297858,-0.84331641,-0.84365391,-0.84399108,
   -0.84432792,-0.84466442,-0.84500059,-0.84533643,-0.84567193,
   -0.84600710,-0.84634194,-0.84667644,-0.84701060,-0.84734443,
   -0.84767793,-0.84801109,-0.84834392,-0.84867641,-0.84900857,
   -0.84934040,-0.84967188,-0.85000303,-0.85033385,-0.85066433,
   -0.85099448,-0.85132429,-0.85165376,-0.85198290,-0.85231170,
   -0.85264016,-0.85296829,-0.85329608,-0.85362353,-0.85395065,
   -0.85427743,-0.85460387,-0.85492997,-0.85525574,-0.85558117,
   -0.85590626,-0.85623102,-0.85655543,-0.85687951,-0.85720325,
   -0.85752665,-0.85784971,-0.85817244,-0.85849482,-0.85881687,
   -0.85913858,-0.85945994,-0.85978097,-0.86010166,-0.86042201,
   -0.86074202,-0.86106169,-0.86138102,-0.86170001,-0.86201866,
   -0.86233697,-0.86265494,-0.86297257,-0.86328986,-0.86360681,
   -0.86392341,-0.86423968,-0.86455560,-0.86487118,-0.86518643,
   -0.86550133,-0.86581588,-0.86613010,-0.86644397,-0.86675751,
   -0.86707070,-0.86738354,-0.86769605,-0.86800821,-0.86832003,
   -0.86863151,-0.86894264,-0.86925344,-0.86956388,-0.86987399,
   -0.87018375,-0.87049317,-0.87080224,-0.87111097,-0.87141936,
   -0.87172740,-0.87203510,-0.87234245,-0.87264946,-0.87295613,
   -0.87326245,-0.87356843,-0.87387406,-0.87417934,-0.87448428,
   -0.87478888,-0.87509313,-0.87539703,-0.87570059,-0.87600381,
   -0.87630668,-0.87660920,-0.87691137,-0.87721320,-0.87751469,
   -0.87781582,-0.87811661,-0.87841706,-0.87871715,-0.87901690,
   -0.87931631,-0.87961536,-0.87991407,-0.88021243,-0.88051044,
   -0.88080811,-0.88110543,-0.88140240,-0.88169902,-0.88199530,
   -0.88229122,-0.88258680,-0.88288203,-0.88317691,-0.88347144,
   -0.88376563,-0.88405946,-0.88435295,-0.88464608,-0.88493887,
   -0.88523131,-0.88552339,-0.88581513,-0.88610652,-0.88639756,
   -0.88668825,-0.88697859,-0.88726858,-0.88755822,-0.88784751,
   -0.88813644,-0.88842503,-0.88871327,-0.88900115,-0.88928869,
   -0.88957587,-0.88986270,-0.89014918,-0.89043531,-0.89072109,
   -0.89100652,-0.89129159,-0.89157632,-0.89186069,-0.89214471,
   -0.89242837,-0.89271169,-0.89299465,-0.89327726,-0.89355952,
   -0.89384142,-0.89412297,-0.89440417,-0.89468501,-0.89496550,
   -0.89524564,-0.89552543,-0.89580486,-0.89608394,-0.89636266,
   -0.89664103,-0.89691905,-0.89719671,-0.89747402,-0.89775097,
   -0.89802757,-0.89830382,-0.89857971,-0.89885524,-0.89913042,
   -0.89940525,-0.89967972,-0.89995383,-0.90022759,-0.90050100,
   -0.90077405,-0.90104674,-0.90131908,-0.90159106,-0.90186268,
   -0.90213395,-0.90240487,-0.90267543,-0.90294563,-0.90321547,
   -0.90348496,-0.90375409,-0.90402287,-0.90429128,-0.90455934,
   -0.90482705,-0.90509439,-0.90536138,-0.90562801,-0.90589429,
   -0.90616021,-0.90642576,-0.90669096,-0.90695581,-0.90722029,
   -0.90748442,-0.90774819,-0.90801160,-0.90827465,-0.90853734,
   -0.90879968,-0.90906165,-0.90932327,-0.90958453,-0.90984543,
   -0.91010597,-0.91036615,-0.91062597,-0.91088543,-0.91114453,
   -0.91140327,-0.91166165,-0.91191968,-0.91217734,-0.91243464,
   -0.91269158,-0.91294816,-0.91320439,-0.91346025,-0.91371575,
   -0.91397089,-0.91422566,-0.91448008,-0.91473414,-0.91498783,
   -0.91524117,-0.91549414,-0.91574675,-0.91599900,-0.91625089,
   -0.91650242,-0.91675358,-0.91700438,-0.91725483,-0.91750490,
   -0.91775462,-0.91800397,-0.91825297,-0.91850160,-0.91874986,
   -0.91899777,-0.91924531,-0.91949249,-0.91973930,-0.91998575,
   -0.92023184,-0.92047757,-0.92072293,-0.92096793,-0.92121256,
   -0.92145684,-0.92170074,-0.92194429,-0.92218747,-0.92243028,
   -0.92267273,-0.92291482,-0.92315655,-0.92339790,-0.92363890,
   -0.92387953,-0.92411979,-0.92435969,-0.92459923,-0.92483840,
   -0.92507720,-0.92531564,-0.92555372,-0.92579142,-0.92602877,
   -0.92626575,-0.92650236,-0.92673860,-0.92697449,-0.92721000,
   -0.92744515,-0.92767993,-0.92791435,-0.92814840,-0.92838208,
   -0.92861540,-0.92884835,-0.92908093,-0.92931315,-0.92954500,
   -0.92977648,-0.93000760,-0.93023835,-0.93046873,-0.93069874,
   -0.93092839,-0.93115767,-0.93138658,-0.93161512,-0.93184330,
   -0.93207111,-0.93229855,-0.93252562,-0.93275232,-0.93297866,
   -0.93320463,-0.93343023,-0.93365546,-0.93388032,-0.93410481,
   -0.93432894,-0.93455269,-0.93477608,-0.93499910,-0.93522175,
   -0.93544403,-0.93566594,-0.93588748,-0.93610865,-0.93632945,
   -0.93654988,-0.93676994,-0.93698964,-0.93720896,-0.93742791,
   -0.93764649,-0.93786471,-0.93808255,-0.93830002,-0.93851712,
   -0.93873385,-0.93895021,-0.93916620,-0.93938182,-0.93959707,
   -0.93981195,-0.94002645,-0.94024059,-0.94045435,-0.94066774,
   -0.94088076,-0.94109341,-0.94130569,-0.94151760,-0.94172913,
   -0.94194030,-0.94215109,-0.94236151,-0.94257155,-0.94278123,
   -0.94299053,-0.94319946,-0.94340802,-0.94361621,-0.94382402,
   -0.94403146,-0.94423853,-0.94444522,-0.94465154,-0.94485749,
   -0.94506307,-0.94526827,-0.94547310,-0.94567756,-0.94588164,
   -0.94608535,-0.94628869,-0.94649165,-0.94669424,-0.94689646,
   -0.94709830,-0.94729977,-0.94750086,-0.94770158,-0.94790193,
   -0.94810190,-0.94830150,-0.94850072,-0.94869957,-0.94889804,
   -0.94909614,-0.94929386,-0.94949121,-0.94968819,-0.94988479,
   -0.95008101,-0.95027686,-0.95047234,-0.95066744,-0.95086216,
   -0.95105651,-0.95125048,-0.95144408,-0.95163730,-0.95183015,
   -0.95202262,-0.95221472,-0.95240644,-0.95259778,-0.95278875,
   -0.95297934,-0.95316955,-0.95335939,-0.95354885,-0.95373794,
   -0.95392665,-0.95411498,-0.95430293,-0.95449051,-0.95467771,
   -0.95486454,-0.95505099,-0.95523706,-0.95542275,-0.95560807,
   -0.95579301,-0.95597757,-0.95616176,-0.95634556,-0.95652899,
   -0.95671205,-0.95689472,-0.95707702,-0.95725894,-0.95744048,
   -0.95762164,-0.95780243,-0.95798283,-0.95816286,-0.95834251,
   -0.95852178,-0.95870068,-0.95887919,-0.95905733,-0.95923509,
   -0.95941247,-0.95958947,-0.95976609,-0.95994233,-0.96011820,
   -0.96029368,-0.96046879,-0.96064351,-0.96081786,-0.96099183,
   -0.96116542,-0.96133863,-0.96151146,-0.96168391,-0.96185598,
   -0.96202767,-0.96219898,-0.96236991,-0.96254046,-0.96271063,
   -0.96288042,-0.96304983,-0.96321886,-0.96338751,-0.96355578,
   -0.96372367,-0.96389118,-0.96405831,-0.96422506,-0.96439143,
   -0.96455741,-0.96472302,-0.96488824,-0.96505309,-0.96521755,
   -0.96538163,-0.96554533,-0.96570865,-0.96587159,-0.96603415,
   -0.96619633,-0.96635812,-0.96651953,-0.96668057,-0.96684121,
   -0.96700148,-0.96716137,-0.96732087,-0.96748000,-0.96763874,
   -0.96779710,-0.96795507,-0.96811267,-0.96826988,-0.96842671,
   -0.96858316,-0.96873922,-0.96889490,-0.96905020,-0.96920512,
   -0.96935966,-0.96951381,-0.96966758,-0.96982097,-0.96997397,
   -0.97012659,-0.97027883,-0.97043068,-0.97058216,-0.97073325,
   -0.97088395,-0.97103427,-0.97118421,-0.97133377,-0.97148294,
   -0.97163173,-0.97178013,-0.97192815,-0.97207579,-0.97222305,
   -0.97236992,-0.97251640,-0.97266250,-0.97280822,-0.97295356,
   -0.97309851,-0.97324307,-0.97338725,-0.97353105,-0.97367446,
   -0.97381749,-0.97396014,-0.97410240,-0.97424427,-0.97438576,
   -0.97452687,-0.97466759,-0.97480793,-0.97494788,-0.97508744,
   -0.97522662,-0.97536542,-0.97550383,-0.97564186,-0.97577950,
   -0.97591676,-0.97605363,-0.97619011,-0.97632621,-0.97646193,
   -0.97659726,-0.97673220,-0.97686676,-0.97700093,-0.97713472,
   -0.97726812,-0.97740113,-0.97753376,-0.97766601,-0.97779786,
   -0.97792933,-0.97806042,-0.97819112,-0.97832143,-0.97845136,
   -0.97858090,-0.97871005,-0.97883882,-0.97896720,-0.97909520,
   -0.97922281,-0.97935003,-0.97947686,-0.97960331,-0.97972937,
   -0.97985505,-0.97998034,-0.98010524,-0.98022975,-0.98035388,
   -0.98047762,-0.98060097,-0.98072394,-0.98084652,-0.98096871,
   -0.98109051,-0.98121193,-0.98133296,-0.98145360,-0.98157386,
   -0.98169372,-0.98181320,-0.98193230,-0.98205100,-0.98216932,
   -0.98228725,-0.98240479,-0.98252194,-0.98263871,-0.98275508,
   -0.98287107,-0.98298667,-0.98310189,-0.98321671,-0.98333115,
   -0.98344520,-0.98355886,-0.98367213,-0.98378502,-0.98389751,
   -0.98400962,-0.98412134,-0.98423267,-0.98434361,-0.98445416,
   -0.98456433,-0.98467411,-0.98478349,-0.98489249,-0.98500110,
   -0.98510932,-0.98521715,-0.98532460,-0.98543165,-0.98553831,
   -0.98564459,-0.98575048,-0.98585597,-0.98596108,-0.98606580,
   -0.98617013,-0.98627407,-0.98637762,-0.98648078,-0.98658356,
   -0.98668594,-0.98678793,-0.98688954,-0.98699075,-0.98709157,
   -0.98719201,-0.98729205,-0.98739171,-0.98749097,-0.98758985,
   -0.98768834,-0.98778643,-0.98788414,-0.98798145,-0.98807838,
   -0.98817491,-0.98827106,-0.98836681,-0.98846218,-0.98855716,
   -0.98865174,-0.98874593,-0.98883974,-0.98893315,-0.98902618,
   -0.98911881,-0.98921105,-0.98930290,-0.98939436,-0.98948543,
   -0.98957611,-0.98966640,-0.98975630,-0.98984581,-0.98993493,
   -0.99002365,-0.99011199,-0.99019993,-0.99028749,-0.99037465,
   -0.99046142,-0.99054780,-0.99063379,-0.99071939,-0.99080460,
   -0.99088941,-0.99097384,-0.99105787,-0.99114151,-0.99122477,
   -0.99130763,-0.99139009,-0.99147217,-0.99155386,-0.99163515,
   -0.99171606,-0.99179657,-0.99187669,-0.99195641,-0.99203575,
   -0.99211470,-0.99219325,-0.99227141,-0.99234918,-0.99242656,
   -0.99250355,-0.99258014,-0.99265634,-0.99273215,-0.99280757,
   -0.99288260,-0.99295723,-0.99303148,-0.99310533,-0.99317879,
   -0.99325185,-0.99332453,-0.99339681,-0.99346870,-0.99354020,
   -0.99361131,-0.99368202,-0.99375234,-0.99382227,-0.99389181,
   -0.99396095,-0.99402970,-0.99409806,-0.99416603,-0.99423360,
   -0.99430079,-0.99436757,-0.99443397,-0.99449998,-0.99456559,
   -0.99463081,-0.99469563,-0.99476007,-0.99482411,-0.99488776,
   -0.99495101,-0.99501387,-0.99507634,-0.99513842,-0.99520011,
   -0.99526140,-0.99532230,-0.99538280,-0.99544291,-0.99550263,
   -0.99556196,-0.99562089,-0.99567943,-0.99573758,-0.99579534,
   -0.99585270,-0.99590966,-0.99596624,-0.99602242,-0.99607821,
   -0.99613360,-0.99618861,-0.99624321,-0.99629743,-0.99635125,
   -0.99640468,-0.99645772,-0.99651036,-0.99656261,-0.99661446,
   -0.99666592,-0.99671699,-0.99676767,-0.99681795,-0.99686783,
   -0.99691733,-0.99696643,-0.99701514,-0.99706345,-0.99711137,
   -0.99715890,-0.99720603,-0.99725277,-0.99729911,-0.99734506,
   -0.99739062,-0.99743578,-0.99748055,-0.99752493,-0.99756891,
   -0.99761250,-0.99765570,-0.99769850,-0.99774090,-0.99778292,
   -0.99782454,-0.99786576,-0.99790659,-0.99794703,-0.99798707,
   -0.99802672,-0.99806598,-0.99810484,-0.99814331,-0.99818138,
   -0.99821906,-0.99825635,-0.99829324,-0.99832973,-0.99836584,
   -0.99840155,-0.99843686,-0.99847178,-0.99850631,-0.99854044,
   -0.99857418,-0.99860752,-0.99864047,-0.99867302,-0.99870519,
   -0.99873695,-0.99876832,-0.99879930,-0.99882989,-0.99886007,
   -0.99888987,-0.99891927,-0.99894828,-0.99897689,-0.99900511,
   -0.99903293,-0.99906036,-0.99908739,-0.99911403,-0.99914028,
   -0.99916613,-0.99919159,-0.99921665,-0.99924132,-0.99926559,
   -0.99928947,-0.99931295,-0.99933604,-0.99935874,-0.99938104,
   -0.99940294,-0.99942445,-0.99944557,-0.99946629,-0.99948662,
   -0.99950656,-0.99952609,-0.99954524,-0.99956399,-0.99958234,
   -0.99960030,-0.99961787,-0.99963504,-0.99965182,-0.99966820,
   -0.99968418,-0.99969978,-0.99971497,-0.99972978,-0.99974419,
   -0.99975820,-0.99977182,-0.99978504,-0.99979787,-0.99981031,
   -0.99982235,-0.99983399,-0.99984524,-0.99985610,-0.99986656,
   -0.99987663,-0.99988630,-0.99989558,-0.99990446,-0.99991295,
   -0.99992104,-0.99992874,-0.99993604,-0.99994295,-0.99994946,
   -0.99995558,-0.99996131,-0.99996664,-0.99997157,-0.99997611,
   -0.99998026,-0.99998401,-0.99998736,-0.99999032,-0.99999289,
   -0.99999506,-0.99999684,-0.99999822,-0.99999921,-0.99999980,
   -1.00000000 // extra element
};

static const float * const _COS_VALUES_ = &(_SIN_VALUES_[2500]);


#endif // USE_FAST_SINCOS

