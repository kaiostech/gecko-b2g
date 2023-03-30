# PermissionsManager API
PermissionsManager provides the interfaces to fetch and add the permission settings of the specified origin.

## Get the permissions settings of other apps
Apps can simply get the permissions settings by calling `get`, for example:
```javascript
navigator.b2g.permissions.get("camera", "https://camera.local").then(...);
```

## Set the permissions settings
Apps can simply set the permissions settings by calling `set`, for example:
```javascript
navigator.b2g.permissions.set("camera", "allow", "https://camera.local")
```
