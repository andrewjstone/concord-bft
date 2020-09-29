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

/***************************************
 Autogenerated by cmfc. Do not modify.
***************************************/

#include "../../bftengine/include/messages/messages.hpp"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cmf {

class DeserializeError : public std::runtime_error {
 public:
  DeserializeError(const std::string& error) : std::runtime_error(("DeserializeError: " + error).c_str()) {}
};

class NoDataLeftError : public DeserializeError {
 public:
  NoDataLeftError() : DeserializeError("Data left in buffer is less than what is needed for deserialization") {}
};

class BadDataError : public DeserializeError {
 public:
  BadDataError(const std::string& expected, const std::string& got) : DeserializeError(str(expected, got)) {}

 private:
  static std::string str(const std::string& expected, const std::string& actual) {
    std::ostringstream oss;
    oss << "Expected " << expected << ", got" << actual;
    return oss.str();
  }
};

/******************************************************************************
 * Integers
 *
 * All integers are encoded in little endian
 ******************************************************************************/
template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
void serialize(std::vector<uint8_t>& output, const T& t) {
  if constexpr (std::is_same_v<T, bool>) {
    output.push_back(t ? 1 : 0);
  } else {
    for (auto i = 0u; i < sizeof(T); i++) {
      output.push_back(255 & t >> (i * 8));
    }
  }
}

