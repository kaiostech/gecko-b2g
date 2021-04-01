/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const window = Services.wm.getMostRecentWindow("navigator:browser");

const OrientationChangeHandler = {
  // Clockwise orientations, looping
  orientations: [
    "portrait-primary",
    "landscape-secondary",
    "portrait-secondary",
    "landscape-primary",
    "portrait-primary",
  ],

  lastOrientation: "portrait-primary",

  init() {
    window.screen.addEventListener("mozorientationchange", this, true);
    this.lastOrientation = window.screen.mozOrientation;
  },

  handleEvent(evt) {
    // console.log(
    //   `OrientationChangeHandler: ${this.lastOrientation} -> ${window.screen.mozOrientation}`
    // );
    const system = window.document.getElementById("systemapp");
    if (!system) {
      console.error(`OrientationChangeHandler: no system app frame found!`);
      return;
    }

    let newOrientation = window.screen.mozOrientation;
    let orientationIndex = this.orientations.indexOf(this.lastOrientation);
    let nextClockwiseOrientation = this.orientations[orientationIndex + 1];
    let fullSwitch =
      newOrientation.split("-")[0] == this.lastOrientation.split("-")[0];

    this.lastOrientation = newOrientation;

    let angle, xFactor, yFactor;
    if (fullSwitch) {
      angle = 180;
      xFactor = 1;
    } else {
      angle = nextClockwiseOrientation == newOrientation ? 90 : -90;
      xFactor = window.innerWidth / window.innerHeight;
    }
    yFactor = 1 / xFactor;

    system.style.transition = "";
    system.style.transform = `rotate(${angle}deg) scale(${xFactor}, ${yFactor})`;

    const trigger = () => {
      // console.log(
      //   `OrientationChangeHandler: triggering orientation change transition`
      // );
      system.style.transition = "transform .25s cubic-bezier(.15, .7, .6, .9)";

      system.style.opacity = "";
      system.style.transform = "";
    };

    // 180deg rotation, no resize
    if (fullSwitch) {
      window.setTimeout(trigger);
      return;
    }

    window.addEventListener("resize", trigger, { once: true });
  },
};

OrientationChangeHandler.init();
