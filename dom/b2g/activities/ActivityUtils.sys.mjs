/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function debug(aMsg) {
  dump(`-*- ActivityUtils: ${aMsg}\n`);
}

export function ActivityUtils() {
  debug("constructor");
}

ActivityUtils.prototype = {
  // nsIDOMGlobalPropertyInitializer implementation
  init(aWindow) {
    debug("init");
    this._window = aWindow;
    this._requestCount = 0;
  },

  getInstalled(aName) {
    debug(`getInstalled ${aName}`);
    let requestID = `Activities:Get:${this.getRequestCount()}`;
    let self = this;
    let promise = new this._window.Promise(function(aResolve, aReject) {
      Services.cpmm.addMessageListener(requestID, function receiveActivitiesGet(
        aMessage
      ) {
        debug(`receive ${requestID} ${JSON.stringify(aMessage)}`);
        Services.cpmm.removeMessageListener(requestID, receiveActivitiesGet);
        let json = aMessage.json ? aMessage.json : {};
        if (json.success) {
          let array = new self._window.Array();
          let options = json?.results?.options ? json.results.options : [];
          options.forEach(function(aObject) {
            array.push(Cu.cloneInto(aObject, self._window));
          });
          aResolve(array);
        } else {
          aReject(json.error);
        }
      });

      Services.cpmm.sendAsyncMessage("Activities:Get", {
        requestID,
        name: aName,
      });
    });

    return promise;
  },

  getRequestCount() {
    return this._requestCount++;
  },

  classID: Components.ID("{b857ca8c-616b-4121-ae92-464c356e6a80}"),
  contractID: "@mozilla.org/activity-utils;1",
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDOMGlobalPropertyInitializer]),
};
