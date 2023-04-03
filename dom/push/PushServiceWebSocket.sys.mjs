/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const isGonk = AppConstants.platform === "gonk";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  AlarmService: "resource://gre/modules/AlarmService.sys.mjs",
  PushCredential: "resource://gre/modules/PushCredential.sys.mjs",
  pushBroadcastService: "resource://gre/modules/PushBroadcastService.sys.mjs",
});

import { PushDB } from "resource://gre/modules/PushDB.sys.mjs";
import { PushRecord } from "resource://gre/modules/PushRecord.sys.mjs";
import { PushCrypto } from "resource://gre/modules/PushCrypto.sys.mjs";

ChromeUtils.defineModuleGetter(
  lazy,
  "ObjectUtils",
  "resource://gre/modules/ObjectUtils.jsm"
);
XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "gPowerManagerService",
  "@mozilla.org/power/powermanagerservice;1",
  "nsIPowerManagerService"
);

const kPUSHWSDB_DB_NAME = "pushapi";
const kPUSHWSDB_DB_VERSION = 6; // Change this if the IndexedDB format changes
const kPUSHWSDB_STORE_NAME = "pushapi";

// WebSocket close code sent by the server to indicate that the client should
// not automatically reconnect.
const kBACKOFF_WS_STATUS_CODE = 4774;

const kMinRequestTimeOut = 1000; // 1s

// Maps ack statuses, unsubscribe reasons, and delivery error reasons to codes
// included in request payloads.
const kACK_STATUS_TO_CODE = {
  [Ci.nsIPushErrorReporter.ACK_DELIVERED]: 100,
  [Ci.nsIPushErrorReporter.ACK_DECRYPTION_ERROR]: 101,
  [Ci.nsIPushErrorReporter.ACK_NOT_DELIVERED]: 102,
};

const kUNREGISTER_REASON_TO_CODE = {
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_MANUAL]: 200,
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_QUOTA_EXCEEDED]: 201,
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_PERMISSION_REVOKED]: 202,
  [Ci.nsIPushErrorReporter.UNSUBSCRIBE_PENDING_RECORD]: 203,
};

const kDELIVERY_REASON_TO_CODE = {
  [Ci.nsIPushErrorReporter.DELIVERY_UNCAUGHT_EXCEPTION]: 301,
  [Ci.nsIPushErrorReporter.DELIVERY_UNHANDLED_REJECTION]: 302,
  [Ci.nsIPushErrorReporter.DELIVERY_INTERNAL_ERROR]: 303,
};

const prefs = Services.prefs.getBranch("dom.push.");

XPCOMUtils.defineLazyGetter(lazy, "console", () => {
  let { ConsoleAPI } = ChromeUtils.importESModule(
    "resource://gre/modules/Console.sys.mjs"
  );
  return new ConsoleAPI({
    maxLogLevelPref: "dom.push.loglevel",
    prefix: "PushServiceWebSocket",
  });
});

/**
 * A proxy between the PushService and the WebSocket. The listener is used so
 * that the PushService can silence messages from the WebSocket by setting
 * PushWebSocketListener._pushService to null. This is required because
 * a WebSocket can continue to send messages or errors after it has been
 * closed but the PushService may not be interested in these. It's easier to
 * stop listening than to have checks at specific points.
 */
var PushWebSocketListener = function(pushService) {
  this._pushService = pushService;
};

PushWebSocketListener.prototype = {
  onStart(context) {
    if (!this._pushService) {
      return;
    }
    this._pushService._wsOnStart(context);
  },

  onStop(context, statusCode) {
    if (!this._pushService) {
      return;
    }
    this._pushService._wsOnStop(context, statusCode);
  },

  onAcknowledge(context, size) {
    // EMPTY
  },

  onBinaryMessageAvailable(context, message) {
    // EMPTY
  },

  onMessageAvailable(context, message) {
    if (!this._pushService) {
      return;
    }
    this._pushService._wsOnMessageAvailable(context, message);
  },

  onServerClose(context, aStatusCode, aReason) {
    if (!this._pushService) {
      return;
    }
    this._pushService._wsOnServerClose(context, aStatusCode, aReason);
  },
};

// websocket states
// websocket is off
const STATE_SHUT_DOWN = 0;
// Websocket has been opened on client side, waiting for successful open.
// (_wsOnStart)
const STATE_WAITING_FOR_WS_START = 1;
// Websocket opened, hello sent, waiting for server reply (_handleHelloReply).
const STATE_WAITING_FOR_HELLO = 2;
// Websocket operational, handshake completed, begin protocol messaging.
const STATE_READY = 3;

