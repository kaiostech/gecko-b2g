/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { IndexedDBHelper } from "resource://gre/modules/IndexedDBHelper.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  ActivitiesServiceFilter:
    "resource://gre/modules/ActivitiesServiceFilter.sys.mjs",
});

const DEBUG = Services.prefs.getBoolPref("dom.activity.debug", false);
function debug(aMsg) {
  dump(`ActivitiesService: ${aMsg}\n`);
}

const DB_NAME = "activities";
const DB_VERSION = 1;
const STORE_NAME = "activities";

function ActivitiesDb() {}

ActivitiesDb.prototype = {
  __proto__: IndexedDBHelper.prototype,

  init: function actdb_init() {
    this.initDBHelper(DB_NAME, DB_VERSION, [STORE_NAME]);
  },

  /**
   * Create the initial database schema.
   *
   * The schema of records stored is as follows:
   *
   * {
   *  id:                  String
   *  manifest:            String
   *  name:                String
   *  icon:                String
   *  description:         jsval
   * }
   */
  upgradeSchema: function actdb_upgradeSchema(
    aTransaction,
    aDb,
    aOldVersion,
    aNewVersion
  ) {
    DEBUG && debug("Upgrade schema " + aOldVersion + " -> " + aNewVersion);

    let self = this;

    /**
     * WARNING!! Before upgrading the Activities DB take into account that an
     * OTA unregisters all the activities and reinstalls them during the first
     * run process. Check Bug 1193503.
     */

    function upgrade(currentVersion) {
      let next = upgrade.bind(self, currentVersion + 1);
      switch (currentVersion) {
        case 0:
          self.createSchema(aDb, next);
          break;
      }
    }

    upgrade(aOldVersion);
  },

  createSchema(aDb, aNext) {
    let objectStore = aDb.createObjectStore(STORE_NAME, { keyPath: "id" });

    // indexes
    objectStore.createIndex("name", "name", { unique: false });
    objectStore.createIndex("manifest", "manifest", { unique: false });

    DEBUG && debug("Created object stores and indexes");

    aNext();
  },

  // unique ids made of (uri, action)
  createId: function actdb_createId(aObject) {
    let hasher = Cc["@mozilla.org/security/hash;1"].createInstance(
      Ci.nsICryptoHash
    );
    hasher.init(hasher.SHA1);

    // add uri and action to the hash
    ["manifest", "name", "description"].forEach(function (aProp) {
      if (!aObject[aProp]) {
        return;
      }

      let property = aObject[aProp];
      if (aProp == "description") {
        property = JSON.stringify(aObject[aProp]);
      }

      let data = new TextEncoder().encode(property);
      hasher.update(data, data.length);
    });

    return hasher.finish(true);
  },

  // Add all the activities carried in the |aObjects| array.
  add: function actdb_add(aObjects, aSuccess, aError) {
    this.newTxn(
      "readwrite",
      STORE_NAME,
      function (txn, store) {
        DEBUG && debug("Object count: " + aObjects.length);
        aObjects.forEach(function (aObject) {
          let object = {
            manifest: aObject.manifest,
            name: aObject.name,
            icon: aObject.icon || "",
            description: aObject.description,
          };
          object.id = this.createId(object);
          DEBUG && debug("Going to add " + JSON.stringify(object));
          store.put(object);
        }, this);
      }.bind(this),
      aSuccess,
      aError
    );
  },

  // Remove all the activities carried in the |aObjects| array.
  remove: function actdb_remove(aObjects) {
    this.newTxn(
      "readwrite",
      STORE_NAME,
      (txn, store) => {
        aObjects.forEach(aObject => {
          let object = {
            manifest: aObject.manifest,
            name: aObject.name,
            description: aObject.description,
          };
          DEBUG && debug("Going to remove " + JSON.stringify(object));
          store.delete(this.createId(object));
        });
      },
      function () {},
      function () {}
    );
  },

  // Remove all activities associated with the given |aManifest| URL.
  removeAll: function actdb_removeAll(aManifest) {
    this.newTxn("readwrite", STORE_NAME, function (txn, store) {
      let index = store.index("manifest");
      let request = index.mozGetAll(aManifest);
      request.onsuccess = function manifestActivities(aEvent) {
        aEvent.target.result.forEach(function (result) {
          DEBUG && debug("Removing activity: " + JSON.stringify(result));
          store.delete(result.id);
        });
      };
    });
  },

  find: function actdb_find(aObject, aSuccess, aError, aMatch) {
    this.newTxn(
      "readonly",
      STORE_NAME,
      function (txn, store) {
        let index = store.index("name");
        let request = index.mozGetAll(aObject.options.name);
        request.onsuccess = function findSuccess(aEvent) {
          if (!txn.result) {
            txn.result = {
              name: aObject.options.name,
              options: [],
            };
          }

          aEvent.target.result.forEach(function (result) {
            if (!aMatch(result)) {
              return;
            }

            txn.result.options.push({
              manifest: result.manifest,
              icon: result.icon,
              description: result.description,
            });
          });
        };
      },
      aSuccess,
      aError
    );
  },
};

