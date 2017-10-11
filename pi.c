/*
**  PI.C - Computes Pi to an arbitrary number of digits
**
**  Originally updated for WIN32 by R.E.F. - 7/95
**  Windows version includes code to reduce priority slightly so that you can STILL
**  get work done while PI is being calculated (in the background).
**
**  (this was primarily needed due to Win '9x having a somewhat poor quality scheduler)
**  (Linux and BSD don't need this, their schedulers are just fine, k?)
**
**  Original WINDOWS version compiled as 32-bit code to allow gigantic arrays and NO segment loads.
**  (it was a REALLY BIG DEAL back in 1995, for those of us who remember)
**  The NEWEST version is targeted at AMD64 with multiple cores on Linux or BSD.  That's right, it
**  adds THREADING to the algorithm.  When I did a comparison, the 'breakover' point was around 2000,
**  where the threaded version stops taking longer and starts improving the time.
**  You can define 'SINGLE_THREAD' to build a 'single thread' version, or you can modify the value of
**  'USE_THREAD' to change the point at which threads are used.
**
**  MAJOR BUG FIX:  the size of 'stor[]' array at 21 had stack overflows, and caused previous
**  'pi_million.txt' file to contain errors around 89,000 digits (original windows version), and
**  CRASHED when I attempted to run it on FreeBSD (revealing the bug).  Preliminary testing revealed
**  I need at least '[32]', but the actual size requirement for this array is currently "not determined".
**
**  A few notes about this program
**  I found it without any copyright on the internet back in 1995, and did some mods to it so
**  that it would a) compile in windows and b) run faster (including the inline assembly).
**  Recently I decided to 'modernize' it a bit and so I added the multi-thread code.  Keep in
**  mind that this code has MANY CHARACTERISTICS that are either 'holdovers' from the original
**  (like using 'Fcalloc'), global variables that SHOULD NOT BE GLOBALS, and a few other 'bad style'
**  aspects.  The label, however, is mine (in the 'run anyway' code in case the threader fails to work).
**  So if you spot any mistakes (like the 'stor[]' array size bug I ran into) please let me know since
**  I want the actual value this thing spits out to be 100% accurate.
**
*/

// Linux/BSD/Cygwin BUILD: gcc -O2 -o pi pi.c -Wall -lpthread
// Linux/BSD/Cygwin SINGLE THREAD BUILD: gcc -O2 -o pi pi.c -Wall -DSINGLE_THREAD
//
// (this will probably work on OSX as well)


// NOTE:  uncomment these for native windows build
// #define WIN32
// #include "windows.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <malloc.h>
#else  // _WIN32
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#endif  // _WIN32
#include <string.h>

// WIN32 fixes[this used to be a WIN16 app with some non-standard symbols]
#define FAR
#define Fcalloc calloc
#define Ffree free
#define Size_T size_t

// what was once 'long' becomes int32_t for Linux, BSD
// this deals with 64-bit compiler differences with windows
#ifdef _WIN32

#ifndef int32_t
#define int32_t long
#endif // int32_t

#else // gcc compile

// 32-bit vs 64-bit pointers
#if defined(amd64__) || defined(__amd64__) || defined(x86_64__) || defined(__x86_64__) // AMD64
#define ARCH_AMD64
#define INTPTR long long
#elif defined(__LP64__) // 64-bit pointers
#define INTPTR long long
#else // 32-bit pointers
#if defined(__i386) || defined(__i386__) || defined(__i586) || defined(__i586__)
#define ARCH_X86
#endif
#define INTPTR long /* 32-bit long integer */
#endif

#endif  // _WIN32



#define STOR_SIZE 999   /* was 21, very large value may be needed. Highest measured 'max' needed is 31 */

// NOTE:'stor' was too small for previous WIN32 million-digit calc, causing errors
// NEW output fails to match old 'million digit' run (using 7/95 WIN32 code) after about 89,000 digits

#ifndef SINGLE_THREAD
#define USE_THREAD 2000 /* based on performance comparison measurements */
#endif // SINGLE_THREAD

// uncomment this to determine the requirements (and boundary check) the 'stor[]' array
// #define CHECK_LOC


// GLOBAL VARIABLES - original author seemed to want to use these
// it's probably much faster to use a structure.  nearly every MODERN CPU
// nowadays can directly access members via an offset to a structure pointer
// within one or 2 clock cycles
// TODO:  move these into a structure, pass structure pointer to functions

