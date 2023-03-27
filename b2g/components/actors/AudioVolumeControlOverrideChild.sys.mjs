/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export class AudioVolumeControlOverrideChild extends JSWindowActorChild {
  constructor() {
    super();
  }

  actorCreated() {
    this.contentWindow.addEventListener("audiovolumecontroloverride", this);
  }

  didDestroy() {
    this.contentWindow?.removeEventListener("audiovolumecontroloverride", this);
  }

  handleEvent(event) {
    if (event.type === "audiovolumecontroloverride") {
      this.sendQuery("AudioVolumeControlOverride:OnOverride", event.detail);
    }
  }
}
