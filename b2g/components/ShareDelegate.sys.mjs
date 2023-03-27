/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const domBundle = Services.strings.createBundle(
  "chrome://global/locale/dom/dom.properties"
);

export class ShareDelegate {
  init(aParent) {
    this._openerWindow = aParent;
  }

  get openerWindow() {
    return this._openerWindow;
  }

  async share(aTitle, aText, aUri /* as a nsIURI */) {
    // No need to check that at least one parameter is not null, since this is
    // done before in Navigator::Share()

    // Our activities usually expect the payload to have a "type" property indicating
    // which data they will get, and the data itself. That doesn't map directly to this
    // api where multiple parameters can be set.
    //
    // We need to use heuristic here, since for instance WhatsApp expects `type` to be
    // set to `text/plain` for text, and the value as an item of a `blobs` array like
    // the Bluetooth app.
    //
    // To accomodate all these variations, the activitiy data is made of:
    // - `type` which is 'url' if a uri is passed or 'text/plain' otherwise.
    // - the values of `title`, `text` and `url` on their own.
    // - a `blobs` array with the available text values and their mime type.

    // Default for when aText or aTitle are set.
    let type = "text/plain";
    if (aUri) {
      // Expected by the SMS app for instance.
      type = "url";
    }

    // Add all the parameters to the `blobs` array with a matching mime type.
    let blobs = [];
    if (aText) {
      blobs.push(new Blob([aText], { type: "text/plain" }));
    }
    if (aTitle) {
      blobs.push(new Blob([aTitle], { type: "text/plain" }));
    }
    if (aUri) {
      blobs.push(new Blob([aUri], { type: "text/x-uri" }));
    }

    // Create the activity with the type, raw values and blobs.
    let activity = new this._openerWindow.WebActivity("share", {
      type,
      url: aUri?.spec,
      title: aTitle,
      text: aText,
      blobs,
    });

    // Launch the activity and convert any error to a DOM AbortError exception.
    try {
      let result = await activity.start();
      // Not all `share` providers return a useful value...
      if (result === false) {
        throw new DOMException(
          domBundle.GetStringFromName("WebShareAPI_Failed"),
          "DataError"
        );
      }
    } catch (e) {
      throw new DOMException(
        domBundle.GetStringFromName("WebShareAPI_Aborted"),
        "AbortError"
      );
    }
  }
}

ShareDelegate.prototype.classID = Components.ID(
  "{1201d357-8417-4926-a694-e6408fbedcf8}"
);
ShareDelegate.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsISharePicker",
]);
