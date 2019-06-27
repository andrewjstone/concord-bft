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

#include <cstring>
#include <unordered_map>
#include <set>

#include "PrimitiveTypes.hpp"
#include "Digest.hpp"
#include "Crypto.hpp"
#include "SimpleThreadPool.hpp"
#include "IncomingMsgsStorage.hpp"
#include "assertUtils.hpp"

namespace bftEngine {
namespace impl {

class InternalMessage;

template <typename PART, typename FULL, typename ExternalFunc>
// TODO(GG): consider to enforce the requirements from PART, FULL and
// ExternalFunc
class CollectorOfThresholdSignatures {
 public:
  CollectorOfThresholdSignatures(void* cnt) { this->context = cnt; }

  ~CollectorOfThresholdSignatures() { resetAndFree(); }

  bool addMsgWithPartialSignature(std::unique_ptr<PART> partialSigMsg,
                                  ReplicaId repId) {
    Assert(partialSigMsg.get() != nullptr);

    if (combinedValidSignatureMsg || (replicasInfo.count(repId) > 0))
      return false;

    // add partialSigMsg to replicasInfo
    RepInfo info = {std::move(partialSigMsg), SigState::Unknown};
    replicasInfo[repId] = std::move(info);

    numberOfUnknownSignatures++;

    trySendToBkThread();

    return true;
  }

  bool addMsgWithCombinedSignature(std::unique_ptr<FULL> combinedSigMsg) {
    if (combinedValidSignatureMsg || candidateCombinedSignatureMsg)
      return false;

    candidateCombinedSignatureMsg.swap(combinedSigMsg);

    trySendToBkThread();

    return true;
  }

  void setExpected(SeqNum seqNumber, ViewNum view, Digest& digest) {
    Assert(seqNumber != 0);
    Assert(expectedSeqNumber == 0);
    Assert(!processingSignaturesInTheBackground);

    expectedSeqNumber = seqNumber;
    expectedView = view;
    expectedDigest = digest;

    trySendToBkThread();
  }

  void resetAndFree() {
    if ((replicasInfo.size() == 0) && !combinedValidSignatureMsg &&
        !candidateCombinedSignatureMsg && (expectedSeqNumber == 0))
      return;  // already empty

    resetContent();
  }

  void getAndReset(std::unique_ptr<FULL>& outcombinedValidSignatureMsg) {
    outcombinedValidSignatureMsg.swap(combinedValidSignatureMsg);

    if ((replicasInfo.size() == 0) && !combinedValidSignatureMsg &&
        !candidateCombinedSignatureMsg && (expectedSeqNumber == 0))
      return;  // already empty

    resetContent();
  }

  bool hasPartialMsgFromReplica(ReplicaId repId) const {
    return (replicasInfo.count(repId) > 0);
  }

  const PART* getPartialMsgFromReplica(ReplicaId repId) const {
    if (replicasInfo.count(repId) == 0) return nullptr;

    const RepInfo& r = replicasInfo.at(repId);
    return r.partialSigMsg.get();
  }

  const FULL* getMsgWithValidCombinedSignature() const {
    return combinedValidSignatureMsg.get();
  }

  bool isComplete() const {
    return (combinedValidSignatureMsg.get() != nullptr);
  }

  void onCompletionOfSignaturesProcessing(
      SeqNum seqNumber,
      ViewNum view,
      const std::set<ReplicaId>&
          replicasWithBadSigs)  // if we found bad signatures
  {
    Assert(expectedSeqNumber == seqNumber);
    Assert(expectedView == view);
    Assert(processingSignaturesInTheBackground);
    Assert(combinedValidSignatureMsg.get() == nullptr);

    processingSignaturesInTheBackground = false;

    for (const ReplicaId repId : replicasWithBadSigs) {
      RepInfo& repInfo = replicasInfo[repId];
      repInfo.state = SigState::Invalid;
      numberOfUnknownSignatures--;
    }

    trySendToBkThread();
  }

  void onCompletionOfSignaturesProcessing(
      SeqNum seqNumber,
      ViewNum view,
      const char* combinedSig,
      uint16_t combinedSigLen)  // if we compute a valid combined signature
  {
    Assert(expectedSeqNumber == seqNumber);
    Assert(expectedView == view);
    Assert(processingSignaturesInTheBackground);
    Assert(combinedValidSignatureMsg == nullptr);

    processingSignaturesInTheBackground = false;

    if (candidateCombinedSignatureMsg.get() != nullptr) {
      candidateCombinedSignatureMsg.reset();
    }

    combinedValidSignatureMsg = ExternalFunc::createCombinedSignatureMsg(
        context, seqNumber, view, combinedSig, combinedSigLen);
  }

  void onCompletionOfCombinedSigVerification(SeqNum seqNumber,
                                             ViewNum view,
                                             bool isValid) {
    Assert(expectedSeqNumber == seqNumber);
    Assert(expectedView == view);
    Assert(processingSignaturesInTheBackground);
    Assert(combinedValidSignatureMsg.get() == nullptr);
    Assert(candidateCombinedSignatureMsg.get() != nullptr);

    processingSignaturesInTheBackground = false;

    if (isValid) {
      combinedValidSignatureMsg.swap(candidateCombinedSignatureMsg);
      candidateCombinedSignatureMsg.reset();
    } else {
      candidateCombinedSignatureMsg.reset();
      trySendToBkThread();
    }
  }

