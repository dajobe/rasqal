dnl
dnl configure.ac - autoconf file for rasqal
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl $Id$
dnl
dnl Copyright (C) 2003-2004 David Beckett - http://purl.org/net/dajobe/
dnl Institute for Learning and Research Technology - http://www.ilrt.bris.ac.uk/
dnl University of Bristol - http://www.bristol.ac.uk/
dnl 
dnl This package is Free Software or Open Source available under the
dnl following licenses (these are alternatives):
dnl   1. GNU Lesser General Public License (LGPL)
dnl   2. GNU General Public License (GPL)
dnl   3. Mozilla Public License (MPL)
dnl 
dnl See LICENSE.html or LICENSE.txt at the top of this package for the
dnl full license terms.
dnl 


AC_INIT(Rasqal RDF Query Library, 0.2.0, redland-dev@lists.librdf.org, rasqal)
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(rdql_parser.y)
AC_REVISION($Revision$)

AM_INIT_AUTOMAKE([1.7 check-news std-options -Wall])
AM_CONFIG_HEADER(rasqal_config.h)
AM_MAINTAINER_MODE

release_version=no
AC_ARG_ENABLE(release, [  --enable-release        Turn on optimizations.  ], \
 if test "$enableval" = "yes"; then \
	release_version=yes
fi;)	

if test $release_version = no; then 
  CFLAGS=`echo $CFLAGS | sed s/-O2//`
  CXXFLAGS=`echo $CXXFLAGS | sed s/-O2//`
  CPPFLAGS=`echo $CPPFLAGS | sed s/-O2//`
fi

dnl Checks for programs.
AC_CANONICAL_HOST
AM_SANITY_CHECK
AM_PROG_CC_STDC
AM_PROG_LIBTOOL
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET


AM_PROG_LEX
if test "$LEX" != flex; then
  LEX="$SHELL $missing_dir/missing flex"
  AC_SUBST(LEX_OUTPUT_ROOT, lex.yy)
  AC_SUBST(LEXLIB, '')
fi
AC_PROG_YACC

# Find a tar command for 'make dist'
AC_CHECK_PROGS(TAR, gnutar gtar tar)

AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)


release_version=no
AC_ARG_ENABLE(release, [  --enable-release        Turn on optimizations.  ], \
 if test "$enableval" = "yes"; then \
	release_version=yes
fi;)	

if test $release_version = no; then 
  CFLAGS=`echo $CFLAGS | sed s/-O2//`
  CXXFLAGS=`echo $CXXFLAGS | sed s/-O2//`
  CPPFLAGS=`echo $CPPFLAGS | sed s/-O2//`
fi



dnl compiler checks
# if using gcc...
if test "$ac_cv_prog_gcc" = yes; then
  STANDARD_CFLAGS=
  MAINTAINER_CFLAGS="-Wall -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
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


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h stdlib.h unistd.h string.h dmalloc.h getopt.h regex.h)


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
AC_CHECK_FUNCS(getopt getopt_long stricmp strcasecmp vsnprintf)

# was AC_REPLACE_FUNCS(strcasecmp) but strcasecmp has two alternatives:
# the other non-standard function stricmp or a local alternative
if test $ac_cv_func_stricmp = no -a $ac_cv_func_stricmp = no; then
  AC_LIBOBJ([strcasecmp])
fi

ROQET_EXTRA_OBJS=
if test $ac_cv_func_getopt = no -a $ac_cv_func_getopt_long = no; then
  ROQET_EXTRA_OBJS='getopt.$(OBJEXT)'
  AC_SUBST([ROQET_EXTRA_OBJS])
fi


