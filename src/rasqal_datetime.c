/*
 * rasqal_datetime.c - Rasqal XSD dateTime and XSD date
 *
 * Copyright (C) 2007-2011, David Beckett http://www.dajobe.org/
 *
 * Contributions:
 *   Copyright (C) 2007, Lauri Aalto <laalto@iki.fi>
 * 
 * This package is Free Software and part of Redland http://librdf.org/
 * 
 * It is licensed under the following three licenses as alternatives:
 *   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
 *   2. GNU General Public License (GPL) V2 or any newer version
 *   3. Apache License, V2.0 or any newer version
 * 
 * You may not use this file except in compliance with at least one of
 * the above three licenses.
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * complete terms and further detail along with the license texts for
 * the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
 * 
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
#include <ctype.h>
#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# ifdef HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <limits.h>

#include "rasqal.h"
#include "rasqal_internal.h"

/* Local definitions */
 
static int rasqal_xsd_datetime_parse(const char *datetime_string, rasqal_xsd_datetime *result, int is_dateTime);
static unsigned int days_per_month(int month, int year);


#ifndef ISNUM
#define ISNUM(c) ((c) >= '0' && (c) <= '9')
#endif


/**
 * rasqal_xsd_datetime_normalize:
 * @datetime: date time
 *
 * INTERNAL - Normalize a date time into the allowed range
 *
 * The result will always give either
 *   have_tz 'N' with timezone_minutes RASQAL_XSD_DATETIME_NO_TZ
 *   have_tz 'Z' with timezone_minutes 0
 *
 * Return value: zero on success, non zero on failure.
 */
static int
rasqal_xsd_datetime_normalize(rasqal_xsd_datetime *datetime)
{
  int t;

  if(datetime->have_tz == 'Y') {
    if(datetime->timezone_minutes) {
      /* Normalize to Zulu if there was a timezone offset */
      datetime->hour   = RASQAL_GOOD_CAST(signed char, datetime->hour - (datetime->timezone_minutes / 60));
      datetime->minute = RASQAL_GOOD_CAST(signed char, datetime->minute - (datetime->timezone_minutes % 60));

      datetime->timezone_minutes = 0;
    }
    datetime->have_tz = 'Z';
  }
  
  /* second & second parts: no need to normalize as they are not
   * touched after range check
   */
  
  /* minute */
  if(datetime->minute < 0) {
    datetime->minute = RASQAL_GOOD_CAST(signed char, datetime->minute + 60);
    datetime->hour--;
  } else if(datetime->minute > 59) {
    datetime->minute = RASQAL_GOOD_CAST(signed char, datetime->minute - 60);
    datetime->hour++;
  }
  
  /* hour */
  if(datetime->hour < 0) {
    datetime->hour = RASQAL_GOOD_CAST(signed char, datetime->hour + 24);
    datetime->day--;
  } else if(datetime->hour > 23) {
    datetime->hour = RASQAL_GOOD_CAST(signed char, datetime->hour - 24);
    datetime->day++;
  }
  
  /* day */
  if(datetime->day < 1) {
    int y2;
    t = --datetime->month;
    /* going back beyond year boundary? */
    if(!t) {
      t = 12;
      y2 = datetime->year-1;
    } else
      y2 = datetime->year;
    datetime->day = RASQAL_GOOD_CAST(unsigned char, datetime->day + days_per_month(t, y2));
  } else {
    t = RASQAL_GOOD_CAST(int, days_per_month(datetime->month, datetime->year));
    if(datetime->day > t) {
      datetime->day = RASQAL_GOOD_CAST(unsigned char, datetime->day - t);
      datetime->month++;
    }
  }

  /* month & year */
  if(datetime->month < 1) {
    datetime->month = RASQAL_GOOD_CAST(unsigned char, datetime->month + 12);
    datetime->year--;
    /* there is no year 0 - go backwards to year -1 */
    if(!datetime->year)
      datetime->year--;
  } else if(datetime->month > 12) {
    datetime->month = RASQAL_GOOD_CAST(unsigned char, datetime->month - 12);
    datetime->year++;
    /* there is no year 0 - go forwards to year 1 */
    if(!datetime->year)
      datetime->year++;
  }

  datetime->time_on_timeline = rasqal_xsd_datetime_get_as_unixtime(datetime);

  /* success */
  return 0;
}


/**
 * rasqal_xsd_datetime_parse:
 * @datetime_string: xsd:dateTime as lexical form string
 * @result: target struct for holding dateTime components
 * @is_dateTime: is xsd:dateTime and should look for time (hour, mins, secs)
 *   otherwise is xsd:date and should skip to looking for timezone
 *
 * INTERNAL - Parse a xsd:dateTime string into a #rasqal_xsd_datetime struct.
 *
 * Does NOT normalize the structure.  Call
 * rasqal_xsd_datetime_normalize() to do that.
 *
 * http://www.w3.org/TR/xmlschema-2/#dt-dateTime
 *
 * "The lexical space of dateTime consists of finite-length sequences of
 * characters of the form:
 * '-'? yyyy '-' mm '-' dd 'T' hh ':' mm ':' ss ('.' s+)? (zzzzzz)?,
 * where
 *
 * * '-'? yyyy is a four-or-more digit optionally negative-signed numeral that
 *   represents the year; if more than four digits, leading zeros are
 *   prohibited, and '0000' is prohibited (see the Note above (3.2.7); also
 *   note that a plus sign is not permitted);
 * * the remaining '-'s are separators between parts of the date portion;
 * * the first mm is a two-digit numeral that represents the month;
 * * dd is a two-digit numeral that represents the day;
 * * 'T' is a separator indicating that time-of-day follows;
 * * hh is a two-digit numeral that represents the hour; '24' is permitted if
 *   the minutes and seconds represented are zero, and the dateTime value so
 *   represented is the first instant of the following day (the hour property
 *   of a dateTime object in the value space cannot have a value greater
 *   than 23);
 * * ':' is a separator between parts of the time-of-day portion;
 * * the second mm is a two-digit numeral that represents the minute;
 * * ss is a two-integer-digit numeral that represents the whole seconds;
 * * '.' s+ (if present) represents the fractional seconds;
 * * zzzzzz (if present) represents the timezone"
 *
 *
 *  http://www.w3.org/TR/xmlschema-2/#dt-date
 *  lexical space: '-'? yyyy '-' mm '-' dd zzzzzz? 
 *
 * Return value: zero on success, non zero on failure.
 */
