/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Handling of the navigation area buttons.

document.addEventListener(
  "DOMContentLoaded",
  () => {
    let wm = document.getElementById("wm");

    function click_action(action, func) {
      document
        .getElementById(`action-${action}`)
        .addEventListener("click", () => {
          func.call(wm);
        });
    }

    click_action("go-back", wm.go_back);
    click_action("go-forward", wm.go_forward);
    click_action("close", wm.close_frame);
    click_action("home", wm.go_home);

    document.getElementById("action-screenshot").onclick = async () => {
      let blob = await window.embedder.takeScreenshot();
      console.log(`takeScreenshot result: `, blob);
    };
  },
  { once: true }
);
