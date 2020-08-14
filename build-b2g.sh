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

if [ -z ${GET_FRAMEBUFFER_FORMAT_FROM_HWC+x} ]; then
    echo "GET_FRAMEBUFFER_FORMAT_FROM_HWC is not set"
else
    HWC_DEFINE="-DGET_FRAMEBUFFER_FORMAT_FROM_HWC"
    echo "Setting -DGET_FRAMEBUFFER_FORMAT_FROM_HWC"
fi

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

ANDROID_NDK=${ANDROID_NDK:-$HOME/.mozbuild/android-ndk-r20b-canary}
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

SYSROOT=$ANDROID_NDK/platforms/$ANDROID_PLATFORM/$ARCH_DIR/
GONK_LIBS=$GONK_PATH/out/target/product/$GONK_PRODUCT_NAME/obj/lib/

export GONK_PRODUCT=$GONK_PRODUCT_NAME

rustc --version

if [ "$TARGET_ARCH" = "$TARGET_ARCH_VARIANT" ] && [ "$TARGET_ARCH" = "$TARGET_CPU_VARIANT" ]; then
  TARGET_FOLDER="$TARGET_ARCH"
else
  TARGET_FOLDER="$TARGET_ARCH_$TARGET_ARCH_VARIANT_$TARGET_CPU_VARIANT"
fi

INTERMEDIATES="$GONK_PATH/out/soong/.intermediates"
HIDL_HW="$INTERMEDIATES/hardware/interfaces"

export CFLAGS="-Wno-nullability-completeness"