static int
rasqal_xsd_datetime_parse(const char *datetime_string,
                          rasqal_xsd_datetime *result,
                          int is_dateTime)
{
  const char *p, *q; 
#define B_SIZE 16
  char b[B_SIZE];
  unsigned int l, t, t2, is_neg;
  unsigned long u;
#define MICROSECONDS_MAX_DIGITS 6

  if(!datetime_string || !result)
    return -1;
  
  p = (const char *)datetime_string;
  is_neg = 0;

  /* Parse year */
  
  /* negative years permitted */
  if(*p == '-') {
    is_neg = 1;
    p++;
  }
  for(q = p; ISNUM(*p); p++)
    ;
  l = RASQAL_GOOD_CAST(unsigned int, p - q);
  
  /* error if
     - less than 4 digits in year
     - more than 4 digits && leading zeros
     - '-' does not follow numbers
   */
  if(l < 4 || (l > 4 && *q=='0') || *p != '-')
    return -1;

  if(l >= (B_SIZE - 1))
    l = RASQAL_GOOD_CAST(unsigned int, B_SIZE - 1);

  memcpy(b, q, l);
  b[l] = 0; /* ensure nul termination */
  u = strtoul(b, 0, 10);
  
  /* year "0000" not permitted
   * restrict to signed int range
   * >= instead of > to allow for +-1 year adjustment in normalization
   * (however, these +-INT_MAX years cannot be parsed back in if
   * converted to string)
   */
  if(!u || u >= INT_MAX)
    return -1;
    
  result->year = is_neg ? -RASQAL_GOOD_CAST(int, u) : RASQAL_GOOD_CAST(int, u);

  /* parse month */
  
  for(q = ++p; ISNUM(*p); p++)
    ;
  l = RASQAL_GOOD_CAST(unsigned int, p - q);
  
  /* error if month is not 2 digits or '-' is not the separator */
  if(l != 2 || *p != '-')
    return -2;
  
  t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0')*10);
  t += RASQAL_GOOD_CAST(unsigned int, *q - '0');
  
  /* month must be 1..12 */
  if(t < 1 || t > 12)
    return -2;
  
  result->month = RASQAL_GOOD_CAST(unsigned char, t);
  
  /* parse day */
  
  for(q = ++p; ISNUM(*p); p++)
    ;
  l = RASQAL_GOOD_CAST(unsigned int, p - q);

  if(is_dateTime) {
    /* xsd:dateTime: error if day is not 2 digits or 'T' is not the separator */
    if(l != 2 || *p != 'T')
      return -3;
  } else {
    /* xsd:date: error if day is not 2 digits or separator is not
     * 'Z' (utc)
     * '+' or '-' (timezone offset)
     * nul (end of string - timezone is optional)
     */
    if(l != 2 || (*p && *p != 'Z' && *p != '+' && *p != '-'))
      return -3;
  }
  
  t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0') * 10);
  t += RASQAL_GOOD_CAST(unsigned int, *q - '0');

  /* day must be 1..days_per_month */
  if(t < 1 || t > days_per_month(result->month, result->year))
    return -3;

  result->day = RASQAL_GOOD_CAST(unsigned char, t);

  if(is_dateTime) {
    /* parse hour */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = RASQAL_GOOD_CAST(unsigned int, p - q);

    /* error if hour is not 2 digits or ':' is not the separator */
    if(l != 2 || *p != ':')
      return -4;

    t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0')*10);
    t += RASQAL_GOOD_CAST(unsigned int, *q - '0');

    /* hour must be 0..24 - will handle special case 24 later
     * (no need to check for < 0)
     */
    if(t > 24)
      return -4;

    result->hour = RASQAL_GOOD_CAST(signed char, t);

    /* parse minute */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = RASQAL_GOOD_CAST(unsigned int, p - q);

    /* error if minute is not 2 digits or ':' is not the separator */
    if(l != 2 || *p != ':')
      return -5;

    t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0') * 10);
    t += RASQAL_GOOD_CAST(unsigned int, *q - '0');

    /* minute must be 0..59
     * (no need to check for < 0)
     */
    if(t > 59)
      return -5;

    result->minute = RASQAL_GOOD_CAST(signed char, t);

    /* parse second whole part */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = RASQAL_GOOD_CAST(unsigned int, p - q);

    /* error if second is not 2 digits or separator is not 
     * '.' (second fraction)
     * 'Z' (utc)
     * '+' or '-' (timezone offset)
     * nul (end of string - second fraction and timezone are optional)
     */
    if(l != 2 || (*p && *p != '.' && *p != 'Z' && *p != '+' && *p != '-'))
      return -6;

    t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0')*10);
    t += RASQAL_GOOD_CAST(unsigned int, *q - '0');

    /* second must be 0..59
    * (no need to check for < 0)
    */
    if(t > 59)
      return -6;

    result->second = RASQAL_GOOD_CAST(signed char, t);

    /* now that we have hour, minute and second, we can check
     * if hour == 24 -> only 24:00:00 permitted (normalized later)
     */
    if(result->hour == 24 && (result->minute || result->second))
      return -7;

    /* parse fraction seconds if any */
    result->microseconds = 0;
    if(*p == '.') {
      for(q = ++p; ISNUM(*p); p++)
        ;

      /* ignore trailing zeros */
      while(*--p == '0')
        ;
      p++;

      if(!(*q == '0' && q == p)) {
        /* allow ".0" */
        l = RASQAL_GOOD_CAST(unsigned int, p - q);

        if(l < 1) /* need at least 1 num */
          return -8;

        /* support only to microseconds with truncation */
        if(l > MICROSECONDS_MAX_DIGITS)
          l = MICROSECONDS_MAX_DIGITS;
        
        result->microseconds = 0;
        for(t2 = 0; t2 < MICROSECONDS_MAX_DIGITS; ++t2) {
          if(t2 < l)
            result->microseconds += (*q++ - '0');
          if(t2 != MICROSECONDS_MAX_DIGITS - 1)
            result->microseconds *= 10;
        }

      }

      /* skip ignored trailing zeros */
      while(*p == '0')
        p++;
    }

  } else { /* end if is_dateTime */
    /* set to center of day interval (noon) */
    result->hour = 12;
    result->minute = 0;
    result->second = 0;
    result->microseconds = 0;
  }

  
  /* parse & adjust timezone offset */
  /* result is normalized later */
  result->timezone_minutes = RASQAL_XSD_DATETIME_NO_TZ;
  result->have_tz = 'N';
  if(*p) {
    if(*p == 'Z') {
      /* utc timezone - no need to adjust */
      result->timezone_minutes = 0;
      result->have_tz = 'Z';
      p++;
    } else if(*p == '+' || *p == '-') {
      result->timezone_minutes = 0;
      result->have_tz = 'Y';

      /* work out timezone offsets */
      is_neg = *p == '-';
     
      /* timezone hours */
      for(q = ++p; ISNUM(*p); p++)
        ;
      l = RASQAL_GOOD_CAST(unsigned int, p - q);
      if(l != 2 || *p!=':')
        return -9;

      t2 = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0') * 10);
      t2 += RASQAL_GOOD_CAST(unsigned int, *q - '0');
      if(t2 > 14)
        /* tz offset hours are restricted to 0..14
         * (no need to check for < 0)
         */
        return -9;
    
      result->timezone_minutes = RASQAL_GOOD_CAST(short int, (is_neg ? -t2 : t2) * 60);

      /* timezone minutes */    
      for(q = ++p; ISNUM(*p); p++)
        ;
      l = RASQAL_GOOD_CAST(unsigned int, p - q);
      if(l != 2)
        return -10;

      t = RASQAL_GOOD_CAST(unsigned int, (*q++ - '0') * 10);
      t += RASQAL_GOOD_CAST(unsigned int, *q - '0');
      if(t > 59 || (t2 == 14 && t != 0)) {
        /* tz offset minutes are restricted to 0..59
         * (no need to check for < 0)
         * or 0 if hour offset is exactly +-14 
         */
        return -10;
      }
    
      result->timezone_minutes = RASQAL_GOOD_CAST(short int, result->timezone_minutes + RASQAL_GOOD_CAST(short int, (is_neg ? -t : t)));
    }
    
    /* failure if extra chars after the timezone part */
    if(*p)
      return -11;

  }

  /* Initialise field even though this is not valid before
   * rasqal_xsd_datetime_normalize() is called on this object.
   */
  result->time_on_timeline = 0;

  return 0;
}


static int
rasqal_xsd_date_parse(const char *date_string, rasqal_xsd_date *result)
{
  rasqal_xsd_datetime dt_result; /* on stack */
  int rc;

  rc = rasqal_xsd_datetime_parse(date_string, &dt_result, 0);
  if(!rc) {
    result->year = dt_result.year;
    result->month = dt_result.month;
    result->day = dt_result.day;
    result->time_on_timeline = dt_result.time_on_timeline;
    result->timezone_minutes = dt_result.timezone_minutes;
    result->have_tz = dt_result.have_tz; /* This will be N or Z */
  }

  return rc;
}

#ifdef STANDALONE
/**
 * rasqal_xsd_date_normalize:
 * @date: date
 *
 * INTERNAL - Normalize a date into the allowed range
 *
 * Return value: zero on success, non zero on failure.
 */
static int
rasqal_xsd_date_normalize(rasqal_xsd_date *date)
{
  rasqal_xsd_datetime dt_result; /* on stack */
  int rc;

  memset(&dt_result, '\0', sizeof(dt_result));

  dt_result.year = date->year;
  dt_result.month = date->month;
  dt_result.day = date->day;
  /* set to center of day interval (noon) */
  dt_result.hour   = 12;
  dt_result.minute =  0;
  dt_result.second =  0;
  dt_result.microseconds = 0;
  dt_result.timezone_minutes = date->timezone_minutes;
  dt_result.have_tz = date->have_tz;

  rc = rasqal_xsd_datetime_normalize(&dt_result);
  if(!rc) {
    date->year = dt_result.year;
    date->month = dt_result.month;
    date->day = dt_result.day;
    date->time_on_timeline = dt_result.time_on_timeline;
    date->timezone_minutes = dt_result.timezone_minutes;
    date->have_tz = dt_result.have_tz; /* This will be N or Z */
  }

  return rc;
}
#endif /* STANDALONE */



/**
 * rasqal_new_xsd_datetime:
 * @world: world object
 * @datetime_string: XSD Datetime string
 *
 * Constructor - make a new XSD datetime object from a string
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime(rasqal_world* world, const char *datetime_string)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = RASQAL_MALLOC(rasqal_xsd_datetime*, sizeof(*dt));
  if(!dt)
    return NULL;
  
  rc = rasqal_xsd_datetime_parse(datetime_string, dt, 1);
  if(!rc) {
    rasqal_xsd_datetime dt_temp; /* copy on stack to normalize */
    memcpy(&dt_temp, dt, sizeof(dt_temp));
    
    rc = rasqal_xsd_datetime_normalize(&dt_temp);
    if(!rc)
      dt->time_on_timeline = dt_temp.time_on_timeline;
  }

  if(rc) {
    rasqal_free_xsd_datetime(dt); dt = NULL;
  }

  return dt;
}