int32_t FAR * mf, FAR * ms;
int32_t kf, ks;
int32_t cnt, n, temp, nd;
int32_t i;
int32_t col, col1;
int32_t loc, stor[STOR_SIZE];
#ifdef CHECK_LOC
int32_t max_loc; // use for bounds-check
#endif // CHECK_LOC


#ifndef _WIN32 /* assume NOT a 16-bit compile */
unsigned int MyGetTickCount()
{
struct timeval tv;

  gettimeofday(&tv, NULL); // 2nd parameter is obsolete anyway so pass NULL

  // NOTE:this won 't roll over the way ' GetTickCount ' does in WIN32 so I' ll truncate it
  // down to a 32 - bit value to make it happen.Everything that uses 'MyGetTickCount'
  // must handle this rollover properly using 'int' and not 'long' (or cast afterwards)
  return ((unsigned int)((unsigned long)tv.tv_sec * 1000L + (unsigned long)tv.tv_usec / 1000L));
}
#endif  // _WIN32

__inline void shift(int32_t FAR * l1, int32_t FAR * l2, int32_t lp, int32_t lmod)
{
register int32_t k, k0, k1;


  k0 = *l2;

  if(*l2 > 0)
  {
#ifdef _WIN32 /* 32-bit windows */
    k = k0;

    _asm mov eax, k
    _asm mov edx, 0
    _asm div lmod
    _asm mov k1, edx
    _asm mov k, eax

    *l2 = k1;  //remainder
#else // #elif defined(ARCH_AMD64) || defined(ARCH_X86)
    k = k0;
    k1 = lmod; // hand optimization

    {
      register u_int32_t q = k / k1, r = k % k1;

      k = q;
      *l2 = r;
    }
//#else
//    k = k0 / lmod;
//    *l2 -= k * lmod; // the remainder (the old way)
#endif // _WIN32

  }
  else
  {
#ifdef _WIN32
    k = k0;

    _asm mov eax, k
    _asm neg eax
    _asm mov edx, 0
    _asm div lmod
    _asm mov k1, edx
    _asm inc eax
    _asm neg eax
    _asm mov k, eax

    *l2 = lmod - k1;
#else // #elif defined(ARCH_AMD64) || defined(ARCH_X86)
    // NOTE:  for best optimization, use -O2 and gcc SHOULD make it efficient
    k = -(k0);
    k1 = lmod;

    {
      register u_int32_t q = k / k1, r = k % k1;

      k = -(q) - 1;
      *l2 = k1 - r;
    }
//#else
//    k = -(-(k0) / lmod) - 1; // ensure truncation as positive number
//    *l2 -= k * lmod; // the remainder (the old way)
#endif // _WIN32

  }

  //original code
  // k = ((*l2) > 0 ? (*l2) / lmod : -(-(*l2) / lmod) - 1);
  // *l2 -= k * lmod;


  *l1 += k * lp;
}

__inline void shift1(int32_t FAR * l1, int32_t FAR * l2, int32_t lmod)
{
register int32_t k, k0, k1;


  k0 = *l2;

  if(*l2 > 0)
  {
#ifdef _WIN32
    k = k0;

    _asm mov eax, k
    _asm mov edx, 0
    _asm div lmod
    _asm mov k1, edx
    _asm mov k, eax

    *l2 = k1;  //remainder
#else // #elif defined(ARCH_AMD64) || defined(ARCH_X86)
    k = k0;
    k1 = lmod; // hand optimization

    {
      register u_int32_t q = k / k1, r = k % k1;

      k = q;
      *l2 = r;
    }
//#else  // _WIN32
//    k = *l2 / lmod;
//  *l2 -= k * lmod; // the remainder
#endif // _WIN32
  }
  else
  {
#ifdef _WIN32
    k = k0;

    _asm mov eax, k
    _asm neg eax
    _asm mov edx, 0
    _asm div lmod
    _asm mov k1, edx
    _asm inc eax
    _asm neg eax
    _asm mov k, eax

    *l2 = lmod - k1;
#else // #elif defined(ARCH_AMD64) || defined(ARCH_X86)
    // NOTE:  for best optimization, use -O2 and gcc SHOULD make it efficient
    k = -(k0);
    k1 = lmod;

    {
      register u_int32_t q = k / k1, r = k % k1;

      k = -(q) - 1;
      *l2 = k1 - r;
    }
//#else  // _WIN32
//    k = -(-(*l2) / lmod) - 1; // ensure truncation as positive number
//  *l2 -= k * lmod; // the remainder
#endif // _WIN32
  }

  //original code
  // k = ((*l2) > 0 ? (*l2) / lmod : -(-(*l2) / lmod) - 1);
  // *l2 -= k * lmod; // the remainder


  *l1 += k;
}

