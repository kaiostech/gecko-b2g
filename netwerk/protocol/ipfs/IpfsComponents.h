/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef IPFS_COMPONENTS_H_
#define IPFS_COMPONENTS_H_

#include "nsIProtocolHandler.h"
#include "nsCOMPtr.h"

namespace ipfs {

already_AddRefed<nsIProtocolHandler> ConstructIpfsProtocolHandler();

already_AddRefed<nsIProtocolHandler> ConstructIpnsProtocolHandler();

}  // namespace ipfs

#endif  // IPFS_COMPONENTS_H_
