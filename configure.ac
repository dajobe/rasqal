dnl -*- Mode: sh -*-
dnl
dnl configure.ac - autoconf file for rasqal
dnl (Process this file with autoconf to produce a configure script.)
dnl
dnl $Id$
dnl
dnl Copyright (C) 2003 David Beckett - http://purl.org/net/dajobe/
dnl Institute for Learning and Research Technology - http://www.ilrt.org/
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


AC_INIT(RDF Query Library, 1.0, redland-dev@lists.librdf.org, rasqal)
AC_PREREQ(2.50)
AC_CONFIG_SRCDIR(rdql_parser.y)
AC_REVISION($Revision$)

AM_INIT_AUTOMAKE(1.6)
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


AC_PROG_LEX
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



dnl compiler checks
# if using gcc...
if test "$ac_cv_prog_gcc" = yes; then
  STANDARD_CFLAGS=
  MAINTAINER_CFLAGS="-Wall -Wshadow -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls"
else
  STANDARD_CFLAGS=
  MAINTAINER_CFLAGS=
fi



dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(errno.h stdlib.h unistd.h string.h dmalloc.h getopt.h)


dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_BIGENDIAN

dnl need to change quotes to allow square brackets
changequote(<<, >>)dnl
version_major=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\1/'`
version_minor=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\2/'`
version_release=`echo $VERSION | sed -e 's/^\([^\.]*\)\.\([^\.]*\)\.\(.*\)$/\3/'`
changequote([, ])dnl

AC_DEFINE_UNQUOTED(RASQAL_VERSION_MAJOR, $version_major, [Major version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_MINOR, $version_minor, [Minor version number])
AC_DEFINE_UNQUOTED(RASQAL_VERSION_RELEASE, $version_release, [Release version number])

# syntax: CURRENT[:REVISION[:AGE]]
RASQAL_LIBTOOL_VERSION=0:0:0
AC_SUBST(RASQAL_LIBTOOL_VERSION)


#dnl Checks for library functions.
#AC_CHECK_FUNCS(getopt getopt_long vsnprintf)
#
## was AC_REPLACE_FUNCS(strcasecmp) but strcasecmp has two alternatives:
## the other non-standard function stricmp or a local alternative
#if test $ac_cv_func_stricmp = no -a $ac_cv_func_stricmp = no; then
#  AC_LIBOBJ([strcasecmp])
#fi

#RAPPER_EXTRA_OBJS=
#if test $ac_cv_func_getopt = no -a $ac_cv_func_getopt_long = no; then
#  RAPPER_EXTRA_OBJS='getopt.$(OBJEXT)'
#  AC_SUBST([RAPPER_EXTRA_OBJS])
#fi


#AC_MSG_CHECKING(whether need to declare optind)
#AC_TRY_LINK([#ifdef HAVE_GETOPT_H
##include <getopt.h>
##endif], [int x=optind;],
#            AC_MSG_RESULT(no),
#            AC_DEFINE(NEED_OPTIND_DECLARATION, 1, [need 'extern int optind' declaration?])
#            AC_MSG_RESULT(yes))



#if test $ac_cv_func_vsnprintf = yes; then
#  AC_MSG_CHECKING(vsnprintf has C99 compatible return value)
#  AC_TRY_RUN([#include <stdarg.h>
#int is_c99(char *s, ...) {
#  char buffer[32];
#  va_list args;
#  int r;
#  va_start(args, s);
#  r = vsnprintf(buffer, 5, s, args);
#  va_end(args);
#
#  return (r == 7);
#}
#
#int main(int argc, char* argv) {
#  return is_c99("1234567");
#}], AC_MSG_RESULT(no),
#    AC_DEFINE(HAVE_C99_VSNPRINTF, 1, [vsnprint has C99 compatible return value])
#    AC_MSG_RESULT(yes))
#fi


AC_MSG_CHECKING(for redland sources in parent directory)
if test -d $srcdir/../librdf -a -r $srcdir/../librdf/rdf_node.c ; then
  have_redland=1
  AC_MSG_RESULT(yes)
  AC_DEFINE(RASQAL_IN_REDLAND, 1, [is this being built inside redland?])
  REDLAND_LIBS="\$(top_builddir)/../librdf/librdf.la"
  REDLAND_CFLAGS=" -I\$(srcdir)/../librdf"
  AC_SUBST(REDLAND_LIBS)
  AC_SUBST(REDLAND_CFLAGS)
else
  AC_MSG_RESULT(no)
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

AC_SUBST(MEM)
AC_SUBST(MEM_LIBS)
AC_SUBST(STANDARD_CFLAGS)

ECHO_N="$ECHO_N"
ECHO_C="$ECHO_C"
AC_SUBST(ECHO_N)
AC_SUBST(ECHO_C)

abs_top_srcdir=`cd $srcdir; pwd`
AC_SUBST(abs_top_srcdir)
abs_top_builddir=`pwd`
AC_SUBST(abs_top_builddir)

AC_CONFIG_FILES([Makefile
rasqal.spec
rasqal.pc])
AC_CONFIG_FILES([rasqal-config], [chmod +x rasqal-config])
AC_CONFIG_FILES([rasqal-src-config], [chmod +x rasqal-src-config])

AC_OUTPUT
