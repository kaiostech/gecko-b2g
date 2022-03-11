define build-sidl-daemon
  echo "run run-sidl-build-daemon";
  cd $(B2G_DIR)/sidl/daemon; RUST_BACKTRACE=1 BUILD_TYPE=debug RUST_LOG=debug ${CARGO_HOME}/bin/cargo build; cd ../../;
endef

define run-sidl-release
  echo "run run-sidl-release";
  cd $(B2G_DIR)/sidl; JS_BUILD_TYPE="build" ./release_libs.sh; cd ..;
endef