export CPPFLAGS="-DANDROID -DTARGET_OS_GONK \
-DJE_FORCE_SYNC_COMPARE_AND_SWAP_4=1 \
-D_USING_LIBCXX \
-DGR_GL_USE_NEW_SHADER_SOURCE_SIGNATURE=1 \
-isystem $ANDROID_NDK/platforms/$ANDROID_PLATFORM/$ARCH_DIR/usr/include \
-isystem $GONK_PATH/frameworks/av/camera/include \
-isystem $GONK_PATH/frameworks/av/camera/include/camera \
-isystem $GONK_PATH/frameworks/av/include \
-isystem $GONK_PATH/frameworks/av/media/libaudioclient/include \
-isystem $GONK_PATH/frameworks/av/media/libmedia/aidl \
-isystem $GONK_PATH/frameworks/av/media/libmedia/include \
-isystem $GONK_PATH/frameworks/av/media/libstagefright/foundation/include \
-isystem $GONK_PATH/frameworks/av/media/libstagefright/include \
-isystem $GONK_PATH/frameworks/native/headers/media_plugin \
-isystem $GONK_PATH/frameworks/native/include/gui \
-isystem $GONK_PATH/frameworks/native/include/media/openmax \
-isystem $GONK_PATH/frameworks/native/libs/binder/include \
-isystem $GONK_PATH/frameworks/native/libs/gui/include \
-isystem $GONK_PATH/frameworks/native/libs/math/include \
-isystem $GONK_PATH/frameworks/native/libs/nativebase/include \
-isystem $GONK_PATH/frameworks/native/libs/nativewindow/include \
-isystem $GONK_PATH/frameworks/native/libs/ui/include \
-isystem $GONK_PATH/frameworks/native/opengl/include \
-isystem $GONK_PATH/gonk-misc/libcarthage/HWC \
-isystem $GONK_PATH/gonk-misc/libcarthage/include \
-isystem $GONK_PATH/hardware/libhardware/include \
-isystem $GONK_PATH/hardware/libhardware_legacy/include \
-isystem $GONK_PATH/system/connectivity \
-isystem $GONK_PATH/system/core/base/include \
-isystem $GONK_PATH/system/core/include \
-isystem $GONK_PATH/system/core/libcutils/include \
-isystem $GONK_PATH/system/core/liblog/include \
-isystem $GONK_PATH/system/core/libprocessgroup/include \
-isystem $GONK_PATH/system/core/libsuspend/include \
-isystem $GONK_PATH/system/core/libsync/include \
-isystem $GONK_PATH/system/core/libsystem/include \
-isystem $GONK_PATH/system/core/libsysutils/include \
-isystem $GONK_PATH/system/core/libutils/include \
-isystem $GONK_PATH/system/libhidl/base/include \
-isystem $GONK_PATH/system/libhidl/transport/token/1.0/utils/include \
-isystem $GONK_PATH/system/media/audio/include \
-isystem $GONK_PATH/system/media/camera/include \
-isystem $HIDL_HW/gnss/1.0/android.hardware.gnss@1.0_genc++_headers/gen \
-isystem $HIDL_HW/gnss/1.1/android.hardware.gnss@1.1_genc++_headers/gen \
-isystem $HIDL_HW/gnss/2.0/android.hardware.gnss@2.0_genc++_headers/gen \
-isystem $HIDL_HW/gnss/measurement_corrections/1.0/android.hardware.gnss.measurement_corrections@1.0_genc++_headers/gen \
-isystem $HIDL_HW/gnss/visibility_control/1.0/android.hardware.gnss.visibility_control@1.0_genc++_headers/gen \
-isystem $HIDL_HW/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen \
-isystem $HIDL_HW/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen \
-isystem $HIDL_HW/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen \
-isystem $HIDL_HW/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen \
-isystem $HIDL_HW/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen \
-isystem $HIDL_HW/media/1.0/android.hardware.media@1.0_genc++_headers/gen \
-isystem $HIDL_HW/media/omx/1.0/android.hardware.media.omx@1.0_genc++_headers/gen \
-isystem $HIDL_HW/radio/1.0/android.hardware.radio@1.0_genc++_headers/gen \
-isystem $HIDL_HW/sensors/1.0/android.hardware.sensors@1.0_genc++_headers/gen \
-isystem $HIDL_HW/vibrator/1.0/android.hardware.vibrator@1.0_genc++_headers/gen \
-isystem $HIDL_HW/wifi/1.0/android.hardware.wifi@1.0_genc++_headers/gen \
-isystem $HIDL_HW/wifi/1.1/android.hardware.wifi@1.1_genc++_headers/gen \
-isystem $HIDL_HW/wifi/1.2/android.hardware.wifi@1.2_genc++_headers/gen \
-isystem $HIDL_HW/wifi/1.3/android.hardware.wifi@1.3_genc++_headers/gen \
-isystem $HIDL_HW/wifi/hostapd/1.0/android.hardware.wifi.hostapd@1.0_genc++_headers/gen \
-isystem $HIDL_HW/wifi/hostapd/1.1/android.hardware.wifi.hostapd@1.1_genc++_headers/gen \
-isystem $HIDL_HW/wifi/supplicant/1.0/android.hardware.wifi.supplicant@1.0_genc++_headers/gen \
-isystem $HIDL_HW/wifi/supplicant/1.1/android.hardware.wifi.supplicant@1.1_genc++_headers/gen \
-isystem $HIDL_HW/wifi/supplicant/1.2/android.hardware.wifi.supplicant@1.2_genc++_headers/gen \
-isystem $INTERMEDIATES/frameworks/av/camera/libcamera_client/android_${TARGET_FOLDER}_core_shared/gen/aidl \
-isystem $INTERMEDIATES/frameworks/av/media/libaudioclient/libaudioclient/android_${TARGET_FOLDER}_core_shared/gen/aidl \
-isystem $INTERMEDIATES/frameworks/av/media/libmedia/libmedia_omx/android_${TARGET_FOLDER}_core_shared/gen/aidl \
-isystem $INTERMEDIATES/gonk-misc/gonk-binder/binder_b2g_connectivity_interface-cpp-source/gen/include \
-isystem $INTERMEDIATES/gonk-misc/gonk-binder/binder_b2g_telephony_interface-cpp-source/gen/include \
-isystem $INTERMEDIATES/system/connectivity/wificond/libwificond_ipc/android_${TARGET_FOLDER}_core_static/gen/aidl \
-isystem $INTERMEDIATES/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen \
-isystem $INTERMEDIATES/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen \
-isystem $INTERMEDIATES/system/netd/resolv/dnsresolver_aidl_interface-V2-cpp-source/gen/include \
-isystem $INTERMEDIATES/system/netd/server/netd_aidl_interface-V2-cpp-source/gen/include \
-isystem $INTERMEDIATES/system/netd/server/netd_event_listener_interface-V1-cpp-source/gen/include \
-isystem $INTERMEDIATES/system/vold/libvold_binder_shared/android_${TARGET_FOLDER}_core_shared/gen/aidl"

# export RUSTC_OPT_LEVEL=z

GCC_LIB="-L$GONK_PATH/prebuilts/gcc/linux-x86/$ARCH_NAME/$TARGET_TRIPLE-4.9/lib/gcc/$TARGET_TRIPLE/4.9.x/"

export ANDROID_PLATFORM=$ANDROID_PLATFORM

OBJ_LIB=$GONK_PATH/out/target/product/$GONK_PRODUCT_NAME/obj/lib
SYS_LIB=$GONK_PATH/out/target/product/$GONK_PRODUCT_NAME/system/lib$BINSUFFIX
export SYS_LIB

export LDFLAGS="-L$SYS_LIB -Wl,-rpath-link=$OBJ_LIB \
--sysroot=$SYSROOT $GCC_LIB -ldl -lstdc++ -Wl,--no-as-needed \
-llog -landroid -lnativewindow -lbinder \
-lui -lgui \
-lutils -lcutils -lsysutils \
-lhardware_legacy -lhardware -lsuspend \
-lhidltransport \
-lhidlbase -lbase -lhidlmemory -lhwbinder -laudioclient"

./mach build $@