/**
 * rasqal_new_xsd_datetime_from_unixtime:
 * @world: world object
 * @secs: unixtime
 *
 * Constructor - make a new XSD datetime object from unixtime seconds
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime_from_unixtime(rasqal_world* world, time_t secs)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = RASQAL_MALLOC(rasqal_xsd_datetime*, sizeof(*dt));
  if(!dt)
    return NULL;

  rc = rasqal_xsd_datetime_set_from_unixtime(dt, secs);

  if(rc) {
    rasqal_free_xsd_datetime(dt); dt = NULL;
  }

  return dt;
}


/**
 * rasqal_new_xsd_datetime_from_timeval:
 * @world: world object
 * @tv: pointer to struct timeval
 *
 * Constructor - make a new XSD datetime object from a timeval
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime_from_timeval(rasqal_world* world, struct timeval *tv)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = RASQAL_MALLOC(rasqal_xsd_datetime*, sizeof(*dt));
  if(!dt)
    return NULL;

  rc = rasqal_xsd_datetime_set_from_timeval(dt, tv);

  if(rc) {
    rasqal_free_xsd_datetime(dt); dt = NULL;
  }

  return dt;
}


/**
 * rasqal_new_xsd_datetime_from_xsd_date:
 * @world: world object
 * @date: pointer to XSD date
 *
 * Constructor - make a new XSD datetime object from an XSD date
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime_from_xsd_date(rasqal_world* world, rasqal_xsd_date *date)
{
  rasqal_xsd_datetime* dt;
  
  dt = RASQAL_CALLOC(rasqal_xsd_datetime*, 1, sizeof(*dt));
  if(!dt)
    return NULL;

  dt->year = date->year;
  dt->month = date->month;
  dt->day = date->day;
  /* hour, minute, seconds, microseconds are all zero from calloc */
  dt->timezone_minutes = date->timezone_minutes;
  dt->time_on_timeline = date->time_on_timeline;
  dt->have_tz = date->have_tz;
  
  return dt;
}


/**
 * rasqal_free_xsd_datetime:
 * @dt: datetime object
 * 
 * Destroy XSD datetime object.
 **/
void
rasqal_free_xsd_datetime(rasqal_xsd_datetime* dt)
{
  if(!dt)
    return;
  
  RASQAL_FREE(datetime, dt);
}


#define TIMEZONE_BUFFER_LEN 6

/*
 * rasqal_xsd_timezone_format:
 * @timezone_minutes: timezone minutes from #rasqal_xsd_datetime or #rasqal_xsd_date
 * @have_tz: have tz flag from #rasqal_xsd_datetime or #rasqal_xsd_date
 * @buffer: buffer to write the formatted timezone
 * @bufsize: length of @buffer; must be 7 or larger
 *
 * INTERNAL - format a timezone into the passed in buffer
 *
 * Return value: size of buffer or 0 on failure
 */
static int
rasqal_xsd_timezone_format(signed short timezone_minutes,
                           char have_tz,
                           char* buffer, size_t bufsize)
{
  size_t tz_len;
  
  if(!buffer || !bufsize)
    return -1;
  
  if(have_tz == 'N') {
    tz_len = 0;

    buffer[0] = '\0';
  } else if(have_tz == 'Z') {
    tz_len = 1;
    if(bufsize < (tz_len + 1))
      return -1;

    buffer[0] = 'Z';
    buffer[1] = '\0';
  } else {
    int mins;
    int hours;
    int digit;

    tz_len = TIMEZONE_BUFFER_LEN;

    if(bufsize < (tz_len + 1))
      return -1;

    mins = abs(timezone_minutes);
    buffer[0] = (!mins || mins != timezone_minutes ? '-' : '+');

    hours = (mins / 60);
    digit = (hours / 10);
    buffer[1] = RASQAL_GOOD_CAST(char, digit + '0');
    buffer[2] = RASQAL_GOOD_CAST(char, hours - (digit * 10) + '0');
    buffer[3] = ':';

    mins -= hours * 60;
    buffer[4] = RASQAL_GOOD_CAST(char, (mins / 10) + '0');
    mins -= mins * 10;
    buffer[5] = RASQAL_GOOD_CAST(char, mins + '0');

    buffer[6] = '\0';
  }

  return RASQAL_GOOD_CAST(int, tz_len);
}


static int
rasqal_xsd_format_microseconds(char* buffer, size_t bufsize, 
                               unsigned int microseconds)
{
  int len = 0;
  char *p;
  unsigned int value;
  unsigned int base = 10;
  unsigned int multiplier;

  value = microseconds;
  multiplier = 100000;
  do {
    value = value % multiplier;
    multiplier /= base;
    len++;
  } while(value && multiplier);

  if(!buffer || RASQAL_GOOD_CAST(int, bufsize) < (len + 1)) /* +1 for NUL */
    return len;

  value = microseconds;
  multiplier = 100000;
  p = buffer;
  do {
    unsigned digit = value / multiplier;
    *p++ = RASQAL_GOOD_CAST(char, '0' + digit);
    value = value % multiplier;
    multiplier /= base;
  } while(value && multiplier);
  *p = '\0';

  return len;
}


/**
 * rasqal_xsd_datetime_to_counted_string:
 * @dt: datetime struct
 * @len_p: output length (or NULL)
 *
 * Convert a #rasqal_xsd_datetime struct to a xsd:dateTime lexical form counted string.
 *
 * Caller should rasqal_free_memory() the returned string.
 *
 * See http://www.w3.org/TR/xmlschema-2/#dateTime-canonical-representation
 *
 * Return value: lexical form string or NULL on failure.
 */
char*
rasqal_xsd_datetime_to_counted_string(const rasqal_xsd_datetime *dt,
                                      size_t *len_p)
{
  size_t len;
  char *buffer = NULL;
  char *p;
  /* "[+-]HH:MM\0" */
  char timezone_string[TIMEZONE_BUFFER_LEN + 1];
  size_t year_len;
  int tz_string_len;
  size_t microseconds_len = 0;

  /*
   * http://www.w3.org/TR/xmlschema-2/#dateTime-canonical-representation
   *
   * "Except for trailing fractional zero digits in the seconds representation,
   * '24:00:00' time representations, and timezone (for timezoned values),
   * the mapping from literals to values is one-to-one.
   * Where there is more than one possible representation,
   * the canonical representation is as follows:
   *    * The 2-digit numeral representing the hour must not be '24';
   *    * The fractional second string, if present, must not end in '0';
   *    * for timezoned values, the timezone must be represented with 'Z'
   *      (All timezoned dateTime values are UTC.)."
   */ 

  if(!dt)
    return NULL;
    
  tz_string_len = rasqal_xsd_timezone_format(dt->timezone_minutes, dt->have_tz,
                                             timezone_string,
                                             TIMEZONE_BUFFER_LEN + 1);
  if(tz_string_len < 0)
    return NULL;

  year_len = rasqal_format_integer(NULL, 0, dt->year, 4, '0');
  
  len = year_len +
        RASQAL_GOOD_CAST(size_t, 15) + /* "-MM-DDTHH:MM:SS" = 15 */
        RASQAL_GOOD_CAST(size_t, tz_string_len);
  if(dt->microseconds) {
    microseconds_len = RASQAL_GOOD_CAST(size_t, rasqal_xsd_format_microseconds(NULL, 0,
                                                                               RASQAL_GOOD_CAST(unsigned int, dt->microseconds)));
    len += 1 /* . */ + microseconds_len;
  }
  
  if(len_p)
    *len_p = len;
    
  buffer = RASQAL_MALLOC(char*, len + 1);
  if(!buffer)
    return NULL;

  p = buffer;
  p += rasqal_format_integer(p, year_len + 1, dt->year, 4, '0');
  *p++ = '-';
  p += rasqal_format_integer(p, 2 + 1, dt->month, 2, '0');
  *p++ = '-';
  p += rasqal_format_integer(p, 2 + 1, dt->day, 2, '0');
  *p++ = 'T';

  p += rasqal_format_integer(p, 2 + 1, dt->hour, 2, '0');
  *p++ = ':';
  p += rasqal_format_integer(p, 2 + 1, dt->minute, 2, '0');
  *p++ = ':';
  p += rasqal_format_integer(p, 2 + 1, dt->second, 2, '0');

  if(dt->microseconds) {
    *p++ = '.';
    p += rasqal_xsd_format_microseconds(p, microseconds_len + 1,
                                        RASQAL_GOOD_CAST(unsigned int, dt->microseconds));
  }
  if(tz_string_len) {
    memcpy(p, timezone_string, RASQAL_GOOD_CAST(size_t, tz_string_len));
    p += tz_string_len;
  }

  *p = '\0';

  return buffer;
}


