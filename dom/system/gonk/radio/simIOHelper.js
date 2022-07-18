/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);

Cu.import("resource://gre/modules/ril_consts.js");

var RIL_DEBUG = ChromeUtils.import(
  "resource://gre/modules/ril_consts_debug.js"
);

// set to true in ril_consts.js to see debug messages
var DEBUG = RIL_DEBUG.DEBUG_RIL;
var GLOBAL = this;

// Timeout value for emergency callback mode.
const EMERGENCY_CB_MODE_TIMEOUT_MS = 300000; // 5 mins = 300000 ms.

const GET_CURRENT_CALLS_RETRY_MAX = 3;

// Timeout value for _waitingModemRestart
const MODEM_RESTART_TIMEOUT_MS = 15000; // 15 seconds

const PDU_HEX_OCTET_SIZE = 2;

const EARTH_RADIUS_METER = 6371 * 1000;
const GEO_FENCING_MAXIMUM_WAIT_TIME = 0x01;
const GEO_FENCING_POLYGON = 0x02;
const GEO_FENCING_CIRCLE = 0x03;

var RILQUIRKS_CALLSTATE_EXTRA_UINT32;
var RILQUIRKS_REQUEST_USE_DIAL_EMERGENCY_CALL;
var RILQUIRKS_SIM_APP_STATE_EXTRA_FIELDS;
var RILQUIRKS_SIGNAL_EXTRA_INT32;
var RILQUIRKS_AVAILABLE_NETWORKS_EXTRA_STRING;
// Needed for call-waiting on Peak device
var RILQUIRKS_EXTRA_UINT32_2ND_CALL;
// On the emulator we support querying the number of lock retries
var RILQUIRKS_HAVE_QUERY_ICC_LOCK_RETRY_COUNT;
// Ril quirk to Send STK Profile Download
var RILQUIRKS_SEND_STK_PROFILE_DOWNLOAD;
// Ril quirk to attach data registration on demand.
var RILQUIRKS_DATA_REGISTRATION_ON_DEMAND;
// Ril quirk to control the uicc/data subscription.
var RILQUIRKS_SUBSCRIPTION_CONTROL;
// Ril quirk to describe the SMSC address format.
var RILQUIRKS_SMSC_ADDRESS_FORMAT;
// APP Cellbroadcast list configuration
/* true : controlled by customization which is controlled by mcc-mnc.json.
 * false: controlled by setting "ril.cellbroadcast.searchlist" which is loaded from apn.json
 */
const kPrefAppCBConfigurationEnabled = "dom.app_cb_configuration";

if (!this.debug) {
  // Debugging stub that goes nowhere.
  this.debug = function debug(message) {
    dump("SimIOHelper: " + message + "\n");
  };
}

function updateDebugFlag() {
  // Read debug setting from pref
  try {
    DEBUG =
      RIL_DEBUG.DEBUG_RIL ||
      Services.prefs.getBoolPref(RIL_DEBUG.PREF_RIL_DEBUG_ENABLED);
  } catch (e) {}
}
updateDebugFlag();

//TODO: Find better place
function LatLng(alat, alng, aAccuracyInMeters = 0) {
  this.lat = alat;
  this.lng = alng;
  this.accuracyInMeters = aAccuracyInMeters;
}
LatLng.prototype = {
  lat: 0,
  lng: 0,
  accuracyInMeters: 0,

  toRadians(aDegree) {
    return aDegree * (Math.PI / 180);
  },

  distance(aLatLng) {
    let dlat = Math.sin(0.5 * this.toRadians(this.lat - aLatLng.lat));
    let dlng = Math.sin(0.5 * this.toRadians(this.lng - aLatLng.lng));
    let x =
      dlat * dlat +
      dlng *
        dlng *
        Math.cos(this.toRadians(this.lat)) *
        Math.cos(this.toRadians(aLatLng.lat));
    return 2 * Math.atan2(Math.sqrt(x), Math.sqrt(1 - x)) * EARTH_RADIUS_METER;
  },
};

function Geometry() {}
Geometry.prototype = {
  type: GEOMETRY_TYPE_UNKNOW,
  contains(aLatLng, _aThresholdInMeters = 0) {
    if (DEBUG) {
      debug("Non-implemented geometry method");
    }
  },
};

function Point(aX, aY) {
  this.x = aX;
  this.y = aY;
}
Point.prototype = {
  subtract(aPoint) {
    return new Point(this.x - aPoint.x, this.y - aPoint.y);
  },

  distance(aPoint) {
    return Math.sqrt(Math.pow(this.x - aPoint.x, 2) + Math.pow(this.y - aPoint.y, 2));
  },

  equals(aPoint) {
    if (this == aPoint){
      return true;
    }
    return (this.x == aPoint.x && this.y == aPoint.y);
  },
};

function LineSegment(aPointA, aPointB) {
  this.pointA = aPointA;
  this.pointB = aPointB;
}
LineSegment.prototype = {
  length() {
    return this.pointA.distance(this.pointB);
  },

  distance(aPoint) {
    if (this.pointA.equals(this.pointB)) {
      return aPoint.distance(this.pointA);
    }

    let sub1 = aPoint.subtract(this.pointA);
    let sub2 = this.pointB.subtract(this.pointA);
    let dot = sub1.x * sub2.x + sub1.y * sub2.y;

    let magnitude = dot / (Math.pow(this.length(), 2));

    if (magnitude > 1.0) {
      magnitude = 1.0;
    } else if (magnitude < 0.0) {
      magnitude = 0.0;
    }

    let projectX = this.pointA.x + ((this.pointB.x - this.pointA.x) * magnitude);
    let projectY = this.pointA.y + ((this.pointB.y - this.pointA.y) * magnitude);

    return aPoint.distance(new Point(projectX, projectY));
    },
};

function Polygon(aLatLngs) {
  this._vertices = [];
  this._scaledVertices = [];
  aLatLngs.forEach(latlng => {this._vertices.push(new LatLng(latlng.lat, latlng.lng));});
  this.type = GEOMETRY_TYPE_POLYGON;

  // Find the point with smallest longitude as the mOrigin point.
  let idx = 0;
  for (let i = 1; i < aLatLngs.length; i++) {
    if (aLatLngs[i].lng < aLatLngs[idx].lng) {
      idx = i;
    }
  }
  this._origin = aLatLngs[idx];

  for (let i = 0; i < aLatLngs.length; i++) {
    let latLng = aLatLngs[i];
    this._scaledVertices.push(this._convertAndScaleLatLng(latLng));
  }
}
Polygon.prototype = {
  __proto__: Geometry.prototype,
  type: GEOMETRY_TYPE_POLYGON,
  _vertices: [],
  _scaledVertices: [],
  _origin: null,

  _convertAndScaleLatLng(aLatLng) {
    let pointX = aLatLng.lat - this._origin.lat;
    let pointY = aLatLng.lng - this._origin.lng;

    if (
      Math.sign(this._origin.lng) != 0 &&
      Math.sign(this._origin.lng) != Math.sign(aLatLng.lng)
    ) {
      let distCross0thMeridian =
        Math.abs(this._origin.lng) + Math.abs(aLatLng.lng);
      if (Math.sign(distCross0thMeridian * 2 - 360) > 0) {
        pointY = Math.sign(this._origin.lng) * (360 - distCross0thMeridian);
      }
    }
    return new Point(pointX * 1000.0, pointY * 1000.0);
  },

  _crossProduct(aPointA, aPointB) {
    return aPointA.x * aPointB.y - aPointA.y * aPointB.x;
  },

  _convertToDistanceFromOrigin(aLatLng) {
    let x = new LatLng(aLatLng.lat, _origin.lng).distance(new LatLng(_origin.lat, _origin.lng));
    let y = new LatLng(_origin.lat, aLatLng.lng).distance(new LatLng(_origin.lat, _origin.lng));

    x = aLatLng.lat > _origin.lat ? x : -x;
    y = aLatLng.lng > _origin.lng ? y : -y;
    return new Point(x,y);
  },

  distance(aLatLng) {
    let minDistance = Number.MAX_VALUE;
    let verticesLength = this._vertices.length;

    for (let i = 0; i < verticesLength; i++) {
      let verticeA = this._vertices[i];
      let verticeB = this._vertices[(i+1) % verticesLength];

      let pointSA = _convertToDistanceFromOrigin(verticeA);
      let pointSB = _convertToDistanceFromOrigin(verticeB);
      let pointSP = _convertToDistanceFromOrigin(aLatLng);

      let line = new LineSegment(pointSA, pointSB);
      let distance = line.distance(pointSP);

      minDistance = Math.min(distance, minDistance);
    }
      return minDistance;
  },

  /**
   * Check if the given point is inside the polygon.
   * Winding Number Algorithm.
   * The winding number would be zero if the point inside the polygon.
   */
  contains(aLatLng, aThresholdInMeters = 0) {
    if(aThresholdInMeters > 0) {
      //TODO: Handle accuracy > threshold
      return this.distance(aLatLng) <= aThresholdInMeters;
    }
    let scaledPoint = this._convertAndScaleLatLng(aLatLng);

    let verticesLength = this._scaledVertices.length;
    let windingNumber = 0;
    for (let i = 0; i < verticesLength; i++) {
      let pointA = this._scaledVertices[i];
      let pointB = this._scaledVertices[(i + 1) % verticesLength];

      let counterClockwise = Math.sign(
        this._crossProduct(
          pointB.subtract(pointA),
          scaledPoint.subtract(pointA)
        )
      );
      if (counterClockwise == 0) {
        if (
          Math.min(pointA.x, pointB.x) <= scaledPoint.x &&
          scaledPoint.x <= Math.max(pointA.x, pointB.x) &&
          Math.min(pointA.y, pointB.y) <= scaledPoint.y &&
          scaledPoint.y <= Math.max(pointA.y, pointB.y)
        ) {
          return true;
        }
      } else if (Math.sign(pointA.y - scaledPoint.y) <= 0) {
        // upward crossing
        if (counterClockwise > 0 && Math.sign(pointB.y - scaledPoint.y) > 0) {
          ++windingNumber;
        }
      } else {
        // downward crossing
        if (counterClockwise < 0 && Math.sign(pointB.y - scaledPoint.y) <= 0) {
          --windingNumber;
        }
      }
    }
    return windingNumber != 0;
  },
};

function Circle(aCenter, aRadiusInMeters) {
  this._center = new LatLng(aCenter.lat, aCenter.lng);
  this._radius = aRadiusInMeters;
  this.type = GEOMETRY_TYPE_CIRCLE;
}
Circle.prototype = {
  __proto__: Geometry.prototype,
  type: GEOMETRY_TYPE_CIRCLE,
  _center: null,
  _radius: 0,

  contains(aLatLng, aThresholdInMeters = 0) {
    //TODO: Handle accuracy > threshold
    return this._center.distance(aLatLng) <= (this._radius + aThresholdInMeters);
  },

};

function Context(aRadioInterfcae) {
  this.clientId = aRadioInterfcae.clientId;
  this.RIL = aRadioInterfcae;
  updateDebugFlag();
}
Context.prototype = {
  RIL: null,

  debug(aMessage) {
    GLOBAL.debug("[" + this.RIL.clientId + "] " + aMessage);
  },
};

(function() {
  let lazySymbols = [
    "ICCContactHelper",
    "ICCRecordHelper",
    "ICCIOHelper",
    "ICCFileHelper",
    "ICCUtilsHelper",
    "ICCPDUHelper",
    "GsmPDUHelper",
    "SimRecordHelper",
    "ISimRecordHelper",
    "BerTlvHelper",
    "ComprehensionTlvHelper",
    "StkProactiveCmdHelper",
    "StkCommandParamsFactory",
  ];

  for (let i = 0; i < lazySymbols.length; i++) {
    let symbol = lazySymbols[i];
    Object.defineProperty(Context.prototype, symbol, {
      get() {
        let real = new GLOBAL[symbol + "Object"](this);
        Object.defineProperty(this, symbol, {
          value: real,
          enumerable: true,
        });
        return real;
      },
      configurable: true,
      enumerable: true,
    });
  }
})();

/**
 * Helper for ICC IO functionalities.
 */
function ICCIOHelperObject(aContext) {
  this.context = aContext;
}
ICCIOHelperObject.prototype = {
  context: null,

  /**
   * Load EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber [optional]
   *        The number of the record shall be loaded.
   * @param recordSize [optional]
   *        The size of the record.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadLinearFixedEF(options) {
    let cb;
    let readRecord = function(options) {
      options.command = ICC_COMMAND_READ_RECORD;
      options.p1 = options.recordNumber || 1; // Record number
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      options.callback = cb || options.callback;

      this.context.RIL.sendWorkerMessage("iccIO", options, null);
    }.bind(this);

    options.structure = EF_STRUCTURE_LINEAR_FIXED;
    options.pathId =
      options.pathId || this.context.ICCFileHelper.getEFPath(options.fileId);
    if (options.recordSize) {
      readRecord(options);
      return;
    }
    cb = options.callback;
    options.callback = readRecord;
    this.getResponse(options);
  },

  /**
   * Load next record from current record Id.
   */
  loadNextRecord(options) {
    options.p1++;
    this.context.RIL.sendWorkerMessage("iccIO", options, null);
  },

  /**
   * Update EF with type 'Linear Fixed'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param recordNumber
   *        The number of the record shall be updated.
   * @param dataWriter [optional]
   *        The function for writing string parameter for the ICC_COMMAND_UPDATE_RECORD.
   * @param pin2 [optional]
   *        PIN2 is required when updating ICC_EF_FDN.
   * @param callback [optional]
   *        The callback function shall be called when the record is updated.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  updateLinearFixedEF(options) {
    if (!options.fileId || !options.recordNumber) {
      throw new Error(
        "Unexpected fileId " +
          options.fileId +
          " or recordNumber " +
          options.recordNumber
      );
    }

    options.structure = EF_STRUCTURE_LINEAR_FIXED;
    options.pathId = this.context.ICCFileHelper.getEFPath(options.fileId);
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_UPDATE_RECORD;
      options.p1 = options.recordNumber;
      options.p2 = READ_RECORD_ABSOLUTE_MODE;
      options.p3 = options.recordSize;
      if (options.command == ICC_COMMAND_UPDATE_RECORD && options.dataWriter) {
        options.dataWriter(options.recordSize);
        options.dataWriter = this.context.GsmPDUHelper.pdu;
      }
      this.context.RIL.sendWorkerMessage("iccIO", options, null);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Load EF with type 'Transparent'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is read.
   * @param onerror [optional]
   *        The callback function shall be called when failure.
   */
  loadTransparentEF(options) {
    options.structure = EF_STRUCTURE_TRANSPARENT;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_READ_BINARY;
      options.p2 = 0x00;
      options.p3 = options.fileSize;
      this.context.RIL.sendWorkerMessage("iccIO", options, null);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Update EF with type 'Transparent'.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   * @param dataWriter [optional]
   *        The function for writing string parameter for the ICC_COMMAND_UPDATE_BINARY.
   * @param callback [optional]
   *        The callback function shall be called when the record(s) is update.
   * @param on error [optional]
   *        The callback function shall be called when failure.
   */
  updateTransparentEF(options) {
    options.structure = EF_STRUCTURE_TRANSPARENT;
    let cb = options.callback;
    options.callback = function callback(options) {
      options.callback = cb;
      options.command = ICC_COMMAND_UPDATE_BINARY;
      options.p2 = 0x00;
      options.p3 = options.fileSize;
      this.context.RIL.sendWorkerMessage("iccIO", options, null);
    }.bind(this);
    this.getResponse(options);
  },

  /**
   * Use ICC_COMMAND_GET_RESPONSE to query the EF.
   *
   * @param fileId
   *        The file to operate on, one of the ICC_EF_* constants.
   */
  getResponse(options) {
    options.command = ICC_COMMAND_GET_RESPONSE;
    //
    options.pathId =
      options.pathId || this.context.ICCFileHelper.getEFPath(options.fileId);
    if (!options.pathId) {
      throw new Error("Unknown pathId for " + options.fileId.toString(16));
    }
    options.p1 = 0; // For GET_RESPONSE, p1 = 0
    options.p2 = 0x00;
    options.p3 = GET_RESPONSE_EF_SIZE_BYTES;
    this.context.RIL.sendWorkerMessage("iccIO", options, null);
  },

  /**
   * Process ICC I/O response.
   */
  processICCIO(options) {
    let func = this[options.command];
    func.call(this, options);
  },

  /**
   * Process a ICC_COMMAND_GET_RESPONSE type command for REQUEST_SIM_IO.
   */
  processICCIOGetResponse(options) {
    let strLen = options.simResponse.length;
    // Get the data[0] data[1] for check the peek.
    let peek = this.context.GsmPDUHelper.processHexToInt(
      options.simResponse.slice(0, 4),
      16
    );
    if (peek === BER_FCP_TEMPLATE_TAG) {
      // Cameron mark first. No sim card for test.
      //this.processUSimGetResponse(options, strLen / 2);
    } else {
      this.processSimGetResponse(options);
    }

    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Helper function for processing USIM get response.
   */
  processUSimGetResponse(options, octetLen) {
    let BerTlvHelper = this.context.BerTlvHelper;

    // Mark this first.
    //let berTlv = BerTlvHelper.decode(octetLen);
    // See TS 102 221 Table 11.4 for the content order of getResponse.
    let iter = Iterator(berTlv.value);
    let tlv = BerTlvHelper.searchForNextTag(BER_FCP_FILE_DESCRIPTOR_TAG, iter);
    if (
      !tlv ||
      tlv.value.fileStructure !== UICC_EF_STRUCTURE[options.structure]
    ) {
      throw new Error(
        "Expected EF structure " +
          UICC_EF_STRUCTURE[options.structure] +
          " but read " +
          tlv.value.fileStructure
      );
    }

    if (
      tlv.value.fileStructure ===
        UICC_EF_STRUCTURE[EF_STRUCTURE_LINEAR_FIXED] ||
      tlv.value.fileStructure === UICC_EF_STRUCTURE[EF_STRUCTURE_CYCLIC]
    ) {
      options.recordSize = tlv.value.recordLength;
      options.totalRecords = tlv.value.numOfRecords;
    }

    tlv = BerTlvHelper.searchForNextTag(BER_FCP_FILE_IDENTIFIER_TAG, iter);
    if (!tlv || tlv.value.fileId !== options.fileId) {
      throw new Error(
        "Expected file ID " +
          options.fileId.toString(16) +
          " but read " +
          fileId.toString(16)
      );
    }

    tlv = BerTlvHelper.searchForNextTag(BER_FCP_FILE_SIZE_DATA_TAG, iter);
    if (!tlv) {
      throw new Error("Unexpected file size data");
    }
    options.fileSize = tlv.value.fileSizeData;
  },

  /**
   * Helper function for processing SIM get response.
   */
  processSimGetResponse(options) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    // The format is from TS 51.011, clause 9.2.1

    // Skip RFU, data[0] data[1].
    // File size, data[2], data[3]
    options.fileSize = GsmPDUHelper.processHexToInt(
      options.simResponse.slice(4, 8),
      16
    );

    // 2 bytes File id. data[4], data[5]
    let fileId = GsmPDUHelper.processHexToInt(
      options.simResponse.slice(8, 12),
      16
    );
    if (fileId != options.fileId) {
      if (DEBUG) {
        this.context.debug(
          "Expected file ID " +
            options.fileId.toString(16) +
            " but read " +
            fileId.toString(16)
        );
      }
    }

    // Type of file, data[6]
    let fileType = GsmPDUHelper.processHexToInt(
      options.simResponse.slice(12, 14),
      16
    );
    if (fileType != TYPE_EF) {
      throw new Error("Unexpected file type " + fileType);
    }

    // Skip 1 byte RFU, data[7],
    //      3 bytes Access conditions, data[8] data[9] data[10],
    //      1 byte File status, data[11],
    //      1 byte Length of the following data, data[12].

    // Read Structure of EF, data[13]
    let efStructure = GsmPDUHelper.processHexToInt(
      options.simResponse.slice(26, 28),
      16
    );
    if (efStructure != options.structure) {
      throw new Error(
        "Expected EF structure " +
          options.structure +
          " but read " +
          efStructure
      );
    }

    // Length of a record, data[14].
    // Only available for LINEAR_FIXED and CYCLIC.
    if (
      efStructure == EF_STRUCTURE_LINEAR_FIXED ||
      efStructure == EF_STRUCTURE_CYCLIC
    ) {
      options.recordSize = GsmPDUHelper.processHexToInt(
        options.simResponse.slice(28, 30),
        16
      );
      options.totalRecords = options.fileSize / options.recordSize;
    }
  },

  /**
   * Process a ICC_COMMAND_READ_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOReadRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_READ_BINARY type command for REQUEST_SIM_IO.
   */
  processICCIOReadBinary(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Process a ICC_COMMAND_UPDATE_RECORD type command for REQUEST_SIM_IO.
   */
  processICCIOUpdateRecord(options) {
    if (options.callback) {
      options.callback(options);
    }
  },

  /**
   * Helper function for HEX to INT.
   */
  /*processHexToInt: function(x, base) {
    const parsed = parseInt(x, base);
    if (isNaN(parsed)) { return 0 }
    return parsed;
  },*/
};
ICCIOHelperObject.prototype[ICC_COMMAND_SEEK] = null;
ICCIOHelperObject.prototype[
  ICC_COMMAND_READ_BINARY
] = function ICC_COMMAND_READ_BINARY(options) {
  this.processICCIOReadBinary(options);
};
ICCIOHelperObject.prototype[
  ICC_COMMAND_READ_RECORD
] = function ICC_COMMAND_READ_RECORD(options) {
  this.processICCIOReadRecord(options);
};
ICCIOHelperObject.prototype[
  ICC_COMMAND_GET_RESPONSE
] = function ICC_COMMAND_GET_RESPONSE(options) {
  this.processICCIOGetResponse(options);
};
ICCIOHelperObject.prototype[ICC_COMMAND_UPDATE_BINARY] = null;
ICCIOHelperObject.prototype[
  ICC_COMMAND_UPDATE_RECORD
] = function ICC_COMMAND_UPDATE_RECORD(options) {
  this.processICCIOUpdateRecord(options);
};

/**
 * ICC Helper for getting EF path.
 */
function ICCFileHelperObject(aContext) {
  this.context = aContext;
}
ICCFileHelperObject.prototype = {
  context: null,

  /**
   * This function handles only EFs that are common to RUIM, SIM, USIM
   * and other types of ICC cards.
   */
  getCommonEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_ICCID:
        return EF_PATH_MF_SIM;
      case ICC_EF_ADN:
      case ICC_EF_SDN: // Fall through.
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_PBR:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
      case ICC_EF_IMG:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_GRAPHICS;
    }
    return null;
  },

  /**
   * This function handles EFs for SIM.
   */
  getSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_FDN:
      case ICC_EF_MSISDN:
      case ICC_EF_SMS:
      case ICC_EF_EXT1:
      case ICC_EF_EXT2:
      case ICC_EF_EXT3:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      case ICC_EF_AD:
      case ICC_EF_MBDN:
      case ICC_EF_MWIS:
      case ICC_EF_CFIS:
      case ICC_EF_PLMNsel:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_SST:
      case ICC_EF_PHASE:
      case ICC_EF_CBMI:
      case ICC_EF_CBMID:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
      case ICC_EF_GID1:
      case ICC_EF_GID2:
      case ICC_EF_CPHS_CFF:
      case ICC_EF_CPHS_INFO:
      case ICC_EF_CPHS_MBN:
      case ICC_EF_CPHS_ONS:
      case ICC_EF_CPHS_ONSF:
        return EF_PATH_MF_SIM + EF_PATH_DF_GSM;
      default:
        return null;
    }
  },

  /**
   * This function handles EFs for USIM.
   */
  getUSimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_AD:
      case ICC_EF_FDN:
      case ICC_EF_MBDN:
      case ICC_EF_MWIS:
      case ICC_EF_CFIS:
      case ICC_EF_UST:
      case ICC_EF_MSISDN:
      case ICC_EF_SPN:
      case ICC_EF_SPDI:
      case ICC_EF_CBMI:
      case ICC_EF_CBMID:
      case ICC_EF_CBMIR:
      case ICC_EF_OPL:
      case ICC_EF_PNN:
      case ICC_EF_SMS:
      case ICC_EF_GID1:
      case ICC_EF_GID2:
      // CPHS spec was provided in 1997 based on SIM requirement, there is no
      // detailed info about how these ICC_EF_CPHS_XXX are allocated in USIM.
      // What we can do now is to follow what has been done in AOSP to have file
      // path equal to MF_SIM/DF_GSM.
      case ICC_EF_CPHS_CFF:
      case ICC_EF_CPHS_INFO:
      case ICC_EF_CPHS_MBN:
      case ICC_EF_CPHS_ONS:
      case ICC_EF_CPHS_ONSF:
        return EF_PATH_MF_SIM + EF_PATH_ADF_USIM;
      default:
        // The file ids in USIM phone book entries are decided by the
        // card manufacturer. So if we don't match any of the cases
        // above and if its a USIM return the phone book path.
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM + EF_PATH_DF_PHONEBOOK;
    }
  },

  /**
   * This function handles EFs for RUIM
   */
  getRuimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_CSIM_IMSI_M:
      case ICC_EF_CSIM_CDMAHOME:
      case ICC_EF_CSIM_CST:
      case ICC_EF_CSIM_SPN:
        return EF_PATH_MF_SIM + EF_PATH_DF_CDMA;
      case ICC_EF_FDN:
      case ICC_EF_EXT1:
      case ICC_EF_EXT2:
      case ICC_EF_EXT3:
        return EF_PATH_MF_SIM + EF_PATH_DF_TELECOM;
      default:
        return null;
    }
  },

  /*
   * This function handles EFs for ISIM
   */
  getIsimEFPath(fileId) {
    switch (fileId) {
      case ICC_EF_ISIM_IMPI:
      case ICC_EF_ISIM_DOMAIN:
      case ICC_EF_ISIM_IMPU:
      case ICC_EF_ISIM_IST:
      case ICC_EF_ISIM_PCSCF:
        return EF_PATH_MF_SIM + EF_PATH_ADF_ISIM;
      default:
        return null;
    }
  },

  /**
   * Helper function for getting the pathId for the specific ICC record
   * depeding on which type of ICC card we are using.
   *
   * @param fileId
   *        File id.
   * @return The pathId or null in case of an error or invalid input.
   */
  getEFPath(fileId) {
    let path = this.getCommonEFPath(fileId);
    if (path) {
      return path;
    }

    switch (this.context.RIL.appType) {
      case CARD_APPTYPE_SIM:
        return this.getSimEFPath(fileId);
      case CARD_APPTYPE_USIM:
        return this.getUSimEFPath(fileId);
      case CARD_APPTYPE_RUIM:
        return this.getRuimEFPath(fileId);
      default:
        return null;
    }
  },
};

/**
 * Helper functions for ICC utilities.
 */
