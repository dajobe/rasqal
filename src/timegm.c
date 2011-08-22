/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * timegm.c - timegm compatibility
 *
 * This file is in the public domain.
 * 
 */

#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif


#include "rasqal.h"
#include "rasqal_internal.h"

#ifdef WIN32
time_t
rasqal_timegm(struct tm *tm)
{
  struct tm my_tm;

  memcpy(&my_tm, tm, sizeof(struct tm));

  /* _mkgmtime() changes the value of the struct tm* you pass in, so
   * use a copy
   */
  return _mkgmtime(&my_tm);
}

#else

time_t
rasqal_timegm(struct tm *tm)
{
  time_t result;
  char *zone;

  zone = getenv("TZ");
  if(zone) {
    /* save it so that setenv() does not destroy shared value */
    size_t zone_len = strlen(zone);
    char *zone_copy = RASQAL_MALLOC(char*, zone_len + 1);
    if(!zone_copy)
      return -1;

    memcpy(zone_copy, zone, zone_len + 1); /* Copy NUL */
    zone = zone_copy;
  }
    
  setenv("TZ", "UTC", 1);
  tzset();

  result = mktime(tm);

  if(zone)
    setenv("TZ", zone, 1);
  else
    unsetenv("TZ");
  tzset();

  if(zone)
    RASQAL_FREE(char*, zone);

  return result;
}
#endif
