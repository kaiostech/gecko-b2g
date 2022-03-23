/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef B2G_LINUX_HAL_H_
#define B2G_LINUX_HAL_H_

#include "nsIB2gLinuxHal.h"
#include "nsCOMPtr.h"

namespace b2ghal {

already_AddRefed<nsIB2gLinuxHal> ConstructB2gLinuxHal();

}  // namespace b2ghal

#endif  // B2G_LINUX_HAL_H_
