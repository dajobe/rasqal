dnl -*- Mode: autoconf -*-
dnl
dnl configure.ac - autoconf file for rasqal
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl $Id$
dnl
dnl Copyright (C) 2003-2005, David Beckett http://purl.org/net/dajobe/
dnl Institute for Learning and Research Technology http://www.ilrt.bristol.ac.uk/
dnl University of Bristol, UK http://www.bristol.ac.uk/
dnl 
dnl This package is Free Software and part of Redland http://librdf.org/
dnl 
dnl It is licensed under the following three licenses as alternatives:
dnl   1. GNU Lesser General Public License (LGPL) V2.1 or any newer version
dnl   2. GNU General Public License (GPL) V2 or any newer version
dnl   3. Apache License, V2.0 or any newer version
dnl 
dnl You may not use this file except in compliance with at least one of
dnl the above three licenses.
dnl 
dnl See LICENSE.html or LICENSE.txt at the top of this package for the
dnl complete terms and further detail along with the license texts for
dnl the licenses in COPYING.LIB, COPYING and LICENSE-2.0.txt respectively.
dnl 


AC_INIT(Rasqal RDF Query Library, 0.9.11, http://bugs.librdf.org/, rasqal)
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(rdql_parser.y)
AC_REVISION($Revision$)

AM_INIT_AUTOMAKE([1.7 check-news std-options -Wall])
AM_CONFIG_HEADER(rasqal_config.h)
AM_MAINTAINER_MODE

release_version=no
AC_ARG_ENABLE(release, [  --enable-release        Turn on optimizations (for maintainer).  ], \
 if test "$enableval" = "yes"; then \
	  release_version=yes
fi;)	

if test "$USE_MAINTAINER_MODE" = yes -a $release_version = no; then 
  CFLAGS=`echo $CFLAGS | sed s/-O2//`
  CXXFLAGS=`echo $CXXFLAGS | sed s/-O2//`
  CPPFLAGS=`echo $CPPFLAGS | sed s/-O2//`
fi

dnl Checks for programs.
AC_CANONICAL_HOST
AM_SANITY_CHECK
AM_PROG_CC_STDC
AC_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET


AM_PROG_LEX
if test "$LEX" != flex; then
  LEX="$SHELL $missing_dir/missing flex"
  AC_SUBST(LEX_OUTPUT_ROOT, lex.yy)
  AC_SUBST(LEXLIB, '')
fi

FLEX_MIN_VERSION=2.5.19
FLEX_REC_VERSION=2.5.31
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(flex version)
  if test "X$LEX" != "X:"; then
    FLEX_VERSION=`$LEX -V 2>&1 | awk '{print $NF}'`
    FLEX_VERSION_DEC=`echo $FLEX_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    FLEX_MIN_VERSION_DEC=`echo $FLEX_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    if test $FLEX_VERSION_DEC -ge $FLEX_MIN_VERSION_DEC; then
      AC_MSG_RESULT($FLEX_VERSION - OK)
    else
      AC_MSG_WARN(Please get flex from http://lex.sourceforge.net/)
      AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
      AC_MSG_FAILURE(flex version $FLEX_VERSION is too old)
    fi
  else
    AC_MSG_WARN(Please get flex from http://lex.sourceforge.net/)
    AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
    AC_MSG_FAILURE(flex not present)
  fi
fi

AC_PROG_YACC
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(for GNU bison)
  YACC_VERSION=`$YACC --version 2>&1 | sed -ne 's/^.*GNU Bison. //p'`
  if test "X$YACC_VERSION" != X; then
    AC_MSG_RESULT($YACC_VERSION - OK)
  else
    AC_MSG_FAILURE($YACC is not GNU bison)
  fi
fi


# Find a tar command for 'make dist'
AC_CHECK_PROGS(TAR, gnutar gtar tar)
AC_CHECK_PROGS(PERL, perl)

AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)


dnl compiler checks
# if using gcc...
if test "$ac_cv_prog_gcc" = yes; then
  STANDARD_CFLAGS=
  MAINTAINER_CFLAGS="-Wall -Wshadow -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wredundant-decls -Wswitch-enum"
else
  STANDARD_CFLAGS=
  MAINTAINER_CFLAGS=
fi


# Externally linked libraries - appear in rasqal-config
# -Dfoo -Idir
RASQAL_CPPFLAGS=$CPPFLAGS
# -Ldir
RASQAL_LDFLAGS=$LDFLAGS
# -llib
RASQAL_LIBS=$LIBS

# Internally linked libraries - never given to users
# -Dfoo -Idir
RASQAL_INTERNAL_CPPFLAGS=
# -Ldir -llib
RASQAL_INTERNAL_LIBS=

# Libraries linked externally also may be internal - just raptor
# Currently only substituted into rasqal-src-config.
# -Dfoo -Idir
RASQAL_EXTERNAL_CPPFLAGS=
# -Ldir -llib
RASQAL_EXTERNAL_LIBS=
# Extra pkgconfig requires
PKGCONFIG_REQUIRES=
PKGCONFIG_LIBS=
PKGCONFIG_CFLAGS=

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h stdlib.h unistd.h string.h dmalloc.h getopt.h regex.h sys/time.h)
AC_HEADER_TIME


dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN

dnl need to change quotes to allow square brackets
changequote(<<, >>)dnl
version_major=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\1/'`
version_minor=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\2/'`
version_release=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\3/'`
changequote([, ])dnl

version_decimal=`expr $version_major \* 10000 + $version_minor \* 100 + $version_release`

AC_DEFINE_UNQUOTED(RASQAL_VERSION_MAJOR, $version_major, [Major version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_MINOR, $version_minor, [Minor version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_RELEASE, $version_release, [Release version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_DECIMAL, $version_decimal, [Release version as a decimal])

# for rasqal-config.in
RASQAL_VERSION_DECIMAL=$version_decimal
AC_SUBST(RASQAL_VERSION_DECIMAL)

# Libtool versioning
#
# CURRENT
#      The most recent interface number that this library implements.
# 
# REVISION
#      The implementation number of the CURRENT interface.
# 
# AGE
#      The difference between the newest and oldest interfaces that this
#      library implements.  In other words, the library implements all the
#      interface numbers in the range from number `CURRENT - AGE' to
#      `CURRENT'.
#
# Rules:
#  1. Start with version information of `0:0:0' for each libtool library.
#
#  2. Update the version information only immediately before a public
#     release of your software.  More frequent updates are unnecessary,
#     and only guarantee that the current interface number gets larger
#     faster.
#
#  3. If the library source code has changed at all since the last
#     update, then increment REVISION (`C:R:A' becomes `C:r+1:A').
#
#  4. If any interfaces have been added, removed, or changed since the
#     last update, increment CURRENT, and set REVISION to 0.
#
#  5. If any interfaces have been added since the last public release,
#     then increment AGE.
#
#  6. If any interfaces have been removed since the last public release,
#     then set AGE to 0.
#
# syntax: CURRENT[:REVISION[:AGE]]
RASQAL_LIBTOOL_VERSION=0:0:0
AC_SUBST(RASQAL_LIBTOOL_VERSION)


dnl Checks for library functions.
AC_CHECK_FUNCS(getopt getopt_long stricmp strcasecmp vsnprintf srandomdev)

AM_CONDITIONAL(STRCASECMP, test $ac_cv_func_stricmp = no -a $ac_cv_func_stricmp = no)
AM_CONDITIONAL(GETOPT, test $ac_cv_func_getopt = no -a $ac_cv_func_getopt_long = no)


AC_MSG_CHECKING(whether need to declare optind)
AC_TRY_LINK([#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif], [int x=optind;],
            AC_MSG_RESULT(no),
            AC_DEFINE(NEED_OPTIND_DECLARATION, 1, [need 'extern int optind' declaration?])
            AC_MSG_RESULT(yes))



# raptor is REQUIRED despite the checking here
AC_CHECK_PROG(RAPTOR_CONFIG, raptor-config, raptor-config)
AC_ARG_WITH(raptor, [  --with-raptor=SOURCE        Pick raptor source - system/internal (default auto)], with_raptor="$withval", with_raptor="auto")

RAPTOR_MIN_VERSION=1.4.8
AC_SUBST(RAPTOR_MIN_VERSION)
if test "X$RAPTOR_CONFIG" = X; then
  with_raptor=internal
else
  RAPTOR_VERSION=`$RAPTOR_CONFIG --version 2>/dev/null`

  RAPTOR_VERSION_DEC=`$RAPTOR_CONFIG --version-decimal 2>/dev/null`
  RAPTOR_MIN_VERSION_DEC=`echo $RAPTOR_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

  if test "X$with_raptor" = Xauto; then
    if test "X$RAPTOR_VERSION" -a $RAPTOR_VERSION_DEC -ge $RAPTOR_MIN_VERSION_DEC; then
      with_raptor=system
    else
      AC_MSG_WARN(System raptor $RAPTOR_VERSION is too old - need $RAPTOR_MIN_VERSION)
      with_raptor=internal
    fi
  elif test $with_raptor = system; then
    if test $RAPTOR_VERSION_DEC -lt $RAPTOR_MIN_VERSION_DEC; then
      AC_MSG_WARN(System raptor $RAPTOR_VERSION is too old - need $RAPTOR_MIN_VERSION)
      AC_MSG_WARN(Proceeding anyway since --with-raptor=system was given)
      AC_MSG_WARN(To use the internal raptor use --with-raptor=internal)
      AC_MSG_WARN(or omit any --with-raptor argument.)
    fi
  fi
fi
AC_SUBST(with_raptor)

AC_MSG_CHECKING(raptor library source)
if test $with_raptor = system; then
  AC_MSG_RESULT(system $RAPTOR_VERSION)
else
  RAPTOR_VERSION=internal
  AC_MSG_RESULT(internal)
  if test ! -r $srcdir/../raptor/raptor_internal.h; then
    AC_MSG_ERROR(Raptor sources missing from $srcdir/../raptor directory - cannot use internal)
  fi
fi


dnl Checks for regex libraries
have_regex_pcre=0
have_regex_posix=0
need_regex_pcre=0
need_regex_posix=0

AC_CHECK_PROG(PCRE_CONFIG, pcre-config, pcre-config)
AC_ARG_WITH(pcre, [  --with-pcre-config=PATH     Location of PCRE pcre-config (auto)], pcre_config="$withval", pcre_config="")

if test "X$pcre_config" != "X" ; then
  AC_MSG_CHECKING(for $pcre_config)

  if test -f $pcre_config ; then
    PCRE_CONFIG=$pcre_config
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no - searching PATH)
  fi
fi
if test "X$PCRE_CONFIG" = "X"; then
  AC_CHECK_PROGS(PCRE_CONFIG, pcre-config)
fi


AC_MSG_CHECKING(for pcre)
PCRE_VERSION=`$PCRE_CONFIG --version 2>/dev/null`
PCRE_MIN_VERSION=3.9

PCRE_VERSION_DEC=`echo $PCRE_VERSION | $AWK -F. '{printf("%d\n", 100*$1 + $2)};'`
PCRE_MIN_VERSION_DEC=`echo $PCRE_MIN_VERSION | $AWK -F. '{printf("%d\n", 100*$1 + $2)};'`
if test "X$PCRE_VERSION" = X; then
  AC_MSG_RESULT(not present)
elif test "X$PCRE_VERSION" -a $PCRE_VERSION_DEC -ge $PCRE_MIN_VERSION_DEC; then
  have_regex_pcre=1
  AC_MSG_RESULT($PCRE_VERSION)
else
  AC_MSG_WARN($PCRE_VERSION - too old - need $PCRE_MIN_VERSION)
fi

if test $have_regex_pcre = 1; then
  AC_DEFINE(HAVE_REGEX_PCRE, 1, [have PCRE regex - Perl Compatible Regular Expressions])
fi


AC_MSG_CHECKING(for posix regex library)
oLIBS="$LIBS"
if test $ac_cv_header_regex_h = yes; then
  AC_TRY_LINK([#include <sys/types.h>
              #include <regex.h>],
             [regex_t reg; char *pat="abc"; regcomp(&reg, pat, 0);],
             worked=yes, worked=no, worked=no)
  if test $worked = yes; then
    have_regex_posix=1
  fi
fi

LIBS="$oLIBS"

if test $have_regex_posix = 1; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_REGEX_PCRE, 1, [have PCRE regex - Perl Compatible Regular Expressions])
else
  AC_MSG_RESULT(no)
fi



AC_ARG_WITH(regex-library, [  --with-regex-library=NAME   Use regex library - posix, pcre (auto)], regex_library="$withval", regex_library="") 

for regex_library_name in $regex_library posix pcre; do
  case $regex_library_name in
    pcre)
      if test $have_regex_pcre = 1; then
        need_regex_pcre=1
        AC_DEFINE(RASQAL_REGEX_PCRE, 1, [Use PCRE regex library])
        break
      fi
      ;;

    posix)
      if test $have_regex_posix = 1; then
        need_regex_posix=1
        AC_DEFINE(RASQAL_REGEX_POSIX, 1, [Use posix regex library])
        break
      fi
      ;;

    *)
      AC_MSG_ERROR(No such regex library $regex_library_name)
      ;;
esac
done

AC_MSG_CHECKING(regex library to use)
result=
if test $need_regex_pcre = 1; then
  result=pcre
elif test $need_regex_posix = 1; then
  result=posix
else
  result=none
  AC_DEFINE(RASQAL_REGEX_NONE, 1, [Use no regex library])
  AC_MSG_WARN(No regex library available - please install pcre or a POSIX regex library)
fi
AC_MSG_RESULT($result)


AC_ARG_WITH(xml2-config, [  --with-xml2-config=PATH   Location of libxml xml2-config []], xml2_config="$withval", xml2_config="")

if test "X$xml2_config" != "X" ; then
  AC_MSG_CHECKING(for $xml2_config)

  if test -f $xml2_config ; then
    XML_CONFIG=$xml2_config
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no - searching PATH)
  fi
fi
if test "X$XML_CONFIG" = "X"; then
  AC_CHECK_PROGS(XML_CONFIG, xml2-config)
fi


have_libxml=0
need_libxml=0

LIBXML_MIN_VERSION=2.4.0
AC_MSG_CHECKING(for libxml2 library)
oCPPFLAGS="$CPPFLAGS"
oLIBS="$LIBS"
if test "X$XML_CONFIG" != X; then
  LIBS="$LIBS `$XML_CONFIG --libs`"
  CPPFLAGS="`$XML_CONFIG --cflags` $CPPFLAGS"
  LIBXML_VERSION=`$XML_CONFIG --version`
  libxml_version_dec=`echo $LIBXML_VERSION | awk -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
  LIBXML_MIN_VERSION_dec=`echo $LIBXML_MIN_VERSION | awk -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
  AC_MSG_RESULT(yes - version $LIBXML_VERSION)
  if test $libxml_version_dec -lt $LIBXML_MIN_VERSION_dec; then
     AC_MSG_WARN(Using libxml $LIBXML_VERSION is unsupported - $LIBXML_MIN_VERSION or newer required.)
  else
    AC_CHECK_HEADERS(libxml/xmlschemas.h libxml/xmlschemastypes.h libxml/schemasInternals.h)
    if test "$ac_cv_header_libxml_xmlschemas_h" = no; then
      AC_MSG_WARN(libxml library found but not xml schema headers - disabling)
    else
      have_libxml=1
    fi
  fi
else
  AC_MSG_RESULT(no)
fi
CPPFLAGS="$oCPPFLAGS"
LIBS="$oLIBS"


AC_MSG_CHECKING(enable datatypes support)
datatypes=no
AC_ARG_ENABLE(datatypes, [  --enable-datatypes        Turn on datatypes], datatypes="$enableval")
if test "X$datatypes" = Xno -a "$USE_MAINTAINER_MODE" = yes; then
  datatypes=yes
fi
if test "X$datatypes" = Xyes; then
  if test $have_libxml = 1; then
    need_libxml=1;
    AC_MSG_RESULT(yes)
   else
    AC_MSG_RESULT(no - libxml with XML schema datatypes not available)
    datatypes=no
  fi
else
  AC_MSG_RESULT(yes)
fi
AM_CONDITIONAL(RASQAL_DATATYPES, test "X$datatypes" = Xyes)


dnl checks for triples source libraries
have_redland=0
need_redland=0

AC_CHECK_PROG(REDLAND_CONFIG, redland-config, redland-config)
AC_ARG_WITH(redland-config, [  --with-redland-config=PATH  Location of redland redland-config ()], redland_config="$withval", redland_config="")

if test "X$redland_config" != "X" ; then
  AC_MSG_CHECKING(for $redland_config)

  if test -f $redland_config ; then
    REDLAND_CONFIG=$redland_config
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no - searching PATH)
  fi
fi

REDLAND_MIN_VERSION=0.9.16
if test "X$REDLAND_CONFIG" != "X"; then
  REDLAND_VERSION=`$REDLAND_CONFIG --version 2>/dev/null`

  REDLAND_VERSION_DEC=`$REDLAND_CONFIG --version-decimal 2>/dev/null`
  REDLAND_MIN_VERSION_DEC=`echo $REDLAND_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

  if test "X$REDLAND_VERSION" = X; then
    AC_MSG_ERROR(Redland RDF library missing - please install from http://librdf.org/)
  fi

  if test "X$REDLAND_VERSION" -a $REDLAND_VERSION_DEC -ge $REDLAND_MIN_VERSION_DEC; then
    have_redland=1
  else
    AC_MSG_WARN(System redland $REDLAND_VERSION is too old - need $REDLAND_MIN_VERSION)
  fi
fi


AC_ARG_WITH(triples-source, [  --with-triples-source=NAME  Use NAME to provide triples - raptor (default), redland], triples_source="$withval", triples_source="raptor") 

for triples_source_name in $triples_source raptor redland; do
  case $triples_source_name in
    raptor)
      AC_DEFINE([RAPTOR_TRIPLES_SOURCE_RAPTOR], 1, [Use raptor to provide triples])
      break
      ;;

    redland)
      if test $have_redland = 1; then
        need_redland=1
	AC_DEFINE([RAPTOR_TRIPLES_SOURCE_REDLAND], 1, [Use redland to provide triples])
        break
      fi
      ;;

    *)
      AC_MSG_ERROR(No known triples source $triples_source_name)
      ;;
esac
done

AC_MSG_CHECKING(triples source libraries available)
triples_source_libraries_available="raptor $RAPTOR_VERSION"
if test $have_redland = 1; then
  triples_source_libraries_available="$triples_source_libraries_available redland $REDLAND_VERSION"
fi
AC_MSG_RESULT($triples_source_libraries_available)


AC_MSG_CHECKING(triples source library to use)
triples_source_library=
if test $need_redland = 1; then
  triples_source_library="redland $REDLAND_VERSION"
else
  triples_source_library="raptor $RAPTOR_VERSION"
fi
AC_MSG_RESULT($triples_source_library)



dnl RDF Query Languages
rdql_query_language=no
sparql_query_language=no

rdf_query_languages_available="rdql sparql"
rdf_query_languages_enabled=


# This is needed because autoheader can't work out which computed
# symbols must be pulled from acconfig.h into config.h.in
if test "x" = "y"; then
  AC_DEFINE(RASQAL_QUERY_RDQL, 1, [Building RDQL query])
  AC_DEFINE(RASQAL_QUERY_SPARQL, 1, [Building SPARQL query])
fi

AC_MSG_CHECKING(query languages required)
AC_ARG_ENABLE(query-languages, [  --enable-query-languages=LIST   Enable query languages (default=all)], query_languages="$enableval") 
if test "X$query_languages" = Xall -o "X$query_languages" = X; then
  query_languages="$rdf_query_languages_available"
  AC_MSG_RESULT(all)
else
  AC_MSG_RESULT($query_languages)
fi

for query_language in $query_languages; do
  p=$query_language
  eval $p'_query_language=yes'
  NAME=`echo $p | tr 'abcdefghijklmnopqrstuvwxyz' 'ABCDEFGHIJKLMNOPQRSTUVWXYZ'`
  n=RASQAL_QUERY_${NAME}
  AC_DEFINE_UNQUOTED($n)
  rdf_query_languages_enabled="$rdf_query_languages_enabled $query_language"
done

AM_CONDITIONAL(RASQAL_QUERY_RDQL, test $rdql_query_language = yes)
AM_CONDITIONAL(RASQAL_QUERY_SPARQL, test $sparql_query_language = yes)



if test $with_raptor = internal; then
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS -I\$(top_srcdir)/../raptor"
  RASQAL_INTERNAL_LIBS="$RASQAL_INTERNAL_LIBS \$(top_builddir)/../raptor/libraptor.la"
  RAPPER_PATH=":\$(top_srcdir)/../raptor"
else
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `$RAPTOR_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$RAPTOR_CONFIG --libs`"
  RAPPER_PATH=""
fi

if test $need_regex_pcre = 1; then
  C=`$PCRE_CONFIG --cflags`
  L=`$PCRE_CONFIG --libs`
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  PKGCONFIG_CFLAGS="PKGCONFIG_CFLAGS $C"
  PKGCONFIG_LIBS="PKGCONFIG_LIBS $L"
  unset C
  unset L
fi

if test $need_redland = 1; then
  AC_LIBOBJ([rasqal_redland])
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `redland-config --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `redland-config --libs`"
  PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES, redland >= $REDLAND_MIN_VERSION"
fi

if test $need_libxml = 1; then
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `$XML_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$XML_CONFIG --libs`"
  PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES, libxml-2.0 >= $LIBXML_MIN_VERSION"
fi


# Make final changes to cflags
MEM=
MEM_LIBS=
CPPFLAGS="-DRASQAL_INTERNAL=1 $CPPFLAGS"
AC_ARG_WITH(dmalloc, [  --with-dmalloc              Use dmalloc debugging library (default=no)], use_dmalloc="$withval", use_dmalloc="no") 
AC_MSG_CHECKING(using dmalloc library)
if test "$USE_MAINTAINER_MODE" = yes; then
  if test "$ac_cv_header_dmalloc_h" = yes; then
    if test "X$use_dmalloc" = Xauto; then
      use_dmalloc=yes
    fi
  else
    use_dmalloc=no
  fi
else
  use_dmalloc=no
fi
AC_MSG_RESULT($use_dmalloc);
if test $use_dmalloc = yes; then
  MEM=-DRASQAL_MEMORY_DEBUG_DMALLOC=1
  MEM_LIBS=-ldmalloc
fi
if test "$USE_MAINTAINER_MODE" = yes; then
  CPPFLAGS="-g -DRASQAL_DEBUG=1 $CPPFLAGS"
fi
AC_ARG_WITH(memory-signing, [  --with-memory-signing       Sign allocated memory (default=no)], use_memory_signing="$withval", use_memory_signing="no") 
AC_MSG_CHECKING(using memory signing)
AC_MSG_RESULT($use_memory_signing);
if test $use_memory_signing = yes; then
  MEM=-DRASQAL_MEMORY_SIGN=1
  MEM_LIBS=
fi

STANDARD_CFLAGS="$STANDARD_CFLAGS $CFLAGS"
if test "$USE_MAINTAINER_MODE" = yes; then
  CFLAGS="$MAINTAINER_CFLAGS $CFLAGS"
fi

AC_SUBST(RASQAL_EXTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_EXTERNAL_LIBS)

AC_SUBST(RASQAL_INTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_INTERNAL_LIBS)
AC_SUBST(RAPPER_PATH)

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)
AC_SUBST(STANDARD_CFLAGS)

ECHO_N="$ECHO_N"
ECHO_C="$ECHO_C"
AC_SUBST(ECHO_N)
AC_SUBST(ECHO_C)

RASQAL_LIBTOOLLIBS=librasqal.la
AC_SUBST(RASQAL_LIBTOOLLIBS)

AC_SUBST(PKGCONFIG_REQUIRES)
AC_SUBST(PKGCONFIG_LIBS)
AC_SUBST(PKGCONFIG_CFLAGS)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

AC_CONFIG_FILES([Makefile
data/Makefile
tests/Makefile
tests/rdql/Makefile
tests/rdql/testsuite/Makefile
tests/sparql/Makefile
tests/sparql/simple/Makefile
tests/sparql/part1/Makefile
tests/sparql/examples/Makefile
tests/sparql/syntax/Makefile
tests/sparql/bound/Makefile
tests/sparql/survey/Makefile
tests/sparql/SyntaxFull/Makefile
tests/sparql/sort/Makefile
tests/sparql/ValueTesting/Makefile
tests/sparql/regex/Makefile
tests/sparql/Expr1/Makefile
tests/sparql/Expr2/Makefile
tests/sparql/ExprBuiltins/Makefile
tests/sparql/ExprEquals/Makefile
docs/Makefile
docs/version.xml
win32/Makefile
rasqal.spec
rasqal.rdf
rasqal.pc])
AC_CONFIG_FILES([rasqal-config], [chmod +x rasqal-config])
AC_CONFIG_FILES([rasqal-src-config], [chmod +x rasqal-src-config])

dnl Check for gtk-doc and docbook
GTK_DOC_CHECK([1.4])

AC_OUTPUT

AC_MSG_RESULT([
Rasqal build summary:
  RDF query languages available : $rdf_query_languages_available
  RDF query languages enabled   :$rdf_query_languages_enabled
  Triples source                : $triples_source_library
])
