/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL", function() {
  return ChromeUtils.import("resource://gre/modules/ril_consts.js");
});

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

XPCOMUtils.defineLazyGetter(lazy, "gRadioInterfaceLayer", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}
  return ril;
});

const GONK_SUBSIDY_LOCK_SERVICE_CID = Components.ID(
  "{bcac78c1-38d5-46fa-a8a4-2feae85db443}"
);

var DEBUG = RIL_DEBUG.DEBUG_RIL;
/*function debug(s) {
  dump("SubsidyLockService: " + s);
}*/

function SubsidyLockService() {
  this._providers = [];

  let numClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
  for (let i = 0; i < numClients; i++) {
    let radioInterface = lazy.gRadioInterfaceLayer.getRadioInterface(i);
    let provider = new SubsidyLockProvider(i, radioInterface);
    this._providers.push(provider);
  }
}
SubsidyLockService.prototype = {
  classID: GONK_SUBSIDY_LOCK_SERVICE_CID,

  QueryInterface: ChromeUtils.generateQI([Ci.nsISubsidyLockService]),

  // An array of SubsidyLockProvider instances.
  _providers: null,

  /**
   * nsISubsidyLockService interface.
   */

  get numItems() {
    return this._providers.length;
  },

  getItemByServiceId(aServiceId) {
    let provider = this._providers[aServiceId];
    if (!provider) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    return provider;
  },
};

function SubsidyLockProvider(aClientId, aRadioInterface) {
  this._clientId = aClientId;
  this._radioInterface = aRadioInterface;
}
SubsidyLockProvider.prototype = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsISubsidyLock]),

  _clientId: null,

  _radioInterface: null,

  /**
   * A utility function to dump debug message.
   */
  _debug(aMessage) {
    dump("SubsidyLockProvider[" + this._clientId + "]: " + aMessage + "\n");
  },

  getSubsidyLockStatus(aCallback) {
    this._radioInterface.sendWorkerMessage(
      "iccGetSubsidyLockStatus",
      null,
      aResponse => {
        if (aResponse.errorMsg) {
          aCallback.notifyError(aResponse.errorMsg);
          return;
        }

        let finalResult = [];
        for (let status of aResponse.persoStatus) {
          if (status.hasPersoStatus) {
            finalResult.push(
              this.convertGeckoPersoTypeToSubsidyLockType(status.type)
            );
          }
        }

        if (DEBUG) {
          this._debug(
            "getSubsidyLockStatus finalResult :" + JSON.stringify(finalResult)
          );
        }
        aCallback.notifyGetSubsidyLockStatusSuccess(
          finalResult.length,
          finalResult
        );
      }
    );
  },

  unlockSubsidyLock(aLockType, aPassword, aCallback) {
    if (DEBUG) {
      this._debug("unlockSubsidyLock : " + aLockType + " / " + aPassword);
    }

    let geckoPersoType = this.convertSubsidyLockTypeToGeckoPersoType(aLockType);
    this._radioInterface.sendWorkerMessage(
      "iccUnlockSubsidyLock",
      { lockType: geckoPersoType, password: aPassword },
      aResponse => {
        if (aResponse.errorMsg) {
          let retryCount =
            aResponse.remainingRetries !== undefined
              ? aResponse.remainingRetries
              : -1;
          this._debug(
            "unlockSubsidyLock" +
              " errorMsg:" +
              aResponse.errorMsg +
              " , retryCount:" +
              retryCount
          );
          aCallback.notifyUnlockSubsidyLockError(
            aResponse.errorMsg,
            retryCount
          );
          return;
        }
        aCallback.notifySuccess();
      }
    );
  },

  // Helper function

  // Convert the gecko perso type to webidl perso type.
  // Mapping to subsidylock.webidl
  // Unknown Key.
  //const long SUBSIDY_LOCK_UNKNOWN                  = 0;
  // NCK (Network Control Key).
  //const long SUBSIDY_LOCK_SIM_NETWORK              = 1;
  // NSCK (Network Subset Control Key).
  //const long SUBSIDY_LOCK_NETWORK_SUBSET           = 2;
  // CCK (Corporate Control Key).
  //const long SUBSIDY_LOCK_SIM_CORPORATE            = 3;
  // SPCK (Service Provider Control Key).
  //const long SUBSIDY_LOCK_SIM_SERVICE_PROVIDER     = 4;
  // PCK (Personalisation Control Key).
  //const long SUBSIDY_LOCK_SIM_SIM                  = 5;
  // PUK for NCK (Network Control Key).
  //const long SUBSIDY_LOCK_SIM_NETWORK_PUK          = 6;
  // PUK for NSCK (Network Subset Control Key).
  //const long SUBSIDY_LOCK_NETWORK_SUBSET_PUK       = 7;
  // PUK for CCK (Corporate Control Key).
  //const long SUBSIDY_LOCK_SIM_CORPORATE_PUK        = 8;
  // PUK for SPCK (Service Provider Control Key).
  //const long SUBSIDY_LOCK_SIM_SERVICE_PROVIDER_PUK = 9;
  // PUK for PCK (Personalisation Control Key).
  //const long SUBSIDY_LOCK_SIM_SIM_PUK              = 10;
  convertGeckoPersoTypeToSubsidyLockType(aType) {
    switch (aType) {
      case lazy.RIL.GECKO_CARDLOCK_NCK:
        return 1;
      case lazy.RIL.GECKO_CARDLOCK_NSCK:
        return 2;
      case lazy.RIL.GECKO_CARDLOCK_CCK:
        return 3;
      case lazy.RIL.GECKO_CARDLOCK_SPCK:
        return 4;
      default:
        return 0;
    }
  },

  convertSubsidyLockTypeToGeckoPersoType(aType) {
    switch (aType) {
      case 1:
        return lazy.RIL.GECKO_CARDLOCK_NCK;
      case 2:
        return lazy.RIL.GECKO_CARDLOCK_NSCK;
      case 3:
        return lazy.RIL.GECKO_CARDLOCK_CCK;
      case 4:
        return lazy.RIL.GECKO_CARDLOCK_SPCK;
      default:
        return 0;
    }
  },
};

var EXPORTED_SYMBOLS = ["SubsidyLockService"];
