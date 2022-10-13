/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

dictionary RsuBlob
{
  DOMString data;    // The data blob from modem
};

dictionary RsuTimer
{
  long timer;        // The time of temporary connectivity 
};

dictionary RsuStatus
{
  long type;         // The locked type
  long long timer;   // The left time for temporary unlock
};

dictionary RsuVersion
{
  unsigned long minorVersion; // The highest major version of blob
  unsigned long maxVersion;   // The highest minor version of blob
};

[Exposed=Window]
interface RemoteSimUnlock {
  /**
   * To get the current lock status.
   *
   * @return RsuStatus.
   */
  [Throws]
  Promise<RsuStatus> getStatus();

  /**
   * To get the major/minor version of binary data.
   *
   * @return RsuVersion.
   */
  [Throws]
  Promise<RsuVersion> getVersion();

  /**
   * To generate binary data for unlock request.
   *
   * @return RsuBlob.
   */
  [Throws]
  Promise<RsuBlob> generateRequest();

  /**
   * To unlock device with blob from server.
   *
   * @param type
   *   The binary data for unlock modem.
   *
   * @return RsuBlob.
   */
  [Throws]
  Promise<RsuBlob> updataBlob(DOMString data);

  /**
   * To start a timer allowing temporary connectivity.
   *
   * @return RsuTimer.
   */
  [Throws]
  Promise<RsuTimer> openRF();

  /**
   * To close the temporary connectivity.
   *
   * @return Void.
   */
  [Throws]
  Promise<undefined> closeRF();

  /**
   * To get the version mode from property.
   *
   * @return DOMString.
   */
  [Throws]
  Promise<DOMString> getVersionMode();
};
