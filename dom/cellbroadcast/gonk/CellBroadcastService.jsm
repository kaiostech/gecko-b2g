/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const RIL = ChromeUtils.import("resource://gre/modules/ril_consts.js");

const lazy = {};

XPCOMUtils.defineLazyGetter(lazy, "RIL_DEBUG", function() {
  // eslint-disable-next-line mozilla/reject-chromeutils-import-params
  let obj = ChromeUtils.import("resource://gre/modules/ril_consts_debug.js");
  return obj;
});

const kSettingsCellBroadcastDisabled = "ril.cellbroadcast.disabled";
const kSettingsCellBroadcastSearchList = "ril.cellbroadcast.searchlist";
const kPrefAppCBConfigurationEnabled = "dom.app_cb_configuration";
//Geo-Fencing Maximum wait time(second).
const kPrefCellBroadcastMaxWaitTime = "dom.cellbroadcast.maximumWaitTime";
//GeoFencing Threshold in meters.
const kPrefCellBroadcastGeoFencingThreshold =
  "dom.cellbroadcast.geoFencingThreshold";

const DEFAULT_MAXIMUM_WAIT_TIME = 30; // 30 seconds

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gCellbroadcastMessenger",
  "@mozilla.org/ril/system-messenger-helper;1",
  "nsICellbroadcastMessenger"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gSettingsManager",
  "@mozilla.org/sidl-native/settings;1",
  "nsISettingsManager"
);