function ICCUtilsHelperObject(aContext) {
  this.context = aContext;
}
ICCUtilsHelperObject.prototype = {
  context: null,

  /**
   * Get network names by using EF_OPL and EF_PNN
   *
   * @See 3GPP TS 31.102 sec. 4.2.58 and sec. 4.2.59 for USIM,
   *      3GPP TS 51.011 sec. 10.3.41 and sec. 10.3.42 for SIM.
   *
   * @param mcc   The mobile country code of the network.
   * @param mnc   The mobile network code of the network.
   * @param lac   The location area code of the network.
   */
  getNetworkNameFromICC(mcc, mnc, lac) {
    let RIL = this.context.RIL;
    let iccInfoPriv = RIL.iccInfoPrivate;
    let iccInfo = RIL.iccInfo;
    let pnnEntry;

    if (!mcc || !mnc || lac == null || lac < 0) {
      return null;
    }

    // We won't get network name if there is no PNN file.
    if (!iccInfoPriv.PNN) {
      return null;
    }

    if (!this.isICCServiceAvailable("OPL")) {
      // When OPL is not present:
      // According to 3GPP TS 31.102 Sec. 4.2.58 and 3GPP TS 51.011 Sec. 10.3.41,
      // If EF_OPL is not present, the first record in this EF is used for the
      // default network name when registered to the HPLMN.
      // If we haven't get pnnEntry assigned, we should try to assign default
      // value to it.
      if (mcc == iccInfo.mcc && mnc == iccInfo.mnc) {
        pnnEntry = iccInfoPriv.PNN[0];
      }
    } else {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      let wildChar = GsmPDUHelper.extendedBcdChars.charAt(0x0d);
      // According to 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
      // the ME shall use this EF_OPL in association with the EF_PNN in place
      // of any network name stored within the ME's internal list and any network
      // name received when registered to the PLMN.
      let length = iccInfoPriv.OPL ? iccInfoPriv.OPL.length : 0;
      for (let i = 0; i < length; i++) {
        let unmatch = false;
        let opl = iccInfoPriv.OPL[i];
        // Try to match the MCC/MNC. Besides, A BCD value of 'D' in any of the
        // MCC and/or MNC digits shall be used to indicate a "wild" value for
        // that corresponding MCC/MNC digit.
        if (opl.mcc.indexOf(wildChar) !== -1) {
          for (let j = 0; j < opl.mcc.length; j++) {
            if (opl.mcc[j] !== wildChar && opl.mcc[j] !== mcc[j]) {
              unmatch = true;
              break;
            }
          }
          if (unmatch) {
            continue;
          }
        } else if (mcc !== opl.mcc) {
          continue;
        }

        if (mnc.length !== opl.mnc.length) {
          continue;
        }

        if (opl.mnc.indexOf(wildChar) !== -1) {
          for (let j = 0; j < opl.mnc.length; j++) {
            if (opl.mnc[j] !== wildChar && opl.mnc[j] !== mnc[j]) {
              unmatch = true;
              break;
            }
          }
          if (unmatch) {
            continue;
          }
        } else if (mnc !== opl.mnc) {
          continue;
        }

        // Try to match the location area code. If current local area code is
        // covered by lac range that specified in the OPL entry, use the PNN
        // that specified in the OPL entry.
        if (
          (opl.lacTacStart === 0x0 && opl.lacTacEnd == 0xfffe) ||
          (opl.lacTacStart <= lac && opl.lacTacEnd >= lac)
        ) {
          if (opl.pnnRecordId === 0) {
            // See 3GPP TS 31.102 Sec. 4.2.59 and 3GPP TS 51.011 Sec. 10.3.42,
            // A value of '00' indicates that the name is to be taken from other
            // sources.
            return null;
          }
          pnnEntry = iccInfoPriv.PNN[opl.pnnRecordId - 1];
          break;
        }
      }
    }

    if (!pnnEntry) {
      return null;
    }

    // Return a new object to avoid global variable, PNN, be modified by accident.
    return {
      fullName: pnnEntry.fullName || "",
      shortName: pnnEntry.shortName || "",
    };
  },

  /**
   * This will compute the spnDisplay field of the network.
   * See TS 22.101 Annex A and TS 51.011 10.3.11 for details.
   *
   * @return True if some of iccInfo is changed in by this function.
   */
  updateDisplayCondition() {
    let RIL = this.context.RIL;

    // If EFspn isn't existed in SIM or it haven't been read yet, we should
    // just set isDisplayNetworkNameRequired = true and
    // isDisplaySpnRequired = false
    let iccInfo = RIL.iccInfo;
    let iccInfoPriv = RIL.iccInfoPrivate;
    let displayCondition = iccInfoPriv.spnDisplayCondition;
    let origIsDisplayNetworkNameRequired = iccInfo.isDisplayNetworkNameRequired;
    let origIsDisplaySPNRequired = iccInfo.isDisplaySpnRequired;

    if (displayCondition === undefined) {
      iccInfo.isDisplayNetworkNameRequired = true;
      iccInfo.isDisplaySpnRequired = false;
    } else if (RIL._isCdma) {
      // CDMA family display rule.
      let cdmaHome = RIL.cdmaHome;
      let cell = RIL.voiceRegistrationState.cell;
      let sid = cell && cell.cdmaSystemId;
      let nid = cell && cell.cdmaNetworkId;

      iccInfo.isDisplayNetworkNameRequired = false;

      // If display condition is 0x0, we don't even need to check network id
      // or system id.
      if (displayCondition === 0x0) {
        iccInfo.isDisplaySpnRequired = false;
      } else {
        // CDMA SPN Display condition dosen't specify whenever network name is
        // reqired.
        if (
          !cdmaHome ||
          !cdmaHome.systemId ||
          cdmaHome.systemId.length === 0 ||
          cdmaHome.systemId.length != cdmaHome.networkId.length ||
          !sid ||
          !nid
        ) {
          // CDMA Home haven't been ready, or we haven't got the system id and
          // network id of the network we register to, assuming we are in home
          // network.
          iccInfo.isDisplaySpnRequired = true;
        } else {
          // Determine if we are registered in the home service area.
          // System ID and Network ID are described in 3GPP2 C.S0005 Sec. 2.6.5.2.
          let inHomeArea = false;
          for (let i = 0; i < cdmaHome.systemId.length; i++) {
            let homeSid = cdmaHome.systemId[i],
              homeNid = cdmaHome.networkId[i];
            if (
              homeSid === 0 ||
              homeNid === 0 || // Reserved system id/network id
              homeSid != sid
            ) {
              continue;
            }
            // According to 3GPP2 C.S0005 Sec. 2.6.5.2, NID number 65535 means
            // all networks in the system should be considered as home.
            if (homeNid == 65535 || homeNid == nid) {
              inHomeArea = true;
              break;
            }
          }
          iccInfo.isDisplaySpnRequired = inHomeArea;
        }
      }
    } else {
      // GSM family display rule.
      let operatorMnc = RIL.operator ? RIL.operator.mnc : -1;
      let operatorMcc = RIL.operator ? RIL.operator.mcc : -1;

      // First detect if we are on HPLMN or one of the PLMN
      // specified by the SIM card.
      let isOnMatchingPlmn = false;

      // If the current network is the one defined as mcc/mnc
      // in SIM card, it's okay.
      if (iccInfo.mcc == operatorMcc && iccInfo.mnc == operatorMnc) {
        isOnMatchingPlmn = true;
      }

      // Test to see if operator's mcc/mnc match mcc/mnc of PLMN.
      if (!isOnMatchingPlmn && iccInfoPriv.SPDI) {
        let iccSpdi = iccInfoPriv.SPDI; // PLMN list
        for (let plmn in iccSpdi) {
          let plmnMcc = iccSpdi[plmn].mcc;
          let plmnMnc = iccSpdi[plmn].mnc;
          isOnMatchingPlmn = plmnMcc == operatorMcc && plmnMnc == operatorMnc;
          if (isOnMatchingPlmn) {
            break;
          }
        }
      }

      // See 3GPP TS 22.101 A.4 Service Provider Name indication, and TS 31.102
      // clause 4.2.12 EF_SPN for detail.
      if (isOnMatchingPlmn) {
        // The first bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) {
          this.context.debug("PLMN is HPLMN or PLMN " + "is in PLMN list");
        }

        // TS 31.102 Sec. 4.2.66 and TS 51.011 Sec. 10.3.50
        // EF_SPDI contains a list of PLMNs in which the Service Provider Name
        // shall be displayed.
        iccInfo.isDisplaySpnRequired = true;
        iccInfo.isDisplayNetworkNameRequired = (displayCondition & 0x01) !== 0;
      } else {
        // The second bit of display condition tells us if we should display
        // registered PLMN.
        if (DEBUG) {
          this.context.debug("PLMN isn't HPLMN and PLMN isn't in PLMN list");
        }

        iccInfo.isDisplayNetworkNameRequired = true;
        iccInfo.isDisplaySpnRequired = (displayCondition & 0x02) === 0;
      }
    }

    if (DEBUG) {
      this.context.debug(
        "isDisplayNetworkNameRequired = " + iccInfo.isDisplayNetworkNameRequired
      );
      this.context.debug(
        "isDisplaySpnRequired = " + iccInfo.isDisplaySpnRequired
      );
    }

    return (
      origIsDisplayNetworkNameRequired !==
        iccInfo.isDisplayNetworkNameRequired ||
      origIsDisplaySPNRequired !== iccInfo.isDisplaySpnRequired
    );
  },

  decodeSimTlvs(tlvsLen) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let index = 0;
    let tlvs = [];
    while (index < tlvsLen) {
      let simTlv = {
        tag: GsmPDUHelper.readHexOctet(),
        length: GsmPDUHelper.readHexOctet(),
      };
      simTlv.value = GsmPDUHelper.readHexOctetArray(simTlv.length);
      tlvs.push(simTlv);
      index += simTlv.length + 2; // The length of 'tag' and 'length' field.
    }
    return tlvs;
  },

  /**
   * Parse those TLVs and convert it to an object.
   */
  parsePbrTlvs(pbrTlvs) {
    let pbr = {};
    for (let i = 0; i < pbrTlvs.length; i++) {
      let pbrTlv = pbrTlvs[i];
      let anrIndex = 0;
      for (let j = 0; j < pbrTlv.value.length; j++) {
        let tlv = pbrTlv.value[j];
        let tagName = USIM_TAG_NAME[tlv.tag];
        // ANR could have multiple files. We save it as anr0, anr1,...etc.
        if (tlv.tag == ICC_USIM_EFANR_TAG) {
          tagName += anrIndex;
          anrIndex++;
        }
        pbr[tagName] = tlv;
        pbr[tagName].fileType = pbrTlv.tag;
        pbr[tagName].fileId = (tlv.value[0] << 8) | tlv.value[1];
        pbr[tagName].sfi = tlv.value[2];

        // For Type 2, the order of files is in the same order in IAP.
        if (pbrTlv.tag == ICC_USIM_TYPE2_TAG) {
          pbr[tagName].indexInIAP = j;
        }
      }
    }

    return pbr;
  },

  /**
   * Update the ICC information to RadioInterfaceLayer.
   */
  handleICCInfoChange() {
    let RIL = this.context.RIL;
    RIL.iccInfo.rilMessageType = "iccinfochange";
    RIL.handleUnsolicitedMessage(RIL.iccInfo);
    //RIL.sendChromeMessage(RIL.iccInfo);
  },

  /**
   * Update the ISIM information to RadioInterfaceLayer.
   */
  handleISIMInfoChange(options) {
    options.rilMessageType = "isiminfochange";
    let RIL = this.context.RIL;
    RIL.handleUnsolicitedMessage(options);
    //RIL.sendChromeMessage(options);
  },

  /**
   * Get whether specificed (U)SIM service is available.
   *
   * @param geckoService
   *        Service name like "ADN", "BDN", etc.
   *
   * @return true if the service is enabled, false otherwise.
   */
  isICCServiceAvailable(geckoService) {
    let RIL = this.context.RIL;
    let serviceTable = RIL._isCdma
      ? RIL.iccInfoPrivate.cst
      : RIL.iccInfoPrivate.sst;
    let index, bitmask;
    if (RIL.appType == CARD_APPTYPE_SIM || RIL.appType == CARD_APPTYPE_RUIM) {
      /**
       * Service id is valid in 1..N, and 2 bits are used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not allocated.
       *      1, service allocated.
       * b2 = 0, service not activated.
       *      1, service activated.
       *
       * @see 3GPP TS 51.011 10.3.7.
       */
      let simService;
      if (RIL.appType == CARD_APPTYPE_SIM) {
        simService = GECKO_ICC_SERVICES.sim[geckoService];
      } else {
        simService = GECKO_ICC_SERVICES.ruim[geckoService];
      }
      if (!simService) {
        return false;
      }
      simService -= 1;
      index = Math.floor(simService / 4);
      bitmask = 2 << (simService % 4 << 1);
    } else if (RIL.appType == CARD_APPTYPE_USIM) {
      /**
       * Service id is valid in 1..N, and 1 bit is used to code each service.
       *
       * +----+--  --+----+----+
       * | b8 | ...  | b2 | b1 |
       * +----+--  --+----+----+
       *
       * b1 = 0, service not avaiable.
       *      1, service available.
       *
       * @see 3GPP TS 31.102 4.2.8.
       */
      let usimService = GECKO_ICC_SERVICES.usim[geckoService];
      if (!usimService) {
        return false;
      }
      usimService -= 1;
      index = Math.floor(usimService / 8);
      bitmask = 1 << (usimService % 8 << 0);
    }
    if (!serviceTable) {
      return false;
    }
    return index < serviceTable.length && (serviceTable[index] & bitmask) !== 0;
  },

  /**
   * Get whether specificed CPHS service is available.
   *
   * @param geckoService
   *        Service name like "MDN", etc.
   *
   * @return true if the service is enabled, false otherwise.
   */
  isCphsServiceAvailable(geckoService) {
    let RIL = this.context.RIL;
    let serviceTable = RIL.iccInfoPrivate.cphsSt;

    if (!(serviceTable instanceof Uint8Array)) {
      return false;
    }

    /**
     * Service id is valid in 1..N, and 2 bits are used to code each service.
     *
     * +----+--  --+----+----+
     * | b8 | ...  | b2 | b1 |
     * +----+--  --+----+----+
     *
     * b1 = 0, service not allocated.
     *      1, service allocated.
     * b2 = 0, service not activated.
     *      1, service activated.
     *
     * @See  B.3.1.1 CPHS Information in CPHS Phase 2.
     */
    let cphsService = GECKO_ICC_SERVICES.cphs[geckoService];

    if (!cphsService) {
      return false;
    }
    cphsService -= 1;

    let index = Math.floor(cphsService / 4);
    let bitmask = 2 << (cphsService % 4 << 1);
    return index < serviceTable.length && (serviceTable[index] & bitmask) !== 0;
  },

  /**
   * Check if the string is of GSM default 7-bit coded alphabets with bit 8
   * set to 0.
   *
   * @param str  String to be checked.
   */
  isGsm8BitAlphabet(str) {
    if (!str) {
      return false;
    }

    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable =
      PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for (let i = 0; i < str.length; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);
      if (octet == -1) {
        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          return false;
        }
      }
    }

    return true;
  },

  /**
   * Parse MCC/MNC from IMSI. If there is no available value for the length of
   * mnc, it will use the data in MCC table to parse.
   *
   * @param imsi
   *        The imsi of icc.
   * @param mncLength [optional]
   *        The length of mnc.
   *        Zero indicates we haven't got a valid mnc length.
   *
   * @return An object contains the parsing result of mcc and mnc.
   *         Or null if any error occurred.
   */
  parseMccMncFromImsi(imsi, mncLength) {
    if (!imsi) {
      return null;
    }

    // MCC is the first 3 digits of IMSI.
    let mcc = imsi.substr(0, 3);
    if (!mncLength) {
      // Check the MCC/MNC table for MNC length = 3 first for the case we don't
      // have the 4th byte data from EF_AD.
      if (
        PLMN_HAVING_3DIGITS_MNC[mcc] &&
        PLMN_HAVING_3DIGITS_MNC[mcc].indexOf(imsi.substr(3, 3)) !== -1
      ) {
        mncLength = 3;
      } else {
        // Check the MCC table to decide the length of MNC.
        let index = MCC_TABLE_FOR_MNC_LENGTH_IS_3.indexOf(mcc);
        mncLength = index !== -1 ? 3 : 2;
      }
    }
    let mnc = imsi.substr(3, mncLength);
    if (DEBUG) {
      this.context.debug("IMSI: " + imsi + " MCC: " + mcc + " MNC: " + mnc);
    }

    return { mcc, mnc };
  },
};

/**
 * This object exposes the functionality to parse and serialize PDU strings
 *
 * A PDU is a string containing a series of hexadecimally encoded octets
 * or nibble-swapped binary-coded decimals (BCDs). It contains not only the
 * message text but information about the sender, the SMS service center,
 * timestamp, etc.
 */
