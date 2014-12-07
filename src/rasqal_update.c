/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_update.c - Rasqal graph update operations
 *
 * Copyright (C) 2010-2011, Beckett http://www.dajobe.org/
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
  "DROP",
  "LOAD",
  "UPDATE",
  "ADD",
  "MOVE",
  "COPY"
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

  return rasqal_update_type_labels[RASQAL_GOOD_CAST(int, type)];
}
  

/*
 * rasqal_new_update_operation:
 * @type: type of update
 * @graph_uri: optional graph URI
 * @document_uri: optional document URI
 * @insert_templates: optional sequence of #rasqal_triple to insert. Data triples if @flags is #RASQAL_UPDATE_FLAGS_DATA set, templates otherwise.
 * @delete_templates: optional sequence of #rasqal_triple templates to delete
 * @where: optional where BASIC graph pattern
 * @flags: update flags - bit-or of flags defined in #rasqal_update_flags
 * @applies: graph to which update applies defined in #rasqal_update_graph_applies
 *
 * INTERNAL - Constructor - Create new update operation
 *
 * All parameters become owned by the update operation.
 *
 * At least one of @graph_uri, @document_uri, @insert_templates,
 * @delete_templates or @graph_pattern must be given unless type is
 * #RASQAL_UPDATE_TYPE_CLEAR
 *
 * Return value: new update object or NULL on failure
 */
rasqal_update_operation*
rasqal_new_update_operation(rasqal_update_type type,
                            raptor_uri* graph_uri,
                            raptor_uri* document_uri,
                            raptor_sequence* insert_templates,
                            raptor_sequence* delete_templates,
                            rasqal_graph_pattern* where,
                            int flags,
                            rasqal_update_graph_applies applies)
{
  rasqal_update_operation* update;
  int is_always_2_args = (type >= RASQAL_UPDATE_TYPE_ADD &&
                          type <= RASQAL_UPDATE_TYPE_COPY);

  if(!is_always_2_args && 
     type != RASQAL_UPDATE_TYPE_CLEAR) {
    if(!graph_uri && !document_uri && !insert_templates && !delete_templates &&
       !where)
      return NULL;
  }
  
  update = RASQAL_MALLOC(rasqal_update_operation*, sizeof(*update));
  if(!update)
    return NULL;
  
  update->type = type;
  update->graph_uri = graph_uri;
  update->document_uri = document_uri;
  update->insert_templates = insert_templates;
  update->delete_templates = delete_templates;
  update->where = where;
  update->flags = flags;
  update->applies = applies;
  
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
  if(!update)
    return;
  
  if(update->graph_uri)
    raptor_free_uri(update->graph_uri);
  if(update->document_uri)
    raptor_free_uri(update->document_uri);
  if(update->insert_templates)
    raptor_free_sequence(update->insert_templates);
  if(update->delete_templates)
    raptor_free_sequence(update->delete_templates);
  if(update->where)
    rasqal_free_graph_pattern(update->where);

  RASQAL_FREE(update_operation, update);
}


int
rasqal_update_operation_print(rasqal_update_operation *update, FILE* stream)
{
  int is_always_2_args = (update->type >= RASQAL_UPDATE_TYPE_ADD &&
                          update->type <= RASQAL_UPDATE_TYPE_COPY);
  
  fputs("update-operation(type=", stream);
  fputs(rasqal_update_type_label(update->type), stream);
  if(update->graph_uri || is_always_2_args) {
    fputs(", graph-uri=", stream);
    if(update->graph_uri)
      raptor_uri_print(update->graph_uri, stream);
    else
      fputs("default", stream);
  }
  if(update->document_uri || is_always_2_args) {
    fputs(", document-uri=", stream);
    if(update->document_uri)
      raptor_uri_print(update->document_uri, stream);
    else
      fputs("default", stream);
  }

  switch(update->applies) {
    case RASQAL_UPDATE_GRAPH_ONE:
      fputs(", applies: one graph", stream);
      break;
      
    case RASQAL_UPDATE_GRAPH_DEFAULT:
      fputs(", applies: default", stream);
      break;
      
    case RASQAL_UPDATE_GRAPH_NAMED:
      fputs(", applies: named", stream);
      break;
      
    case RASQAL_UPDATE_GRAPH_ALL:
      fputs(", applies: all", stream);
      break;
  }
  
  if(update->insert_templates) {
    fputs(", insert-templates=", stream);
    raptor_sequence_print(update->insert_templates, stream);
  }
  if(update->delete_templates) {
    fputs(", delete-templates=", stream);
    raptor_sequence_print(update->delete_templates, stream);
  }
  if(update->where) {
    fputs(", where=", stream);
    rasqal_graph_pattern_print(update->where, stream);
  }
  fputc(')', stream);

  return 0;
}


