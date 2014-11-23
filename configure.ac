dnl -*- Mode: autoconf -*-
dnl
dnl configure.ac - autoconf file for rasqal
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl Copyright (C) 2003-2011, David Beckett http://www.dajobe.org/
dnl Copyright (C) 2003-2005, University of Bristol, UK http://www.bristol.ac.uk/
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


AC_PREREQ([2.62])
AC_INIT([Rasqal RDF Query Library],[0.9.33],[http://bugs.librdf.org/],[rasqal])
AC_CONFIG_SRCDIR([src/rasqal_general.c])
AC_CONFIG_HEADERS([src/rasqal_config.h])
AC_CONFIG_AUX_DIR(build)
AC_CONFIG_MACRO_DIR(build)
AM_INIT_AUTOMAKE([1.11 check-news std-options -Wobsolete -Wportability -Wsyntax -Wunsupported -Wextra-portability])

AM_MAINTAINER_MODE

release_version=no
AC_ARG_ENABLE(release, [  --enable-release        Turn on optimizations (for maintainer).  ], \
 if test "$enableval" = "yes"; then \
	  release_version=yes
fi;)	

if test "$USE_MAINTAINER_MODE" = yes -a $release_version = no; then 
  CFLAGS=`echo $CFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
  CXXFLAGS=`echo $CXXFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
  CPPFLAGS=`echo $CPPFLAGS | sed -e "s/-O[A-Za-z0-9]*//"`
fi

dnl Checks for programs.
AC_CANONICAL_HOST
AM_SANITY_CHECK
AM_PROG_AR
AC_PROG_CC
AM_PROG_CC_C_O
AC_MSG_CHECKING(whether $CC is clang)

dnl Initialize libtool
LT_INIT

AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET


FLEX_MIN_VERSION=2.5.19
FLEX_REC_VERSION=2.5.36

# Do not want AM_PROG_LEX which adds 'missing' to LEX if it's not around
AC_PROG_LEX
AC_MSG_CHECKING(flex)
if test "$USE_MAINTAINER_MODE" = yes; then
  # maintainer mode - flex is required
  if test "X$LEX" = "X:" ; then
    AC_MSG_RESULT(not present)
    AC_MSG_WARN(Please get flex from http://flex.sourceforge.net/)
    AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
    AC_MSG_FAILURE(flex not present)
  fi

  # some kind of lexer is present
  if echo "$LEX" | grep flex >/dev/null 2>&1; then
    # flex is present
    FLEX_VERSION=`$LEX -V 2>&1 | $AWK '{print $2}'`
    FLEX_VERSION_DEC=`echo $FLEX_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    FLEX_MIN_VERSION_DEC=`echo $FLEX_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    if test $FLEX_VERSION_DEC -ge $FLEX_MIN_VERSION_DEC; then
      AC_MSG_RESULT($FLEX_VERSION - OK)
    else
      AC_MSG_RESULT(version $FLEX_VERSION - too old)
      AC_MSG_WARN(Please get flex from http://flex.sourceforge.net/)
      AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
      AC_MSG_FAILURE(flex is too old)
    fi
  else
    AC_MSG_RESULT(present - but is not flex)
    AC_MSG_WARN(Please get flex from http://flex.sourceforge.net/)
    AC_MSG_WARN(version $FLEX_MIN_VERSION ($FLEX_REC_VERSION recommended))
    AC_MSG_FAILURE($LEX is not not flex)
  fi
else
  # not maintainer mode; flex is not required
  AC_MSG_RESULT(not present - not required for non maintainer builds)
  LEX="$SHELL $missing_dir/missing flex"
  AC_SUBST(LEX_OUTPUT_ROOT, lex.yy)
  AC_SUBST(LEXLIB, '')
  FLEX_VERSION_DEC=00000
fi
AC_DEFINE_UNQUOTED(FLEX_VERSION_DECIMAL, $FLEX_VERSION_DEC, [Flex version as a decimal])


BISON_MIN_VERSION=3.0.0
AC_CHECK_PROGS(BISON, bison3 bison)
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(for GNU bison)
  # Match these styles of versions
  # GNU Bison version 1.28
  # bison (GNU Bison) 1.875
dnl need to change quotes to allow square brackets
changequote(<<, >>)dnl
  BISON_VERSION=`$BISON --version 2>&1 | sed -ne 's/^.*GNU Bison[^0-9]*//p'`
changequote([, ])dnl
  if test "X$BISON_VERSION" != X; then
    BISON_VERSION_DEC=`echo $BISON_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    BISON_MIN_VERSION_DEC=`echo $BISON_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`

    if test $BISON_VERSION_DEC -ge $BISON_MIN_VERSION_DEC; then
      AC_MSG_RESULT($BISON_VERSION - OK)
    else
      AC_MSG_RESULT(version $BISON_VERSION is too old)
      AC_MSG_WARN(Please get GNU Bison from http://www.gnu.org/software/bison/)
      AC_MSG_WARN(version $BISON_MIN_VERSION or newer)
      AC_MSG_FAILURE(GNU Bison too old)
    fi
  else
    AC_MSG_FAILURE($BISON is not GNU bison)
  fi
fi
AC_SUBST(BISON)

AC_CHECK_PROGS(AWK, awk gawk nawk)
# Find a tar command for 'make dist'
AC_CHECK_PROGS(TAR, gnutar gtar tar)
AC_CHECK_PROGS(PERL, perl)

AM_MISSING_PROG(ACLOCAL, aclocal, $missing_dir)
AM_MISSING_PROG(AUTOCONF, autoconf, $missing_dir)
AM_MISSING_PROG(AUTOMAKE, automake, $missing_dir)
AM_MISSING_PROG(AUTOHEADER, autoheader, $missing_dir)

AC_CHECK_PROGS(RECHO, echo)
RECHO_C=
RECHO_N=
case `$RECHO -n x` in
  -n*)
    case `$RECHO 'xy\c'` in
      *c*)
         ;;
      xy)
         RECHO_C='\c'
         ;;
    esac;;
  *)
    RECHO_N='-n'
    ;;
esac



dnl compiler checks

AC_DEFUN([REDLAND_CC_TRY_FLAG], [
  AC_MSG_CHECKING([whether $CC supports $1])

  ## backup CFLAGS and werror status
  redland_save_CFLAGS="$CFLAGS"
  redland_save_ac_c_werror_flag="${ac_c_werror_flag}"
  AC_LANG_WERROR

  CFLAGS="$CFLAGS $1"
  AC_COMPILE_IFELSE([AC_LANG_SOURCE([ ])], [redland_cc_flag=yes], [redland_cc_flag=no])

  ## restore CFLAGS and werror status
  CFLAGS="$redland_save_CFLAGS"
  ac_c_werror_flag="${redland_save_ac_c_werror_flag}"

  if test "X$redland_cc_flag" = "Xyes"; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi
  AC_MSG_RESULT($redland_cc_flag)
])

# GCC warning options
# http://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
#
# Too noisy:
#
#  -Wconversion        : cases that need RASQAL_GOOD/BAD_CAST etc.
#  -Wcast-qual         : cases that need RASQAL_GOOD/BAD_CAST etc.
#  -Wswitch-default    : noisy cases in flex output
#  -Wsystem-headers    : not debugging system
#  -Wunused-parameter  : variables can be marked  __attribute__('unused')
#
# Apple gcc specific (probably): -Wshorten-64-to-32
possible_warnings="\
-Wall \
-Wc++-compat \
-Wextra \
-Wunused \
\
-Waggregate-return \
-Wbad-function-cast \
-Wcast-align \
-Wdeclaration-after-statement \
-Wdisabled-optimization \
-Wdiv-by-zero \
-Wendif-labels \
-Werror-implicit-function-declaration \
-Wfloat-equal \
-Wformat=2 \
-Wframe-larger-than=4096 \
-Winit-self \
-Winline \
-Wmissing-declarations \
-Wmissing-format-attribute \
-Wmissing-noreturn \
-Wmissing-prototypes \
-Wnested-externs \
-Wold-style-definition \
-Wpacked \
-Wpointer-arith \
-Wredundant-decls \
-Wshadow \
-Wsign-compare \
-Wstrict-prototypes \
-Wswitch-enum \
-Wundef \
-Wunreachable-code \
-Wunsafe-loop-optimizations \
-Wwrite-strings \
\
-Wshorten-64-to-32
\
-Wno-conversion \
-Wno-cast-qual \
-Wno-switch-default \
-Wno-system-headers \
-Wno-unused-parameter \
-Wc90-c99-compat \
-Wc99-c11-compat \
"

warning_cflags=
if test "$USE_MAINTAINER_MODE" = yes; then
  AC_MSG_CHECKING(for supported $CC warning flags)
  AC_MSG_RESULT($warning_cflags)
  for warning in $possible_warnings; do
    REDLAND_CC_TRY_FLAG([$warning], [warning_cflags="$warning_cflags $warning"])
  done
  AC_MSG_CHECKING($CC supports warning flags)
  AC_MSG_RESULT($warning_cflags)
fi

STANDARD_CFLAGS=
MAINTAINER_CPPFLAGS="$warning_cflags"

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
# -Ldir -llib
RASQAL_EXTERNAL_LIBS=
# Extra pkgconfig requires
PKGCONFIG_REQUIRES=
PKGCONFIG_LIBS=
PKGCONFIG_CFLAGS=

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h stddef.h stdlib.h stdint.h unistd.h string.h strings.h getopt.h regex.h sys/time.h time.h math.h limits.h errno.h float.h)
AC_HEADER_TIME

if test "$ac_cv_header_sys_time_h" = "yes"; then
  HAVE_SYS_TIME_H=1
else
  HAVE_SYS_TIME_H=0
fi
AC_SUBST(HAVE_SYS_TIME_H)
if test "$ac_cv_header_time_h" = "yes"; then
  HAVE_TIME_H=1
else
  HAVE_TIME_H=0
fi
AC_SUBST(HAVE_TIME_H)


dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN

AC_MSG_CHECKING(whether __FUNCTION__ is available)
AC_COMPILE_IFELSE([AC_LANG_SOURCE([int main() { printf(__FUNCTION__); }])],
    [AC_DEFINE([HAVE___FUNCTION__], [1], [Is __FUNCTION__ available])
     AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)])


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
RASQAL_LIBTOOL_VERSION=3:0:0
AC_SUBST(RASQAL_LIBTOOL_VERSION)


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
RASQAL_VERSION_MAJOR=$version_major
RASQAL_VERSION_MINOR=$version_minor
RASQAL_VERSION_RELEASE=$version_release
RASQAL_VERSION_DECIMAL=$version_decimal
AC_SUBST(RASQAL_VERSION)
AC_SUBST(RASQAL_VERSION_MAJOR)
AC_SUBST(RASQAL_VERSION_MINOR)
AC_SUBST(RASQAL_VERSION_RELEASE)
AC_SUBST(RASQAL_VERSION_DECIMAL)


dnl Checks for library functions.
AC_CHECK_FUNCS(getopt getopt_long stricmp strcasecmp vsnprintf initstate_r initstate random_r random gmtime_r rand_r rand srand timegm gettimeofday)

AM_CONDITIONAL(STRCASECMP, test $ac_cv_func_stricmp = no -a $ac_cv_func_strcasecmp = no)
AM_CONDITIONAL(GETOPT, test $ac_cv_func_getopt = no -a $ac_cv_func_getopt_long = no)
AM_CONDITIONAL(TIMEGM, test $ac_cv_func_timegm = no)
dnl Windows only version
AM_CONDITIONAL(GETTIMEOFDAY, test $ac_cv_func_gettimeofday = no)


AC_MSG_CHECKING(whether need to declare optind)
AC_LINK_IFELSE([AC_LANG_PROGRAM([[#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
]], [[int x=optind;]])],[AC_MSG_RESULT(no)],[AC_DEFINE(NEED_OPTIND_DECLARATION, 1, need 'extern int optind' declaration?)
            AC_MSG_RESULT(yes)])


AC_MSG_CHECKING(whether struct tm has tm_gmtoff field)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
]], [[struct tm my_time; my_time.tm_gmtoff = 0;]])],[have_tm_gmtoff="yes"],[have_tm_gmtoff="no"])
AC_MSG_RESULT($have_tm_gmtoff)
if test $have_tm_gmtoff = "yes" ; then
  AC_DEFINE(HAVE_TM_GMTOFF, 1, [struct tm has tm_gmtoff field])
fi


# raptor V2 is REQUIRED; --enable-raptor2 option was removed
RAPTOR_MIN_VERSION=2.0.7


PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES([RAPTOR2],[raptor2],[
  RAPTOR_VERSION=`$PKG_CONFIG raptor2 --modversion 2>/dev/null`
  raptor_too_old=0
  AS_VERSION_COMPARE([$RAPTOR_VERSION], $RAPTOR_MIN_VERSION, raptor_too_old="1")
  if test $raptor_too_old = 1; then
    AC_MSG_ERROR(Raptor $RAPTOR_VERSION is too old - see http://librdf.org/raptor/ to get a $RAPTOR_MIN_VERSION or newer)
  fi
],[
  AC_MSG_ERROR(Raptor is not installed - see http://librdf.org/raptor/ to get $RAPTOR_MIN_VERSION or newer)
])
AC_SUBST(RAPTOR2_CFLAGS)
AC_SUBST(RAPTOR2_LIBS)

RAPTOR_VERSION_DEC=`echo $RAPTOR_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
AC_SUBST(RAPTOR_VERSION_DEC)
AC_SUBST(RAPTOR_MIN_VERSION)

AM_CONDITIONAL(RASQAL_SORT, test $RAPTOR_VERSION_DEC '<' 20015)

dnl Checks for regex libraries
have_regex_pcre=0
have_regex_posix=0
need_regex_pcre=0
need_regex_posix=0

AC_ARG_ENABLE(pcre,
	[  --disable-pcre                  Disable PCRE support (default auto)],
	[enable_pcre=$enableval], [enable_pcre="auto"])

if test "x$enable_pcre" != "xno" ; then

  AC_ARG_WITH(pcre-config, [  --with-pcre-config=PATH     Location of PCRE pcre-config (auto)], pcre_config="$withval", pcre_config="")

  if test "X$pcre_config" != "Xno" ; then
    if test "X$pcre_config" != "X" ; then
      AC_MSG_CHECKING(for $pcre_config)

      if test -x $pcre_config ; then
	PCRE_CONFIG=$pcre_config
	AC_MSG_RESULT(yes)
      else
	AC_MSG_ERROR([pcre-config not found at specified path $pcre_config])
      fi
    fi
    if test "X$PCRE_CONFIG" = "X" ; then
      AC_CHECK_PROGS(PCRE_CONFIG, pcre-config)
    fi
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
  elif test "x$enable_pcre" = "xyes"; then
    AC_MSG_ERROR(PCRE requested but not found)
  fi

fi


AC_MSG_CHECKING(for posix regex library)
oLIBS="$LIBS"
if test $ac_cv_header_regex_h = yes; then
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
              #include <regex.h>]], [[regex_t reg; char *pat="abc"; regcomp(&reg, pat, 0);]])],[worked=yes],[worked=no])
  if test $worked = yes; then
    have_regex_posix=1
  fi
fi

LIBS="$oLIBS"

if test $have_regex_posix = 1; then
  AC_MSG_RESULT(yes)
  AC_DEFINE(HAVE_REGEX_POSIX, 1, [have POSIX regex library])
else
  AC_MSG_RESULT(no)
fi



AC_ARG_WITH(regex-library, [  --with-regex-library=NAME   Use regex library - posix, pcre (auto)], regex_library="$withval", regex_library="") 

for regex_library_name in $regex_library pcre posix; do
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
        AC_DEFINE(RASQAL_REGEX_POSIX, 1, [Use POSIX regex library])
        break
      fi
      ;;

    *)
      AC_MSG_ERROR(No such regex library $regex_library_name)
      ;;
esac
done



AC_MSG_CHECKING(regex library to use)
regex_library=
if test $need_regex_pcre = 1; then
  regex_library=pcre
elif test $need_regex_posix = 1; then
  regex_library=posix
else
  regex_library=none
  AC_DEFINE(RASQAL_REGEX_NONE, 1, [Use no regex library])
  AC_MSG_WARN(No regex library available - please install pcre or a POSIX regex library)
fi
AC_MSG_RESULT($regex_library)


dnl Checks for message digest libraries
have_digest_mhash=no
have_digest_gcrypt=no
dnl always available
have_digest_internal=yes
need_digest_mhash=no
need_digest_gcrypt=no
need_digest_internal=no

AC_ARG_WITH(mhash,
        [  --with-mhash=DIR  mhash installation prefix (no default)],
	[mhash_dir=$withval], [mhash_dir=""])

oCPPFLAGS="$CPPFLAGS"
oLIBS="$LIBS"
if test "X$mhash_dir" != "X" ; then
  CPPFLAGS="$CPPFLAGS -I$mhash_dir/include"
  LIBS="$LIBS -L$mhash_dir/lib"
fi
LIBS="$LIBS -lmhash"

AC_CHECK_HEADERS(mhash.h)

AC_MSG_CHECKING(for mhash)

AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#include <mhash.h>]], [[ MHASH m; mhash_init(MHASH_MD5); ]])],[have_digest_mhash=yes],[have_digest_mhash=no])

AC_MSG_RESULT($have_digest_mhash)

CPPFLAGS="$oCPPFLAGS"
LIBS="$oLIBS"


AC_ARG_WITH(libgcrypt-config, [  --with-libgcrypt-config=PATH     Location of GCRYPT libgcrypt-config (auto)], libgcrypt_config="$withval", libgcrypt_config="")
if test "X$libgcrypt_config" != "Xno" ; then
  if test "X$libgcrypt_config" != "X" ; then
    AC_MSG_CHECKING(for $libgcrypt_config)

    if test -x $libgcrypt_config ; then
	LIBGCRYPT_CONFIG=$libgcrypt_config
	AC_MSG_RESULT(yes)
    else
	AC_MSG_ERROR([libgcrypt-config not found at specified path $libgcrypt_config])
    fi
  fi
  if test "X$LIBGCRYPT_CONFIG" = "X" ; then
    AC_CHECK_PROGS(LIBGCRYPT_CONFIG, libgcrypt-config)
  fi
fi

AC_CHECK_HEADERS(gcrypt.h)

AC_MSG_CHECKING(for gcrypt)
GCRYPT_VERSION=`$LIBGCRYPT_CONFIG --version 2>/dev/null`
if test "X$GCRYPT_VERSION" = X; then
  have_digest_gcrypt=no
elif test "$ac_cv_header_gcrypt_h" = "yes"; then
  oCPPFLAGS="$CPPFLAGS"
  oLIBS="$LIBS"
  CPPFLAGS="$CPPFLAGS `$LIBGCRYPT_CONFIG --cflags`"
  LIBS="$LIBS `$LIBGCRYPT_CONFIG --libs`"

  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <gcrypt.h>]], [[ gcry_md_hd_t hash; gcry_md_open(hash, GCRY_MD_MD5, 0); ]])],[have_digest_gcrypt=yes],[have_digest_gcrypt=no])

  CPPFLAGS="$oCPPFLAGS"
  LIBS="$oLIBS"