function GsmPDUHelperObject(aContext) {
  this.context = aContext;
  this.pdu = "";
  this.pduWriteIndex = 0;
  this.pduReadIndex = 0;
  this.lengthIndex = 0;
}
GsmPDUHelperObject.prototype = {
  context: null,
  pdu: null,
  pduWriteIndex: null,
  pduReadIndex: null,
  lengthIndex: null,

  initWith(data = "") {
    this.pduReadIndex = 0;
    this.pduWriteIndex = 0;
    this.pdu = "";
    if (typeof data === "string") {
      this.pdu = data;
      this.pduWriteIndex = data.length;
    } else {
      for (let i = 0; i < data.length; i++) {
        this.writeHexOctet(data[i]);
      }
    }
  },

  getReadAvailable() {
    return this.pduWriteIndex - this.pduReadIndex;
  },

  seekIncoming(len) {
    if (this.getReadAvailable() >= len) {
      this.pduReadIndex = this.pduReadIndex + len;
    } else {
      throw "Seek PDU out of bounds";
    }
  },

  startCalOutgoingSize() {
    this.lengthIndex = this.pduWriteIndex;
  },

  insert(baseString, insertString, pos) {
    return baseString.slice(0, pos) + insertString + baseString.slice(pos);
  },

  stopCalOutgoingSize() {
    let length = (this.pduWriteIndex - this.lengthIndex) / 2;
    let nibble = length >> 4;
    this.pdu = this.insert(this.pdu, nibble.toString(16), this.lengthIndex);
    this.pduWriteIndex++;
    nibble = length & 0x0f;
    this.pdu = this.insert(this.pdu, nibble.toString(16), this.lengthIndex+1);
    this.pduWriteIndex++;
    this.lengthIndex = 0;
  },

  /**
   * Helper function for HEX to INT.
   */
  processHexToInt(x, base) {
    const parsed = parseInt(x, base);
    if (isNaN(parsed)) {
      return 0;
    }
    return parsed;
  },

  /**
   * Read one character (2 bytes) from a RIL string and decode as hex.
   *
   * @return the nibble as a number.
   */
  readHexNibble() {
    if (this.getReadAvailable() <= 0) {
      throw "Read PDU out of bounds";
    }

    let nibble = this.pdu.charCodeAt(this.pduReadIndex);
    if (nibble >= 48 && nibble <= 57) {
      nibble -= 48; // ASCII '0'..'9'
    } else if (nibble >= 65 && nibble <= 70) {
      nibble -= 55; // ASCII 'A'..'F'
    } else if (nibble >= 97 && nibble <= 102) {
      nibble -= 87; // ASCII 'a'..'f'
    } else {
      throw "Found invalid nibble during PDU parsing: " +
        String.fromCharCode(nibble);
    }
    this.pduReadIndex++;
    return nibble;
  },

  /**
   * Encode a nibble as one hex character in a RIL string (2 bytes).
   *
   * @param nibble
   *        The nibble to encode (represented as a number)
   */
  writeHexNibble(nibble) {
    nibble &= 0x0f;
    this.pdu += nibble.toString(16);
    this.pduWriteIndex++;
  },

  /**
   * Read a hex-encoded octet (two nibbles).
   *
   * @return the octet as a number.
   */
  readHexOctet() {
    return (this.readHexNibble() << 4) | this.readHexNibble();
  },

  /**
   * Write an octet as two hex-encoded nibbles.
   *
   * @param octet
   *        The octet (represented as a number) to encode.
   */
  writeHexOctet(octet) {
    this.writeHexNibble(octet >> 4);
    this.writeHexNibble(octet);
  },

  /**
   * Read an array of hex-encoded octets.
   */
  readHexOctetArray(length) {
    let array = new Uint8Array(length);
    for (let i = 0; i < length; i++) {
      array[i] = this.readHexOctet();
    }
    return array;
  },

  /**
   * Helper to write data into a temporary buffer for easier length encoding when
   * the number of octets for the length encoding is varied.
   *
   * @param writeFunction
   *        Function of how the data to be written into temporary buffer.
   *
   * @return array of written octets.
   **/
  writeWithBuffer(writeFunction) {
    let buf = [];
    let writeHexOctet = this.writeHexOctet;
    this.writeHexOctet = function(octet) {
      buf.push(octet);
    };

    try {
      writeFunction();
    } catch (e) {
      if (DEBUG) {
        debug("Error when writeWithBuffer: " + e);
      }
      buf = [];
    } finally {
      this.writeHexOctet = writeHexOctet;
    }

    return buf;
  },

  /**
   * Convert an octet (number) to a BCD number.
   *
   * Any nibbles that are not in the BCD range count as 0.
   *
   * @param octet
   *        The octet (a number, as returned by getOctet())
   *
   * @return the corresponding BCD number.
   */
  octetToBCD(octet) {
    return (
      ((octet & 0xf0) <= 0x90) * ((octet >> 4) & 0x0f) +
      ((octet & 0x0f) <= 0x09) * (octet & 0x0f) * 10
    );
  },

  /**
   * Convert a BCD number to an octet (number)
   *
   * Only take two digits with absolute value.
   *
   * @param bcd
   *
   * @return the corresponding octet.
   */
  BCDToOctet(bcd) {
    bcd = Math.abs(bcd);
    return (bcd % 10 << 4) + (Math.floor(bcd / 10) % 10);
  },

  /**
   * Convert a semi-octet (number) to a GSM BCD char, or return empty
   * string if invalid semiOctet and suppressException is set to true.
   *
   * @param semiOctet
   *        Nibble to be converted to.
   * @param suppressException [optional]
   *        Suppress exception if invalid semiOctet and suppressException is set
   *        to true.
   *
   * @return GSM BCD char, or empty string.
   */
  bcdChars: "0123456789",
  semiOctetToBcdChar(semiOctet, suppressException) {
    if (semiOctet >= this.bcdChars.length) {
      if (suppressException) {
        return "";
      }
      throw new RangeError();
    }

    let result = this.bcdChars.charAt(semiOctet);
    return result;
  },

  /**
   * Convert a semi-octet (number) to a GSM extended BCD char, or return empty
   * string if invalid semiOctet and suppressException is set to true.
   *
   * @param semiOctet
   *        Nibble to be converted to.
   * @param suppressException [optional]
   *        Suppress exception if invalid semiOctet and suppressException is set
   *        to true.
   *
   * @return GSM extended BCD char, or empty string.
   */
  extendedBcdChars: "0123456789*#,;",
  semiOctetToExtendedBcdChar(semiOctet, suppressException) {
    if (semiOctet >= this.extendedBcdChars.length) {
      if (suppressException) {
        return "";
      }
      throw new RangeError();
    }

    return this.extendedBcdChars.charAt(semiOctet);
  },

  /**
   * Convert string to a GSM extended BCD string
   */
  stringToExtendedBcd(string) {
    return string
      .replace(/[^0-9*#,]/g, "")
      .replace(/\*/g, "a")
      .replace(/\#/g, "b")
      .replace(/\,/g, "c");
  },

  /**
   * Read a *swapped nibble* binary coded decimal (BCD)
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   *
   * @return the decimal as a number.
   */
  readSwappedNibbleBcdNum(pairs) {
    let number = 0;
    for (let i = 0; i < pairs; i++) {
      let octet = this.readHexOctet();
      // Ignore 'ff' octets as they're often used as filler.
      if (octet == 0xff) {
        continue;
      }
      // If the first nibble is an "F" , only the second nibble is to be taken
      // into account.
      if ((octet & 0xf0) == 0xf0) {
        number *= 10;
        number += octet & 0x0f;
        continue;
      }
      number *= 100;
      number += this.octetToBCD(octet);
    }
    return number;
  },

  /**
   * Read a *swapped nibble* binary coded decimal (BCD) string
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   * @param suppressException [optional]
   *        Suppress exception if invalid semiOctet and suppressException is set
   *        to true.
   *
   * @return The BCD string.
   */
  readSwappedNibbleBcdString(pairs, suppressException) {
    let str = "";
    for (let i = 0; i < pairs; i++) {
      let nibbleH = this.readHexNibble();
      let nibbleL = this.readHexNibble();
      if (nibbleL == 0x0f) {
        break;
      }

      str += this.semiOctetToBcdChar(nibbleL, suppressException);
      if (nibbleH != 0x0f) {
        str += this.semiOctetToBcdChar(nibbleH, suppressException);
      }
    }

    return str;
  },

  /**
   * Read a *swapped nibble* extended binary coded decimal (BCD) string
   *
   * @param pairs
   *        Number of nibble *pairs* to read.
   * @param suppressException [optional]
   *        Suppress exception if invalid semiOctet and suppressException is set
   *        to true.
   *
   * @return The BCD string.
   */
  readSwappedNibbleExtendedBcdString(pairs, suppressException) {
    let str = "";
    for (let i = 0; i < pairs; i++) {
      let nibbleH = this.readHexNibble();
      let nibbleL = this.readHexNibble();
      if (nibbleL == 0x0f) {
        break;
      }

      str += this.semiOctetToExtendedBcdChar(nibbleL, suppressException);
      if (nibbleH != 0x0f) {
        str += this.semiOctetToExtendedBcdChar(nibbleH, suppressException);
      }
    }

    return str;
  },

  /**
   * Write numerical data as swapped nibble BCD.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCD(data) {
    data = data.toString();
    if (data.length % 2) {
      data += "F";
    }
    for (let i = 0; i < data.length; i += 2) {
      this.pdu += data.charAt(i + 1);
      this.pduWriteIndex++;
      this.pdu += data.charAt(i);
      this.pduWriteIndex++;
    }
  },

  /**
   * Write numerical data as swapped nibble BCD.
   * If the number of digit of data is even, add '0' at the beginning.
   *
   * @param data
   *        Data to write (as a string or a number)
   */
  writeSwappedNibbleBCDNum(data) {
    data = data.toString();
    if (data.length % 2) {
      data = "0" + data;
    }
    for (let i = 0; i < data.length; i += 2) {
      this.pdu += data.charAt(i + 1);
      this.pduWriteIndex++;
      this.pdu += data.charAt(i);
      this.pduWriteIndex++;
    }
  },

  /**
   * Read user data, convert to septets, look up relevant characters in a
   * 7-bit alphabet, and construct string.
   *
   * @param length
   *        Number of septets to read (*not* octets)
   * @param paddingBits
   *        Number of padding bits in the first byte of user data.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   *
   * @return a string.
   */
  readSeptetsToString(length, paddingBits, langIndex, langShiftIndex) {
    let ret = "";
    let byteLength = Math.ceil((length * 7 + paddingBits) / 8);

    /**
     * |<-                    last byte in header                    ->|
     * |<-           incompleteBits          ->|<- last header septet->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   1st byte in user data                   ->|
     * |<-               data septet 1               ->|<-paddingBits->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     *
     * |<-                   2nd byte in user data                   ->|
     * |<-                   data spetet 2                   ->|<-ds1->|
     * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
     */
    let data = 0;
    let dataBits = 0;
    if (paddingBits) {
      data = this.readHexOctet() >> paddingBits;
      dataBits = 8 - paddingBits;
      --byteLength;
    }

    let escapeFound = false;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];
    do {
      // Read as much as fits in 32bit word
      let bytesToRead = Math.min(byteLength, dataBits ? 3 : 4);
      for (let i = 0; i < bytesToRead; i++) {
        data |= this.readHexOctet() << dataBits;
        dataBits += 8;
        --byteLength;
      }

      // Consume available full septets
      for (; dataBits >= 7; dataBits -= 7) {
        let septet = data & 0x7f;
        data >>>= 7;

        if (escapeFound) {
          escapeFound = false;
          if (septet == PDU_NL_EXTENDED_ESCAPE) {
            // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
            // receipt of this code, a receiving entity shall display a space
            // until another extensiion table is defined."
            ret += " ";
          } else if (septet == PDU_NL_RESERVED_CONTROL) {
            // According to 3GPP TS 23.038 B.2, "This code represents a control
            // character and therefore must not be used for language specific
            // characters."
            ret += " ";
          } else {
            ret += langShiftTable[septet];
          }
        } else if (septet == PDU_NL_EXTENDED_ESCAPE) {
          escapeFound = true;

          // <escape> is not an effective character
          --length;
        } else {
          ret += langTable[septet];
        }
      }
    } while (byteLength);

    if (ret.length != length) {
      /**
       * If num of effective characters does not equal to the length of read
       * string, cut the tail off. This happens when the last octet of user
       * data has following layout:
       *
       * |<-              penultimate octet in user data               ->|
       * |<-               data septet N               ->|<-   dsN-1   ->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * |<-                  last octet in user data                  ->|
       * |<-                       fill bits                   ->|<-dsN->|
       * +===7===|===6===|===5===|===4===|===3===|===2===|===1===|===0===|
       *
       * The fill bits in the last octet may happen to form a full septet and
       * be appended at the end of result string.
       */
      ret = ret.slice(0, length);
    }
    return ret;
  },

  writeStringAsSeptets(message, paddingBits, langIndex, langShiftIndex) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[langIndex];
    const langShiftTable = PDU_NL_SINGLE_SHIFT_TABLES[langShiftIndex];

    let dataBits = paddingBits;
    let data = 0;
    for (let i = 0; i < message.length; i++) {
      let c = message.charAt(i);
      let septet = langTable.indexOf(c);
      if (septet == PDU_NL_EXTENDED_ESCAPE) {
        continue;
      }

      if (septet >= 0) {
        data |= septet << dataBits;
        dataBits += 7;
      } else {
        septet = langShiftTable.indexOf(c);
        if (septet == -1) {
          throw new Error(
            "'" +
              c +
              "' is not in 7 bit alphabet " +
              langIndex +
              ":" +
              langShiftIndex +
              "!"
          );
        }

        if (septet == PDU_NL_RESERVED_CONTROL) {
          continue;
        }

        data |= PDU_NL_EXTENDED_ESCAPE << dataBits;
        dataBits += 7;
        data |= septet << dataBits;
        dataBits += 7;
      }

      for (; dataBits >= 8; dataBits -= 8) {
        this.writeHexOctet(data & 0xff);
        data >>>= 8;
      }
    }

    if (dataBits !== 0) {
      this.writeHexOctet(data & 0xff);
    }
  },

  writeStringAs8BitUnpacked(text) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable =
      PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    let len = text ? text.length : 0;
    for (let i = 0; i < len; i++) {
      let c = text.charAt(i);
      let octet = langTable.indexOf(c);

      if (octet == -1) {
        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          // Fallback to ASCII space.
          octet = langTable.indexOf(" ");
        } else {
          this.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
        }
      }
      this.writeHexOctet(octet);
    }
  },

  /**
   * Read user data and decode as a UCS2 string.
   *
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   *
   * @return a string.
   */
  readUCS2String(numOctets) {
    let str = "";
    let length = numOctets / 2;
    for (let i = 0; i < length; ++i) {
      let code = (this.readHexOctet() << 8) | this.readHexOctet();
      str += String.fromCharCode(code);
    }

    if (DEBUG) {
      this.context.debug("Read UCS2 string: " + str);
    }

    return str;
  },

  /**
   * Write user data as a UCS2 string.
   *
   * @param message
   *        Message string to encode as UCS2 in hex-encoded octets.
   */
  writeUCS2String(message) {
    for (let i = 0; i < message.length; ++i) {
      let code = message.charCodeAt(i);
      this.writeHexOctet((code >> 8) & 0xff);
      this.writeHexOctet(code & 0xff);
    }
  },

  /**
   * Read 1 + UDHL octets and construct user data header.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.24
   */
  readUserDataHeader(msg) {
    /**
     * A header object with properties contained in received message.
     * The properties set include:
     *
     * length: totoal length of the header, default 0.
     * langIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     * langShiftIndex: used locking shift table index, default
     * PDU_NL_IDENTIFIER_DEFAULT.
     *
     */
    let header = {
      length: 0,
      langIndex: PDU_NL_IDENTIFIER_DEFAULT,
      langShiftIndex: PDU_NL_IDENTIFIER_DEFAULT,
    };

    header.length = this.readHexOctet();
    if (DEBUG) {
      this.context.debug("Read UDH length: " + header.length);
    }

    let dataAvailable = header.length;
    while (dataAvailable >= 2) {
      let id = this.readHexOctet();
      let length = this.readHexOctet();
      if (DEBUG) {
        this.context.debug("Read UDH id: " + id + ", length: " + length);
      }

      dataAvailable -= 2;

      switch (id) {
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT: {
          let ref = this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 3;
          if (max && seq && seq <= max) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_8BIT: {
          let dstp = this.readHexOctet();
          let orip = this.readHexOctet();
          dataAvailable -= 2;
          if (
            dstp < PDU_APA_RESERVED_8BIT_PORTS ||
            orip < PDU_APA_RESERVED_8BIT_PORTS
          ) {
            // 3GPP TS 23.040 clause 9.2.3.24.3: "A receiving entity shall
            // ignore any information element where the value of the
            // Information-Element-Data is Reserved or not supported"
            break;
          }
          header.destinationPort = dstp;
          header.originatorPort = orip;
          break;
        }
        case PDU_IEI_APPLICATION_PORT_ADDRESSING_SCHEME_16BIT: {
          let dstp = (this.readHexOctet() << 8) | this.readHexOctet();
          let orip = (this.readHexOctet() << 8) | this.readHexOctet();
          dataAvailable -= 4;
          if (
            dstp >= PDU_APA_VALID_16BIT_PORTS ||
            orip >= PDU_APA_VALID_16BIT_PORTS
          ) {
            // 3GPP TS 23.040 clause 9.2.3.24.4: "A receiving entity shall
            // ignore any information element where the value of the
            // Information-Element-Data is Reserved or not supported"
            // Bug 1130292, some carriers set originatorPort to reserved port
            // numbers for wap push. We rise this as a warning in debug message
            // instead of ingoring this IEI to allow user to receive Wap Push
            // under these carriers.
            this.context.debug(
              "Warning: Invalid port numbers [dstp, orip]: " +
                JSON.stringify([dstp, orip])
            );
          }
          header.destinationPort = dstp;
          header.originatorPort = orip;
          break;
        }
        case PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT: {
          let ref = (this.readHexOctet() << 8) | this.readHexOctet();
          let max = this.readHexOctet();
          let seq = this.readHexOctet();
          dataAvailable -= 4;
          if (max && seq && seq <= max) {
            header.segmentRef = ref;
            header.segmentMaxSeq = max;
            header.segmentSeq = seq;
          }
          break;
        }
        case PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT:
          let langShiftIndex = this.readHexOctet();
          --dataAvailable;
          if (langShiftIndex < PDU_NL_SINGLE_SHIFT_TABLES.length) {
            header.langShiftIndex = langShiftIndex;
          }
          break;
        case PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT:
          let langIndex = this.readHexOctet();
          --dataAvailable;
          if (langIndex < PDU_NL_LOCKING_SHIFT_TABLES.length) {
            header.langIndex = langIndex;
          }
          break;
        case PDU_IEI_SPECIAL_SMS_MESSAGE_INDICATION:
          let msgInd = this.readHexOctet() & 0xff;
          let msgCount = this.readHexOctet();
          dataAvailable -= 2;

          /*
           * TS 23.040 V6.8.1 Sec 9.2.3.24.2
           * bits 1 0   : basic message indication type
           * bits 4 3 2 : extended message indication type
           * bits 6 5   : Profile id
           * bit  7     : storage type
           */
          let storeType = msgInd & PDU_MWI_STORE_TYPE_BIT;
          let mwi = msg.mwi;
          if (!mwi) {
            mwi = msg.mwi = {};
          }

          if (storeType == PDU_MWI_STORE_TYPE_STORE) {
            // Store message because TP_UDH indicates so, note this may override
            // the setting in DCS, but that is expected
            mwi.discard = false;
          } else if (mwi.discard === undefined) {
            // storeType == PDU_MWI_STORE_TYPE_DISCARD
            // only override mwi.discard here if it hasn't already been set
            mwi.discard = true;
          }

          mwi.msgCount = msgCount & 0xff;
          mwi.active = mwi.msgCount > 0;

          if (DEBUG) {
            this.context.debug(
              "MWI in TP_UDH received: " + JSON.stringify(mwi)
            );
          }

          break;
        default:
          if (DEBUG) {
            this.context.debug(
              "readUserDataHeader: unsupported IEI(" +
                id +
                "), " +
                length +
                " bytes."
            );
          }

          // Read out unsupported data
          if (length) {
            let octets;
            if (DEBUG) {
              octets = new Uint8Array(length);
            }

            for (let i = 0; i < length; i++) {
              let octet = this.readHexOctet();
              if (DEBUG) {
                octets[i] = octet;
              }
            }
            dataAvailable -= length;

            if (DEBUG) {
              this.context.debug("readUserDataHeader: " + Array.slice(octets));
            }
          }
          break;
      }
    }

    if (dataAvailable !== 0) {
      throw new Error("Illegal user data header found!");
    }

    msg.header = header;
  },

  /**
   * Write out user data header.
   *
   * @param options
   *        Options containing information for user data header write-out. The
   *        `userDataHeaderLength` property must be correctly pre-calculated.
   */
  writeUserDataHeader(options) {
    this.writeHexOctet(options.userDataHeaderLength);

    if (options.segmentMaxSeq > 1) {
      if (options.segmentRef16Bit) {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_16BIT);
        this.writeHexOctet(4);
        this.writeHexOctet((options.segmentRef >> 8) & 0xff);
      } else {
        this.writeHexOctet(PDU_IEI_CONCATENATED_SHORT_MESSAGES_8BIT);
        this.writeHexOctet(3);
      }
      this.writeHexOctet(options.segmentRef & 0xff);
      this.writeHexOctet(options.segmentMaxSeq & 0xff);
      this.writeHexOctet(options.segmentSeq & 0xff);
    }

    if (options.dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      if (options.langIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_LOCKING_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langIndex);
      }

      if (options.langShiftIndex != PDU_NL_IDENTIFIER_DEFAULT) {
        this.writeHexOctet(PDU_IEI_NATIONAL_LANGUAGE_SINGLE_SHIFT);
        this.writeHexOctet(1);
        this.writeHexOctet(options.langShiftIndex);
      }
    }
  },

  /**
   * Read SM-TL Address.
   *
   * @param len
   *        Length of useful semi-octets within the Address-Value field. For
   *        example, the lenth of "12345" should be 5, and 4 for "1234".
   *
   * @see 3GPP TS 23.040 9.1.2.5
   */
  readAddress(len) {
    // Address Length
    if (!len || len < 0) {
      if (DEBUG) {
        this.context.debug("PDU error: invalid sender address length: " + len);
      }
      return null;
    }
    if (len % 2 == 1) {
      len += 1;
    }
    if (DEBUG) {
      this.context.debug("PDU: Going to read address: " + len);
    }

    // Type-of-Address
    let toa = this.readHexOctet();
    let addr = "";

    if ((toa & 0xf0) == PDU_TOA_ALPHANUMERIC) {
      addr = this.readSeptetsToString(
        Math.floor((len * 4) / 7),
        0,
        PDU_NL_IDENTIFIER_DEFAULT,
        PDU_NL_IDENTIFIER_DEFAULT
      );
      return addr;
    }

    addr = this.readSwappedNibbleExtendedBcdString(len / 2);
    if (addr.length <= 0) {
      if (DEBUG) {
        this.context.debug("PDU error: no number provided");
      }
      return null;
    }
    if ((toa & 0xf0) == PDU_TOA_INTERNATIONAL) {
      addr = "+" + addr;
    }

    return addr;
  },

  /**
   * Read TP-Protocol-Indicator(TP-PID).
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.9
   */
  readProtocolIndicator(msg) {
    // `The MS shall interpret reserved, obsolete, or unsupported values as the
    // value 00000000 but shall store them exactly as received.`
    msg.pid = this.readHexOctet();

    msg.epid = msg.pid;
    switch (msg.epid & 0xc0) {
      case 0x40:
        // Bit 7..0 = 01xxxxxx
        switch (msg.epid) {
          case PDU_PID_SHORT_MESSAGE_TYPE_0:
          case PDU_PID_ANSI_136_R_DATA:
          case PDU_PID_USIM_DATA_DOWNLOAD:
            return;
        }
        break;
    }

    msg.epid = PDU_PID_DEFAULT;
  },

  /**
   * Read TP-Data-Coding-Scheme(TP-DCS)
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.040 9.2.3.10, 3GPP TS 23.038 4.
   */
  readDataCodingScheme(msg) {
    let dcs = this.readHexOctet();
    if (DEBUG) {
      this.context.debug("PDU: read SMS dcs: " + dcs);
    }

    // No message class by default.
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;
    // 7 bit is the default fallback encoding.
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x40: // bits 7..4 = 01xx
      case 0x50:
      case 0x60:
      case 0x70:
      // Bit 5..0 are coded exactly the same as Group 00xx
      case 0x00: // bits 7..4 = 00xx
      case 0x10:
      case 0x20:
      case 0x30:
        if (dcs & 0x10) {
          messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        }
        switch (dcs & 0x0c) {
          case 0x4:
            encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
            break;
          case 0x8:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            break;
        }
        break;

      case 0xe0: // bits 7..4 = 1110
        encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
      // Bit 3..0 are coded exactly the same as Message Waiting Indication
      // Group 1101.
      // Fall through.
      case 0xc0: // bits 7..4 = 1100
      case 0xd0: // bits 7..4 = 1101
        // Indiciates voicemail indicator set or clear
        let active =
          (dcs & PDU_DCS_MWI_ACTIVE_BITS) == PDU_DCS_MWI_ACTIVE_VALUE;

        // If TP-UDH is present, these values will be overwritten
        switch (dcs & PDU_DCS_MWI_TYPE_BITS) {
          case PDU_DCS_MWI_TYPE_VOICEMAIL:
            let mwi = msg.mwi;
            if (!mwi) {
              mwi = msg.mwi = {};
            }

            mwi.active = active;
            mwi.discard = (dcs & PDU_DCS_CODING_GROUP_BITS) == 0xc0;
            mwi.msgCount = active ? GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN : 0;

            if (DEBUG) {
              this.context.debug(
                "MWI in DCS received for voicemail: " + JSON.stringify(mwi)
              );
            }
            break;
          case PDU_DCS_MWI_TYPE_FAX:
            if (DEBUG) {
              this.context.debug("MWI in DCS received for fax");
            }
            break;
          case PDU_DCS_MWI_TYPE_EMAIL:
            if (DEBUG) {
              this.context.debug("MWI in DCS received for email");
            }
            break;
          default:
            if (DEBUG) {
              this.context.debug('MWI in DCS received for "other"');
            }
            break;
        }
        break;

      case 0xf0: // bits 7..4 = 1111
        if (dcs & 0x04) {
          encoding = PDU_DCS_MSG_CODING_8BITS_ALPHABET;
        }
        messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        break;

      default:
        // Falling back to default encoding.
        break;
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];

    if (DEBUG) {
      this.context.debug("PDU: message encoding is " + encoding + " bit.");
    }
  },

  /**
   * Read GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  readTimestamp() {
    let year = this.readSwappedNibbleBcdNum(1) + PDU_TIMESTAMP_YEAR_OFFSET;
    let month = this.readSwappedNibbleBcdNum(1) - 1;
    let day = this.readSwappedNibbleBcdNum(1);
    let hour = this.readSwappedNibbleBcdNum(1);
    let minute = this.readSwappedNibbleBcdNum(1);
    let second = this.readSwappedNibbleBcdNum(1);
    let timestamp = Date.UTC(year, month, day, hour, minute, second);

    // If the most significant bit of the least significant nibble is 1,
    // the timezone offset is negative (fourth bit from the right => 0x08):
    //   localtime = UTC + tzOffset
    // therefore
    //   UTC = localtime - tzOffset
    let tzOctet = this.readHexOctet();
    let tzOffset = this.octetToBCD(tzOctet & ~0x08) * 15 * 60 * 1000;
    tzOffset = tzOctet & 0x08 ? -tzOffset : tzOffset;
    timestamp -= tzOffset;

    return timestamp;
  },

  /**
   * Write GSM TP-Service-Centre-Time-Stamp(TP-SCTS).
   *
   * @see 3GPP TS 23.040 9.2.3.11
   */
  writeTimestamp(date) {
    this.writeSwappedNibbleBCDNum(
      date.getFullYear() - PDU_TIMESTAMP_YEAR_OFFSET
    );

    // The value returned by getMonth() is an integer between 0 and 11.
    // 0 is corresponds to January, 1 to February, and so on.
    this.writeSwappedNibbleBCDNum(date.getMonth() + 1);
    this.writeSwappedNibbleBCDNum(date.getDate());
    this.writeSwappedNibbleBCDNum(date.getHours());
    this.writeSwappedNibbleBCDNum(date.getMinutes());
    this.writeSwappedNibbleBCDNum(date.getSeconds());

    // the value returned by getTimezoneOffset() is the difference,
    // in minutes, between UTC and local time.
    // For example, if your time zone is UTC+10 (Australian Eastern Standard Time),
    // -600 will be returned.
    // In TS 23.040 9.2.3.11, the Time Zone field of TP-SCTS indicates
    // the different between the local time and GMT.
    // And expressed in quarters of an hours. (so need to divid by 15)
    let zone = date.getTimezoneOffset() / 15;
    let octet = this.BCDToOctet(zone);

    // the bit3 of the Time Zone field represents the algebraic sign.
    // (0: positive, 1: negative).
    // For example, if the time zone is -0800 GMT,
    // 480 will be returned by getTimezoneOffset().
    // In this case, need to mark sign bit as 1. => 0x08
    if (zone > 0) {
      octet = octet | 0x08;
    }
    this.writeHexOctet(octet);
  },

  /**
   * User data can be 7 bit (default alphabet) data, 8 bit data, or 16 bit
   * (UCS2) data.
   *
   * @param msg
   *        message object for output.
   * @param length
   *        length of user data to read in octets.
   */
  readUserData(msg, length) {
    if (DEBUG) {
      this.context.debug("Reading " + length + " bytes of user data.");
    }

    let paddingBits = 0;
    if (msg.udhi) {
      this.readUserDataHeader(msg);

      if (msg.encoding == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
        let headerBits = (msg.header.length + 1) * 8;
        let headerSeptets = Math.ceil(headerBits / 7);

        length -= headerSeptets;
        paddingBits = headerSeptets * 7 - headerBits;
      } else {
        length -= msg.header.length + 1;
      }
    }

    if (DEBUG) {
      this.context.debug(
        "After header, " + length + " septets left of user data"
      );
    }

    msg.body = null;
    msg.data = null;

    if (length <= 0) {
      // No data to read.
      return;
    }

    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        // 7 bit encoding allows 140 octets, which means 160 characters
        // ((140x8) / 7 = 160 chars)
        if (length > PDU_MAX_USER_DATA_7BIT) {
          if (DEBUG) {
            this.context.debug("PDU error: user data is too long: " + length);
          }
          break;
        }

        let langIndex = msg.udhi
          ? msg.header.langIndex
          : PDU_NL_IDENTIFIER_DEFAULT;
        let langShiftIndex = msg.udhi
          ? msg.header.langShiftIndex
          : PDU_NL_IDENTIFIER_DEFAULT;
        msg.body = this.readSeptetsToString(
          length,
          paddingBits,
          langIndex,
          langShiftIndex
        );
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = this.readHexOctetArray(length);
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        msg.body = this.readUCS2String(length);
        break;
    }
  },

  /**
   * Read extra parameters if TP-PI is set.
   *
   * @param msg
   *        message object for output.
   */
  readExtraParams(msg) {
    // Because each PDU octet is converted to two UCS2 char2, we should always
    // get even messageStringLength in this#_processReceivedSms(). So, we'll
    // always need two delimitors at the end.

    if (this.getReadAvailable() <= 4) {
      return;
    }

    // TP-Parameter-Indicator
    let pi;
    do {
      // `The most significant bit in octet 1 and any other TP-PI octets which
      // may be added later is reserved as an extension bit which when set to a
      // 1 shall indicate that another TP-PI octet follows immediately
      // afterwards.` ~ 3GPP TS 23.040 9.2.3.27
      pi = this.readHexOctet();
    } while (pi & PDU_PI_EXTENSION);

    // `If the TP-UDL bit is set to "1" but the TP-DCS bit is set to "0" then
    // the receiving entity shall for TP-DCS assume a value of 0x00, i.e. the
    // 7bit default alphabet.` ~ 3GPP 23.040 9.2.3.27
    msg.dcs = 0;
    msg.encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;

    // TP-Protocol-Identifier
    if (pi & PDU_PI_PROTOCOL_IDENTIFIER) {
      this.readProtocolIndicator(msg);
    }
    // TP-Data-Coding-Scheme
    if (pi & PDU_PI_DATA_CODING_SCHEME) {
      this.readDataCodingScheme(msg);
    }
    // TP-User-Data-Length
    if (pi & PDU_PI_USER_DATA_LENGTH) {
      let userDataLength = this.readHexOctet();
      this.readUserData(msg, userDataLength);
    }
  },

  /**
   * Read and decode a PDU-encoded message from the stream.
   *
   * TODO: add some basic sanity checks like:
   * - do we have the minimum number of chars available
   */
  readMessage() {
    // An empty message object. This gets filled below and then returned.
    let msg = {
      // D:DELIVER, DR:DELIVER-REPORT, S:SUBMIT, SR:SUBMIT-REPORT,
      // ST:STATUS-REPORT, C:COMMAND
      // M:Mandatory, O:Optional, X:Unavailable
      //                          D  DR S  SR ST C
      SMSC: null, // M  M  M  M  M  M
      mti: null, // M  M  M  M  M  M
      udhi: null, // M  M  O  M  M  M
      sender: null, // M  X  X  X  X  X
      recipient: null, // X  X  M  X  M  M
      pid: null, // M  O  M  O  O  M
      epid: null, // M  O  M  O  O  M
      dcs: null, // M  O  M  O  O  X
      mwi: null, // O  O  O  O  O  O
      replace: false, // O  O  O  O  O  O
      header: null, // M  M  O  M  M  M
      body: null, // M  O  M  O  O  O
      data: null, // M  O  M  O  O  O
      sentTimestamp: null, // M  X  X  X  X  X
      status: null, // X  X  X  X  M  X
      scts: null, // X  X  X  M  M  X
      dt: null, // X  X  X  X  M  X
    };

    // SMSC info
    let smscLength = this.readHexOctet();
    if (smscLength > 0) {
      let smscTypeOfAddress = this.readHexOctet();
      // Subtract the type-of-address octet we just read from the length.
      msg.SMSC = this.readSwappedNibbleExtendedBcdString(smscLength - 1);
      if (smscTypeOfAddress >> 4 == PDU_TOA_INTERNATIONAL >> 4) {
        msg.SMSC = "+" + msg.SMSC;
      }
    }

    // First octet of this SMS-DELIVER or SMS-SUBMIT message
    let firstOctet = this.readHexOctet();
    // Message Type Indicator
    msg.mti = firstOctet & 0x03;
    // User data header indicator
    msg.udhi = firstOctet & PDU_UDHI;

    switch (msg.mti) {
      case PDU_MTI_SMS_RESERVED:
      // `If an MS receives a TPDU with a "Reserved" value in the TP-MTI it
      // shall process the message as if it were an "SMS-DELIVER" but store
      // the message exactly as received.` ~ 3GPP TS 23.040 9.2.3.1
      case PDU_MTI_SMS_DELIVER:
        return this.readDeliverMessage(msg);
      case PDU_MTI_SMS_STATUS_REPORT:
        return this.readStatusReportMessage(msg);
      default:
        return null;
    }
  },

  /**
   * Helper for processing received SMS parcel data.
   *
   * @param length
   *        Length of SMS string in the incoming parcel.
   *
   * @return Message parsed or null for invalid message.
   */
  processReceivedSms(length) {
    if (!length) {
      if (DEBUG) {
        this.context.debug("Received empty SMS!");
      }
      return [null, PDU_FCS_UNSPECIFIED];
    }

    // An SMS is a string, but we won't read it as such, so let's read the
    // string length and then defer to PDU parsing helper.
    let messageStringLength = length;
    if (DEBUG) {
      this.context.debug("Got new SMS, length " + messageStringLength);
    }
    let message = this.readMessage();
    if (DEBUG) {
      this.context.debug("Got new SMS: " + JSON.stringify(message));
    }

    // Determine result
    if (!message) {
      return [null, PDU_FCS_UNSPECIFIED];
    }

    if (message.epid == PDU_PID_SHORT_MESSAGE_TYPE_0) {
      // `A short message type 0 indicates that the ME must acknowledge receipt
      // of the short message but shall discard its contents.` ~ 3GPP TS 23.040
      // 9.2.3.9
      return [null, PDU_FCS_OK];
    }

    if (
      message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]
    ) {
      let RIL = this.context.RIL;
      switch (message.epid) {
        case PDU_PID_ANSI_136_R_DATA:
        case PDU_PID_USIM_DATA_DOWNLOAD:
          let ICCUtilsHelper = this.context.ICCUtilsHelper;
          if (ICCUtilsHelper.isICCServiceAvailable("DATA_DOWNLOAD_SMS_PP")) {
            // `If the service "data download via SMS Point-to-Point" is
            // allocated and activated in the (U)SIM Service Table, ... then the
            // ME shall pass the message transparently to the UICC using the
            // ENVELOPE (SMS-PP DOWNLOAD).` ~ 3GPP TS 31.111 7.1.1.1
            RIL.dataDownloadViaSMSPP(message);

            // `the ME shall not display the message, or alert the user of a
            // short message waiting.` ~ 3GPP TS 31.111 7.1.1.1
            return [null, PDU_FCS_RESERVED];
          }

        // If the service "data download via SMS-PP" is not available in the
        // (U)SIM Service Table, ..., then the ME shall store the message in
        // EFsms in accordance with TS 31.102` ~ 3GPP TS 31.111 7.1.1.1

        // Fall through.
        default:
          RIL.writeSmsToSIM(message);
          break;
      }
    }

    // TODO: Bug 739143: B2G SMS: Support SMS Storage Full event
    if (
      message.messageClass != GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_0] &&
      !true
    ) {
      // `When a mobile terminated message is class 0..., the MS shall display
      // the message immediately and send a ACK to the SC ..., irrespective of
      // whether there is memory available in the (U)SIM or ME.` ~ 3GPP 23.038
      // clause 4.

      if (
        message.messageClass == GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_2]
      ) {
        // `If all the short message storage at the MS is already in use, the
        // MS shall return "memory capacity exceeded".` ~ 3GPP 23.038 clause 4.
        return [null, PDU_FCS_MEMORY_CAPACITY_EXCEEDED];
      }

      return [null, PDU_FCS_UNSPECIFIED];
    }

    return [message, PDU_FCS_OK];
  },

  /**
   * Read and decode a SMS-DELIVER PDU.
   *
   * @param msg
   *        message object for output.
   */
  readDeliverMessage(msg) {
    // - Sender Address info -
    let senderAddressLength = this.readHexOctet();
    msg.sender = this.readAddress(senderAddressLength);
    // - TP-Protocolo-Identifier -
    this.readProtocolIndicator(msg);
    // - TP-Data-Coding-Scheme -
    this.readDataCodingScheme(msg);
    // - TP-Service-Center-Time-Stamp -
    msg.sentTimestamp = this.readTimestamp();
    // - TP-User-Data-Length -
    let userDataLength = this.readHexOctet();

    // - TP-User-Data -
    if (userDataLength > 0) {
      this.readUserData(msg, userDataLength);
    }

    return msg;
  },

  /**
   * Read and decode a SMS-STATUS-REPORT PDU.
   *
   * @param msg
   *        message object for output.
   */
  readStatusReportMessage(msg) {
    // TP-Message-Reference
    msg.messageRef = this.readHexOctet();
    // TP-Recipient-Address
    let recipientAddressLength = this.readHexOctet();
    msg.recipient = this.readAddress(recipientAddressLength);
    // TP-Service-Centre-Time-Stamp
    msg.scts = this.readTimestamp();
    // TP-Discharge-Time
    msg.dt = this.readTimestamp();
    // TP-Status
    msg.status = this.readHexOctet();

    this.readExtraParams(msg);

    return msg;
  },

  /**
   * Serialize a SMS-SUBMIT PDU message and write it to the output stream.
   *
   * This method expects that a data coding scheme has been chosen already
   * and that the length of the user data payload in that encoding is known,
   * too. Both go hand in hand together anyway.
   *
   * @param address
   *        String containing the address (number) of the SMS receiver
   * @param userData
   *        String containing the message to be sent as user data
   * @param dcs
   *        Data coding scheme. One of the PDU_DCS_MSG_CODING_*BITS_ALPHABET
   *        constants.
   * @param userDataHeaderLength
   *        Length of embedded user data header, in bytes. The whole header
   *        size will be userDataHeaderLength + 1; 0 for no header.
   * @param encodedBodyLength
   *        Length of the user data when encoded with the given DCS. For UCS2,
   *        in bytes; for 7-bit, in septets.
   * @param langIndex
   *        Table index used for normal 7-bit encoded character lookup.
   * @param langShiftIndex
   *        Table index used for escaped 7-bit encoded character lookup.
   * @param requestStatusReport
   *        Request status report.
   */
  writeMessage(options) {
    if (DEBUG) {
      this.context.debug("writeMessage: " + JSON.stringify(options));
    }

    let address = options.number;
    let body = options.body;
    let dcs = options.dcs;
    let userDataHeaderLength = options.userDataHeaderLength;
    let encodedBodyLength = options.encodedBodyLength;
    let langIndex = options.langIndex;
    let langShiftIndex = options.langShiftIndex;

    // SMS-SUBMIT Format:
    //
    // PDU Type - 1 octet
    // Message Reference - 1 octet
    // DA - Destination Address - 2 to 12 octets
    // PID - Protocol Identifier - 1 octet
    // DCS - Data Coding Scheme - 1 octet
    // VP - Validity Period - 0, 1 or 7 octets
    // UDL - User Data Length - 1 octet
    // UD - User Data - 140 octets

    let addressFormat = PDU_TOA_ISDN; // 81
    if (address[0] == "+") {
      addressFormat = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      address = address.substring(1);
    }
    //TODO validity is unsupported for now
    let validity = 0;

    let headerOctets = userDataHeaderLength ? userDataHeaderLength + 1 : 0;
    let paddingBits;
    let userDataLengthInSeptets;
    let userDataLengthInOctets;
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      let headerSeptets = Math.ceil((headerOctets * 8) / 7);
      userDataLengthInSeptets = headerSeptets + encodedBodyLength;
      userDataLengthInOctets = Math.ceil((userDataLengthInSeptets * 7) / 8);
      paddingBits = headerSeptets * 7 - headerOctets * 8;
    } else {
      userDataLengthInOctets = headerOctets + encodedBodyLength;
      paddingBits = 0;
    }

    let pduOctetLength =
      4 + // PDU Type, Message Ref, address length + format
      Math.ceil(address.length / 2) +
      3 + // PID, DCS, UDL
      userDataLengthInOctets;
    if (validity) {
      //TODO: add more to pduOctetLength
    }

    // - PDU-TYPE-

    // +--------+----------+---------+---------+--------+---------+
    // | RP (1) | UDHI (1) | SRR (1) | VPF (2) | RD (1) | MTI (2) |
    // +--------+----------+---------+---------+--------+---------+
    // RP:    0   Reply path parameter is not set
    //        1   Reply path parameter is set
    // UDHI:  0   The UD Field contains only the short message
    //        1   The beginning of the UD field contains a header in addition
    //            of the short message
    // SRR:   0   A status report is not requested
    //        1   A status report is requested
    // VPF:   bit4  bit3
    //        0     0     VP field is not present
    //        0     1     Reserved
    //        1     0     VP field present an integer represented (relative)
    //        1     1     VP field present a semi-octet represented (absolute)
    // RD:        Instruct the SMSC to accept(0) or reject(1) an SMS-SUBMIT
    //            for a short message still held in the SMSC which has the same
    //            MR and DA as a previously submitted short message from the
    //            same OA
    // MTI:   bit1  bit0    Message Type
    //        0     0       SMS-DELIVER (SMSC ==> MS)
    //        0     1       SMS-SUBMIT (MS ==> SMSC)

    // PDU type. MTI is set to SMS-SUBMIT
    let firstOctet = PDU_MTI_SMS_SUBMIT;

    // Status-Report-Request
    if (options.requestStatusReport) {
      firstOctet |= PDU_SRI_SRR;
    }

    // Validity period
    if (validity) {
      //TODO: not supported yet, OR with one of PDU_VPF_*
    }
    // User data header indicator
    if (headerOctets) {
      firstOctet |= PDU_UDHI;
    }
    this.writeHexOctet(firstOctet);

    // Message reference 00
    this.writeHexOctet(0x00);

    // - Destination Address -
    this.writeHexOctet(address.length);
    this.writeHexOctet(addressFormat);
    this.writeSwappedNibbleBCD(address);

    // - Protocol Identifier -
    this.writeHexOctet(0x00);

    // - Data coding scheme -
    // For now it assumes bits 7..4 = 1111 except for the 16 bits use case
    this.writeHexOctet(dcs);

    // - Validity Period -
    if (validity) {
      this.writeHexOctet(validity);
    }

    // - User Data -
    if (dcs == PDU_DCS_MSG_CODING_7BITS_ALPHABET) {
      this.writeHexOctet(userDataLengthInSeptets);
    } else {
      this.writeHexOctet(userDataLengthInOctets);
    }

    if (headerOctets) {
      this.writeUserDataHeader(options);
    }

    switch (dcs) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        this.writeStringAsSeptets(body, paddingBits, langIndex, langShiftIndex);
        break;
      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        // Unsupported.
        break;
      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        this.writeUCS2String(body);
        break;
    }
  },

  /**
   * Read GSM CBS message serial number.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.1
   */
  readCbSerialNumber(msg) {
    msg.serial = (this.readHexOctet() << 8) | this.readHexOctet();
    msg.geographicalScope = (msg.serial >>> 14) & 0x03;
    msg.messageCode = (msg.serial >>> 4) & 0x03ff;
    msg.updateNumber = msg.serial & 0x0f;
  },

  /**
   * Read GSM CBS message message identifier.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.2
   */
  readCbMessageIdentifier(msg) {
    msg.messageId = (this.readHexOctet() << 8) | this.readHexOctet();
    if (DEBUG) {
      this.context.debug("readCbMessageIdentifier messageId: " + msg.messageId);
    }

  },

  /**
   * Read ETWS information from message identifier and serial Number
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.1 & 9.4.1.2.2
   */
  readCbEtwsInfo(msg) {
    if (msg.format != CB_FORMAT_ETWS && this.isEtwsMessage(msg)) {
      // `In the case of transmitting CBS message for ETWS, a part of
      // Message Code can be used to command mobile terminals to activate
      // emergency user alert and message popup in order to alert the users.`
      msg.etws = {
        emergencyUserAlert: !!(msg.messageCode & 0x0200),
        popup: !!(msg.messageCode & 0x0100),
      };

      let warningType = msg.messageId - CB_GSM_MESSAGEID_ETWS_BEGIN;
      if (warningType < CB_ETWS_WARNING_TYPE_NAMES.length) {
        msg.etws.warningType = warningType;
      }
    }
  },

  /**
   * Read CBS Data Coding Scheme.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.038 section 5.
   */
  readCbDataCodingScheme(msg) {
    let dcs = this.readHexOctet();
    if (DEBUG) {
      this.context.debug("PDU: read CBS dcs: " + dcs);
    }

    let language = null,
      hasLanguageIndicator = false;
    // `Any reserved codings shall be assumed to be the GSM 7bit default
    // alphabet.`
    let encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
    let messageClass = PDU_DCS_MSG_CLASS_NORMAL;

    switch (dcs & PDU_DCS_CODING_GROUP_BITS) {
      case 0x00: // 0000
        language = CB_DCS_LANG_GROUP_1[dcs & 0x0f];
        break;

      case 0x10: // 0001
        switch (dcs & 0x0f) {
          case 0x00:
            hasLanguageIndicator = true;
            break;
          case 0x01:
            encoding = PDU_DCS_MSG_CODING_16BITS_ALPHABET;
            hasLanguageIndicator = true;
            break;
        }
        break;

      case 0x20: // 0010
        language = CB_DCS_LANG_GROUP_2[dcs & 0x0f];
        break;

      case 0x40: // 01xx
      case 0x50:
      //case 0x60: Text Compression, not supported
      //case 0x70: Text Compression, not supported
      case 0x90: // 1001
        encoding = dcs & 0x0c;
        if (encoding == 0x0c) {
          encoding = PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        }
        messageClass = dcs & PDU_DCS_MSG_CLASS_BITS;
        break;

      case 0xf0:
        encoding =
          dcs & 0x04
            ? PDU_DCS_MSG_CODING_8BITS_ALPHABET
            : PDU_DCS_MSG_CODING_7BITS_ALPHABET;
        switch (dcs & PDU_DCS_MSG_CLASS_BITS) {
          case 0x01:
            messageClass = PDU_DCS_MSG_CLASS_USER_1;
            break;
          case 0x02:
            messageClass = PDU_DCS_MSG_CLASS_USER_2;
            break;
          case 0x03:
            messageClass = PDU_DCS_MSG_CLASS_3;
            break;
        }
        break;

      case 0x30: // 0011 (Reserved)
      case 0x80: // 1000 (Reserved)
      case 0xa0: // 1010..1100 (Reserved)
      case 0xb0:
      case 0xc0:
        break;

      default:
        throw new Error("Unsupported CBS data coding scheme: " + dcs);
    }

    msg.dcs = dcs;
    msg.encoding = encoding;
    msg.language = language;
    msg.messageClass = GECKO_SMS_MESSAGE_CLASSES[messageClass];
    msg.hasLanguageIndicator = hasLanguageIndicator;
  },

  /**
   * Read GSM CBS message page parameter.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.4.1.2.4
   */
  readCbPageParameter(msg) {
    let octet = this.readHexOctet();
    msg.pageIndex = (octet >>> 4) & 0x0f;
    msg.numPages = octet & 0x0f;
    if (!msg.pageIndex || !msg.numPages) {
      // `If a mobile receives the code 0000 in either the first field or the
      // second field then it shall treat the CBS message exactly the same as a
      // CBS message with page parameter 0001 0001 (i.e. a single page message).`
      msg.pageIndex = msg.numPages = 1;
    }
  },

  /**
   * Read ETWS Primary Notification message warning type.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 section 9.3.24
   */
  readCbWarningType(msg) {
    let word = (this.readHexOctet() << 8) | this.readHexOctet();
    msg.etws = {
      warningType: (word >>> 9) & 0x7f,
      popup: !!(word & 0x80),
      emergencyUserAlert: !!(word & 0x100),
    };
  },

  /**
   * Read GSM CB Data
   *
   * This parameter is a copy of the 'CBS-Message-Information-Page' as sent
   * from the CBC to the BSC.
   *
   * @see  3GPP TS 23.041 section 9.4.1.2.5
   */
  readGsmCbData(msg, length) {
    let bufAdapter = {
      context: this.context,
      readHexOctet() {
        return (this.context.GsmPDUHelper.readHexNibble() << 4) | this.context.GsmPDUHelper.readHexNibble();
      },
    };

    msg.body = null;
    msg.data = null;
    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET:
        msg.body = this.readSeptetsToString.call(
          bufAdapter,
          Math.floor((length * 8) / 7),
          0,
          PDU_NL_IDENTIFIER_DEFAULT,
          PDU_NL_IDENTIFIER_DEFAULT
        );
        if (msg.hasLanguageIndicator) {
          msg.language = msg.body.substring(0, 2);
          msg.body = msg.body.substring(3);
        }
        break;

      case PDU_DCS_MSG_CODING_8BITS_ALPHABET:
        msg.data = this.readHexOctetArray(length);
        break;

      case PDU_DCS_MSG_CODING_16BITS_ALPHABET:
        if (msg.hasLanguageIndicator) {
          msg.language = this.readSeptetsToString.call(
            bufAdapter,
            2,
            0,
            PDU_NL_IDENTIFIER_DEFAULT,
            PDU_NL_IDENTIFIER_DEFAULT
          );
          length -= 2;
        }
        msg.body = this.readUCS2String.call(bufAdapter, length);
        break;
    }

    if (msg.data || !msg.body) {
      return;
    }

    // According to 9.3.19 CBS-Message-Information-Page in TS 23.041:
    // "
    //  This parameter is of a fixed length of 82 octets and carries up to and
    //  including 82 octets of user information. Where the user information is
    //  less than 82 octets, the remaining octets must be filled with padding.
    // "
    // According to 6.2.1.1 GSM 7 bit Default Alphabet and 6.2.3 UCS2 in
    // TS 23.038, the padding character is <CR>.
    for (let i = msg.body.length - 1; i >= 0; i--) {
      if (msg.body.charAt(i) !== "\r") {
        msg.body = msg.body.substring(0, i + 1);
        break;
      }
    }
  },

  /**
   * Read Warning Area Coordinates Data
   *
   */
  readWacData(msg) {
    let waePduLength = this.getReadAvailable(); // nibbles
    let readWACLatLng = () => {
      // lat: 22 bist, lng: 22 bits
      let wacLat = (this.readHexOctet() << 14) | (this.readHexOctet() << 6);
      let thirdByte = this.readHexOctet();
      wacLat = wacLat | (thirdByte >> 2);
      let wacLng =
        ((thirdByte & 0x03) << 20) |
        (this.readHexOctet() << 12) |
        (this.readHexOctet() << 4) |
        this.readHexNibble();

      if (DEBUG) {
        this.context.debug("readWACLatLng wacLng: " + wacLng + " wacLat: " + wacLat);
      }

      // latitude = wacLatitude * 180 / 2^22 - 90
      // longitude = wacLongitude * 360 / 2^22 -180
      return new LatLng(
        (wacLat * 180.0) / (1 << 22) - 90,
        (wacLng * 360.0) / (1 << 22) - 180
      );
    };

    let wacDataLength = this.readHexOctet() | (this.readHexOctet() << 8);
    if (DEBUG) {
      this.context.debug("readWacData wacDataLength: " + wacDataLength);
    }
    if (DEBUG) {
      this.context.debug("readWacData pdu: " + this.pdu);
    }

    if (wacDataLength > this.getReadAvailable() / PDU_HEX_OCTET_SIZE) {
      // Seek back as no validate WAC data
      this.seekIncoming(-2 * PDU_HEX_OCTET_SIZE);
      throw "Invalid wac data";
    } else {
      let maxWaitTimeSec = GEO_FENCING_MAXIMUM_WAIT_TIME_NOT_SET;
      let remain = wacDataLength;
      let geo = [];
      while (remain > 0) {
        // Type: 4bits
        // Length: 10 bits and skip 2 remained bits
        let geoType = this.readHexNibble();
        if (DEBUG) {
          this.context.debug("readWacData geoType: " + geoType);
        }

        let geoLength =
          (this.readHexNibble() << 6) | (this.readHexOctet() >> 2);
        if (DEBUG) {
          this.context.debug("readWacData geoLength: " + geoLength);
        }
        remain = remain - geoLength;
        switch (geoType) {
          case GEO_FENCING_MAXIMUM_WAIT_TIME:
            maxWaitTimeSec = this.readHexOctet();
            if (DEBUG) {
              this.context.debug("maxWaitTimeSec: " + maxWaitTimeSec);
            }
            break;
          case GEO_FENCING_POLYGON:
            let latLngs = [];
            // Each coordinate is represented by 44 bits integer.
            // ATIS-0700041 5.2.4 Coordinate coding
            let n = Math.floor(((geoLength - 2) * 8) / 44);
            if (DEBUG) {
              this.context.debug("readWacData n: " + n);
            }
            for (let i = 0; i < n; i++) {
              latLngs.push(readWACLatLng());
            }
            if (n % 2) {
              //skip padding bits if the remained bits is less than 8.
              //here we have 4 bits remained.
              this.readHexNibble();
            }
            geo.push(new Polygon(latLngs));
            break;
          case GEO_FENCING_CIRCLE:
            let center = readWACLatLng();
            // radius = (wacRadius / 2^6). The unit of wacRadius is km, we use meter as the
            // distance unit during geo-fencing.
            // ATIS-0700041 5.2.5 radius coding
            //radius: 20bits
            let wacRadius =
              (this.readHexOctet() << 12) |
              (this.readHexOctet() << 4) |
              this.readHexNibble();
            let radius = ((wacRadius * 1.0) / (1 << 6)) * 1000.0;
            geo.push(new Circle(center, radius));
            break;
          default:
            throw "Unsupported geoType " + geoType;
            break;
        }
      }
      msg.geometries = geo;
      msg.maximumWaitingTimeSec = maxWaitTimeSec;

      if (DEBUG) {
        this.context.debug("readWacData done msg: " + JSON.stringify(msg));
      }

      // Seek back to beginning of WAC data
      let remainPduLength = this.getReadAvailable();
      this.seekIncoming(-1 * (waePduLength - remainPduLength));
    }
  },

  readGeoFencingTriggerData(msg) {
    //type: 4bits, length 7 bits
    let type = this.readHexNibble();
    let length = this.readHexOctet() >> 1;
    // Skip the remained 4 bits
    this.readHexNibble();

    let messageIdentifierCount = ((length - 2) * 8) / 32;
    var cbIdentifiers = [];
    for (let i = 0; i < messageIdentifierCount; i++) {
      // Both messageIdentifier and serialNumber are 16 bits integers.
      // ATIS-0700041 Section 5.1.6
      let cellBroadcastIdentity = {};
      cellBroadcastIdentity.messageIdentifier =
        (this.readHexOctet() << 8) | this.readHexOctet();
      cellBroadcastIdentity.serialNumber =
        (this.readHexOctet() << 8) | this.readHexOctet();
      cbIdentifiers.push(cellBroadcastIdentity);
    }
    msg.geoFencingTrigger = {type, cbIdentifiers };
  },

  /**
   * Read UMTS CB Data
   *
   * Octet Number(s)  Parameter
   *               1  Number-of-Pages
   *          2 - 83  CBS-Message-Information-Page 1
   *              84  CBS-Message-Information-Length 1
   *             ...
   *                  CBS-Message-Information-Page n
   *                  CBS-Message-Information-Length n
   *
   * @see 3GPP TS 23.041 section 9.4.2.2.5
   */
  readUmtsCbData(msg) {
    if (DEBUG) {
      this.context.debug("readUmtsCbData msg: " + JSON.stringify(msg));
    }
    let numOfPages = this.readHexOctet();
    if (DEBUG) {
      this.context.debug("readUmtsCbData numOfPages: " + numOfPages);
    }
    if (numOfPages < 0 || numOfPages > 15) {
      throw new Error("Invalid numOfPages: " + numOfPages);
    }

    if (this.isCMASGeoFencingTriggerMessage(msg)) {
      this.readGeoFencingTriggerData(msg);
      return;
    }

    let bufAdapter = {
      context: this.context,
      readHexOctet() {
        return (this.context.GsmPDUHelper.readHexNibble() << 4) | this.context.GsmPDUHelper.readHexNibble();
      },
    };

    let removePaddingCharactors = function(text) {
      for (let i = text.length - 1; i >= 0; i--) {
        if (text.charAt(i) !== "\r") {
          return text.substring(0, i + 1);
        }
      }
      return text;
    };

    let totalLength = 0,
      length,
      pageLengths = [];
    for (let i = 0; i < numOfPages; i++) {
      this.seekIncoming(CB_MSG_PAGE_INFO_SIZE * PDU_HEX_OCTET_SIZE);
      length = this.readHexOctet();
      if (DEBUG) {
        this.context.debug("readUmtsCbData page length: " +length);
      }
      totalLength += length;
      pageLengths.push(length);
    }

    // Seek back to beginning of CB Data.
    this.seekIncoming(
      -numOfPages * (CB_MSG_PAGE_INFO_SIZE + 1) * PDU_HEX_OCTET_SIZE
    );

    switch (msg.encoding) {
      case PDU_DCS_MSG_CODING_7BITS_ALPHABET: {
        let body;
        msg.body = "";
        for (let i = 0; i < numOfPages; i++) {
          body = this.readSeptetsToString.call(
            bufAdapter,
            Math.floor((pageLengths[i] * 8) / 7),
            0,
            PDU_NL_IDENTIFIER_DEFAULT,
            PDU_NL_IDENTIFIER_DEFAULT
          );
          if (msg.hasLanguageIndicator) {
            if (!msg.language) {
              msg.language = body.substring(0, 2);
            }
            body = body.substring(3);
          }

          msg.body += removePaddingCharactors(body);

          // Skip padding octets
          this.seekIncoming(
            (CB_MSG_PAGE_INFO_SIZE - pageLengths[i]) * PDU_HEX_OCTET_SIZE
          );
          // Read the octet of CBS-Message-Information-Length
          this.readHexOctet();
        }

        break;
      }

      case PDU_DCS_MSG_CODING_8BITS_ALPHABET: {
        msg.data = new Uint8Array(totalLength);
        for (let i = 0, j = 0; i < numOfPages; i++) {
          for (let pageLength = pageLengths[i]; pageLength > 0; pageLength--) {
            msg.data[j++] = this.readHexOctet();
          }

          // Skip padding octets
          this.seekIncoming(
            (CB_MSG_PAGE_INFO_SIZE - pageLengths[i]) * PDU_HEX_OCTET_SIZE
          );
          // Read the octet of CBS-Message-Information-Length
          this.readHexOctet();
        }

        break;
      }

      case PDU_DCS_MSG_CODING_16BITS_ALPHABET: {
        msg.body = "";
        for (let i = 0; i < numOfPages; i++) {
          let pageLength = pageLengths[i];
          if (msg.hasLanguageIndicator) {
            if (!msg.language) {
              msg.language = this.readSeptetsToString.call(
                bufAdapter,
                2,
                0,
                PDU_NL_IDENTIFIER_DEFAULT,
                PDU_NL_IDENTIFIER_DEFAULT
              );
            } else {
              this.readHexOctet();
              this.readHexOctet();
            }

            pageLength -= 2;
          }

          msg.body += removePaddingCharactors(
            this.readUCS2String.call(bufAdapter, pageLength)
          );

          // Skip padding octets
          this.seekIncoming(
            (CB_MSG_PAGE_INFO_SIZE - pageLengths[i]) * PDU_HEX_OCTET_SIZE
          );
          // Read the octet of CBS-Message-Information-Length
          this.readHexOctet();
        }

        break;
      }
    }

    let waePduLength = this.getReadAvailable(); // nibbles
    if (waePduLength > 0) {
      this.readWacData(msg);
    }
  },

  /**
   * Read Cell GSM/ETWS/UMTS Broadcast Message.
   *
   * @param pduLength
   *        total length of the incoming PDU in octets.
   */
  readCbMessage(pduLength) {
    // Validity                                                   GSM ETWS UMTS
    let msg = {
      // Internally used in ril_worker:
      serial: null, //  O   O    O
      updateNumber: null, //  O   O    O
      format: null, //  O   O    O
      dcs: 0x0f, //  O   X    O
      encoding: PDU_DCS_MSG_CODING_7BITS_ALPHABET, //  O   X    O
      hasLanguageIndicator: false, //  O   X    O
      data: null, //  O   X    O
      body: null, //  O   X    O
      pageIndex: 1, //  O   X    X
      numPages: 1, //  O   X    X

      // DOM attributes:
      geographicalScope: null, //  O   O    O
      messageCode: null, //  O   O    O
      messageId: null, //  O   O    O
      language: null, //  O   X    O
      fullBody: null, //  O   X    O
      fullData: null, //  O   X    O
      messageClass: GECKO_SMS_MESSAGE_CLASSES[PDU_DCS_MSG_CLASS_NORMAL], //  O   x    O
      etws: null, //  ?   O    ?
      /*{
        warningType:        null,                              //  X   O    X
        popup:              false,                             //  X   O    X
        emergencyUserAlert: false,                             //  X   O    X
      }*/
      geometries: null, //  X   X    O
      maximumWaitingTimeSec: null, //  X   X    O
      geoFencingTrigger: null, //  X   X    O
      /*{
        type:               null,                             //  X   X    O
        cbIdentifiers:      null,                             //  X   X    O
        {
        messageIdentifier: null,                             //  X   X    O
        serialNumber:      null,                             //  X   X    O
        }
      }*/
    };

    if (pduLength < CB_MESSAGE_HEADER_SIZE) {
      throw new Error("Invalid PDU Length: " + pduLength);
    }

    if (pduLength <= CB_MESSAGE_SIZE_GSM) {
      this.readCbSerialNumber(msg);
      this.readCbMessageIdentifier(msg);
      if (this.isEtwsMessage(msg) && pduLength <= CB_MESSAGE_SIZE_ETWS) {
        msg.format = CB_FORMAT_ETWS;
        return this.readEtwsCbMessage(msg);
      }
      msg.format = CB_FORMAT_GSM;
      return this.readGsmCbMessage(msg, pduLength);
    }

    if (
      pduLength >= CB_MESSAGE_SIZE_UMTS_MIN &&
      pduLength <= CB_MESSAGE_SIZE_UMTS_MAX
    ) {
      msg.format = CB_FORMAT_UMTS;
      return this.readUmtsCbMessage(msg);
    }

    throw new Error("Invalid PDU Length: " + pduLength);
  },

  isEtwsMessage(msg) {
    return (
      msg.messageId >= CB_GSM_MESSAGEID_ETWS_BEGIN &&
      msg.messageId <= CB_GSM_MESSAGEID_ETWS_END
    );
  },

  isCMASGeoFencingTriggerMessage(msg) {
    return msg.messageId === CB_CMAS_MESSAGEID_GEO_FENCING_TRIGGER;
  },

  /**
   * Read UMTS CBS Message.
   *
   * @param msg
   *        message object for output.
   *
   * Octet Number(s)  Parameter
   *               1  Message Type
   *           2 - 3  Message ID
   *           4 - 5  Serial Number
   *               6  DCS
   *           7 - N  CB Data
   *
   * @see 3GPP TS 23.041 section 9.4.2
   * @see 3GPP TS 25.324 section 10.2
   */
  readUmtsCbMessage(msg) {
    let type = this.readHexOctet();
    if (type != CB_UMTS_MESSAGE_TYPE_CBS) {
      throw new Error("Unsupported UMTS Cell Broadcast message type: " + type);
    }

    this.readCbMessageIdentifier(msg);
    this.readCbSerialNumber(msg);
    this.readCbEtwsInfo(msg);
    this.readCbDataCodingScheme(msg);
    this.readUmtsCbData(msg);

    return msg;
  },

  /**
   * Read GSM Cell Broadcast Message.
   *
   * @param msg
   *        message object for output.
   * @param pduLength
   *        total length of the incomint PDU in octets.
   *
   * Octet Number(s)  Parameter
   *           1 - 2  Message Type
   *           3 - 4  Message ID
   *               5  DCS
   *               6  Page Parameter
   *           7 - 88 CB Data

   * @see 3GPP TS 23.041 clause 9.4.1.2
   */
  readGsmCbMessage(msg, pduLength) {
    this.readCbEtwsInfo(msg);
    this.readCbDataCodingScheme(msg);
    this.readCbPageParameter(msg);

    // GSM CB message header takes 6 octets.
    this.readGsmCbData(msg, pduLength - 6);

    return msg;
  },

  /**
   * Read ETWS Primary Notification Message.
   *
   * @param msg
   *        message object for output.
   *
   * @see 3GPP TS 23.041 clause 9.4.1.3
   */
  readEtwsCbMessage(msg) {
    this.readCbWarningType(msg);

    // Octet 7..56 is Warning Security Information. However, according to
    // section 9.4.1.3.6, `The UE shall ignore this parameter.` So we just skip
    // processing it here.

    return msg;
  },

  /**
   * Read network name.
   *
   * @param len Length of the information element.
   * @return
   *   {
   *     networkName: network name.
   *     shouldIncludeCi: Should Country's initials included in text string.
   *   }
   * @see TS 24.008 clause 10.5.3.5a.
   */
  readNetworkName(value, len) {
    // According to TS 24.008 Sec. 10.5.3.5a, the first octet is:
    // bit 8: must be 1.
    // bit 5-7: Text encoding.
    //          000 - GSM default alphabet.
    //          001 - UCS2 (16 bit).
    //          else - reserved.
    // bit 4: MS should add the letters for Country's Initials and a space
    //        to the text string if this bit is true.
    // bit 1-3: number of spare bits in last octet.
    let GsmPDUHelper = this.context.GsmPDUHelper;
    // one byte for the codingInfo;
    let codingInfo = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
    if (!(codingInfo & 0x80)) {
      return null;
    }

    let textEncoding = (codingInfo & 0x70) >> 4;
    let shouldIncludeCountryInitials = !!(codingInfo & 0x08);
    let spareBits = codingInfo & 0x07;
    let resultString;
    let nameValue = value.slice(2);

    switch (textEncoding) {
      case 0:
        // GSM Default alphabet.
        this.initWith(nameValue);
        resultString = this.readSeptetsToString(
          Math.floor(((len - 1) * 8 - spareBits) / 7),
          0,
          PDU_NL_IDENTIFIER_DEFAULT,
          PDU_NL_IDENTIFIER_DEFAULT
        );
        break;
      case 1:
        // UCS2 encoded.
        resultString = this.context.ICCPDUHelper.readAlphaIdentifier(
          nameValue,
          len - 1
        );
        break;
      default:
        // Not an available text coding.
        return null;
    }

    // TODO - Bug 820286: According to shouldIncludeCountryInitials, add
    // country initials to the resulting string.
    return resultString;
  },
};

