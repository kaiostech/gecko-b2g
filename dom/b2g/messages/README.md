# SystemMessage API

System messages are messages from the system, they are sent by the system based on their types, for example, a system message with name *alarm* is sent to its subscriber when a timer goes off; a system message with name *sms-received* is sent when the system receives an sms message. **SystemMessage API** gives applications the ability to subscribe system messages of particular names, and events `SystemMessageEvent` will dispatched to its registered ServiceWorker in different scenarios.

## Message types

| **Name**                                  | **Permission**       |
| :-                                        | :-                   |
| activity                                  |                      |
| alarm                                     | alarms               |
| bluetooth-dialer-command                  | bluetooth-privileged |
| bluetooth-map-request                     | bluetooth-privileged |
| bluetooth-opp-receiving-file-confirmation | bluetooth-privileged |
| bluetooth-opp-transfer-complete           | bluetooth-privileged |
| bluetooth-opp-transfer-start              | bluetooth-privileged |
| bluetooth-opp-update-progress             | bluetooth-privileged |
| bluetooth-pairing-aborted                 | bluetooth-privileged |
| bluetooth-pairing-request                 | bluetooth-privileged |
| bluetooth-pbap-request                    | bluetooth-privileged |
| cellbroadcast-received                    | cellbroadcast        |
| data-sms-received                         | sms                  |
| icc-stkcommand                            | settings:read, settings:write |
| media-button                              |                      |
| networkstats-alarm                        | networkstats-manage  |
| sms-delivery-error                        | sms                  |
| sms-delivery-success                      | sms                  |
| sms-failed                                | sms                  |
| sms-received                              | sms                  |
| sms-sent                                  | sms                  |
| system-time-change                        | system-time:read |
| telephony-call-ended                      | telephony            |
| telephony-hac-mode-changed                | telephony            |
| telephony-new-call                        | telephony            |
| telephony-tty-mode-changed                | telephony            |
| ussd-received                             | mobileconnection     |
| wappush-received                          | wappush              |

## For front-end and application developers

To subscribe and receive system messages, an app has to have an active service worker.

### Subscribe system messages from application manifest

First, we need to specify the registration info of service worker in manifest, the format is as follows

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

Second, subscribe the names of system messages in manifest.

For example:
```javascript
"messages": [
  "alarm",
  "sms-received"
],
```

### Subscribe system messages manually in scripts

Apps can use `SystemMessageManager.subscribe()` to subscribe system messages on an active service worker. `systemMessageManager` is a property of `ServiceWorkerRegistration`, which returns a reference to the `SystemMessageManager` interface.

##### Subscribe from window script
```javascript
navigator.serviceWorker.register('your_service_worker_script.js').then(registration => {
  registration.systemMessageManager.subscribe('name_of_systemmessage').then(
    () => {
      console.log(`Subscribe system message success!`);
    },
    error => {
      console.log(`Subscribe system message failed: ${error}`);
    }
  );
});
```

##### Subscribe from service worker script
```javascript
self.registration.systemMessageManager.subscribe('name_of_systemmessage').then(
  () => {
    console.log(`Subscribe system message success!`);
  },
  error => {
    console.log(`Subscribe system message failed: ${error}`);
  }
);
```

### Receive system messages

System messages are delivered as `SystemMessageEvent`, which is an ExtendableEvent and will be dispatched to `ServiceWorkerGlobalScope`.

<dl>
<b>SystemMessageEvent</b>
    <dt>name</dt>
    <dd>The type of this system message.</dd>
    <dt>data</dt>
    <dd>Returns an object <code>SystemMessageData</code> which wraps the details of this system message.</dd>
</dl>

<dl>
<b>SystemMessageData</b>
    <dt>json()</dt>
    <dd>Might throw, extracts the message data as a JSON object, not available for <em>activity</em> messages.</dd>
    <dt>WebActivityRequestHandler()</dt>
    <dd>Might throw, only available for <em>activity</em> messages, details stated in <code>WebActivity API</code>.</dd>
</dl>

```javascript
self.addEventListener('systemmessage', (event) => {
  console.log(`Receive systemmessage event with system message of name: ${event.name}`);
  try {
    console.log(` with data detail: ${event.data.json()}`);
  } catch (error) {
    console.log(`${error}`);
  }
});
```

## For gecko developers

`SystemMessageService` provides an interface for module developers to send system messages to target applications, or broadcasting messages to all subscribers.

**Syntax**
```
void sendMessage(in AString messageName, in jsval message, in ACString origin);

void broadcastMessage(in AString messageName, in jsval message);
```

**Example**
```javascript
let sm = Cc["@mozilla.org/systemmessage-service;1"].getService(
  Ci.nsISystemMessageService
);
let msg = { test1: "aaa", test2: "bbb" };
let appOrigin = "http://calculator.localhost";
sm.sendMessage("alarm", msg, appOrigin);
```
