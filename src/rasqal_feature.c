/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_feature.c - Query system features
 *
 * Copyright (C) 2006-2009, David Beckett http://www.dajobe.org/
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
#include <ctype.h>
#include <stdarg.h>

/* Rasqal includes */
#include "rasqal.h"
#include "rasqal_internal.h"


static const struct
{
  rasqal_feature feature;
  /* flag bits
   *  1=query feature
   *  2=unused
   *  4=string value (else int)
   */
  int flags;
  const char *name;
  const char *label;
} rasqal_features_list [RASQAL_FEATURE_LAST + 1]= {
  { RASQAL_FEATURE_NO_NET,    1,  "noNet",    "Deny network requests." } ,
  { RASQAL_FEATURE_RAND_SEED, 1,  "randSeed", "Set rand() seed." }
};


static const char * const rasqal_feature_uri_prefix="http://feature.librdf.org/rasqal-";
/* NOTE: this is strlen(rasqal_feature_uri_prefix) */
#define RASQAL_FEATURE_URI_PREFIX_LEN 33


/*
 * rasqal_features_enumerate_common:
 * @world: rasqal_world object
 * @feature: feature enumeration (0+)
 * @name: pointer to store feature short name (or NULL)
 * @uri: pointer to store feature URI (or NULL)
 * @label: pointer to feature label (or NULL)
 * @flags: flags to match
 * 
 * Internal: Get list of rasqal features.
 *
 * If @uri is not NULL, a pointer to a new raptor_uri is returned
 * that must be freed by the caller with raptor_free_uri().
 *
 * Return value: 0 on success, <0 on failure, >0 if feature is unknown
 **/
static int
rasqal_features_enumerate_common(rasqal_world* world,
                                 const rasqal_feature feature,
                                 const char **name, 
                                 raptor_uri **uri, const char **label,
                                 int flags)
{
  int i;

  /* for compatibility with older binaries that do not call it */
  rasqal_world_open(world);
  
  for(i=0; i <= RASQAL_FEATURE_LAST; i++)
    if(rasqal_features_list[i].feature == feature &&
       (rasqal_features_list[i].flags & flags)) {
      if(name)
        *name=rasqal_features_list[i].name;
      
      if(uri) {
        raptor_uri *base_uri;
        base_uri = raptor_new_uri(world->raptor_world_ptr, 
                                  RASQAL_GOOD_CAST(const unsigned char*, rasqal_feature_uri_prefix));
        if(!base_uri)
          return -1;
        *uri = raptor_new_uri_from_uri_local_name(world->raptor_world_ptr, base_uri,
                                                  RASQAL_GOOD_CAST(const unsigned char*, rasqal_features_list[i].name));
        raptor_free_uri(base_uri);
      }
      if(label)
        *label=rasqal_features_list[i].label;
      return 0;
    }

  return 1;
}


/**
 * rasqal_features_enumerate:
 * @world: rasqal_world object
 * @feature: feature enumeration (0+)
 * @name: pointer to store feature short name (or NULL)
 * @uri: pointer to store feature URI (or NULL)
 * @label: pointer to feature label (or NULL)
 *
 * Get list of rasqal features.
 * 
 * If uri is not NULL, a pointer to a new raptor_uri is returned
 * that must be freed by the caller with raptor_free_uri().
 *
 * Return value: 0 on success, <0 on failure, >0 if feature is unknown
 **/
int
rasqal_features_enumerate(rasqal_world* world,
                          const rasqal_feature feature,
                          const char **name, 
                          raptor_uri **uri, const char **label)
{
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, -1);

  return rasqal_features_enumerate_common(world, feature, name, uri, label, 1);
}


/**
 * rasqal_feature_value_type
 * @feature: rasqal query feature
 *
 * Get the type of a features.
 *
 * The type of the @feature is 0=integer , 1=string.  Other values are
 * undefined.  Most features are integer values and use
 * rasqal_query_set_feature rasqal_query_get_feature()
 *
 * Return value: the type of the feature or <0 if @feature is unknown
 */
int
rasqal_feature_value_type(const rasqal_feature feature) {
  if(feature > RASQAL_FEATURE_LAST)
    return -1;
  return (rasqal_features_list[feature].flags & 4) ? 1 : 0;
}


/**
 * rasqal_feature_from_uri:
 * @world: rasqal_world object
 * @uri: feature URI
 *
 * Turn a feature URI into an feature enum.
 * 
 * The allowed feature URIs are available via rasqal_features_enumerate().
 *
 * Return value: < 0 if the feature is unknown
 **/
rasqal_feature
rasqal_feature_from_uri(rasqal_world* world, raptor_uri *uri)
{
  unsigned char *uri_string;
  int i;
  rasqal_feature feature = (rasqal_feature)-1;
  
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(world, rasqal_world, feature);
  RASQAL_ASSERT_OBJECT_POINTER_RETURN_VALUE(uri, rasqal_world, feature);

  /* for compatibility with older binaries that do not call it */
  rasqal_world_open(world);
  
  uri_string = raptor_uri_as_string(uri);
  if(strncmp(RASQAL_GOOD_CAST(const char*, uri_string),
             rasqal_feature_uri_prefix,
             RASQAL_FEATURE_URI_PREFIX_LEN))
    return feature;

  uri_string += RASQAL_FEATURE_URI_PREFIX_LEN;

  for(i = 0; i <= RASQAL_FEATURE_LAST; i++)
    if(!strcmp(rasqal_features_list[i].name,
               RASQAL_GOOD_CAST(const char*, uri_string))) {
      feature = (rasqal_feature)i;
      break;
    }

  return feature;
}


/**
 * rasqal_get_feature_count:
 *
 * Get the count of features defined.
 *
 * This is prefered to the compile time-only symbol #RASQAL_FEATURE_LAST
 * and returns a count of the number of features which is
 * #RASQAL_FEATURE_LAST+1.
 *
 * Return value: count of features in the #rasqal_feature enumeration
 **/
unsigned int
rasqal_get_feature_count(void) {
  return RASQAL_FEATURE_LAST+1;
}
