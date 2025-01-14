/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

[Exposed=Window,
 JSImplementation="@mozilla.org/networkStatsdata;1",
 ChromeOnly,
 Pref="dom.networkStats.enabled"]
interface NetworkStatsData {
  readonly attribute unsigned long   rxBytes;   // Received bytes.
  readonly attribute unsigned long   txBytes;   // Sent bytes.
  readonly attribute object          date;      // Date.
};
