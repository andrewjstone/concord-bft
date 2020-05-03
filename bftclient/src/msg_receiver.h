// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License"). You may not use
// this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright notices and license
// terms. Your use of these subcomponents is subject to the terms and conditions of the
// subcomponent's license, as noted in the LICENSE file.

#pragma once

#include <condition_variable>
#include <queue>

#include "communication/ICommunication.hpp"
#include "bftclient/config.h"

namespace bft::client {

// Metadata stripped from a ClientReplyMsgHeader
struct ReplyMetadata {
  ReplicaId primary;
  uint64_t seq_num;

  bool operator==(const ReplyMetadata& other) const { return primary == other.primary && seq_num == other.seq_num; }
  bool operator!=(const ReplyMetadata& other) const { return !(*this == other); }
  bool operator<(const ReplyMetadata& other) const {
    if (primary < other.primary) {
      return true;
    }
    if (primary == other.primary && seq_num < other.seq_num) {
      return true;
    }
    return false;
  }
};

// A Reply that has been received, but not yet matched.
struct UnmatchedReply {
  ReplyMetadata metadata;
  Msg data;
  ReplicaSpecificInfo rsi;
};

// A thread-safe queue that allows the ASIO thread to push newly received messages and the client
// send thread to wait for those messages to be received.
class UnmatchedReplyQueue {
 public:
  // Push a new msg on the queue
  //
  // This function is thread safe.
  void push(UnmatchedReply&& reply);

  // Wait for any messages to be pushed.
  //
  // This function is thread safe.
  std::vector<UnmatchedReply> wait(std::chrono::milliseconds timeout);

 private:
  std::vector<UnmatchedReply> msgs_;
  std::mutex lock_;
  std::condition_variable cond_var_;
};

// A message receiver receives packed ClientReplyMsg structs off the wire. This is a
// ClientReplyMsgHeader followed by opaque reply data.
//
// We want to destructure reply into something sane for the rest of the client to handle.
class MsgReceiver : public bft::communication::IReceiver {
 public:
  // IReceiver methods
  // These are called from the ASIO thread when a new message is received.
  void onNewMessage(const bft::communication::NodeNum sourceNode,
                    const char* const message,
                    const size_t messageLength) override;

  void onConnectionStatusChanged(const bft::communication::NodeNum node,
                                 const bft::communication::ConnectionStatus newStatus) override {}

  // Wait for messages to be received.
  //
  // Return all received messages.
  // Return an empty queue if timeout occurs.
  //
  // This should be called from the thread that calls `Client::send`.
  std::vector<UnmatchedReply> wait(std::chrono::milliseconds timeout);

 private:
  UnmatchedReplyQueue queue_;
};

}  // namespace bft::client