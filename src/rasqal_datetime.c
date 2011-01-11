/*
 * rasqal_datetime.c - Rasqal XSD dateTime and XSD date
 *
 * Copyright (C) 2007-2010, David Beckett http://www.dajobe.org/
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
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>
#include <limits.h>

#include "rasqal.h"
#include "rasqal_internal.h"

/* Local definitions */
 
/**
 * rasqal_xsd_date:
 * @year: year
 * @month: month 1-12
 * @day: 0-31
 *
 * INTERNAL - XML schema date datatype
 *
 */
typedef struct {
  signed int year;
  /* the following fields are integer values not characters */
  unsigned char month;
  unsigned char day;
} rasqal_xsd_date;


static int rasqal_xsd_datetime_parse(const char *datetime_string, rasqal_xsd_datetime *result, int is_dateTime);
static unsigned int days_per_month(int month, int year);


#ifndef ISNUM
#define ISNUM(c) ((c) >= '0' && (c) <= '9')
#endif


/**
 * rasqal_xsd_datetime_normalize:
 * @datetime: date time
 *
 * INTERNAl - Normalize a date time into the allowed range
 *
 * Return value: zero on success, non zero on failure.
 */
static int
rasqal_xsd_datetime_normalize(rasqal_xsd_datetime *datetime)
{
  int t;

  if(datetime->timezone_minutes &&
     (datetime->timezone_minutes  != RASQAL_XSD_DATETIME_NO_TZ)) {
    /* Normalize to Zulu if there was a timezone offset */

    datetime->hour   += (datetime->timezone_minutes / 60);
    datetime->minute += (datetime->timezone_minutes % 60);

    datetime->timezone_minutes = 0;
  }
  
  /* second & second parts: no need to normalize as they are not
   * touched after range check
   */
  
  /* minute */
  if(datetime->minute < 0) {
    datetime->minute += 60;
    datetime->hour--;
  } else if(datetime->minute > 59) {
    datetime->minute -= 60;
    datetime->hour++;
  }
  
  /* hour */
  if(datetime->hour < 0) {
    datetime->hour += 24;
    datetime->day--;
  } else if(datetime->hour > 23) {
    datetime->hour -= 24;
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
    datetime->day += days_per_month(t, y2);
  } else if(datetime->day > (t = days_per_month(datetime->month, datetime->year))) {
    datetime->day -= t;
    datetime->month++;
  }
  
  /* month & year */
  if(datetime->month < 1) {
    datetime->month += 12;
    datetime->year--;
    /* there is no year 0 - go backwards to year -1 */
    if(!datetime->year)
      datetime->year--;
  } else if(datetime->month > 12) {
    datetime->month -= 12;
    datetime->year++;
    /* there is no year 0 - go forwards to year 1 */
    if(!datetime->year)
      datetime->year++;
  }

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
  char b[16];
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
  l = p - q;
  
  /* error if
     - less than 4 digits in year
     - more than 4 digits && leading zeros
     - '-' does not follow numbers
   */
  if(l < 4 || (l > 4 && *q=='0') || *p != '-')
    return -1;

  l = (l < sizeof(b)-1 ? l : sizeof(b)-1);
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
    
  result->year = is_neg ? -(int)u : (int)u;

  /* parse month */
  
  for(q = ++p; ISNUM(*p); p++)
    ;
  l = p - q;
  
  /* error if month is not 2 digits or '-' is not the separator */
  if(l != 2 || *p != '-')
    return -2;
  
  t = (*q++ - '0')*10;
  t += *q - '0';
  
  /* month must be 1..12 */
  if(t < 1 || t > 12)
    return -2;
  
  result->month = t;
  
  /* parse day */
  
  for(q = ++p; ISNUM(*p); p++)
    ;
  l = p - q;

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
  
  t = (*q++ - '0') * 10;
  t += *q - '0';

  /* day must be 1..days_per_month */
  if(t < 1 || t > days_per_month(result->month, result->year))
    return -3;

  result->day = t;

  if(is_dateTime) {
    /* parse hour */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = p - q;

    /* error if hour is not 2 digits or ':' is not the separator */
    if(l != 2 || *p != ':')
      return -4;

    t = (*q++-'0')*10;
    t += *q-'0';

    /* hour must be 0..24 - will handle special case 24 later
     * (no need to check for < 0)
     */
    if(t > 24)
      return -4;

    result->hour = t;

    /* parse minute */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = p - q;

    /* error if minute is not 2 digits or ':' is not the separator */
    if(l != 2 || *p != ':')
      return -5;

    t = (*q++ - '0') * 10;
    t += *q - '0';

    /* minute must be 0..59
     * (no need to check for < 0)
     */
    if(t > 59)
      return -5;

    result->minute = t;

    /* parse second whole part */

    for(q = ++p; ISNUM(*p); p++)
      ;
    l = p - q;

    /* error if second is not 2 digits or separator is not 
     * '.' (second fraction)
     * 'Z' (utc)
     * '+' or '-' (timezone offset)
     * nul (end of string - second fraction and timezone are optional)
     */
    if(l != 2 || (*p && *p != '.' && *p != 'Z' && *p != '+' && *p != '-'))
      return -6;

    t = (*q++ - '0')*10;
    t += *q - '0';

    /* second must be 0..59
    * (no need to check for < 0)
    */
    if(t > 59)
      return -6;

    result->second = t;

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
        l = p - q;

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
    /* set rest of dateTime field to center of day interval (noon) */
    result->hour = 12;
    result->minute = 0;
    result->second = 0;
    result->microseconds = 0;
  }

  
  /* parse & adjust timezone offset */
  /* result is normalized later */
  result->timezone_minutes = RASQAL_XSD_DATETIME_NO_TZ;
  if(*p) {
    if(*p == 'Z') {
      /* utc timezone - no need to adjust */
      result->timezone_minutes = 0;
      p++;
    } else if(*p == '+' || *p == '-') {
      result->timezone_minutes = 0;

      /* work out timezone offsets */
      is_neg = *p == '-';
     
      /* timezone hours */
      for(q = ++p; ISNUM(*p); p++)
        ;
      l = p - q;
      if(l != 2 || *p!=':')
        return -9;

      t2 = (*q++ - '0') * 10;
      t2 += *q - '0';
      if(t2 > 14)
        /* tz offset hours are restricted to 0..14
         * (no need to check for < 0)
         */
        return -9;
    
      /* negative tz offset adds to the result */
      result->timezone_minutes += (is_neg ? t2 : -t2) * 60;

      /* timezone minutes */    
      for(q = ++p; ISNUM(*p); p++)
        ;
      l = p - q;
      if(l != 2)
        return -10;

      t =(*q++ - '0') * 10;
      t += *q - '0';
      if(t > 59 || (t2 == 14 && t != 0)) {
        /* tz offset minutes are restricted to 0..59
         * (no need to check for < 0)
         * or 0 if hour offset is exactly +-14 
         */
        return -10;
      }
    
      /* negative tz offset adds to the result */
      result->timezone_minutes += is_neg ? t : -t;
    }
    
    /* failure if extra chars after the timezone part */
    if(*p)
      return -11;

  }

  return 0;
}