/**
 * Helper for ICC records.
 */
function ICCRecordHelperObject(aContext) {
  this.context = aContext;
  // Cache the possible free record id for all files, use fileId as key.
  this._freeRecordIds = {};
}
ICCRecordHelperObject.prototype = {
  context: null,

  /**
   * Fetch ICC records.
   */
  fetchICCRecords() {
    switch (this.context.RIL.appType) {
      case CARD_APPTYPE_SIM:
      case CARD_APPTYPE_USIM:
        this.context.SimRecordHelper.fetchSimRecords();
        break;
      case CARD_APPTYPE_RUIM:
        // Cameron mark first. handle ruim later.
        //this.context.RuimRecordHelper.fetchRuimRecords();
        break;
    }
    this.context.ISimRecordHelper.fetchISimRecords();
  },

  /**
   * Read the ICCID.
   */
  readICCID() {
    function callback(options) {
      let RIL = this.context.RIL;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      let octetLen = options.simResponse.length / PDU_HEX_OCTET_SIZE;
      GsmPDUHelper.initWith(options.simResponse);

      RIL.iccInfo.iccid = GsmPDUHelper.readSwappedNibbleBcdString(
        octetLen,
        true
      );
      if (DEBUG) {
        this.context.debug("ICCID: " + RIL.iccInfo.iccid);
      }
      if (RIL.iccInfo.iccid) {
        this.context.ICCUtilsHelper.handleICCInfoChange();
        this.context.RIL.sendWorkerMessage(
          "reportStkServiceIsRunning",
          null,
          null
        );
      }
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_ICCID,
      callback: callback.bind(this),
    });
  },

  /**
   * Read ICC ADN like EF, i.e. EF_ADN, EF_FDN.
   *
   * @param fileId      EF id of the ADN, FDN or SDN.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readRecordCount(fileId, onsuccess, onerror) {
    let ICCIOHelper = this.context.ICCIOHelper;
    function callback(options) {
      if (onsuccess) {
        onsuccess(options.totalRecords);
      }
    }
    ICCIOHelper.loadLinearFixedEF({
      fileId,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Read ICC ADN like EF, i.e. EF_ADN, EF_FDN.
   *
   * @param fileId      EF id of the ADN, FDN or SDN.
   * @param extFileId   EF id of the EXT.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readADNLike(fileId, extFileId, onsuccess, onerror) {
    let ICCIOHelper = this.context.ICCIOHelper;

    function callback(options) {
      let loadNextContactRecord = () => {
        if (options.p1 < options.totalRecords) {
          ICCIOHelper.loadNextRecord(options);
          return;
        }
        if (DEBUG) {
          for (let i = 0; i < contacts.length; i++) {
            this.context.debug(
              "contact [" + i + "] " + JSON.stringify(contacts[i])
            );
          }
        }
        if (onsuccess) {
          onsuccess(contacts);
        }
      };

      let contact = this.context.ICCPDUHelper.readAlphaIdDiallingNumber(
        options
      );
      if (contact) {
        let record = {
          recordId: options.p1,
          alphaId: contact.alphaId,
          number: contact.number,
        };
        contacts.push(record);

        if (extFileId && contact.extRecordNumber != 0xff) {
          this.readExtension(
            extFileId,
            contact.extRecordNumber,
            number => {
              if (number) {
                record.number += number;
              }
              loadNextContactRecord();
            },
            () => loadNextContactRecord()
          );
          return;
        }
      }
      loadNextContactRecord();
    }

    let contacts = [];
    ICCIOHelper.loadLinearFixedEF({
      fileId,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Update ICC ADN like EFs, like EF_ADN, EF_FDN.
   *
   * @param fileId          EF id of the ADN or FDN.
   * @param extRecordNumber The record identifier of the EXT.
   * @param contact         The contact will be updated. (Shall have recordId property)
   * @param pin2            PIN2 is required when updating ICC_EF_FDN.
   * @param onsuccess       Callback to be called when success.
   * @param onerror         Callback to be called when error.
   */
  updateADNLike(fileId, extRecordNumber, contact, pin2, onsuccess, onerror) {
    let updatedContact;
    function dataWriter(recordSize) {
      updatedContact = this.context.ICCPDUHelper.writeAlphaIdDiallingNumber(
        recordSize,
        contact.alphaId,
        contact.number,
        extRecordNumber
      );
    }

    function callback(options) {
      if (onsuccess) {
        onsuccess(updatedContact);
      }
    }

    if (!contact || !contact.recordId) {
      if (onerror) {
        onerror(GECKO_ERROR_INVALID_ARGUMENTS);
      }
      return;
    }

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber: contact.recordId,
      dataWriter: dataWriter.bind(this),
      pin2,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Read USIM/RUIM Phonebook.
   *
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readPBR(onsuccess, onerror) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let ICCIOHelper = this.context.ICCIOHelper;
    let ICCUtilsHelper = this.context.ICCUtilsHelper;
    let RIL = this.context.RIL;

    function callback(options) {
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE,
        readLen = 0;
      GsmPDUHelper.initWith(value);

      let pbrTlvs = [];
      while (readLen < octetLen) {
        let tag = GsmPDUHelper.readHexOctet();
        if (tag == 0xff) {
          readLen++;
          GsmPDUHelper.seekIncoming((octetLen - readLen) * PDU_HEX_OCTET_SIZE);
          break;
        }

        let tlvLen = GsmPDUHelper.readHexOctet();
        let tlvs = ICCUtilsHelper.decodeSimTlvs(tlvLen);
        pbrTlvs.push({ tag, length: tlvLen, value: tlvs });

        readLen += tlvLen + 2; // +2 for tag and tlvLen
      }

      if (pbrTlvs.length > 0) {
        let pbr = ICCUtilsHelper.parsePbrTlvs(pbrTlvs);
        // EF_ADN is mandatory if and only if DF_PHONEBOOK is present.
        if (!pbr.adn) {
          if (onerror) {
            onerror("Cannot access ADN.");
          }
          return;
        }
        pbrs.push(pbr);
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else if (onsuccess) {
        RIL.iccInfoPrivate.pbrs = pbrs;
        onsuccess(pbrs);
      }
    }

    if (RIL.iccInfoPrivate.pbrs) {
      onsuccess(RIL.iccInfoPrivate.pbrs);
      return;
    }

    let pbrs = [];
    ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_PBR,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Cache EF_IAP record size.
   */
  _iapRecordSize: null,

  /**
   * Read ICC EF_IAP. (Index Administration Phonebook)
   *
   * @see TS 131.102, clause 4.4.2.2
   *
   * @param fileId       EF id of the IAP.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readIAP(fileId, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      GsmPDUHelper.initWith(value);
      this._iapRecordSize = options.recordSize;

      let iap = this.context.GsmPDUHelper.readHexOctetArray(octetLen);
      if (onsuccess) {
        onsuccess(iap);
      }
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      recordSize: this._iapRecordSize,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Update USIM/RUIM Phonebook EF_IAP.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param fileId       EF id of the IAP.
   * @param recordNumber The identifier of the record shall be updated.
   * @param iap          The IAP value to be written.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateIAP(fileId, recordNumber, iap, onsuccess, onerror) {
    let dataWriter = function dataWriter(recordSize) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(iap);
    }.bind(this);

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber,
      dataWriter,
      callback: onsuccess,
      onerror,
    });
  },

  /**
   * Cache EF_Email record size.
   */
  _emailRecordSize: null,

  /**
   * Read USIM/RUIM Phonebook EF_EMAIL.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param fileId       EF id of the EMAIL.
   * @param fileType     The type of the EMAIL, one of the ICC_USIM_TYPE* constants.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readEmail(fileId, fileType, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      let ICCPDUHelper = this.context.ICCPDUHelper;
      let email = null;
      this._emailRecordSize = options.recordSize;

      // Read contact's email
      //
      // | Byte     | Description                 | Length | M/O
      // | 1 ~ X    | E-mail Address              |   X    |  M
      // | X+1      | ADN file SFI                |   1    |  C
      // | X+2      | ADN file Record Identifier  |   1    |  C
      // Note: The fields marked as C above are mandatort if the file
      //       is not type 1 (as specified in EF_PBR)
      if (fileType == ICC_USIM_TYPE1_TAG) {
        email = ICCPDUHelper.read8BitUnpackedToString(value, octetLen);
      } else {
        email = ICCPDUHelper.read8BitUnpackedToString(value, octetLen - 2);
      }

      if (onsuccess) {
        onsuccess(email);
      }
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      recordSize: this._emailRecordSize,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Update USIM/RUIM Phonebook EF_EMAIL.
   *
   * @see TS 131.102, clause 4.4.2.13
   *
   * @param pbr          Phonebook Reference File.
   * @param recordNumber The identifier of the record shall be updated.
   * @param email        The value to be written.
   * @param adnRecordId  The record Id of ADN, only needed if the fileType of Email is TYPE2.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateEmail(pbr, recordNumber, email, adnRecordId, onsuccess, onerror) {
    let fileId = pbr[USIM_PBR_EMAIL].fileId;
    let fileType = pbr[USIM_PBR_EMAIL].fileType;
    let writtenEmail;
    let dataWriter = function dataWriter(recordSize) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();
      let ICCPDUHelper = this.context.ICCPDUHelper;

      if (fileType == ICC_USIM_TYPE1_TAG) {
        writtenEmail = ICCPDUHelper.writeStringTo8BitUnpacked(
          recordSize,
          email
        );
      } else {
        writtenEmail = ICCPDUHelper.writeStringTo8BitUnpacked(
          recordSize - 2,
          email
        );
        GsmPDUHelper.writeHexOctet(pbr.adn.sfi || 0xff);
        GsmPDUHelper.writeHexOctet(adnRecordId);
      }
    }.bind(this);

    let callback = options => {
      if (onsuccess) {
        onsuccess(writtenEmail);
      }
    };

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber,
      dataWriter,
      callback,
      onerror,
    });
  },

  /**
   * Cache EF_ANR record size.
   */
  _anrRecordSize: null,

  /**
   * Read USIM/RUIM Phonebook EF_ANR.
   *
   * @see TS 131.102, clause 4.4.2.9
   *
   * @param fileId       EF id of the ANR.
   * @param fileType     One of the ICC_USIM_TYPE* constants.
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readANR(fileId, fileType, recordNumber, onsuccess, onerror) {
    function callback(options) {
      let value = options.simResponse;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      this._anrRecordSize = options.recordSize;
      GsmPDUHelper.initWith(value);

      // Skip EF_AAS Record ID.
      GsmPDUHelper.seekIncoming(1 * PDU_HEX_OCTET_SIZE);
      let number = null;
      number = this.context.ICCPDUHelper.readNumberWithLength();
      if (onsuccess) {
        onsuccess(number);
      }
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      recordSize: this._anrRecordSize,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Update USIM/RUIM Phonebook EF_ANR.
   *
   * @see TS 131.102, clause 4.4.2.9
   *
   * @param pbr          Phonebook Reference File.
   * @param recordNumber The identifier of the record shall be updated.
   * @param number       The value to be written.
   * @param adnRecordId  The record Id of ADN, only needed if the fileType of Email is TYPE2.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateANR(pbr, recordNumber, number, adnRecordId, onsuccess, onerror) {
    let fileId = pbr[USIM_PBR_ANR0].fileId;
    let fileType = pbr[USIM_PBR_ANR0].fileType;
    let writtenNumber;
    let dataWriter = function dataWriter(recordSize) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();

      // EF_AAS record Id. Unused for now.
      GsmPDUHelper.writeHexOctet(0xff);

      writtenNumber = this.context.ICCPDUHelper.writeNumberWithLength(number);

      // Write unused octets 0xff, CCP and EXT1.
      GsmPDUHelper.writeHexOctet(0xff);
      GsmPDUHelper.writeHexOctet(0xff);

      // For Type 2 there are two extra octets.
      if (fileType == ICC_USIM_TYPE2_TAG) {
        GsmPDUHelper.writeHexOctet(pbr.adn.sfi || 0xff);
        GsmPDUHelper.writeHexOctet(adnRecordId);
      }
    }.bind(this);

    let callback = options => {
      if (onsuccess) {
        onsuccess(writtenNumber);
      }
    };

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber,
      dataWriter,
      callback,
      onerror,
    });
  },

  /**
   * Cache the possible free record id for all files.
   */
  _freeRecordIds: null,

  /**
   * Find free record id.
   *
   * @param fileId      EF id.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  findFreeRecordId(fileId, onsuccess, onerror) {
    let ICCIOHelper = this.context.ICCIOHelper;
    function callback(options) {
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(value);

      let readLen = 0;
      while (readLen < octetLen) {
        let octet = GsmPDUHelper.readHexOctet();
        readLen++;
        if (octet != 0xff) {
          break;
        }
      }

      let nextRecord = (options.p1 % options.totalRecords) + 1;

      if (readLen == octetLen) {
        // Find free record, assume next record is probably free.
        if (DEBUG) {
          this.context.debug(
            "findFreeRecordId free record found: " + options.p1
          );
        }
        this._freeRecordIds[fileId] = nextRecord;
        if (onsuccess) {
          onsuccess(options.p1);
        }
        return;
      }
      GsmPDUHelper.seekIncoming((octetLen - readLen) * PDU_HEX_OCTET_SIZE);

      if (nextRecord !== recordNumber) {
        options.p1 = nextRecord;
        this.context.RIL.sendWorkerMessage("iccIO", options, null);
      } else {
        // No free record found.
        delete this._freeRecordIds[fileId];
        if (DEBUG) {
          this.context.debug(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        }
        onerror(CONTACT_ERR_NO_FREE_RECORD_FOUND);
      }
    }

    // Start searching free records from the possible one.
    let recordNumber = this._freeRecordIds[fileId] || 1;
    ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Read Extension Number from TS 151.011 clause 10.5.10, TS 31.102, clause 4.4.2.4
   *
   * @param fileId        EF Extension id
   * @param recordNumber  The number of the record shall be loaded.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readExtension(fileId, recordNumber, onsuccess, onerror) {
    let callback = options => {
      let value = options.simResponse;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(value);

      let recordType = this.context.GsmPDUHelper.readHexOctet();
      let number = "";

      // TS 31.102, clause 4.4.2.4 EFEXT1
      // Case 1, Extension1 record is additional data
      if (recordType & 0x02) {
        let numLen = this.context.GsmPDUHelper.readHexOctet();
        if (numLen != 0xff) {
          if (numLen > EXT_MAX_BCD_NUMBER_BYTES) {
            if (DEBUG) {
              this.context.debug(
                "Error: invalid length of BCD number/SSC contents - " + numLen
              );
            }
            onerror();
            return;
          }

          number = this.context.GsmPDUHelper.readSwappedNibbleExtendedBcdString(
            numLen
          );
          if (DEBUG) {
            this.context.debug("Contact Extension Number: " + number);
          }
          GsmPDUHelper.seekIncoming(
            (EXT_MAX_BCD_NUMBER_BYTES - numLen) * PDU_HEX_OCTET_SIZE
          );
        } else {
          GsmPDUHelper.seekIncoming(
            EXT_MAX_BCD_NUMBER_BYTES * PDU_HEX_OCTET_SIZE
          );
        }
      } else {
        // Don't support Case 2, Extension1 record is Called Party Subaddress.
        // +1 skip numLen
        //Buf.seekIncoming((EXT_MAX_BCD_NUMBER_BYTES + 1) * Buf.PDU_HEX_OCTET_SIZE);
      }

      onsuccess(number);
    };

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      callback,
      onerror,
    });
  },

  /**
   * Update Extension.
   *
   * @param fileId       EF id of the EXT.
   * @param recordNumber The number of the record shall be updated.
   * @param number       Dialling Number to be written.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  updateExtension(fileId, recordNumber, number, onsuccess, onerror) {
    let dataWriter = recordSize => {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();

      // We don't support extension chain.
      if (number.length > EXT_MAX_NUMBER_DIGITS) {
        number = number.substring(0, EXT_MAX_NUMBER_DIGITS);
      }

      let numLen = Math.ceil(number.length / 2);
      // Write Extension record
      GsmPDUHelper.writeHexOctet(0x02);
      GsmPDUHelper.writeHexOctet(numLen);
      GsmPDUHelper.writeSwappedNibbleBCD(number);
      // Write trailing 0xff of Extension data.
      for (let i = 0; i < EXT_MAX_BCD_NUMBER_BYTES - numLen; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
      // Write trailing 0xff for Identifier.
      GsmPDUHelper.writeHexOctet(0xff);
    };

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber,
      dataWriter,
      callback: onsuccess,
      onerror,
    });
  },

  /**
   * Clean an EF record.
   *
   * @param fileId       EF id.
   * @param recordNumber The number of the record shall be updated.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  cleanEFRecord(fileId, recordNumber, onsuccess, onerror) {
    let dataWriter = recordSize => {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();

      // Write record to 0xff
      for (let i = 0; i < recordSize; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
    };

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId,
      recordNumber,
      dataWriter,
      callback: onsuccess,
      onerror,
    });
  },

  /**
   * Get ADNLike extension record number.
   *
   * @param  fileId       EF id of the ADN or FDN.
   * @param  recordNumber EF record id of the ADN or FDN.
   * @param  onsuccess    Callback to be called when success.
   * @param  onerror      Callback to be called when error.
   */
  getADNLikeExtensionRecordNumber(fileId, recordNumber, onsuccess, onerror) {
    let callback = options => {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(options.simResponse);

      // Skip alphaLen, numLen, BCD Number, CCP octets.
      GsmPDUHelper.seekIncoming((options.recordSize - 1) * PDU_HEX_OCTET_SIZE);

      let extRecordNumber = this.context.GsmPDUHelper.readHexOctet();
      onsuccess(extRecordNumber);
    };

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId,
      recordNumber,
      callback,
      onerror,
    });
  },
};

/**
 * Helper for (U)SIM Records.
 */
