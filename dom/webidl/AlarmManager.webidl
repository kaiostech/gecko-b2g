/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * https://wiki.mozilla.org/WebAPI/AlarmAPI
 */

// Wrap all parameters into AlarmOptions so that workers can pass them to main
// thread.
[GenerateConversionToJS]
dictionary AlarmOptions {
  required any date;
  boolean ignoreTimezone = true;
  any data = null;
};

[Exposed=(Window,Worker),
  Pref="dom.alarm.enabled"]
  interface AlarmManager {
    Promise<any> getAll();
    Promise<long> add(AlarmOptions options);
    undefined remove(long id);
  };
