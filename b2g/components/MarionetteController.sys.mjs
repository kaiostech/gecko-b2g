/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
*/

export var MarionetteController;

#ifdef HAS_KOOST_MODULES
import { MarionetteRunner } from "resource://gre/modules/MarionetteRunner.sys.mjs";

MarionetteController = {
  enableRunner() {
    MarionetteRunner.run();
  }
}
#endif
