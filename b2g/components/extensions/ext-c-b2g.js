"use strict";

extensions.registerModules({
  tabs: {
    url: "chrome://b2g/content/ext-c-tabs.js",
    scopes: ["addon_child"],
    paths: [["tabs"]],
  },
});