void yprint(int32_t m)
{
  if(cnt < n)
  {
    if(++col == 11)
    {
      col = 1;
      if(++col1 == 6)
      {
        col1 = 0;
        printf("\n");
        printf("%4ld", (long)(m % 10));
      }
      else
      {
        printf("%3ld", (long)(m % 10));
      }
    }
    else
    {
      printf("%ld", (long)m);
    }
    cnt++;
  }
}

void xprint(int32_t m)
{
int32_t ii, wk, wk1;

  if(m < 8)
  {
#ifdef CHECK_LOC
    // boundary check for 'loc' within 'stor[]' array
    if(loc < 0 || loc >= sizeof(stor) / sizeof(stor[0]))
    {
      fprintf(stderr, "ii out of bounds, %d\n", loc);
      fflush(stderr);
      exit(4);
    }
#endif  // CHECK_LOC
    for(ii = 1; ii <= loc;)
    {
      yprint(stor[(int)(ii++)]);
    }
    loc = 0;
  }
  else
  {
    if(m > 9)
    {
      wk = m / 10;
      m %= 10;

#ifdef CHECK_LOC
      // boundary check for 'loc' within 'stor[]' array
      if(loc < 0 || loc >= sizeof(stor) / sizeof(stor[0]))
      {
        fprintf(stderr, "wk1 out of bounds, %d\n", loc);
        fflush(stderr);
        exit(4);
      }
#endif  // CHECK_LOC
      for(wk1 = loc; wk1 >= 1; wk1--)
      {
        wk += stor[(int)wk1];
        stor[(int)wk1] = wk % 10;
        wk /= 10;
      }
    }
  }
#ifdef CHECK_LOC
  // boundary check for 'loc' within 'stor[]' array
  if(loc < -1 || loc >= (sizeof(stor) / sizeof(stor[0]) - 1))
  {
    fprintf(stderr, "loc out of bounds, %d\n", loc);
    fflush(stderr);
    exit(4);
  }
#endif  // CHECK_LOC

  stor[(int)(++loc)] = m;

#ifdef CHECK_LOC
  if(loc > max_loc)
  {
    max_loc = loc;
  }
#endif // CHECK_LOC
}

void memerr(int errno)
{
  printf("\a\nOut of memory error #%d\n", errno);
  if(2 == errno)
    Ffree(mf);
#ifdef _WIN32
  _exit(2);
#else  // _WIN32
  exit(2);
#endif  // _WIN32

}

#ifdef USE_THREAD
void *the_thread_proc(void *pArg)
{
int32_t temp0s, ks2, temp2;
int32_t *ps;
int i = (int)(INTPTR)pArg;

  temp2 = 2 * i - 1;
  temp0s = temp2 * ks;
  ks2 = ks << 1;

  ps = ms + i;

  if(i >= 2)
  {
    *(ps--) *= 10;
    *ps *= 10;
  }

  for(; i >= 2; i--)
  {
    temp2 -= 2;
    shift(ps, ps + 1, temp2, temp0s);
    temp0s -= ks2;
    if(i >= 2)
    {
      *(--ps) *= 10;
    }
  }

  return NULL;
}

pthread_t MyCreateThread(int i)
{
pthread_t thr;

  if(!pthread_create(&thr, NULL, the_thread_proc, (void *)(INTPTR)i))
  {
    return thr;
  }

  return 0;
}
#endif  // USE_THREAD


