/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

export const TetheringConfigStore = (function() {
  var tetheringConfigStore = {};

  // Sync from TetheringService
  const TETHERING_TYPE_WIFI = "WiFi";
  const TETHERING_TYPE_USB = "USB";

  const WIFI_TETHERING_CONFIG_PATH =
    "/data/misc/tethering/wifi_tethering_config.json";
  const USB_TETERING_CONFIG_PATH =
    "/data/misc/tethering/usb_tethering_config.json";

  // TetheringConfigStore parameters

  // TetheringConfigStore functions
  tetheringConfigStore.read = read;
  tetheringConfigStore.write = write;
  tetheringConfigStore.TETHERING_TYPE_WIFI = TETHERING_TYPE_WIFI;
  tetheringConfigStore.TETHERING_TYPE_USB = TETHERING_TYPE_USB;

  function read(aType) {
    let path =
      aType == TETHERING_TYPE_WIFI
        ? WIFI_TETHERING_CONFIG_PATH
        : USB_TETERING_CONFIG_PATH;
    let tetheringConfigFile = new FileUtils.File(path);
    let networks;
    if (tetheringConfigFile.exists()) {
      let fstream = Cc[
        "@mozilla.org/network/file-input-stream;1"
      ].createInstance(Ci.nsIFileInputStream);
      let sstream = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
        Ci.nsIScriptableInputStream
      );

      const RO = 0x01;
      const READ_OTHERS = 4;

      fstream.init(tetheringConfigFile, RO, READ_OTHERS, 0);
      sstream.init(fstream);
      let data = sstream.read(sstream.available());
      sstream.close();
      fstream.close();
      networks = JSON.parse(data);
    } else {
      networks = null;
    }

    return networks;
  }

  function write(aType, aConfig, aCallback) {
    if (!aConfig) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    let path =
      aType == TETHERING_TYPE_WIFI
        ? WIFI_TETHERING_CONFIG_PATH
        : USB_TETERING_CONFIG_PATH;
    let tetheringConfigFile = new FileUtils.File(path);
    // Initialize the file output stream.
    let ostream = FileUtils.openSafeFileOutputStream(tetheringConfigFile);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc[
      "@mozilla.org/intl/scriptableunicodeconverter"
    ].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(JSON.stringify(aConfig));
    NetUtil.asyncCopy(istream, ostream, function(rc) {
      FileUtils.closeSafeFileOutputStream(ostream);
      if (aCallback) {
        aCallback();
      }
    });
  }

  return tetheringConfigStore;
})();
