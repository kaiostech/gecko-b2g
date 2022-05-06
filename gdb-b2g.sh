#!/bin/bash

GECKO_OBJDIR=${GECKO_OBJDIR:-objdir-gecko}
SYSROOT=$GECKO_OBJDIR/dist/sysroot/
GONK_PATH=${GONK_PATH:-.}

case "$TARGET_ARCH" in
    arm)
        GDB=$GONK_PATH/gonk-misc/arm-unknown-linux-androideabi-gdb
        GDBSERVER=gdbserver
        ;;
    arm64)
        GDB=$GONK_PATH/gonk-misc/aarch64-unknown-linux-androideabi-gdb
        GDBSERVER=gdbserver64
        ;;
esac

if [ -z "$GECKO_OBJDIR" -o ! -e "$GECKO_OBJDIR/config.log" ]; then
    echo "GECKO_OBJDIR is invalid!" >&2
    exit 255
fi

if [ ! -e "$SYSROOT" ]; then
    echo "Run prepare-gdb-syms.sh at first." >&2
    exit 255
fi

if [ -z "$GONK_PATH" -o ! -e "$GDB" ]; then
    echo "GONK_PATH is invalid!" >&2
    exit 255
fi

echo -e "Please run b2g with \e[31m${GDBSERVER} at localhost:8859\e[0m"
echo -e "For example, \e[92madb shell 'COMMAND_PREFIX=\"${GDBSERVER} localhost:8859\" b2g.sh'\e[0m"
echo
echo

adb forward tcp:8859 tcp:8859
$GDB --data-directory=$(dirname $GDB)/gdb-share -n -ex "set sysroot $SYSROOT" -ex "set debug-file-directory $SYSROOT" -ex 'target remote localhost:8859'
