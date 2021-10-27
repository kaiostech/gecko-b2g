/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- /
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const charKeys = "abcdefghijklmnopqrstuvwxyz@,.+-_/()%*=&#:;!?0123456789";

// special keys that don't have specified keyCode
const specialKeys = [
  "SoftLeft",
  "SoftRight",
  "Call",
  "EndCall",
  "ArrowUp",
  "ArrowRight",
  "ArrowDown",
  "ArrowLeft",
  "Sym",
  "Message",
  "Power",
  "SpeechInputToggle",
  "ContextMenu",
  "Home",
];

const KeycapMap = {
  Space: {
    detail: {
      key: "Space",
      keyCode: 32,
    },
  },
  Backspace: {
    detail: {
      key: "Backspace",
      keyCode: 8,
    },
  },
  Shift: {
    detail: {
      key: "Shift",
      keyCode: 16,
    },
  },
  Enter: {
    detail: {
      key: "Enter",
      keyCode: 13,
    },
  },
  Alt: {
    detail: {
      key: "Alt",
      keyCode: 18,
    },
  },
};

Array.from(charKeys).forEach((aChar) => {
  KeycapMap[aChar] = {
    detail: {
      key: aChar,
      keyCode: aChar.charCodeAt(),
    },
  };
});

specialKeys.forEach((aChar) => {
  KeycapMap[aChar] = {
    detail: {
      key: aChar,
      keyCode: 0,
    },
  };
});

function sendKeyboardEvent(eventName, detail) {
  const contentWindow = document.getElementById("systemapp").contentWindow;
  contentWindow.focus();

  const keg = new contentWindow.KeyboardEventGenerator();
  const keyEvent = new contentWindow.KeyboardEvent(eventName, detail);
  keg.generate(keyEvent);
}

document.getElementById("controls").addEventListener("mousedown", (evt) => {
  const key = evt.target.dataset.key;
  if (key && KeycapMap[key]) {
    sendKeyboardEvent("keydown", KeycapMap[key].detail);
  }
});

document.getElementById("controls").addEventListener("mouseup", (evt) => {
  const key = evt.target.dataset.key;
  if (key && KeycapMap[key]) {
    sendKeyboardEvent("keyup", KeycapMap[key].detail);
  }
});

window.addEventListener("systemappframeprepended", () => {
  let typeFlag;
  let isKeypadEnabled = true;
  let desktopSize;
  try {
    const args = Cc[
      "@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"
    ].getService(Ci.nsISupports).wrappedJSObject.cmdLine;
    typeFlag = args.handleFlagWithParam("type", false) || "touch";
    desktopSize = args.handleFlagWithParam("size", false) || "1366x768";
  } catch (e) {
    console.error(e);
  }

  const systemAppFrame = document.getElementById("systemapp");
  let kind = typeFlag;
  if (typeFlag === "session" || typeFlag === "desktop") {
    kind = "touch";
  }
  systemAppFrame.setAttribute("kind", kind);
  switch (typeFlag) {
    case "bartype":
      systemAppFrame.style.width = "240px";
      systemAppFrame.style.height = "320px";
      break;
    case "qwerty":
      systemAppFrame.style.width = "320px";
      systemAppFrame.style.height = "240px";
      break;
    case "desktop":
      const [width, height] = desktopSize.split("x");
      systemAppFrame.style.width = `${width}px`;
      systemAppFrame.style.height = `${height}px`;
      isKeypadEnabled = false;
      break;
    case "session":
      systemAppFrame.style.width = `${screen.width}px`;
      systemAppFrame.style.height = `${screen.height}px`;
      isKeypadEnabled = false;
      break;
    default:
      typeFlag = "mobile"
      systemAppFrame.style.width = "480px";
      systemAppFrame.style.height = "854px";
      isKeypadEnabled = false;
  }

  Services.prefs.setCharPref("b2g.session-type", typeFlag);

  if (isKeypadEnabled) {
    document.getElementById("controls").classList.add("show");
  }
});
