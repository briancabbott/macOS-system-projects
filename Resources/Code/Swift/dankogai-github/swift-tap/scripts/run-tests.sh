#!/bin/bash

set -ev

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
    sudo apt-get update
    sudo apt-get -y install \
          binutils \
          git \
          gnupg2 \
          libc6-dev \
          libcurl4 \
          libedit2 \
          libgcc-9-dev \
          libpython2.7 \
          libsqlite3-0 \
          libstdc++-9-dev \
          libxml2 \
          libz3-dev \
          pkg-config \
          tzdata \
          zlib1g-dev
    DIR="$(pwd)"
    cd ..
    export SWIFT_VERSION=swift-5.2.5
    export SWIFT_RELEASE=${SWIFT_VERSION}-RELEASE
    export UBUNTU_VERSION=ubuntu20.04
    wget https://swift.org/builds/${SWIFT_VERSION}-release/ubuntu2004/${SWIFT_RELEASE}/${SWIFT_RELEASE}-${UBUNTU_VERSION}.tar.gz
    tar xzf ${SWIFT_RELEASE}-${UBUNTU_VERSION}.tar.gz
    export PATH="${PWD}/${SWIFT_RELEASE}-${UBUNTU_VERSION}/usr/bin:${PATH}"
    cd "$DIR"
fi
