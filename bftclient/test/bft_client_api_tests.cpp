// Concord
//
// Copyright (c) 2020 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the 'License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the sub-component's license, as noted in the
// LICENSE file.

#include <vector>

#include "gtest/gtest.h"

#include "bftclient/bft_client.h"

using namespace bft::client;
using namespace bft::communication;

// This communication fake takes a Behavior callback parameterized by each test. This callback
// receives messages sent into the network via sendAsyncMessage. Depending on the test specific
// behavior it replies with the appropriate reply messages to the receiver.
template <typename Behavior>
class FakeCommunication : public bft::communication::ICommunication {
 public:
  FakeCommunication(Behavior behavior) : behavior_(behavior) {}

  int getMaxMessageSize() override { return 1024; }
  int Start() override { return 0; }
  int Stop() override { return 0; }
  bool isRunning() const override { return true; }
  ConnectionStatus getCurrentConnectionStatus(const NodeNum node) const override { return ConnectionStatus{}; }

  int sendAsyncMessage(const NodeNum dest, const char* const msg, const size_t len) override {
    Behavior(receiver_, ReplicaId{(uint16_t)dest}, Msg(msg, msg + len));
    return 0;
  }

  void setReceiver(NodeNum id, IReceiver* receiver) override { receiver_ = receiver; }

 private:
  IReceiver* receiver_;
  Behavior behavior_;
};
