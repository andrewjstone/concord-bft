// Copyright 2018 VMware, all rights reserved

/**
 * Sliver -- Zero-copy management of bytes.
 *
 * Sliver provides a view into an allocated region of memory. Views of
 * sub-regions, or "sub-slivers" do not copy data, but instead reference the
 * memory of the "base" sliver.
 *
 * The memory is managed through a std::shared_ptr. If the `Sliver(char* data,
 * size_t length)` constructor is called, that sliver wraps the data pointer in
 * a shared pointer. Sub-slivers reference this same shared pointer, such that
 * the memory is kept around as long as the base sliver or any sub-sliver needs
 * it, and cleaned up once the base sliver and all sub-slivers have finished
 * using it.
 *
 * Intentionally copyable (via default copy constructor and assignment
 * operator). Copying the shared_ptr increases its reference count by one, so
 * that it is not released until both copies go out of scope.
 *
 * Intentionally movable (via default move constructor and assignment
 * operator). Moving the shared_ptr avoids modifying its reference count, which
 * requires an atomic operation that might be considered expensive.
 */

#ifndef CONCORD_BFT_UTIL_SLIVER_HPP_
#define CONCORD_BFT_UTIL_SLIVER_HPP_

#include <ios>
#include <memory>
#include <cassert>

namespace concordUtils {

class ISliver {
  public:

    virtual size_t length() const;
    virtual const char* data() const;

//    virtual std::ostream& operator<<(std::ostream& s) const;
//   virtual bool operator==(const ISliver& other) const;
//   virtual bool operator!=(const ISliver& other) const;
//   virtual int compare(const ISliver& other) const;
};

// A subsliver should only live inside a shared_ptr<ISliver>.
// It gets created by calling ISliver::subsliver().
class SubSliver : public ISliver {

  size_t length() const override { return length_; }
    const char* data() const override { return data_->data() + offset_; }

  private:
    SubSliver(std::shared_ptr<ISliver> s, const size_t offset, const size_t length) : data_(s), offset_(offset), length_(length) {
      assert(offset + length < s->length());
    }

    std::shared_ptr<ISliver> data_;
    size_t offset_;
    size_t length_;

    friend class ISliver;
};

// A BufSliver is an ISliver backed by a std::unique_ptr<char[]>
class BufSliver : public ISliver {
  public:
    BufSliver(char* data, const size_t length) : data_(data), length_(length) {}

    size_t length() const override { return length_; }
    const char* data() const override { return data_.get(); }

  private:
    std::unique_ptr<char[]> data_;
    size_t length_;
};

// A StringSliver is an ISliver backed by a std::string
class StringSliver : public ISliver {
  public:
    StringSliver(const std::string& s): data_(s) {}
    StringSliver(const std::string&& s): data_(s) {}

    size_t length() const override { return data_.size(); }
    const char* data() const override { return data_.data(); }

  private:
    std::string data_;
};

class Sliver {
 public:
  Sliver();
  Sliver(uint8_t* data, const size_t length);
  Sliver(char* data, const size_t length);
  Sliver(const Sliver& base, const size_t offset, const size_t length);
  Sliver(const uint8_t* data, const size_t length);
  Sliver(const char* data, const size_t length);
  Sliver(const std::string& s):Sliver(s.data(), s.length()){}
  static Sliver copy(uint8_t* data, const size_t length);
  static Sliver copy(char* data, const size_t length);

  uint8_t operator[](const size_t offset) const;

  Sliver subsliver(const size_t offset, const size_t length) const;

  size_t length() const;
  uint8_t* data() const;

  std::ostream& operator<<(std::ostream& s) const;
  bool operator==(const Sliver& other) const;
  bool operator!=(const Sliver& other) const;
  int compare(const Sliver& other) const;

 private:
  // these are never modified, but need to be non-const to support copy
  // assignment
  std::shared_ptr<uint8_t> m_data;
  size_t m_offset;
  size_t m_length;

  // Delete new and delete, to force the Sliver to be allocated on the stack, so
  // that it is cleaned up properly via RAII scoping.
  static void* operator new(size_t) = delete;
  static void* operator new[](size_t) = delete;
  static void operator delete(void*) = delete;
  static void operator delete[](void*) = delete;
};

std::ostream& operator<<(std::ostream& s, const Sliver& sliver);

bool copyToAndAdvance(uint8_t* _buf, size_t* _offset, size_t _maxOffset,
                      uint8_t* _src, size_t _srcSize);

}  // namespace concordUtils

#endif  // CONCORD_BFT_UTIL_SLIVER_HPP_
