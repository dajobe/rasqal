/* -*- Mode: c; c-basic-offset: 2 -*-
/*
 * gettimeofday.c - gettimeofday compatibility (Windows)
 *
 * This file is in the public domain.
 * 
 */

#ifdef WIN32
#include <win32_rasqal_config.h>

#include <time.h>
#include <windows.h>

/* Windows time epoch is 100ns units since 1 Jan 1601
 * Unix time epoch is second units since 1 Jan 1970 
 */
#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
  #define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif
 

int
rasqal_gettimeofday(struct timeval *tv, struct timezone *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;
 
  if(tv) {
    /* Abailable from Win95+ */
    GetSystemTimeAsFileTime(&ft);
 
    tmpres |= ft.dwHighDateTime;
    tmpres <<= 32;
    tmpres |= ft.dwLowDateTime;
 
    /* convert file time epoch to unix epoch */
    tmpres -= DELTA_EPOCH_IN_MICROSECS; 

    /* convert from 100ns units (0.1 microseconds) into microseconds */
    tmpres /= 10;

    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }
 
  if(tz) {
    if(!tzflag) {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }
 
  return 0;
}

#endif