var Activities = {
  messages: [
    // ActivityProxy.sys.mjs
    "Activity:Start",
    "Activity:Cancel",

    // ActivityRequestHandlerProxy.sys.mjs
    "Activity:Ready",
    "Activity:PostResult",
    "Activity:PostError",

    // ServiceWorkerAssistant.jsm
    "Activities:Register",
    "Activities:Unregister",
    "Activities:UnregisterAll",

    // ActivityUtils.sys.mjs
    "Activities:Get",

    "child-process-shutdown",
  ],

  init: function activities_init() {
    this.messages.forEach(function (msgName) {
      Services.ppmm.addMessageListener(msgName, this);
    }, this);

    Services.obs.addObserver(this, "xpcom-shutdown");
    Services.obs.addObserver(this, "service-worker-shutdown");
    Services.obs.addObserver(this, "b2g-sw-registration-done");

    this.db = new ActivitiesDb();
    this.db.init();
    this.callers = {};
    this.activityChoiceID = 0;
    this.allRegistrationsReady = false;
    this.pendingGetRequests = [];
  },

  observe: function activities_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case "xpcom-shutdown":
        this.messages.forEach(function (msgName) {
          Services.ppmm.removeMessageListener(msgName, this);
        }, this);

        if (this.db) {
          this.db.close();
          this.db = null;
        }

        Services.obs.removeObserver(this, "xpcom-shutdown");
        Services.obs.removeObserver(this, "service-worker-shutdown");
        break;
      case "service-worker-shutdown":
        // Activity handlers registered on the root scope, but this shutdown
        // notification might come from any sw which has registered.
        // Close activities only when the scope is equal to the origin.
        let shutdown_scope = Services.io.newURI(aData);
        let messages = [];
        for (const [key, value] of Object.entries(this.callers)) {
          if (shutdown_scope.equals(Services.io.newURI(value.handlerOrigin))) {
            messages.push(key);
            let detail = {
              reason: "service-worker-shutdown",
              id: key,
              name: value.name,
              caller: value.origin,
              handler: value.handlerOrigin,
              handlerPID: value.handlerPID,
            };
            DEBUG &&
              debug(`sending activity-aborted: ${JSON.stringify(detail)}`);
            this.notifyWebEmbedder("activity-aborted", detail);
          }
        }
        let self = this;
        messages.forEach(function (id) {
          self.trySendAndCleanup(id, "Activity:FireError", {
            id,
            error: "ACTIVITY_HANDLER_SHUTDOWN",
          });
        });
        break;
      case "b2g-sw-registration-done":
        Services.obs.removeObserver(this, "b2g-sw-registration-done");
        this.allRegistrationsReady = true;
        this.pendingGetRequests.forEach(
          function (request) {
            DEBUG &&
              debug(
                `handle pending Get request ${request.msg.name} ${request.msg.requestID}`
              );
            this.handleGetRequest(request.target, request.msg);
          }.bind(this)
        );
        this.pendingGetRequests = [];
        break;
    }
  },

  /**
   * Starts an activity by doing:
   * - finds a list of matching activities.
   * - sends the list to web-embedder to get activity choice from user
   * - fire an system message of type "activity" to this app, sending the
   *   activity data as a payload.
   */
  startActivity: function activities_startActivity(aMsg) {
    let self = this;
    let successCb = function successCb(aResults) {
      // We have no matching activity registered, let's fire an error.
      if (aResults.options.length === 0) {
        self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
          id: aMsg.id,
          error: "NO_PROVIDER",
        });
        return;
      }

      let getActivityChoice = function (aResult) {
        // The user has cancelled the choice, fire an error.
        if (aResult === -1) {
          self.trySendAndCleanup(aMsg.id, "Activity:FireError", {
            id: aMsg.id,
            error: "ACTIVITY_CHOICE_CANCELED",
          });
          return;
        }

        if (!self.callers[aMsg.id]) {
          DEBUG && debug(`This activity(${aMsg.id}) is already removed.`);
          return;
        }

        let sysmm = Cc["@mozilla.org/systemmessage-service;1"].getService(
          Ci.nsISystemMessageService
        );
        if (!sysmm) {
          self.removeCaller(aMsg.id);
          DEBUG && debug(`Error: SystemMessageService is not present.`);
          return;
        }

        let result = aResults.options[aResult];
        let origin = Services.io.newURI(result.manifest).prePath;
        self.callers[aMsg.id].handlerOrigin = origin;

        try {
          sysmm.sendMessage(
            "activity",
            {
              id: aMsg.id,
              payload: aMsg.options,
              returnValue: result.description.returnValue,
            },
            origin
          );
        } catch (e) {
          DEBUG &&
            debug(`Error: fail in SystemMessageService.sendMessage - ${e}`);
        }

        if (!result.description.returnValue) {
          // No need to notify observers, since we don't want the caller
          // to be raised on the foreground that quick.
          self.trySendAndCleanup(aMsg.id, "Activity:FireSuccess", {
            id: aMsg.id,
            result: null,
          });
        }
      };

      let id = "activity-choice" + self.activityChoiceID++;
      let choices = [];
      aResults.options.forEach(function (item) {
        choices.push({ manifest: item.manifest, icon: item.icon });
      });
      let detail = {
        id,
        name: aMsg.options.name,
        choices,
      };
      if (aMsg.options.data && aMsg.options.data.type) {
        detail.activityType = aMsg.options.data.type;
      }

      Services.obs.addObserver(function choiceObsrver(wrappedResult) {
        let result = wrappedResult.wrappedJSObject;
        if (result.id != id) {
          return;
        }

        Services.obs.removeObserver(choiceObsrver, "activity-choice-result");
        getActivityChoice(result.value !== undefined ? result.value : -1);
      }, "activity-choice-result");

      self.notifyWebEmbedder("activity-choice", detail);
    };

    let errorCb = function errorCb(aError) {
      // Something unexpected happened. Should we send an error back?
      DEBUG && debug(`Error: in finding activity - ${aError}`);
    };

    let matchFunc = function matchFunc(aResult) {
      return lazy.ActivitiesServiceFilter.match(
        aMsg.options.data,
        aMsg.origin,
        aResult.description
      );
    };

    this.db.find(aMsg, successCb, errorCb, matchFunc);
  },

  trySendAndCleanup: function activities_trySendAndCleanup(
    aId,
    aName,
    aPayload
  ) {
    try {
      this.callers[aId].mm.sendAsyncMessage(aName, aPayload);
    } finally {
      this.removeCaller(aId);
    }
  },

  notifyWebEmbedder: function activities_notifyWebEmbedder(aTopic, aDetail) {
    Services.obs.notifyObservers({ wrappedJSObject: aDetail }, aTopic);
  },

  receiveMessage: function activities_receiveMessage(aMessage) {
    let mm = aMessage.target;
    let msg = aMessage.json;

    let caller;

    if (
      aMessage.name == "Activity:PostResult" ||
      aMessage.name == "Activity:PostError" ||
      aMessage.name == "Activity:Cancel" ||
      aMessage.name == "Activity:Ready"
    ) {
      caller = this.callers[msg.id];
      if (!caller) {
        if (aMessage.name == "Activity:Ready") {
          const debugInfo = { id: msg.id, handlerPID: msg.handlerPID };
          DEBUG &&
            debug(
              `activity handler resolved already: ${JSON.stringify(debugInfo)}`
            );
        }
        return;
      }
    }

    switch (aMessage.name) {
      case "Activity:Start": {
        this.callers[msg.id] = {
          mm,
          name: msg.options.name,
          origin: msg.origin,
        };
        let detail = {
          id: msg.id,
          name: msg.options.name,
          origin: msg.origin,
        };
        DEBUG && debug(`sending activity-opened: ${JSON.stringify(detail)}`);
        this.notifyWebEmbedder("activity-opened", detail);
        this.startActivity(msg);
        break;
      }

      case "Activity:Cancel": {
        let detail = {
          reason: "caller-canceled",
          id: msg.id,
          name: caller.name,
          caller: caller.origin,
          handler: caller.handlerOrigin,
          handlerPID: caller.handlerPID,
        };
        DEBUG && debug(`sending activity-aborted: ${JSON.stringify(detail)}`);
        this.notifyWebEmbedder("activity-aborted", detail);
        this.trySendAndCleanup(msg.id, "Activity:FireCancel", msg);
        break;
      }

      case "Activity:Ready": {
        caller.handlerMM = mm;
        caller.handlerPID = msg.handlerPID;
        const debugInfo = {
          id: msg.id,
          name: caller.name,
          caller: caller.origin,
          handler: caller.handlerOrigin,
          handlerPID: caller.handlerPID,
        };
        DEBUG && debug(`activity handler ready: ${JSON.stringify(debugInfo)}`);
        break;
      }
      case "Activity:PostResult":
        this.trySendAndCleanup(msg.id, "Activity:FireSuccess", msg);
        break;
      case "Activity:PostError":
        this.trySendAndCleanup(msg.id, "Activity:FireError", msg);
        break;

      case "Activities:Register":
        this.db.add(
          msg,
          function onSuccess(aEvent) {
            DEBUG && debug(`Register success.`);
          },
          function onError(aEvent) {
            DEBUG && debug(`Register fail.`);
          }
        );
        break;
      case "Activities:Unregister":
        this.db.remove(msg);
        break;
      case "Activities:UnregisterAll":
        this.db.removeAll(msg);
        break;
      case "Activities:Get":
        if (!this.allRegistrationsReady) {
          DEBUG &&
            debug(
              `Receive Activities:Get but not ready yet. ${msg.name} ${msg.requestID}`
            );
          this.pendingGetRequests.push({ target: mm, msg });
          break;
        }
        this.handleGetRequest(mm, msg);
        break;
      case "child-process-shutdown":
        for (let id in this.callers) {
          if (this.callers[id].handlerMM == mm) {
            let detail = {
              reason: "process-shutdown",
              id,
              name: this.callers[id].name,
              caller: this.callers[id].origin,
              handler: this.callers[id].handlerOrigin,
              handlerPID: this.callers[id].handlerPID,
            };
            DEBUG &&
              debug(`sending activity-aborted: ${JSON.stringify(detail)}`);
            this.notifyWebEmbedder("activity-aborted", detail);

            this.trySendAndCleanup(id, "Activity:FireError", {
              id,
              error: "ACTIVITY_HANDLER_SHUTDOWN",
            });
          } else if (this.callers[id].mm == mm) {
            // if the caller crash, remove it from the callers.
            DEBUG &&
              debug(
                "Caller shutdown due to process shutdown, caller=" +
                  this.callers[id].origin
              );
            this.removeCaller(id);
          }
        }
        break;
    }
  },

  removeCaller: function activities_removeCaller(id) {
    let detail = {
      id,
      name: this.callers[id].name,
      caller: this.callers[id].origin,
      handler: this.callers[id].handlerOrigin,
      handlerPID: this.callers[id].handlerPID,
    };
    DEBUG && debug(`sending activity-closed: ${JSON.stringify(detail)}`);
    this.notifyWebEmbedder("activity-closed", detail);
    delete this.callers[id];
  },

  handleGetRequest: function activities_handleGetRequest(mm, msg) {
    let obj = {
      options: {
        name: msg.name,
      },
    };
    this.db.find(
      obj,
      function onSuccess(aResults) {
        mm.sendAsyncMessage(msg.requestID, {
          results: aResults,
          success: true,
        });
      },
      function onError(aError) {
        mm.sendAsyncMessage(msg.requestID, {
          error: aError,
          success: false,
        });
      },
      function matchFunc(aResult) {
        return true;
      }
    );
  },
};

Activities.init();
