/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const ActivitiesServiceFilter = {
  match(aValues, aOrigin, aDescription) {
    function matchValue(aValue, aFilter, aFilterObj) {
      if (aFilter !== null) {
        // Custom functions for the different types.
        switch (typeof aFilter) {
          case "boolean":
            return aValue === aFilter;

          case "number":
            return Number(aValue) === aFilter;

          case "string":
            return String(aValue) === aFilter;

          default:
            // not supported
            return false;
        }
      }

      // Pattern.
      if ("pattern" in aFilterObj) {
        var pattern = String(aFilterObj.pattern);

        var patternFlags = "";
        if ("patternFlags" in aFilterObj) {
          patternFlags = String(aFilterObj.patternFlags);
        }

        var re = new RegExp("^(?:" + pattern + ")$", patternFlags);
        return re.test(aValue);
      }

      // Validation of the min/Max.
      if ("min" in aFilterObj || "max" in aFilterObj) {
        // Min value.
        if ("min" in aFilterObj && aFilterObj.min > aValue) {
          return false;
        }

        // Max value.
        if ("max" in aFilterObj && aFilterObj.max < aValue) {
          return false;
        }
      }

      return true;
    }

    // this function returns true if the value matches with the filter object
    function matchObject(aValue, aFilterObj) {
      // Let's consider anything an array.
      let arrayValues = Array.isArray(aFilterObj.value)
        ? aFilterObj.value
        : [aFilterObj.value];
      let filters = "value" in aFilterObj ? arrayValues : [null];
      let values = Array.isArray(aValue) ? aValue : [aValue];

      for (var filterId = 0; filterId < filters.length; ++filterId) {
        for (var valueId = 0; valueId < values.length; ++valueId) {
          if (matchValue(values[valueId], filters[filterId], aFilterObj)) {
            return true;
          }
        }
      }

      return false;
    }

    function filterResult(aValues, aFilters) {
      // Creation of a filter map useful to know what has been
      // matched and what is not.
      let filtersMap = new Map();
      for (let filter in aFilters) {
        // Convert this filter in an object if needed
        let filterObj = aFilters[filter];

        if (Array.isArray(filterObj) || typeof filterObj !== "object") {
          filterObj = {
            required: false,
            value: filterObj,
          };
        }

        filtersMap.set(filter, { filter: filterObj, found: false });
      }

      // For any incoming property.
      for (let prop in aValues) {
        // If this is unknown for the app, let's continue.
        if (!filtersMap.has(prop)) {
          continue;
        }

        if (Array.isArray(aValues[prop]) && !aValues[prop].length) {
          continue;
        }

        // Otherwise, let's check the value against the filter.
        if (!matchObject(aValues[prop], filtersMap.get(prop).filter)) {
          return false;
        }

        filtersMap.get(prop).found = true;
      }

      // Required filters:
      for (const value of filtersMap.values()) {
        if (value.filter.required && !value.found) {
          return false;
        }
      }

      return true;
    }

    function allowedOriginsResult(aCallerOrigin, aAllowedOrigins) {
      if (!aAllowedOrigins) {
        // Grant for all requests if 'allowedOrigins' is not defined
        // in handler's manifest.
        return true;
      }

      let allowedOrigins = Array.isArray(aAllowedOrigins)
        ? aAllowedOrigins
        : [aAllowedOrigins];
      return (
        allowedOrigins.findIndex(origin => origin === aCallerOrigin) !== -1
      );
    }

    return (
      filterResult(aValues, aDescription.filters) &&
      allowedOriginsResult(aOrigin, aDescription.allowedOrigins)
    );
  },
};