static int
rasqal_xsd_datetime_parse_and_normalize(const char *datetime_string,
                                        rasqal_xsd_datetime *result)
{
  if(rasqal_xsd_datetime_parse(datetime_string, result, 1))
    return 1;
  
  return rasqal_xsd_datetime_normalize(result);
}

static int
rasqal_xsd_date_parse_and_normalize(const char *date_string,
                                    rasqal_xsd_date *result)
{
  rasqal_xsd_datetime dt_result; /* on stack */
  int rc;

  rc = rasqal_xsd_datetime_parse(date_string, &dt_result, 0);
  if(!rc)
    rc = rasqal_xsd_datetime_normalize(&dt_result);

  if(!rc) {
    result->year = dt_result.year;
    result->month = dt_result.month;
    result->day = dt_result.day;
  }

  return rc;
}


/**
 * rasqal_new_xsd_datetime:
 * @world: world object
 * @datetime_string: XSD Decimal string
 *
 * Constructor - make a new XSD decimal object from a string
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime(rasqal_world* world, const char *datetime_string)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = (rasqal_xsd_datetime*)RASQAL_MALLOC(datetime, sizeof(*dt));
  if(!dt)
    return NULL;
  
  rc = rasqal_xsd_datetime_parse(datetime_string, dt, 1);
  if(!rc)
    rc = rasqal_xsd_datetime_normalize(dt);

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
 * Constructor - make a new XSD decimal object from unixtime seconds
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime_from_unixtime(rasqal_world* world, time_t secs)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = (rasqal_xsd_datetime*)RASQAL_MALLOC(datetime, sizeof(*dt));
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
 * Constructor - make a new XSD decimal object from a timeval
 *
 * Return value: new datetime or NULL on failure
 */
rasqal_xsd_datetime*
rasqal_new_xsd_datetime_from_timeval(rasqal_world* world, struct timeval *tv)
{
  rasqal_xsd_datetime* dt;
  int rc = 0;
  
  dt = (rasqal_xsd_datetime*)RASQAL_MALLOC(datetime, sizeof(*dt));
  if(!dt)
    return NULL;

  rc = rasqal_xsd_datetime_set_from_timeval(dt, tv);

  if(rc) {
    rasqal_free_xsd_datetime(dt); dt = NULL;
  }

  return dt;
}


