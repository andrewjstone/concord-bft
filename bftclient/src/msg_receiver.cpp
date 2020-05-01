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

#include "bftengine/ClientMsgs.hpp"
#include "msg_receiver.h"

namespace bft::client {

void UnmatchedReplyQueue::push(UnmatchedReply&& reply) {
  {
    std::lock_guard<std::mutex> guard(lock_);
    msgs_.push_back(std::move(reply));
  }
  cond_var_.notify_one();
}

std::vector<UnmatchedReply> UnmatchedReplyQueue::wait(std::chrono::milliseconds timeout) {
  std::vector<UnmatchedReply> new_msgs;
  std::unique_lock<std::mutex> lock(lock_);
  cond_var_.wait_for(lock, timeout, [this] { return !msgs_.empty(); });
  if (!msgs_.empty()) {
    msgs_.swap(new_msgs);
  }
  return new_msgs;
}

void MsgReceiver::onNewMessage(const bft::communication::NodeNum source,
                               const char* const message,
                               const size_t msg_len) {
  // TODO: Check max message size

  if (msg_len < sizeof(bftEngine::ClientReplyMsgHeader)) {
    // TODO: Add warning log
    return;
  }

  auto* header = reinterpret_cast<const bftEngine::ClientReplyMsgHeader*>(message);
  if (header->msgType != REPLY_MSG_TYPE) {
    // TODO: Add warning log
    return;
  }

  auto metadata = ReplyMetadata{};
  metadata.primary = ReplicaId{header->currentPrimaryId};
  metadata.seq_num = header->reqSeqNum;

  auto start_of_body = message + sizeof(bftEngine::ClientReplyMsgHeader);
  auto start_of_rsi = start_of_body + (header->replyLength - header->replicaSpecificInfoLength);
  auto end_of_rsi = start_of_rsi + header->replicaSpecificInfoLength;

  auto rsi = ReplicaSpecificInfo{};
  rsi.from = ReplicaId{static_cast<uint16_t>(source)};
  rsi.data = Msg(start_of_rsi, end_of_rsi);

  auto reply = UnmatchedReply{};
  reply.metadata = metadata;
  reply.rsi = std::move(rsi);
  reply.data = Msg(start_of_body, start_of_rsi);

  queue_.push(std::move(reply));
}

std::vector<UnmatchedReply> MsgReceiver::wait(std::chrono::milliseconds timeout) { return queue_.wait(timeout); }

}  // namespace bft::client