/**
 * rasqal_xsd_datetime_to_string:
 * @dt: datetime struct
 *
 * Convert a #rasqal_xsd_datetime struct to a xsd:dateTime lexical form string.
 *
 * Caller should rasqal_free_memory() the returned string.
 *
 * Return value: lexical form string or NULL on failure.
 */
char*
rasqal_xsd_datetime_to_string(const rasqal_xsd_datetime *dt)
{
  return rasqal_xsd_datetime_to_counted_string(dt, NULL);
}


/**
 * rasqal_xsd_datetime_equals2:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * @incomparible_p: address to store incomparable flag (or NULL)
 * 
 * Compare two XSD dateTimes for equality.
 * 
 * Return value: non-0 if equal.
 **/
int
rasqal_xsd_datetime_equals2(const rasqal_xsd_datetime *dt1,
                            const rasqal_xsd_datetime *dt2,
                            int *incomparible_p)
{
  int cmp = rasqal_xsd_datetime_compare2(dt1, dt2, incomparible_p);
  return !cmp;
}


#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_xsd_datetime_equals:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * 
 * Compare two XSD dateTimes for equality.
 *
 * @Deprecated: for rasqal_xsd_datetime_equals2 that returns incomparibility.
 *
 * Return value: non-0 if equal.
 **/
int
rasqal_xsd_datetime_equals(const rasqal_xsd_datetime *dt1,
                           const rasqal_xsd_datetime *dt2)
{
  return rasqal_xsd_datetime_equals2(dt1, dt2, NULL);
}
#endif

/*
 * 3.2.7.4 Order relation on dateTime
 * http://www.w3.org/TR/2004/REC-xmlschema-2-20041028/#dateTime
 */
static int
rasqal_xsd_timeline_compare(time_t dt_timeline1, signed int dt_msec1,
                            signed short tz_minutes1,
                            time_t dt_timeline2, signed int dt_msec2,
                            signed short tz_minutes2,
                            int *incomparible_p)
{
  int dt1_has_tz = (tz_minutes1 != RASQAL_XSD_DATETIME_NO_TZ);
  int dt2_has_tz = (tz_minutes2 != RASQAL_XSD_DATETIME_NO_TZ);
  int rc;

#define SECS_FOR_14_HOURS (14 * 3600)

  /* Normalize - if there is a timezone that is not Z, convert it to Z
   *
   * Already done in rasqal_xsd_datetime_normalize() on construction
   */

  if(dt1_has_tz == dt2_has_tz) {
    /* both are on same timeline */
    if(dt_timeline1 < dt_timeline2)
      rc = -1;
    else if(dt_timeline1 > dt_timeline2)
      rc = 1;
    else
      rc = dt_msec1 - dt_msec2;
  } else if(dt1_has_tz) {
    /* dt1 has a tz, dt2 has no tz */
    if(dt_timeline1 < (dt_timeline2 - SECS_FOR_14_HOURS))
      rc = -1;
    else if(dt_timeline1 > (dt_timeline2 + SECS_FOR_14_HOURS))
      rc = 1;
    else {
      if(incomparible_p)
        *incomparible_p = 1;
      rc = 2; /* incomparible really */
    }
  } else {
    /* dt1 has no tz, dt2 has a tz */
    if((dt_timeline1 + SECS_FOR_14_HOURS) < dt_timeline2)
      rc = -1;
    else if((dt_timeline1 - SECS_FOR_14_HOURS) > dt_timeline2)
      rc = 1;
    else {
      if(incomparible_p)
        *incomparible_p = 1;
      rc = 2; /* incomparible really */
    }
  }

  return rc;
}


/**
 * rasqal_xsd_datetime_compare2:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * @incomparible_p: address to store incomparable flag (or NULL)
 * 
 * Compare two XSD dateTimes
 *
 * If the only one of the two dateTimes have timezones, the results
 * may be incomparible and that will return >0 and set the
 * value of the int point to by @incomparible_p to non-0
 * 
 * Return value: <0 if @dt1 is less than @dt2, 0 if equal, >0 otherwise
 **/
int
rasqal_xsd_datetime_compare2(const rasqal_xsd_datetime *dt1,
                             const rasqal_xsd_datetime *dt2,
                             int *incomparible_p)
{
  if(incomparible_p)
    *incomparible_p = 0;

  /* Handle NULLs */
  if(!dt1 || !dt2) {
    /* NULLs sort earlier. equal only if both are NULL */
    if(!dt1 && !dt2)
      return 0;

    return (!dt1) ? -1 : 1;
  }

  return rasqal_xsd_timeline_compare(dt1->time_on_timeline, dt1->microseconds,
                                     dt1->timezone_minutes,
                                     dt2->time_on_timeline, dt2->microseconds,
                                     dt2->timezone_minutes,
                                     incomparible_p);
}

#ifndef RASQAL_DISABLE_DEPRECATED
/**
 * rasqal_xsd_datetime_compare:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * 
 * Compare two XSD dateTimes
 * 
 * @Deprecated for rasqal_xsd_datetime_compare2() which can return the incomparible result.
 *
 * Return value: <0 if @dt1 is less than @dt2, 0 if equal, >0 otherwise
 **/
int
rasqal_xsd_datetime_compare(const rasqal_xsd_datetime *dt1,
                            const rasqal_xsd_datetime *dt2)
{
  return rasqal_xsd_datetime_compare2(dt1, dt2, NULL);
}
#endif

/**
 * rasqal_xsd_datetime_get_seconds_as_decimal:
 * @world: world object
 * @dt: XSD dateTime
 * 
 * Get the seconds component of a dateTime as a decimal
 * 
 * Return value: decimal object or NULL on failure
 **/
rasqal_xsd_decimal*
rasqal_xsd_datetime_get_seconds_as_decimal(rasqal_world* world,
                                           rasqal_xsd_datetime* dt)
{
  rasqal_xsd_decimal* dec;

  dec = rasqal_new_xsd_decimal(world);
  if(!dec)
    return NULL;
  
  if(!dt->microseconds) {
    rasqal_xsd_decimal_set_long(dec, (long)dt->second);
  } else {
    /* Max len 9 "SS.UUUUUU\0" */
    char str[10];

    sprintf(str, "%d.%06d", dt->second, dt->microseconds);

    rasqal_xsd_decimal_set_string(dec, str);
  }
  
  return dec;
}


/* xsd:date formatted length excluding formatted year length */
#define DATE_BUFFER_LEN_NO_YEAR 6

/**
 * rasqal_xsd_date_to_counted_string:
 * @date: date struct
 * @len_p: output length (or NULL)
 *
 * Convert a #rasqal_xsd_date struct to a xsd:date lexical form string.
 *
 * Caller should rasqal_free_memory() the returned string.
 *
 * See http://www.w3.org/TR/xmlschema-2/#date-canonical-representation
 * 
 * Return value: lexical form string or NULL on failure.
 */
char*
rasqal_xsd_date_to_counted_string(const rasqal_xsd_date *date, size_t *len_p)
{
  char *buffer = NULL;
  size_t len;
  char *p;
  int value;
  unsigned int d;
  size_t year_len;
  /* "[+-]HH:MM\0" */
  char timezone_string[TIMEZONE_BUFFER_LEN + 1];
  int tz_string_len;
  
  /* http://www.w3.org/TR/xmlschema-2/#date-canonical-representation
   *
   * "the date portion of the canonical representation (the entire
   * representation for nontimezoned values, and all but the timezone
   * representation for timezoned values) is always the date portion of
   * the dateTime canonical representation of the interval midpoint
   * (the dateTime representation, truncated on the right to eliminate
   * 'T' and all following characters). For timezoned values, append
   * the canonical representation of the ·recoverable timezone·. "
   *
   */

  if(!date)
    return NULL;
    
  tz_string_len = rasqal_xsd_timezone_format(date->timezone_minutes,
                                             date->have_tz,
                                             timezone_string,
                                             TIMEZONE_BUFFER_LEN + 1);
  if(tz_string_len < 0)
    return NULL;

  year_len = rasqal_format_integer(NULL, 0, date->year, -1, '\0');

  len = year_len + DATE_BUFFER_LEN_NO_YEAR + RASQAL_GOOD_CAST(size_t, tz_string_len);
  
  if(len_p)
    *len_p = len;
  
  buffer = RASQAL_MALLOC(char*, len + 1);
  if(!buffer)
    return NULL;

  p = buffer;

  /* value is year; length can vary */
  p += rasqal_format_integer(p, year_len + 1, date->year, -1, '\0');

  *p++ = '-';

  /* value is 2-digit month */
  value = date->month;
  d = RASQAL_GOOD_CAST(unsigned int, (value / 10));
  *p++ = RASQAL_GOOD_CAST(char, d + '0');
  value -= RASQAL_GOOD_CAST(int, d * 10);
  *p++ = RASQAL_GOOD_CAST(char, value + '0');

  *p++ = '-';

  /* value is 2-digit day */
  value = date->day;
  d = RASQAL_GOOD_CAST(unsigned int, (value / 10));
  *p++ = RASQAL_GOOD_CAST(char, d + '0');
  value -= RASQAL_GOOD_CAST(int, d * 10);
  *p++ = RASQAL_GOOD_CAST(char, value + '0');

  if(tz_string_len) {
    memcpy(p, timezone_string, RASQAL_GOOD_CAST(size_t, tz_string_len));
    p += tz_string_len;
  }

  *p = '\0';

  return buffer;
}


