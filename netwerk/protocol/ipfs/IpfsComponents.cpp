/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "IpfsComponents.h"

namespace {

// Implemented in Rust.
extern "C" {
void ipfs_construct(nsIProtocolHandler** aResult);
void ipns_construct(nsIProtocolHandler** aResult);
}

}  // namespace

namespace ipfs {

already_AddRefed<nsIProtocolHandler> ConstructIpfsProtocolHandler() {
  printf_stderr("Ipfs: ConstructIpfsProtocolHandler\n");
  nsCOMPtr<nsIProtocolHandler> handler;
  ipfs_construct(getter_AddRefs(handler));
  return handler.forget();
}

already_AddRefed<nsIProtocolHandler> ConstructIpnsProtocolHandler() {
  printf_stderr("Ipfs: ConstructIpnsProtocolHandler\n");
  nsCOMPtr<nsIProtocolHandler> handler;
  ipns_construct(getter_AddRefs(handler));
  return handler.forget();
}

}  // namespace ipfs
