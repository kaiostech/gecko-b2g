/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// <gaia-wm> custom element: a simplistic window manager.

class WindowManager extends HTMLElement {
  constructor() {
    // console.log(`Creating WindowManager`);
    super();

    // Hooks up webview state to the UI.
    // FIXME: Do something fancy instead??
    this.ui_updater = state => {
      let statusbar = document.getElementById("statusbar");
      if (!statusbar) {
        return;
      }

      // console.log(`Updating UI state with: ${JSON.stringify(state)}`);
      statusbar.update_state(state);

      if (state.canGoBack) {
        document.getElementById("action-go-back").removeAttribute("disabled");
      } else {
        document
          .getElementById("action-go-back")
          .setAttribute("disabled", "true");
      }

      if (state.canGoForward) {
        document
          .getElementById("action-go-forward")
          .removeAttribute("disabled");
      } else {
        document
          .getElementById("action-go-forward")
          .setAttribute("disabled", "true");
      }

      if (state.isHomescreen) {
        document
          .getElementById("action-close")
          .setAttribute("disabled", "true");
        document.getElementById("action-home").setAttribute("disabled", "true");
      } else {
        document.getElementById("action-close").removeAttribute("disabled");
        document.getElementById("action-home").removeAttribute("disabled");
      }
    };
  }

  connectedCallback() {
    // FIXME: We can't use the shadow DOM here because that makes loading <web-view> fail.
    // let shadow = this.attachShadow({ mode: "open" });

    this.container = document.createElement("main");
    this.container.innerHTML = `<link rel="stylesheet" href="style/window_manager.css">`;
    this.appendChild(this.container);

    let options = {
      root: this.container,
      rootMargin: "0px",
      threshold: [0, 1],
    };

    let intersection_callback = (entries, observer) => {
      entries.forEach(entry => {
        // console.log(
        //   `Intersection: isIntersecting=${
        //     entry.isIntersecting
        //   } target=${entry.target.getAttribute("id")} ratio=${
        //     entry.intersectionRatio
        //   }`
        // );

        // Change the active status of the webview based on its visibility in
        // the container.
        entry.target.active = entry.isIntersecting;
        let frame_id = entry.target.getAttribute("id");
        if (entry.isIntersecting && entry.intersectionRatio == 1) {
          // This is the "really" active frame, use it as a source of UI state.
          console.log(`Activating frame ${frame_id}`);
          this.frames[frame_id].activate();
          this.active_frame = frame_id;
        } else if (this.frames[frame_id]) {
          // The frame may have been removed if we just closed it.
          this.frames[frame_id].deactivate();
        }
      });
    };

    this.intersection_observer = new IntersectionObserver(
      intersection_callback,
      options
    );

    this.frames = {};
    this.frame_id = 0;
    this.active_frame = null;
  }

  // Open a new url with a given configuration.
  // Recognized configuration properties:
  // isHomescreen (bool) : the homescreen gets a transparent background and can't be closed.
  // activate (bool) : if true, selects this frame as the active one.
  open_frame(url = "about:blank", config = {}, aParams = null) {
    console.log(`WindowManager::open ${url} ${JSON.stringify(config)}`);
    let attr_id = `frame${this.frame_id}`;
    this.frame_id += 1;
    let webview = document.createElement("web-view");
    // Bug 109000, we must set the openWindowInfo.
    webview.openWindowInfo = aParams ? aParams.openWindowInfo : null;
    webview.setAttribute("remote", "true");
    webview.setAttribute("id", attr_id);
    if (config.isHomescreen) {
      this.homescreen_id = attr_id;
      webview.setAttribute("transparent", true);
    }
    this.intersection_observer.observe(webview);
    this.container.appendChild(webview);
    this.frames[attr_id] = new WebViewHandler(webview, config, this.ui_updater);
    webview.src = url;

    if (config.activate) {
      window.location = `#${attr_id}`;
    }

    return webview;
  }

  go_back() {
    this.active_frame && this.frames[this.active_frame].go_back();
  }

  go_forward() {
    this.active_frame && this.frames[this.active_frame].go_forward();
  }