/**
 * rasqal_xsd_date_to_string:
 * @d: date struct
 *
 * Convert a #rasqal_xsd_date struct to a xsd:date lexical form string.
 *
 * Caller should rasqal_free_memory() the returned string.
 *
 * Return value: lexical form string or NULL on failure.
 */
char*
rasqal_xsd_date_to_string(const rasqal_xsd_date *d)
{
  return rasqal_xsd_date_to_counted_string(d, NULL);
}


/**
 * days_per_month:
 * @month: month 1-12
 * @year: gregorian year
 *
 * INTERNAL - returns the number of days in given month and year.
 *
 * Return value: number of days or 0 on invalid arguments
 */
static unsigned int
days_per_month(int month, int year) {
  switch(month) {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
      return 31;
  
    case 4:
    case 6:
    case 9:
    case 11:
      return 30;
  
    case 2:
      /* any of bottom 2 bits non-zero -> not 0 mod 4 -> not leap year */
      if(year & 3)
        return 28;

      /* 0 mod 400 and 0 mod 4 -> leap year */
      if(!(year % 400))
        return 29;

      /* 0 mod 100 and not 0 mod 400 and 0 mod 4 -> not leap year */
      if(!(year % 100))
        return 28;

      /* other 0 mod 4 years -> leap year */
      return 29;

    default:
       /* error */
      return 0;
  }
}


int
rasqal_xsd_datetime_check(const char* string)
{
  rasqal_xsd_datetime d;
  
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#dateTime
   */
  return !rasqal_xsd_datetime_parse(string, &d, 1);
}


int
rasqal_xsd_date_check(const char* string)
{
  rasqal_xsd_date d;
  
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#date
   */
  return !rasqal_xsd_date_parse(string, &d);
}


#define TM_YEAR_ORIGIN 1900
#define TM_MONTH_ORIGIN 1

/**
 * rasqal_xsd_datetime_set_from_timeval:
 * @dt: datetime
 * @tv: timeval
 * 
 * Set an XSD dateTime from a struct timeval pointer
 * 
 * Returns: non-0 on failure
 **/
int
rasqal_xsd_datetime_set_from_timeval(rasqal_xsd_datetime *dt,
                                     struct timeval *tv)
{
  struct tm* my_time;
#ifdef HAVE_GMTIME_R
  struct tm time_buf;
#endif
  time_t sec;

  if(!dt || !tv)
    return 1;

  sec = (time_t)tv->tv_sec;
#ifdef HAVE_GMTIME_R
  memset(&time_buf, '\0', sizeof(time_buf));
  my_time = gmtime_r(&sec, &time_buf);
#else
  my_time = gmtime(&sec);
#endif
  if(!my_time)
    return 1;

  dt->year = my_time->tm_year + TM_YEAR_ORIGIN;
  dt->month = RASQAL_GOOD_CAST(unsigned char, my_time->tm_mon + TM_MONTH_ORIGIN);
  dt->day = RASQAL_GOOD_CAST(unsigned char, my_time->tm_mday);
  dt->hour = RASQAL_GOOD_CAST(signed char, my_time->tm_hour);
  dt->minute = RASQAL_GOOD_CAST(signed char, my_time->tm_min);
  dt->second = RASQAL_GOOD_CAST(signed char, my_time->tm_sec);
  dt->microseconds = RASQAL_GOOD_CAST(int, tv->tv_usec);
  dt->timezone_minutes = 0; /* always Zulu time */
  dt->have_tz = 'Z';
  
  return 0;
}


/**
 * rasqal_xsd_datetime_set_from_unixtime:
 * @dt: date time
 * @clock: unix time in seconds
 * 
 * Set an XSD dateTime from unixtime seconds
 * 
 * Returns: non-0 on failure
 **/
int
rasqal_xsd_datetime_set_from_unixtime(rasqal_xsd_datetime* dt,
                                      time_t secs)
{
  struct timeval tv;

  if(!dt)
    return 1;
  
  tv.tv_sec = secs;
  tv.tv_usec = 0;

  return rasqal_xsd_datetime_set_from_timeval(dt, &tv);
}


/**
 * rasqal_xsd_datetime_get_as_unixtime:
 * @dt: datetime
 * 
 * Get a datetime as unix seconds
 *
 * Returns: unix seconds or 0 if @dt is NULL
 **/
time_t
rasqal_xsd_datetime_get_as_unixtime(rasqal_xsd_datetime* dt)
{
  struct tm time_buf;

  if(!dt)
    return 0;
  
  memset(&time_buf, '\0', sizeof(time_buf));

  time_buf.tm_year = dt->year - TM_YEAR_ORIGIN;
  time_buf.tm_mon  = dt->month - TM_MONTH_ORIGIN;
  time_buf.tm_mday = dt->day;
  time_buf.tm_hour = dt->hour;
  time_buf.tm_min  = dt->minute;
  time_buf.tm_sec  = dt->second;
  time_buf.tm_wday = 0;
  time_buf.tm_yday = 0;
  time_buf.tm_isdst = -1;

#ifdef HAVE_TM_GMTOFF
  if(dt->timezone_minutes == RASQAL_XSD_DATETIME_NO_TZ)
    time_buf.tm_gmtoff = 0;
  else
    time_buf.tm_gmtoff = dt->timezone_minutes * 60;
#endif

  return rasqal_timegm(&time_buf);
}


/**
 * rasqal_xsd_datetime_get_as_timeval:
 * @dt: datetime
 * 
 * Get a datetime as struct timeval
 *
 * The returned timeval must be freed by the caller such as using
 * rasqal_free_memory().
 *
 * Returns: pointer to a new timeval structure or NULL on failure
 **/
struct timeval*
rasqal_xsd_datetime_get_as_timeval(rasqal_xsd_datetime *dt)
{
  struct timeval *tv;
  
  if(!dt)
    return NULL;
  
  tv = RASQAL_CALLOC(struct timeval*, 1, sizeof(*tv));
  if(!tv)
    return NULL;
  
  tv->tv_sec = rasqal_xsd_datetime_get_as_unixtime(dt);
  tv->tv_usec = dt->microseconds;
  
  return tv;
}


/**
 * rasqal_xsd_datetime_get_timezone_as_counted_string:
 * @dt: datetime
 * @len_p: pointer to store returned string length
 * 
 * Get the timezone of a datetime as a duration format string with optional length count
 *
 * The returned string is owned by the caller and must be freed
 * by rasqal_free_memory().
 *
 * Returns: pointer to a new string or NULL on failure
 **/