export var PushServiceWebSocket = {
  QueryInterface: ChromeUtils.generateQI(["nsINamed", "nsIObserver"]),
  name: "PushServiceWebSocket",

  _mainPushService: null,
  _serverURI: null,
  _currentlyRegistering: new Set(),
  _alarmID: null,

  newPushDB() {
    return new PushDB(
      kPUSHWSDB_DB_NAME,
      kPUSHWSDB_DB_VERSION,
      kPUSHWSDB_STORE_NAME,
      "channelID",
      PushRecordWebSocket
    );
  },

  disconnect() {
    this._shutdownWS();
  },

  observe(aSubject, aTopic, aData) {
    if (aTopic == "nsPref:changed" && aData == "dom.push.userAgentID") {
      this._onUAIDChanged();
    } else if (aTopic == "timer-callback") {
      this._onTimerFired(aSubject);
    }
  },

  /**
   * Handles a UAID change. Unlike reconnects, we cancel all pending requests
   * after disconnecting. Existing subscriptions stored in IndexedDB will be
   * dropped on reconnect.
   */
  _onUAIDChanged() {
    lazy.console.debug("onUAIDChanged()");

    this._shutdownWS();
    if (this._alarmEnabled) {
      this._startBackoffAlarm();
    } else {
      this._startBackoffTimer();
    }
  },

  /** Handles a ping, backoff, or request timeout timer event. */
  _onTimerFired(timer) {
    lazy.console.debug("onTimerFired()");

    if (timer == this._pingTimer) {
      this._sendPing();
      return;
    }

    if (timer == this._backoffTimer) {
      lazy.console.debug("onTimerFired: Reconnecting after backoff");
      this._beginWSSetup();
      return;
    }

    if (timer == this._requestTimeoutTimer) {
      this._timeOutRequests();
    }
  },

  /**
   * There is only one alarm active at any time. This alarm has 3 intervals
   * corresponding to 3 tasks.
   *
   * 1) Reconnect on ping timeout.
   *    If we haven't received any messages from the server by the time this
   *    alarm fires, the connection is closed and PushService tries to
   *    reconnect, repurposing the alarm for (3).
   *
   * 2) Send a ping.
   *    The protocol sends a ping ({}) on the wire every pingInterval ms. Once
   *    it sends the ping, the alarm goes to task (1) which is waiting for
   *    a pong. If data is received after the ping is sent,
   *    _wsOnMessageAvailable() will reset the ping alarm (which cancels
   *    waiting for the pong). So as long as the connection is fine, pong alarm
   *    never fires.
   *
   * 3) Reconnect after backoff.
   *    The alarm is set by _reconnectAfterBackoff() and increases in duration
   *    every time we try and fail to connect.  When it triggers, websocket
   *    setup begins again. On successful socket setup, the socket starts
   *    receiving messages. The alarm now goes to (2) where it monitors the
   *    WebSocket by sending a ping.  Since incoming data is a sign of the
   *    connection being up, the ping alarm is reset every time data is
   *    received.
   */
  _onAlarmFired() {
    // Conditions are arranged in decreasing specificity.
    // i.e. when _lastPingTime is not 0, other conditions are also true.
    if (this._lastPingTime > 0) {
      console.debug("Did not receive pong in time. Reconnecting WebSocket.");
      this._shutdownWS();
      this._startBackoffAlarm();
    } else if (this._currentState == STATE_READY) {
      // Send a ping.
      // Bypass the queue; we don't want this to be kept pending.
      // Watch out for exception in case the socket has disconnected.
      // When this happens, we pretend the ping was sent and don't specially
      // handle the exception, as the lack of a pong will lead to the socket
      // being reset.
      this._sendPing();
    } else if (this._alarmID !== null) {
      console.debug("reconnect alarm fired.");
      // Reconnect after back-off.
      // The check for a non-null _alarmID prevents a situation where the alarm
      // fires, but _shutdownWS() is called from another code-path (e.g.
      // network state change) and we don't want to reconnect.
      //
      // It also handles the case where _beginWSSetup() is called from another
      // code-path.
      //
      // alarmID will be non-null only when no shutdown/connect is
      // called between _reconnectAfterBackoff() setting the alarm and the
      // alarm firing.

      // Websocket is shut down. Backoff interval expired, try to connect.
      this._beginWSSetup();
    }
  },

  /**
   * Sends a ping to the server. Bypasses the request queue, but starts the
   * request timeout timer. If the socket is already closed, or the server
   * does not respond within the timeout, the client will reconnect.
   */
  _sendPing() {
    lazy.console.debug("sendPing()");

    if (this._alarmEnabled) {
      this._setAlarm(prefs.getIntPref("requestTimeout"));
    } else {
      this._startRequestTimeoutTimer();
    }
    try {
      this._wsSendMessage({});
      this._lastPingTime = Date.now();
    } catch (e) {
      lazy.console.debug("sendPing: Error sending ping", e);
      this._reconnect();
    }
  },

  /** Times out any pending requests. */
  _timeOutRequests() {
    lazy.console.debug("timeOutRequests()");

    if (!this._hasPendingRequests()) {
      // Cancel the repeating timer and exit early if we aren't waiting for
      // pongs or requests.
      this._requestTimeoutTimer.cancel();
      this._requestTimeout = this._requestTimeoutBase;
      return;
    }

    let now = Date.now();

    // Set to true if at least one request timed out, or we're still waiting
    // for a pong after the request timeout.
    let requestTimedOut = false;

    if (
      !this._alarmEnabled &&
      this._lastPingTime > 0 &&
      now - this._lastPingTime > this._requestTimeout
    ) {
      lazy.console.debug("timeOutRequests: Did not receive pong in time");
      requestTimedOut = true;
    } else {
      for (let [key, request] of this._pendingRequests) {
        let duration = now - request.ctime;
        // If any of the registration requests time out, all the ones after it
        // also made to fail, since we are going to be disconnecting the
        // socket.
        requestTimedOut |= duration > this._requestTimeout;
        if (requestTimedOut) {
          request.reject(new Error("Request timed out: " + key));
          this._pendingRequests.delete(key);
        }
      }
    }

    // The most likely reason for a pong or registration request timing out is
    // that the socket has disconnected. Best to reconnect.
    if (requestTimedOut) {
      this._reconnect();
    }
  },

  get _UAID() {
    return prefs.getStringPref("userAgentID");
  },

  set _UAID(newID) {
    if (typeof newID !== "string") {
      lazy.console.warn(
        "Got invalid, non-string UAID",
        newID,
        "Not updating userAgentID"
      );
      return;
    }
    lazy.console.debug("New _UAID", newID);
    prefs.setStringPref("userAgentID", newID);
  },

  _ws: null,
  _pendingRequests: new Map(),
  _currentState: STATE_SHUT_DOWN,
  _requestTimeout: 0,
  _requestTimeoutTimer: null,
  _requestTimeoutBase: 0,
  _retryFailCount: 0,

  /**
   * Holds if the adaptive ping is enabled. This is read on init().
   * If adaptive ping is enabled, a new ping is calculed each time we receive
   * a pong message, trying to maximize network resources while minimizing
   * cellular signalling storms.
   */
  _adaptiveEnabled: false,

  /**
   * According to the WS spec, servers should immediately close the underlying
   * TCP connection after they close a WebSocket. This causes wsOnStop to be
   * called with error NS_BASE_STREAM_CLOSED. Since the client has to keep the
   * WebSocket up, it should try to reconnect. But if the server closes the
   * WebSocket because it wants the client to back off, then the client
   * shouldn't re-establish the connection. If the server sends the backoff
   * close code, this field will be set to true in wsOnServerClose. It is
   * checked in wsOnStop.
   */
  _skipReconnect: false,

  /** Indicates whether the server supports Web Push-style message delivery. */
  _dataEnabled: false,

  _socketWakeLock: {},

  /**
   * The last time the client sent a ping to the server. If non-zero, keeps the
   * request timeout timer active. Reset to zero when the server responds with
   * a pong or pending messages.
   */
  _lastPingTime: 0,

  /**
   * This saves a flag about if we need to recalculate a new ping, based on:
   *   1) the gap between the maximum working ping and the first ping that
   *      gives an error (timeout) OR
   *   2) we have reached the pref of the maximum value we allow for a ping
   *      (dom.push.adaptive.upperLimit)
   */
  _recalculatePing: true,

  /**
   * This map holds a (pingInterval, triedTimes) of each pingInterval tried.
   * It is used to check if the pingInterval has been tested enough to know that
   * is incorrect and is above the limit the network allow us to keep the
   * connection open.
   */
  _pingIntervalRetryTimes: {},

  /**
   * Holds the lastGoodPingInterval for our current connection.
   */
  _lastGoodPingInterval: 0,

  /**
   * Maximum ping interval that we can reach.
   */
  _upperLimit: 0,

  /**
   * A one-shot timer used to ping the server, to avoid timing out idle
   * connections. Reset to the ping interval on each incoming message.
   */
  _pingTimer: null,

  /** A one-shot timer fired after the reconnect backoff period. */
  _backoffTimer: null,

  /* A flag indicated whether using alarm for ping and backoff instead*/
  _alarmEnabled: isGonk,

  /**
   * Sends a message to the Push Server through an open websocket.
   * typeof(msg) shall be an object
   */
  _wsSendMessage(msg) {
    if (!this._ws) {
      lazy.console.warn(
        "wsSendMessage: No WebSocket initialized.",
        "Cannot send a message"
      );
      return;
    }
    msg = JSON.stringify(msg);
    lazy.console.debug("wsSendMessage: Sending message", msg);
    this._ws.sendMsg(msg);
  },

  init(options, mainPushService, serverURI) {
    lazy.console.debug("init()");

    this._mainPushService = mainPushService;
    this._serverURI = serverURI;
    // Filled in at connect() time
    this._broadcastListeners = null;

    // Override the default WebSocket factory function. The returned object
    // must be null or satisfy the nsIWebSocketChannel interface. Used by
    // the tests to provide a mock WebSocket implementation.
    if (options.makeWebSocket) {
      this._makeWebSocket = options.makeWebSocket;
    }

    this._networkInfo = options.networkInfo;
    if (!this._networkInfo) {
      this._networkInfo = PushNetworkInfo;
    }

    this._requestTimeoutBase = prefs.getIntPref("requestTimeout");
    if (this._requestTimeoutBase < kMinRequestTimeOut) {
      this._requestTimeoutBase = kMinRequestTimeOut;
      console.warn("requestTimeout smaller than min, set it as default");
    }
    this._requestTimeout = this._requestTimeoutBase;
    this._adaptiveEnabled = prefs.getBoolPref("adaptive.enabled", false);
    this._upperLimit = prefs.getIntPref("adaptive.upperLimit");

    if (prefs.getBoolPref("authorization.enabled", false)) {
      this.credential = new lazy.PushCredential();
    }

    // reset adaptive-set value if adaptive mode is disabled
    if (!this._adaptiveEnabled) {
      prefs.clearUserPref("pingInterval");
      // mobile network
      prefs.clearUserPref("adaptive.mobile");
      prefs.clearUserPref("pingInterval.mobile");
      prefs.clearUserPref("adaptive.lastGoodPingInterval.mobile");
      // wifi network
      prefs.clearUserPref("pingInterval.wifi");
      prefs.clearUserPref("adaptive.lastGoodPingInterval.wifi");
    }

    return Promise.resolve();
  },

  _reconnect() {
    lazy.console.debug("reconnect()");
    this._shutdownWS(false);
    if (this._alarmEnabled) {
      this._startBackoffAlarm();
    } else {
      this._startBackoffTimer();
    }
  },

  _shutdownWS(shouldCancelPending = true) {
    lazy.console.debug("shutdownWS()");

    if (this._currentState == STATE_READY) {
      prefs.removeObserver("userAgentID", this);
    }

    this._currentState = STATE_SHUT_DOWN;
    this._skipReconnect = false;

    if (this._wsListener) {
      this._wsListener._pushService = null;
    }

    if (this._ws && this._currentState >= STATE_WAITING_FOR_WS_START) {
      try {
        this._ws.close(0, null);
      } catch (e) {}
    }
    this._ws = null;

    this._lastPingTime = 0;

    if (this._pingTimer) {
      this._pingTimer.cancel();
    }

    if (shouldCancelPending) {
      this._cancelPendingRequests();
    }

    if (this._notifyRequestQueue) {
      this._notifyRequestQueue();
      this._notifyRequestQueue = null;
    }

    if (this._alarmEnabled) {
      this._stopAlarm();
    }
  },

  uninit() {
    // All pending requests (ideally none) are dropped at this point. We
    // shouldn't have any applications performing registration/unregistration
    // or receiving notifications.
    this._shutdownWS();
    // Release all wakeLocks.
    for (var index in this._socketWakeLock) {
      this._releaseWakeLock(index);
    }

    if (this._backoffTimer) {
      this._backoffTimer.cancel();
    }
    if (this._requestTimeoutTimer) {
      this._requestTimeoutTimer.cancel();
      this._requestTimeout = this._requestTimeoutBase;
    }

    this._mainPushService = null;

    this._dataEnabled = false;
    if (this.credential) {
      this.credential = null;
    }
  },

  /**
   * How retries work: If the WS is closed due to a socket error,
   * _startBackoffTimer() is called. The retry timer is started and when
   * it times out, beginWSSetup() is called again.
   *
   * If we are in the middle of a timeout (i.e. waiting), but
   * a register/unregister is called, we don't want to wait around anymore.
   * _sendRequest will automatically call beginWSSetup(), which will cancel the
   * timer. In addition since the state will have changed, even if a pending
   * timer event comes in (because the timer fired the event before it was
   * cancelled), so the connection won't be reset.
   */
  _startBackoffTimer() {
    lazy.console.debug("startBackoffTimer()");

    // Calculate new timeout, but cap it to pingInterval.
    let retryTimeout =
      prefs.getIntPref("retryBaseInterval") * Math.pow(2, this._retryFailCount);
    retryTimeout = Math.min(retryTimeout, prefs.getIntPref("pingInterval"));

    this._retryFailCount++;

    lazy.console.debug(
      "startBackoffTimer: Retry in",
      retryTimeout,
      "Try number",
      this._retryFailCount
    );

    if (!this._backoffTimer) {
      this._backoffTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }
    this._backoffTimer.init(this, retryTimeout, Ci.nsITimer.TYPE_ONE_SHOT);
  },

  /**
   * How retries work:  The goal is to ensure websocket is always up on
   * networks not supporting UDP. So the websocket should only be shutdown if
   * onServerClose indicates UDP wakeup.  If WS is closed due to socket error,
   * _startBackoffAlarm() is called. The retry timer is started and when
   * it times out, beginWSSetup() is called again.
   *
   * If we are in the middle of a timeout (i.e. waiting), but
   * a register/unregister is called, we don't want to wait around anymore.
   * _sendRequest will automatically call beginWSSetup(), which will cancel the
   * timer. In addition since the state will have changed, even if a pending
   * timer event comes in (because the timer fired the event before it was
   * cancelled), so the connection won't be reset.
   */
  _startBackoffAlarm() {
    console.debug("startBackoffAlarm()");
    //Calculate new ping interval
    this._calculateAdaptivePing(true /* wsWentDown */);

    // Calculate new timeout, but cap it to pingInterval.
    let retryTimeout =
      prefs.getIntPref("retryBaseInterval") * Math.pow(2, this._retryFailCount);
    retryTimeout = Math.min(retryTimeout, prefs.getIntPref("pingInterval"));

    this._retryFailCount++;

    console.debug(
      "startBackoffAlarm: Retry in",
      retryTimeout,
      "Try number",
      this._retryFailCount
    );

    this._setAlarm(retryTimeout);
  },

  /** Indicates whether we're waiting for pongs or requests. */
  _hasPendingRequests() {
    return this._lastPingTime > 0 || this._pendingRequests.size > 0;
  },

  /**
   * Starts the request timeout timer unless we're already waiting for a pong
   * or register request.
   */
  _startRequestTimeoutTimer() {
    if (this._hasPendingRequests()) {
      return;
    }
    if (!this._requestTimeoutTimer) {
      this._requestTimeoutTimer = Cc["@mozilla.org/timer;1"].createInstance(
        Ci.nsITimer
      );
    }

    let extendTimeout = 0;
    if (!this._ws) {
      if (this.credential && this.credential.isExpired) {
        extendTimeout += prefs.getIntPref("extendTimeout.token", 0);
      }
    }
    if (extendTimeout > 0) {
      this._requestTimeout = this._requestTimeoutBase + extendTimeout;
    }

    this._requestTimeoutTimer.init(
      this,
      this._requestTimeout,
      Ci.nsITimer.TYPE_REPEATING_SLACK
    );
  },

  /** Starts or resets the ping timer. */
  _startPingTimer() {
    if (!this._pingTimer) {
      this._pingTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    }
    this._pingTimer.init(
      this,
      prefs.getIntPref("pingInterval"),
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  /** |delay| should be in milliseconds. */
  _setAlarm(delay) {
    // Bug 909270: Since calls to AlarmService.add() are async, calls must be
    // 'queued' to ensure only one alarm is ever active.
    if (this._settingAlarm) {
      // onSuccess will handle the set. Overwriting the variable enforces the
      // last-writer-wins semantics.
      this._queuedAlarmDelay = delay;
      this._waitingForAlarmSet = true;
      return;
    }
    // Stop any existing alarm.
    this._stopAlarm();
    this._settingAlarm = true;
    lazy.AlarmService.add(
      {
        date: new Date(Date.now() + delay),
        ignoreTimezone: true,
      },
      this._onAlarmFired.bind(this),
      function onSuccess(alarmID) {
        this._alarmID = alarmID;
        console.debug("Set alarm " + delay + " in the future " + this._alarmID);
        this._settingAlarm = false;
        if (this._waitingForAlarmSet) {
          this._waitingForAlarmSet = false;
          this._setAlarm(this._queuedAlarmDelay);
        }
      }.bind(this)
    );
  },

  _stopAlarm() {
    if (this._alarmID !== null) {
      console.debug("Stopped existing alarm " + this._alarmID);
      lazy.AlarmService.remove(this._alarmID);
      this._alarmID = null;
    }
  },

  /**
   * We need to calculate a new ping based on:
   *  1) Latest good ping
   *  2) A safe gap between 1) and the calculated new ping (which is
   *  by default, 1 minute)
   *
   * This is for 3G networks, whose connections keepalives differ broadly,
   * for example:
   *  1) Movistar Spain: 29 minutes
   *  2) VIVO Brazil: 5 minutes
   *  3) Movistar Colombia: XXX minutes
   *
   * So a fixed ping is not good for us for two reasons:
   *  1) We might lose the connection, so we need to reconnect again (wasting
   *  resources)
   *  2) We use a lot of network signaling just for pinging.
   *
   * This algorithm tries to search the best value between a disconnection and a
   * valid ping, to ensure better battery life and network resources usage.
   *
   * The value is saved in dom.push.pingInterval
   * @param wsWentDown [Boolean] if the WebSocket was closed or it is still
   * alive
   *
   */
  _calculateAdaptivePing(wsWentDown) {
    console.debug("_calculateAdaptivePing()");
    if (!this._adaptiveEnabled) {
      console.debug("calculateAdaptivePing: Adaptive ping is disabled");
      return;
    }

    if (this._retryFailCount > 0) {
      console.warn(
        "calculateAdaptivePing: Push has failed to connect to the",
        "Push Server",
        this._retryFailCount,
        "times. Do not calculate a new",
        "pingInterval now"
      );
      return;
    }

    if (!this._recalculatePing && !wsWentDown) {
      console.debug(
        "calculateAdaptivePing: We do not need to recalculate the",
        "ping now, based on previous data"
      );
      return;
    }

    // Save actual state of the network
    let ns = this._networkInfo.getNetworkInformation();

    if (ns.ip) {
      // mobile
      console.debug("calculateAdaptivePing: mobile");
      let oldNetwork = prefs.getStringPref("adaptive.mobile", "");
      let newNetwork = "mobile-" + ns.mcc + "-" + ns.mnc;

      // Mobile networks differ, reset all intervals and pings
      if (oldNetwork !== newNetwork) {
        // Network differ, reset all values
        console.debug(
          "calculateAdaptivePing: Mobile networks differ. Old",
          "network is",
          oldNetwork,
          "and new is",
          newNetwork
        );
        prefs.setStringPref("adaptive.mobile", newNetwork);
        //We reset the upper bound member
        this._recalculatePing = true;
        this._pingIntervalRetryTimes = {};

        // Put default values
        let defaultPing = prefs.getIntPref("pingInterval.default");
        prefs.setIntPref("pingInterval", defaultPing);
        this._lastGoodPingInterval = defaultPing;
      } else {
        // Mobile network is the same, let's just update things
        prefs.setIntPref(
          "pingInterval",
          prefs.getIntPref("pingInterval.mobile")
        );
        this._lastGoodPingInterval = prefs.getIntPref(
          "adaptive.lastGoodPingInterval.mobile"
        );
      }
    } else {
      // wifi
      console.debug("calculateAdaptivePing: wifi");
      prefs.setIntPref("pingInterval", prefs.getIntPref("pingInterval.wifi"));
      this._lastGoodPingInterval = prefs.getIntPref(
        "adaptive.lastGoodPingInterval.wifi"
      );
    }

    let nextPingInterval;
    let lastTriedPingInterval = prefs.getIntPref("pingInterval");

    if (wsWentDown) {
      console.debug(
        "calculateAdaptivePing: The WebSocket was disconnected.",
        "Calculating next ping"
      );

      // If we have not tried this pingInterval yet, initialize
      this._pingIntervalRetryTimes[lastTriedPingInterval] =
        (this._pingIntervalRetryTimes[lastTriedPingInterval] || 0) + 1;

      // Try the pingInterval at least 3 times, just to be sure that the
      // calculated interval is not valid.
      if (this._pingIntervalRetryTimes[lastTriedPingInterval] < 2) {
        console.debug(
          "calculateAdaptivePing: pingInterval=",
          lastTriedPingInterval,
          "tried only",
          this._pingIntervalRetryTimes[lastTriedPingInterval],
          "times"
        );
        return;
      }

      // Latest ping was invalid, we need to lower the limit to limit / 2
      nextPingInterval = Math.floor(lastTriedPingInterval / 2);

      // If the new ping interval is close to the last good one, we are near
      // optimum, so stop calculating.
      if (
        nextPingInterval - this._lastGoodPingInterval <
        prefs.getIntPref("adaptive.gap")
      ) {
        console.debug(
          "calculateAdaptivePing: We have reached the gap, we",
          "have finished the calculation. nextPingInterval=",
          nextPingInterval,
          "lastGoodPing=",
          this._lastGoodPingInterval
        );
        nextPingInterval = this._lastGoodPingInterval;
        this._recalculatePing = false;
      } else {
        console.debug("calculateAdaptivePing: We need to calculate next time");
        this._recalculatePing = true;
      }
    } else {
      console.debug("calculateAdaptivePing: The WebSocket is still up");
      this._lastGoodPingInterval = lastTriedPingInterval;
      nextPingInterval = Math.floor(lastTriedPingInterval * 1.5);
    }

    // Check if we have reached the upper limit
    if (this._upperLimit < nextPingInterval) {
      console.debug(
        "calculateAdaptivePing: Next ping will be bigger than the",
        "configured upper limit, capping interval"
      );
      this._recalculatePing = false;
      this._lastGoodPingInterval = lastTriedPingInterval;
      nextPingInterval = lastTriedPingInterval;
    }

    console.debug(
      "calculateAdaptivePing: Setting the pingInterval to",
      nextPingInterval
    );
    prefs.setIntPref("pingInterval", nextPingInterval);

    //Save values for our current network
    if (ns.ip) {
      prefs.setIntPref("pingInterval.mobile", nextPingInterval);
      prefs.setIntPref(
        "adaptive.lastGoodPingInterval.mobile",
        this._lastGoodPingInterval
      );
    } else {
      prefs.setIntPref("pingInterval.wifi", nextPingInterval);
      prefs.setIntPref(
        "adaptive.lastGoodPingInterval.wifi",
        this._lastGoodPingInterval
      );
    }
  },

  _makeWebSocket(uri) {
    if (!prefs.getBoolPref("connection.enabled")) {
      lazy.console.warn(
        "makeWebSocket: connection.enabled is not set to true.",
        "Aborting."
      );
      return null;
    }
    if (Services.io.offline) {
      lazy.console.warn("makeWebSocket: Network is offline.");
      return null;
    }
    let contractId =
      uri.scheme == "ws"
        ? "@mozilla.org/network/protocol;1?name=ws"
        : "@mozilla.org/network/protocol;1?name=wss";
    let socket = Cc[contractId].createInstance(Ci.nsIWebSocketChannel);

    socket.initLoadInfo(
      null, // aLoadingNode
      Services.scriptSecurityManager.getSystemPrincipal(),
      null, // aTriggeringPrincipal
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      Ci.nsIContentPolicy.TYPE_WEBSOCKET
    );
    // Allow deprecated HTTP request from SystemPrincipal
    socket.loadInfo.allowDeprecatedSystemRequests = true;

    return socket;
  },

  _beginWSSetup() {
    lazy.console.debug("beginWSSetup()");
    if (this._currentState != STATE_SHUT_DOWN) {
      lazy.console.error(
        "_beginWSSetup: Not in shutdown state! Current state",
        this._currentState
      );
      return Promise.resolve();
    }

    // Stop any pending reconnects scheduled for the near future.
    if (this._backoffTimer) {
      this._backoffTimer.cancel();
    }

    let uri = this._serverURI;
    if (!uri) {
      return Promise.resolve();
    }
    let socket = this._makeWebSocket(uri);
    if (!socket) {
      return Promise.resolve();
    }
    this._ws = socket.QueryInterface(Ci.nsIWebSocketChannel);

    this._wsListener = new PushWebSocketListener(this);
    this._ws.protocol = "push-notification";

    function tryOpenWS() {
      lazy.console.debug("beginWSSetup: Connecting to", uri.spec);
      try {
        // Grab a wakelock before we open the socket to ensure we don't go to
        // sleep before connection the is opened.
        this._ws.asyncOpen(uri, uri.spec, {}, 0, this._wsListener, null);
        this._acquireWakeLock("WebSocketSetup", this._requestTimeout);
        this._currentState = STATE_WAITING_FOR_WS_START;
      } catch (e) {
        lazy.console.error(
          "beginWSSetup: Error opening websocket.",
          "asyncOpen failed",
          e
        );
        this._reconnect();
      }
      return Promise.resolve();
    }

    if (this.credential) {
      return this.credential.requireAccessToken().then(
        _ => {
          return tryOpenWS.bind(this)();
        },
        errorStatus => {
          if (!this._hasPendingRequests()) {
            if (errorStatus > 0) {
              return this._mainPushService.getAllUnexpired().then(records => {
                if (records.length) {
                  this._reconnect();
                } else {
                  this._shutdownWS();
                }
                return Promise.resolve();
              });
            }
            this._shutdownWS();
            return Promise.resolve();
          }
          return Promise.resolve();
        }
      );
    }
    return tryOpenWS.bind(this)();
  },

  connect(broadcastListeners) {
    lazy.console.debug("connect()", broadcastListeners);
    this._broadcastListeners = broadcastListeners;
    this._beginWSSetup();
  },

  isConnected() {
    return !!this._ws;
  },

  _acquireWakeLock(reason, duration) {
    if (!AppConstants.MOZ_B2G) {
      return;
    }

    if (!this._socketWakeLock[reason]) {
      this._socketWakeLock[reason] = {
        wakeLock: null,
        timer: null,
      };
      console.debug(
        "acquireWakeLock: Acquiring " + reason + " Wakelock and Creating Timer"
      );
      this._socketWakeLock[
        reason
      ].wakeLock = lazy.gPowerManagerService.newWakeLock("cpu");
      this._socketWakeLock[reason].timer = Cc[
        "@mozilla.org/timer;1"
      ].createInstance(Ci.nsITimer);
    } else {
      if (!this._socketWakeLock[reason].wakeLock) {
        console.debug("acquireWakeLock: Acquiring " + reason + " Wakelock");
        this._socketWakeLock[
          reason
        ].wakeLock = lazy.gPowerManagerService.newWakeLock("cpu");
      }
      if (!this._socketWakeLock[reason].timer) {
        console.debug(
          "acquireWakeLock: Creating " + reason + " WakeLock Timer"
        );
        this._socketWakeLock[reason].timer = Cc[
          "@mozilla.org/timer;1"
        ].createInstance(Ci.nsITimer);
      }
    }

    let timeout = this._requestTimeoutBase;
    if (typeof duration === "number" && duration > 0) {
      timeout = duration;
    }
    console.debug("acquireWakeLock: Setting  " + reason + "  WakeLock Timer");
    this._socketWakeLock[reason].timer.initWithCallback(
      this._releaseWakeLock.bind(this, reason),
      // Allow the same time for socket setup as we do for
      // requests after the setup. Fudge it a bit since
      // timers can be a little off and we don't want to go
      // to sleep just as the socket connected.
      timeout + 1000,
      Ci.nsITimer.TYPE_ONE_SHOT
    );
  },

  _releaseWakeLock(reason) {
    if (!AppConstants.MOZ_B2G) {
      return;
    }
    if (!this._socketWakeLock[reason]) {
      console.warn(
        "Drop releasing " + reason + " wakeLock due to nonexistent!"
      );
      return;
    }

    console.debug("releaseWakeLock: Releasing " + reason + " WakeLock");
    if (this._socketWakeLock[reason].timer) {
      this._socketWakeLock[reason].timer.cancel();
    }
    if (this._socketWakeLock[reason].wakeLock) {
      this._socketWakeLock[reason].wakeLock.unlock();
      this._socketWakeLock[reason].wakeLock = null;
    }
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleHelloReply(reply) {
    lazy.console.debug("handleHelloReply()");
    if (this._currentState != STATE_WAITING_FOR_HELLO) {
      lazy.console.error(
        "handleHelloReply: Unexpected state",
        this._currentState,
        "(expected STATE_WAITING_FOR_HELLO)"
      );
      this._shutdownWS();
      return;
    }

    if (reply.status == 401) {
      console.error("handleHelloReply: Invalid Token");
      this._shutdownWS();
      if (this.credential) {
        this._mainPushService.getAllUnexpired().then(records => {
          if (records.length) {
            this.credential.refreshAccessToken();
            this._reconnect();
          }
        });
      }
      return;
    }

    if (typeof reply.uaid !== "string") {
      lazy.console.error("handleHelloReply: Received invalid UAID", reply.uaid);
      this._shutdownWS();
      return;
    }

    if (reply.uaid === "") {
      lazy.console.error("handleHelloReply: Received empty UAID");
      this._shutdownWS();
      return;
    }

    // To avoid sticking extra large values sent by an evil server into prefs.
    if (reply.uaid.length > 128) {
      lazy.console.error(
        "handleHelloReply: UAID received from server was too long",
        reply.uaid
      );
      this._shutdownWS();
      return;
    }

    this._retryFailCount = 0;

    let sendRequests = () => {
      if (this._notifyRequestQueue) {
        this._notifyRequestQueue();
        this._notifyRequestQueue = null;
      }
      this._sendPendingRequests();
    };

    function finishHandshake() {
      this._UAID = reply.uaid;
      this._currentState = STATE_READY;
      prefs.addObserver("userAgentID", this);

      // Handle broadcasts received in response to the "hello" message.
      if (!lazy.ObjectUtils.isEmpty(reply.broadcasts)) {
        // The reply isn't technically a broadcast message, but it has
        // the shape of a broadcast message (it has a broadcasts field).
        const context = { phase: lazy.pushBroadcastService.PHASES.HELLO };
        this._mainPushService.receivedBroadcastMessage(reply, context);
      }

      this._dataEnabled = !!reply.use_webpush;
      if (this._dataEnabled) {
        this._mainPushService.executeAllPendingUnregistering();
        this._mainPushService
          .getAllUnexpired()
          .then(records =>
            Promise.all(
              records.map(record =>
                this._mainPushService.ensureCrypto(record).catch(error => {
                  lazy.console.error(
                    "finishHandshake: Error updating record",
                    record.keyID,
                    error
                  );
                })
              )
            )
          )
          .then(sendRequests);
      } else {
        sendRequests();
      }
    }

    // By this point we've got a UAID from the server that we are ready to
    // accept.
    //
    // We unconditionally drop all existing registrations and notify service
    // workers if we receive a new UAID. This ensures we expunge all stale
    // registrations if the `userAgentID` pref is reset.
    if (this._UAID != reply.uaid) {
      lazy.console.debug("handleHelloReply: Received new UAID");

      this._mainPushService
        .dropUnexpiredRegistrations()
        .then(finishHandshake.bind(this));

      return;
    }

    // otherwise we are good to go
    finishHandshake.bind(this)();
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleRegisterReply(reply) {
    lazy.console.debug("handleRegisterReply()");

    let tmp = this._takeRequestForReply(reply);
    if (!tmp) {
      return;
    }

    if (reply.status == 200) {
      try {
        Services.io.newURI(reply.pushEndpoint);
      } catch (e) {
        tmp.reject(new Error("Invalid push endpoint: " + reply.pushEndpoint));
        return;
      }

      let record = new PushRecordWebSocket({
        channelID: reply.channelID,
        pushEndpoint: reply.pushEndpoint,
        scope: tmp.record.scope,
        originAttributes: tmp.record.originAttributes,
        version: null,
        systemRecord: tmp.record.systemRecord,
        appServerKey: tmp.record.appServerKey,
        ctime: Date.now(),
      });
      tmp.resolve(record);
    } else {
      lazy.console.error(
        "handleRegisterReply: Unexpected server response",
        reply
      );
      tmp.reject(
        new Error("Wrong status code for register reply: " + reply.status)
      );
    }
  },

  _handleUnregisterReply(reply) {
    lazy.console.debug("handleUnregisterReply()");

    let request = this._takeRequestForReply(reply);
    if (!request) {
      return;
    }

    let success = reply.status === 200;
    request.resolve(success);

    if (success) {
      if (typeof reply.channelID !== "string") {
        console.warn(
          "handleUnregisterReply: Discarding delete db without channel ID"
        );
        return;
      }
      this._mainPushService.removePendingUnsubscribe(reply.channelID);
    }
  },

  _queueUpdateStart: Promise.resolve(),
  _queueUpdate: null,
  _enqueueUpdateCount: 0,
  _enqueueUpdate(op) {
    console.debug("enqueueUpdate()");
    if (!this._queueUpdate) {
      this._queueUpdate = this._queueUpdateStart;
      this._enqueueUpdateCount = 0;
    }
    this._enqueueUpdateCount++;
    this._queueUpdate = this._queueUpdate
      .then(op)
      .catch(_ => {})
      .then(_ => {
        this._enqueueUpdateCount--;
        if (this._enqueueUpdateCount == 0) {
          this._releaseWakeLock("DataUpdate");
        }
      });
  },

  _handleDataUpdate(update) {
    this._acquireWakeLock("DataUpdate");
    let promise;
    if (typeof update.channelID != "string") {
      lazy.console.warn(
        "handleDataUpdate: Discarding update without channel ID",
        update
      );
      return;
    }

    function updateRecord(record) {
      // Ignore messages that we've already processed. This can happen if the
      // connection drops between notifying the service worker and acking the
      // the message. In that case, the server will re-send the message on
      // reconnect.
      if (record.hasRecentMessageID(update.version)) {
        lazy.console.warn(
          "handleDataUpdate: Ignoring duplicate message",
          update.version
        );
        return null;
      }
      record.noteRecentMessageID(update.version);
      return record;
    }
    if (typeof update.data != "string") {
      promise = this._mainPushService.receivedPushMessage(
        update.channelID,
        update.version,
        null,
        null,
        updateRecord
      );
    } else {
      let message = ChromeUtils.base64URLDecode(update.data, {
        // The Push server may append padding.
        padding: "ignore",
      });
      promise = this._mainPushService.receivedPushMessage(
        update.channelID,
        update.version,
        update.headers,
        message,
        updateRecord
      );
    }
    promise
      .then(
        status => {
          this._sendAck(update.channelID, update.version, status);
        },
        err => {
          lazy.console.error(
            "handleDataUpdate: Error delivering message",
            update,
            err
          );
          this._sendAck(
            update.channelID,
            update.version,
            Ci.nsIPushErrorReporter.ACK_DECRYPTION_ERROR
          );
        }
      )
      .catch(err => {
        lazy.console.error(
          "handleDataUpdate: Error acknowledging message",
          update,
          err
        );
      });
  },

  /**
   * Protocol handler invoked by server message.
   */
  _handleNotificationReply(reply) {
    lazy.console.debug("handleNotificationReply()");
    if (this._dataEnabled) {
      this._enqueueUpdate(_ => this._handleDataUpdate(reply));
      return;
    }

    if (typeof reply.updates !== "object") {
      lazy.console.warn(
        "handleNotificationReply: Missing updates",
        reply.updates
      );
      return;
    }

    lazy.console.debug("handleNotificationReply: Got updates", reply.updates);
    for (let i = 0; i < reply.updates.length; i++) {
      let update = reply.updates[i];
      lazy.console.debug("handleNotificationReply: Handling update", update);
      if (typeof update.channelID !== "string") {
        lazy.console.debug(
          "handleNotificationReply: Invalid update at index",
          i,
          update
        );
        continue;
      }

      if (update.version === undefined) {
        lazy.console.debug("handleNotificationReply: Missing version", update);
        continue;
      }

      let version = update.version;

      if (typeof version === "string") {
        version = parseInt(version, 10);
      }

      if (typeof version === "number" && version >= 0) {
        // FIXME(nsm): this relies on app update notification being infallible!
        // eventually fix this
        this._receivedUpdate(update.channelID, version);
      }
    }
  },

  _handleBroadcastReply(reply) {
    let phase = lazy.pushBroadcastService.PHASES.BROADCAST;
    // Check if this reply is the result of registration.
    for (const id of Object.keys(reply.broadcasts)) {
      const wasRegistering = this._currentlyRegistering.delete(id);
      if (wasRegistering) {
        // If we get multiple broadcasts and only one is "registering",
        // then we consider the phase to be REGISTER for all of them.
        // It is acceptable since registrations do not happen so often,
        // and are all very likely to occur soon after browser startup.
        phase = lazy.pushBroadcastService.PHASES.REGISTER;
      }
    }
    const context = { phase };
    this._mainPushService.receivedBroadcastMessage(reply, context);
  },

  reportDeliveryError(messageID, reason) {
    lazy.console.debug("reportDeliveryError()");
    let code = kDELIVERY_REASON_TO_CODE[reason];
    if (!code) {
      throw new Error("Invalid delivery error reason");
    }
    let data = { messageType: "nack", version: messageID, code };
    this._queueRequest(data);
  },

  _sendAck(channelID, version, status) {
    lazy.console.debug("sendAck()");
    let code = kACK_STATUS_TO_CODE[status];
    if (!code) {
      throw new Error("Invalid ack status");
    }
    let data = { messageType: "ack", updates: [{ channelID, version, code }] };
    this._queueRequest(data);
  },

  _generateID() {
    // generateUUID() gives a UUID surrounded by {...}, slice them off.
    return Services.uuid
      .generateUUID()
      .toString()
      .slice(1, -1);
  },

  register(record) {
    lazy.console.debug("register() ", record);

    let data = { channelID: this._generateID(), messageType: "register" };

    if (record.appServerKey) {
      data.key = ChromeUtils.base64URLEncode(record.appServerKey, {
        // The Push server requires padding.
        pad: true,
      });
    }

    return this._sendRequestForReply(record, data).then(record => {
      if (!this._dataEnabled) {
        return record;
      }
      return PushCrypto.generateKeys().then(([publicKey, privateKey]) => {
        record.p256dhPublicKey = publicKey;
        record.p256dhPrivateKey = privateKey;
        record.authenticationSecret = PushCrypto.generateAuthenticationSecret();
        return record;
      });
    });
  },

  unregister(record, reason) {
    lazy.console.debug("unregister() ", record, reason);

    return Promise.resolve().then(_ => {
      let code = kUNREGISTER_REASON_TO_CODE[reason];
      if (!code) {
        throw new Error("Invalid unregister reason");
      }
      let data = {
        channelID: record.channelID,
        messageType: "unregister",
        code,
      };

      return this._sendRequestForReply(record, data);
    });
  },

  _queueStart: Promise.resolve(),
  _notifyRequestQueue: null,
  _queue: null,
  _enqueue(op) {
    lazy.console.debug("enqueue()");
    if (!this._queue) {
      this._queue = this._queueStart;
    }
    this._queue = this._queue.then(op).catch(_ => {});
  },

  /** Sends a request to the server. */
  _send(data) {
    if (this._currentState != STATE_READY) {
      lazy.console.warn(
        "send: Unexpected state; ignoring message",
        this._currentState
      );
      return;
    }
    if (!this._requestHasReply(data)) {
      this._wsSendMessage(data);
      return;
    }
    // If we're expecting a reply, check that we haven't cancelled the request.
    let key = this._makePendingRequestKey(data);
    if (!this._pendingRequests.has(key)) {
      lazy.console.log("send: Request cancelled; ignoring message", key);
      return;
    }
    this._wsSendMessage(data);
  },

  /** Indicates whether a request has a corresponding reply from the server. */
  _requestHasReply(data) {
    return data.messageType == "register" || data.messageType == "unregister";
  },

  /**
   * Sends all pending requests that expect replies. Called after the connection
   * is established and the handshake is complete.
   */
  _sendPendingRequests() {
    this._enqueue(_ => {
      for (let request of this._pendingRequests.values()) {
        this._send(request.data);
      }
    });
  },

  /** Queues an outgoing request, establishing a connection if necessary. */
  _queueRequest(data) {
    lazy.console.debug("queueRequest()", data);

    if (this._currentState == STATE_READY) {
      // If we're ready, no need to queue; just send the request.
      this._send(data);
      return;
    }

    // Otherwise, we're still setting up. If we don't have a request queue,
    // make one now.
    if (!this._notifyRequestQueue) {
      let promise = new Promise((resolve, reject) => {
        this._notifyRequestQueue = resolve;
      });
      this._enqueue(_ => promise);
    }

    let isRequest = this._requestHasReply(data);
    if (!isRequest) {
      // Don't queue requests, since they're stored in `_pendingRequests`, and
      // `_sendPendingRequests` will send them after reconnecting. Without this
      // check, we'd send requests twice.
      this._enqueue(_ => this._send(data));
    }

    if (!this._ws) {
      // This will end up calling notifyRequestQueue().
      this._beginWSSetup().then(_ => {
        // If beginWSSetup does not succeed to make ws, notifyRequestQueue will
        // not be call.
        if (!this._ws && this._notifyRequestQueue) {
          this._notifyRequestQueue();
          this._notifyRequestQueue = null;
        }
      });
    }
  },

  _receivedUpdate(aChannelID, aLatestVersion) {
    lazy.console.debug(
      "receivedUpdate: Updating",
      aChannelID,
      "->",
      aLatestVersion
    );

    this._mainPushService
      .receivedPushMessage(aChannelID, "", null, null, record => {
        if (record.version === null || record.version < aLatestVersion) {
          lazy.console.debug(
            "receivedUpdate: Version changed for",
            aChannelID,
            aLatestVersion
          );
          record.version = aLatestVersion;
          return record;
        }
        lazy.console.debug(
          "receivedUpdate: No significant version change for",
          aChannelID,
          aLatestVersion
        );
        return null;
      })
      .then(status => {
        this._sendAck(aChannelID, aLatestVersion, status);
      })
      .catch(err => {
        lazy.console.error(
          "receivedUpdate: Error acknowledging message",
          aChannelID,
          aLatestVersion,
          err
        );
      });
  },

  // begin Push protocol handshake
  _wsOnStart(context) {
    lazy.console.debug("wsOnStart()");
    this._releaseWakeLock("WebSocketSetup");

    if (this._currentState != STATE_WAITING_FOR_WS_START) {
      lazy.console.error(
        "wsOnStart: NOT in STATE_WAITING_FOR_WS_START. Current",
        "state",
        this._currentState,
        "Skipping"
      );
      return;
    }

    this._mainPushService
      .getAllUnexpired()
      .then(
        records => this._sendHello(records),
        err => {
          lazy.console.warn(
            "Error fetching existing records before handshake; assuming none",
            err
          );
          this._sendHello([]);
        }
      )
      .catch(err => {
        // If we failed to send the handshake, back off and reconnect.
        lazy.console.warn("Failed to send handshake; reconnecting", err);
        this._reconnect();
      });
  },

  /**
   * Sends a `hello` handshake to the server.
   *
   * @param {Array<PushRecordWebSocket>} An array of records for existing
   *        subscriptions, used to determine whether to rotate our UAID.
   */
  _sendHello(records) {
    let data = {
      messageType: "hello",
      broadcasts: this._broadcastListeners,
      use_webpush: true,
    };

    if (this._UAID) {
      data.uaid = this._UAID;
    }

    if (this.credential && this.credential.token) {
      data.token = this.credential.token;
    }

    this._wsSendMessage(data);
    this._currentState = STATE_WAITING_FOR_HELLO;
  },

  /**
   * This statusCode is not the websocket protocol status code, but the TCP
   * connection close status code.
   *
   * If we do not explicitly call ws.close() then statusCode is always
   * NS_BASE_STREAM_CLOSED, even on a successful close.
   */
  _wsOnStop(context, statusCode) {
    lazy.console.debug("wsOnStop()");
    this._releaseWakeLock("WebSocketSetup");

    if (statusCode != Cr.NS_OK && !this._skipReconnect) {
      lazy.console.debug(
        "wsOnStop: Reconnecting after socket error",
        statusCode
      );
      this._reconnect();
      return;
    }

    this._shutdownWS();
  },

  _wsOnMessageAvailable(context, message) {
    lazy.console.debug("wsOnMessageAvailable()", message);

    // Clearing the last ping time indicates we're no longer waiting for a pong.
    this._lastPingTime = 0;

    let reply;
    try {
      reply = JSON.parse(message);
    } catch (e) {
      lazy.console.warn("wsOnMessageAvailable: Invalid JSON", message, e);
      return;
    }

    // If we receive a message, we know the connection succeeded. Reset the
    // connection attempt and ping interval counters.
    if (this._currentState == STATE_READY) {
      this._retryFailCount = 0;
      this._pingIntervalRetryTimes = {};
    }

    let doNotHandle = false;
    if (
      message === "{}" ||
      reply.messageType === undefined ||
      reply.messageType === "ping" ||
      typeof reply.messageType != "string"
    ) {
      lazy.console.debug("wsOnMessageAvailable: Pong received");
      this._calculateAdaptivePing(false);
      doNotHandle = true;
    }

    // Reset the ping timer.  Note: This path is executed at every step of the
    // handshake, so this timer does not need to be set explicitly at startup.
    if (this._alarmEnabled) {
      this._setAlarm(prefs.getIntPref("pingInterval"));
    } else {
      this._startPingTimer();
    }

    // If it is a ping, do not handle the message.
    if (doNotHandle) {
      if (!this._hasPendingRequests()) {
        this._requestTimeoutTimer.cancel();
      }
      return;
    }

    // An allowlist of protocol handlers. Add to these if new messages are added
    // in the protocol.
    let handlers = [
      "Hello",
      "Register",
      "Unregister",
      "Notification",
      "Broadcast",
    ];

    // Build up the handler name to call from messageType.
    // e.g. messageType == "register" -> _handleRegisterReply.
    let handlerName =
      reply.messageType[0].toUpperCase() +
      reply.messageType.slice(1).toLowerCase();

    if (!handlers.includes(handlerName)) {
      lazy.console.warn(
        "wsOnMessageAvailable: No allowlisted handler",
        handlerName,
        "for message",
        reply.messageType
      );
      return;
    }

    let handler = "_handle" + handlerName + "Reply";

    if (typeof this[handler] !== "function") {
      lazy.console.warn(
        "wsOnMessageAvailable: Handler",
        handler,
        "allowlisted but not implemented"
      );
      return;
    }

    this[handler](reply);
  },

  /**
   * The websocket should never be closed. Since we don't call ws.close(),
   * _wsOnStop() receives error code NS_BASE_STREAM_CLOSED (see comment in that
   * function), which calls reconnect and re-establishes the WebSocket
   * connection.
   *
   * If the server requested that we back off, we won't reconnect until the
   * next network state change event, or until we need to send a new register
   * request.
   */
  _wsOnServerClose(context, aStatusCode, aReason) {
    lazy.console.debug("wsOnServerClose()", aStatusCode, aReason);

    if (aStatusCode == kBACKOFF_WS_STATUS_CODE) {
      lazy.console.debug("wsOnServerClose: Skipping automatic reconnect");
      this._skipReconnect = true;
    }
  },

  /**
   * Rejects all pending register requests with errors.
   */
  _cancelPendingRequests() {
    for (let request of this._pendingRequests.values()) {
      request.reject(new Error("Request aborted"));
    }
    this._pendingRequests.clear();
  },

  /** Creates a case-insensitive map key for a request that expects a reply. */
  _makePendingRequestKey(data) {
    return (data.messageType + "|" + data.channelID).toLowerCase();
  },

  /** Sends a request and waits for a reply from the server. */
  _sendRequestForReply(record, data) {
    return Promise.resolve().then(_ => {
      // start the timer since we now have at least one request
      this._startRequestTimeoutTimer();

      let key = this._makePendingRequestKey(data);
      if (!this._pendingRequests.has(key)) {
        let request = {
          data,
          record,
          ctime: Date.now(),
        };
        request.promise = new Promise((resolve, reject) => {
          request.resolve = resolve;
          request.reject = reject;
        });
        this._pendingRequests.set(key, request);
        this._queueRequest(data);
      }

      return this._pendingRequests.get(key).promise;
    });
  },

  /** Removes and returns a pending request for a server reply. */
  _takeRequestForReply(reply) {
    if (typeof reply.channelID !== "string") {
      return null;
    }
    let key = this._makePendingRequestKey(reply);
    let request = this._pendingRequests.get(key);
    if (!request) {
      return null;
    }
    this._pendingRequests.delete(key);
    if (!this._hasPendingRequests()) {
      this._requestTimeoutTimer.cancel();
      this._requestTimeout = this._requestTimeoutBase;
    }
    return request;
  },

  sendSubscribeBroadcast(serviceId, version) {
    this._currentlyRegistering.add(serviceId);
    let data = {
      messageType: "broadcast_subscribe",
      broadcasts: {
        [serviceId]: version,
      },
    };

    this._queueRequest(data);
  },
};

var PushNetworkInfo = {
  /**
   * Returns information about MCC-MNC and the IP of the current connection.
   */
  getNetworkInformation() {
    console.debug("PushNetworkInfo: getNetworkInformation()");

    try {
      let nm = Cc["@mozilla.org/network/manager;1"].getService(
        Ci.nsINetworkManager
      );
      if (
        nm.activeNetworkInfo &&
        nm.activeNetworkInfo.type == Ci.nsINetworkInfo.NETWORK_TYPE_MOBILE
      ) {
        let iccService = Cc["@mozilla.org/icc/iccservice;1"].getService(
          Ci.nsIIccService
        );
        // TODO: Bug 927721 - PushService for multi-sim
        // In Multi-sim, there is more than one client in iccService. Each
        // client represents a icc handle. To maintain backward compatibility
        // with single sim, we always use client 0 for now. Adding support
        // for multiple sim will be addressed in bug 927721, if needed.
        let clientId = 0;
        let icc = iccService.getIccByServiceId(clientId);
        let iccInfo = icc && icc.iccInfo;
        if (iccInfo) {
          console.debug("getNetworkInformation: Running on mobile data");

          let ips = {};
          let prefixLengths = {};
          nm.activeNetworkInfo.getAddresses(ips, prefixLengths);

          return {
            mcc: iccInfo.mcc,
            mnc: iccInfo.mnc,
            ip: ips.value[0],
          };
        }
      }
    } catch (e) {
      console.error(
        "getNetworkInformation: Error recovering mobile network",
        "information",
        e
      );
    }

    console.debug("getNetworkInformation: Running on wifi");
    return {
      mcc: 0,
      mnc: 0,
      ip: undefined,
    };
  },
};

function PushRecordWebSocket(record) {
  PushRecord.call(this, record);
  this.channelID = record.channelID;
  this.version = record.version;
}

PushRecordWebSocket.prototype = Object.create(PushRecord.prototype, {
  keyID: {
    get() {
      return this.channelID;
    },
  },
});

PushRecordWebSocket.prototype.toSubscription = function() {
  let subscription = PushRecord.prototype.toSubscription.call(this);
  subscription.version = this.version;
  return subscription;
};
