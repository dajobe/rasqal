/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_update.c - Rasqal graph update operations
 *
 * Copyright (C) 2010, Beckett http://www.dajobe.org/
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "rasqal.h"
#include "rasqal_internal.h"


static const char* const rasqal_update_type_labels[RASQAL_UPDATE_TYPE_LAST + 1] = {
  "Unknown",
  "CLEAR",
  "CREATE",
  "DELETE",
  "DROP",
  "INSERT",
  "LOAD",
  "UPDATE"
};



/*
 * rasqal_update_type_label:
 * @type: the #rasqal_update_type type of the update operation
 *
 * INTERNAL - Get a string for the update operation type.
 * 
 * Return value: pointer to a shared string label
 **/
const char*
rasqal_update_type_label(rasqal_update_type type)
{
  if(type <= RASQAL_UPDATE_TYPE_UNKNOWN || 
     type > RASQAL_UPDATE_TYPE_LAST)
    type = RASQAL_UPDATE_TYPE_UNKNOWN;

  return rasqal_update_type_labels[(int)type];
}
  

/*
 * rasqal_new_update_operation:
 * @type: type of update
 * @graph_uri: optional graph URI
 * @document_uri: optional document URI
 * @triple_templates: optional sequence of triple patterns (BASIC graph pattern)
 * @where: optional where template (BASIC graph pattern)
 *
 * INTERNAL - Constructor - Create new update operation
 *
 * All parameters become owned by the update operation.
 *
 * At least one of @graph_uri, @document_uri, @triples or
 * @graph_pattern must be given unless type is #RASQAL_UPDATE_TYPE_CLEAR
 *
 * Return value: new update object or NULL on failure
 */
rasqal_update_operation*
rasqal_new_update_operation(rasqal_update_type type,
                            raptor_uri* graph_uri,
                            raptor_uri* document_uri,
                            raptor_sequence* triple_templates,
                            rasqal_graph_pattern* where) 
{
  rasqal_update_operation* update;

  if(type != RASQAL_UPDATE_TYPE_CLEAR) {
    if(!graph_uri && !document_uri && !triple_templates && !where)
      return NULL;
  }
  
  update = (rasqal_update_operation*)RASQAL_MALLOC(update_operation, sizeof(*update));
  if(!update)
    return NULL;
  
  update->type = type;
  update->graph_uri = graph_uri;
  update->document_uri = document_uri;
  update->triple_templates = triple_templates;
  update->where = where;
  
  return update;
}


/*
 * rasqal_free_update_operation:
 * @update: update operation
 *
 * INTERNAL - Destructor - Free update operation
 *
 */
void
rasqal_free_update_operation(rasqal_update_operation *update)
{
  if(update->graph_uri)
    raptor_free_uri(update->graph_uri);
  if(update->document_uri)
    raptor_free_uri(update->document_uri);
  if(update->triple_templates)
    raptor_free_sequence(update->triple_templates);
  if(update->where)
    rasqal_free_graph_pattern(update->where);

  RASQAL_FREE(update_operation, update);
}


int
rasqal_update_operation_print(rasqal_update_operation *update, FILE* stream)
{
  fputs("update-operation(type=", stream);
  fputs(rasqal_update_type_label(update->type), stream);
  fputs(", graph-uri=", stream);
  if(update->graph_uri)
    raptor_uri_print(update->graph_uri, stream);
  fputs(", triple-templates=", stream);
  if(update->triple_templates)
    raptor_sequence_print(update->triple_templates, stream);
  fputs(", where=", stream);
  if(update->where)
    rasqal_graph_pattern_print(update->where, stream);
  fputc(')', stream);

  return 0;
}