template <typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
void deserialize(uint8_t*& start, const uint8_t* end, T& t) {
  if constexpr (std::is_same_v<T, bool>) {
    if (start + 1 > end) {
      throw NoDataLeftError();
    }
    if (*start == 0) {
      t = false;
    } else if (*start == 1) {
      t = true;
    } else {
      throw BadDataError("0 or 1", std::to_string(*start));
    }
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
void serialize(std::vector<uint8_t>& output, const std::string& s) {
  assert(s.size() <= 0xFFFFFFFF);
  uint32_t length = s.size() & 0xFFFFFFFF;
  serialize(output, length);
  std::copy(s.begin(), s.end(), std::back_inserter(output));
}

void deserialize(uint8_t*& start, const uint8_t* end, std::string& s) {
  uint32_t length;
  deserialize(start, end, length);
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
void serialize(std::vector<uint8_t>& output, const std::vector<T>& v);
template <typename T>
void deserialize(uint8_t*& start, const uint8_t* end, std::vector<T>& v);

// KVPairs
template <typename K, typename V>
void serialize(std::vector<uint8_t>& output, const std::pair<K, V>& kvpair);
template <typename K, typename V>
void deserialize(uint8_t*& start, const uint8_t* end, std::pair<K, V>& kvpair);

// Maps
template <typename K, typename V>
void serialize(std::vector<uint8_t>& output, const std::map<K, V>& m);
template <typename K, typename V>
void deserialize(uint8_t*& start, const uint8_t* end, std::map<K, V>& m);

// Optionals
template <typename T>
void serialize(std::vector<uint8_t>& output, const std::optional<T>& t);
template <typename T>
void deserialize(uint8_t*& start, const uint8_t* end, std::optional<T>& t);

/******************************************************************************
 * Lists are modeled as std::vectors
 *
 * Lists are preceded by a uint32_t length
 ******************************************************************************/
template <typename T>
void serialize(std::vector<uint8_t>& output, const std::vector<T>& v) {
  assert(v.size() <= 0xFFFFFFFF);
  uint32_t length = v.size() & 0xFFFFFFFF;
  serialize(output, length);
  for (auto& it : v) {
    serialize(output, it);
  }
}

template <typename T>
void deserialize(uint8_t*& start, const uint8_t* end, std::vector<T>& v) {
  uint32_t length;
  deserialize(start, end, length);
  if constexpr (std::is_integral_v<T> && sizeof(T) == 1) {
    // Optimized for bytes
    if (start + length > end) {
      throw NoDataLeftError();
    }
    std::copy_n(start, length, std::back_inserter(v));
    start += length;
  } else {
    for (auto i = 0u; i < length; i++) {
      T t;
      deserialize(start, end, t);
      v.push_back(t);
    }
  }
}

/******************************************************************************
 * KVPairs are modeled as std::pairs.
 ******************************************************************************/
template <typename K, typename V>
void serialize(std::vector<uint8_t>& output, const std::pair<K, V>& kvpair) {
  serialize(output, kvpair.first);
  serialize(output, kvpair.second);
}

template <typename K, typename V>
void deserialize(uint8_t*& start, const uint8_t* end, std::pair<K, V>& kvpair) {
  deserialize(start, end, kvpair.first);
  deserialize(start, end, kvpair.second);
}

/******************************************************************************
 * Maps
 *
 * Maps are preceded by a uint32_t size
 ******************************************************************************/
template <typename K, typename V>
void serialize(std::vector<uint8_t>& output, const std::map<K, V>& m) {
  assert(m.size() <= 0xFFFFFFFF);
  uint32_t size = m.size() & 0xFFFFFFFF;
  serialize(output, size);
  for (auto& it : m) {
    serialize(output, it);
  }
}

template <typename K, typename V>
void deserialize(uint8_t*& start, const uint8_t* end, std::map<K, V>& m) {
  uint32_t size;
  deserialize(start, end, size);
  for (auto i = 0u; i < size; i++) {
    std::pair<K, V> kvpair;
    deserialize(start, end, kvpair);
    m.insert(kvpair);
  }
}

/******************************************************************************
 * Optionals are modeled as std::optional
 *
 * Optionals are preceded by a bool indicating whether a value is present or not.
 ******************************************************************************/
template <typename T>
void serialize(std::vector<uint8_t>& output, const std::optional<T>& t) {
  serialize(output, t.has_value());
  if (t.has_value()) {
    serialize(output, t.value());
  }
}

template <typename T>
void deserialize(uint8_t*& start, const uint8_t* end, std::optional<T>& t) {
  bool has_value;
  deserialize(start, end, has_value);
  if (has_value) {
    T value;
    deserialize(start, end, value);
    t = value;
  } else {
    t = std::nullopt;
  }
}

}  // namespace cmf

namespace concord::messages {

bool operator==(const ClientReq& l, const ClientReq& r) {
  return l.client_id == r.client_id && l.flags == r.flags && l.seq_num == r.seq_num && l.timeout_milli == r.timeout_milli && l.span == r.span && l.correlation_id == r.correlation_id && l.data == r.data;
}
void serialize(std::vector<uint8_t>& output, const ClientReq& t) {
  cmf::serialize(output, t.client_id);
  cmf::serialize(output, t.flags);
  cmf::serialize(output, t.seq_num);
  cmf::serialize(output, t.timeout_milli);
  cmf::serialize(output, t.span);
  cmf::serialize(output, t.correlation_id);
  cmf::serialize(output, t.data);
}
void deserialize(uint8_t*& input, const uint8_t* end, ClientReq& t) {
  cmf::deserialize(input, end, t.client_id);
  cmf::deserialize(input, end, t.flags);
  cmf::deserialize(input, end, t.seq_num);
  cmf::deserialize(input, end, t.timeout_milli);
  cmf::deserialize(input, end, t.span);
  cmf::deserialize(input, end, t.correlation_id);
  cmf::deserialize(input, end, t.data);
}
void deserialize(const std::vector<uint8_t>& input, ClientReq& t) {
    auto* begin = const_cast<uint8_t*>(input.data());
    deserialize(begin, begin + input.size(), t);
}

bool operator==(const ClientRpy& l, const ClientRpy& r) {
  return l.seq_num == r.seq_num && l.primary == r.primary && l.span == r.span && l.data == r.data && l.replica_specific_info == r.replica_specific_info;
}
void serialize(std::vector<uint8_t>& output, const ClientRpy& t) {
  cmf::serialize(output, t.seq_num);
  cmf::serialize(output, t.primary);
  cmf::serialize(output, t.span);
  cmf::serialize(output, t.data);
  cmf::serialize(output, t.replica_specific_info);
}
void deserialize(uint8_t*& input, const uint8_t* end, ClientRpy& t) {
  cmf::deserialize(input, end, t.seq_num);
  cmf::deserialize(input, end, t.primary);
  cmf::deserialize(input, end, t.span);
  cmf::deserialize(input, end, t.data);
  cmf::deserialize(input, end, t.replica_specific_info);
}
void deserialize(const std::vector<uint8_t>& input, ClientRpy& t) {
    auto* begin = const_cast<uint8_t*>(input.data());
    deserialize(begin, begin + input.size(), t);
}

bool operator==(const PrePrepare& l, const PrePrepare& r) {
  return l.view == r.view && l.seq_num == r.seq_num && l.flags == r.flags && l.span == r.span && l.requests == r.requests && l.digest_of_requests == r.digest_of_requests;
}
void serialize(std::vector<uint8_t>& output, const PrePrepare& t) {
  cmf::serialize(output, t.view);
  cmf::serialize(output, t.seq_num);
  cmf::serialize(output, t.flags);
  cmf::serialize(output, t.span);
  cmf::serialize(output, t.requests);
  cmf::serialize(output, t.digest_of_requests);
}
void deserialize(uint8_t*& input, const uint8_t* end, PrePrepare& t) {
  cmf::deserialize(input, end, t.view);
  cmf::deserialize(input, end, t.seq_num);
  cmf::deserialize(input, end, t.flags);
  cmf::deserialize(input, end, t.span);
  cmf::deserialize(input, end, t.requests);
  cmf::deserialize(input, end, t.digest_of_requests);
}
void deserialize(const std::vector<uint8_t>& input, PrePrepare& t) {
    auto* begin = const_cast<uint8_t*>(input.data());
    deserialize(begin, begin + input.size(), t);
}

} // namespace concord::messages
