#!/usr/bin/env bash

set -o errexit
shopt -s nullglob

if ! [[ -d ../.env ]]; then
    virtualenv ../.env --always-copy -p $(which python2)
fi

source ../.env/bin/activate

for f in requirements/*; do
    bash $f
done
