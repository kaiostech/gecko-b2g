"use strict";

console.log(`ext-c-b2g.js loaded`);

extensions.registerModules({
  tabs: {
    url: "chrome://b2g/content/ext-c-tabs.js",
    scopes: ["addon_child"],
    paths: [["tabs"]],
  },
});
