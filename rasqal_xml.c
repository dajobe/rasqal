/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_xml.c - Rasqal XML
 *
 * $Id$
 *
 * Copyright (C) 2004 David Beckett - http://purl.org/net/dajobe/
 * Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
 * University of Bristol - http://www.bristol.ac.uk/
 * 
 * This package is Free Software or Open Source available under the
 * following licenses (these are alternatives):
 *   1. GNU Lesser General Public License (LGPL)
 *   2. GNU General Public License (GPL)
 *   3. Mozilla Public License (MPL)
 * 
 * See LICENSE.html or LICENSE.txt at the top of this package for the
 * full license terms.
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


static int
rasqal_xml_print_xml_attribute(FILE *handle,
                               unsigned char *attr, unsigned char *value,
                               void *user_data, raptor_message_handler handler)
{
  size_t attr_len;
  size_t len;
  size_t escaped_len;
  unsigned char *buffer;
  unsigned char *p;
  
  attr_len=strlen((const char*)attr);
  len=strlen((const char*)value);

  escaped_len=raptor_xml_escape_string(value, len,
                                       NULL, 0, '"',
                                       user_data, handler);

  buffer=(unsigned char*)RASQAL_MALLOC(cstring,
                                       1 + attr_len + 2 + escaped_len + 1 +1);
  if(!buffer)
    return 1;
  p=buffer;
  *p++=' ';
  strncpy((char*)p, (const char*)attr, attr_len);
  p+= attr_len;
  *p++='=';
  *p++='"';
  raptor_xml_escape_string(value, len,
                           p, escaped_len, '"',
                           user_data, handler);
  p+= escaped_len;
  *p++='"';
  *p++='\0';
  
  fputs((const char*)buffer, handle);
  RASQAL_FREE(cstring, buffer);

  return 0;
}



int
rasqal_query_results_print_as_xml(rasqal_query_results *results, FILE *fh,
                                  void *user_data, 
                                  raptor_message_handler handler)
{
  fputs("<results xmlns=\"http://www.w3.org/sw/2001/DataAccess/result1#\">\n\n",
        fh);
  

  while(!rasqal_query_results_finished(results)) {
    int i;

    fputs("  <result>\n", fh);
    for(i=0; i<rasqal_query_results_get_bindings_count(results); i++) {
      const unsigned char *name=rasqal_query_results_get_binding_name(results, i);
      rasqal_literal *l=rasqal_query_results_get_binding_value(results, i);
      int print_end=1;
      size_t len;
      int is_xml=0;
      
      if(!l)
        continue;
      
      fputs("    <", fh);
      fputs(name, fh);

      switch(l->type) {
        case RASQAL_LITERAL_URI:
          if(rasqal_xml_print_xml_attribute(fh, "uri",
                                            raptor_uri_as_string(l->value.uri),
                                            user_data, handler))
            return 1;
          
          fputs("/>\n", fh);
          print_end=0;
          break;
        case RASQAL_LITERAL_STRING:
          len=strlen(l->string);
          if(!len) {
            fputs("/>\n", fh);
            print_end=0;
            break;
          }
          
          if(l->language) {
            if(rasqal_xml_print_xml_attribute(fh, "xml:lang",
                                              (unsigned char *)l->language,
                                              user_data, handler))
              return 1;
          }

          if(l->datatype) {
            if(!strcmp(raptor_uri_as_string(l->datatype),
                       raptor_xml_literal_datatype_uri_string))
              is_xml=1;
            else {
              if(rasqal_xml_print_xml_attribute(fh, "datatype",
                                                raptor_uri_as_string(l->datatype),
                                                user_data, handler))
                return 1;
            }
          }
          
          fputc('>', fh);

          if(is_xml)
            fputs(l->string, fh);
          else {
            int xml_string_len=raptor_xml_escape_string(l->string, len,
                                                        NULL, 0, 0,
                                                        NULL, NULL);
            if(xml_string_len == (int)len)
              fputs(l->string, fh);
            else {
              unsigned char *xml_string=(unsigned char*)RASQAL_MALLOC(cstring, xml_string_len+1);
              
              xml_string_len=raptor_xml_escape_string(l->string, len,
                                                      xml_string, xml_string_len, 0,
                                                      NULL, NULL);
              fputs(xml_string, fh);
              RASQAL_FREE(cstring, xml_string);
            }
          }
          
          break;
        case RASQAL_LITERAL_BLANK:
        case RASQAL_LITERAL_PATTERN:
        case RASQAL_LITERAL_QNAME:
        case RASQAL_LITERAL_INTEGER:
        case RASQAL_LITERAL_BOOLEAN:
        case RASQAL_LITERAL_FLOATING:
        case RASQAL_LITERAL_VARIABLE:
        default:
          RASQAL_FATAL2("Cannot turn literal type %d into XML", l->type);
      }

      if(print_end) {
        fputs("</", fh);
        fputs(name, fh);
        fputs(">\n", fh);
      }
    }
    fputs("  </result>\n\n", fh);
    
    rasqal_query_results_next(results);
  }

  fputs("</results>\n", fh);

  return 0;
}
