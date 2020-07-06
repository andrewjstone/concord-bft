// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the sub-component's license, as noted in the
// LICENSE file.

#pragma once

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

#include <iostream>
using namespace std;

namespace cmf {

class DeserializeError : public std::runtime_error {
 public:
  DeserializeError(const std::string& error) : std::runtime_error(("DeserializeError: " + error).c_str()) {}
};

class NoDataLeftError : public DeserializeError {
 public:
  NoDataLeftError() : DeserializeError("Data left in buffer is less than what is needed for deserialization") {}
};

/******************************************************************************
 * Integers
 *
 * All integers are encoded in little endian
 ******************************************************************************/
template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
void Serialize(std::vector<uint8_t>& output, const T& t) {
  if constexpr (std::is_same_v<T, bool>) {
    output.push_back(t ? 1 : 0);
  } else {
    for (auto i = 0u; i < sizeof(T); i++) {
      output.push_back(255 & t >> (i * 8));
    }
  }
}

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
void Deserialize(uint8_t*& start, const uint8_t* end, T& t) {
  if constexpr (std::is_same_v<T, bool>) {
    if (start + 1 > end) {
      throw NoDataLeftError();
    }
    (*start == 0) ? t = false : t = true;
    start += 1;
  } else {
    if (start + sizeof(T) > end) {
      throw NoDataLeftError();
    }
    t = 0;
    for (auto i = 0u; i < sizeof(T); i++) {
      t |= ((*(start + i)) << (i * 8));
    }
    start += sizeof(T);
  }
}

/******************************************************************************
 * Strings
 *
 * Strings are preceded by a uint32_t length
 ******************************************************************************/
void Serialize(std::vector<uint8_t>& output, const std::string& s) {
  assert(s.size() <= 0xFFFFFFFF);
  uint32_t length = s.size() & 0xFFFFFFFF;
  Serialize(output, length);
  std::copy(s.begin(), s.end(), std::back_inserter(output));
}

void Deserialize(uint8_t*& start, const uint8_t* end, std::string& s) {
  uint32_t length;
  Deserialize(start, end, length);
  if (start + length > end) {
    throw NoDataLeftError();
  }
  std::copy_n(start, length, std::back_inserter(s));
  start += length;
}

/******************************************************************************
 Forward declarations needed by recursive types
 ******************************************************************************/
// Lists
template <typename T>
void Serialize(std::vector<uint8_t>& output, const std::vector<T>& v);
template <typename T>
void Deserialize(uint8_t*& start, const uint8_t* end, std::vector<T>& v);

// KVPairs
template <typename K, typename V>
void Serialize(std::vector<uint8_t>& output, const std::pair<K, V>& kvpair);
template <typename K, typename V>
void Deserialize(uint8_t*& start, const uint8_t* end, std::pair<K, V>& kvpair);

// Maps
template <typename K, typename V>
void Serialize(std::vector<uint8_t>& output, const std::map<K, V>& m);
template <typename K, typename V>
void Deserialize(uint8_t*& start, const uint8_t* end, std::map<K, V>& m);

// Optionals
template <typename T>
void Serialize(std::vector<uint8_t>& output, const std::optional<T>& t);
template <typename T>
void Deserialize(uint8_t*& start, const uint8_t* end, std::optional<T>& t);

/******************************************************************************
 * Lists are modeled as std::vectors
 *
 * Lists are preceded by a uint32_t length
 ******************************************************************************/
template <typename T>
void Serialize(std::vector<uint8_t>& output, const std::vector<T>& v) {
  assert(v.size() <= 0xFFFFFFFF);
  uint32_t length = v.size() & 0xFFFFFFFF;
  Serialize(output, length);
  for (auto& it : v) {
    Serialize(output, it);
  }
}

template <typename T>
void Deserialize(uint8_t*& start, const uint8_t* end, std::vector<T>& v) {
  uint32_t length;
  Deserialize(start, end, length);
  if constexpr (std::is_integral_v<T> && sizeof(T) == 1) {
    // Optimized for bytes
    if (start + length > end) {
      throw DeserializeError("Data left in buffer is less than what is needed for deserialization");
    }
    std::copy_n(start, length, std::back_inserter(v));
    start += length;
  } else {
    for (auto i = 0u; i < length; i++) {
      T t;
      Deserialize(start, end, t);
      v.push_back(t);
    }
  }
}

/******************************************************************************
 * KVPairs are modeled as std::pairs.
 ******************************************************************************/
template <typename K, typename V>
void Serialize(std::vector<uint8_t>& output, const std::pair<K, V>& kvpair) {
  Serialize(output, kvpair.first);
  Serialize(output, kvpair.second);
}

template <typename K, typename V>
void Deserialize(uint8_t*& start, const uint8_t* end, std::pair<K, V>& kvpair) {
  Deserialize(start, end, kvpair.first);
  Deserialize(start, end, kvpair.second);
}

/******************************************************************************
 * Maps
 *
 * Maps are preceded by a uint32_t size
 ******************************************************************************/
template <typename K, typename V>
void Serialize(std::vector<uint8_t>& output, const std::map<K, V>& m) {
  assert(m.size() <= 0xFFFFFFFF);
  uint32_t size = m.size() & 0xFFFFFFFF;
  Serialize(output, size);
  for (auto& it : m) {
    Serialize(output, it);
  }
}

template <typename K, typename V>
void Deserialize(uint8_t*& start, const uint8_t* end, std::map<K, V>& m) {
  uint32_t size;
  Deserialize(start, end, size);
  for (auto i = 0u; i < size; i++) {
    std::pair<K, V> kvpair;
    Deserialize(start, end, kvpair);
    m.insert(kvpair);
  }
}

/******************************************************************************
 * Optionals are modeled as std::optional
 *
 * Optionals are preceded by a bool indicating whether a value is present or not.
 ******************************************************************************/
template <typename T>
void Serialize(std::vector<uint8_t>& output, const std::optional<T>& t) {
  Serialize(output, t.has_value());
  if (t.has_value()) {
    Serialize(output, t.value());
  }
}

template <typename T>
void Deserialize(uint8_t*& start, const uint8_t* end, std::optional<T>& t) {
  bool has_value;
  Deserialize(start, end, has_value);
  if (has_value) {
    T value;
    Deserialize(start, end, value);
    t = value;
  } else {
    t = std::nullopt;
  }
}

}  // namespace cmf
