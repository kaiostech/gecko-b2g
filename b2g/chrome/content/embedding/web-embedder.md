# WebEmbedder API

The WebEmbedder API is exposed to the system app by including the script from `chrome://b2g/content/embedding/web-embedder.js`.

The embedder is responsible for providing delegates that are used for each functional area exposed to the embedder when it creates a `WebEmbedder` object:

```js
const embedder = new WebEmbedder({
  windowProvider: windowProvider,
  processSelector: processSelector
});
embedder.addEventListener("runtime-ready", e => {
  console.log(`Embedder event: ${e.type}`);
  ...
});
```

## WebEmbedder object

The `WebEmbedder` constructor accepts one parameter used to provide the supported delegates:
```js
const embedder = new WebEmbedder({
  windowProvider: <WindowProvider implementation>,
  processSelector: <ProcessSelector implementation>,
  notifications: <Notifications implementation>,
  activityChooser: <ActivityChooser implementation>,
  imeHandler: <imeHandler implementation>,
  webExtensions: <WebExtensions implementation>,
});
```
All delegates are optional.

### Methods

The `WebEmbedder` object exposes the following methods:
- `launchPreallocatedProcess()`: this will create a new content process with no content.
- `addSystemEventListener(type, listener, useCapture)`: proxies the `Services.els` method of the same name, using the shell document as a target.
- `removeSystemEventListener(type, listener, useCapture)`: proxies the `Services.els` method of the same name, using the shell document as a target.
- `systemAlerts`: this object exposes methods for invoking notification features of the system.
  - `resendAll()`: this will resend all stored notifications from the Notifications DB.
  - `click(data)`: this will perform click action. The data parameter is an object with the following properties:
    - `id`: the unique id for this notification.
    - `action`: the string ID of the notification button the user clicked.
  - `close(id)`: this will perform close action with a parameter id which is the unique id for this notification.
- `isDaemonReady`: returns a boolean that is true if the api-daemon is running and is usable.
- `isGonk`: returns true if this is a device build.
- `takeScreenshot`: take a screenshot and return a `File` object with png file, if something failed it will return `null`.
- `customAccessible`: this object exposes methods for invoking customAccessible features.
  - `send(domNode)`: send the custom accessible to screen reader.
    - `domNode`: the dom node for custom accessible.
  - `startOutput()`: enable custom accessible output.
  - `stopOutput()`: disable custom accessible output.
- `userIdle`: this object exposes methods for invoking userIdle features.
  - `addObserver(observer, idleTime)`: add an observer to be notified when the user is `idle` or `active`.
    - `observer`: callback function `observer(topic, idleTime)`.
      - `topic`: `idle` or `active`.
      - `idleTime`: user idle time.
    - `idleTime`: the amount of time in seconds the user should be idle.
  - `removeObserver(observer, idleTime)`: remove an observer registered with addObserver.
    - `observer`: callback function that needs to be removed.
    - `idleTime`: the amount of time in seconds.
- `doSelectionAction(action)`: send selection action to the active window, `action` can be one of ['cut', 'copy', 'paste', 'selectAll'].
- `launchRemoteWindows()`: this will open remote shell windows if the device supports multiscreen.
- `getContentProcesses()`: returns a list of process information items.

### Events

Since the `WebEmbedder` object extends `EventTarget` you can attach event listeners on it. The list of events that can be dispatched is:
- `runtime-ready`: this event is dispatched once the the embedder is setup. At this point you can safely call methods on the object itself.
- `daemon-disconnected`: this event is dispatched when the api-daemon connectivity is lost.
- `daemon-reconnected`: this event is dispatched when the api-daemon connectivity is available again.
- `headphones-status-changed`: this event is dispatched when the headphone is plugged or unplugged. `CustomEvent.detail` is a string representing the current headphone state that can be:
  - `off`
  - `headset`
  - `headphone`
  - `lineout`
  - `unknown`