else
  have_digest_gcrypt=no
fi
AC_MSG_RESULT($have_digest_gcrypt)


AC_ARG_WITH(digest-library, [  --with-digest-library=NAME   Use message digest library - mhash, gcrypt, internal (auto)], digest_library="$withval", digest_library="")

for digest_library_name in $digest_library mhash gcrypt internal; do
  case $digest_library_name in
    mhash)
      if test $have_digest_mhash = yes; then
        need_digest_mhash=yes
        AC_DEFINE(RASQAL_DIGEST_MHASH, 1, [Use mhash library for message digests])
        break
      fi
      ;;

    gcrypt)
      if test $have_digest_gcrypt = yes; then
        need_digest_gcrypt=yes
        AC_DEFINE(RASQAL_DIGEST_GCRYPT, 1, [Use gcrypt library for message digests])
        break
      fi
      ;;

    internal)
      need_digest_internal=yes
      AC_DEFINE(RASQAL_DIGEST_INTERNAL, 1, [Use internal routines for message digests])
      break
      ;;

    *)
      AC_MSG_ERROR(No such message digest library $digest_library_name)
      ;;
esac
done

AC_MSG_CHECKING(message digest library to use)
digest_library=
if test $need_digest_mhash = yes; then
  digest_library=mhash
elif test $need_digest_gcrypt = yes; then
  digest_library=gcrypt
