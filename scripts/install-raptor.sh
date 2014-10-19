#!/bin/sh
# Helper script to install a particula raptor version for travis CI

set -x

PACKAGE=raptor2
MIN_VERSION=$1
INSTALL_VERSION=$2

raptor_version=`pkg-config --modversion $PACKAGE`
if pkg-config --atleast-version $MIN_VERSION $PACKAGE; then
  echo "Raptor2 $raptor_version is new enough"
else
  cd /tmp
  wget http://download.librdf.org/source/$PACKAGE-$INSTALL_VERSION.tar.gz
  tar -x -z -f $PACKAGE-$INSTALL_VERSION.tar.gz
  cd $PACKAGE-$INSTALL_VERSION && ./configure --prefix=/usr && make && sudo make install
  cd ..
  rm -rf raptor-$INSTALL_VERSION
fi
