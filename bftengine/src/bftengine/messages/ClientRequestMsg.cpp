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

#include <cstring>
#include "ClientRequestMsg.hpp"
#include "../assertUtils.hpp"

namespace bftEngine {
namespace impl {

// local helper functions

static uint16_t getSender(const ClientRequestMsgHeader* r) {
  return r->idOfClientProxy;
}

static int32_t compRequestMsgSize(const ClientRequestMsgHeader* r) {
  return (sizeof(ClientRequestMsgHeader) + r->requestLength);
}

uint32_t getRequestSizeTemp(const char* request)  // TODO(GG): change - TBD
{
  const ClientRequestMsgHeader* r = (ClientRequestMsgHeader*)request;
  return compRequestMsgSize(r);
}

// class ClientRequestMsg

ClientRequestMsg::ClientRequestMsg(NodeIdType sender,
                                   bool isReadOnly,
                                   uint64_t reqSeqNum,
                                   uint32_t requestLength,
                                   const char* request)
    : MessageBase(sender,
                  MsgCode::Request,
                  (sizeof(ClientRequestMsgHeader) + requestLength)) {
  // TODO(GG): asserts

  b()->idOfClientProxy = sender;
  b()->flags = 0;
  if (isReadOnly) b()->flags |= 0x1;
  b()->reqSeqNum = reqSeqNum;
  b()->requestLength = requestLength;

  memcpy(mut_data() + sizeof(ClientRequestMsgHeader), request, requestLength);
}

ClientRequestMsg::ClientRequestMsg(ClientRequestMsgHeader* body)
    : MessageBase(getSender(body), (char*)body, compRequestMsgSize(body)) {}

bool ClientRequestMsg::ToActualMsgType(const ReplicasInfo& repInfo,
                                       std::unique_ptr<MessageBase>& inMsg,
                                       std::unique_ptr<ClientRequestMsg>& outMsg) {
  Assert(inMsg->type() == MsgCode::Request);
  if (inMsg->size() < sizeof(ClientRequestMsgHeader)) return false;

  ClientRequestMsg* t = (ClientRequestMsg*)inMsg.get();

  if (t->size() < (sizeof(ClientRequestMsgHeader) + t->b()->requestLength))
    return false;

  inMsg.release();
  outMsg.reset(t);

  return true;
}

}  // namespace impl
}  // namespace bftEngine
