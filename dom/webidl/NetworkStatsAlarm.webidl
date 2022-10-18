/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window,
 JSImplementation="@mozilla.org/networkstatsalarm;1",
 ChromeOnly,
 Pref="dom.networkStats.enabled"]
interface NetworkStatsAlarm {
  readonly attribute unsigned long alarmId;
  readonly attribute NetworkStatsInterface network;
  readonly attribute long long threshold;
  readonly attribute any data;
};