AC_MSG_CHECKING(whether need to declare optind)
AC_TRY_LINK([#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif], [int x=optind;],
            AC_MSG_RESULT(no),
            AC_DEFINE(NEED_OPTIND_DECLARATION, 1, [need 'extern int optind' declaration?])
            AC_MSG_RESULT(yes))



AC_CHECK_PROG(RAPTOR_CONFIG, raptor-config, raptor-config)
AC_ARG_WITH(raptor, [  --with-raptor=SOURCE    Pick raptor source - system/internal (default auto)], with_raptor="$withval", with_raptor="auto")

RAPTOR_VERSION=`$RAPTOR_CONFIG --version 2>/dev/null`
RAPTOR_MIN_VERSION=1.3.0

# FIXME - could use raptor-config --decimal-version at some point
RAPTOR_VERSION_DEC=`echo $RAPTOR_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
RAPTOR_MIN_VERSION_DEC=`echo $RAPTOR_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

if test "X$RAPTOR_VERSION" = X; then
  AC_MSG_ERROR(Raptor RDF parser library missing - please install from http://www.redland.opensource.ac.uk/raptor/)
fi


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

AC_MSG_CHECKING(raptor library source)
if test $with_raptor = system; then
  AC_MSG_RESULT(system $RAPTOR_VERSION)
else
  RAPTOR_VERSION=internal
  AC_MSG_RESULT(internal)
  if test ! -r ../raptor/raptor_internal.h; then
    AC_MSG_ERROR(Raptor sources missing in parent directory - cannot use internal)
  fi
fi


dnl Checks for regex libraries
have_regex_pcre=0
have_regex_posix=0
need_regex_pcre=0
need_regex_posix=0

AC_CHECK_PROG(PCRE_CONFIG, pcre-config, pcre-config)
AC_ARG_WITH(pcre, [  --with-pcre-config=PATH    Location of PCRE pcre-config (auto)], pcre_config="$withval", pcre_config="")

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
  LIBS="$LIBS -lexpat"
  AC_TRY_RUN([#include <sys/types.h>
              #include <regex.h>
             main() {regex_t reg; char *pat="abc"; regcomp(&reg, pat, 0);}],
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



AC_ARG_WITH(regex-library, [  --with-regex-library=NAME  Use regex library - posix, pcre (auto)], regex_library="$withval", regex_library="") 

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


dnl checks for triples source libraries
have_redland=0
need_redland=0

AC_CHECK_PROG(REDLAND_CONFIG, redland-config, redland-config)
AC_ARG_WITH(redland-config, [  --with-redland-config=PATH Location of redland redland-config ()], redland_config="$withval", redland_config="")

if test "X$redland_config" != "X" ; then
  AC_MSG_CHECKING(for $redland_config)

  if test -f $redland_config ; then
    REDLAND_CONFIG=$redland_config
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no - searching PATH)
  fi
fi

if test "X$REDLAND_CONFIG" = "X"; then
  AC_CHECK_PROGS(REDLAND_CONFIG, redland-config)
fi

if test "X$REDLAND_CONFIG" != "X"; then
  REDLAND_VERSION=`$REDLAND_CONFIG --version 2>/dev/null`
  REDLAND_MIN_VERSION=0.9.16

  # FIXME - could use redland-config --version-decimal at some point
  REDLAND_VERSION_DEC=`echo $REDLAND_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
  REDLAND_MIN_VERSION_DEC=`echo $REDLAND_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

  if test "X$REDLAND_VERSION" = X; then
    AC_MSG_ERROR(Redland RDF library missing - please install from http://www.redland.opensource.ac.uk/)
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


if test $with_raptor = internal; then
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS -I\$(top_srcdir)/../raptor"
  RASQAL_INTERNAL_LIBS="$RASQAL_INTERNAL_LIBS \$(top_builddir)/../raptor/libraptor.la"
else
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `$RAPTOR_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$RAPTOR_CONFIG --libs`"
fi

if test $need_regex_pcre = 1; then
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `$PCRE_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$PCRE_CONFIG --libs`"
fi

if test $need_redland = 1; then
  AC_LIBOBJ([rasqal_redland])
  RASQAL_EXTERNAL_CPPFLAGS="$RASQAL_EXTERNAL_CPPFLAGS `redland-config --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `redland-config --libs`"
fi


# Make final changes to cflags
MEM=
MEM_LIBS=
CPPFLAGS="-DRASQAL_INTERNAL=1 $CPPFLAGS"
if test "$USE_MAINTAINER_MODE" = yes; then
  if test "$ac_cv_header_dmalloc_h" = yes; then
    MEM=-DRASQAL_MEMORY_DEBUG_DMALLOC=1
    MEM_LIBS=-ldmalloc
  fi
  CPPFLAGS="-g -DRASQAL_DEBUG=1 $CPPFLAGS"
fi
STANDARD_CFLAGS="$STANDARD_CFLAGS $CFLAGS"
if test "$USE_MAINTAINER_MODE" = yes; then
  CFLAGS="$MAINTAINER_CFLAGS $CFLAGS"
fi

AC_SUBST(RASQAL_EXTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_EXTERNAL_LIBS)

AC_SUBST(RASQAL_INTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_INTERNAL_LIBS)

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)
AC_SUBST(STANDARD_CFLAGS)

ECHO_N="$ECHO_N"
ECHO_C="$ECHO_C"
AC_SUBST(ECHO_N)
AC_SUBST(ECHO_C)

RASQAL_LIBTOOLLIBS=librasqal.la
AC_SUBST(RASQAL_LIBTOOLLIBS)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

AC_CONFIG_FILES([Makefile
tests/Makefile
rdql-testsuite/Makefile
rasqal.spec
rasqal.pc])
AC_CONFIG_FILES([rasqal-config], [chmod +x rasqal-config])
AC_CONFIG_FILES([rasqal-src-config], [chmod +x rasqal-src-config])

AC_OUTPUT
