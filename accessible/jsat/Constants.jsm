/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

function ConstantsMap(aObject, aPrefix, aMap = {}, aModifier = null) {
  let offset = aPrefix.length;
  for (var name in aObject) {
    if (name.indexOf(aPrefix) === 0) {
      aMap[name.slice(offset)] = aModifier
        ? aModifier(aObject[name])
        : aObject[name];
    }
  }

  return aMap;
}

const EXPORTED_SYMBOLS = [];

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "Roles", function() {
  return ConstantsMap(Ci.nsIAccessibleRole, "ROLE_");
});

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "Events", function() {
  return ConstantsMap(Ci.nsIAccessibleEvent, "EVENT_");
});

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "Relations", function() {
  return ConstantsMap(Ci.nsIAccessibleRelation, "RELATION_");
});

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "Prefilters", function() {
  return ConstantsMap(Ci.nsIAccessibleTraversalRule, "PREFILTER_");
});

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "Filters", function() {
  return ConstantsMap(Ci.nsIAccessibleTraversalRule, "FILTER_");
});

XPCOMUtils.defineLazyGetter(EXPORTED_SYMBOLS, "States", function() {
  let statesMap = ConstantsMap(Ci.nsIAccessibleStates, "STATE_", {}, val => {
    return { base: val, extended: 0 };
  });
  ConstantsMap(Ci.nsIAccessibleStates, "EXT_STATE_", statesMap, val => {
    return { base: 0, extended: val };
  });
  return statesMap;
});