 protected:
  void trySendToBkThread() {
    Assert(combinedValidSignatureMsg.get() == nullptr);

    if (numOfRequiredSigs == 0)  // init numOfRequiredSigs
      numOfRequiredSigs = ExternalFunc::numberOfRequiredSignatures(context);

    if (processingSignaturesInTheBackground || expectedSeqNumber == 0) return;

    if (candidateCombinedSignatureMsg.get() != nullptr) {
      processingSignaturesInTheBackground = true;

      CombinedSigVerificationJob* bkJob = new CombinedSigVerificationJob(
          ExternalFunc::thresholdVerifier(context),
          &ExternalFunc::incomingMsgsStorage(context),
          expectedSeqNumber,
          expectedView,
          expectedDigest,
          candidateCombinedSignatureMsg->signatureBody(),
          candidateCombinedSignatureMsg->signatureLen(),
          context);

      ExternalFunc::threadPool(context).add(bkJob);
    } else if (numberOfUnknownSignatures >= numOfRequiredSigs) {
      processingSignaturesInTheBackground = true;

      SignaturesProcessingJob* bkJob = new SignaturesProcessingJob(
          ExternalFunc::thresholdVerifier(context),
          &ExternalFunc::incomingMsgsStorage(context),
          expectedSeqNumber,
          expectedView,
          expectedDigest,
          numOfRequiredSigs,
          context);

      uint16_t numOfPartSigsInJob = 0;
      for (auto&& info : replicasInfo) {
        if (info.second.state != SigState::Invalid) {
          const char* sig = info.second.partialSigMsg->signatureBody();
          uint16_t len = info.second.partialSigMsg->signatureLen();
          bkJob->add(info.first, sig, len);
          numOfPartSigsInJob++;
        }

        if (numOfPartSigsInJob == numOfRequiredSigs) break;
      }

      Assert(numOfPartSigsInJob == numOfRequiredSigs);

      ExternalFunc::threadPool(context).add(bkJob);
    }
  }

  class SignaturesProcessingJob
      : public util::SimpleThreadPool::Job  // TODO(GG): include the replica Id
                                            // (to identify replicas that send
                                            // bad combined signatures)
  {
   private:
    struct SigData {
      ReplicaId srcRepId;
      char* sigBody;
      uint16_t sigLength;
    };

    IThresholdVerifier* const verifier;
    IncomingMsgsStorage* const repMsgsStorage;
    const SeqNum expectedSeqNumber;
    const ViewNum expectedView;
    const Digest expectedDigest;
    const uint16_t reqDataItems;
    SigData* const sigDataItems;

    uint16_t numOfDataItems;

    void* context;

    virtual ~SignaturesProcessingJob() {}

   public:
    SignaturesProcessingJob(IThresholdVerifier* const thresholdVerifier,
                            IncomingMsgsStorage* const replicaMsgsStorage,
                            SeqNum seqNum,
                            ViewNum view,
                            Digest& digest,
                            uint16_t numOfRequired,
                            void* cnt)
        : verifier{thresholdVerifier},
          repMsgsStorage{replicaMsgsStorage},
          expectedSeqNumber{seqNum},
          expectedView{view},
          expectedDigest{digest},
          reqDataItems{numOfRequired},
          sigDataItems{new SigData[numOfRequired]},
          numOfDataItems(0) {
      this->context = cnt;
    }

    void add(ReplicaId srcRepId, const char* sigBody, uint16_t sigLength) {
      Assert(numOfDataItems < reqDataItems);

      SigData d;
      d.srcRepId = srcRepId;
      d.sigLength = sigLength;
      d.sigBody = (char*)std::malloc(sigLength);
      memcpy(d.sigBody, sigBody, sigLength);

      sigDataItems[numOfDataItems] = d;
      numOfDataItems++;
    }

    virtual void release() override {
      for (uint16_t i = 0; i < numOfDataItems; i++) {
        SigData& d = sigDataItems[i];
        std::free(d.sigBody);
      }

      delete[] sigDataItems;

      delete this;
    }

    virtual void execute() override {
      Assert(numOfDataItems == reqDataItems);

      // TODO(GG): can utilize several threads (discuss with Alin)

      const uint16_t bufferSize =
          (uint16_t)verifier->requiredLengthForSignedData();
      char* const bufferForSigComputations =
          (char*)alloca(bufferSize);  // TODO(GG): check

      {
        IThresholdAccumulator* acc = verifier->newAccumulator(false);

        for (uint16_t i = 0; i < reqDataItems; i++) {
          acc->add(sigDataItems[i].sigBody, sigDataItems[i].sigLength);
        }

        acc->setExpectedDigest(
            reinterpret_cast<unsigned char*>(expectedDigest.content()),
            DIGEST_SIZE);

        acc->getFullSignedData(bufferForSigComputations, bufferSize);

        verifier->release(acc);
      }

      bool succ = verifier->verify((char*)&expectedDigest,
                                   sizeof(Digest),
                                   bufferForSigComputations,
                                   bufferSize);

      if (!succ) {
        std::set<ReplicaId> replicasWithBadSigs;

        // TODO(GG): A clumsy way to do verification - find a better way ....

        IThresholdAccumulator* accWithVer = verifier->newAccumulator(true);
        accWithVer->setExpectedDigest(
            reinterpret_cast<unsigned char*>(expectedDigest.content()),
            DIGEST_SIZE);

        uint16_t currNumOfValidShares = 0;
        for (uint16_t i = 0; i < reqDataItems; i++) {
          uint16_t prevNumOfValidShares = currNumOfValidShares;

          currNumOfValidShares = (uint16_t)accWithVer->add(
              sigDataItems[i].sigBody, sigDataItems[i].sigLength);

          if (prevNumOfValidShares + 1 != currNumOfValidShares)
            replicasWithBadSigs.insert(sigDataItems[i].srcRepId);
        }

        if (replicasWithBadSigs.size() == 0) {
          // TODO(GG): print warning / error ??
        }

        verifier->release(accWithVer);

        // send internal message with the results
        std::unique_ptr<InternalMessage> iMsg(
            ExternalFunc::createInterCombinedSigFailed(
                context, expectedSeqNumber, expectedView, replicasWithBadSigs));
        repMsgsStorage->pushInternalMsg(std::move(iMsg));
      } else {
        // send internal message with the results
        std::unique_ptr<InternalMessage> iMsg(
            ExternalFunc::createInterCombinedSigSucceeded(
                context,
                expectedSeqNumber,
                expectedView,
                bufferForSigComputations,
                bufferSize));
        repMsgsStorage->pushInternalMsg(std::move(iMsg));
      }
    }
  };

