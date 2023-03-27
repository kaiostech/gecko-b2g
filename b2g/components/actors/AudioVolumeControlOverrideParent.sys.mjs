/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export class AudioVolumeControlOverrideParent extends JSWindowActorParent {
  constructor() {
    super();
  }

  receiveMessage(message) {
    let browser = this.browsingContext.top.embedderElement;
    if (!browser) {
      return;
    }
    let win = browser.ownerGlobal;
    let keg = new win.KeyboardEventGenerator();
    keg.generate(new win.KeyboardEvent("keydown", { key: message.data }));
    keg.generate(new win.KeyboardEvent("keyup", { key: message.data }));
  }
}
