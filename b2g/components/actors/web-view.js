/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Re-create the WebView class, which is in in chrome/embedding/web-view.js,
// and define the customElement <web-view> for content process to use.
{
  const classData = window.getWebViewClassData();
  delete window.getWebViewClassData;

  // A getter/setter is not allow to be exported,
  // we use the helper _getter()/_setter() instead.
  Object.keys(classData.properties).forEach(key => {
    const property = classData.properties[key];
    if (property.get) {
      property.get = function () {
        return this._getter(key);
      };
    }
    if (property.set) {
      property.set = function (val) {
        return this._setter(key, val);
      };
    }
  });

  const LocalClass = class extends HTMLElement {
    constructor() {
      super();
      this._constructorInternal(true);
    }
  };

  Object.defineProperties(LocalClass.prototype, classData.properties);
  Object.defineProperties(LocalClass, classData.staticProperties);

  window.customElements.define("web-view", LocalClass);
}