  class CombinedSigVerificationJob : public util::SimpleThreadPool::Job {
   private:
    IThresholdVerifier* const verifier;
    IncomingMsgsStorage* const repMsgsStorage;
    const SeqNum expectedSeqNumber;
    const ViewNum expectedView;
    const Digest expectedDigest;
    const char* combinedSig;
    uint16_t combinedSigLen;
    void* context;

    virtual ~CombinedSigVerificationJob() {}

   public:
    CombinedSigVerificationJob(IThresholdVerifier* const thresholdVerifier,
                               IncomingMsgsStorage* const replicaMsgsStorage,
                               SeqNum seqNum,
                               ViewNum view,
                               Digest& digest,
                               const char* combinedSigBody,
                               uint16_t combinedSigLength,
                               void* cnt)
        : verifier{thresholdVerifier},
          repMsgsStorage{replicaMsgsStorage},
          expectedSeqNumber{seqNum},
          expectedView{view},
          expectedDigest{digest},
          combinedSig{(char*)std::malloc(combinedSigLength)},
          combinedSigLen{combinedSigLength} {
      memcpy((char*)combinedSig, combinedSigBody, combinedSigLen);
      this->context = cnt;
    }

    virtual void release() override {
      std::free((void*)combinedSig);

      delete this;
    }

    virtual void execute() override {
      bool succ = verifier->verify(
          (char*)&expectedDigest, sizeof(Digest), combinedSig, combinedSigLen);

      std::unique_ptr<InternalMessage> iMsg(
          ExternalFunc::createInterVerifyCombinedSigResult(
              context, expectedSeqNumber, expectedView, succ));
      repMsgsStorage->pushInternalMsg(std::move(iMsg));
    }
  };

 protected:
  void* context;

  uint16_t numOfRequiredSigs = 0;

  void resetContent() {
    processingSignaturesInTheBackground = false;

    numberOfUnknownSignatures = 0;

    for (auto&& m : replicasInfo) {
      RepInfo& repInfo = m.second;
      repInfo.partialSigMsg.reset();
    }
    replicasInfo.clear();

    combinedValidSignatureMsg.reset();
    candidateCombinedSignatureMsg.reset();

    if (expectedSeqNumber != 0)  // if we have expected values
    {
      expectedSeqNumber = 0;
      expectedView = 0;
      expectedDigest.makeZero();
    }
  }

  enum class SigState { Unknown = 0, Invalid };

  struct RepInfo {
    std::unique_ptr<PART> partialSigMsg;
    SigState state;
  };

  bool processingSignaturesInTheBackground = false;

  uint16_t numberOfUnknownSignatures = 0;
  std::unordered_map<ReplicaId, RepInfo> replicasInfo;

  std::unique_ptr<FULL> combinedValidSignatureMsg;
  std::unique_ptr<FULL>
      candidateCombinedSignatureMsg;  // holds msg when expectedSeqNumber is not
                                      // known yet

  SeqNum expectedSeqNumber = 0;
  ViewNum expectedView = 0;
  Digest expectedDigest;
};

}  // namespace impl
}  // namespace bftEngine
