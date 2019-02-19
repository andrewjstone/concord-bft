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
#include "TestCommunication.hpp"

using namespace std;

bftEngine::ConnectionStatus TestCommunication::getCurrentConnectionStatus(
      const NodeNum node) const {
   if (connected_.find(node) == connected_.end()) {
      return bftEngine::ConnectionStatus::Disconnected;
   }
   return bftEngine::ConnectionStatus::Connected;
}

int TestCommunication::sendAsyncMessage(const NodeNum dest,
                                        const char *const message,
                                        const size_t messageLength) {
   if (connected_.find(dest) == connected_.end()) {
      // Drop the message
      return -1;
   }
   mailboxes_.at(dest).push_back(Msg{message, messageLength});
   return 0;
}
