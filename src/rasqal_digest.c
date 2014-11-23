/* -*- Mode: c; c-basic-offset: 2 -*-
 *
 * rasqal_digest.c - Rasqal message digests
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
#include <sys/types.h>

#include "rasqal.h"
#include "rasqal_internal.h"


/* Implementations with alternative message digest libraries follow */


/*
 * libmhash (LGPL 2+)
 * http://mhash.sourceforge.net/
 *
 * Requires: (nothing)
 *
 * code: #include <mhash.h>
 * cflags:
 * ldflags -lmhash
 *
 * No config program
 *
 * No pkg-config support
 *
 * Debian packages: libmhash2, libmhash-dev
 * Redhat packages: ?
 */

#ifdef RASQAL_DIGEST_MHASH

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

#include <mhash.h>


static hashid rasqal_digest_to_hashid[RASQAL_DIGEST_LAST + 1]={
  (hashid)-1,
  MHASH_MD5,
  MHASH_SHA1,
  MHASH_SHA224,
  MHASH_SHA256,
  MHASH_SHA384,
  MHASH_SHA512
};

int
rasqal_digest_buffer(rasqal_digest_type type, unsigned char *output,
                     const unsigned char * const input, size_t len)
{
  hashid hash_type;
  size_t output_len;
  MHASH m;
  
  if(type > RASQAL_DIGEST_LAST)
    return -1;
  
  hash_type = rasqal_digest_to_hashid[type];
  if(hash_type == (hashid)-1)
    return -1;
  
  output_len = mhash_get_block_size(hash_type);
  if(!input)
    return RASQAL_GOOD_CAST(int, output_len);
  
  m = mhash_init(hash_type);
  if(m == MHASH_FAILED)
    return -1;

  mhash(m, (const void *)input, (mutils_word32)len);
  
  mhash_deinit(m, (void*)output);

  return RASQAL_GOOD_CAST(int, output_len);
}

#endif


/* libgcrypt (LGPL 2.1+)
 * http://directory.fsf.org/project/libgcrypt/
 *
 * Requires: GnuPG error [ libgpg-error0, libgpg-error-dev]
 *
 * Config program: libgcrypt-config
 *
 * code: #include <gcrypt.h>
 * cflags: `libgcrypt-config --cflags`
 * ldflags: `libgcrypt-config --libs`
 *
 * Config program: libgcrypt-config
 *
 * No pkg-config support
 *
 * Debian packages: libgcrypt11, libgcrypt11-dev
 * Redhat packages:?
 */

#ifdef RASQAL_DIGEST_GCRYPT

#ifdef HAVE_GCRYPT_H
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <gcrypt.h>
#pragma GCC diagnostic warning "-Wdeprecated-declarations"
#endif

static enum gcry_md_algos rasqal_digest_to_gcry_md_algos[RASQAL_DIGEST_LAST + 1]={
  GCRY_MD_NONE,
  GCRY_MD_MD5,
  GCRY_MD_SHA1,
  GCRY_MD_SHA224,
  GCRY_MD_SHA256,
  GCRY_MD_SHA384,
  GCRY_MD_SHA512
};

int
rasqal_digest_buffer(rasqal_digest_type type, unsigned char *output,
                     const unsigned char * const input, size_t len)
{
  gcry_md_hd_t hash;
  enum gcry_md_algos algo;
  unsigned int output_len;
  
  if(type > RASQAL_DIGEST_LAST)
    return -1;
  
  algo = rasqal_digest_to_gcry_md_algos[type];
  if(algo == GCRY_MD_NONE)
    return -1;
  
  output_len = gcry_md_get_algo_dlen(algo);
  if(!input)
    return output_len;

  if(gcry_md_open(&hash, algo, 0))
    return -1;
  gcry_md_write(hash, input, len);
  gcry_md_final(hash);
  memcpy(output, gcry_md_read(hash, algo), output_len);
  gcry_md_close(hash);
  
  return output_len;
}

#endif



/* apr-util (Apache 2.0) - a sub project of APR
 * http://apr.apache.org/ 
 *
 * Requires: (probably) APR of same major version
 *
 * code: #include <apr-1/apr_md5.h>  #include <apr-1/apr_sha1.h>
 * cflags: `apu-config --includes`
 * ldflags: `apu-config --link-ld --libs`
 *
 * Config program: apu-config
 *
 * pkg-config: apr-util-1
 *
 * Debian packages: libaprutil1, libaprutil1-dev
 * Redhat packages: apr-util
 *
 */
#ifdef RASQAL_DIGEST_APR

#ifdef HAVE_APR_SHA1_H
#include <apr-1/apr_sha1.h>
#endif
#ifdef HAVE_APR_MD5_H
#include <apr-1/apr_md5.h>
#endif


int
rasqal_digest_buffer(rasqal_digest_type type, unsigned char *output,
                     const unsigned char *input, size_t len)
{
  unsigned int output_len = 0;
  
  if(type != RASQAL_DIGEST_SHA1 && type != RASQAL_DIGEST_MD5)
    return -1;

#ifdef HAVE_APR_SHA1_H
  if(type == RASQAL_DIGEST_SHA1)
    output_len = APR_SHA1_DIGESTSIZE;
#endif

#ifdef HAVE_APR_MD5_H
  if(type == RASQAL_DIGEST_MD5)
    output_len = APR_MD5_DIGESTSIZE;

  if(!input)
    return output_len;
#endif

#ifdef HAVE_APR_SHA1_H
  if(type == RASQAL_DIGEST_SHA1) {
    struct apr_sha1_ctx_t c;

    apr_sha1_init(&c);
    apr_sha1_update_binary(&c, input, len);
    apr_sha1_final(RASQAL_GOOD_CAST(unsigned char*, output), &c);
  }
#endif
  
#ifdef HAVE_APR_MD5_H
  if(type == RASQAL_DIGEST_MD5) {
    if(apr_md5(RASQAL_GOOD_CAST(unsigned char*, output), input, len))
      output_len = -1;
  }
#endif
  
  return output_len;
}
#endif


/* Internal message digests - MD5 and SHA1 */
#ifdef RASQAL_DIGEST_INTERNAL
int
rasqal_digest_buffer(rasqal_digest_type type, unsigned char *output,
                     const unsigned char *input, size_t len)
{
  int output_len = -1;
  
  if(type != RASQAL_DIGEST_SHA1 && type != RASQAL_DIGEST_MD5)
    return -1;

  if(type == RASQAL_DIGEST_SHA1)
    output_len = rasqal_digest_sha1_buffer(output, input, len);
  else
    output_len = rasqal_digest_md5_buffer(output, input, len);
  
  return output_len;
}
#endif
