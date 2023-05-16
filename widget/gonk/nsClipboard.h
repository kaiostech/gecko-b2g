/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsClipbard_h__
#define nsClipbard_h__

#include "GonkClipboardData.h"
#include "mozilla/UniquePtr.h"
#include "nsBaseClipboard.h"

class nsClipboard final : public ClipboardSetDataHelper {
 public:
  nsClipboard();

  NS_DECL_ISUPPORTS_INHERITED

  // nsIClipboard
  NS_IMETHOD GetData(nsITransferable* aTransferable,
                     int32_t aWhichClipboard) override;
  NS_IMETHOD EmptyClipboard(int32_t aWhichClipboard) override;
  NS_IMETHOD HasDataMatchingFlavors(const nsTArray<nsCString>& aFlavorList,
                                    int32_t aWhichClipboard,
                                    bool* _retval) override;
  NS_IMETHOD IsClipboardTypeSupported(int32_t aWhichClipboard,
                                      bool* _retval) override;
  RefPtr<mozilla::GenericPromise> AsyncGetData(
      nsITransferable* aTransferable, int32_t aWhichClipboard) override;
  RefPtr<DataFlavorsPromise> AsyncHasDataMatchingFlavors(
      const nsTArray<nsCString>& aFlavorList, int32_t aWhichClipboard) override;

  protected:
  // Implement the native clipboard behavior.
  NS_IMETHOD SetNativeClipboardData(nsITransferable* aTransferable,
                                    nsIClipboardOwner* aOwner,
                                    int32_t aWhichClipboard) override;
 
 private:
  ~nsClipboard() = default;
  mozilla::UniquePtr<mozilla::GonkClipboardData> mClipboard;
};

#endif
