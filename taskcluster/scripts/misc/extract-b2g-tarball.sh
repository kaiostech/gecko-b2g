#!/bin/bash
set -x -e -v

cd "${MOZ_FETCHES_DIR}"
tar -x -a -f b2g-build.tar "${1}"
mkdir -p "$UPLOAD_DIR"
mv "${1}" "$UPLOAD_DIR/${1}"
