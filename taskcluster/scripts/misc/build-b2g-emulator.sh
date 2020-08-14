#!/bin/bash
set -x -e -v

# Find the local patches
cd "${GECKO_PATH}"
gonk_misc_patch_file="$(pwd)/taskcluster/scripts/misc/gonk-misc.patch"
build_soong_patch_file="$(pwd)/taskcluster/scripts/misc/build_soong.patch"
cd ..

# Configure the manifest repository, it needs to be a valid git repository so
# we create an artificial entry to satisfy the config.sh script.
cd "${MOZ_FETCHES_DIR}/manifests"
git config --global user.email "dummy@dummy.org"
git config --global user.name "dummy"
git init .
git add .
git commit -m dummy

# Configure the emulator build and fetch all the AOSP dependencies
cd "${MOZ_FETCHES_DIR}/B2G"
GITREPO="${MOZ_FETCHES_DIR}/manifests" REPO_SYNC_FLAGS="-j4" REPO_INIT_FLAGS="--depth=1" ./config.sh emulator-10-x86_64

# Wipe the repos, we don't need them anymore
rm -rf .repo

# Remove the Gecko build & packaging steps
patch -d gonk-misc -p1 <<'EOF'
diff --git a/Android.mk b/Android.mk
index 6225f10..6b707b4 100644
--- a/Android.mk
+++ b/Android.mk
@@ -170,15 +170,15 @@ endif
 $(LOCAL_INSTALLED_MODULE) : $(LOCAL_BUILT_MODULE)
 	@echo Install dir: $(TARGET_OUT)/b2g
 	rm -rf $(filter-out $(addprefix $(TARGET_OUT)/b2g/,$(PRESERVE_DIRS)),$(wildcard $(TARGET_OUT)/b2g/*))
-	cd $(TARGET_OUT) && tar xvfz $(abspath $<)
+	:
 ifneq ($(EXPORT_DEVICE_PREFS),)
-	cp -n $(EXPORT_DEVICE_PREFS)/*.js $(TARGET_OUT)/b2g/defaults/pref/
+	:
 endif
 ifneq ($(EXPORT_DEVICE_USER_BUILD_PREFS),)
-	cp -n $(EXPORT_DEVICE_USER_BUILD_PREFS) $(TARGET_OUT)/b2g/defaults/pref/
+	:
 endif
 ifneq ($(EXPORT_BUILD_PREFS),)
-	cp -n $(EXPORT_BUILD_PREFS) $(TARGET_OUT)/b2g/defaults/pref/
+	:
 endif
 
 GECKO_LIB_DEPS := \
@@ -242,35 +242,7 @@ endif
 
 .PHONY: $(LOCAL_BUILT_MODULE)
 $(LOCAL_BUILT_MODULE): $(TARGET_CRTBEGIN_DYNAMIC_O) $(TARGET_CRTEND_O) $(addprefix $(TARGET_OUT_SHARED_LIBRARIES)/,$(GECKO_LIB_DEPS)) $(GECKO_LIB_STATIC)
-ifeq ($(USE_PREBUILT_B2G),1)
-	@echo -e "\033[0;33m ==== Use prebuilt gecko ==== \033[0m";
-	mkdir -p $(@D) && cp $(abspath $(PREFERRED_B2G)) $@
-else
-	echo "export GECKO_OBJDIR=$(abspath $(GECKO_OBJDIR))"; \
-	echo "export GONK_PRODUCT_NAME=$(TARGET_DEVICE)"; \
-	echo "export GONK_PATH=$(abspath .)"; \
-	echo "export PLATFORM_VERSION=$(PLATFORM_SDK_VERSION)"; \
-	echo "export TARGET_ARCH=$(TARGET_ARCH)"; \
-	echo "export TARGET_ARCH_VARIANT=$(TARGET_ARCH_VARIANT)"; \
-	echo "export TARGET_CPU_VARIANT=$(TARGET_CPU_VARIANT)"; \
-	echo "export PRODUCT_MANUFACTURER=$(PRODUCT_MANUFACTURER)"; \
-	echo "export MOZ_DISABLE_LTO=$(MOZ_DISABLE_LTO)"; \
-	echo "export HOST_OS=$(HOST_OS)";	\
-	unset CC_WRAPPER && unset CXX_WRAPPER && \
-	export GECKO_OBJDIR="$(abspath $(GECKO_OBJDIR))" && \
-	export GONK_PATH="$(abspath .)" && \
-	export GONK_PRODUCT_NAME="$(TARGET_DEVICE)" && \
-	export PLATFORM_VERSION="$(PLATFORM_SDK_VERSION)" && \
-	export TARGET_ARCH="$(TARGET_ARCH)" && \
-	export TARGET_ARCH_VARIANT="$(TARGET_ARCH_VARIANT)" && \
-	export TARGET_CPU_VARIANT="$(TARGET_CPU_VARIANT)" && \
-	export PRODUCT_MANUFACTURER="$(PRODUCT_MANUFACTURER)" && \
-	export MOZ_DISABLE_LTO="$(MOZ_DISABLE_LTO)" && \
-	export HOST_OS="$(HOST_OS)" && \
-	(cd $(GECKO_PATH) ; $(SHELL) build-b2g.sh) && \
-	(cd $(GECKO_PATH) ; $(SHELL) build-b2g.sh package) && \
-	mkdir -p $(@D) && cp $(GECKO_OBJDIR)/dist/b2g-*.tar.gz $@
-endif
+	:
 
 # Include a copy of the repo manifest that has the revisions used
 ifneq ($(DISABLE_SOURCES_XML),true)
EOF

# Force compressing debug symbols
patch -d build/soong -p1 <<'EOF'
diff --git a/cc/config/global.go b/cc/config/global.go
index 815c31d8..9d82d460 100644
--- a/cc/config/global.go
+++ b/cc/config/global.go
@@ -44,6 +44,7 @@ var (
 
 		"-O2",
 		"-g",
+		"-gz", // Compress debug symbols
 
 		"-fno-strict-aliasing",
 	}
@@ -69,6 +70,7 @@ var (
 		"-Werror=sequence-point",
 		"-Werror=date-time",
 		"-Werror=format-security",
+		"-gz", // Compress debug symbols
 	}
 
 	deviceGlobalCppflags = []string{
@@ -85,6 +87,7 @@ var (
 		"-Wl,--no-undefined-version",
 		"-Wl,--exclude-libs,libgcc.a",
 		"-Wl,--exclude-libs,libgcc_stripped.a",
+		"-Wl,--compress-debug-sections=zlib", // Compress debug symbols
 	}
 
 	deviceGlobalLldflags = append(ClangFilterUnknownLldflags(deviceGlobalLdflags),
EOF

# Build the emulator
./build.sh

# Package the emulator
./scripts/package-emulator.sh "b2g-emulator.tar.zst" \
  "$GECKO_PATH/taskcluster/scripts/misc/zstdpy"

###############################################################################
# Package the sysroot

# Copy the contents of the directories in the first argument to the sysroot
# preserving their full paths
function copy_to_sysroot_full_path() {
  printf "${1}\n" | while read path; do
    if [ -d "${path}" ]; then
      mkdir -p "b2g-sysroot/${path}"
      rsync -r "${path}/" "b2g-sysroot/${path}/"
    else
      mkdir -p "b2g-sysroot/$(dirname ${path})"
      cp "${path}" "b2g-sysroot/${path}"
    fi
  done
}

# Copy the contents of the directories in the first argument to the sysroot
# using the second argument as the destination folder
function copy_to_sysroot() {
  mkdir -p "b2g-sysroot/${2}"
  printf "${1}\n" | while read path; do
    rsync -r --copy-links --times --exclude=".git" "${path}/" "b2g-sysroot/${2}/"
  done
}

# Copy the prebuilts to the sysroot
PREBUILTS="prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.9/lib/gcc/x86_64-linux-android/4.9.x"

copy_to_sysroot_full_path "${PREBUILTS}"

# Copy the system libraries to the sysroot
LIBRARIES="out/target/product/generic_x86_64/system/lib64/android.hardware.gnss@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.gnss@1.1.so
out/target/product/generic_x86_64/system/lib64/android.hardware.gnss@2.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.radio@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.sensors@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.vibrator@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi@1.1.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi@1.2.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi@1.3.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi.hostapd@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi.hostapd@1.1.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi.supplicant@1.0.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi.supplicant@1.1.so
out/target/product/generic_x86_64/system/lib64/android.hardware.wifi.supplicant@1.2.so
out/target/product/generic_x86_64/system/lib64/binder_b2g_connectivity_interface-cpp.so
out/target/product/generic_x86_64/system/lib64/binder_b2g_telephony_interface-cpp.so
out/target/product/generic_x86_64/system/lib64/dnsresolver_aidl_interface-V2-cpp.so
out/target/product/generic_x86_64/system/lib64/libaudioclient.so
out/target/product/generic_x86_64/system/lib64/libbase.so
out/target/product/generic_x86_64/system/lib64/libbinder.so
out/target/product/generic_x86_64/system/lib64/libcamera_client.so
out/target/product/generic_x86_64/system/lib64/libc++.so
out/target/product/generic_x86_64/system/lib64/libcutils.so
out/target/product/generic_x86_64/system/lib64/libgui.so
out/target/product/generic_x86_64/system/lib64/libhardware_legacy.so
out/target/product/generic_x86_64/system/lib64/libhardware.so
out/target/product/generic_x86_64/system/lib64/libhidlbase.so
out/target/product/generic_x86_64/system/lib64/libhidlmemory.so
out/target/product/generic_x86_64/system/lib64/libhidltransport.so
out/target/product/generic_x86_64/system/lib64/libhwbinder.so
out/target/product/generic_x86_64/system/lib64/libmedia_omx.so
out/target/product/generic_x86_64/system/lib64/libmedia.so
out/target/product/generic_x86_64/system/lib64/libstagefright_foundation.so
out/target/product/generic_x86_64/system/lib64/libstagefright_omx.so
out/target/product/generic_x86_64/system/lib64/libstagefright.so
out/target/product/generic_x86_64/system/lib64/libsuspend.so
out/target/product/generic_x86_64/system/lib64/libsysutils.so
out/target/product/generic_x86_64/system/lib64/libui.so
out/target/product/generic_x86_64/system/lib64/libutils.so
out/target/product/generic_x86_64/system/lib64/libvold_binder_shared.so
out/target/product/generic_x86_64/system/lib64/libwificond_ipc_shared.so
out/target/product/generic_x86_64/system/lib64/netd_aidl_interface-V2-cpp.so
out/target/product/generic_x86_64/system/lib64/netd_event_listener_interface-V1-cpp.so"

copy_to_sysroot_full_path "${LIBRARIES}"

# Store the system includes in the sysroot
INCLUDE_FOLDERS="frameworks/av/camera/include
frameworks/av/include
frameworks/av/media/libaudioclient/include
frameworks/av/media/libmedia/aidl
frameworks/av/media/libmedia/include
frameworks/av/media/libstagefright/foundation/include
frameworks/av/media/libstagefright/include
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
gonk-misc/libcarthage/HWC
gonk-misc/libcarthage/include
hardware/libhardware/include
hardware/libhardware_legacy/include
system/connectivity
system/core/base/include
system/core/include
system/core/libcutils/include
system/core/liblog/include
system/core/libprocessgroup/include
system/core/libsuspend/include
system/core/libsync/include
system/core/libsystem/include
system/core/libsysutils/include
system/core/libutils/include
system/libhidl/base/include
system/libhidl/transport/token/1.0/utils/include
system/media/audio/include
system/media/camera/include"

copy_to_sysroot "${INCLUDE_FOLDERS}" "include"

# Store the generated HIDL headers in the sysroot
GENERATED_AIDL_HEADERS="out/soong/.intermediates/frameworks/av/camera/libcamera_client/android_x86_64_core_shared/gen/aidl
out/soong/.intermediates/frameworks/av/media/libaudioclient/libaudioclient/android_x86_64_core_shared/gen/aidl
out/soong/.intermediates/frameworks/av/media/libmedia/libmedia_omx/android_x86_64_core_shared/gen/aidl
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_connectivity_interface-cpp-source/gen/include
out/soong/.intermediates/gonk-misc/gonk-binder/binder_b2g_telephony_interface-cpp-source/gen/include
out/soong/.intermediates/system/connectivity/wificond/libwificond_ipc/android_x86_64_core_static/gen/aidl
out/soong/.intermediates/system/netd/resolv/dnsresolver_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/netd/server/netd_aidl_interface-V2-cpp-source/gen/include
out/soong/.intermediates/system/netd/server/netd_event_listener_interface-V1-cpp-source/gen/include
out/soong/.intermediates/system/vold/libvold_binder_shared/android_x86_64_core_shared/gen/aidl"

copy_to_sysroot "${GENERATED_AIDL_HEADERS}" "include"

# Store the generated AIDL headers in the sysroot
GENERATED_HIDL_HEADERS="out/soong/.intermediates/system/libhidl/transport/base/1.0/android.hidl.base@1.0_genc++_headers/gen
out/soong/.intermediates/system/libhidl/transport/manager/1.0/android.hidl.manager@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/1.0/android.hardware.gnss@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/1.1/android.hardware.gnss@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/2.0/android.hardware.gnss@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/measurement_corrections/1.0/android.hardware.gnss.measurement_corrections@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/gnss/visibility_control/1.0/android.hardware.gnss.visibility_control@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/1.0/android.hardware.graphics.bufferqueue@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/bufferqueue/2.0/android.hardware.graphics.bufferqueue@2.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.0/android.hardware.graphics.common@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.1/android.hardware.graphics.common@1.1_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/graphics/common/1.2/android.hardware.graphics.common@1.2_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/1.0/android.hardware.media@1.0_genc++_headers/gen
out/soong/.intermediates/hardware/interfaces/media/omx/1.0/android.hardware.media.omx@1.0_genc++_headers/gen
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
out/soong/.intermediates/hardware/interfaces/wifi/supplicant/1.2/android.hardware.wifi.supplicant@1.2_genc++_headers/gen"

copy_to_sysroot "${GENERATED_HIDL_HEADERS}" "include"

tar -c b2g-sysroot | $GECKO_PATH/taskcluster/scripts/misc/zstdpy > "b2g-sysroot.tar.zst"

# Bundle both tarballs into a single artifact
mkdir -p "$UPLOAD_DIR"
tar -c "b2g-sysroot.tar.zst" "b2g-emulator.tar.zst" > "$UPLOAD_DIR/b2g-build.tar"
