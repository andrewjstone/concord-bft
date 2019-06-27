// Concord
//
// Copyright (c) 2018 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the LICENSE
// file.

#include <string.h>
#include "ClientReplyMsg.hpp"
#include "assertUtils.hpp"

namespace bftEngine {
namespace impl {

ClientReplyMsg::ClientReplyMsg(ReplicaId primaryId,
                               ReqId reqSeqNum,
                               ReplicaId replicaId)
    : MessageBase(replicaId, MsgCode::Reply, sizeof(ClientReplyMsgHeader)) {
  b()->reqSeqNum = reqSeqNum;
  b()->currentPrimaryId = primaryId;
  b()->replyLength = 0;
}

ClientReplyMsg::ClientReplyMsg(ReplicaId replicaId,
                               ReqId reqSeqNum,
                               const char* reply,
                               uint32_t replyLength)
    : MessageBase(replicaId,
                  MsgCode::Reply,
                  sizeof(ClientReplyMsgHeader) + replyLength) {
  b()->reqSeqNum = reqSeqNum;
  b()->currentPrimaryId = 0;
  b()->replyLength = replyLength;

  memcpy(mut_data() + sizeof(ClientReplyMsgHeader), reply, replyLength);
}

ClientReplyMsg::ClientReplyMsg(ReplicaId replicaId, uint32_t replyLength)
    : MessageBase(replicaId,
                  MsgCode::Reply,
                  sizeof(ClientReplyMsgHeader) + replyLength) {
  b()->reqSeqNum = 0;
  b()->currentPrimaryId = 0;
  b()->replyLength = replyLength;
}

void ClientReplyMsg::setReplyLength(uint32_t replyLength) {
  Assert(replyLength <= maxReplyLength());
  b()->replyLength = replyLength;
  mut_data().resize(sizeof(ClientReplyMsgHeader) + replyLength);
}

void ClientReplyMsg::setPrimaryId(ReplicaId primaryId) {
  b()->currentPrimaryId = primaryId;
}

bool ClientReplyMsg::ToActualMsgType(NodeIdType myId,
                                     std::unique_ptr<MessageBase>& inMsg,
                                     std::unique_ptr<ClientReplyMsg>& outMsg) {
  Assert(inMsg->type() == MsgCode::Reply);
  if (inMsg->size() < sizeof(ClientReplyMsgHeader)) return false;

  ClientReplyMsg* t = (ClientReplyMsg*)inMsg;

  if (t->size() < ((int)sizeof(ClientReplyMsgHeader) + t->replyLength()))
    return false;

  outMsg.swap(inMsg);

  return true;

  // TODO(GG): the client should make sure that the message was actually sent by
  // a valid replica
}

uint64_t ClientReplyMsg::debugHash() const {
  uint64_t retVal = 0;

  uint32_t replyLen = replyLength();
  Assert(replyLen > 0);

  uint32_t firstWordLen = replyLen % sizeof(uint64_t);
  if (firstWordLen == 0) firstWordLen = sizeof(uint64_t);

  Assert(((replyLen - firstWordLen) % sizeof(uint64_t)) == 0);
  uint32_t numberOfWords = ((replyLen - firstWordLen) / sizeof(uint64_t)) + 1;

  const char* repBuf = replyBuf();

  // copy first word
  {
    char* p = (char*)&retVal;
    for (uint32_t i = 0; i < firstWordLen; i++) {
      p[i] = repBuf[i];
    }
  }

  if (numberOfWords > 1) {
    uint64_t* p = (uint64_t*)(repBuf + firstWordLen);
    for (uint32_t i = 0; i < numberOfWords - 1; i++) {
      retVal = retVal ^ p[i];
    }
  }

  return retVal;
}

}  // namespace impl
}  // namespace bftEngine
