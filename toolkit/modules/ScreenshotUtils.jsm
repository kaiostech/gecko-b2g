/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ScreenshotUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(
  this,
  "PromiseUtils",
  "resource://gre/modules/PromiseUtils.jsm"
);

var ScreenshotUtils = {
  getScreenshot(content, maxWidth, maxHeight, mimeType) {
    let deferred = PromiseUtils.defer();

    let takeScreenshotClosure = () => {
      this.takeScreenshot(content, maxWidth, maxHeight, mimeType, deferred);
    };

    let maxDelayMs = Services.prefs.getIntPref(
      "dom.webview.maxScreenshotDelayMS",
      /* default */ 2000
    );

    // Try to wait for the event loop to go idle before we take the screenshot,
    // but once we've waited maxDelayMS milliseconds, go ahead and take it
    // anyway.
    Cc["@mozilla.org/message-loop;1"]
      .getService(Ci.nsIMessageLoop)
      .postIdleTask(takeScreenshotClosure, maxDelayMs);

    return deferred.promise;
  },

  // Actually take a screenshot and foward the result up to our parent, given
  // the desired maxWidth and maxHeight (in CSS pixels), and given the
  // message manager id associated with the request from the parent.
  takeScreenshot(content, maxWidth, maxHeight, mimeType, deferred) {
    // You can think of the screenshotting algorithm as carrying out the
    // following steps:
    //
    // - Calculate maxWidth, maxHeight, and viewport's width and height in the
    //   dimension of device pixels by multiply the numbers with
    //   window.devicePixelRatio.
    //
    // - Let scaleWidth be the factor by which we'd need to downscale the
    //   viewport pixel width so it would fit within maxPixelWidth.
    //   (If the viewport's pixel width is less than maxPixelWidth, let
    //   scaleWidth be 1.) Compute scaleHeight the same way.
    //
    // - Scale the viewport by max(scaleWidth, scaleHeight).  Now either the
    //   viewport's width is no larger than maxWidth, the viewport's height is
    //   no larger than maxHeight, or both.
    //
    // - Crop the viewport so its width is no larger than maxWidth and its
    //   height is no larger than maxHeight.
    //
    // - Set mozOpaque to true and background color to solid white
    //   if we are taking a JPEG screenshot, keep transparent if otherwise.
    //
    // - Resolve with a screenshot of the page's viewport scaled and cropped per
    //   above.
    let devicePixelRatio = content.devicePixelRatio;

    let maxPixelWidth = Math.round(maxWidth * devicePixelRatio);
    let maxPixelHeight = Math.round(maxHeight * devicePixelRatio);

    let contentPixelWidth = content.innerWidth * devicePixelRatio;
    let contentPixelHeight = content.innerHeight * devicePixelRatio;

    let scaleWidth = Math.min(1, maxPixelWidth / contentPixelWidth);
    let scaleHeight = Math.min(1, maxPixelHeight / contentPixelHeight);

    let scale = Math.max(scaleWidth, scaleHeight);

    let canvasWidth = Math.min(
      maxPixelWidth,
      Math.round(contentPixelWidth * scale)
    );
    let canvasHeight = Math.min(
      maxPixelHeight,
      Math.round(contentPixelHeight * scale)
    );

    var canvas = content.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "canvas"
    );

    let transparent = mimeType !== "image/jpeg";
    if (!transparent) {
      canvas.mozOpaque = true;
    }
    canvas.width = canvasWidth;
    canvas.height = canvasHeight;

    let ctx = canvas.getContext("2d", { willReadFrequently: true });
    ctx.scale(scale * devicePixelRatio, scale * devicePixelRatio);

    let flags =
      ctx.DRAWWINDOW_DRAW_VIEW |
      ctx.DRAWWINDOW_USE_WIDGET_LAYERS |
      ctx.DRAWWINDOW_DO_NOT_FLUSH |
      ctx.DRAWWINDOW_ASYNC_DECODE_IMAGES;
    ctx.drawWindow(
      content,
      0,
      0,
      content.innerWidth,
      content.innerHeight,
      transparent ? "rgba(255,255,255,0)" : "rgb(255,255,255)",
      flags
    );

    // Take a JPEG screenshot by default instead of PNG with alpha channel.
    // This requires us to unpremultiply the alpha channel, which
    // is expensive on ARM processors because they lack a hardware integer
    // division instruction.
    canvas.toBlob(blob => {
      deferred.resolve(blob);
    }, mimeType);
  },
};

Object.freeze(ScreenshotUtils);
