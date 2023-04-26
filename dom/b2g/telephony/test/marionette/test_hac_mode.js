/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

startTest(function() {
  log('= test_hac_mode =');
  return Promise.resolve()
    .then(() => {
      telephony.hacMode = true;
      is(telephony.hacMode, true);
      telephony.hacMode = false;
      is(telephony.hacMode, false);
      finish();
    });
});
