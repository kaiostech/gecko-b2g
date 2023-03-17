/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(aMsg) {
  //dump(`ActivityProxy: ${aMsg}\n`);
}

/*
 * ActivityProxy is a helper of passing requests from WebActivity.cpp to
 * ActivitiesService.sys.mjs, receiving results from ActivitiesService and send it
 * back to WebActivity.
 */

export function ActivityProxy() {
  debug(`consturctor`);
  // NOTE: I think it's okay not to remove MessageListeners because this
  // ActivityProxy runs as a singleton service per process.
  Services.cpmm.addMessageListener("Activity:FireSuccess", this);
  Services.cpmm.addMessageListener("Activity:FireError", this);
  Services.cpmm.addMessageListener("Activity:FireCancel", this);
  Services.obs.addObserver(this, "inner-window-destroyed");
}

ActivityProxy.prototype = {
  activities: new Map(),

  create(owner, options, origin) {
    let id = Services.uuid.generateUUID().toString();

    this.activities.set(id, {
      owner: owner ? owner : {},
      options,
      origin,
      innerWindowId: owner?.windowGlobalChild.innerWindowId,
    });

    debug(`${id} create activity`);
    return id;
  },

  start(id, callback) {
    debug(`${id} start activity`);
    let activity = this.activities.get(id);
    if (!activity) {
      debug(`${id} Cannot find activity.`);
      return;
    }
    activity.callback = callback;

    Services.cpmm.sendAsyncMessage("Activity:Start", {
      id,
      options: activity.options,
      origin: activity.origin,
    });
  },

  cancel(id) {
    debug(`${id} cancel activity`);
    Services.cpmm.sendAsyncMessage("Activity:Cancel", {
      id,
      error: "ACTIVITY_CANCELED",
    });
  },

  receiveMessage(message) {
    let msg = message.json;
    debug(`${msg.id} receiveMessage: ${message.name}`);
    let activity = this.activities.get(msg.id);
    if (!activity) {
      debug(`${msg.id} Cannot find activity.`);
      return;
    }

    switch (message.name) {
      case "Activity:FireSuccess":
        activity.callback.onStart(
          Cr.NS_OK,
          Cu.cloneInto(msg.result, activity.owner)
        );
        break;
      case "Activity:FireCancel":
      case "Activity:FireError":
        activity.callback.onStart(
          Cr.NS_ERROR_FAILURE,
          Cu.cloneInto(msg.error, activity.owner)
        );
        break;
    }

    this.activities.delete(msg.id);
  },

  observe(subject, topic, data) {
    switch (topic) {
      case "inner-window-destroyed":
        let id = subject.QueryInterface(Ci.nsISupportsPRUint64).data;
        debug(`inner-window-destroyed id=${id}`);

        let keys = [];
        this.activities.forEach((value, key, map) => {
          if (value.innerWindowId == id) {
            value.callback.onStart(
              Cr.NS_ERROR_FAILURE,
              Cu.cloneInto("ACTIVITY_CANCELED", value.owner)
            );
            keys.push(key);
          }
        });
        keys.forEach((key, index, array) => {
          this.activities.delete(key);
        });
        break;
      default:
        debug("Observed unexpected topic " + topic);
    }
  },

  contractID: "@mozilla.org/dom/activities/proxy;1",

  classID: Components.ID("{1d069f6a-bb82-4648-8bcd-b8671b4a963d}"),

  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver, Ci.nsIActivityProxy]),
};

//module initialization
