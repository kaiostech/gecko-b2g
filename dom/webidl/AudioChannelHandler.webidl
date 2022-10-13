/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[ChromeOnly,
 Exposed=Window]
interface AudioChannelHandler : EventTarget {
  readonly attribute AudioChannel name;

  // This event is dispatched when this audiochannel is actually in used by the
  // app or one of the sub-iframes.
  attribute EventHandler onactivestatechanged;

  [Throws]
  Promise<float> getVolume();

  [Throws]
  Promise<undefined> setVolume(float aVolume);

  [Throws]
  Promise<boolean> getMuted();

  [Throws]
  Promise<undefined> setMuted(boolean aMuted);

  [Throws]
  Promise<boolean> isActive();

  [Throws]
  static sequence<AudioChannelHandler> generateAllowedChannels(
      FrameLoader aFrameLoader);
};
