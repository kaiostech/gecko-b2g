#!/bin/bash


function setup_xcompile_envs() {
    # This is the Rust target architecture, which may not directly map to the clang triple.
    export TARGET_ARCH=${TARGET_ARCH:-aarch64-unknown-linux-gnu}
    export ANDROID_API=${ANDROID_API:-29}
    export ANDROID_PLATFORM=${ANDROID_PLATFORM:-android-29}
    LIB_SUFFIX=""

    case "$TARGET_ARCH" in
    armv7-linux-androideabi)
        TARGET_TRIPLE=armv7-linux-androideabi
	TARGET_INCLUDE=arm-linux-androideabi
        TOOLCHAIN_PREFIX=armv7a-linux-androideabi${ANDROID_API}
        ;;
    aarch64-linux-android)
        TARGET_TRIPLE=aarch64-linux-android
	TARGET_INCLUDE=${TARGET_TRIPLE}
        TOOLCHAIN_PREFIX=${TARGET_TRIPLE}${ANDROID_API}
        LIB_SUFFIX=64
        ;;
    x86_64-linux-android)
        TARGET_TRIPLE=x86_64-linux-android
	TARGET_INCLUDE=${TARGET_TRIPLE}
        TOOLCHAIN_PREFIX=${TARGET_TRIPLE}${ANDROID_API}
        LIB_SUFFIX=64
        ;;
    aarch64-unknown-linux-gnu)
        # Non-android targets will use the toolchain installed in $HOME/.mozbuild
        # since it's the same as the gecko one.
        TARGET_TRIPLE=aarch64-unknown-linux-gnu
        TARGET_INCLUDE=aarch64-linux-gnu
        ;;
    esac

    HOST_OS=$(uname -s)

    # Check that the BUILD_WITH_NDK_DIR environment variable is set
    # and build the .cargo/config file from it.
    if [ -n "${BUILD_WITH_NDK_DIR}" ]; then
	if [ ! -d "${BUILD_WITH_NDK_DIR}" ]; then
            echo "${BUILD_WITH_NDK_DIR} doesn't exixt."
	    exit 1
	fi
        export TOOLCHAIN_CC=${TOOLCHAIN_PREFIX}-clang
        export TOOLCHAIN_CXX=${TOOLCHAIN_PREFIX}-clang++
        export SYSROOT=${BUILD_WITH_NDK_DIR}/toolchains/llvm/prebuilt/linux-x86_64/sysroot
        export SYS_INCLUDE_DIR=${SYSROOT}/usr/include
        export ANDROID_NDK=${BUILD_WITH_NDK_DIR}
        export PATH=${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/bin:${PATH}

        echo "Building for ${TARGET_TRIPLE} using NDK '${BUILD_WITH_NDK_DIR}'"
    elif [ -n "${MOZBUILD}" ]; then
        export TOOLCHAIN_CC=clang
        export TOOLCHAIN_CXX=clang++
        export SYSROOT=${MOZBUILD}/sysroot-${TARGET_INCLUDE}
        export SYS_INCLUDE_DIR=${SYSROOT}/usr/include
        export PATH=${MOZBUILD}/clang/bin:${PATH}

        echo "Building for ${TARGET_TRIPLE} using MOZBUILD '${MOZBUILD}'"
    else
        echo "Set BUILD_WITH_NDK_DIR to your ndk directory to build, or MOZBUILD for non-Android targets."
        exit 2
    fi

    XCFLAGS="--sysroot=${SYSROOT} -I${SYS_INCLUDE_DIR} -I${SYS_INCLUDE_DIR}/${TARGET_INCLUDE} --target=${TARGET_TRIPLE}"

    export GIT_BUILD_INFO=$(
        git log -n 1 --pretty=format:"%H "
        date +%d/%m/%Y-%H:%M:%S
    )
}

function xcompile() {
    export CARGO_BUILD_TARGET=${TARGET_TRIPLE}
    export CARGO_CONFIG=$(pwd)/.cargo/config

    echo "Creating '$CARGO_CONFIG'"
    mkdir -p $(pwd)/.cargo
    cat <<EOF >>$CARGO_CONFIG

[target.${TARGET_TRIPLE}]
linker = "${TOOLCHAIN_CC}"
rustflags = [
  "-C", "link-arg=-fuse-ld=lld",
  "-C", "link-arg=--target=${TARGET_TRIPLE}",
  "-C", "link-arg=--sysroot=${SYSROOT}",
  "-C", "link-arg=-L",
  "-C", "link-arg=${SYSROOT}/usr/lib",
  "-C", "link-arg=-L",
  "-C", "link-arg=${SYSROOT}/usr/lib/${TARGET_INCLUDE}",
  "-C", "link-arg=-L",
  "-C", "link-arg=${SYSROOT}/usr/lib/gcc/${TARGET_INCLUDE}/8/",
  "-C", "opt-level=z",
]
EOF

    export CC=${TOOLCHAIN_CC}
    export CXX=${TOOLCHAIN_CXX}
    export LD=${TOOLCHAIN_CC}

    # And set CFLAGS again for the remaining crates.
    export CFLAGS=${XCFLAGS}

    export TARGET_CC=${TOOLCHAIN_CC}
    export TARGET_LD=${TOOLCHAIN_CC}

    cat <<EOF >$(pwd)/env.txt
export CARGO_BUILD_TARGET=${TARGET_TRIPLE}
export CARGO_CONFIG=$(pwd)/.cargo/config
export CC=${TOOLCHAIN_CC}
export CXX=${TOOLCHAIN_CXX}
export LD=${TOOLCHAIN_CC}
export CFLAGS=${XCFLAGS}
export TARGET_CC=${TOOLCHAIN_CC}
export TARGET_LD=${TOOLCHAIN_CC}
EOF

    printenv
    rustc --version
    cargo --version
    cargo build --target=${TARGET_TRIPLE} ${OPT}
}

function xstrip() {
    echo "Stripping with `which llvm-strip`"
    # Explicitely strip the binary since even release builds have symbols.
    llvm-strip $1
}

STRIP=
OPT=
BUILD_TYPE=debug
while [[ $# -gt 0 ]]; do
    case "$1" in
    --release)
        OPT="$OPT --release"
        BUILD_TYPE=release
        ;;
    --strip)
        STRIP=yes
        ;;
    --no-default-features)
        OPT="$OPT --no-default-features"
        ;;
    esac
    shift
done

echo "Doing a ${BUILD_TYPE} build."

setup_xcompile_envs
xcompile 

binary=./target/${TARGET_ARCH}/${BUILD_TYPE}/
if [ "${STRIP}" = "yes" ]; then
    xstrip ${binary}/b2ghald
    xstrip ${binary}/b2ghalctl
fi