char*
rasqal_xsd_datetime_get_timezone_as_counted_string(rasqal_xsd_datetime *dt,
                                                   size_t *len_p)
{
  /* timezone duration as implemented here is a signed integer number
   * of seconds like +- 14 hours:minutes (no days or larger units, no
   * seconds or smaller).
   *
   * When written in the canonical format, a restricted
   * xsd:dayTimeDuration format, it is constraint to a format like -?PThhmm
   *
   * For example: -PT14H59M PT14H59M and PT0S for a zero timezone offset
   */
#define TZ_STR_SIZE 10
  char* tz_str;
  char* p;
  int minutes;
  unsigned int hours;
  
  if(!dt)
    return NULL;

  minutes = dt->timezone_minutes;
  if(minutes == RASQAL_XSD_DATETIME_NO_TZ)
    return NULL;
  
  tz_str = RASQAL_MALLOC(char*, TZ_STR_SIZE + 1);
  if(!tz_str)
    return NULL;
  
  p = tz_str;

  if(minutes < 0) {
    *p++ = '-';
    minutes = -minutes;
  }

  *p++ = 'P';
  *p++ = 'T';
    
  hours = RASQAL_GOOD_CAST(unsigned int, (minutes / 60));
  if(hours) {
#if 1
    if(hours > 9) {
      *p++ = RASQAL_GOOD_CAST(char, '0' + (hours / 10));
      hours %= 10;
    }
    *p++ = RASQAL_GOOD_CAST(char, '0' + hours);
    *p++ = 'H';
#else
    p += sprintf(p, "%dH", hours);
#endif
    minutes -= RASQAL_GOOD_CAST(int, hours * 60);
  }
  
  if(minutes) {
#if 1
    if(minutes > 9) {
      *p++ = RASQAL_GOOD_CAST(char, '0' + (minutes / 10));
      minutes %= 10;
    }
    *p++ = RASQAL_GOOD_CAST(char, '0' + minutes);
    *p++ = 'M';
#else
    p += sprintf(p, "%dM", minutes);
#endif
  }

  if(!dt->timezone_minutes) {
    *p++ = '0';
    *p++ = 'S';
  }
  
  *p = '\0';

  if(len_p)
    *len_p = RASQAL_GOOD_CAST(size_t, p - tz_str);
  
  return tz_str;
}


/**
 * rasqal_xsd_datetime_get_tz_as_counted_string:
 * @dt: datetime
 * @len_p: pointer to store returned string length
 * 
 * Get the timezone of a datetime as a timezone string
 *
 * The returned string is owned by the caller and must be freed
 * by rasqal_free_memory().
 *
 * Returns: pointer to a new string or NULL on failure
 **/
char*
rasqal_xsd_datetime_get_tz_as_counted_string(rasqal_xsd_datetime* dt,
                                             size_t *len_p)
{
  char* s;
  
  s = RASQAL_MALLOC(char*, TIMEZONE_BUFFER_LEN + 1);
  if(!s)
    return NULL;

  if(rasqal_xsd_timezone_format(dt->timezone_minutes, dt->have_tz,
                                s, TIMEZONE_BUFFER_LEN + 1) < 0)
    goto failed;

  if(len_p)
    *len_p = TIMEZONE_BUFFER_LEN;
  
  return s;

  failed:
  RASQAL_FREE(char*, s);
  return NULL;
}


/**
 * rasqal_new_xsd_date:
 * @world: world object
 * @date_string: XSD date string
 *
 * Constructor - make a new XSD date object from a string
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_date*
rasqal_new_xsd_date(rasqal_world* world, const char *date_string)
{
  rasqal_xsd_datetime dt_result; /* on stack */
  rasqal_xsd_date* d;
  int rc = 0;
  
  d = RASQAL_CALLOC(rasqal_xsd_date*, 1, sizeof(*d));
  if(!d)
    return NULL;
  
  rc = rasqal_xsd_datetime_parse(date_string, &dt_result, 0);
  if(!rc) {
    d->year = dt_result.year;
    d->month = dt_result.month;
    d->day = dt_result.day;
    d->timezone_minutes = dt_result.timezone_minutes;
    d->have_tz = dt_result.have_tz;

    dt_result.hour   = 12; /* Noon */
    dt_result.minute =  0;
    dt_result.second =  0;
    dt_result.microseconds = 0;

    rc = rasqal_xsd_datetime_normalize(&dt_result);

    /* Track the starting instant as determined by the timezone */
    d->time_on_timeline = dt_result.time_on_timeline;
    if(d->timezone_minutes != RASQAL_XSD_DATETIME_NO_TZ)
      d->time_on_timeline += (60 * dt_result.timezone_minutes);
  }

  if(rc) {
    rasqal_free_xsd_date(d); d = NULL;
  }

  return d;
}


/**
 * rasqal_free_xsd_date:
 * @d: date object
 * 
 * Destroy XSD date object.
 **/
void
rasqal_free_xsd_date(rasqal_xsd_date* d)
{
  if(!d)
    return;
  
  RASQAL_FREE(rasqal_xsd_date, d);
}


/**
 * rasqal_xsd_date_equals:
 * @d1: first XSD date
 * @d2: second XSD date
 * @incomparible_p: address to store incomparable flag (or NULL)
 * 
 * Compare two XSD dates for equality.
 * 
 * Return value: non-0 if equal.
 **/
int
rasqal_xsd_date_equals(const rasqal_xsd_date *d1,
                       const rasqal_xsd_date *d2,
                       int *incomparible_p)
{
  int cmp = rasqal_xsd_date_compare(d1, d2, incomparible_p);
  return !cmp;
}


/**
 * rasqal_xsd_date_compare:
 * @d1: first XSD date
 * @d2: second XSD date
 * @incomparible_p: address to store incomparable flag (or NULL)
 * 
 * Compare two XSD dates
 * 
 * If the only one of the two dates have timezones, the results
 * may be incomparible and that will return >0 and set the
 * value of the int point to by @incomparible_p to non-0
 * 
 * Return value: <0 if @d1 is less than @d2, 0 if equal, >0 otherwise
 **/
int
rasqal_xsd_date_compare(const rasqal_xsd_date *d1,
                        const rasqal_xsd_date *d2,
                        int *incomparible_p)
{
  if(incomparible_p)
    *incomparible_p = 0;

  /* Handle NULLs */
  if(!d1 || !d2) {
    /* NULLs sort earlier. equal only if both are NULL */
    if(!d1 && !d2)
      return 0;

    return (!d1) ? -1 : 1;
  }

  return rasqal_xsd_timeline_compare(d1->time_on_timeline, 0 /* msec */,
                                     d1->timezone_minutes,
                                     d2->time_on_timeline, 0 /* msec */,
                                     d2->timezone_minutes,
                                     incomparible_p);
}


#ifdef STANDALONE
#include <stdio.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

int main(int argc, char *argv[]);

