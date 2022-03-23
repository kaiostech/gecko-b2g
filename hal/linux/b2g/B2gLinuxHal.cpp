/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "B2gLinuxHal.h"

namespace {

// Implemented in Rust.
extern "C" {
void b2glinuxhal_construct(nsIB2gLinuxHal** aResult);
}

}  // namespace

namespace b2ghal {

already_AddRefed<nsIB2gLinuxHal> ConstructB2gLinuxHal() {
  printf_stderr("B2gLinuxHal: ConstructB2gLinuxHal\n");
  nsCOMPtr<nsIB2gLinuxHal> xpcom;
  b2glinuxhal_construct(getter_AddRefs(xpcom));
  return xpcom.forget();
}

}  // namespace b2ghal
