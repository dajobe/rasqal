/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * strcasecmp.c - strcasecmp compatibility
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
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <ctype.h>

#include "rasqal.h"
#include "rasqal_internal.h"


#ifndef STANDALONE

int
rasqal_strcasecmp(const char* s1, const char* s2)
{
  register int c1, c2;
  
  while(*s1 && *s2) {
    c1 = tolower(RASQAL_GOOD_CAST(int, *s1));
    c2 = tolower(RASQAL_GOOD_CAST(int, *s2));
    if (c1 != c2)
      return (c1 - c2);
    s1++;
    s2++;
  }
  return RASQAL_GOOD_CAST(int, (*s1 - *s2));
}


int
rasqal_strncasecmp(const char* s1, const char* s2, size_t n)
{
  register int c1, c2;
  
  while(*s1 && *s2 && n) {
    c1 = tolower(RASQAL_GOOD_CAST(int, *s1));
    c2 = tolower(RASQAL_GOOD_CAST(int, *s2));
    if (c1 != c2)
      return (c1 - c2);
    s1++;
    s2++;
    n--;
  }
  return 0;
}

#endif /* not STANDALONE */


#ifdef STANDALONE

#include <stdio.h>

/* one more prototype */
int main(int argc, char *argv[]);



static int
assert_strcasecmp (const char *s1, const char *s2, int expected)
{
  int result=strcasecmp(s1, s2);
  result=(result>0) ? 1 : ((result <0) ? -1 : 0);

  if (result != expected)
    {
      fprintf(stderr, "FAIL strcasecmp (%s, %s) gave %d != %d\n",
              s1, s2, result, expected);
      return 1;
    }
  return 0;
}


static int
assert_strncasecmp (const char *s1, const char *s2, size_t size, int expected)
{
  int result=strncasecmp(s1, s2, size);
  result=(result>0) ? 1 : ((result <0) ? -1 : 0);

  if (result != expected)
    {
      fprintf(stderr, "FAIL strncasecmp (%s, %s, %u) gave %d != %d\n",
              s1, s2, RASQAL_GOOD_CAST(unsigned int, size), result, expected);
      return 1;
    }
  return 0;
}


int
main(int argc, char *argv[]) 
{
  int failures=0;
  
  failures += assert_strcasecmp("foo", "foo", 0);
  failures += assert_strcasecmp("foo", "FOO", 0);
  failures += assert_strcasecmp("foo", "BaR", 1);

  failures += assert_strncasecmp("foo", "foobar", 3, 0);
  failures += assert_strncasecmp("foo", "FOOxyz", 3, 0);
  failures += assert_strncasecmp("foo", "BaRfoo", 3, 1);

  return failures;
}

#endif /* STANDALONE */