XPCOMUtils.defineLazyGetter(lazy, "gRadioInterfaceLayer", function() {
  let ril = { numRadioInterfaces: 0 };
  try {
    ril = Cc["@mozilla.org/ril;1"].getService(Ci.nsIRadioInterfaceLayer);
  } catch (e) {}
  return ril;
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gMobileMessageDBService",
  "@mozilla.org/mobilemessage/gonkmobilemessagedatabaseservice;1",
  "nsIGonkMobileMessageDatabaseService"
);

XPCOMUtils.defineLazyGetter(lazy, "gGeolocation", function() {
  let geolocation = {};
  try {
    geolocation = Cc["@mozilla.org/geolocation;1"].getService(Ci.nsISupports);
  } catch (e) {}
  return geolocation;
});

const GONK_CELLBROADCAST_SERVICE_CID = Components.ID(
  "{7ba407ce-21fd-11e4-a836-1bfdee377e5c}"
);

const NS_XPCOM_SHUTDOWN_OBSERVER_ID = "xpcom-shutdown";

var DEBUG;
function debug(s) {
  dump("CellBroadcastService: " + s);
}

var CB_SEARCH_LIST_GECKO_CONFIG = false;
const CB_HANDLED_WAKELOCK_TIMEOUT = 10000;
const TYPE_ACTIVE_ALERT_SHARE_WAC = 2;

function CellBroadcastService() {
  this._listeners = [];

  this._updateDebugFlag();

  Services.obs.addObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);

  try {
    CB_SEARCH_LIST_GECKO_CONFIG =
      Services.prefs.getBoolPref(kPrefAppCBConfigurationEnabled) || false;
  } catch (e) {}

  if (CB_SEARCH_LIST_GECKO_CONFIG) {
    /**
     * Read the settings of the toggle of Cellbroadcast Service:
     *
     * Simple Format: Boolean
     *   true if CBS is disabled. The value is applied to all RadioInterfaces.
     * Enhanced Format: Array of Boolean
     *   Each element represents the toggle of CBS per RadioInterface.
     */
    this.getSettingValue(kSettingsCellBroadcastDisabled);

    /**
     * Read the Cell Broadcast Search List setting to set listening channels:
     *
     * Simple Format:
     *   String of integers or integer ranges separated by comma.
     *   For example, "1, 2, 4-6"
     * Enhanced Format:
     *   Array of Objects with search lists specified in gsm/cdma network.
     *   For example, [{'gsm' : "1, 2, 4-6", 'cdma' : "1, 50, 99"},
     *                 {'cdma' : "3, 6, 8-9"}]
     *   This provides the possibility to
     *   1. set gsm/cdma search list individually for CDMA+LTE device.
     *   2. set search list per RadioInterface.
     */
    this.getSettingValue(kSettingsCellBroadcastSearchList);

    this.addSettingObserver(kSettingsCellBroadcastDisabled);
    this.addSettingObserver(kSettingsCellBroadcastSearchList);
  }
}
CellBroadcastService.prototype = {
  classID: GONK_CELLBROADCAST_SERVICE_CID,

  QueryInterface: ChromeUtils.generateQI([
    Ci.nsICellBroadcastService,
    Ci.nsIGonkCellBroadcastService,
    Ci.nsISettingsObserver,
    Ci.nsIObserver,
  ]),

  // An array of nsICellBroadcastListener instances.
  _listeners: null,

  // Setting values of Cell Broadcast SearchList.
  _cellBroadcastSearchList: null,

  _updateDebugFlag() {
    try {
      DEBUG =
        lazy.RIL_DEBUG.DEBUG_RIL ||
        Services.prefs.getBoolPref(lazy.RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
    } catch (e) {}
  },

  _retrieveSettingValueByClient(aClientId, aSettings) {
    return Array.isArray(aSettings) ? aSettings[aClientId] : aSettings;
  },

  /**
   * Helper function to set CellBroadcastDisabled to each RadioInterface.
   */
  setCellBroadcastDisabled(aSettings) {
    if (!RIL.CB_SEARCH_LIST_GECKO_CONFIG) {
      return;
    }

    let numOfRilClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
    for (let clientId = 0; clientId < numOfRilClients; clientId++) {
      lazy.gRadioInterfaceLayer
        .getRadioInterface(clientId)
        .sendWorkerMessage("setCellBroadcastDisabled", {
          disabled: this._retrieveSettingValueByClient(clientId, aSettings),
        });
    }
  },

  /**
   * Helper function to set CellBroadcastSearchList to each RadioInterface.
   */
  setCellBroadcastSearchList(aSettings) {
    if (!RIL.CB_SEARCH_LIST_GECKO_CONFIG) {
      return;
    }

    let numOfRilClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
    let responses = [];
    for (let clientId = 0; clientId < numOfRilClients; clientId++) {
      let newSearchList = this._retrieveSettingValueByClient(
        clientId,
        aSettings
      );
      let oldSearchList = this._retrieveSettingValueByClient(
        clientId,
        this._cellBroadcastSearchList
      );

      if (
        newSearchList == oldSearchList ||
        (newSearchList &&
          oldSearchList &&
          newSearchList.gsm == oldSearchList.gsm &&
          newSearchList.cdma == oldSearchList.cdma)
      ) {
        responses.push({});
        return;
      }

      lazy.gRadioInterfaceLayer.getRadioInterface(clientId).sendWorkerMessage(
        "setCellBroadcastSearchList",
        { searchList: newSearchList },
        function callback(aResponse) {
          if (DEBUG && aResponse.errorMsg) {
            debug(
              "Failed to set new search list: " +
                newSearchList +
                " to client id: " +
                clientId
            );
          }

          responses.push(aResponse);
          if (responses.length == numOfRilClients) {
            let successCount = 0;
            for (let i = 0; i < responses.length; i++) {
              if (!responses[i].errorMsg) {
                successCount++;
              }
            }
            if (successCount == numOfRilClients) {
              this._cellBroadcastSearchList = aSettings;
            } else {
              // Rollback the change when failure.
              // TODO Renable it after nsISetting get ready
              // let lock = gSettingsService.createLock();
              // lock.set(kSettingsCellBroadcastSearchList,
              //          this._cellBroadcastSearchList, null);
            }
          }

          return false;
        }.bind(this)
      );
    }
  },

  /**
   * nsICellBroadcastService interface
   */
  registerListener(aListener) {
    if (this._listeners.includes(aListener)) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.push(aListener);
  },

  unregisterListener(aListener) {
    let index = this._listeners.indexOf(aListener);

    if (index < 0) {
      throw Components.Exception("", Cr.NS_ERROR_UNEXPECTED);
    }

    this._listeners.splice(index, 1);
  },

  setCBSearchList(aClientId, aGsmCount, aGsms, aCdmaCount, aCdmas) {
    if (DEBUG) {
      debug("setCellBroadcastSearchList: " + aClientId);
    }

    let radioInterface = this._getRadioInterface(aClientId);
    let options = {
      gsm: this._convertCBSearchList(aGsms),
      cdma: this._convertCBSearchList(aCdmas),
    };

    radioInterface.sendWorkerMessage(
      "setCellBroadcastSearchList",
      { searchList: options },
      function callback(aResponse) {
        if (DEBUG) {
          debug(
            "Set cell broadcast search list to client id: " +
              this._clientId +
              ", with errorMsg: " +
              aResponse.errorMsg
          );
        }
      }.bind(this)
    );
  },

  /**
   * To translate input cb search list array to the form ril_worker expected.
   * From (start, end, start, end) to (start-end, start-end).
   * @param {int[]} aSearchArray
   *                The start-end pair array, like [start1, end1, start2, end2...etc]
   *                Array size must be Multiple of 2
   */
  _convertCBSearchList(aSearchArray) {
    let result = [];

    for (let i = 0; i < aSearchArray.length; i += 2) {
      let start = aSearchArray[i];
      let end = aSearchArray[i + 1];
      if (start && end) {
        result.push(start.toString() + "-" + end.toString());
      } else if (start) {
        result.push(start.toString());
      } else if (end) {
        result.push(end.toString());
      }
    }

    return result.toString();
  },

  setCBDisabled(aClientId, aDisabled) {
    if (DEBUG) {
      debug("to disable cb: " + aClientId);
    }
    let radioInterface = this._getRadioInterface(aClientId);
    radioInterface.sendWorkerMessage("setCellBroadcastDisabled", {
      diesabled: true,
    });
  },

  _getRadioInterface(aClientId) {
    let numOfRilClients = lazy.gRadioInterfaceLayer.numRadioInterfaces;
    if (aClientId < 0 || aClientId >= numOfRilClients) {
      if (DEBUG) {
        debug("unexpedted client: " + aClientId);
      }
      throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
    }

    let radioInterface = lazy.gRadioInterfaceLayer.getRadioInterface(aClientId);
    if (!radioInterface) {
      if (DEBUG) {
        debug("cannot retrieve radiointerface for client: " + aClientId);
      }
      throw Components.Exception("", Cr.NS_ERROR_NOT_AVAILABLE);
    }

    return radioInterface;
  },

  // The following attributes/functions are used for acquiring/releasing the
  // CPU wake lock when the RIL handles received CB. Note that we need
  // a timer to bound the lock's life cycle to avoid exhausting the battery.
  _cbHandledWakeLock: null,
  _cbHandledWakeLockTimer: null,
  _acquireCbHandledWakeLock() {
    if (!this._cbHandledWakeLock) {
      if (DEBUG) {
        debug("Acquiring a CPU wake lock for handling CB");
      }
      this._cbHandledWakeLock = lazy.gPowerManagerService.newWakeLock("cpu");
    }
    if (!this._cbHandledWakeLockTimer) {
      if (DEBUG) {
        debug("Creating a timer for releasing the CPU wake lock");
      }
      this._cbHandledWakeLockTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }
    if (DEBUG) {
      debug("Setting the timer for releasing the CPU wake lock");
    }
    this._cbHandledWakeLockTimer.initWithCallback(
      () => this._releaseCbHandledWakeLock(),
      CB_HANDLED_WAKELOCK_TIMEOUT,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _releaseCbHandledWakeLock() {
    if (DEBUG) {
      debug("Releasing the CPU wake lock for handling CB");
    }
    if (this._cbHandledWakeLockTimer) {
      this._cbHandledWakeLockTimer.cancel();
    }
    if (this._cbHandledWakeLock) {
      this._cbHandledWakeLock.unlock();
      this._cbHandledWakeLock = null;
    }
  },

  broadcastGeometryMessage(aServiceId, aGeometryMessage) {
    let callback = (aRv, aDeletedRecord, aCellBroadCastMessage) => {
      if (DEBUG && !Components.isSuccessCode(aRv)) {
        debug("Failed to delete cellbroadcast message");
      }
    };
    lazy.gMobileMessageDBService.deleteCellBroadcastMessage(
      aGeometryMessage.serialNumber,
      aGeometryMessage.messageId,
      callback
    );
    this._broadcastCellBroadcastMessage(aServiceId, aGeometryMessage);
  },

  _performGeoFencing(
    aServiceId,
    aCellBroadcastMessage,
    aLatLng,
    aBroadcastArea
  ) {
    let thresholdInMeters = Services.prefs.getIntPref(
      kPrefCellBroadcastGeoFencingThreshold,
      0
    );
    for (let geo of aBroadcastArea) {
      if (geo.contains(aLatLng, thresholdInMeters)) {
        if (DEBUG) {
          debug(
            "Location " +
              JSON.stringify(aLatLng) +
              " inside geometry " +
              JSON.stringify(geo)
          );
        }
        this.broadcastGeometryMessage(aServiceId, aCellBroadcastMessage);
        return;
      }
    }
  },

  async _handleGeoFencingTrigger(aServiceId, aGeoTriggerMessage) {
    let cbIdentifiers = aGeoTriggerMessage.cellBroadcastIdentifiers;
    let allBroadcastMessages = [];

    let findCellbroadcastMessage = identity =>
      new Promise(resolve =>
        lazy.gMobileMessageDBService.getCellBroadcastMessage(
          identity.serialNumber,
          identity.messageIdentifier,
          function(aRv, aMessageRecord, aCellBroadCastMessage) {
            if (Components.isSuccessCode(aRv) && aMessageRecord) {
              allBroadcastMessages.push(aMessageRecord);
            }
            resolve();
          }
        )
      );
    for (let i = 0; i < cbIdentifiers.length; i++) {
      await findCellbroadcastMessage(cbIdentifiers[i]);
    }

    //handle TYPE_ACTIVE_ALERT_SHARE_WAC to share all geometries together
    let commonBroadcastArea = [];
    if (
      aGeoTriggerMessage.geoFencingTriggerType == TYPE_ACTIVE_ALERT_SHARE_WAC
    ) {
      allBroadcastMessages.forEach(msg => {
        commonBroadcastArea.push(...msg.geometries);
      });
    }
    allBroadcastMessages.forEach(msg => {
      if (commonBroadcastArea.length > 0) {
        this._handleGeometryMessage(aServiceId, msg, commonBroadcastArea);
      } else {
        this._handleGeometryMessage(aServiceId, msg);
      }
    });
  },

  _handleGeometryMessage(
    aServiceId,
    aGeometryMessage,
    aBroadcastArea = aGeometryMessage.geometries
  ) {
    if (aGeometryMessage.maximumWaitingTimeSec > 0) {
      let timeout = aGeometryMessage.maximumWaitingTimeSec * 1000;
      if (
        aGeometryMessage.maximumWaitingTimeSec ==
        RIL.GEO_FENCING_MAXIMUM_WAIT_TIME_NOT_SET
      ) {
        timeout =
          Services.prefs.getIntPref(
            kPrefCellBroadcastMaxWaitTime,
            DEFAULT_MAXIMUM_WAIT_TIME
          ) * 1000;
      }
      let options = {
        //TODO: check which accuracy we should use.
        enableHighAccuracy: false,
        timeout,
        maximumAge: 0,
      };
      let success = pos => {
        let latLng = {
          lat: pos.coords.latitude,
          lng: pos.coords.longitude,
          accuracy: pos.accuracy,
        };
        if (DEBUG) {
          debug(
            "getCurrentPosition successfully with LatLng: " +
              JSON.stringify(latLng)
          );
        }
        this._performGeoFencing(
          aServiceId,
          aGeometryMessage,
          latLng,
          aBroadcastArea
        );
      };
      let error = err => {
        if (DEBUG) {
          debug("getCurrentPosition error: " + err.message);
        }

        // Broadcast the message directly if the location is not available.
        this.broadcastGeometryMessage(aServiceId, aGeometryMessage);
      };
      lazy.gGeolocation.getCurrentPosition(success, error, options);
    } else {
      // Broadcast the message directly because no geo-fencing required.
      this.broadcastGeometryMessage(aServiceId, aGeometryMessage);
    }
  },

  _broadcastCellBroadcastMessage(aServiceId, aCellBroadcastMessage) {
    this._acquireCbHandledWakeLock();
    // Broadcast CBS System message
    lazy.gCellbroadcastMessenger.notifyCbMessageReceived(
      aServiceId,
      aCellBroadcastMessage.gsmGeographicalScope,
      aCellBroadcastMessage.messageCode,
      aCellBroadcastMessage.messageId,
      aCellBroadcastMessage.language,
      aCellBroadcastMessage.body,
      aCellBroadcastMessage.messageClass,
      aCellBroadcastMessage.timeStamp,
      aCellBroadcastMessage.cdmaServiceCategory,
      aCellBroadcastMessage.hasEtwsInfo,
      aCellBroadcastMessage.etwsWarningType,
      aCellBroadcastMessage.etwsEmergencyUserAlert,
      aCellBroadcastMessage.etwsPopup
    );

    // Notify received message to registered listener
    for (let listener of this._listeners) {
      try {
        listener.notifyMessageReceived(
          aServiceId,
          aCellBroadcastMessage.gsmGeographicalScope,
          aCellBroadcastMessage.messageCode,
          aCellBroadcastMessage.messageId,
          aCellBroadcastMessage.language,
          aCellBroadcastMessage.body,
          aCellBroadcastMessage.messageClass,
          aCellBroadcastMessage.timeStamp,
          aCellBroadcastMessage.cdmaServiceCategory,
          aCellBroadcastMessage.hasEtwsInfo,
          aCellBroadcastMessage.etwsWarningType,
          aCellBroadcastMessage.etwsEmergencyUserAlert,
          aCellBroadcastMessage.etwsPopup
        );
      } catch (e) {
        debug("listener threw an exception: " + e);
      }
    }
  },
  /**
   * nsIGonkCellBroadcastService interface
   */
  notifyMessageReceived(aServiceId, aCellBroadcastMessage) {
    if (aCellBroadcastMessage.geoFencingTriggerType) {
      this._handleGeoFencingTrigger(aServiceId, aCellBroadcastMessage);
    } else if (aCellBroadcastMessage.geometries) {
      let callback = (aRv, aSaveedRecord) => {
        if (Components.isSuccessCode(aRv)) {
          if (DEBUG) {
            debug("Cellbroadcast message saved successfully");
          }
          this._handleGeometryMessage(aServiceId, aCellBroadcastMessage);
        } else {
          if (DEBUG) {
            debug("Failed to save cellbroadcast message");
          }
          this._broadcastCellBroadcastMessage(
            aServiceId,
            aCellBroadcastMessage
          );
        }
      };
      lazy.gMobileMessageDBService.saveCellBroadcastMessage(
        aCellBroadcastMessage,
        callback
      );
    } else {
      this._broadcastCellBroadcastMessage(aServiceId, aCellBroadcastMessage);
    }
  },

  /**
   * nsIObserver interface.
   */
  observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case NS_XPCOM_SHUTDOWN_OBSERVER_ID:
        // Release the CPU wake lock for handling the received CB.
        this._releaseCbHandledWakeLock();
        Services.obs.removeObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
        this.removeSettingObserver(kSettingsCellBroadcastDisabled);
        this.removeSettingObserver(kSettingsCellBroadcastSearchList);

        // Remove all listeners.
        this._listeners = [];
        break;
    }
  },

  handleSettingChanged(aName, aResult) {
    switch (aName) {
      case kSettingsCellBroadcastSearchList:
        if (DEBUG) {
          debug(
            "'" +
              kSettingsCellBroadcastSearchList +
              "' is now " +
              JSON.stringify(aResult)
          );
        }
        let resultListObj = JSON.parse(aResult);
        this.setCellBroadcastSearchList(resultListObj);
        break;
      case kSettingsCellBroadcastDisabled:
        if (DEBUG) {
          debug(
            "'" +
              kSettingsCellBroadcastDisabled +
              "' is now " +
              JSON.stringify(aResult)
          );
        }
        let resultDisableObj = JSON.parse(aResult);
        this.setCellBroadcastDisabled(resultDisableObj);
        break;
    }
  },

  /**
   * nsISettingsObserver
   */
  observeSetting(aSettingInfo) {
    if (aSettingInfo) {
      let name = aSettingInfo.name;
      let result = aSettingInfo.value;
      this.handleSettingChanged(name, result);
    }
  },

  // Helper functions.
  getSettingValue(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug("get " + aKey + " setting.");
      }
      let self = this;
      lazy.gSettingsManager.get(aKey, {
        resolve: info => {
          self.observeSetting(info);
        },
        reject: () => {
          if (DEBUG) {
            debug("get " + aKey + " failed.");
          }
        },
      });
    }
  },

  setSettingValue(aKey, aValue) {
    if (!aKey || !aValue) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug(
          "set " + aKey + " setting with value = " + JSON.stringify(aValue)
        );
      }
      lazy.gSettingsManager.set(
        [{ name: aKey, value: JSON.stringify(aValue) }],
        {
          resolve: () => {
            if (DEBUG) {
              debug(" Set " + aKey + " succedded. ");
            }
          },
          reject: () => {
            if (DEBUG) {
              debug("Set " + aKey + " failed.");
            }
          },
        }
      );
    }
  },

  //When the setting value change would be notify by the observe function.
  addSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug("add " + aKey + " setting observer.");
      }
      lazy.gSettingsManager.addObserver(aKey, this, {
        resolve: () => {
          if (DEBUG) {
            debug("observed " + aKey + " successed.");
          }
        },
        reject: () => {
          if (DEBUG) {
            debug("observed " + aKey + " failed.");
          }
        },
      });
    }
  },

  removeSettingObserver(aKey) {
    if (!aKey) {
      return;
    }

    if (lazy.gSettingsManager) {
      if (DEBUG) {
        debug("remove " + aKey + " setting observer.");
      }
      lazy.gSettingsManager.removeObserver(aKey, this, {
        resolve: () => {
          if (DEBUG) {
            debug("remove observer " + aKey + " successed.");
          }
        },
        reject: () => {
          if (DEBUG) {
            debug("remove observer " + aKey + " failed.");
          }
        },
      });
    }
  },
};

var EXPORTED_SYMBOLS = ["CellBroadcastService"];
