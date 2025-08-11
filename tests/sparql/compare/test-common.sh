#!/bin/sh
# Common test environment for Rasqal SPARQL Compare tests
# This script sets up the test environment and common variables

# Exit on any error
set -e

# Set up paths
: ${srcdir:=.}
: ${builddir:=.}
: ${top_srcdir:=../..}
: ${top_builddir:=../..}

# Set the rasqal-compare path
: ${RASQAL_COMPARE:="${top_builddir}/utils/rasqal-compare"}

# Check if rasqal-compare exists
if [ ! -x "$RASQAL_COMPARE" ]; then
  # Automake: exit code 77 means SKIP
  exit 77
fi

# Export for use in tests
export RASQAL_COMPARE
export srcdir
export builddir
export top_srcdir
export top_builddir