elif test $need_digest_internal = yes; then
  digest_library=internal
else
  digest_library=none
  AC_DEFINE(RASQAL_DIGEST_NONE, 1, [Use no message digest library])
  AC_MSG_WARN(No message digest library selected - no message digest support will work)
fi
AC_MSG_RESULT($digest_library)

AM_CONDITIONAL(RASQAL_DIGEST_INTERNAL, test $digest_library = internal)


dnl Check for uuid libraries
have_uuid_libuuid=no
have_uuid_libc=no
have_uuid_ossp=no
need_uuid_libuuid=no
need_uuid_libc=no
need_uuid_internal=no
need_uuid_ossp=no

PKG_CHECK_MODULES(UUID, uuid, [
  LIBUUID_VERSION=`$PKG_CONFIG uuid --modversion 2>/dev/null`

  oCPPFLAGS="$CPPFLAGS"
  oLIBS="$LIBS"

  CPPFLAGS="$CPPFLAGS `$PKG_CONFIG uuid --cflags`"
  LIBS="$LIBS `$PKG_CONFIG uuid --libs`"

  dnl check for OSSP uuid defines and error out so we only get linux libuuid
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <uuid.h>]], [[
  #if defined(UUID_LEN_BIN) && defined(UUID_VERSION)
  FAIL
  #endif
  uuid_t data; uuid_clear(&data); ]])],[have_uuid_libuuid=yes],[have_uuid_libuuid=no])

  CPPFLAGS="$oCPPFLAGS"
  LIBS="$oLIBS"
], [have_uuid_libuuid=no])
AC_MSG_CHECKING(for libuuid)
AC_MSG_RESULT($have_uuid_libuuid)