int main(int argc, char *argv[])
{
#ifdef _WIN32
unsigned long dwStartTick = GetTickCount();
#else   // _WIN32
unsigned long dwStartTick = MyGetTickCount();
#endif // _WIN32

int i = 0;
char *endp;

  mf = ms = NULL;
  kf = ks = 0;
  cnt = n = temp = nd = 0;
  i = 0;
  col = col1 = 0;
  loc = 0;
#ifdef CHECK_LOC
  max_loc = 0;
#endif  // CHECK_LOC
  memset(stor, 0, sizeof(stor));

  stor[i++] = 0;
  if(argc < 2)
  {
    fprintf(stderr, "\nUsage: %s <number_of_digits>\n\n", argv[0]);
    return (1);
  }

  n = strtol(argv[1], &endp, 10);

  if(NULL == (mf = (int32_t *) Fcalloc((Size_T) (n + 3L), (Size_T) sizeof(int32_t))))
  {
    memerr(1);
  }
  if(NULL == (ms = (int32_t *) Fcalloc((Size_T) (n + 3L), (Size_T) sizeof(int32_t))))
  {
    memerr(2);
  }
  printf("\nApproximation of PI to %ld digits\n", (long)n);
  cnt = 0;
  kf = 25;
  ks = 57121L;
  mf[1] = 1L;

#ifdef _WIN32

  // WIN32 addition - reduce priority down to 'background' level
  SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL - 1);

#endif  // _WIN32

  for(i = 2; i <= (int)n; i += 2)
  {
    mf[i] = -16L;
    mf[i + 1] = 16L;
  }
  for(i = 1; i <= (int)n; i += 2)
  {
    ms[i] = -4L;
    ms[i + 1] = 4L;
  }
  printf("\n 3.");
  while(cnt < n)
  {
    int32_t temp0f, temp0s;
    int32_t kf2, ks2;
    int32_t *pf, *ps;

    i = (int)(n - cnt);

#ifdef USE_THREAD
    if(i >= USE_THREAD)
    {
      void *pRval;
      pthread_t iThread = MyCreateThread(i);

      if(!iThread) // didn't create the thread -- oops!
      {
        goto old_way;
      }

      temp = 2 * i - 1;
      temp0f = temp * kf;
      kf2 = kf << 1;

      pf = mf + i;

      if(i >= 2)
      {
        *(pf--) *= 10;
        *pf *= 10;
      }

      for(; i >= 2; i--)
      {
        temp -= 2;
        shift(pf, pf + 1, temp, temp0f);
        temp0f -= kf2;
        if(i >= 2)
        {
          *(--pf) *= 10;
        }
      }

      pthread_join(iThread, &pRval); // waits for thread to exit, then continues
    }
    else
#endif // USE_THREAD
    {
#ifdef USE_THREAD
old_way:
#endif // USE_THREAD
      temp = 2 * i - 1;
      temp0f = temp * kf;
      kf2 = kf << 1;
      temp0s = temp * ks;
      ks2 = ks << 1;

      pf = mf + i;
      ps = ms + i;

      if(i >= 2)
      {
        *(pf--) *= 10;
        *pf *= 10;
        *(ps--) *= 10;
        *ps *= 10;
      }

      for(; i >= 2; i--)
      {
        temp -= 2;
        shift(pf, pf + 1, temp, temp0f);
        shift(ps, ps + 1, temp, temp0s);
        temp0f -= kf2;
        temp0s -= ks2;
        if(i >= 2)
        {
          *(--pf) *= 10;
          *(--ps) *= 10;
        }
      }
    }

    nd = 0;
    shift1((int32_t FAR *) & nd, mf + 1, 5L);
    shift1((int32_t FAR *) & nd, ms + 1, 239L);
    xprint(nd);
  }
#ifdef CHECK_LOC
  printf("\n\nCalculations Completed!  max_loc=%d\n", max_loc);
#else  // CHECK_LOC
  printf("\n\nCalculations Completed!\n");
#endif  // CHECK_LOC
#ifdef _WIN32
  dwStartTick = GetTickCount() - dwStartTick;
#else  // _WIN32
  dwStartTick = MyGetTickCount() - dwStartTick;
#endif  // _WIN32

  // proper display of total time
  printf("\nTotal time:  %d:%02d:%02d.%03d  (%ld msecs)\n",
         (int)(dwStartTick / 3600000L),
         (int)((dwStartTick / 60000L) % 60),
         (int)((dwStartTick / 1000L) % 60),
         (int)(dwStartTick % 1000L),
         dwStartTick);

  Ffree(ms);
  Ffree(mf);
  return (0);
}

