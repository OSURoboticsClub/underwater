#!/usr/bin/env bash

set -o errexit
shopt -s nullglob

ENV=.env

if ! [[ -d $ENV ]]; then
    virtualenv $ENV --always-copy -p $(which python2)
fi

source $ENV/bin/activate

pip install numpy==1.9.1

mkdir -p vision/lib/opencv/buil
pushd vision/lib/opencv/build
cmake ..
make
popd
