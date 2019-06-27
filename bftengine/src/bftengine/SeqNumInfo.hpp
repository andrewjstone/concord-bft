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

#include <set>

// TODO(GG): clean/move 'include' statements
#include "PrimitiveTypes.hpp"
#include "SysConsts.hpp"
#include "messages/PrePrepareMsg.hpp"
#include "messages/SignedShareMsgs.hpp"
#include "PartialProofsSet.hpp"
#include "PartialExecProofsSet.hpp"
#include "Logger.hpp"
#include "CollectorOfThresholdSignatures.hpp"
#include "SequenceWithActiveWindow.hpp"

namespace util {
class SimpleThreadPool;
}
namespace bftEngine {
namespace impl {

class SeqNumInfo {
 public:
  SeqNumInfo();
  ~SeqNumInfo();

  void resetAndFree();
  void getAndReset(std::unique_ptr<PrePrepareMsg>& outPrePrepare,
                   std::unique_ptr<PrepareFullMsg>& outcombinedValidSignatureMsg);

  bool addMsg(std::unique_ptr<PrePrepareMsg> m);
  bool addSelfMsg(std::unique_ptr<PrePrepareMsg> m);

  bool addMsg(std::unique_ptr<PreparePartialMsg> m);
  bool addSelfMsg(std::unique_ptr<PreparePartialMsg> m);

  bool addMsg(std::unique_ptr<PrepareFullMsg> m);

  bool addMsg(std::unique_ptr<CommitPartialMsg> m);
  bool addSelfCommitPartialMsgAndDigest(std::unique_ptr<CommitPartialMsg> m,
                                        Digest& commitDigest);

  bool addMsg(std::unique_ptr<CommitFullMsg> m);

  void forceComplete();

  const PrePrepareMsg* getPrePrepareMsg() const;
  const PrePrepareMsg* getSelfPrePrepareMsg() const;

  const PreparePartialMsg* getSelfPreparePartialMsg() const;
  const PrepareFullMsg* getValidPrepareFullMsg() const;

  const CommitPartialMsg* getSelfCommitPartialMsg() const;
  const CommitFullMsg* getValidCommitFullMsg() const;

  bool hasPrePrepareMsg() const;

  bool isPrepared() const;

  // TODO(GG): beware this name may mislead (not sure...). rename ??
  bool isCommitted__gg() const;  

  bool preparedOrHasPreparePartialFromReplica(ReplicaId repId) const;
  bool committedOrHasCommitPartialFromReplica(ReplicaId repId) const;

  Time getTimeOfFisrtRelevantInfoFromPrimary() const;
  Time getTimeOfLastInfoRequest() const;
  Time lastUpdateTimeOfCommitMsgs() const {
    return commitUpdateTime;
  }  // TODO(GG): check usage....

  PartialProofsSet& partialProofs();
  PartialExecProofsSet& partialExecProofs();
  void startSlowPath();
  bool slowPathStarted();

  void setTimeOfLastInfoRequest(Time t);

  void onCompletionOfPrepareSignaturesProcessing(
      SeqNum seqNumber,
      ViewNum viewNumber,
      const std::set<ReplicaId>& replicasWithBadSigs);
  void onCompletionOfPrepareSignaturesProcessing(SeqNum seqNumber,
                                                 ViewNum viewNumber,
                                                 const char* combinedSig,
                                                 uint16_t combinedSigLen);
  void onCompletionOfCombinedPrepareSigVerification(SeqNum seqNumber,
                                                    ViewNum viewNumber,
                                                    bool isValid);

  void onCompletionOfCommitSignaturesProcessing(
      SeqNum seqNumber,
      ViewNum viewNumber,
      const std::set<uint16_t>& replicasWithBadSigs) {
    commitMsgsCollector->onCompletionOfSignaturesProcessing(
        seqNumber, viewNumber, replicasWithBadSigs);
  }

