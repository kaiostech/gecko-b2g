/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRilWorkerService_H
#define nsRilWorkerService_H

#include "nsCOMPtr.h"
#include <nsIRilWorkerService.h>
#include <nsISupportsImpl.h>
#include <nsRilWorker.h>
#include <nsTArray.h>

class nsRilWorkerService final : public nsIRilWorkerService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIRILWORKERSERVICE

  static already_AddRefed<nsRilWorkerService> CreateRilWorkerService();

  nsRilWorkerService();

private:
  ~nsRilWorkerService();
  nsTArray<RefPtr<nsRilWorker>> mRilWorkers;
  int32_t mNumRilWorkers;
};
#endif