- `default-volume-channel-changed`: this event is dispatched when the audio channel that can be adjusted by hardware volume keys is changed. `CustomEvent.detail` is a string representing its audio channel type.
- `bluetooth-volumeset`: this event is dispatched when Bluetooth handsfree repots its volume. Its `detail` is a integer from 0 to 15 representing speaker gain level.
- `mtp-state-changed`: this event is dispatched when MTP server is started or disabled. There are two status, `started` and `finished`. `started` means MTP is enabled and `finished` means MTP is disabled.
- `volume-state-changed`: this event is dispatched when volume state changed, the state includes `Init`, `NoMedia`, `Idle`, `Pending`, `Checking`, `Mounted`, `Unmounting`, `Formatting`, `Shared`, `Shared-Mounted`, `Check-Mounted` and `MountFail`.
- `almost-low-disk-space`: this event is dispatched when available space across the threshold. Its `detail` is a bool representing the statement `the available space is lower than threshold` is true/false.
- `geolocation-status`: this event is dispatched when geolocation status changes. Its `detail.active` is a bool representing active/inactive.
- `captive-portal-login-request`: this event is dispatched when captive portal detection redirect to a login page. Its `detail` is an object as { type, id, url }, type contains a string of this event, id contains a string of number which has increment 1 when each time the event is sent, and url contains a string of login page url.
- `captive-portal-login-result`: this event is dispatched when captive portal login process is finished. Its `detail` is an object as { result, id }, result contains a boolean ture if login successfully, otherwise false. The id corresponds to the id from `captive-portal-login-request`, same id means that they are request/result pair.
- `caret-state-changed`: this event is dispatched when the accessible-caret changes. See [`CaretStateChangedEvent.webidl`](https://searchfox.org/mozilla-central/source/dom/webidl/CaretStateChangedEvent.webidl) for `detail`.
- `sw-registration-done`: this event is dispatched when all service workers defined in webmanifests has done processing registrations during system bootup.
- `activity-aborted`: this event is dispatched when a processing activity is aborted by shutdown of handler's service worker, shutdown of handler's service worker hosted process, or cancel of activity requester.
    - `Event.detail` is ```{ reason: "activity-canceled/process-shutdown/service-worker-shutdown", id: "activity uid", name: "activity name", caller: "caller origin", handler: "handler origin" }```
- `activity-opened`: this event is dispatched when caller has start an activity, a.k.a. activity.start().
    - `Event.detail` is ```{ id: "activity uid", name: "activity name", caller: "caller origin" }```
- `activity-closed`: this event is dispatched when a started activity is done processing, a.k.a. activity.start() is resolve/reject.
    - `Event.detail` is ```{ id: "activity uid", name: "activity name", caller: "caller origin", handler: "handler origin" }```

## WindowProvider delegate

This delegate is used when the plaform needs to create a new top level window. It's mostly a reflection of the `nsIBrowserDOMWindow` interface, hence the documentation at `https://searchfox.org/mozilla-central/source/dom/interfaces/base/nsIBrowserDOMWindow.idl` can be used. One key difference is that `openXXX()` and `createXXX()` methods need to return a `<web-view>` element.

### Methods

- `openURI(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal, aCsp)`.
- `createContentWindow(aURI, aOpener, aWhere, aFlags, aTriggeringPrincipal, aCsp)`.
- `openURIInFrame(aURI, aParams, aWhere, aFlags, aName)`.
- `createContentWindowInFrame(aURI, aParams, aWhere, aFlags, aName)`.
- `canClose()`.

## ProcessSelector delegate

This delegate allows the embedder to control how content processes are reused or created when an out of process page is loaded.

## Methods

- `provideProcess(aType, aProcesses, aMaxCount)` returns the index of the process to reuse or -1 to create a new process.

- `suggestServiceWorkerProcess(aScope)` returns a suggested content process pid for a service worker to spawn at. Return 0 as default behavior, and gecko will select a pid randomly. aScope is the scope of service worker which will be spawned.

## Notifications delegate

This delegate is responsible for displaying desktop notifications UI.

## Methods

- `showNotification(notification)`. The `notification` parameter is an object with the following properties:
  - `type`: a string equals to "desktop-notification".
  - `id`: a unique id for this notification.
  - `icon`: the url of the icon for this notification.
  - `image`: the url of the image for this notification.
  - `title`: the notification title.
  - `text`: the notification text.
  - `dir`: whether this notification should be displayed in ltr or rtl direction.
  - `lang`: the language code for this notification.
  - `origin`: the origin the notification is sent from.
  - `timestamp`: when the notification was triggered.
  - `data`: Arbitrary data that you want associated with the notification.
  - `requireInteraction`: Indicates that a notification should remain active until the user clicks or dismisses it, rather than closing automatically.
  - `actions`: A array of NotificationAction objects, each describing a single action the user can choose within a notification.
  - `silent`: whether this notification should be silent.
  - `mozbehavior`: a dictionary indicates custom notification behavior.
  - `serviceWorkerRegistrationScope`: the unique identifier for the service worker registration the notification is sent from.

- `closeNotification(id)`: Notifies the system UI that it should close the notification created with this id.

## ActivityChooser delegate

This delegate let the system UI display the list of potential choices when an several app can fulfill an activity request.

## Methods

- `choseActivity(detail)` returns a promise that resolves with the chosen app as `{ id: <opaque id>, value: <index in the choice> }`. If no activity is selected, `value` should be undefined or set to -1. The `detail` parameter is an object with the following properties:
  - `id`: an opaque id representing this activity, to be used in the resolved object.
  - `name`: the activity name.
  - `choices`: an array of `{ manifest: <url>, icon: <url>}` representing the apps that can provide the activity.

## ScreenReaderProvider delegate

This delegate is responsible for screen reader.

## Methods

- `showScreenReader(data)`. The `data` parameter is an object with the following properties:
 - `type`: the event type. Valid value: `accessibility-output`.
 - `details`: the PresentationData of accessible.

## imeHandler delegate

This delegate handle focus/blur from IME to inform system useful information.

## Methods

- `focusChanged(detail)` The `detail` parameter is an object with following properties:
  - `isFocus`: an boolean to identify focus or blur event it got.
  - `type`: tagName of element.
  - `inputType`: the element's type attribute. Types for `<input> and <textarea>`.
  - `value`: the current value of the control.
  - `max`: the element's max attribute, containing the maximum (numeric or date-time) value for this item.
  - `min`: the element's min attribute, containing the minimum (numeric or date-time) value for this item.
  - `lang`: the element's lang attribute.
  - `inputMode`: the element's `x-inputmode` attribute.
  - `voiceInputSupported`: an boolean to identify voiceInputSupport or not.
  - `name`: the element's name attribute.
  - `selectionStart`: unsigned long. The element's selectionStart attribute.
  - `selectionEnd`: unsigned long. The element's selectionEnd attribute.
  - `maxLength`: the element's maxlength attribute
  - `imeGroup`: the `imegroup` attribute of the element to be focused.
  - `lastImeGroup`: the `imegroup` attribute of the last focused element.
  - `activeEditable`: current active EditableSupport.

### EditableSupport

A wrapper which represents the editing element and provides interfaces to control it.

### Methods
- `setSelectedOption`: setup selected option of the current editing element.
- `setSelectedOptions`: setup selected options of the current editing element.
- `removeFocus`: remove focus from the current editing element.

## WebExtensions delegate

This delegates provides the interface to various WebExtension functionalities: install prompts, management of browser and page actions, etc.

### Methods

- `permissionPrompt(extension)`: Called when the user should be prompted to confirm a WebExtension installation. The `extension` parameter is an object representing the extension. This method returns a promise that is expected to resolve or reject to indicate the user's decision.
- `setProvider(<WebExtensionProvider)`: Gives to the embedder a handle to an object that can be used to notify of actions or events. This method is guaranteed to be the first one called on the delegate.
- `updateBrowserAction(extensionId, tabId, action)`
  
## WebExtensionProvider object

### Methods

- `browserActionClick(id)`: indicates that the user clicked on a browser action icon.
