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
mv "${MOZ_FETCHES_DIR}/gonk-misc" "${MOZ_FETCHES_DIR}/B2G/"
mv "${MOZ_FETCHES_DIR}/api-daemon" "${MOZ_FETCHES_DIR}/B2G/gonk-misc/"
cd "${MOZ_FETCHES_DIR}/B2G"
GITREPO="${MOZ_FETCHES_DIR}/manifests" REPO_SYNC_FLAGS="-j4" REPO_INIT_FLAGS="--depth=1" ./config.sh emulator-10-x86_64

# Wipe the repos, we don't need them anymore
rm -rf .repo

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

# Create a dummy b2g archive
mkdir b2g
tar cvjf b2g-dummy.tar.bz2 b2g
rm -rf b2g

# Build the emulator
env ANDROID_NDK="${MOZ_FETCHES_DIR}/android-ndk" \
    PATH="${PATH}:${MOZ_FETCHES_DIR}/rustc/bin" \
    PREFERRED_B2G="$PWD/b2g-dummy.tar.bz2" \
    USE_PREBUILT_B2G=1 \
    ./build.sh

# Package the emulator
./scripts/package-emulator.sh "b2g-emulator.tar.zst" \
  "$GECKO_PATH/taskcluster/scripts/misc/zstdpy"

###############################################################################
# Package the sysroot

env GONK_PRODUCT_NAME="generic_x86_64" \
    TARGET_ARCH_VARIANT="x86_64" \
    TARGET_ARCH="x86_64" \
    TARGET_CPU_VARIANT="x86_64" \
    $GECKO_PATH/taskcluster/scripts/misc/create-b2g-sysroot.sh
tar -c b2g-sysroot | $GECKO_PATH/taskcluster/scripts/misc/zstdpy > "b2g-sysroot.tar.zst"

# Bundle both tarballs into a single artifact
mkdir -p "$UPLOAD_DIR"
tar -c "b2g-sysroot.tar.zst" "b2g-emulator.tar.zst" > "$UPLOAD_DIR/b2g-build.tar"
