/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MobileConnectionInfo.h"

#include "mozilla/dom/ScriptSettings.h"

#include "jsapi.h"

#ifdef CONVERT_STRING_TO_NULLABLE_ENUM
#  undef CONVERT_STRING_TO_NULLABLE_ENUM
#endif
#define CONVERT_STRING_TO_NULLABLE_ENUM(_string, _enumType, _enum)          \
  {                                                                         \
    _enum.SetNull();                                                        \
                                                                            \
    uint32_t i = 0;                                                         \
    for (const EnumEntry* entry = _enumType##Values::strings; entry->value; \
         ++entry, ++i) {                                                    \
      if (_string.EqualsASCII(entry->value)) {                              \
        _enum.SetValue(static_cast<_enumType>(i));                          \
      }                                                                     \
    }                                                                       \
  }

#define CONVERT_NULLABLE_ENUM_TO_STRING(_enumType, _enum, _string)   \
  {                                                                  \
    if (_enum.IsNull()) {                                            \
      _string.SetIsVoid(true);                                       \
    } else {                                                         \
      uint32_t index = uint32_t(_enum.Value());                      \
      _string.AssignASCII(_enumType##Values::strings[index].value,   \
                          _enumType##Values::strings[index].length); \
    }                                                                \
  }

using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(MobileConnectionInfo, mWindow,
                                      mNetworkInfo, mDOMNetworkInfo, mCellInfo)

NS_IMPL_CYCLE_COLLECTING_ADDREF(MobileConnectionInfo)
NS_IMPL_CYCLE_COLLECTING_RELEASE(MobileConnectionInfo)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MobileConnectionInfo)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
  NS_INTERFACE_MAP_ENTRY(nsIMobileConnectionInfo)
NS_INTERFACE_MAP_END

MobileConnectionInfo::MobileConnectionInfo(nsPIDOMWindowInner* aWindow)
    : mConnected(false),
      mEmergencyCallsOnly(false),
      mRoaming(false),
      mWindow(aWindow) {}

MobileConnectionInfo::MobileConnectionInfo(
    const nsAString& aState, bool aConnected, bool aEmergencyCallsOnly,
    bool aRoaming, nsIMobileNetworkInfo* aNetworkInfo, const nsAString& aType,
    nsIMobileCellInfo* aCellInfo, int32_t aReasonDataDenied)
    : mConnected(aConnected),
      mEmergencyCallsOnly(aEmergencyCallsOnly),
      mRoaming(aRoaming),
      mReasonDataDenied(aReasonDataDenied) {
  // The instance created by this way is only used for IPC stuff. It won't be
  // exposed to JS directly, we will clone this instance to the one that is
  // maintained in MobileConnectionChild.

  // Update mState and mType
  CONVERT_STRING_TO_NULLABLE_ENUM(aState, MobileConnectionState, mState);
  CONVERT_STRING_TO_NULLABLE_ENUM(aType, MobileConnectionType, mType);

  // Update mNetworkInfo
  if (aNetworkInfo) {
    mNetworkInfo = new MobileNetworkInfo();
    mNetworkInfo->Update(aNetworkInfo);
  }

  // Update mCellInfo
  if (aCellInfo) {
    mCellInfo = new MobileCellInfo(mWindow);
    mCellInfo->Update(aCellInfo);
  }
}

void MobileConnectionInfo::Update(nsIMobileConnectionInfo* aInfo) {
  if (!aInfo) {
    return;
  }

  aInfo->GetConnected(&mConnected);
  aInfo->GetEmergencyCallsOnly(&mEmergencyCallsOnly);
  aInfo->GetRoaming(&mRoaming);

  // Update mReasonDataDenied
  aInfo->GetReasonDataDenied(&mReasonDataDenied);

  // Update mState
  nsAutoString state;
  aInfo->GetState(state);
  CONVERT_STRING_TO_NULLABLE_ENUM(state, MobileConnectionState, mState);

  // Update mType
  nsAutoString type;
  aInfo->GetType(type);
  CONVERT_STRING_TO_NULLABLE_ENUM(type, MobileConnectionType, mType);

  // Update mNetworkInfo
  nsCOMPtr<nsIMobileNetworkInfo> networkInfo;
  aInfo->GetNetwork(getter_AddRefs(networkInfo));
  if (networkInfo) {
    if (!mNetworkInfo) {
      mNetworkInfo = new MobileNetworkInfo();
    }
    mNetworkInfo->Update(networkInfo);
  } else {
    mNetworkInfo = nullptr;
  }

  // Update mCellInfo
  nsCOMPtr<nsIMobileCellInfo> cellInfo;
  aInfo->GetCell(getter_AddRefs(cellInfo));
  if (cellInfo) {
    if (!mCellInfo) {
      mCellInfo = new MobileCellInfo(mWindow);
    }
    mCellInfo->Update(cellInfo);
  } else {
    mCellInfo = nullptr;
  }
}

void MobileConnectionInfo::UpdateDOMNetworkInfo(
    nsIMobileConnectionInfo* aInfo) {
  if (!aInfo) {
    return;
  }

  // Update mDOMNetworkInfo
  nsCOMPtr<nsIMobileNetworkInfo> networkInfo;
  aInfo->GetNetwork(getter_AddRefs(networkInfo));
  if (networkInfo) {
    if (!mDOMNetworkInfo) {
      mDOMNetworkInfo = new DOMMobileNetworkInfo(mWindow);
    }
    mDOMNetworkInfo->Update(networkInfo);
  } else {
    mDOMNetworkInfo = nullptr;
  }
}

JSObject* MobileConnectionInfo::WrapObject(JSContext* aCx,
                                           JS::Handle<JSObject*> aGivenProto) {
  return MobileConnectionInfo_Binding::Wrap(aCx, this, aGivenProto);
}

// nsIMobileConnectionInfo

NS_IMETHODIMP
MobileConnectionInfo::GetState(nsAString& aState) {
  CONVERT_NULLABLE_ENUM_TO_STRING(MobileConnectionState, mState, aState);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetConnected(bool* aConnected) {
  *aConnected = Connected();
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetEmergencyCallsOnly(bool* aEmergencyCallsOnly) {
  *aEmergencyCallsOnly = EmergencyCallsOnly();
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetRoaming(bool* aRoaming) {
  *aRoaming = Roaming();
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetNetwork(nsIMobileNetworkInfo** aInfo) {
  if (mNetworkInfo) {
    NS_IF_ADDREF(*aInfo = mNetworkInfo);
  }

  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetType(nsAString& aType) {
  CONVERT_NULLABLE_ENUM_TO_STRING(MobileConnectionType, mType, aType);
  return NS_OK;
}

NS_IMETHODIMP
MobileConnectionInfo::GetCell(nsIMobileCellInfo** aInfo) {
  NS_IF_ADDREF(*aInfo = GetCell());
  return NS_OK;
}

NS_IMETHODIMP MobileConnectionInfo::GetReasonDataDenied(
    int32_t* aReasonDataDenied) {
  *aReasonDataDenied = ReasonDataDenied();
  return NS_OK;
}
