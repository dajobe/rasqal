/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * common.c - Rasqal command line utility functions
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
#include <stdarg.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include <rasqal.h>


#ifdef BUFSIZ
#define FILE_READ_BUF_SIZE BUFSIZ
#else
#define FILE_READ_BUF_SIZE 1024
#endif

#include "rasqalcmdline.h"


unsigned char*
rasqal_cmdline_read_file_fh(const char* program,
                            FILE* fh,
                            const char* filename, 
                            const char* label,
                            size_t* len_p)
{
  raptor_stringbuffer *sb;
  size_t len;
  unsigned char* string = NULL;
  unsigned char* buffer = NULL;

  sb = raptor_new_stringbuffer();
  if(!sb)
    return NULL;

  buffer = (unsigned char*)malloc(FILE_READ_BUF_SIZE);
  if(!buffer)
    goto tidy;

  while(!feof(fh)) {
    size_t read_len;
    
    read_len = fread((char*)buffer, 1, FILE_READ_BUF_SIZE, fh);
    if(read_len > 0)
      raptor_stringbuffer_append_counted_string(sb, buffer, read_len, 1);
    
    if(read_len < FILE_READ_BUF_SIZE) {
      if(ferror(fh)) {
        fprintf(stderr, "%s: file '%s' read failed - %s\n",
                program, filename, strerror(errno));
        goto tidy;
      }
      
      break;
    }
  }
  len = raptor_stringbuffer_length(sb);
  
  string = (unsigned char*)malloc(len + 1);
  if(string) {
    raptor_stringbuffer_copy_to_string(sb, string, len);
    if(len_p)
      *len_p = len;
  }
  
  tidy:
  if(buffer)
    free(buffer);

  if(fh)
    fclose(fh);

  if(sb)
    raptor_free_stringbuffer(sb);

  return string;
}


unsigned char*
rasqal_cmdline_read_file_string(const char* program,
                                const char* filename, 
                                const char* label,
                                size_t* len_p)
{
  FILE *fh = NULL;
  unsigned char* string = NULL;
  
  fh = fopen(filename, "r");
  if(!fh) {
    fprintf(stderr, "%s: %s '%s' open failed - %s", 
            program, label, filename, strerror(errno));
    goto tidy;
  }

  string = rasqal_cmdline_read_file_fh(program, fh, filename, label, len_p);
  
  fclose(fh);
  
  tidy:

  return string;
}
