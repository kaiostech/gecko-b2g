/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const DATACALLINTERFACE_CID = Components.ID(
  "{ff669306-4390-462a-989b-ba37fc42153f}"
);
const DATACALLINTERFACESERVICE_CID = Components.ID(
  "{e23e9337-592d-40b9-8cef-7bd47c28b72e}"
);

const TOPIC_XPCOM_SHUTDOWN = "xpcom-shutdown";
const TOPIC_PREF_CHANGED = "nsPref:changed";

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL", function () {
  let obj = ChromeUtils.import("resource://gre/modules/ril_consts.js");
  return obj;
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gRil",
  "@mozilla.org/ril;1",
  "nsIRadioInterfaceLayer"
);

//var DEBUG = RIL_DEBUG.DEBUG_RIL;
var DEBUG = true;
function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref = Services.prefs.getBoolPref(
    RIL_DEBUG.PREF_RIL_DEBUG_ENABLED,
    false
  );
  DEBUG = debugPref || RIL_DEBUG.DEBUG_RIL;
}
updateDebugFlag();

function DataCall(aAttributes) {
  for (let key in aAttributes) {
    if (key === "pdpType") {
      // Convert pdp type into constant int value.
      this[key] = lazy.RIL.RIL_DATACALL_PDP_TYPES.indexOf(aAttributes[key]);
      continue;
    }
    if (key === "addresses") {
      this[key] = aAttributes[key][0] ? aAttributes[key][0].address : "";
      for (let index = 1; index < aAttributes[key].length; index++) {
        this[key] = this[key] + " " + aAttributes[key][index].address;
      }
    } else {
      this[key] = aAttributes[key];
    }
  }
}
DataCall.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDataCall]),

  failCause: Ci.nsIDataCallInterface.DATACALL_FAIL_NONE,
  suggestedRetryTime: -1,
  cid: -1,
  active: -1,
  pdpType: -1,
  ifname: null,
  addresses: null,
  dnses: null,
  gateways: null,
  pcscf: null,
  mtu: -1,
  mtuV4: -1,
  mtuV6: -1,
  pduSessionId: 0,
  handoverFailureMode: 0,
  trafficDescriptors: [],
  sliceInfo: null,
  defaultQos: null,
  qosSessions: [],
};

function SliceInfo(aAttributes) {
  for (let key in aAttributes) {
    this[key] = aAttributes[key];
  }
}
SliceInfo.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISliceInfo]),

  sst: -1,
  sliceDifferentiator: -1,
  mappedHplmnSst: -1,
  mappedHplmnSD: -1,
  status: -1,
};

function DataCallInterfaceService() {
  this._dataCallInterfaces = [];

  let numClients = lazy.gRil.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    this._dataCallInterfaces.push(new DataCallInterface(i));
  }

  Services.obs.addObserver(this, TOPIC_XPCOM_SHUTDOWN);
  Services.prefs.addObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
}
DataCallInterfaceService.prototype = {
  classID: DATACALLINTERFACESERVICE_CID,
  QueryInterface: ChromeUtils.generateQI(
    [Ci.nsIDataCallInterfaceService, Ci.nsIGonkDataCallInterfaceService],
    Ci.nsIObserver
  ),

  // An array of DataCallInterface instances.
  _dataCallInterfaces: null,

  debug(aMessage) {
    dump("-*- DataCallInterfaceService: " + aMessage + "\n");
  },

  // nsIDataCallInterfaceService

  getDataCallInterface(aClientId) {
    let dataCallInterface = this._dataCallInterfaces[aClientId];
    if (!dataCallInterface) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    return dataCallInterface;
  },

  // nsIGonkDataCallInterfaceService

  notifyDataCallListChanged(aClientId, aCount, aDataCalls) {
    let dataCallInterface = this.getDataCallInterface(aClientId);
    dataCallInterface.handleDataCallListChanged(aCount, aDataCalls);
  },

  // nsIObserver

  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case TOPIC_PREF_CHANGED:
        if (aData === RIL_DEBUG.PREF_RIL_DEBUG_ENABLED) {
          updateDebugFlag();
        }
        break;
      case TOPIC_XPCOM_SHUTDOWN:
        Services.prefs.removeObserver(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED, this);
        Services.obs.removeObserver(this, TOPIC_XPCOM_SHUTDOWN);
        break;
    }
  },
};

