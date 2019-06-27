// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0
// License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#pragma once

#include <vector>
#include <cstring>

#include "../SysConsts.hpp"
#include "../PrimitiveTypes.hpp"
#include "../MsgCode.hpp"
#include "../ReplicasInfo.hpp"

namespace bftEngine {
namespace impl {

class MessageBase {
 public:
#pragma pack(push, 1)
  struct Header {
    MsgType msgType;
  };
#pragma pack(pop)

  static_assert(sizeof(Header) == 2, "MessageBase::Header is 2B");

  MessageBase(NodeIdType sender, MsgType type, MsgSize size)
      : sender_(sender), data_(size) {
    reinterpret_cast<Header*>(data_.data())->msgType = type;
    debugLiveMessages();
  }

  MessageBase(NodeIdType sender, const char* msg, MsgSize size)
      : sender_(sender), data_(msg, msg + size) {
    debugLiveMessages();
  }


  MessageBase(NodeIdType sender, MsgType type, const char* msg_without_header,
      MsgSize msg_without_header_size) :
    sender_(sender), data_(sizeof(Header) + msg_without_header_size) {
      reinterpret_cast<Header*>(data_.data())->msgType = type;
      memcpy(data_.data() + sizeof(Header), msg_without_header, msg_without_header_size);
    }

  virtual ~MessageBase();

  MsgSize size() const { return data_.size(); }

  NodeIdType senderId() const { return sender_; }

  MsgType type() const {
    return reinterpret_cast<const Header*>(data_.data())->msgType;
  }

  const char* body() const { return data_.data(); }

#ifdef DEBUG_MEMORY_MSG
  static void printLiveMessages();
#endif

 protected:
#pragma pack(push, 1)
  struct RawHeaderOfObjAndMsg {
    uint32_t magicNum;
    MsgSize msgSize;
    NodeIdType sender;
    // TODO(GG): consider to add checksum
  };
#pragma pack(pop)

  static const uint32_t magicNumOfRawFormat = 0x5555897BU;
  const char* data() const { return data_.data(); }

  char* mut_data() { return data_.data(); }

  void resize(size_t n) { data_.resize(n); }
  void shrinkToFit() { data_.shrink_to_fit(); } 

 private:
  NodeIdType sender_;
  std::vector<char> data_;

  void debugLiveMessages();
};

class InternalMessage  // TODO(GG): move class to another file
{
 public:
  virtual ~InternalMessage() {}
  virtual void handle() = 0;
};

}  // namespace impl
}  // namespace bftEngine