AC_CHECK_PROGS(UUID_CONFIG, uuid-config)
AC_MSG_CHECKING(for ossp libuuid)
if test "X$UUID_CONFIG" != X; then
  OSSP_UUID_VERSION=`$UUID_CONFIG --version 2>/dev/null`

  oCPPFLAGS="$CPPFLAGS"
  oLIBS="$LIBS"

  CPPFLAGS="$CPPFLAGS `$UUID_CONFIG --cflags`"
  LIBS="$LIBS `$UUID_CONFIG --ldflags --libs`"

  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <uuid.h>]], [[ uuid_t *data; uuid_create(&data); ]])],[have_uuid_ossp=yes],[have_uuid_ossp=no])

  CPPFLAGS="$oCPPFLAGS"
  LIBS="$oLIBS"
fi
AC_MSG_RESULT($have_uuid_ossp)

AC_CHECK_HEADERS(uuid/uuid.h)
AC_MSG_CHECKING(for UUID in libc)
if test "$ac_cv_header_uuid_uuid_h" = "yes"; then
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[
  #include <uuid/uuid.h>]], [[ uuid_t data; uuid_clear(&data); ]])],[have_uuid_libc=yes],[have_uuid_libc=no])
fi
AC_MSG_RESULT($have_uuid_libc)


AC_ARG_WITH(uuid-library, [  --with-uuid-library=NAME   Use UUID library - libuuid, libc (auto)], uuid_library="$withval", uuid_library="")

for uuid_library_name in $uuid_library libuuid ossp libc internal; do
  case $uuid_library_name in
    libuuid)
      if test $have_uuid_libuuid = yes; then
        need_uuid_libuuid=yes
        AC_DEFINE(RASQAL_UUID_LIBUUID, 1, [Use libuuid library for UUID])
        break
      fi
      ;;

    ossp)
      if test $have_uuid_ossp = yes; then
        need_uuid_ossp=yes
        AC_DEFINE(RASQAL_UUID_OSSP, 1, [Use OSSP library for UUID])
        break
      fi
      ;;

    libc)
      if test $have_uuid_libc = yes; then
        need_uuid_libc=yes
        AC_DEFINE(RASQAL_UUID_LIBC, 1, [Use UUID in libc library for UUID])
        break
      fi
      ;;

    internal)
      need_uuid_internal=yes
      AC_DEFINE(RASQAL_UUID_INTERNAL, 1, [Use internal routines for UUID])
      break
      ;;

    *)
      AC_MSG_ERROR(No such UUID library $uuid_library_name)
      ;;