#define MYASSERT(c) \
  if(!(c)) { \
    fprintf(stderr, "%s: assertion failed at %s:%d: %s\n", program, __FILE__, __LINE__, #c); \
    exit(1); \
  }

static int
test_datetime_parse_and_normalize(const char *datetime_string,
                                  rasqal_xsd_datetime *result)
{
  if(rasqal_xsd_datetime_parse(datetime_string, result, 1))
    return 1;
  
  return rasqal_xsd_datetime_normalize(result);
}

static int
test_datetime_parser_tostring(const char *in_str, const char *out_expected)
{
  rasqal_xsd_datetime d; /* allocated on stack */
  char const *s = NULL;
  int r = 1;

  if(!test_datetime_parse_and_normalize(in_str, &d)) {
    s = rasqal_xsd_datetime_to_string(&d);
  }
  
  if(s) {
    r = strcmp(RASQAL_GOOD_CAST(char*, s), out_expected);
    if(r)
      fprintf(stderr, "input dateTime \"%s\" converted to canonical \"%s\", expected \"%s\"\n", in_str, s, out_expected);
    RASQAL_FREE(char*, s);
  } else
    fprintf(stderr, "input dateTime \"%s\" converted to canonical (null), expected \"%s\"\n", in_str, out_expected);
  return r;
}


static int
test_date_parse_and_normalize(const char *date_string,
                              rasqal_xsd_date *result)
{
  if(rasqal_xsd_date_parse(date_string, result))
    return 1;
  
  return rasqal_xsd_date_normalize(result);
}


static int
test_date_parser_tostring(const char *in_str, const char *out_expected)
{
  rasqal_xsd_date d; /* allocated on stack */
  char const *s = NULL;
  int r = 1;

  if(!test_date_parse_and_normalize(in_str, &d)) {
    s = rasqal_xsd_date_to_string(&d);
  }
  
  if(s) {
    r = strcmp(RASQAL_GOOD_CAST(char*, s), out_expected);
    if(r)
      fprintf(stderr, "input date \"%s\" converted to canonical \"%s\", expected \"%s\"\n", in_str, s, out_expected);
    RASQAL_FREE(char*, s);
  } else
    fprintf(stderr, "input date \"%s\" converted to canonical (null), expected \"%s\"\n", in_str, out_expected);
  return r;
}


#define INCOMPARABLE 2

static int
test_date_equals(rasqal_world* world, const char *value1, const char *value2,
                 int expected_eq)
{
  rasqal_xsd_date* d1;
  rasqal_xsd_date* d2;
  int r = 1;
  int incomparable = 0;
  int eq;
  
  d1 = rasqal_new_xsd_date(world, value1);
  d2 = rasqal_new_xsd_date(world, value2);

  eq = rasqal_xsd_date_equals(d1, d2, &incomparable);
  if(incomparable)
    eq = INCOMPARABLE;
  rasqal_free_xsd_date(d1);
  rasqal_free_xsd_date(d2);

  if(eq != expected_eq) {
    fprintf(stderr, "date equals \"%s\" to \"%s\" returned %d expected %d\n",
            value1, value2, eq, expected_eq);
    r = 1;
  }
  return r;
}


static int
test_date_not_equals(rasqal_world* world,
                     const char *value1, const char *value2,
                     int expected_neq)
{
  rasqal_xsd_date* d1;
  rasqal_xsd_date* d2;
  int r = 1;
  int incomparable = 0;
  int neq;
  
  d1 = rasqal_new_xsd_date(world, value1);
  d2 = rasqal_new_xsd_date(world, value2);

  neq = !rasqal_xsd_date_equals(d1, d2, &incomparable);
  if(incomparable)
    neq = INCOMPARABLE;
  rasqal_free_xsd_date(d1);
  rasqal_free_xsd_date(d2);

  if(neq != expected_neq) {
    fprintf(stderr,
            "date not equals \"%s\" to \"%s\" returned %d expected %d\n",
            value1, value2, neq, expected_neq);
    r = 1;
  }
  return r;
}


static int
test_date_compare(rasqal_world* world, const char *value1, const char *value2,
                  int expected_cmp)
{
  rasqal_xsd_date* d1;
  rasqal_xsd_date* d2;
  int r = 1;
  int incomparable = 0;
  int cmp;
  
  d1 = rasqal_new_xsd_date(world, value1);
  d2 = rasqal_new_xsd_date(world, value2);

  cmp = rasqal_xsd_date_compare(d1, d2, &incomparable);
  if(incomparable)
    cmp = INCOMPARABLE;
  else if (cmp < 0)
    cmp = -1;
  else if (cmp > 0)
    cmp = 1;
  
  rasqal_free_xsd_date(d1);
  rasqal_free_xsd_date(d2);

  if(cmp != expected_cmp) {
    fprintf(stderr, "date compare \"%s\" to \"%s\" returned %d expected %d\n",
            value1, value2, cmp, expected_cmp);
    r = 1;
  }
  return r;
}


static int
test_datetime_equals(rasqal_world* world, const char *value1, const char *value2,
                 int expected_eq)
{
  rasqal_xsd_datetime* d1;
  rasqal_xsd_datetime* d2;
  int r = 1;
  int incomparable = 0;
  int eq;
  
  d1 = rasqal_new_xsd_datetime(world, value1);
  d2 = rasqal_new_xsd_datetime(world, value2);

  eq = rasqal_xsd_datetime_equals2(d1, d2, &incomparable);
  if(incomparable)
    eq = INCOMPARABLE;
  rasqal_free_xsd_datetime(d1);
  rasqal_free_xsd_datetime(d2);

  if(eq != expected_eq) {
    fprintf(stderr,
            "datetime equals \"%s\" to \"%s\" returned %d expected %d\n",
            value1, value2, eq, expected_eq);
    r = 1;
  }
  return r;
}


static int
test_datetime_compare(rasqal_world* world, const char *value1, const char *value2,
                  int expected_cmp)
{
  rasqal_xsd_datetime* d1;
  rasqal_xsd_datetime* d2;
  int r = 1;
  int incomparable = 0;
  int cmp;
  
  d1 = rasqal_new_xsd_datetime(world, value1);
  d2 = rasqal_new_xsd_datetime(world, value2);

  cmp = rasqal_xsd_datetime_compare2(d1, d2, &incomparable);
  if(incomparable)
    cmp = INCOMPARABLE;
  else if (cmp < 0)
    cmp = -1;
  else if (cmp > 0)
    cmp = 1;
  
  rasqal_free_xsd_datetime(d1);
  rasqal_free_xsd_datetime(d2);

  if(cmp != expected_cmp) {
    fprintf(stderr,
            "datetime compare \"%s\" to \"%s\" returned %d expected %d\n",
            value1, value2, cmp, expected_cmp);
    r = 1;
  }
  return r;
}


int
main(int argc, char *argv[])
{
  char const *program = rasqal_basename(*argv);
  rasqal_world* world;
  rasqal_xsd_datetime dt;
  rasqal_xsd_date d;

  world = rasqal_new_world();
  
  /* days_per_month */
  
  MYASSERT(!days_per_month(0,287));
  
  MYASSERT(days_per_month(1,467) == 31);

  MYASSERT(days_per_month(2,1900) == 28);  
  MYASSERT(days_per_month(2,1901) == 28);
  MYASSERT(days_per_month(2,2000) == 29);
  MYASSERT(days_per_month(2,2004) == 29);
  
  MYASSERT(days_per_month(3,1955) == 31);
  MYASSERT(days_per_month(4,3612) == 30);
  MYASSERT(days_per_month(5,467) == 31);
  MYASSERT(days_per_month(6,398) == 30);
  MYASSERT(days_per_month(7,1832) == 31);
  MYASSERT(days_per_month(8,8579248) == 31);
  MYASSERT(days_per_month(9,843) == 30);
  MYASSERT(days_per_month(10,84409) == 31);
  MYASSERT(days_per_month(11,398) == 30);
  MYASSERT(days_per_month(12,4853) == 31);
  MYASSERT(!days_per_month(13,45894));
  
  /* DATETIME */

  /* rasqal_xsd_datetime_parse_and_normalize,
     rasqal_xsd_datetime_to_string and
     rasqal_xsd_datetime_string_to_canonical */
  
  #define PARSE_AND_NORMALIZE_DATETIME(_s,_d) \
    test_datetime_parse_and_normalize(_s, _d)
  
  /* generic */

  MYASSERT(!rasqal_xsd_datetime_to_string(0));

  MYASSERT(PARSE_AND_NORMALIZE_DATETIME(0,0));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("uhgsufi",0));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME(0 ,&dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("fsdhufhdsuifhidu", &dt));
  
  /* year */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("123-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("-123-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("0000-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("01234-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("-01234-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("1234a12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("-1234b12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("g162-12-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("5476574658746587465874-12-12T12:12:12Z", &dt));
  
  MYASSERT(test_datetime_parser_tostring("1234-12-12T12:12:12Z", "1234-12-12T12:12:12Z") == 0);
  MYASSERT(test_datetime_parser_tostring("-1234-12-12T12:12:12Z", "-1234-12-12T12:12:12Z") == 0);
  MYASSERT(test_datetime_parser_tostring("1234567890-12-12T12:12:12Z", "1234567890-12-12T12:12:12Z") == 0);
  MYASSERT(test_datetime_parser_tostring("-1234567890-12-12T12:12:12Z", "-1234567890-12-12T12:12:12Z") == 0);
  
  /* month */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-v-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-00-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("PARSE_AND_NORMALIZE-011-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-13-12T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-12.12T12:12:12Z", &dt));

  MYASSERT(test_datetime_parser_tostring("2004-01-01T12:12:12Z", "2004-01-01T12:12:12Z") == 0);

  /* day */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-ffT12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-00T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-007T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-32T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01t12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01- 1T12:12:12Z", &dt));
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2005-02-29T12:12:12Z", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2005-02-28T12:12:12Z", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-02-29T12:12:12Z", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2000-02-29T12:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("1900-02-29T12:12:12Z", &dt));

  MYASSERT(test_datetime_parser_tostring("2012-04-12T12:12:12Z", "2012-04-12T12:12:12Z") == 0);
  
  /* hour */

  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01Tew:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T-1:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T001:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T25:12:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T01.12:12Z", &dt));
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T24:12:00Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T24:00:34Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T24:12:34Z", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T24:00:00Z", &dt));
  
  MYASSERT(test_datetime_parser_tostring("2012-04-12T24:00:00", "2012-04-13T00:00:00") == 0);
  
  /* minute */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:ij:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:-1:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:042:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:69:12Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12.12Z", &dt));
  
  /* second */

  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:ijZ", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:-1", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:054Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:69Z", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12z", &dt));
  
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12", &dt));
  
  /* fraction second */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.i", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.0", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.01", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.1", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.100", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.1000000000000000000000000000000000000000000", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.5798459847598743987549", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.1d", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12.1Z", &dt));

  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.01Z", "2006-05-18T18:36:03.01Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.10Z", "2006-05-18T18:36:03.1Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.010Z", "2006-05-18T18:36:03.01Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.1234Z", "2006-05-18T18:36:03.1234Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.1234", "2006-05-18T18:36:03.1234") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.1239Z", "2006-05-18T18:36:03.1239Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2006-05-18T18:36:03.1239", "2006-05-18T18:36:03.1239") == 0);

  /* timezones + normalization */

  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12-", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+00.00", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+aa:bb", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+15:00", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+14:01", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+14:00", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12-14:01", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12-14:00", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+10:99", &dt));
  MYASSERT(!PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+10:59", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+10:059", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+010:59", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+10:59a", &dt));
  MYASSERT(PARSE_AND_NORMALIZE_DATETIME("2004-01-01T12:12:12+10:059", &dt));

  MYASSERT(test_datetime_parser_tostring("2004-12-31T23:50:22-01:15", "2005-01-01T01:05:22Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2005-01-01T01:00:05+02:12", "2004-12-31T22:48:05Z") == 0);
  MYASSERT(test_datetime_parser_tostring("0001-01-01T00:00:00+00:01", "-0001-12-31T23:59:00Z") == 0);
  MYASSERT(test_datetime_parser_tostring("-0001-12-31T23:59:00-00:01", "0001-01-01T00:00:00Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2005-03-01T00:00:00+01:00", "2005-02-28T23:00:00Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2004-03-01T00:00:00+01:00", "2004-02-29T23:00:00Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2005-02-28T23:00:00-01:00", "2005-03-01T00:00:00Z") == 0);
  MYASSERT(test_datetime_parser_tostring("2004-02-29T23:00:00-01:00", "2004-03-01T00:00:00Z") == 0);


  /* DATE */

  #define PARSE_AND_NORMALIZE_DATE(_s,_d) \
    test_date_parse_and_normalize(_s, _d)
  
  /* generic */

  MYASSERT(!rasqal_xsd_date_to_string(0));

  MYASSERT(PARSE_AND_NORMALIZE_DATE(0,0));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("uhgsufi",0));
  MYASSERT(PARSE_AND_NORMALIZE_DATE(0 ,&d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("fsdhufhdsuifhidu", &d));
  
  /* year */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATE("123-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("-123-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("0000-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("01234-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("-01234-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("1234a12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("-1234b12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("g162-12-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("5476574658746587465874-12-12Z", &d));
  
  MYASSERT(test_date_parser_tostring("1234-12-12Z", "1234-12-12Z") == 0);
  MYASSERT(test_date_parser_tostring("-1234-12-12Z", "-1234-12-12Z") == 0);
  MYASSERT(test_date_parser_tostring("1234567890-12-12Z", "1234567890-12-12Z") == 0);
  MYASSERT(test_date_parser_tostring("-1234567890-12-12Z", "-1234567890-12-12Z") == 0);
  
  /* month */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-v-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-00-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("PARSE_AND_NORMALIZE-011-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-13-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-12.12Z", &d));

  MYASSERT(test_date_parser_tostring("2004-01-01Z", "2004-01-01Z") == 0);

  /* day */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-ffZ", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-00Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-007Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-32Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01t12:12:12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01- 1Z", &d));
  
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2005-02-29Z", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2005-02-28Z", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2004-02-29Z", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2000-02-29Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("1900-02-29Z", &d));

  MYASSERT(test_date_parser_tostring("2012-04-12Z", "2012-04-12Z") == 0);
  
  /* timezones + normalization */

  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01-", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+00.00", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+aa:bb", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+15:00", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+14:01", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2004-01-01+14:00", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01-14:01", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2004-01-01-14:00", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+10:99", &d));
  MYASSERT(!PARSE_AND_NORMALIZE_DATE("2004-01-01+10:59", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+10:059", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+010:59", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+10:59a", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-01-01+10:059", &d));

  MYASSERT(test_date_parser_tostring("2004-12-31-13:00", "2005-01-01Z") == 0);
  MYASSERT(test_date_parser_tostring("2005-01-01+13:00", "2004-12-31Z") == 0);
  MYASSERT(test_date_parser_tostring("2004-12-31-11:59", "2004-12-31Z") == 0);
  MYASSERT(test_date_parser_tostring("2005-01-01+11:59", "2005-01-01Z") == 0);

  /* Date equality */
  /* May not be comparible since <14hrs apart */
  MYASSERT(test_date_equals(world, "2011-01-02Z", "2011-01-02" , INCOMPARABLE));
  MYASSERT(test_date_equals(world, "2011-01-02" , "2011-01-02" , 1));
  MYASSERT(test_date_equals(world, "2011-01-02",  "2011-01-02Z", INCOMPARABLE));
  MYASSERT(test_date_equals(world, "2011-01-02Z", "2011-01-02Z", 1));

  /* Are comparible across timelines since >14hrs apart */
  MYASSERT(test_date_equals(world, "2011-01-02Z", "2011-01-03" , 0));
  MYASSERT(test_date_equals(world, "2011-01-02" , "2011-01-03" , 0));
  MYASSERT(test_date_equals(world, "2011-01-02",  "2011-01-03Z", 0));
  MYASSERT(test_date_equals(world, "2011-01-02Z", "2011-01-03Z", 0));

  MYASSERT(test_date_not_equals(world, "2006-08-23", "2006-08-23", 0));
  MYASSERT(test_date_not_equals(world, "2006-08-23", "2006-08-23Z", INCOMPARABLE));
  MYASSERT(test_date_not_equals(world, "2006-08-23", "2006-08-23+00:00", INCOMPARABLE));
  /* More than 14hrs apart so are comparible */
  MYASSERT(test_date_not_equals(world, "2006-08-23", "2001-01-01", 1));
  MYASSERT(test_date_not_equals(world, "2006-08-23", "2001-01-01Z", 1));

  /* Date comparisons */
  MYASSERT(test_date_compare(world, "2011-01-02Z", "2011-01-02" , INCOMPARABLE));
  MYASSERT(test_date_compare(world, "2011-01-02",  "2011-01-02" , 0));
  MYASSERT(test_date_compare(world, "2011-01-02",  "2011-01-02Z", INCOMPARABLE));
  MYASSERT(test_date_compare(world, "2011-01-02Z", "2011-01-02Z", 0));

  MYASSERT(test_date_compare(world, "2011-01-02Z", "2011-01-03" , -1));
  MYASSERT(test_date_compare(world, "2011-01-02",  "2011-01-03" , -1));
  MYASSERT(test_date_compare(world, "2011-01-02",  "2011-01-03Z", -1));
  MYASSERT(test_date_compare(world, "2011-01-02Z", "2011-01-03Z", -1));

  /* DateTime equality */
  MYASSERT(test_datetime_equals(world, "2011-01-02T00:00:00",  "2011-01-02T00:00:00",  1));
  MYASSERT(test_datetime_equals(world, "2011-01-02T00:00:00",  "2011-01-02T00:00:00Z", INCOMPARABLE));
  MYASSERT(test_datetime_equals(world, "2011-01-02T00:00:00Z", "2011-01-02T00:00:00",  INCOMPARABLE));
  MYASSERT(test_datetime_equals(world, "2011-01-02T00:00:00Z", "2011-01-02T00:00:00Z", 1));

  /* DateTime comparisons */
  MYASSERT(test_datetime_compare(world, "2011-01-02T00:00:00",  "2011-01-02T00:00:00" , 0));
  MYASSERT(test_datetime_compare(world, "2011-01-02T00:00:00",  "2011-01-02T00:00:00Z", INCOMPARABLE));
  MYASSERT(test_datetime_compare(world, "2011-01-02T00:00:00Z", "2011-01-02T00:00:00",  INCOMPARABLE));
  MYASSERT(test_datetime_compare(world, "2011-01-02T00:00:00Z", "2011-01-02T00:00:00Z", 0));


  if(1) {
    struct timeval my_tv;
    time_t secs;
    time_t new_secs;
    struct timeval* new_tv;

    /* 2010-12-14T06:22:36.868099Z or 2010-12-13T22:22:36.868099+0800 
     * when I was testing this
     */
    my_tv.tv_sec = 1292307756;
    my_tv.tv_usec = 868099;

    secs = my_tv.tv_sec;
    
    MYASSERT(rasqal_xsd_datetime_set_from_timeval(&dt, &my_tv) == 0);

    MYASSERT((new_tv = rasqal_xsd_datetime_get_as_timeval(&dt)));
    MYASSERT(new_tv->tv_sec == my_tv.tv_sec);
    MYASSERT(new_tv->tv_usec == my_tv.tv_usec);

    RASQAL_FREE(timeval, new_tv);
    
    MYASSERT(rasqal_xsd_datetime_set_from_unixtime(&dt, secs) == 0);
    
    MYASSERT((new_secs = rasqal_xsd_datetime_get_as_unixtime(&dt)));
    MYASSERT(new_secs == secs);
  }
  
  rasqal_free_world(world);

  return 0;
}

#endif /* STANDALONE */

