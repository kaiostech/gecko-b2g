/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

var EXPORTED_SYMBOLS = ["TelephonyRequestQueue"];

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

const TELEPHONY_REQUESTS = [
  "getCurrentCalls",
  "answerCall",
  "conferenceCall",
  "dial",
  "explicitCallTransfer",
  "hangUpCall",
  "hangUpForeground",
  "hangUpBackground",
  "separateCall",
  "switchActiveCall",
  "udub",
];

// Set to true in ril_consts_debug.js to see debug messages
var DEBUG = RIL_DEBUG.DEBUG_WORKER;

function updateDebugFlag() {
  // Read debug setting from pref
  let debugPref;
  try {
    debugPref =
      debugPref ||
      Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_WORKER_DEBUG_ENABLED);
  } catch (e) {
    debugPref = false;
  }
  DEBUG = RIL_DEBUG.DEBUG_RIL || debugPref;
}
updateDebugFlag();

/**
 * Queue entry; only used in the queue.
 */
function TelephonyRequestEntry(request, callback) {
  this.request = request;
  this.callback = callback;
}

function TelephonyRequestQueue(ril) {
  this.ril = ril;
  this.currentQueue = null; // Point to the current running queue.

  this.queryQueue = [];
  this.controlQueue = [];
}

TelephonyRequestQueue.prototype = {
  _getQueue(request) {
    return request === "getCurrentCalls" ? this.queryQueue : this.controlQueue;
  },

  _getAnotherQueue(queue) {
    return this.queryQueue === queue ? this.controlQueue : this.queryQueue;
  },

  _find(queue, request) {
    for (let i = 0; i < queue.length; ++i) {
      if (queue[i].request === request) {
        return i;
      }
    }
    return -1;
  },

  _startQueue(queue) {
    if (queue.length === 0) {
      return;
    }

    // We only need to keep one entry for queryQueue.
    if (queue === this.queryQueue) {
      queue.splice(1, queue.length - 1);
    }

    this.currentQueue = queue;
    for (let entry of queue) {
      this._executeEntry(entry);
    }
  },

  _executeEntry(entry) {
    if (DEBUG) {
      this.debug("execute " + entry.request);
    }
    entry.callback();
  },

  debug(msg) {
    this.ril.debug("[TeleQ] " + msg);
  },

  isValidRequest(request) {
    return TELEPHONY_REQUESTS.includes(request);
  },

  push(request, callback) {
    if (!this.isValidRequest(request)) {
      if (DEBUG) {
        this.debug("Error: " + request + " is not a telephony request");
      }
      return;
    }

    if (DEBUG) {
      this.debug("push " + request);
    }
    let entry = new TelephonyRequestEntry(request, callback);
    let queue = this._getQueue(request);
    queue.push(entry);

    // Try to run the request.
    if (this.currentQueue === queue) {
      this._executeEntry(entry);
    } else if (!this.currentQueue) {
      this._startQueue(queue);
    }
  },

  pop(request) {
    if (!this.isValidRequest(request)) {
      if (DEBUG) {
        this.debug("Error: " + request + " is not a telephony request");
      }
      return;
    }

    if (DEBUG) {
      this.debug("pop " + request);
    }
    let queue = this._getQueue(request);
    let index = this._find(queue, request);
    if (index === -1) {
      throw new Error("Cannot find the request in telephonyRequestQueue.");
    } else {
      queue.splice(index, 1);
    }

    if (queue.length === 0) {
      this.currentQueue = null;
      this._startQueue(this._getAnotherQueue(queue));
    }
  },
};