esac
done

AC_MSG_CHECKING(UUID library to use)
uuid_library=
if test $need_uuid_libuuid = yes; then
  uuid_library="libuuid $LIBUUID_VERSION"
elif test $need_uuid_ossp = yes; then
  uuid_library="ossp $OSSP_UUID_VERSION"
elif test $need_uuid_libc = yes; then
  uuid_library=libc
elif test $need_uuid_internal = yes; then
  uuid_library=internal
else
  uuid_library=none
  AC_DEFINE(RASQAL_UUID_NONE, 1, [Use no UUID library])
  AC_MSG_WARN(No UUID library selected - no UUID support will work)
fi
AC_MSG_RESULT($uuid_library)



have_libxml=0
need_libxml=0

AC_ARG_ENABLE(xml2,
	[  --disable-xml2                  Disable XML2 support (default auto)],
	[enable_xml2=$enableval], [enable_xml2="auto"])

if test "x$enable_xml2" != "xno" ; then

  AC_ARG_WITH(xml2-config, [  --with-xml2-config=PATH     Location of libxml xml2-config ()], xml2_config="$withval", xml2_config="")

  if test "X$xml2_config" != "X" ; then
    AC_MSG_CHECKING(for $xml2_config)

    if test -x $xml2_config ; then
      XML_CONFIG=$xml2_config
      AC_MSG_RESULT(yes)
    else
      AC_MSG_ERROR([xml2-config not found at specified path $xml2_config])
    fi
  fi
  if test "X$XML_CONFIG" = "X"; then
    AC_CHECK_PROGS(XML_CONFIG, xml2-config)
  fi


  LIBXML_MIN_VERSION=2.6.8
  AC_MSG_CHECKING(for libxml2 library)
  oCPPFLAGS="$CPPFLAGS"
  oLIBS="$LIBS"
  if test "X$XML_CONFIG" != X; then
    LIBS="$LIBS `$XML_CONFIG --libs`"
    CPPFLAGS="`$XML_CONFIG --cflags` $CPPFLAGS"
    LIBXML_VERSION=`$XML_CONFIG --version`
    libxml_version_dec=`echo $LIBXML_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
    LIBXML_MIN_VERSION_dec=`echo $LIBXML_MIN_VERSION | $AWK -F. '{printf("%d\n", 10000*$1 + 100*$2 + $3)};'`
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

  if test $have_libxml = 0 && test "x$enable_xml2" = "xyes" ; then
    AC_MSG_ERROR(libxml2 requested but not found)
  fi

fi


dnl RDF Query Languages
sparql_query_language=no
laqrs_query_language=no

rdf_query_languages_available="sparql laqrs"
rdf_query_languages_enabled=


# This is needed because autoheader can't work out which computed
# symbols must be pulled from acconfig.h into config.h.in
if test "x" = "y"; then
  AC_DEFINE(RASQAL_QUERY_LAQRS, 1, [Building LAQRS query])
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

AM_CONDITIONAL(RASQAL_QUERY_LAQRS, test $laqrs_query_language = yes)
AM_CONDITIONAL(RASQAL_QUERY_SPARQL, test $sparql_query_language = yes)



RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $RAPTOR2_CFLAGS"
RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $RAPTOR2_LIBS"


if test $need_regex_pcre = 1; then
  C=`$PCRE_CONFIG --cflags`
  L=`$PCRE_CONFIG --libs`
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $C"
  PKGCONFIG_LIBS="$PKGCONFIG_LIBS $L"
  unset C
  unset L
fi


if test $need_digest_mhash = yes; then
  C=""
  L="-lmhash"
  if test "X$mhash_dir" != "X" ; then
    C="-I$mhash_dir/include"
    L="-L$mhash_dir/lib $L"
  fi

  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  unset C
  unset L
fi


if test $need_digest_gcrypt = yes; then
  C=`$LIBGCRYPT_CONFIG --cflags`
  L=`$LIBGCRYPT_CONFIG --libs`
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  unset C
  unset L
fi


if test $need_uuid_libuuid = yes; then
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $UUID_CFLAGS"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $UUID_LIBS"

  PKGCONFIG_CFLAGS="$PKGCONFIG_CFLAGS $UUID_CFLAGS"
  PKGCONFIG_LIBS="$PKGCONFIG_LIBS $UUID_LIBS"
fi


if test $need_uuid_ossp = yes; then
  C=`$UUID_CONFIG --cflags`
  L=`$UUID_CONFIG --ldflags --libs`
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $C"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $L"

  unset C
  unset L
fi


gmp_lib_dir=
gmp_include_dir=
AC_ARG_WITH(gmp, [  --with-gmp=DIR          GMP install area], gmp_prefix="$withval", gmp_prefix="none") 
if test "X$gmp_prefix" != "Xnone" ; then
  gmp_libs="-L$gmp_prefix/lib"
  gmp_includes="-I$gmp_prefix/include"
fi

mpfr_lib_dir=
mpfr_include_dir=
AC_ARG_WITH(mpfr, [  --with-mpfr=DIR          MPFR install area], mpfr_prefix="$withval", mpfr_prefix="none") 
if test "X$mpfr_prefix" != "Xnone" ; then
  mpfr_libs="-L$mpfr_prefix/lib"
  mpfr_includes="-I$mpfr_prefix/include"
fi

oCPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $gmp_includes"
AC_CHECK_HEADERS(gmp.h)
CPPFLAGS="$oCPPFLAGS"

CPPFLAGS="$CPPFLAGS $mpfr_includes"
AC_CHECK_HEADERS([mpfr.h], [], [], [
#ifdef HAVE_GMP_H
#include <gmp.h>
#endif
])
CPPFLAGS="$oCPPFLAGS"


have_mpfr=0
have_gmp=0

oCPPFLAGS="$CPPFLAGS"
oLIBS="$LIBS"
if test "$ac_cv_header_mpfr_h" = yes; then
  CPPFLAGS="$CPPFLAGS $mpfr_includes"
  LIBS="$LIBS $mpfr_libs -lmpfr"
  AC_CHECK_FUNC(mpfr_init, have_mpfr_init=yes, have_mpfr_init=no)
  # Several functions are used that are not in GMP version of MPFR:
  #   mpfr_get_default_rounding_mode mpfr_equal_p mpfr_free_str
  #     MPFR 2.1.0 released 3 Nov 2004 http://www.mpfr.org/mpfr-2.1.0/
  #   mpfr_snprintf
  #     MPFR 2.4.0 released 26 January 2009 http://www.mpfr.org/mpfr-2.4.0/
  # so just test for the latter.
  AC_CHECK_FUNC(mpfr_snprintf, have_mpfr_snprintf=yes, have_mpfr_snprintf=no)
  LIBS="$oLIBS"
  CPPFLAGS="$oCPPFLAGS"
  AC_MSG_CHECKING(for working MPFR library)
  if test $have_mpfr_init = yes -a $have_mpfr_snprintf = yes; then
    have_mpfr=1
    AC_MSG_RESULT(yes)
  else
    AC_MSG_RESULT(no)
  fi
fi
CPPFLAGS="$oCPPFLAGS"
LIBS="$oLIBS"

have_gmp_version=0
have_gmp_random=no
if test "$ac_cv_header_gmp_h" = yes; then
  CPPFLAGS="$CPPFLAGS $gmp_includes"
  LIBS="$LIBS $gmp_libs -lgmp"
  AC_MSG_CHECKING(for working GNU MP library)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gmp.h>]], [[mpz_t n; mpz_init(n);]])],[have_gmp=1
	      AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
  AC_MSG_CHECKING(for GNU MP library random support)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gmp.h>]], [[
gmp_randstate_t r;
unsigned int ui = 12345;
mpz_t iresult;
mpz_t n;
mpf_t fresult;
 
gmp_randinit_default(r);

gmp_randseed_ui(r, ui);
mpz_init(iresult);
mpz_init_set_ui(n, 12345);
mpz_urandomm(iresult, r, n);
mpz_clear(iresult);

mpf_urandomb(fresult, r, 5);
mpf_clear(fresult);
mpz_clear(n);

gmp_randclear(r);
]])],[have_gmp_random=yes
	      AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
  AC_MSG_CHECKING(for gmp_version)
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <gmp.h>]], [[const char* v=gmp_version; ]])],[have_gmp_version=1
	      AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)])
fi
CPPFLAGS="$oCPPFLAGS"
LIBS="$oLIBS"

if test $have_gmp_version = 1; then
  AC_DEFINE(HAVE_GMP_VERSION, 1, [Have gmp_version constant])
fi

need_mpfr=0
need_gmp=0

AC_ARG_WITH(decimal, [  --with-decimal=NAME           Use decimal library - mpfr (default), gmp, none], decimal="$withval", decimal="mpfr") 
for decimal_name in $decimal mpfr gmp none; do
  case $decimal_name in
    mpfr)
      if test $have_mpfr = 1; then
        need_mpfr=1
        AC_DEFINE([RASQAL_DECIMAL_MPFR], 1, [Decimal from MPFR library])
        break
      fi
      ;;

    gmp)
      if test $have_gmp = 1; then
        need_gmp=1
	AC_DEFINE([RASQAL_DECIMAL_GMP], 1, [Decimal from GNU MP library])
        break
      fi
      ;;

    none)
      need_mpfr=0
      need_gmp=0
      AC_DEFINE([RASQAL_DECIMAL_NONE], 1, [Decimal without a library])
      break
      ;;

    *)
      AC_MSG_ERROR(No such decimal library $decimal_name)
      ;;
esac
done


AC_MSG_CHECKING(decimal libraries available)
decimal_libraries_available=
if test $have_mpfr = 1; then
  decimal_libraries_available="$decimal_libraries_available mpfr"
fi
if test $have_gmp = 1; then
  decimal_libraries_available="$decimal_libraries_available gmp"
fi
AC_MSG_RESULT($decimal_libraries_available)


need_libm=0
AC_MSG_CHECKING(decimal library to use)
decimal_library=
if test $need_mpfr = 1; then
  decimal_library="mpfr $MPFR_VERSION"
elif test $need_gmp = 1; then
  decimal_library="gmp $GMP_VERSION"
else
  decimal_library=none
fi
AC_MSG_RESULT($decimal_library)



dnl Random generator
random_approaches="mtwist"
if test "$have_gmp_random" = yes; then
  random_approaches="$random_approaches gmp"
fi
random_approaches="$random_approaches random_r rand_r random rand"

AC_ARG_WITH(random-approach, [  --with-random-approach=NAME   Use random approach - mtwist, random_r, rand_r, random, rand (auto)], random_with="$withval", random_with="")

AC_MSG_CHECKING(random approach) 
random_approach=none
need_mtwist=no
for random_algo in $random_with $random_approaches; do
  case $random_algo in
    mtwist)
      random_approach="Internal Mersenne Twister"
      need_mtwist=yes
      AC_DEFINE(RANDOM_ALGO_MTWIST, 1, [Use internal Mersenne Twister])
      break
      ;;
      
    gmp)
      if test "$have_gmp_random" = yes; then
        random_approach="GMP gmp_randseed_ui and mpz_urandomm()"
	AC_DEFINE(RANDOM_ALGO_GMP_RAND, 1, [Use GMP gmp_randseed_ui() and mpz_urandomm()])
        break
      fi
      ;;
      
    random_r)
      if test "$ac_cv_func_initstate_r" = "yes" -a "$ac_cv_func_random_r" = "yes"; then
        random_approach="glibc initstate_r() and random_r()"
	AC_DEFINE(RANDOM_ALGO_RANDOM_R, 1, [Use glibc initstate_r() and random_r()])
        break
      fi
      ;;

    rand_r)
      if test "$ac_cv_func_rand_r" = "yes"; then
        random_approach="POSIX rand_r()"
	AC_DEFINE(RANDOM_ALGO_RAND_R, 1, [Use POSIX rand_r()])
        break
      fi
      ;;

    random)
      if test "$ac_cv_func_initstate" = "yes" -a "$ac_cv_func_random" = "yes"; then
	 random_approach="BSD initstate() and random()"
         AC_DEFINE(RANDOM_ALGO_RANDOM, 1, [Use BSD initstate() and random()])
        break
      fi
      ;;

    rand)
      if test "$ac_cv_func_srand" = "yes" -a "$ac_cv_func_rand" = "yes"; then
          random_approach="POSIX srand() and rand()"
          AC_DEFINE(RANDOM_ALGO_RAND, 1, [Use POSIX srand() and rand()])
          break
      fi
      ;;

    *)
      ;;
esac
done

if test "$random_approach" = none; then
    AC_MSG_ERROR(No random number generator approach. POSIX rand should always be available)
fi
AC_MSG_RESULT($random_approach)

AM_CONDITIONAL(RASQAL_MTWIST, test $need_mtwist = yes)




dnl Always need ceil, floor and round in rasqal_literal_ceil() etc.
AC_MSG_CHECKING([for ceil, floor and round in default libs])
ceil_lib=none
AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
]], [[
extern char **environ;
double t = strtod(environ[0], NULL);
return (int)(ceil(t) * floor(t) * round(t));
]])],
   AC_MSG_RESULT(yes)
   ceil_lib=libc,
   AC_MSG_RESULT(no),
   ceil_lib=cross)
if test $ceil_lib = none; then
  LIBS="$LIBS -lm"
  AC_MSG_CHECKING([for ceil, floor and round with -lm])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([[
#ifdef HAVE_MATH_H
#include <math.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
]], [[
extern char **environ;
double t = strtod(environ[0], NULL);
return (int)(ceil(t) * floor(t) * round(t));
]])],
     AC_MSG_RESULT(yes)
     need_libm=1
     ceil_lib=libm,
     AC_MSG_RESULT(no),
     ceil_lib=cross)
  LIBS="$oLIBS"
fi
if test $ceil_lib = cross; then
  AC_MSG_WARN([Assuming ceil, floor and round are in default libs for cross compiling])
  ceil_lib=libc
elif test $ceil_lib = none; then
  AC_MSG_ERROR([Could not find ceil, floor, round in default libs or with -lm])
fi
if test $need_libm = 1; then
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS -lm"
fi


DECIMAL_INCLUDES=
DECIMAL_LIBS=
if test $need_mpfr = 1; then
  DECIMAL_INCLUDES="$mpfr_includes"
  DECIMAL_LIBS="$mpfr_libs -lmpfr"
fi
if test $need_gmp = 1; then
  DECIMAL_INCLUDES="$gmp_includes"
  DECIMAL_LIBS="$gmp_libs -lgmp"
fi
RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS $DECIMAL_INCLUDES"
RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS $DECIMAL_LIBS"


if test $need_libxml = 1; then
  RASQAL_INTERNAL_CPPFLAGS="$RASQAL_INTERNAL_CPPFLAGS `$XML_CONFIG --cflags`"
  RASQAL_EXTERNAL_LIBS="$RASQAL_EXTERNAL_LIBS `$XML_CONFIG --libs`"
  if test "X$PKGCONFIG_REQUIRES" != X; then
    PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES,"
  fi
  PKGCONFIG_REQUIRES="$PKGCONFIG_REQUIRES libxml-2.0 >= $LIBXML_MIN_VERSION"
fi


# Make final changes to cflags
MEM=
MEM_LIBS=
CPPFLAGS="-DRASQAL_INTERNAL=1 $CPPFLAGS"

debug_messages=no

AC_ARG_ENABLE(debug, [  --enable-debug          Enable debug messages (default no).  ], debug_messages=$enableval)
if test "$debug_messages" = "yes"; then
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
  CPPFLAGS="-DMAINTAINER_MODE $MAINTAINER_CPPFLAGS $CPPFLAGS"
fi

AC_SUBST(RASQAL_EXTERNAL_LIBS)

AC_SUBST(RASQAL_INTERNAL_CPPFLAGS)
AC_SUBST(RASQAL_INTERNAL_LIBS)

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)
AC_SUBST(STANDARD_CFLAGS)

AC_SUBST(RECHO)
AC_SUBST(RECHO_N)
AC_SUBST(RECHO_C)

RASQAL_LIBTOOLLIBS=librasqal.la
AC_SUBST(RASQAL_LIBTOOLLIBS)

AC_SUBST(PKGCONFIG_REQUIRES)
AC_SUBST(PKGCONFIG_LIBS)
AC_SUBST(PKGCONFIG_CFLAGS)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

dnl automake 1.11
AM_SILENT_RULES([no])

AC_CONFIG_FILES([Makefile
data/Makefile
docs/Makefile
docs/version.xml
scripts/Makefile
libsv/Makefile
libmtwist/Makefile
getopt/Makefile
src/Makefile
src/rasqal.h
src/win32_rasqal_config.h
tests/Makefile
tests/algebra/Makefile
tests/engine/Makefile
tests/laqrs/Makefile
tests/laqrs/syntax/Makefile
tests/sparql/Makefile
tests/sparql/bugs/Makefile
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
tests/sparql/SyntaxDev/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL2/Makefile
tests/sparql/SyntaxDev/Syntax-SPARQL3/Makefile
tests/sparql/update/Makefile
tests/sparql/aggregate/Makefile
tests/sparql/sparql11/Makefile
tests/sparql/federated/Makefile
tests/sparql/warnings/Makefile
utils/Makefile
win32/Makefile
rasqal.spec
rasqal.rdf
rasqal.pc])
AC_CONFIG_FILES([src/rasqal-config], [chmod +x src/rasqal-config])
AC_CONFIG_FILES([rasqal-src-config], [chmod +x rasqal-src-config])

dnl Check for gtk-doc and docbook
GTK_DOC_CHECK([1.3])

AC_OUTPUT

AC_MSG_RESULT([
Rasqal build summary:
  RDF query languages available : $rdf_query_languages_available
  RDF query languages enabled   :$rdf_query_languages_enabled
  Raptor version                : $RAPTOR_VERSION
  Decimal library               : $decimal_library
  Regex library                 : $regex_library
  Message digest library        : $digest_library
  UUID library                  : $uuid_library
  Random approach               : $random_approach
  ceil, floor, round source     : $ceil_lib
])
