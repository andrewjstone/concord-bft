// Concord
//
// Copyright (c) 2018-2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include "CommDefs.hpp"
#include "TlsTcpImpl.h"

namespace bftEngine {

// This is the public interface to this library. TlsTcpCommunication implements ICommunication.
TlsTCPCommunication::TlsTCPCommunication(const TlsTcpConfig &config) {
  ptrImpl_.reset(new TlsTCPCommunication::TlsTcpImpl(config));
}

TlsTCPCommunication::~TlsTCPCommunication() {}

TlsTCPCommunication *TlsTCPCommunication::create(const TlsTcpConfig &config) { return new TlsTCPCommunication(config); }

int TlsTCPCommunication::getMaxMessageSize() { return ptrImpl_->getMaxMessageSize(); }

int TlsTCPCommunication::Start() {
  ptrImpl_->Start();
  return 0;
}

int TlsTCPCommunication::Stop() {
  if (!ptrImpl_) return 0;

  ptrImpl_->Stop();
  return 0;
}

bool TlsTCPCommunication::isRunning() const { return ptrImpl_->isRunning(); }

ConnectionStatus TlsTCPCommunication::getCurrentConnectionStatus(const NodeNum node) const {
  return ptrImpl_->getCurrentConnectionStatus(node);
}

int TlsTCPCommunication::sendAsyncMessage(const NodeNum destNode,
                                          const char *const message,
                                          const size_t messageLength) {
  ptrImpl_->sendAsyncMessage(destNode, message, messageLength);
  return 0;
}

void TlsTCPCommunication::setReceiver(NodeNum receiverNum, IReceiver *receiver) {
  ptrImpl_->setReceiver(receiverNum, receiver);
}

}  // namespace bftEngine