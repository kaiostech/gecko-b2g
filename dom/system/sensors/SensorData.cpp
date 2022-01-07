/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SensorData.h"

#include "nsISupportsImpl.h"

namespace b2g {

NS_IMPL_ISUPPORTS(SensorData, nsISensorData)

SensorData::SensorData(nsISensors::SensorType aType, double aX, double aY, double aZ, double aW)
    : mType(aType), mX(aX), mY(aY), mZ(aZ), mW(aW) {}

NS_IMETHODIMP SensorData::GetType(nsISensors::SensorType *aType) {
  NS_ENSURE_ARG_POINTER(aType);
  *aType = mType;
  return NS_OK;
}

NS_IMETHODIMP SensorData::GetX(double *aX) {
  NS_ENSURE_ARG_POINTER(aX);
  *aX = mX;
  return NS_OK;
}

NS_IMETHODIMP SensorData::GetY(double *aY) {
  NS_ENSURE_ARG_POINTER(aY);
  *aY = mY;
  return NS_OK;
}

NS_IMETHODIMP SensorData::GetZ(double *aZ) {
  NS_ENSURE_ARG_POINTER(aZ);
  *aZ = mZ;
  return NS_OK;
}

NS_IMETHODIMP SensorData::GetW(double *aW) {
  NS_ENSURE_ARG_POINTER(aW);
  *aW = mW;
  return NS_OK;
}

} // namespace b2g
