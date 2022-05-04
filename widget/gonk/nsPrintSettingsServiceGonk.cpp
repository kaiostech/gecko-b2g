/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsPrintSettingsServiceGonk.h"

#include "nsPrintSettingsImpl.h"

class nsPrintSettingsGonk : public nsPrintSettings {
 public:
  nsPrintSettingsGonk() {
    // The aim here is to set up the objects enough that silent printing works
    SetOutputFormat(nsIPrintSettings::kOutputFormatPDF);
    SetPrinterName(u"PDF printer"_ns);
  }
};

nsresult nsPrintSettingsServiceGonk::_CreatePrintSettings(
    nsIPrintSettings** _retval) {
  nsPrintSettings* printSettings = new nsPrintSettingsGonk();
  NS_ENSURE_TRUE(printSettings, NS_ERROR_OUT_OF_MEMORY);
  NS_ADDREF(*_retval = printSettings);
  (void)InitPrintSettingsFromPrefs(*_retval, false,
                                   nsIPrintSettings::kInitSaveAll);
  return NS_OK;
}

already_AddRefed<nsIPrintSettings> CreatePlatformPrintSettings(
    const mozilla::PrintSettingsInitializer& aSettings) {
  RefPtr<nsPrintSettings> settings = new nsPrintSettingsGonk();
  settings->InitWithInitializer(aSettings);
  return settings.forget();
}
