#!/bin/bash
set -x -e -v

src="${1-.}"
dest="${2-.}"

# Copy the contents of the files & directories in the first argument to the
# sysroot using the second argument as the destination folder. File
# creation/modification timestamps are preserved.
function copy_to_sysroot() {
    mkdir -p "${dest}/b2g-sysroot/${2}/" && \
    rsync --times -r --no-relative --copy-links \
        --exclude=".git" --exclude=Android.bp --exclude=AndroidTest.xml \
        --files-from="${1}" "${src}" "${dest}/b2g-sysroot/${2}/"
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

if [ "$TARGET_ARCH_VARIANT" = "$TARGET_ARCH" ] ||
   [ "$TARGET_ARCH_VARIANT" = "generic" ]; then
TARGET_ARCH_VARIANT=""
else
TARGET_ARCH_VARIANT="_$TARGET_ARCH_VARIANT"
fi

if [ "$TARGET_CPU_VARIANT" = "$TARGET_ARCH" ] ||
   [ "$TARGET_CPU_VARIANT" = "generic" ]; then
TARGET_CPU_VARIANT=""
else
TARGET_CPU_VARIANT="_$TARGET_CPU_VARIANT"
fi

ARCH_FOLDER="${TARGET_ARCH}${TARGET_ARCH_VARIANT}${TARGET_CPU_VARIANT}"

libraries_list=$(mktemp)
includes_list=$(mktemp)

# Copy the system libraries to the sysroot
tee "${libraries_list}" << EOF
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss@2.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.gnss.visibility_control@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.graphics.composer@2.3.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.power@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.power-V1-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.radio@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.radio@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.sensors@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.sensors@2.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.vibrator@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.vibrator-V1-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.3.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi@1.4.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.hostapd@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.hostapd@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.1.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.2.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.hardware.wifi.supplicant@1.3.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/android.system.wifi.keystore@1.0.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_stub.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_connectivity_interface-V1-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_system_interface-V1-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_telephony_interface-V1-cpp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/binder_b2g_remotesimunlock_interface-V1-cpp.so
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
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmedia_helper.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmedia_omx.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmedia.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmediadrm.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libmtp.so
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/libnetdbpf.so
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
out/target/product/${GONK_PRODUCT_NAME}/system/lib${BINSUFFIX}/netd_event_listener_interface-V1-cpp.so
EOF

copy_to_sysroot "${libraries_list}" "libs"

# Store the system includes in the sysroot
sed 's/$/\//' > "${includes_list}" << EOF
external/fmtlib/include
frameworks/av/camera/include
frameworks/av/drm/libmediadrm/interface
frameworks/av/include
frameworks/av/media/libaudioclient/include
frameworks/av/media/libaudiofoundation/include
frameworks/av/media/libmedia/aidl
frameworks/av/media/libmedia/include
frameworks/av/media/libmediahelper/include
frameworks/av/media/libmediametrics/include
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
gonk-misc/gonk-binder/binder_b2g_stub/include/
hardware/interfaces/graphics/composer/2.1/utils/command-buffer/include
hardware/interfaces/graphics/composer/2.2/utils/command-buffer/include
hardware/interfaces/graphics/composer/2.3/utils/command-buffer/include
hardware/libhardware/include
hardware/libhardware_legacy/include
system/bpf/libbpf_android/include
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
system/media/camera/include
system/netd/libnetdbpf/include
system/netd/libnetdutils/include
EOF

# Store the generated AIDL headers in the sysroot
sed 's/$/\//' >> "${includes_list}" << EOF
out/soong/.intermediates/frameworks/av/camera/libcamera_client/android_${ARCH_FOLDER}_shared_cfi/gen/aidl
out/soong/.intermediates/frameworks/av/media/libaudioclient/libaudioclient/android_${ARCH_FOLDER}_shared_cfi/gen/aidl
out/soong/.intermediates/frameworks/av/media/libmedia/libmedia_omx/android_${ARCH_FOLDER}_shared_cfi/gen/aidl
out/soong/.intermediates/frameworks/base/libincremental_aidl-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_connectivity_interface-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_system_interface-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_telephony_interface-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_remotesimunlock_interface-cpp-source/gen/include
out/soong/.intermediates/packages/modules/DnsResolver/dnsresolver_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/connectivity/wificond/libwificond_ipc/android_${ARCH_FOLDER}_static/gen/aidl
out/soong/.intermediates/system/netd/server/netd_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/netd/server/netd_event_listener_interface-cpp-source/gen/include
out/soong/.intermediates/system/vold/libvold_binder_shared/android_${ARCH_FOLDER}_shared/gen/aidl
EOF

# Store the generated HIDL headers in the sysroot
sed 's/$/\//' >> "${includes_list}" << EOF
out/soong/.intermediates/hardware/interfaces/gnss/1.0/android.hardware.gnss@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/1.1/android.hardware.gnss@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/2.0/android.hardware.gnss@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/measurement_corrections/1.0/android.hardware.gnss.measurement_corrections@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/visibility_control/1.0/android.hardware.gnss.visibility_control@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/aidl/android.hardware.graphics.common-ndk_platform-source/gen/include
out/soong/.intermediates/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.1/android.hardware.graphics.composer@2.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.2/android.hardware.graphics.composer@2.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/composer/2.3/android.hardware.graphics.composer@2.3_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/1.0/android.hardware.media@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/omx/1.0/android.hardware.media.omx@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/power/aidl/android.hardware.power-cpp-source/gen/include
out/soong/.intermediates/hardware/interfaces/radio/1.0/android.hardware.radio@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/radio/1.1/android.hardware.radio@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/sensors/1.0/android.hardware.sensors@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/sensors/2.0/android.hardware.sensors@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/vibrator/1.0/android.hardware.vibrator@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/vibrator/aidl/android.hardware.vibrator-cpp-source/gen/include
out/soong/.intermediates/hardware/interfaces/wifi/1.0/android.hardware.wifi@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.1/android.hardware.wifi@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.2/android.hardware.wifi@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.3/android.hardware.wifi@1.3_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/1.4/android.hardware.wifi@1.4_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/hostapd/1.0/android.hardware.wifi.hostapd@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/hostapd/1.1/android.hardware.wifi.hostapd@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.0/android.hardware.wifi.supplicant@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.1/android.hardware.wifi.supplicant@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.2/android.hardware.wifi.supplicant@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.3/android.hardware.wifi.supplicant@1.3_genc++_headers/gen
out/soong/.intermediates/system/hardware/interfaces/wifi/keystore/1.0/android.system.wifi.keystore@1.0_genc++_headers/gen
out/soong/.intermediates/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen
out/soong/.intermediates/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen
EOF

copy_to_sysroot "${includes_list}" "include"


lib_c="${src}/out/target/product/${GONK_PRODUCT_NAME}/system/apex/com.android.runtime.release/lib${BINSUFFIX}/bionic/libc.so"
if test -f ${lib_c}; then
    rsync --times --no-relative --copy-links \
        "${lib_c}" \
        "${dest}/b2g-sysroot/libs/"
elif test -f "${src}/out/target/product/${GONK_PRODUCT_NAME}/apex/com.android.runtime/lib${BINSUFFIX}/bionic/libc.so"; then
    rsync --times --no-relative --copy-links \
        "${src}/out/target/product/${GONK_PRODUCT_NAME}/apex/com.android.runtime/lib${BINSUFFIX}/bionic/libc.so" \
        "${dest}/b2g-sysroot/libs/"
else
    rsync --times --no-relative --copy-links \
        "${src}/out/target/product/${GONK_PRODUCT_NAME}/obj/SHARED_LIBRARIES/libc_intermediates/libc.so" \
        "${dest}/b2g-sysroot/libs/"
fi

if test -z "$DISABLE_OEMHOOK"; then
    # Put HIDL headers and libraries of OEM hook into sysroot
    rsync --times --no-relative --copy-links \
        "${src}/out/target/product/${GONK_PRODUCT_NAME}/system/product/lib${BINSUFFIX}/vendor.qti.hardware.radio.qcrilhook@1.0.so" \
        "${dest}/b2g-sysroot/libs/"
    rsync --times --no-relative --copy-links -r \
        "${src}/out/soong/.intermediates/vendor/qcom/proprietary/commonsys-intf/telephony/interfaces/hal/qcrilhook/1.0/vendor.qti.hardware.radio.qcrilhook@1.0_genc++_headers/gen/" \
        "${dest}/b2g-sysroot/include/"
else
    echo "OEM hook is disabled by DISABLE_OEMHOOK"
fi
