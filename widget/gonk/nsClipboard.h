/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipbard_h__
#define nsClipbard_h__

#include "GonkClipboardData.h"
#include "mozilla/UniquePtr.h"
#include "nsBaseClipboard.h"

class nsClipboard final : public nsBaseClipboard {
 public:
  nsClipboard();

  NS_DECL_ISUPPORTS_INHERITED

 protected:
  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) override;
  NS_IMETHOD GetNativeClipboardData(nsITransferable* aTransferable,
                                    int32_t aWhichClipboard) override;
  nsresult EmptyNativeClipboardData(int32_t aWhichClipboard) override;
  mozilla::Result<int32_t, nsresult> GetNativeClipboardSequenceNumber(
      int32_t aWhichClipboard) override;
  mozilla::Result<bool, nsresult> HasNativeClipboardDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;

 private:
  ~nsClipboard() = default;
  mozilla::UniquePtr<mozilla::GonkClipboardData> mClipboard;
  // One sequence number is enough since gonk supports global-clipboard only.
  uint32_t mSequenceNumber = 0;
};

#endif
