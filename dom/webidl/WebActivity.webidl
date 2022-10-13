/* -*- Mode: IDL; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

[GenerateConversionToJS, GenerateInit]
dictionary WebActivityOptions {
  required DOMString name;
           any data = null;
};

[Exposed=(Window,Worker)]
interface WebActivity {
  [Throws]
  constructor(DOMString name, optional any data = null);

  [Throws]
  Promise<any> start();

  undefined cancel();
};
