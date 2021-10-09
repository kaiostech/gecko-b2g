/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PasspointEventCallback_H
#define PasspointEventCallback_H

#include <utils/RefBase.h>

BEGIN_WIFI_NAMESPACE

struct PasspointEventCallback : public virtual android::RefBase {
  // Callback to indicate the ANQP response received.
  virtual void NotifyAnqpResponse(const nsACString& aIface,
                                  const nsAString& aBssid,
                                  AnqpResponseMap& aAnqpData) = 0;

  // Callback to indicate the icon response received.
  virtual void NotifyIconResponse(const nsACString& aIface,
                                  const nsAString& aBssid) = 0;

  // Callback to indicate the Wireless Network Management frame received.
  virtual void NotifyWnmFrameReceived(const nsACString& aIface,
                                      const nsAString& aBssid) = 0;
};

END_WIFI_NAMESPACE

#endif  // PasspointEventCallback_H
