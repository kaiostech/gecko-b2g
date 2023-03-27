/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export class SelectionActionParent extends JSWindowActorParent {
  receiveMessage(message) {
    switch (message.name) {
      case "caret-state-changed": {
        SelectionActionParent.setActor(this);

        Services.obs.notifyObservers(
          { wrappedJSObject: message.data },
          "caret-state-changed"
        );

        break;
      }
    }
  }

  static setActor(newActor) {
    if (SelectionActionParent._actor !== newActor) {
      SelectionActionParent._actor = newActor;
    }
  }

  static sendCommand(command) {
    SelectionActionParent._actor?.sendAsyncMessage(
      "selectionaction-do-command",
      { command }
    );
  }
}
