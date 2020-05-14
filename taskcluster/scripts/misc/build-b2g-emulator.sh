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
index 1c8a3cf..f5daaab 100644
--- a/Android.mk
+++ b/Android.mk
@@ -160,15 +160,15 @@ endif
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
@@ -226,24 +226,7 @@ endif
 
 .PHONY: $(LOCAL_BUILT_MODULE)
 $(LOCAL_BUILT_MODULE): $(TARGET_CRTBEGIN_DYNAMIC_O) $(TARGET_CRTEND_O) $(addprefix $(TARGET_OUT_SHARED_LIBRARIES)/,$(GECKO_LIB_DEPS)) $(GECKO_LIB_STATIC)
-	echo "export GECKO_OBJDIR=$(abspath $(GECKO_OBJDIR))"; \
-	echo "export GONK_PRODUCT_NAME=$(TARGET_DEVICE)"; \
-	echo "export GONK_PATH=$(abspath .)"; \
-	echo "export PLATFORM_VERSION=$(PLATFORM_SDK_VERSION)"; \
-	echo "export TARGET_ARCH=$(TARGET_ARCH)"; \
-	echo "export TARGET_ARCH_VARIANT=$(TARGET_ARCH_VARIANT)"; \
-	echo "export TARGET_CPU_VARIANT=$(TARGET_CPU_VARIANT)"; \
-	unset CC_WRAPPER && unset CXX_WRAPPER && \
-	export GECKO_OBJDIR="$(abspath $(GECKO_OBJDIR))" && \
-	export GONK_PATH="$(abspath .)" && \
-	export GONK_PRODUCT_NAME="$(TARGET_DEVICE)" && \
-	export PLATFORM_VERSION="$(PLATFORM_SDK_VERSION)" && \
-	export TARGET_ARCH="$(TARGET_ARCH)" && \
-	export TARGET_ARCH_VARIANT="$(TARGET_ARCH_VARIANT)" && \
-	export TARGET_CPU_VARIANT="$(TARGET_CPU_VARIANT)" && \
-	(cd gecko ; $(SHELL) build-b2g.sh) && \
-	(cd gecko ; $(SHELL) build-b2g.sh package) && \
-	mkdir -p $(@D) && cp $(GECKO_OBJDIR)/dist/b2g-*.tar.gz $@
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

# Bundle the tarballs into a single artifact
mkdir -p "$UPLOAD_DIR"
tar -c "b2g-emulator.tar.zst" > "$UPLOAD_DIR/b2g-build.tar"
