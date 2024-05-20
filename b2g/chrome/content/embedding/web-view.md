# \<web-view\> custom element API.

## Attribute

The <web-view> element supports the following attributes:
- `src` : the url of the page to load.
- `remote` : a boolean to decide if that browser should load content in a content process.
- `ignorefocus` : a boolean that when set let the browser get pointer events without focusing it. This is useful for virtual keyboard frames.
- `transparent` : if true, the background of the browser will be transparent instead of white.

## Methods

- `activateKeyForwarding() : void` : enable key forward to this web-view first.
- `blur() : void` : blurs the browser.
- `cleanup() : void` : releases resources before removing the <web-view> from the DOM. The <web-view> is not usable after this call.
- `deactivateKeyForwarding() : void` : disable key forwarding.
- `disableCursor() : void` : disables the virtual cursor.
- `download(uri) : void` : init download.
- `enableCursor() : void` : enables the virtual cursor.
- `enterModalState() : void` : Set into a state where scripts are frozen and events suppressed.
- `executeScript(source) : Promise<jsval>` : Execute the script source in a sandbox of the loaded document and returns the script value.
- `focus() : void` : focuses the browser.
- `getBackgroundColor() : Promise<String>`: returns the CSS value of the page's background color.
- `getCursorEnabled() : Promise<boolean>` : query whether the virtual cursor is enabled.
- `getScreenshot(max_width, max_height, mime_type) : Promise<Blob>` : takes a screenshot of the current page.
- `goForward() : void` : navigates one step forwared in the history.
- `goBack() : void` : navigates one step back in the history.
- `leaveModalState() : void` : Resume normal state where scripts can run and events are delivered.
- `reload(forced) : void` : reload the patch, bypassing the cache if `forced` is true.
- `scrollToTop(smooth = true) : void` : scrolls to the top of the document.
- `scrollToBottom(smooth = true) : void` : scrolls to the bottom of the document.
- `stop() : void` : stops the current page loading.
- `enterSelectionMode() : void` : Switch the virtual cursor to selection mode, virtual cursor must be enabled.
- `exitSelectionMode() : void` : Exit the virtual cursor from selection mode.
- `startSelection() : void` : Start selecting contents, virtual cursor must be enabled.
- `stopSelection() : void` : Stop selecting contents, clear the selection, virtual cursor must be enabled.

## Properties

