#!/bin/bash
set -x -e -v

src="${1-.}"
dest="${2-.}"

# Copy the contents of the directories in the first argument to the sysroot
# using the second argument as the destination folder
function copy_to_sysroot() {
  mkdir -p "${dest}/b2g-sysroot/${2}"
  printf "${1}\n" | while read path; do
    path="${src}/${path}"
    if [ -d "${path}" ]; then
      rsync --times --exclude=Android.bp --exclude=AndroidTest.xml -r --copy-links --exclude=".git" "${path}/" "${dest}/b2g-sysroot/${2}/"
    else
      mkdir -p "${dest}/b2g-sysroot/${2}"
      cp --preserve=timestamps "${path}" "${dest}/b2g-sysroot/${2}/"
    fi
  done
}

# Prepare the device-specific paths in the AOSP build files
case "${TARGET_ARCH}" in
    arm)
        ARCH_NAME="arm"
        ARCH_ABI="androideabi"
        ;;
    arm64)
        ARCH_NAME="aarch64"
        ARCH_ABI="android"
        TARGET_TRIPLE=$ARCH_NAME-linux-$ARCH_ABI
        BINSUFFIX=64
        ;;
    x86)
        ARCH_NAME="i686"
        ARCH_ABI="android"
        TARGET_TRIPLE=$ARCH_NAME-linux-$ARCH_ABI
        ;;
    x86_64)
        ARCH_NAME="x86"
        ARCH_ABI="android"
        BINSUFFIX=64
        ;;
    *)
        echo "Unsupported $TARGET_ARCH"
        exit 1
        ;;
esac

TARGET_TRIPLE=${TARGET_TRIPLE:-$TARGET_ARCH-linux-$ARCH_ABI}

ARCH_FOLDER="${TARGET_ARCH}"
if [ "${TARGET_ARCH}" != "${TARGET_ARCH_VARIANT}" ]; then
    ARCH_FOLDER="${TARGET_ARCH}_${TARGET_ARCH_VARIANT}"
fi

# Copy the system libraries to the sysroot
LIBRARIES="out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@2.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss.visibility_control@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.3.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.power@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.radio@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.sensors@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.vibrator@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.3.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.hostapd@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.hostapd@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.system.wifi.keystore@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_connectivity_interface-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_telephony_interface-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/dnsresolver_aidl_interface-V2-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libaudioclient.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libbase.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libbinder.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libcamera_client.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libc++.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libcutils.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libfmq.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libgui.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhardware_legacy.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhardware.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhidlbase.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhidlmemory.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhidltransport.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libhwbinder.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmedia_omx.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmedia.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmtp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libstagefright_foundation.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libstagefright_omx.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libstagefright.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libsuspend.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libsync.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libsysutils.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libui.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libutils.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libvold_binder_shared.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libwificond_ipc_shared.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/netd_aidl_interface-V2-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/netd_event_listener_interface-V1-cpp.so"

copy_to_sysroot "${LIBRARIES}" "libs"

# Store the system includes in the sysroot
INCLUDE_FOLDERS="frameworks/av/camera/include
frameworks/av/include
frameworks/av/media/libaudioclient/include
frameworks/av/media/libmedia/aidl
frameworks/av/media/libmedia/include
frameworks/av/media/libstagefright/foundation/include
frameworks/av/media/libstagefright/include
frameworks/av/media/mtp
frameworks/native/headers/media_plugin
frameworks/native/include/gui
frameworks/native/include/media/openmax
frameworks/native/libs/binder/include
frameworks/native/libs/gui/include
frameworks/native/libs/math/include
frameworks/native/libs/nativebase/include
frameworks/native/libs/nativewindow/include
frameworks/native/libs/ui/include
frameworks/native/opengl/include
hardware/interfaces/graphics/composer/2.1/utils/command-buffer/include
hardware/interfaces/graphics/composer/2.2/utils/command-buffer/include
hardware/interfaces/graphics/composer/2.3/utils/command-buffer/include
hardware/libhardware/include
hardware/libhardware_legacy/include
system/connectivity
system/core/base/include
system/core/libcutils/include
system/core/liblog/include
system/core/libprocessgroup/include
system/core/libsuspend/include
system/core/libsync/include
system/core/libsystem/include
system/core/libsysutils/include
system/core/libutils/include
system/libfmq/include
system/libhidl/base/include
system/libhidl/transport/include
system/libhidl/transport/token/1.0/utils/include
system/media/audio/include
system/media/camera/include"

copy_to_sysroot "${INCLUDE_FOLDERS}" "include"

# Store the generated AIDL headers in the sysroot
GENERATED_AIDL_HEADERS="out/soong/.intermediates/frameworks/av/camera/libcamera_client/android_${ARCH_FOLDER}_core_shared/gen/aidl
out/soong/.intermediates/frameworks/av/media/libaudioclient/libaudioclient/android_${ARCH_FOLDER}_core_shared/gen/aidl
out/soong/.intermediates/frameworks/av/media/libmedia/libmedia_omx/android_${ARCH_FOLDER}_core_shared/gen/aidl
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_connectivity_interface-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_telephony_interface-cpp-source/gen/include
out/soong/.intermediates/system/connectivity/wificond/libwificond_ipc/android_${ARCH_FOLDER}_core_static/gen/aidl
out/soong/.intermediates/system/netd/resolv/dnsresolver_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/netd/server/netd_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/netd/server/netd_event_listener_interface-V1-cpp-source/gen/include
out/soong/.intermediates/system/vold/libvold_binder_shared/android_${ARCH_FOLDER}_core_shared/gen/aidl"

copy_to_sysroot "${GENERATED_AIDL_HEADERS}" "include"

# Store the generated HIDL headers in the sysroot
GENERATED_HIDL_HEADERS="out/soong/.intermediates/hardware/interfaces/gnss/1.0/android.hardware.gnss@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/1.1/android.hardware.gnss@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/2.0/android.hardware.gnss@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/measurement_corrections/1.0/android.hardware.gnss.measurement_corrections@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/visibility_control/1.0/android.hardware.gnss.visibility_control@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.1/android.hardware.graphics.composer@2.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.2/android.hardware.graphics.composer@2.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.3/android.hardware.graphics.composer@2.3_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/1.0/android.hardware.media@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/omx/1.0/android.hardware.media.omx@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/power/1.0/android.hardware.power@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/radio/1.0/android.hardware.radio@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/sensors/1.0/android.hardware.sensors@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/vibrator/1.0/android.hardware.vibrator@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.0/android.hardware.wifi@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.1/android.hardware.wifi@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.2/android.hardware.wifi@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.3/android.hardware.wifi@1.3_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/hostapd/1.0/android.hardware.wifi.hostapd@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/hostapd/1.1/android.hardware.wifi.hostapd@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.0/android.hardware.wifi.supplicant@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.1/android.hardware.wifi.supplicant@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.2/android.hardware.wifi.supplicant@1.2_genc++_headers/gen
out/soong/.intermediates/system/hardware/interfaces/wifi/keystore/1.0/android.system.wifi.keystore@1.0_genc++_headers/gen
out/soong/.intermediates/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen
out/soong/.intermediates/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen"

copy_to_sysroot "${GENERATED_HIDL_HEADERS}" "include"
