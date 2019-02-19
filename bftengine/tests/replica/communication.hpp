// Concord
//
// Copyright (c) 2019 VMware, Inc. All Rights Reserved.
//
// This product is licensed to you under the Apache 2.0 license (the "License").
// You may not use this product except in compliance with the Apache 2.0 License.
//
// This product may include a number of subcomponents with separate copyright
// notices and license terms. Your use of these subcomponents is subject to the
// terms and conditions of the subcomponent's license, as noted in the
// LICENSE file.

#include <unordered_map>
#include <vector>
#include "ICommunication.hpp"

using namespace std;

typedef struct Msg {
   const char *const buf;
   const size_t size;
} msg;

// This is a test implementation of ICommunication
class TestCommunication : public bftEngine::ICommunication {

   static constexpr int kMaxMsgSize = 1024*1024;

   int getMaxMessageSize() override {
      return kMaxMsgSize;
   }

   int Start() override {
      running_ = true;
      return 0;
   }

   int Stop() override {
      running_ = false;
      return 0;
   }

   bool isRunning() const override {
      return running_;
   }

   bftEngine::ConnectionStatus getCurrentConnectionStatus(
         const NodeNum node) const override; 


   // TODO(AJS): Capitalize function name to match google c++ style guide
   // TODO(AJS): An error code return value is not really a good idea for async
   // messages, since it implies the message will be delivered on '0'. Better to
   // return void.
   int sendAsyncMessage(const NodeNum dest,
                        const char *const message,
                        const size_t messageLength) override;

   // TODO(AJS): Capitalize function name to match google c++ style guide
   void setReceiver(NodeNum _, bftEngine::IReceiver* receiver) override {
      receiver_ = receiver;
   }

   // The destructor should be called here before the destruction of the
   // receiver. Otherwise, this code may still try to use the receiver. 
   ~TestCommunication() {}

   private:
      bool running_;
      bftEngine::IReceiver* receiver_;
      unordered_map<NodeNum, bool> connected_;

      // Invariant: Every entry in connected_ must have a corresponding entry in
      // mailboxes_. We keep connected_ and mailboxes_ separate, since we want
      // to allow the possiblity of delivering previous messages put into the
      // mailbox before the disconnection happened. We are simulating
      // a real network, so messages may be sitting in an intermediate router,
      // or on the nic endpoint and may still possibly get delivered even though
      // the senders connection closed.
      unordered_map<NodeNum, vector<msg>> mailboxes_;
};