  close_frame(id = "<current>") {
    let frame = null;
    if (id === "<current>") {
      id = this.active_frame;
      frame = this.frames[this.active_frame];
    } else if (this.frames[id]) {
      frame = this.frames[id];
    }

    if (id == this.homescreen_id) {
      console.error("WindowManager: can't close the homescreen!!");
      return;
    }

    if (frame) {
      this.container.removeChild(frame.web_view);
      frame = null;
      delete this.frames[id];

      // Go to the homescreen.
      this.go_home();
    }
  }

  go_home() {
    if (this.homescreen_id) {
      window.location = `#${this.homescreen_id}`;
    }
  }
}

customElements.define("gaia-wm", WindowManager);

// A lightweight wrapper around a <web-view> element, managing events.
class WebViewHandler {
  constructor(web_view, config, ui_updater) {
    this.web_view = web_view;

    let handler = this.handle_event.bind(this);

    // Properties used to sync the UI state with this web-view state.
    this.state = {
      url: "",
      title: "",
      secure: false,
      iconUrl: "",
      iconSize: 0,
      canGoBack: false,
      canGoForward: false,
      isHomescreen: config.isHomescreen,
    };

    this.activated = false;
    this.ui_updater = ui_updater;

    // Attaching all event listeners.
    [
      "close",
      "contextmenu",
      "documentfirstpaint",
      "iconchange",
      "loadstart",
      "loadend",
      "manifestchange",
      "metachange",
      "opensearch",
      "titlechange",
      "locationchange",
      "securitychange",
      "visibilitychange",
      "error",
      "showmodalprompt",
      "scrollareachanged",
    ].forEach(event_name => {
      web_view.addEventListener(`${event_name}`, handler);
    });
  }

  activate() {
    this.activated = true;
    if (this.ui_updater) {
      this.ui_updater(this.state);
    }
  }

  deactivate() {
    this.activated = false;
  }

  update_ui() {
    if (this.activated && this.ui_updater) {
      this.ui_updater(this.state);
    }
  }

  go_back() {
    this.web_view.goBack();
  }

  go_forward() {
    this.web_view.goForward();
  }

  handle_event(event) {
    let detail = event.detail;
    let ui_update_needed = false;

    switch (event.type) {
      case "titlechange":
        this.state.title = detail.title;
        ui_update_needed = true;
        break;
      case "securitychange":
        this.state.secure = detail.state;
        ui_update_needed = true;
        break;
      case "locationchange":
        // We don't reset the icon url until we get a new one.
        this.state.iconSize = 0;
        this.state.canGoBack = detail.canGoBack;
        this.state.canGoForward = detail.canGoForward;
        this.state.url = detail.url;
        ui_update_needed = true;
        break;
      case "visibilitychange":
        this.update_ui();
        break;
      case "showmodalprompt":
        let type = event.detail.promptType || event.detail.type;
        console.log(`Handling ${type}`);
        event.detail.unblock && event.detail.unblock();
        break;
      case "scrollareachanged":
        console.log(
          `Handling scrollareachanged width:${event.detail.width} height:${event.detail.height}`
        );
        break;
    }

    if (ui_update_needed) {
      this.update_ui();
    }
  }

  // Update the url of the icon, trying to use the "best" one.
  iconchanged(data) {
    let max_size = this.state.iconSize;
    let rel = data.rel || "icon";
    let found = false;

    rel.split(" ").forEach(rel => {
      let size = 0;
      if (rel == "icon" || rel == "shortcut") {
        size = 32;
      }

      if (rel == "apple-touch-icon" || rel == "apple-touch-icon-precomposed") {
        // Default size according to https://developer.apple.com/library/archive/documentation/AppleApplications/Reference/SafariWebContent/ConfiguringWebApplications/ConfiguringWebApplications.html
        size = 57;
      }

      // If there is a `sizes` property, trust it.
      if (data.sizes === "any") {
        // Scalable icon, can't beat that!
        this.state.iconUrl = data.href;
        this.state.iconSize = 1000000;
        found = true;
        return;
      }

      if (data.sizes) {
        data.sizes
          .toLowerCase()
          .split(" ")
          .forEach(item => {
            let width = item.split("x")[0];
            if (width > size) {
              size = width;
              this.state.iconUrl = data.href;
            }
          });
      }

      if (size > max_size) {
        max_size = size;
        found = true;
        this.state.iconUrl = data.href;
        this.state.iconSize = size;
      }
    });

    // We have a new icon, update the UI state.
    if (found) {
      this.update_ui();
    }
  }
}
