/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * sv_config.h: Rasqal configure wrapper for libsv
 *
 * Copyright (C) 2013, David Beckett http://www.dajobe.org/
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


#ifndef SV_CONFIG_H
#define SV_CONFIG_H


#ifdef __cplusplus
extern "C" {
#endif

#include <rasqal_config.h>

#define sv_new rasqal_sv_new
#define sv_free rasqal_sv_free
#define sv_set_option rasqal_sv_set_option
#define sv_get_line rasqal_sv_get_line
#define sv_get_header rasqal_sv_get_header
#define sv_parse_chunk rasqal_sv_parse_chunk

#ifdef RASQAL_DEBUG
#define SV_DEBUG 1
#endif

#ifdef __cplusplus
}
#endif


#endif
