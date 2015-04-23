#!/usr/bin/env bash

set -o errexit
shopt -s nullglob

ENV=.env

if ! [[ -d $ENV ]]; then
    virtualenv $ENV --always-copy -p $(which python2)
fi

source $ENV/bin/activate

pip install 'http://pygame.org/ftp/pygame-1.9.1release.tar.gz' pyserial==2.7
