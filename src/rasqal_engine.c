/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_engine.c - Rasqal query engine utility functions
 *
 * Copyright (C) 2011, David Beckett http://www.dajobe.org/
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
 */


#ifdef HAVE_CONFIG_H
#include <rasqal_config.h>
#endif

#ifdef WIN32
#include <win32_rasqal_config.h>
#endif

#include <stdio.h>
#include <string.h>

#include <raptor.h>

#include "rasqal.h"
#include "rasqal_internal.h"



#ifdef RASQAL_DEBUG
static const char rasqal_engine_parts_string[16][5] = {
  /*  0 -  3 */  "----", "S---", "-P--", "SP--",
  /*  4 -  7 */  "--O-", "S-O-", "-PO-", "SPO-",
  /*  8 - 11 */  "---G", "S--G", "-P-G", "SP-G",
  /* 12 - 15 */  "--OG", "S-OG", "-POG", "SPOG",
};


const char*
rasqal_engine_get_parts_string(rasqal_triple_parts parts)
{
  return rasqal_engine_parts_string[RASQAL_GOOD_CAST(int, parts) & RASQAL_TRIPLE_SPOG];
}
#endif


#ifdef RASQAL_DEBUG
static const char* const rasqal_engine_error_labels[RASQAL_ENGINE_ERROR_LAST+2]=
{
  "ok",
  "FAILED",
  "finished",
  "unknown"
};

const char*
rasqal_engine_error_as_string(rasqal_engine_error error) 
{
  if(error > RASQAL_ENGINE_ERROR_LAST)
    error = (rasqal_engine_error)(RASQAL_ENGINE_ERROR_LAST+1);

  return rasqal_engine_error_labels[RASQAL_GOOD_CAST(int, error)];
}
#endif
