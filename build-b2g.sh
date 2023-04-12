#!/bin/bash

set -e

export RUSTUP_TOOLCHAIN=${RUSTUP_TOOLCHAIN:-stable}

# Check that the GONK_PATH environment variable is set.
if [ -z ${GONK_PATH+x} ];
then
    echo "Please set GONK_PATH to the root of your Gonk directory first.";
    exit 1;
else
    echo "Using '$GONK_PATH'";
fi

# Check that the GONK_PRODUCT_NAME environment variable is set.
if [ -z ${GONK_PRODUCT_NAME+x} ];
then
    echo "Please set GONK_PRODUCT_NAME to the name of the device (look at $GONK_PATH/out/target/product).";
    exit 1;
else
    echo "Product is '$GONK_PRODUCT_NAME'";
fi

if [ -z ${GECKO_OBJDIR+x} ]; then
    echo "Using default objdir"
else
    export MOZ_OBJDIR="$GECKO_OBJDIR"
    echo "Building in $MOZ_OBJDIR"
fi

if [ -z ${PLATFORM_VERSION+x} ]; then
    echo "Please set PLATFORM_VERSION to the android version of the device"
    exit 1;
elif [ $PLATFORM_VERSION -lt 27 ]; then
    echo "This script is not supporting platform version less than 27"
    exit 1;
else
    echo "Building in platform version $PLATFORM_VERSION"
fi

export ANDROID_PLATFORM=android-${PLATFORM_VERSION}

# When user build, check if the JS shell is available. If not, download it
# to make sure we can minify JS code when packaging.
if [[ "$VARIANT" == "user" ]];then
    if [ -f "./jsshell/js" ]; then
        echo "JS shell found."
    else
        echo "Downloading JS shell..."
        HOST_OS=$(uname -s)
        if [ "$HOST_OS" == "Darwin" ]; then
            SHELL_ARCH=mac
        else
            SHELL_ARCH=linux-x86_64
        fi

        mkdir -p jsshell
        curl https://ftp.mozilla.org/pub/firefox/releases/67.0b8/jsshell/jsshell-${SHELL_ARCH}.zip > /tmp/jsshell-${SHELL_ARCH}.zip
        cd jsshell
        unzip /tmp/jsshell-${SHELL_ARCH}.zip
        rm /tmp/jsshell-${SHELL_ARCH}.zip
        cd ..
    fi
    export JS_BINARY=`pwd`/jsshell/js
fi

export MOZCONFIG=`pwd`/mozconfig-b2g

ANDROID_NDK=${ANDROID_NDK:-$HOME/.mozbuild/android-ndk-r21d}
export ANDROID_NDK="${ANDROID_NDK/#\~/$HOME}"

TARGET_GCC_VERSION=${TARGET_GCC_VERSION:-4.9}

export CLANG_PATH=${CLANG_PATH:-$HOME/.mozbuild/clang/bin}

export PYTHON_PATH=${PYTHON_PATH:-/usr/bin}

case $TARGET_ARCH in
    arm)
        ARCH_NAME="arm"
        ARCH_DIR="arch-arm"
        ARCH_ABI="androideabi"
        ;;
    arm64)
        ARCH_NAME="aarch64"
        ARCH_DIR="arch-arm64"
        ARCH_ABI="android"
        TARGET_TRIPLE=$ARCH_NAME-linux-$ARCH_ABI
        BINSUFFIX=64
        ;;
    x86)
        ARCH_NAME="i686"
        ARCH_DIR="arch-x86"
        ARCH_ABI="android"
        TARGET_TRIPLE=$ARCH_NAME-linux-$ARCH_ABI
        ;;
    x86_64)
        ARCH_NAME="x86"
        ARCH_DIR="arch-x86_64"
        ARCH_ABI="android"
        BINSUFFIX=64
        ;;
    *)
        echo "Unsupported $TARGET_ARCH"
        exit 1
        ;;
esac

TARGET_TRIPLE=${TARGET_TRIPLE:-$TARGET_ARCH-linux-$ARCH_ABI}
export TARGET_TRIPLE

export CROSS_TOOLCHAIN_LINKER_PATH=${CROSS_TOOLCHAIN_LINKER_PATH=:-$GONK_PATH/prebuilts/gcc/linux-x86/$ARCH_NAME/$TARGET_TRIPLE-$TARGET_GCC_VERSION/$TARGET_TRIPLE/bin}

export PATH=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64/bin:$GONK_PATH/prebuilts/linux-x86_64/bin/:$CLANG_PATH:$PYTHON_PATH:$CROSS_TOOLCHAIN_LINKER_PATH:$PATH

export GONK_PRODUCT=$GONK_PRODUCT_NAME

# Create the sysroot
if [ -z ${GECKO_OBJDIR+x} ]; then
  SYSROOT_DEST=$(./mach environment | grep -A1 '^object directory' | tail -n 1 | cut -b2-)
else
  SYSROOT_DEST="${GECKO_OBJDIR}"
fi

if [ -d "koost" ]; then
    export BUILD_KOOST=1
fi

# Export OEM hook flag since it's used by create-b2g-sysroot.sh for HIDL
if [ "$PRODUCT_MANUFACTURER" == "QUALCOMM" ]; then
    export DISABLE_OEMHOOK
else
    # OEM hook is only supported on Qualcomm platform
    export DISABLE_OEMHOOK=1
fi

if [ "$PLATFORM_VERSION" == "30" ]; then
  SYSROOT_SCRIPT=taskcluster/scripts/misc/create-b2g-sysroot-aosp11.sh
else
  SYSROOT_SCRIPT=taskcluster/scripts/misc/create-b2g-sysroot.sh
fi

if [ -z ${B2G_STANDALONE_BUILD+x} ]; then
  rm -rf "${SYSROOT_DEST}/b2g-sysroot"
  $SYSROOT_SCRIPT "${GONK_PATH}" "${SYSROOT_DEST}"
fi

rustc --version

export ANDROID_PLATFORM=$ANDROID_PLATFORM
export PRODUCTION_OS_NAME=$PRODUCTION_OS_NAME
# force mach to use system python installation
export MACH_USE_SYSTEM_PYTHON=${MACH_USE_SYSTEM_PYTHON-1}
export MOZ_LINKER=

./mach build $@
