/* eslint-disable no-undef */
// Stolen from SpecialPowers, since at this point we don't know we're in a test.
var isMainProcess = function() {
  try {
    return (
      // eslint-disable-next-line mozilla/use-services
      Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
        .processType == Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
    );
  } catch (e) {}
  return true;
};

var fakeLibcUtils = {
  _props_: {},
  property_set(name, value) {
    dump(
      "property_set('" + name + "', '" + value + "' [" + typeof value + "]);\n"
    );
    this._props_[name] = value;
  },
  property_get(name, defaultValue) {
    dump("property_get('" + name + "', '" + defaultValue + "');\n");
    if (Object.keys(this._props_).indcludes(name)) {
      return this._props_[name];
    }
    return defaultValue;
  },
};

var kUserValues;

function installUserValues(next) {
  var fakeValues = {
    settings: {
      "lockscreen.locked": false,
      "lockscreen.lock-immediately": false,
    },
    prefs: {
      "b2g.killswitch.test": false,
    },
    properties: {
      "dalvik.vm.heapmaxfree": "32m",
      "dalvik.vm.isa.arm.features": "fdiv",
      "dalvik.vm.lockprof.threshold": "5000",
      "net.bt.name": "BTAndroid",
      "dalvik.vm.stack-trace-file": "/data/anr/stack-traces.txt",
    },
  };

  IOUtils.write(kUserValues, JSON.stringify(fakeValues)).then(() => {
    next();
  });
}

if (isMainProcess()) {
  ChromeUtils.import("resource://gre/modules/SettingsRequestManager.jsm");
  ChromeUtils.import("resource://gre/modules/KillSwitchMain.jsm");

  kUserValues = PathUtils.join(PathUtils.profileDir, "killswitch.json");

  installUserValues(() => {
    KillSwitchMain._libcutils = fakeLibcUtils;
  });
}
