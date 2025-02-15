/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ObexBase.h"

BEGIN_BLUETOOTH_NAMESPACE

//
// Internal functions
//

/**
 * Append byte array and length to header
 */
int AppendHeader(uint8_t aHeaderId, uint8_t* aRetBuf, int aBufferSize,
                 const uint8_t* aData, int aLength) {
  int headerLength = aLength + 3;
  int writtenLength = (headerLength < aBufferSize) ? headerLength : aBufferSize;

  aRetBuf[0] = aHeaderId;
  BigEndian::writeUint16(&aRetBuf[1], headerLength);
  memcpy(&aRetBuf[3], aData, writtenLength - 3);

  return writtenLength;
}

/**
 * Append 4-byte integer to header
 */
int AppendHeader(uint8_t aHeaderId, uint8_t* aRetBuf, int aValue) {
  aRetBuf[0] = aHeaderId;
  BigEndian::writeInt32(&aRetBuf[1], aValue);

  return 5;
}

/**
 * Append byte to header
 */
int AppendHeader(uint8_t aHeaderId, uint8_t* aRetBuf, bool aValue) {
  aRetBuf[0] = aHeaderId;
  aRetBuf[1] = (uint8_t) aValue;

  return 2;
}

//
// Exposed functions
//

int AppendHeaderName(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aName,
                     int aLength) {
  return AppendHeader(ObexHeaderId::Name, aRetBuf, aBufferSize, aName, aLength);
}

int AppendHeaderBody(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aBody,
                     int aLength) {
  return AppendHeader(ObexHeaderId::Body, aRetBuf, aBufferSize, aBody, aLength);
}

int AppendHeaderEndOfBody(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aBody,
                          int aLength) {
  return AppendHeader(ObexHeaderId::EndOfBody, aRetBuf, aBufferSize, aBody, aLength);
}

int AppendHeaderTarget(uint8_t* aRetBuf, int aBufferSize,
                       const uint8_t* aTarget, int aLength) {
  return AppendHeader(ObexHeaderId::Target, aRetBuf, aBufferSize, aTarget,
                      aLength);
}

int AppendHeaderWho(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aWho,
                    int aLength) {
  return AppendHeader(ObexHeaderId::Who, aRetBuf, aBufferSize, aWho, aLength);
}

int AppendAuthResponse(uint8_t* aRetBuf, int aBufferSize,
                       const uint8_t* aDigest, int aLength) {
  return AppendHeader(ObexHeaderId::AuthResponse, aRetBuf, aBufferSize, aDigest,
                      aLength);
}

int AppendHeaderType(uint8_t* aRetBuf, int aBufferSize, const uint8_t* aType,
                     int aLength) {
  return AppendHeader(ObexHeaderId::Type, aRetBuf, aBufferSize, aType, aLength);
}

int AppendHeaderAppParameters(uint8_t* aRetBuf, int aBufferSize,
                              const uint8_t* aAppParameters, int aLength) {
  return AppendHeader(ObexHeaderId::AppParameters, aRetBuf, aBufferSize,
                      aAppParameters, aLength);
}

int AppendAppParameter(uint8_t* aRetBuf, int aBufferSize, const uint8_t aTagId,
                       const uint8_t* aValue, int aLength) {
  // An application parameter is a [tag]-[length]-[value] triplet. The [tag] and
  // [length] fields are 1-byte length each.

  if (aBufferSize < aLength + 2) {
    // aBufferSize should be larger than size of AppParameter + header.
    BT_WARNING("Return buffer size is too small for the AppParameter");
    return 0;
  }

  aRetBuf[0] = aTagId;
  aRetBuf[1] = aLength;
  memcpy(&aRetBuf[2], aValue, aLength);

  return aLength + 2;
}

int AppendHeaderLength(uint8_t* aRetBuf, int aObjectLength) {
  return AppendHeader(ObexHeaderId::Length, aRetBuf, aObjectLength);
}

int AppendHeaderConnectionId(uint8_t* aRetBuf, int aConnectionId) {
  return AppendHeader(ObexHeaderId::ConnectionId, aRetBuf, aConnectionId);
}

int AppendHeaderSRM(uint8_t* aRetBuf, bool aEnabled) {
  return AppendHeader(ObexHeaderId::SingleResponseMode, aRetBuf, aEnabled);
}

int AppendHeaderEndOfBody(uint8_t* aRetBuf) {
  aRetBuf[0] = ObexHeaderId::EndOfBody;
  aRetBuf[1] = 0x00;
  aRetBuf[2] = 0x03;

  return 3;
}

void SetObexPacketInfo(uint8_t* aRetBuf, uint8_t aOpcode, int aPacketLength) {
  aRetBuf[0] = aOpcode;
  BigEndian::writeUint16(&aRetBuf[1], aPacketLength);
}

bool ParseHeaders(const uint8_t* aHeaderStart, int aTotalLength,
                  ObexHeaderSet* aRetHandlerSet) {
  const uint8_t* ptr = aHeaderStart;

  while (ptr - aHeaderStart < aTotalLength) {
    ObexHeaderId headerId = (ObexHeaderId)*ptr++;

    uint16_t contentLength = 0;

    // Defined in 2.1 OBEX Headers, IrOBEX 1.2
    switch (headerId >> 6) {
      case 0x00:
        // Null-terminated Unicode text, length prefixed with 2-byte
        // unsigned integer.
      case 0x01:
        // byte sequence, length prefixed with 2 byte unsigned integer.
        contentLength = BigEndian::readUint16(ptr) - 3;
        ptr += 2;
        break;

      case 0x02:
        // 1 byte quantity
        contentLength = 1;
        break;

      case 0x03:
        // 4 byte quantity
        contentLength = 4;
        break;
    }

    // Length check to prevent from memory pollution.
    if (ptr + contentLength > aHeaderStart + aTotalLength) {
      // Severe error occurred. We can't even believe the received data, so
      // clear all headers.
      MOZ_ASSERT(false);
      aRetHandlerSet->ClearHeaders();
      return false;
    }

    aRetHandlerSet->AddHeader(new ObexHeader(headerId, contentLength, ptr));
    ptr += contentLength;
  }

  return true;
}

END_BLUETOOTH_NAMESPACE
