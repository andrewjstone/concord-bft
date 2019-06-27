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

#pragma once

#include "Digest.hpp"
#include "MessageBase.hpp"

class IThresholdSigner;

namespace bftEngine {
namespace impl {

///////////////////////////////////////////////////////////////////////////////
// SignedShareBase
///////////////////////////////////////////////////////////////////////////////

class SignedShareBase : public MessageBase {
 public:
  ViewNum viewNumber() const { return b()->viewNumber; }

  SeqNum seqNumber() const { return b()->seqNumber; }

  uint16_t signatureLen() const { return b()->thresSigLength; }

  const char* signatureBody() const { return body() + sizeof(SignedShareBaseHeader); }

 protected:

#pragma pack(push, 1)
  struct SignedShareBaseHeader {
    MessageBase::Header header;
    ViewNum viewNumber;
    SeqNum seqNumber;
    uint16_t thresSigLength;
    // Followed by threshold signature of <viewNumber, seqNumber, and the
    // preprepre digest>
  };
#pragma pack(pop)

  static_assert(sizeof(SignedShareBaseHeader) == (2 + 8 + 8 + 2),
                "SignedShareBaseHeader is 58B");

  static std::unique_ptr<SignedShareBase> create(int16_t type,
                                 ViewNum v,
                                 SeqNum s,
                                 ReplicaId senderId,
                                 Digest& digest,
                                 IThresholdSigner* thresholdSigner);
  static std::unique_ptr<SignedShareBase> create(int16_t type,
                                 ViewNum v,
                                 SeqNum s,
                                 ReplicaId senderId,
                                 const char* sig,
                                 uint16_t sigLen);
  static bool ToActualMsgType(const ReplicasInfo& repInfo,
                              int16_t type,
                              std::unique_ptr<MessageBase>& inMsg,
                              std::unique_ptr<SignedShareBase>& outMsg);

  SignedShareBase(ReplicaId sender, int16_t type, size_t msgSize);

  SignedShareBaseHeader* b() const { return (SignedShareBaseHeader*)mut_data(); }
};

///////////////////////////////////////////////////////////////////////////////
// PreparePartialMsg
///////////////////////////////////////////////////////////////////////////////

class PreparePartialMsg : public SignedShareBase {
 public:
  static std::unique_ptr<PreparePartialMsg> create(ViewNum v,
                                   SeqNum s,
                                   ReplicaId senderId,
                                   Digest& ppDigest,
                                   IThresholdSigner* thresholdSigner);
  static bool ToActualMsgType(const ReplicasInfo& repInfo,
                              std::unique_ptr<MessageBase>& inMsg,
                              std::unique_ptr<PreparePartialMsg>& outMsg);
};

///////////////////////////////////////////////////////////////////////////////
// PrepareFullMsg
///////////////////////////////////////////////////////////////////////////////

class PrepareFullMsg : public SignedShareBase {
 public:
  static MsgSize maxSizeOfPrepareFull();
  static MsgSize maxSizeOfPrepareFullInLocalBuffer();
  static std::unique_ptr<PrepareFullMsg> create(ViewNum v,
                                SeqNum s,
                                ReplicaId senderId,
                                const char* sig,
                                uint16_t sigLen);
  static bool ToActualMsgType(const ReplicasInfo& repInfo,
                              std::unique_ptr<MessageBase>& inMsg,
                              std::unique_ptr<PrepareFullMsg>& outMsg);
};

///////////////////////////////////////////////////////////////////////////////
// CommitPartialMsg
///////////////////////////////////////////////////////////////////////////////

class CommitPartialMsg : public SignedShareBase {
 public:
  static std::unique_ptr<CommitPartialMsg> create(ViewNum v,
                                  SeqNum s,
                                  ReplicaId senderId,
                                  Digest& ppDoubleDigest,
                                  IThresholdSigner* thresholdSigner);
  static bool ToActualMsgType(const ReplicasInfo& repInfo,
                              std::unique_ptr<MessageBase>& inMsg,
                              std::unique_ptr<CommitPartialMsg>& outMsg);
};

///////////////////////////////////////////////////////////////////////////////
// CommitFullMsg
///////////////////////////////////////////////////////////////////////////////

class CommitFullMsg : public SignedShareBase {
 public:
  static MsgSize maxSizeOfCommitFull();
  static MsgSize maxSizeOfCommitFullInLocalBuffer();
  static std::unique_ptr<CommitFullMsg> create(
      ViewNum v, SeqNum s, int16_t senderId, const char* sig, uint16_t sigLen);
  static bool ToActualMsgType(const ReplicasInfo& repInfo,
                              std::unique_ptr<MessageBase>& inMsg,
                              std::unique_ptr<CommitFullMsg>& outMsg);
};

}  // namespace impl
}  // namespace bftEngine