function SimRecordHelperObject(aContext) {
  this.context = aContext;
}
SimRecordHelperObject.prototype = {
  context: null,
  // aid for USIM application.
  aid: null,

  setAid(aid) {
    if (DEBUG) {
      this.context.debug("USIM aid : " + aid);
    }
    this.aid = aid;
  },

  /**
   * Fetch (U)SIM records.
   */
  fetchSimRecords() {
    this.context.RIL.sendWorkerMessage(
      "getIMSI",
      {
        aid: this.aid,
      },
      null
    );
    //this.context.RIL.getIMSI();
    this.readAD();

    // Due to CPHS ONS is mandatory in CPHS spec., but no clear definition on
    // how to determine valid or not. Force to read ONS here, but leave ONSF to
    // CphsInfo block.
    this.readCphsONS();

    // CPHS CFF is mandatory.
    // Cameron mark this due to no sim card for test.
    //this.readCphsCFF();

    // CPHS was widely introduced in Europe during GSM(2G) era to provide easier
    // access to carrier's core service like voicemail, call forwarding, manual
    // PLMN selection, and etc.
    // Addition EF like EF_CPHS_MBN, EF_CPHS_ONSF, EF_CPHS_CSP, etc are
    // introduced to support these feature.
    // In USIM, the replancement of these EFs are provided. (EF_MBDN, EF_MWIS, ...)
    // However, some carriers in Europe still rely on these EFs.

    // Cameron no sim card for test readCphsInfo.
    this.readCphsInfo(
      () => this.readSST(),
      aErrorMsg => {
        this.context.debug("Failed to read CPHS_INFO: " + aErrorMsg);
        this.readSST();
      }
    );
  },

  // Sim refresh file updated.
  handleFileUpdate(aEfId) {
    switch (aEfId) {
      case ICC_EF_MSISDN:
        if (DEBUG) this.context.debug("File refresh for EF_MSISDN.");
        if (ICCUtilsHelper.isICCServiceAvailable("MSISDN")) {
          if (DEBUG) {
            this.context.debug("MSISDN: MSISDN is available");
          }
          this.readMSISDN();
        } else if (DEBUG) {
          this.context.debug("MSISDN: MSISDN service is not available");
        }
        break;
      case ICC_EF_AD:
        if (DEBUG) this.context.debug("File refresh for EF_AD.");
        this.readAD();
        break;
      case ICC_EF_MBDN:
        if (DEBUG) this.context.debug("File refresh for EF_MBDN.");
        if (ICCUtilsHelper.isICCServiceAvailable("MDN")) {
          if (DEBUG) {
            this.context.debug("MDN: MDN available.");
          }
          this.readMBDN();
        } else {
          if (DEBUG) {
            this.context.debug("MDN: MDN service is not available");
          }

          if (ICCUtilsHelper.isCphsServiceAvailable("MBN")) {
            // read CPHS_MBN in advance if MBDN is not available.
            this.readCphsMBN();
          } else if (DEBUG) {
            this.context.debug("CPHS_MBN: CPHS_MBN service is not available");
          }
        }
        break;
      case ICC_EF_SPDI:
        if (DEBUG) this.context.debug("File refresh for EF_SPDI.");
        if (ICCUtilsHelper.isICCServiceAvailable("SPDI")) {
          if (DEBUG) {
            this.context.debug("SPDI: SPDI available.");
          }
          this.readSPDI();
        } else if (DEBUG) {
          this.context.debug("SPDI: SPDI service is not available");
        }
        break;
      default:
        // No one knew how to handle this particular file, so to be safe just
        // fetch all records.
        if (DEBUG) this.context.debug("SIM Refresh for all.");
        this.fetchSimRecords();
        break;
    }
  },

  /**
   * Read EF_phase.
   * This EF is only available in SIM.
   */
  readSimPhase() {
    function callback(options) {
      let value = options.simResponse;

      let GsmPDUHelper = this.context.GsmPDUHelper;
      let phase = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
      // If EF_phase is coded '03' or greater, an ME supporting STK shall
      // perform the PROFILE DOWNLOAD procedure.
      if (
        libcutils.property_get("ro.moz.ril.send_stk_profile_dl", "false") &&
        phase >= ICC_PHASE_2_PROFILE_DOWNLOAD_REQUIRED
      ) {
        this.context.RIL.sendStkTerminalProfile(STK_SUPPORTED_TERMINAL_PROFILE);
      }
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_PHASE,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read the MSISDN from the (U)SIM.
   */
  readMSISDN() {
    function callback(options) {
      let RIL = this.context.RIL;
      let contact = this.context.ICCPDUHelper.readAlphaIdDiallingNumber(
        options
      );
      if (
        !contact ||
        (RIL.iccInfo.msisdn !== undefined &&
          RIL.iccInfo.msisdn === contact.number)
      ) {
        return;
      }
      RIL.iccInfo.msisdn = contact.number;
      if (DEBUG) {
        this.context.debug("MSISDN: " + RIL.iccInfo.msisdn);
      }
      this.context.ICCUtilsHelper.handleICCInfoChange();
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_MSISDN,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read the AD (Administrative Data) from the (U)SIM.
   */
  readAD() {
    function callback(options) {
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      this.context.GsmPDUHelper.initWith(value);
      let ad = this.context.GsmPDUHelper.readHexOctetArray(octetLen);

      if (DEBUG) {
        let str = "";
        for (let i = 0; i < ad.length; i++) {
          str += ad[i] + ", ";
        }
        this.context.debug("AD: " + str);
      }

      let ICCUtilsHelper = this.context.ICCUtilsHelper;
      let RIL = this.context.RIL;
      // TS 31.102, clause 4.2.18 EFAD
      let mncLength = 0;
      if (ad && ad[3]) {
        mncLength = ad[3] & 0x0f;
        if (mncLength != 0x02 && mncLength != 0x03) {
          mncLength = 0;
        }
      }
      // The 4th byte of the response is the length of MNC.
      let mccMnc = ICCUtilsHelper.parseMccMncFromImsi(
        RIL.iccInfoPrivate.imsi,
        mncLength
      );
      if (mccMnc) {
        RIL.iccInfo.mcc = mccMnc.mcc;
        RIL.iccInfo.mnc = mccMnc.mnc;
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_AD,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read the SPN (Service Provider Name) from the (U)SIM.
   */
  readSPN() {
    function callback(options) {
      let value = options.simResponse;
      // Each octet is encoded into two chars.
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      let spnDisplayCondition = this.context.GsmPDUHelper.processHexToInt(
        value.slice(0, 2),
        16
      );
      // Minus 1 because the first octet is used to store display condition.
      let spn_value = value.slice(2);
      let spn = this.context.ICCPDUHelper.readAlphaIdentifier(
        spn_value,
        octetLen - 1
      );

      if (DEBUG) {
        this.context.debug(
          "SPN: spn = " + spn + ", spnDisplayCondition = " + spnDisplayCondition
        );
      }

      let RIL = this.context.RIL;
      RIL.iccInfoPrivate.spnDisplayCondition = spnDisplayCondition;
      RIL.iccInfo.spn = spn;
      let ICCUtilsHelper = this.context.ICCUtilsHelper;
      ICCUtilsHelper.updateDisplayCondition();
      ICCUtilsHelper.handleICCInfoChange();
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_SPN,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  readIMG(recordNumber, onsuccess, onerror) {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(value);
      // Each octet is encoded into two chars.
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;

      let numInstances = GsmPDUHelper.readHexOctet();

      // Data length is defined as 9n+1 or 9n+2. See TS 31.102, sub-clause
      // 4.6.1.1. However, it's likely to have padding appended so we have a
      // rather loose check.
      if (octetLen < 9 * numInstances + 1) {
        GsmPDUHelper.seekIncoming((octetLen - 1) * PDU_HEX_OCTET_SIZE);
        if (onerror) {
          onerror();
        }
        return;
      }

      let imgDescriptors = [];
      for (let i = 0; i < numInstances; i++) {
        imgDescriptors[i] = {
          width: GsmPDUHelper.readHexOctet(),
          height: GsmPDUHelper.readHexOctet(),
          codingScheme: GsmPDUHelper.readHexOctet(),
          fileId:
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet(),
          offset:
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet(),
          dataLen:
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet(),
        };
      }
      GsmPDUHelper.seekIncoming(
        (octetLen - 9 * numInstances - 1) * PDU_HEX_OCTET_SIZE
      );

      let instances = [];
      let currentInstance = 0;
      let readNextInstance = function(img) {
        instances[currentInstance] = img;
        currentInstance++;

        if (currentInstance < numInstances) {
          let imgDescriptor = imgDescriptors[currentInstance];
          this.readIIDF(
            imgDescriptor.fileId,
            imgDescriptor.offset,
            imgDescriptor.dataLen,
            imgDescriptor.codingScheme,
            readNextInstance,
            onerror
          );
        } else if (onsuccess) {
          onsuccess(instances);
        }
      }.bind(this);

      this.readIIDF(
        imgDescriptors[0].fileId,
        imgDescriptors[0].offset,
        imgDescriptors[0].dataLen,
        imgDescriptors[0].codingScheme,
        readNextInstance,
        onerror
      );
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_IMG,
      aid: this.aid,
      recordNumber,
      callback: callback.bind(this),
      onerror,
    });
  },

  readIIDF(fileId, offset, dataLen, codingScheme, onsuccess, onerror) {
    // Valid fileId is '4FXX', see TS 31.102, clause 4.6.1.2.
    if (fileId >> 8 != 0x4f) {
      if (onerror) {
        onerror();
      }
      return;
    }

    function callback(options) {
      let value = options.simResponse;
      let RIL = this.context.RIL;
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(value);
      // Each octet is encoded into two chars.
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;

      if (octetLen < offset + dataLen) {
        // Data length is not enough. See TS 31.102, clause 4.6.1.1, the
        // paragraph "Bytes 8 and 9: Length of Image Instance Data."
        GsmPDUHelperseekIncoming(octetLen * PDU_HEX_OCTET_SIZE);
        if (onerror) {
          onerror();
        }
        return;
      }

      GsmPDUHelper.seekIncoming(offset * PDU_HEX_OCTET_SIZE);

      let rawData = {
        width: GsmPDUHelper.readHexOctet(),
        height: GsmPDUHelper.readHexOctet(),
        codingScheme,
      };

      switch (codingScheme) {
        case ICC_IMG_CODING_SCHEME_BASIC:
          rawData.body = GsmPDUHelper.readHexOctetArray(
            dataLen - ICC_IMG_HEADER_SIZE_BASIC
          );
          GsmPDUHelper.seekIncoming(
            (octetLen - offset - dataLen) * PDU_HEX_OCTET_SIZE
          );
          break;

        case ICC_IMG_CODING_SCHEME_COLOR:
        case ICC_IMG_CODING_SCHEME_COLOR_TRANSPARENCY:
          rawData.bitsPerImgPoint = GsmPDUHelper.readHexOctet();
          let num = GsmPDUHelper.readHexOctet();
          // The value 0 shall be interpreted as 256. See TS 31.102, Annex B.2.
          rawData.numOfClutEntries = num === 0 ? 0x100 : num;
          rawData.clutOffset =
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          rawData.body = GsmPDUHelper.readHexOctetArray(
            dataLen - ICC_IMG_HEADER_SIZE_COLOR
          );

          GsmPDUHelper.seekIncoming(
            (rawData.clutOffset - offset - dataLen) * PDU_HEX_OCTET_SIZE
          );
          let clut = GsmPDUHelper.readHexOctetArray(
            rawData.numOfClutEntries * ICC_CLUT_ENTRY_SIZE
          );

          rawData.clut = clut;
      }

      if (onsuccess) {
        onsuccess(rawData);
      }
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId,
      aid: this.aid,
      pathId: this.context.ICCFileHelper.getEFPath(ICC_EF_IMG),
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Read the (U)SIM Service Table from the (U)SIM.
   */
  readSST() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;

      // Each octet is encoded into two chars.
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      this.context.GsmPDUHelper.initWith(options.simResponse);
      let sst = this.context.GsmPDUHelper.readHexOctetArray(octetLen);
      RIL.iccInfoPrivate.sst = sst;
      if (DEBUG) {
        let str = "";
        let id = 0;
        for (let i = 0; i < sst.length; i++) {
          for (let j = 0; j < 8; j++) {
            if (sst[i] & (1 << j)) {
              id = i * 8 + j + 1;
              str = str + "[" + id + "], ";
            }
          }
        }
        this.context.debug("SST: Service available for " + str);
      }

      let ICCUtilsHelper = this.context.ICCUtilsHelper;
      if (ICCUtilsHelper.isICCServiceAvailable("MSISDN")) {
        if (DEBUG) {
          this.context.debug("MSISDN: MSISDN is available");
        }
        this.readMSISDN();
      } else if (DEBUG) {
        this.context.debug("MSISDN: MSISDN service is not available");
      }

      // Fetch SPN and PLMN list, if some of them are available.
      if (ICCUtilsHelper.isICCServiceAvailable("SPN")) {
        if (DEBUG) {
          this.context.debug("SPN: SPN is available");
        }
        this.readSPN();
      } else if (DEBUG) {
        this.context.debug("SPN: SPN service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("MDN")) {
        if (DEBUG) {
          this.context.debug("MDN: MDN available.");
        }
        this.readMBDN();
      } else {
        if (DEBUG) {
          this.context.debug("MDN: MDN service is not available");
        }

        if (ICCUtilsHelper.isCphsServiceAvailable("MBN")) {
          // read CPHS_MBN in advance if MBDN is not available.
          this.readCphsMBN();
        } else if (DEBUG) {
          this.context.debug("CPHS_MBN: CPHS_MBN service is not available");
        }
      }

      if (ICCUtilsHelper.isICCServiceAvailable("MWIS")) {
        if (DEBUG) {
          this.context.debug("MWIS: MWIS is available");
        }
        this.readMWIS();
      } else if (DEBUG) {
        this.context.debug("MWIS: MWIS is not available");
      }

      if (ICCUtilsHelper.isCphsServiceAvailable("ONSF")) {
        if (DEBUG) {
          this.context.debug("ONSF: ONSF is available");
        }
        this.readCphsONSF();
      } else if (DEBUG) {
        this.context.debug("ONSF: ONSF is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("SPDI")) {
        if (DEBUG) {
          this.context.debug("SPDI: SPDI available.");
        }
        this.readSPDI();
      } else if (DEBUG) {
        this.context.debug("SPDI: SPDI service is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("PNN")) {
        if (DEBUG) {
          this.context.debug("PNN: PNN is available");
        }
        this.readPNN();
      } else if (DEBUG) {
        this.context.debug("PNN: PNN is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("OPL")) {
        if (DEBUG) {
          this.context.debug("OPL: OPL is available");
        }
        this.readOPL();
      } else if (DEBUG) {
        this.context.debug("OPL: OPL is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("GID1")) {
        if (DEBUG) {
          this.context.debug("GID1: GID1 is available");
        }
        this.readGID1();
      } else if (DEBUG) {
        this.context.debug("GID1: GID1 is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("GID2")) {
        if (DEBUG) {
          this.context.debug("GID2: GID2 is available");
        }
        this.readGID2();
      } else if (DEBUG) {
        this.context.debug("GID2: GID2 is not available");
      }

      if (ICCUtilsHelper.isICCServiceAvailable("CFIS")) {
        if (DEBUG) {
          this.context.debug("CFIS: CFIS is available");
        }
        this.readCFIS();
      } else if (DEBUG) {
        this.context.debug("CFIS: CFIS is not available");
      }

      if (Services.prefs.getBoolPref(kPrefAppCBConfigurationEnabled)) {
        if (ICCUtilsHelper.isICCServiceAvailable("CBMI")) {
          this.readCBMI();
        } else {
          if (DEBUG) {
            this.context.debug("CBMI: CBMI is not available");
          }
          RIL.cellBroadcastConfigs.CBMI = null;
        }
        if (ICCUtilsHelper.isICCServiceAvailable("DATA_DOWNLOAD_SMS_CB")) {
          this.readCBMID();
        } else {
          if (DEBUG) {
            this.context.debug("CBMID: CBMID is not available");
          }
          RIL.cellBroadcastConfigs.CBMID = null;
        }
        if (ICCUtilsHelper.isICCServiceAvailable("CBMIR")) {
          this.readCBMIR();
        } else {
          if (DEBUG) {
            this.context.debug("CBMIR: CBMIR is not available");
          }
          RIL.cellBroadcastConfigs.CBMIR = null;
        }
      }
      RIL._mergeAllCellBroadcastConfigs();
    }

    // ICC_EF_UST has the same value with ICC_EF_SST.
    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_SST,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read (U)SIM MBDN. (Mailbox Dialling Number)
   *
   * @see TS 131.102, clause 4.2.60
   */
  readMBDN() {
    function callback(options) {
      let RIL = this.context.RIL;
      let contact = this.context.ICCPDUHelper.readAlphaIdDiallingNumber(
        options
      );
      if (
        (!contact ||
          ((!contact.alphaId || contact.alphaId == "") &&
            (!contact.number || contact.number == ""))) &&
        this.context.ICCUtilsHelper.isCphsServiceAvailable("MBN")
      ) {
        // read CPHS_MBN in advance if MBDN is invalid or empty.
        this.readCphsMBN();
        return;
      }

      if (
        !contact ||
        (RIL.iccInfoPrivate.mbdn !== undefined &&
          RIL.iccInfoPrivate.mbdn === contact.number)
      ) {
        return;
      }
      RIL.iccInfoPrivate.mbdn = contact.number;
      if (DEBUG) {
        this.context.debug(
          "MBDN, alphaId=" + contact.alphaId + " number=" + contact.number
        );
      }

      contact.rilMessageType = "iccmbdn";
      RIL.handleUnsolicitedMessage(contact);
    }
    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_MBDN,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read ICC MWIS. (Message Waiting Indication Status)
   *
   * @see TS 31.102, clause 4.2.63 for USIM and TS 51.011, clause 10.3.45 for SIM.
   */
  readMWIS() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;

      // Each octet is encoded into two chars.
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      this.context.GsmPDUHelper.initWith(options.simResponse);
      let mwis = this.context.GsmPDUHelper.readHexOctetArray(octetLen);
      if (!mwis) {
        return;
      }
      RIL.iccInfoPrivate.mwis = mwis; //Keep raw MWIS for updateMWIS()

      let mwi = {};
      // b8 b7 B6 b5 b4 b3 b2 b1   4.2.63, TS 31.102 version 11.6.0
      //  |  |  |  |  |  |  |  |__ Voicemail
      //  |  |  |  |  |  |  |_____ Fax
      //  |  |  |  |  |  |________ Electronic Mail
      //  |  |  |  |  |___________ Other
      //  |  |  |  |______________ Videomail
      //  |__|__|_________________ RFU
      mwi.active = (mwis[0] & 0x01) != 0;

      if (mwi.active) {
        // In TS 23.040 msgCount is in the range from 0 to 255.
        // The value 255 shall be taken to mean 255 or greater.
        //
        // However, There is no definition about 0 when MWI is active.
        //
        // Normally, when mwi is active, the msgCount must be larger than 0.
        // Refer to other reference phone,
        // 0 is usually treated as UNKNOWN for storing 2nd level MWI status (DCS).
        mwi.msgCount =
          mwis[1] === 0 ? GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN : mwis[1];
      } else {
        mwi.msgCount = 0;
      }

      let response = {};
      response.rilMessageType = "iccmwis";
      response.mwi = mwi;
      RIL.handleUnsolicitedMessage(response);
      /*RIL.sendChromeMessage({ rilMessageType: "iccmwis",
                              mwi: mwi });*/
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_MWIS,
      aid: this.aid,
      recordNumber: 1, // Get 1st Subscriber Profile.
      callback: callback.bind(this),
    });
  },

  /**
   * Update ICC MWIS. (Message Waiting Indication Status)
   *
   * @see TS 31.102, clause 4.2.63 for USIM and TS 51.011, clause 10.3.45 for SIM.
   */
  updateMWIS(mwi) {
    let RIL = this.context.RIL;
    if (!RIL.iccInfoPrivate.mwis) {
      return;
    }

    function dataWriter(recordSize) {
      let mwis = RIL.iccInfoPrivate.mwis;

      let msgCount =
        mwi.msgCount === GECKO_VOICEMAIL_MESSAGE_COUNT_UNKNOWN
          ? 0
          : mwi.msgCount;

      [mwis[0], mwis[1]] = mwi.active
        ? [mwis[0] | 0x01, msgCount]
        : [mwis[0] & 0xfe, 0];

      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();
      for (let i = 0; i < mwis.length; i++) {
        GsmPDUHelper.writeHexOctet(mwis[i]);
      }
    }

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId: ICC_EF_MWIS,
      aid: this.aid,
      recordNumber: 1, // Update 1st Subscriber Profile.
      dataWriter: dataWriter.bind(this),
    });
  },

  /**
   * Read the SPDI (Service Provider Display Information) from the (U)SIM.
   *
   * See TS 131.102 section 4.2.66 for USIM and TS 51.011 section 10.3.50
   * for SIM.
   */
  readSPDI() {
    function callback(options) {
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      let readLen = 0;
      let endLoop = false;

      let RIL = this.context.RIL;
      RIL.iccInfoPrivate.SPDI = null;

      let GsmPDUHelper = this.context.GsmPDUHelper;
      while (readLen < octetLen && !endLoop) {
        let tlvTag = GsmPDUHelper.processHexToInt(
          value.slice(readLen, readLen + 2),
          16
        );
        let tlvLen = GsmPDUHelper.processHexToInt(
          value.slice(readLen + 2, readLen + 4),
          16
        );
        readLen += 4; // For tag and length fields.
        switch (tlvTag) {
          case SPDI_TAG_SPDI:
            // The value part itself is a TLV.
            continue;
          case SPDI_TAG_PLMN_LIST:
            // This PLMN list is what we want.
            let plmn_value = value.slice(readLen);
            RIL.iccInfoPrivate.SPDI = this.readPLMNEntries(
              plmn_value,
              tlvLen / 3
            );
            readLen += tlvLen * 2;
            endLoop = true;
            break;
          default:
            // We don't care about its content if its tag is not SPDI nor
            // PLMN_LIST.
            endLoop = true;
            break;
        }
      }

      if (DEBUG) {
        this.context.debug("SPDI: " + JSON.stringify(RIL.iccInfoPrivate.SPDI));
      }
      let ICCUtilsHelper = this.context.ICCUtilsHelper;
      if (ICCUtilsHelper.updateDisplayCondition()) {
        ICCUtilsHelper.handleICCInfoChange();
      }
    }

    // PLMN List is Servive 51 in USIM, EF_SPDI
    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_SPDI,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  _readCbmiHelper(which) {
    let RIL = this.context.RIL;

    function callback(options) {
      let value = options.simResponse;
      let strLength = value.length;

      // Each Message Identifier takes two octets and each octet is encoded
      // into two chars.
      let numIds = strLength / 4,
        list = null;
      if (numIds) {
        list = [];
        let GsmPDUHelper = this.context.GsmPDUHelper;
        GsmPDUHelper.initWith(value);
        for (let i = 0, id; i < numIds; i++) {
          id = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if (id != 0xffff) {
            list.push(id);
            list.push(id + 1);
          }
        }
      }
      if (DEBUG) {
        this.context.debug(which + ": " + JSON.stringify(list));
      }

      RIL.cellBroadcastConfigs[which] = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs[which] = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    let fileId = GLOBAL["ICC_EF_" + which];
    this.context.ICCIOHelper.loadTransparentEF({
      fileId,
      aid: this.aid,
      callback: callback.bind(this),
      onerror: onerror.bind(this),
    });
  },

  /**
   * Read EFcbmi (Cell Broadcast Message Identifier selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.14 EFcbmi
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.13 EFcbmi
   */
  readCBMI() {
    this._readCbmiHelper("CBMI");
  },

  /**
   * Read EFcbmid (Cell Broadcast Message Identifier for Data Download)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.20 EFcbmid
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.26 EFcbmid
   */
  readCBMID() {
    this._readCbmiHelper("CBMID");
  },

  /**
   * Read EFcbmir (Cell Broadcast Message Identifier Range selection)
   *
   * @see 3GPP TS 31.102 v110.02.0 section 4.2.22 EFcbmir
   * @see 3GPP TS 51.011 v5.0.0 section 10.3.28 EFcbmir
   */
  readCBMIR() {
    let RIL = this.context.RIL;

    function callback(options) {
      let value = options.simResponse;
      let strLength = value.length;

      // Each Message Identifier range takes four octets and each octet is
      // encoded into two chars.
      let numIds = strLength / 8,
        list = null;
      if (numIds) {
        list = [];
        let GsmPDUHelper = this.context.GsmPDUHelper;
        GsmPDUHelper.initWith(value);
        for (let i = 0, from, to; i < numIds; i++) {
          // `Bytes one and two of each range identifier equal the lower value
          // of a cell broadcast range, bytes three and four equal the upper
          // value of a cell broadcast range.`
          from =
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          to = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          // `Unused entries shall be set to 'FF FF'.`
          if (from != 0xffff && to != 0xffff) {
            list.push(from);
            list.push(to + 1);
          }
        }
      }
      if (DEBUG) {
        this.context.debug("CBMIR: " + JSON.stringify(list));
      }

      RIL.cellBroadcastConfigs.CBMIR = list;
      RIL._mergeAllCellBroadcastConfigs();
    }

    function onerror() {
      RIL.cellBroadcastConfigs.CBMIR = null;
      RIL._mergeAllCellBroadcastConfigs();
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_CBMIR,
      aid: this.aid,
      callback: callback.bind(this),
      onerror: onerror.bind(this),
    });
  },

  /**
   * Read OPL (Operator PLMN List) from (U)SIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.59 for USIM
   *     3GPP TS 51.011 Sec. 10.3.42 for SIM.
   */
  readOPL() {
    let ICCIOHelper = this.context.ICCIOHelper;
    let opl = [];
    function callback(options) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      let value = options.simResponse;
      // The first 7 bytes are LAI (for UMTS) and the format of LAI is defined
      // in 3GPP TS 23.003, Sec 4.1
      //    +-------------+---------+
      //    | Octet 1 - 3 | MCC/MNC |
      //    +-------------+---------+
      //    | Octet 4 - 7 |   LAC   |
      //    +-------------+---------+
      //    | Octet 8     |   NNRI  |
      //    +-------------+---------+
      let mccMnc = [
        GsmPDUHelper.processHexToInt(value.slice(0, 2), 16),
        GsmPDUHelper.processHexToInt(value.slice(2, 4), 16),
        GsmPDUHelper.processHexToInt(value.slice(4, 6), 16),
      ];
      if (mccMnc[0] != 0xff || mccMnc[1] != 0xff || mccMnc[2] != 0xff) {
        let oplElement = {};
        let semiOctets = [];
        for (let i = 0; i < mccMnc.length; i++) {
          semiOctets.push((mccMnc[i] & 0xf0) >> 4);
          semiOctets.push(mccMnc[i] & 0x0f);
        }
        let reformat = [
          semiOctets[1],
          semiOctets[0],
          semiOctets[3],
          semiOctets[5],
          semiOctets[4],
          semiOctets[2],
        ];

        let buf = "";
        for (let i = 0; i < reformat.length; i++) {
          if (reformat[i] != 0xf) {
            buf += GsmPDUHelper.semiOctetToExtendedBcdChar(reformat[i]);
          }
          if (i === 2) {
            // 0-2: MCC
            oplElement.mcc = buf;
            buf = "";
          } else if (i === 5) {
            // 3-5: MNC
            oplElement.mnc = buf;
          }
        }
        // LAC/TAC
        oplElement.lacTacStart =
          (GsmPDUHelper.processHexToInt(value.slice(6, 8), 16) << 8) |
          GsmPDUHelper.processHexToInt(value.slice(8, 10), 16);
        oplElement.lacTacEnd =
          (GsmPDUHelper.processHexToInt(value.slice(10, 12), 16) << 8) |
          GsmPDUHelper.processHexToInt(value.slice(12, 14), 16);
        // PLMN Network Name Record Identifier
        oplElement.pnnRecordId = GsmPDUHelper.processHexToInt(
          value.slice(14, 16),
          16
        );
        if (DEBUG) {
          this.context.debug(
            "OPL: [" + (opl.length + 1) + "]: " + JSON.stringify(oplElement)
          );
        }
        opl.push(oplElement);
      } else {
      }

      let RIL = this.context.RIL;
      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        RIL.iccInfoPrivate.OPL = opl;
        RIL.overrideNetworkName();
      }
    }

    ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_OPL,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read PNN (PLMN Network Name) from (U)SIM.
   *
   * See 3GPP TS 31.102 Sec. 4.2.58 for USIM
   *     3GPP TS 51.011 Sec. 10.3.41 for SIM.
   */
  readPNN() {
    let ICCIOHelper = this.context.ICCIOHelper;
    function callback(options) {
      let pnnElement;
      let value = options.simResponse;
      let strLen = value.length;
      let octetLen = strLen / PDU_HEX_OCTET_SIZE;
      let readLen = 0;

      let GsmPDUHelper = this.context.GsmPDUHelper;

      while (readLen < strLen) {
        // One byte for the TAG.
        let tlvTag = GsmPDUHelper.processHexToInt(
          value.slice(readLen, readLen + 2),
          16
        );

        if (tlvTag == 0xff) {
          // Unused byte
          readLen += 2;
          break;
        }

        // Needs this check to avoid initializing twice.
        pnnElement = pnnElement || {};

        // One byte for the length.
        let tlvLen = GsmPDUHelper.processHexToInt(
          value.slice(readLen + 2, readLen + 4),
          16
        );

        let nameValue = value.slice(readLen + 4);
        switch (tlvTag) {
          case PNN_IEI_FULL_NETWORK_NAME:
            pnnElement.fullName = GsmPDUHelper.readNetworkName(
              nameValue,
              tlvLen
            );
            break;
          case PNN_IEI_SHORT_NETWORK_NAME:
            pnnElement.shortName = GsmPDUHelper.readNetworkName(
              nameValue,
              tlvLen
            );
            break;
          default:
            break;
        }

        readLen += tlvLen * 2 + 4; // +4 for tlvTag and tlvLen
      }

      pnn.push(pnnElement);

      let RIL = this.context.RIL;
      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        if (DEBUG) {
          for (let i = 0; i < pnn.length; i++) {
            this.context.debug("PNN: [" + i + "]: " + JSON.stringify(pnn[i]));
          }
        }
        RIL.iccInfoPrivate.PNN = pnn;
        RIL.overrideNetworkName();
      }
    }

    let pnn = [];
    ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_PNN,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   *  Read the list of PLMN (Public Land Mobile Network) entries
   *  We cannot directly rely on readSwappedNibbleBcdToString(),
   *  since it will no correctly handle some corner-cases that are
   *  not a problem in our case (0xFF 0xFF 0xFF).
   *
   *  @param length The number of PLMN records.
   *  @return An array of string corresponding to the PLMNs.
   */
  readPLMNEntries(value, length) {
    let plmnList = [];
    // Each PLMN entry has 3 bytes.
    if (DEBUG) {
      this.context.debug(
        "PLMN entries length = " + length + " , value=" + value
      );
    }
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let index = 0;
    while (index < length) {
      // Unused entries will be 0xFFFFFF, according to EF_SPDI
      // specs (TS 131 102, section 4.2.66)
      try {
        let plmn = [
          GsmPDUHelper.processHexToInt(value.slice(0, 2), 16),
          GsmPDUHelper.processHexToInt(value.slice(2, 4), 16),
          GsmPDUHelper.processHexToInt(value.slice(4, 6), 16),
        ];
        if (DEBUG) {
          this.context.debug(
            "Reading PLMN entry: [" + index + "]: '" + plmn + "'"
          );
        }
        if (plmn[0] != 0xff && plmn[1] != 0xff && plmn[2] != 0xff) {
          let semiOctets = [];
          for (let idx = 0; idx < plmn.length; idx++) {
            semiOctets.push((plmn[idx] & 0xf0) >> 4);
            semiOctets.push(plmn[idx] & 0x0f);
          }

          // According to TS 24.301, 9.9.3.12, the semi octets is arranged
          // in format:
          // Byte 1: MCC[2] | MCC[1]
          // Byte 2: MNC[3] | MCC[3]
          // Byte 3: MNC[2] | MNC[1]
          // Therefore, we need to rearrange them.
          let reformat = [
            semiOctets[1],
            semiOctets[0],
            semiOctets[3],
            semiOctets[5],
            semiOctets[4],
            semiOctets[2],
          ];
          let buf = "";
          let plmnEntry = {};
          for (let i = 0; i < reformat.length; i++) {
            if (reformat[i] != 0xf) {
              buf += GsmPDUHelper.semiOctetToExtendedBcdChar(reformat[i]);
            }
            if (i === 2) {
              // 0-2: MCC
              plmnEntry.mcc = buf;
              buf = "";
            } else if (i === 5) {
              // 3-5: MNC
              plmnEntry.mnc = buf;
            }
          }
          if (DEBUG) {
            this.context.debug(
              "PLMN = " + plmnEntry.mcc + ", " + plmnEntry.mnc
            );
          }
          plmnList.push(plmnEntry);
        }
      } catch (e) {
        if (DEBUG) {
          this.context.debug("PLMN entry " + index + " is invalid.");
        }
        break;
      }
      // Remove the readed value.
      value = value.slice(index + 6);
      index++;
    }
    return plmnList;
  },

  /**
   * Read the SMS from the ICC.
   *
   * @param recordNumber The number of the record shall be loaded.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  readSMS(recordNumber, onsuccess, onerror) {
    function callback(options) {
      let value = options.simResponse;

      // TS 51.011, 10.5.3 EF_SMS
      // b3 b2 b1
      //  0  0  1 message received by MS from network; message read
      //  0  1  1 message received by MS from network; message to be read
      //  1  1  1 MS originating message; message to be sent
      //  1  0  1 MS originating message; message sent to the network:
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(value);
      let status = GsmPDUHelper.readHexOctet();

      let message = GsmPDUHelper.readMessage();
      message.simStatus = status;

      if (message) {
        onsuccess(message);
      } else {
        onerror("Failed to decode SMS on SIM #" + recordNumber);
      }
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_SMS,
      aid: this.aid,
      recordNumber,
      callback: callback.bind(this),
      onerror,
    });
  },

  readGID1() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;

      RIL.iccInfo.gid1 = value;
      if (DEBUG) {
        this.context.debug("GID1: " + RIL.iccInfo.gid1);
      }
      this.context.ICCUtilsHelper.handleICCInfoChange();
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_GID1,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  readGID2() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;

      RIL.iccInfo.gid2 = value;
      if (DEBUG) {
        this.context.debug("GID2: " + RIL.iccInfo.gid2);
      }
      this.context.ICCUtilsHelper.handleICCInfoChange();
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_GID2,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  readCFIS() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;
      let PDUHelper = this.context.GsmPDUHelper;
      PDUHelper.initWith(value);
      let cfis = {};

      cfis.msp = PDUHelper.readHexOctet();
      cfis.indicator = PDUHelper.readHexOctet();
      cfis.number = this.context.ICCPDUHelper.readNumberWithLength();
      cfis.ccpRecordNumber = PDUHelper.readHexOctet();
      cfis.extRecordNumber = PDUHelper.readHexOctet();

      if (cfis.msp < 1 || cfis.msp > 4) {
        if (DEBUG) {
          this.context.debug("CFIS content error: invalid msp");
        }
        return;
      }

      if (
        !RIL.iccInfoPrivate.cfis ||
        RIL.iccInfoPrivate.cfis.indicator != cfis.indicator
      ) {
        RIL.iccInfoPrivate.cfis = cfis;
        if (DEBUG) {
          this.context.debug("cfis changed, NotifyCFStatechanged");
        }
        RIL.sendChromeMessage({
          rilMessageType: "cfstatechanged",
          action: cfis.indicator & 0x01,
          reason: CALL_FORWARD_REASON_UNCONDITIONAL,
          number: cfis.number,
          timeSeconds: 0,
          serviceClass: ICC_SERVICE_CLASS_VOICE,
        });
      }
    }

    function onerror(errorMsg) {
      if (DEBUG) {
        this.context.debug("readCFIS onerror: " + errorMsg);
      }
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_CFIS,
      aid: this.aid,
      recordNumber: 1,
      recordSize: CFIS_RECORD_SIZE_BYTES,
      callback: callback.bind(this),
      onerror: onerror.bind(this),
    });
  },

  updateCFIS(options) {
    let dataWriter = function dataWriter(recordSize) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith();
      let cfis = this.context.RIL.iccInfoPrivate.cfis;

      GsmPDUHelper.writeHexOctet(cfis.msp);

      // Update service class voice only
      if (options.action) {
        cfis.indicator = cfis.indicator | 1;
      } else {
        cfis.indicator = cfis.indicator & 0xfe;
      }
      GsmPDUHelper.writeHexOctet(cfis.indicator);

      let writtenNumber = this.context.ICCPDUHelper.writeNumberWithLength(
        options.number
      );
      cfis.number = options.number;

      GsmPDUHelper.writeHexOctet(cfis.ccpRecordNumber);
      GsmPDUHelper.writeHexOctet(cfis.extRecordNumber);
    }.bind(this);

    let callback = function callback(options) {
      if (DEBUG) {
        this.context.debug("updateCFIS success");
      }
    }.bind(this);

    let onerror = function onerror(errorMsg) {
      if (DEBUG) {
        this.context.debug("updateCFIS errorMessage: " + errorMsg);
      }
    }.bind(this);

    this.context.ICCIOHelper.updateLinearFixedEF({
      fileId: ICC_EF_CFIS,
      aid: this.aid,
      recordNumber: 1,
      dataWriter,
      callback,
      onerror,
    });
  },

  /**
   * Read CPHS Phase & Service Table from CPHS Info.
   *
   * @See  B.3.1.1 CPHS Information in CPHS Phase 2.
   *
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readCphsInfo(onsuccess, onerror) {
    function callback(options) {
      try {
        let RIL = this.context.RIL;
        this.context.GsmPDUHelper.initWith(options.simResponse);
        let octetLen = options.simResponse.length / PDU_HEX_OCTET_SIZE;
        let cphsInfo = this.context.GsmPDUHelper.readHexOctetArray(octetLen);
        if (DEBUG) {
          let str = "";
          for (let i = 0; i < cphsInfo.length; i++) {
            str += cphsInfo[i] + ", ";
          }
          this.context.debug("CPHS INFO: " + str);
        }

        /**
         * CPHS INFORMATION
         *
         * Byte 1: CPHS Phase
         * 01 phase 1
         * 02 phase 2
         * etc.
         *
         * Byte 2: CPHS Service Table
         * +----+----+----+----+----+----+----+----+
         * | b8 | b7 | b6 | b5 | b4 | b3 | b2 | b1 |
         * +----+----+----+----+----+----+----+----+
         * |   ONSF  |   MBN   |   SST   |   CSP   |
         * | Phase 2 |   ALL   | Phase 1 |   All   |
         * +----+----+----+----+----+----+----+----+
         *
         * Byte 3: CPHS Service Table continued
         * +----+----+----+----+----+----+----+----+
         * | b8 | b7 | b6 | b5 | b4 | b3 | b2 | b1 |
         * +----+----+----+----+----+----+----+----+
         * |   RFU   |   RFU   |   RFU   | INFO_NUM|
         * |         |         |         | Phase 2 |
         * +----+----+----+----+----+----+----+----+
         */
        let cphsPhase = cphsInfo[0];
        if (cphsPhase == 1) {
          // Clear 'Phase 2 only' services.
          cphsInfo[1] &= 0x3f;
          // We don't know whether Byte 3 is available in CPHS phase 1 or not.
          // Add boundary check before accessing it.
          if (cphsInfo.length > 2) {
            cphsInfo[2] = 0x00;
          }
        } else if (cphsPhase == 2) {
          // Clear 'Phase 1 only' services.
          cphsInfo[1] &= 0xf3;
        } else {
          throw new Error("Unknown CPHS phase: " + cphsPhase);
        }

        RIL.iccInfoPrivate.cphsSt = cphsInfo.subarray(1);
        onsuccess();
      } catch (e) {
        onerror(e.toString());
      }
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_CPHS_INFO,
      aid: this.aid,
      callback: callback.bind(this),
      onerror,
    });
  },

  /**
   * Read CPHS MBN. (Mailbox Numbers)
   *
   * @See B.4.2.2 Voice Message Retrieval and Indicator Clearing
   */
  readCphsMBN() {
    function callback(options) {
      let RIL = this.context.RIL;
      let contact = this.context.ICCPDUHelper.readAlphaIdDiallingNumber(
        options
      );
      if (
        !contact ||
        (RIL.iccInfoPrivate.mbdn !== undefined &&
          RIL.iccInfoPrivate.mbdn === contact.number)
      ) {
        return;
      }
      RIL.iccInfoPrivate.mbdn = contact.number;
      if (DEBUG) {
        this.context.debug(
          "CPHS_MDN, alphaId=" + contact.alphaId + " number=" + contact.number
        );
      }
      contact.rilMessageType = "iccmbdn";
      RIL.sendChromeMessage(contact);
    }

    this.context.ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_CPHS_MBN,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Process CHPS Operator Name String/Operator Name Short Form.
   *
   * @return A string corresponding to the CPHS ONS.
   */
  _processCphsOnsResponse(value) {
    // Each octet is encoded into two chars.
    let octetLen = value.length / PDU_HEX_OCTET_SIZE;
    let ons = this.context.ICCPDUHelper.readAlphaIdentifier(value, octetLen);
    return ons;
  },

  /**
   * Read CPHS Operator Name String from the SIM.
   */
  readCphsONS() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;
      RIL.iccInfoPrivate.ons = this._processCphsOnsResponse(value);
      if (DEBUG) {
        this.context.debug(
          "CPHS Operator Name String = " + RIL.iccInfoPrivate.ons
        );
      }
      RIL.overrideNetworkName();
    }
    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_CPHS_ONS,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read CPHS Operator Name Shortform from the SIM.
   */
  readCphsONSF() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;
      RIL.iccInfoPrivate.ons_short_form = this._processCphsOnsResponse(value);
      if (DEBUG) {
        this.context.debug(
          "CPHS Operator Name Shortform = " + RIL.iccInfoPrivate.ons_short_form
        );
      }
      RIL.overrideNetworkName();
    }

    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_CPHS_ONSF,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Read CPHS Call Forward Flags from the SIM.
   */
  readCphsCFF() {
    function callback(options) {
      let RIL = this.context.RIL;
      let value = options.simResponse;
      let octetLen = value.length / PDU_HEX_OCTET_SIZE;
      this.context.GsmPDUHelper.initWith(value);

      if (octetLen != 0) {
        // CPHS 4.2 B4.5
        let cff = [];
        for (let i = 0; octetLen > i; i++) {
          cff[i] = this.context.GsmPDUHelper.readHexOctet();
        }

        // The first byte for mandatory voice CFF
        if (!RIL.iccInfoPrivate.cff || RIL.iccInfoPrivate.cff[0] != cff[0]) {
          // There's only flag in CFF but no detail, fill fake data
          RIL.iccInfoPrivate.cff = cff;
          RIL.sendChromeMessage({
            rilMessageType: "cfstatechanged",
            action: cff[0] & 0xa0,
            reason: CALL_FORWARD_REASON_UNCONDITIONAL,
            number: "0000000000",
            timeSeconds: 0,
            serviceClass: ICC_SERVICE_CLASS_VOICE,
          });
        }
      }
    }
    this.context.ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_CPHS_CFF,
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  /**
   * Update CPHS Call Forward Flag to sim
   */
  updateCphsCFF(flag) {
    function dataWriter(fileSize) {
      let cff = this.context.RIL.iccInfoPrivate.cff;

      if (flag) {
        cff[0] = (cff[0] & 0x0f) | 0xa0;
      } else {
        cff[0] = (cff[0] & 0x0f) | 0x50;
      }
      this.context.RIL.iccInfoPrivate.cff = cff;
      this.context.GsmPDUHelpe.initWith();

      for (let i = 0; i < fileSize; i++) {
        this.context.GsmPDUHelper.writeHexOctet(cff[i]);
      }
    }

    function callback() {
      if (DEBUG) {
        this.context.debug("updateCphsCFF success");
      }
    }

    function onerror() {
      if (DEBUG) {
        this.context.debug("updateCphsCFF fail");
      }
    }

    // Cphs4.2 6F13 Call Forwarding Flags
    this.context.ICCIOHelper.updateTransparentEF({
      fileId: ICC_EF_CPHS_CFF,
      aid: this.aid,
      dataWriter: dataWriter.bind(this),
      callback: callback.bind(this),
      onerror: onerror.bind(this),
    });
  },
};

//FIXME
function ISimRecordHelperObject(aContext) {
  this.context = aContext;
}
ISimRecordHelperObject.prototype = {
  context: null,
  // aid for ISIM application.
  aid: null,

  // IMS private user identity. Transparent EF.
  impi: null,
  // IMS public user identity. Linear fixed EF.
  impus: [],

  setAid(aid) {
    if (DEBUG) {
      this.context.debug("ISIM aid : " + aid);
    }
    this.aid = aid;
  },

  fetchISimRecords() {
    // Return if no ISIM aid was cached.
    if (!this.aid) {
      return;
    }

    this.readIMPI();
    this.readIMPU();
  },

  // Sim refresh file updated.
  handleFileUpdate(aEfId) {
    switch (aEfId) {
      case ICC_EF_ISIM_IMPI:
        if (DEBUG) this.context.debug("File refresh for EF_ISIM_IMPI.");
        this.readIMPI();
        break;
      case ICC_EF_ISIM_IMPU:
        if (DEBUG) this.context.debug("File refresh for ISIM_IMPU.");
        this.readIMPU();
        break;
      default:
        // No one knew how to handle this particular file, so to be safe just
        // fetch all records.
        if (DEBUG) this.context.debug("ISIM Refresh for all.");
        this.fetchISimRecords();
        break;
    }
  },

  readIMPI() {
    let ICCFileHelper = this.context.ICCFileHelper;
    let ICCIOHelper = this.context.ICCIOHelper;

    function callback(options) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(options.simResponse);
      let tlvTag = GsmPDUHelper.readHexOctet();
      let tlvLen = GsmPDUHelper.readHexOctet();
      if (tlvTag === ICC_ISIM_NAI_TLV_DATA_OBJECT_TAG) {
        let str = "";
        for (let i = 0; i < tlvLen; i++) {
          str += String.fromCharCode(GsmPDUHelper.readHexOctet());
        }
        if (DEBUG) {
          this.context.debug("impi : " + str);
        }
        this.impi = str;
      }
      this._handleIsimInfoChange();
    }

    ICCIOHelper.loadTransparentEF({
      fileId: ICC_EF_ISIM_IMPI,
      pathId: ICCFileHelper.getIsimEFPath(ICC_EF_ISIM_IMPI),
      aid: this.aid,
      callback: callback.bind(this),
    });
  },

  readIMPU() {
    // Reset impu to empty array for avoiding redundant push.
    this.impus = [];
    let ICCFileHelper = this.context.ICCFileHelper;
    let ICCIOHelper = this.context.ICCIOHelper;

    function callback(options) {
      let GsmPDUHelper = this.context.GsmPDUHelper;
      GsmPDUHelper.initWith(options.simResponse);

      let ICCIOHelper = this.context.ICCIOHelper;

      let tlvTag = GsmPDUHelper.readHexOctet();
      let tlvLen = GsmPDUHelper.readHexOctet();
      if (tlvTag === ICC_ISIM_URI_TLV_DATA_OBJECTTAG) {
        let str = "";
        for (let i = 0; i < tlvLen; i++) {
          str += String.fromCharCode(GsmPDUHelper.readHexOctet());
        }
        if (str.length) {
          if (DEBUG) {
            this.context.debug("impu : " + str);
          }
          this.impus.push(str);
        }
      }

      if (options.p1 < options.totalRecords) {
        ICCIOHelper.loadNextRecord(options);
      } else {
        this._handleIsimInfoChange();
      }
    }
    function onerror(errorMsg) {
      this.context.debug("Error on reading readIMPU  : " + errorMsg);
    }

    ICCIOHelper.loadLinearFixedEF({
      fileId: ICC_EF_ISIM_IMPU,
      pathId: ICCFileHelper.getIsimEFPath(ICC_EF_ISIM_IMPU),
      aid: this.aid,
      callback: callback.bind(this),
      onerror: onerror.bind(this),
    });
  },

  _handleIsimInfoChange() {
    if (this.impi && this.impus && this.impus.length) {
      this.context.ICCUtilsHelper.handleISIMInfoChange({
        impi: this.impi,
        impus: this.impus,
      });
    }
  },
};

/**
 * Helper for processing ICC PDUs.
 */
function ICCPDUHelperObject(aContext) {
  this.context = aContext;
}
ICCPDUHelperObject.prototype = {
  context: null,

  /**
   * Read GSM 8-bit unpacked octets,
   * which are default 7-bit alphabets with bit 8 set to 0.
   *
   * @param numOctets
   *        Number of octets to be read.
   */
  read8BitUnpackedToString(value, numOctets) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let ret = "";
    let escapeFound = false;
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable =
      PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    for (let i = 0; i < numOctets * 2; i += 2) {
      let octet = GsmPDUHelper.processHexToInt(value.slice(i, i + 2), 16);

      if (octet == 0xff) {
        break;
      }

      if (escapeFound) {
        escapeFound = false;
        if (octet == PDU_NL_EXTENDED_ESCAPE) {
          // According to 3GPP TS 23.038, section 6.2.1.1, NOTE 1, "On
          // receipt of this code, a receiving entity shall display a space
          // until another extensiion table is defined."
          ret += " ";
        } else if (octet == PDU_NL_RESERVED_CONTROL) {
          // According to 3GPP TS 23.038 B.2, "This code represents a control
          // character and therefore must not be used for language specific
          // characters."
          ret += " ";
        } else {
          ret += langShiftTable[octet];
        }
      } else if (octet == PDU_NL_EXTENDED_ESCAPE) {
        escapeFound = true;
      } else {
        ret += langTable[octet];
      }
    }
    return ret;
  },

  /**
   * Write GSM 8-bit unpacked octets.
   *
   * @param numOctets   Number of total octets to be writen, including trailing
   *                    0xff.
   * @param str         String to be written. Could be null.
   *
   * @return The string has been written into Buf. "" if str is null.
   */
  writeStringTo8BitUnpacked(numOctets, str) {
    const langTable = PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];
    const langShiftTable =
      PDU_NL_SINGLE_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT];

    let GsmPDUHelper = this.context.GsmPDUHelper;

    // If the character is GSM extended alphabet, two octets will be written.
    // So we need to keep track of number of octets to be written.
    let i, j;
    let len = str ? str.length : 0;
    for (i = 0, j = 0; i < len && j < numOctets; i++) {
      let c = str.charAt(i);
      let octet = langTable.indexOf(c);

      if (octet == -1) {
        // Make sure we still have enough space to write two octets.
        if (j + 2 > numOctets) {
          break;
        }

        octet = langShiftTable.indexOf(c);
        if (octet == -1) {
          // Fallback to ASCII space.
          octet = langTable.indexOf(" ");
        } else {
          GsmPDUHelper.writeHexOctet(PDU_NL_EXTENDED_ESCAPE);
          j++;
        }
      }
      GsmPDUHelper.writeHexOctet(octet);
      j++;
    }

    // trailing 0xff
    while (j++ < numOctets) {
      GsmPDUHelper.writeHexOctet(0xff);
    }

    return str ? str.substring(0, i) : "";
  },

  /**
   * Write UCS2 String on UICC.
   * The default choose 0x81 or 0x82 encode, otherwise use 0x80 encode.
   *
   * @see TS 102.221, Annex A.
   * @param numOctets
   *        Total number of octets to be written. This includes the length of
   *        alphaId and the length of trailing unused octets(0xff).
   * @param str
   *        String to be written.
   *
   * @return The string has been written into Buf.
   */
  writeICCUCS2String(numOctets, str) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let scheme = 0x80;
    let basePointer;

    if (str.length > 2) {
      let min = 0xffff;
      let max = 0;
      for (let i = 0; i < str.length; i++) {
        let code = str.charCodeAt(i);
        // filter out GSM Default Alphabet character
        if (code & 0xff80) {
          if (min > code) {
            min = code;
          }
          if (max < code) {
            max = code;
          }
        }
      }

      // 0x81 and 0x82 only support 'half-page', i.e., 128 characters.
      if (max - min >= 0 && max - min < 128) {
        // 0x81 base pointer is 0hhh hhhh h000 0000, and bit 16 is set to zero,
        // therefore it can't compute 0x8000~0xFFFF.
        // Since 0x81 only support 128 characters,
        // either XX00~XX7f(bit 8 are 0) or XX80~XXff(bit 8 are 1)
        if ((min & 0x7f80) == (max & 0x7f80) && (max & 0x8000) == 0) {
          scheme = 0x81;
          basePointer = min & 0x7f80;
        } else {
          scheme = 0x82;
          basePointer = min;
        }
      }
    }

    switch (scheme) {
      /**
       * +------+---------+---------+---------+---------+------+------+
       * | 0x80 | Ch1_msb | Ch1_lsb | Ch2_msb | Ch2_lsb | 0xff | 0xff |
       * +------+---------+---------+---------+---------+------+------+
       */
      case 0x80: {
        // 0x80 support UCS2 0000~ffff
        GsmPDUHelper.writeHexOctet(0x80);
        numOctets--;
        // Now the str is UCS2 string, each character will take 2 octets.
        if (str.length * 2 > numOctets) {
          str = str.substring(0, Math.floor(numOctets / 2));
        }
        GsmPDUHelper.writeUCS2String(str);

        // trailing 0xff
        for (let i = str.length * 2; i < numOctets; i++) {
          GsmPDUHelper.writeHexOctet(0xff);
        }
        return str;
      }
      /**
       * +------+-----+--------------+-----+-----+-----+--------+------+
       * | 0x81 | len | base_pointer | Ch1 | Ch2 | ... | Ch_len | 0xff |
       * +------+-----+--------------+-----+-----+-----+--------+------+
       *
       * len: The length of characters.
       * base_pointer: 0hhh hhhh h000 0000
       * Ch_n: bit 8 = 0
       *       GSM default alphabets
       *       bit 8 = 1
       *       UCS2 character whose char code is (Ch_n - base_pointer) | 0x80
       *
       */
      case 0x81: {
        GsmPDUHelper.writeHexOctet(0x81);

        if (str.length > numOctets - 3) {
          str = str.substring(0, numOctets - 3);
        }

        GsmPDUHelper.writeHexOctet(str.length);
        GsmPDUHelper.writeHexOctet((basePointer >> 7) & 0xff);
        numOctets -= 3;
        break;
      }
      /* +------+-----+------------------+------------------+-----+-----+-----+--------+
       * | 0x82 | len | base_pointer_msb | base_pointer_lsb | Ch1 | Ch2 | ... | Ch_len |
       * +------+-----+------------------+------------------+-----+-----+-----+--------+
       *
       * len: The length of characters.
       * base_pointer_msb, base_pointer_lsn: base_pointer
       * Ch_n: bit 8 = 0
       *       GSM default alphabets
       *       bit 8 = 1
       *       UCS2 character whose char code is (Ch_n - base_pointer) | 0x80
       */
      case 0x82: {
        GsmPDUHelper.writeHexOctet(0x82);

        if (str.length > numOctets - 4) {
          str = str.substring(0, numOctets - 4);
        }

        GsmPDUHelper.writeHexOctet(str.length);
        GsmPDUHelper.writeHexOctet((basePointer >> 8) & 0xff);
        GsmPDUHelper.writeHexOctet(basePointer & 0xff);
        numOctets -= 4;
        break;
      }
    }

    if (scheme == 0x81 || scheme == 0x82) {
      for (let i = 0; i < str.length; i++) {
        let code = str.charCodeAt(i);

        // bit 8 = 0,
        // GSM default alphabets
        if (code >> 8 == 0) {
          GsmPDUHelper.writeHexOctet(code & 0x7f);
        } else {
          // bit 8 = 1,
          // UCS2 character whose char code is (code - basePointer) | 0x80
          GsmPDUHelper.writeHexOctet((code - basePointer) | 0x80);
        }
      }

      // trailing 0xff
      for (let i = 0; i < numOctets - str.length; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
    }
    return str;
  },

  /**
   * Read UCS2 String on UICC.
   *
   * @see TS 101.221, Annex A.
   * @param scheme
   *        Coding scheme for UCS2 on UICC. One of 0x80, 0x81 or 0x82.
   * @param numOctets
   *        Number of octets to be read as UCS2 string.
   */
  readICCUCS2String(scheme, value, numOctets) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let str = "";
    switch (scheme) {
      /**
       * +------+---------+---------+---------+---------+------+------+
       * | 0x80 | Ch1_msb | Ch1_lsb | Ch2_msb | Ch2_lsb | 0xff | 0xff |
       * +------+---------+---------+---------+---------+------+------+
       */
      case 0x80:
        let isOdd = numOctets % 2;
        let i;
        for (i = 0; i < numOctets - isOdd; i += 2) {
          let code = GsmPDUHelper.processHexToInt(
            value.slice(i * 2, i * 2 + 4),
            16
          );
          if (code == 0xffff) {
            i += 2;
            break;
          }
          str += String.fromCharCode(code);
        }

        break;
      case 0x81: // Fall through
      case 0x82:
        /**
         * +------+-----+--------+-----+-----+-----+--------+------+
         * | 0x81 | len | offset | Ch1 | Ch2 | ... | Ch_len | 0xff |
         * +------+-----+--------+-----+-----+-----+--------+------+
         *
         * len : The length of characters.
         * offset : 0hhh hhhh h000 0000
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         *
         * +------+-----+------------+------------+-----+-----+-----+--------+
         * | 0x82 | len | offset_msb | offset_lsb | Ch1 | Ch2 | ... | Ch_len |
         * +------+-----+------------+------------+-----+-----+-----+--------+
         *
         * len : The length of characters.
         * offset_msb, offset_lsn: offset
         * Ch_n: bit 8 = 0
         *       GSM default alphabets
         *       bit 8 = 1
         *       UCS2 character whose char code is (Ch_n & 0x7f) + offset
         */
        GsmPDUHelper.initWith(value);
        let len = GsmPDUHelper.readHexOctet();
        let offset, headerLen;
        if (scheme == 0x81) {
          offset = GsmPDUHelper.readHexOctet() << 7;
          headerLen = 2;
        } else {
          offset =
            (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
          headerLen = 3;
        }

        for (let i = 0; i < len; i++) {
          let ch = GsmPDUHelper.readHexOctet();
          if (ch & 0x80) {
            // UCS2
            str += String.fromCharCode((ch & 0x7f) + offset);
          } else {
            // GSM 8bit
            let count = 0,
              gotUCS2 = 0;
            while (i + count + 1 < len) {
              count++;
              if (GsmPDUHelper.readHexOctet() & 0x80) {
                GsmPDUHelper.seekIncoming(-1 * PDU_HEX_OCTET_SIZE);
                gotUCS2 = 1;
                break;
              }
            }
            // Unread.
            // +1 for the GSM alphabet indexed at i,
            //GsmPDUHelper.seekIncoming(-1 * (count + 1) * PDU_HEX_OCTET_SIZE);
            let gsm8bitValue = value.slice(GsmPDUHelper.pduReadIndex -(count * PDU_HEX_OCTET_SIZE));
            str += this.read8BitUnpackedToString(gsm8bitValue, count + 1 - gotUCS2);
            i += count - gotUCS2;
          }
        }

        // Skipping trailing 0xff
        GsmPDUHelper.seekIncoming(
          (numOctets - len - headerLen) * PDU_HEX_OCTET_SIZE
        );
        break;
    }
    return str;
  },

  /**
   * Read Alpha Id and Dialling number from TS TS 151.011 clause 10.5.1
   *
   * @param recordSize  The size of linear fixed record.
   */
  readAlphaIdDiallingNumber(options) {
    let recordSize = options.recordSize;
    let value = options.simResponse;
    let length = value.length;
    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    let alphaId = this.readAlphaIdentifier(value, alphaLen);

    // value = value - alphaLen's value
    let number_value = value.slice(alphaLen * 2);
    this.context.GsmPDUHelper.initWith(number_value);
    let number = this.readNumberWithLength();

    // the last recordSize byte is the extRecordNumber
    let extRecordNumber = this.context.GsmPDUHelper.processHexToInt(
      value.slice((recordSize - 1) * 2),
      16
    );

    let contact = null;
    if (alphaId || number) {
      contact = {
        alphaId,
        number,
        extRecordNumber,
      };
    }

    return contact;
  },

  /**
   * Write Alpha Identifier and Dialling number from TS 151.011 clause 10.5.1
   *
   * @param recordSize       The size of linear fixed record.
   * @param alphaId          Alpha Identifier to be written.
   * @param number           Dialling Number to be written.
   * @param extRecordNumber  The record identifier of the EXT.
   *
   * @return An object contains the alphaId and number
   *         that have been written into Buf.
   */
  writeAlphaIdDiallingNumber(recordSize, alphaId, number, extRecordNumber) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.pduWriteIndex = 0;
    GsmPDUHelper.pdu = "";

    let alphaLen = recordSize - ADN_FOOTER_SIZE_BYTES;
    let writtenAlphaId = this.writeAlphaIdentifier(alphaLen, alphaId);
    let writtenNumber = this.writeNumberWithLength(number);

    // Write unused CCP octet 0xff.
    GsmPDUHelper.writeHexOctet(0xff);
    GsmPDUHelper.writeHexOctet(
      extRecordNumber != null ? extRecordNumber : 0xff
    );

    return { alphaId: writtenAlphaId, number: writtenNumber };
  },

  /**
   * Read Alpha Identifier.
   *
   * @see TS 131.102
   *
   * @param numOctets
   *        Number of octets to be read.
   *
   * It uses either
   *  1. SMS default 7-bit alphabet with bit 8 set to 0.
   *  2. UCS2 string.
   *
   * Unused bytes should be set to 0xff.
   */
  readAlphaIdentifier(value, numOctets) {
    if (numOctets === 0) {
      return "";
    }
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let scheme = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
    // Read the 1st octet to determine the encoding.
    if (scheme == 0x80 || scheme == 0x81 || scheme == 0x82) {
      numOctets--;
      value = value.slice(2);
      let string = this.readICCUCS2String(scheme, value, numOctets);
      return string;
    }
    let string = this.read8BitUnpackedToString(value, numOctets);
    return string;
  },

  /**
   * Write Alpha Identifier.
   *
   * @param numOctets
   *        Total number of octets to be written. This includes the length of
   *        alphaId and the length of trailing unused octets(0xff).
   * @param alphaId
   *        Alpha Identifier to be written.
   *
   * @return The Alpha Identifier has been written into Buf.
   *
   * Unused octets will be written as 0xff.
   */
  writeAlphaIdentifier(numOctets, alphaId) {
    if (numOctets === 0) {
      return "";
    }

    // If alphaId is empty or it's of GSM 8 bit.
    if (!alphaId || this.context.ICCUtilsHelper.isGsm8BitAlphabet(alphaId)) {
      return this.writeStringTo8BitUnpacked(numOctets, alphaId);
    }
    return this.writeICCUCS2String(numOctets, alphaId);
  },

  /**
   * Read Dialling number.
   *
   * @see TS 131.102
   *
   * @param len
   *        The Length of BCD number.
   *
   * From TS 131.102, in EF_ADN, EF_FDN, the field 'Length of BCD number'
   * means the total bytes should be allocated to store the TON/NPI and
   * the dialing number.
   * For example, if the dialing number is 1234567890,
   * and the TON/NPI is 0x81,
   * The field 'Length of BCD number' should be 06, which is
   * 1 byte to store the TON/NPI, 0x81
   * 5 bytes to store the BCD number 2143658709.
   *
   * Here the definition of the length is different from SMS spec,
   * TS 23.040 9.1.2.5, which the length means
   * "number of useful semi-octets within the Address-Value field".
   */
  readDiallingNumber(len) {
    if (DEBUG) {
      this.context.debug("PDU: Going to read Dialling number: " + len);
    }
    if (len === 0) {
      return "";
    }

    let GsmPDUHelper = this.context.GsmPDUHelper;

    // TOA = TON + NPI. 1 byte.
    let toa = GsmPDUHelper.readHexOctet();
    let number = GsmPDUHelper.readSwappedNibbleExtendedBcdString(len - 1);
    if (number.length <= 0) {
      if (DEBUG) {
        this.context.debug("No number provided");
      }
      return "";
    }
    if (toa >> 4 == PDU_TOA_INTERNATIONAL >> 4) {
      number = "+" + number;
    }
    return number;
  },

  /**
   * Write Dialling Number.
   *
   * @param number  The Dialling number
   */
  writeDiallingNumber(number) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let toa = PDU_TOA_ISDN; // 81
    if (number[0] == "+") {
      toa = PDU_TOA_INTERNATIONAL | PDU_TOA_ISDN; // 91
      number = number.substring(1);
    }
    GsmPDUHelper.writeHexOctet(toa);
    GsmPDUHelper.writeSwappedNibbleBCD(number);
  },

  readNumberWithLength() {
    let number = "";
    let GsmPDUHelper = this.context.GsmPDUHelper;
    // 1 bytes number lenth.
    let numLen = GsmPDUHelper.readHexOctet();
    if (numLen != 0xff) {
      if (numLen > ADN_MAX_BCD_NUMBER_BYTES) {
        if (DEBUG) {
          this.context.debug(
            "Error: invalid length of BCD number/SSC contents - " + numLen
          );
        }
        return number;
      }
      number = this.readDiallingNumber(numLen);
    }

    return number;
  },

  /**
   * Write Number with Length
   *
   * @param number The value to be written.
   *
   * @return The number has been written into Buf.
   */
  writeNumberWithLength(number) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    if (number) {
      let numStart = number[0] == "+" ? 1 : 0;
      let writtenNumber =
        number.substring(0, numStart) +
        number.substring(numStart).replace(/[^0-9*#,]/g, "");
      let numDigits = writtenNumber.length - numStart;

      if (numDigits > ADN_MAX_NUMBER_DIGITS) {
        writtenNumber = writtenNumber.substring(
          0,
          ADN_MAX_NUMBER_DIGITS + numStart
        );
        numDigits = writtenNumber.length - numStart;
      }

      // +1 for TON/NPI
      let numLen = Math.ceil(numDigits / 2) + 1;
      GsmPDUHelper.writeHexOctet(numLen);
      this.writeDiallingNumber(
        writtenNumber
          .replace(/\*/g, "a")
          .replace(/\#/g, "b")
          .replace(/\,/g, "c")
      );
      // Write trailing 0xff of Dialling Number.
      for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES - numLen; i++) {
        GsmPDUHelper.writeHexOctet(0xff);
      }
      return writtenNumber;
    }
    // +1 for numLen
    for (let i = 0; i < ADN_MAX_BCD_NUMBER_BYTES + 1; i++) {
      GsmPDUHelper.writeHexOctet(0xff);
    }
    return "";
  },
};

/**
 * Helper for ICC Contacts.
 */
function ICCContactHelperObject(aContext) {
  this.context = aContext;
}
ICCContactHelperObject.prototype = {
  context: null,

  /**
   * Helper function to check DF_PHONEBOOK.
   */
  hasDfPhoneBook(appType) {
    switch (appType) {
      case CARD_APPTYPE_SIM:
        return false;
      case CARD_APPTYPE_USIM:
        return true;
      case CARD_APPTYPE_RUIM:
        let ICCUtilsHelper = this.context.ICCUtilsHelper;
        return ICCUtilsHelper.isICCServiceAvailable("ENHANCED_PHONEBOOK");
      default:
        return false;
    }
  },

  /**
   * Helper function to read ICC contacts.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   One of GECKO_CARDCONTACT_TYPE_*.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readICCContacts(appType, contactType, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let ICCUtilsHelper = this.context.ICCUtilsHelper;

    switch (contactType) {
      case GECKO_CARDCONTACT_TYPE_ADN:
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.readADNLike(
            ICC_EF_ADN,
            ICCUtilsHelper.isICCServiceAvailable("EXT1") ? ICC_EF_EXT1 : null,
            onsuccess,
            onerror
          );
        } else {
          this.readUSimContacts(onsuccess, onerror);
        }
        break;
      case GECKO_CARDCONTACT_TYPE_FDN:
        if (!ICCUtilsHelper.isICCServiceAvailable("FDN")) {
          onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
          break;
        }
        ICCRecordHelper.readADNLike(
          ICC_EF_FDN,
          ICCUtilsHelper.isICCServiceAvailable("EXT2") ? ICC_EF_EXT2 : null,
          onsuccess,
          onerror
        );
        break;
      case GECKO_CARDCONTACT_TYPE_SDN:
        if (!ICCUtilsHelper.isICCServiceAvailable("SDN")) {
          onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
          break;
        }

        ICCRecordHelper.readADNLike(
          ICC_EF_SDN,
          ICCUtilsHelper.isICCServiceAvailable("EXT3") ? ICC_EF_EXT3 : null,
          onsuccess,
          onerror
        );
        break;
      default:
        if (DEBUG) {
          this.context.debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

  getMaxContactCount(appType, contactType, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let ICCUtilsHelper = this.context.ICCUtilsHelper;
    switch (contactType) {
      case GECKO_CARDCONTACT_TYPE_ADN:
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.readRecordCount(ICC_EF_ADN, onsuccess, onerror);
        } else {
          let gotPbrCb = function gotPbrCb(pbrs) {
            this.readAllPbrRecordCount(pbrs, onsuccess, onerror);
          }.bind(this);

          let gotPbrErrCb = function gotPbrErrCb() {
            ICCRecordHelper.readRecordCount(ICC_EF_ADN, onsuccess, onerror);
          };
          this.context.ICCRecordHelper.readPBR(gotPbrCb, gotPbrErrCb);
        }
        break;
      default:
        if (DEBUG) {
          this.context.debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
    }
  },

  /**
   * Helper function to find free contact record.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   One of GECKO_CARDCONTACT_TYPE_*.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  findFreeICCContact(appType, contactType, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    switch (contactType) {
      case GECKO_CARDCONTACT_TYPE_ADN:
        if (!this.hasDfPhoneBook(appType)) {
          ICCRecordHelper.findFreeRecordId(
            ICC_EF_ADN,
            onsuccess.bind(null, 0),
            onerror
          );
        } else {
          let gotPbrCb = function gotPbrCb(pbrs) {
            this.findUSimFreeADNRecordId(pbrs, onsuccess, onerror);
          }.bind(this);

          let gotPbrErrCb = function gotPbrErrCb() {
            if (DEBUG) {
              this.context.debug("findFreeICCContact gotPbrErrCb");
            }
            ICCRecordHelper.findFreeRecordId(
              ICC_EF_ADN,
              onsuccess.bind(null, 0),
              onerror
            );
          }.bind(this);

          ICCRecordHelper.readPBR(gotPbrCb, gotPbrErrCb);
        }
        break;
      case GECKO_CARDCONTACT_TYPE_FDN:
        ICCRecordHelper.findFreeRecordId(
          ICC_EF_FDN,
          onsuccess.bind(null, 0),
          onerror
        );
        break;
      default:
        if (DEBUG) {
          this.context.debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Cache the pbr index of the possible free record.
   */
  _freePbrIndex: 0,

  /**
   * Find free ADN record id in USIM.
   *
   * @param pbrs          All Phonebook Reference Files read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  findUSimFreeADNRecordId(pbrs, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;

    function callback(pbrIndex, recordId) {
      // Assume other free records are probably in the same phonebook set.
      this._freePbrIndex = pbrIndex;
      onsuccess(pbrIndex, recordId);
    }

    let nextPbrIndex = -1;
    (function findFreeRecordId(pbrIndex) {
      if (nextPbrIndex === this._freePbrIndex) {
        // No free record found, reset the pbr index of free record.
        this._freePbrIndex = 0;
        if (DEBUG) {
          this.context.debug(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        }
        onerror(CONTACT_ERR_NO_FREE_RECORD_FOUND);
        return;
      }

      let pbr = pbrs[pbrIndex];
      nextPbrIndex = (pbrIndex + 1) % pbrs.length;
      ICCRecordHelper.findFreeRecordId(
        pbr.adn.fileId,
        callback.bind(this, pbrIndex),
        findFreeRecordId.bind(this, nextPbrIndex)
      );
    }.call(this, this._freePbrIndex));
  },

  /**
   * Helper function to add a new ICC contact.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   One of GECKO_CARDCONTACT_TYPE_*.
   * @param contact       The contact will be added.
   * @param pin2          PIN2 is required for FDN.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  addICCContact(appType, contactType, contact, pin2, onsuccess, onerror) {
    let foundFreeCb = function foundFreeCb(pbrIndex, recordId) {
      contact.pbrIndex = pbrIndex;
      contact.recordId = recordId;
      this.updateICCContact(
        appType,
        contactType,
        contact,
        pin2,
        onsuccess,
        onerror
      );
    }.bind(this);

    // Find free record first.
    this.findFreeICCContact(appType, contactType, foundFreeCb, onerror);
  },

  /**
   * Helper function to update ICC contact.
   *
   * @param appType       One of CARD_APPTYPE_*.
   * @param contactType   One of GECKO_CARDCONTACT_TYPE_*.
   * @param contact       The contact will be updated.
   * @param pin2          PIN2 is required for FDN.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateICCContact(appType, contactType, contact, pin2, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let ICCUtilsHelper = this.context.ICCUtilsHelper;

    let updateContactCb = updatedContact => {
      updatedContact.pbrIndex = contact.pbrIndex;
      updatedContact.recordId = contact.recordId;
      onsuccess(updatedContact);
    };

    switch (contactType) {
      case GECKO_CARDCONTACT_TYPE_ADN:
        if (!this.hasDfPhoneBook(appType)) {
          if (ICCUtilsHelper.isICCServiceAvailable("EXT1")) {
            this.updateADNLikeWithExtension(
              ICC_EF_ADN,
              ICC_EF_EXT1,
              contact,
              null,
              updateContactCb,
              onerror
            );
          } else {
            ICCRecordHelper.updateADNLike(
              ICC_EF_ADN,
              0xff,
              contact,
              null,
              updateContactCb,
              onerror
            );
          }
        } else {
          this.updateUSimContact(contact, updateContactCb, onerror);
        }
        break;
      case GECKO_CARDCONTACT_TYPE_FDN:
        if (!pin2) {
          onerror(GECKO_ERROR_SIM_PIN2);
          return;
        }
        if (!ICCUtilsHelper.isICCServiceAvailable("FDN")) {
          onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
          break;
        }
        if (ICCUtilsHelper.isICCServiceAvailable("EXT2")) {
          this.updateADNLikeWithExtension(
            ICC_EF_FDN,
            ICC_EF_EXT2,
            contact,
            pin2,
            updateContactCb,
            onerror
          );
        } else {
          ICCRecordHelper.updateADNLike(
            ICC_EF_FDN,
            0xff,
            contact,
            pin2,
            updateContactCb,
            onerror
          );
        }
        break;
      default:
        if (DEBUG) {
          this.context.debug("Unsupported contactType :" + contactType);
        }
        onerror(CONTACT_ERR_CONTACT_TYPE_NOT_SUPPORTED);
        break;
    }
  },

  /**
   * Read contacts from USIM.
   *
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readUSimContacts(onsuccess, onerror) {
    let gotPbrCb = function gotPbrCb(pbrs) {
      this.readAllPhonebookSets(pbrs, onsuccess, onerror);
    }.bind(this);

    let gotPbrErrCb = function gotPbrErrCb() {
      if (DEBUG) {
        this.context.debug("readUSimContacts gotPbrErrCb");
      }
      this.context.ICCRecordHelper.readADNLike(
        ICC_EF_ADN,
        this.context.ICCUtilsHelper.isICCServiceAvailable("EXT1")
          ? ICC_EF_EXT1
          : null,
        onsuccess,
        onerror
      );
    }.bind(this);

    this.context.ICCRecordHelper.readPBR(gotPbrCb, gotPbrErrCb);
  },

  /**
   * Read all Phonebook sets.
   *
   * @param pbrs          All Phonebook Reference Files read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readAllPhonebookSets(pbrs, onsuccess, onerror) {
    let allContacts = [],
      pbrIndex = 0;
    let readPhonebook = function(contacts) {
      if (contacts) {
        allContacts = allContacts.concat(contacts);
      }

      let cLen = contacts ? contacts.length : 0;
      for (let i = 0; i < cLen; i++) {
        contacts[i].pbrIndex = pbrIndex;
      }

      pbrIndex++;
      if (pbrIndex >= pbrs.length) {
        if (onsuccess) {
          onsuccess(allContacts);
        }
        return;
      }

      this.readPhonebookSet(pbrs[pbrIndex], readPhonebook, onerror);
    }.bind(this);

    this.readPhonebookSet(pbrs[pbrIndex], readPhonebook, onerror);
  },

  readAllPbrRecordCount(pbrs, onsuccess, onerror) {
    let totalRecords = 0,
      pbrIndex = 0;
    let readPhoneBook = function(aTotalRecord) {
      totalRecords += aTotalRecord;
      pbrIndex++;
      if (pbrIndex >= pbrs.length) {
        if (onsuccess) {
          onsuccess(totalRecords);
        }
        return;
      }

      this.readPerPbrRecordCount(pbrs[pbrIndex], readPhoneBook, onerror);
    }.bind(this);

    this.readPerPbrRecordCount(pbrs[pbrIndex], readPhoneBook, onerror);
  },

  readPerPbrRecordCount(pbr, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    ICCRecordHelper.readRecordCount(pbr.adn.fileId, onsuccess, onerror);
  },

  /**
   * Read from Phonebook Reference File.
   *
   * @param pbr           Phonebook Reference File to be read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readPhonebookSet(pbr, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let gotAdnCb = function gotAdnCb(contacts) {
      this.readSupportedPBRFields(pbr, contacts, onsuccess, onerror);
    }.bind(this);

    ICCRecordHelper.readADNLike(
      pbr.adn.fileId,
      pbr.ext1 ? pbr.ext1.fileId : null,
      gotAdnCb,
      onerror
    );
  },

  /**
   * Read supported Phonebook fields.
   *
   * @param pbr         Phone Book Reference file.
   * @param contacts    Contacts stored on ICC.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  readSupportedPBRFields(pbr, contacts, onsuccess, onerror) {
    let fieldIndex = 0;
    (function readField() {
      let field = USIM_PBR_FIELDS[fieldIndex];
      fieldIndex += 1;
      if (!field) {
        if (onsuccess) {
          onsuccess(contacts);
        }
        return;
      }

      this.readPhonebookField(
        pbr,
        contacts,
        field,
        readField.bind(this),
        onerror
      );
    }.call(this));
  },

  /**
   * Read Phonebook field.
   *
   * @param pbr           The phonebook reference file.
   * @param contacts      Contacts stored on ICC.
   * @param field         Phonebook field to be retrieved.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readPhonebookField(pbr, contacts, field, onsuccess, onerror) {
    if (!pbr[field]) {
      if (onsuccess) {
        onsuccess(contacts);
      }
      return;
    }

    (function doReadContactField(n) {
      if (n >= contacts.length) {
        // All contact's fields are read.
        if (onsuccess) {
          onsuccess(contacts);
        }
        return;
      }

      // get n-th contact's field.
      this.readContactField(
        pbr,
        contacts[n],
        field,
        doReadContactField.bind(this, n + 1),
        onerror
      );
    }.call(this, 0));
  },

  /**
   * Read contact's field from USIM.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to get field.
   * @param field         Phonebook field to be retrieved.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  readContactField(pbr, contact, field, onsuccess, onerror) {
    let gotRecordIdCb = function gotRecordIdCb(recordId) {
      if (recordId == 0xff) {
        if (onsuccess) {
          onsuccess();
        }
        return;
      }

      let fileId = pbr[field].fileId;
      let fileType = pbr[field].fileType;
      let gotFieldCb = function gotFieldCb(value) {
        if (value) {
          // Move anr0 anr1,.. into anr[].
          if (field.startsWith(USIM_PBR_ANR)) {
            if (!contact[USIM_PBR_ANR]) {
              contact[USIM_PBR_ANR] = [];
            }
            contact[USIM_PBR_ANR].push(value);
          } else {
            contact[field] = value;
          }
        }

        if (onsuccess) {
          onsuccess();
        }
      };

      let ICCRecordHelper = this.context.ICCRecordHelper;
      // Detect EF to be read, for anr, it could have anr0, anr1,...
      let ef = field.startsWith(USIM_PBR_ANR) ? USIM_PBR_ANR : field;
      switch (ef) {
        case USIM_PBR_EMAIL:
          ICCRecordHelper.readEmail(
            fileId,
            fileType,
            recordId,
            gotFieldCb,
            onerror
          );
          break;
        case USIM_PBR_ANR:
          ICCRecordHelper.readANR(
            fileId,
            fileType,
            recordId,
            gotFieldCb,
            onerror
          );
          break;
        default:
          if (DEBUG) {
            this.context.debug("Unsupported field :" + field);
          }
          onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
          break;
      }
    }.bind(this);

    this.getContactFieldRecordId(pbr, contact, field, gotRecordIdCb, onerror);
  },

  /**
   * Get the recordId.
   *
   * If the fileType of field is ICC_USIM_TYPE1_TAG, use corresponding ADN recordId.
   * otherwise get the recordId from IAP.
   *
   * @see TS 131.102, clause 4.4.2.2
   *
   * @param pbr          The phonebook reference file.
   * @param contact      The contact will be updated.
   * @param onsuccess    Callback to be called when success.
   * @param onerror      Callback to be called when error.
   */
  getContactFieldRecordId(pbr, contact, field, onsuccess, onerror) {
    if (pbr[field].fileType == ICC_USIM_TYPE1_TAG) {
      // If the file type is ICC_USIM_TYPE1_TAG, use corresponding ADN recordId.
      if (onsuccess) {
        onsuccess(contact.recordId);
      }
    } else if (pbr[field].fileType == ICC_USIM_TYPE2_TAG) {
      // If the file type is ICC_USIM_TYPE2_TAG, the recordId shall be got from IAP.
      let gotIapCb = function gotIapCb(iap) {
        let indexInIAP = pbr[field].indexInIAP;
        let recordId = iap[indexInIAP];

        if (onsuccess) {
          onsuccess(recordId);
        }
      };

      this.context.ICCRecordHelper.readIAP(
        pbr.iap.fileId,
        contact.recordId,
        gotIapCb,
        onerror
      );
    } else {
      if (DEBUG) {
        this.context.debug(
          "USIM PBR files in Type 3 format are not supported."
        );
      }
      onerror(CONTACT_ERR_REQUEST_NOT_SUPPORTED);
    }
  },

  /**
   * Update USIM contact.
   *
   * @param contact       The contact will be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateUSimContact(contact, onsuccess, onerror) {
    let updateContactCb = updatedContact => {
      updatedContact.pbrIndex = contact.pbrIndex;
      updatedContact.recordId = contact.recordId;
      onsuccess(updatedContact);
    };

    let gotPbrCb = function gotPbrCb(pbrs) {
      let pbr = pbrs[contact.pbrIndex];
      if (!pbr) {
        if (DEBUG) {
          this.context.debug(CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);
        }
        onerror(CONTACT_ERR_CANNOT_ACCESS_PHONEBOOK);
        return;
      }
      this.updatePhonebookSet(pbr, contact, onsuccess, onerror);
    }.bind(this);

    let gotPbrErrCb = function gotPbrErrCb() {
      if (DEBUG) {
        this.context.debug("updateUSimContact gotPbrErrCb");
      }
      if (this.context.ICCUtilsHelper.isICCServiceAvailable("EXT1")) {
        this.updateADNLikeWithExtension(
          ICC_EF_ADN,
          ICC_EF_EXT1,
          contact,
          null,
          updateContactCb,
          onerror
        );
      } else {
        this.context.ICCRecordHelper.updateADNLike(
          ICC_EF_ADN,
          0xff,
          contact,
          null,
          updateContactCb,
          onerror
        );
      }
    }.bind(this);

    this.context.ICCRecordHelper.readPBR(gotPbrCb, gotPbrErrCb);
  },

  /**
   * Update fields in Phonebook Reference File.
   *
   * @param pbr           Phonebook Reference File to be read.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updatePhonebookSet(pbr, contact, onsuccess, onerror) {
    let updateAdnCb = function(updatedContact) {
      this.updateSupportedPBRFields(
        pbr,
        contact,
        updatedContactField => {
          onsuccess(Object.assign(updatedContact, updatedContactField));
        },
        onerror
      );
    }.bind(this);

    if (pbr.ext1) {
      this.updateADNLikeWithExtension(
        pbr.adn.fileId,
        pbr.ext1.fileId,
        contact,
        null,
        updateAdnCb,
        onerror
      );
    } else {
      this.context.ICCRecordHelper.updateADNLike(
        pbr.adn.fileId,
        0xff,
        contact,
        null,
        updateAdnCb,
        onerror
      );
    }
  },

  /**
   * Update supported Phonebook fields.
   *
   * @param pbr         Phone Book Reference file.
   * @param contact     Contact to be updated.
   * @param onsuccess   Callback to be called when success.
   * @param onerror     Callback to be called when error.
   */
  updateSupportedPBRFields(pbr, contact, onsuccess, onerror) {
    let fieldIndex = 0;
    let contactField = {};

    (function updateField() {
      let field = USIM_PBR_FIELDS[fieldIndex];
      fieldIndex += 1;

      if (!field) {
        if (onsuccess) {
          onsuccess(contactField);
        }
        return;
      }

      // Check if PBR has this field.
      if (!pbr[field]) {
        updateField.call(this);
        return;
      }

      this.updateContactField(
        pbr,
        contact,
        field,
        fieldEntry => {
          contactField = Object.assign(contactField, fieldEntry);
          updateField.call(this);
        },
        errorMsg => {
          // Bug 1194149, there are some sim cards without sufficient
          // Type 2 USIM contact fields record. We allow user continue
          // importing contacts.
          if (errorMsg === CONTACT_ERR_NO_FREE_RECORD_FOUND) {
            updateField.call(this);
            return;
          }
          onerror(errorMsg);
        }
      );
    }.call(this));
  },

  /**
   * Update contact's field from USIM.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactField(pbr, contact, field, onsuccess, onerror) {
    if (pbr[field].fileType === ICC_USIM_TYPE1_TAG) {
      this.updateContactFieldType1(pbr, contact, field, onsuccess, onerror);
    } else if (pbr[field].fileType === ICC_USIM_TYPE2_TAG) {
      this.updateContactFieldType2(pbr, contact, field, onsuccess, onerror);
    } else {
      if (DEBUG) {
        this.context.debug(
          "USIM PBR files in Type 3 format are not supported."
        );
      }
      onerror(CONTACT_ERR_REQUEST_NOT_SUPPORTED);
    }
  },

  /**
   * Update Type 1 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactFieldType1(pbr, contact, field, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;

    if (field === USIM_PBR_EMAIL) {
      ICCRecordHelper.updateEmail(
        pbr,
        contact.recordId,
        contact.email,
        null,
        updatedEmail => {
          onsuccess({ email: updatedEmail });
        },
        onerror
      );
    } else if (field === USIM_PBR_ANR0) {
      let anr = Array.isArray(contact.anr) ? contact.anr[0] : null;
      ICCRecordHelper.updateANR(
        pbr,
        contact.recordId,
        anr,
        null,
        updatedANR => {
          // ANR could have multiple files. If we support more than one anr,
          // we will save it as anr0, anr1,...etc.
          onsuccess(updatedANR ? { anr: [updatedANR] } : null);
        },
        onerror
      );
    } else {
      if (DEBUG) {
        this.context.debug("Unsupported field :" + field);
      }
      onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
    }
  },

  /**
   * Update Type 2 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  updateContactFieldType2(pbr, contact, field, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;

    // Case 1 : EF_IAP[adnRecordId] doesn't have a value(0xff)
    //   Find a free recordId for EF_field
    //   Update field with that free recordId.
    //   Update IAP.
    //
    // Case 2: EF_IAP[adnRecordId] has a value
    //   update EF_field[iap[field.indexInIAP]]

    let gotIapCb = function gotIapCb(iap) {
      let recordId = iap[pbr[field].indexInIAP];
      if (recordId === 0xff) {
        // If the value in IAP[index] is 0xff, which means the contact stored on
        // the SIM doesn't have the additional attribute (email or anr).
        // So if the contact to be updated doesn't have the attribute either,
        // we don't have to update it.
        if (
          (field === USIM_PBR_EMAIL && contact.email) ||
          (field === USIM_PBR_ANR0 &&
            Array.isArray(contact.anr) &&
            contact.anr[0])
        ) {
          // Case 1.
          this.addContactFieldType2(pbr, contact, field, onsuccess, onerror);
        } else if (onsuccess) {
          onsuccess();
        }
        return;
      }

      // Case 2.
      if (field === USIM_PBR_EMAIL) {
        ICCRecordHelper.updateEmail(
          pbr,
          recordId,
          contact.email,
          contact.recordId,
          updatedEmail => {
            onsuccess({ email: updatedEmail });
          },
          onerror
        );
      } else if (field === USIM_PBR_ANR0) {
        let anr = Array.isArray(contact.anr) ? contact.anr[0] : null;
        ICCRecordHelper.updateANR(
          pbr,
          recordId,
          anr,
          contact.recordId,
          updatedANR => {
            // ANR could have multiple files. If we support more than one anr,
            // we will save it as anr0, anr1,...etc.
            onsuccess(updatedANR ? { anr: [updatedANR] } : null);
          },
          onerror
        );
      } else {
        if (DEBUG) {
          this.context.debug("Unsupported field :" + field);
        }
        onerror(CONTACT_ERR_FIELD_NOT_SUPPORTED);
      }
    }.bind(this);

    ICCRecordHelper.readIAP(
      pbr.iap.fileId,
      contact.recordId,
      gotIapCb,
      onerror
    );
  },

  /**
   * Add Type 2 USIM contact fields.
   *
   * @param pbr           The phonebook reference file.
   * @param contact       The contact needs to be updated.
   * @param field         Phonebook field to be updated.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   */
  addContactFieldType2(pbr, contact, field, onsuccess, onerror) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let successCb = function successCb(recordId) {
      let updateCb = function updateCb(contactField) {
        this.updateContactFieldIndexInIAP(
          pbr,
          contact.recordId,
          field,
          recordId,
          () => {
            onsuccess(contactField);
          },
          onerror
        );
      }.bind(this);

      if (field === USIM_PBR_EMAIL) {
        ICCRecordHelper.updateEmail(
          pbr,
          recordId,
          contact.email,
          contact.recordId,
          updatedEmail => {
            updateCb({ email: updatedEmail });
          },
          onerror
        );
      } else if (field === USIM_PBR_ANR0) {
        ICCRecordHelper.updateANR(
          pbr,
          recordId,
          contact.anr[0],
          contact.recordId,
          updatedANR => {
            // ANR could have multiple files. If we support more than one anr,
            // we will save it as anr0, anr1,...etc.
            updateCb(updatedANR ? { anr: [updatedANR] } : null);
          },
          onerror
        );
      }
    }.bind(this);

    let errorCb = function errorCb(errorMsg) {
      if (DEBUG) {
        this.context.debug(errorMsg + " USIM field " + field);
      }
      onerror(errorMsg);
    }.bind(this);

    ICCRecordHelper.findFreeRecordId(pbr[field].fileId, successCb, errorCb);
  },

  /**
   * Update IAP value.
   *
   * @param pbr           The phonebook reference file.
   * @param recordNumber  The record identifier of EF_IAP.
   * @param field         Phonebook field.
   * @param value         The value of 'field' in IAP.
   * @param onsuccess     Callback to be called when success.
   * @param onerror       Callback to be called when error.
   *
   */
  updateContactFieldIndexInIAP(
    pbr,
    recordNumber,
    field,
    value,
    onsuccess,
    onerror
  ) {
    let ICCRecordHelper = this.context.ICCRecordHelper;

    let gotIAPCb = function gotIAPCb(iap) {
      iap[pbr[field].indexInIAP] = value;
      ICCRecordHelper.updateIAP(
        pbr.iap.fileId,
        recordNumber,
        iap,
        onsuccess,
        onerror
      );
    };
    ICCRecordHelper.readIAP(pbr.iap.fileId, recordNumber, gotIAPCb, onerror);
  },

  /**
   * Update ICC ADN like EFs with Extension, like EF_ADN, EF_FDN.
   *
   * @param  fileId    EF id of the ADN or FDN.
   * @param  extFileId EF id of the EXT.
   * @param  contact   The contact will be updated. (Shall have recordId property)
   * @param  pin2      PIN2 is required when updating ICC_EF_FDN.
   * @param  onsuccess Callback to be called when success.
   * @param  onerror   Callback to be called when error.
   */
  updateADNLikeWithExtension(
    fileId,
    extFileId,
    contact,
    pin2,
    onsuccess,
    onerror
  ) {
    let ICCRecordHelper = this.context.ICCRecordHelper;
    let extNumber;

    if (contact.number) {
      let numStart = contact.number[0] == "+" ? 1 : 0;
      let number =
        contact.number.substring(0, numStart) +
        this.context.GsmPDUHelper.stringToExtendedBcd(
          contact.number.substring(numStart)
        );
      extNumber = number.substr(
        numStart + ADN_MAX_NUMBER_DIGITS,
        EXT_MAX_NUMBER_DIGITS
      );
    }

    ICCRecordHelper.getADNLikeExtensionRecordNumber(
      fileId,
      contact.recordId,
      extRecordNumber => {
        let updateADNLike = extRecordNumber => {
          ICCRecordHelper.updateADNLike(
            fileId,
            extRecordNumber,
            contact,
            pin2,
            updatedContact => {
              if (extNumber && extRecordNumber != 0xff) {
                updatedContact.number = updatedContact.number.concat(extNumber);
              }
              onsuccess(updatedContact);
            },
            onerror
          );
        };

        let updateExtension = extRecordNumber => {
          ICCRecordHelper.updateExtension(
            extFileId,
            extRecordNumber,
            extNumber,
            () => updateADNLike(extRecordNumber),
            () => updateADNLike(0xff)
          );
        };

        if (extNumber) {
          if (extRecordNumber != 0xff) {
            updateExtension(extRecordNumber);
            return;
          }

          ICCRecordHelper.findFreeRecordId(
            extFileId,
            extRecordNumber => updateExtension(extRecordNumber),
            errorMsg => {
              if (DEBUG) {
                this.context.debug(
                  "Couldn't find free extension record Id for " +
                    extFileId +
                    ": " +
                    errorMsg
                );
              }
              updateADNLike(0xff);
            }
          );
          return;
        }

        if (extRecordNumber != 0xff) {
          ICCRecordHelper.cleanEFRecord(
            extFileId,
            extRecordNumber,
            () => updateADNLike(0xff),
            onerror
          );
          return;
        }

        updateADNLike(0xff);
      },
      onerror
    );
  },
};