  void onCompletionOfCommitSignaturesProcessing(SeqNum seqNumber,
                                                ViewNum viewNumber,
                                                const char* combinedSig,
                                                uint16_t combinedSigLen) {
    commitMsgsCollector->onCompletionOfSignaturesProcessing(
        seqNumber, viewNumber, combinedSig, combinedSigLen);
  }

  void onCompletionOfCombinedCommitSigVerification(SeqNum seqNumber,
                                                   ViewNum viewNumber,
                                                   bool isValid) {
    commitMsgsCollector->onCompletionOfCombinedSigVerification(
        seqNumber, viewNumber, isValid);
  }

 protected:
  class ExFuncForPrepareCollector {
   public:
    // external messages
    static std::unique_ptr<PrepareFullMsg> createCombinedSignatureMsg(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        const char* const combinedSig,
        uint16_t combinedSigLen);

    // internal messages
    static InternalMessage* createInterCombinedSigFailed(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        std::set<uint16_t> replicasWithBadSigs);
    static InternalMessage* createInterCombinedSigSucceeded(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        const char* combinedSig,
        uint16_t combinedSigLen);
    static InternalMessage* createInterVerifyCombinedSigResult(
        void* context, SeqNum seqNumber, ViewNum viewNumber, bool isValid);

    // from the Replica object
    static uint16_t numberOfRequiredSignatures(void* context);
    static IThresholdVerifier* thresholdVerifier(void* context);
    static util::SimpleThreadPool& threadPool(void* context);
    static IncomingMsgsStorage& incomingMsgsStorage(void* context);
  };

  class ExFuncForCommitCollector {
   public:
    // external messages
    static std::unique_ptr<CommitFullMsg> createCombinedSignatureMsg(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        const char* const combinedSig,
        uint16_t combinedSigLen);

    // internal messages
    static InternalMessage* createInterCombinedSigFailed(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        std::set<uint16_t> replicasWithBadSigs);
    static InternalMessage* createInterCombinedSigSucceeded(
        void* context,
        SeqNum seqNumber,
        ViewNum viewNumber,
        const char* combinedSig,
        uint16_t combinedSigLen);
    static InternalMessage* createInterVerifyCombinedSigResult(
        void* context, SeqNum seqNumber, ViewNum viewNumber, bool isValid);

    // from the ReplicaImp object
    static uint16_t numberOfRequiredSignatures(void* context);
    static IThresholdVerifier* thresholdVerifier(void* context);
    static util::SimpleThreadPool& threadPool(void* context);
    static IncomingMsgsStorage& incomingMsgsStorage(void* context);
  };

  InternalReplicaApi* replica = nullptr;

  std::unique_ptr<PrePrepareMsg> prePrepareMsg;

  CollectorOfThresholdSignatures<PreparePartialMsg,
                                 PrepareFullMsg,
                                 ExFuncForPrepareCollector>*
      prepareSigCollector;
  CollectorOfThresholdSignatures<CommitPartialMsg,
                                 CommitFullMsg,
                                 ExFuncForCommitCollector>* commitMsgsCollector;

  PartialProofsSet* partialProofsSet;  // TODO(GG): replace with an instance of
                                       // CollectorOfThresholdSignatures
  PartialExecProofsSet*
      partialExecProofsSet;  // TODO(GG): replace with an instance of
                             // CollectorOfThresholdSignatures

  bool primary;  // true iff PrePrepareMsg was added with addSelfMsg

  bool forcedCompleted;

  bool slowPathHasStarted;

  Time firstSeenFromPrimary;
  Time timeOfLastInfoRequest;
  Time commitUpdateTime;

 public:
  // methods for SequenceWithActiveWindow
  static void init(SeqNumInfo& i, void* d);

  static void free(SeqNumInfo& i) { i.resetAndFree(); }

  static void reset(SeqNumInfo& i) { i.resetAndFree(); }
};

}  // namespace impl
}  // namespace bftEngine
