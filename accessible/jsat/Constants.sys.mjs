/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

function getStatesMap() {
  let statesMap = ConstantsMap(Ci.nsIAccessibleStates, "STATE_", {}, val => {
    return { base: val, extended: 0 };
  });
  ConstantsMap(Ci.nsIAccessibleStates, "EXT_STATE_", statesMap, val => {
    return { base: 0, extended: val };
  });
  return statesMap;
}

export const Roles = ConstantsMap(Ci.nsIAccessibleRole, "ROLE_");
export const Events = ConstantsMap(Ci.nsIAccessibleEvent, "EVENT_");
export const Relations = ConstantsMap(Ci.nsIAccessibleRelation, "RELATION_");
export const Prefilters = ConstantsMap(
  Ci.nsIAccessibleTraversalRule,
  "PREFILTER_"
);
export const Filters = ConstantsMap(Ci.nsIAccessibleTraversalRule, "FILTER_");
export const States = getStatesMap();