function BerTlvHelperObject(aContext) {
  this.context = aContext;
}
BerTlvHelperObject.prototype = {
  context: null,

  /**
   * Decode Ber TLV.
   *
   * @param dataLen
   *        The length of data in bytes.
   */
  decode(value) {
    let dataLen = value.length / 2;
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let hlen = 0;
    // data[0] data[1] for tag
    let tag = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
    hlen++;

    // The length is coded onto 1 or 2 bytes.
    // Length    | Byte 1                | Byte 2
    // 0 - 127   | length ('00' to '7f') | N/A
    // 128 - 255 | '81'                  | length ('80' to 'ff')
    let length;
    // data[2] data[3] for temp
    let temp = GsmPDUHelper.processHexToInt(value.slice(2, 4), 16);
    hlen++;
    if (temp < 0x80) {
      length = temp;
    } else if (temp === 0x81) {
      length = GsmPDUHelper.processHexToInt(value.slice(4, 6), 16);
      hlen++;
      if (length < 0x80) {
        throw new Error("Invalid length " + length);
      }
    } else {
      throw new Error("Invalid length octet " + temp);
    }

    // Header + body length check.
    if (dataLen - hlen !== length) {
      throw new Error("Unexpected BerTlvHelper value length!!");
    }

    let method = this[tag];
    if (typeof method != "function") {
      throw new Error("Unknown Ber tag 0x" + tag.toString(16));
    }

    // Remove the tag and size value.
    value = value.slice(hlen * PDU_HEX_OCTET_SIZE);

    let decodeValue = method.call(this, value, length);
    return {
      tag,
      length,
      value: decodeValue,
    };
  },

  /**
   * Process the value part for FCP template TLV.
   *
   * @param length
   *        The length of data in bytes.
   */
  processFcpTemplate(value, length) {
    let tlvs = this.decodeChunks(value, length);
    return tlvs;
  },

  /**
   * Process the value part for proactive command TLV.
   *
   * @param length
   *        The length of data in bytes.
   */
  processProactiveCommand(value, length) {
    let ctlvs = this.context.ComprehensionTlvHelper.decodeChunks(value, length);
    return ctlvs;
  },

  /**
   * Decode raw data to a Ber-TLV.
   */
  decodeInnerTlv(value) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let tag = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
    let length = GsmPDUHelper.processHexToInt(value.slice(2, 4), 16);

    // Remove the tag and size value.
    value = value.slice(2 * PDU_HEX_OCTET_SIZE);
    let decodeValue = this.retrieve(tag, value, length);
    return {
      tag,
      length,
      value: decodeValue,
    };
  },

  decodeChunks(value, length) {
    let chunks = [];
    let index = 0;
    while (index < length) {
      let tlv = this.decodeInnerTlv(value);
      if (tlv.value) {
        chunks.push(tlv);
      }
      index += tlv.length;
      // tag + length fields consume 2 bytes.
      index += 2;
    }
    return chunks;
  },

  //FIXME: value slice should be a problem that could not modify input value
  retrieve(tag, value, length) {
    let method = this[tag];
    if (typeof method != "function") {
      if (DEBUG) {
        this.context.debug("Unknown Ber tag : 0x" + tag.toString(16));
      }

      value = value.slice(length * PDU_HEX_OCTET_SIZE);
      return null;
    }
    return method.call(this, value, length);
  },

  /**
   * File Size Data.
   *
   * | Byte        | Description                                | Length |
   * |  1          | Tag                                        |   1    |
   * |  2          | Length                                     |   1    |
   * |  3 to X+24  | Number of allocated data bytes in the file |   X    |
   * |             | , excluding structural information         |        |
   */
  retrieveFileSizeData(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let fileSizeData = 0;
    for (let i = 0; i < length; i++) {
      fileSizeData = fileSizeData << 8;
      fileSizeData += GsmPDUHelper.readHexOctet();
    }

    return { fileSizeData };
  },

  /**
   * File Descriptor.
   *
   * | Byte    | Description           | Length |
   * |  1      | Tag                   |   1    |
   * |  2      | Length                |   1    |
   * |  3      | File descriptor byte  |   1    |
   * |  4      | Data coding byte      |   1    |
   * |  5 ~ 6  | Record length         |   2    |
   * |  7      | Number of records     |   1    |
   */
  retrieveFileDescriptor(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let fileDescriptorByte = GsmPDUHelper.readHexOctet();
    let dataCodingByte = GsmPDUHelper.readHexOctet();
    // See TS 102 221 Table 11.5, we only care the least 3 bits for the
    // structure of file.
    let fileStructure = fileDescriptorByte & 0x07;

    let fileDescriptor = {
      fileStructure,
    };
    // byte 5 ~ 7 are mandatory for linear fixed and cyclic files, otherwise
    // they are not applicable.
    if (
      fileStructure === UICC_EF_STRUCTURE[EF_STRUCTURE_LINEAR_FIXED] ||
      fileStructure === UICC_EF_STRUCTURE[EF_STRUCTURE_CYCLIC]
    ) {
      fileDescriptor.recordLength =
        (GsmPDUHelper.readHexOctet() << 8) + GsmPDUHelper.readHexOctet();
      fileDescriptor.numOfRecords = GsmPDUHelper.readHexOctet();
    }

    return fileDescriptor;
  },

  /**
   * File identifier.
   *
   * | Byte    | Description      | Length |
   * |  1      | Tag              |   1    |
   * |  2      | Length           |   1    |
   * |  3 ~ 4  | File identifier  |   2    |
   */
  retrieveFileIdentifier(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    return {
      fileId: (GsmPDUHelper.readHexOctet() << 8) + GsmPDUHelper.readHexOctet(),
    };
  },

  searchForNextTag(tag, iter) {
    for (let [index, tlv] in iter) {
      if (tlv.tag === tag) {
        return tlv;
      }
    }
    return null;
  },
};
BerTlvHelperObject.prototype[
  BER_FCP_TEMPLATE_TAG
] = function BER_FCP_TEMPLATE_TAG(value, length) {
  return this.processFcpTemplate(value, length);
};
BerTlvHelperObject.prototype[
  BER_PROACTIVE_COMMAND_TAG
] = function BER_PROACTIVE_COMMAND_TAG(value, length) {
  return this.processProactiveCommand(value, length);
};
BerTlvHelperObject.prototype[
  BER_FCP_FILE_SIZE_DATA_TAG
] = function BER_FCP_FILE_SIZE_DATA_TAG(value, length) {
  return this.retrieveFileSizeData(value, length);
};
BerTlvHelperObject.prototype[
  BER_FCP_FILE_DESCRIPTOR_TAG
] = function BER_FCP_FILE_DESCRIPTOR_TAG(value, length) {
  return this.retrieveFileDescriptor(value, length);
};
BerTlvHelperObject.prototype[
  BER_FCP_FILE_IDENTIFIER_TAG
] = function BER_FCP_FILE_IDENTIFIER_TAG(value, length) {
  return this.retrieveFileIdentifier(value, length);
};