void
rasqal_free_xsd_datetime(rasqal_xsd_datetime* dt)
{
  if(!dt)
    return;
  
  RASQAL_FREE(datetime, dt);
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
 * Return value: lexical form string or NULL on failure.
 */
char*
rasqal_xsd_datetime_to_counted_string(const rasqal_xsd_datetime *dt,
                                      size_t *len_p)
{
  char *ret = 0;
  int is_neg;
  int r = 0;
  int i;
  /* "[+-]HH:MM\0" */
#define TIMEZONE_STRING_LEN 7
  char timezone_string[TIMEZONE_STRING_LEN];
  int mins;
  
  if(!dt)
    return NULL;
    
  is_neg = dt->year < 0;

  mins = abs(dt->timezone_minutes);
  if(mins == RASQAL_XSD_DATETIME_NO_TZ)
    timezone_string[0] = '\0';
  else if(!mins)
     memcpy(timezone_string, "Z", 2);
  else {
    int hrs = (mins / 60);
    snprintf(timezone_string, TIMEZONE_STRING_LEN, "%c%02d:%02d", 
             (mins != dt->timezone_minutes ? '-' : '+'),
             hrs, mins);
  }
  

  /* format twice: first with null buffer of zero size to get the
   * required buffer size second time to the allocated buffer
   */
  for(i = 0; i < 2; i++) { 
    if(dt->microseconds) {
      char microsecs[9];
      int j;
      
      snprintf(microsecs, 9, "%.6f", ((double)dt->microseconds) / 1000000);
      for(j = strlen(microsecs) - 1; j > 2 && microsecs[j] == '0'; j--)
        microsecs[j] = '\0';
      
      r = snprintf(ret, r, "%s%04d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d%s%s",
                   is_neg ? "-" : "",
                   is_neg ? -dt->year : dt->year,
                   dt->month,
                   dt->day,
                   dt->hour,
                   dt->minute,
                   dt->second,
                   microsecs+1,
                   timezone_string);
    } else {
      r = snprintf((char*)ret, r, "%s%04d-%2.2d-%2.2dT%2.2d:%2.2d:%2.2d%s",
                   is_neg ? "-" : "",
                   is_neg ? -dt->year : dt->year,
                   dt->month,
                   dt->day,
                   dt->hour,
                   dt->minute,
                   dt->second,
                   timezone_string);
    }
    
    /* error? */
    if(r < 0) {
      if(ret)
        RASQAL_FREE(cstring, ret);
      return NULL;
    }

    /* alloc return buffer on first pass */
    if(!i) {
      if(len_p)
        *len_p = r;

      ret = (char*)RASQAL_MALLOC(cstring, ++r);
      if(!ret)
        return NULL;
    }
  }
  return ret;
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
 * rasqal_xsd_datetime_string_to_canonical:
 * @datetime_string: xsd:dateTime as lexical form string
 *
 * Convert a XML Schema dateTime lexical form string to its canonical form.
 *
 * Caller should RASQAL_FREE() the returned string.
 *
 * Return value: canonical lexical form string or NULL on failure.
 *
 *
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
const char*
rasqal_xsd_datetime_string_to_canonical(const char* datetime_string)
{
  rasqal_xsd_datetime d; /* allocated on stack */

  /* parse_and_normalize makes the rasqal_xsd_datetime canonical... */
  if(rasqal_xsd_datetime_parse_and_normalize(datetime_string, &d))
    return NULL;

  /* ... so return a string representation of it */
  return rasqal_xsd_datetime_to_string(&d);
}



/**
 * rasqal_xsd_datetime_equals:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * 
 * Compare two XSD dateTimes for equality.
 * 
 * Return value: non-0 if equal.
 **/
int
rasqal_xsd_datetime_equals(const rasqal_xsd_datetime *dt1,
                           const rasqal_xsd_datetime *dt2)
{
  /* Handle NULLs */
  if(!dt1 || !dt2) {
    /* equal only if both are NULL */
    return (dt1 && dt2);
  }
  
  return ((dt1->year == dt2->year) &&
          (dt1->month == dt2->month) &&
          (dt1->day == dt2->day) &&
          (dt1->hour == dt2->hour) &&
          (dt1->minute == dt2->minute) &&
          (dt1->second == dt2->second) &&
          (dt1->microseconds == dt2->microseconds));
}


/**
 * rasqal_xsd_datetime_compare:
 * @dt1: first XSD dateTime
 * @dt2: second XSD dateTime
 * 
 * Compare two XSD dateTimes
 * 
 * Return value: <0 if @dt1 is less than @dt2, 0 if equal, >1 otherwise
 **/
int
rasqal_xsd_datetime_compare(const rasqal_xsd_datetime *dt1,
                            const rasqal_xsd_datetime *dt2)
{
  int rc;

  /* Handle NULLs */
  if(!dt1 || !dt2) {
    /* NULLs sort earlier. equal only if both are NULL */
    if(dt1 && dt2)
      return 0;

    return (!dt1) ? -1 : 1;
  }
  
  rc = dt1->year - dt2->year;
  if(rc)
    return rc;

  rc = dt1->month - dt2->month;
  if(rc)
    return rc;

  rc = dt1->day - dt2->day;
  if(rc)
    return rc;

  rc = dt1->hour - dt2->hour;
  if(rc)
    return rc;

  rc = dt1->minute - dt2->minute;
  if(rc)
    return rc;

  rc = dt1->second - dt2->second;
  if(rc)
    return rc;

  rc = dt1->microseconds - dt2->microseconds;

  return rc;
}


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
  size_t len;
  char *str;
  char *p;
  rasqal_xsd_decimal* dec;
  
  str = rasqal_xsd_datetime_to_counted_string(dt, &len);
  if(!str)
    return NULL;
  
  /* Get rid of Z */
  str[len-1] = '\0';
  
  /* 17 bytes into canonical form "YYYY-MM-DDTHH:MM:SS.sss" is start of SS */
  p = str + 17;
  /* if seconds is < 10, skip leading 0 */
  if(*p == '0')
    p++;

  dec = rasqal_new_xsd_decimal(world);
  if(dec)
    rasqal_xsd_decimal_set_string(dec, p);

  RASQAL_FREE(cstring, str);

  return dec;
}


/**
 * rasqal_xsd_date_to_string:
 * @d: date struct
 *
 * INTERNAL - Convert a #rasqal_xsd_date struct to a xsd:date lexical form string.
 *
 * Caller should RASQAL_FREE() the returned string.
 *
 * Return value: lexical form string or NULL on failure.
 */
static char*
rasqal_xsd_date_to_string(const rasqal_xsd_date *d)
{
  char *ret = 0;
  int is_neg;
  int r = 0;
  int i;
   
  if(!d)
    return NULL;
    
  is_neg = d->year < 0;

  /* format twice: first with null buffer of zero size to get the
   * required buffer size second time to the allocated buffer
   */
  for(i = 0; i < 2; i++) {
    r = snprintf((char*)ret, r, "%s%04d-%2.2d-%2.2d",
                 is_neg ? "-" : "",
                 is_neg ? -d->year : d->year,
                 d->month,
                 d->day);

    /* error? */
    if(r < 0) {
      if(ret)
        RASQAL_FREE(cstring, ret);
      return NULL;
    }

    /* alloc return buffer on first pass */
    if(!i) {
      ret = (char*)RASQAL_MALLOC(cstring, ++r);
      if(!ret)
        return NULL;
    }
  }
  return ret;
}


/**
 * rasqal_xsd_date_string_to_canonical:
 * @date_string: xsd:date as lexical form string
 *
 * Convert a XML Schema date lexical form string to its canonical form.
 *
 * Caller should RASQAL_FREE() the returned string.
 *
 * Return value: canonical lexical form string or NULL on failure.
 *
 *
 * http://www.w3.org/TR/xmlschema-2/#date-canonical-representation
 * 
 * "the date portion of the canonical representation (the entire
 * representation for nontimezoned values, and all but the timezone
 * representation for timezoned values) is always the date portion of
 * the dateTime canonical representation of the interval midpoint
 * (the dateTime representation, truncated on the right to eliminate
 * 'T' and all following characters). For timezoned values, append
 * the canonical representation of the ·recoverable timezone·. "
 */
const char*
rasqal_xsd_date_string_to_canonical(const char* date_string)
{
  rasqal_xsd_date d; /* allocated on stack */

  /* parse_and_normalize makes the rasqal_xsd_date canonical... */
  if(rasqal_xsd_date_parse_and_normalize(date_string, &d))
    return NULL;

  /* ... so return a string representation of it */
  return rasqal_xsd_date_to_string(&d);
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
  return !rasqal_xsd_datetime_parse_and_normalize(string, &d);
}


int
rasqal_xsd_date_check(const char* string)
{
  rasqal_xsd_date d;
  
  /* This should be correct according to 
   * http://www.w3.org/TR/xmlschema-2/#date
   */
  return !rasqal_xsd_date_parse_and_normalize(string, &d);
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
  dt->month = my_time->tm_mon + TM_MONTH_ORIGIN;
  dt->day = my_time->tm_mday;
  dt->hour = my_time->tm_hour;
  dt->minute = my_time->tm_min;
  dt->second = my_time->tm_sec;
  dt->microseconds = tv->tv_usec;
  dt->timezone_minutes = 0; /* always Zulu time */

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

  return timegm(&time_buf);
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
  
  tv = RASQAL_CALLOC(timeval, 1, sizeof(*tv));
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
 * Returns: pointer to a new strng or NULL on failure
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
  
  tz_str = RASQAL_MALLOC(cstring, TZ_STR_SIZE + 1);
  if(!tz_str)
    return NULL;
  
  p = tz_str;

  minutes = dt->timezone_minutes;
  if(minutes < 0) {
    *p++ = '-';
    minutes = -minutes;
  }

  *p++ = 'P';
  *p++ = 'T';
    
  hours = (minutes / 60);
  if(hours) {
#if 1
    if(hours > 9) {
      *p++ = '0' + (hours / 10);
      hours %= 10;
    }
    *p++ = '0' + hours;
    *p++ = 'H';
#else
    p += sprintf(p, "%dH", hours);
#endif
    minutes -= hours * 60;
  }
  
  if(minutes) {
#if 1
    if(minutes > 9) {
      *p++ = '0' + (minutes / 10);
      minutes %= 10;
    }
    *p++ = '0' + minutes;
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
    *len_p = p - tz_str;
  
  return tz_str;
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


static int test_datetime_parser_tostring(const char *in_str, const char *out_expected)
{
  char const *s;
  int r = 1;
  s = rasqal_xsd_datetime_string_to_canonical((const char*)in_str);
  if(s) {
    r = strcmp((char*)s, out_expected);
    if(r)
      fprintf(stderr, "input dateTime \"%s\" converted to canonical \"%s\", expected \"%s\"\n", in_str, s, out_expected);
    RASQAL_FREE(cstring, (void*)s);
  } else
    fprintf(stderr, "input dateTime \"%s\" converted to canonical (null), expected \"%s\"\n", in_str, out_expected);
  return r;
}


static int test_date_parser_tostring(const char *in_str, const char *out_expected)
{
  char const *s;
  int r = 1;
  s = rasqal_xsd_date_string_to_canonical((const char*)in_str);
  if(s) {
    r = strcmp((char*)s, out_expected);
    if(r)
      fprintf(stderr, "input date \"%s\" converted to canonical \"%s\", expected \"%s\"\n", in_str, s, out_expected);
    RASQAL_FREE(cstring, (void*)s);
  } else
    fprintf(stderr, "input date \"%s\" converted to canonical (null), expected \"%s\"\n", in_str, out_expected);
  return r;
}


int
main(int argc, char *argv[])
{
  char const *program = rasqal_basename(*argv);
  rasqal_xsd_datetime dt;
  rasqal_xsd_date d;

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
    rasqal_xsd_datetime_parse_and_normalize((const char*)_s, _d)
  
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
    rasqal_xsd_date_parse_and_normalize((const char*)_s, _d)
  
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
  
  MYASSERT(test_date_parser_tostring("1234-12-12Z", "1234-12-12") == 0);
  MYASSERT(test_date_parser_tostring("-1234-12-12Z", "-1234-12-12") == 0);
  MYASSERT(test_date_parser_tostring("1234567890-12-12Z", "1234567890-12-12") == 0);
  MYASSERT(test_date_parser_tostring("-1234567890-12-12Z", "-1234567890-12-12") == 0);
  
  /* month */
  
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-v-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-00-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("PARSE_AND_NORMALIZE-011-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-13-12Z", &d));
  MYASSERT(PARSE_AND_NORMALIZE_DATE("2004-12.12Z", &d));

  MYASSERT(test_date_parser_tostring("2004-01-01Z", "2004-01-01") == 0);

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

  MYASSERT(test_date_parser_tostring("2012-04-12Z", "2012-04-12") == 0);
  
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

  MYASSERT(test_date_parser_tostring("2004-12-31-13:00", "2005-01-01") == 0);
  MYASSERT(test_date_parser_tostring("2005-01-01+13:00", "2004-12-31") == 0);
  MYASSERT(test_date_parser_tostring("2004-12-31-11:59", "2004-12-31") == 0);
  MYASSERT(test_date_parser_tostring("2005-01-01+11:59", "2005-01-01") == 0);


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
  

  return 0;
}

#endif
