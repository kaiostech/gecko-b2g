# mozharness configuration for KaiOS x86_64  unit tests
#
# This configuration should be combined with suite definitions and other
# mozharness configuration from android_common.py, or similar.

config = {
    "uses_avd_fetch": True,
    "emulator_avd_name": "test-1",
    "emulator_process_name": "qemu-system-x86_64",
    "emulator_extra_args": "-gpu on -skip-adb-auth -verbose -show-kernel -ranchu -writable-system -selinux permissive -memory 3072 -cores 4 -feature GrallocSync,LogcatPipe,GLAsyncSwap,GLESDynamicVersion,GLDMA,EncryptUserData,IntelPerformanceMonitoringUnit,Wifi,HostComposition,DynamicPartition",
    "exes": {
        "adb": "%(abs_sdk_dir)s/platform-tools/adb",
    },
    "env": {
        "DISPLAY": ":0.0",
        "PATH": "%(PATH)s:%(abs_sdk_dir)s/emulator:%(abs_sdk_dir)s/tools:%(abs_sdk_dir)s/platform-tools",
        # "LIBGL_DEBUG": "verbose"
    },
    "bogomips_minimum": 3000,
    # in support of test-verify
    "android_version": 24,
    "is_fennec": False,
    "is_emulator": True,
    "suite_definitions": {
        "xpcshell": {
            "run_filename": "remotexpcshelltests.py",
            "testsdir": "xpcshell",
            "install": False,
            "options": [
                "--xre-path=%(xre_path)s",
                "--testing-modules-dir=%(modules_dir)s",
                "--bundle=%(installer_path)s",
                "--no-logfiles",
                "--symbols-path=%(symbols_path)s",
                "--manifest=tests/xpcshell.ini",
                "--log-raw=%(raw_log_file)s",
                "--log-raw-level=%(log_raw_level)s",
                "--log-errorsummary=%(error_summary_file)s",
                "--log-tbpl-level=%(log_tbpl_level)s",
                "--test-plugin-path=none",
                "--deviceSerial=%(device_serial)s",
                "%(xpcshell_extra)s",
            ],
        },
    },
}