function ComprehensionTlvHelperObject(aContext) {
  this.context = aContext;
}
ComprehensionTlvHelperObject.prototype = {
  context: null,

  /**
   * Decode raw data to a Comprehension-TLV.
   */
  decode(value) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let hlen = 0; // For header(tag field + length field) length.
    let temp = GsmPDUHelper.processHexToInt(value.slice(0, 2), 16);
    hlen++;

    // TS 101.220, clause 7.1.1
    let tag, cr;
    switch (temp) {
      // TS 101.220, clause 7.1.1
      case 0x0: // Not used.
      case 0xff: // Not used.
      case 0x80: // Reserved for future use.
        throw new Error(
          "Invalid octet when parsing Comprehension TLV :" + temp
        );
      case 0x7f: // Tag is three byte format.
        // TS 101.220 clause 7.1.1.2.
        // | Byte 1 | Byte 2                        | Byte 3 |
        // |        | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |        |
        // | 0x7f   |CR | Tag Value                          |
        //tag = (GsmPDUHelper.readHexOctet() << 8) | GsmPDUHelper.readHexOctet();
        tag = GsmPDUHelper.processHexToInt(value.slice(2, 6), 16);
        hlen += 2;
        cr = (tag & 0x8000) !== 0;
        tag &= ~0x8000;
        break;
      default:
        // Tag is single byte format.
        tag = temp;
        // TS 101.220 clause 7.1.1.1.
        // | 8 | 7 | 6 | 5 | 4 | 3 | 2 | 1 |
        // |CR | Tag Value                 |
        cr = (tag & 0x80) !== 0;
        tag &= ~0x80;
    }

    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    //
    // Length errors: TS 11.14, clause 6.10.6

    let length; // Data length.
    temp = GsmPDUHelper.processHexToInt(
      value.slice(hlen * 2, hlen * 2 + 2),
      16
    );
    hlen++;
    if (temp < 0x80) {
      length = temp;
    } else if (temp == 0x81) {
      length = GsmPDUHelper.processHexToInt(
        value.slice(hlen * 2, hlen * 2 + 2),
        16
      );
      hlen++;
      if (length < 0x80) {
        throw new Error("Invalid length in Comprehension TLV :" + length);
      }
    } else if (temp == 0x82) {
      length = GsmPDUHelper.processHexToInt(
        value.slice(hlen * 2, hlen * 2 + 4),
        16
      );
      hlen += 2;
      if (length < 0x0100) {
        throw new Error(
          "Invalid length in 3-byte Comprehension TLV :" + length
        );
      }
    } else if (temp == 0x83) {
      length = GsmPDUHelper.processHexToInt(
        value.slice(hlen * 2, hlen * 2 + 6),
        16
      );
      hlen += 3;
      if (length < 0x010000) {
        throw new Error(
          "Invalid length in 4-byte Comprehension TLV :" + length
        );
      }
    } else {
      throw new Error("Invalid octet in Comprehension TLV :" + temp);
    }

    let decodeValue = value.slice(hlen * 2);
    let ctlv = {
      tag,
      length,
      value: this.context.StkProactiveCmdHelper.retrieve(
        tag,
        decodeValue,
        length
      ),
      cr,
      hlen,
    };

    return ctlv;
  },

  decodeChunks(value, length) {
    let chunks = [];
    let index = 0;
    while (index < length) {
      let tlv = this.decode(value);
      chunks.push(tlv);
      index += tlv.length;
      index += tlv.hlen;
      // Remove the readed value.
      value = value.slice((tlv.length + tlv.hlen) * 2);
    }
    return chunks;
  },

  /**
   * Write Location Info Comprehension TLV.
   *
   * @param loc location Information.
   */
  writeLocationInfoTlv(loc) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    GsmPDUHelper.writeHexOctet(
      COMPREHENSIONTLV_TAG_LOCATION_INFO | COMPREHENSIONTLV_FLAG_CR
    );
    GsmPDUHelper.writeHexOctet(loc.gsmCellId > 0xffff ? 9 : 7);
    // From TS 11.14, clause 12.19
    // "The mobile country code (MCC), the mobile network code (MNC),
    // the location area code (LAC) and the cell ID are
    // coded as in TS 04.08."
    // And from TS 04.08 and TS 24.008,
    // the format is as follows:
    //
    // MCC = MCC_digit_1 + MCC_digit_2 + MCC_digit_3
    //
    //   8  7  6  5    4  3  2  1
    // +-------------+-------------+
    // | MCC digit 2 | MCC digit 1 | octet 2
    // | MNC digit 3 | MCC digit 3 | octet 3
    // | MNC digit 2 | MNC digit 1 | octet 4
    // +-------------+-------------+
    //
    // Also in TS 24.008
    // "However a network operator may decide to
    // use only two digits in the MNC in the LAI over the
    // radio interface. In this case, bits 5 to 8 of octet 3
    // shall be coded as '1111'".

    // MCC & MNC, 3 octets
    let mcc = loc.mcc,
      mnc;
    if (loc.mnc.length == 2) {
      mnc = "F" + loc.mnc;
    } else {
      mnc = loc.mnc[2] + loc.mnc[0] + loc.mnc[1];
    }
    GsmPDUHelper.writeSwappedNibbleBCD(mcc + mnc);

    // LAC, 2 octets
    GsmPDUHelper.writeHexOctet((loc.gsmLocationAreaCode >> 8) & 0xff);
    GsmPDUHelper.writeHexOctet(loc.gsmLocationAreaCode & 0xff);

    // Cell Id
    if (loc.gsmCellId > 0xffff) {
      // UMTS/WCDMA, gsmCellId is 28 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 24) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    } else {
      // GSM, gsmCellId is 16 bits.
      GsmPDUHelper.writeHexOctet((loc.gsmCellId >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(loc.gsmCellId & 0xff);
    }
  },

  /**
   * Given a geckoError string, this function translates it into cause value
   * and write the value into buffer.
   *
   * @param geckoError Error string that is passed to gecko.
   */
  writeCauseTlv(geckoError) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    let cause = -1;
    for (let errorNo in RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR) {
      if (geckoError == RIL_CALL_FAILCAUSE_TO_GECKO_CALL_ERROR[errorNo]) {
        cause = errorNo;
        break;
      }
    }

    // Causes specified in 10.5.4.11 of TS 04.08 are less than 128.
    // we can ignore causes > 127 since Cause TLV is optional in
    // STK_EVENT_TYPE_CALL_DISCONNECTED.
    if (cause > 127) {
      return;
    }

    cause = cause == -1 ? Ci.nsIRilResponseResult.RADIO_ERROR_NONE : cause;

    GsmPDUHelper.writeHexOctet(
      COMPREHENSIONTLV_TAG_CAUSE | COMPREHENSIONTLV_FLAG_CR
    );
    GsmPDUHelper.writeHexOctet(2); // For single cause value.

    // TS 04.08, clause 10.5.4.11:
    // Code Standard : Standard defined for GSM PLMNS
    // Location: User
    GsmPDUHelper.writeHexOctet(0x60);

    // TS 04.08, clause 10.5.4.11: ext bit = 1 + 7 bits for cause.
    // +-----------------+----------------------------------+
    // | Ext = 1 (1 bit) |          Cause (7 bits)          |
    // +-----------------+----------------------------------+
    GsmPDUHelper.writeHexOctet(0x80 | cause);
  },

  writeDateTimeZoneTlv(date) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_DATE_TIME_ZONE);
    GsmPDUHelper.writeHexOctet(7);
    GsmPDUHelper.writeTimestamp(date);
  },

  writeLanguageTlv(language) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    GsmPDUHelper.writeHexOctet(COMPREHENSIONTLV_TAG_LANGUAGE);
    GsmPDUHelper.writeHexOctet(2);

    // ISO 639-1, Alpha-2 code
    // TS 123.038, clause 6.2.1, GSM 7 bit Default Alphabet
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(
        language[0]
      )
    );
    GsmPDUHelper.writeHexOctet(
      PDU_NL_LOCKING_SHIFT_TABLES[PDU_NL_IDENTIFIER_DEFAULT].indexOf(
        language[1]
      )
    );
  },

  /**
   * Write Timer Value Comprehension TLV.
   *
   * @param seconds length of time during of the timer.
   * @param cr Comprehension Required or not
   */
  writeTimerValueTlv(seconds, cr) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    GsmPDUHelper.writeHexOctet(
      COMPREHENSIONTLV_TAG_TIMER_VALUE | (cr ? COMPREHENSIONTLV_FLAG_CR : 0)
    );
    GsmPDUHelper.writeHexOctet(3);

    // TS 102.223, clause 8.38
    // +----------------+------------------+-------------------+
    // | hours (1 byte) | minutes (1 btye) | seconds (1 byte)  |
    // +----------------+------------------+-------------------+
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60 / 60));
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds / 60) % 60);
    GsmPDUHelper.writeSwappedNibbleBCDNum(Math.floor(seconds) % 60);
  },

  writeTextStringTlv(text, coding) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let buf = GsmPDUHelper.writeWithBuffer(() => {
      // Write Coding.
      GsmPDUHelper.writeHexOctet(coding);

      // Write Text String.
      switch (coding) {
        case STK_TEXT_CODING_UCS2:
          GsmPDUHelper.writeUCS2String(text);
          break;
        case STK_TEXT_CODING_GSM_7BIT_PACKED:
          GsmPDUHelper.writeStringAsSeptets(text, 0, 0, 0);
          break;
        case STK_TEXT_CODING_GSM_8BIT:
          GsmPDUHelper.writeStringAs8BitUnpacked(text);
          break;
      }
    });

    let length = buf.length;
    if (length) {
      // Write Tag.
      GsmPDUHelper.writeHexOctet(
        COMPREHENSIONTLV_TAG_TEXT_STRING | COMPREHENSIONTLV_FLAG_CR
      );
      // Write Length.
      this.writeLength(length);
      // Write Value.
      for (let i = 0; i < length; i++) {
        GsmPDUHelper.writeHexOctet(buf[i]);
      }
    }
  },

  getSizeOfLengthOctets(length) {
    if (length >= 0x10000) {
      return 4; // 0x83, len_1, len_2, len_3
    } else if (length >= 0x100) {
      return 3; // 0x82, len_1, len_2
    } else if (length >= 0x80) {
      return 2; // 0x81, len
    }
    return 1; // len
  },

  writeLength(length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;

    // TS 101.220 clause 7.1.2, Length Encoding.
    // Length   |  Byte 1  | Byte 2 | Byte 3 | Byte 4 |
    // 0 - 127  |  00 - 7f | N/A    | N/A    | N/A    |
    // 128-255  |  81      | 80 - ff| N/A    | N/A    |
    // 256-65535|  82      | 0100 - ffff     | N/A    |
    // 65536-   |  83      |     010000 - ffffff      |
    // 16777215
    if (length < 0x80) {
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x80 <= length && length < 0x100) {
      GsmPDUHelper.writeHexOctet(0x81);
      GsmPDUHelper.writeHexOctet(length);
    } else if (0x100 <= length && length < 0x10000) {
      GsmPDUHelper.writeHexOctet(0x82);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else if (0x10000 <= length && length < 0x1000000) {
      GsmPDUHelper.writeHexOctet(0x83);
      GsmPDUHelper.writeHexOctet((length >> 16) & 0xff);
      GsmPDUHelper.writeHexOctet((length >> 8) & 0xff);
      GsmPDUHelper.writeHexOctet(length & 0xff);
    } else {
      throw new Error("Invalid length value :" + length);
    }
  },
};