- `(readonly) allowedAudioChannels` : returns the list of audio channel supported for this element.
- `(readonly) frame` : returns a handle to the underlying browser. Use with care!
- `src : string` : mirror of the `src` attribute.
- `(readonly) canGoForward : boolean` : return `true` if calling `goForward()` would be effective.
- `(readonly) canGoBack : boolean` : return `true` if calling `goBack()` would be effective.
- `active : boolean` : control the active state of the browser's docShell.
- `(readonly) processid : int` : returns the process ID of its content process if there is, or -1 if there is not.
- `visible : boolean`: currently similar to `active`.
- `(readonly) currentURI : string` : returns the current URI loaded in the web-view, which can be different from the `src` attribute.
- `fullZoom` : controls the overall zoom level of the page.
- `textZoom` : controls the zoom level of the page's text.
- `openWindowInfo` : setting this property is mandatory to fully create the web-view. A null value is valid.
- `userAgent : string` : The User Agent used by the web-view. It mirrors the value of the useragent attribute, and can only be set by the system app.
- `userAgentExtensions : string` : Custom extensions to be appended to the User Agent for the web-view. The final User Agent will be reflected in the `userAgent` property.
- `(readonly) mediaController`: an instance of the [MediaController interface](https://hg.mozilla.org/mozilla-central/file/tip/dom/chrome-webidl/MediaController.webidl), usable to keep track of media playback in the tab.

## Events

All events are CustomEvents, with an event payload specific to each type.

- `close` : `{}`
- `contextmenu` : `{ <depends on the element that was selected> }`
- `documentfirstpaint` : `{}`
- `error` : `{ type: string }`
- `iconchange` : `{ href: string, sizes: string, rel: string }`
- `loadend` : `{ backgroundColor: string}`
- `loadstart` : `{}`
- `locationchange` : `{ url: string, canGoBack: boolean, cangoForward: boolean}`
- `manifestchange` : `{ href: string}`
- `opensearch` : `{ title: string, href: string}`
- `processready`: `{ processid: int}`
- `promptpermission` :
  - `detail` object of the `promptpermission` event:
    - `requestId` : string type, a unique request id, such as "permission-prompt-{4bd60020-01aa-4f77-ace6-a1ee28f021f1}".
    - `origin` : string type, the origin of the permission requester.
    - `requestAction` : string type, "prompt" means to prompt to user, "cancel" means to cancel the request.
    - `permissions` : an object with keys of permission types, such as `{"video-capture": {"action": "prompt", "options": ["back", "front"]}}`.
      - `action` : string type, the current permission setting of the permission type for the origin, might be "prompt" or "allow".
      - `options` : an array of options, such as ["back", "front"] for "video-capture" permission type, could be empty if no choice to make.
  - expected event after sending `promptpermission` with requestAction == "prompt":
    - event type : `requestId`
    - event detail: `{ origin: string, choices: {}, granted: bool, remember: bool }`
      - `origin` : string type, the origin of the permission requester.
      - `granted` : bool type, granted or not.
      - `remember` : bool type, remember the decision or not.
      - `choices` : an object with keys of permission types, such as `{"video-capture": "back"}`.
- `recordingstatus` : `{ audio: boolean, video: boolean }`
  - dispatched when the recording status of this page is updated. `detail.audio` is true when the page is capturing audio through `MediaDevices.getUserMedia()` or recording audio through camera API. Same goes for `detail.video`.
- `resize` : `{ width: int, height: int}`
- `scroll` : `{ top: int, left: int}`
- `scrollareachanged` : `{ width: int, height: int}`
- `securitychange` : `{ state: string, mixedState: string, extendedValidation: boolean, mixedContent: boolean }`
- `showmodalprompt` : `{ type: string, detail: {} }`
  - `type` : A custom event named `showmodalprompt`.
  - `detail` object of the custom event.
    - `promptType` : string type, refer to `Prompter.sys.mjs`, value could be `alert`, `alertCheck`, `confirm`, `confirmCheck`, `prompt`
    - `title` : string type, title of dialog.
    - `text` : string type, text context in dialog.
    - `unblock(returnValue)` : function, app need to call this function to unblock waiting response after `returnValue` is set.
                               App will set checkState if dialog has checkbox.
      - `returnValue` : object of event.detail and app fill in dialog response including checkbox and if `OK` or `Cancel` clicked.
        | prompType | value in returnValue should be filled. |
        | :--- | :--- |
        | `alert` | N/A |
        | `alertCheck` | boolean: `returnValue.checked` represent checkBox's state. |
        | `confirm` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. |
        | `confirmCheck` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. <br> boolean: `returnValue.checked` represent checkBox's state. |
        | `confirmEx` | boolean: `returnValue.checked` represent checkBox's state. <br> int: `returnValue.buttonNumClicked` index of button pressed. 0-2, default 0. |
        | `prompt` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. <br> boolean: `returnValue.checked` represent checkBox's state. <br> string: `returnValue.value` user input string. |
        | `promptUsernameAndPassword` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. <br> string: `returnValue.user` user input string. <br> string: `returnValue.pass` user input string. |
        | `promptPassword` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. <br> string: `returnValue.pass` user input string. |
        | `select` | boolean: `returnValue.ok` true for `OK`, false for `Cancel`. <br> int: `returnValue.selected` index of item been selected. |
- `titlechange` : `{ title: string }`
- `visibilitychange` : `{ visible: boolean }`
- `httponmodifyrequest` : `{ uri: string, host: string }`
  - dispatched when the <web-view> element starts loading a main document.