function DataCallInterface(aClientId) {
  this._clientId = aClientId;
  this._radioInterface = lazy.gRil.getRadioInterface(aClientId);
  this._listeners = [];

  if (DEBUG) {
    this.debug("DataCallInterface: " + aClientId);
  }
}
DataCallInterface.prototype = {
  classID: DATACALLINTERFACE_CID,
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDataCallInterface]),

  debug(aMessage) {
    dump("-*- DataCallInterface[" + this._clientId + "]: " + aMessage + "\n");
  },

  _clientId: -1,

  _radioInterface: null,

  _listeners: null,

  // nsIDataCallInterface

  setupDataCall(
    aRadioTechnology,
    aAccessNetworkType,
    aProfile,
    aModemConfig,
    aAllowRoaming,
    aIsRoaming,
    aReason,
    aAddresses,
    aDnses,
    aPduSessionId,
    aSliceInfo,
    aTrafficDescriptor,
    aMatchAllRuleAllowed,
    aCallback
  ) {
    this._radioInterface.sendWorkerMessage(
      "setupDataCall",
      {
        radioTechnology: aRadioTechnology,
        accessNetworkType: aAccessNetworkType,
        profile: aProfile,
        modemConfig: aModemConfig,
        allowRoaming: aAllowRoaming,
        isRoaming: aIsRoaming,
        reason: aReason,
        addresses: aAddresses,
        dnses: aDnses,
        pduSessionId: aPduSessionId,
        sliceInfo: aSliceInfo,
        trafficDescriptor: aTrafficDescriptor,
        matchAllRuleAllowed: aMatchAllRuleAllowed,
      },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          let dataCall = new DataCall(aResponse.dcResponse);
          aCallback.notifySetupDataCallSuccess(dataCall);
        }
      }
    );
  },

  deactivateDataCall(aCid, aReason, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "deactivateDataCall",
      {
        cid: aCid,
        reason: aReason,
      },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          aCallback.notifySuccess();
        }
      }
    );
  },

  getDataCallList(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getDataCallList",
      null,
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          let dataCalls = aResponse.datacalls.map(
            dataCall => new DataCall(dataCall)
          );
          aCallback.notifyGetDataCallListSuccess(dataCalls.length, dataCalls);
        }
      }
    );
  },

  setDataRegistration(aAttach, aCallback) {
    this._radioInterface.sendWorkerMessage(
      "setDataRegistration",
      {
        attach: aAttach,
      },
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          aCallback.notifySuccess();
        }
      }
    );
  },

  setInitialAttachApn(aProfile, aIsRoaming) {
    this._radioInterface.sendWorkerMessage(
      "setInitialAttachApn",
      {
        profile: aProfile,
        isRoaming: aIsRoaming,
      },
      aResponse => {
        if (aResponse.errorMsg) {
          if (DEBUG) {
            this.debug("setInitialAttachApn errorMsg : " + aResponse.errorMsg);
          }
        }
      }
    );
  },

  getSlicingConfig(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "getSlicingConfig",
      {},
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
        } else {
          let sliceInfos = [];
          for (let i = 0; i < aResponse.slicingConfig.sliceInfo.length; i++) {
            sliceInfos.push(
              new SliceInfo(aResponse.slicingConfig.sliceInfo[i])
            );
          }
          this.debug(
            "notifygetSlicingConfigSuccess with sliceInfos :" +
              JSON.stringify(sliceInfos)
          );
          aCallback.notifyGetSlicingConfigSuccess(sliceInfos);
        }
      }
    );
  },

  handleDataCallListChanged(aCount, aDataCalls) {
    this._notifyAllListeners("notifyDataCallListChanged", [aCount, aDataCalls]);
  },

  _notifyAllListeners(aMethodName, aArgs) {
    let listeners = this._listeners.slice();
    for (let listener of listeners) {
      if (!this._listeners.includes(listener)) {
        // Listener has been unregistered in previous run.
        continue;
      }

      let handler = listener[aMethodName];
      try {
        handler.apply(listener, aArgs);
      } catch (e) {
        if (DEBUG) {
          this.debug(
            "listener for " + aMethodName + " threw an exception: " + e
          );
        }
      }
    }
  },

  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      return;
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);
    if (index >= 0) {
      this._listeners.splice(index, 1);
    }
  },
};

var EXPORTED_SYMBOLS = ["DataCallInterfaceService"];
