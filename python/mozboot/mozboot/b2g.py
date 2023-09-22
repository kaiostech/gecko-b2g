# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import errno
import os
import stat
import subprocess
import sys

B2G_MOZCONFIG_TEMPLATE = """
# Build Boot2Gecko
ac_add_options --enable-application=b2g
ac_add_options --with-app-basename=b2g

# Android
ac_add_options --with-android-version=29
ac_add_options --target=x86_64-linux-android

# Compiler options
ac_add_options --with-android-ndk="$HOME/.mozbuild/android-ndk-r21d"

# B2G-specific options
ac_add_options --with-gonk-gfx
ac_add_options --with-gonk="$HOME/.mozbuild/b2g-sysroot/"
ac_add_options --enable-b2g-camera
ac_add_options --enable-b2g-ril
ac_add_options --enable-b2g-fm
ac_add_options --enable-forkserver

# Only for x86-64
ac_add_options --enable-wasm-simd

# Sandbox & profiler are not supported yet
ac_add_options --disable-profiling
ac_add_options --disable-sandbox

# Compiler/Linker options

# Since we use lld we need to disable elf-hack
ac_add_options --enable-linker=lld
ac_add_options --disable-elf-hack
"""

def generate_mozconfig():
    return B2G_MOZCONFIG_TEMPLATE.strip()
