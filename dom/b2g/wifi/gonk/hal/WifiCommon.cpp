/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define LOG_TAG "WifiCommon"

#include "WifiCommon.h"
#include <iomanip>

bool gWifiDebug = false;

static int32_t HexToNum(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

template <typename T>
std::string ConvertMacToString(const T& mac) {
  size_t len = mac.size();
  if (len != 6) {
    return std::string();
  }
  std::ostringstream stream;
  stream << std::hex << std::uppercase;
  for (size_t i = 0; i < len; i++) {
    stream << std::setw(2) << std::setfill('0') << unsigned(mac[i]);
    if (i != len - 1) stream << ":";
  }
  return stream.str();
}

template <typename T>
std::string ConvertByteArrayToHexString(const T& in) {
  std::string out;
  const char hexChar[] = "0123456789ABCDEF";

  for (size_t i = 0; i < in.size(); i++) {
    uint8_t byte = in[i];
    out.push_back(hexChar[(byte >> 4) & 0x0F]);
    out.push_back(hexChar[byte & 0x0F]);
  }
  return out;
}

template <typename T>
int32_t ConvertMacToByteArray(const std::string& mac, T& out) {
  size_t pos = 0;
  for (size_t i = 0; i < 6; i++) {
    int32_t a, b;

    while (mac.at(pos) == ':' || mac.at(pos) == '.' || mac.at(pos) == '-') {
      pos = pos + 1;
    }

    a = HexToNum(mac.at(pos++));
    if (a < 0) return -1;
    b = HexToNum(mac.at(pos++));
    if (b < 0) return -1;
    out[i] = (a << 4) | b;
  }
  return pos;
}

template <typename T>
int32_t ConvertHexStringToBytes(const std::string& in, T& out) {
  if (in.empty()) return -1;
  if (in.size() % 2 != 0) return -1;

  std::string::const_iterator it = in.cbegin();
  while (it != in.cend()) {
    int32_t number = (HexToNum(*it) << 4) | (HexToNum(*(it + 1)));
    if (number < 0) return -1;
    out.push_back(number);
    it += 2;
  }
  return out.size();
}

template <typename T>
int32_t ConvertHexStringToByteArray(const std::string& in, T& out) {
  if (in.empty()) return -1;
  if (in.size() % 2 != 0) return -1;

  uint32_t index = 0;
  std::string::const_iterator it = in.cbegin();
  while (it != in.cend()) {
    int32_t number = (HexToNum(*it) << 4) | (HexToNum(*(it + 1)));
    if (number < 0) return -1;
    out[index++] = number;
    it += 2;
  }
  return out.size();
}

int32_t ByteToInteger(std::vector<uint8_t>::const_iterator& iter,
                      uint32_t length, int32_t endian) {
  std::vector<uint8_t>::const_iterator& it = iter;
  const std::vector<uint8_t>::const_iterator& end = iter + length;

  int32_t out = 0;
  int32_t count = 0;
  for (; it != end; ++it, ++count) {
    out += (endian == LITTLE_ENDIAN) ? (*it << (8 * count))
                                     : (*it << (8 * (length - count - 1)));
  }
  return out;
}

std::string ByteToString(std::vector<uint8_t>::const_iterator& iter,
                         uint32_t length) {
  std::vector<uint8_t>::const_iterator& begin = iter;
  std::string out(begin, begin + length);
  begin += length;
  return Trim(out);
}

std::string Quote(std::string& s) {
  s = "\"" + s + "\"";
  return s;
}

std::string Dequote(std::string& s) {
  if (s.front() != '"' || s.back() != '"') {
    return s;
  }
  s.erase(0, 1);
  s.erase(s.length() - 1);
  return s;
}

std::string Trim(std::string& s) {
  if (s.empty()) {
    return s;
  }
  s.erase(0, s.find_first_not_of(" "));
  s.erase(s.find_last_not_of(" ") + 1);
  return s;
}
