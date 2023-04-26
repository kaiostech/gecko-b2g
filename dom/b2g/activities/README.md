# WebActivity API

The **WebActivity API** provides a way for applications to delegate an activity to another application. An *activity* is something a user wants to do: pick an image, play a video, etc. Apps can declare themselves as an activity hander, for example, Gallery App and installed 3rd-party photo-manage Apps can all be candidates when users request to pick photos. Usually such list of choices are provided to users.

The `WebActivity` interface exposes to both *Window* and *Worker* scope, in the window scope, creation of WebActivity is allowed when the requester application is visible (in the foreground). In the worker scope, WebActivity is allowed iff permission **worker-activity** is declared and granted, otherwise an insecure error will throw and the creation of WebActivity returns nullptr.

## Starting an activity

The constructor of `WebActivity` takes a name of the activity and an optional Object specifying its filter type or other information, for example:

```javascript
let activity = new WebActivity('pick', { type: 'image/ipeg' });
```

In this example, we are trying to initiate a "pick" activity, and the handlers should support providing at least jpeg images. Please note that the constructor may throw and return a null object with illegal access.

Then, start the activity by `start()` method, if an activity handler (i.e. Gallery App) is launched and the request is handled successfully, the promise will be resolved with the picking result.

```
activity.start().then(
  result => {
    console.log(`Result pass back from activity handler: ${result}`);
  },
  error => {
    console.log(`Failed: ${error}`);
  }
);
```

Please note that one WebActivity instance starts once, duplicate calls of start() is rejected.

## Cancel an activity

To cancel a started activity, use `cancel()`, and the pending promise of `start()` will also be rejected.

```javascript
activity.cancel();
```

## Handle an activity

Once the user has picked an activity handler (or picked by System App secretly), a `SystemMessageEvent`, which is an ExtendableEvent, with name *activity* will be dispatched to its `ServiceWorkerGlobalScope`. Unlike other system message events, `SystemMessageData` has a property `WebActivityRequestHandler`, which provides methods to resolve or reject the pending promise of `activity.start()`.

Please note that per spec of ServiceWorkerGlobalScope[1], the activity handler is not persisted across the termination/restart cycle of worker. See [ServiceWorkerGlobalScope](https://developer.mozilla.org/en-US/docs/Web/API/ServiceWorkerGlobalScope) for more details.

>[1] Developers should keep in mind that the ServiceWorker state is not persisted across the termination/restart cycle, so each event handler should assume it's being invoked with a bare, default global state.

When handling `systemmessage` event of name *activity*, handler can decide whether to launch the application window to perform user interaction, by using ServiceWorker API, `Clients.openWindow()`. Or can pass back results with `WebActivityRequestHandler.postResult()` directly. Suggest using `postMessage()` method to communicate between main scripts and service worker script.

Use `WebActivityRequestHandler.postError()` to send back an error message if something goes wrong.

`WebActivityRequestHandler.source` is a `WebActivityOptions` object, representing information sent from activity requester.

<dl>
<b>WebActivityOptions</b>
    <dt>name</dt>
    <dd>A string represents the name of the activity request.</dd>
    <dt>data</dt>
    <dd>An object represents data that is sent from activity requester.</dd>
</dl>

```javascript
self.addEventListener('systemmessage', event => {
  if (event.name === 'activity') {
    let handler = event.data.webActivityRequestHandler();
    let fakeResult = { fakedata: 'some fake data.' };
    handler.postResult(fakeResult);
  }
});
```
<dl>
<b>WebActivityRequestHandler</b>
    <dt>source</dt>
    <dd>A <code>WebActivityOptions</code> object contains the information about the current activity. Set by the app who starts the activity.</dd>
    <dt>postResult()</dt>
    <dd>Send back response and resolve the promise of app that starts the activity, takes a structured clone JS Object.</dd>
    <dt>postError()</dt>
    <dd>Send back error message and reject the promise of app that starts the activity, takes a DOMString of error reasons.</dd>
</dl>

### Open its window with Clients.openWindow

We provide an additional options to the method of `Clients.openWindow`, syntax below:

```javascript
self.clients.openWindow(url, ClientWindowOptions).then(function(windowClient) {
  // Do something with your WindowClient
});
```

If you like to open your app window in "activity app style", please specify `disposition: 'inline'` in the options of openWindow, for example:
```javascript
self.addEventListener('systemmessage', event => {
  event.waitUntil(
    clients
      .openWindow('/page_to_open.html', { disposition: 'inline' })
      .then(windowClient => {
        // Success.
      })
  );
});
````

Other typs of disposition:
<br>
<code>**window**</code> |default
<br>Open in a new window.

<code>**inline**</code>
<br>Open in an activity window.

<code>**attention**</code>
<br>Open in an attention window.


## Register an App as an activity handler

App can register itself as an activity handler to handle one or more activities. Besides declaring activity related information in the app manifest, app has to set up the service worker script in its manifest first.

### Set up service worker registration

```javascript
"serviceworker": {
  "script_url": "script_url",
  "options": {
    "scope": "scope_of_sw",
    "update_via_cache": "value_of_update_via_cache"
  }
},
```
The `options` object is optional, as defined in [ServiceWorkerContainer.register()#Syntax](https://developer.mozilla.org/en-US/docs/Web/API/ServiceWorkerContainer/register#Syntax), for example:

`script_url` refers to the URL of service worker script, relative to the root directory of your site, and by default, the `scope` value for a service worker registration is set to the directory where the service worker script is located. So in the following example, service worker will loaded as `site_root_directory/your_service_worker_script.js`, and will control pages underneath it.

```javascript
"serviceworker": {
  "script_url": "your_service_worker_script.js"
},
```

In the most case the above example is good enough.

### Set up activity handler registration

In general, please reference  [Registering an App as an activity handler](https://developer.mozilla.org/en-US/docs/Archive/B2G_OS/API/Web_Activities#Registering_an_App_as_an_activity_handler)



#### New attributes and updates for activity handler description

~~<code>**href**</code>~~
<br>No need to specify the href anymore.

~~<code>**disposition**</code>~~
<br>No need to specify the disposition anymore.

<code>**filters**</code> |Optional
<br>Same as before.

<code>**returnValue**</code> |Optional
<br>Same as before.

<code>**allowedOrigins**</code> |Optional
<br>An array of origins. This is for handler to limit its usage to specific users, will filter out callers who's origin is not in the list. For example, setting `"allowedOrigins": ["http://testapp1.localhost", "http://testapp2.localhost"]`, only allows requests made from "http://testapp1.localhost" or "http://testapp2.localhost".
