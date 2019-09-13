// Copyright 2018 VMware, all rights reserved

/**
 * Sliver -- Zero-copy management of bytes.
 *
 * See sliver.hpp for design details.
 */

#include "sliver.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <ios>
#include <memory>

#include "hex_tools.h"

using namespace std;

namespace concordUtils {

/**
 * Create an empty sliver.
 *
 * Default to an empty string representation for simplicity.
 *
 * Remember the data inside slivers is immutable.
 */
Sliver::Sliver() : data_(string()), offset_(0), length_(0) {}

/**
 * Create a new sliver that will own the memory pointed to by `data`, which is
 * `length` bytes in size. A new sliver created from a char* buffer always
 * stores a unique_ptr underneath. This has less overhead than always using a
 * shared pointer. The unique pointer will be converted to a shared pointer if
 * the Sliver is copied or a subsliver is created.
 *
 * Important: the `data` buffer should have been allocated with `new`, and not
 * `malloc`, because the shared pointer will use `delete` and not `free`.
 */
Sliver::Sliver(char* data, const size_t length)
    : data_(std::unique_ptr<char[]>(data)),
      offset_(0),
      length_(length) {
  // Data must be non-null.
  assert(data);
}

/**
 * Create a sub-sliver that references a region of a base sliver.
 *
 * The Sliver parameter cannot be const because it may be upgraded to a
 * shared_ptr.
 */
Sliver::Sliver(Sliver& base, const size_t offset, const size_t length) {
  base.subsliver(offset, length);
};

/**
 * Create a sliver from a copy of the memory pointed to by `data`, which is
 * `length` bytes in size.
 */
Sliver Sliver::copy(char* data, const size_t length) {
   auto* copy = new char[length];
   memcpy(copy, data, length);
   return Sliver(copy, length);
}

/**
 * Get the byte at `offset` in this sliver.
 */
char Sliver::operator[](const size_t offset) const {
  const size_t total_offset = offset_ + offset;
  // This offset must be within this sliver.
  assert(total_offset < length_);

  // The data for the requested offset is that many bytes after the offset from
  // the base sliver.
  if (std::holds_alternative<string>(data_)) {
    return std::get<string>(data_).data()[total_offset];
  } else if (std::holds_alternative<shared_ptr<StringBuf>>(data_)) {
    return std::get<shared_ptr<StringBuf>>(data_)->s.data()[total_offset];
  } else if (std::holds_alternative<unique_ptr<char[]>>(data_)) {
    return std::get<unique_ptr<char[]>>(data_).get()[total_offset];
  } else {
    return std::get<shared_ptr<char[]>>(data_).get()[total_offset];
  }
}

/**
 * Get a direct pointer to the data for this sliver. Remember that the Sliver
 * (or its base) still owns the data, so ensure that the lifetime of this Sliver
 * (or its base) is at least as long as the lifetime of the returned pointer.
 */
const char* Sliver::data() const {
  if (std::holds_alternative<string>(data_)) {
    return std::get<string>(data_).data() + offset_;
  } else if (std::holds_alternative<shared_ptr<StringBuf>>(data_)) {
    return std::get<shared_ptr<StringBuf>>(data_)->s.data() + offset_;
  } else if (std::holds_alternative<unique_ptr<char[]>>(data_)) {
    return std::get<unique_ptr<char[]>>(data_).get() + offset_;
  } else {
    return std::get<shared_ptr<char[]>>(data_).get() + offset_;
  }
}

/**
 * Create a subsliver.
 *
 * If the backing data type is not a shared_ptr then upgrade it.
 * */
Sliver Sliver::subsliver(const size_t offset, const size_t length) const {
  Sliver s;
  if (std::holds_alternative<string>(data_)) {
    // promote data_ to shared_ptr<StringBuf>
    auto copy = std::make_shared<StringBuf>(StringBuf());
    copy.get()->s = std::move(std::get<string>(data_));
    data_ = copy;
    s.data_ = std::move(copy);
  } else if (std::holds_alternative<unique_ptr<char[]>>(data_)) {
    // promote data_ to shared_ptr<char[]>
    shared_ptr<char[]> copy = std::move(std::get<unique_ptr<char[]>>(data_));
    data_ = copy;
    s.data_ = std::move(copy);
  }
  s.offset_ = offset_;
  s.length_ = length_;
  return s;
}

/**
 * Clone a sliver.
 *
 * If the backing data type is not a shared_ptr then upgrade it first.
 * */
Sliver Sliver::clone() const  {
  return subsliver(0, length_);
}


size_t Sliver::length() const { return length_; }

std::ostream& Sliver::operator<<(std::ostream& s) const {
  return hexPrint(s, data(), length());
}

std::ostream& operator<<(std::ostream& s, const Sliver& sliver) {
  return sliver.operator<<(s);
}

/**
 * Slivers are == if their lengths are the same, and each byte of their data is
 * the same.
 */
bool Sliver::operator==(const Sliver& other) const {
  // This could be just "compare(other) == 0", but the short-circuit of checking
  // lengths first can save us many cycles in some cases.
  return length() == other.length() &&
         memcmp(data(), other.data(), length()) == 0;
}

bool Sliver::operator!=(const Sliver& other) const {
  return !(*this == other);
}

/**
 * a.compare(b) is:
 *  - 0 if lengths are the same, and bytes are the same
 *  - -1 if bytes are the same, but a is shorter
 *  - 1 if bytes are the same, but a is longer
 */
int Sliver::compare(const Sliver& other) const {
  int comp = memcmp(data(), other.data(), std::min(length(), other.length()));
  if (comp == 0) {
    if (length() < other.length()) {
      comp = -1;
    } else if (length() > other.length()) {
      comp = 1;
    }
  }
  return comp;
}

}  // namespace concordUtils
