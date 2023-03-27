/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const Screenshot = {
  async get(window) {
    let document = window.document;
    let browsingContext = window.browsingContext;

    let snapshot = await browsingContext.currentWindowGlobal.drawSnapshot(
      null /* rect */,
      1 /* scale */,
      "rgb(255,255,255)" /* background color */
    );

    let canvas = document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "html:canvas"
    );
    let context = canvas.getContext("2d");
    canvas.width = snapshot.width;
    canvas.height = snapshot.height;
    context.drawImage(snapshot, 0, 0);

    return new Promise(resolve => {
      canvas.toBlob(blob => {
        resolve(blob);
      });

      snapshot.close();
    });
  },
};
