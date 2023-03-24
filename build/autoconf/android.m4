dnl This Source Code Form is subject to the terms of the Mozilla Public
dnl License, v. 2.0. If a copy of the MPL was not distributed with this
dnl file, You can obtain one at http://mozilla.org/MPL/2.0/.

AC_DEFUN([MOZ_ANDROID_NDK],
[

case "$target" in
*-android*|*-linuxandroid*)
    dnl $extra_android_flags will be set for us by Python configure.
    dnl $_topsrcdir/build/android is a hack for versions of rustc < 1.68
    LDFLAGS="$extra_android_flags -L$_topsrcdir/build/android $LDFLAGS"
    CPPFLAGS="$extra_android_flags $CPPFLAGS"
    CFLAGS="-fno-short-enums $CFLAGS"
    CXXFLAGS="-fno-short-enums $CXXFLAGS"
    ASFLAGS="$extra_android_flags -DANDROID $ASFLAGS"

    # If we're building for a gonk target add more sysroot system includes and
    # library paths
    if test -n "$gonkdir"; then
        CPPFLAGS="$CPPFLAGS -isystem $gonkdir/include"
        # HACK: Some system headers in the AOSP sources are laid out
        # differently than the others so they get included both as
        # #include <camera/path/to/header.h> and directly as
        # #include <path/to/header.h>. To accomodate for this we need camera/
        # in the default include path until we can fix the issue.
        CPPFLAGS="$CPPFLAGS -isystem $gonkdir/include/camera"

        # Needed for config/system-headers.mozbuild
        ANDROID_VERSION="${android_version}"
        AC_SUBST(ANDROID_VERSION)
    fi
    ;;
esac

])
