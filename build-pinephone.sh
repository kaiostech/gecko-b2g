#!/bin/bash

export MOZCONFIG=./mozconfig-b2g-mobian
export MOZ_OBJDIR=./obj-b2g-mobian/

# export B2G_DEBUG=1

./mach build $1
