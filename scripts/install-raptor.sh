#!/bin/sh
# Helper script to install a particula raptor version for travis CI

set -x

MIN_VERSION=$1
INSTALL_VERSION=$2

if pkg-config --atleast-version $MIN_VERSION raptor2; then
  cd /tmp
  wget http://download.librdf.org/source/raptor2-$INSTALL_VERSION.tar.gz
  tar -x -z -f raptor-$INSTALL_VERSION.tar.gz
  cd raptor-$INSTALL_VERSION && ./configure --prefix=/usr && make && sudo make install
  cd ..
  rm -rf raptor-$INSTALL_VERSION
fi
