/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_VirtualCursorProxy_h
#define mozilla_dom_VirtualCursorProxy_h

#include "CursorSimulator.h"

class nsPIDOMWindowInner;

namespace mozilla {
namespace dom {

// interfaces for cursor simulator to control the cursor.
class nsIVirtualCursor : public nsISupports {
 public:
  virtual void UpdatePos(const LayoutDeviceIntPoint& aPos) = 0;
  virtual void CursorClick() = 0;
  virtual void CursorDown() = 0;
  virtual void CursorUp() = 0;
  virtual void CursorMove() = 0;
  virtual void CursorOut(bool aCheckActive = false) = 0;
  virtual void ShowContextMenu() = 0;
};

class VirtualCursorProxy final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  VirtualCursorProxy(nsPIDOMWindowOuter* aWindow, nsIVirtualCursor* aDelegate,
                     const LayoutDeviceIntPoint& aChromeOffset);

  void RequestEnable();
  void RequestDisable();
  void IsEnabled(bool* aEnabled);
  void UpdateScreenSize(int32_t aWidth, int32_t aHeight);
  void UpdateChromeOffset(const LayoutDeviceIntPoint& aChromeOffset);
  void SetSelectionMode(CursorSimulator::SelectionMode aMode);
  bool GetSelectionActive();

 private:
  virtual ~VirtualCursorProxy();
  void DestroyCursor();

  nsCOMPtr<nsPIDOMWindowOuter> mOuterWindow;
  RefPtr<CursorSimulator> mSimulator;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_VirtualCursorProxy_h
