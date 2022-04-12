#!/bin/bash

export MOZCONFIG=./mozconfig-b2g-mobian
export MOZ_OBJDIR=./obj-b2g-mobian/

export RUSTUP_TOOLCHAIN=1.59

# export B2G_DEBUG=1

./mach build $1
