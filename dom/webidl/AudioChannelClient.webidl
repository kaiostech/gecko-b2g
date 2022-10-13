/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[Exposed=Window]
interface AudioChannelClient : EventTarget {
  [Throws]
  constructor(AudioChannel aChannel);

  // Called when APP wants to occupy the audio channel which is specified in the
  // constructor. After calling this function, it doesn't mean that this channel
  // is immediately unmuted. APP may want to hook onstatechange handler in order
  // to be notified when muted state changes.
  // This function is useful for APP which wants background music/FM to be
  // muted.
  [Throws]
  undefined requestChannel();

  // Called when APP no longer wants to occupy the audio channel. APP must call
  // this function if it has called requestChannel().
  [Throws]
  undefined abandonChannel();

  // This event is dispatched when muted state changes. APP should check
  // channelMuted attribute to know current muted state.
  attribute EventHandler onstatechange;

  // When channelMuted is false, it means APP is allowed to play content through
  // this channel. It also means the competing channel may be muted, or its
  // volume may be reduced.
  // If the channel owned by us is suspended (muted), or APP did not request
  // channel, or APP has abandoned the channel, channelMuted is set to true.
  readonly attribute boolean channelMuted;
};