function StkProactiveCmdHelperObject(aContext) {
  this.context = aContext;
}
StkProactiveCmdHelperObject.prototype = {
  context: null,

  retrieve(tag, value, length) {
    let method = this[tag];
    if (typeof method != "function") {
      if (DEBUG) {
        this.context.debug("Unknown comprehension tag " + tag.toString(16));
      }
      return null;
    }
    return method.call(this, value, length);
  },

  /**
   * Command Details.
   *
   * | Byte | Description         | Length |
   * |  1   | Command details Tag |   1    |
   * |  2   | Length = 03         |   1    |
   * |  3   | Command number      |   1    |
   * |  4   | Type of Command     |   1    |
   * |  5   | Command Qualifier   |   1    |
   */
  retrieveCommandDetails(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let cmdDetails = {
      commandNumber: GsmPDUHelper.processHexToInt(value.slice(0, 2), 16),
      typeOfCommand: GsmPDUHelper.processHexToInt(value.slice(2, 4), 16),
      commandQualifier: GsmPDUHelper.processHexToInt(value.slice(4, 6), 16),
    };

    return cmdDetails;
  },

  /**
   * Device Identities.
   *
   * | Byte | Description            | Length |
   * |  1   | Device Identity Tag    |   1    |
   * |  2   | Length = 02            |   1    |
   * |  3   | Source device Identity |   1    |
   * |  4   | Destination device Id  |   1    |
   */
  retrieveDeviceId(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    let deviceId = {
      sourceId: GsmPDUHelper.processHexToInt(value.slice(0, 2), 16),
      destinationId: GsmPDUHelper.processHexToInt(value.slice(2, 4), 16),
    };
    return deviceId;
  },

  /**
   * Alpha Identifier.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3 ~    | Alpha identfier        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAlphaId(value, length) {
    let alphaId = {
      identifier: this.context.ICCPDUHelper.readAlphaIdentifier(value, length),
    };
    return alphaId;
  },

  /**
   * Duration.
   *
   * | Byte | Description           | Length |
   * |  1   | Response Length Tag   |   1    |
   * |  2   | Lenth = 02            |   1    |
   * |  3   | Time unit             |   1    |
   * |  4   | Time interval         |   1    |
   */
  retrieveDuration(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let duration = {
      timeUnit: GsmPDUHelper.readHexOctet(),
      timeInterval: GsmPDUHelper.readHexOctet(),
    };
    return duration;
  },

  /**
   * Address.
   *
   * | Byte         | Description            | Length |
   * |  1           | Alpha Identifier Tag   |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | TON and NPI            |   1    |
   * | (Y-1)+4 ~    | Dialling number        |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveAddress(value, length) {
    this.context.GsmPDUHelper.initWith(value);
    let address = {
      number: this.context.ICCPDUHelper.readDiallingNumber(length),
    };
    return address;
  },

  /**
   * Text String.
   *
   * | Byte         | Description        | Length |
   * |  1           | Text String Tag    |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)         |   Y    |
   * | (Y-1)+3      | Data coding scheme |   1    |
   * | (Y-1)+4~     | Text String        |   X    |
   * | (Y-1)+X+2    |                    |        |
   */
  retrieveTextString(value, length) {
    if (!length) {
      // null string.
      return { textString: null };
    }

    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let text = {
      codingScheme: GsmPDUHelper.readHexOctet(),
    };

    length--; // -1 for the codingScheme.
    switch (text.codingScheme & 0x0c) {
      case STK_TEXT_CODING_GSM_7BIT_PACKED:
        text.textString = GsmPDUHelper.readSeptetsToString(
          Math.floor((length * 8) / 7),
          0,
          0,
          0
        );
        break;
      case STK_TEXT_CODING_GSM_8BIT:
        text.textString = this.context.ICCPDUHelper.read8BitUnpackedToString(value,
          length
        );
        break;
      case STK_TEXT_CODING_UCS2:
        text.textString = GsmPDUHelper.readUCS2String(length);
        break;
    }
    return text;
  },

  /**
   * Tone.
   *
   * | Byte | Description     | Length |
   * |  1   | Tone Tag        |   1    |
   * |  2   | Lenth = 01      |   1    |
   * |  3   | Tone            |   1    |
   */
  retrieveTone(value, length) {
    this.context.GsmPDUHelper.initWith(value);
    let tone = {
      tone: this.context.GsmPDUHelper.readHexOctet(),
    };
    return tone;
  },

  /**
   * Item.
   *
   * | Byte         | Description            | Length |
   * |  1           | Item Tag               |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Identifier of item     |   1    |
   * | (Y-1)+4 ~    | Text string of item    |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveItem(value, length) {
    // TS 102.223 ,clause 6.6.7 SET-UP MENU
    // If the "Item data object for item 1" is a null data object
    // (i.e. length = '00' and no value part), this is an indication to the ME
    // to remove the existing menu from the menu system in the ME.
    if (!length) {
      return null;
    }
    let identifier = this.context.GsmPDUHelper.processHexToInt(
      value.slice(0, 2),
      16
    );
    // skip the identifier.
    value = value.slice(2);
    let item = {
      identifier,
      text: this.context.ICCPDUHelper.readAlphaIdentifier(value, length - 1),
    };
    return item;
  },

  /**
   * Item Identifier.
   *
   * | Byte | Description               | Length |
   * |  1   | Item Identifier Tag       |   1    |
   * |  2   | Lenth = 01                |   1    |
   * |  3   | Identifier of Item chosen |   1    |
   */
  retrieveItemId(value, length) {
    this.context.GsmPDUHelper.initWith(value);
    let itemId = {
      identifier: this.context.GsmPDUHelper.readHexOctet(),
    };
    return itemId;
  },

  /**
   * Response Length.
   *
   * | Byte | Description                | Length |
   * |  1   | Response Length Tag        |   1    |
   * |  2   | Lenth = 02                 |   1    |
   * |  3   | Minimum length of response |   1    |
   * |  4   | Maximum length of response |   1    |
   */
  retrieveResponseLength(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let rspLength = {
      minLength: GsmPDUHelper.readHexOctet(),
      maxLength: GsmPDUHelper.readHexOctet(),
    };
    return rspLength;
  },

  /**
   * File List.
   *
   * | Byte         | Description            | Length |
   * |  1           | File List Tag          |   1    |
   * | 2 ~ (Y-1)+2  | Length (X)             |   Y    |
   * | (Y-1)+3      | Number of files        |   1    |
   * | (Y-1)+4 ~    | Files                  |   X    |
   * | (Y-1)+X+2    |                        |        |
   */
  retrieveFileList(value, length) {
    this.context.GsmPDUHelper.initWith(value);
    let num = this.context.GsmPDUHelper.readHexOctet();
    let fileList = "";
    length--; // -1 for the num octet.
    for (let i = 0; i < 2 * length; i++) {
      // Didn't use readHexOctet here,
      // otherwise 0x00 will be "0", not "00"
      //fileList += String.fromCharCode(this.context.Buf.readUint16());
      fileList += this.context.GsmPDUHelper.readHexNibble();
    }
    return {
      fileList,
    };
  },

  /**
   * Default Text.
   *
   * Same as Text String.
   */
  retrieveDefaultText(value, length) {
    let text = this.retrieveTextString(value, length);

    return text;
  },

  /**
   * Event List.
   */
  retrieveEventList(value, length) {
    if (!length) {
      // null means an indication to ME to remove the existing list of events
      // in ME.
      return null;
    }

    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let eventList = [];
    for (let i = 0; i < length; i++) {
      eventList.push(GsmPDUHelper.readHexOctet());
    }

    return {
      eventList,
    };
  },

  /**
   * Icon Id.
   *
   * | Byte  | Description          | Length |
   * |  1    | Icon Identifier Tag  |   1    |
   * |  2    | Length = 02          |   1    |
   * |  3    | Icon qualifier       |   1    |
   * |  4    | Icon identifier      |   1    |
   */
  retrieveIconId(value, length) {
    if (!length) {
      return null;
    }
    this.context.GsmPDUHelper.initWith(value);
    let iconId = {
      qualifier: this.context.GsmPDUHelper.readHexOctet(),
      identifier: this.context.GsmPDUHelper.readHexOctet(),
    };

    return iconId;
  },

  /**
   * Icon Id List.
   *
   * | Byte  | Description          | Length |
   * |  1    | Icon Identifier Tag  |   1    |
   * |  2    | Length = X           |   1    |
   * |  3    | Icon qualifier       |   1    |
   * |  4~   | Icon identifier      |  X-1   |
   * | 4+X-2 |                      |        |
   */
  retrieveIconIdList(value, length) {
    if (!length) {
      return null;
    }
    this.context.GsmPDUHelper.initWith(value);
    let iconIdList = {
      qualifier: this.context.GsmPDUHelper.readHexOctet(),
      identifiers: [],
    };
    for (let i = 0; i < length - 1; i++) {
      iconIdList.identifiers.push(this.context.GsmPDUHelper.readHexOctet());
    }

    return iconIdList;
  },

  /**
   * Timer Identifier.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Identifier Tag |   1    |
   * |  2    | Length = 01          |   1    |
   * |  3    | Timer Identifier     |   1    |
   */
  retrieveTimerId(value, length) {
    this.context.GsmPDUHelper.initWith(value);
    let id = {
      timerId: this.context.GsmPDUHelper.readHexOctet(),
    };

    return id;
  },

  /**
   * Timer Value.
   *
   * | Byte  | Description          | Length |
   * |  1    | Timer Value Tag      |   1    |
   * |  2    | Length = 03          |   1    |
   * |  3    | Hour                 |   1    |
   * |  4    | Minute               |   1    |
   * |  5    | Second               |   1    |
   */
  retrieveTimerValue(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let timer = {
      timerValue:
        GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60 * 60 +
        GsmPDUHelper.readSwappedNibbleBcdNum(1) * 60 +
        GsmPDUHelper.readSwappedNibbleBcdNum(1),
    };
    this.context.debug(
      "StkProactiveCmdHelperObject retrieveTimerValue timer: " +
        JSON.stringify(timer)
    );
    return timer;
  },

  /**
   * Immediate Response.
   *
   * | Byte  | Description            | Length |
   * |  1    | Immediate Response Tag |   1    |
   * |  2    | Length = 00            |   1    |
   */
  retrieveImmediaResponse(value, length) {
    return {};
  },

  /**
   * URL
   *
   * | Byte      | Description         | Length |
   * |  1        | URL Tag             |   1    |
   * | 2 ~ (Y+1) | Length(X)           |   Y    |
   * | (Y+2) ~   | URL                 |   X    |
   * | (Y+1+X)   |                     |        |
   */
  retrieveUrl(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let s = "";
    for (let i = 0; i < length; i++) {
      s += String.fromCharCode(GsmPDUHelper.readHexOctet());
    }

    return { url: s };
  },

  /**
   * Next Action Indicator List.
   *
   * | Byte  | Description      | Length |
   * |  1    | Next Action tag  |   1    |
   * |  1    | Length(X)        |   1    |
   * |  3~   | Next Action List |   X    |
   * | 3+X-1 |                  |        |
   */
  retrieveNextActionList(value, length) {
    let GsmPDUHelper = this.context.GsmPDUHelper;
    GsmPDUHelper.initWith(value);
    let nextActionList = [];
    for (let i = 0; i < length; i++) {
      nextActionList.push(GsmPDUHelper.readHexOctet());
    }

    return nextActionList;
  },

  searchForTag(tag, ctlvs) {
    let ctlv = null;
    ctlvs.forEach(aCtlv => {
      if ((aCtlv.tag & ~COMPREHENSIONTLV_FLAG_CR) == tag) {
        ctlv = aCtlv;
      }
    });
    return ctlv;
  },

  searchForSelectedTags(ctlvs, tags) {
    let ret = {
      // Handy utility to de-queue the 1st ctlv of the specified tag.
      retrieve(aTag) {
        return this[aTag] ? this[aTag].shift() : null;
      },
    };

    ctlvs.forEach(aCtlv => {
      tags.forEach(aTag => {
        if ((aCtlv.tag & ~COMPREHENSIONTLV_FLAG_CR) == aTag) {
          if (!ret[aTag]) {
            ret[aTag] = [];
          }
          ret[aTag].push(aCtlv);
        }
      });
    });
    return ret;
  },
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_COMMAND_DETAILS
] = function COMPREHENSIONTLV_TAG_COMMAND_DETAILS(value, length) {
  return this.retrieveCommandDetails(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_DEVICE_ID
] = function COMPREHENSIONTLV_TAG_DEVICE_ID(value, length) {
  return this.retrieveDeviceId(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ALPHA_ID
] = function COMPREHENSIONTLV_TAG_ALPHA_ID(value, length) {
  return this.retrieveAlphaId(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_DURATION
] = function COMPREHENSIONTLV_TAG_DURATION(value, length) {
  return this.retrieveDuration(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ADDRESS
] = function COMPREHENSIONTLV_TAG_ADDRESS(value, length) {
  return this.retrieveAddress(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_TEXT_STRING
] = function COMPREHENSIONTLV_TAG_TEXT_STRING(value, length) {
  return this.retrieveTextString(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_TONE
] = function COMPREHENSIONTLV_TAG_TONE(value, length) {
  return this.retrieveTone(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ITEM
] = function COMPREHENSIONTLV_TAG_ITEM(value, length) {
  return this.retrieveItem(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ITEM_ID
] = function COMPREHENSIONTLV_TAG_ITEM_ID(value, length) {
  return this.retrieveItemId(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_RESPONSE_LENGTH
] = function COMPREHENSIONTLV_TAG_RESPONSE_LENGTH(value, length) {
  return this.retrieveResponseLength(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_FILE_LIST
] = function COMPREHENSIONTLV_TAG_FILE_LIST(value, length) {
  return this.retrieveFileList(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_DEFAULT_TEXT
] = function COMPREHENSIONTLV_TAG_DEFAULT_TEXT(value, length) {
  return this.retrieveDefaultText(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_EVENT_LIST
] = function COMPREHENSIONTLV_TAG_EVENT_LIST(value, length) {
  return this.retrieveEventList(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ICON_ID
] = function COMPREHENSIONTLV_TAG_ICON_ID(value, length) {
  return this.retrieveIconId(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_ICON_ID_LIST
] = function COMPREHENSIONTLV_TAG_ICON_ID_LIST(value, length) {
  return this.retrieveIconIdList(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER
] = function COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER(value, length) {
  return this.retrieveTimerId(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_TIMER_VALUE
] = function COMPREHENSIONTLV_TAG_TIMER_VALUE(value, length) {
  return this.retrieveTimerValue(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE
] = function COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE(value, length) {
  return this.retrieveImmediaResponse(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_URL
] = function COMPREHENSIONTLV_TAG_URL(value, length) {
  return this.retrieveUrl(value, length);
};
StkProactiveCmdHelperObject.prototype[
  COMPREHENSIONTLV_TAG_NEXT_ACTION_IND
] = function COMPREHENSIONTLV_TAG_NEXT_ACTION_IND(value, length) {
  return this.retrieveNextActionList(value, length);
};

function StkCommandParamsFactoryObject(aContext) {
  this.context = aContext;
}
StkCommandParamsFactoryObject.prototype = {
  context: null,

  createParam(cmdDetails, ctlvs, onComplete) {
    let method = this[cmdDetails.typeOfCommand];
    if (typeof method != "function") {
      if (DEBUG) {
        this.context.debug(
          "Unknown proactive command " + cmdDetails.typeOfCommand.toString(16)
        );
      }
      return;
    }
    method.call(this, cmdDetails, ctlvs, onComplete);
  },

  loadIcons(iconIdCtlvs, callback) {
    if (
      !iconIdCtlvs ||
      !this.context.ICCUtilsHelper.isICCServiceAvailable("IMG")
    ) {
      callback(null);
      return;
    }

    let onerror = function() {
      callback(null);
    };

    let onsuccess = function(aIcons) {
      callback(aIcons);
    };

    this.context.IconLoader.loadIcons(
      iconIdCtlvs.map(aCtlv => aCtlv.value.identifier),
      onsuccess,
      onerror
    );
  },

  appendIconIfNecessary(iconIdCtlvs, result, onComplete) {
    this.loadIcons(iconIdCtlvs, aIcons => {
      if (aIcons) {
        result.icons = aIcons[0];
        result.iconSelfExplanatory = iconIdCtlvs[0].value.qualifier == 0;
      }

      onComplete(result);
    });
  },

  /**
   * Construct a param for Refresh.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processRefresh(cmdDetails, ctlvs, onComplete) {
    let refreshType = cmdDetails.commandQualifier;
    switch (refreshType) {
      case STK_REFRESH_FILE_CHANGE:
      case STK_REFRESH_NAA_INIT_AND_FILE_CHANGE:
        let ctlv = this.context.StkProactiveCmdHelper.searchForTag(
          COMPREHENSIONTLV_TAG_FILE_LIST,
          ctlvs
        );
        if (ctlv) {
          let list = ctlv.value.fileList;
          if (DEBUG) {
            this.context.debug("Refresh, list = " + list);
          }
          this.context.ICCRecordHelper.fetchICCRecords();
        }
        break;
    }

    onComplete(null);
  },

  /**
   * Construct a param for Poll Interval.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processPollInterval(cmdDetails, ctlvs, onComplete) {
    // Duration is mandatory.
    let ctlv = this.context.StkProactiveCmdHelper.searchForTag(
      COMPREHENSIONTLV_TAG_DURATION,
      ctlvs
    );
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Poll Interval: Required value missing : Duration");
    }

    onComplete(ctlv.value);
  },

  /**
   * Construct a param for Poll Off.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processPollOff(cmdDetails, ctlvs, onComplete) {
    onComplete(null);
  },

  /**
   * Construct a param for Set Up Event list.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processSetUpEventList(cmdDetails, ctlvs, onComplete) {
    // Event list is mandatory.
    let ctlv = this.context.StkProactiveCmdHelper.searchForTag(
      COMPREHENSIONTLV_TAG_EVENT_LIST,
      ctlvs
    );
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Event List: Required value missing : Event List");
    }

    onComplete(ctlv.value || { eventList: null });
  },

  /**
   * Construct a param for Setup Menu.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processSetupMenu(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let menu = {
      // Help information available.
      isHelpAvailable: !!(cmdDetails.commandQualifier & 0x80),
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_ITEM,
      COMPREHENSIONTLV_TAG_ITEM_ID,
      COMPREHENSIONTLV_TAG_NEXT_ACTION_IND,
      COMPREHENSIONTLV_TAG_ICON_ID,
      COMPREHENSIONTLV_TAG_ICON_ID_LIST,
    ]);

    // Alpha identifier is optional.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      menu.title = ctlv.value.identifier;
    }

    // Item data object for item 1 is mandatory.
    let menuCtlvs = selectedCtlvs[COMPREHENSIONTLV_TAG_ITEM];
    if (!menuCtlvs) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Menu: Required value missing : items");
    }
    menu.items = menuCtlvs.map(aCtlv => aCtlv.value);

    // Item identifier is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ITEM_ID);
    if (ctlv) {
      menu.defaultItem = ctlv.value.identifier - 1;
    }

    // Items next action indicator is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_NEXT_ACTION_IND);
    if (ctlv) {
      menu.nextActionList = ctlv.value;
    }

    // Icon identifier is optional.
    let iconIdCtlvs = null;
    let menuIconCtlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ICON_ID);
    if (menuIconCtlv) {
      iconIdCtlvs = [menuIconCtlv];
    }

    // Item icon identifier list is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ICON_ID_LIST);
    if (ctlv) {
      if (!iconIdCtlvs) {
        iconIdCtlvs = [];
      }
      let iconIdList = ctlv.value;
      iconIdCtlvs = iconIdCtlvs.concat(
        iconIdList.identifiers.map(aId => {
          return {
            value: { qualifier: iconIdList.qualifier, identifier: aId },
          };
        })
      );
    }

    this.loadIcons(iconIdCtlvs, aIcons => {
      if (aIcons) {
        if (menuIconCtlv) {
          menu.iconSelfExplanatory = iconIdCtlvs.shift().value.qualifier == 0;
          menu.icons = aIcons.shift();
        }

        for (let i = 0; i < aIcons.length; i++) {
          menu.items[i].icons = aIcons[i];
          menu.items[i].iconSelfExplanatory =
            iconIdCtlvs[i].value.qualifier == 0;
        }
      }

      onComplete(menu);
    });
  },

  /**
   * Construct a param for Select Item.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processSelectItem(cmdDetails, ctlvs, onComplete) {
    this.processSetupMenu(cmdDetails, ctlvs, menu => {
      // The 1st bit and 2nd bit determines the presentation type.
      menu.presentationType = cmdDetails.commandQualifier & 0x03;
      onComplete(menu);
    });
  },

  /**
   * Construct a param for Display Text.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processDisplayText(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let textMsg = {
      isHighPriority: !!(cmdDetails.commandQualifier & 0x01),
      userClear: !!(cmdDetails.commandQualifier & 0x80),
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_TEXT_STRING,
      COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE,
      COMPREHENSIONTLV_TAG_DURATION,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Text string is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TEXT_STRING);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Display Text: Required value missing : Text String");
    }
    textMsg.text = ctlv.value.textString;

    if (DEBUG) {
      this.context.debug("processDisplayText, text = " + textMsg.text);
    }
    // TS102.384 27.22.4.1.1.9
    if (!textMsg.text) {
      if (DEBUG) {
        this.context.debug(
          "processDisplayText, text empty, through error CMD_DATA_NOT_UNDERSTOOD"
        );
      }
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_CMD_DATA_NOT_UNDERSTOOD,
      });
      throw new Error("Stk Display Text: Required value empty : Text String");
    }

    // Immediate response is optional.
    textMsg.responseNeeded = !!selectedCtlvs.retrieve(
      COMPREHENSIONTLV_TAG_IMMEDIATE_RESPONSE
    );

    // Duration is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DURATION);
    if (ctlv) {
      textMsg.duration = ctlv.value;
    }

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      textMsg,
      onComplete
    );
  },

  /**
   * Construct a param for Setup Idle Mode Text.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processSetUpIdleModeText(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let textMsg = {};

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_TEXT_STRING,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Text string is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TEXT_STRING);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error(
        "Stk Set Up Idle Text: Required value missing : Text String"
      );
    }
    textMsg.text = ctlv.value.textString;

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      textMsg,
      onComplete
    );
  },

  /**
   * Construct a param for Get Inkey.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processGetInkey(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let input = {
      minLength: 1,
      maxLength: 1,
      isAlphabet: !!(cmdDetails.commandQualifier & 0x01),
      isUCS2: !!(cmdDetails.commandQualifier & 0x02),
      // Character sets defined in bit 1 and bit 2 are disable and
      // the YES/NO reponse is required.
      isYesNoRequested: !!(cmdDetails.commandQualifier & 0x04),
      // Help information available.
      isHelpAvailable: !!(cmdDetails.commandQualifier & 0x80),
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_TEXT_STRING,
      COMPREHENSIONTLV_TAG_DURATION,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Text string is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TEXT_STRING);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Get InKey: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    // Duration is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DURATION);
    if (ctlv) {
      input.duration = ctlv.value;
    }

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      input,
      onComplete
    );
  },

  /**
   * Construct a param for Get Input.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processGetInput(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let input = {
      isAlphabet: !!(cmdDetails.commandQualifier & 0x01),
      isUCS2: !!(cmdDetails.commandQualifier & 0x02),
      // User input shall not be revealed
      hideInput: !!(cmdDetails.commandQualifier & 0x04),
      // User input in SMS packed format
      isPacked: !!(cmdDetails.commandQualifier & 0x08),
      // Help information available.
      isHelpAvailable: !!(cmdDetails.commandQualifier & 0x80),
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_TEXT_STRING,
      COMPREHENSIONTLV_TAG_RESPONSE_LENGTH,
      COMPREHENSIONTLV_TAG_DEFAULT_TEXT,
      COMPREHENSIONTLV_TAG_DURATION,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Text string is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TEXT_STRING);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Get Input: Required value missing : Text String");
    }
    input.text = ctlv.value.textString;

    // Response length is mandatory.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_RESPONSE_LENGTH);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error(
        "Stk Get Input: Required value missing : Response Length"
      );
    }
    input.minLength = ctlv.value.minLength;
    input.maxLength = ctlv.value.maxLength;

    // Default text is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DEFAULT_TEXT);
    if (ctlv) {
      input.defaultText = ctlv.value.textString;
    }

    // Duration is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DURATION);
    if (ctlv) {
      input.duration = ctlv.value;
    }

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      input,
      onComplete
    );
  },

  /**
   * Construct a param for SendSS/SMS/USSD/DTMF.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processEventNotify(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let textMsg = {};

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Alpha identifier is optional.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      textMsg.text = ctlv.value.identifier;
    }

    // According to section 6.4.10 of |ETSI TS 102 223|:
    //
    // - if the alpha identifier is provided by the UICC and is a null data
    //   object (i.e. length = '00' and no value part), this is an indication
    //   that the terminal should not give any information to the user on the
    //   fact that the terminal is sending a short message;
    //
    // - if the alpha identifier is not provided by the UICC, the terminal may
    //   give information to the user concerning what is happening.
    //
    // ICCPDUHelper reads alpha id as an empty string if the length is zero,
    // hence we'll notify the caller when it's not an empty string.
    if (textMsg.text !== "") {
      // Icon identifier is optional.
      this.appendIconIfNecessary(
        selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
        textMsg,
        onComplete
      );
    }
  },

  /**
   * Construct a param for Setup Call.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processSetupCall(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let call = {};
    let confirmMessage = {};
    let callMessage = {};

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_ADDRESS,
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_ICON_ID,
      COMPREHENSIONTLV_TAG_DURATION,
    ]);

    // Address is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ADDRESS);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Set Up Call: Required value missing : Address");
    }
    call.address = ctlv.value.number;

    // Alpha identifier (user confirmation phase) is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      confirmMessage.text = ctlv.value.identifier;
      call.confirmMessage = confirmMessage;
    }

    // Alpha identifier (call set up phase) is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      callMessage.text = ctlv.value.identifier;
      call.callMessage = callMessage;
    }

    // Duration is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DURATION);
    if (ctlv) {
      call.duration = ctlv.value;
    }

    // Icon identifier is optional.
    let iconIdCtlvs = selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null;
    this.loadIcons(iconIdCtlvs, aIcons => {
      if (aIcons) {
        confirmMessage.icons = aIcons[0];
        confirmMessage.iconSelfExplanatory =
          iconIdCtlvs[0].value.qualifier == 0;
        call.confirmMessage = confirmMessage;

        if (aIcons.length > 1) {
          callMessage.icons = aIcons[1];
          callMessage.iconSelfExplanatory = iconIdCtlvs[1].value.qualifier == 0;
          call.callMessage = callMessage;
        }
      }

      onComplete(call);
    });
  },

  /**
   * Construct a param for Launch Browser.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processLaunchBrowser(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let browser = {
      mode: cmdDetails.commandQualifier & 0x03,
    };
    let confirmMessage = {};

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_URL,
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // URL is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_URL);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error("Stk Launch Browser: Required value missing : URL");
    }
    browser.url = ctlv.value.url;

    // Alpha identifier is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      confirmMessage.text = ctlv.value.identifier;
      browser.confirmMessage = confirmMessage;
    }

    // Icon identifier is optional.
    let iconIdCtlvs = selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null;
    this.loadIcons(iconIdCtlvs, aIcons => {
      if (aIcons) {
        confirmMessage.icons = aIcons[0];
        confirmMessage.iconSelfExplanatory =
          iconIdCtlvs[0].value.qualifier == 0;
        browser.confirmMessage = confirmMessage;
      }

      onComplete(browser);
    });
  },

  /**
   * Construct a param for Play Tone.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processPlayTone(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let playTone = {
      // The vibrate is only defined in TS 102.223.
      isVibrate: !!(cmdDetails.commandQualifier & 0x01),
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_TONE,
      COMPREHENSIONTLV_TAG_DURATION,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Alpha identifier is optional.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      playTone.text = ctlv.value.identifier;
    }

    // Tone is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TONE);
    if (ctlv) {
      playTone.tone = ctlv.value.tone;
    }

    // Duration is optional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_DURATION);
    if (ctlv) {
      playTone.duration = ctlv.value;
    }

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      playTone,
      onComplete
    );
  },

  /**
   * Construct a param for Provide Local Information.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processProvideLocalInfo(cmdDetails, ctlvs, onComplete) {
    let provideLocalInfo = {
      localInfoType: cmdDetails.commandQualifier,
    };

    onComplete(provideLocalInfo);
  },

  /**
   * Construct a param for Timer Management.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processTimerManagement(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let timer = {
      timerAction: cmdDetails.commandQualifier,
    };

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER,
      COMPREHENSIONTLV_TAG_TIMER_VALUE,
    ]);

    // Timer identifier is mandatory.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TIMER_IDENTIFIER);
    if (!ctlv) {
      this.context.RIL.processSendStkTerminalResponse({
        command: cmdDetails,
        resultCode: STK_RESULT_REQUIRED_VALUES_MISSING,
      });
      throw new Error(
        "Stk Timer Management: Required value missing : Timer Identifier"
      );
    }
    timer.timerId = ctlv.value.timerId;

    // Timer value is conditional.
    ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_TIMER_VALUE);
    if (ctlv) {
      timer.timerValue = ctlv.value.timerValue;
    }

    onComplete(timer);
  },

  /**
   * Construct a param for BIP commands.
   *
   * @param cmdDetails
   *        The value object of CommandDetails TLV.
   * @param ctlvs
   *        The all TLVs in this proactive command.
   * @param onComplete
   *        Callback to be called when complete.
   */
  processBipMessage(cmdDetails, ctlvs, onComplete) {
    let StkProactiveCmdHelper = this.context.StkProactiveCmdHelper;
    let bipMsg = {};

    let selectedCtlvs = StkProactiveCmdHelper.searchForSelectedTags(ctlvs, [
      COMPREHENSIONTLV_TAG_ALPHA_ID,
      COMPREHENSIONTLV_TAG_ICON_ID,
    ]);

    // Alpha identifier is optional.
    let ctlv = selectedCtlvs.retrieve(COMPREHENSIONTLV_TAG_ALPHA_ID);
    if (ctlv) {
      bipMsg.text = ctlv.value.identifier;
    }

    // Icon identifier is optional.
    this.appendIconIfNecessary(
      selectedCtlvs[COMPREHENSIONTLV_TAG_ICON_ID] || null,
      bipMsg,
      onComplete
    );
  },
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_REFRESH
] = function STK_CMD_REFRESH(cmdDetails, ctlvs, onComplete) {
  return this.processRefresh(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_POLL_INTERVAL
] = function STK_CMD_POLL_INTERVAL(cmdDetails, ctlvs, onComplete) {
  return this.processPollInterval(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_POLL_OFF
] = function STK_CMD_POLL_OFF(cmdDetails, ctlvs, onComplete) {
  return this.processPollOff(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_PROVIDE_LOCAL_INFO
] = function STK_CMD_PROVIDE_LOCAL_INFO(cmdDetails, ctlvs, onComplete) {
  return this.processProvideLocalInfo(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SET_UP_EVENT_LIST
] = function STK_CMD_SET_UP_EVENT_LIST(cmdDetails, ctlvs, onComplete) {
  return this.processSetUpEventList(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SET_UP_MENU
] = function STK_CMD_SET_UP_MENU(cmdDetails, ctlvs, onComplete) {
  return this.processSetupMenu(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SELECT_ITEM
] = function STK_CMD_SELECT_ITEM(cmdDetails, ctlvs, onComplete) {
  return this.processSelectItem(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_DISPLAY_TEXT
] = function STK_CMD_DISPLAY_TEXT(cmdDetails, ctlvs, onComplete) {
  return this.processDisplayText(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SET_UP_IDLE_MODE_TEXT
] = function STK_CMD_SET_UP_IDLE_MODE_TEXT(cmdDetails, ctlvs, onComplete) {
  return this.processSetUpIdleModeText(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_GET_INKEY
] = function STK_CMD_GET_INKEY(cmdDetails, ctlvs, onComplete) {
  return this.processGetInkey(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_GET_INPUT
] = function STK_CMD_GET_INPUT(cmdDetails, ctlvs, onComplete) {
  return this.processGetInput(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SEND_SS
] = function STK_CMD_SEND_SS(cmdDetails, ctlvs, onComplete) {
  return this.processEventNotify(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SEND_USSD
] = function STK_CMD_SEND_USSD(cmdDetails, ctlvs, onComplete) {
  return this.processEventNotify(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SEND_SMS
] = function STK_CMD_SEND_SMS(cmdDetails, ctlvs, onComplete) {
  return this.processEventNotify(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SEND_DTMF
] = function STK_CMD_SEND_DTMF(cmdDetails, ctlvs, onComplete) {
  return this.processEventNotify(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SET_UP_CALL
] = function STK_CMD_SET_UP_CALL(cmdDetails, ctlvs, onComplete) {
  return this.processSetupCall(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_LAUNCH_BROWSER
] = function STK_CMD_LAUNCH_BROWSER(cmdDetails, ctlvs, onComplete) {
  return this.processLaunchBrowser(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_PLAY_TONE
] = function STK_CMD_PLAY_TONE(cmdDetails, ctlvs, onComplete) {
  return this.processPlayTone(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_TIMER_MANAGEMENT
] = function STK_CMD_TIMER_MANAGEMENT(cmdDetails, ctlvs, onComplete) {
  return this.processTimerManagement(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_OPEN_CHANNEL
] = function STK_CMD_OPEN_CHANNEL(cmdDetails, ctlvs, onComplete) {
  return this.processBipMessage(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_CLOSE_CHANNEL
] = function STK_CMD_CLOSE_CHANNEL(cmdDetails, ctlvs, onComplete) {
  return this.processBipMessage(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_RECEIVE_DATA
] = function STK_CMD_RECEIVE_DATA(cmdDetails, ctlvs, onComplete) {
  return this.processBipMessage(cmdDetails, ctlvs, onComplete);
};
StkCommandParamsFactoryObject.prototype[
  STK_CMD_SEND_DATA
] = function STK_CMD_SEND_DATA(cmdDetails, ctlvs, onComplete) {
  return this.processBipMessage(cmdDetails, ctlvs, onComplete);
};

// Allow this file to be imported via Components.utils.import().
this.EXPORTED_SYMBOLS = Object.keys(this);
