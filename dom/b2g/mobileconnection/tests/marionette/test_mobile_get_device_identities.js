/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testGetDeviceIdentities() {
  return getDeviceIdentities();
}

// Start tests
startTestCommon(function() {
  return Promise.resolve().then(() => testGetDeviceIdentities());
});
