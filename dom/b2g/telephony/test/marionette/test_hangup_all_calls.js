/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = "head.js";

function testHangUpAllCalls() {
  log("= testHangUpAllCalls =");

  let outCall;
  let inCall;
  let outNumber = "5555550101";
  let inNumber = "5555550201";
  let inNumber2 = "5555550301";
  let inInfo = gInCallStrPool(inNumber);
  let inInfo2 = gInCallStrPool(inNumber2);

  let disconnectingCalls;

  return Promise.resolve()
    .then(() => gSetupConference([outNumber, inNumber, inNumber2]))
    .then(calls => {
      [outNumber, inNumber, inNumber2] = calls;
      disconnectingCalls = calls;
    })
    .then(() => gHangUpAllCalls(disconnectingCalls))
    .then(() => gCheckAll(null, [], "", [], []));
}

// Start the test
startTest(function() {
  testHangUpAllCalls()
    .catch(error => ok(false, "Promise reject: " + error))
    .then(finish);
});
