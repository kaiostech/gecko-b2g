/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const COMMAND_MAP = {
  cut: "cmd_cut",
  copy: "cmd_copy",
  paste: "cmd_paste",
  selectall: "cmd_selectAll",
};

export class SelectionActionChild extends JSWindowActorChild {
  _isCommandEnabled(cmd) {
    let command = COMMAND_MAP[cmd];
    if (!command) {
      return false;
    }

    let context = this.manager.browsingContext;
    let docShell = context.docShell;

    return docShell.isCommandEnabled(command);
  }

  _getFrameOffset(currentWindow) {
    // Get correct offset in case of nested iframe.
    const offset = {
      left: 0,
      top: 0,
    };

    while (currentWindow.realFrameElement) {
      const frameElement = currentWindow.realFrameElement;
      currentWindow = frameElement.ownerGlobal;

      // The offset of the iframe window relative to the parent window
      // includes the iframe's border, and the iframe's origin in its
      // containing document.
      const currentRect = frameElement.getBoundingClientRect();
      const style = currentWindow.getComputedStyle(frameElement);
      const borderLeft = parseFloat(style.borderLeftWidth) || 0;
      const borderTop = parseFloat(style.borderTopWidth) || 0;
      const paddingLeft = parseFloat(style.paddingLeft) || 0;
      const paddingTop = parseFloat(style.paddingTop) || 0;

      offset.left += currentRect.left + borderLeft + paddingLeft;
      offset.top += currentRect.top + borderTop + paddingTop;
    }

    // Now we have coordinates relative to the root content document's
    // layout viewport. Subtract the offset of the visual viewport
    // relative to the layout viewport, to get coordinates relative to
    // the visual viewport.
    var offsetX = {};
    var offsetY = {};
    currentWindow.windowUtils.getVisualViewportOffsetRelativeToLayoutViewport(
      offsetX,
      offsetY
    );
    offset.left -= offsetX.value;
    offset.top -= offsetY.value;

    return offset;
  }

  handleEvent(event) {
    switch (event.type) {
      case "mozcaretstatechanged": {
        event.stopPropagation();

        const offset = this._getFrameOffset(event.target.defaultView);
        const rect = event.boundingClientRect;

        let detail = {
          rect: {
            width: rect.width,
            height: rect.height,
            left: rect.left + offset.left,
            top: rect.top + offset.top,
            right: rect.right + offset.left,
            bottom: rect.bottom + offset.top,
          },
          commands: {
            canSelectAll: this._isCommandEnabled("selectall"),
            canCut: this._isCommandEnabled("cut"),
            canCopy: this._isCommandEnabled("copy"),
            canPaste: this._isCommandEnabled("paste"),
          },
          reason: event.reason,
          collapsed: event.collapsed,
          caretVisible: event.caretVisible,
          selectionVisible: event.selectionVisible,
          selectionEditable: event.selectionEditable,
          selectedTextContent: event.selectedTextContent,
        };

        this.sendAsyncMessage("caret-state-changed", detail);

        break;
      }
    }
  }

  receiveMessage(message) {
    let context = this.manager.browsingContext;
    let docShell = context.docShell;

    switch (message.name) {
      case "selectionaction-do-command":
        if (this._isCommandEnabled(message.data.command)) {
          docShell.doCommand(COMMAND_MAP[message.data.command]);
        }
        break;
    }
  }
}
