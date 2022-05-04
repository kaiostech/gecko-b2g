/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintSettingsServiceGonk_h
#define nsPrintSettingsServiceGonk_h

#include "nsPrintSettingsService.h"
#include "nsIPrintSettings.h"

class nsPrintSettingsServiceGonk final : public nsPrintSettingsService {
 public:
  nsPrintSettingsServiceGonk() {}

  nsresult _CreatePrintSettings(nsIPrintSettings** _retval) override;
};

#endif  // nsPrintSettingsServiceGonk_h
